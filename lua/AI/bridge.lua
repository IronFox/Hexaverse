--#require "player.lua"
--[[
Bridge: an outside player decides, the scripts do the handwork
=============================================================

For a person or a model to play a faction through the same interface the AIs use. Every
Bridge.every rounds (and at once when something of mine is attacked after a quiet spell) the
script writes what the faction knows to a file and waits for orders. The orders are Lua, run
inside the AI with everything the scripts have, plus a few helpers for the usual decisions
(below). Between the orders the player.lua scripts carry on: they build, produce, move and
shoot, following the directions given.

Files, in the folder named by the environment variable HEXAVERSE_AI_BRIDGE (the bridge is off
without it, and this script then plays exactly like player.lua with directions):
  help_<name>.txt           written once: this text and the type names
  state_<name>_<round>.txt  what the faction knows that round
  orders_<name>_<round>.lua answered by the outside player; an empty file means "carry on"
  result_<name>_<round>.txt what became of the orders (errors, what was accepted)
<name> is Bridge.name ('p1' unless a variant sets it). The game waits for the orders file up to
Bridge.patience seconds, then carries on with the standing orders.

Helpers for the orders file:
  Bridge.Take(region [, priority])   direction: get this region (build there, drive out who holds it)
  Bridge.Hold(region [, priority])   direction: keep this region, defend it
  Bridge.Avoid(region)               stay out of it
  Bridge.Drop(region)                withdraw what was said about the region
  Bridge.Attack(region)              the army goes for this region now, however strong it is
  Bridge.Defend()                    the army stays home
  Bridge.Auto()                      posture is left to the scripts again
  Bridge.Build(typeName, region [, count])   next free builders put this structure near that region
                                     (a builder builds within a few moves of where it is; mines are
                                     not ordered this way, they follow Bridge.Take)
  Bridge.BuildAt(typeName, cell)     the next free builder builds it on exactly that cell (mines too);
                                     if it cannot, the reason comes with the next state
  Bridge.Research(text)              research this next (part of a topic name, for example 'unlock:artillery')
  Bridge.Weights{army = 3, defense = 6, ...} how much each need counts (see "Weights:" in the state)
  Bridge.Set(name, value)            any setting of Cfg (for example Bridge.Set('attackMinUnits', 8))
  Bridge.Every(rounds)               how often to be asked
  Bridge.Say(text)                   a note for the log (why this decision), kept with the match
  Bridge.Out(text)                   comes back in the result file of these orders (ask and get the answer at once)
  Bridge.Cells(region)               the region cell by cell into the result file: id, height, state, what stands there, neighbours
  Bridge.Units([region])             my units one by one into the result file: id, type, cell, health
  Bridge.Post(unitId, cell)          that unit goes to the cell and stays, shooting what comes in range (scout, guard, artillery post)
  Bridge.Free(unitId)                gives it back to the scripts
Directions and posture stand until changed. Anything else is plain Lua with the game API
(Game, Sense, Map, Economy, Military, Control, Significance ...).

Everything decided here is logged ("Bridge orders r=N: ...", and the directions as
"Direction: ..." like any other), so that matches played this way can be learned from.
]]

Cfg.logPrefix = 'bridge: ';
Cfg.useDirections = true;

Bridge = {
	name = 'p1',
	every = 10,
	patience = 900,			-- seconds to wait for orders
	alarmAfter = 15,		-- rounds without an attack after which a new attack asks at once
	alarmGap = 4,			-- but not within this many rounds of the last time asked
	pins = {},				-- region id -> {kind, priority}
	posture = nil,			-- nil (scripts decide), 'defend', or a region id to attack
	build = {},				-- queue of {type, region}
	notes = {},
	lastAsked = nil,
	lastAttack = -100,
	pending = {},			-- cell -> {job, unit}: builders on their way to exact cells
	known = {},				-- cell index -> hostile structure seen there
	upgrades = {},
	posts = {},				-- unit id -> {cell}: units placed by the outside player
	results = {},			-- "Direction result" and other lines since the last state
};

local function Add(lines, fmt, ...)
	lines[#lines+1] = (select('#', ...) > 0) and string.format(fmt, ...) or fmt;
end;

function Bridge.On()
	if Bridge.on == nil then
		Bridge.on = Game.BridgeWrite ~= nil and Game.BridgeWrite('alive_'..Bridge.name..'.txt', 'bridge '..Bridge.name) or false;
		Log('Bridge: '..(Bridge.on and ('on as '..Bridge.name..', asking every '..Bridge.every..' rounds') or 'off (no HEXAVERSE_AI_BRIDGE folder): playing as player.lua'));
	end;
	return Bridge.on;
end;

-------------------------------------------------------------------------------
-- helpers for the orders
-------------------------------------------------------------------------------

local function Region(id)
	id = tonumber(id);
	if id == nil or Map.regions[id] == nil then error('no such region: '..tostring(id), 3); end;
	return id;
end;

function Bridge.Take(region, priority) Bridge.pins[Region(region)] = {kind = 'take', priority = priority or 1}; end;
function Bridge.Hold(region, priority) Bridge.pins[Region(region)] = {kind = 'hold', priority = priority or 1}; end;
function Bridge.Avoid(region) Bridge.pins[Region(region)] = {kind = 'avoid', priority = 1}; end;
function Bridge.Drop(region) Bridge.pins[Region(region)] = nil; end;
function Bridge.Attack(region) Bridge.posture = Region(region); end;
function Bridge.Defend() Bridge.posture = 'defend'; end;
function Bridge.Auto() Bridge.posture = nil; end;
function Bridge.Every(rounds) Bridge.every = math.max(1, math.floor(tonumber(rounds) or 10)); end;
function Bridge.Say(text) Bridge.notes[#Bridge.notes+1] = tostring(text); end;

function Bridge.Set(name, value)
	if Cfg[name] == nil then error('no such setting: '..tostring(name), 2); end;
	Cfg[name] = value;
end;

-- No more of this structure type than n (nil lifts the cap). Labs also follow wantLabsRich and wantLabsMax.
function Bridge.Cap(typeName, n)
	typeName = string.gsub(tostring(typeName), ':Tower$', ' Tower');
	if Game.Type.units[typeName] == nil then error('no such type: '..tostring(typeName), 2); end;
	Cfg.caps[typeName] = n and tonumber(n) or nil;
end;

-- Switches structures of a type off or on (off: they draw no power). Returns how many were switched.
function Bridge.Switch(typeName, on, count)
	local n = 0;
	typeName = string.gsub(tostring(typeName), ':Tower$', ' Tower');
	for _,st in ipairs(Sense.myStructures or {}) do
		if st.type and string.gsub(st.type.name, ':Tower$', ' Tower') == typeName and st.Enable and (count == nil or n < count) and (st.isEnabled == nil or st.isEnabled ~= on) then
			if pcall(st.Enable, on) then
				n = n + 1;
				-- (what the outside player switched off stays off: the power care leaves it alone)
				Mem.keepOff = Mem.keepOff or {};
				Mem.keepOff[st.id] = (not on) and true or nil;
			end;
		end;
	end;
	Bridge.Out('%d %s switched %s', n, typeName, on and 'on' or 'off');
	return n;
end;

function Bridge.Weights(w)
	Cfg.needWeights = Cfg.needWeights or {};
	for k,v in pairs(w) do Cfg.needWeights[k] = tonumber(v); end;
end;

local function Structure(typeName)
	local t = Game.Type.units[typeName];
	if t == nil or not t.isStructure then error('no such structure type: '..tostring(typeName), 3); end;
	if Types.deprecated[typeName] then error(typeName..' is deprecated and must not be built: use a regular structure', 3); end;
	if not Types.IsUnlocked(t) then error(typeName..' is not researched yet (see "Can build" in the state; Bridge.Research gets it)', 3); end;
	return t;
end;

function Bridge.BuildAt(typeName, cell)
	local t = Structure(typeName);
	cell = tonumber(cell);
	if cell == nil or Economy.Cell(cell) == nil then error('no such cell: '..tostring(cell), 2); end;
	Bridge.build[#Bridge.build+1] = {type = t, cell = cell};
end;

function Bridge.Research(text)
	text = tostring(text);
	-- (say what it matches: a name that matches nothing would otherwise do nothing, silently)
	do
		local parts, hits = {}, {};
		for p in string.gmatch(string.lower(text), '[^:]+') do parts[#parts+1] = p; end;
		for name,topic in pairs(Game.Research.topics) do
			local fn = string.lower(topic.fullName or name);
			local match = true;
			for _,p in ipairs(parts) do if not string.find(fn, p, 1, true) then match = false; break; end; end;
			if match then
				hits[#hits+1] = tostring(topic.fullName or name)..(topic.isCompleted and ' (done)' or ((topic.dependency and not topic.dependency.isCompleted)
					and (' (waits for '..tostring(topic.dependency.fullName or topic.dependency.name or '?')..')') or ''));
			end;
		end;
		table.sort(hits);
		Bridge.Out('Research "%s" matches: %s', text, #hits > 0 and table.concat(hits, '; ') or 'NOTHING (see Game.Research.topics for the names)');
		-- into the faction's queue now, in place of the last entry if it is full
		for name,topic in pairs(Game.Research.topics) do
			local fn = string.lower(topic.fullName or name);
			local match = not topic.isEndless and not topic.isCompleted and not topic.isQueued
				and (topic.dependency == nil or topic.dependency.isCompleted);
			for _,p in ipairs(parts) do if not string.find(fn, p, 1, true) then match = false; break; end; end;
			if match then
				local queue = Game.Research.myQueue or {};
				if #queue >= (Game.Research.maxQueueLength or 3) and queue[#queue].Dequeue then pcall(queue[#queue].Dequeue); end;
				local ok, err = pcall(topic.Enqueue);
				Bridge.Out(ok and 'queued now: %s' or 'could not queue %s: %s', tostring(topic.fullName or name), tostring(err));
			end;
		end;
	end;
	for i,w in ipairs(Cfg.research) do
		if w == text then table.remove(Cfg.research, i); break; end;
	end;
	table.insert(Cfg.research, 1, text);
end;

function Bridge.Build(typeName, region, count)
	local t = Structure(typeName);
	if t == Types.mine then error('mines are placed by the scripts where the resources are: use Bridge.Take(region) to get them built there', 2); end;
	region = Region(region);
	for _ = 1,(count or 1) do Bridge.build[#Bridge.build+1] = {type = t, region = region}; end;
end;

-- Anything said with Bridge.Out comes back in the result file of these orders: a question asked
-- in the orders is answered at once, without waiting for the next state.
function Bridge.Out(fmt, ...)
	Bridge.out = Bridge.out or {};
	if #Bridge.out < 400 then Bridge.out[#Bridge.out+1] = (select('#', ...) > 0) and string.format(fmt, ...) or tostring(fmt); end;
end;

local function CellState(cell)
	if Sense.UnderEMP(cell) then return 'emp'; end;
	if Sense.structureCells[cell.index] then return 'built'; end;
	if Control.At(cell) == 0 then return 'unseen'; end;
	if Control.hot[cell.index] then return 'contested'; end;
	return 'ok';
end;

-- The cells of a region, one per line: cell id, ground height, state, what stands there, neighbours.
function Bridge.Cells(region)
	local reg = Map.regions[Region(region)];
	local probe = (Sense.my.constructor or {})[1] or (Sense.my.combat or {})[1];
	local what = {};
	for _,list in pairs(Sense.my) do
		for _,u in ipairs(list) do
			if u.location then what[u.location.cell.index] = 'my '..u.type.name..' #'..tostring(u.id); end;
		end;
	end;
	for _,u in ipairs(Sense.myStructures or {}) do
		if u.location then what[u.location.cell.index] = 'my '..u.type.name; end;
	end;
	for _,e in pairs(Game.hostileUnits) do
		if e.health and e.health > 0 and e.location then what[e.location.cell.index] = 'HOSTILE '..tostring(e.type and e.type.name); end;
	end;
	Bridge.Out('CELLS of region %d (id h=ground height state, water, resources [what] > neighbours; h=? means no ground a builder can stand on; a * marks neighbours in other regions):', reg.id);
	for _,cell in ipairs(reg.cells) do
		local loc = probe and Move.GroundAt(probe, cell);
		local nb = {};
		for _,n in ipairs(cell.neighbors or {}) do nb[#nb+1] = n.index..(Map.cellRegion[n.index] ~= reg.id and '*' or ''); end;
		local extra = '';
		if loc and Economy.IsWaterSurface(cell, loc) then extra = extra..' water'; end;
		for _,res in ipairs({'Minerals', 'Fuel', 'Uranium'}) do
			local ok, lvl = pcall(Game.Grid.GetNaturalResourceAt, res, cell);
			if ok and lvl and lvl > 0 then extra = extra..' '..string.lower(res)..'='..tostring(lvl); end;
		end;
		Bridge.Out('  %d h=%s %s%s%s > %s', cell.index, loc and tostring(loc.height) or '?', CellState(cell), extra,
			what[cell.index] and (' ['..what[cell.index]..']') or '', table.concat(nb, ' '));
	end;
end;

-- Cell steps from a cell: returns {cell index -> steps} up to maxSteps.
local function Steps(from, maxSteps)
	local dist, edge = {[from.index] = 0}, {from};
	for d = 1, maxSteps do
		local nextEdge = {};
		for _,c in ipairs(edge) do
			for _,n in ipairs(c.neighbors or {}) do
				if dist[n.index] == nil then dist[n.index] = d; nextEdge[#nextEdge+1] = n; end;
			end;
		end;
		edge = nextEdge;
	end;
	return dist;
end;

--[[
Hostile structures the faction has seen stay known after they drop out of sight. The game keeps
that memory itself, as ghosts (hostile units with health 0 or less); what is listed here is those
ghosts plus what was sensed while this script watched. Nothing here comes from anywhere else.
]]
function Bridge.Remember()
	-- the game's own memory: markers of hostile contacts carry the unit as it was last seen (a ghost).
	-- Ghost entries are rebuilt from the markers every round, so they go when the game drops them.
	for idx,k in pairs(Bridge.known) do if k.ghost then Bridge.known[idx] = nil; end; end;
	for _,m in pairs(Game.myMarkers or {}) do
		local e = m.type == 'HostileContact' and m.unitReference;
		if e and e.location and e.type and e.type.isStructure and not e.isOwnedByMe then
			local idx = e.location.cell.index;
			if Bridge.known[idx] == nil then
				Bridge.known[idx] = {name = e.type.name, loc = e.location, round = Sense.round - (m.age or 0), ghost = true};
			end;
		end;
	end;
	for _,e in pairs(Game.hostileUnits) do
		if e.health and e.location and e.type and e.type.isStructure and not e.isOwnedByMe then
			local idx = e.location.cell.index;
			if e.health > 0 then
				Bridge.known[idx] = {name = e.type.name, loc = e.location, round = Sense.round};
			elseif Bridge.known[idx] == nil and Control.At(e.location) == 0 then
				-- the game's own memory: a ghost (health 0 or less) is what was seen there and is out of sight now
				Bridge.known[idx] = {name = e.type.name, loc = e.location, round = Sense.round, ghost = true};
			end;
		end;
	end;
	for idx,k in pairs(Bridge.known) do
		if Sense.structureCells[idx] then Bridge.known[idx] = nil;	-- mine now
		elseif not k.ghost and k.round < Sense.round and Control.At(k.loc) > 0 then
			-- the cell is in sight and nothing hostile stands there any more
			Bridge.results[#Bridge.results+1] = k.name..' at cell '..idx..' is gone (the cell is in sight and empty)';
			Bridge.known[idx] = nil;
		end;
	end;
	-- what became of the builders sent to exact cells
	for cell,p in pairs(Bridge.pending) do
		local job = Mem.jobs[p.unit];
		local line = nil;
		if Sense.structureCells[cell] then line = 'built (or under construction)';
		elseif job == nil or job.cell ~= cell then
			line = 'the builder gave it up (danger on its way or at the site, or it was lost)';
			p.job.tries = (p.job.tries or 0) + 1;
			if p.job.tries < 4 then
				Bridge.build[#Bridge.build+1] = p.job;
				line = line..'; it will be tried again';
			else
				line = line..'; given up after 4 tries: make the site and the way safe first';
			end;
		end;
		if line then
			Bridge.results[#Bridge.results+1] = 'BuildAt '..p.job.type.name..' at cell '..cell..': '..line;
			Bridge.pending[cell] = nil;
		end;
	end;
end;

-- Fire at a cell for some rounds (default 3), seen or not: for artillery-like weapons (artillery, EMP)
-- against a structure remembered there. The unit must be posted and in range; what happened comes
-- with the next state.
function Bridge.FireAt(unitId, cell, rounds)
	local post = Bridge.posts[tostring(unitId)];
	if post == nil then error('unit #'..tostring(unitId)..' is not posted: Bridge.Post it within range first', 2); end;
	cell = tonumber(cell);
	if cell == nil or Economy.Cell(cell) == nil then error('no such cell: '..tostring(cell), 2); end;
	post.fire = {cell = cell, left = math.max(1, math.floor(tonumber(rounds) or 3))};
end;

local function FireAtCell(u, post)
	local f = post.fire;
	local known = Bridge.known[f.cell];
	local loc = (known and known.loc) or Move.GroundAt(u, Economy.Cell(f.cell));
	local why = nil;
	if loc == nil then why = 'no known height for that cell (nothing was ever seen there and there is no ground)';
	elseif not (u.turret and u.Attack) then why = 'the unit has no weapon';
	else
		local t = u.turret;
		if t.ammoDemandPerShot and t.ammoAvailability and t.ammoDemandPerShot > t.ammoAvailability then why = 'no ammunition (needs '..tostring(t.ammoDemandPerShot)..')';
		else
			local okr, inRange = pcall(t.IsInRange, loc.world);
			if okr and inRange == false then why = 'out of range from cell '..tostring(u.location.cell.index);
			else
				local ok, err = pcall(u.Attack, {loc});
				if not ok then why = 'refused: '..tostring(err); end;
			end;
		end;
	end;
	Bridge.results[#Bridge.results+1] = 'FireAt cell '..f.cell..' by unit #'..tostring(u.id)..' in round '..Sense.round..': '..(why or 'fired');
	f.left = f.left - 1;
	if why or f.left <= 0 then post.fire = nil; end;
	return why == nil;
end;

-- Hostiles in sight one by one: type, cell, region, health, firing range in cells (structures marked).
function Bridge.Hostiles()
	Bridge.Out('HOSTILES IN SIGHT (type, cell, region, health, range):');
	for _,e in pairs(Game.hostileUnits) do
		if e.health and e.health > 0 and e.location then
			local reg = Map.RegionOfLoc(e.location);
			local tt = e.turret and e.turret.type;
			Bridge.Out('  %s%s, cell %d, region %s, h=%s, health %s, range %s', tostring(e.type and e.type.name), (e.type and e.type.isStructure) and ' (structure)' or '',
				e.location.cell.index, reg and tostring(reg.id) or '?', tostring(e.location.height), tostring(e.health), tt and tostring(tt.attackRange or tt.range or '?') or '-');
		end;
	end;
end;

-- The cells exactly `steps` away from a cell (or up to it with all = true), with their state: where to
-- stand just inside my own range of a target, or just outside its range.
function Bridge.Ring(cell, steps, all)
	local c = Economy.Cell(tonumber(cell));
	if c == nil then error('no such cell: '..tostring(cell), 2); end;
	steps = math.min(12, math.floor(tonumber(steps) or 1));
	local dist = Steps(c, steps);
	local probe = (Sense.my.constructor or {})[1] or (Sense.my.combat or {})[1];
	local rows = {};
	for idx,d in pairs(dist) do
		if d == steps or (all and d > 0) then
			local cc = Economy.Cell(idx);
			local loc = probe and Move.GroundAt(probe, cc);
			rows[#rows+1] = string.format('%d(%dst r%s h=%s %s)', idx, d, tostring(Map.cellRegion[idx] or '?'), loc and tostring(loc.height) or '?', CellState(cc));
		end;
	end;
	table.sort(rows);
	Bridge.Out('CELLS %s%d steps from cell %d: %s', all and 'within ' or '', steps, c.index, table.concat(rows, ' '));
end;

-- How far a unit of mine reaches from where it stands, in cell steps, and from which cells around a
-- target it could hit that target (ranges are distances in space, not steps: height counts).
function Bridge.Reach(unitId, targetCell)
	local u = Bridge.MyUnit(unitId);
	if u == nil or not (u.turret and u.turret.IsInRange) then error('no armed unit of mine #'..tostring(unitId), 2); end;
	local dist = Steps(u.location.cell, 14);
	local far = 0;
	for idx,d in pairs(dist) do
		if d > far then
			local loc = Move.GroundAt(u, Economy.Cell(idx));
			local ok, hit = pcall(function() return u.turret.IsInRange(loc.world); end);
			if ok and hit == true then far = d; end;
		end;
	end;
	Bridge.Out('unit #%s (%s) reaches %d steps from cell %d', tostring(unitId), u.type.name, far, u.location.cell.index);
	if targetCell and u.turret.IsInRangeFrom then
		local tc = Economy.Cell(tonumber(targetCell));
		if tc == nil then error('no such cell: '..tostring(targetCell), 2); end;
		local k = Bridge.known[tc.index];
		local tloc = (k and k.loc) or Move.GroundAt(u, tc);
		local rows = {};
		for idx,d in pairs(Steps(tc, 14)) do
			local from = Move.GroundAt(u, Economy.Cell(idx));
			if from and tloc then
				local ok, hit = pcall(u.turret.IsInRangeFrom, from, tloc.world);
				if ok and hit == true and d >= 6 then rows[#rows+1] = string.format('%d(%dst r%s %s)', idx, d, tostring(Map.cellRegion[idx] or '?'), CellState(Economy.Cell(idx))); end;
			end;
		end;
		table.sort(rows);
		Bridge.Out('cells 6 or more steps from cell %d from which it can hit that cell: %s', tc.index, #rows > 0 and table.concat(rows, ' ') or 'none');
	end;
	return far;
end;

-- The vehicle plants one by one: on or off, what waits finished inside and whether it can get out,
-- what is queued and since when the head of the queue has not changed.
function Bridge.Plants()
	Bridge.Out('PLANTS (a finished vehicle that cannot deploy blocks everything behind it; heavy bases need level ground beside the plant):');
	for _,p in ipairs(Sense.plants or {}) do
		local q = {};
		for _,item in ipairs(p.productionQueue or {}) do q[#q+1] = tostring(item.name or item.id or '?'); end;
		local h = Economy.plantHead[p.id];
		Bridge.Out('  %s at cell %s: %s; finished inside: %s; deployment options %s; queue [%s]%s%s', tostring(p.niceName or p.type.name), tostring(p.location.cell.index),
			p.isEnabled == false and 'OFF' or 'on', tostring(p.productionCompleted or 'nothing'), p.deploymentOptions and tostring(#p.deploymentOptions) or '?',
			table.concat(q, ', '), h and (', head unchanged since r'..h.since) or '',
			Economy.plantBlocked[p.id] and (', BLOCKED since r'..Economy.plantBlocked[p.id]) or '');
		local ex, why = Economy.Exits(p.location);
		Bridge.Out('    exits by terrain: %d of %d neighbours (plant height %s)%s', ex, #p.location.cell.neighbors, tostring(p.location.height), why ~= '' and ('; no exit: '..why) or '');
	end;
end;

-- Steps between two cells (nil beyond 30).
function Bridge.Dist(a, b)
	local ca, cb = Economy.Cell(tonumber(a)), Economy.Cell(tonumber(b));
	if ca == nil or cb == nil then error('no such cell', 2); end;
	local d = Steps(ca, 30)[cb.index];
	Bridge.Out('cell %d to cell %d: %s steps', ca.index, cb.index, tostring(d or 'more than 30'));
	return d;
end;

-- My units one by one (all, or those in a region): id, type, cell, region, health, post.
function Bridge.Units(region)
	region = region and Region(region);
	Bridge.Out('UNITS%s (#id type, cell, region, health, post):', region and (' in region '..region) or '');
	local told = {};
	local function One(u, role)
		if told[tostring(u.id)] then return; end;
		told[tostring(u.id)] = true;
		local reg = u.location and Map.RegionOfLoc(u.location);
		if region == nil or (reg and reg.id == region) then
			local post = Bridge.posts[tostring(u.id)];
			Bridge.Out('  #%s %s (%s), cell %s, region %s, health %s%s', tostring(u.id), u.type.name, role, u.location and tostring(u.location.cell.index) or '?',
				reg and tostring(reg.id) or '?', tostring(u.health), post and (', POSTED at cell '..post.cell) or '');
		end;
	end;
	for role,list in pairs(Sense.my) do for _,u in ipairs(list) do One(u, role); end; end;
	for _,u in ipairs(Bridge.posted or {}) do One(u, 'posted'); end;
	if region == nil then
		for _,u in ipairs(Sense.stored or {}) do
			Bridge.Out('  #%s %s, STORED in a vehicle store (Bridge.Deploy(id) brings it out)', tostring(u.id), u.type.name);
		end;
	end;
end;

local function Stored(unitId)
	for _,u in ipairs(Sense.stored or {}) do
		if tostring(u.id) == tostring(unitId) then return u; end;
	end;
end;

function Bridge.MyUnit(unitId)
	for _,list in pairs(Sense.my) do
		for _,u in ipairs(list) do if tostring(u.id) == tostring(unitId) then return u; end; end;
	end;
	for _,u in ipairs(Bridge.posted or {}) do if tostring(u.id) == tostring(unitId) then return u; end; end;
end;

-- Brings a stored unit out of its vehicle store (next to the store). True if it came out.
function Bridge.Deploy(unitId)
	local u = Stored(unitId);
	if u == nil then error('unit #'..tostring(unitId)..' is not in a store', 2); end;
	local store = u.storedIn;
	local ok, options = pcall(function() return store.GetStoredDeploymentOptions(u); end);
	if not ok or options == nil or #options == 0 then Bridge.Out('unit #%s cannot leave its store: no free cell beside it', tostring(unitId)); return false; end;
	local ok2, err = pcall(store.DeployStored, u, options[1]);
	Bridge.Out(ok2 and 'unit #%s deployed to cell %s' or 'unit #%s: deploying failed (%s)', tostring(unitId), ok2 and tostring(options[1].cell.index) or tostring(err));
	return ok2;
end;

-- The mechanic goes to the unit and upgrades it (T2; costs composite materials). Both are taken out of
-- their stores if need be and kept from the scripts until it is done or given up (30 rounds).
function Bridge.Upgrade(mechanicId, unitId)
	Bridge.upgrades[#Bridge.upgrades+1] = {mech = tostring(mechanicId), unit = tostring(unitId), since = Sense.round, calls = 0};
end;

function Bridge.RunUpgrades()
	for i = #Bridge.upgrades, 1, -1 do
		local job = Bridge.upgrades[i];
		local done = nil;
		local mech, unit = Bridge.MyUnit(job.mech), Bridge.MyUnit(job.unit);
		if Sense.round - job.since > 30 then done = 'given up after 30 rounds';
		elseif (mech == nil and Stored(job.mech) == nil) or (unit == nil and Stored(job.unit) == nil) then done = 'the mechanic or the unit is gone';
		else
			if mech == nil then pcall(Bridge.Deploy, job.mech); end;
			if unit == nil then pcall(Bridge.Deploy, job.unit); end;
			if mech and unit and mech.location and unit.location then
				local uc = unit.location.cell;
				Bridge.posts[job.unit] = Bridge.posts[job.unit] or {cell = uc.index, since = Sense.round, forUpgrade = true};
				local beside = false;
				for _,n in ipairs(uc.neighbors or {}) do if n.index == mech.location.cell.index then beside = true; break; end; end;
				if beside then
					if mech.Upgrade == nil then done = 'unit #'..job.mech..' cannot upgrade (not a mechanic?)';
					else
						local ok, err = pcall(mech.Upgrade, unit.location);
						job.calls = job.calls + 1;
						if not ok then done = 'refused: '..tostring(err);
						elseif job.calls >= 2 then done = 'ordered (see the unit and the composite materials in the next state)'; end;
					end;
				else
					local p = Bridge.posts[job.mech];
					if p == nil or not p.forUpgrade or p.target ~= uc.index then
						local to = nil;
						for _,n in ipairs(uc.neighbors or {}) do
							if not Sense.structureCells[n.index] and Move.GroundAt(mech, n) ~= nil and to == nil then to = n.index; end;
						end;
						if to == nil then done = 'no free cell beside the unit';
						else Bridge.posts[job.mech] = {cell = to, since = Sense.round, forUpgrade = true, target = uc.index}; end;
					end;
				end;
			end;
		end;
		if done then
			Bridge.results[#Bridge.results+1] = 'Upgrade of unit #'..job.unit..' by mechanic #'..job.mech..': '..done;
			for _,id in ipairs({job.mech, job.unit}) do
				if Bridge.posts[id] and Bridge.posts[id].forUpgrade then Bridge.posts[id] = nil; end;
			end;
			table.remove(Bridge.upgrades, i);
		end;
	end;
end;

-- A unit of mine goes to that cell and stays there, shooting what comes in range, until freed.
-- The scripts leave it alone meanwhile (a posted builder builds nothing, a posted mechanic repairs nothing).
function Bridge.Post(unitId, cell)
	if Stored(unitId) then error('unit #'..tostring(unitId)..' is in a vehicle store: Bridge.Deploy('..tostring(unitId)..') first', 2); end;
	cell = tonumber(cell);
	if cell == nil or Economy.Cell(cell) == nil then error('no such cell: '..tostring(cell), 2); end;
	Bridge.posts[tostring(unitId)] = {cell = cell, since = Sense.round};
end;
function Bridge.Free(unitId) Bridge.posts[tostring(unitId)] = nil; end;

-- Posted units are taken out of the scripts' lists for the round and handled here.
function Bridge.TakePosted()
	Bridge.posted = {};
	if next(Bridge.posts) == nil then return; end;
	local alive = {};
	for role,list in pairs(Sense.my) do
		for i = #list, 1, -1 do
			local u = list[i];
			if Bridge.posts[tostring(u.id)] then
				alive[tostring(u.id)] = true;
				u.bridgeRole = role;
				Bridge.posted[#Bridge.posted+1] = u;
				table.remove(list, i);
			end;
		end;
	end;
	for id,post in pairs(Bridge.posts) do
		if not alive[id] then
			Bridge.results[#Bridge.results+1] = 'Posted unit #'..tostring(id)..' (cell '..post.cell..') is gone: lost, stored or never existed';
			Bridge.posts[id] = nil;
		end;
	end;
end;

local function Dist2(a,b)
	local dx,dy,dz = a.x-b.x, a.y-b.y, a.z-b.z;
	return dx*dx+dy*dy+dz*dz;
end;

function Bridge.RunPosted()
	for _,u in ipairs(Bridge.posted or {}) do
		SafeCall('posted '..tostring(u.niceName), function()
			local post = Bridge.posts[tostring(u.id)];
			local fired = false;
			local inReach = false;
			if post.fire and u.location and u.turret and u.turret.IsInRange then
				local k = Bridge.known[post.fire.cell];
				local loc = (k and k.loc) or Move.GroundAt(u, Economy.Cell(post.fire.cell));
				local okr, hit = pcall(function() return u.turret.IsInRange(loc.world); end);
				inReach = okr and hit == true;
			end;
			-- (fires as soon as the target is in range, on the way to the post too, and says so once it has arrived)
			if post.fire and u.location and (inReach or post.arrived or u.location.cell.index == post.cell) then fired = FireAtCell(u, post);
			-- (what fires uranium, and what has a target of its own, holds its fire for that target:
			-- an EMP posted for a tower spent every 50 uranium on passing vehicles)
			elseif not post.fire and not string.find(u.type.name or '', 'EMP') and not string.find(u.type.name or '', 'Bomb') then fired = Military.Fire(u); end;
			if fired and not u.motionIgnoresAttack then return; end;
			if u.location and u.location.cell.index == post.cell then
				if not post.arrived then
					post.arrived = Sense.round;
					Bridge.results[#Bridge.results+1] = 'Posted unit #'..tostring(u.id)..' ('..u.type.name..') arrived at cell '..post.cell..' in round '..Sense.round;
				end;
				return;
			end;
			post.arrived = nil;
			local cell = Economy.Cell(post.cell);
			local loc = Move.GroundAt(u, cell);
			if loc == nil then
				if not post.told then post.told = true; Bridge.results[#Bridge.results+1] = 'Posted unit #'..tostring(u.id)..': no ground for it on cell '..post.cell; end;
				return;
			end;
			if Move.IsIdle(u) or (Sense.round - post.since) % 5 == 0 then
				-- the whole way at once, through my own units if they stand in it; why not, if not
				local ok, path = pcall(u.FindShortestPath, loc, nil, false);
				if not ok or path == nil or #path == 0 then ok, path = pcall(u.FindShortestPath, loc, nil, true); end;
				-- (the engine can refuse a path the unit itself found, round after round: after 5 rounds
				-- on the spot the unit goes step by step, to the reachable cell nearest the post)
				local stuck = post.at and post.at.cell == u.location.cell.index and Sense.round - post.at.round >= 5;
				if stuck then
					local best, bestD;
					local okR, reach = pcall(u.GetReach, nil, math.max(2, u.movementSpeed), true, false, false, true);
					if okR and reach then
						for _,n in ipairs(reach) do
							local l = n.location;
							if l and not l.isSuborbital and l.cell.index ~= u.location.cell.index and Game.GetMobileAt(l) == nil
								and not Game.Grid.AffectedByEMP(l) then
								local d = Dist2(l.world, loc.world);
								if bestD == nil or d < bestD then best, bestD = l, d; end;
							end;
						end;
					end;
					if best and bestD < Dist2(u.location.world, loc.world) then
						pcall(u.DirectTo, best);
					elseif not post.toldWall then
						post.toldWall = true;
						Bridge.results[#Bridge.results+1] = 'Posted unit #'..tostring(u.id)..' ('..u.type.name..') on cell '..u.location.cell.index..': the engine refuses its path to cell '..post.cell
							..' and no cell within one move lies nearer ('..(okR and reach and #reach or 0)..' cells in reach). Post it by another way, cell by cell.';
					end;
				elseif ok and path and #path > 0 then
					local ok2, err = pcall(u.DirectTo, path[#path]);
					if not ok2 and not post.told then post.told = true; Bridge.results[#Bridge.results+1] = 'Posted unit #'..tostring(u.id)..': the move order was refused: '..tostring(err); end;
				elseif not Move.Towards(u, loc.world, cell, false, true) and not post.told then
					post.told = true;
					Bridge.results[#Bridge.results+1] = 'Posted unit #'..tostring(u.id)..' ('..u.type.name..') finds no path from cell '..u.location.cell.index..' to cell '..post.cell
						..(ok and '' or (' ('..tostring(path)..')'))..': choose a cell it can reach (Bridge.Cells shows ground; heavy bases need level ground or roads)';
				end;
			end;
			post.at = post.at or {cell = u.location.cell.index, round = Sense.round};
			if post.at.cell ~= u.location.cell.index then post.at = {cell = u.location.cell.index, round = Sense.round};
			elseif Sense.round - post.at.round >= 10 and not post.toldStuck then
				post.toldStuck = true;
				Bridge.results[#Bridge.results+1] = 'Posted unit #'..tostring(u.id)..' ('..u.type.name..') has stood on cell '..u.location.cell.index..' for 10 rounds without reaching cell '..post.cell;
			end;
		end);
	end;
end;

-------------------------------------------------------------------------------
-- hooks into the scripts
-------------------------------------------------------------------------------

-- directions: what the outside player pinned comes first and stays
do
	local base = Significance.Direct;
	function Significance.Direct()
		base();
		local have = {};
		local keep = {};
		for _,d in ipairs(Significance.directions) do
			local pin = Bridge.pins[d.region];
			-- (the layer's own direction for a pinned region gives way)
			if pin == nil or pin.kind == d.kind then
				keep[#keep+1] = d;
				have[d.kind..':'..d.region] = d;
			end;
		end;
		for region,pin in pairs(Bridge.pins) do
			if pin.kind == 'avoid' then
				Significance.avoid[region] = 1;
			else
				local d = have[pin.kind..':'..region];
				if d then
					d.priority = pin.priority;
				else
					keep[#keep+1] = {kind = pin.kind, region = region, priority = pin.priority, issued = Sense.round,
						deaths = Map.regions[region].lost or 0, pinned = true};
					Log(string.format('Direction: %s region %d priority %s (round %d) by the outside player', pin.kind, region,
						(string.gsub(string.format('%.2f', pin.priority), ',', '.')), Sense.round));
				end;
				-- a taken region is held from then on
				if pin.kind == 'take' and Sense.mine[region] then Bridge.pins[region] = {kind = 'hold', priority = pin.priority}; end;
			end;
		end;
		Significance.directions = keep;
	end;
end;

do
	local base = Military.UpdatePosture;
	function Military.UpdatePosture()
		if Bridge.posture == nil or Cfg.useFormations then
			base();
			-- (the scripts' own choice never goes where the outside player said to stay out)
			local pin = Mem.attackTarget and Bridge.pins[Mem.attackTarget];
			if pin and pin.kind == 'avoid' then
				Log('Bridge: the scripts wanted to attack region '..Mem.attackTarget..', which is to be avoided: staying home');
				Mem.posture, Mem.attackTarget = 'defend', nil;
			end;
			return;
		end;
		if Bridge.posture == 'defend' then
			Mem.posture, Mem.attackTarget = 'defend', nil;
		else
			Mem.posture, Mem.attackTarget = 'attack', Bridge.posture;
		end;
	end;
end;

do
	local base = Economy.NextStructure;
	function Economy.NextStructure(constructorIndex)
		local me = Sense.my.constructor[constructorIndex];
		local job, at = nil, nil;
		for i,j in ipairs(Bridge.build) do
			-- an exact cell goes to a builder that has ground there and has not failed at it yet
			if j.cell == nil or me == nil or (not (j.failed and j.failed[tostring(me.id)]) and Move.GroundAt(me, Economy.Cell(j.cell)) ~= nil) then
				job, at = j, i; break;
			end;
		end;
		if job then table.remove(Bridge.build, at); end;
		for _,j in ipairs(Bridge.build) do
			j.asked = j.asked or Sense.round;
			if j.cell and Sense.round - j.asked >= 20 and not j.toldWaiting then
				j.toldWaiting = true;
				Bridge.results[#Bridge.results+1] = 'BuildAt '..j.type.name..' at cell '..j.cell..': waiting 20 rounds, no free builder has ground to stand on there (hover builders reach more; Bridge.Units shows the builders)';
			end;
		end;
		if job == nil then
			local want = base(constructorIndex);
			-- (caps hold for whatever the scripts choose, their direct rules included)
			if want and want.type and Economy.Capped(want.type) then return nil; end;
			return want;
		end;
		if job.cell then
			return {type = job.type, kind = 'base', cell = job.cell, done = function(why, u)
				local line = 'BuildAt '..job.type.name..' at cell '..job.cell..': '
					..(why and ('FAILED, '..why) or 'a builder is on its way')..(u and (' ('..tostring(u.niceName)..')') or '');
				if not why and u then Bridge.pending[job.cell] = {job = job, unit = u.id}; end;
				if why and string.find(why, 'step off', 1, true) then
					-- (the cell is being cleared: the same order again, not counted as a failure, 10 times at most)
					job.waits = (job.waits or 0) + 1;
					if job.waits <= 10 then
						Bridge.build[#Bridge.build+1] = job;
						if job.waits > 1 then return; end;
						line = line..'; the order stays';
					end;
				elseif why and u then
					job.failed = job.failed or {};
					job.failed[tostring(u.id)] = true;
					job.tries = (job.tries or 0) + 1;
					if job.tries < 4 then
						Bridge.build[#Bridge.build+1] = job;
						line = line..'; another builder will try';
					end;
				end;
				Log('Bridge: '..line);
				Bridge.results[#Bridge.results+1] = line;
			end};
		end;
		Log('Bridge: builder sent to build '..job.type.name..' near region '..job.region);
		return {type = job.type, kind = job.type.turretType and 'tower' or 'base', region = job.region};
	end;
end;

-------------------------------------------------------------------------------
-- what the faction knows
-------------------------------------------------------------------------------

local function CountByType(list, into, where)
	for _,u in ipairs(list) do
		local name = u.type and u.type.name or '?';
		local reg = u.location and Map.RegionOfLoc(u.location);
		local rid = reg and reg.id or 0;
		into[name] = (into[name] or 0) + 1;
		where[rid] = where[rid] or {};
		where[rid][name] = (where[rid][name] or 0) + 1;
	end;
end;

local function Listing(counts)
	local parts = {};
	for name,n in pairs(counts) do parts[#parts+1] = n..' '..name; end;
	table.sort(parts);
	return table.concat(parts, ', ');
end;

function Bridge.State()
	local L = {};
	local cur = Game.myFaction.currentResources;
	Add(L, 'ROUND %d   faction %s', Sense.round, Bridge.name);
	Add(L, 'minerals %d, energy out %d (spare %d), posture %s%s', cur.storage.minerals or 0,
		(Game.myFaction.maxResources.supply or {}).energy or 0, cur.balance.energy or 0, Mem.posture,
		Mem.attackTarget and (' -> region '..Mem.attackTarget) or '');
	Add(L, 'home region %s; planes about: %s; firing range assumed %.1f cells', tostring(Mem.homeRegion),
		Sense.AirMatters() and 'yes' or 'no', Control.Reach() / Control.Spacing());
	do
		local since = math.min(12, Sense.round - (Bridge.prevAsked or 0));
		local lost, builders = Sense.RecentLosses(math.max(1, since));
		local minerals = cur.storage.minerals or 0;
		Add(L, 'minerals changed by %+d since you were last asked (round %s); lost %d units in the last %d rounds (%d builders)',
			minerals - (Bridge.prevMinerals or minerals), tostring(Bridge.prevAsked or '-'), lost, math.max(1, since), builders);
		Bridge.prevMinerals, Bridge.prevAsked = minerals, Sense.round;
		-- (the faction's maxResources.storage does not cover every network: count the containers)
		local capacity = 'about '..(500 * Economy.Count(Types.mineralContainer))..' (500 per container)';
		Add(L, 'VITAL (the faction dies without a mineral container, a generator, a mineral mine and a fuel mine): mineral containers %d, generators %d, mines %d, builders %d, vehicle plants %d; mineral storage capacity %s',
			Economy.Count(Types.mineralContainer), Economy.Count(Types.generator), Economy.Count(Types.mine), #(Sense.my.constructor or {}),
			Types.plant and Economy.Count(Types.plant) or -1, capacity);
		do
			local bal, sup = cur.balance or {}, Game.myFaction.maxResources.supply or {};
			local parts = {};
			for k,v in pairs(bal) do if k ~= 'minerals' then parts[#parts+1] = string.format('%s %+g (supply %s)', tostring(k), tonumber(v) or 0, tostring(sup[k] or '?')); end; end;
			table.sort(parts);
			Add(L, 'Balance per round: %s. Energy spare 0 means plants, labs and towers wait for power: more generators (and fuel mines to feed them).', table.concat(parts, ', '));
			if #(Sense.off or {}) > 0 then
				local names = {};
				for _,u in ipairs(Sense.off) do names[u.type.name] = (names[u.type.name] or 0) + 1; end;
				Add(L, 'SWITCHED OFF BY THE ENGINE (a balance ran negative: generators off = short of fuel, others off = short of energy; a port off is not controlled): %s', Listing(names));
			end;
			if Economy.siteWhy then Add(L, 'Last building site refused by the scripts because: %s', tostring(Economy.siteWhy)); end;
		end;
		if Economy.Count(Types.mineralContainer) <= 1 then Add(L, 'WARNING: %d mineral container(s). Minerals at 0 with mines standing means the container is gone.', Economy.Count(Types.mineralContainer)); end;
		local can, cannot = {}, {};
		for name,t in pairs(Game.Type.units) do
			if t.isStructure then
				if Types.deprecated[name] then -- (not offered)
				elseif Types.IsUnlocked(t) then can[#can+1] = name; else cannot[#cannot+1] = name; end;
			end;
		end;
		table.sort(can); table.sort(cannot);
		Add(L, 'Can build: %s', table.concat(can, ', '));
		Add(L, 'Not researched yet: %s', #cannot > 0 and table.concat(cannot, ', ') or 'nothing');
		local q = {};
		local ok = pcall(function()
			for _,t in ipairs(Game.Research.myQueue or {}) do q[#q+1] = tostring(t.fullName or t.name or t); end;
		end);
		Add(L, 'Research queue: %s', (ok and #q > 0) and table.concat(q, '; ') or 'empty or unknown');
	end;
	-- mine
	local units, unitsAt, structs, structsAt = {}, {}, {}, {};
	for _,list in pairs(Sense.my) do CountByType(list, units, unitsAt); end;
	CountByType(Sense.myStructures or {}, structs, structsAt);
	Add(L, '');
	Add(L, 'MY UNITS: %s', Listing(units));
	Add(L, 'MY STRUCTURES: %s', Listing(structs));
	-- hostile, as sensed now
	local foes, foesAt = {}, {};
	local seen = {};
	for _,e in pairs(Game.hostileUnits) do
		if e.health and e.health > 0 then seen[#seen+1] = e; end;
	end;
	CountByType(seen, foes, foesAt);
	Add(L, 'HOSTILE IN SIGHT: %s', #seen > 0 and Listing(foes) or 'nothing');
	do
		local rows = {};
		for idx,k in pairs(Bridge.known) do
			rows[#rows+1] = string.format('%s at cell %d (region %s, h=%s, %s r%d)', k.name, idx, tostring(Map.cellRegion[idx] or '?'), tostring(k.loc.height), k.ghost and 'ghost, last seen' or 'seen', k.round);
		end;
		table.sort(rows);
		if #rows > 0 then Add(L, 'HOSTILE STRUCTURES SEEN SO FAR (remembered; may be gone): %s', table.concat(rows, '; ')); end;
	end;
	-- ports: where a structure would have to stand, and what is in the way
	do
		Economy.NextToPort();
		local mine = {};
		for _,u in ipairs(Sense.myStructures or {}) do
			if u.type and u.type.name and string.find(u.type.name, 'Nexus Port', 1, true) and u.location then mine[u.location.cell.index] = true; end;
		end;
		Add(L, '');
		Add(L, 'PORTS AND POLES (a structure of yours on a cell next to one takes it; cells listed as cell:state,');
		Add(L, ' state = ok, emp (EMP zone, cannot be built on), built, unseen, contested):');
		local ids = {};
		for idx,_ in pairs(Economy.portCells or {}) do ids[#ids+1] = idx; end;
		table.sort(ids);
		for _,idx in ipairs(ids) do
			local cell = Economy.portCells[idx];
			local rid = Map.cellRegion[idx];
			local hops = rid and Sense.hops[rid] or 99;
			if mine[idx] or hops <= 6 then
				local parts = {};
				for _,nb in ipairs(cell.neighbors or {}) do
					local state = 'ok';
					local ok, emp = true, Sense.UnderEMP(nb);
					if ok and emp then state = 'emp';
					elseif Sense.structureCells[nb.index] then state = 'built';
					elseif Control.At(nb) == 0 then state = 'unseen';
					elseif Control.hot[nb.index] then state = 'contested'; end;
					parts[#parts+1] = nb.index..':'..state;
				end;
				Add(L, '  cell %d in region %s (%s hops)%s: %s', idx, tostring(rid), tostring(hops), mine[idx] and ' MINE' or '', table.concat(parts, ' '));
			end;
		end;
	end;
	-- needs of the scripts
	local w, parts = Needs.Weights(), {};
	for k,x in pairs(w) do parts[#parts+1] = string.format('%s=%.3g', k, x); end;
	table.sort(parts);
	Add(L, 'Weights: %s', table.concat(parts, ', '));
	parts = {};
	for k,x in pairs(Needs.v or {}) do
		if x > 0 then parts[#parts+1] = string.format('%s %.2f', k, x); end;
	end;
	table.sort(parts);
	Add(L, 'Needs as the scripts see them: %s', table.concat(parts, ', '));
	-- standing orders
	Add(L, '');
	parts = {};
	for region,pin in pairs(Bridge.pins) do parts[#parts+1] = pin.kind..' '..region; end;
	table.sort(parts);
	Add(L, 'YOUR STANDING ORDERS: directions %s; posture %s; build queue %d', #parts > 0 and table.concat(parts, ', ') or 'none',
		Bridge.posture == nil and 'auto' or (Bridge.posture == 'defend' and 'defend' or ('attack region '..Bridge.posture)), #Bridge.build);
	parts = {};
	for _,d in ipairs(Significance.directions) do parts[#parts+1] = string.format('%s %d (%.2f, since r%d)', d.kind, d.region, d.priority, d.issued); end;
	Add(L, 'Active directions: %s', #parts > 0 and table.concat(parts, '; ') or 'none');
	if #Bridge.results > 0 then
		Add(L, 'Since you were last asked:');
		for _,line in ipairs(Bridge.results) do Add(L, '  %s', line); end;
	end;
	Bridge.results = {};
	-- regions
	Add(L, '');
	Add(L, 'REGIONS (only those known or near; hops = regions from what is mine; ctl/cont = share controlled/contested;');
	Add(L, ' res = minerals,fuel 0..1; threat = mobile,static,air; sig = how the strategic layer rates hold,take,danger)');
	local F = Significance.index or {};
	for _,reg in ipairs(Map.regions) do
		local f = Significance.features[reg.id];
		local hops = Sense.hops[reg.id] or 99;
		local port = Significance.ports[reg.id];
		local interesting = hops <= 3 or port or reg.hostileStructures > 0 or reg.mobileThreat > 0 or reg.staticThreat > 0
			or Bridge.pins[reg.id] or foesAt[reg.id];
		if interesting then
			local nb = {};
			for id,_ in pairs(reg.neighbors) do nb[#nb+1] = id; end;
			table.sort(nb);
			local o = Significance.out[reg.id] or {0, 0, 0};
			local emp = 0;
			for _,cell in ipairs(reg.cells) do
				local ok, e = true, Sense.UnderEMP(cell);
				if ok and e then emp = emp + 1; end;
			end;
			local line = string.format('  %3d %s hops %d cells %d nb %s | ctl %.2f cont %.2f%s | res %.2f,%.2f height %.1f | threat %.1f,%.1f,%.1f hostile structures %d, lost here %d%s | sig %.2f,%.2f,%.2f',
				reg.id, Sense.mine[reg.id] and 'MINE' or (reg.seenNow and 'seen' or (reg.lastSeen >= 0 and ('last seen r'..reg.lastSeen) or 'never seen')),
				hops, #reg.cells, table.concat(nb, ','),
				Control.Of(reg.id, 'controlled'), Control.Of(reg.id, 'contested'),
				reg.id == Mem.homeRegion and ' HOME' or '',
				f and f[F.minerals or 1] or 0, f and f[F.fuel or 2] or 0, (Significance.static[reg.id] or {}).height or 0,
				reg.mobileThreat or 0, reg.staticThreat or 0, reg.airThreat or 0, reg.hostileStructures or 0, reg.lost or 0,
				port and (' | NEXUS PORT ('..(port == 'mine' and 'mine' or 'not mine')..')') or '',
				o[1] or 0, o[2] or 0, o[3] or 0);
			if emp > 0 then line = line..string.format(' | EMP ZONE %d%% (nothing can be built or driven there)', math.floor(100 * emp / math.max(1, #reg.cells) + 0.5)); end;
			Add(L, line);
			if structsAt[reg.id] then Add(L, '        my structures: %s', Listing(structsAt[reg.id])); end;
			if unitsAt[reg.id] then Add(L, '        my units: %s', Listing(unitsAt[reg.id])); end;
			if foesAt[reg.id] then Add(L, '        hostile in sight: %s', Listing(foesAt[reg.id])); end;
		end;
	end;
	Add(L, '');
	Add(L, 'Answer by writing orders_%s_%d.lua into this folder (see help_%s.txt). An empty file means carry on.', Bridge.name, Sense.round, Bridge.name);
	return table.concat(L, '\n')..'\n';
end;

function Bridge.Help()
	local L = {};
	Add(L, 'Hexaverse bridge: you decide for faction %s, the player.lua scripts do the handwork.', Bridge.name);
	Add(L, 'Every %d rounds (and when attacked after a quiet spell) state_%s_<round>.txt is written and the game waits', Bridge.every, Bridge.name);
	Add(L, 'up to %d seconds for orders_%s_<round>.lua. Orders are Lua. Helpers:', Bridge.patience, Bridge.name);
	Add(L, '  Bridge.Take(region [, priority])   get this region (build there, drive out who holds it)');
	Add(L, '  Bridge.Hold(region [, priority])   keep and defend this region');
	Add(L, '  Bridge.Avoid(region)   Bridge.Drop(region)');
	Add(L, '  Bridge.Attack(region)  the army goes there now      Bridge.Defend()  stays home      Bridge.Auto()  scripts decide');
	Add(L, '  Bridge.Build(typeName, region [, count])   next free builders build it near that region (within a few moves of the builder).');
	Add(L, '                                             Not for mines: the scripts place those where resources are, in regions you Take.');
	Add(L, '  Bridge.BuildAt(typeName, cell)             the next free builder builds it on exactly that cell (mines too, for a port:');
	Add(L, '                                             see PORTS AND POLES in the state); a failure and its reason come with the next state');
	Add(L, '  Bridge.Research(text)                      research this next (part of a topic name, for example "unlock:artillery")');
	Add(L, '  Structures that are not researched yet are refused with an error (see "Can build" in the state).');
	Add(L, '  Bridge.Weights{army = 3, defense = 6}      how much each need of the scripts counts');
	Add(L, '  Bridge.Set(name, value)   a Cfg setting      Bridge.Every(rounds)      Bridge.Say(text)  your reasoning, kept in the log');
	Add(L, '  Bridge.Out(text)  comes back in result_%s_<round>.txt of the same orders. With it, ask and get the answer at once:', Bridge.name);
	Add(L, '  Bridge.Cells(region)   the region cell by cell (id, ground height, state, what stands there, neighbours), for BuildAt and Post');
	Add(L, '  Bridge.Units([region]) my units one by one (id, type, cell, health)');
	Add(L, '  Bridge.Post(unitId, cell)  that unit goes to the cell and stays there, shooting what comes in range, until Bridge.Free(unitId).');
	Add(L, '                             For eyes on a spot, a guard, an artillery post. The scripts leave a posted unit alone.');
	Add(L, '  Bridge.FireAt(unitId, cell [, rounds])   a posted unit fires at that cell, seen or not (artillery, EMP), for instance at a hostile');
	Add(L, '                             structure listed under HOSTILE STRUCTURES SEEN SO FAR; out of range, no ammunition or fired comes with the next state');
	Add(L, '  Bridge.Reach(unitId [, targetCell])   how many steps that unit reaches from where it stands, and the cells from which it could hit the target cell');
	Add(L, '  Bridge.Plants()   the vehicle plants: on or off, what waits finished inside, deployment options, queue, blocked since');
	Add(L, '  Bridge.Deploy(unitId)   brings a stored unit out of its vehicle store (Bridge.Units() lists the stored ones)');
	Add(L, '  Bridge.Upgrade(mechanicId, unitId)   the mechanic goes to the unit and upgrades it to T2 (composite materials); result in the next state');
	Add(L, '  Bridge.Hostiles()   hostiles in sight one by one with cell and firing range');
	Add(L, '  Bridge.Ring(cell, steps [, true])   the cells that many steps from a cell (true: all up to it), with region, height, state');
	Add(L, '  Bridge.Dist(cellA, cellB)   steps between two cells');
	Add(L, '  Bridge.Cap(typeName, n)    the scripts buy no more of this structure than n (nil lifts it); nothing that draws power is');
	Add(L, '                             bought while energy spare is 0 anyway.   Bridge.Switch(typeName, false [, count])  switches them off');
	Add(L, 'Directions and posture stand until you change them. Anything else is plain Lua with the game API.');
	Add(L, '');
	Add(L, 'How the game is won: hold Nexus ports. A port goes to whoever completes a connected structure (a mine will do) next');
	Add(L, 'to it, and is lost when that structure is destroyed. Structures are only built on ground that is controlled:');
	Add(L, 'seen by sensors, out of firing range of anything unseen or hostile, not overlooked by uncontrolled higher ground.');
	Add(L, '');
	local structs, mobiles = {}, {};
	for name,t in pairs(Game.Type.units) do
		if Types.deprecated[name] then -- (not offered)
		elseif t.isStructure then structs[#structs+1] = name; else mobiles[#mobiles+1] = name; end;
	end;
	table.sort(structs);
	table.sort(mobiles);
	Add(L, 'Structure types: %s', table.concat(structs, ', '));
	Add(L, 'Unit types: %s', table.concat(mobiles, ', '));
	return table.concat(L, '\n')..'\n';
end;

-------------------------------------------------------------------------------
-- the turn of the outside player
-------------------------------------------------------------------------------

function Bridge.Turn()
	if not Bridge.On() then return; end;
	local round = Sense.round;
	if Bridge.lastAsked == round then return; end;
	if Bridge.helpWritten == nil then
		Bridge.helpWritten = true;
		Game.BridgeWrite('help_'..Bridge.name..'.txt', Bridge.Help());
	end;
	local attacked = #(Sense.recentAttacks or {}) > 0;
	local alarm = attacked and round - Bridge.lastAttack > Bridge.alarmAfter and (Bridge.lastAsked == nil or round - Bridge.lastAsked >= Bridge.alarmGap);
	if attacked then Bridge.lastAttack = round; end;
	if alarm then Bridge.results[#Bridge.results+1] = 'ATTACKED this round after a quiet spell (asked at once)'; end;
	if not (alarm or Bridge.lastAsked == nil or round - Bridge.lastAsked >= Bridge.every) then return; end;
	Bridge.lastAsked = round;
	if not Game.BridgeWrite('state_'..Bridge.name..'_'..round..'.txt', Bridge.State()) then
		Log('Bridge: could not write the state for round '..round);
		return;
	end;
	local file = 'orders_'..Bridge.name..'_'..round..'.lua';
	local timer = Game.MeasureSecondsFromNow();
	local text = Game.BridgeRead(file);
	while (text == nil or text == '') and timer() < Bridge.patience do
		Game.BridgeWait(500);
		text = Game.BridgeRead(file);
		-- (an empty orders file means carry on: it is told apart from "not there yet" by its marker)
		if (text == nil or text == '') and Game.BridgeRead('orders_'..Bridge.name..'_'..round..'.none') ~= '' then text = '-- carry on'; end;
	end;
	local result = {};
	if text == nil or text == '' then
		Add(result, 'no orders within %d seconds: carrying on', Bridge.patience);
	else
		Bridge.notes = {};
		Bridge.out = {};
		local chunk, err = load(text, file, 't');
		if chunk == nil then
			Add(result, 'ERROR, nothing done: %s', tostring(err));
		else
			local ok, msg = pcall(chunk);
			Add(result, ok and 'orders run' or ('ERROR while running (what came before it stands): '..tostring(msg)));
			-- directions take effect this round
			SafeCall('Directions', Significance.Direct);
		end;
		Log('Bridge orders r='..round..': '..(string.gsub(text, '%s+', ' ')));
		for _,note in ipairs(Bridge.notes) do Log('Bridge says r='..round..': '..note); end;
	end;
	Add(result, 'waited %.0f seconds', timer());
	for _,line in ipairs(Bridge.out or {}) do result[#result+1] = line; end;
	Bridge.out = nil;
	Log('Bridge: round '..round..': '..result[1]);
	Game.BridgeWrite('result_'..Bridge.name..'_'..round..'.txt', table.concat(result, '\n')..'\n');
end;

-- asked after the picture of the round is complete and before anything acts on it
do
	local base = Needs.Update;
	function Needs.Update()
		SafeCall('Bridge', Bridge.Turn);
		SafeCall('BridgeRemember', Bridge.Remember);
		SafeCall('BridgeUpgrades', Bridge.RunUpgrades);
		SafeCall('BridgePosts', Bridge.TakePosted);
		return base();
	end;
end;

do
	local base = Military.Combat;
	function Military.Combat()
		SafeCall('BridgePosted', Bridge.RunPosted);
		return base();
	end;
end;

-- what happened to directions is told with the next state
do
	local base = Log;
	function Log(msg)
		if type(msg) == 'string' and (string.find(msg, 'Direction result:', 1, true) or string.find(msg, 'Port mine ordered', 1, true)
				or string.find(msg, 'Attacked ', 1, true) == 1 or string.find(msg, 'BuildStructure failed', 1, true)
				or string.find(msg, 'did not happen', 1, true) or string.find(msg, 'no path to the', 1, true)
				or string.find(msg, 'cannot deploy', 1, true) or string.find(msg, 'is stuck. Aborting', 1, true)
				or string.find(msg, 'came to nothing', 1, true) or string.find(msg, 'no site', 1, true) or string.find(msg, 'No mine', 1, true)) and #Bridge.results < 30 then
			Bridge.results[#Bridge.results+1] = msg;
		end;
		return base(msg);
	end;
end;

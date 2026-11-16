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
			if pcall(st.Enable, on) then n = n + 1; end;
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
end;

-- A unit of mine goes to that cell and stays there, shooting what comes in range, until freed.
-- The scripts leave it alone meanwhile (a posted builder builds nothing, a posted mechanic repairs nothing).
function Bridge.Post(unitId, cell)
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

function Bridge.RunPosted()
	for _,u in ipairs(Bridge.posted or {}) do
		SafeCall('posted '..tostring(u.niceName), function()
			local post = Bridge.posts[tostring(u.id)];
			local fired = false;
			fired = Military.Fire(u);
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
			if Move.IsIdle(u) or (Sense.round - post.since) % 5 == 0 then Move.Towards(u, loc.world, cell, false, true); end;
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
		if job == nil then
			local want = base(constructorIndex);
			-- (caps hold for whatever the scripts choose, their direct rules included)
			local cap = want and want.type and Cfg.caps[want.type.name];
			if cap and Economy.Count(want.type) >= cap then return nil; end;
			return want;
		end;
		if job.cell then
			return {type = job.type, kind = 'base', cell = job.cell, done = function(why, u)
				local line = 'BuildAt '..job.type.name..' at cell '..job.cell..': '
					..(why and ('FAILED, '..why) or 'a builder is on its way')..(u and (' ('..tostring(u.niceName)..')') or '');
				if why and u then
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
				or string.find(msg, 'came to nothing', 1, true) or string.find(msg, 'no site', 1, true) or string.find(msg, 'No mine', 1, true)) and #Bridge.results < 30 then
			Bridge.results[#Bridge.results+1] = msg;
		end;
		return base(msg);
	end;
end;

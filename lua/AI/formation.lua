--[[
Formations: the layer between strategy and units
================================================

The strategic layer says what it wants (significance.lua: take region 12, hold region 3).
A formation takes over one such direction and sees it through: it claims units for it,
moves them as a group, and reports how it went. Units that belong to no formation stay
with the home defence.

A formation that takes a region goes through stages:
  gather    members collect in a staging region next to the target, outside its reach
  advance   they go in together
  secure    the region is clear (seen, nothing hostile left): they stay on it, so that
            builders can follow. After Formation.secureRounds the direction counts as
            achieved, whether or not something is built there yet
It breaks off (back to gather) when too few are left, and ends with its direction:
achieved, or failed when the direction runs out.

A formation that holds a region is a garrison: a few units that stay there.

What it logs is what a formation is judged by, and what a learned one is trained on later,
with the strategic layer held fixed:
  Formation 4 (take region 12): created, wants 6 units, staging region 9
  Formation 4 (take region 12): gather -> advance at round 84 with 5 of 6
  Formation result: 4 take region 12: achieved at round 131 after 61 rounds, peak 7 units, lost 2
The decisions worth learning are few: how many units to commit (Formation.Want), where to
gather (Formation.Staging), when to go in (Formation.Ready) and when to break off
(Formation.Broken). Each is one function, so each can be replaced on its own.

Active only with Cfg.useFormations (which needs the directions of significance.lua).
]]

Formation = {
	list = {},			-- active formations
	of = {},			-- unit id -> its formation
	nextId = 1,
	maxTake = 2,		-- formations taking regions at the same time
	homeReserve = 2,	-- combat units never claimed
	gatherShare = 0.8,	-- share of the wanted units that must have arrived to go in
	gatherRounds = 25,	-- after this long, go in with less (Formation.lateShare)
	lateShare = 0.6,
	breakShare = 0.4,	-- broken off below this share of its peak size
	secureRounds = 5,	-- rounds on a cleared region until it counts as taken
	garrison = 2,		-- units of a holding formation
	commitShare = 0.5,	-- share of the free units a taking formation asks for (if that is more than the job needs)
	maxSize = 30,
	stallRounds = 30,	-- gathered units not arriving, or advancing without anyone getting in, for this long: start over
	restRounds = 10,	-- after starting over, no units are claimed for this long
	reportEvery = 10,	-- rounds between status lines
};

local function Dist2(a, b)
	local dx, dy, dz = a.x-b.x, a.y-b.y, a.z-b.z;
	return dx*dx + dy*dy + dz*dz;
end;

local function Name(f)
	return 'Formation '..f.id..' ('..f.kind..' region '..f.region..')';
end;

-- how many units to commit: what the job needs at least, and a good part of what stands idle
function Formation.Want(kind, regionId, available)
	if kind == 'hold' then return Formation.garrison; end;
	local idle = math.min(Formation.maxSize, math.floor((available or 0) * Formation.commitShare));
	local need = Military.RequiredForce(regionId);
	-- (a region with someone else's port is a base: what is sensed of it from afar is a fraction.
	-- Five units were asked for the Nexus port region; they cleared it for one round. Everything
	-- that can be spared goes, and never fewer than twice the count)
	if Significance.ports and Significance.ports[regionId] and Significance.ports[regionId] ~= 'mine' then
		return math.max(need * 2, math.min(Formation.maxSize, available or 0));
	end;
	return math.max(need, idle);
end;

-- where to collect: the quiet neighbour of the target that is closest to what I have
Formation.minSize = {};	-- target region -> units wanted at least before going in (Bridge.Take)
Formation.stageAt = {};	-- target region -> staging region chosen from outside (Bridge.Stage)
Formation.badStage = {};	-- region -> round it cost a gathering formation four members: no staging there for 150 rounds
function Formation.Staging(regionId)
	local function Bad(id) return Formation.badStage[id] and Sense.round - Formation.badStage[id] < 150; end;
	if Formation.stageAt[regionId] and not Bad(Formation.stageAt[regionId]) then return Formation.stageAt[regionId]; end;
	local best, bestH = nil, nil;
	for nb,_ in pairs(Map.regions[regionId].neighbors) do
		local reg = Map.regions[nb];
		local h = Sense.hops[nb] or 99;
		if not Bad(nb) and Sense.Threat(reg) <= 0 and Significance.Directed(nb, 'avoid') <= 0 and Map.EmpShare(nb) < 0.5 and (bestH == nil or h < bestH) then	-- (never in an EMP zone: nobody gets there)
			best, bestH = nb, h;
		end;
	end;
	return best or Mem.homeRegion or regionId;
end;

-- enough of them together to go in?
function Formation.Ready(f, arrived)
	local share = (Sense.round - f.stageSince >= Formation.gatherRounds) and Formation.lateShare or Formation.gatherShare;
	local least = f.kind == 'take' and Formation.minSize[f.region];
	if least and arrived < math.ceil(least * Formation.gatherShare) then return false; end;
	return arrived >= math.max(2, math.ceil(f.want * share));
end;

-- too few left to go on?
function Formation.Broken(f, count)
	return f.peak >= 3 and count < math.max(2, math.ceil(f.peak * Formation.breakShare));
end;

local function SetStage(f, stage, note)
	Log(Name(f)..': '..f.stage..' -> '..stage..' at round '..Sense.round..(note and (' '..note) or ''));
	f.stage = stage;
	f.stageSince = Sense.round;
end;

local function Finish(f, how)
	Log(string.format('Formation result: %d %s region %d: %s at round %d after %d rounds, peak %d units, lost %d',
		f.id, f.kind, f.region, how, Sense.round, Sense.round - f.born, f.peak, f.lost));
	for id,_ in pairs(f.members) do Formation.of[id] = nil; end;
	f.members = {};
	f.over = true;
end;

function Formation.Update()
	if not Cfg.useFormations then return; end;
	local alive = {};
	for _,u in ipairs(Sense.my.combat) do alive[u.id] = u; end;
	-- (members parked in a vehicle store while their formation gathers are not lost)
	local parked = {};
	for _,u in ipairs(Sense.stored or {}) do
		if Formation.of[u.id] then alive[u.id] = u; parked[u.id] = true; end;
	end;

	-- 1. formations follow the directions: end what is no longer asked for, start what is new
	local asked = {};
	for _,d in ipairs(Significance.directions) do
		asked[d.kind..':'..d.region] = d;
	end;
	local keep, have, takes = {}, {}, 0;
	for _,f in ipairs(Formation.list) do
		local d = asked[f.kind..':'..f.region];
		if d == nil or d.issued ~= f.issued then
			-- the direction was settled: by its own rules, or by this formation reporting the region taken
			local mine = Sense.mine[f.region] and true or false;
			local won = (f.kind == 'take' and (mine or f.taken)) or (f.kind == 'hold' and mine);
			Finish(f, won and 'achieved' or 'failed');
		else
			keep[#keep+1] = f;
			have[f.kind..':'..f.region] = true;
			if f.kind == 'take' then takes = takes + 1; end;
		end;
	end;
	Formation.list = keep;
	for _,kind in ipairs({'take', 'hold'}) do
		for _,d in ipairs(Significance.Directions(kind)) do
			if not have[kind..':'..d.region] and (kind ~= 'take' or takes < Formation.maxTake) then
				local f = {
					id = Formation.nextId, kind = kind, region = d.region, issued = d.issued, priority = d.priority, pinned = d.pinned,
					members = {}, want = Formation.Want(kind, d.region), peak = 0, lost = 0,
					stage = kind == 'take' and 'gather' or 'garrison', born = Sense.round, stageSince = Sense.round,
				};
				Formation.nextId = Formation.nextId + 1;
				f.staging = kind == 'take' and Formation.Staging(d.region) or d.region;
				Formation.list[#Formation.list+1] = f;
				if kind == 'take' then takes = takes + 1; end;
				Log(Name(f)..': created, wants '..f.want..' units, staging region '..f.staging);
			end;
		end;
	end;

	-- 2. members: forget the dead, claim free units (what matters most claims first)
	table.sort(Formation.list, function(a, b)
		if a.kind ~= b.kind then return a.kind == 'take'; end;
		if a.priority ~= b.priority then return a.priority > b.priority; end;
		return a.id < b.id;
	end);
	local free = {};
	for _,f in ipairs(Formation.list) do
		for id,_ in pairs(f.members) do
			if alive[id] == nil then
				f.members[id] = nil;
				Formation.of[id] = nil;
				f.lost = f.lost + 1;
				if f.stage == 'gather' then f.lostGather = (f.lostGather or 0) + 1; end;
			end;
		end;
	end;
	for id,_ in pairs(Formation.of) do
		if alive[id] == nil then Formation.of[id] = nil; end;
	end;
	for _,u in ipairs(Sense.my.combat) do
		if Formation.of[u.id] == nil and not Military.stashing[u.id] and not Military.peakGuard[u.id] then
			free[#free+1] = u;
		end;
	end;
	-- what the pressure at home needs stays out of it
	local spare = #free - Formation.homeReserve - math.ceil(Sense.Pressure() / Military.UnitPotential());
	Formation.short = nil;
	for _,f in ipairs(Formation.list) do
		local count = 0;
		for _ in pairs(f.members) do count = count + 1; end;
		-- while gathering, the size follows what is available
		if f.stage == 'gather' then f.want = math.max(count, Formation.Want('take', f.region, spare + count)); end;
		-- (a take the outside player ordered gets all but the home reserve, whatever the pressure
		-- at home: 9 of 47 units went for the port, the pressure rule kept the rest, and they
		-- died at home's edge all the same)
		local spareHere = spare;
		if f.pinned and f.kind == 'take' then
			spareHere = #free - Formation.homeReserve;
			if f.stage == 'gather' then f.want = math.max(f.want, math.min(Formation.maxSize, count + math.max(0, spareHere))); end;
		end;
		if f.restUntil and Sense.round < f.restUntil then
			-- just started over
		elseif count < f.want and f.stage ~= 'secure' then
			-- nearest first
			local goal = Map.WorldCenter(Map.regions[f.staging]);
			table.sort(free, function(a, b) return Dist2(a.location.world, goal) < Dist2(b.location.world, goal) end);
			local i = 1;
			while count < f.want and spareHere > 0 and i <= #free do
				local u = free[i];
				if Formation.of[u.id] == nil then
					f.members[u.id] = true;
					Formation.of[u.id] = f;
					count = count + 1;
					spare = spare - 1;
					spareHere = spareHere - 1;
				end;
				i = i + 1;
			end;
			local rest = {};
			for _,u in ipairs(free) do
				if Formation.of[u.id] == nil then rest[#rest+1] = u; end;
			end;
			free = rest;
			-- still short: units waiting in stores can fill it (Military.Stores reads this)
			if count < f.want and Formation.short == nil then
				Formation.short = {region = f.staging, count = f.want - count};
			end;
		end;
		f.count = count;
		if count > f.peak then f.peak = count; end;
	end;

	-- 3. stages
	for _,f in ipairs(Formation.list) do
		if f.kind == 'take' then
			local target = Map.regions[f.region];
			local near, inside, at = 0, 0, {};
			-- (gathered means in the staging region itself: counting its neighbours too, a formation
			-- "gathered" in the round it was created, its six members in six regions)
			local around = {[f.staging] = true};
			-- (gathering on my own ground: the members stay what they were, the defence of it, and
			-- count wherever they stand on it. Called to one cell among the base's buildings they
			-- found no path, stood apart and were picked off by the raids, 23 of them)
			f.atHome = (Sense.mine[f.staging] or f.staging == Mem.homeRegion) and true or false;
			if f.atHome then
				for id,_ in pairs(Sense.mine) do around[id] = true; end;
			end;
			for id,_ in pairs(f.members) do
				local m = alive[id];
				local reg = Map.RegionOfLoc(m.location or (m.storedIn and m.storedIn.location));
				if reg and reg.id == f.region then inside = inside + 1; end;
				local k = reg and reg.id or -1;
				at[k] = (at[k] or 0) + 1;
				if reg and (around[reg.id] or reg.id == f.region) then near = near + 1; end;
			end;
			if inside > 0 then f.lastInside = Sense.round; end;
			f.near = near;
			-- (where to gather: beside a store of the staging region if one stands, else on the cell
			-- of it that lies farthest from the target. Spread over all its cells, some stood in
			-- the reach of the target's towers)
			if f.atHome then f.rally = nil; end;
			if f.stage == 'gather' and not f.atHome and (f.rally == nil or f.rallyFor ~= f.staging or Sense.round % 10 == 0) then
				f.rallyFor = f.staging;
				f.rally = nil;
				local tc = Map.WorldCenter(target);
				local bestD = nil;
				for _,st in ipairs(Sense.stores or {}) do
					local sr = Map.RegionOfLoc(st.location);
					if sr and sr.id == f.staging then
						for _,nb in ipairs(st.location.cell.neighbors or {}) do
							local w = {x = nb.center.x * Map.groundRadius, y = nb.center.y * Map.groundRadius, z = nb.center.z * Map.groundRadius};
							local dd = Dist2(w, tc);
							if not Sense.structureCells[nb.index] and (bestD == nil or dd > bestD) then bestD, f.rally = dd, nb; end;
						end;
					end;
				end;
				if f.rally == nil then
					for _,c in ipairs(Map.regions[f.staging].cells or {}) do
						local w = {x = c.center.x * Map.groundRadius, y = c.center.y * Map.groundRadius, z = c.center.z * Map.groundRadius};
						local dd = Dist2(w, tc);
						if not Sense.structureCells[c.index] and not Sense.UnderEMP(c) and (bestD == nil or dd > bestD) then bestD, f.rally = dd, c; end;
					end;
				end;
			end;
			f.parked = 0;
			for id,_ in pairs(f.members) do
				if parked[id] then f.parked = f.parked + 1; end;
			end;
			if f.stage ~= 'gather' then f.unload = nil; end;
			-- (one way in for all: the cell of the target region nearest to where they gathered.
			-- Each going for a cell of its own, half the army came in from the far side, alone)
			if f.entryFor ~= f.staging then
				f.entryFor = f.staging;
				f.entry = nil;
				local from = Map.WorldCenter(Map.regions[f.staging]);
				local bestD = nil;
				for _,c in ipairs(target.cells or {}) do
					local w = {x = c.center.x * Map.groundRadius, y = c.center.y * Map.groundRadius, z = c.center.z * Map.groundRadius};
					local dd = Dist2(w, from);
					if bestD == nil or dd < bestD then bestD, f.entry = dd, c; end;
				end;
			end;
			local clear = target.seenNow and target.hostileStructures == 0 and target.mobileThreat == 0 and target.staticThreat == 0;
			if f.stage ~= 'gather' and Formation.Broken(f, f.count) then
				f.want = math.max(f.want, Formation.Want('take', f.region, 0));
				f.staging = Formation.Staging(f.region);
				SetStage(f, 'gather', 'broken off with '..f.count..' of '..f.peak..' left; now wants '..f.want);
				f.peak = f.count;
			elseif f.stage == 'gather' then
				-- (a staging ground that eats those who gather there is none: the neighbour of the
				-- target chosen for one match was the road of the enemy's raids, and 36 tanks and six
				-- stores were lost on it, one by one. Back to my own ground, and all go out together)
				if (f.lostGather or 0) >= 4 and f.staging ~= Mem.homeRegion and not Sense.mine[f.staging] then
					Formation.badStage[f.staging] = Sense.round;
					local was = f.staging;
					f.staging = Formation.Staging(f.region);
					if Formation.badStage[f.staging] or f.staging == was then f.staging = Mem.homeRegion or f.staging; end;
					f.lostGather = 0;
					f.rally = nil;
					Log(Name(f)..': four members lost gathering at region '..was..', staging moves to region '..f.staging..' at round '..Sense.round);
				elseif (f.lostGather or 0) >= 4 then
					f.lostGather = 0;
				end;
				if Formation.minSize[f.region] then f.want = math.max(f.want, Formation.minSize[f.region]); f.stageSince = math.max(f.stageSince, Sense.round - Formation.stallRounds + 1); end;
				if Formation.Ready(f, near) and f.parked > 0 then
					-- (all out first, then all go: Military.Stores empties the stores, six a store a round)
					if not f.unload then Log(Name(f)..': ready with '..near..' of '..f.want..', '..f.parked..' come out of the stores at round '..Sense.round); end;
					f.unload = true;
				elseif Formation.Ready(f, near) then
					SetStage(f, 'advance', 'with '..near..' of '..f.want);
					f.lastInside = Sense.round;
				elseif Sense.round - f.stageSince >= Formation.stallRounds and f.count >= math.max(2, math.ceil(f.want * Formation.lateShare)) then
					-- (with too few members it is waiting for units, which is no fault of theirs)
					f.gaveUp = 'never gathered: '..near..' near of '..f.count..' members, wanted '..f.want;
				end;
			elseif f.stage == 'advance' then
				if inside > 0 and clear then
					SetStage(f, 'secure', 'with '..f.count..' units');
				elseif Sense.round - (f.lastInside or f.stageSince) >= Formation.stallRounds then
					f.gaveUp = 'nobody reached the region: '..near..' near of '..f.count..' members';
				end;
			elseif f.stage == 'secure' then
				if not clear then
					-- (a region that is cleared and contested again and again is under fire from
					-- outside it: two formations went round this loop 13 times and bled out. After
					-- the third time it is given up for 40 rounds)
					f.flips = (f.flips or 0) + 1;
					if f.flips >= 3 then
						f.gaveUp = 'contested again '..f.flips..' times after being cleared';
						f.longRest = true;
					end;
					SetStage(f, 'advance', 'region contested again');
				elseif Sense.round - f.stageSince >= Formation.secureRounds and not f.taken then
					f.taken = true;
					Significance.taken[f.region] = Sense.round;	-- settles the direction
				end;
			end;
			if f.gaveUp then
				-- the units go home and it starts over, with others and maybe from elsewhere; the direction stays
				Log(Name(f)..': given up at round '..Sense.round..' in '..f.stage..' ('..f.gaveUp..')');
				for id,_ in pairs(f.members) do Formation.of[id] = nil; end;
				f.members = {};
				f.count, f.peak, f.gaveUp = 0, 0, nil;
				f.stage, f.stageSince, f.lastInside = 'gather', Sense.round, nil;
				f.staging = Formation.Staging(f.region);
				f.restUntil = Sense.round + (f.longRest and 40 or Formation.restRounds);
				f.longRest, f.flips = nil, nil;
				f.restarts = (f.restarts or 0) + 1;
			elseif (Sense.round - f.born) % Formation.reportEvery == 0 and Sense.round > f.born then
				local where = {};
				for id,n in pairs(at) do where[#where+1] = id..'x'..n; end;
				table.sort(where);
				Log(string.format('%s: status round %d stage %s members %d of %d near %d inside %d seen %s structures %d mobile %.1f static %.1f staging %d at %s',
					Name(f), Sense.round, f.stage, f.count, f.want, near, inside, tostring(target.seenNow and true or false),
					target.hostileStructures or 0, target.mobileThreat or 0, target.staticThreat or 0, f.staging, table.concat(where, ' ')..(f.rally and (' rally cell '..f.rally.index) or '')..' parked '..tostring(f.parked or 0)));
			end;
		end;
	end;
end;

--[[
For Military.Combat: where a member goes this round.
Returns region id, whether it may go into danger, and the region for stationary shooters
(artillery keeps to the staging region while the others go in).
]]
function Formation.Orders(v)
	local f = Cfg.useFormations and Formation.of[v.id];
	if not f or f.over then return nil; end;
	if f.kind == 'hold' then return f.region, false, f.region; end;
	if f.stage == 'gather' and f.atHome then return nil; end;
	if f.stage == 'gather' then return f.staging, false, f.staging, nil, f.rally; end;
	return f.region, true, (f.stage == 'secure') and f.region or f.staging, (f.stage == 'advance') and f.entry or nil;
end;

-- The take formation whose members wait in stores for the word (nil: the unit is free to be deployed)
function Formation.Parks(u)
	local f = Cfg.useFormations and Formation.of[u.id];
	if f and not f.over and f.kind == 'take' and f.stage == 'gather' and not f.unload then return f; end;
	return nil;
end;

-- How far a take of that region has gathered (0..1), 1 once it has set out; nil if there is none
function Formation.Gathered(regionId)
	for _,f in ipairs(Formation.list) do
		if not f.over and f.kind == 'take' and f.region == regionId then
			if f.stage ~= 'gather' then return 1; end;
			return (f.near or 0) / math.max(1, f.want);
		end;
	end;
	return nil;
end;

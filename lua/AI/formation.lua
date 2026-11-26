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
-- Can a combat vehicle of mine drive to that region? (A tower of mine made region 68 "mine" in
-- match 176; the take staged there twice and no member had a path to its rally, even through my own)
Formation.reach = {round = -1, at = {}};
function Formation.Reachable(id)
	if id == Mem.homeRegion or Formation.stageReach == false then return true; end;
	if Formation.reach.round ~= Sense.round then Formation.reach = {round = Sense.round, at = {}}; end;
	local r = Formation.reach.at[id];
	if r ~= nil then return r; end;
	r = true;
	local reg = Map.regions[id];
	local probe = nil;
	for _,m in ipairs(Sense.my.combat or {}) do
		if m.location and not m.storedIn and not m.location.isSuborbital and m.FindShortestPath then
			local mr = Map.RegionOfLoc(m.location);
			if mr and mr.id == id then probe = nil; r = true; Formation.reach.at[id] = true; return true; end;
			if probe == nil or string.find(tostring(m.type and m.type.name), 'Heavy', 1, true) then probe = m; end;
		end;
	end;
	if probe and reg and reg.centerCell and Move and Move.GroundAt then
		r = false;
		local tries = {reg.centerCell};
		for _,nb in ipairs(reg.centerCell.neighbors or {}) do tries[#tries+1] = nb; end;
		for _,c in ipairs(tries) do
			local dest = Move.GroundAt(probe, c);
			if dest then
				local ok, path = pcall(probe.FindShortestPath, dest, nil, true);
				if ok and path and #path > 0 then r = true; break; end;
			end;
		end;
	end;
	Formation.reach.at[id] = r;
	return r;
end;

function Formation.Staging(regionId, keep)
	-- (nor where vehicles of mine fell lately: in match 173 the take staged in the region before the
	-- Nexus port region, where a store of mine stood and the army had been dying all match, and
	-- gathered there for 450 rounds with 4 of 30 near)
	if Formation.hotRound ~= Sense.round then
		Formation.hotRound, Formation.hot = Sense.round, {};
		for idx,d in pairs(Move and Move.deaths or {}) do
			if Sense.round - d.round < 150 and d.n >= 2 then
				local rid = Map.cellRegion and Map.cellRegion[idx];
				if rid then Formation.hot[rid] = (Formation.hot[rid] or 0) + 1; end;
			end;
		end;
	end;
	local function Bad(id)
		if id ~= Mem.homeRegion and (Formation.hot[id] or 0) >= (Formation.hotCells or 3) then return true; end;
		return Formation.badStage[id] and Sense.round - Formation.badStage[id] < 150;
	end;
	if Formation.stageAt[regionId] and not Bad(Formation.stageAt[regionId]) then return Formation.stageAt[regionId]; end;
	-- (where the army is gathering it stays, unless that ground has gone bad: in match 182 the
	-- staging moved 17 times, every 30 rounds to whichever region was nearer just then, and the take
	-- gathered for 280 rounds without going)
	if keep and (keep == Mem.homeRegion or Sense.mine[keep]) and not Bad(keep) and Map.EmpShare(keep) < 0.5 and Formation.Reachable(keep) then return keep; end;
	--[[
	First choice: the region of mine nearest to the target by the way the army would take. A
	neighbour of the target that shows no threat is no place to gather: in match 172 the take staged
	in the region beside the Nexus port region, under towers nobody had seen.
	]]
	if Formation.stageOwn ~= false and Map.regions[regionId] then
		local dist, done = {[regionId] = 0}, {};
		local own, ownD = nil, nil;
		while true do
			local at, atD = nil, nil;
			for id,d in pairs(dist) do
				if not done[id] and (atD == nil or d < atD) then at, atD = id, d; end;
			end;
			if at == nil or (ownD and atD > ownD) then break; end;
			done[at] = true;
			if at ~= regionId and Sense.mine[at] and not Bad(at) and Map.EmpShare(at) < 0.5 and (ownD == nil or atD < ownD) and Formation.Reachable(at) then own, ownD = at, atD; end;
			for nb,_ in pairs(Map.regions[at].neighbors or {}) do
				local r = Map.regions[nb];
				if r then
					local step = 1 + ((Map.EmpShare(nb) >= 0.5) and 1000 or 0) + (((r.staticThreat or 0) > 0) and 1.5 or 0)
						+ ((Significance.Directed(nb, 'avoid') > 0) and 3 or 0);
					if dist[nb] == nil or atD + step < dist[nb] then dist[nb] = atD + step; end;
				end;
			end;
		end;
		if own and ownD < 1000 then return own; end;
	end;
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
	-- (not without its eye below ground: in match 183 seven advances entered the Nexus port region
	-- and none had a surveyor with it; the buried towers at the entry, never sensed, broke each one
	-- within 16 rounds. It waits eyeWait rounds for one, not for ever.)
	if f.kind == 'take' and Military and Military.EyeWith and Sense.round - (f.gatherSince or Sense.round) <= (Formation.eyeWait or 120)
		and not Military.EyeWith(f) then
		if Sense.round % 20 == 0 then Log(Name(f)..': waits for a surveyor to go with it'); end;
		return false;
	end;
	--[[
	Not against towers without guns. The towers round the Nexus port region outrange everything and
	were what broke the advances: in match 208 73 of 103 vehicles lost before round 600 were take
	members on the march, and match 207 sent 23 hover tanks with no artillery at all (17 lost in 14
	rounds). Every take that held the region had 9 to 18 artillery pieces with it. It waits for
	takeGuns of them, gunWait rounds at most.
	]]
	if f.kind == 'take' and Cfg.takeGuns ~= 0 then
		local reg = Map.regions[f.region];
		if reg and ((reg.staticThreat or 0) > 0 or (reg.hostileStructures or 0) > 0 or (f.lost or 0) >= 5) then
			local guns = 0;
			for _,u in ipairs(Sense.my.combat or {}) do
				-- (the artillery turret, not the trajectory: cannons shoot in an arc too and were all counted)
				local tt = u.turret and u.turret.type;
				if Formation.of[u.id] == f and tt and (string.find(tostring(tt.name), 'Artillery', 1, true) or string.find(tostring(u.type and u.type.name), 'Artillery', 1, true)) then guns = guns + 1; end;
			end;
			f.gunSince = f.gunSince or Sense.round;
			if guns < (Cfg.takeGuns or 6) and Sense.round - f.gunSince <= (Formation.gunWait or 300) then
				Formation.needGuns = Sense.round;
				if Sense.round % 20 == 0 then Log(Name(f)..': waits for artillery to go with it ('..guns..' of '..(Cfg.takeGuns or 6)..', '..(Sense.round - f.gunSince)..' rounds)'); end;
				return false;
			end;
		end;
	end;
	-- (gathering for gatherForce rounds and still not together: it goes from where its members stand,
	-- the legs of the march bring them together. In match 194 the towers of the Nexus port region
	-- were gone by round 504 and the take then gathered for 450 rounds, 30 of 30 members, 4 to 8 at
	-- the rally, the rest held in the crowd at home or standing further forward than the rally)
	if f.kind == 'take' and Sense.round - (f.gatherSince or Sense.round) > (Formation.gatherForce or 250) and (f.count or 0) >= (Formation.goFloor or 12) then
		if f.forcedTold ~= f.gatherSince then
			f.forcedTold = f.gatherSince;
			Log(Name(f)..': gathered for '..(Sense.round - f.gatherSince)..' rounds with '..tostring(arrived)..' at the rally: it goes from where its members stand');
		end;
		return true;
	end;
	local share = (Sense.round - f.stageSince >= Formation.gatherRounds) and Formation.lateShare or Formation.gatherShare;
	local least = f.kind == 'take' and Formation.minSize[f.region];
	-- (gathering for longer than gatherMax: what there is has to do, see the want)
	if least and Sense.round - (f.gatherSince or Sense.round) > (Formation.gatherMax or 150) then least = math.min(least, math.max(f.count or least, Formation.goFloor or 12)); end;	-- (but not three or four units: such advances in match 149 only fed the towers)
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
	if stage == 'gather' then f.gatherSince = Sense.round; f.letTotal = 0; f.gunSince = nil; end;
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
	-- (who finds no way for 30 rounds while the others march is let go: it stays what it was at
	-- home. Counted on, nine vehicles that never left home made the others wait 90 rounds a leg
	-- in the enemy's fire, "5 of 14 up", and were then all that was left to go "on to region 1
	-- with 0 of 9 up", advance after advance)
	Formation.stuckOut = Formation.stuckOut or {};
	local function Stuck(id)
		local ps = Passage and Passage.stuck and Passage.stuck[tostring(id)];
		return ps ~= nil and ps.n >= (Formation.stuckNotes or 6);
	end;
	for _,f in ipairs(Formation.list) do
		if f.kind == 'take' and f.stage ~= 'gather' then
			local let = 0;
			-- (only a few among many: when most of them find no way, the way is what is missing,
			-- not the vehicles. 13 hovers of 15 were let go in one round and the advance went
			-- "on to region 1 with 0 of 0 up")
			local all, stuckN = 0, 0;
			for id,_ in pairs(f.members) do
				all = all + 1;
				if Stuck(id) then stuckN = stuckN + 1; end;
			end;
			-- (and over the whole advance, not round by round: 12 of 18 went three at a time)
			local gone = f.letTotal or 0;
			local few = (stuckN + gone) * 3 <= all + gone;
			if stuckN > 0 and not few and Sense.round % 20 == 0 then
				Log(Name(f)..': '..stuckN..' of '..all..' find no way at round '..Sense.round..': kept, the way is what is missing');
			end;
			for id,_ in pairs(f.members) do
				if few and Stuck(id) then
					f.members[id] = nil;
					Formation.of[id] = nil;
					Formation.stuckOut[id] = Sense.round;
					f.letTotal = (f.letTotal or 0) + 1;
					let = let + 1;
				end;
			end;
			if let > 0 then
				f.peak = math.max(0, (f.peak or 0) - let);
				Log(Name(f)..': '..let..' let go at round '..Sense.round..', no way from where they stand');
			end;
		end;
	end;
	for _,u in ipairs(Sense.my.combat) do
		local out = Formation.stuckOut[u.id];
		if out and Sense.round - out > (Formation.stuckOutRounds or 60) then Formation.stuckOut[u.id] = nil; out = nil; end;
		if Formation.of[u.id] == nil and not Military.stashing[u.id] and not Military.peakGuard[u.id] and not out and not Stuck(u.id) then
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
		if f.stage == 'gather' then
			f.want = math.max(count, Formation.Want('take', f.region, spare + count));
			-- (but not for good more than there is: "members 10 of 22" stood for 650 rounds with an
			-- army of 20. After 150 rounds of gathering it goes with what can be spared)
			f.gatherSince = f.gatherSince or f.stageSince or Sense.round;
			if f.kind == 'take' and Sense.round - f.gatherSince > (Formation.gatherMax or 150) then
				f.want = math.max(count, math.min(f.want, count + math.max(0, spare)));
			end;
		end;
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
		elseif count < f.want and (f.stage == 'gather' or f.kind ~= 'take') then	-- (no one joins a take on the march: each came after it alone)
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
			-- (... but a staging region of mine that is not the home region is a place to gather: in
			-- match 167, staged in region 10, the members "gathered" spread over six regions of
			-- mine, and the advances reached the Nexus region with 7 of 19)
			f.atHome = (f.staging == Mem.homeRegion or (Sense.mine[f.staging] and Formation.forwardRally == false)) and true or false;
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
					--[[
					On open ground: the cell of the far half of the region with the most level, free
					ground around it for the heaviest kind in the formation. (The farthest cell was a
					corner: in match 164 the heavy members stood on ledges one or two cells wide, three
					or four of six neighbours no ground for them, the rest held by each other, and 10
					of 20 came out.)
					]]
					local probe = nil;
					for _,m in ipairs(Sense.my.combat or {}) do
						if f.members[m.id] and m.location and not m.storedIn then
							if probe == nil or string.find(tostring(m.type and m.type.name), 'Heavy', 1, true) then probe = m; end;
						end;
					end;
					local cands, maxD = {}, 0;
					for _,c in ipairs(Map.regions[f.staging].cells or {}) do
						local w = {x = c.center.x * Map.groundRadius, y = c.center.y * Map.groundRadius, z = c.center.z * Map.groundRadius};
						local dd = Dist2(w, tc);
						if not Sense.structureCells[c.index] and not Sense.UnderEMP(c) then
							cands[#cands + 1] = {c = c, dd = dd};
							if dd > maxD then maxD = dd; end;
						end;
					end;
					local bestOpen = nil;
					for _,k in ipairs(cands) do
						local open = 0;
						if probe and Formation.openRally ~= false then
							local g0 = Move.GroundAt(probe, k.c);
							if g0 == nil then open = -1;
							else
								local seen = {[k.c.index] = true};
								for _,n1 in ipairs(k.c.neighbors or {}) do
									for _,x in ipairs({n1, table.unpack(n1.neighbors or {})}) do
										if not seen[x.index] then
											seen[x.index] = true;
											if not Sense.structureCells[x.index] and not Sense.UnderEMP(x) then
												local g = Move.GroundAt(probe, x);
												if g and math.abs((g.height or 0) - (g0.height or 0)) <= 1 then open = open + 1; end;
											end;
										end;
									end;
								end;
							end;
						end;
						-- (the far half by distance; among those the most open, then the farthest)
						if open >= 0 and k.dd >= maxD * 0.5 and (bestOpen == nil or open > bestOpen or (open == bestOpen and k.dd > bestD)) then
							bestOpen, bestD, f.rally = open, k.dd, k.c;
						end;
					end;
					if f.rally == nil then
						for _,k in ipairs(cands) do
							if bestD == nil or k.dd > bestD then bestD, f.rally = k.dd, k.c; end;
						end;
					end;
					if f.rally and f.rallyTold ~= f.rally.index then
						f.rallyTold = f.rally.index;
						Log(Name(f)..': gathers at cell '..f.rally.index..' with '..tostring(bestOpen)..' cells of level ground around it');
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
			--[[
			And one way there, region by region: the route is laid once (fewest regions, none that
			is mostly EMP zone, towers on the way counted against it), and the formation moves to
			the next region on it only when seven in ten stand in the one before, or after 15
			rounds. Twenty that set out from three regions of mine for one entry cell were in six
			regions eight rounds later and met six missile towers one by one.
			]]
			if f.stage == 'advance' then
				if f.route == nil or f.routeFor ~= f.staging then
					f.routeFor = f.staging;
					local dist, prev, done = {[f.staging] = 0}, {}, {};
					while true do
						local at, atD = nil, nil;
						for id,d in pairs(dist) do
							if not done[id] and (atD == nil or d < atD) then at, atD = id, d; end;
						end;
						if at == nil or at == f.region then break; end;
						done[at] = true;
						for nb,_ in pairs(Map.regions[at].neighbors) do
							local r = Map.regions[nb];
							local step = 1 + ((Map.EmpShare(nb) >= 0.5) and 1000 or 0) + ((nb ~= f.region and (r.staticThreat or 0) > 0) and 1.5 or 0)
								+ ((Significance.Directed(nb, 'avoid') > 0) and 3 or 0);
							if dist[nb] == nil or atD + step < dist[nb] then dist[nb] = atD + step; prev[nb] = at; end;
						end;
					end;
					f.route = {f.region};
					local at = f.region;
					while prev[at] do at = prev[at]; table.insert(f.route, 1, at); end;
					if f.route[1] ~= f.staging then f.route = {f.staging, f.region}; end;
					f.leg = math.min(2, #f.route);
					f.legSince = Sense.round;
					f.entryLeg = nil;
					Log(Name(f)..': route '..table.concat(f.route, ' > ')..' at round '..Sense.round);
				end;
				local ahead = {};
				for k = f.leg, #f.route do ahead[f.route[k]] = true; end;
				local arrived = 0;
				for id,_ in pairs(f.members) do
					local m = alive[id];
					local reg = Map.RegionOfLoc(m.location or (m.storedIn and m.storedIn.location));
					if reg and ahead[reg.id] then arrived = arrived + 1; end;
				end;
				if f.leg < #f.route then
					f.lastInside = Sense.round;	-- (on the march: not stalled)
					local waited = Sense.round - f.legSince;
					-- (not on with a handful because time is up: "on to region 38 with 5 of 19 up" fed the last
					-- leg half a group at a time, six advances in a row)
					local go = arrived >= math.ceil(f.count * 0.7) or (waited >= (Cfg.marchWait or 30) and arrived >= math.ceil(f.count * 0.5)) or waited >= (Cfg.marchWaitMax or 90);
					-- (and not on without its eye below ground: in match 186 the surveyor gathered with the
					-- army, and at each of eleven entries into the Nexus port region it was two or three
					-- regions behind. Each leg waits eyeLegWait rounds for it.)
					if go and f.kind == 'take' and Military and Military.EyeIn and waited < (Cfg.eyeLegWait or 45) and not Military.EyeIn(ahead) then
						go = false;
						if Sense.round % 10 == 0 then Log(Name(f)..': waits in region '..tostring(f.route[f.leg])..' for its surveyor to come up'); end;
					end;
					if go then
						f.leg = f.leg + 1;
						f.legSince = Sense.round;
						Log(Name(f)..': on to region '..f.route[f.leg]..' at round '..Sense.round..' with '..arrived..' of '..f.count..' up');
					end;
				end;
				if f.entryLeg ~= f.leg then
					f.entryLeg = f.leg;
					f.entry = nil;
					local from = Map.WorldCenter(Map.regions[f.route[f.leg - 1] or f.staging]);
					local bestD = nil;
					for _,c in ipairs(Map.regions[f.route[f.leg]].cells or {}) do
						local w = {x = c.center.x * Map.groundRadius, y = c.center.y * Map.groundRadius, z = c.center.z * Map.groundRadius};
						local dd = Dist2(w, from);
						if not Sense.structureCells[c.index] and (bestD == nil or dd < bestD) then bestD, f.entry = dd, c; end;
					end;
					-- (a cell each, around the entry cell and inside the region: sent to the one cell,
					-- the first stood on it and the others had "no path" to an occupied cell; of 21
					-- only three or four ever came up, match after match)
					f.spots, f.slot, f.nextSlot = {}, {}, 1;
					if f.entry then
						local wr = f.route[f.leg];
						local seen, edge = {[f.entry.index] = true}, {f.entry};
						f.spots[1] = f.entry;
						while #edge > 0 and #f.spots < f.count + 8 do
							local c = table.remove(edge, 1);
							for _,nb in ipairs(c.neighbors or {}) do
								if not seen[nb.index] then
									seen[nb.index] = true;
									local nr = Map.RegionOfLoc({cell = nb, height = 0});
									if nr and nr.id == wr and not Sense.structureCells[nb.index] and not Sense.UnderEMP(nb) then
										f.spots[#f.spots+1] = nb;
										edge[#edge+1] = nb;
									end;
								end;
							end;
						end;
					end;
				end;
			else
				f.route = nil;
			end;
			local clear = target.seenNow and target.hostileStructures == 0 and target.mobileThreat == 0 and target.staticThreat == 0;
			if f.stage ~= 'gather' and Formation.Broken(f, f.count) then
				f.want = math.max(f.want, Formation.Want('take', f.region, 0));
				f.staging = Formation.Staging(f.region);
				SetStage(f, 'gather', 'broken off with '..f.count..' of '..f.peak..' left; now wants '..f.want);
				f.peak = f.count;
			elseif f.stage == 'gather' then
				-- (a staging region gone bad while gathering is left for a better one)
				if Sense.round % 30 == 0 then
					local s2 = Formation.Staging(f.region, f.staging);
					if s2 ~= f.staging then
						Log(Name(f)..': staging moves from region '..tostring(f.staging)..' to region '..tostring(s2)..' at round '..Sense.round);
						f.staging = s2;
					end;
				end;
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
				if Formation.minSize[f.region] then f.want = math.max(f.want, (Sense.round - (f.gatherSince or Sense.round) > (Formation.gatherMax or 150)) and math.min(Formation.minSize[f.region], math.max(f.count, Formation.goFloor or 12)) or Formation.minSize[f.region]); f.stageSince = math.max(f.stageSince, Sense.round - Formation.stallRounds + 1); end;
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
	if f.stage == 'gather' then
		--[[
		A cell each around the rally cell, inside the staging region: all sent to the one cell, the
		first stood on it and those beside it, and the rest had "no path" and held each other in at
		home. In match 169 a take of 30 gathered for 530 rounds with 3 to 12 near the rally cell,
		every one of 81 lines "no path to cell 1596, held in by vehicles of mine".
		]]
		local spot = f.rally;
		if f.rally and Formation.rallySpots ~= false then
			if f.gFor ~= f.rally.index then
				f.gFor = f.rally.index;
				f.gspots, f.gslot, f.gnext = {f.rally}, {}, 1;
				local seen, edge = {[f.rally.index] = true}, {f.rally};
				while #edge > 0 and #f.gspots < (f.count or 0) + 8 do
					local c = table.remove(edge, 1);
					for _,nb in ipairs(c.neighbors or {}) do
						if not seen[nb.index] then
							seen[nb.index] = true;
							local nr = Map.RegionOfLoc({cell = nb, height = 0});
							if nr and nr.id == f.staging and not Sense.structureCells[nb.index] and not Sense.UnderEMP(nb) then
								f.gspots[#f.gspots+1] = nb;
								edge[#edge+1] = nb;
							end;
						end;
					end;
				end;
			end;
			if not f.gslot[v.id] then f.gslot[v.id] = f.gnext; f.gnext = f.gnext + 1; end;
			spot = f.gspots[((f.gslot[v.id] - 1) % #f.gspots) + 1];
			-- (who stands in the staging region within two cells of its spot is there)
		end;
		return f.staging, false, f.staging, nil, spot;
	end;
	if f.stage == 'advance' and f.route and f.entry then
		-- (short of the last region the whole formation holds at the entry cell of the one it is on)
		-- (the stationary shooters follow one region behind: posted in the staging region they stayed home)
		local spot = f.entry;
		if f.spots and #f.spots > 0 then
			if not f.slot[v.id] then f.slot[v.id] = f.nextSlot; f.nextSlot = f.nextSlot + 1; end;
			spot = f.spots[((f.slot[v.id] - 1) % #f.spots) + 1];
		end;
		return f.route[f.leg], true, f.route[math.max(1, f.leg - 1)], spot, nil, f.leg < #f.route;
	end;
	return f.region, true, (f.stage == 'secure') and f.region or f.staging, nil;
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

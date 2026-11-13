--[[
Hexaverse AI player
===================

Plays a regular faction. Launched by the server for each AI player
(Rules/AIPlayers/count, Rules/AIPlayers/script = 'player').

Ground rules
------------
* The script only ever sees what the faction's sensors deliver (the client
  receives nothing else). It never pretends otherwise: targets must be sensed
  now, everything else is memory that ages.
* The volumetric world is reduced to a fixed graph of planar regions
  (clusters of unit cells). Strategy works on regions. Actual 3D motion is
  left to the engine's pathfinding (FindShortestPath / GetReach).
* Persistent state (ExportState) is a small, flat table of numbers and
  strings. No unit/location references, no cycles. Everything else is rebuilt
  from the current scene each round.

Structure
---------
  Types      lazily resolved unit/turret/base types
  Map        regions, region graph, per-region memory (last seen, threat, ...)
  Sense      updates region memory from the current sensor picture
  Economy    research, constructor jobs, factory production
  Military   posture (defend/attack), army movement, scouting, firing
]]

--#require <util/util.lua>
--#require <util/resvec.lua>
--#require <util/queue.lua>
--#require <AI/vehicles.lua>


-------------------------------------------------------------------------------
-- Settings
-------------------------------------------------------------------------------

Cfg = {
	stateVersion = 2,
	logPrefix = 'Player AI: ',
	verbose = true,

	-- desired unit counts
	constructorsEarly = 3,
	constructorsLate = 5,
	radarPerCombat = 6,			-- one radar per N combat units
	maxSurveyors = 1,
	maxSurveyorsDeep = 3,		-- once underground activity is suspected
	aaPerCombat = 4,			-- one AA per N combat units, once air activity is known

	-- posture
	attackMinUnits = 8,			-- combat units needed before attacking
	attackMinRound = 25,
	retreatBelowUnits = 3,		-- attack group falls back below this size

	-- memory
	mobileThreatDecay = 0.85,	-- per round, for remembered (unseen) mobile threat
	staleRounds = 12,			-- region considered stale after this many unseen rounds

	-- structures
	wantLabs = 3,
	wantTowersPerFrontier = 1,
	maxMinesPerRound = 2,

	-- research, in order of preference. Matched against topic fullName (case-insensitive substring)
	research = {
		'Unlock:ReactorCore',
		'Unlock:MissileLauncher',
		'Unlock:HeavyBase',
		'Unlock:Artillery',
		'Attack Range:Cannon',
		'Health:Hoverbase',
		'Attack Range:Artillery',
		'Health:HeavyBase',
	},
};


-------------------------------------------------------------------------------
-- Logging helpers
-------------------------------------------------------------------------------

function Log(msg)
	if Cfg.verbose then
		Game.Log(Cfg.logPrefix..msg);
	end;
end;

function SafeCall(name, func, ...)
	local ok, err = pcall(func, ...);
	if not ok then
		Game.Log(Cfg.logPrefix..'ERROR in '..name..': '..tostring(err));
	end;
	return ok;
end;


-------------------------------------------------------------------------------
-- Types
-------------------------------------------------------------------------------

Types = {resolved = false};

function Types.Resolve()
	if Types.resolved then
		return;
	end;
	local u = Game.Type.units;
	Types.mine = u['Mine'];
	Types.generator = u['Generator'];
	Types.reactor = u['Reactor Core'];
	Types.plant = u['Vehicle Plant'];
	Types.lab = u['Laboratory'];
	Types.store = u['Vehicle Store'];
	Types.mineralContainer = u['Mineral Container'];
	Types.fuelContainer = u['Fuel Container'];

	-- towers are named after their turret
	Types.cannonTower = nil;
	Types.radarTower = nil;
	Types.aaTower = nil;
	for name,t in pairs(u) do
		if t.isStructure and t.turretType then
			local tn = t.turretType.name;
			if tn == 'Cannon' then Types.cannonTower = t;
			elseif tn == 'Radar' then Types.radarTower = t;
			elseif tn == 'Anti Aircraft' then Types.aaTower = t;
			elseif tn == 'Surveyor' then Types.scannerTower = t;
			end;
		end;
	end;

	local tur = Game.Type.turrets;
	local bases = Game.Type.vehicleBases;
	Types.turret = {
		terraformer = tur['Terraformer'],
		cannon = tur['Cannon'],
		artillery = tur['Artillery'],
		launcher = tur['Missile Launcher'],
		radar = tur['Radar'],
		surveyor = tur['Surveyor'],
		aa = tur['Anti Aircraft'],
	};
	Types.base = {
		hover = bases['Hoverbase'],
		heavy = bases['Heavy Base'],
		cat = bases['Catamaran Base'],
	};
	for k,v in pairs(Types) do
		if v == nil then
			Log('Type '..k..' not found');
		end;
	end;
	Types.resolved = true;
end;

function Types.IsUnlocked(t)
	if t == nil then
		return false;
	end;
	local ok, rs = pcall(function() return t.IsUnlockedForFaction(Game.myFaction) end);
	return ok and rs;
end;

-- Best available base for ground vehicles
function Types.GroundBase()
	if Types.IsUnlocked(Types.base.heavy) and Game.GetMainRoundNumber() > 30 then
		return Types.base.heavy;
	end;
	return Types.base.hover;
end;

function Types.Vehicle(turret, base)
	if turret == nil or base == nil then
		return nil;
	end;
	if not Types.IsUnlocked(turret) or not Types.IsUnlocked(base) then
		return nil;
	end;
	local ok, rs = pcall(Game.Type.Vehicle, turret, base);
	if ok then
		return rs;
	end;
	return nil;
end;

-- Classifies one of my mobile units
function Types.Role(u)
	if not u.type.isMobile or not u.turret then
		return nil;
	end;
	local tt = u.turret.type;
	if u.type.isSuborbitalPlane then
		return 'plane';
	end;
	if tt.canConstruct then
		return 'constructor';
	end;
	if tt.detectsResources then
		return 'surveyor';
	end;
	if tt.trajectoryType == 'AAMissile' then
		return 'aa';
	end;
	if tt.canAttack then
		return 'combat';
	end;
	if tt.name == 'Radar' then
		return 'radar';
	end;
	return 'other';
end;


-------------------------------------------------------------------------------
-- Map: fixed region graph over the planar unit cells
-------------------------------------------------------------------------------

Map = {
	built = false,
	regions = {},		-- Region[]
	cellRegion = {},	-- cell index -> region id
	groundRadius = 1,
};

--[[
Region
{
	int			id;
	Vector		center;			-- normalized
	Cell		centerCell;
	Cell[]		cells;
	{id->true}	neighbors;
	-- memory (persisted)
	int			lastSeen;		-- round the ground layer was last covered, -1 = never
	int			lastSeenAir;	-- same for the air (suborbital) layer: AA, planes
	int			lastSeenDeep;	-- same for underground: surveyor vehicles/towers
	float		airThreat;		-- remembered hostile plane presence
	float		mobileThreat;	-- remembered hostile mobile damage potential
	float		staticThreat;	-- remembered hostile structure damage potential
	int			hostileStructures;
	int			deaths;			-- my units lost here (unseen killers count too)
	-- transient (per round)
	bool		seenNow;
	int			myStructures;
	int			myMobiles;
	float		myStrength;
}
]]

local function Dot(a,b)
	return a.x*b.x + a.y*b.y + a.z*b.z;
end;

local function Normalize(v)
	local l = math.sqrt(Dot(v,v));
	if l <= 0 then return {x=0,y=0,z=1}; end;
	return {x=v.x/l, y=v.y/l, z=v.z/l};
end;

local function Dist2(a,b)
	local dx,dy,dz = a.x-b.x, a.y-b.y, a.z-b.z;
	return dx*dx+dy*dy+dz*dz;
end;

function Map.Build()
	if Map.built then
		return;
	end;
	local timer = Game.MeasureSecondsFromNow();
	local cells = Game.Grid.unitCells;
	local n = #cells;
	local k = math.max(24, math.min(96, math.floor(n / 40)));
	Map.groundRadius = Game.Grid.HeightToRadius(0);
	Map.airRadius = Game.Grid.HeightToRadius(Game.Grid.suborbitalHeight or 0);

	-- deterministic farthest point sampling (same regions on every client restart)
	local step = math.max(1, math.floor(n / 4000));
	local seeds = {cells[1]};
	local best = {};	-- sample index -> max dot to any seed (lower = farther)
	for i = 1,n,step do
		best[i] = Dot(cells[i].center, cells[1].center);
	end;
	while #seeds < k do
		local pick, pickVal = nil, 2;
		for i,v in pairs(best) do
			if v < pickVal then
				pick, pickVal = i, v;
			end;
		end;
		if pick == nil then break; end;
		local s = cells[pick];
		seeds[#seeds+1] = s;
		for i,v in pairs(best) do
			local d = Dot(cells[i].center, s.center);
			if d > v then best[i] = d; end;
		end;
	end;

	Map.regions = {};
	for i,s in ipairs(seeds) do
		Map.regions[i] = {
			id = i, center = s.center, centerCell = s, cells = {}, neighbors = {},
			lastSeen = -1, lastSeenAir = -1, lastSeenDeep = -1, airThreat = 0, mobileThreat = 0, staticThreat = 0, hostileStructures = 0, deaths = 0,
			seenNow = false, seenAirNow = false, seenDeepNow = false, myStructures = 0, myMobiles = 0, myStrength = 0,
		};
	end;

	for i = 1,n do
		local c = cells[i];
		local bi, bv = 1, -2;
		for j,s in ipairs(seeds) do
			local d = Dot(c.center, s.center);
			if d > bv then bi, bv = j, d; end;
		end;
		Map.cellRegion[c.index] = bi;
		local r = Map.regions[bi].cells;
		r[#r+1] = c;
	end;

	for i = 1,n do
		local c = cells[i];
		local a = Map.cellRegion[c.index];
		for _,nb in ipairs(c.neighbors) do
			local b = Map.cellRegion[nb.index];
			if b and b ~= a then
				Map.regions[a].neighbors[b] = true;
			end;
		end;
	end;

	Map.built = true;
	Log('Built '..#Map.regions..' regions from '..n..' cells in '..string.format('%.2f',timer())..'s');
end;

function Map.RegionOfLoc(loc)
	if loc == nil or loc.cell == nil then return nil; end;
	local id = Map.cellRegion[loc.cell.index];
	if id == nil then return nil; end;
	return Map.regions[id];
end;

function Map.WorldCenter(r)
	local g = Map.groundRadius;
	return {x=r.center.x*g, y=r.center.y*g, z=r.center.z*g};
end;

function Map.AirCenter(r)
	local g = Map.airRadius or Map.groundRadius;
	return {x=r.center.x*g, y=r.center.y*g, z=r.center.z*g};
end;

-- Region graph distance (BFS hops) from a set of start regions
function Map.Hops(startIds)
	local dist = {};
	local q = Queue.New();
	for _,id in ipairs(startIds) do
		dist[id] = 0;
		Queue.PushRight(q, id);
	end;
	while Queue.IsNotEmpty(q) do
		local id = Queue.PopLeft(q);
		for nb,_ in pairs(Map.regions[id].neighbors) do
			if dist[nb] == nil then
				dist[nb] = dist[id] + 1;
				Queue.PushRight(q, nb);
			end;
		end;
	end;
	return dist;
end;


-------------------------------------------------------------------------------
-- Persistent state
-------------------------------------------------------------------------------

Mem = {
	posture = 'defend',		-- 'defend' | 'attack'
	attackTarget = nil,		-- region id
	homeRegion = nil,		-- region id
	airSeen = -1,			-- round hostile air activity (planes, plane factories) was last sensed
	deepSeen = -1,			-- round hostile underground activity was last sensed or suspected
	pendingImport = nil,
	blacklist = {},			-- cell index -> round until which build attempts are skipped
	jobs = {},				-- unit id -> {kind, cell, since, at}  (transient, not exported)
};

function ExportState()
	if not Map.built then
		return Mem.pendingImport;
	end;
	local r = {};
	for i,reg in ipairs(Map.regions) do
		if reg.lastSeen >= 0 or reg.lastSeenAir >= 0 or reg.lastSeenDeep >= 0 or reg.staticThreat > 0 or reg.deaths > 0 then
			-- flat record: id, lastSeen (ground, air, deep), mobile threat, static threat, hostile structures, deaths, air threat
			r[#r+1] = {i, reg.lastSeen, reg.lastSeenAir, reg.lastSeenDeep,
				math.floor(reg.mobileThreat*10)/10, math.floor(reg.staticThreat*10)/10,
				reg.hostileStructures, reg.deaths, math.floor(reg.airThreat*10)/10};
		end;
	end;
	return {
		v = Cfg.stateVersion,
		n = #Map.regions,
		posture = Mem.posture,
		target = Mem.attackTarget or 0,
		home = Mem.homeRegion or 0,
		air = Mem.airSeen,
		deep = Mem.deepSeen,
		r = r,
	};
end;

function Init(state)
	Mem.pendingImport = state;
end;

function ApplyImport()
	local s = Mem.pendingImport;
	Mem.pendingImport = nil;
	if type(s) ~= 'table' or s.v ~= Cfg.stateVersion or s.n ~= #Map.regions then
		if s ~= nil then
			Log('Discarding incompatible saved state');
		end;
		return;
	end;
	Mem.posture = (s.posture == 'attack') and 'attack' or 'defend';
	if s.target and s.target > 0 then Mem.attackTarget = s.target; end;
	if s.home and s.home > 0 then Mem.homeRegion = s.home; end;
	Mem.airSeen = s.air or -1;
	Mem.deepSeen = s.deep or -1;
	if type(s.r) == 'table' then
		for _,rec in ipairs(s.r) do
			local reg = Map.regions[rec[1]];
			if reg then
				reg.lastSeen = rec[2] or -1;
				reg.lastSeenAir = rec[3] or -1;
				reg.lastSeenDeep = rec[4] or -1;
				reg.mobileThreat = rec[5] or 0;
				reg.staticThreat = rec[6] or 0;
				reg.hostileStructures = rec[7] or 0;
				reg.deaths = rec[8] or 0;
				reg.airThreat = rec[9] or 0;
			end;
		end;
	end;
	Log('Restored memory of '..(s.r and #s.r or 0)..' regions');
end;


-------------------------------------------------------------------------------
-- Sense: update memory strictly from what is sensed now
-------------------------------------------------------------------------------

Sense = {
	round = 0,
	my = {},			-- role -> Unit[]
	myStructures = {},
	structuresByType = {},	-- type name -> count (incl. under construction)
	plants = {},
	labs = {},
	baseCenter = nil,	-- Vector (world)
	frontier = {},		-- region ids bordering my regions
	myRegionIds = {},
	enemyRegions = {},	-- region ids with remembered hostile presence
};

local function DamagePotential(u)
	if u.turret and u.turret.type.canAttack and u.turret.damageLevel then
		return (u.turret.damageLevel or 0) * (u.turret.attacksPerRound or 1);
	end;
	return 0;
end;

function Sense.Update()
	local round = Game.GetMainRoundNumber();
	Sense.round = round;
	Sense.my = {constructor={}, combat={}, radar={}, surveyor={}, aa={}, plane={}, other={}};
	Sense.myStructures = {};
	Sense.structuresByType = {};
	Sense.plants = {};
	Sense.labs = {};

	for _,reg in ipairs(Map.regions) do
		reg.seenNow = false;
		reg.seenAirNow = false;
		reg.seenDeepNow = false;
		reg.myStructures = 0;
		reg.myMobiles = 0;
		reg.myStrength = 0;
	end;

	-- my units, and what they currently observe
	--[[
	Three separate sensor layers:
	* ground: regular scanning range of ground units/structures, and ground scanning of airborne sensor planes.
	  Barely penetrates the ground
	* air: suborbital scanning (AA, planes). Regular radar can't see planes
	* deep: units with resource-detecting turrets (surveyors). Deep, but short horizontal range
	]]
	local observers = {};		-- {pos=Vector, range=float} for the ground layer
	local airObservers = {};
	local deepObservers = {};
	local sx, sy, sz, sc = 0, 0, 0, 0;
	for _,u in pairs(Game.myUnits) do
		local reg = Map.RegionOfLoc(u.location);
		if u.type.isMobile then
			local role = Types.Role(u) or 'other';
			local list = Sense.my[role];
			list[#list+1] = u;
			if reg then
				reg.myMobiles = reg.myMobiles + 1;
				reg.myStrength = reg.myStrength + DamagePotential(u);
			end;
		else
			Sense.myStructures[#Sense.myStructures+1] = u;
			local tn = u.type.name;
			Sense.structuresByType[tn] = (Sense.structuresByType[tn] or 0) + 1;
			if u.type.isMobileEntityProducer and not u.type.isHangar then
				Sense.plants[#Sense.plants+1] = u;
			end;
			if Types.lab and u.type == Types.lab then
				Sense.labs[#Sense.labs+1] = u;
			end;
			if reg then
				reg.myStructures = reg.myStructures + 1;
				reg.myStrength = reg.myStrength + DamagePotential(u);
			end;
			local w = u.location.world;
			sx, sy, sz, sc = sx + w.x, sy + w.y, sz + w.z, sc + 1;
		end;
		local w = u.location.world;
		if u.location.isSuborbital then
			if u.groundScanningRange and u.groundScanningRange > 0 then
				-- distance measured at suborbital height (see Util.CheckVisibility)
				observers[#observers+1] = {pos=w, range=u.groundScanningRange, air=true};
			end;
			if u.scanningRange and u.scanningRange > 0 then
				airObservers[#airObservers+1] = {pos=w, range=u.scanningRange};
			end;
		else
			if u.scanningRange and u.scanningRange > 0 then
				observers[#observers+1] = {pos=w, range=u.scanningRange};
			end;
			if u.suborbitalScanningRange and u.suborbitalScanningRange > 0 then
				local ok, sub = pcall(Util.ToSuborbit, u.location);
				if ok and sub then
					airObservers[#airObservers+1] = {pos=sub.world, range=u.suborbitalScanningRange};
				end;
			end;
			if u.turret and u.turret.type.detectsResources and u.scanningRange then
				deepObservers[#deepObservers+1] = {pos=w, range=u.scanningRange};
			end;
		end;
	end;

	-- jobs in progress count towards structure numbers
	for _,u in ipairs(Sense.my.constructor) do
		if u.tasks then
			for _,t in ipairs(u.tasks) do
				if t.job and t.job.buildingType then
					local tn = t.job.buildingType;
					Sense.structuresByType[tn] = (Sense.structuresByType[tn] or 0) + 1;
				end;
			end;
		end;
	end;

	if sc > 0 then
		Sense.baseCenter = {x=sx/sc, y=sy/sc, z=sz/sc};
	elseif #Sense.my.constructor > 0 then
		Sense.baseCenter = Sense.my.constructor[1].location.world;
	end;
	if Sense.baseCenter then
		local home = nil;
		local bv = -2;
		local bc = Normalize(Sense.baseCenter);
		for _,reg in ipairs(Map.regions) do
			local d = Dot(reg.center, bc);
			if d > bv then home, bv = reg.id, d; end;
		end;
		Mem.homeRegion = home;
	end;

	-- which regions are observed right now, per layer: region center within an observer's range
	local function Covered(list, groundCenter, airCenter)
		for _,o in ipairs(list) do
			local c = o.air and airCenter or groundCenter;
			if Dist2(o.pos, c) <= o.range*o.range then
				return true;
			end;
		end;
		return false;
	end;
	local refUnit = Sense.my.constructor[1] or Sense.my.combat[1] or Sense.my.radar[1] or Sense.my.surveyor[1];
	for _,reg in ipairs(Map.regions) do
		local c = Map.WorldCenter(reg);
		local a = Map.AirCenter(reg);
		-- Ground: range is only a pre-filter. Rock casts shadows, so the engine's actual sensor
		-- grid decides (sampled at a few surface points of the region)
		if Covered(observers, c, a) and Sense.GroundVisible(reg, refUnit) then
			reg.seenNow = true;
			reg.lastSeen = round;
		end;
		if Covered(airObservers, a, a) then
			reg.seenAirNow = true;
			reg.lastSeenAir = round;
		end;
		if Covered(deepObservers, c, c) then
			reg.seenDeepNow = true;
			reg.lastSeenDeep = round;
		end;
	end;

	-- hostile presence: what is sensed now replaces memory of seen regions
	local seenMobile, seenStatic, seenStructs, seenAir = {}, {}, {}, {};
	for _,e in pairs(Game.hostileUnits) do
		if e.health > 0 and e.location.isSuborbital then
			local reg = Map.RegionOfLoc(e.location);
			if reg then
				seenAir[reg.id] = (seenAir[reg.id] or 0) + math.max(DamagePotential(e), 1);
			end;
			if e.owner == nil or not e.owner.alliance or e.owner.alliance ~= Game.myFaction.alliance then
				Mem.airSeen = round;
			end;
		end;
		if e.health > 0 and not e.location.isSuborbital then
			-- the hostile can build or launch planes: air coverage matters from now on
			if e.type.name == 'Plane Factory' or (e.type.isSuborbitalPlane) then
				Mem.airSeen = round;
			end;
			-- sensed clearly below the surface, or digging: underground coverage matters
			if (e.location.height or 0) <= -3 or (e.turret and e.turret.type.canRemoveTerrain) then
				if (e.location.height or 0) <= -3 then
					Mem.deepSeen = round;
				end;
			end;
			local reg = Map.RegionOfLoc(e.location);
			if reg then
				local id = reg.id;
				local p = DamagePotential(e);
				if e.type.isMobile then
					seenMobile[id] = (seenMobile[id] or 0) + math.max(p, 1);
				else
					seenStatic[id] = (seenStatic[id] or 0) + p;
					seenStructs[id] = (seenStructs[id] or 0) + 1;
				end;
			end;
		end;
	end;

	for _,reg in ipairs(Map.regions) do
		local id = reg.id;
		if reg.seenNow then
			reg.mobileThreat = seenMobile[id] or 0;
			reg.staticThreat = seenStatic[id] or 0;
			reg.hostileStructures = seenStructs[id] or 0;
		else
			-- unseen: mobile threat may have moved on, structures stay where they were
			reg.mobileThreat = math.max(reg.mobileThreat * Cfg.mobileThreatDecay, seenMobile[id] or 0);
			if seenStatic[id] then reg.staticThreat = math.max(reg.staticThreat, seenStatic[id]); end;
			if seenStructs[id] then reg.hostileStructures = math.max(reg.hostileStructures, seenStructs[id]); end;
			if reg.mobileThreat < 0.5 then reg.mobileThreat = 0; end;
		end;
		if reg.seenAirNow then
			reg.airThreat = seenAir[id] or 0;
		else
			reg.airThreat = math.max(reg.airThreat * Cfg.mobileThreatDecay, seenAir[id] or 0);
			if reg.airThreat < 0.5 then reg.airThreat = 0; end;
		end;
	end;

	-- unexplained deaths of my units: something we can't see is dangerous there
	if Game.myMarkers then
		for _,m in pairs(Game.myMarkers) do
			if m.type == 'EntityDeath' and m.age == 0 then
				local reg = Map.RegionOfLoc(m.location);
				if reg then
					reg.deaths = reg.deaths + 1;
					-- a loss inside my territory that no sensed hostile explains: maybe undermined
					if (reg.myStructures > 0 or reg.id == Mem.homeRegion) and reg.mobileThreat == 0 and reg.staticThreat == 0 then
						if reg.airThreat > 0 or Mem.airSeen >= round - 3 then
							Mem.airSeen = round;
						else
							Mem.deepSeen = round;
						end;
					end;
				end;
			end;
		end;
	end;

	-- my territory, frontier, known enemy regions
	Sense.myRegionIds = {};
	local mine = {};
	for _,reg in ipairs(Map.regions) do
		if reg.myStructures > 0 then
			mine[reg.id] = true;
			Sense.myRegionIds[#Sense.myRegionIds+1] = reg.id;
		end;
	end;
	if #Sense.myRegionIds == 0 and Mem.homeRegion then
		mine[Mem.homeRegion] = true;
		Sense.myRegionIds[1] = Mem.homeRegion;
	end;
	Sense.mine = mine;
	Sense.frontier = {};
	for id,_ in pairs(mine) do
		for nb,_ in pairs(Map.regions[id].neighbors) do
			if not mine[nb] then
				Sense.frontier[nb] = true;
			end;
		end;
	end;
	Sense.enemyRegions = {};
	for _,reg in ipairs(Map.regions) do
		if reg.hostileStructures > 0 or reg.staticThreat > 0 or reg.mobileThreat > 0 then
			Sense.enemyRegions[#Sense.enemyRegions+1] = reg.id;
		end;
	end;
	Sense.hops = Map.Hops(Sense.myRegionIds);
end;

--[[
Surface sample points per region (cached; terrain changes slowly, unknown terrain has no surface yet)
]]
Sense.surfaceCache = {};	-- cell index -> {loc=Loc|false, round=int}

function Sense.SamplesOf(reg)
	if reg.samples == nil then
		local s = {reg.centerCell};
		local n = #reg.cells;
		if n > 2 then
			s[#s+1] = reg.cells[math.floor(n/3)+1];
			s[#s+1] = reg.cells[math.floor(2*n/3)+1];
		end;
		reg.samples = s;
	end;
	return reg.samples;
end;

function Sense.SurfaceAt(cell, refUnit)
	local e = Sense.surfaceCache[cell.index];
	if e and (e.loc or Sense.round - e.round < 5) and Sense.round - e.round < 20 then
		return e.loc or nil;
	end;
	local loc = false;
	if refUnit then
		local ok, l = pcall(Game.Grid.FindHighestVehicleGround, refUnit, cell);
		if ok and l then loc = l; end;
	end;
	Sense.surfaceCache[cell.index] = {loc=loc, round=Sense.round};
	return loc or nil;
end;

function Sense.GroundVisible(reg, refUnit)
	if not Game.Terrain or not Game.Terrain.GetLowestSensorStrength then
		return true;	-- API unavailable: fall back to range only
	end;
	for _,cell in ipairs(Sense.SamplesOf(reg)) do
		local loc = Sense.SurfaceAt(cell, refUnit);
		if loc then
			local ok, strength = pcall(Game.Terrain.GetLowestSensorStrength, loc, 1);
			if ok and strength and strength > 0 then
				return true;
			end;
		end;
	end;
	return false;
end;

function Sense.Threat(reg)
	return reg.mobileThreat + reg.staticThreat + reg.deaths * 2;
end;

function Sense.Staleness(reg, layer)
	local last = reg.lastSeen;
	if layer == 'air' then last = reg.lastSeenAir;
	elseif layer == 'deep' then last = reg.lastSeenDeep; end;
	if last < 0 then
		return 1000;
	end;
	return Sense.round - last;
end;

-- Does a layer currently matter? Weight decays if the enemy hasn't shown that capability for a while
function Sense.AirMatters()
	return Mem.airSeen >= 0 and Sense.round - Mem.airSeen < 80;
end;

function Sense.DeepMatters()
	return Mem.deepSeen >= 0 and Sense.round - Mem.deepSeen < 60;
end;


-------------------------------------------------------------------------------
-- Motion helpers (3D handled by the engine)
-------------------------------------------------------------------------------

Move = {};

-- Ground location a unit could stand on in the given cell, or nil
function Move.GroundAt(u, cell)
	local ok, loc = pcall(Game.Grid.FindHighestVehicleGround, u, cell);
	if ok then return loc; end;
	return nil;
end;

--[[
Moves the unit towards a world position.
Tries a full path first. If the destination is unknown or unreachable, steps to
the reachable node closest to the goal (frontier stepping). Unknown terrain is
only entered by units with permission (scouts).
@return true if a motion was scheduled
]]
function Move.Towards(u, goalWorld, goalCell, avoidThreat, mayEnterUnknown)
	if goalCell then
		local dest = Move.GroundAt(u, goalCell);
		if dest and dest ~= u.location then
			local ok, path = pcall(u.FindShortestPath, dest, nil, false);
			if ok and path and #path > 0 then
				local limit = math.max(1, u.movementSpeed * 3);
				local pick = path[#path];
				-- don't commit to the full route; re-plan each round as the picture changes
				for _,n in ipairs(path) do
					if n.movementCostsFromRoot <= limit then pick = n; end;
				end;
				if not avoidThreat or not Move.IsDangerous(pick.location) then
					u.DirectTo(pick);
					return true;
				end;
			end;
		end;
	end;

	local block = nil;
	if avoidThreat then
		block = function(loc) return Move.IsDangerous(loc); end;
	end;
	local ok, reach = pcall(u.GetReach, block, math.max(1, u.movementSpeed * 2), false, false, false, not mayEnterUnknown);
	if not ok or reach == nil or #reach == 0 then
		return false;
	end;
	local best, bestD = nil, Dist2(u.location.world, goalWorld);
	for _,n in ipairs(reach) do
		local d = Dist2(n.location.world, goalWorld) + n.movementCostsFromRoot * 0.01;
		if d < bestD then
			best, bestD = n, d;
		end;
	end;
	if best then
		u.DirectTo(best);
		return true;
	end;
	return false;
end;

-- A location is dangerous if a sensed or remembered hostile turret could hit it
function Move.IsDangerous(loc)
	local reg = Map.RegionOfLoc(loc);
	if reg and reg.staticThreat > 0 and not reg.seenNow then
		return true;	-- remembered towers we currently can't see
	end;
	for _,e in pairs(Game.hostileUnits) do
		if e.health > 0 and e.turret and e.turret.type.canAttack and e.turret.IsInRange then
			local ok, inRange = pcall(e.turret.IsInRange, loc.world);
			if ok and inRange then
				return true;
			end;
		end;
	end;
	return false;
end;

function Move.IsIdle(u)
	return u.tasks == nil or #u.tasks == 0;
end;


-------------------------------------------------------------------------------
-- Economy
-------------------------------------------------------------------------------

Economy = {};

function Economy.Resources()
	local f = Game.myFaction;
	return f.currentResources, f.maxResources;
end;

function Economy.Count(t)
	if t == nil then return 0; end;
	return Sense.structuresByType[t.name] or 0;
end;

function Economy.Research()
	local queue = Game.Research.myQueue or {};
	local maxLen = Game.Research.maxQueueLength or 3;
	if #queue >= math.min(maxLen, 2) then
		return;
	end;
	local queued = #queue;
	for _,want in ipairs(Cfg.research) do
		if queued >= math.min(maxLen, 2) then break; end;
		local w = string.lower(want);
		local parts = {};
		for p in string.gmatch(w, '[^:]+') do parts[#parts+1] = p; end;
		for name,topic in pairs(Game.Research.topics) do
			local fn = string.lower(topic.fullName or name);
			local match = true;
			for _,p in ipairs(parts) do
				if not string.find(fn, p, 1, true) then match = false; break; end;
			end;
			if match and not topic.isCompleted and not topic.isQueued
				and (topic.dependency == nil or topic.dependency.isCompleted)
				and topic.max ~= 0
			then
				Log('Research: '..topic.fullName);
				topic.Enqueue();
				queued = queued + 1;
				break;
			end;
		end;
	end;
end;

-- What should the next idle constructor build? Returns {type, kind} or nil
function Economy.NextStructure(constructorIndex)
	local cur, max = Economy.Resources();
	local bal = max.balance;
	local round = Sense.round;

	if Types.plant and Economy.Count(Types.plant) == 0 then
		return {type=Types.plant, kind='base'};
	end;
	-- energy first: everything else depends on it
	if bal.energy < 0 or (cur.balance.energy <= 0 and Economy.Count(Types.generator) < 2) then
		if Types.IsUnlocked(Types.reactor) and Economy.Count(Types.reactor) == 0 and Economy.Count(Types.generator) >= 2 then
			return {type=Types.reactor, kind='base'};
		end;
		if bal.fuel <= 0 and constructorIndex % 2 == 1 then
			return {type=Types.mine, kind='mine', resource='Fuel'};
		end;
		return {type=Types.generator, kind='base'};
	end;
	if Types.lab and Economy.Count(Types.lab) == 0 and round >= 3 then
		return {type=Types.lab, kind='base'};
	end;
	-- a tower on a threatened frontier
	if Types.cannonTower and constructorIndex == 1 then
		for id,_ in pairs(Sense.frontier) do
			local reg = Map.regions[id];
			if reg.mobileThreat > 0 and Economy.TowersIn(reg) < Cfg.wantTowersPerFrontier then
				return {type=Types.cannonTower, kind='tower', region=id};
			end;
		end;
	end;
	-- suspected undermining: a scanner tower per built-up region (scanners see down, not far sideways)
	if Types.scannerTower and Sense.DeepMatters() and Types.IsUnlocked(Types.scannerTower) then
		for id,_ in pairs(Sense.mine) do
			local reg = Map.regions[id];
			if reg.myStructures >= 3 and Economy.ScannersIn(reg) == 0 and not Economy.scannerClaimed[id] then
				Economy.scannerClaimed[id] = true;
				return {type=Types.scannerTower, kind='tower', region=id};
			end;
		end;
	end;
	if Types.aaTower and Sense.AirMatters() and Types.IsUnlocked(Types.aaTower) and Economy.Count(Types.aaTower) < 2 then
		return {type=Types.aaTower, kind='tower'};
	end;
	if Economy.Count(Types.mine) < 4 + math.floor(round / 10) then
		local res = 'Minerals';
		if bal.fuel <= 0 then res = 'Fuel'; end;
		return {type=Types.mine, kind='mine', resource=res};
	end;
	if Types.lab and Economy.Count(Types.lab) < Cfg.wantLabs and round >= 15 then
		return {type=Types.lab, kind='base'};
	end;
	if Types.plant and Economy.Count(Types.plant) < 2 and round >= 30 then
		return {type=Types.plant, kind='base'};
	end;
	if Types.cannonTower and Economy.Count(Types.cannonTower) < 2 + math.floor(round / 20) then
		return {type=Types.cannonTower, kind='tower'};
	end;
	if Types.radarTower and Types.IsUnlocked(Types.radarTower) and Economy.Count(Types.radarTower) < 1 + math.floor(round / 25) then
		return {type=Types.radarTower, kind='tower'};
	end;
	return {type=Types.mine, kind='mine', resource='Minerals'};
end;

function Economy.ScannersIn(reg)
	local n = 0;
	for _,s in ipairs(Sense.myStructures) do
		if s.turret and s.turret.type.detectsResources and Map.RegionOfLoc(s.location) == reg then
			n = n + 1;
		end;
	end;
	return n;
end;

function Economy.TowersIn(reg)
	local n = 0;
	for _,s in ipairs(Sense.myStructures) do
		if s.turret and s.turret.type.canAttack and Map.RegionOfLoc(s.location) == reg then
			n = n + 1;
		end;
	end;
	return n;
end;

-- Is this cell a reasonable construction site?
function Economy.SiteOK(loc, forMine)
	local idx = loc.cell.index;
	local bl = Mem.blacklist[idx];
	if bl and bl > Sense.round then return false; end;
	if Game.GetStructureAt(loc) ~= nil then return false; end;
	local other = Game.GetMobileAt(loc);
	if other ~= nil and not other.isOwnedByMe then return false; end;
	if Game.Grid.AffectedByEMP(loc) then return false; end;
	if not forMine then
		-- keep resource cells free for mines
		local res = Game.Grid.GetNaturalResourcesAt and Game.Grid.GetNaturalResourcesAt(loc.cell);
		if res ~= nil and (res.minerals > 0 or res.fuel > 0 or res.uranium > 0) then
			return false;
		end;
	end;
	-- don't build where remembered or sensed hostiles can hit
	if Move.IsDangerous(loc) then return false; end;
	return true;
end;

-- Cells another constructor has already claimed this round
Economy.claimed = {};
Economy.scannerClaimed = {};

function Economy.AssignConstructor(u, index)
	local job = Mem.jobs[u.id];
	if not Move.IsIdle(u) then
		return;	-- busy with a queued motion/construction
	end;
	-- detect stuck jobs: still idle at the same place where we ordered a construction
	if job and job.cell and job.at == u.location.cell.index and Sense.round - job.since >= 1 then
		Mem.blacklist[job.cell] = Sense.round + 30;
		Log(u.niceName..': construction at cell '..job.cell..' seems to have failed. Blacklisting');
	end;
	Mem.jobs[u.id] = nil;

	local want = Economy.NextStructure(index);
	if want == nil or want.type == nil then
		return;
	end;

	if want.kind == 'mine' then
		local cell = nil;
		local ok, c = pcall(Game.Grid.FindHighestAvailableResourceLocationExt, want.resource, true, true);
		if ok then cell = c; end;
		if cell == nil and want.resource ~= 'Minerals' then
			ok, c = pcall(Game.Grid.FindHighestAvailableResourceLocationExt, 'Minerals', true, true);
			if ok then cell = c; end;
		end;
		if cell and not Economy.claimed[cell.index] then
			local dest = Move.GroundAt(u, cell);
			local reg = Map.RegionOfLoc(dest);
			local safe = reg and Sense.Threat(reg) <= 0 and (Sense.hops[reg.id] or 99) <= 2;
			if dest and safe and Economy.SiteOK(dest, true) then
				if Economy.BuildAt(u, dest, want.type) then
					Economy.claimed[cell.index] = true;
					return;
				end;
			end;
		end;
		-- no safe known resource spot: build something useful at home instead
		want = {type = Types.cannonTower or Types.generator, kind='tower'};
		if Sense.round < 10 then
			want = {type = Types.generator, kind='base'};
		end;
	end;

	-- non-mine: choose a reachable site near home (or in the requested region)
	local goal = Sense.baseCenter or u.location.world;
	if want.region then
		goal = Map.WorldCenter(Map.regions[want.region]);
	end;
	local ok, reach = pcall(u.GetReach, nil, math.max(2, u.movementSpeed * 3), true, false, false, true);
	if not ok or reach == nil then
		return;
	end;
	Util.AddCurrentToReach(reach, u);
	local best, bestScore = nil, nil;
	for _,n in ipairs(reach) do
		local loc = n.location;
		if not loc.isSuborbital and not Economy.claimed[loc.cell.index] and Economy.SiteOK(loc, false) then
			local d = Dist2(loc.world, goal);
			-- towers prefer the edge of the base, everything else the middle
			local score = d;
			if want.kind == 'tower' and not want.region then
				score = math.abs(d - 9.0);
			end;
			score = score + n.movementCostsFromRoot * 0.05;
			if bestScore == nil or score < bestScore then
				best, bestScore = n, score;
			end;
		end;
	end;
	if best then
		Economy.claimed[best.location.cell.index] = true;
		Economy.BuildAt(u, best.location, want.type, best);
	end;
end;

function Economy.BuildAt(u, loc, t, reachable)
	if loc ~= u.location then
		if reachable == nil then
			local ok, path = pcall(u.FindShortestPath, loc, nil, false);
			if not ok or path == nil or #path == 0 then
				Mem.blacklist[loc.cell.index] = Sense.round + 20;
				return false;
			end;
			reachable = path[#path];
		end;
		u.DirectTo(reachable);
	end;
	local ok, err = pcall(u.BuildStructure, t);
	if not ok then
		Log('BuildStructure failed: '..tostring(err));
		return false;
	end;
	Mem.jobs[u.id] = {kind = t.name, cell = loc.cell.index, since = Sense.round, at = loc.cell.index};
	Log(u.niceName..' -> build '..t.name..' at cell '..loc.cell.index);
	return true;
end;

-- What should a vehicle plant produce next?
function Economy.NextVehicle()
	local my = Sense.my;
	local base = Types.GroundBase();
	local hover = Types.base.hover;
	local wantConstructors = Sense.round < 40 and Cfg.constructorsEarly or Cfg.constructorsLate;
	if #my.constructor < wantConstructors then
		return Types.Vehicle(Types.turret.terraformer, base) or Types.Vehicle(Types.turret.terraformer, hover);
	end;
	if #my.radar < 1 + math.floor(#my.combat / Cfg.radarPerCombat) then
		return Types.Vehicle(Types.turret.radar, hover);
	end;
	if Sense.AirMatters() and #my.aa * Cfg.aaPerCombat < #my.combat + 2 then
		local aa = Types.Vehicle(Types.turret.aa, hover);
		if aa then return aa; end;
	end;
	local wantSurveyors = Sense.DeepMatters() and Cfg.maxSurveyorsDeep or Cfg.maxSurveyors;
	if #my.surveyor < wantSurveyors and Sense.round > 10 then
		return Types.Vehicle(Types.turret.surveyor, Types.base.heavy) or Types.Vehicle(Types.turret.surveyor, hover);
	end;
	-- mostly cannons, some artillery once available
	local nArt = 0;
	for _,u in ipairs(my.combat) do
		if u.turret.type == Types.turret.artillery then nArt = nArt + 1; end;
	end;
	if Types.IsUnlocked(Types.turret.artillery) and nArt * 3 < #my.combat then
		return Types.Vehicle(Types.turret.artillery, base) or Types.Vehicle(Types.turret.artillery, hover);
	end;
	return Types.Vehicle(Types.turret.cannon, base) or Types.Vehicle(Types.turret.cannon, hover);
end;

function Economy.Production()
	for _,p in ipairs(Sense.plants) do
		if p.productionCompleted or p.deploymentOptions then
			local ok, loc = pcall(p.GetIdealDeploymentOption);
			if ok and loc then
				Log(p.niceName..': deploying '..tostring(p.productionCompleted));
				p.DeployProductionAt(loc);
			end;
		end;
		local queued = p.productionQueue and #p.productionQueue or 0;
		if queued < 2 and p.BuildMobile then
			local t = Economy.NextVehicle();
			if t then
				local ok, err = pcall(p.BuildMobile, t);
				if ok then
					Log(p.niceName..': producing '..t.name);
				else
					Log(p.niceName..': cannot produce '..t.name..': '..tostring(err));
				end;
			end;
		end;
	end;
end;

function Economy.Constructors()
	Economy.claimed = {};
	Economy.scannerClaimed = {};
	-- remember cells constructors are already heading to
	for id,job in pairs(Mem.jobs) do
		if job.cell then Economy.claimed[job.cell] = true; end;
	end;
	for i,u in ipairs(Sense.my.constructor) do
		SafeCall('constructor '..u.niceName, Economy.AssignConstructor, u, i);
	end;
end;


-------------------------------------------------------------------------------
-- Military
-------------------------------------------------------------------------------

Military = {};

-- Fire at sensed targets only. Returns true if any attack was scheduled
function Military.Fire(v)
	local t = v.turret;
	if not t or not t.type.canAttack or not v.Attack then
		return false;
	end;
	if t.ammoDemandPerShot and t.ammoAvailability and t.ammoDemandPerShot > t.ammoAvailability then
		return false;
	end;
	local isAA = t.type.trajectoryType == 'AAMissile';
	local candidates = {};
	for _,e in pairs(Game.hostileUnits) do
		if e.health > 0 then	-- sensed now; no firing at ghosts
			SafeCall('target', ProcessAttackTarget, v, e, candidates, isAA, not isAA);
		end;
	end;
	if #candidates == 0 then
		return false;
	end;
	table.sort(candidates, function(a,b) return a.priority > b.priority end);
	local targets = {};
	for i = 1, math.min(#candidates, t.attacksPerRound or 1) do
		targets[i] = candidates[i].target;
	end;
	v.Attack(targets);
	return true;
end;

function Military.ArmyStrength()
	local n = 0;
	for _,u in ipairs(Sense.my.combat) do
		if u.health > 0 then n = n + 1; end;
	end;
	return n;
end;

function Military.ChooseAttackTarget()
	local best, bestScore = nil, 0;
	for _,id in ipairs(Sense.enemyRegions) do
		local reg = Map.regions[id];
		if not Sense.mine[id] then
			-- prefer remembered structures (they don't move), weigh against distance and staleness
			local value = reg.hostileStructures * 3 + reg.mobileThreat * 0.2;
			local hops = Sense.hops[id] or 20;
			local score = value / (1 + hops * 0.3) / (1 + Sense.Staleness(reg) * 0.02);
			if score > bestScore then
				best, bestScore = id, score;
			end;
		end;
	end;
	return best;
end;

function Military.UpdatePosture()
	local n = Military.ArmyStrength();
	if Mem.posture == 'defend' then
		if n >= Cfg.attackMinUnits and Sense.round >= Cfg.attackMinRound then
			local target = Military.ChooseAttackTarget();
			if target then
				Mem.posture = 'attack';
				Mem.attackTarget = target;
				Log('Posture: attack region '..target..' with '..n..' units');
			end;
		end;
	else
		if n < Cfg.retreatBelowUnits then
			Mem.posture = 'defend';
			Mem.attackTarget = nil;
			Log('Posture: fall back to defense');
		else
			local reg = Mem.attackTarget and Map.regions[Mem.attackTarget];
			-- target region cleared: we're there, see it, and it's empty
			if reg == nil or (reg.seenNow and reg.hostileStructures == 0 and reg.mobileThreat == 0) then
				Mem.attackTarget = Military.ChooseAttackTarget();
				if Mem.attackTarget == nil then
					Mem.posture = 'defend';
					Log('Posture: no known targets left, defend and scout');
				else
					Log('Posture: next attack target region '..Mem.attackTarget);
				end;
			end;
		end;
	end;
end;

-- Region the defense should cover: the most threatened of my regions or the frontier
function Military.DefenseRegion()
	local best, bestT = Mem.homeRegion, -1;
	for id,_ in pairs(Sense.mine) do
		local t = Sense.Threat(Map.regions[id]);
		for nb,_ in pairs(Map.regions[id].neighbors) do
			t = t + Map.regions[nb].mobileThreat * 0.5;
		end;
		if t > bestT then
			best, bestT = id, t;
		end;
	end;
	return best;
end;

function Military.Combat()
	local defense = Military.DefenseRegion();
	-- army centroid, for cohesion while attacking
	local cx, cy, cz, cn = 0, 0, 0, 0;
	for _,u in ipairs(Sense.my.combat) do
		local w = u.location.world;
		cx, cy, cz, cn = cx + w.x, cy + w.y, cz + w.z, cn + 1;
	end;
	local centroid = cn > 0 and {x=cx/cn, y=cy/cn, z=cz/cn} or nil;

	for i,v in ipairs(Sense.my.combat) do
		SafeCall('combat '..v.niceName, function()
			local fired = Military.Fire(v);
			if fired and not v.motionIgnoresAttack then
				return;	-- e.g. artillery: stay and shoot
			end;
			if not Move.IsIdle(v) and not fired then
				-- re-plan every round, the picture changes
				v.ClearSafeTasks();
				if fired then return; end;
			end;
			local goalId = defense;
			if Mem.posture == 'attack' and Mem.attackTarget then
				goalId = Mem.attackTarget;
			end;
			if goalId == nil then return; end;
			local reg = Map.regions[goalId];
			local goal = Map.WorldCenter(reg);
			-- stragglers regroup first
			if Mem.posture == 'attack' and centroid and Dist2(v.location.world, centroid) > 100 then
				goal = centroid;
			end;
			-- don't crowd the defense region center: spread a bit
			local cell = reg.centerCell;
			if #reg.cells > 1 then
				cell = reg.cells[((i * 7) % #reg.cells) + 1];
				goal = {x=cell.center.x*Map.groundRadius, y=cell.center.y*Map.groundRadius, z=cell.center.z*Map.groundRadius};
			end;
			local attacking = Mem.posture == 'attack';
			Move.Towards(v, goal, cell, not attacking, attacking);
		end);
	end;
end;

-- Scouts (radars) keep the map fresh: the core answer to sensor-limited play
function Military.Scouts()
	local taken = {};
	local scouts = {};
	for _,u in ipairs(Sense.my.radar) do scouts[#scouts+1] = u; end;
	for _,u in ipairs(Sense.my.surveyor) do scouts[#scouts+1] = u; end;

	for _,v in ipairs(scouts) do
		local deep = v.turret and v.turret.type.detectsResources;
		local layer = deep and 'deep' or 'ground';
		SafeCall('scout '..v.niceName, function()
			local myReg = Map.RegionOfLoc(v.location);
			local best, bestScore = nil, nil;
			for _,reg in ipairs(Map.regions) do
				if not taken[reg.id] then
					local hops = Sense.hops[reg.id] or 30;
					local stale = math.min(Sense.Staleness(reg, layer), 60);
					-- what we care about: frontier and enemy regions, and finding enemies at all
					local importance = 1;
					if Sense.frontier[reg.id] then importance = importance + 3; end;
					if reg.hostileStructures > 0 then importance = importance + 2; end;
					if #Sense.enemyRegions == 0 then importance = importance + 1; end;
					local danger = Sense.Threat(reg);
					local score = stale * importance / (1 + hops) / (1 + danger);
					if deep then
						-- surveyors: short horizontal range, so they patrol close to home.
						-- With suspected underground activity they sweep my own regions first
						score = score / (1 + hops * hops);
						if Sense.DeepMatters() and Sense.mine[reg.id] then
							score = score * 4;
						end;
					end;
					if stale >= Cfg.staleRounds then
						if bestScore == nil or score > bestScore then
							best, bestScore = reg, score;
						end;
					end;
				end;
			end;
			if best == nil then
				return;
			end;
			taken[best.id] = true;
			v.ClearSafeTasks();
			-- scouts may step into the unknown; that's their job. They still avoid known fire
			Move.Towards(v, Map.WorldCenter(best), best.centerCell, true, true);
		end);
	end;
end;

-- AA follows the largest concentration of my units
function Military.AA()
	-- spread AA over my regions, most air-threatened and most built-up first
	local targets = {};
	for id,_ in pairs(Sense.mine) do
		local reg = Map.regions[id];
		local nearAir = reg.airThreat;
		for nb,_ in pairs(reg.neighbors) do nearAir = nearAir + Map.regions[nb].airThreat * 0.5; end;
		targets[#targets+1] = {reg=reg, score=reg.myStructures + nearAir * 5};
	end;
	table.sort(targets, function(a,b) return a.score > b.score end);
	for i,v in ipairs(Sense.my.aa) do
		SafeCall('aa '..v.niceName, function()
			Military.Fire(v);
			local t = targets[((i-1) % math.max(1,#targets)) + 1];
			if t then
				Move.Towards(v, Map.WorldCenter(t.reg), t.reg.centerCell, true, false);
			end;
		end);
	end;
end;

-- Stationary towers just shoot
function Military.Towers()
	for _,s in ipairs(Sense.myStructures) do
		if s.turret and s.turret.type.canAttack and s.Attack then
			SafeCall('tower '..s.niceName, Military.Fire, s);
		end;
	end;
end;


-------------------------------------------------------------------------------
-- Entry points
-------------------------------------------------------------------------------

function Execute()
	local timer = Game.MeasureSecondsFromNow();
	if not Game.myFaction then
		Game.Log(Cfg.logPrefix..'No faction (yet)');
		return;
	end;
	if not SafeCall('Types.Resolve', Types.Resolve) then return; end;
	if not SafeCall('Map.Build', Map.Build) then return; end;
	if Mem.pendingImport ~= nil then
		SafeCall('ApplyImport', ApplyImport);
	end;
	if not SafeCall('Sense.Update', Sense.Update) then return; end;

	SafeCall('Research', Economy.Research);
	SafeCall('Production', Economy.Production);
	SafeCall('Constructors', Economy.Constructors);
	SafeCall('Posture', Military.UpdatePosture);
	SafeCall('Towers', Military.Towers);
	SafeCall('Combat', Military.Combat);
	SafeCall('Scouts', Military.Scouts);
	SafeCall('AA', Military.AA);

	local cur = Game.myFaction.currentResources;
	local seen, seenA, seenD = 0, 0, 0;
	for _,reg in ipairs(Map.regions) do
		if reg.lastSeen >= 0 then seen = seen + 1; end;
		if reg.lastSeenAir >= 0 then seenA = seenA + 1; end;
		if reg.lastSeenDeep >= 0 then seenD = seenD + 1; end;
	end;
	Game.Log(string.format('%sround %d | %s | units c%d m%d r%d s%d aa%d | structures %d | seen g%d a%d d%d /%d, enemy %d%s%s | minerals %d energy %d | %.2fs',
		Cfg.logPrefix, Sense.round, Mem.posture..(Mem.attackTarget and ('->'..Mem.attackTarget) or ''),
		#Sense.my.constructor, #Sense.my.combat, #Sense.my.radar, #Sense.my.surveyor, #Sense.my.aa,
		#Sense.myStructures, seen, seenA, seenD, #Map.regions, #Sense.enemyRegions,
		Sense.AirMatters() and ' AIR' or '', Sense.DeepMatters() and ' DEEP' or '',
		cur.storage.minerals or 0, cur.balance.energy or 0, timer()));
end;

function OnMessage(sender--[[Faction]], message--[[string]])
	if sender == nil then return; end;
	local m = string.lower(message or '');
	if string.find(m, 'status', 1, true) then
		Game.SendMessage(sender, string.format('Round %d. Posture: %s. Combat units: %d.',
			Sense.round, Mem.posture, #(Sense.my.combat or {})));
	elseif string.find(m, 'help', 1, true) or string.find(m, 'attack', 1, true) then
		if sender.alliance == Game.myFaction.alliance then
			local target = Military.ChooseAttackTarget();
			if target then
				Mem.posture = 'attack';
				Mem.attackTarget = target;
				Game.SendMessage(sender, 'On my way to the nearest enemy position I know of.');
			else
				Game.SendMessage(sender, "I don't know of any enemy positions yet. Scouting.");
			end;
		end;
	end;
end;

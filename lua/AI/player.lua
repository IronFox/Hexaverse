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
	nexusAirPrepRound = 50,		-- the Nexus starts building planes (default Rules/Nexus/minRoundForPlanes = 80). Unseen bombers are lethal: have AA before then
	aaTowersPerRegion = 1,		-- AA towers per built-up region once air matters

	-- posture
	attackMinUnits = 8,			-- combat units needed before attacking
	attackMinRound = 25,
	retreatBelowUnits = 3,		-- attack group falls back below this size

	-- memory
	mobileThreatDecay = 0.85,	-- per round, for remembered (unseen) mobile threat
	staleRounds = 12,			-- region considered stale after this many unseen rounds

	-- vehicle stores (shared park: units stored in one can be deployed at any other)
	maxStores = 5,
	garrison = 6,				-- active combat units kept out while defending; the rest wait in stores
	maxDeploysPerRound = 6,

	-- structures
	wantLabs = 3,
	wantTowersPerFrontier = 1,
	-- economy
	energyHeadroom = 2,	-- build power while spare energy (all structures enabled) is below this
	minMines = 3,		-- mines rebuilt even under pressure
	earlyMines = 3,		-- mines before towers while home is quiet (+1 per 10 rounds)
	roadMinRound = 20,		-- roads once the economy is up
	roadMinerals = 200,		-- and minerals pile up
	roadRepaveRounds = 80,
	roadMaxPressure = 3,	-- no road works while threatened more than this (see Sense.Pressure)	-- each road target at most once in this many rounds
	plantFreeCells = 3,
	mineCandidates = 6,		-- resource cells a constructor tries per mine order		-- free cells to keep around each vehicle plant for deployment
	stuckProductionRounds = 15,	-- abort a plant's head production after this many rounds without progress
	brokeMinerals = 60,	-- below this, plants queue one vehicle at a time
	maxMinesPerRound = 2,

	-- research, in order of preference. Matched against topic fullName (case-insensitive substring)
	researchReplanRounds = 10,	-- re-rank improvement research this often
	waterPlanetShare = 0.6,		-- known surface water share from which catamarans are the main base
	solidPlanetShare = 0.15,	-- below this, heavy bases are the main base
	research = {
		'Unlock:ReactorCore',
		'Unlock:MissileLauncher',
		'Unlock:HeavyBase',
		'Unlock:Artillery',
		-- endless improvements are chosen by usage (Economy.ImprovementUsage)
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
	Types.towerByTurret = {};
	Types.radarTower = nil;
	Types.aaTower = nil;
	for name,t in pairs(u) do
		if t.isStructure and t.turretType then
			local tn = t.turretType.name;
			Types.towerByTurret[tn] = t;
			if tn == 'Cannon' then Types.cannonTower = t;
			elseif tn == 'Radar' then Types.radarTower = t;
			elseif tn == 'Anti Aircraft' then Types.aaTower = t;
			elseif tn == 'Surveyor' then Types.scannerTower = t;
			elseif tn == 'Missile Launcher' then Types.launcherTower = t;
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

-- Best unlocked anti-ground tower (the Cannon Tower is deprecated and never unlockable)
function Types.GroundTower()
	for _,name in ipairs({'Rocket Launcher', 'Artillery', 'Cannon'}) do
		local t = Types.towerByTurret and Types.towerByTurret[name];
		if t and Types.IsUnlocked(t) then
			return t;
		end;
	end;
	return nil;
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
	-- water planets: catamarans carry the fight (constructors stay on hover, see NextVehicle)
	if Types.base.cat and Types.IsUnlocked(Types.base.cat) and Economy and Economy.PlanetStyle() == 'cat' then
		return Types.base.cat;
	end;
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
	powerCenters = {},		-- cell indices kept free for reactor cores; generators are built in rings around them
	airSeen = -1,			-- round hostile air activity (planes, plane factories) was last sensed
	deepSeen = -1,			-- round hostile underground activity was last sensed or suspected
	heavySeen = -1,			-- round hostile heavy vehicles were last sensed
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
		heavy = Mem.heavySeen,
		power = Mem.powerCenters,
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
	Mem.heavySeen = s.heavy or -1;
	Mem.powerCenters = {};
	if type(s.power) == 'table' then
		for _,c in ipairs(s.power) do
			if type(c) == 'number' then Mem.powerCenters[#Mem.powerCenters+1] = c; end;
		end;
	end;
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
	Sense.stores = {};
	Sense.stored = {};
	Sense.storedByRole = {};
	Sense.structureCells = {};
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
		if u.type.isMobile and u.storedIn ~= nil then
			local role = Types.Role(u) or 'other';
			Sense.stored[#Sense.stored+1] = u;
			Sense.storedByRole[role] = (Sense.storedByRole[role] or 0) + 1;
		elseif u.type.isMobile then
			local role = Types.Role(u) or 'other';
			local list = Sense.my[role];
			list[#list+1] = u;
			if reg then
				reg.myMobiles = reg.myMobiles + 1;
				reg.myStrength = reg.myStrength + DamagePotential(u);
			end;
		else
			Sense.myStructures[#Sense.myStructures+1] = u;
			Sense.structureCells[u.location.cell.index] = true;
			local tn = u.type.name;
			Sense.structuresByType[tn] = (Sense.structuresByType[tn] or 0) + 1;
			if u.type.isMobileEntityProducer and not u.type.isHangar then
				Sense.plants[#Sense.plants+1] = u;
			end;
			if u.type.isHangar then
				Sense.stores[#Sense.stores+1] = u;
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
			if e.type.isMobile and e.type.vehicleBaseType and e.type.vehicleBaseType.name == 'Heavy Base' then
				Mem.heavySeen = round;
			end;
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

	-- losses: my mobiles that disappeared since last round (stored units don't count as gone)
	local now = {};
	for _,list in pairs(Sense.my) do for _,u in ipairs(list) do now[u.id] = Types.Role(u) or 'other'; end; end;
	for _,u in ipairs(Sense.stored) do now[u.id] = 'stored'; end;
	local lost, lostConstructors = 0, 0;
	if Sense.prevMobiles then
		for id,role in pairs(Sense.prevMobiles) do
			if now[id] == nil then
				lost = lost + 1;
				if role == 'constructor' then lostConstructors = lostConstructors + 1; end;
			end;
		end;
	end;
	Sense.prevMobiles = now;
	Sense.lossLog = Sense.lossLog or {};
	Sense.lossLog[round] = {lost, lostConstructors};
	Sense.lossLog[round - 12] = nil;
	if lost > 0 then
		Log('Lost '..lost..' unit(s) this round ('..lostConstructors..' constructor(s))');
	end;
	Sense.nexus = Sense.NexusPresent();
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

-- Losses within the last n rounds: {all, constructors}
function Sense.RecentLosses(n)
	local a, c = 0, 0;
	for r = Sense.round - n + 1, Sense.round do
		local e = Sense.lossLog and Sense.lossLog[r];
		if e then a, c = a + e[1], c + e[2]; end;
	end;
	return a, c;
end;

-- How much hostile pressure is on my territory right now (sensed or remembered), 0 = none
function Sense.Pressure()
	local p = 0;
	for id,_ in pairs(Sense.mine or {}) do
		local reg = Map.regions[id];
		p = p + reg.mobileThreat;
		for nb,_ in pairs(reg.neighbors) do p = p + Map.regions[nb].mobileThreat * 0.5; end;
	end;
	local lost = Sense.RecentLosses(6);
	return p + lost * 3;
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
function Sense.NexusPresent()
	for _,f in pairs(Game.factions) do
		if f.name == 'Nexus' then return true; end;
	end;
	return false;
end;

function Sense.AirMatters()
	if Mem.airSeen >= 0 and Sense.round - Mem.airSeen < 80 then
		return true;
	end;
	-- Against the Nexus, prepare before its planes arrive. ShouldBuildPlanes() reflects the server's
	-- public rule (minRoundForPlanes), not anything sensed
	if Sense.nexus then
		if Sense.round >= Cfg.nexusAirPrepRound then return true; end;
		local ok, rs = pcall(function() return Game.AI and Game.AI.ShouldBuildPlanes and Game.AI.ShouldBuildPlanes() end);
		if ok and rs then return true; end;
	end;
	return false;
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

-- Energy output is gone (or about to be): no generator, or nothing feeds the generators
function Economy.PowerCritical()
	local _, max = Economy.Resources();
	return Economy.Count(Types.generator) == 0 or ((max.supply or {}).energy or 0) <= 0;
end;

-- Minerals to keep back while the power base is thin: enough to rebuild a generator and a fuel mine
function Economy.PowerReserve()
	local _, max = Economy.Resources();
	if Economy.Count(Types.generator) >= 2 and ((max.supply or {}).fuel or 0) > 0 then
		return 0;
	end;
	local g = Types.generator and Types.generator.constructionCost or 0;
	local m = Types.mine and Types.mine.constructionCost or 0;
	return g + m;
end;

function Economy.Count(t)
	if t == nil then return 0; end;
	return Sense.structuresByType[t.name] or 0;
end;

local function NormName(n)
	return string.lower(string.gsub(n or '', '[%s_%-]', ''));
end;

--[[
How much each endless improvement topic would pay off right now, based on what
the faction actually fields. Improvement research absorbs whatever research
capacity the unlock topics leave over, so focusing it on the most used unit
types matters.
@return {normalized topic key -> score}, key = group..':'..name, e.g. 'attackrange:cannon'
]]
--[[
Share of the known surface that is water, from what this faction has seen.
Decides which vehicle base gets health research: catamaran on water planets,
heavy on solid ones, hover only on very mixed planets. Refreshed every 25 rounds.
]]
Economy.waterInfo = nil;
function Economy.WaterShare()
	local wi = Economy.waterInfo;
	if wi and Sense.round - wi.round < 25 then
		return wi.share, wi.known;
	end;
	local C = Game.Terrain and Game.Terrain.Content;
	local water, known = 0, 0;
	if C and Game.Terrain.GetContent and Game.Terrain.TranslateUnitCell then
		for _,uc in ipairs(Game.Grid.unitCells) do
			local ok, tc = pcall(Game.Terrain.TranslateUnitCell, uc);
			if ok and tc then
				-- top-down: the first non-clear content is the surface
				for h = 31, 0, -1 do
					local ok2, c = pcall(Game.Terrain.GetContent, tc, h);
					if ok2 and c ~= nil and c ~= C.Clear then
						if c ~= C.Unknown then
							known = known + 1;
							if c == C.Water then water = water + 1; end;
						end;
						break;
					end;
				end;
			end;
		end;
	end;
	local share = known > 0 and water / known or 0;
	Economy.waterInfo = {round=Sense.round, share=share, known=known};
	return share, known;
end;

-- 'cat', 'heavy' or 'mixed'
function Economy.PlanetStyle()
	local share, known = Economy.WaterShare();
	if known < 20 then return 'heavy'; end;
	if share >= Cfg.waterPlanetShare then return 'cat'; end;
	if share <= Cfg.solidPlanetShare then return 'heavy'; end;
	return 'mixed';
end;

--[[
How much each endless improvement topic would pay off right now, based on what
the faction actually fields. Improvement research absorbs whatever research
capacity the unlock topics leave over, so it is focused:
  attack range   on the turrets used most (mobile and towers)
  scanning range on radars (and sensor planes / scanners if such topics exist);
                 AA only while air matters
  health         on the base that suits the planet: catamaran on water planets,
                 heavy on solid ones, hover only on very mixed ones
@return {normalized topic key -> score}, key = group..':'..name, e.g. 'attackrange:cannon'
]]
function Economy.ImprovementUsage()
	local score = {};
	local function Add(k, v) score[k] = (score[k] or 0) + v; end;
	local air = Sense.AirMatters();
	for _,u in pairs(Game.myUnits) do
		local t = u.turret and u.turret.type;
		local tn = t and NormName(t.name);
		local mobile = u.type.isMobile;
		if t and t.canAttack then
			Add('attackrange:'..tn, mobile and 2 or 1.5);
		end;
		if t and not t.canAttack then
			-- sensors: radar first, scanners mostly look down (odd cases only)
			if tn == 'radar' then Add('scanningrange:radar', mobile and 1.5 or 1);
			elseif t.detectsResources then Add('scanningrange:'..tn, 0.3);
			elseif u.type.isSuborbitalPlane then Add('scanningrange:'..tn, 1.5);
			end;
		end;
		if tn == 'antiaircraft' and air then Add('scanningrange:antiaircraft', 1); end;
		local base = mobile and u.type.vehicleBaseType;
		if base and base.name then Add('health:'..NormName(base.name), (t and t.canAttack) and 2 or 1); end;
	end;
	if air then
		Add('attackrange:antiaircraft', 3);
		Add('scanningrange:antiaircraft', 2);
	end;
	-- health research goes to the base that suits the planet
	local style = Economy.PlanetStyle();
	for k,v in pairs(score) do
		if string.sub(k, 1, 7) == 'health:' then
			local b = string.sub(k, 8);
			local f = 1;
			if b == 'hoverbase' then f = (style == 'mixed') and 1 or 0.2;
			elseif b == 'heavybase' then f = (style == 'heavy') and 1.5 or 0.5;
			elseif b == 'catamaranbase' then f = (style == 'cat') and 1.5 or 0.5;
			end;
			score[k] = v * f;
		end;
	end;
	return score;
end;

local function TopicKey(topic)
	local full = string.lower(topic.fullName or '');
	local name = NormName(topic.name);
	for _,g in ipairs({'attack range', 'scanning range', 'health'}) do
		if string.find(full, g, 1, true) then
			return NormName(g)..':'..name;
		end;
	end;
	return nil;
end;

local function TopicAvailable(topic)
	return not topic.isCompleted and topic.max ~= 0
		and (topic.dependency == nil or topic.dependency.isCompleted);
end;

Economy.researchPlanRound = -100;

--[[
Keeps the research queue full: pending unlocks from Cfg.research first, the rest
of the queue filled with the endless improvements that pay off most. The engine
spends research on the queue in order, and an endless topic takes all that is
left, so an idle or short queue wastes capacity.
]]
function Economy.Research()
	local queue = Game.Research.myQueue or {};
	local maxLen = Game.Research.maxQueueLength or 3;

	-- unlocks wanted next, in priority order
	local unlocks = {};
	local style = Economy.PlanetStyle();
	for _,want in ipairs(Cfg.research) do
		local w = string.lower(want);
		if style == 'cat' then w = string.gsub(w, 'heavybase', 'catamaranbase'); end;
		if string.find(w, 'unlock', 1, true) then
			local parts = {};
			for p in string.gmatch(w, '[^:]+') do parts[#parts+1] = p; end;
			for name,topic in pairs(Game.Research.topics) do
				local fn = string.lower(topic.fullName or name);
				local match = not topic.isEndless;
				for _,p in ipairs(parts) do
					if not string.find(fn, p, 1, true) then match = false; break; end;
				end;
				if match and TopicAvailable(topic) then
					unlocks[#unlocks+1] = topic;
					break;
				end;
			end;
		end;
	end;

	-- improvements ranked by current usage (re-planned every few rounds to avoid churn)
	local replan = Sense.round - Economy.researchPlanRound >= Cfg.researchReplanRounds;
	local improvements = {};
	if replan or Economy.researchPlan == nil then
		local usage = Economy.ImprovementUsage();
		for name,topic in pairs(Game.Research.topics) do
			if topic.isEndless and TopicAvailable(topic) then
				local key = TopicKey(topic);
				local sc = key and usage[key] or 0;
				if topic.isQueued then sc = sc * 1.3; end;	-- hysteresis: don't flip-flop between close candidates
				if sc > 0 then
					improvements[#improvements+1] = {topic=topic, score=sc};
				end;
			end;
		end;
		table.sort(improvements, function(a,b) return a.score > b.score end);
		Economy.researchPlan = improvements;
		Economy.researchPlanRound = Sense.round;
	end;
	improvements = Economy.researchPlan;

	-- desired queue: up to 2 unlocks (keep at least one slot for an improvement), rest improvements
	local desired, want = {}, {};
	local unlockSlots = math.min(#unlocks, math.max(1, maxLen - 1), 2);
	for i = 1, unlockSlots do desired[#desired+1] = unlocks[i]; end;
	for _,e in ipairs(improvements) do
		if #desired >= maxLen then break; end;
		desired[#desired+1] = e.topic;
	end;
	for _,t in ipairs(desired) do want[t.fullName] = true; end;

	-- drop improvements that are no longer among the best (progress is kept)
	local queued = {};
	local len = #queue;
	for _,t in ipairs(queue) do
		if t.isEndless and not want[t.fullName] and replan and pcall(t.Dequeue) then
			Log('Research: dropping '..t.fullName);
			len = len - 1;
		else
			queued[t.fullName] = true;
		end;
	end;
	for _,t in ipairs(desired) do
		if len >= maxLen then break; end;
		if not queued[t.fullName] and not t.isQueued then
			Log('Research: '..t.fullName);
			t.Enqueue();
			len = len + 1;
		end;
	end;
end;

-- Mine request, unless this round already showed there is no usable site for that resource
Economy.noMineSite = {};
function Economy.MineReq(resource)
	if Economy.noMineSite[resource] == Sense.round then
		return nil;
	end;
	return {type=Types.mine, kind='mine', resource=resource};
end;

-- What should the next idle constructor build? Returns {type, kind} or nil
function Economy.NextStructure(constructorIndex)
	local cur, max = Economy.Resources();
	local bal = max.balance;
	local round = Sense.round;

	-- no energy output means the faction is as good as dead. Rebuilding one generator and
	-- one fuel mine beats everything else, including the vehicle plant
	if Economy.PowerCritical() then
		if ((max.supply or {}).fuel or 0) <= 0 and constructorIndex % 2 == 0 then
			do local m = Economy.MineReq('Fuel'); if m then return m; end; end;
		end;
		if Economy.powerOrders < 2 then
			Economy.powerOrders = Economy.powerOrders + 1;
			return {type=Types.generator, kind='base'};
		end;
		if ((max.supply or {}).fuel or 0) <= 0 then
			do local m = Economy.MineReq('Fuel'); if m then return m; end; end;
		end;
	end;
	if Types.plant and Economy.WorkingPlants() == 0 then
		return {type=Types.plant, kind='base'};
	end;
	-- a spare fuel mine: losing the only one starves every generator
	if round >= 10 and Economy.Count(Types.generator) >= 1 and Economy.FuelMines() < math.min(2, Economy.Count(Types.generator)) then
		do local m = Economy.MineReq('Fuel'); if m then return m; end; end;
	end;
	-- energy first: everything else depends on it. Keep some headroom so the next mine
	-- (or a vehicle plant) runs as soon as it is finished instead of sitting disabled
	if bal.energy < Cfg.energyHeadroom or (cur.balance.energy <= 0 and Economy.Count(Types.generator) < 2) then
		if (bal.fuel or 0) <= 0 and (constructorIndex % 2 == 1 or Economy.Count(Types.mine) == 0) then
			do local m = Economy.MineReq('Fuel'); if m then return m; end; end;
		end;
		-- a generator without spare fuel produces nothing: fuel first
		if (bal.fuel or 0) <= 0 and Economy.Count(Types.generator) >= 1 then
			do local m = Economy.MineReq('Fuel'); if m then return m; end; end;
		elseif Economy.powerOrders < 2 then
			Economy.powerOrders = Economy.powerOrders + 1;
			return {type=Types.generator, kind='power'};
		end;
	end;
	-- a floor of mineral income, even under pressure: without it nothing else gets built
	if Economy.Count(Types.mine) < Cfg.minMines then
		do local m = Economy.MineReq('Minerals'); if m then return m; end; end;
	end;
	-- income before towers: a broke faction cannot build defenses or tanks either.
	-- Only while home is quiet; under attack the defense below comes first
	local wantMines = math.min(Cfg.earlyMines + math.floor(round / 10), 10);
	if Economy.Count(Types.mine) < wantMines and Sense.Pressure() <= 0 then
		local res = 'Minerals';
		if (bal.fuel or 0) <= 0 then res = 'Fuel'; end;
		do local m = Economy.MineReq(res); if m then return m; end; end;
	end;
	-- a reactor core in the middle of a generator ring, once unlocked and fed with uranium
	if Types.reactor and Types.IsUnlocked(Types.reactor) and Economy.ReactorSiteReady() then
		local uranium = (max.balance.uranium or 0) > 0 or (cur.storage.uranium or 0) > 0;
		if uranium then
			return {type=Types.reactor, kind='power'};
		end;
		if constructorIndex == 2 then
			do local m = Economy.MineReq('Uranium'); if m then return m; end; end;
		end;
	end;
	-- minerals piling up means the plants are the bottleneck
	-- (the 500 starting minerals don't count: only with real income)
	if Types.plant and round >= 25 and Economy.Count(Types.mine) >= 4
		and (cur.storage.minerals or 0) >= 300 and Economy.Count(Types.plant) < 3 then
		return {type=Types.plant, kind='base'};
	end;
	-- the home region gets its first defense early; raids start around round 15
	if round >= 4 then
		local need = Economy.DefenseNeed(false, true);
		if need then
			return need;
		end;
	end;
	if Types.lab and Economy.Count(Types.lab) == 0 and round >= 3 then
		return {type=Types.lab, kind='base'};
	end;
	-- a tower on a threatened frontier
	if Types.GroundTower() and constructorIndex == 1 then
		for id,_ in pairs(Sense.frontier) do
			local reg = Map.regions[id];
			if reg.mobileThreat > 0 and Economy.TowersIn(reg) < Cfg.wantTowersPerFrontier then
				return {type=Types.GroundTower(), kind='tower', region=id};
			end;
		end;
	end;
	-- layered defense of built-up regions (see Economy.DefenseNeed)
	local need = Economy.DefenseNeed(false);
	if need then
		return need;
	end;
	if Economy.Count(Types.mine) < 4 + math.floor(round / 10) then
		local res = 'Minerals';
		if bal.fuel <= 0 then res = 'Fuel'; end;
		do local m = Economy.MineReq(res); if m then return m; end; end;
	end;
	if Types.lab and Economy.Count(Types.lab) < Cfg.wantLabs and round >= 15 then
		return {type=Types.lab, kind='base'};
	end;
	if Types.plant and Economy.Count(Types.plant) < 2 and round >= 30 then
		return {type=Types.plant, kind='base'};
	end;
	-- a vehicle store next to the plants and in every defended region: reserves can appear anywhere we have one
	if Types.store and round >= 12 and Types.IsUnlocked(Types.store) and Economy.Count(Types.store) < Cfg.maxStores then
		local want = Economy.StoreRegion();
		if want then
			return {type=Types.store, kind='tower', region=want};
		end;
	end;
	-- surplus: thicken the defense where the threat is highest
	local extra = Economy.DefenseNeed(true);
	if extra then
		return extra;
	end;
	local gt = Types.GroundTower();
	if gt and Economy.Count(gt) < 2 + math.floor(round / 20) then
		return {type=gt, kind='tower'};
	end;
	if Types.radarTower and Types.IsUnlocked(Types.radarTower) and Economy.Count(Types.radarTower) < 1 + math.floor(round / 25) then
		return {type=Types.radarTower, kind='tower'};
	end;
	do local m = Economy.MineReq('Minerals'); if m then return m; end; end;
end;

--[[
Defense ladder. Coverage wins or loses fights, and every layer that is covered
shifts the enemy's best answer to the next one:
  sensor only      -> attacked from the air      => AA
  + AA             -> attacked by tanks          => ground tower (cannon)
  + cannon         -> attacked by heavy tanks    => missile launcher tower
  + launcher       -> attacked from underground  => scanner tower
Layers are added in the order the enemy has shown the matching capability
(air/heavy/deep), with the ground tower always first. Regions are served
most valuable first. With surplus=true, adds a second ground/launcher tower
where remembered threat is high (more units can always beat a fixed defense).
@return {type, kind='tower', region} or nil
]]
function Economy.DefenseNeed(surplus, homeOnly)
	local regions = {};
	for id,_ in pairs(Sense.mine) do
		local reg = Map.regions[id];
		local value = reg.myStructures;
		for _,st in ipairs(Sense.myStructures) do
			if st.turret and not st.turret.type.canAttack and Map.RegionOfLoc(st.location) == reg then
				value = value + 2;	-- sensors are what the enemy wants to blind
			end;
			if (st.type == Types.generator or st.type == Types.mine) and Map.RegionOfLoc(st.location) == reg then
				value = value + 3;	-- losing power output (generators and the mines feeding them) is losing the game
			end;
		end;
		if homeOnly and id ~= Mem.homeRegion then
			value = 0;
		end;
		if value >= 2 or (homeOnly and id == Mem.homeRegion) then
			regions[#regions+1] = {reg=reg, value=value};
		end;
	end;
	table.sort(regions, function(a,b) return a.value > b.value end);

	local heavy = Mem.heavySeen >= 0 or Sense.round >= 60;
	for _,e in ipairs(regions) do
		local reg = e.reg;
		local id = reg.id;
		local c = Economy.DefenseCounts(reg);
		local ladder = {};
		if surplus then
			local threat = Sense.Threat(reg);
			for nb,_ in pairs(reg.neighbors) do threat = threat + Map.regions[nb].mobileThreat; end;
			if threat > 10 then
				ladder[#ladder+1] = {Types.GroundTower(), c.ground < 2};
				ladder[#ladder+1] = {Types.launcherTower, heavy and c.launcher < 2};
				ladder[#ladder+1] = {Types.aaTower, Sense.AirMatters() and c.aa < 2};
			end;
		else
			ladder[#ladder+1] = {Types.GroundTower(), c.ground < 1};
			ladder[#ladder+1] = {Types.aaTower, Sense.AirMatters() and c.aa < Cfg.aaTowersPerRegion};
			ladder[#ladder+1] = {Types.launcherTower, heavy and c.launcher < 1};
			ladder[#ladder+1] = {Types.scannerTower, Sense.DeepMatters() and c.scanner < 1};
		end;
		for _,step in ipairs(ladder) do
			local t, wanted = step[1], step[2];
			local key = (t and t.name or '?')..'@'..id;
			if t and wanted and Types.IsUnlocked(t) and not Economy.scannerClaimed[key] then
				Economy.scannerClaimed[key] = true;
				return {type=t, kind='tower', region=id};
			end;
		end;
	end;
	return nil;
end;

-- Region that should get the next vehicle store: plant regions first, then defended regions, most valuable first
function Economy.StoreRegion()
	local hasStore = {};
	for _,st in ipairs(Sense.stores) do
		local reg = Map.RegionOfLoc(st.location);
		if reg then hasStore[reg.id] = true; end;
	end;
	for _,job in pairs(Mem.jobs) do
		if Types.store and job.kind == Types.store.name and job.region then hasStore[job.region] = true; end;
	end;
	for _,p in ipairs(Sense.plants) do
		local reg = Map.RegionOfLoc(p.location);
		if reg and not hasStore[reg.id] and not Economy.scannerClaimed['store'..reg.id] then
			Economy.scannerClaimed['store'..reg.id] = true;
			return reg.id;
		end;
	end;
	local best, bestV = nil, 3;
	for id,_ in pairs(Sense.mine) do
		local reg = Map.regions[id];
		local c = Economy.DefenseCounts(reg);
		local v = reg.myStructures + (c.ground + c.aa + c.launcher) * 2;
		if not hasStore[id] and not Economy.scannerClaimed['store'..id] and v > bestV then
			best, bestV = id, v;
		end;
	end;
	if best then Economy.scannerClaimed['store'..best] = true; end;
	return best;
end;

-- Counts my defensive structures in a region (including towers under construction there)
function Economy.DefenseCounts(reg)
	local c = {ground=0, aa=0, launcher=0, scanner=0};
	for _,st in ipairs(Sense.myStructures) do
		if st.turret and Map.RegionOfLoc(st.location) == reg then
			local tt = st.turret.type;
			if tt.detectsResources then c.scanner = c.scanner + 1;
			elseif tt.trajectoryType == 'AAMissile' then c.aa = c.aa + 1;
			elseif tt.name == 'Missile Launcher' then c.launcher = c.launcher + 1;
			elseif tt.canAttack then c.ground = c.ground + 1;
			end;
		end;
	end;
	for _,job in pairs(Mem.jobs) do
		if job.region == reg.id then
			if Types.scannerTower and job.kind == Types.scannerTower.name then c.scanner = c.scanner + 1;
			elseif Types.aaTower and job.kind == Types.aaTower.name then c.aa = c.aa + 1;
			elseif Types.launcherTower and job.kind == Types.launcherTower.name then c.launcher = c.launcher + 1;
			elseif Types.GroundTower() and job.kind == Types.GroundTower().name then c.ground = c.ground + 1;
			end;
		end;
	end;
	return c;
end;

function Economy.AATowersIn(reg)
	local n = 0;
	for _,s in ipairs(Sense.myStructures) do
		if s.turret and s.turret.type.trajectoryType == 'AAMissile' and Map.RegionOfLoc(s.location) == reg then
			n = n + 1;
		end;
	end;
	return n;
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
Economy.plantCells = {};	-- cell index -> cell of my vehicle plants, refreshed per round

function Economy.SiteOK(loc, forMine)
	local idx = loc.cell.index;
	if Economy.IsPowerCenter(idx) and not Economy.allowPowerCenter then return false; end;
	if Economy.IsPowerRing(idx) and not Economy.allowPowerRing then return false; end;
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
	-- never box in a vehicle plant: finished vehicles need somewhere to deploy
	for _,nb in ipairs(loc.cell.neighbors) do
		if Economy.plantCells[nb.index] then
			local pfree = 0;
			for _,pn in ipairs(Economy.plantCells[nb.index].neighbors) do
				if pn.index ~= idx and not Sense.structureCells[pn.index] and not Economy.claimed[pn.index] then pfree = pfree + 1; end;
			end;
			if pfree < Cfg.plantFreeCells then return false; end;
		end;
	end;
	-- the constructor must be able to move off the finished structure
	local free = 0;
	for _,nb in ipairs(loc.cell.neighbors) do
		if not Sense.structureCells[nb.index] and not Economy.claimed[nb.index] then free = free + 1; end;
	end;
	if free < 2 then return false; end;
	return true;
end;

--[[
Power: generators work best in a ring around a reactor core. Keep a center cell
free, build generators on its neighbors, and put a reactor core in the middle
once unlocked. When a ring is full, start a new one.
]]
function Economy.IsPowerCenter(idx)
	for _,c in ipairs(Mem.powerCenters) do
		if c == idx then return true; end;
	end;
	return false;
end;

function Economy.IsPowerRing(idx)
	for _,c in ipairs(Mem.powerCenters) do
		local center = Economy.Cell(c);
		if center then
			for _,nb in ipairs(center.neighbors) do
				if nb.index == idx then return true; end;
			end;
		end;
	end;
	return false;
end;

Economy.cellByIndex = nil;
function Economy.Cell(idx)
	if Economy.cellByIndex == nil then
		Economy.cellByIndex = {};
		for _,c in ipairs(Game.Grid.unitCells) do Economy.cellByIndex[c.index] = c; end;
	end;
	return Economy.cellByIndex[idx];
end;

function Economy.StructureTypeAt(cell)
	for _,st in ipairs(Sense.myStructures) do
		if st.location.cell == cell then return st.type; end;
	end;
	return nil;
end;

function Economy.ReactorSiteReady()
	for _,idx in ipairs(Mem.powerCenters) do
		local center = Economy.Cell(idx);
		if center and Economy.StructureTypeAt(center) == nil and not Economy.claimed[idx] then
			local gens = 0;
			for _,nb in ipairs(center.neighbors) do
				if Economy.StructureTypeAt(nb) == Types.generator then gens = gens + 1; end;
			end;
			if gens >= 2 then return true; end;
		end;
	end;
	return false;
end;

-- Finds a new ring center near home: a free cell whose neighbors are free and level with it
function Economy.NewPowerCenter(u)
	local ok, reach = pcall(u.GetReach, nil, math.max(2, u.movementSpeed * 3), true, false, false, true);
	if not ok or reach == nil then return nil; end;
	local goal = Sense.baseCenter or u.location.world;
	local best, bestD = nil, nil;
	for _,n in ipairs(reach) do
		local loc = n.location;
		if not loc.isSuborbital and Economy.SiteOK(loc, false) then
			local good = 0;
			for _,nb in ipairs(loc.cell.neighbors) do
				local g = Move.GroundAt(u, nb);
				if g and g.height == loc.height and Economy.SiteOK(g, false) then good = good + 1; end;
			end;
			if good >= 4 then
				local d = Dist2(loc.world, goal) - good * 2;
				if bestD == nil or d < bestD then best, bestD = loc, d; end;
			end;
		end;
	end;
	if best then
		Mem.powerCenters[#Mem.powerCenters+1] = best.cell.index;
		Log('New power ring centered at cell '..best.cell.index);
		return best.cell.index;
	end;
	return nil;
end;

-- Site for a generator (ring around a center) or a reactor (the center itself)
function Economy.PowerSite(u, t)
	if t == Types.reactor then
		for _,idx in ipairs(Mem.powerCenters) do
			local center = Economy.Cell(idx);
			if center and Economy.StructureTypeAt(center) == nil and not Economy.claimed[idx] then
				local loc = Move.GroundAt(u, center);
				if loc then
					Economy.allowPowerCenter = true;
					local ok = Economy.SiteOK(loc, false);
					Economy.allowPowerCenter = false;
					if ok then return loc; end;
				end;
			end;
		end;
		return nil;
	end;
	for pass = 1,2 do
		for i = #Mem.powerCenters, 1, -1 do
			local center = Economy.Cell(Mem.powerCenters[i]);
			local ringFree = 0;
			local centerOpen = center and Economy.StructureTypeAt(center) == nil;
			if center then
				for _,nb in ipairs(center.neighbors) do
					if Economy.StructureTypeAt(nb) == nil and not Economy.claimed[nb.index] then ringFree = ringFree + 1; end;
				end;
			end;
			-- until the reactor core stands, keep one ring cell open so a constructor can reach the center
			if center and (not centerOpen or ringFree > 1) then
				for _,nb in ipairs(center.neighbors) do
					if not Economy.claimed[nb.index] and Economy.StructureTypeAt(nb) == nil then
						local loc = Move.GroundAt(u, nb);
						Economy.allowPowerRing = true;
						local ok = loc and Economy.SiteOK(loc, false);
						Economy.allowPowerRing = false;
						if ok then
							return loc;
						end;
					end;
				end;
			end;
		end;
		if pass == 1 and Economy.NewPowerCenter(u) == nil then
			return nil;
		end;
	end;
	return nil;
end;

-- Cells another constructor has already claimed this round
Economy.claimed = {};
Economy.scannerClaimed = {};
Economy.powerOrders = 0;

--[[
Best known resource cell for a new mine in safe, nearby territory.
The engine's global search only returns the single highest spot, which is often
far away or under fire. This scans my regions and their quiet neighbors instead
(cached per round), and falls back to the engine's pick if that one is safe.
]]
Economy.mineCache = {};
function Economy.FindMineCells(resource, maxCount)
	local cache = Economy.mineCache[resource];
	if cache == nil or cache.round ~= Sense.round then
		local list = {};
		local seen = {};
		local function Scan(reg)
			if seen[reg.id] then return; end;
			seen[reg.id] = true;
			if Sense.Threat(reg) > 0 or (Sense.hops[reg.id] or 99) > 2 then return; end;
			for _,cell in ipairs(reg.cells) do
				if not Sense.structureCells[cell.index] then
					local ok, lvl = pcall(Game.Grid.GetNaturalResourceAt, resource, cell);
					if ok and lvl and lvl > 0 then
						list[#list+1] = {cell=cell, level=lvl, hops=Sense.hops[reg.id] or 0};
					end;
				end;
			end;
		end;
		for id,_ in pairs(Sense.mine) do
			Scan(Map.regions[id]);
			for nb,_ in pairs(Map.regions[id].neighbors) do Scan(Map.regions[nb]); end;
		end;
		-- richer and closer first
		table.sort(list, function(a,b) return a.level / (1 + a.hops) > b.level / (1 + b.hops) end);
		cache = {round=Sense.round, list=list};
		Economy.mineCache[resource] = cache;
	end;
	local out = {};
	for _,e in ipairs(cache.list) do
		if #out >= (maxCount or 1) then break; end;
		local bl = Mem.blacklist[e.cell.index];
		if not Economy.claimed[e.cell.index] and not Sense.structureCells[e.cell.index] and not (bl and bl > Sense.round) then
			out[#out+1] = e.cell;
		end;
	end;
	if #out < (maxCount or 1) then
		local ok, c = pcall(Game.Grid.FindHighestAvailableResourceLocationExt, resource, true, true);
		if ok and c and not Economy.claimed[c.index] then out[#out+1] = c; end;
	end;
	return out;
end;

function Economy.FindMineCell(resource)
	return Economy.FindMineCells(resource, 1)[1];
end;

--[[
Roads. A one-time investment that makes every later trip faster.
Once the economy is stable, the last constructor paves a route from home to
the farthest of my built-up regions (mines, outposts) whose surroundings are
quiet. Roads are not built towards fronts, where fighting craters them. Each target
gets paved at most once per Cfg.roadRepaveRounds. Never into danger.
@return true if a road segment was scheduled
]]
Mem.roadsDone = Mem.roadsDone or {};
function Economy.RoadTarget()
	-- only through my own quiet territory: fights rip holes into the ground (and roads),
	-- and terraformers are priority targets for the Nexus, since they can take over hubs
	local best, bestH = nil, 1;
	for id,_ in pairs(Sense.mine) do
		local reg = Map.regions[id];
		local h = Sense.hops[id] or 0;
		local done = Mem.roadsDone[id];
		local quiet = Sense.Threat(reg) <= 0;
		for nb,_ in pairs(reg.neighbors) do
			if Map.regions[nb].mobileThreat > 0 then quiet = false; end;
		end;
		if reg.myStructures > 0 and h > bestH and quiet
			and not (done and Sense.round - done < Cfg.roadRepaveRounds) then
			best, bestH = id, h;
		end;
	end;
	return best;
end;

function Economy.TryRoad(u)
	if not u.BuildRoadTo or Sense.round < Cfg.roadMinRound or #Sense.my.constructor < 3 then return false; end;
	local cur = Economy.Resources();
	if (cur.storage.minerals or 0) < Cfg.roadMinerals or Economy.PowerCritical() or Sense.Pressure() > Cfg.roadMaxPressure then
		return false;
	end;
	-- start near home, so roads radiate from the base
	local here = Map.RegionOfLoc(u.location);
	if not here or (Sense.hops[here.id] or 99) > 1 then return false; end;
	local target = Economy.RoadTarget();
	if target == nil then return false; end;
	local reg = Map.regions[target];
	local dest = Move.GroundAt(u, reg.centerCell);
	if dest == nil then return false; end;
	local ok, path = pcall(u.FindShortestPath, dest, nil, false);
	if not ok or path == nil or #path < 3 then
		Mem.roadsDone[target] = Sense.round;
		return false;
	end;
	-- stop short of anything dangerous
	local pick = nil;
	for _,n in ipairs(path) do
		if Move.IsDangerous(n.location) then break; end;
		pick = n;
	end;
	if pick == nil then return false; end;
	local ok2, err = pcall(u.BuildRoadTo, pick);
	if not ok2 then
		Log('BuildRoadTo failed: '..tostring(err));
		Mem.roadsDone[target] = Sense.round;
		return false;
	end;
	Mem.roadsDone[target] = Sense.round;
	Mem.jobs[u.id] = {kind = 'Road', since = Sense.round, region = target};
	Log(u.niceName..': paving road to region '..target..' ('..#path..' steps)');
	return true;
end;

function Economy.AssignConstructor(u, index)
	local job = Mem.jobs[u.id];
	-- danger: get out first, build later
	local reg = Map.RegionOfLoc(u.location);
	if Move.IsDangerous(u.location) or (reg and reg.mobileThreat > reg.myStrength) then
		local safest, bestS = nil, -1;
		for id,_ in pairs(Sense.mine) do
			local r = Map.regions[id];
			local sc = r.myStrength - r.mobileThreat;
			if r ~= reg and sc > bestS then safest, bestS = r, sc; end;
		end;
		if safest then
			u.ClearSafeTasks();
			if job and job.cell then Mem.blacklist[job.cell] = Sense.round + 15; end;
			Mem.jobs[u.id] = nil;
			Move.Towards(u, Map.WorldCenter(safest), safest.centerCell, true, false);
			Log(u.niceName..': retreating from danger');
			return;
		end;
	end;
	if not Move.IsIdle(u) then
		-- a job that runs far too long is stuck (e.g. no room to relocate after constructing)
		-- (rounds without minerals don't count: construction simply pauses then)
		if job and (Game.myFaction.currentResources.storage.minerals or 0) >= Cfg.brokeMinerals then
			job.busy = (job.busy or 0) + 1;
		end;
		if job and (job.busy or 0) > (job.kind == 'Road' and 40 or 20) then
			Log(u.niceName..': '..job.kind..' at cell '..tostring(job.cell)..' is stuck. Dropping it');
			if job.cell then Mem.blacklist[job.cell] = Sense.round + 50; end;
			u.ClearTasks();
			Mem.jobs[u.id] = nil;
		end;
		return;	-- busy with a queued motion/construction
	end;
	-- detect stuck jobs: still idle at the same place where we ordered a construction
	if job and job.cell and Sense.round - job.since >= 1 and not Sense.structureCells[job.cell] then
		Mem.blacklist[job.cell] = Sense.round + 30;
		Log(u.niceName..': construction of '..job.kind..' at cell '..job.cell..' did not happen. Blacklisting the site');
	end;
	Mem.jobs[u.id] = nil;

	if index == #Sense.my.constructor and Economy.TryRoad(u) then
		return;
	end;

	local want = Economy.NextStructure(index);
	if want == nil or want.type == nil then
		return;
	end;

	if want.kind == 'power' then
		local loc = Economy.PowerSite(u, want.type);
		if loc and Economy.BuildAt(u, loc, want.type) then
			Economy.claimed[loc.cell.index] = true;
			return;
		end;
		if want.type == Types.reactor then
			return;
		end;
		want = {type = want.type, kind = 'base'};	-- no ring possible here: plain placement
	end;

	if want.kind == 'mine' then
		-- try the best few candidates; the first one is often blocked or crowded
		local resources = {want.resource};
		if want.resource ~= 'Minerals' then resources[2] = 'Minerals'; end;
		for _,res in ipairs(resources) do
			for _,cell in ipairs(Economy.FindMineCells(res, Cfg.mineCandidates)) do
				local dest = Move.GroundAt(u, cell);
				local reg = dest and Map.RegionOfLoc(dest);
				local safe = reg and Sense.Threat(reg) <= 0 and (Sense.hops[reg.id] or 99) <= 2;
				if dest and safe and Economy.SiteOK(dest, true) then
					if Economy.BuildAt(u, dest, want.type) then
						Economy.claimed[cell.index] = true;
						return;
					end;
				end;
			end;
		end;
		-- no safe known resource spot: remember that for this round and build the next best thing instead
		-- (a constructor waiting for a mine site while minerals pile up is a stalled economy)
		Economy.noMineSite[want.resource] = Sense.round;
		if want.resource ~= 'Minerals' then Economy.noMineSite['Minerals'] = Sense.round; end;
		local alt = Economy.NextStructure(index);
		if alt and alt.type and alt.kind ~= 'mine' and alt.type ~= Types.reactor then
			want = alt;
			if want.kind == 'power' then want = {type = want.type, kind = 'base'}; end;
		else
			want = Economy.DefenseNeed(true);
		end;
		if want == nil and Sense.round < 10 and Economy.Count(Types.generator) < 2 then
			want = {type = Types.generator, kind='base'};
		end;
		if want == nil then
			return;
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
	local reg = Map.RegionOfLoc(loc);
	Mem.jobs[u.id] = {kind = t.name, cell = loc.cell.index, since = Sense.round, at = loc.cell.index, region = reg and reg.id};
	-- count it right away, so other constructors assigned this frame don't pick the same structure
	Sense.structuresByType[t.name] = (Sense.structuresByType[t.name] or 0) + 1;
	Log(u.niceName..' -> build '..t.name..' at cell '..loc.cell.index);
	return true;
end;

-- What should a vehicle plant produce next?
function Economy.NextVehicle()
	local my = Sense.my;
	local base = Types.GroundBase();
	local hover = Types.base.hover;
	local function Constructor()
		-- heavy and catamaran bases terraform twice as fast: heavy as soon as it is unlocked.
		-- On water planets catamarans dig waterways, but every other constructor stays on hover to reach land
		local cb = Types.IsUnlocked(Types.base.heavy) and Types.base.heavy or hover;
		if base == Types.base.cat then
			local nCat = 0;
			for _,u in ipairs(my.constructor) do
				if u.type.vehicleBaseType == Types.base.cat then nCat = nCat + 1; end;
			end;
			cb = (nCat * 2 < #my.constructor) and Types.base.cat or hover;
		end;
		return Types.Vehicle(Types.turret.terraformer, cb) or Types.Vehicle(Types.turret.terraformer, hover);
	end;
	local function Tank()
		local nArt = 0;
		for _,u in ipairs(my.combat) do
			if u.turret.type == Types.turret.artillery then nArt = nArt + 1; end;
		end;
		if Types.IsUnlocked(Types.turret.artillery) and nArt * 3 < #my.combat then
			local a = Types.Vehicle(Types.turret.artillery, base) or Types.Vehicle(Types.turret.artillery, hover);
			if a then return a; end;
		end;
		return Types.Vehicle(Types.turret.cannon, base) or Types.Vehicle(Types.turret.cannon, hover);
	end;
	local combat = #my.combat + (Sense.storedByRole.combat or 0);
	local constructors = #my.constructor + (Sense.storedByRole.constructor or 0);
	local pressure = Sense.Pressure();

	-- never fewer than two constructors: without them there is no economy at all
	if constructors < 2 then
		return Constructor();
	end;
	-- no scanner at all means no resource coverage: always get one
	if #my.surveyor == 0 and (Sense.storedByRole.surveyor or 0) == 0 and Economy.ScannerTowers() == 0 then
		local sv = Types.Vehicle(Types.turret.surveyor, Types.base.heavy) or Types.Vehicle(Types.turret.surveyor, hover);
		if sv then return sv; end;
	end;
	-- no mine site but minerals to spend: scanners to find resources (below), tanks to conquer them
	if Economy.MineShortage() and (Game.myFaction.currentResources.storage.minerals or 0) >= 150
		and #my.surveyor >= 1 and combat < Cfg.retreatBelowUnits * 3 then
		return Tank();
	end;
	-- under pressure, tanks first. Replacing constructors that keep dying feeds the enemy
	if pressure > 0 and combat < 4 + math.floor(pressure / 3) then
		return Tank();
	end;
	local wantConstructors = Sense.round < 40 and Cfg.constructorsEarly or Cfg.constructorsLate;
	if constructors < wantConstructors and (combat >= constructors or Sense.round < 12) then
		return Constructor();
	end;
	if #my.radar < math.min(1, math.floor(combat / 2)) + math.floor(combat / Cfg.radarPerCombat) then
		return Types.Vehicle(Types.turret.radar, hover);
	end;
	if Sense.AirMatters() and #my.aa * Cfg.aaPerCombat < #my.combat + 2 then
		local aa = Types.Vehicle(Types.turret.aa, hover);
		if aa then return aa; end;
	end;
	local wantSurveyors = Sense.DeepMatters() and Cfg.maxSurveyorsDeep or Cfg.maxSurveyors;
	if Economy.MineShortage() then wantSurveyors = wantSurveyors + 1; end;
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

Economy.plantHead = {};	-- plant id -> {id=production id, since=round}
Economy.plantBlocked = {};	-- plant id -> round since which its finished vehicle cannot deploy

-- Vehicle plants that can actually deliver (not boxed in for 5+ rounds)
function Economy.WorkingPlants()
	local n = 0;
	for _,p in ipairs(Sense.plants) do
		local b = Economy.plantBlocked[p.id];
		if not (b and Sense.round - b >= 5) then n = n + 1; end;
	end;
	return n;
end;

-- Known, unclaimed resource cells in a region (cached per region for 10 rounds)
Economy.resCache = {};
function Economy.ResourceCells(reg)
	local c = Economy.resCache[reg.id];
	if c and Sense.round - c.round < 10 then return c.n; end;
	local n = 0;
	for _,cell in ipairs(reg.cells) do
		if not Sense.structureCells[cell.index] then
			local ok, res = pcall(Game.Grid.GetNaturalResourcesAt, cell);
			if ok and res and ((res.minerals or 0) > 0 or (res.fuel or 0) > 0) then n = n + 1; end;
		end;
	end;
	Economy.resCache[reg.id] = {round=Sense.round, n=n};
	return n;
end;

-- Too few mines and the last mine order found no site
function Economy.MineShortage()
	local last = Economy.noMineSite['Minerals'] or Economy.noMineSite['Fuel'];
	return Economy.Count(Types.mine) < Cfg.minMines + 1 and last ~= nil and Sense.round - last <= 5;
end;

function Economy.ScannerTowers()
	local n = 0;
	for _,st in ipairs(Sense.myStructures) do
		if st.turret and st.turret.type.detectsResources then n = n + 1; end;
	end;
	return n;
end;

-- Mine constructions currently ordered
function Economy.MineJobs()
	local n = 0;
	for _,job in pairs(Mem.jobs) do
		if Types.mine and job.kind == Types.mine.name then n = n + 1; end;
	end;
	return n;
end;

-- Mines sitting on fuel: they feed the generators, losing the last one starves power
function Economy.FuelMines()
	local n = 0;
	for _,st in ipairs(Sense.myStructures) do
		if st.type == Types.mine then
			local ok, lvl = pcall(Game.Grid.GetNaturalResourceAt, 'Fuel', st.location.cell);
			if ok and lvl and lvl > 0 then n = n + 1; end;
		end;
	end;
	return n;
end;

function Economy.Production()
	for _,p in ipairs(Sense.plants) do
		if p.productionCompleted or p.deploymentOptions then
			local ok, loc = pcall(p.GetIdealDeploymentOption);
			if not (ok and loc) and p.deploymentOptions then
				-- no ideal spot (e.g. crowded around the plant): any safe option beats a blocked plant
				for _,o in ipairs(p.deploymentOptions) do
					if not Move.IsDangerous(o) then loc = o; ok = true; break; end;
				end;
			end;
			if ok and loc then
				Log(p.niceName..': deploying '..tostring(p.productionCompleted));
				p.DeployProductionAt(loc);
				Economy.plantBlocked[p.id] = nil;
			else
				Economy.plantBlocked[p.id] = Economy.plantBlocked[p.id] or Sense.round;
				if Sense.round % 10 == 0 then
					Log(p.niceName..': cannot deploy '..tostring(p.productionCompleted)..', no deployment option');
				end;
			end;
		else
			Economy.plantBlocked[p.id] = nil;
		end;
		-- a production that does not progress for many rounds blocks the plant: drop it
		local head = p.productionQueue and p.productionQueue[1];
		if head and head.id == nil then head = nil; end;
		if head then
			local h = Economy.plantHead[p.id];
			if h == nil or h.id ~= head.id then
				Economy.plantHead[p.id] = {id=head.id, since=Sense.round};
			elseif Sense.round - h.since > Cfg.stuckProductionRounds and not p.productionCompleted
				and (Game.myFaction.currentResources.storage.minerals or 0) >= Cfg.brokeMinerals then
				Log(p.niceName..': production of '..tostring(head.name)..' is stuck. Aborting it');
				pcall(head.Abort);
				Economy.plantHead[p.id] = nil;
			end;
		else
			Economy.plantHead[p.id] = nil;
		end;
		local queued = p.productionQueue and #p.productionQueue or 0;
		-- while broke and short on mines, queue only one vehicle at a time so constructors get the minerals
		local cur = Economy.Resources();
		local broke = (cur.storage.minerals or 0) < Cfg.brokeMinerals
			and Economy.Count(Types.mine) < Cfg.earlyMines + 2
			and Sense.Pressure() <= 0;
		local maxQueue = broke and 1 or 2;
		local reserveBlocked = (cur.storage.minerals or 0) < Economy.PowerReserve() and #Sense.my.combat >= 2;
		-- while short on mines and broke, mines under construction get the minerals first
		if not reserveBlocked and broke and Economy.MineJobs() > 0 and #Sense.my.combat >= 2 then
			reserveBlocked = true;
		end;
		if queued < maxQueue and p.BuildMobile then
			local t = Economy.NextVehicle();
			-- never spend the power reserve on vehicles, except the minimum of constructors and a pinch of defense
			local essential = #Sense.my.constructor + (Sense.storedByRole.constructor or 0) < 2
				or (t and t.turretType == Types.turret.surveyor and #Sense.my.surveyor == 0);
			if t and reserveBlocked and not essential then
				t = nil;
			end;
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
	Economy.plantCells = {};
	for _,p in ipairs(Sense.plants) do
		Economy.plantCells[p.location.cell.index] = p.location.cell;
	end;
	Economy.powerOrders = 0;
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
	local n = Sense.storedByRole.combat or 0;
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
			-- short on mines: known resources in contested ground are worth conquering
			if Economy.MineShortage() or Economy.Count(Types.mine) < Cfg.minMines then
				value = value + Economy.ResourceCells(reg) * 2;
			end;
			local score = value / (1 + hops * 0.3) / (1 + Sense.Staleness(reg) * 0.02);
			if score > bestScore then
				best, bestScore = id, score;
			end;
		end;
	end;
	return best;
end;

-- Fixed defenses can always be beaten with more units: size the attack by what we remember of the target
function Military.RequiredForce(id)
	local reg = Map.regions[id];
	local need = Cfg.attackMinUnits + reg.hostileStructures * 2 + math.floor(reg.staticThreat / 4);
	for nb,_ in pairs(reg.neighbors) do
		need = need + math.floor(Map.regions[nb].staticThreat / 8);
	end;
	return need;
end;

function Military.UpdatePosture()
	local n = Military.ArmyStrength();
	if Mem.posture == 'defend' then
		local target = (Sense.round >= Cfg.attackMinRound) and Military.ChooseAttackTarget() or nil;
		if target and n >= Military.RequiredForce(target) then
			do
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

--[[
Vehicle stores form one shared park. Surplus defenders wait inside; when a
region is threatened or an attack starts, they are deployed from the store
nearest to where they are needed.
]]
Military.stashing = {};

function Military.NearestStore(regionId)
	if #Sense.stores == 0 then return nil; end;
	local hops = Map.Hops({regionId});
	local best, bestH = nil, nil;
	for _,st in ipairs(Sense.stores) do
		local reg = Map.RegionOfLoc(st.location);
		local h = reg and hops[reg.id] or 99;
		-- a store in a region with sensed hostile fire is a bad exit
		if reg and reg.seenNow and reg.mobileThreat > 0 then h = h + 2; end;
		if bestH == nil or h < bestH then best, bestH = st, h; end;
	end;
	return best, bestH;
end;

function Military.DeployFrom(store, count, goalWorld)
	local deployed = 0;
	local used = {};
	for _,u in ipairs(Sense.stored) do
		if deployed >= count then break; end;
		if Types.Role(u) == 'combat' and store.DeployStored then
			local ok, options = pcall(store.GetStoredDeploymentOptions, u);
			if ok and options and #options > 0 then
				local best, bestD = nil, nil;
				for _,loc in ipairs(options) do
					if not used[loc.cell.index] then
						local d = goalWorld and Dist2(loc.world, goalWorld) or 0;
						if bestD == nil or d < bestD then best, bestD = loc, d; end;
					end;
				end;
				if best then
					used[best.cell.index] = true;
					local ok2, err = pcall(store.DeployStored, u, best);
					if ok2 then
						deployed = deployed + 1;
					else
						Log('DeployStored failed: '..tostring(err));
					end;
				end;
			end;
		end;
	end;
	if deployed > 0 then
		Log('Deployed '..deployed..' stored unit(s) at '..store.niceName);
	end;
	return deployed;
end;

function Military.Stores()
	Military.stashing = {};
	if #Sense.stores == 0 then
		return;
	end;
	local budget = Cfg.maxDeploysPerRound;
	local storedCombat = Sense.storedByRole.combat or 0;

	-- attack: everything stored joins, exiting as close to the target as possible
	if storedCombat > 0 and Mem.posture == 'attack' and Mem.attackTarget then
		local st = Military.NearestStore(Mem.attackTarget);
		if st then
			budget = budget - Military.DeployFrom(st, budget, Map.WorldCenter(Map.regions[Mem.attackTarget]));
		end;
	end;

	-- defense: regions where remembered hostile strength exceeds what we have there
	if storedCombat > 0 and budget > 0 then
		for id,_ in pairs(Sense.mine) do
			if budget <= 0 then break; end;
			local reg = Map.regions[id];
			local threat = reg.mobileThreat;
			for nb,_ in pairs(reg.neighbors) do threat = threat + Map.regions[nb].mobileThreat * 0.5; end;
			local lack = threat - reg.myStrength;
			if threat > 0 and lack > 0 then
				local st = Military.NearestStore(id);
				if st then
					local want = math.min(budget, math.ceil(lack / 2));
					budget = budget - Military.DeployFrom(st, want, Map.WorldCenter(reg));
				end;
			end;
		end;
	end;

	-- stash surplus defenders that have nothing to do
	if Mem.posture == 'defend' and #Sense.my.combat > Cfg.garrison then
		local surplus = #Sense.my.combat - Cfg.garrison;
		for i = #Sense.my.combat, 1, -1 do
			if surplus <= 0 then break; end;
			local v = Sense.my.combat[i];
			local reg = Map.RegionOfLoc(v.location);
			if reg and Sense.Threat(reg) == 0 and v.CanStandAt then
				local ok, reach = pcall(v.GetReach, nil, math.max(2, v.movementSpeed * 2), false, true, true, true);
				if ok and reach then
					for _,n in ipairs(reach) do
						if n.hangar and (not n.hangar.CanStore or n.hangar.CanStore(v)) then
							v.ClearSafeTasks();
							v.DirectTo(n);
							Military.stashing[v.id] = true;
							surplus = surplus - 1;
							break;
						end;
					end;
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

-- Units whose turret only fires when the vehicle did not move (artillery, AA, ...)
function Military.NeedsStationary(v)
	return v.turret and v.turret.requiresStationaryVehicle and not v.motionIgnoresAttack;
end;

-- Firing posts for stationary shooters: the highest ground of a region, held until the goal changes
Military.posts = {};

local function Height(loc)
	local w = loc.world;
	return math.sqrt(w.x*w.x + w.y*w.y + w.z*w.z);
end;

-- Region a stationary shooter should hold: the goal itself while defending,
-- the neighbor of the attack target that is closest to home while attacking
function Military.PostRegion(goalId)
	if Mem.posture ~= 'attack' or goalId ~= Mem.attackTarget then
		return goalId;
	end;
	local best, bestH = goalId, nil;
	for nb,_ in pairs(Map.regions[goalId].neighbors) do
		local h = Sense.hops[nb] or 99;
		if Sense.Threat(Map.regions[nb]) <= 0 and (bestH == nil or h < bestH) then
			best, bestH = nb, h;
		end;
	end;
	return best;
end;

function Military.PostFor(v, regionId, taken)
	local post = Military.posts[v.id];
	if post and post.region == regionId and not taken[post.key] then
		return post;
	end;
	local reg = Map.regions[regionId];
	local best, bestH = nil, nil;
	local step = math.max(1, math.floor(#reg.cells / 16));
	for k = 1, #reg.cells, step do
		local cell = reg.cells[k];
		local key = regionId..':'..k;
		if not taken[key] then
			local loc = Move.GroundAt(v, cell);
			if loc and not Move.IsDangerous(loc) then
				local h = Height(loc);
				if bestH == nil or h > bestH then
					best, bestH = {region=regionId, key=key, cell=cell, loc=loc}, h;
				end;
			end;
		end;
	end;
	Military.posts[v.id] = best;
	return best;
end;

function Military.Combat()
	local defense = Military.DefenseRegion();
	local takenPosts = {};
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
			if Military.stashing[v.id] then
				return;	-- on its way into a vehicle store
			end;
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
			if Military.NeedsStationary(v) then
				-- artillery: get onto high ground and stay there. Moving costs it the round's fire
				local post = Military.PostFor(v, Military.PostRegion(goalId), takenPosts);
				if post then
					takenPosts[post.key] = true;
					if Dist2(v.location.world, post.loc.world) < 0.01 then
						if not Move.IsIdle(v) then v.ClearSafeTasks(); end;
						return;
					end;
					Move.Towards(v, post.loc.world, post.cell, true, false);
					return;
				end;
			end;
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
						-- short on mines and no known site: survey my nearby ground for resources
						if Economy.MineShortage() and (Sense.hops[reg.id] or 99) <= 2 then
							score = score * 6;
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
	-- while attacking, every other AA escorts the army (bombers hunt armies too)
	local cx, cy, cz, cn = 0, 0, 0, 0;
	for _,u in ipairs(Sense.my.combat) do
		local w = u.location.world;
		cx, cy, cz, cn = cx + w.x, cy + w.y, cz + w.z, cn + 1;
	end;
	local aaPosts = {};
	for i,v in ipairs(Sense.my.aa) do
		SafeCall('aa '..v.niceName, function()
			local fired = Military.Fire(v);
			if fired and not v.motionIgnoresAttack then
				return;	-- moving now would cancel this round's shot
			end;
			if Military.NeedsStationary(v) and Mem.posture ~= 'attack' then
				-- guarding: hold the highest ground of the assigned region
				local t = targets[((i-1) % math.max(1,#targets)) + 1];
				local post = t and Military.PostFor(v, t.reg.id, aaPosts);
				if post then
					aaPosts[post.key] = true;
					if Dist2(v.location.world, post.loc.world) >= 0.01 then
						Move.Towards(v, post.loc.world, post.cell, true, false);
					end;
					return;
				end;
			end;
			if Mem.posture == 'attack' and cn > 0 and i % 2 == 0 then
				local c = {x=cx/cn, y=cy/cn, z=cz/cn};
				Move.Towards(v, c, nil, true, false);
				return;
			end;
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
	SafeCall('Stores', Military.Stores);
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
	Game.Log(string.format('%sround %d | %s | units c%d m%d(+%d stored) r%d s%d aa%d | structures %d | seen g%d a%d d%d /%d, enemy %d%s%s | minerals %d energy out %d spare %d | mines %d gen %d | %.2fs',
		Cfg.logPrefix, Sense.round, Mem.posture..(Mem.attackTarget and ('->'..Mem.attackTarget) or ''),
		#Sense.my.constructor, #Sense.my.combat, Sense.storedByRole.combat or 0, #Sense.my.radar, #Sense.my.surveyor, #Sense.my.aa,
		#Sense.myStructures, seen, seenA, seenD, #Map.regions, #Sense.enemyRegions,
		Sense.AirMatters() and ' AIR' or '', Sense.DeepMatters() and ' DEEP' or '',
		cur.storage.minerals or 0, (Game.myFaction.maxResources.supply or {}).energy or 0, cur.balance.energy or 0,
		Economy.Count(Types.mine), Economy.Count(Types.generator), timer()));
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

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
--#require "needs.lua"


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
	wantLabsRich = 6,		-- labs while minerals pile up
	richMinerals = 400,		-- minerals from which surplus spending kicks in
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
	unseenAttackThreat = 4,	-- threat assumed at the origin of an attack whose shooter we cannot sense
	deepBelowSurface = 3,
	-- air (planes) and bombs. Successful games fight both on the ground and in the air
	planeFactoryRound = 35,	-- first plane factory from this round, once the economy runs
	planeFactories = 1,		-- how many to keep
	planeFactoryMines = 4,	-- mines needed before the first plane factory (it eats fuel and minerals every round)
	planePriority = 'normal',	-- 'early': plane factories right after the first vehicle plant (air personality)
	maxPlanes = 6,
	doomsdayUranium = 300,	-- uranium for a doomsday payload: only then do bombers pay off in numbers
	planeMinerals = 150,	-- queue planes only with this much in storage
	bomberShare = 0.6,		-- bombers among attack planes (raptors are the rest)
	wantDrones = 2,		-- sensor drones for air and ground coverage
	bombShare = 0,			-- share of combat vehicles built as bomb vehicles (raised when fighting underground)
	bombShareDeep = 0.33,
	minesBelowThreat = false,	-- mine sites in threatened regions too (underground personality: mines below the fight)	-- ... once hostile underground activity was seen
	artyZoneRounds = 10,	-- remember where hostile artillery fired from this long
	artyHighLayers = 2,		-- artillery firing from this many layers above its impact counts as "high"
	artyRushUnits = 5,		-- last resort against high artillery: rush only with this many units at once	-- shots from this many layers below the local surface count as underground
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
		'Unlock:Plane',		-- plane factory (topic names: 'Unlock/...', matched by substring)
		'Unlock:Raptor',
		'Unlock:Bomber',
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
	Types.planeFactory = u['Plane Factory'];
	Types.bomber = u['Bomber'];
	Types.raptor = u['Raptor'];
	Types.drone = u['Sensor Drone'];
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
		bomb = tur['Bomb'],
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
	if tt.canSelfDetonate then
		return 'bomb';
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

-- Region whose seed direction is closest to a world position (any height)
function Map.RegionOfWorld(w)
	if w == nil then return nil; end;
	local len = math.sqrt(w.x*w.x + w.y*w.y + w.z*w.z);
	if len <= 0 then return nil; end;
	local v = {x=w.x/len, y=w.y/len, z=w.z/len};
	local best, bv = nil, -2;
	for _,r in ipairs(Map.regions) do
		local d = Dot(v, r.center);
		if d > bv then best, bv = r, d; end;
	end;
	return best, len;
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
	siteFails = {},			-- cell index -> failed build attempts there: each one doubles the next blacklisting
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

function DamagePotential(u)
	if u.turret and u.turret.type.canAttack and u.turret.damageLevel then
		return (u.turret.damageLevel or 0) * (u.turret.attacksPerRound or 1);
	end;
	return 0;
end;

function Sense.Update()
	local round = Game.GetMainRoundNumber();
	Sense.round = round;
	Sense.my = {constructor={}, combat={}, radar={}, surveyor={}, aa={}, plane={}, bomb={}, other={}};
	Sense.planeFactories = {};
	Sense.myStructures = {};
	Sense.stores = {};
	Sense.stored = {};
	Sense.storedByRole = {};
	Sense.structureCells = {};
	Sense.structureAt = {};		-- cell index -> my structure there
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
			Sense.structureAt[u.location.cell.index] = u;
			local tn = u.type.name;
			Sense.structuresByType[tn] = (Sense.structuresByType[tn] or 0) + 1;
			if Types.planeFactory and u.type == Types.planeFactory then
				Sense.planeFactories[#Sense.planeFactories+1] = u;
			elseif u.type.isMobileEntityProducer and not u.type.isHangar then
				Sense.plants[#Sense.plants+1] = u;
			end;
			if u.type.isHangar and u.type ~= Types.planeFactory then
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

	-- attacks seen last round, including those whose attacker we cannot sense (shot trails, as
	-- players see them). The rough origin tells which layer and region to worry about
	Sense.recentAttacks = {};
	if Game.GetRecentAttacks then
		local ok, list = pcall(Game.GetRecentAttacks, 1);
		if ok and list then
			for _,a in ipairs(list) do
				if a.isHostile and a.source then
					local src, len = Map.RegionOfWorld(a.source);
					local hit = Map.RegionOfWorld(a.impact);
					local layer = Sense.AttackLayer(a, src, len);
					local tt0 = a.turretType;
					Sense.recentAttacks[#Sense.recentAttacks+1] = {src=src, hit=hit, layer=layer, precise=a.precise, turretType=tt0,
						splash=a.explosionRadius, arty=(layer == 'ground' and tt0 and tt0.requiresStationaryVehicle and tt0.trajectoryType == 'Artillery') or false};
					local tt = a.turretType;
					-- artillery proper: cannons share the ballistic trajectory but fire on the move
					if layer == 'ground' and tt and tt.requiresStationaryVehicle and tt.trajectoryType == 'Artillery' and a.impact then
						Military.NoteArtillery(a);
					end;
					if src and not a.precise then
						-- unseen shooter: remember the threat at its origin
						if layer == 'air' then
							src.airThreat = math.max(src.airThreat, Cfg.unseenAttackThreat);
							Mem.airSeen = round;
						elseif layer == 'deep' then
							Mem.deepSeen = round;
							src.mobileThreat = math.max(src.mobileThreat, Cfg.unseenAttackThreat);
						elseif a.turretType and a.turretType.isStructure then
							src.staticThreat = math.max(src.staticThreat, Cfg.unseenAttackThreat);
						else
							src.mobileThreat = math.max(src.mobileThreat, Cfg.unseenAttackThreat);
						end;
					end;
				end;
			end;
			if #Sense.recentAttacks > 0 then
				local by = {};
				for _,a in ipairs(Sense.recentAttacks) do
					local k = a.layer..':'..((a.turretType and a.turretType.name) or '?');
					by[k] = (by[k] or 0) + 1;
				end;
				local parts = {};
				for k,n in pairs(by) do parts[#parts+1] = k..' x'..n; end;
				table.sort(parts);
				Log('Attacked '..#Sense.recentAttacks..' time(s) last round ('..table.concat(parts, ', ')..')');
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
-- Sensed or remembered hostile air threat over and around my regions
function Sense.AirPressure()
	local p = 0;
	for id,_ in pairs(Sense.mine or {}) do
		local reg = Map.regions[id];
		p = p + (reg.airThreat or 0);
		for nb,_ in pairs(reg.neighbors) do p = p + (Map.regions[nb].airThreat or 0) * 0.5; end;
	end;
	return p;
end;

--[[
Which sensor layer an attack came from. Altitude alone says little (towers and
artillery sit on mountains), so the attacking turret decides first: plane
turrets mean air. Without a known turret, only shots from near flight height
count as air. Shots from well below the local surface count as underground.
]]
function Sense.AttackLayer(a, srcRegion, len)
	local tt = a.turretType;
	if tt and tt.mountType == 'Plane' then
		return 'air';
	end;
	local h0, h1 = Game.Grid.HeightToRadius(0), Game.Grid.HeightToRadius(1);
	local height = (h1 > h0) and (len - h0) / (h1 - h0) or 0;
	if tt == nil and Map.airRadius and Map.airRadius > Map.groundRadius
		and len >= Map.airRadius - (h1 - h0) * 2 then
		return 'air';
	end;
	-- compare with the surface where it came from
	local ref = Sense.my.constructor[1] or Sense.my.combat[1] or Sense.my.radar[1];
	if ref and srcRegion then
		local cell = srcRegion.centerCell;
		local best, bd = nil, nil;
		for _,c in ipairs(srcRegion.cells) do
			local d = Dist2(a.source, c.center);
			if bd == nil or d < bd then best, bd = c, d; end;
		end;
		local ok, loc = pcall(Game.Grid.FindHighestVehicleGround, ref, best or cell);
		if ok and loc and loc.height and height < loc.height - Cfg.deepBelowSurface then
			return 'deep';
		end;
	end;
	return 'ground';
end;

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
-- avoidThreat: true avoids every known threat, 'arty' only remembered artillery fire (attacking units)
function Move.Towards(u, goalWorld, goalCell, avoidThreat, mayEnterUnknown)
	local danger = Move.IsDangerous;
	if avoidThreat == 'arty' then
		danger = function(loc) return Military.InArtilleryZone(loc.world) ~= nil; end;
	end;
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
				if not avoidThreat or not danger(pick.location) then
					u.DirectTo(pick);
					return true;
				end;
			end;
		end;
	end;

	local block = nil;
	if avoidThreat then
		block = danger;
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
	if Military.InArtilleryZone(loc.world) then
		return true;	-- where hostile artillery has been shelling from out of sight
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
			-- scanners: a border ring of them watches for drilling; researched, they rival a radar
			elseif t.detectsResources then Add('scanningrange:'..tn, ((Needs.deepRisk or 0) >= 0.3 and not mobile) and 1.2 or 0.3);
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

	Economy.pendingUnlocks = #unlocks;	-- reactor core, heavy base, artillery, planes...: all need labs
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

-- What should the next idle constructor build? Returns {type, kind} or nil.
-- Weighs the current needs against every option (see needs.lua)
function Economy.NextStructure(constructorIndex)
	return Needs.NextStructure(constructorIndex);
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
	-- a peak over our resources: a store on top holds it with no standing garrison (nobody
	-- else can sit there, and the store releases units onto the high ground when needed)
	local peak = Needs.StorePeak(hasStore);
	if peak and not Economy.scannerClaimed['store'..peak] then
		Economy.scannerClaimed['store'..peak] = true;
		return peak, true;
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
-- May this tower type be built at all? (personalities restrict it)
function Economy.TowerAllowed(t)
	return t ~= nil and Types.IsUnlocked(t);
end;

function Economy.DefenseCounts(reg)
	local c = {ground=0, aa=0, launcher=0, scanner=0, radar=0};
	for _,st in ipairs(Sense.myStructures) do
		if st.turret and Map.RegionOfLoc(st.location) == reg then
			local tt = st.turret.type;
			if st.type == Types.radarTower then c.radar = c.radar + 1;
			elseif tt.detectsResources then c.scanner = c.scanner + 1;
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
			elseif Types.radarTower and job.kind == Types.radarTower.name then c.radar = c.radar + 1;
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

--[[
Lab array. Labs pay off in clusters (every two enabled lab neighbors add a point), so the
first lab goes into a reserved 6-cell triangle on ground nobody else needs: no natural
resources, nothing to harvest, safe and not exposed. Other buildings keep off it; more
labs fill it later (1, 3, 6). If there is no room on the surface, it can go underground.
]]
Economy.labReserved = {};	-- cell index -> true

local function CellById(idx)
	for _,reg in ipairs(Map.regions) do
		for _,c in ipairs(reg.cells) do if c.index == idx then return c; end; end;
	end;
	return nil;
end;

-- Grow a compact 6-cell cluster from a triangle of three mutually adjacent cells
local function Triangle(a)
	for _,b in ipairs(a.neighbors) do
		for _,c in ipairs(b.neighbors) do
			local ac = false;
			for _,x in ipairs(a.neighbors) do if x == c then ac = true; break; end; end;
			if ac and c ~= a then
				local set, list = {[a.index]=true, [b.index]=true, [c.index]=true}, {a, b, c};
				-- each further cell: the one touching most of the set (a triangle of 6 grows this way)
				while #list < 6 do
					local best, bn = nil, 1;
					for _,m in ipairs(list) do
						for _,cand in ipairs(m.neighbors) do
							if not set[cand.index] then
								local n = 0;
								for _,cn in ipairs(cand.neighbors) do if set[cn.index] then n = n + 1; end; end;
								if n > bn then best, bn = cand, n; end;
							end;
						end;
					end;
					if best == nil then break; end;
					set[best.index] = true; list[#list+1] = best;
				end;
				if #list == 6 then return list; end;
			end;
		end;
	end;
	return nil;
end;

function Economy.IsWaterSurface(cell, loc)
	local C = Game.Terrain and Game.Terrain.Content;
	if not (C and C.Water and Game.Terrain.TranslateUnitCell and Game.Terrain.GetContent) then return false; end;
	local ok, tc = pcall(Game.Terrain.TranslateUnitCell, cell);
	if not ok or tc == nil then return false; end;
	for dh = -1, 0 do
		local okc, content = pcall(Game.Terrain.GetContent, tc, loc.height + dh);
		if okc and content == C.Water then return true; end;
	end;
	return false;
end;

function Economy.LabSiteCellOK(u, cell)
	if Sense.structureCells[cell.index] or Economy.claimed[cell.index] then return false; end;
	local ok, res = pcall(Game.Grid.GetNaturalResourcesAt, cell);
	if ok and res and ((res.minerals or 0) > 0 or (res.fuel or 0) > 0 or (res.uranium or 0) > 0) then return false; end;
	if Economy.plantCells[cell.index] or Economy.IsPowerCenter(cell.index) or Economy.IsPowerRing(cell.index) then return false; end;
	local loc = Move.GroundAt(u, cell);
	if loc == nil or loc.isSuborbital then return false; end;
	local reg = Map.RegionOfLoc(loc);
	if reg == nil or not Sense.mine[reg.id] or Sense.Threat(reg) > 0 then return false; end;
	return loc;
end;

-- Reserve the lab triangle (once), near home. Returns the reserved cells or nil
function Economy.LabReserve(u)
	if Mem.labCells then
		local out = {};
		for _,idx in ipairs(Mem.labCells) do out[#out+1] = CellById(idx); Economy.labReserved[idx] = true; end;
		return out;
	end;
	local home = Mem.homeRegion and Map.regions[Mem.homeRegion];
	if home == nil then return nil; end;
	local cands = {};
	for id,_ in pairs(Sense.mine) do
		if (Sense.hops[id] or 99) <= 1 then
			for _,c in ipairs(Map.regions[id].cells) do cands[#cands+1] = c; end;
		end;
	end;
	local best, bestS = nil, nil;
	local tried = 0;
	local hover = u.type and u.type.vehicleBaseType == Types.base.hover;
	for _,a in ipairs(cands) do
		if tried > 60 then break; end;
		if Economy.LabSiteCellOK(u, a) then
			tried = tried + 1;
			local tri = Triangle(a);
			if tri then
				local ok, score, h0 = true, 0, nil;
				for _,c in ipairs(tri) do
					local loc = Economy.LabSiteCellOK(u, c);
					if not loc then ok = false; break; end;
					h0 = h0 or loc.height;
					score = score + math.abs(loc.height - h0) * 3 + Needs.Exposure(u, loc) * 2;
					-- water surfaces are great: nothing grows there
					-- (only for builders that float: a heavy-base terraformer can't get out there)
					if hover and Economy.IsWaterSurface(c, loc) then score = score - 2; end;
					-- nothing to harvest next door either: resource neighbors are for mines
					for _,nb in ipairs(c.neighbors) do
						local okr, r = pcall(Game.Grid.GetNaturalResourcesAt, nb);
						if okr and r and ((r.minerals or 0) > 0 or (r.fuel or 0) > 0) then score = score + 1; end;
					end;
				end;
				if ok then
					score = score + Dist2(Map.WorldCenter(Map.RegionOfLoc(Move.GroundAt(u, a)) or home), Sense.baseCenter or Map.WorldCenter(home)) * 0.01;
					if bestS == nil or score < bestS then best, bestS = tri, score; end;
				end;
			end;
		end;
	end;
	if best == nil then return nil; end;
	Mem.labCells = {};
	for _,c in ipairs(best) do
		Mem.labCells[#Mem.labCells+1] = c.index;
		Economy.labReserved[c.index] = true;
	end;
	Log('Lab array reserved: cells '..table.concat(Mem.labCells, ', '));
	return best;
end;

-- Build a lab into the reserved triangle, next to the labs already there. True if ordered
function Economy.BuildLab(u, t)
	local cells = Economy.LabReserve(u);
	if cells == nil then return false; end;
	local best, bestN, bestLoc = nil, -1, nil;
	for _,c in ipairs(cells) do
		if c and not Sense.structureCells[c.index] and not Economy.claimed[c.index] then
			local n = 0;
			for _,nb in ipairs(c.neighbors) do
				local st = Sense.structureAt[nb.index];
				if st and st.type == t then n = n + 1; end;
			end;
			local loc = Move.GroundAt(u, c);
			Economy.allowLabSite = true;
			local ok = loc and Economy.SiteOK(loc, false);
			Economy.allowLabSite = false;
			if ok and n > bestN then best, bestN, bestLoc = c, n, loc; end;
		end;
	end;
	if best == nil then return false; end;
	if Economy.BuildAt(u, bestLoc, t) then
		Economy.claimed[best.index] = true;
		return true;
	end;
	return false;
end;

function Economy.SiteOK(loc, forMine)
	local idx = loc.cell.index;
	if Economy.labReserved[idx] and not Economy.allowLabSite then Economy.siteWhy = 'lab reserve'; return false; end;
	if Economy.IsPowerCenter(idx) and not Economy.allowPowerCenter then Economy.siteWhy = 'power center'; return false; end;
	-- a resource cell on a planned generator ring still goes to a mine: income beats a ring slot
	if Economy.IsPowerRing(idx) and not Economy.allowPowerRing and not forMine then Economy.siteWhy = 'power ring'; return false; end;
	local bl = Mem.blacklist[idx];
	if bl and bl > Sense.round then Economy.siteWhy = 'blacklisted'; return false; end;
	if Game.GetStructureAt(loc) ~= nil then Economy.siteWhy = 'structure'; return false; end;
	local other = Game.GetMobileAt(loc);
	if other ~= nil and not other.isOwnedByMe then Economy.siteWhy = 'hostile unit'; return false; end;
	if Game.Grid.AffectedByEMP(loc) then Economy.siteWhy = 'emp'; return false; end;
	if not forMine then
		-- keep resource cells free for mines
		local res = Game.Grid.GetNaturalResourcesAt and Game.Grid.GetNaturalResourcesAt(loc.cell);
		if res ~= nil and (res.minerals > 0 or res.fuel > 0 or res.uranium > 0) then
			return false;
		end;
	end;
	-- don't build where remembered or sensed hostiles can hit
	if Move.IsDangerous(loc) then Economy.siteWhy = 'danger'; return false; end;
	-- never box in a vehicle plant: finished vehicles need somewhere to deploy
	for _,nb in ipairs(loc.cell.neighbors) do
		if Economy.plantCells[nb.index] then
			local pfree = 0;
			for _,pn in ipairs(Economy.plantCells[nb.index].neighbors) do
				if pn.index ~= idx and not Sense.structureCells[pn.index] and not Economy.claimed[pn.index] then pfree = pfree + 1; end;
			end;
			if pfree < Cfg.plantFreeCells then Economy.siteWhy = 'boxes in plant'; return false; end;
		end;
	end;
	-- the constructor must be able to move off the finished structure
	local free = 0;
	for _,nb in ipairs(loc.cell.neighbors) do
		if not Sense.structureCells[nb.index] and not Economy.claimed[nb.index] then free = free + 1; end;
	end;
	if free < 2 then Economy.siteWhy = 'no room to leave'; return false; end;
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
		local maxHops = 2;
		local function Scan(reg)
			if seen[reg.id] then return; end;
			seen[reg.id] = true;
			if (Sense.Threat(reg) > 0 and not Cfg.minesBelowThreat) or (Sense.hops[reg.id] or 99) > maxHops then return; end;
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
		-- nothing close: look one ring further out (expanding beats starving)
		if #list == 0 then
			maxHops = 3;
			seen = {};
			for id,reg in pairs(Map.regions) do
				if (Sense.hops[id] or 99) <= 3 then Scan(reg); end;
			end;
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
	local best, bestH = nil, 0;
	for id,_ in pairs(Sense.mine) do
		local reg = Map.regions[id];
		local h = Sense.hops[id] or 0;
		local done = Mem.roadsDone[id];
		local quiet = Sense.Threat(reg) <= 0;
		if reg.myStructures > 0 and h > bestH and quiet
			and not (done and Sense.round - done < Cfg.roadRepaveRounds) then
			best, bestH = id, h;
		end;
	end;
	return best;
end;

Economy.busy = {};	-- constructor id -> busy rounds (with minerals) since it was last idle
Economy.retreats = {};	-- constructor id -> consecutive rounds spent retreating

-- Skip a build site for a while. Sites that keep failing are skipped for longer each time
function Economy.Blacklist(cell, rounds)
	local n = (Mem.siteFails[cell] or 0) + 1;
	Mem.siteFails[cell] = n;
	Mem.blacklist[cell] = Sense.round + math.min(rounds * 2 ^ (n - 1), 1000);
end;

-- Why no road this round (logged every 10 rounds while minerals pile up, for tuning)
local function NoRoad(why)
	if Sense.round % 10 == 0 and (Game.myFaction.currentResources.storage.minerals or 0) >= Cfg.roadMinerals
		and Economy.noRoadLogged ~= Sense.round then
		Economy.noRoadLogged = Sense.round;
		Log('No road: '..why);
	end;
	return false;
end;

function Economy.TryRoad(u)
	if not u.BuildRoadTo then return NoRoad('constructor cannot pave'); end;
	if Sense.round < Cfg.roadMinRound or #Sense.my.constructor < 2 then return false; end;
	local cur = Economy.Resources();
	if (cur.storage.minerals or 0) < Cfg.roadMinerals then return false; end;
	if Economy.PowerCritical() then return NoRoad('power critical'); end;
	local pressure = Sense.Pressure();
	-- the route itself stays in quiet regions (RoadTarget, IsDangerous); with minerals piling up,
	-- some pressure elsewhere is no reason to keep the constructors idle
	local maxP = Cfg.roadMaxPressure * (((cur.storage.minerals or 0) >= Cfg.richMinerals) and 3 or 1);
	if pressure > maxP then return NoRoad('pressure '..pressure); end;
	-- start inside own territory, so roads connect what we hold
	local here = Map.RegionOfLoc(u.location);
	if not here or not (Sense.mine[here.id] or (Sense.hops[here.id] or 99) <= 1) then return NoRoad('constructor outside territory'); end;
	local target = Economy.RoadTarget();
	if target == nil then return NoRoad('no quiet built-up region away from home'); end;
	local reg = Map.regions[target];
	local dest = Move.GroundAt(u, reg.centerCell);
	if dest == nil then return false; end;
	local ok, path = pcall(u.FindShortestPath, dest, nil, false);
	if not ok or path == nil or #path < 3 then
		Mem.roadsDone[target] = Sense.round;
		return NoRoad('no path to region '..target);
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
			-- a construction that already spent resources survives ClearSafeTasks and keeps the
			-- constructor in place. Retreating several rounds in a row: give it up
			Economy.retreats[u.id] = (Economy.retreats[u.id] or 0) + 1;
			if Economy.retreats[u.id] >= 3 and not Move.IsIdle(u) then
				u.ClearTasks();
				Economy.retreats[u.id] = 0;
				Log(u.niceName..': abandoning its task to get out');
			else
				u.ClearSafeTasks();
			end;
			if job and job.cell then Economy.Blacklist(job.cell, 15); end;
			Mem.jobs[u.id] = nil;
			Move.Towards(u, Map.WorldCenter(safest), safest.centerCell, true, false);
			Log(u.niceName..': retreating from danger');
			return;
		end;
	end;
	Economy.retreats[u.id] = nil;
	-- what it is building may have lost its foundation since (power, fuel): drop it, so the
	-- builder can restore the foundation instead
	if job and job.kind and not Move.IsIdle(u) then
		local why = Needs.Unsupported(Game.Type.units[job.kind]);
		if why then
			Log(u.niceName..': dropping '..job.kind..' ('..why..')');
			Needs.Block(job.kind, 'energy', 1);
			u.ClearTasks();
			Mem.jobs[u.id] = nil;
			Economy.busy[u.id] = nil;
			return;
		end;
	end;
	if not Move.IsIdle(u) then
		-- a job that runs far too long is stuck (e.g. no room to relocate after constructing)
		-- (rounds without minerals don't count: construction simply pauses then)
		-- Untracked tasks count too: a retreat keeps a construction that already spent resources
		-- (ClearSafeTasks), and such a leftover can wait forever for room to relocate
		-- a job that advanced since last round is not stuck, however long it takes
		-- (a plane factory needs 20 rounds of construction)
		local t1 = u.tasks and u.tasks[1];
		local left = t1 and t1.job and t1.job.continuationsToCompletion;
		Economy.jobLeft = Economy.jobLeft or {};
		local prev = Economy.jobLeft[u.id];
		Economy.jobLeft[u.id] = left;
		if left and prev and left < prev then
			Economy.busy[u.id] = 0;
		elseif (Game.myFaction.currentResources.storage.minerals or 0) >= Cfg.brokeMinerals then
			Economy.busy[u.id] = (Economy.busy[u.id] or 0) + 1;
		end;
		local limit = (job and job.kind == 'Road') and 40 or 20;
		if (Economy.busy[u.id] or 0) > limit then
			Log(u.niceName..': '..(job and job.kind or 'leftover task')..' at cell '..tostring(job and job.cell)..' is stuck. Dropping it');
			if job and job.cell then Economy.Blacklist(job.cell, 50); end;
			u.ClearTasks();
			Mem.jobs[u.id] = nil;
			Economy.busy[u.id] = nil;
		end;
		return;	-- busy with a queued motion/construction
	end;
	Economy.busy[u.id] = nil;
	-- detect stuck jobs: still idle at the same place where we ordered a construction
	if job and job.cell and Sense.round - job.since >= 1 and not Sense.structureCells[job.cell] then
		Economy.Blacklist(job.cell, 30);
		Log(u.niceName..': construction of '..job.kind..' at cell '..job.cell..' did not happen. Blacklisting the site');
	end;
	Mem.jobs[u.id] = nil;

	if not Economy.roadOrdered and Economy.TryRoad(u) then
		Economy.roadOrdered = true;
		return;
	end;

	Economy.freeBuilders = (Economy.freeBuilders or 0) + 1;
	local want = Economy.NextStructure(index);
	if want == nil or want.type == nil then
		return;
	end;

	if want.type == Types.lab and Economy.BuildLab(u, want.type) then
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
			-- why a mine order comes to nothing (logged every 5 rounds): no candidates, no ground,
			-- unsafe or too far, site rejected, or the build order failed
			local why = {cands = 0, noGround = 0, unsafe = 0, far = 0, site = 0, build = 0};
			if res == want.resource then Economy.mineWhy = why; end;
			for _,cell in ipairs(Economy.FindMineCells(res, Cfg.mineCandidates)) do
				why.cands = why.cands + 1;
				local dest = Move.GroundAt(u, cell);
				local reg = dest and Map.RegionOfLoc(dest);
				local safe = reg and (Sense.Threat(reg) <= 0 or Cfg.minesBelowThreat) and (Sense.hops[reg.id] or 99) <= 3;
				if dest == nil then why.noGround = why.noGround + 1;
				elseif not safe then
					if reg and (Sense.hops[reg.id] or 99) > 3 then why.far = why.far + 1; else why.unsafe = why.unsafe + 1; end;
				elseif not Economy.SiteOK(dest, true) then
					why.site = why.site + 1;
					why.reasons = (why.reasons or '')..' '..tostring(Economy.siteWhy);
				elseif Economy.BuildAt(u, dest, want.type) then
					Economy.claimed[cell.index] = true;
					return;
				else why.build = why.build + 1; end;
			end;
			if Sense.round % 5 == 0 then
				Log(string.format('No mine site for %s: %d candidates, %d no ground, %d unsafe, %d too far, %d site rejected (%s), %d build failed',
					res, why.cands, why.noGround, why.unsafe, why.far, why.site, why.reasons or '', why.build));
			end;
		end;
		-- the requirement that is missing: no known site (survey), or the known ones are under
		-- threat (guard them). Rejected sites and failed orders just weigh the mine down
		do
			local urgency = Needs.pendingUrgency or 0.5;
			local w = Economy.mineWhy or {};
			if w.cands == 0 or w.far + w.noGround >= w.cands then
				Needs.Block('Mine ('..want.resource..')', 'site', urgency);
			elseif w.unsafe > 0 and w.unsafe >= w.site + w.build then
				Needs.Block('Mine ('..want.resource..')', 'danger', urgency * 0.8);
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
			-- on the peak itself: the highest ground of the region
			if want.peak then score = score - (loc.height or 0) * 3; end;
			-- clusters (labs): next to as many of the same kind as possible, each two neighbors add output
			if want.near then
				local nbs = 0;
				for _,nb in ipairs(loc.cell.neighbors or {}) do
					local st = Sense.structureAt and Sense.structureAt[nb.index];
					if st and st.type == want.near then nbs = nbs + 1; end;
				end;
				score = score - nbs * 6;
			end;
			if want.kind ~= 'mine' and not want.peak then
				score = score + Needs.Spacing(loc);
				-- exposed to high ground within artillery reach: towers suffer most
				local ex = Needs.Exposure(u, loc);
				if ex > 0 then score = score + ex * (0.3 + Needs.artyRisk) * (want.kind == 'tower' and 8 or 4); end;
			end;
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
	Needs.Started();
	local reg = Map.RegionOfLoc(loc);
	Mem.jobs[u.id] = {kind = t.name, cell = loc.cell.index, since = Sense.round, at = loc.cell.index, region = reg and reg.id};
	-- count it right away, so other constructors assigned this frame don't pick the same structure
	Sense.structuresByType[t.name] = (Sense.structuresByType[t.name] or 0) + 1;
	Log(u.niceName..' -> build '..t.name..' at cell '..loc.cell.index);
	return true;
end;

-- A plane factory, if one is wanted now. It consumes fuel and minerals every round
function Economy.PlaneFactoryNeed()
	local t = Types.planeFactory;
	if t == nil or not Types.IsUnlocked(t) or Economy.Count(t) >= Cfg.planeFactories then return nil; end;
	-- a factory without any researched plane only costs fuel and minerals
	-- (the air personality builds it while the plane research finishes)
	if Cfg.planePriority ~= 'early' and not (Types.IsUnlocked(Types.bomber) or Types.IsUnlocked(Types.raptor) or Types.IsUnlocked(Types.drone)) then return nil; end;
	if Sense.round < Cfg.planeFactoryRound or Economy.Count(Types.mine) < Cfg.planeFactoryMines then return nil; end;
	local cur, max = Economy.Resources();
	local demand = t.balance and t.balance.demand or {};
	-- it needs fuel every round: a fuel mine at least, and some spare fuel unless it is the air personality's core
	if Economy.FuelMines() < 1 or (Cfg.planePriority ~= 'early' and (max.balance.fuel or 0) < (demand.fuel or 0)) then
		local m = Economy.MineReq('Fuel');
		if m and Cfg.planePriority == 'early' then return m; end;
		return nil;
	end;
	if (cur.storage.minerals or 0) < (tonumber(t.constructionCost) or 200) then return nil; end;
	return {type=t, kind='base'};
end;

-- Next plane for a plane factory
function Economy.NextPlane()
	local n = {bomber=0, raptor=0, drone=0};
	local parked = {bomber=0, raptor=0, drone=0};
	local function count(p, into)
		if p.type == Types.bomber then into.bomber = into.bomber + 1;
		elseif p.type == Types.raptor then into.raptor = into.raptor + 1;
		elseif p.type == Types.drone then into.drone = into.drone + 1; end;
	end;
	for _,p in ipairs(Sense.my.plane) do count(p, n); end;
	for _,p in ipairs(Sense.stored or {}) do
		if p.type and p.type.isSuborbitalPlane then count(p, n); count(p, parked); end;
	end;
	-- sensor drones: late game the best way to keep large land masses covered, and they see
	-- planes too. Enough to cover my land (one per few regions), first wherever it is out of sight
	local own, blind = 0, 0;
	for id,_ in pairs(Sense.mine) do own = own + 1; end;
	for _ in pairs(Needs.blind or {}) do blind = blind + 1; end;
	local wantDrones = Cfg.wantDrones;
	if Sense.round >= 60 then wantDrones = math.max(wantDrones, math.ceil(own / 4)); end;
	-- the drones that cover my land don't count against the cap on combat planes
	local total = n.bomber + n.raptor + math.max(0, n.drone - wantDrones);
	if total >= Cfg.maxPlanes and n.drone >= wantDrones then return nil; end;
	-- planes parked in a store and not taken out: probably enough of those already
	-- (unless they are being saved up for a run)
	if parked.bomber > 0 and not Military.bomberRun then n.bomber = n.bomber + 2; end;
	if parked.raptor > 0 then n.raptor = n.raptor + 2; end;
	if parked.drone > 0 then n.drone = n.drone + 2; end;
	-- only what is researched: the server rejects the rest
	local bomber = Types.IsUnlocked(Types.bomber) and Types.bomber or nil;
	local raptor = Types.IsUnlocked(Types.raptor) and Types.raptor or nil;
	local drone = Types.IsUnlocked(Types.drone) and Types.drone or nil;
	-- hostile planes around: raptors hunt them
	local hostileAir = Sense.AirPressure() > 0;
	if hostileAir and raptor and n.raptor <= n.bomber and total < Cfg.maxPlanes then return raptor; end;
	if drone and n.drone < wantDrones and (n.bomber + n.raptor >= 1 or (blind > 0 and Sense.round >= 60)) then return drone; end;
	if total >= Cfg.maxPlanes then return nil; end;
	--[[
	Raptors are invaluable: they hunt planes and hit ground targets twice a round on the move.
	Without enough uranium for doomsday strikes, bombers are not that effective: one or two.
	Raptors and artillery with lots of sensor drones tend to win the day.
	]]
	local uranium = Game.myFaction.currentResources.storage.uranium or 0;
	local bomberCap = uranium >= Cfg.doomsdayUranium and math.ceil((n.bomber + n.raptor + 1) * Cfg.bomberShare) or
		(n.raptor >= 4 and 2 or (n.raptor >= 1 and 1 or 0));
	if not raptor then bomberCap = math.max(bomberCap, 1); end;
	if bomber and n.bomber < bomberCap then return bomber; end;
	return raptor or (bomber and n.bomber < bomberCap + 1 and bomber) or drone;
end;

function Economy.PlaneProduction()
	for _,f in ipairs(Sense.planeFactories) do
		if f.productionCompleted or f.deploymentOptions then
			local ok, loc = pcall(f.GetIdealDeploymentOption);
			if not (ok and loc) and f.deploymentOptions then loc = f.deploymentOptions[1]; ok = loc ~= nil; end;
			if ok and loc then
				Log(f.niceName..': launching '..tostring(f.productionCompleted));
				f.DeployProductionAt(loc);
			end;
		end;
		-- planes parked inside: launch them
		if f.storedUnits and f.GetStoredDeploymentOptions then
			for _,p in ipairs(f.storedUnits) do
				local ok, opts = pcall(f.GetStoredDeploymentOptions, p);
				if ok and opts and opts[1] then
					pcall(f.DeployStored, p, opts[1]);
				end;
			end;
		end;
		local queued = f.productionQueue and #f.productionQueue or 0;
		if queued < 1 and f.BuildMobile and (Game.myFaction.currentResources.storage.minerals or 0) >= Cfg.planeMinerals
			and (Game.myFaction.currentResources.storage.minerals or 0) >= Economy.PowerReserve() then
			local t = Economy.NextPlane();
			if t then
				local ok, err = pcall(f.BuildMobile, t);
				Log(f.niceName..': '..(ok and 'producing ' or 'cannot produce ')..t.name..(ok and '' or (': '..tostring(err))));
			end;
		end;
	end;
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
		-- bomb vehicles: underground, a tank's range is worthless, a blast clears a tunnel
		local share = Sense.DeepMatters() and math.max(Cfg.bombShare, Cfg.bombShareDeep) or Cfg.bombShare;
		if share > 0 and Types.turret.bomb and Types.IsUnlocked(Types.turret.bomb) and #my.bomb < (#my.combat + #my.bomb + 1) * share then
			local b = Types.Vehicle(Types.turret.bomb, base) or Types.Vehicle(Types.turret.bomb, hover);
			if b then return b; end;
		end;
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
	-- weigh the current needs against every vehicle option (see needs.lua)
	return Needs.NextVehicle({Constructor = Constructor, Tank = Tank});
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
	-- every plant busy with a full queue (read by Needs: spending capacity)
	Economy.plantsFull = #Sense.plants > 0;
	for _,p in ipairs(Sense.plants) do
		if p.productionCompleted or p.deploymentOptions then
			local ok, loc = pcall(p.GetIdealDeploymentOption);
			-- the "ideal" option can be a neighboring vehicle store: a new unit parked there does
			-- nothing (constructors build nothing in there). Prefer open ground
			if ok and loc and p.deploymentOptions and Game.GetStructureAt(loc) ~= nil then
				for _,o in ipairs(p.deploymentOptions) do
					if Game.GetStructureAt(o) == nil and not Move.IsDangerous(o) then loc = o; break; end;
				end;
			end;
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
		if queued < maxQueue then Economy.plantsFull = false; end;
		local reserveBlocked = (cur.storage.minerals or 0) < Economy.PowerReserve() and #Sense.my.combat >= 2;
		-- while short on mines and broke, mines under construction get the minerals first
		if not reserveBlocked and broke and Economy.MineJobs() > 0 and #Sense.my.combat >= 2 then
			reserveBlocked = true;
		end;
		if queued < maxQueue and p.BuildMobile then
			local t = Economy.NextVehicle();
			-- never spend the power reserve on vehicles, except the minimum of constructors and a pinch of defense
			local essential = #Sense.my.constructor + (Sense.storedByRole.constructor or 0) < 2
				or (t and t.turretType == Types.turret.surveyor and #Sense.my.surveyor == 0 and Sense.Pressure() <= 0);
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
	Economy.freeBuilders = 0;
	Economy.roadOrdered = false;
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
	local tr = t.type.trajectoryType;
	local plane = v.type.isSuborbitalPlane;
	local hitsAir = tr == 'AAMissile' or (plane and tr == 'Missile');
	local hitsGround = (not plane and tr ~= 'AAMissile') or (plane and (tr == 'A2GMissile' or tr == 'VerticalBeam' or tr == 'Missile'));
	local candidates = {};
	for _,e in pairs(Game.hostileUnits) do
		if e.health > 0 then	-- sensed now; no firing at ghosts
			SafeCall('target', ProcessAttackTarget, v, e, candidates, hitsAir, hitsGround);
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

-- Average damage potential of one of our combat units (what a hostile threat figure is worth in units)
function Military.UnitPotential()
	local sum, n = 0, 0;
	for _,u in ipairs(Sense.my.combat) do
		local p = DamagePotential(u);
		if p > 0 then sum = sum + p; n = n + 1; end;
	end;
	return n > 0 and sum / n or 4;
end;

-- Hostile defense at and around a region, in threat figures
function Military.DefenseAt(reg)
	local d = reg.staticThreat + reg.mobileThreat;
	for nb,_ in pairs(reg.neighbors) do
		local r = Map.regions[nb];
		d = d + (r.staticThreat + r.mobileThreat) * 0.4;
	end;
	return d;
end;

--[[
The weakest worthwhile target. Any weakness in a defense is a fatal vulnerability,
and not using one can decide the match: a lone mine or an undefended factory is worth
more than a fortress full of structures. Score: what is there, against how well it is
defended, how far it is and how old our knowledge is.
]]
function Military.ChooseAttackTarget()
	local best, bestScore = nil, 0;
	local pot = Military.UnitPotential();
	for _,id in ipairs(Sense.enemyRegions) do
		local reg = Map.regions[id];
		if not Sense.mine[id] then
			-- what is there, plus what holding it gives: a peak over resource valleys can decide the match
			local value = reg.hostileStructures * 3 + reg.mobileThreat * 0.2 + Needs.StrategicValue(reg) * 0.5;
			local hops = Sense.hops[id] or 20;
			if Economy.MineShortage() or Economy.Count(Types.mine) < Cfg.minMines then
				value = value + Economy.ResourceCells(reg) * 2;
			end;
			local defense = Military.DefenseAt(reg) / pot;	-- in our units
			local score = value / (1 + defense * 0.5) / (1 + hops * 0.3) / (1 + Sense.Staleness(reg) * 0.02);
			if score > bestScore then
				best, bestScore = id, score;
			end;
		end;
	end;
	return best;
end;

-- Fixed defenses can always be beaten with more units: size the attack by what we remember of the target
-- Units needed to take a region: enough to outgun its defense, and a few more
function Military.RequiredForce(id)
	local reg = Map.regions[id];
	local units = Military.DefenseAt(reg) / Military.UnitPotential();
	local need = math.ceil(units * 1.3) + 3 + math.floor(reg.hostileStructures / 3);
	-- a lone, undefended target needs a raid, not an army
	return math.max(3, math.min(need, Cfg.attackMinUnits * 3));
end;

function Military.UpdatePosture()
	local n = Military.ArmyStrength();
	if Mem.posture == 'defend' then
		-- before attackMinRound only clear chances: twice the force a target needs
		local target = Military.ChooseAttackTarget();
		local need = target and Military.RequiredForce(target);
		if need and Sense.round < Cfg.attackMinRound then need = need * 2; end;
		-- keep enough at home for the pressure there
		if need then need = need + math.ceil(Sense.Pressure() / Military.UnitPotential()); end;
		-- with minerals piling up, losses are cheap to replace: attack with less
		if need and (Game.myFaction.currentResources.storage.minerals or 0) >= Cfg.richMinerals then
			need = math.ceil(need * 0.7);
		end;
		if target and n >= need then
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
Military.peakGuard = {};	-- unit id -> peak region it was offloaded onto from a store
Military.peakQuiet = {};	-- unit id -> last round its peak was threatened

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

function Military.DeployFrom(store, count, goalWorld, filter, out, role)
	local deployed = 0;
	local used = {};
	for _,u in ipairs(Sense.stored) do
		if deployed >= count then break; end;
		if Types.Role(u) == (role or 'combat') and store.DeployStored and (filter == nil or filter(u)) then
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
						if out then out[#out+1] = u; end;
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

	-- parked constructors build nothing: take them out while builders are short
	local storedC = Sense.storedByRole.constructor or 0;
	if storedC > 0 and #Sense.my.constructor < (Cfg.constructorsLate or 4) then
		local home = Sense.baseCenter or (Mem.homeRegion and Map.WorldCenter(Map.regions[Mem.homeRegion]));
		for _,st in ipairs(Sense.stores) do
			if budget <= 0 then break; end;
			local n = Military.DeployFrom(st, math.min(budget, storedC), Military.ConstructorGoal and Military.ConstructorGoal(st) or home, nil, nil, 'constructor');
			budget = budget - n;
			storedC = storedC - n;
			if storedC <= 0 then break; end;
		end;
	end;

	-- attack: everything stored joins, exiting as close to the target as possible
	if storedCombat > 0 and Mem.posture == 'attack' and Mem.attackTarget then
		local st = Military.NearestStore(Mem.attackTarget);
		if st then
			budget = budget - Military.DeployFrom(st, budget, Map.WorldCenter(Map.regions[Mem.attackTarget]));
		end;
	end;

	--[[
	A store on a peak holds it with no standing garrison. When anything comes close,
	offload an artillery tank or three onto the high ground: they command the slopes.
	]]
	do
		-- forget guards that died (deployed last round or earlier: they are active units by now)
		local alive = {};
		for _,u in ipairs(Sense.my.combat) do alive[u.id] = true; end;
		for id,_ in pairs(Military.peakGuard) do
			if not alive[id] then Military.peakGuard[id] = nil; Military.peakQuiet[id] = nil; end;
		end;
	end;
	if storedCombat > 0 and budget > 0 then
		for _,st in ipairs(Sense.stores) do
			local reg = Map.RegionOfLoc(st.location);
			if budget > 0 and reg and Needs.Overlook(reg) >= 2 then
				local threat = Sense.Threat(reg);
				for nb,_ in pairs(reg.neighbors) do threat = threat + Map.regions[nb].mobileThreat; end;
				local have = 0;
				for id,r in pairs(Military.peakGuard) do
					if r == reg.id then have = have + 1; end;
				end;
				if threat > 0 and have < 3 then
					local want = math.min(budget, 3 - have, math.max(1, math.ceil(threat / Military.UnitPotential())));
					local out = {};
					local function arty(u) return u.turret and u.turret.type == Types.turret.artillery; end;
					local n = Military.DeployFrom(st, want, st.location.world, arty, out);
					if n == 0 then n = Military.DeployFrom(st, want, st.location.world, nil, out); end;
					for _,u in ipairs(out) do Military.peakGuard[u.id] = reg.id; end;
					if n > 0 then Log('Hostiles near the peak at region '..reg.id..': offloaded '..n..' unit(s) onto it'); end;
					budget = budget - n;
				end;
			end;
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
	-- quiet at home: hold the high ground over our resources before someone else does
	if bestT <= 0 then
		local ow = Needs.Overwatch();
		if ow then
			if Military.overwatch ~= ow then Log('Holding high ground at region '..ow..' over our resources'); end;
			Military.overwatch = ow;
			return ow;
		end;
	end;
	return best;
end;

--[[
Hostile artillery zones. Artillery on high ground out-ranges everything below it,
and tanks sent toward it die on the way, one after another. Each shell we observe
marks a circle around its origin that units avoid. Counters, best first:
planes (fly over walls), tunnels (come up beneath it), and only then a rush with
many units at once, if the cliff can be reached at all. A walled-off cliff can't.
]]
Military.artyZones = {};	-- list of {source, radius2, expires, high}

local function VLen(v) return math.sqrt(v.x*v.x + v.y*v.y + v.z*v.z); end;

function Military.NoteArtillery(a)
	local tt = a.turretType;
	local h0, h1 = Game.Grid.HeightToRadius(0), Game.Grid.HeightToRadius(1);
	local layer = math.max(1e-3, h1 - h0);
	local rise = (VLen(a.source) - VLen(a.impact)) / layer;
	-- artillery reaches further downhill: widen the circle by the height it fires from
	local range = (tt.flatAttackRange or 0) * 1.25 + (a.explosionRadius or 0) + math.max(0, rise) * layer;
	local seen = math.sqrt(Dist2(a.source, a.impact)) * 1.1;
	range = math.max(range, seen);
	local high = rise >= Cfg.artyHighLayers;
	for _,z in ipairs(Military.artyZones) do
		if Dist2(z.source, a.source) < layer * layer then
			z.expires = Sense.round + Cfg.artyZoneRounds;
			z.radius2 = math.max(z.radius2, range * range);
			z.high = z.high or high;
			return;
		end;
	end;
	Military.artyZones[#Military.artyZones+1] = {source=a.source, radius2=range*range, expires=Sense.round + Cfg.artyZoneRounds, high=high, since=Sense.round};
	Log('Hostile artillery '..(high and 'on high ground ' or '')..'shelling from out of sight: keeping units clear');
end;

function Military.ExpireArtyZones()
	local keep = {};
	for _,z in ipairs(Military.artyZones) do
		if z.expires >= Sense.round then keep[#keep+1] = z; end;
	end;
	Military.artyZones = keep;
end;

function Military.InArtilleryZone(world)
	for _,z in ipairs(Military.artyZones) do
		if z.expires >= (Sense.round or 0) and Dist2(world, z.source) < z.radius2 then
			return z;
		end;
	end;
	return nil;
end;

--[[
Counter a high artillery zone with combat units outside of it. No planes or
tunnels yet, so the only answer left is a rush: all at once, and only if there
is a ground path up to it. Otherwise stay out of range and let it be.
Returns true for units that got an order here.
]]
Military.rush = nil;	-- {zone, units = {id=true}}

function Military.ArtyCounter()
	local zone = nil;
	for _,z in ipairs(Military.artyZones) do
		if z.high and (zone == nil or z.since < zone.since) then zone = z; end;
	end;
	if zone == nil then
		Military.rush = nil;
		return {};
	end;
	local ordered = {};
	if Military.rush == nil or Military.rush.zone ~= zone then
		-- gather: who could reach it?
		local cands = {};
		local reachable = false;
		local srcRegion = Map.RegionOfWorld(zone.source);
		for _,v in ipairs(Sense.my.combat) do
			if v.health > 0 and not Military.stashing[v.id] and not Military.NeedsStationary(v) then
				cands[#cands+1] = v;
			end;
		end;
		if #cands < Cfg.artyRushUnits or srcRegion == nil then
			return ordered;
		end;
		-- walled off cliffs: no ground path, no rush
		local probe = cands[1];
		local best, bd = nil, nil;
		for _,c in ipairs(srcRegion.cells) do
			local loc = Move.GroundAt(probe, c);
			if loc then
				local d = Dist2(loc.world, zone.source);
				if bd == nil or d < bd then best, bd = loc, d; end;
			end;
		end;
		if best then
			local ok, path = pcall(probe.FindShortestPath, best, nil, false);
			reachable = ok and path ~= nil and #path > 0;
		end;
		if not reachable then
			if not zone.noPath then
				Log('High artillery cannot be reached by ground: staying out of its range');
				zone.noPath = true;
			end;
			return ordered;
		end;
		Military.rush = {zone=zone, units={}, cell=best.cell};
		for _,v in ipairs(cands) do Military.rush.units[v.id] = true; end;
		Log('Rushing high artillery with '..#cands..' units');
	end;
	for _,v in ipairs(Sense.my.combat) do
		if Military.rush.units[v.id] and v.health > 0 then
			ordered[v.id] = true;
			if not Military.Fire(v) or v.motionIgnoresAttack then
				if not Move.IsIdle(v) then v.ClearSafeTasks(); end;
				Move.Towards(v, zone.source, Military.rush.cell, false, true);
			end;
		end;
	end;
	return ordered;
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
	Military.ExpireArtyZones();
	local rushing = Military.ArtyCounter();

	for i,v in ipairs(Sense.my.combat) do
		SafeCall('combat '..v.niceName, function()
			if rushing[v.id] then return; end;
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
			-- offloaded onto a peak: hold it while anything is near, then go back into storage
			local peak = Military.peakGuard[v.id];
			if peak then
				local pr = Map.regions[peak];
				local threat = Sense.Threat(pr);
				for nb,_ in pairs(pr.neighbors) do threat = threat + Map.regions[nb].mobileThreat; end;
				if threat > 0 or Military.peakQuiet[v.id] == nil then
					Military.peakQuiet[v.id] = Sense.round;
					goalId = peak;
				elseif Sense.round - Military.peakQuiet[v.id] >= 10 then
					Military.peakGuard[v.id] = nil;
				else
					goalId = peak;
				end;
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
			-- never walk into remembered artillery fire, attacking or not: a rush is decided above
			Move.Towards(v, goal, cell, (not attacking) or 'arty', attacking);
		end);
	end;
end;

--[[
Planes. They fly over walls, cliffs and tunnels' worth of rock, and regular
radar can't see them, so only AA can answer them.
* Bombers hit what is directly beneath them: hostile artillery on high ground
  first (the best answer to it), then the attack target, then known structures.
* Raptors hunt hostile planes, otherwise escort the bombers.
* Sensor drones keep the stalest nearby regions in view.
Planes avoid sensed AA, except when diving onto their target.
]]
function Move.AirDanger(loc)
	for _,e in pairs(Game.hostileUnits) do
		if e.health > 0 and e.turret and e.turret.type.trajectoryType == 'AAMissile' and e.turret.IsInRange then
			local ok, r = pcall(e.turret.IsInRange, loc.world);
			if ok and r then return true; end;
		end;
	end;
	return false;
end;

function Move.Fly(p, goalWorld, avoidAA)
	local block = avoidAA and Move.AirDanger or nil;
	local ok, reach = pcall(p.GetReach, block, math.max(1, p.movementSpeed), true, false, false, false);
	if not ok or reach == nil then return false; end;
	local best, bestD = nil, Dist2(p.location.world, goalWorld);
	for _,n in ipairs(reach) do
		if n.location.isSuborbital then
			local d = Dist2(n.location.world, goalWorld);
			if d < bestD then best, bestD = n, d; end;
		end;
	end;
	if best then p.DirectTo(best); return true; end;
	return false;
end;

function Military.BomberGoal()
	-- high artillery: planes are the best answer
	for _,z in ipairs(Military.artyZones) do
		if z.high then return z.source, true; end;
	end;
	local sensedBest, sbd = nil, nil;
	local home = Sense.baseCenter;
	for _,e in pairs(Game.hostileUnits) do
		if e.health > 0 and not e.location.isSuborbital and not e.isIndestructible then
			local w = e.location.world;
			local d = home and Dist2(w, home) or 0;
			-- artillery and towers first: they hold ground our tanks can't take
			if e.turret and e.turret.type.requiresStationaryVehicle then d = d * 0.25; end;
			if sbd == nil or d < sbd then sensedBest, sbd = w, d; end;
		end;
	end;
	if sensedBest then return sensedBest, true; end;
	local target = Mem.attackTarget or Military.ChooseAttackTarget();
	if target then return Map.AirCenter(Map.regions[target]), false; end;
	return nil, false;
end;

function Military.Planes()
	local bombers = {};
	for _,p in ipairs(Sense.my.plane) do
		if p.type == Types.bomber then bombers[#bombers+1] = p; end;
	end;
	local taken = {};
	for _,p in ipairs(Sense.my.plane) do
		SafeCall('plane '..p.niceName, function()
			local fired = Military.Fire(p);
			if not Move.IsIdle(p) then p.ClearSafeTasks(); end;
			local goal, dive = nil, false;
			if p.type == Types.bomber then
				goal, dive = Military.BomberGoal();
			elseif p.type == Types.drone or not p.turret or not p.turret.type.canAttack then
				local best, bs = nil, nil;
				for _,reg in ipairs(Map.regions) do
					if not taken[reg.id] then
						local hops = Sense.hops[reg.id] or 30;
						local sc = math.min(Sense.Staleness(reg), 60) / (1 + hops * 0.5);
						if Sense.frontier[reg.id] then sc = sc * 2; end;
						if Sense.mine[reg.id] then sc = sc * 2; end;	-- blind spots in my own land
						if bs == nil or sc > bs then best, bs = reg, sc; end;
					end;
				end;
				if best then taken[best.id] = true; goal = Map.AirCenter(best); end;
			else
				-- raptor: nearest hostile plane, else over the bombers, else over home
				local bd = nil;
				for _,e in pairs(Game.hostileUnits) do
					if e.health > 0 and e.location.isSuborbital then
						local d = Dist2(e.location.world, p.location.world);
						if bd == nil or d < bd then goal, bd = e.location.world, d; end;
					end;
				end;
				dive = goal ~= nil;
				-- no planes to hunt: raptors fire at ground targets too (range 9, twice a round,
				-- while moving). Work with the army: over the attack target, or the threatened region
				if goal == nil then
					local tid = (Mem.posture == 'attack' and Mem.attackTarget) or Military.DefenseRegion();
					local treg = tid and Map.regions[tid];
					if treg and (Mem.posture == 'attack' or Sense.Threat(treg) > 0) then goal = Map.AirCenter(treg); end;
				end;
				if goal == nil and #bombers > 0 then goal = bombers[1].location.world; end;
			end;
			if goal == nil and Mem.homeRegion then goal = Map.AirCenter(Map.regions[Mem.homeRegion]); end;
			if goal and not (fired and not p.motionIgnoresAttack) then
				-- close to the target, accept AA fire; on the way, avoid it
				local near = Dist2(p.location.world, goal) < 36;
				Move.Fly(p, goal, not (dive and near));
			end;
		end);
	end;
end;

--[[
Bomb vehicles blow up next to hostiles. Underground, where tanks can't use their
range, they are the better weapon. Never detonate next to own units or structures.
]]
function Military.Bombs()
	local defense = Military.DefenseRegion();
	for _,b in ipairs(Sense.my.bomb) do
		SafeCall('bomb '..b.niceName, function()
			local r = (b.turret and b.turret.explosionRadius) or 2;
			local r2 = r * r;
			local w = b.location.world;
			local hostile, nearest, nd = 0, nil, nil;
			for _,e in pairs(Game.hostileUnits) do
				if e.health > 0 and not e.location.isSuborbital then
					local d = Dist2(e.location.world, w);
					if d < r2 * 0.6 then hostile = hostile + (tonumber(e.type.constructionCost) or 50); end;
					if nd == nil or d < nd then nearest, nd = e, d; end;
				end;
			end;
			if hostile >= 80 then
				local own = false;
				for _,u in pairs(Game.myUnits) do
					if u.id ~= b.id and not u.location.isSuborbital and Dist2(u.location.world, w) < r2 * 1.5 then own = true; break; end;
				end;
				if not own and b.SelfDetonate then
					b.ClearTasks();
					b.SelfDetonate();
					Log(b.niceName..': detonating');
					return;
				end;
			end;
			if not Move.IsIdle(b) then b.ClearSafeTasks(); end;
			-- go for sensed hostiles in or next to my territory, else wait in the defense region
			if nearest then
				local reg = Map.RegionOfLoc(nearest.location);
				if reg and (Sense.mine[reg.id] or Sense.frontier[reg.id]) then
					Move.Towards(b, nearest.location.world, nearest.location.cell, false, false);
					return;
				end;
			end;
			if defense then
				local reg = Map.regions[defense];
				Move.Towards(b, Map.WorldCenter(reg), reg.centerCell, true, false);
			end;
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
					-- my own land out of sight: I can't be sure nothing slipped through
					if Sense.mine[reg.id] then importance = importance + 2; end;
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
						local noMinerals = Economy.noMineSite['Minerals'] and Sense.round - Economy.noMineSite['Minerals'] <= 10;
						if (Economy.MineShortage() or noMinerals) and (Sense.hops[reg.id] or 99) <= 3 then
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
	--[[
	Leapfrog: AA that must stand still to fire covers nothing while it moves. Two in one
	group (the same guarded region, or the army escort) take turns while air is around:
	one moves, the other stays put and covers it.
	]]
	local groupOf, groupSize = {}, {};
	for i,v in ipairs(Sense.my.aa) do
		local g = (Mem.posture == 'attack' and cn > 0 and i % 2 == 0) and 'escort'
			or tostring(((i-1) % math.max(1,#targets)) + 1);
		groupOf[v.id] = g;
		groupSize[g] = (groupSize[g] or 0) + 1;
	end;
	local turn = {};
	local function MayMove(v)
		local g = groupOf[v.id];
		if (groupSize[g] or 0) < 2 or not Military.NeedsStationary(v) then return true; end;
		local reg = Map.RegionOfLoc(v.location);
		local air = reg and reg.airThreat or 0;
		if reg then for nb,_ in pairs(reg.neighbors) do air = air + Map.regions[nb].airThreat; end; end;
		if air <= 0 then return true; end;
		turn[g] = (turn[g] or 0) + 1;
		return (turn[g] + Sense.round) % 2 == 0;
	end;
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
					if Dist2(v.location.world, post.loc.world) >= 0.01 and MayMove(v) then
						Move.Towards(v, post.loc.world, post.cell, true, false);
					end;
					return;
				end;
			end;
			if not MayMove(v) then return; end;	-- the partner moves this round
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

-- Idle units parked next to a vehicle plant block its deployment: move them aside
Military.parked = {};
function Military.ClearPlants()
	local near = {};
	for _,p in ipairs(Sense.plants) do
		near[p.location.cell.index] = true;
		for _,nb in ipairs(p.location.cell.neighbors) do near[nb.index] = true; end;
	end;
	if next(near) == nil then return; end;
	-- only units that have been sitting there for a while (not ones just ordered elsewhere)
	local parked = {};
	for role,list in pairs(Sense.my) do
		for _,u in ipairs(list) do
			if role ~= 'constructor' and u.type.isMobile and near[u.location.cell.index] then
				parked[u.id] = (Military.parked[u.id] or 0) + 1;
			end;
		end;
	end;
	Military.parked = parked;
	for role,list in pairs(Sense.my) do
		for _,u in ipairs(list) do
			if (parked[u.id] or 0) >= 2 and Move.IsIdle(u) and u.GetReach then
				SafeCall('clear plant '..u.niceName, function()
					local ok, reach = pcall(u.GetReach, nil, math.max(2, u.movementSpeed * 2), false, false, false, true);
					if not ok or reach == nil then return; end;
					local best, bestC = nil, nil;
					for _,n in ipairs(reach) do
						if not near[n.location.cell.index] and not Move.IsDangerous(n.location) then
							if bestC == nil or n.movementCostsFromRoot < bestC then
								best, bestC = n, n.movementCostsFromRoot;
							end;
						end;
					end;
					if best then u.DirectTo(best); end;
				end);
			end;
		end;
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
	SafeCall('Needs', Needs.Update);
	SafeCall('NeedsReport', Needs.Report);

	SafeCall('Research', Economy.Research);
	SafeCall('Production', Economy.Production);
	SafeCall('PlaneProduction', Economy.PlaneProduction);
	SafeCall('Constructors', Economy.Constructors);
	SafeCall('Posture', Military.UpdatePosture);
	SafeCall('Towers', Military.Towers);
	SafeCall('Stores', Military.Stores);
	SafeCall('Combat', Military.Combat);
	SafeCall('Scouts', Military.Scouts);
	SafeCall('AA', Military.AA);
	SafeCall('Planes', Military.Planes);
	SafeCall('Bombs', Military.Bombs);
	SafeCall('ClearPlants', Military.ClearPlants);

	local cur = Game.myFaction.currentResources;
	local seen, seenA, seenD = 0, 0, 0;
	for _,reg in ipairs(Map.regions) do
		if reg.lastSeen >= 0 then seen = seen + 1; end;
		if reg.lastSeenAir >= 0 then seenA = seenA + 1; end;
		if reg.lastSeenDeep >= 0 then seenD = seenD + 1; end;
	end;
	Game.Log(string.format('%sround %d | %s | units c%d m%d(+%d stored) r%d s%d aa%d p%d b%d | structures %d | seen g%d a%d d%d /%d, enemy %d%s%s | minerals %d energy out %d spare %d | mines %d gen %d | %.2fs',
		Cfg.logPrefix, Sense.round, Mem.posture..(Mem.attackTarget and ('->'..Mem.attackTarget) or ''),
		#Sense.my.constructor, #Sense.my.combat, Sense.storedByRole.combat or 0, #Sense.my.radar, #Sense.my.surveyor, #Sense.my.aa, #Sense.my.plane, #Sense.my.bomb,
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

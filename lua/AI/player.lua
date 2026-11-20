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
--#require "significance.lua"
--#require "formation.lua"


-------------------------------------------------------------------------------
-- Settings
-------------------------------------------------------------------------------

Cfg = {
	stateVersion = 2,
	logPrefix = 'Player AI: ',
	useDirections = false,	-- scripts follow the strategic layer's directions (significance.lua)
	controlCoreHops = 1,	-- regions from mine that still count as the core, where quiet contested ground may be built on
	controlMinReach = 3,	-- cells: firing range assumed of hostiles before any weapon of theirs was seen
	useControl = true,		-- valuable structures only on controlled ground (Control)
	portMineBias = 10,		-- how much better a mine site next to a Nexus port is than the same site elsewhere
	portMineHops = 4,		-- how far from my regions such a site may be
	useFormations = false,	-- formations carry out the directions (formation.lua); needs useDirections
	verbose = true,

	-- desired unit counts
	constructorsEarly = 3,
	constructorsLate = 5,
	earlyTowers = 3,			-- rocket towers at home before the first raid (with a Nexus on the planet they come by round 20 to 35)
	portAttackValue = 12,		-- what a port that is not mine adds to a region as an attack target (a hostile structure counts 3)
	stuckTries = 3,				-- refused orders from one spot before a unit is left alone
	stuckRest = 12,				-- for this many rounds
	maxArmy = 36,				-- combat units of all kinds; more than the ground carries jam the paths
	maxPlants = 4,				-- vehicle plants worth having
	mineralReserve = 200,		-- minerals left untouched by everything the faction does not live on
	reserveFromRound = 30,
	minGenerators = 3,			-- rebuilt before anything else from round 20
	minContainers = 3,			-- mineral containers kept (the faction dies without one)
	spareContainerRound = 10,	-- from this round the spare one is built (in another region than the first)
	portConnectors = true,		-- free builders claim near ports with a structure on a cell beside them
	-- what is put beside a port, the cheapest of these that is researched (never a Connector:
	-- those are deprecated, the game window no longer offers them and the scripts must not use them)
	-- Nothing the faction lives on: beside a port is the front line (with Mineral Container in this
	-- list every lost match had lost all its containers there).
	-- In order of preference, the first that is researched and paid for. (Radar towers beside a
	-- port never got built, "construction did not happen", in every match: not in this list.)
	portStructures = {'Rocket Launcher Tower', 'Fuel Container'},
	portMinMines = 2,			-- mines and generators standing before a builder is spared for a port
	portMinGenerators = 2,
	portBuilders = 2,			-- builders on their way to a port at the same time
	portFarHops = 12,			-- ... and how far once the army is portFarArmy strong: winning takes most ports, not the near ones
	portFarArmy = 16,
	portGuard = 2,				-- structures of mine beside each port of mine (one tower alone lost the port again)
	wallTakeDown = true,		-- once lifted, the whole wall is removed again, not just a gate
	walls = true,				-- wall off the home base (port, mines, plants): the Nexus does not cross or attack walls
	wallMargin = 1,				-- free cells between my structures and the wall, at least
	wallReach = 6,				-- cells out from my structures that count as the outside
	wallMaxCells = 20,			-- no wall where it would take more cells than this: open ground is held by towers
	wallFromRound = 25,			-- ... or as soon as the home port is mine
	wallHold = 60,				-- rounds a finished wall is kept shut and mended: it buys time, not for long. Then a gate is opened
	wallGiveUp = 120,			-- rounds after which an unfinished wall is left as it is
	wallBuilders = 3,			-- terraformers on the wall at the same time
	earlyAA = 1,				-- AA towers at home as soon as the economy stands, whoever the opponent
	wallAA = 2,					-- AA towers inside the wall, whether planes were seen or not (sensor drones show the Nexus where to strike)
	mechanics = 1,				-- always at least this many mechanics: they repair what the raids damage
	mechanicFromRound = 15,
	portNeedsArmy = true,		-- ports other than the nearest one only where my units stand in strength
	berthHome = false,			-- true: the home region is exempt from the berth
	berthProbe = 40,			-- the berth holds this many rounds unproven; after that only if the port owner showed forces
	berthCells = 11,			-- structures keep this many cell steps from a claimable port that is not mine (0: off)
	nextDoorHops = 2,			-- a second claimable port this close to home means towers first, with a radar tower
	portRetry = 15,				-- rounds a port is left alone after an order for it
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
	useMarkers = true,		-- remembered hostile contacts (the game's markers) keep their reach contested
	powerCare = true,		-- switch structures the engine switched off on again in order of need, labs give way
	wantLabsMax = 15,		-- labs late and rich, while T2 is out of reach
	aaBySight = true,		-- with planes about, an AA tower within its sight of every storage, reactor, plant, container and mine
	towersPerRegionMax = 2,	-- towers of one kind for each region held (plus one), unless capped from outside
	uraniumReserve = 150,	-- uranium kept for what fires it (EMP): below it a reactor core rests while the energy carries that
	caps = {},				-- structure type name -> most to have (empty: no caps)
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
	assaultStores = 2,	-- vehicle stores built where a take formation gathers (up to one per six units, five at most; 0: none)
	storesEmptyUnderAir = true,	-- with planes about, nothing is parked in vehicle stores
	unitCaps = {},	-- turret name -> most vehicles of that kind (set from outside)
	plantScrap = 60,	-- a plant with no exit by the terrain for this many rounds is scrapped (false: never)
	plantBlockedRounds = 30,	-- a finished vehicle that cannot deploy is given up after this many rounds
	safeWays = true,		-- units not attacking take the way round known danger and round where vehicles were lost
	scrapTrapped = true,	-- a unit shut in for ten rounds on the exit of a blocked plant is scrapped
	plantKeepExits = 4,		-- exits of a standing plant that no structure may take (the site itself needs plantFreeCells)
	plantFreeCells = 3,
	autoSurveyors = true,	-- surveyors roam automated (the game's own prospecting), replaced when lost
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

-- (the same error hundreds of times a match says nothing new: three per name and round)
local safeSeen = {round = -1, n = {}};
function SafeCall(name, func, ...)
	local ok, err = pcall(func, ...);
	if not ok then
		local r = Sense and Sense.round or 0;
		if safeSeen.round ~= r then safeSeen.round, safeSeen.n = r, {}; end;
		safeSeen.n[name] = (safeSeen.n[name] or 0) + 1;
		if safeSeen.n[name] <= 3 then
			Game.Log(Cfg.logPrefix..'ERROR in '..name..': '..tostring(err));
		end;
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
	Types.uraniumStorage = u['Uranium Storage'];
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

-- still in the type table for old saves, but not to be built
Types.deprecated = {Connector = true};

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
	if tt.name == 'Mechanic' then
		return 'mechanic';
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
		-- (no way leads through a region that is mostly a locked EMP zone: nothing drives there.
		-- It keeps its distance as a place to reach, but the count does not go on through it. An
		-- army sent to the second port by the shortest count stood still for 50 rounds, the
		-- region in between being 90% EMP zone, while three open regions led around it)
		if dist[id] == 0 or Map.EmpShare(id) < 0.5 then
			for nb,_ in pairs(Map.regions[id].neighbors) do
				if dist[nb] == nil then
					dist[nb] = dist[id] + 1;
					Queue.PushRight(q, nb);
				end;
			end;
		end;
	end;
	return dist;
end;

-- Share of a region's cells that lie in a locked EMP zone (0..1), read again every 100 rounds
Map.empShare = {};
function Map.EmpShare(id)
	local round = (Sense and Sense.round) or 0;
	local e = Map.empShare[id];
	if e and round - e.round < 100 then return e.share; end;
	local reg = Map.regions[id];
	local n, emp = 0, 0;
	for _,c in ipairs(reg and reg.cells or {}) do
		n = n + 1;
		local ok, v = pcall(Game.Grid.AffectedByEMP, {cell = c, height = 0});
		if ok and v then emp = emp + 1; end;
	end;
	local share = n > 0 and emp / n or 0;
	Map.empShare[id] = {round = round, share = share};
	return share;
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
	Sense.my = {constructor={}, combat={}, radar={}, surveyor={}, aa={}, plane={}, bomb={}, mechanic={}, other={}};
	Sense.planeFactories = {};
	Sense.myStructures = {};
	Sense.stores = {};
	Sense.stored = {};
	Sense.storedByRole = {};
	Sense.structureCells = {};
	Sense.structureAt = {};		-- cell index -> my structure there
	Sense.structuresByType = {};
	Sense.standingByType = {};
	Sense.off = {};			-- structures of mine the engine switched off
	Sense.offByKind = {generator = 0, mine = 0, port = 0, other = 0};
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
			--[[
			The faction's structures share one balance. When any resource runs negative (fuel for the
			generators, energy for the rest), the engine switches structures off until it holds, and
			the spare then reads 0, not negative: only the switched-off structures show the shortage.
			A mine switched off earns nothing, a Nexus Port switched off is no longer controlled.
			(Plants and plane factories switch themselves off with an empty queue: not counted.)
			]]
			if u.isEnabled == false and u.type ~= Types.plant and u.type ~= Types.planeFactory and not (Mem.keepOff and Mem.keepOff[u.id]) then
				Sense.off[#Sense.off+1] = u;
				local kind = (u.type == Types.generator and 'generator') or (u.type == Types.mine and 'mine')
					or (string.find(tn, 'Nexus Port', 1, true) and 'port') or 'other';
				Sense.offByKind[kind] = Sense.offByKind[kind] + 1;
			end;
			--[[
			A standing tower is named 'Turret:Tower', the type to build 'Turret Tower'. Economy.Count asks by
			the build name and so never saw standing towers, only those on order: the needs took every tower
			for the first of its kind. Counting them right (one batch and 12 duels) cost three times the
			structures and half the ports held, so the needs keep their generous count, and what really
			stands is kept apart for caps and reports (Economy.Standing).
			]]
			local alias = string.gsub(tn, ':Tower$', ' Tower');
			Sense.standingByType[alias] = (Sense.standingByType[alias] or 0) + 1;
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
				airObservers[#airObservers+1] = {pos=w, range=u.scanningRange, armed=(u.turret and u.turret.type.canAttack) or false};
			end;
		else
			if u.scanningRange and u.scanningRange > 0 then
				observers[#observers+1] = {pos=w, range=u.scanningRange};
			end;
			if u.suborbitalScanningRange and u.suborbitalScanningRange > 0 then
				local ok, sub = pcall(Util.ToSuborbit, u.location);
				if ok and sub then
					airObservers[#airObservers+1] = {pos=sub.world, range=u.suborbitalScanningRange,
						armed=(u.turret and u.turret.type.name == 'Anti Aircraft') or false};
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

	Sense.airObservers = airObservers;
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
		-- a Nexus that does more than own ports (bases, vehicles, planes): it will raid and bomb
		if not Mem.nexusActive and e.health > 0 and e.owner and Sense.IsNexus(e.owner)
				and not (e.type and e.type.name and string.find(e.type.name, 'Nexus Port', 1, true)) then
			Mem.nexusActive = round;
			Log('Nexus is active: '..tostring(e.type and e.type.name)..' seen at round '..round);
		end;
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
				elseif e.isIndestructible or e.type.isIndestructible then
					-- (a Nexus port can't be shot: it is no hostile structure to clear out. Counting it
					-- made the army attack its region and stay there for good)
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
					reg.lost = (reg.lost or 0) + 1;	-- (never reset: for what a direction or formation cost)
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
					Control.Attack(a);
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
-- (the faction is called '<Nexus>': the test for 'Nexus' never matched, and everything that
-- depended on it was dead until this was found in the logs)
-- True if the cell lies in a locked EMP zone (with EMP zones on, only some ports can be reached)
function Sense.UnderEMP(cell)
	if cell == nil then return false; end;
	-- (the API wants a location, not a cell; EMP goes by the cell stack, any height does)
	local ok, e = pcall(Game.Grid.AffectedByEMP, {cell = cell, height = 0});
	return ok and e and true or false;
end;

-- A second port close to home that can be claimed (not under EMP, not mine), besides the nearest
-- one: contested ground next door. Whoever holds it, or wants it, is a neighbour from round one.
-- (Against the Nexus the raids of rounds 7 to 12 all came from the base at such a port.)
function Sense.PortNextDoor()
	if Sense.portNextDoorAt == Sense.round then return Sense.portNextDoor; end;
	Sense.portNextDoorAt = Sense.round;
	Sense.portNextDoor = false;
	local okp = pcall(Economy.NextToPort);
	local list = {};
	for idx,cell in pairs(okp and Economy.portCells or {}) do
		local rid = Map.cellRegion[idx];
		local hops = rid and Sense.hops[rid];
		if hops and not Sense.UnderEMP(cell) then list[#list+1] = {idx = idx, hops = hops}; end;
	end;
	table.sort(list, function(a, b) return a.hops < b.hops; end);
	local st = Sense.structureAt or {};
	for i = 2, #list do
		if list[i].hops <= Cfg.nextDoorHops and not st[list[i].idx] then Sense.portNextDoor = true; break; end;
	end;
	return Sense.portNextDoor;
end;

--[[
The berth. Whoever holds a port answers to what stands near it: the closer, the stronger the answer
(the Nexus counts everything within 20 cell steps of its base port, seen or not, a third at 11 to 20
steps, in full from 11, double under 8, and builds forces to match). So structures keep
Cfg.berthCells steps away from a claimable port that is not mine, the nearest one (home) aside.
Returns the steps from the cell to the nearest such port, or nil beyond 20.
]]
function Sense.Berth(idx)
	local okp = pcall(Economy.NextToPort);
	if not okp then return nil; end;
	local list = {};
	for pidx,cell in pairs(Economy.portCells or {}) do
		local rid = Map.cellRegion[pidx];
		local hops = rid and Sense.hops[rid];
		if hops and not Sense.UnderEMP(cell) then list[#list+1] = {idx = pidx, hops = hops, cell = cell}; end;
	end;
	table.sort(list, function(a, b) if a.hops ~= b.hops then return a.hops < b.hops; end; return a.idx < b.idx; end);
	local st = Sense.structureAt or {};
	local key, src = '', {};
	for i = 2, #list do
		if not st[list[i].idx] then key = key..list[i].idx..','; src[#src+1] = list[i].cell; end;
	end;
	if Sense.berthKey ~= key then
		Sense.berthKey = key;
		local dist, edge = {}, {};
		for _,c in ipairs(src) do dist[c.index] = 0; edge[#edge+1] = c; end;
		for d = 1, 20 do
			local nextEdge = {};
			for _,c in ipairs(edge) do
				for _,nb in ipairs(c.neighbors or {}) do
					if dist[nb.index] == nil then dist[nb.index] = d; nextEdge[#nextEdge+1] = nb; end;
				end;
			end;
			edge = nextEdge;
		end;
		Sense.berth = dist;
		if #src > 0 then Log('Berth: '..#src..' claimable port(s) not mine besides the nearest, at cell(s) '..key); end;
	end;
	return Sense.berth[idx];
end;

function Sense.IsNexus(f)
	return f ~= nil and type(f.name) == 'string' and string.find(f.name, 'Nexus', 1, true) ~= nil;
end;

function Sense.NexusPresent()
	for _,f in pairs(Game.factions) do
		if Sense.IsNexus(f) then return true; end;
	end;
	return false;
end;

function Sense.AirMatters()
	if Mem.airSeen >= 0 and Sense.round - Mem.airSeen < 80 then
		return true;
	end;
	-- Against the Nexus, prepare before its planes arrive. ShouldBuildPlanes() reflects the server's
	-- public rule (minRoundForPlanes), not anything sensed
	if Sense.nexus and Mem.nexusActive then
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
-- Control score
-------------------------------------------------------------------------------
--[[
Territory under control: what the faction is playing for, and the yardstick for comparing
AI versions (and for training). A cell counts only if it is both seen by my sensors and
within range of one of my weapons: seeing without reach, or reach without sight, controls
nothing. The surface counts in full. Open cells below ground count for little, so that
burying weapons to claim volume doesn't pay.
Computed every few rounds, logged as "Control: ...", and kept in Score.control.
]]
Score = {
	control = 0, surface = 0, underground = 0,
	every = 5,				-- rounds between measurements
	undergroundWeight = 0.1,	-- an open cell below ground, compared to a surface cell
	undergroundCap = 3,		-- at most this many cells below ground per column
	depth = 8,				-- layers below the surface that are looked at
	ground = {},			-- cell index -> {h, round}: cached surface height
};

--[[
Variants for comparing and training: a script that requires this one (or a personality) and
then overrides settings, e.g.
	--#require "player.lua"
	Cfg.logPrefix = 'gen_017: ';
	Cfg.needWeights = {defense = 4.2, army = 2.1, income = 2.6};
Selected like any other script (Rules/AIPlayers/script). The weights in use are logged once
at the start, the reward as it accrues.
]]
function Score.Announce()
	if Score.announced then return; end;
	Score.announced = true;
	local w, parts = Needs.Weights(), {};
	for k,x in pairs(w) do parts[#parts+1] = string.format('%s=%.3g', k, x); end;
	table.sort(parts);
	Log('Weights: '..table.concat(parts, ', '));
end;

function Score.Update()
	Score.Announce();
	Score.Track();
	if Sense.round % Score.every ~= 0 or Score.lastRound == Sense.round then return; end;
	if not (Game.Terrain and Game.Terrain.GetLowestSensorStrength and Game.Grid.ToLocation) then return; end;
	Score.lastRound = Sense.round;
	-- my weapons, and a vehicle to ask for the ground height
	local armed, probe = {}, nil;
	local function add(u)
		if u.health and u.health > 0 and u.turret and u.turret.type and u.turret.type.canAttack and u.turret.IsInRange then
			armed[#armed+1] = u.turret;
		end;
	end;
	for _,list in pairs(Sense.my) do
		for _,u in ipairs(list) do
			add(u);
			if probe == nil and u.location and not u.location.isSuborbital and u.type and not u.type.isSuborbitalPlane then probe = u; end;
		end;
	end;
	for _,u in ipairs(Sense.myStructures or {}) do add(u); end;
	local surface, below = 0, 0;
	local held = {};	-- region id -> surface cells held there
	Score.heldByRegion = held;
	local heldCell = {};	-- cell index -> true: seen and within range of one of my weapons
	Score.heldCell = heldCell;
	if #armed > 0 then
		local C = Game.Terrain.Content or {};
		local function Held(loc)
			local oks, seen = pcall(Game.Terrain.GetLowestSensorStrength, loc, 1);
			if not (oks and seen and seen > 0) then return false; end;
			local w = loc.world;
			for _,t in ipairs(armed) do
				local okr, hit = pcall(t.IsInRange, w);
				if okr and hit then return true; end;
			end;
			return false;
		end;
		for _,c in ipairs(Game.Grid.unitCells) do
			-- surface height: cached, refreshed now and then (terrain changes slowly)
			local g = Score.ground[c.index];
			if (g == nil or Sense.round - g.round >= 40) and probe then
				local okg, loc = pcall(Game.Grid.FindHighestVehicleGround, probe, c);
				g = {h = okg and loc and loc.height or false, round = Sense.round};
				Score.ground[c.index] = g;
			end;
			if g and g.h then
				local okl, loc = pcall(Game.Grid.ToLocation, c, g.h);
				if okl and loc and Held(loc) then
					surface = surface + 1;
					heldCell[c.index] = true;
					local rid = Map.cellRegion[c.index];
					if rid then held[rid] = (held[rid] or 0) + 1; end;
				end;
				-- open space below: only where I am (my regions), and only a few cells per column
				local reg = Map.RegionOfLoc and okl and loc and Map.RegionOfLoc(loc);
				if reg and Sense.mine and Sense.mine[reg.id] and C.Clear and Game.Terrain.TranslateUnitCell then
					local okt, tc = pcall(Game.Terrain.TranslateUnitCell, c);
					local n = 0;
					for h = g.h - 2, g.h - Score.depth, -1 do
						if n >= Score.undergroundCap then break; end;
						local okc, content = false, nil;
						if okt and tc then okc, content = pcall(Game.Terrain.GetContent, tc, h); end;
						if okc and content == C.Clear then
							local okb, bl = pcall(Game.Grid.ToLocation, c, h);
							if okb and bl and Held(bl) then n = n + 1; end;
						end;
					end;
					below = below + n;
				end;
			end;
		end;
	end;
	Score.surface, Score.underground = surface, below;
	Score.control = surface + below * Score.undergroundWeight;
	Log(string.format('Control: %.1f (surface %d, underground %d x %.2f, %d weapons)',
		Score.control, surface, below, Score.undergroundWeight, #armed));
	Score.Settle();
end;

--[[
Reward: how the last rounds went, as one number built from signals that each mean something.
  control     gained territory is positive, lost territory negative (change of Score.control)
  reserve     combat units at hand, flattening fast: n / (n + 2). One or two spare tanks are
              almost always good; a hundred are worth hardly more than ten, unless they are
              sent at the enemy, and then they earn through territory and kills
  units       a little for each hostile unit or tower destroyed, a little off for each own one lost
  structures  a lot for each hostile structure destroyed that is no tower (plants, labs, mines,
              generators, ...), a lot off for each own one lost
  mining      what the mines bring in per round (minerals, and fuel at half value), counted at
              every measurement: mining early and keeping it up pays all match long. What is
              mined into a full storage is wasted and counts a tenth, so more mines than
              the faction can use don't pay (they cost energy)
Own losses are exact. Kills are an estimate from what the client sees: a hostile that was in
reach of my weapons, is gone now, and whose place I still see. (The server knows exactly; a
referee there can replace the estimate.) Structures I demolish myself are no loss.
Logged with the control score as "Reward: ..."; Score.reward is the sum since the last
measurement, Score.total the sum over the match.
]]
Score.w = {control = 1, reserve = 10, unit = 2, structure = 20, mining = 0.2};
Score.reward, Score.total = 0, 0;
Score.parts = {control = 0, reserve = 0, units = 0, structures = 0};
Score.mineBefore = {};		-- id -> {tower, structure} of my units and structures last round
Score.hostileBefore = {};	-- id -> {loc, tower, structure, reach} of hostiles last round
Score.demolished = {};		-- ids I took down myself

local function IsTower(u)
	return u.type and u.type.isStructure and u.type.turretType ~= nil and u.type.turretType.canAttack and true or false;
end;

-- every round: who is gone since the last one?
function Score.Track()
	local P = Score.parts;
	-- my own
	local now = {};
	local function mine(u)
		if u.id then now[u.id] = {structure = u.type and u.type.isStructure and true or false, tower = IsTower(u),
			cell = u.location and not u.location.isSuborbital and u.location.cell or nil, mobile = u.type and u.type.isMobile and u.storedIn == nil}; end;
	end;
	for _,list in pairs(Sense.my) do for _,u in ipairs(list) do mine(u); end; end;
	for _,u in ipairs(Sense.stored or {}) do mine(u); end;
	for _,u in ipairs(Sense.myStructures or {}) do mine(u); end;
	for id,e in pairs(Score.mineBefore) do
		if now[id] == nil and not Score.demolished[id] then
			if e.structure and not e.tower then P.structures = P.structures - 1; else P.units = P.units - 1; end;
			-- (where a vehicle of mine was lost: ways are counted around it for a while, see Move.Cost)
			if e.mobile and e.cell and Move and Move.NoteDeath then Move.NoteDeath(e.cell); end;
		end;
	end;
	Score.mineBefore = now;
	-- theirs: gone from a place I still see, and it was in reach of my weapons
	local armed = {};
	for _,list in pairs(Sense.my) do
		for _,u in ipairs(list) do
			if u.turret and u.turret.type and u.turret.type.canAttack and u.turret.IsInRange then armed[#armed+1] = u.turret; end;
		end;
	end;
	for _,u in ipairs(Sense.myStructures or {}) do
		if u.turret and u.turret.type and u.turret.type.canAttack and u.turret.IsInRange then armed[#armed+1] = u.turret; end;
	end;
	local known = {};
	for id,e in pairs(Game.hostileUnits or {}) do
		if e.health and e.health > 0 and e.location then
			local reach = false;
			local w = e.location.world;
			for _,t in ipairs(armed) do
				local ok, hit = pcall(t.IsInRange, w);
				if ok and hit then reach = true; break; end;
			end;
			known[id] = {loc = e.location, structure = e.type and e.type.isStructure and true or false, tower = IsTower(e), reach = reach};
		end;
	end;
	if Game.Terrain and Game.Terrain.GetLowestSensorStrength then
		for id,e in pairs(Score.hostileBefore) do
			if known[id] == nil and e.reach and (Game.hostileUnits or {})[id] == nil then
				local ok, seen = pcall(Game.Terrain.GetLowestSensorStrength, e.loc, 1);
				if ok and seen and seen > 0 then
					if e.structure and not e.tower then P.structures = P.structures + 1; else P.units = P.units + 1; end;
				end;
			end;
		end;
	end;
	Score.hostileBefore = known;
end;

-- with each control measurement: turn the signals into the reward since the last one
function Score.Settle()
	local P, w = Score.parts, Score.w;
	local n = #Sense.my.combat + (Sense.storedByRole and Sense.storedByRole.combat or 0);
	local reserve = n / (n + 2);
	if Score.controlBefore ~= nil then
		P.control = Score.control - Score.controlBefore;
		P.reserve = reserve - Score.reserveBefore;
		-- mining speed: supply of the working mines; a full storage wastes it
		local cur, max = Economy.Resources();
		local sup = cur and cur.supply or {};
		-- (more mines than the faction can use is no gain: they cost energy. What piles up in a
		-- full storage counts a tenth, for fuel as for minerals; mines without power supply nothing)
		local function used(res)
			local x = sup[res] or 0;
			local cap = max and max.storage and max.storage[res] or 0;
			if cap > 0 and (cur.storage[res] or 0) >= cap - 1 then x = x * 0.1; end;
			return x;
		end;
		P.mining = used('minerals') + used('fuel') * 0.5;
		Score.reward = w.control * P.control + w.reserve * P.reserve + w.unit * P.units + w.structure * P.structures
			+ w.mining * P.mining;
		Score.total = Score.total + Score.reward;
		Log(string.format('Reward: %+.1f (control %+.1f, reserve %+.2f, units %+d, structures %+d, mining %.1f); total %.1f; combat units %d',
			Score.reward, P.control, P.reserve, P.units, P.structures, P.mining, Score.total, n));
	end;
	Score.controlBefore, Score.reserveBefore = Score.control, reserve;
	P.control, P.reserve, P.units, P.structures = 0, 0, 0, 0;
end;

-------------------------------------------------------------------------------
-- Control: how much is a location mine?
-------------------------------------------------------------------------------
--[[
The one question everything that builds asks first. Per surface cell:
  0  not seen       outside my sensors. What I cannot see I do not control
  1  contested      seen, but
                    * within firing range of something I cannot see, or
                    * within sensor or firing range of a hostile unit seen, or of an attack on
                      anything of mine, now or in the last rounds (Control.memory), or
                    * overlooked by higher ground within firing range that is not controlled
                      itself: a valley is contested unless the peaks around it are mine, or
                    * with planes about (Sense.AirMatters): not watched in the air layer. Seeing
                      what flies there is enough until a bomber shows up (or anything of mine
                      is hit from the air); from then on only what my AA and armed planes
                      watch counts. Towers that cannot shoot upwards change nothing here; what
                      is contested only for this reason is in Control.air
  2  controlled     what is left: seen, quiet, and no blind or hostile spot can reach it
"Firing range" is the longest weapon range seen on a hostile so far, and Cfg.controlMinReach
cells before any was seen.

Sightings and attacks count at once (every round); sensor coverage and the heights are gone
through every Control.every rounds.

What may be built where (Control.Allows):
  controlled                 anything
  hostiles near (seen or     nothing of value, wherever it is
  attacking, Control.hot)
  the core: my regions and   also on ground that is contested only for the quiet reasons (near the
  those next to them         edge of sight, below unwatched heights, open air): a base has to start
                             somewhere, and at the start nothing at all is controlled
  the fringe, further out    only on controlled ground; a mine also where my weapons cover the cell
                             (so a port mine out there waits for the towers or the army)
  towers                     anywhere: they, and radar, are what makes ground mine
Logged as "Control map: ...".
]]
Control = {
	every = 5,
	memory = 15,		-- rounds a sighting or an attack keeps its surroundings contested
	peakMargin = 2,		-- height levels above a cell from which ground overlooks it
	level = {},			-- cell index -> 0, 1, 2
	air = {},			-- cell index -> true: contested only because the air above is not watched
	hot = {},			-- cell index -> true: within reach of a hostile seen or an attack remembered
	zones = {},			-- key -> {x, y, z, r2, round}
	reach = 0,			-- longest hostile weapon range seen (world units)
	byRegion = {},		-- region id -> {controlled = share, contested = share}
	attackCount = 0,
	bomberSeen = nil,	-- round a hostile bomber was last seen, or something of mine hit from the air
	bomberMemory = 80,
};

function Control.At(cellOrLoc)
	local cell = cellOrLoc and (cellOrLoc.cell or cellOrLoc);
	local l = cell and cell.index and Control.level[cell.index];
	if l == nil then return 2; end;
	return l;
end;

-- may something of value be built here? (second result: why not)
function Control.Allows(loc, forMine)
	local cell = loc.cell or loc;
	local idx = cell.index;
	local l = Control.At(cell);
	if l == 2 then return true; end;
	if Control.hot[idx] then return false, 'hostiles near'; end;
	local rid = Map.cellRegion[idx];
	if rid and (Sense.hops[rid] or 99) <= Cfg.controlCoreHops then return true; end;
	if l == 0 then return false, 'not seen'; end;
	if forMine and Score.heldCell and Score.heldCell[idx] then return true; end;
	return false, Control.air[idx] and 'open to planes' or 'contested';
end;

function Control.Of(regId, what)
	local r = Control.byRegion[regId];
	if r == nil then return what == 'controlled' and 1 or 0; end;
	return r[what] or 0;
end;

function Control.Spacing()
	if Control.spacing == nil then
		local c = Game.Grid.unitCells and Game.Grid.unitCells[1];
		local n = c and c.neighbors and c.neighbors[1];
		if n then
			Control.spacing = math.sqrt(Dist2(c.center, n.center)) * Map.groundRadius;
		end;
	end;
	return Control.spacing or 1;
end;

function Control.Reach()
	return math.max(Control.reach, Control.Spacing() * Cfg.controlMinReach);
end;

-- everything around this position is contested from now on
function Control.Note(key, pos, range)
	if pos == nil then return; end;
	local r = math.max(range or 0, Control.Reach());
	local z = {x = pos.x, y = pos.y, z = pos.z, r2 = r * r, round = Sense.round};
	Control.zones[key] = z;
	Control.fresh[#Control.fresh+1] = z;
end;

-- called for every hostile shot seen (Sense.Update)
function Control.Attack(a)
	Control.fresh = Control.fresh or {};
	Control.attackCount = Control.attackCount + 1;
	local reachBack = (a.source and a.impact) and math.sqrt(Dist2(a.source, a.impact)) * 1.1 or 0;
	if reachBack > Control.reach then Control.reach = reachBack; end;
	Control.Note('a'..Control.attackCount, a.source, reachBack);
	Control.Note('i'..Control.attackCount, a.impact, 0);
end;

local function InZone(zones, c, R)
	local x, y, z = c.center.x * R, c.center.y * R, c.center.z * R;
	for _,zn in pairs(zones) do
		local dx, dy, dz = x - zn.x, y - zn.y, z - zn.z;
		if dx*dx + dy*dy + dz*dz <= zn.r2 then return true; end;
	end;
	return false;
end;

function Control.Update()
	if not (Game.Terrain and Game.Terrain.GetLowestSensorStrength and Game.Grid.ToLocation and Game.Grid.unitCells) then return; end;
	Control.fresh = Control.fresh or {};
	local round, R = Sense.round, Map.groundRadius;
	-- hostiles in sight: where they are, and as far as they see or shoot
	for _,a in ipairs(Sense.recentAttacks or {}) do
		if a.layer == 'air' then Control.bomberSeen = round; end;
	end;
	for _,e in pairs(Game.hostileUnits) do
		local tt = e.turret and e.turret.type;
		if e.health and e.health > 0 and e.type.name and string.find(e.type.name, 'Bomber', 1, true) then Control.bomberSeen = round; end;
		if e.health and e.health > 0 and e.location and not (e.isIndestructible or e.type.isIndestructible)
				and (e.type.isMobile or (tt and tt.canAttack)) then
			local range = (tt and tt.canAttack and tt.flatAttackRange or 0) * 1.25;
			if range > Control.reach then Control.reach = range; end;
			Control.Note('u'..tostring(e.id), e.location.world, math.max(range, e.type.scanningRange or 0));
		end;
	end;
	--[[
	What was seen and is out of sight now: the game keeps a marker for each hostile contact, with the
	unit as it was last seen (the Nexus AI remembers where its enemies are the same way). A tower
	remembered there still reaches as far as it did; a vehicle is taken to have moved on after 10
	rounds. A marker whose cell is in sight again with nothing hostile on it is done with.
	]]
	if Cfg.useMarkers and Game.myMarkers then
		local sensed = {};
		for _,e in pairs(Game.hostileUnits) do
			if e.health and e.health > 0 and e.location then sensed[e.location.cell.index] = true; end;
		end;
		local n = 0;
		for _,m in pairs(Game.myMarkers) do
			local e = m.type == 'HostileContact' and m.unitReference;
			if e and e.location and e.type and not e.isOwnedByMe and not sensed[e.location.cell.index] then
				local tt = e.turret and e.turret.type;
				Control.markersGone = Control.markersGone or {};
				if Control.markersGone[m.id] then
					-- (already given up; the list may lag a round)
				elseif (m.age or 0) > 0 and (Control.level[e.location.cell.index] or 0) > 0 and m.Destroy then
					Control.markersGone[m.id] = true;
					pcall(m.Destroy);	-- seen empty
				elseif not (e.isIndestructible or e.type.isIndestructible) and tt and tt.canAttack
						and (not e.type.isMobile or (m.age or 0) < 10) then
					local range = (tt.flatAttackRange or 0) * 1.25;
					Control.Note('m'..tostring(m.id), e.location.world, math.max(range, e.type.scanningRange or 0));
					n = n + 1;
				end;
			end;
		end;
		if n ~= Control.markerZones then
			Control.markerZones = n;
			Log('Control: '..n..' remembered hostile weapon(s) out of sight keep their ground contested (round '..round..')');
		end;
	end;
	for key,z in pairs(Control.zones) do
		if round - z.round > Control.memory then Control.zones[key] = nil; end;
	end;
	local full = round % Control.every == 0 or Control.lastFull == nil;
	if not full then
		-- at once: what a new sighting or attack reaches is no longer controlled
		if #Control.fresh > 0 then
			for _,c in ipairs(Game.Grid.unitCells) do
				if not Control.hot[c.index] and InZone(Control.fresh, c, R) then
					Control.hot[c.index] = true;
					if Control.level[c.index] == 2 then Control.level[c.index] = 1; end;
				end;
			end;
		end;
		Control.fresh = {};
		return;
	end;
	Control.fresh = {};
	Control.lastFull = round;
	local probe = nil;
	for _,list in pairs(Sense.my) do
		for _,u in ipairs(list) do
			if probe == nil and u.location and not u.location.isSuborbital and u.type and not u.type.isSuborbitalPlane then probe = u; end;
		end;
	end;
	local cells = Game.Grid.unitCells;
	local seen, height, first = {}, {}, {};
	-- 1. what my sensors see, at the surface
	local queue, dist = {}, {};
	for _,c in ipairs(cells) do
		local g = Score.ground[c.index];
		if (g == nil or round - g.round >= 40) and probe then
			local okg, loc = pcall(Game.Grid.FindHighestVehicleGround, probe, c);
			g = {h = okg and loc and loc.height or false, round = round};
			Score.ground[c.index] = g;
		end;
		local s = false;
		if g and g.h then
			height[c.index] = g.h;
			local okl, loc = pcall(Game.Grid.ToLocation, c, g.h);
			if okl and loc then
				local oks, strength = pcall(Game.Terrain.GetLowestSensorStrength, loc, 1);
				s = (oks and strength and strength > 0) or false;
			end;
		end;
		seen[c.index] = s;
		if not s then
			queue[#queue+1] = c;
			dist[c.index] = 0;
		end;
	end;
	-- 2. the belt within firing range of what I cannot see
	local rings = math.max(1, math.ceil(Control.Reach() / Control.Spacing()));
	local at = 1;
	while at <= #queue do
		local c = queue[at];
		at = at + 1;
		local d = dist[c.index];
		if d < rings then
			for _,nb in ipairs(c.neighbors) do
				if dist[nb.index] == nil then
					dist[nb.index] = d + 1;
					queue[#queue+1] = nb;
				end;
			end;
		end;
	end;
	local zones = 0;
	for _ in pairs(Control.zones) do zones = zones + 1; end;
	Control.hot = {};
	local hotCount = 0;
	for _,c in ipairs(cells) do
		local hot = zones > 0 and InZone(Control.zones, c, R);
		if hot then
			Control.hot[c.index] = true;
			hotCount = hotCount + 1;
		end;
		first[c.index] = seen[c.index] and dist[c.index] == nil and not hot;
	end;
	-- 3. higher ground within range that is not controlled overlooks what lies below it:
	-- the highest such ground around each cell, spread ring by ring
	local high = {};
	for _,c in ipairs(cells) do
		high[c.index] = (not first[c.index]) and (height[c.index] or -100) or -100;
	end;
	for _ = 1,rings do
		local nextHigh = {};
		for _,c in ipairs(cells) do
			local h = high[c.index];
			for _,nb in ipairs(c.neighbors) do
				local x = high[nb.index];
				if x and x > h then h = x; end;
			end;
			nextHigh[c.index] = h;
		end;
		high = nextHigh;
	end;
	local counts = {[0] = 0, [1] = 0, [2] = 0};
	local per = {};
	local planes = Sense.AirMatters();
	local watchers = Sense.airObservers or {};
	local airR = Map.airRadius or R;
	local airOnly = 0;
	local bombers = Control.bomberSeen ~= nil and round - Control.bomberSeen <= Control.bomberMemory;
	Control.air = {};
	for _,c in ipairs(cells) do
		local l = 0;
		if seen[c.index] then
			l = (first[c.index] and high[c.index] < (height[c.index] or 0) + Control.peakMargin) and 2 or 1;
		end;
		if l == 2 and planes then
			local x, y, z = c.center.x * airR, c.center.y * airR, c.center.z * airR;
			local watched = false;
			for _,o in ipairs(watchers) do
				local dx, dy, dz = x - o.pos.x, y - o.pos.y, z - o.pos.z;
				if (o.armed or not bombers) and dx*dx + dy*dy + dz*dz <= o.range * o.range then watched = true; break; end;
			end;
			if not watched then
				l = 1;
				Control.air[c.index] = true;
				airOnly = airOnly + 1;
			end;
		end;
		Control.level[c.index] = l;
		counts[l] = counts[l] + 1;
		local rid = Map.cellRegion[c.index];
		if rid then
			local p = per[rid];
			if p == nil then p = {n = 0, [0] = 0, [1] = 0, [2] = 0}; per[rid] = p; end;
			p.n = p.n + 1;
			p[l] = p[l] + 1;
		end;
	end;
	Control.byRegion = {};
	for rid,p in pairs(per) do
		Control.byRegion[rid] = {controlled = p[2] / p.n, contested = p[1] / p.n};
	end;
	if round % 20 == 0 then
		Log(string.format('Control map: %d cells controlled, %d contested (%d only for the air above%s), %d not seen; %d cells with hostiles near; firing range %.1f cells, %d sightings and attacks remembered',
			counts[2], counts[1], airOnly, bombers and ', bombers about' or (planes and ', planes about' or ''), counts[0], hotCount, Control.Reach() / Control.Spacing(), zones));
	end;
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
--[[
The server can refuse a destination the client found a path to ("Unable to plot path to
destination. Task rejected."). The script is not told, and would give the same order every round.
So: what a unit was sent to from a spot it then did not leave is not tried again from there.
]]
Move.tried = {};	-- unit id -> {from = spot, to = {destination -> true}}
local function Spot(loc)
	local w = loc.world;
	return string.format('%.2f,%.2f,%.2f', w.x, w.y, w.z);
end;
local function Tried(u)
	local from = Spot(u.location);
	local t = Move.tried[u.id];
	if t == nil or t.from ~= from then
		t = {from = from, to = {}};
		Move.tried[u.id] = t;
	end;
	return t.to;
end;

-- avoidThreat: true avoids every known threat, 'arty' only remembered artillery fire (attacking units)
--[[
A unit that has had Cfg.stuckTries orders from the same spot and still stands there is boxed in
(by structures or by its own side): each further order is refused by the server too, a dozen
neighbours one after the other, a hundred rounds long. It is left alone for Cfg.stuckRest rounds
and then tried afresh. (Three parked AA units made over 300 of the 699 refusals in one match.)
]]
function Move.Stuck(u)
	local t = Move.tried[u.id];
	if t == nil or t.from ~= Spot(u.location) then return false; end;
	local n = 0;
	for _ in pairs(t.to) do n = n + 1; end;
	if n < Cfg.stuckTries then return false; end;
	if t.rest == nil then t.rest = Sense.round + Cfg.stuckRest; end;
	if Sense.round >= t.rest then Move.tried[u.id] = nil; return false; end;
	return true;
end;

--[[
Ways around what kills. The game's shortest path knows nothing of towers: units sent to gather
next door took the straight way through the reach of two missile towers, one of them never
sensed, and of an army of 30 a third was lost on the way to its own staging region, while the
rest, refusing the dangerous step, never left home. So the path is asked for with a price on
cells in reach of known hostile weapons and on cells where vehicles of mine were lost in the
last 80 rounds (and their neighbours): the way goes round if there is one.
]]
Move.deaths = {};	-- cell index -> {round, n}
function Move.NoteDeath(cell)
	local function mark(c, w)
		local d = Move.deaths[c.index];
		if d and Sense.round - d.round < 80 then d.n = d.n + w; d.round = Sense.round;
		else Move.deaths[c.index] = {round = Sense.round, n = w}; end;
	end;
	mark(cell, 2);
	for _,nb in ipairs(cell.neighbors or {}) do mark(nb, 1); end;
end;
Move.costCache = {round = -1, at = {}};
function Move.Cost(loc, cost)
	local ok, rs = pcall(function()
		if Move.costCache.round ~= Sense.round then Move.costCache = {round = Sense.round, at = {}}; end;
		local idx = loc.cell.index;
		local extra = Move.costCache.at[idx];
		if extra == nil then
			extra = 0;
			local d = Move.deaths[idx];
			if d and Sense.round - d.round < 80 then extra = extra + 15 * math.min(4, d.n); end;
			if Move.IsDangerous(loc) then extra = extra + 40; end;
			Move.costCache.at[idx] = extra;
		end;
		return cost + extra;
	end);
	return ok and rs or cost;
end;

function Move.CostLost(loc, cost)
	local d = Move.deaths[loc.cell.index];
	if d and Sense.round - d.round < 80 then return cost + 15 * math.min(4, d.n); end;
	return cost;
end;

function Move.Towards(u, goalWorld, goalCell, avoidThreat, mayEnterUnknown)
	-- (Move.Stuck is not applied to every unit here: with it the losers against the Nexus died at
	-- a median of round 67 where it had been 137. It is used where the cause is known, the AA posts.)
	local tried = Tried(u);
	local danger = Move.IsDangerous;
	if avoidThreat == 'arty' then
		danger = function(loc) return Military.InArtilleryZone(loc.world) ~= nil; end;
	end;
	Move.why = nil;
	if goalCell then
		local dest = Move.GroundAt(u, goalCell);
		if dest == nil then Move.why = 'no ground for it at cell '..goalCell.index; end;
		if dest and dest ~= u.location then
			-- (keeping out of danger: the way that costs least with danger priced in)
			-- (who goes in to attack pays for cells where vehicles were lost, not for the reach of the
			-- towers it is sent against: priced in full, part of an army went three regions round)
			local price = nil;
			-- (nor for the cells where others fell: with those priced, the last leg of an advance
			-- went round by twelve regions)
			if avoidThreat and Cfg.safeWays and not (avoidThreat == 'arty' and mayEnterUnknown) then price = Move.Cost; end;
			local ok, path = pcall(u.FindShortestPath, dest, price, false);
			if ok and path and #path > 0 then
				local limit = math.max(1, u.movementSpeed * 3);
				local pick = path[#path];
				-- don't commit to the full route; re-plan each round as the picture changes
				for _,n in ipairs(path) do
					if n.movementCostsFromRoot <= limit then pick = n; end;
				end;
				-- (the way was priced for danger and is the best there is: up to the last step
				-- before the danger, not none at all)
				if avoidThreat and danger(pick.location) then
					local short = nil;
					for _,n in ipairs(path) do
						if n.movementCostsFromRoot > limit or danger(n.location) then break; end;
						short = n;
					end;
					if short then pick = short; end;
				end;
				if (not avoidThreat or not danger(pick.location)) and not tried[Spot(pick.location)] then
					tried[Spot(pick.location)] = true;
					u.DirectTo(pick);
					return true;
				end;
				Move.why = danger(pick.location) and ('the way to cell '..goalCell.index..' enters danger at once (cell '..pick.location.cell.index..')') or 'the step was tried already';
			else
				Move.why = 'no path to cell '..goalCell.index;
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
		if d < bestD and not tried[Spot(n.location)] then
			best, bestD = n, d;
		end;
	end;
	if best then
		tried[Spot(best.location)] = true;
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

-- What really stands or is on order of a type to build (towers included, see Sense.Update)
function Economy.Standing(t)
	if t == nil then return 0; end;
	local alias = string.gsub(t.name, ':Tower$', ' Tower');
	local n = Sense.standingByType[alias] or 0;
	for _,u in ipairs(Sense.my.constructor or {}) do
		for _,task in ipairs(u.tasks or {}) do
			if task.job and task.job.buildingType == alias then n = n + 1; end;
		end;
	end;
	return n;
end;

function Economy.Capped(t)
	if t == nil then return false; end;
	local cap = Cfg.caps and Cfg.caps[(string.gsub(t.name, ':Tower$', ' Tower'))];
	-- (towers without a cap from outside: so many of a kind for each region held and no more. With
	-- minerals at the cap one match put up 22 rocket, 21 missile and 17 radar towers while the
	-- plants stood blocked)
	if cap == nil and t.isStructure and t.turretType ~= nil and Cfg.towersPerRegionMax then
		local regs = 0;
		for _ in pairs(Sense.mine or {}) do regs = regs + 1; end;
		cap = Cfg.towersPerRegionMax * math.max(1, regs) + 1;
		-- (AA towers see only about 4 cells into the sub orbit: cover takes more of them)
		if Types.aaTower and t.turretType == Types.aaTower.turretType and Sense.AirMatters() then cap = cap * 2; end;
	end;
	return cap ~= nil and Economy.Standing(t) >= cap;
end;

--[[
Keeping the lights on. The engine switches structures off when fuel or energy runs negative, in
no useful order (the port and the mines as readily as a lab), and does not reliably switch them
on again when there is power to spare. So: whatever is off and matters most is switched on as
soon as the spare carries it (generators need 10 fuel, the rest 10 energy), and while a port,
mine or generator is off for lack of energy, labs give way.
]]
local function IsFuelMine(u)
	if u.type ~= Types.mine or not u.location then return false; end;
	local ok, lvl = pcall(Game.Grid.GetNaturalResourceAt, 'Fuel', u.location.cell);
	if ok and lvl ~= nil and lvl > 0 then return true; end;
	-- (asked both ways: a mine on a fuel cell was once traded away as a mineral mine)
	local ok2, all = pcall(Game.Grid.GetNaturalResourcesAt, u.location.cell);
	return ok2 and all ~= nil and (all.fuel or 0) > 0;
end;

local function OffRank(u)
	local n = u.type.name;
	-- (generators idle for fuel: the fuel mine comes before everything, or nothing else ever comes back)
	if ((Sense.offByKind or {}).generator or 0) > 0 and IsFuelMine(u) then
		-- (the richest fuel cell first: a mine yields by the level of its cell, and only one that
		-- carries a generator changes anything)
		local ok, lvl = pcall(Game.Grid.GetNaturalResourceAt, 'Fuel', u.location.cell);
		return 0 - math.min(0.9, ((ok and lvl) or 0) / 1000);
	end;
	if string.find(n, 'Nexus Port', 1, true) then return 1; end;
	if u.type == Types.mine then return 2; end;
	if u.type == Types.generator then return 3; end;
	if u.type == Types.mineralContainer then return 4; end;
	if u.type == Types.lab then return 9; end;
	-- (with planes about the AA towers come right after the mines: only they see and reach the
	-- sub orbit, and two bombers nobody saw took a base apart in 40 rounds)
	if Types.aaTower and u.type and u.type.turretType == Types.aaTower.turretType and u.type.isStructure and Sense.AirMatters() then return 2.5; end;
	return 6;
end;

--[[
Uranium for what fires it. A reactor core burns what a port brings in (5 a round), so with one
running the store never grows and an EMP has nothing to fire (one match: 85 to 125 in 100
rounds). While a unit of mine fires uranium and the store is under the reserve, a reactor core
rests if the energy carries that (40 to spare and nothing off for lack of power); it comes back
when the store is filled or something goes off.
]]
function Economy.UraniumCare()
	if not Cfg.powerCare or not Types.reactor or not Cfg.uraniumReserve then return; end;
	Mem.uraniumRest = Mem.uraniumRest or {};
	local cur = Economy.Resources();
	local store, energy = cur.storage.uranium or 0, cur.balance.energy or 0;
	-- (what is off and matters: mines, generators, the port. Labs off do not count, they are
	-- what gives way to the EMP's uranium)
	local ob = Sense.offByKind or {};
	local vitalOff = (ob.mine or 0) + (ob.generator or 0) + (ob.port or 0);
	for _,st in ipairs(Sense.myStructures) do
		if st.type == Types.reactor and Mem.uraniumRest[st.id] and st.Enable then
			if store >= Cfg.uraniumReserve + 50 or vitalOff > 0 then
				if pcall(st.Enable, true) then
					Mem.uraniumRest[st.id] = nil;
					if Mem.keepOff then Mem.keepOff[st.id] = nil; end;
					Log(string.format('Reactor Core on cell %d switched on again at round %d (uranium %d, %d structure(s) off)', st.location.cell.index, Sense.round, store, #Sense.off));
				end;
			end;
			return;
		end;
	end;
	if store >= Cfg.uraniumReserve or vitalOff > 0 then return; end;
	-- (the 40 energy it gives must be to spare, or come from labs that then rest with it: an
	-- EMP stood without ammunition before a tower while the reactor burned what came in)
	local labsOn = {};
	for _,st in ipairs(Sense.myStructures) do
		if st.type == Types.lab and st.isEnabled ~= false and st.Enable then labsOn[#labsOn+1] = st; end;
	end;
	local short = math.max(0, math.ceil((50 - energy) / 10));
	if short > #labsOn then return; end;
	local fires = false;
	local function look(u)
		local tt = u.type and u.type.turretType;
		if tt and tt.name and (string.find(tt.name, 'EMP', 1, true) or string.find(tt.name, 'Doomsday', 1, true)) then fires = true; end;
	end;
	for _,list in pairs(Sense.my) do for _,u in ipairs(list) do look(u); end; end;
	for _,u in ipairs(Sense.stored or {}) do look(u); end;
	if not fires then return; end;
	for _,st in ipairs(Sense.myStructures) do
		if st.type == Types.reactor and st.isEnabled ~= false and st.Enable and not (Mem.keepOff and Mem.keepOff[st.id]) then
			if pcall(st.Enable, false) then
				Mem.shed = Mem.shed or {};
				for i = 1, short do
					if pcall(labsOn[i].Enable, false) then Mem.shed[labsOn[i].id] = Sense.round; end;
				end;
				if short > 0 then Log(string.format('%d laboratory(ies) switched off with the reactor core at round %d', short, Sense.round)); end;
				Mem.uraniumRest[st.id] = Sense.round;
				Mem.keepOff = Mem.keepOff or {};
				Mem.keepOff[st.id] = true;
				Log(string.format('Reactor Core on cell %d rests from round %d: uranium %d under the reserve of %d for what fires it, energy spare %d', st.location.cell.index, Sense.round, store, Cfg.uraniumReserve, energy));
			end;
			return;
		end;
	end;
end;

function Economy.Power()
	if #Sense.off == 0 or not Cfg.powerCare then return; end;
	Mem.shed = Mem.shed or {};
	local cur = Economy.Resources();
	local energy, fuel = cur.balance.energy or 0, cur.balance.fuel or 0;
	--[[
	The trade that breaks the deadlock: a fuel mine is off, there is no energy to spare for it, and
	the fuel does not carry another generator (or generators already stand idle). Then no generator
	can ever bring the energy, because the fuel for it is in the mine that is off. A running
	mineral mine is switched off and the fuel mine on, in the same round (done by hand it took
	fuel from 12 to 61 and energy from 20 to 120 within 20 rounds; the engine then brings the
	rest back, or the lines below do).
	]]
	if energy < 10 and (fuel < 10 or (Sense.offByKind.generator or 0) > 0) then
		local fuelMine = nil;
		for _,u in ipairs(Sense.off) do
			if u.Enable and u.type == Types.mine and not fuelMine then
				local ok, lvl = pcall(Game.Grid.GetNaturalResourceAt, 'Fuel', u.location.cell);
				if ok and lvl and lvl > 0 then fuelMine = u; end;
			end;
		end;
		if fuelMine then
			local give, running = nil, 0;
			for _,st in ipairs(Sense.myStructures) do
				-- (never one that was switched on in the last 20 rounds, by the care or from outside)
				local t0 = Mem.onTried and Mem.onTried[st.id];
				if st.type == Types.mine and st.isEnabled ~= false and st.Enable and not IsFuelMine(st)
					and not (t0 and Sense.round - t0.round < 20) and not (Mem.keepOn and Mem.keepOn[st.id] and Sense.round - Mem.keepOn[st.id] < 30) then
					running = running + 1; give = give or st;
				end;
			end;
			-- (no mineral mine running to trade: a lab or tower gives way instead, below)
			if give and pcall(give.Enable, false) then
				Mem.shed[give.id] = Sense.round;
				local okOn = pcall(fuelMine.Enable, true);
				local gl = select(2, pcall(Game.Grid.GetNaturalResourceAt, 'Fuel', give.location.cell));
				local gm = select(2, pcall(Game.Grid.GetNaturalResourceAt, 'Minerals', give.location.cell));
				Log(string.format('Mineral mine on cell %d (fuel %s, minerals %s) switched off at round %d for the fuel mine on cell %d (%s): energy spare %d, fuel spare %d, %d generator(s) idle',
					give.location.cell.index, tostring(gl), tostring(gm), Sense.round, fuelMine.location.cell.index, okOn and 'switched on' or 'could not be switched on', energy, fuel, Sense.offByKind.generator or 0));
				return;
			end;
		end;
	end;
	local list = {};
	for _,u in ipairs(Sense.off) do
		-- (what I switched off myself to make room comes back last, and only with room to spare)
		if u.Enable then list[#list+1] = u; end;
	end;
	table.sort(list, function(a, b)
		local ra, rb = OffRank(a), OffRank(b);
		if ra > 0 and Mem.shed[a.id] then ra = ra + 10; end;
		if rb > 0 and Mem.shed[b.id] then rb = rb + 10; end;
		if ra ~= rb then return ra < rb; end;
		return tostring(a.id) < tostring(b.id);
	end);
	local on = 0;
	local vitalWaits = false;
	for _,u in ipairs(list) do
		local isGen = u.type == Types.generator;
		local need = 10;
		local have = isGen and fuel or energy;
		local shed = Mem.shed[u.id];
		-- (what went off again right after it was switched on lacks something else, an input for
		-- instance: tried again after 5 rounds, then 10, 20, ... 80)
		Mem.onTried = Mem.onTried or {};
		local tried = Mem.onTried[u.id];
		-- (a fuel mine while generators stand idle for fuel comes first and at once, whatever was
		-- tried before: with exactly 10 energy to spare one sat off for 100 rounds beside 7 idle generators)
		local fuelFirst = (Sense.offByKind.generator or 0) > 0 and u.type == Types.mine and IsFuelMine(u);
		-- (tried again after 4 rounds at the most, so that the fuel mines take turns: with no wait
		-- at all the same mine was switched on every round and went off again every round)
		if fuelFirst then
			shed = nil;
			if tried and tried.wait > 4 then tried = {round = tried.round, wait = 4}; end;
		end;
		if tried and Sense.round < tried.round + tried.wait then
			-- wait
		elseif have >= need + ((shed and Sense.round - shed < 50) and 10 or 0) and on < 2 then	-- (the extra margin lapses after 50 rounds)
			if pcall(u.Enable, true) then
				Mem.onTried[u.id] = {round = Sense.round, wait = fuelFirst and 4 or ((tried and Sense.round - tried.round <= 2 * tried.wait) and math.min(80, tried.wait * 2) or 5)};
				on = on + 1;
				if isGen then fuel = fuel - need; else energy = energy - need; end;
				Mem.shed[u.id] = nil;
				Log(string.format('Switched on again at round %d: %s on cell %d (spare energy left %d, fuel %d)', Sense.round, u.type.name, u.location.cell.index, energy, fuel));
			end;
		elseif not isGen and OffRank(u) <= 3 then
			vitalWaits = true;
		end;
	end;
	-- a port or mine waits for energy: a lab gives way (one a round)
	if vitalWaits and on == 0 and Types.lab then
		for _,st in ipairs(Sense.myStructures) do
			if st.type == Types.lab and st.isEnabled ~= false and st.Enable then
				if pcall(st.Enable, false) then
					Mem.shed[st.id] = Sense.round;
					Log(string.format('Laboratory switched off at round %d: a port or mine waits for energy', Sense.round));
				end;
				return;
			end;
		end;
	end;
	-- no lab left to give way, generators idle for fuel and a fuel mine waiting for energy: a
	-- mineral mine gives way to it (the deadlock of a match that stood still for 280 rounds)
	if vitalWaits and on == 0 and (Sense.offByKind.generator or 0) > 0 then
		local fuelWaits = false;
		for _,u in ipairs(list) do if IsFuelMine(u) then fuelWaits = true; break; end; end;
		if fuelWaits then
			for _,st in ipairs(Sense.myStructures) do
				if st.type == Types.mine and st.isEnabled ~= false and st.Enable and not IsFuelMine(st) then
					if pcall(st.Enable, false) then
						Mem.shed[st.id] = Sense.round;
						Log(string.format('Mineral mine switched off at round %d: the fuel mine needs its energy, the generators stand idle for fuel', Sense.round));
					end;
					return;
				end;
			end;
		end;
	end;
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

	-- contested ground next door, or a raiding port owner: the long-range tower comes before everything else
	if Mem.nexusActive or Sense.PortNextDoor() then
		for i,t in ipairs(unlocks) do
			if i > 1 and string.find(string.lower(t.fullName or ''), 'missilelauncher', 1, true) then
				table.remove(unlocks, i); table.insert(unlocks, 1, t); break;
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

--[[
Air cover by sight, not by reach. An AA tower shoots 9 cells and more, but sees into the sub
orbit only as far as its suborbitalScanningRange in world units, and up there a cell step is
about 2.3 of those: 9 gives 4 cells, less the cell or two a bomber stands off its target. Four
AA towers, all on and "in range", never saw the two bombers that took a base apart from 5 to 8
cells away. So with planes about: what the bombers go for (uranium storage, reactor, plants,
containers, mines) gets an AA tower within that sight, the least covered first.
@return a build request on an exact cell, or nil
]]
Economy.aaTried = {};
function Economy.AAGap()
	if not (Cfg.aaBySight and Types.aaTower and Types.IsUnlocked(Types.aaTower) and Sense.AirMatters()) then return nil; end;
	if Economy.aaGapRound and Sense.round - Economy.aaGapRound < 5 then return nil; end;
	local range = 9;
	local edge, dist = {}, {};
	local aaTurret = Types.aaTower.turretType;
	for _,st in ipairs(Sense.myStructures) do
		if st.type and st.type.isStructure and st.type.turretType == aaTurret then
			if st.suborbitalScanningRange and st.suborbitalScanningRange > 0 then range = math.max(range, st.suborbitalScanningRange); end;
			if dist[st.location.cell.index] == nil then dist[st.location.cell.index] = 0; edge[#edge+1] = st.location.cell; end;
		end;
	end;
	for _,job in pairs(Mem.jobs or {}) do
		if job.kind == Types.aaTower.name and job.cell then
			local c = Economy.Cell(job.cell);
			if c and dist[c.index] == nil then dist[c.index] = 0; edge[#edge+1] = c; end;
		end;
	end;
	local reach = math.max(2, math.floor(range / 2.3) - 1);
	for d = 1, reach do
		local nextEdge = {};
		for _,c in ipairs(edge) do
			for _,n in ipairs(c.neighbors or {}) do
				if dist[n.index] == nil then dist[n.index] = d; nextEdge[#nextEdge+1] = n; end;
			end;
		end;
		edge = nextEdge;
	end;
	local function Worth(st)
		local t = st.type;
		if t == Types.uraniumStorage or t == Types.reactor then return 5; end;
		if t == Types.plant then return 4; end;
		if t == Types.mineralContainer or t == Types.fuelContainer then return 3; end;
		if t == Types.mine then return 2; end;
		return 0;
	end;
	local best, bestW = nil, 0;
	for _,st in ipairs(Sense.myStructures) do
		local w = Worth(st);
		if w > bestW and dist[st.location.cell.index] == nil then best, bestW = st, w; end;
	end;
	if best == nil then return nil; end;
	-- a free cell beside it (or one further out), off the plants' exits
	local function Free(c)
		if Sense.structureCells[c.index] or Economy.claimed[c.index] or Sense.UnderEMP(c) then return false; end;
		if (Economy.aaTried[c.index] or -99) > Sense.round - 60 then return false; end;
		local bl = Mem.blacklist[c.index];
		if bl and bl > Sense.round then return false; end;
		for _,n in ipairs(c.neighbors or {}) do
			if Economy.plantCells[n.index] then return false; end;
		end;
		return true;
	end;
	local site = nil;
	for _,n in ipairs(best.location.cell.neighbors or {}) do
		if Free(n) then site = n; break; end;
	end;
	if site == nil then
		for _,n in ipairs(best.location.cell.neighbors or {}) do
			for _,m in ipairs(n.neighbors or {}) do
				if m.index ~= best.location.cell.index and Free(m) then site = m; break; end;
			end;
			if site then break; end;
		end;
	end;
	if site == nil then return nil; end;
	Economy.aaGapRound = Sense.round;
	Economy.aaTried[site.index] = Sense.round;
	Log(string.format('AA by sight: %s on cell %d has no AA tower within %d cells (sight %.1f). One is ordered on cell %d', best.type.name, best.location.cell.index, reach, range, site.index));
	return {type = Types.aaTower, kind = 'base', cell = site.index};
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
Economy.plantDead = {};	-- plant id -> round since which it has no exit by the terrain
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

--[[
Where a vehicle plant standing at loc could put a finished vehicle, by the engine's own rules
(GetProductionChoices / CanDeployHere): a neighbouring cell at the plant's height, one below or
one above, that is clear, has solid ground right beneath, lies in no EMP zone and holds no
structure or unit. A neighbour whose ground lies two levels lower, or a slope the cell is not
clear of, is no exit, however free it looks on the map.
Returns the number of exits (units ignored: they move) and a line telling why not, per cell.
]]
function Economy.Exits(loc, skipIdx)
	local n, why = 0, {};
	local h = loc.height or 0;
	local function Clear(cell, hh)
		local ok, c = pcall(Game.Grid.IsClear, {cell = cell, height = hh});
		return ok and c and true or false;
	end;
	for _,nb in ipairs(loc.cell.neighbors or {}) do
		local r;
		if nb.index == skipIdx then r = 'to be built on';
		elseif Sense.structureCells and Sense.structureCells[nb.index] then r = 'structure';
		elseif Economy.claimed and Economy.claimed[nb.index] then r = 'claimed';
		elseif Sense.UnderEMP(nb) then r = 'EMP zone';
		else
			local found;
			for j = -1, 1 do
				if Clear(nb, h + j) and not Clear(nb, h + j - 1) and (j >= 0 or Clear(nb, h)) then found = j; break; end;
			end;
			if found ~= nil then
				-- (an exit that leads nowhere is a pocket: the first vehicle out stays on it for good.
				-- It needs a way on, to a cell beyond the plant's ring, within one level)
				local ring = {[loc.cell.index] = true};
				for _,r2 in ipairs(loc.cell.neighbors) do ring[r2.index] = true; end;
				-- (two ways on, so that one structure or parked unit does not close it: builders and a
				-- tank stood on such exits with not a single cell in reach)
				local on = 0;
				for _,far in ipairs(nb.neighbors or {}) do
					if not ring[far.index] and far.index ~= skipIdx and not (Sense.structureCells and Sense.structureCells[far.index]) then
						for j2 = -1, 1 do
							if Clear(far, h + found + j2) and not Clear(far, h + found + j2 - 1) then on = on + 1; break; end;
						end;
					end;
				end;
				if on < 2 then r = 'pocket (no way on)'; end;
				-- (and the ground beyond must be open country, not a yard: three exits that led
				-- only to each other took every unit a plant made, ten of them. From the exit, at
				-- least ten cells must be within walking, a level up or down per step)
				if r == nil then
					local seen, edge, count = {[loc.cell.index] = true, [nb.index] = true}, {{nb, h + found}}, 1;
					if skipIdx then seen[skipIdx] = true; end;
					while #edge > 0 and count < 10 do
						local cur = table.remove(edge);
						for _,far in ipairs(cur[1].neighbors or {}) do
							if not seen[far.index] then
								seen[far.index] = true;
								if not (Sense.structureCells and Sense.structureCells[far.index]) then
									for j2 = -1, 1 do
										if Clear(far, cur[2] + j2) and not Clear(far, cur[2] + j2 - 1) then
											count = count + 1;
											edge[#edge+1] = {far, cur[2] + j2};
											break;
										end;
									end;
								end;
							end;
						end;
					end;
					if count < 10 then r = 'pocket (closed in, '..count..' cells)'; end;
				end;
			end;
			if found == nil then
				-- where is the ground then?
				local at;
				for j = -6, 6 do
					if Clear(nb, h + j) and not Clear(nb, h + j - 1) then at = j; break; end;
				end;
				if at == nil then r = 'no clear cell with ground within 6 levels';
				elseif at < -1 then r = 'ground '..(-at)..' levels lower';
				elseif at > 1 then r = 'ground '..at..' levels higher';
				else r = 'not clear (slope)'; end;
			end;
		end;
		if r == nil then
			n = n + 1;
			-- (a unit on an exit blocks it for the moment: told, not counted against the site)
			local okm, m = pcall(Game.GetMobileAt, {cell = nb, height = h});
			for j = -1, 1 do if not (okm and m) then okm, m = pcall(Game.GetMobileAt, {cell = nb, height = h + j}); end; end;
			if okm and m then why[#why+1] = nb.index..' unit '..tostring(m.type and m.type.name or '?')..(m.isOwnedByMe and '' or ' (foreign)'); end;
		else why[#why+1] = nb.index..' '..r; end;
	end;
	return n, table.concat(why, '; ');
end;

function Economy.SiteOK(loc, forMine)
	local idx = loc.cell.index;
	-- a mine may take a reserved lab cell: income beats a lab slot (the reserve once locked a faction out of its last mine sites)
	-- a wide berth around a foreign port, the home region aside (there is no moving that)
	-- (only while the port's owner may still answer: once the probe rounds pass without it stirring, the ground is free)
	if Cfg.berthCells > 0 and not Economy.portSite and (Mem.nexusActive or Sense.round < Cfg.berthProbe) then
		local b = Sense.Berth(idx);
		if b and b < Cfg.berthCells then
			local rid = Map.cellRegion[idx];
			-- (... and the first mines aside: without income nothing else matters)
			-- (... and a fuel mine while fuel is what everything waits for: one start stood still on
			-- 3 generators with five fuel cells in reach, all refused for the berth)
			local fuelNow = false;
			if forMine and Needs.v and (Needs.v.fuel or 0) >= 1 then
				local okf, lvl = pcall(Game.Grid.GetNaturalResourceAt, 'Fuel', loc.cell);
				fuelNow = okf and lvl ~= nil and lvl > 0;
			end;
			if not fuelNow and not (Cfg.berthHome and rid and rid == Mem.homeRegion) and not (forMine and Economy.Count(Types.mine) < (Cfg.minMines or 3)) then
				Economy.siteWhy = 'too close to a foreign port'; return false;
			end;
		end;
	end;
	-- nothing outside the wall, and nothing on it
	if not Wall.Inside(idx) then Economy.siteWhy = 'outside the wall'; return false; end;
	if Economy.labReserved[idx] and not Economy.allowLabSite and not forMine then Economy.siteWhy = 'lab reserve'; return false; end;
	if Economy.IsPowerCenter(idx) and not Economy.allowPowerCenter then Economy.siteWhy = 'power center'; return false; end;
	-- a resource cell on a planned generator ring still goes to a mine: income beats a ring slot
	if Economy.IsPowerRing(idx) and not Economy.allowPowerRing and not forMine then Economy.siteWhy = 'power ring'; return false; end;
	local bl = Mem.blacklist[idx];
	if bl and bl > Sense.round and not (forMine and Economy.retryBanned and bl <= Sense.round + 500) then Economy.siteWhy = 'blacklisted'; return false; end;
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
	-- what is worth something only on ground that is mine (radar and defences push the edge out)
	if Cfg.useControl and not Economy.fringeOK then
		local ok, why = Control.Allows(loc, forMine);
		if not ok then Economy.siteWhy = why; return false; end;
	end;
	-- never box in a vehicle plant: finished vehicles need somewhere to deploy
	for _,nb in ipairs(loc.cell.neighbors) do
		if Economy.plantCells[nb.index] then
			local pfree = 0;
			for _,pn in ipairs(Economy.plantCells[nb.index].neighbors) do
				if pn.index ~= idx and not Sense.structureCells[pn.index] and not Economy.claimed[pn.index] then pfree = pfree + 1; end;
			end;
			if pfree < Cfg.plantKeepExits then Economy.siteWhy = 'boxes in plant'; return false; end;
			-- (and of those, the ones a vehicle can really leave by: never take the last two)
			local pl = Economy.plantLocs and Economy.plantLocs[nb.index];
			if pl then
				local ex = Economy.Exits(pl);
				local exWithout = Economy.Exits(pl, idx);
				if exWithout < ex and exWithout < Cfg.plantKeepExits then Economy.siteWhy = 'takes a plant exit'; return false; end;
			end;
		end;
	end;
	-- (nor the ways on from its exits, one ring further out: a plant went from 4 exits to none as
	-- the base grew around it, each structure taking a way on from an exit)
	for pidx,pl in pairs(Economy.plantLocs or {}) do
		local two = false;
		for _,nb in ipairs(loc.cell.neighbors) do
			if not two then
				for _,n2 in ipairs(nb.neighbors or {}) do if n2.index == pidx then two = true; break; end; end;
			end;
		end;
		if two then
			local ex = Economy.Exits(pl);
			local exWithout = Economy.Exits(pl, idx);
			if exWithout < ex and exWithout < Cfg.plantKeepExits then Economy.siteWhy = 'takes a way on from a plant exit'; return false; end;
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

--[[
Nexus ports decide the game, and a port without a base goes to whoever completes a connected
structure next to it: a mine will do. Ports stand on the poles of the grid (the twelve cells
with five neighbours), so where they can be is known from the start.
@return cell index -> true for the cells next to a port (or pole) that is not mine
]]
function Economy.NextToPort()
	if Economy.portCells == nil then
		Economy.portCells = {};
		for _,reg in ipairs(Map.regions) do
			for _,cell in ipairs(reg.cells) do
				if cell.neighbors and #cell.neighbors == 5 then Economy.portCells[cell.index] = cell; end;
			end;
		end;
	end;
	local function IsPort(u)
		return u.type and u.type.name and string.find(u.type.name, 'Nexus Port', 1, true) and u.location and u.location.cell;
	end;
	for _,u in pairs(Game.hostileUnits) do
		if IsPort(u) then Economy.portCells[u.location.cell.index] = u.location.cell; end;
	end;
	local mine = {};
	for _,u in ipairs(Sense.myStructures or {}) do
		if IsPort(u) then mine[u.location.cell.index] = true; end;
	end;
	local near = {};
	for idx,cell in pairs(Economy.portCells) do
		if not mine[idx] then
			for _,nb in ipairs(cell.neighbors or {}) do near[nb.index] = true; end;
		end;
	end;
	return near;
end;

function Economy.FindMineCells(resource, maxCount)
	local cache = Economy.mineCache[resource];
	if cache == nil or cache.round ~= Sense.round then
		local list = {};
		local seen = {};
		local maxHops = 2;
		local heldBack = {[0] = 0, [1] = 0, port = 0};
		local portNear = Economy.NextToPort();
		local function Scan(reg)
			if seen[reg.id] then return; end;
			if (Sense.Threat(reg) > 0 and not Cfg.minesBelowThreat) or (Sense.hops[reg.id] or 99) > maxHops then return; end;
			seen[reg.id] = true;
			for _,cell in ipairs(reg.cells) do
				if not Sense.structureCells[cell.index] then
					local ok, lvl = pcall(Game.Grid.GetNaturalResourceAt, resource, cell);
					if ok and lvl and lvl > 0 then
						-- a mine only on ground that is mine (Control)
						local c = Cfg.useControl and Control.At(cell) or 2;
						if c == 2 or Control.Allows(cell, true) then
							list[#list+1] = {cell=cell, level=lvl, hops=Sense.hops[reg.id] or 0};
						else
							heldBack[c] = heldBack[c] + 1;
							if portNear and portNear[cell.index] then heldBack.port = heldBack.port + 1; end;
						end;
					end;
				end;
			end;
		end;
		for id,_ in pairs(Sense.mine) do
			Scan(Map.regions[id]);
			for nb,_ in pairs(Map.regions[id].neighbors) do Scan(Map.regions[nb]); end;
		end;
		-- few sites close by: look one ring further out (expanding beats starving, and
		-- resources are usually abundant: knowing only a site or two is the odd case)
		if #list < 4 then
			maxHops = 3;
			seen = {};
			for id,reg in pairs(Map.regions) do
				if (Sense.hops[id] or 99) <= 3 then Scan(reg); end;
			end;
		end;
		-- sites next to a port first, also from further away
		local near = portNear;
		maxHops = Cfg.portMineHops;
		for idx,_ in pairs(near) do
			local rid = Map.cellRegion[idx];
			if rid and Map.regions[rid] then Scan(Map.regions[rid]); end;
		end;
		Economy.nearPort = near;
		local sites, nearCount, poles = 0, 0, 0;
		for _ in pairs(near) do nearCount = nearCount + 1; end;
		for _ in pairs(Economy.portCells) do poles = poles + 1; end;
		for _,e in ipairs(list) do
			if near[e.cell.index] then
				e.bias = Cfg.portMineBias;
				sites = sites + 1;
			end;
		end;
		-- what became of the search for a port site (every 20 rounds per resource)
		Economy.portLogged = Economy.portLogged or {};
		if Sense.round - (Economy.portLogged[resource] or -99) >= 20 then
			Economy.portLogged[resource] = Sense.round;
			Log(string.format('Port sites for %s at round %d: %d ports or poles known, %d cells next to those not mine, %d of them usable sites (of %d sites), bias %s, last obstacle: %s; held back: %d sites contested, %d not seen, %d of those next to a port',
				resource, Sense.round, poles, nearCount, sites, #list, tostring(Cfg.portMineBias), tostring(Economy.portWhy or 'none'),
				heldBack[1], heldBack[0], heldBack.port));
			Economy.portWhy = nil;
		end;
		-- richer and closer first
		if Cfg.useDirections then
			-- sites where the strategic layer wants us first, none where it wants us out
			for _,e in ipairs(list) do
				local rid = Map.cellRegion[e.cell.index];
				e.bias = (e.bias or 1) * (1 + 2 * math.max(Significance.Directed(rid, 'take'), Significance.Directed(rid, 'hold')))
					/ (1 + 3 * Significance.Directed(rid, 'avoid'));
			end;
		end;
		table.sort(list, function(a,b) return a.level * (a.bias or 1) / (1 + a.hops) > b.level * (b.bias or 1) / (1 + b.hops) end);
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
		local bl = ok and c and Mem.blacklist[c.index];
		if ok and c and not Economy.claimed[c.index] and not (bl and bl > Sense.round) then out[#out+1] = c; end;
	end;
	-- how many usable sites I know (read by Needs: few known sites make prospecting matter)
	Economy.knownSites = Economy.knownSites or {};
	Economy.knownSites[resource] = #out;
	-- nothing left but sites a failed build banned: a ban is a guess, not a fact. Trying
	-- again beats having no mine (EMP zones stay banned: those are facts)
	Economy.retryBanned = false;
	if #out == 0 then
		for _,e in ipairs(cache.list) do
			if #out >= (maxCount or 1) then break; end;
			local bl = Mem.blacklist[e.cell.index];
			if not Economy.claimed[e.cell.index] and not Sense.structureCells[e.cell.index] and not (bl and bl > Sense.round + 500) then
				out[#out+1] = e.cell;
				Economy.retryBanned = true;
			end;
		end;
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

--[[
A port goes to whoever completes a structure next to it, whatever the structure. So a free
builder near a port that is not mine puts the cheapest regular structure on a cell
beside it (never a Connector, see Cfg.portStructures), on that exact cell: no waiting for a resource cell to turn up next to the port.
(Learned from the matches played through bridge.lua: five ports by round 30 this way, where the
port mines alone took one or none.)
]]
Economy.portTried = {};		-- port cell index -> round until which it is left alone
function Economy.TryPort(u)
	local t, cost = nil, nil;
	for _,name in ipairs(Cfg.portStructures) do
		local c = Game.Type.units[name];
		local cc = c and (tonumber(c.constructionCost) or 100);
		if t == nil and c and not Types.deprecated[name] and Types.IsUnlocked(c) and (Economy.Resources().storage.minerals or 0) >= cc then t, cost = c, cc; end;
	end;
	if not Cfg.portConnectors or t == nil or Sense.round < 2 then return false; end;
	Economy.NextToPort();
	local mine = {};
	for _,s in ipairs(Sense.myStructures or {}) do
		if s.type and s.type.name and string.find(s.type.name, 'Nexus Port', 1, true) and s.location then mine[s.location.cell.index] = true; end;
	end;
	-- one builder per port, and not every builder at once
	local busy, under = {}, 0;
	for _,job in pairs(Mem.jobs) do
		if job.port then busy[job.port] = true; under = under + 1; end;
	end;
	-- the economy first: ports taken with nothing behind them were lost again with the match
	-- (2 or 3 ports by round 30 and dead before round 100, three times out of six)
	-- ... except the port at home: that one comes first, with its mines (the order a human plays:
	-- port and mines, wall, AA, and only then expansion)
	local econ = Economy.Count(Types.mine) >= Cfg.portMinMines and Economy.Count(Types.generator) >= Cfg.portMinGenerators;
	local builders = #(Sense.my.constructor or {});
	local rich = (Economy.Resources().storage.minerals or 0) >= Cfg.richMinerals;
	if under >= Cfg.portBuilders * (rich and 2 or 1) or under >= math.max(1, math.floor(builders / (rich and 2 or 3))) then return false; end;
	local here = Map.RegionOfLoc(u.location);
	local army = #(Sense.my.combat or {}) + #(Sense.my.bomb or {});
	local maxHops = (army >= Cfg.portFarArmy) and Cfg.portFarHops or Cfg.portMineHops;
	local best, bestD = nil, nil;
	local guard, guardD = nil, nil;
	-- the nearest claimable port counts as the home port, however far (in duels it lay beyond the home region)
	local nearestPort, nearestHops = nil, nil;
	for idx,cell in pairs(Economy.portCells or {}) do
		local rid = Map.cellRegion[idx];
		local hp = rid and Sense.hops[rid];
		if hp and not Sense.UnderEMP(cell) and (nearestHops == nil or hp < nearestHops) then nearestPort, nearestHops = idx, hp; end;
	end;
	for idx,_ in pairs(mine) do
		local cell = Economy.portCells[idx];
		local rid = Map.cellRegion[idx];
		local reg = rid and Map.regions[rid];
		if cell and reg and not busy[idx] and (Economy.portTried[idx] or 0) <= Sense.round
				and (reg.staticThreat or 0) <= 0 and (reg.mobileThreat or 0) <= (reg.myStrength or 0) then
			local n = 0;
			for _,nb in ipairs(cell.neighbors or {}) do
				if Sense.structureCells[nb.index] or Economy.claimed[nb.index] then n = n + 1; end;
			end;
			if n < Cfg.portGuard then
				local d = Dist2(Map.WorldCenter(reg), u.location.world);
				if guardD == nil or d < guardD then guard, guardD = cell, d; end;
			end;
		end;
	end;
	for idx,cell in pairs(Economy.portCells or {}) do
		local rid = Map.cellRegion[idx];
		if not mine[idx] and not busy[idx] and (Economy.portTried[idx] or 0) <= Sense.round and not Sense.UnderEMP(cell) and Wall.Inside(idx)
				and rid then
			local reg = Map.regions[rid];
			local near = (Sense.hops[rid] or 99) <= (econ and maxHops or 1);
			-- (something of mine beside it already and the port still not mine: more of the same won't help)
			local beside = false;
			for _,nb in ipairs(cell.neighbors or {}) do
				if Sense.structureCells[nb.index] then beside = true; break; end;
			end;
			-- (never towards hostile towers: the first units lost in the matches against the Nexus
			-- were builders walking into a missile tower beside a port)
			-- ... unless the army stands there in strength: that is how a lost port is taken back
			local held = (reg.myStrength or 0) > (reg.staticThreat or 0) + (reg.mobileThreat or 0) and (reg.myStrength or 0) > 0;
			-- A port away from home only where my units already stand in strength: its owner sees the
			-- construction and comes for it, and a lone builder there gives the base away and is lost
			-- (in 19 of 24 matches the one try for the second port was a lone builder around round 25)
			local home = not Cfg.portNeedsArmy or (Sense.hops[rid] or 99) <= 1 or idx == nearestPort;
			if not beside and ((held and econ) or (home and near and (reg.staticThreat or 0) <= 0 and (reg.hostileStructures or 0) <= 0
					and (Sense.Threat(reg) <= 0 or reg.mobileThreat <= reg.myStrength))) then
				local d = Dist2(Map.WorldCenter(reg), u.location.world);
				if bestD == nil or d < bestD then best, bestD = cell, d; end;
			end;
		end;
	end;
	-- a new port first; with none in reach, a second structure beside a port of mine
	local guarding = false;
	if best == nil and econ then best, guarding = guard, true; end;
	if best == nil then return false; end;
	Economy.portTried[best.index] = Sense.round + Cfg.portRetry;
	local why = 'no free cell beside it';
	for _,nb in ipairs(best.neighbors or {}) do
		local dest = Move.GroundAt(u, nb);
		if dest and not Economy.claimed[nb.index] and not Sense.structureCells[nb.index] and Game.GetStructureAt(dest) == nil
				and not Game.Grid.AffectedByEMP(dest) and not Control.hot[nb.index] and not Move.IsDangerous(dest) then
			if Economy.BuildAt(u, dest, t) then
				Economy.claimed[nb.index] = true;
				Mem.jobs[u.id].port = best.index;
				Log(string.format('Port %s ordered: %s on cell %d beside the port on cell %d in region %s at round %d',
					guarding and 'guard' or 'structure', t.name, nb.index, best.index, tostring(Map.cellRegion[best.index]), Sense.round));
				return true;
			end;
			why = Economy.buildWhy or 'no path';
		end;
	end;
	Needs.Note('Port on cell '..best.index..' not claimed: '..why);
	return false;
end;

--[[
The wall. A human player's order against the Nexus: port and mines, wall it off, AA, and only
then expansion. Walls of two levels keep ground units out, and the Nexus does not attack a wall
unless it sees units on or right behind it.
The ring: every cell next to the base area (my home structures, widened by Cfg.wallMargin
cells) that is not part of it. A terraformer standing inside puts two blocks on each ring cell
next to it (BuildBlocks at ground + 1 and + 2). Nothing is built outside the ring from then on.
]]
Wall = {tries = {}};

local function WallCellClosed(u, e)
	if Wall.tries[e.idx] and Wall.tries[e.idx] > 4 then return true; end;	-- does not hold: leave it
	local cell = Economy.Cell(e.idx);
	local loc = cell and Move.GroundAt(u, cell);
	return loc == nil or loc.isSuborbital or loc.height >= e.h + 2;
end;

-- The wall goes where the ground makes it short: the smallest set of cells that separates the
-- base (my home structures and the cells around them) from everything Cfg.wallReach cells out,
-- counting only steps a vehicle can take (at most one level). Cliffs and pits do the rest.
-- On open ground no such set is short, and then there is no wall: towers have to do.
function Wall.Plan(u)
	local seed, n = {}, 0;
	for _,st in ipairs(Sense.myStructures or {}) do
		local cell = st.location and st.location.cell;
		local rid = cell and Map.cellRegion[cell.index];
		if cell and rid and (Sense.hops[rid] or 99) <= 1 and not seed[cell.index] then seed[cell.index] = cell; n = n + 1; end;
	end;
	if n < 4 then return false; end;
	-- the area, ring by ring: depth 0 = structures, up to wallMargin = core, wallReach = the outside
	local depth, cells, edge = {}, {}, {};
	for idx,cell in pairs(seed) do depth[idx] = 0; cells[idx] = cell; edge[#edge+1] = cell; end;
	for d = 1, Cfg.wallReach do
		local nextEdge = {};
		for _,cell in ipairs(edge) do
			for _,nb in ipairs(cell.neighbors or {}) do
				if depth[nb.index] == nil then depth[nb.index] = d; cells[nb.index] = nb; nextEdge[#nextEdge+1] = nb; end;
			end;
		end;
		edge = nextEdge;
	end;
	local h, fixed = {}, {};
	for idx,cell in pairs(cells) do
		local loc = Move.GroundAt(u, cell);
		if loc and not loc.isSuborbital then
			h[idx] = loc.height;
			if Sense.structureCells[idx] or Game.GetStructureAt(loc) ~= nil then fixed[idx] = true; end;
		end;
	end;
	-- flow network: every cell is an entry and an exit node; a wall cell carries one unit
	local INF = 1e9;
	local adj = {};
	local function node(k) adj[k] = adj[k] or {}; return adj[k]; end;
	local function link(a, b, cap)
		local ea = {to = b, cap = cap};
		local eb = {to = a, cap = 0};
		ea.rev, eb.rev = eb, ea;
		local na, nb = node(a), node(b);
		na[#na+1] = ea; nb[#nb+1] = eb;
	end;
	local S, T = 's', 't';
	for idx,cell in pairs(cells) do
		if h[idx] then
			local d = depth[idx];
			local free = d <= Cfg.wallMargin or d >= Cfg.wallReach or fixed[idx];
			link('i'..idx, 'o'..idx, free and INF or 1);
			if d <= Cfg.wallMargin then link(S, 'i'..idx, INF); end;
			if d >= Cfg.wallReach then link('o'..idx, T, INF); end;
			for _,nb in ipairs(cell.neighbors or {}) do
				if h[nb.index] and math.abs(h[nb.index] - h[idx]) <= 1 then link('o'..idx, 'i'..nb.index, INF); end;
			end;
		end;
	end;
	node(S); node(T);
	local flow = 0;
	while flow <= Cfg.wallMaxCells do
		local prev, queue, qi = {[S] = true}, {S}, 1;
		while qi <= #queue and not prev[T] do
			local k = queue[qi]; qi = qi + 1;
			for _,e in ipairs(adj[k]) do
				if e.cap > 0 and not prev[e.to] then prev[e.to] = e; queue[#queue+1] = e.to; end;
			end;
		end;
		if not prev[T] then break; end;
		local k = T;
		while k ~= S do
			local e = prev[k];
			e.cap = e.cap - 1; e.rev.cap = e.rev.cap + 1;
			k = e.rev.to;
		end;
		flow = flow + 1;
	end;
	if flow > Cfg.wallMaxCells then
		Mem.wallSkipped = Sense.round;
		Log(string.format('No wall at round %d: the ground around the %d home structures is open (more than %d cells needed)', Sense.round, n, Cfg.wallMaxCells));
		return false;
	end;
	-- the cut: cells entered from the base side whose exit is not reached any more
	local seen, queue, qi = {[S] = true}, {S}, 1;
	while qi <= #queue do
		local k = queue[qi]; qi = qi + 1;
		for _,e in ipairs(adj[k]) do
			if e.cap > 0 and not seen[e.to] then seen[e.to] = true; queue[#queue+1] = e.to; end;
		end;
	end;
	local inside, ring = {}, {};
	for idx,_ in pairs(cells) do
		if h[idx] then
			if seen['o'..idx] then inside[idx] = true;
			elseif seen['i'..idx] then ring[#ring+1] = {idx = idx, h = h[idx]}; end;
		end;
	end;
	if #ring == 0 then
		Mem.wallSkipped = Sense.round;
		Log(string.format('No wall at round %d: the base is closed off by the ground itself', Sense.round));
		return false;
	end;
	Mem.wall = {inside = inside, ring = ring, since = Sense.round, built = {}};
	Log(string.format('Wall planned at round %d: %d cells close off the %d home structures (reach %d)', Sense.round, #ring, n, Cfg.wallReach));
	return true;
end;

-- Sites are kept inside only while the wall stands shut (nothing could get out to build there)
function Wall.Inside(idx)
	local w = Mem.wall;
	return w == nil or not w.complete or w.lifted or w.inside[idx] == true;
end;

-- The wall has done its time: open a gate in it and stop mending
local function WallGate(u, w)
	if w.gateOpen or not u.RemoveBlocks then return false; end;
	if w.gate == nil then
		w.down = w.down or {};
		for _,e in ipairs(w.ring) do
			if not w.down[e.idx] and ((Wall.tries[e.idx] or 0) >= 1 or w.built and w.built[e.idx]) then w.gate = e; break; end;
		end;
		if w.gate == nil then
			w.gateOpen = true;
			if next(w.down) then Log(string.format('Wall taken down by round %d', Sense.round)); end;
			return false;
		end;
	end;
	local e = w.gate;
	local cell = Economy.Cell(e.idx);
	local loc = cell and Move.GroundAt(u, cell);
	if loc and not loc.isSuborbital and loc.height < e.h + 2 then
		Log(string.format('Wall gate open on cell %d at round %d', e.idx, Sense.round));
		--[[
		The whole wall comes down, cell by cell, not just a gate: the base grows across the ring, and
		blocks two high between the structures left plants, reactor sites and vehicles cut off (in
		matches with the wall the home ground stood 3 to 6 levels up by round 280, without it level).
		]]
		if Cfg.wallTakeDown then
			w.down = w.down or {};
			w.down[e.idx] = true; w.gate = nil; w.gateTries = 0;
			return false;
		end;
		w.gateOpen = true;
		return false;
	end;
	for _,job in pairs(Mem.jobs) do if job.kind == 'Wall gate' then return false; end; end;
	local here = u.location.cell;
	if not w.inside[here.index] then return false; end;
	local stand, sd = nil, nil;
	for _,nb in ipairs(cell and cell.neighbors or {}) do
		if nb.index == here.index then
			local ok, tc = pcall(Game.Terrain.TranslateUnitCell, cell);
			if not (ok and tc) then return false; end;
			local okr, err = pcall(u.RemoveBlocks, {Game.Terrain.Pack(tc, e.h + 1), Game.Terrain.Pack(tc, e.h + 2)});
			if not okr then Log('Wall gate: RemoveBlocks failed: '..tostring(err)); w.gateOpen = true; return false; end;
			Mem.jobs[u.id] = {kind = 'Wall gate', since = Sense.round};
			w.gateTries = (w.gateTries or 0) + 1;
			if w.gateTries > 4 then w.gate = nil; w.gateTries = 0; Wall.tries[e.idx] = 0; if w.built then w.built[e.idx] = nil; end; end;
			Log(string.format('Wall gate: %s opens cell %d (round %d)', u.niceName, e.idx, Sense.round));
			return true;
		elseif w.inside[nb.index] and not Sense.structureCells[nb.index] then
			local l = Move.GroundAt(u, nb);
			if l and not l.isSuborbital then
				local d = Dist2(l.world, u.location.world);
				if sd == nil or d < sd then stand, sd = l, d; end;
			end;
		end;
	end;
	if stand then
		local ok, path = pcall(u.FindShortestPath, stand, nil, false);
		if ok and path and #path > 0 then
			u.DirectTo(path[#path]);
			Mem.jobs[u.id] = {kind = 'Wall gate', since = Sense.round};
			return true;
		end;
	end;
	return false;
end;

-- One wall order for this builder. True if it got one (a move to the wall counts)
function Wall.Try(u)
	if not Cfg.walls or not u.BuildBlocks or not (Game.Terrain and Game.Terrain.TranslateUnitCell and Game.Terrain.Pack) then return false; end;
	if Mem.wall == nil then
		if Mem.wallSkipped then return false; end;
		local port = false;
		for _,st in ipairs(Sense.myStructures or {}) do
			if st.type and st.type.name and string.find(st.type.name, 'Nexus Port', 1, true) then port = true; break; end;
		end;
		-- (no economy gate: a poor start with one mine needs the wall most)
		if not (port or Sense.round >= Cfg.wallFromRound) then return false; end;
		if not Wall.Plan(u) then return false; end;
	end;
	local w = Mem.wall;
	if not w.lifted then
		if w.complete and Sense.round >= (w.firstComplete or w.complete) + Cfg.wallHold then
			w.lifted = Sense.round;
			Log(string.format('Wall lifted at round %d after holding since round %d: opening a gate', Sense.round, w.firstComplete or w.complete));
		elseif not w.complete and not w.firstComplete and Sense.round >= w.since + Cfg.wallGiveUp then
			w.lifted = Sense.round; w.gateOpen = not Cfg.wallTakeDown;	-- (what was built of it comes down again)
			Log(string.format('Wall given up at round %d: not closed since round %d', Sense.round, w.since));
		end;
	end;
	if w.lifted then return WallGate(u, w); end;
	-- (a finished wall is looked over every few rounds: shells blast holes into it, and a wall holds
	-- only as long as someone mends it)
	if w.complete and Sense.round % 3 ~= 0 then return false; end;
	local busy = 0;
	for _,job in pairs(Mem.jobs) do if job.kind == 'Wall' then busy = busy + 1; end; end;
	-- (never more than half the builders: in duels the wall cost half the economy)
	if busy >= Cfg.wallBuilders or busy >= math.max(1, math.floor(#(Sense.my.constructor or {}) / 2)) then return false; end;
	local here = u.location.cell;
	if not w.inside[here.index] then return false; end;	-- (builders outside come home through the build sites, all inside now)
	-- the ring cells next to where it stands: block them
	local open, blocks, count = 0, {}, 0;
	local nextTo = {};
	for _,nb in ipairs(here.neighbors or {}) do nextTo[nb.index] = nb; end;
	local best, bestD = nil, nil;
	for _,e in ipairs(w.ring) do
		if not WallCellClosed(u, e) then
			open = open + 1;
			if nextTo[e.idx] then
				local ok, tc = pcall(Game.Terrain.TranslateUnitCell, nextTo[e.idx]);
				if ok and tc then
					blocks[#blocks+1] = Game.Terrain.Pack(tc, e.h + 1);
					blocks[#blocks+1] = Game.Terrain.Pack(tc, e.h + 2);
					Wall.tries[e.idx] = (Wall.tries[e.idx] or 0) + 1;
					if w.built then w.built[e.idx] = true; end;
					count = count + 1;
				end;
			else
				-- a cell inside to stand on, beside this ring cell
				local cell = Economy.Cell(e.idx);
				local any = false;
				for _,nb in ipairs(cell and cell.neighbors or {}) do
					if w.inside[nb.index] and not Sense.structureCells[nb.index] and not Economy.claimed[nb.index] then
						local loc = Move.GroundAt(u, nb);
						if loc and not loc.isSuborbital and not Move.IsDangerous(loc) then
							any = true;
							local d = Dist2(loc.world, u.location.world);
							if bestD == nil or d < bestD then best, bestD = loc, d; end;
						end;
					end;
				end;
				-- nowhere inside to stand beside it, time and again: leave that cell
				if not any then Wall.tries[e.idx] = (Wall.tries[e.idx] or 0) + 0.1; end;
			end;
		end;
	end;
	if open == 0 then
		if not w.complete then
			w.complete = Sense.round;
			w.firstComplete = w.firstComplete or Sense.round;
			Wall.tries = {};
			Log(string.format('Wall complete at round %d: %d cells, begun at round %d', Sense.round, #w.ring, w.since));
		end;
		return false;
	end;
	if w.complete then
		Log(string.format('Wall breached at round %d: %d cells open. Mending', Sense.round, open));
		w.breaches = (w.breaches or 0) + 1;
		w.complete = nil;
	end;
	if count > 0 then
		local ok, err = pcall(u.BuildBlocks, blocks);
		if not ok then Log('Wall: BuildBlocks failed: '..tostring(err)); return false; end;
		Mem.jobs[u.id] = {kind = 'Wall', since = Sense.round};
		Log(string.format('Wall: %s blocks %d cells from cell %d, %d of %d still open (round %d)', u.niceName, count, here.index, open, #w.ring, Sense.round));
		return true;
	end;
	if best then
		local ok, path = pcall(u.FindShortestPath, best, nil, false);
		if ok and path and #path > 0 then
			u.DirectTo(path[#path]);
			Mem.jobs[u.id] = {kind = 'Wall', since = Sense.round};
			Log(string.format('Wall: %s goes to cell %d, %d of %d still open (round %d)', u.niceName, best.cell.index, open, #w.ring, Sense.round));
			return true;
		end;
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
	-- (Economy.shielded: sealed in below ground, what roams the surface can't reach it)
	if not Economy.shielded and (Move.IsDangerous(u.location) or (reg and reg.mobileThreat > reg.myStrength)) then
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
		-- a builder that has not moved a cell through several failed orders is stuck where it
		-- stands (typically on the spot of its last construction, unable to step off by itself).
		-- New orders would all be rejected, and each would ban a good site: step aside first
		Economy.stuck = Economy.stuck or {};
		local st = Economy.stuck[u.id];
		if st and st.cell == u.location.cell.index then st.n = st.n + 1;
		else st = {cell = u.location.cell.index, n = 1}; Economy.stuck[u.id] = st; end;
		if st.n >= 2 then
			Mem.jobs[u.id] = nil;
			local okr, reach = pcall(u.GetReach, nil, math.max(2, u.movementSpeed or 1), true, false, false, true);
			local best, bd = nil, nil;
			for _,n in ipairs(okr and reach or {}) do
				local l = n.location;
				if l and not l.isSuborbital and l.cell.index ~= st.cell and not Sense.structureCells[l.cell.index]
					and not Move.IsDangerous(l) then
					local d = n.movementCostsFromRoot or 1;
					if bd == nil or d < bd then best, bd = n, d; end;
				end;
			end;
			if best then
				u.DirectTo(best);
				if st.n == 2 or Sense.round % 10 == 0 then Log(u.niceName..': stuck at cell '..st.cell..'. Stepping aside to cell '..best.location.cell.index); end;
			else
				-- boxed in for now: no orders for a while (each would fail and ban a site)
				st.wait = Sense.round + 10;
				if st.n == 2 or Sense.round % 10 == 0 then Log(u.niceName..': stuck at cell '..st.cell..' with no way out'); end;
			end;
			return;
		end;
		-- stopped on the way (the engine drops the whole order when something blocks the path,
		-- often a unit that just came into view): the site is as good as before. Order it again
		-- (whatever is left of the order goes first: a construction the game had accepted from
		-- afar while rejecting the way to it stayed with two builders for 90 rounds, "Unable to
		-- automatically relocate", and every later job queued up behind it)
		if u.ClearTasks then pcall(u.ClearTasks); end;
		local t = Game.Type.units[job.kind];
		local cell = Economy.Cell(job.cell);
		local loc = cell and Move.GroundAt(u, cell);
		if t and loc and u.location.cell.index ~= job.cell and (job.tries or 0) < 2
			and not Move.IsDangerous(loc) and Game.GetStructureAt(loc) == nil and Economy.BuildAt(u, loc, t) then
			Mem.jobs[u.id].tries = (job.tries or 0) + 1;
			Economy.claimed[job.cell] = true;
			Log(u.niceName..': '..job.kind..' at cell '..job.cell..' was interrupted on the way. Trying again');
			return;
		end;
		Economy.Blacklist(job.cell, 30);
		Log(u.niceName..': construction of '..job.kind..' at cell '..job.cell..' did not happen. Blacklisting the site');
	end;
	Mem.jobs[u.id] = nil;
	do
		local st = Economy.stuck and Economy.stuck[u.id];
		if st and st.wait and st.wait > Sense.round and st.cell == u.location.cell.index then return; end;
	end;

	if not Economy.portOrdered and Economy.TryPort(u) then
		Economy.portOrdered = true;
		return;
	end;

	if Wall.Try(u) then return; end;

	if not Economy.roadOrdered and Economy.TryRoad(u) then
		Economy.roadOrdered = true;
		return;
	end;

	Economy.freeBuilders = (Economy.freeBuilders or 0) + 1;
	local want = Economy.NextStructure(index);
	if want == nil or want.type == nil then
		return;
	end;

	-- a site chosen from outside (bridge.lua): there or not at all, and the reason is told
	if want.cell then
		local cell = Economy.Cell(want.cell);
		local dest = cell and Move.GroundAt(u, cell);
		local why = nil;
		if dest == nil then why = 'no ground there a builder can stand on';
		elseif Game.Grid.AffectedByEMP(dest) then why = 'the cell lies in an EMP zone';
		elseif Game.GetStructureAt(dest) ~= nil then why = 'a structure stands there already';
		else
			-- (a unit of mine on the cell is told to step off, and the order waits for it: it used
			-- to come back as "no path" four times and was then given up)
			local okm, m = pcall(Game.GetMobileAt, dest);
			if okm and m and m.id ~= u.id then
				if not m.isOwnedByMe then why = 'a foreign unit ('..tostring(m.type and m.type.name or '?')..') stands on the cell';
				else
					why = 'a unit of mine ('..tostring(m.type and m.type.name or '?')..' #'..tostring(m.id)..') stands on the cell; it was told to step off, order again next round';
					pcall(function()
						local reach = m.GetReach(nil, math.max(2, m.movementSpeed), false, false, false, true);
						local best, bestC;
						for _,n in ipairs(reach or {}) do
							local l = n.location;
							if l.cell.index ~= cell.index and not l.isSuborbital and Game.GetMobileAt(l) == nil and not Move.IsDangerous(l)
								and (bestC == nil or n.movementCostsFromRoot < bestC) then best, bestC = n, n.movementCostsFromRoot; end;
						end;
						if best then m.DirectTo(best); end;
					end);
				end;
			end;
			-- (never box in a vehicle plant, by order from outside either)
			for _,nb in ipairs(cell.neighbors or {}) do
				local pc = Economy.plantCells[nb.index];
				if pc and why == nil then
					local pfree = 0;
					for _,pn in ipairs(pc.neighbors) do
						if pn.index ~= cell.index and not Sense.structureCells[pn.index] and not Economy.claimed[pn.index] then pfree = pfree + 1; end;
					end;
					if pfree < Cfg.plantFreeCells then why = 'it would box in the vehicle plant on cell '..nb.index..' (finished vehicles need free cells beside it)'; end;
				end;
			end;
		end;
		if why then -- told below
		elseif not Economy.BuildAt(u, dest, want.type) then why = Economy.buildWhy or 'no path there for the builder';
		else Economy.claimed[cell.index] = true; end;
		if want.done then want.done(why, u); end;
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
			-- (the centre of the generator ring cannot be had: said why, and after three tries the
			-- core goes on plain ground instead. One match asked for it 58 times and built none,
			-- with 500 uranium in store and the plants waiting for power)
			Economy.reactorFails = (Economy.reactorFails or 0) + 1;
			Needs.Note(string.format('%s: no Reactor Core in the generator ring (%s), try %d', u.niceName,
				loc and ('build refused: '..tostring(Economy.buildWhy or 'no path')) or ('no site: '..tostring(Economy.siteWhy or 'no ground at the centre')), Economy.reactorFails));
			-- (and only the first core: four builders each took the fallback in three rounds, four
			-- cores stood and burned the uranium to nothing)
			if Economy.reactorFails < 3 or Economy.Count(Types.reactor) >= 1 then return; end;
			Economy.reactorFails = 0;
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
				-- (a site next to a port is worth a longer way)
				local portSite = Economy.nearPort and Economy.nearPort[cell.index];
				local reach = portSite and math.max(3, Cfg.portMineHops) or 3;
				local safe = reg and (Sense.Threat(reg) <= 0 or Cfg.minesBelowThreat) and (Sense.hops[reg.id] or 99) <= reach;
				-- the builder would turn back from there the round it arrives (see the retreat above)
				if safe and not Economy.shielded and reg.mobileThreat > reg.myStrength then safe = false; end;
				if dest == nil then why.noGround = why.noGround + 1;
				elseif not safe then
					if reg and (Sense.hops[reg.id] or 99) > reach then why.far = why.far + 1; else why.unsafe = why.unsafe + 1; end;
					if portSite then Economy.portWhy = (reg and (Sense.hops[reg.id] or 99) > reach) and 'too far' or 'unsafe'; end;
				elseif not Economy.SiteOK(dest, true) then
					why.site = why.site + 1;
					why.reasons = (why.reasons or '')..' '..tostring(Economy.siteWhy);
					-- an EMP zone stays one: drop the cell from the candidates for good, so it
					-- doesn't crowd out usable ones
					if Economy.siteWhy == 'emp' then Mem.blacklist[cell.index] = Sense.round + 1000; end;
					if portSite then Economy.portWhy = 'site rejected: '..tostring(Economy.siteWhy); end;
				elseif Economy.BuildAt(u, dest, want.type) then
					Economy.claimed[cell.index] = true;
					if portSite then
						Log(string.format('Port mine ordered: cell %d in region %d (%d regions out) for %s at round %d',
							cell.index, reg.id, Sense.hops[reg.id] or -1, res, Sense.round));
					end;
					return;
				elseif portSite then
					Economy.portWhy = 'build failed: '..tostring(Economy.buildWhy);
					why.build = why.build + 1;
				elseif Economy.buildWhy == 'unsafe way' then why.unsafe = why.unsafe + 1;
				else why.build = why.build + 1; end;
			end;
			Needs.Note(string.format('No mine site for %s: %d candidates, %d no ground, %d unsafe, %d too far, %d site rejected (%s), %d build failed',
				res, why.cands, why.noGround, why.unsafe, why.far, why.site, why.reasons or '', why.build));
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
	-- defences may stand on contested ground: they are what makes it mine
	Economy.fringeOK = want.kind == 'tower';
	-- (its own cell is no site for a builder that cannot step off it: the order was refused there,
	-- "no cell to step off to", and chosen again, ten times for one uranium storage)
	local stepOff = false;
	for _,n in ipairs(reach) do
		local l = n.location;
		if l and not l.isSuborbital and l.cell.index ~= u.location.cell.index and not Sense.structureCells[l.cell.index] then stepOff = true; break; end;
	end;
	for _,n in ipairs(reach) do
		local loc = n.location;
		local inReg = stepOff or loc.cell.index ~= u.location.cell.index;
		if want.inRegion then
			local lr = Map.RegionOfLoc(loc);
			inReg = inReg and lr ~= nil and lr.id == want.region;
		end;
		if inReg and not loc.isSuborbital and not Economy.claimed[loc.cell.index] and Economy.SiteOK(loc, false)
			-- (a vehicle plant only where its vehicles can leave: level ground beside it)
			and (want.type ~= Types.plant or Economy.Exits(loc) >= Cfg.plantFreeCells) then
			local d = Dist2(loc.world, goal);
			-- towers prefer the edge of the base, everything else the middle
			local score = d;
			if want.kind == 'tower' and not want.region then
				score = math.abs(d - 9.0);
			end;
			score = score + n.movementCostsFromRoot * 0.05;
			-- (a plant goes where it has the most exits, even a few cells further out: with three
			-- exits and a base growing around it, two parked units shut it, match after match)
			if want.type == Types.plant then score = score + (6 - (Economy.Exits(loc))) * 12; end;
			-- on the peak itself: the highest ground of the region
			if want.peak then score = score - (loc.height or 0) * 3; end;
			-- (a store the army comes out of wants all six cells around it free)
			if want.inRegion and want.type == Types.store then score = score + (6 - Economy.Exits(loc)) * 12; end;
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
	Economy.fringeOK = false;
	if best then
		Economy.claimed[best.location.cell.index] = true;
		local built = Economy.BuildAt(u, best.location, want.type, best);
		if want.inRegion and not built then
			Needs.Note(u.niceName..': '..want.type.name..' for region '..want.region..' at cell '..best.location.cell.index..' refused: '..tostring(Economy.buildWhy or 'no path'));
		end;
	elseif want.inRegion then
		-- (it is wanted in that region and nowhere else: the builder goes there, round by round,
		-- by the safe way. "At region 10" used to mean the nearest cell to it in reach, at home)
		local reg = Map.regions[want.region];
		local at = Map.RegionOfLoc(u.location);
		if at and at.id == want.region then
			-- (there already and no site in reach: another builder's turn, this one is barred for 30 rounds)
			Needs.Note(u.niceName..' at cell '..u.location.cell.index..' is in region '..want.region..' and finds no site for '..want.type.name..' in reach');
			Needs.storeBar = Needs.storeBar or {};
			Needs.storeBar[u.id] = Sense.round + 30;
			if Needs.storeBuilder then Needs.storeBuilder[want.region] = nil; end;
		elseif Move.Towards(u, goal, reg.centerCell, true, false) then
			Needs.Started();
			if Sense.round % 5 == 0 then Needs.Note(u.niceName..' is on its way to region '..want.region..' to build '..want.type.name..' there (at cell '..u.location.cell.index..')'); end;
		else
			Needs.Note(u.niceName..' at cell '..u.location.cell.index..' finds no way towards region '..want.region..' to build '..want.type.name..' there ('..tostring(Move.why)..')');
			Needs.storeBar = Needs.storeBar or {};
			Needs.storeBar[u.id] = Sense.round + 30;
			if Needs.storeBuilder then Needs.storeBuilder[want.region] = nil; end;
		end;
	end;
end;

-- Fuel that is really to spare for one more generator: what the balance shows, less 10 for each
-- generator a builder is on its way to (five were ordered in one round, each seeing the same
-- 23 to spare), and nothing while one stands idle
function Economy.FuelForGenerator()
	local fuel = Economy.Resources().balance.fuel or 0;
	if ((Sense.offByKind or {}).generator or 0) > 0 then return 0; end;
	local gname = Types.generator and Types.generator.name;
	for _,job in pairs(Mem.jobs or {}) do
		if job.kind == gname then fuel = fuel - 10; end;
	end;
	return fuel;
end;

function Economy.BuildAt(u, loc, t, reachable)
	Economy.buildWhy = nil;
	if Economy.Capped(t) then Economy.buildWhy = 'capped'; return false; end;
	-- a generator burns 10 fuel a round: one the fuel does not carry stands idle (12 built, 10 idle, in one match).
	-- (The first is always built; with a fuel mine off for energy, Economy.Power trades it in.)
	if t == Types.generator and Economy.Count(t) >= 1 and Economy.FuelForGenerator() < 10 then	-- (only while one already stands idle: the bare fuel number left a quarter of the starts with one generator)
		Economy.buildWhy = 'the fuel does not carry another generator (fuel mine first)'; return false;
	end;
	-- (on the cell it stands on, the builder has to step off to finish: with nowhere to step to,
	-- the game keeps the begun construction for good, "Unable to automatically relocate", 85 rounds
	-- in one match)
	if loc.cell.index == u.location.cell.index then
		local okr, reach = pcall(u.GetReach, nil, math.max(2, u.movementSpeed or 1), true, false, false, true);
		local free = false;
		for _,n in ipairs(okr and reach or {}) do
			local l = n.location;
			if l and not l.isSuborbital and l.cell.index ~= loc.cell.index and not Sense.structureCells[l.cell.index] then free = true; break; end;
		end;
		if not free then
			Economy.buildWhy = 'no cell to step off to';
			Needs.Note(u.niceName..': not building '..t.name..' on its own cell '..loc.cell.index..', it has no cell to step off to');
			return false;
		end;
	end;
	if loc ~= u.location then
		if reachable == nil then
			local ok, path = pcall(u.FindShortestPath, loc, nil, false);
			-- one of my own units in the way is no reason to give up a site: it will move
			local past = false;
			if not ok or path == nil or #path == 0 then
				ok, path = pcall(u.FindShortestPath, loc, nil, true);
				past = true;
			end;
			-- (but not when the unit in the way stands right in front of it: two builders boxed
			-- in beside a plant took orders the game rejected ("Unable to plot path"), and the
			-- rejected construction then blocked every later job of theirs for 30 rounds and more)
			if past and ok and path and #path > 0 then
				if Economy.unitCellsRound ~= Sense.round then
					Economy.unitCellsRound, Economy.unitCells = Sense.round, {};
					for _,list in pairs(Sense.my) do
						for _,v in ipairs(list) do
							if v.location then Economy.unitCells[v.location.cell.index] = true; end;
						end;
					end;
				end;
				for i = 1, math.min(2, #path) do
					local nl = path[i].location;
					if nl and nl.cell.index ~= u.location.cell.index and Economy.unitCells[nl.cell.index] then
						Economy.buildWhy = 'boxed in by my own units';
						Needs.Note(u.niceName..': the way to the '..t.name..' site at cell '..loc.cell.index..' starts over cell '..nl.cell.index..', where a unit of mine stands. Waiting');
						return false;
					end;
				end;
			end;
			if not ok or path == nil or #path == 0 then
				Mem.blacklist[loc.cell.index] = Sense.round + 20;
				Needs.Note(u.niceName..': no path to the '..t.name..' site at cell '..loc.cell.index..'/'..tostring(loc.height)
					..' from '..u.location.cell.index..'/'..tostring(u.location.height)..(ok and '' or (' ('..tostring(path)..')')));
				return false;
			end;
			-- the way there must be one the builder will walk: it turns back from danger (see
			-- AssignConstructor), and an order it abandons a round later is a wasted one
			if not Economy.shielded then
				for _,n in ipairs(path) do
					local nl = n.location;
					local nr = nl and Map.RegionOfLoc(nl);
					if nl and (Move.IsDangerous(nl) or (nr and nr.mobileThreat > nr.myStrength)) then
						Economy.buildWhy = 'unsafe way';
						Needs.Note(u.niceName..': the way to the '..t.name..' site at cell '..loc.cell.index..' is not safe (cell '..nl.cell.index..')');
						return false;
					end;
				end;
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
				-- planes are the air goal's spending
				if ok then Needs.Charge({serves = {planes = 1}, cost = tonumber(t.constructionCost) or 100}); end;
				Log(f.niceName..': '..(ok and 'producing ' or 'cannot produce ')..t.name..(ok and '' or (': '..tostring(err))));
			end;
		end;
	end;
end;

-- What should a vehicle plant produce next?
-- How many vehicles with this turret I have: standing and stored
function Economy.TurretCount(tn)
	local n = 0;
	for _,list in pairs(Sense.my) do
		for _,u in ipairs(list) do if u.type and u.type.turretType and u.type.turretType.name == tn then n = n + 1; end; end;	-- (u.turret is the attacking turret only: nil on builders and surveyors)
	end;
	-- (those parked in vehicle stores too: five surveyors once came out of one at a stroke)
	for _,u in ipairs(Sense.stored or {}) do
		if u.type and u.type.turretType and u.type.turretType.name == tn then n = n + 1; end;
	end;
	return n;
end;

--[[
Is this kind of vehicle at its cap (Cfg.unitCaps, by turret name, set from outside)? What stands
or is stored counts, and what the plants were told to make in the last 25 rounds and has not
come out yet (a queue shows names like 'Hoverformer', not the turret, so three plants each queued
one more than the cap). An order is taken to have come out when the count has grown past what
it was when the order was given: counted for the full 25 rounds, every builder counted twice
and a cap of 5 held the builders at 3.
]]
function Economy.UnitCapped(t)
	if not (t and t.turretType and Cfg.unitCaps) then return false; end;
	local tn = t.turretType.name;
	local cap = Cfg.unitCaps[tn];
	if not cap then return false; end;
	local n = Economy.TurretCount(tn);
	Economy.capOrders = Economy.capOrders or {};
	local keep, base = {}, nil;
	for _,o in ipairs(Economy.capOrders[tn] or {}) do
		if Sense.round - o.round < 25 then
			keep[#keep+1] = o;
			if base == nil or o.had < base then base = o.had; end;
		end;
	end;
	Economy.capOrders[tn] = keep;
	local pending = #keep - math.max(0, n - (base or n));
	return n + math.max(0, pending) >= cap;
end;

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
	-- a mechanic, always: towers and walls hold only as long as someone repairs them
	local mt = Game.Type.turrets['Mechanic'];
	if mt and Cfg.mechanics > 0 and Sense.round >= Cfg.mechanicFromRound and #my.constructor >= 2
			and #(my.mechanic or {}) + (Sense.storedByRole.mechanic or 0) < Cfg.mechanics
			and (Mem.mechanicOrdered or -99) + 25 <= Sense.round then
		local m = Types.Vehicle(mt, hover) or Types.Vehicle(mt, base);
		local cost = m and (tonumber(m.constructionCost) or 100);
		if m and (Economy.Resources().storage.minerals or 0) >= cost then
			Mem.mechanicOrdered = Sense.round;
			Log('Mechanic ordered at round '..Sense.round);
			return m;
		end;
	end;
	-- weigh the current needs against every vehicle option (see needs.lua)
	local t = Needs.NextVehicle({Constructor = Constructor, Tank = Tank});
	-- a cap on a kind of vehicle (Cfg.unitCaps, by turret name, set from outside): the needs leave
	-- capped kinds out of their choice; one that still comes through (a direct rule) becomes a tank
	if t and Economy.UnitCapped(t) then
		local tn = t.turretType.name;
		if Cfg.unitCaps['Cannon'] == nil and tn ~= 'Cannon' and tn ~= 'Artillery' then t = Tank(); else t = nil; end;
	end;
	return t;
end;

-- The mechanic repairs the most damaged thing of mine it can find, structures first
function Economy.AssignMechanic(u)
	if not u.Repair or not Move.IsIdle(u) then return; end;
	local best, bestS = nil, nil;
	for _,t in pairs(Game.myUnits) do
		if t ~= u and t.location and not t.location.isSuborbital and t.storedIn == nil
				and t.health and t.maxHealth and t.maxHealth > 0 and t.health < t.maxHealth then
			local d = math.sqrt(Dist2(t.location.world, u.location.world));
			local s = (1 - t.health / t.maxHealth) * (t.type.isMobile and 1 or 2) - d * 0.5;
			if bestS == nil or s > bestS then best, bestS = t, s; end;
		end;
	end;
	if best == nil then return; end;
	-- The engine takes a repair only from a cell beside the target (at most one level off), and
	-- wants the target's topmost cell as the choice (a tower is two cells high)
	local tc = best.location.cell;
	local beside = false;
	for _,nb in ipairs(u.location.cell.neighbors or {}) do
		if nb == tc or nb.index == tc.index then beside = true; break; end;
	end;
	if beside and math.abs(u.location.height - best.location.height) <= 1 then
		local th = tonumber(best.height) or tonumber(best.type.height) or 1;
		local ok, err = pcall(u.Repair, {cell = tc, height = best.location.height + th - 1});
		if not ok then Log('Repair failed: '..tostring(err));
		else
			Log(string.format('%s repairs %s on cell %d (%s of %s health)', u.niceName, best.niceName or best.type.name, tc.index, tostring(best.health), tostring(best.maxHealth)));
		end;
		return;
	end;
	local dest, bd = nil, nil;
	for _,nb in ipairs(tc.neighbors or {}) do
		if not Sense.structureCells[nb.index] then
			local loc = Move.GroundAt(u, nb);
			if loc and not loc.isSuborbital and math.abs(loc.height - best.location.height) <= 1 and not Move.IsDangerous(loc) then
				local d = Dist2(loc.world, u.location.world);
				if bd == nil or d < bd then dest, bd = loc, d; end;
			end;
		end;
	end;
	if dest then
		local ok, path = pcall(u.FindShortestPath, dest, nil, false);
		if ok and path and #path > 0 then
			u.DirectTo(path[#path]);
			Log(string.format('%s goes to repair %s on cell %d (%s of %s health)', u.niceName, best.niceName or best.type.name, tc.index, tostring(best.health), tostring(best.maxHealth)));
			return;
		end;
	end;
	if Sense.round % 10 == 0 then Log(u.niceName..': no way to '..(best.niceName or best.type.name)..' on cell '..tc.index..' to repair it'); end;
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
			-- (planes about: not into a store while there is hope for open ground. Three tanks went
			-- from the plant straight into a store under bombers and never came out. After five
			-- blocked rounds the store beats a blocked plant)
			if ok and loc and Cfg.storesEmptyUnderAir and Sense.AirMatters() and Game.GetStructureAt(loc) ~= nil
				and Sense.round - (Economy.plantBlocked[p.id] or Sense.round) < 5 then
				ok = false;
			end;
			if ok and loc then
				Log(p.niceName..': deploying '..tostring(p.productionCompleted)..(Game.GetStructureAt(loc) ~= nil and ' into a store (no open ground)' or ''));
				p.DeployProductionAt(loc);
				Economy.plantBlocked[p.id] = nil;
			else
				Economy.plantBlocked[p.id] = Economy.plantBlocked[p.id] or Sense.round;
				local exits, exWhy = Economy.Exits(p.location);
				if Sense.round % 10 == 0 then
					Log(p.niceName..': cannot deploy '..tostring(p.productionCompleted)..', no deployment option ('..exits..' exits by terrain; '..exWhy..')');
				end;
				--[[
				No exit at all by the terrain (a plant in a pit, on a knoll or at the rim of an EMP zone)
				and blocked for 60 rounds: this plant will never deliver. Scrap it while a builder lives,
				so that the next one goes up where vehicles can leave.
				]]
				if Cfg.plantScrap and exits == 0 and Sense.round - (Economy.plantDead[p.id] or Sense.round) >= Cfg.plantScrap
					and #Sense.my.constructor > 0 and p.Destroy then
					local okD, errD = pcall(p.Destroy);
					Log(p.niceName..' on cell '..p.location.cell.index..': no exit by terrain for '..Cfg.plantScrap..' rounds, scrapping it'
						..(okD and '' or (' (refused: '..tostring(errD)..')')));
				end;
				if exits == 0 then Economy.plantDead[p.id] = Economy.plantDead[p.id] or Sense.round; else Economy.plantDead[p.id] = nil; end;
				--[[
				A finished vehicle that cannot leave blocks the plant for good (one match stood still
				for 200 rounds behind a heavy tank with no level ground beside the plant). After 30
				rounds it is given up, so that what is queued behind it, of another base perhaps, gets
				its turn.
				]]
				local head = p.productionQueue and p.productionQueue[1];
				if Sense.round - Economy.plantBlocked[p.id] >= Cfg.plantBlockedRounds and head and head.Abort then
					local okA, err = pcall(head.Abort);
					Log(p.niceName..': '..tostring(p.productionCompleted)..' could not deploy for '..Cfg.plantBlockedRounds..' rounds, giving it up'
						..(okA and '' or (' (refused: '..tostring(err)..')')));
					Economy.plantBlocked[p.id] = Sense.round;	-- (count anew, for the next one or another try)
				end;
			end;
		else
			Economy.plantBlocked[p.id] = nil;
		end;
		-- no builder at all: whatever else is queued eats the minerals the constructor needs
		if (Needs.v and (Needs.v.survivalBuilder or 0) > 0) and p.productionQueue then
			for _,q in ipairs(p.productionQueue) do
				local n = q.name or '';
				local combat = string.find(n, 'Cannon') or string.find(n, 'Artillery') or string.find(n, 'Bomb')
					or string.find(n, 'Missile') or string.find(n, 'Rocket') or string.find(n, 'Anti Aircraft') or string.find(n, 'Radar');
				if q.id ~= nil and combat and not string.find(n, 'Terraformer') then
					Log(p.niceName..': no builder left, aborting '..tostring(q.name));
					pcall(q.Abort);
				end;
			end;
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
					if t.turretType and Cfg.unitCaps and Cfg.unitCaps[t.turretType.name] then
						Economy.capOrders = Economy.capOrders or {};
						local tn = t.turretType.name;
						Economy.capOrders[tn] = Economy.capOrders[tn] or {};
						table.insert(Economy.capOrders[tn], {round = Sense.round, had = Economy.TurretCount(tn)});
					end;
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
	Economy.plantLocs = {};
	for _,p in ipairs(Sense.plants) do
		Economy.plantCells[p.location.cell.index] = p.location.cell;
		Economy.plantLocs[p.location.cell.index] = p.location;
	end;
	-- (a plant on order keeps its exits too: a container was put on the third exit of the first
	-- plant ten rounds after it was ordered, and units on the other two then made it a pocket)
	if Types.plant then
		for uid,job in pairs(Mem.jobs or {}) do
			if job.kind == Types.plant.name and job.cell and not Economy.plantLocs[job.cell] then
				local c = Economy.Cell(job.cell);
				local b = nil;
				for _,v in ipairs(Sense.my.constructor or {}) do if v.id == uid then b = v; break; end; end;
				local l = c and b and Move.GroundAt(b, c);
				if l then Economy.plantLocs[job.cell] = l; end;
			end;
		end;
	end;
	Economy.powerOrders = 0;
	Economy.freeBuilders = 0;
	Economy.roadOrdered = false;
	Economy.portOrdered = false;
	Economy.scannerClaimed = {};
	-- remember cells constructors are already heading to
	for id,job in pairs(Mem.jobs) do
		if job.cell then Economy.claimed[job.cell] = true; end;
	end;
	for i,u in ipairs(Sense.my.constructor) do
		SafeCall('constructor '..u.niceName, Economy.AssignConstructor, u, i);
	end;
	for _,u in ipairs(Sense.my.mechanic or {}) do
		SafeCall('mechanic '..u.niceName, Economy.AssignMechanic, u);
	end;
end;


-------------------------------------------------------------------------------
-- Military
-------------------------------------------------------------------------------

Military = {};

--[[
Never aim at what cannot be damaged (a Nexus port). ProcessAttackTarget in vehicles.lua has
that check too, but the client looks for required scripts in the user's script folder first
(My Games/Hexaverse/shared/AIScripts), and an old copy of vehicles.lua there replaces the
one next to this file. So the check is made here as well, whichever copy is loaded.
]]
do
	local base = ProcessAttackTarget;
	function ProcessAttackTarget(source, candidate, ...)
		if candidate.isIndestructible or (candidate.type and candidate.type.isIndestructible) then return; end;
		return base(source, candidate, ...);
	end;
end;

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
			-- The match is won with ports: a region with a port that is not mine is worth the trip.
			-- (Nothing aimed at ports before: the army went for whatever hostile structures were
			-- near and weak, and no lost port was ever retaken.)
			-- (not a port under an EMP zone: it cannot be claimed, and the army would only take damage there)
			if Significance.ports and Significance.ports[id] and Significance.ports[id] ~= 'mine' and not Sense.UnderEMP(reg.centerCell) then
				value = value + Cfg.portAttackValue;
			end;
			local hops = Sense.hops[id] or 20;
			if Economy.MineShortage() or Economy.Count(Types.mine) < Cfg.minMines then
				value = value + Economy.ResourceCells(reg) * 2;
			end;
			local defense = Military.DefenseAt(reg) / pot;	-- in our units
			local score = value / (1 + defense * 0.5) / (1 + hops * 0.3) / (1 + Sense.Staleness(reg) * 0.02);
			-- the strategic layer's word: what it wants taken comes first
			if Cfg.useDirections then score = score * (1 + 3 * Significance.Directed(id, 'take')); end;
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
	if Cfg.useFormations then
		-- formations do the attacking; whoever is in none defends
		if Mem.posture ~= 'defend' then
			Mem.posture = 'defend';
			Mem.attackTarget = nil;
		end;
		return;
	end;
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
		if Types.Role(u) == (role or 'combat') and store.DeployStored and (filter == nil or filter(u))
			-- (who waits in a store for its formation stays, but comes out to defend the ground it waits on)
			and not (Formation.Parks(u) and (Military.emptying or not Formation.Parks(u).atHome)) then
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

	--[[
	Stores at the staging ground (Cfg.assaultStores): a take formation gathers inside them, out of
	sight and out of the fire, and comes out in one go, six a store a round, when it is ready.
	Standing in the open the army was seen, bombed and drawn into the towers one by one.
	]]
	if Cfg.useFormations and (Cfg.assaultStores or 0) > 0 then
		-- out: every parked member of a formation that is ready
		local used = {};
		local out = 0;
		for _,u in ipairs(Sense.stored) do
			local f = Formation.of[u.id];
			local st = u.storedIn;
			local sr = st and st.location and Map.RegionOfLoc(st.location);
			-- (also out: who is parked where the formation no longer gathers)
			if f and (f.unload or f.stage ~= 'gather' or f.over or (sr and sr.id ~= f.staging and not (f.atHome and Sense.mine[sr.id]))) and st and st.DeployStored then
				local goal = Map.WorldCenter(Map.regions[f.region]);
				local ok, options = pcall(st.GetStoredDeploymentOptions, u);
				local best, bestD = nil, nil;
				for _,loc in ipairs(ok and options or {}) do
					if not used[loc.cell.index] then
						local d = Dist2(loc.world, goal);
						if bestD == nil or d < bestD then best, bestD = loc, d; end;
					end;
				end;
				if best and pcall(st.DeployStored, u, best) then used[best.cell.index] = true; out = out + 1; end;
			end;
		end;
		if out > 0 then Log('Formation members out of the stores: '..out..' in round '..Sense.round); end;
		storedCombat = math.max(0, storedCombat - out);
		-- in: members gathering in the staging region drive into a store that stands there
		for _,v in ipairs(Sense.my.combat) do
			local f = Formation.Parks(v);
			-- (gathering at home nobody parks: a parked tank defends nothing, and home is where the
			-- raids come. The stores are for a staging ground forward, to come out of in one go)
			if f and f.atHome then f = nil; end;
			local reg = f and Map.RegionOfLoc(v.location);
			local quiet = reg and (reg.mobileThreat or 0) <= 0;
			if quiet then
				for nb,_ in pairs(reg.neighbors) do
					if Map.regions[nb].mobileThreat > 0 then quiet = false; break; end;
				end;
			end;
			if f and reg and quiet and (reg.id == f.staging or (f.atHome and Sense.mine[reg.id])) and v.CanStandAt and not Military.NeedsStationary(v) then
				local ok, reach = pcall(v.GetReach, nil, math.max(2, v.movementSpeed * 2), false, true, true, true);
				for _,n in ipairs(ok and reach or {}) do
					local hr = n.hangar and Map.RegionOfLoc(n.location);
					if n.hangar and hr and (hr.id == f.staging or (f.atHome and Sense.mine[hr.id])) and (not n.hangar.CanStore or n.hangar.CanStore(v)) then
						v.ClearSafeTasks();
						v.DirectTo(n);
						Military.stashing[v.id] = true;
						break;
					end;
				end;
				-- (no store in reach: towards the nearest one that may hold it)
				if not Military.stashing[v.id] then
					local bestSt, bestD = nil, nil;
					for _,st in ipairs(Sense.stores) do
						local sr = Map.RegionOfLoc(st.location);
						if sr and (sr.id == f.staging or (f.atHome and Sense.mine[sr.id])) and (not st.CanStore or select(2, pcall(st.CanStore, v)) ~= false) then
							local d = Dist2(v.location.world, st.location.world);
							if bestD == nil or d < bestD then bestSt, bestD = st, d; end;
						end;
					end;
					if bestSt then
						local nbBest, nbD = nil, nil;
						for _,nb in ipairs(bestSt.location.cell.neighbors or {}) do
							if not Sense.structureCells[nb.index] then
								local w = {x = nb.center.x * Map.groundRadius, y = nb.center.y * Map.groundRadius, z = nb.center.z * Map.groundRadius};
								local d = Dist2(v.location.world, w);
								if nbD == nil or d < nbD then nbBest, nbD = nb, d; end;
							end;
						end;
						if nbBest and nbBest.index ~= v.location.cell.index then
							if not Move.IsIdle(v) then v.ClearSafeTasks(); end;
							if Move.Towards(v, bestSt.location.world, nbBest, true, false) then Military.stashing[v.id] = true; end;
						end;
					end;
				end;
			end;
		end;
	end;

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

	--[[
	Planes about: the stores are emptied. Bombers go for them, and eight tanks died parked in
	two stores within three rounds.
	]]
	if storedCombat > 0 and Cfg.storesEmptyUnderAir and Sense.AirMatters() then
		local home = Sense.baseCenter or (Mem.homeRegion and Map.WorldCenter(Map.regions[Mem.homeRegion]));
		for _,st in ipairs(Sense.stores) do
			if budget <= 0 or storedCombat <= 0 then break; end;
			Military.emptying = true;
			local n = Military.DeployFrom(st, math.min(budget, storedCombat), home);
			Military.emptying = false;
			budget = budget - n;
			storedCombat = storedCombat - n;
		end;
	end;

	-- attack: everything stored joins, exiting as close to the target as possible
	if storedCombat > 0 and Mem.posture == 'attack' and Mem.attackTarget then
		local st = Military.NearestStore(Mem.attackTarget);
		if st then
			budget = budget - Military.DeployFrom(st, budget, Map.WorldCenter(Map.regions[Mem.attackTarget]));
		end;
	end;
	-- a formation short of units: fill it from the store nearest to where it gathers
	if storedCombat > 0 and budget > 0 and Cfg.useFormations and Formation.short then
		local st = Military.NearestStore(Formation.short.region);
		if st then
			local n = Military.DeployFrom(st, math.min(budget, Formation.short.count), Map.WorldCenter(Map.regions[Formation.short.region]));
			budget = budget - n;
			storedCombat = storedCombat - n;
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
	-- (not with planes about: a store is one target, and what is parked in it dies with it)
	if Mem.posture == 'defend' and #Sense.my.combat > Cfg.garrison and not (Cfg.storesEmptyUnderAir and Sense.AirMatters()) then
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
		if Cfg.useDirections then
			local d = Significance.Directions('hold')[1];
			if d then return d.region; end;
		end;
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
			local fGoal, fAttack, fPost, fEntry, fRally, fHold = Formation.Orders(v);
			if fGoal then goalId = fGoal; end;
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
			if fGoal == nil and Mem.posture == 'attack' and centroid and Dist2(v.location.world, centroid) > 100 then
				goal = centroid;
			end;
			-- don't crowd the defense region center: spread a bit
			local cell = reg.centerCell;
			if #reg.cells > 1 then
				cell = reg.cells[((i * 7) % #reg.cells) + 1];
				goal = {x=cell.center.x*Map.groundRadius, y=cell.center.y*Map.groundRadius, z=cell.center.z*Map.groundRadius};
			end;
			-- gathering: all at one spot on the far side from the target, by the stores if any stand there
			if fRally then
				cell = fRally;
				goal = {x=cell.center.x*Map.groundRadius, y=cell.center.y*Map.groundRadius, z=cell.center.z*Map.groundRadius};
			end;
			-- a formation going in takes one way: all make for the same cell until they are inside
			if fEntry then
				local at = Map.RegionOfLoc(v.location);
				if fHold or not (at and at.id == goalId) then
					cell = fEntry;
					goal = {x=cell.center.x*Map.groundRadius, y=cell.center.y*Map.groundRadius, z=cell.center.z*Map.groundRadius};
				end;
			end;
			local attacking = Mem.posture == 'attack';
			if fGoal then attacking = fAttack; end;
			if Military.NeedsStationary(v) then
				-- artillery: get onto high ground and stay there. Moving costs it the round's fire
				local post = Military.PostFor(v, fPost or Military.PostRegion(goalId), takenPosts);
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
			local moved = Move.Towards(v, goal, cell, (not attacking) or 'arty', attacking);
			if fRally and Sense.round % 10 == 0 and (not moved or Move.why) then
				Log(string.format('Gathering: %s at cell %d (region %s) towards cell %d: %s%s', v.niceName, v.location.cell.index,
					tostring((Map.RegionOfLoc(v.location) or {}).id), cell.index, moved and 'moves by sight only, ' or 'does not move, ', tostring(Move.why or 'no step nearer')));
			end;
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
		if e.health > 0 and not e.location.isSuborbital and not (e.isIndestructible or e.type.isIndestructible) then
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
					if b.id then Score.demolished[b.id] = true; end;	-- spent on purpose: no loss
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

	--[[
	Surveyors roam on their own: the game's automation sends them prospecting, as players
	do. If one dies, the scouting need has the plant build another. Scripted only: one that
	sweeps my own land when something may be digging in, and those a personality keeps
	(Military.keepScripted: the cave AI's surveyor stays in the cave).
	]]
	local sweeper = false;
	for _,v in ipairs(scouts) do
		local deep = v.turret and v.turret.type.detectsResources;
		local layer = deep and 'deep' or 'ground';
		local auto = deep and Cfg.autoSurveyors and v.SetAutomation and not (Military.keepScripted and Military.keepScripted[v.id]);
		if auto and Sense.DeepMatters() and not sweeper then sweeper = true; auto = false; end;
		if auto then
			if not v.isAutomated then
				Log(v.niceName..': roaming on its own (automated)');
				pcall(v.SetAutomation, true);
			end;
			goto continue;
		elseif deep and v.isAutomated and v.SetAutomation then
			pcall(v.SetAutomation, false);
		end;
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
						local ks = Economy.knownSites or {};
						local few = (ks['Minerals'] or 9) < 3 or (ks['Fuel'] or 9) < 3;
						if (Economy.MineShortage() or noMinerals or few) and (Sense.hops[reg.id] or 99) <= 3 then
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
		::continue::
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
				-- cannot get there: it guards from where it stands
				if post and Move.Stuck(v) and Dist2(v.location.world, post.loc.world) >= 0.01 then
					post = {region = post.region, key = post.key, cell = v.location.cell, loc = v.location};
					Military.posts[v.id] = post;
				end;
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
	-- beside a plant whose finished vehicle cannot get out, everything makes room at once, builders
	-- too (in the batches most matches lost a plant to its own units parked around it, builders mostly)
	local urgent, urgentLong = {}, {};
	for _,p in ipairs(Sense.plants) do
		if Economy.plantBlocked[p.id] then
			for _,nb in ipairs(p.location.cell.neighbors) do
				urgent[nb.index] = true;
				Military.exitOf = Military.exitOf or {};
				Military.exitOf[nb.index] = p;
				-- (blocked for 10 rounds: a builder at work on the exit leaves too. One stood there
				-- for good, told by the engine to "move manually to finish" its generator)
				if Sense.round - Economy.plantBlocked[p.id] >= 3 then urgentLong[nb.index] = true; end;
			end;
		end;
	end;
	-- only units that have been sitting there for a while (not ones just ordered elsewhere)
	local parked = {};
	for role,list in pairs(Sense.my) do
		for _,u in ipairs(list) do
			local idx = u.location and u.location.cell.index;
			if u.type.isMobile and idx and urgent[idx] then
				parked[u.id] = 2;
				-- (a surveyor roaming under the game's automation walks back onto the exit each
				-- time it is moved: from here on it follows the scripts)
				if u.isAutomated and u.SetAutomation then
					pcall(u.SetAutomation, false);
					Military.keepScripted = Military.keepScripted or {};
					Military.keepScripted[u.id] = true;
					Log(u.niceName..': automation off, it stood on the exit of a blocked plant (cell '..idx..')');
				end;
			-- (an idle builder beside a plant makes room like everybody else: three of them, fresh
			-- from the plant and with nothing to build, stood on its three exits and shut it)
			elseif (role ~= 'constructor' or Move.IsIdle(u)) and u.type.isMobile and idx and near[idx] then
				parked[u.id] = (Military.parked[u.id] or 0) + 1;
			end;
		end;
	end;
	Military.parked = parked;
	for role,list in pairs(Sense.my) do
		for _,u in ipairs(list) do
			if (parked[u.id] or 0) >= 2 and (Move.IsIdle(u) or (role ~= 'constructor' and urgent[u.location.cell.index]) or urgentLong[u.location.cell.index]) and u.GetReach then
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
					if best then u.DirectTo(best); return; end;
					-- (nothing in its short reach: look up to four cells out for open ground away from
					-- the plants and ask for a path there. A surveyor and two heavy builders stood on a
					-- blocked plant's exits for 20 rounds while a path to a cell two steps off existed)
					if urgent[u.location.cell.index] then
						local seen, edge = {[u.location.cell.index] = true}, {u.location.cell};
						for d = 1, 4 do
							local nextEdge = {};
							for _,c in ipairs(edge) do
								for _,n in ipairs(c.neighbors or {}) do
									if not seen[n.index] then
										seen[n.index] = true;
										nextEdge[#nextEdge+1] = n;
										if not near[n.index] and not Sense.structureCells[n.index] then
											local loc = Move.GroundAt(u, n);
											if loc and not Move.IsDangerous(loc) and select(2, pcall(Game.GetMobileAt, loc)) == nil then
												local okp, path = pcall(u.FindShortestPath, loc, nil, false);
												if okp and path and #path > 0 then
													if u.ClearSafeTasks then pcall(u.ClearSafeTasks); end;
													u.DirectTo(path[#path]);
													Log(u.niceName..' leaves the exit of a blocked plant (cell '..u.location.cell.index..') for cell '..n.index);
													return;
												end;
											end;
										end;
									end;
								end;
							end;
							edge = nextEdge;
						end;
					end;
					if urgent[u.location.cell.index] and Sense.round % 10 == 0 then
						Log(u.niceName..' stands on an exit of a blocked plant (cell '..u.location.cell.index..') and has nowhere to go ('..#reach..' cells in reach)');
					end;
					--[[
					Truly shut in on the exit of a plant that has been blocked for a while: the unit can
					never leave and the plant never delivers again (a builder with 0 cells in reach held
					a plant shut for 90 rounds, a cannon in a dead end another; the army stayed at 3 with
					minerals at the cap). After ten such rounds the unit is scrapped: one unit for a plant.
					]]
					if urgentLong[u.location.cell.index] and Cfg.scrapTrapped and u.Destroy then
						Military.trapped = Military.trapped or {};
						local t = Military.trapped[u.id];
						if t and t.cell == u.location.cell.index and Sense.round - t.last <= 2 then t.n = t.n + 1; t.last = Sense.round;
						else t = {cell = u.location.cell.index, n = 1, last = Sense.round}; Military.trapped[u.id] = t; end;
						if t.n >= 10 then
							Log(string.format('%s is shut in on the exit of a blocked plant (cell %d, %d cells in reach) for %d rounds: scrapped at round %d to free the plant', u.niceName, t.cell, #reach, t.n, Sense.round));
							Military.trapped[u.id] = nil;
							-- (a plant whose exits shut in whatever it makes is the fault, not the units:
							-- after two scrapped units the plant goes instead)
							local pl = Military.exitOf and Military.exitOf[t.cell];
							Military.scraps = Military.scraps or {};
							local k = pl and pl.id;
							if k then Military.scraps[k] = (Military.scraps[k] or 0) + 1; end;
							if k and Military.scraps[k] > 2 and pl.Destroy then
								Log(string.format('%s on cell %d shuts in what it makes (%d units scrapped on its exits): the plant is scrapped at round %d', pl.niceName, pl.location.cell.index, Military.scraps[k] - 1, Sense.round));
								Economy.Blacklist(pl.location.cell.index, 500);
								pcall(pl.Destroy);
								return;
							end;
							pcall(u.Destroy);
						end;
					end;
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
	SafeCall('Score', Score.Update);
	SafeCall('Control', Control.Update);
	SafeCall('Significance', Significance.Update);
	SafeCall('Needs', Needs.Update);
	SafeCall('NeedsReport', Needs.Report);

	SafeCall('UraniumCare', Economy.UraniumCare);
	SafeCall('Power', Economy.Power);
	SafeCall('Research', Economy.Research);
	SafeCall('Production', Economy.Production);
	SafeCall('PlaneProduction', Economy.PlaneProduction);
	SafeCall('Constructors', Economy.Constructors);
	SafeCall('Formations', Formation.Update);
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

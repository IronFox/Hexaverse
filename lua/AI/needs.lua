--[[
Hexaverse AI: weighted needs
============================

There is no "always do this when that". Every round the AI rates how badly it
needs each thing right now (0 = not at all, 1 = desperately), and every option
(a structure to build, a vehicle to produce) says which needs it serves. The
option with the highest weighted score wins:

	score = sum over needs of  weight[need] * need[need] * serves[need]
	        divided by (1 + cost / Needs.costScale)

The weights are the personality: Cfg.needWeights. The air personality weighs
planes heavily, the underground one staying hidden, and so on.

Saving up: if the best structure is not affordable yet, cheaper options only
win if they score close to it. So a faction that badly wants a plane factory
saves for it, and one under attack still builds its towers.

Needs (all 0..1):
	survival	the faction dies without a mineral mine, a fuel mine and a generator
	energy		spare energy below the headroom, or no output at all
	fuel		fuel balance at or below zero (generators and plane factories starve)
	income		fewer mines than the round calls for, or minerals running dry
	spend		minerals piling up: production capacity is the bottleneck
	defense		hostiles around the base, recent losses
	air			hostile planes around, or air activity known
	planes		no (or too few) plane factories, scaled by ambition
	research	labs missing
	scouting	no surveyor, scanner or radar coverage
	builders	fewer constructors than wanted
	army		combat units for posture and attacks
]]

Needs = {
	v = {},				-- current need levels
	adapt = {},			-- per need: multiplier on its weight, learned from what happens (see Needs.Learn)
	enemyAASeen = -1,	-- last round hostile anti-air was sensed
	deepRisk = 0.1,		-- how likely something drills in from below (see Needs.Update)
	blind = {},			-- my regions out of sight right now (some layer that matters)
	ordered = {},		-- option key -> rounds it was chosen (a tower ordered for a region may stand next door)
	enemyAirShare = 0.15,	-- how likely the enemy comes by air rather than with tanks (learned)
	artyRisk = 0.1,		-- how likely hostile artillery shells our structures (learned)
	splash = 0,			-- largest hostile artillery explosion radius seen
	costScale = 400,	-- cost at which an option's score halves
	minScore = 0.2,		-- below this, an option is not worth its minerals
	saving = nil,		-- {score, cost, name}: the best structure we could not afford yet
	log = {},
};

local function clamp(x) return math.max(0, math.min(1, x)); end;

function Needs.Weights()
	local w = {
		survival = 10, energy = 4, fuel = 4, income = 3, spend = 2, defense = 3,
		air = 3, planes = 1, research = 2.5, scouting = 1.5, prospecting = 3, builders = 3, army = 1.5,
	};
	for k,v in pairs(Cfg.needWeights or {}) do w[k] = v; end;
	if Needs.eyes then w.planes = math.max(w.planes, 3); end;
	for k,m in pairs(Needs.adapt) do if w[k] then w[k] = w[k] * m; end; end;
	return w;
end;

-- Counts of what keeps the faction alive
function Needs.Essentials()
	local mineral, fuel = 0, 0;
	for _,st in ipairs(Sense.myStructures or {}) do
		if st.type == Types.mine then
			local okf, f = pcall(Game.Grid.GetNaturalResourceAt, 'Fuel', st.location.cell);
			local okm, m = pcall(Game.Grid.GetNaturalResourceAt, 'Minerals', st.location.cell);
			if okf and (f or 0) >= 10 then fuel = fuel + 1; end;
			if okm and (m or 0) >= 5 then mineral = mineral + 1; end;
		end;
	end;
	return mineral, fuel, Economy.Count(Types.generator);
end;

-- How badly one essential is needed: none = desperate, one = fragile
local function Fragile(n)
	if n <= 0 then return 1; end;
	if n == 1 then return 0.45; end;
	if n == 2 then return 0.12; end;
	return 0;
end;

--[[
Signs that a weight was wrong, and what they do:
* hit from the air (a bomber blows up our towers): AA weighed too low
* hit underground: scouting (surveyors, scanners) too low
* hit on the ground: defense too low
* the enemy shows no anti-air while we have no planes: planes too low.
  Anti-air seen, or our planes shot down: planes lower again
Multipliers drift back towards 1 when nothing happens, within [0.5, 3].
]]
local function Nudge(k, f)
	local m = (Needs.adapt[k] or 1) * f;
	Needs.adapt[k] = math.max(0.5, math.min(3, m));
end;

function Needs.Learn()
	for k,m in pairs(Needs.adapt) do
		Needs.adapt[k] = 1 + (m - 1) * 0.98;	-- forget slowly
	end;
	local air, deep, ground = 0, 0, 0;
	for _,a in ipairs(Sense.recentAttacks or {}) do
		if a.layer == 'air' then air = air + 1;
		elseif a.layer == 'deep' then deep = deep + 1;
		else ground = ground + 1; end;
	end;
	if air > 0 then Nudge('air', 1.15 ^ math.min(air, 4)); end;
	-- the enemy's mix: planes vs ground forces sensed, and where the hits come from
	local ea, eg = air, ground;
	for _,e in pairs(Game.hostileUnits or {}) do
		if e.health > 0 and e.type and e.type.isMobile and e.turret and e.turret.type and e.turret.type.canAttack then
			if e.location.isSuborbital then ea = ea + 1; else eg = eg + 1; end;
		end;
	end;
	if ea + eg > 0 then
		Needs.enemyAirShare = Needs.enemyAirShare * 0.9 + 0.1 * (ea / (ea + eg));
	end;
	-- artillery: how likely shells land on our base, and how far their blast reaches
	local arty = 0;
	for _,a in ipairs(Sense.recentAttacks or {}) do
		if a.arty then
			arty = arty + 1;
			Needs.splash = math.max(Needs.splash, tonumber(a.splash) or 0);
		end;
	end;
	for _,e in pairs(Game.hostileUnits or {}) do
		local t = e.health > 0 and e.turret and e.turret.type;
		if t and t.requiresStationaryVehicle and t.trajectoryType == 'Artillery' and not e.location.isSuborbital then arty = arty + 0.5; end;
	end;
	local floor = Sense.round >= 60 and 0.2 or 0.1;
	if Military.artyZones and #Military.artyZones > 0 then floor = 0.6; end;
	Needs.artyRisk = math.max(floor, math.min(1, Needs.artyRisk * 0.97 + arty * 0.1));
	if deep > 0 then Nudge('scouting', 1.12 ^ math.min(deep, 4)); end;
	if ground > 0 and (Needs.adapt.defense or 1) < 2 then Nudge('defense', 1.04 ^ math.min(ground, 4)); end;

	local aa = 0;
	for _,e in pairs(Game.hostileUnits or {}) do
		if e.health > 0 and e.turret and e.turret.type and e.turret.type.trajectoryType == 'AAMissile' then aa = aa + 1; end;
	end;
	if aa > 0 then Needs.enemyAASeen = Sense.round; end;
	Needs.enemyAA = aa;
	local enemyKnown = #(Sense.enemyRegions or {}) > 0;
	if aa > 0 then
		Nudge('planes', 0.97 ^ math.min(aa, 5));
	elseif enemyKnown and Sense.round >= 30 and Sense.round - Needs.enemyAASeen > 20 then
		Nudge('planes', 1.03);	-- the sky looks open
	end;
	-- our planes lost
	local planes = #(Sense.my.plane or {});
	if Needs.lastPlanes and planes < Needs.lastPlanes then Nudge('planes', 0.9 ^ (Needs.lastPlanes - planes)); end;
	Needs.lastPlanes = planes;
end;

-- Can we elevate units to T2? (needs plantations, compounds and mechanics: not handled yet)
function Needs.T2Capable() return false; end;

function Needs.Update()
	Needs.log = {};
	Needs.Learn();
	local v = {};
	local cur, max = Economy.Resources();
	local bal = max.balance or {};
	local minerals = cur.storage.minerals or 0;
	local round = Sense.round;
	local pressure = Sense.Pressure();

	local mineral, fuel, gens = Needs.Essentials();
	Needs.mineralMines, Needs.fuelMines = mineral, fuel;
	-- danger to the base makes every single essential more precious
	local danger = 1 + clamp(pressure / 10);
	v.survivalMineral = clamp(Fragile(mineral) * danger);
	v.survivalFuel = clamp(Fragile(fuel) * danger);
	v.survivalGenerator = clamp(Fragile(gens) * danger);
	v.survivalPlant = (Types.plant and Economy.WorkingPlants() == 0) and 0.9 or 0;

	if Economy.PowerCritical() then
		v.energy = 1;
	else
		v.energy = clamp((Cfg.energyHeadroom - (bal.energy or 0)) / math.max(1, Cfg.energyHeadroom) * 0.8);
		if (cur.balance.energy or 0) <= 0 and gens < 2 then v.energy = math.max(v.energy, 0.7); end;
	end;
	v.fuel = (bal.fuel or 0) <= 0 and 0.8 or 0;
	-- what the engine switched off tells what is short (the spare itself never reads negative)
	do
		local off = Sense.offByKind or {};
		if (off.generator or 0) > 0 then
			v.fuel = 1;		-- generators off: no fuel for them
		elseif (off.mine or 0) + (off.port or 0) + (off.other or 0) > 0 then
			v.energy = 1;	-- generators run, others are off: no power for them
			-- (... and no fuel for one more generator: the fuel mine is what is missing. A start
			-- stood at 3 generators and 7 fuel to spare for 40 rounds, labs and two mines off)
			if (bal.fuel or 0) < 10 then v.fuel = 1; end;
		end;
		local n = #(Sense.off or {});
		if n > 0 and (Needs.offTold ~= n or Sense.round % 10 == 0) then
			Needs.offTold = n;
			Log(string.format('Switched off by the balance at round %d: %d generator(s), %d mine(s), %d port(s), %d other: short of %s',
				Sense.round, off.generator, off.mine, off.port, off.other, off.generator > 0 and 'fuel' or 'energy'));
		elseif n == 0 then Needs.offTold = nil; end;
	end;

	local mines = Economy.Count(Types.mine);
	local wantMines = math.min(Cfg.earlyMines + math.floor(round / 10), 12);
	local deficit = clamp((wantMines - mines) / math.max(1, wantMines));
	local dry = clamp((200 - minerals) / 200);
	v.income = clamp(deficit * 0.8 + dry * 0.3);
	if mines < Cfg.minMines then v.income = math.max(v.income, 0.8); end;
	if minerals >= 450 then v.income = v.income * 0.4; end;

	v.spend = (round >= 20) and clamp((minerals - 250) / 250) or 0;

	v.defense = clamp(pressure / (pressure + 4));
	if round >= 4 and pressure <= 0 then v.defense = 0.12; end;	-- a little standing defense; raids start around round 15

	local ap = Sense.AirPressure();
	v.air = clamp(ap / (ap + 3));
	if Sense.AirMatters() then v.air = math.max(v.air, 0.25); end;
	if round >= Cfg.nexusAirPrepRound then v.air = math.max(v.air, 0.15); end;

	v.planes = 0;
	local pf = Types.planeFactory;
	if pf and Types.IsUnlocked(pf) then
		local want = Cfg.planeFactories or 1;
		local have = Economy.Count(pf);
		if have < want and round >= (Cfg.planeFactoryRound or 0) then
			v.planes = have == 0 and 0.8 or 0.4;
		end;
	end;
	-- (an army sent to take a region without a sensor drone to see what holds it: the factory, and
	-- the fuel it runs on, come before more of the same. 500 rounds of a take passed with the
	-- factory "blocked: needs fuelsupply" at the weight of a nicety)
	Needs.eyes = false;
	-- (only once the economy stands: it must not outweigh the first mines and the defence of the home port)
	if pf and Types.IsUnlocked(pf) and Types.drone and round >= 150 and Economy.Count(Types.mine) >= (Cfg.planeFactoryMines or 4) and Military and Military.SpotRegion and Military.SpotRegion() then
		local drones = 0;
		for _,p in ipairs(Sense.my.plane or {}) do
			if p.type == Types.drone then drones = drones + 1; end;
		end;
		if drones == 0 then
			Needs.eyes = true;
			v.planes = 1;
		end;
	end;
	-- a factory that still wants planes: the air goal keeps its share for them
	if #(Sense.planeFactories or {}) > 0 and Economy.NextPlane and Economy.NextPlane(true) ~= nil then
		v.planes = math.max(v.planes, 0.5);
		Needs.wantCost.airpower = {cost = 150, round = round};
	end;

	v.research = 0;
	if Types.lab and round >= 3 then
		local labs = Economy.Count(Types.lab);
		--[[
		No lab: high priority, not before defense, but right after the first factory. More labs
		pay off in clusters: each lab adds a point, and one more for every two enabled lab
		neighbors. So 1, 3, 6, 10 labs: a lab that completes a cluster is worth far more than
		one that starts a new one.
		]]
		local plants = Economy.WorkingPlants and Economy.WorkingPlants() or 1;
		-- cluster sizes are the triangular numbers 1, 3, 6, 10, 15, ...
		local prevCluster, nextCluster, k = 0, 1, 1;
		while nextCluster <= labs do k = k + 1; prevCluster = nextCluster; nextCluster = nextCluster + k; end;
		-- before T2, lab building was THE endgame job: ever greater range. T2 makes larger lab
		-- arrays pointless if you can afford it. This AI can't do T2 yet (plantations, compounds,
		-- mechanics), so late and rich it keeps growing lab clusters
		local maxLabs = Cfg.wantLabsRich or 6;
		if not Needs.T2Capable() and round >= 80 and minerals >= (Cfg.richMinerals or 400) then maxLabs = math.max(maxLabs, Cfg.wantLabsMax or 15); end;
		Needs.labsMidCluster = labs > prevCluster and labs < maxLabs;
		if labs == 0 then v.research = plants > 0 and 1 or 0.4;
		elseif Needs.labsMidCluster then v.research = clamp(0.4 + 0.4 * (labs - prevCluster) / (nextCluster - prevCluster));	-- finish the cluster
		elseif round >= 15 and labs < Cfg.wantLabs then v.research = 0.3;
		elseif minerals >= Cfg.richMinerals and labs < maxLabs then v.research = 0.25; end;
		-- what we want most is still locked: research is the way to it
		local pfw = Types.planeFactory;
		if pfw and not Types.IsUnlocked(pfw) and Needs.Weights().planes >= 3 and round >= 8 then
			v.research = math.max(v.research, labs == 0 and 1 or 0.5);
		end;
		-- without labs there is no reactor core, heavy base or artillery: while such unlocks are
		-- pending, a first lab cluster is worth a lot
		if (Economy.pendingUnlocks or 1) > 0 and labs < 3 and round >= 12 then
			v.research = math.max(v.research, labs == 0 and 1 or 0.6);
		end;
		-- sitting on minerals with no lab: research is the best use
		if labs == 0 and minerals >= 300 and round >= 8 then v.research = math.max(v.research, 0.9); end;
	end;

	local scouts = #Sense.my.surveyor + (Sense.storedByRole.surveyor or 0) + Economy.ScannerTowers();
	v.scouting = scouts == 0 and 0.7 or (Economy.MineShortage() and 0.4 or 0.05);
	-- prospecting: no mineral site known means no new income. Surveyors find them (they see
	-- resources in the ground); one is rarely enough when the known sites run out
	local noMin = Economy.noMineSite['Minerals'] and round - Economy.noMineSite['Minerals'] <= 10;
	v.prospecting = noMin and 0.9 or 0;
	-- knowing only a site or two is fragile too: one ban or one raid and there is none
	do
		local ks = Economy.knownSites or {};
		local fewest = math.min(ks['Minerals'] or 9, ks['Fuel'] or 9);
		if fewest < 3 and round >= 5 then v.prospecting = math.max(v.prospecting, 0.6 - 0.15 * fewest); end;
	end;
	if Sense.DeepMatters() and #Sense.my.surveyor < Cfg.maxSurveyorsDeep then v.scouting = math.max(v.scouting, 0.5); end;
	-- coverage of my own land: a region of mine out of sight could hide anything that slipped through
	local own, blind = 0, 0;
	Needs.blind = {};
	for id,_ in pairs(Sense.mine) do
		own = own + 1;
		local reg = Map.regions[id];
		local gap = not reg.seenNow
			or (Sense.AirMatters() and not reg.seenAirNow)
			or (Sense.DeepMatters() and not reg.seenDeepNow);
		if gap then blind = blind + 1; Needs.blind[id] = true; end;
	end;
	if own > 0 then v.scouting = math.max(v.scouting, clamp(blind / own * 1.2)); end;
	-- risk of something drilling in: certain once deep activity was seen, real whenever another
	-- player (not the Nexus) is around, since any player can dig
	local player = false;
	for _,f in pairs(Game.factions or {}) do
		if not Sense.IsNexus(f) and f ~= Game.myFaction and f.isHostile ~= false then player = true; end;
	end;
	-- can drones cover my land? (a plane factory, drones researched, and enough of them flying)
	local drones = 0;
	for _,p in ipairs(Sense.my.plane or {}) do if p.type == Types.drone then drones = drones + 1; end; end;
	local canDrones = Types.drone and Types.IsUnlocked(Types.drone) and Types.planeFactory and Economy.Count(Types.planeFactory) > 0;
	Needs.dronesShort = not canDrones or drones < math.ceil(own / 4);
	Needs.deepRisk = Sense.DeepMatters() and 1 or ((player and Sense.round >= 30) and 0.35 or 0.1);

	-- builders that can work: parked ones don't (they get deployed, see Military.Stores)
	local constructors = #Sense.my.constructor;
	local storedC = Sense.storedByRole.constructor or 0;
	-- no builder at all is near-fatal: nothing can be built or rebuilt. Any constructor beats
	-- the fifth cannon
	v.survivalBuilder = (constructors == 0 and storedC == 0) and 1 or 0;
	constructors = constructors + storedC;
	local wantC = round < 40 and Cfg.constructorsEarly or Cfg.constructorsLate;
	v.builders = constructors < 2 and 1 or clamp((wantC - constructors) / wantC);

	local combat = #Sense.my.combat + #Sense.my.bomb + (Sense.storedByRole.combat or 0);
	v.army = clamp(0.35 - combat * 0.02);
	--[[
	A garrison: what stands is what a raid finds. No combat unit at all means the first few
	hover tanks take the base apart, however many minerals sit in storage. None is near-fatal
	once raids can come, a few per region I hold is enough.
	]]
	do
		local nReg = 0;
		for _ in pairs(Sense.mine) do nReg = nReg + 1; end;
		local wantG = 2 + nReg;
		v.survivalGarrison = 0;
		if round >= 8 then
			if combat == 0 then v.survivalGarrison = 0.75;
			elseif combat < wantG then v.survivalGarrison = 0.45 * (1 - combat / wantG); end;
		end;
	end;
	if Mem.posture == 'attack' then v.army = math.max(v.army, 0.3); end;
	v.army = math.max(v.army, v.defense * 0.8);
	-- (heavy vehicles of the enemy seen, and only a handful of mine: in match 184 a Heavytank stood
	-- in my home region at round 48, the army stayed at 4 hover cannons until round 105 while five AA
	-- towers and four rocket towers went up, the raid outranged the towers and the faction was dead
	-- at round 115)
	if (Mem.heavySeen or -1) >= 0 and combat < (Cfg.garrisonHeavy or 8) then v.army = math.max(v.army, Cfg.armyHeavySeen or 0.7); end;
	--[[
	An army is also what a take is made of, and what full stores are for. In match 177 all four
	plants stood off with empty queues, minerals at the cap of 1500, army 0.11, with no hostile in
	sight, while the take on the Nexus region waited for members and the army was 9 to 15.
	]]
	if combat < (Cfg.maxArmy or 56) then
		local taking = false;
		for _,f in ipairs(Formation and Formation.list or {}) do
			if f.kind == 'take' and not f.over then taking = true; break; end;
		end;
		if taking then v.army = math.max(v.army, Cfg.armyForTake or 0.6); end;
		if minerals >= (Cfg.armyStock or 800) then v.army = math.max(v.army, Cfg.armyRich or 0.6); end;
	end;

	Needs.v = v;
	--[[
	Spending needs the means to spend: minerals piling up while every constructor was busy and
	every plant queue full last round means the base can't turn income into anything. That
	is a missing capability, not a lack of ideas: more builders and plants
	]]
	if minerals >= 400 and round >= 10 then
		-- (no constructor at all is not a capacity problem: survivalBuilder covers it)
		local noBuilder = (Economy.freeBuilders or 0) == 0 and constructors > 0;
		local plantsFull = Economy.plantsFull ~= false;
		if noBuilder or plantsFull then
			Needs.Block(noBuilder and 'spending (all builders busy)' or 'spending (plants full)', 'capacity', clamp((minerals - 300) / 200));
		end;
	end;
	Needs.ApplyBlocks(v);
	Needs.UpdateBudgets(v, minerals, math.max(0, cur.balance and cur.balance.minerals or 0));
end;

-- weight of a need key (survival* share the survival weight)
local function W(w, k)
	if string.sub(k, 1, 8) == 'survival' then return w.survival; end;
	return w[k] or 1;
end;

--[[
How much of this do I already have? None makes an option far more significant than
having some: the first radar, tower or AA counts most. Units parked in a store count
double: they are probably more than enough.
]]
-- How often an option was chosen in the last 40 rounds
function Needs.RecentOrders(key)
	local list = Needs.ordered[key];
	if list == nil then return 0; end;
	local n, keep = 0, {};
	for _,r in ipairs(list) do
		if Sense.round - r < 40 then n = n + 1; keep[#keep+1] = r; end;
	end;
	Needs.ordered[key] = keep;
	return n;
end;

--[[
One of something in the right spot always beats zero. A second is only worth it if the
spot or the action can use it (cap): tanks hinder each other, towers block access,
everything competes for space. Beyond what a spot can use, value drops steeply, except
for a swarm floor: numbers still give a blanket of meat shields and sensor coverage.
]]
function Needs.Have(n, cap, swarm)
	if n == nil or n <= 0 then return 1; end;
	local v = (1 + n) ^ -0.8;
	if cap and n >= cap then v = v * 0.35 ^ (n - cap + 1); end;
	if swarm then v = math.max(v, swarm); end;
	return v;
end;

--[[
How close together to keep things. An AA tower next to a radar next to a missile tower
covers each other against most threats, but one elevated artillery tank that reaches any
of them hits all three with each shell. So a site next to a few structures gets a small
bonus while artillery is unlikely, and a penalty per structure within blast reach as it
becomes likely. Returns a site score addition (lower is better).
]]
function Needs.Spacing(loc)
	local r = Needs.splash;
	if r <= 0 then
		local t = Types.turret and Types.turret.artillery;
		r = (t and tonumber(t.explosionRadius)) or 0;
	end;
	r = math.max(r, 1.2) * 1.5;	-- a shell rarely lands exactly on target
	local r2 = r * r;
	local n = 0;
	for _,st in ipairs(Sense.myStructures or {}) do
		local w = st.location and st.location.world;
		if w and not st.location.isSuborbital then
			local dx, dy, dz = w.x - loc.world.x, w.y - loc.world.y, w.z - loc.world.z;
			if dx*dx + dy*dy + dz*dz < r2 then n = n + 1; end;
		end;
	end;
	local risk = Needs.artyRisk;
	return n * risk * 12 - math.min(n, 2) * (1 - risk) * 2;
end;

--[[
Exposure: how much higher ground lies within artillery reach of a site. Around a
mountain there is a wide ring where a tower is almost always a bad idea: artillery on
top rains down on it, out of reach of everything below. And an adversary can make high
ground: a terraformer building a ramp, or an artillery tank climbing one, nearby raises
the exposure too. Returns layers of advantage an attacker would have (0 = none).
]]
Needs.groundCache = {};		-- cell index -> {h, round}
Needs.exposureCache = {};	-- cell index .. '@' .. height -> {e, round}

function Needs.GroundHeight(u, cell)
	local c = Needs.groundCache[cell.index];
	if c and Sense.round - c.round < 15 then return c.h; end;
	local ok, g = pcall(Game.Grid.FindHighestVehicleGround, u, cell);
	local h = ok and g and not g.isSuborbital and g.height or nil;
	Needs.groundCache[cell.index] = {h = h, round = Sense.round};
	return h;
end;

function Needs.Exposure(u, loc)
	if loc == nil or loc.isSuborbital or Map.groundRadius == nil then return 0; end;
	local key = loc.cell.index..'@'..loc.height;
	local c = Needs.exposureCache[key];
	if c and Sense.round - c.round < 10 then return c.e; end;
	local t = Types.turret and Types.turret.artillery;
	local reach = (t and tonumber(t.flatAttackRange)) or 8;
	local h0, h1 = Game.Grid.HeightToRadius(0), Game.Grid.HeightToRadius(1);
	local layer = math.max(1e-3, h1 - h0);
	local here = Map.RegionOfLoc(loc);
	local regs = {};
	if here then
		regs[#regs+1] = here;
		for nb,_ in pairs(here.neighbors) do regs[#regs+1] = Map.regions[nb]; end;
	end;
	local R = Map.groundRadius;
	local function dist(a, b)
		local dx, dy, dz = a.x - b.x, a.y - b.y, a.z - b.z;
		return math.sqrt(dx*dx + dy*dy + dz*dz) * R;
	end;
	local e = 0;
	for _,reg in ipairs(regs) do
		for _,cell in ipairs(reg.cells) do
			local d = dist(cell.center, loc.cell.center);
			-- artillery reaches further downhill: every layer of height adds about a layer of range
			if d <= reach + 8 * layer then
				local h = Needs.GroundHeight(u, cell);
				local rise = h and (h - loc.height) or 0;
				if rise >= 2 and d <= reach + rise * layer then
					e = math.max(e, rise - 1);
				end;
			end;
		end;
	end;
	-- made high ground: hostile terraformers or artillery near (they may be building or climbing a ramp)
	for _,h in pairs(Game.hostileUnits or {}) do
		local tt = h.health > 0 and h.turret and h.turret.type;
		if tt and not h.location.isSuborbital then
			local d = dist(h.location.cell.center, loc.cell.center);
			if tt.canRemoveTerrain and d <= reach * 1.5 then e = e + 1; end;
			if tt.requiresStationaryVehicle and tt.trajectoryType == 'Artillery' and d <= reach * 1.5
				and h.location.height > loc.height then
				e = e + 1 + (h.location.height - loc.height);
			end;
		end;
	end;
	Needs.exposureCache[key] = {e = e, round = Sense.round};
	return e;
end;

--[[
Strategic value of a region. A valley without natural resources is worthless. A peak can
decide the match: artillery up there commands everything below. A valley with lots of
resources is complicated: whoever holds the heights around it owns it, so either take
those heights or put the mines underground where nobody sees them.
  elevation: highest and average ground of a few sample cells (cached)
  overlook:  how far a region looks down on its lowest neighbor (a peak)
  exposed:   how far the highest unheld neighbor looks down on it
]]
Needs.elevCache = {};

local function RefUnit()
	for _,role in ipairs({'combat', 'constructor', 'aa', 'radar', 'surveyor'}) do
		for _,u in ipairs(Sense.my[role] or {}) do
			if u.health > 0 and u.location and not u.location.isSuborbital then return u; end;
		end;
	end;
	return nil;
end;

function Needs.Elevation(reg)
	local c = Needs.elevCache[reg.id];
	if c and Sense.round - c.round < 30 then return c.max, c.mean; end;
	local u = RefUnit();
	if u == nil then return nil, nil; end;
	local step = math.max(1, math.floor(#reg.cells / 8));
	local mx, sum, n = nil, 0, 0;
	for k = 1, #reg.cells, step do
		local h = Needs.GroundHeight(u, reg.cells[k]);
		if h then
			mx = math.max(mx or h, h);
			sum, n = sum + h, n + 1;
		end;
	end;
	if n == 0 then return nil, nil; end;
	Needs.elevCache[reg.id] = {max = mx, mean = sum / n, round = Sense.round};
	return mx, sum / n;
end;

function Needs.Overlook(reg)
	local mx = Needs.Elevation(reg);
	if mx == nil then return 0; end;
	local low = nil;
	for nb,_ in pairs(reg.neighbors) do
		local _, mean = Needs.Elevation(Map.regions[nb]);
		if mean then low = math.min(low or mean, mean); end;
	end;
	return low and math.max(0, mx - low) or 0;
end;

-- How far unheld high ground next door looks down on this region
function Needs.Exposed(reg)
	local _, mean = Needs.Elevation(reg);
	if mean == nil then return 0; end;
	local e = 0;
	for nb,_ in pairs(reg.neighbors) do
		if not Sense.mine[nb] then
			local mx = Needs.Elevation(Map.regions[nb]);
			if mx then e = math.max(e, mx - mean); end;
		end;
	end;
	return e;
end;

function Needs.StrategicValue(reg)
	local res = Economy.ResourceCells(reg);
	local v = res;
	local over = Needs.Overlook(reg);
	if over >= 2 then
		v = v + over;
		-- a peak is worth what it commands: the resources in the valleys below it
		for nb,_ in pairs(reg.neighbors) do
			local nr = Map.regions[nb];
			local _, mean = Needs.Elevation(nr);
			local mx = Needs.Elevation(reg);
			if mean and mx and mx - mean >= 2 then
				v = v + Economy.ResourceCells(nr) * math.min(1, (mx - mean) / 4) * 1.5;
			end;
		end;
	end;
	return v;
end;

--[[
Overwatch: the unheld high ground that most endangers our resources. The peak next to a
resource region of ours, scored by what it commands of ours. Nil if nothing qualifies.
]]
function Needs.Overwatch()
	local best, bestV = nil, 0;
	for id,_ in pairs(Sense.mine) do
		local reg = Map.regions[id];
		local res = Economy.ResourceCells(reg) + (reg.myStructures or 0);
		local _, mean = Needs.Elevation(reg);
		if mean and res > 0 then
			for nb,_ in pairs(reg.neighbors) do
				local nr = Map.regions[nb];
				local mx = Needs.Elevation(nr);
				local stored = false;
				for _,st in ipairs(Sense.stores or {}) do
					if Map.RegionOfLoc(st.location) == nr then stored = true; break; end;
				end;
				if mx and mx - mean >= 2 and nr.hostileStructures == 0 and nr.staticThreat <= 0 and not stored then
					-- a peak deep inside our own territory is probably (not certainly) less
					-- urgent: an enemy has to get through us first. Count how much of it we surround
					local inner, total = Sense.mine[nb] and 1 or 0, 1;
					for nn,_ in pairs(nr.neighbors) do
						total = total + 1;
						if Sense.mine[nn] then inner = inner + 1; end;
					end;
					local v = res * math.min(1, (mx - mean) / 4) * (1 - 0.7 * inner / total);
					if v > bestV then best, bestV = nb, v; end;
				end;
			end;
		end;
	end;
	return best, bestV;
end;

-- The peak over our resources most worth a store on top, inside or next to our territory,
-- wherever there is none yet (interior peaks count fully here: a store is cheap insurance)
function Needs.StorePeak(hasStore)
	local best, bestV = nil, 1;
	for id,_ in pairs(Sense.mine) do
		local reg = Map.regions[id];
		local res = Economy.ResourceCells(reg) + (reg.myStructures or 0);
		local _, mean = Needs.Elevation(reg);
		if mean and res > 0 then
			local cands = {[id] = true};
			for nb,_ in pairs(reg.neighbors) do cands[nb] = true; end;
			for c,_ in pairs(cands) do
				local cr = Map.regions[c];
				local mx = Needs.Elevation(cr);
				if mx and mx - mean >= 2 and not hasStore[c] and cr.hostileStructures == 0 and cr.staticThreat <= 0 then
					local v = res * math.min(1, (mx - mean) / 4);
					if v > bestV then best, bestV = c, v; end;
				end;
			end;
		end;
	end;
	return best;
end;

-- Room left in a region: 1 while half its cells are free, falling to 0 when it is full.
-- Crowded spots block access and plant deployment, so another structure there is worth less
function Needs.Room(reg)
	local n = #(reg.cells or {});
	if n == 0 then return 1; end;
	local used = (reg.myStructures or 0) + (reg.myMobiles or 0) * 0.5;
	return clamp((n - used) / (n * 0.5));
end;

function Needs.Score(serves, cost)
	local w = Needs.Weights();
	local s = 0;
	for k,amount in pairs(serves) do
		s = s + W(w, k) * (Needs.v[k] or 0) * amount;
	end;
	return s / (1 + (cost or 0) / Needs.costScale);
end;

local function Cost(t)
	return tonumber(t and t.constructionCost) or 100;
end;

-- Rate options and pick one. Options: {want=..., serves={...}, cost=n, name=...}.
-- Returns the chosen option or nil (nothing worth it, or saving up)
--[[
Learn from orders that come to nothing: an option chosen but never started (no site, no
path, vetoed further down) is chosen again next round, forever. Each such failure halves
that option's score for a while, so the next best thing gets built instead.
]]
Needs.failed = {};		-- option name -> {n, round}

function Needs.FailFactor(name)
	local f = Needs.failed[name];
	if f == nil then return 1; end;
	local n = f.n - math.floor((Sense.round - f.round) / 5);	-- forget one failure every 5 rounds
	if n <= 0 then Needs.failed[name] = nil; return 1; end;
	return 0.5 ^ n;
end;

function Needs.Failed(name)
	local f = Needs.failed[name] or {n = 0, round = Sense.round};
	f.n = math.min(6, f.n + 1);
	f.round = Sense.round;
	Needs.failed[name] = f;
	-- (for a mine the reason is that of the site search, not of what was tried in its place:
	-- "Mine (fuel) ... last reason: capped" 283 times in match 153 was the tower built instead)
	local why = Economy.buildWhy or Economy.siteWhy;
	local w = Economy.mineWhy;
	if string.sub(name, 1, 4) == 'Mine' and w then
		why = string.format('%d candidates, %d no ground, %d unsafe, %d too far, %d site rejected (%s), %d build failed (%s)',
			w.cands or 0, w.noGround or 0, w.unsafe or 0, w.far or 0, w.site or 0, w.reasons or '', w.build or 0, w.bwhy or '');
	end;
	Needs.Note('order for '..name..' came to nothing ('..f.n..'x): weighing it down'..(why and (' (last reason: '..tostring(why)..')') or ''));
end;

-- A structure order was placed: the pending choice worked out
function Needs.Started()
	Needs.pending = nil;
	if Needs.floorPending then Needs.floorFails[Needs.floorPending] = 0; Needs.floorPending = nil; end;
end;

--[[
Requirements. An option needs more than minerals: a known site, a free builder, income to
save from, the means to spend what comes in. When one of those is missing, the option is
blocked, and the missing capability becomes a need of its own, as urgent as what it blocks
(a mine the economy badly wants makes finding a site just as urgent). It fades over 10
rounds unless the block is seen again.
	cause -> the needs that resolve it
]]
Needs.resolve = {
	site = {prospecting = 1, scouting = 0.6},	-- no known site: survey for one
	danger = {defense = 1, army = 0.6},		-- the sites are under threat: clear or guard them
	builder = {builders = 1},				-- nobody free to build it
	capacity = {builders = 1, spend = 1},	-- income piles up: more builders and plants to spend it
	income = {income = 1},					-- nothing to save from: income first
	energy = {energy = 1, fuel = 0.8},		-- power or fuel lost under what we were building
	fuelsupply = {fuel = 1, prospecting = 0.5},	-- plane factories without fuel to run on
};
Needs.blocks = {};	-- cause -> {value, round, what}

function Needs.BlockValue(cause)
	local b = Needs.blocks[cause];
	if b == nil then return 0; end;
	local age = Sense.round - b.round;
	if age > 10 then Needs.blocks[cause] = nil; return 0; end;
	return b.value * (1 - age / 11);
end;

-- How urgent an option is: the strongest need it serves
function Needs.Urgency(o)
	if o == nil then return 0; end;
	local u = 0;
	for k,amount in pairs(o.serves or {}) do
		u = math.max(u, (Needs.v[k] or 0) * math.min(1, amount));
	end;
	return u;
end;

function Needs.Block(what, cause, value)
	value = clamp(value or 0.5);
	local cur = Needs.BlockValue(cause);
	Needs.blocks[cause] = {value = math.max(cur, value), round = Sense.round, what = what};
	local keys = {};
	for k,_ in pairs(Needs.resolve[cause] or {}) do keys[#keys+1] = k; end;
	table.sort(keys);
	Needs.Note(string.format('%s blocked: needs %s (%.2f), raising %s', what, cause, value, table.concat(keys, ', ')));
end;

-- Blocked requirements pass their urgency on to the needs that resolve them
function Needs.ApplyBlocks(v)
	for cause,map in pairs(Needs.resolve) do
		local b = Needs.BlockValue(cause);
		if b > 0 then
			for k,amount in pairs(map) do v[k] = math.max(v[k] or 0, clamp(b * amount)); end;
		end;
	end;
end;

--[[
React fast. A running construction whose foundation just broke (the only generator or
reactor core went boom, the last fuel mine is gone) is not worth finishing now: a lab or
plane factory without power or fuel is dead weight. Returns why, or nil if it still stands.
Power, mines and plants themselves are what restores the foundation: never dropped.
]]
function Needs.Unsupported(t)
	if t == nil or t == Types.generator or t == Types.reactor or t == Types.mine or t == Types.plant then return nil; end;
	local demand = t.balance and t.balance.demand or {};
	if not Economy.PowerCritical() then Needs.hadPower = true; end;
	if (demand.energy or 0) > 0 and Needs.hadPower and Economy.PowerCritical() then return 'power lost'; end;
	if (demand.fuel or 0) > 0 and Economy.FuelMines() == 0 and (Game.myFaction.currentResources.storage.fuel or 0) <= 0 then
		return 'fuel lost';
	end;
	return nil;
end;

-------------------------------------------------------------------------------
-- Goals and budgets
-------------------------------------------------------------------------------

--[[
Priorities and resource allocation: task -> resources allocated -> implementation. Each need
belongs to a goal. A goal's value is its strongest weighted need, and every round's mineral
income is split across the goals by value. An option can only be bought from its own
goal's account, so a cheap, busy goal can't spend what a valuable, expensive one is saving
up for (builders eating the plane factory's minerals). Survival emergencies may draw on
everything, and with storage full anything goes: income is being lost.
]]
Needs.goalOf = {
	survival = 'survive', defense = 'survive', air = 'survive',
	income = 'economy', energy = 'economy', fuel = 'economy', prospecting = 'economy', builders = 'economy', spend = 'economy',
	research = 'research', planes = 'airpower', scouting = 'knowledge', army = 'army',
};
Needs.account = {};		-- goal -> minerals allocated to it and not yet spent
Needs.committed = 0;	-- ordered but not yet drawn from storage
Needs.goalValue = {};
Needs.wantCost = {};	-- goal -> {cost, round}: the dearest option it wanted lately

local function GoalKey(k)
	if string.sub(k, 1, 8) == 'survival' then return 'survive'; end;
	return Needs.goalOf[k] or 'economy';
end;

-- The goal an option serves most
function Needs.GoalOf(o)
	local w = Needs.Weights();
	local best, bestV = 'economy', -1;
	for k,amount in pairs(o.serves or {}) do
		local val = (k:sub(1, 8) == 'survival' and w.survival or (w[k] or 1)) * (Needs.v[k] or 0) * amount;
		if val > bestV then best, bestV = GoalKey(k), val; end;
	end;
	return best;
end;

function Needs.UpdateBudgets(v, storage, income)
	local w = Needs.Weights();
	local val, sum = {}, 0;
	for k,x in pairs(v) do
		local g = GoalKey(k);
		local wk = k:sub(1, 8) == 'survival' and w.survival or (w[k] or 1);
		val[g] = math.max(val[g] or 0, wk * x);
	end;
	for g,x in pairs(val) do sum = sum + x; end;
	if sum <= 0 then return; end;
	Needs.goalValue = val;
	-- what was drawn from storage since last round pays off commitments first
	local prev = Needs.prevStorage or storage;
	local drawn = math.max(0, prev + income - storage);
	-- (orders that came to nothing never draw: commitments fade, and never exceed storage)
	Needs.committed = math.min(storage, math.max(0, Needs.committed - drawn) * 0.8);
	Needs.prevStorage = storage;
	for g,x in pairs(val) do
		Needs.account[g] = (Needs.account[g] or 0) + income * x / sum;
	end;
	-- storage is the truth: minerals no account holds are shared out by value, accounts
	-- holding more than there is shrink alike
	local held = 0;
	for g,a in pairs(Needs.account) do
		if val[g] == nil then Needs.account[g] = nil; else held = held + math.max(0, a); end;
	end;
	local free = storage - Needs.committed - held;
	if free > 0 then
		for g,x in pairs(val) do Needs.account[g] = (Needs.account[g] or 0) + free * x / sum; end;
	elseif free < 0 then
		-- less in storage than the accounts hold (an emergency spent it): the least valuable
		-- goals give first, what the most valuable goal saved up is touched last
		local order = {};
		for g,_ in pairs(Needs.account) do order[#order+1] = g; end;
		table.sort(order, function(a, b) return (val[a] or 0) < (val[b] or 0); end);
		local short = -free;
		for _,g in ipairs(order) do
			local a = Needs.account[g];
			if a > 0 and short > 0 then
				local take = math.min(a, short);
				Needs.account[g] = a - take;
				short = short - take;
			end;
		end;
	end;
	--[[
	No hoarding: an account holding more than its goal could spend (the dearest thing it
	wanted lately, blocked or not) passes the surplus on to the most valuable goal that is
	still short of what it wants. Minerals locked in a blocked goal help nobody.
	]]
	local order = {};
	for g,_ in pairs(val) do order[#order+1] = g; end;
	table.sort(order, function(a, b) return val[a] > val[b]; end);
	local function Cap(g)
		local wc = Needs.wantCost[g];
		if wc and Sense.round - wc.round <= 10 then return wc.cost + 50; end;
		return 100;
	end;
	local surplus = 0;
	for _,g in ipairs(order) do
		local over = (Needs.account[g] or 0) - Cap(g);
		if over > 0 then Needs.account[g] = Needs.account[g] - over; surplus = surplus + over; end;
	end;
	for _,g in ipairs(order) do
		if surplus <= 0 then break; end;
		local room = Cap(g) - (Needs.account[g] or 0);
		if room > 0 and val[g] > 0 then
			local give = math.min(room, surplus);
			Needs.account[g] = (Needs.account[g] or 0) + give;
			surplus = surplus - give;
		end;
	end;
	-- (whatever is left stays free: shared out by value next round, or spent at full storage)
end;

-- What this option may spend: its goal's account, or everything in an emergency
function Needs.Available(o, budget)
	if budget == nil or budget >= 450 then return budget; end;
	local g = Needs.GoalOf(o);
	if g == 'survive' and Needs.Urgency(o) >= 0.7 then return budget; end;
	return math.min(budget, math.max(0, Needs.account[g] or 0));
end;

function Needs.Charge(o)
	if o == nil then return; end;
	local g = Needs.GoalOf(o);
	-- (emergencies and full storage spend past the account: a small debt at most, not a grudge)
	Needs.account[g] = math.max(-50, (Needs.account[g] or 0) - (o.cost or 0));
	Needs.committed = Needs.committed + (o.cost or 0);
end;

function Needs.Vital(o)
	local s = o.serves or {};
	if s.survivalBuilder or s.survivalMineral then return true; end;
	local t = o.want and o.want.type;
	return t ~= nil and (t == Types.generator or t == Types.mineralContainer or t == Types.plant or t == Types.mine);
end;

function Needs.Choose(options, what, budget)
	-- storage nearly full: income is being thrown away, anything useful beats nothing
	local full = (budget or 0) >= 450;
	local best, bestS = nil, full and 0.01 or Needs.minScore;
	local bestAny, bestAnyS = nil, 0;
	for _,o in ipairs(options) do
		o.score = Needs.Score(o.serves, o.cost) * (o.factor or 1) * Needs.Have(o.have, o.cap, o.swarm) * Needs.FailFactor(o.name);
		-- its foundation is gone: restore that first
		if what == 'structure' and o.want and Needs.Unsupported(o.want.type) then o.score = o.score * 0.1; end;
		if o.score > bestAnyS then bestAny, bestAnyS = o, o.score; end;
		if o.score > 0 then
			local g = Needs.GoalOf(o);
			local wc = Needs.wantCost[g];
			if wc == nil or Sense.round - wc.round > 10 or (o.cost or 0) >= wc.cost then
				Needs.wantCost[g] = {cost = o.cost or 0, round = Sense.round};
			end;
		end;
		local avail = Needs.Available(o, budget);
		local affordable = avail == nil or (o.cost or 0) <= avail + 50;
		-- The reserve: what the faction lives on (generator, container, plant, mine, builder) must
		-- stay replaceable. Everything else leaves that much in the store. (Every match lost to the
		-- Nexus ended with the minerals spent to nothing on towers and stores while the generators
		-- and plants were shot away one by one.)
		if affordable and Sense.round >= Cfg.reserveFromRound and not Needs.Vital(o)
				and (budget or 0) - (o.cost or 0) < Cfg.mineralReserve then
			affordable = false;
		end;
		if affordable and o.score > bestS then best, bestS = o, o.score; end;
	end;
	return best, bestAny;
end;

-------------------------------------------------------------------------------
-- Structures
-------------------------------------------------------------------------------

--[[
Regions worth defending, with a factor for how much is there: the home region always,
then built-up regions by what stands in them (power and sensors count extra) and the
threat around them.
]]
function Needs.DefendedRegions()
	local list = {};
	for id,_ in pairs(Sense.mine) do
		local reg = Map.regions[id];
		local value = reg.myStructures or 0;
		for _,st in ipairs(Sense.myStructures) do
			if Map.RegionOfLoc(st.location) == reg and (st.type == Types.generator or st.type == Types.mine) then
				value = value + 2;
			end;
		end;
		local threat = Sense.Threat(reg);
		for nb,_ in pairs(reg.neighbors) do threat = threat + Map.regions[nb].mobileThreat * 0.5; end;
		if id == Mem.homeRegion then value = value + 4; end;
		if value >= 3 then
			list[#list+1] = {reg = reg, value = value, threat = threat, weight = math.min(1.5, 0.4 + value / 12 + threat / 20)};
		end;
	end;
	table.sort(list, function(a, b) return a.value > b.value; end);
	while #list > 4 do table.remove(list); end;
	return list;
end;

function Needs.StructureOptions(constructorIndex)
	local opts = {};
	local function add(want, serves, name)
		if want and want.type then
			opts[#opts+1] = {want = want, serves = serves, cost = Cost(want.type), name = name or want.type.name};
		end;
	end;
	local cur, max = Economy.Resources();
	local minerals = cur.storage.minerals or 0;

	add(Economy.MineReq('Minerals'), {survivalMineral = 1, income = 1}, 'Mine (minerals)');
	--[[
	A plane factory burns fuel every round: without spare fuel supply it stands disabled.
	Fuel for it is the air goal's own requirement, bought from the air goal's account: the
	fuel mine first, the factory once the supply carries it.
	]]
	local pfT = Types.planeFactory;
	local pfFuel = pfT and pfT.balance and pfT.balance.demand and (pfT.balance.demand.fuel or 0) or 0;
	local spareFuel = max.balance.fuel or 0;
	local nPf = pfT and Economy.Count(pfT) or 0;
	local wantsPf = pfT and Types.IsUnlocked(pfT) and nPf < (Cfg.planeFactories or 1);
	-- short for the factories standing, or for the next one
	local fuelShort = pfFuel > 0 and ((nPf > 0 and spareFuel < 0) or (wantsPf and (Needs.v.planes or 0) > 0 and spareFuel < pfFuel));
	Needs.fuelShortForPlanes = fuelShort;
	local fuelServes = {survivalFuel = 1, fuel = 1, income = 0.3, energy = 0.3};
	if fuelShort then
		-- (a factory that stands and cannot run for want of fuel, while the army has no eyes: the
		-- fuel mine scored 0,08, below a store on a peak, for 850 rounds)
		fuelServes.planes = Needs.eyes and 3 or 1.2;
		-- (the first factory for the army's eyes may run on what is stored: one match had it at
		-- round 168 and 39 drones out of a store of some 300 with supply below demand, the next
		-- stood "blocked: needs fuelsupply" to the end with 79 supply and no drone at all)
		local stock = cur.storage.fuel or 0;
		-- (nor when generators can rest for it: Economy.EyesFuel frees the 20 fuel it draws once
		-- it stands. Blocked 777 times in one match with no factory and so no drone at all)
		local canRest = Needs.eyes and nPf == 0 and Types.generator and Economy.Count(Types.generator) >= 3;
		if not canRest and not (Needs.eyes and nPf == 0 and stock >= pfFuel * (Cfg.planeFuelStock or 2)) then
			Needs.Block('Plane Factory', 'fuelsupply', math.max(0.6, Needs.v.planes or 0));
		end;
	end;
	add(Economy.MineReq('Fuel'), fuelServes, 'Mine (fuel)');
	if Types.generator and Economy.powerOrders < 2 then
		add({type = Types.generator, kind = 'power'}, {survivalGenerator = 1, energy = 1}, 'Generator');
	end;
	if Types.plant then
		local plants = Economy.Count(Types.plant);
		add({type = Types.plant, kind = 'base'}, {survivalPlant = 1, spend = 1, army = 0.3});
		opts[#opts].have = plants;
		opts[#opts].cap = Cfg.maxPlants;
		-- a hard stop, counting the ones on order (the "plants full" factor below outweighed the
		-- soft cap, and plants ordered but not finished were not counted: five orders in 11 rounds)
		local pending = 0;
		for _,job in pairs(Mem.jobs) do
			if job.kind == Types.plant.name then pending = pending + 1; end;
		end;
		if plants + pending >= Cfg.maxPlants then
			opts[#opts] = nil;
		-- every plant queue full while minerals pile up: another plant is the missing capability
		elseif Economy.plantsFull then
			opts[#opts].factor = 1 + 1.5 * Needs.BlockValue('capacity');
		end;
	end;
	if Types.reactor and Types.IsUnlocked(Types.reactor) and Economy.ReactorSiteReady() then
		-- (a core burns 5 uranium a round: another one only when that much still comes in beyond
		-- what the standing ones burn, or none stands and there is a store to live off)
		local uranium = (max.balance.uranium or 0) >= 5 or (Economy.Count(Types.reactor) < 1 and (cur.storage.uranium or 0) > 0);
		if uranium then
			add({type = Types.reactor, kind = 'power'}, {energy = 1.5, spend = 0.3});
		elseif Economy.Count(Types.reactor) < 1 then
			add(Economy.MineReq('Uranium'), {energy = 0.4}, 'Mine (uranium)');
		end;
	elseif Types.reactor and Types.IsUnlocked(Types.reactor) and Economy.Count(Types.reactor) < 1
		and (cur.storage.uranium or 0) >= 100 and (max.balance.uranium or 0) >= 5 and (cur.balance.fuel or 0) < 10 then
		-- no generator ring to put it into, the fuel carries no further generator and uranium lies
		-- in store: one reactor core on plain ground (40 energy for 5 uranium a round)
		add({type = Types.reactor, kind = 'base'}, {energy = 1.5, spend = 0.3}, 'Reactor Core (plain site)');
	end;
	-- uranium comes in (a port gives 5 a round) and nothing keeps it: one store, once the economy
	-- stands. What fires uranium (EMP, bombs) and a reactor in lean rounds live off it
	if Types.uraniumStorage and Types.IsUnlocked(Types.uraniumStorage) and Economy.Count(Types.uraniumStorage) < 1
		and (max.balance.uranium or 0) > 0 and Economy.Count(Types.mine) >= 5 and Economy.Count(Types.generator) >= 3 then
		add({type = Types.uraniumStorage, kind = 'base'}, {energy = 0.4, spend = 0.5}, 'Uranium Storage');
	end;
	if Types.lab then
		-- labs cluster: next to the existing ones. A lab that continues a cluster counts as
		-- the first of its kind (the cluster isn't complete yet)
		add({type = Types.lab, kind = 'base', near = Types.lab}, {research = 1, spend = 0.3});
		opts[#opts].have = Needs.labsMidCluster and 0 or Economy.Count(Types.lab);
	end;
	do
		-- the plane factory without the mineral check: saving for it is the scorer's job
		local pf = Types.planeFactory;
		if pf and Types.IsUnlocked(pf) and Economy.Count(pf) < (Cfg.planeFactories or 1) then
			if Economy.FuelMines() >= 1 then
				-- late game, the sensor drones it makes are the best cover for large land masses
				local late = Sense.round >= 60 and Types.drone ~= nil;
				add({type = pf, kind = 'base'}, {planes = 1, spend = 0.3, scouting = late and 0.6 or 0});
				opts[#opts].have = Economy.Count(pf);
				-- no fuel to run it: the fuel mine comes first
				if fuelShort then opts[#opts].factor = 0.15; end;
			end;
		end;
	end;
	-- defense, per location: at each defended region a gun tower, a radar, an AA tower and a
	-- launcher compete. Whatever the region has none of counts most. AA wins only as far as
	-- the enemy is likely to come by air (Needs.enemyAirShare), so usually radar and guns first
	for _,e in ipairs(Needs.DefendedRegions()) do
		local reg, c, weight = e.reg, Economy.DefenseCounts(e.reg), e.weight;
		-- the right amount in the right spot: one of each kind is always worth it, more only
		-- as far as the threat there can use them, and only while there is room
		local pot = Military.UnitPotential();
		local cap = 1 + math.floor(e.threat / (pot * 3));
		weight = weight * (0.25 + 0.75 * Needs.Room(reg));
		local airShare = Needs.enemyAirShare;
		local ground = 1 - airShare;
		local function tower(t, have, serves, name)
			if t and Economy.TowerAllowed(t) and not Economy.scannerClaimed[t.name..'@'..reg.id] then
				local key = t.name..'@'..reg.id;
				-- what stands there, or was ordered for it lately (the site may end up next door)
				have = math.max(have, Needs.RecentOrders(key));
				opts[#opts+1] = {want = {type = t, kind = 'tower', region = reg.id}, serves = serves,
					cost = Cost(t), have = have, cap = cap, factor = weight, name = (name or t.name)..' @'..reg.id, key = key};
			end;
		end;
		tower(Types.GroundTower(), c.ground, {defense = 0.6 + 0.6 * ground});
		--[[
		Radar sees the ground, AA the sky: each covers only its own layer. A drone covers both,
		so only without enough drones do radar and AA go up in pairs: the one missing from a
		spot that has the other gets a push for the coverage it completes.
		]]
		local noDrones = Needs.dronesShort;
		local pairRadar = (noDrones and c.aa > 0 and c.radar == 0) and 0.4 or 0;
		local pairAA = (noDrones and c.radar > 0 and c.aa == 0 and Sense.AirMatters()) and 0.4 or 0;
		tower(Types.radarTower, c.radar, {scouting = (Needs.blind[reg.id] and 1 or 0.5) + pairRadar, defense = 0.4});
		tower(Types.aaTower, c.aa, {air = math.min(1, airShare * 3), defense = 0.6 * airShare, scouting = pairAA});
		if Mem.heavySeen >= 0 or Sense.round >= 60 then
			tower(Types.launcherTower, c.launcher, {defense = 0.9 * ground});
		end;
		if Needs.deepRisk >= 0.3 then
			tower(Types.scannerTower, c.scanner, {scouting = 0.8 * Needs.deepRisk, defense = 0.2 + 0.3 * Needs.deepRisk});
		end;
	end;
	--[[
	A ring of scanner (surveyor) towers along my border, so nothing drills in unseen. With
	research their range can rival a radar's. One per border region, weighed by deepRisk.
	]]
	local st = Types.scannerTower;
	if st and Needs.deepRisk >= 0.3 and Economy.TowerAllowed(st) and Types.IsUnlocked(st) then
		local listed = {};
		for _,e in ipairs(Needs.DefendedRegions()) do listed[e.reg.id] = true; end;
		for id,_ in pairs(Sense.mine) do
			local border = false;
			for nb,_ in pairs(Map.regions[id].neighbors) do
				if not Sense.mine[nb] then border = true; break; end;
			end;
			local key = st.name..'@'..id;
			if border and not listed[id] and not Economy.scannerClaimed[key] then
				local have = math.max(Economy.DefenseCounts(Map.regions[id]).scanner, Needs.RecentOrders(key));
				opts[#opts+1] = {want = {type = st, kind = 'tower', region = id}, serves = {scouting = 0.6 * Needs.deepRisk, defense = 0.3 * Needs.deepRisk},
					cost = Cost(st), have = have, cap = 1, factor = 0.6, name = st.name..' (border ring) @'..id, key = key};
			end;
		end;
	end;
	-- blind regions of mine that are not worth guns: a radar there keeps them in sight
	do
		local listed = {};
		for _,e in ipairs(Needs.DefendedRegions()) do listed[e.reg.id] = true; end;
		local t = Types.radarTower;
		-- with drones flying (or a factory to make them), radar towers in the hinterland matter less
		local drones = Types.drone and Types.IsUnlocked(Types.drone) and Types.planeFactory and Economy.Count(Types.planeFactory) > 0;
		for id,_ in pairs(Needs.blind or {}) do
			local reg = Map.regions[id];
			if t and not listed[id] and Economy.TowerAllowed(t) and not Economy.scannerClaimed[t.name..'@'..id]
				and not reg.seenNow then
				local key = t.name..'@'..id;
				local have = math.max(Economy.DefenseCounts(reg).radar, Needs.RecentOrders(key));
				opts[#opts+1] = {want = {type = t, kind = 'tower', region = id}, serves = {scouting = 0.8},
					cost = Cost(t), have = have, cap = 1, factor = drones and 0.25 or 0.6, name = t.name..' (blind spot) @'..id, key = key};
			end;
		end;
	end;
	if Types.store and Sense.round >= 12 and Types.IsUnlocked(Types.store) and Economy.Count(Types.store) < Cfg.maxStores then
		local want, peak = Economy.StoreRegion();
		-- a peak store needs a builder on the surface: not while the underground AI lives below
		if peak and Mole and Mole.phase ~= 'surface' then want, peak = nil, nil; end;
		if want then
			add({type = Types.store, kind = 'tower', region = want, peak = peak},
				{defense = peak and 0.8 or 0.3, army = 0.3, spend = 0.3}, peak and ('Store on peak @'..want) or nil);
			opts[#opts].have = Economy.Count(Types.store);
		end;
	end;
	--[[
	What draws power is not bought while there is none to spare: a lab, plant or plane factory
	without power stands idle and takes the minerals (and the site) the generator needed. And
	nothing is bought beyond its cap (Cfg.caps: type name -> most to have; labs: the lab limits).
	]]
	do
		local starved = (cur.balance.energy or 0) <= 0 and Sense.round >= 20 and Economy.Count(Types.generator) >= 1;
		local off = Sense.offByKind or {};
		-- (a mine or the port switched off counts only while the balance is thin: with 80 energy to
		-- spare and a mine rested on purpose, the second plant was refused for 200 rounds)
		if (off.mine or 0) + (off.port or 0) > 0 and (cur.balance.energy or 0) < (Cfg.spareEnergy or 15) then starved = true; end;
		local labs = Types.lab and Economy.Count(Types.lab) or 0;
		local labCap = Cfg.wantLabsRich or 6;
		if not Needs.T2Capable() and Sense.round >= 80 and minerals >= (Cfg.richMinerals or 400) then labCap = math.max(labCap, Cfg.wantLabsMax or 15); end;
		local keep = {};
		for _,o in ipairs(opts) do
			local t = o.want.type;
			local drop = nil;
			if Economy.Capped(t) then drop = 'capped';
			elseif t == Types.lab and labs >= labCap then drop = 'lab limit '..labCap;
			elseif t == Types.generator and Economy.Count(t) >= 1 and Economy.FuelForGenerator() < 10 then drop = 'the fuel does not carry it';
			elseif starved and (t == Types.lab or t == Types.planeFactory or (t == Types.plant and Economy.Count(t) >= 1)) then drop = 'no power to spare'; end;
			if drop then
				if Sense.round % 10 == 0 then Needs.Note(o.name..' not on offer: '..drop); end;
			else
				keep[#keep+1] = o;
			end;
		end;
		opts = keep;
	end;
	return opts;
end;

-- Personalities may adjust options here (veto, boost)
function Needs.AdjustStructureOptions(opts) return opts; end;

--[[
The floors below (never no plant, generators and containers not under a minimum, towers at
home) hand a builder their order before anything is weighed. One that cannot be built must not
hold up everything else: the generator floor did (130 to 170 rounds in two duels), then the
container floor (54 rounds, minerals at the cap, two generators left). So every floor order
goes through here: if it did not become a build order three times running, that floor rests
for 15 rounds and the ordinary choice goes on.
]]
Needs.floorFails, Needs.floorOff = {}, {};
function Needs.Floor(want)
	if want == nil or want.type == nil then return want; end;
	local k = want.type.name;
	if (Needs.floorOff[k] or -1) > Sense.round then return nil; end;
	Needs.floorPending = k;
	return want;
end;

function Needs.NextStructure(constructorIndex)
	-- the last choice never turned into an order
	if Needs.pending then Needs.Failed(Needs.pending); Needs.pending = nil; end;
	-- (a floor's order that did not turn into a build order three times running rests for 15 rounds)
	if Needs.floorPending then
		local k = Needs.floorPending;
		Needs.floorPending = nil;
		Needs.floorFails[k] = (Needs.floorFails[k] or 0) + 1;
		if Needs.floorFails[k] >= 3 then
			Needs.floorFails[k] = 0;
			Needs.floorOff[k] = Sense.round + 15;
			Needs.Note('the floor for '..k..' came to nothing three times: the ordinary choice goes on for 15 rounds');
		end;
	end;
	local cur = Economy.Resources();
	local minerals = cur.storage.minerals or 0;
	-- A faction without a mineral container is dead by the server's rule, and with its only one
	-- gone it stores nothing: production stops and the builders die out. So: never none, and a
	-- second one once the base stands (one bomber run on the first used to end the match).
	local mc = Types.mineralContainer;
	if mc and Types.IsUnlocked(mc) and Sense.round >= 5 then
		local n = Economy.Count(mc);
		if n == 0 or (n < Cfg.minContainers and Sense.round >= Cfg.spareContainerRound and minerals >= Cost(mc)
				and Economy.Count(Types.mine) >= 2 and Economy.Count(Types.generator) >= 2) then
			Needs.Note('Mineral Container ordered: '..n..' standing, '..Cfg.minContainers..' wanted');
			-- not all in one place: one raid or bomber run must not take them all
			local with, region = {}, nil;
			for _,s in ipairs(Sense.myStructures or {}) do
				local reg = s.type == mc and s.location and Map.RegionOfLoc(s.location);
				if reg then with[reg.id] = true; end;
			end;
			for id,_ in pairs(Sense.mine) do
				if not with[id] and Sense.Threat(Map.regions[id]) <= 0 and (region == nil or id < region) then region = id; end;
			end;
			do local w = Needs.Floor({type = mc, kind = 'base', region = n > 0 and region or nil}); if w then return w; end; end;
		end;
	end;
	-- (Only against a Nexus that has shown more than ports, Mem.nexusActive: where it merely owns
	-- the ports, as in a duel, the 110 minerals cost the second and third mine and the match: 0 of 12.)
	-- The first raid comes before the needs see a threat: with a Nexus on the planet, two rocket
	-- towers at home as soon as two mines and two generators stand. (The matches that died early
	-- were raided at round 11 to 33 with 2 to 4 units and no tower.)
	do
		local rt = Game.Type.units['Rocket Launcher Tower'];
		-- ... and at once, before anything else, with a second claimable port next door
		local nextDoor = Cfg.nextDoorHops > 0 and Sense.PortNextDoor();
		-- A tower shoots as far as someone sees for it: the first tower gets a radar tower before the second
		-- (two rocket towers without radar killed 0 to 3 raiders and lost up to 9 structures)
		local rdr = Types.radarTower;
		if rt and rdr and Mem.homeRegion and Types.IsUnlocked(rdr) and Economy.Count(rt) >= 1 and Economy.Count(rdr) < 1
				and minerals >= Cost(rdr) then
			Needs.Note('Radar Tower ordered at home for the towers');
			do local w = Needs.Floor({type = rdr, kind = 'tower', region = Mem.homeRegion}); if w then return w; end; end;
		end;
		-- with the radar standing, the long arm: a missile launcher tower reaches 7 cells but sees 2,
		-- a rocket tower reaches 4 and sees 3
		local lt = Types.launcherTower;
		local long = lt and rdr and Types.IsUnlocked(lt) and Economy.Count(rdr) >= 1 and minerals >= Cost(lt);
		local et = long and lt or rt;
		local standing = Economy.Count(rt) + Economy.Count(lt);
		if et and Mem.homeRegion and Sense.round <= 80 and Types.IsUnlocked(et)
				and standing < Cfg.earlyTowers and minerals >= Cost(et)
				and (nextDoor or (Sense.nexus and Mem.nexusActive and Sense.round >= 6
					and Economy.Count(Types.mine) >= 2 and Economy.Count(Types.generator) >= 2)) then
			Needs.Note('early '..et.name..' ordered at home: '..standing..' standing'..(nextDoor and ' (a port next door)' or ''));
			do local w = Needs.Floor({type = et, kind = 'tower', region = Mem.homeRegion}); if w then return w; end; end;
		end;
	end;
	-- AA inside the wall, seen planes or not: the Nexus scouts with sensor drones, and what they see gets raided
	-- (one AA tower early as insurance, whoever the opponent is; the second and the radar once aircraft
	-- were seen or are due: between two AIs without planes the full set cost the economy its lead)
	do
		local settled = Sense.round >= 12 and Economy.Count(Types.mine) >= 2 and Economy.Count(Types.generator) >= 2;
		local wantAA = settled and (Sense.AirMatters() and Cfg.wallAA or Cfg.earlyAA) or 0;
		if Types.aaTower and Types.IsUnlocked(Types.aaTower) and Mem.homeRegion
				and Economy.Count(Types.aaTower) < wantAA and minerals >= Cost(Types.aaTower) then
			Needs.Note('AA Tower ordered at home: '..Economy.Count(Types.aaTower)..' standing');
			do local w = Needs.Floor({type = Types.aaTower, kind = 'tower', region = Mem.homeRegion}); if w then return w; end; end;
		end;
		-- (and one within sight of whatever a bomber goes for, see Economy.AAGap)
		if settled and minerals >= Cost(Types.aaTower or Types.mine) and not Economy.Capped(Types.aaTower) then
			local gap = Economy.AAGap();
			if gap then return gap; end;
		end;
		-- ... and eyes on the base and its wall: what is not seen is likely not there any more
		if settled and (Sense.AirMatters() or Mem.wall) and Types.radarTower and Types.IsUnlocked(Types.radarTower) and Mem.homeRegion
				and Economy.Count(Types.radarTower) < 1 and minerals >= Cost(Types.radarTower) then
			Needs.Note('Radar Tower ordered at home');
			do local w = Needs.Floor({type = Types.radarTower, kind = 'tower', region = Mem.homeRegion}); if w then return w; end; end;
		end;
	end;
	-- stores where a take formation gathers: it waits inside and comes out in one go
	if Cfg.useFormations and (Cfg.assaultStores or 0) > 0 and Types.store and Types.IsUnlocked(Types.store) and minerals >= Cost(Types.store) then
		for _,f in ipairs(Formation.list or {}) do
			-- (not on my own ground: there the members are the defence and stay out, see Military.Stores)
			if not f.over and f.kind == 'take' and f.stage == 'gather' and f.staging and not f.atHome
				and not Sense.mine[f.staging] and f.staging ~= Mem.homeRegion then
				local n = 0;
				for _,st in ipairs(Sense.stores) do
					local r = Map.RegionOfLoc(st.location);
					if r and r.id == f.staging then n = n + 1; end;
				end;
				for _,job in pairs(Mem.jobs) do
					if job.kind == Types.store.name and job.region == f.staging then n = n + 1; end;
				end;
				local wantN = math.min(5, math.max(Cfg.assaultStores, math.ceil((f.want or 0) / 6)));
				-- (one builder has the job and keeps it till it is lost: asked of whoever came first,
				-- every builder would walk off to the staging ground)
				local me = Sense.my.constructor[constructorIndex];
				Needs.storeBuilder = Needs.storeBuilder or {};
				local who = Needs.storeBuilder[f.staging];
				local there = false;
				for _,c in ipairs(Sense.my.constructor) do
					if c.id == who then there = true; break; end;
				end;
				-- (nor does it stay with one that is never free to be asked)
				Needs.storeAsked = Needs.storeAsked or {};
				if there and me and me.id ~= who and Sense.round - (Needs.storeAsked[f.staging] or Sense.round) > 10 then there = false; end;
				if me and ((Needs.storeBar or {})[me.id] or 0) > Sense.round then
					if who == me.id then who = nil; Needs.storeBuilder[f.staging] = nil; end;
				elseif not there and me then who = me.id; Needs.storeBuilder[f.staging] = who; end;
				if me and me.id == who then Needs.storeAsked[f.staging] = Sense.round; end;
				Needs.storeAsked[f.staging] = Needs.storeAsked[f.staging] or Sense.round;
				if n < wantN and me and me.id == who then
					if Sense.round % 5 == 0 then Needs.Note('Vehicle Store ordered for the gathering at region '..f.staging..' ('..n..' of '..wantN..') from '..me.niceName); end;
					local w = Needs.Floor({type = Types.store, kind = 'tower', region = f.staging, inRegion = true});
					if w then return w; end;
				end;
			end;
		end;
	end;
	-- the same for what makes the energy and the vehicles: never none, and generators not below a floor
	if Sense.round >= 20 then
		if Types.plant and Types.IsUnlocked(Types.plant) and Economy.Count(Types.plant) == 0 and minerals >= Cost(Types.plant) then
			Needs.Note('Vehicle Plant ordered: none standing');
			do local w = Needs.Floor({type = Types.plant, kind = 'base'}); if w then return w; end; end;
		end;
		if Types.generator and Economy.Count(Types.generator) < Cfg.minGenerators and minerals >= Cost(Types.generator) then
			-- (only one the fuel carries: Economy.BuildAt refuses any other, and with the floor
			-- handing every free builder a generator that is refused, nothing was built at all for
			-- 130 to 170 rounds in two duels. Short of fuel, a fuel mine is what the floor asks for;
			-- with no site for one the ordinary choice goes on)
			if Economy.Count(Types.generator) < 1 or Economy.FuelForGenerator() >= 10 then
				Needs.Note('Generator ordered: '..Economy.Count(Types.generator)..' standing, '..Cfg.minGenerators..' wanted');
				do local w = Needs.Floor({type = Types.generator, kind = 'base'}); if w then return w; end; end;
			end;
			local fm = (Needs.floorFuelTried ~= Sense.round and (Economy.noMineSite.Fuel or -99) < Sense.round - 10) and Economy.MineReq('Fuel') or nil;	-- (no site found: asked again after 10 rounds)
			if fm and minerals >= Cost(Types.mine) then
				Needs.floorFuelTried = Sense.round;	-- (one builder a round: the rest take the ordinary choice)
				Needs.Note('Fuel mine ordered: '..Economy.Count(Types.generator)..' generator(s) standing, '..Cfg.minGenerators..' wanted, the fuel carries no further one');
				do local w = Needs.Floor(fm); if w then return w; end; end;
			end;
		end;
	end;
	--[[
	Minerals piling up with every plant queue full: the next plant now, and the power for it.
	Two matches stood at two plants till round 130 with the store at its cap for 31 and 77
	rounds; the army stayed at 14 and the base fell to an assault of twenty.
	]]
	if Sense.round >= 40 and minerals >= (Cfg.plantFloorMinerals or 1000) and Economy.plantsFull ~= false
		and Types.plant and Types.IsUnlocked(Types.plant) and Needs.plantFloorRound ~= Sense.round then
		local plants, pending = Economy.Count(Types.plant), 0;
		for _,job in pairs(Mem.jobs) do
			if job.kind == Types.plant.name then pending = pending + 1; end;
		end;
		if plants >= 1 and pending == 0 and plants < Cfg.maxPlants and not Economy.Capped(Types.plant) then
			Needs.plantFloorRound = Sense.round;	-- (one builder a round)
			if (cur.balance.energy or 0) >= (Cfg.spareEnergy or 15) then
				Needs.Note('Vehicle Plant ordered: '..minerals..' minerals lie unspent and every plant is busy ('..plants..' standing)');
				do local w = Needs.Floor({type = Types.plant, kind = 'base'}); if w then return w; end; end;
			elseif Types.generator and not Economy.Capped(Types.generator) and Economy.FuelForGenerator() >= 10 then
				Needs.Note('Generator ordered: power for the next Vehicle Plant ('..minerals..' minerals unspent)');
				do local w = Needs.Floor({type = Types.generator, kind = 'base'}); if w then return w; end; end;
			end;
		end;
	end;
	local opts = Needs.AdjustStructureOptions(Needs.StructureOptions(constructorIndex));
	local best, top = Needs.Choose(opts, 'structure', minerals);
	-- saving up: the top option is out of reach, and nothing affordable comes close
	-- (never with storage full: saving more is impossible, minerals are being lost)
	-- saving needs something to save from: without mineral income, saving waits forever
	local income = (Needs.mineralMines or 0) > 0;
	if top and top ~= best and minerals < 450 and not income and (best == nil or best.score < top.score * 0.6) then
		Needs.Block('saving for '..top.name, 'income', Needs.Urgency(top));
	end;
	-- (saving is per goal now: another goal's cheaper option spends its own account, it
	-- doesn't take from the one being saved up)
	if top and top ~= best and minerals < 450 and income and (best == nil or best.score < top.score * 0.6)
		and (best == nil or Needs.GoalOf(best) == Needs.GoalOf(top)) then
		Needs.saving = {score = top.score, cost = top.cost, name = top.name, round = Sense.round, goal = Needs.GoalOf(top)};
		Needs.Note('saving for '..top.name..string.format(' (%.2f)', top.score));
		return nil;
	end;
	if Needs.saving and Needs.saving.round < Sense.round - 1 then Needs.saving = nil; end;
	if best == nil then return nil; end;
	local want = best.want;
	if want.kind == 'tower' and want.region and want.type then
		Economy.scannerClaimed[want.type.name..'@'..want.region] = true;
	end;
	if want.type == Types.generator then Economy.powerOrders = Economy.powerOrders + 1; end;
	if best.key then
		Needs.ordered[best.key] = Needs.ordered[best.key] or {};
		table.insert(Needs.ordered[best.key], Sense.round);
	end;
	Needs.Note(string.format('build %s (%.2f)', best.name, best.score));
	Needs.pending = best.name;
	Needs.pendingUrgency = Needs.Urgency(best);
	Needs.Charge(best);
	return want;
end;

-------------------------------------------------------------------------------
-- Vehicles
-------------------------------------------------------------------------------

function Needs.VehicleOptions(kit)
	local opts = {};
	local function add(t, serves, name)
		if t then opts[#opts+1] = {want = t, serves = serves, cost = Cost(t), name = name or t.name}; end;
	end;
	-- what we have of each: active units, plus those parked in stores counted double
	local function have(role)
		return #(Sense.my[role] or {}) + 2 * (Sense.storedByRole[role] or 0);
	end;
	add(kit.Constructor(), {builders = 1, income = 0.2, survivalBuilder = 1});	-- the builders need already counts them
	-- spending capacity can raise the builders need for long: beyond what the base can use,
	-- each further constructor is worth far less
	if #opts > 0 then
		local nC = #Sense.my.constructor + (Sense.storedByRole.constructor or 0);
		-- minerals piling up: the base can use more hands
		local full = (Economy.Resources().storage.minerals or 0) >= 450;
		local capC = (Cfg.constructorsLate or 5) + 1 + (full and 2 or 0);
		if nC >= capC then opts[#opts].factor = 0.35 ^ (nC - capC + 1);
		else
			-- income piling up with every builder busy: a builder is the missing capability
			opts[#opts].factor = 1 + 1.5 * Needs.BlockValue('capacity');
		end;
	end;
	local combat = #Sense.my.combat + #Sense.my.bomb;
	-- tanks: as many as the action can use (the attack target's required force, or the
	-- pressure at home), counted in groups of four. Beyond that only a swarm floor:
	-- meat shields and sensor coverage through numbers
	local tank = kit.Tank();
	add(tank, {defense = 1, army = 1, scouting = 0.15, survivalGarrison = 1});
	if tank and #opts > 0 then
		local o = opts[#opts];
		local pot = Military.UnitPotential();
		local use = math.ceil(Sense.Pressure() / pot) + 2 * #Needs.DefendedRegions();
		local target = Mem.attackTarget or Military.ChooseAttackTarget();
		if target then use = use + Military.RequiredForce(target); end;
		-- (what a take of mine wants is use too: with formations there is no attack target, and a
		-- take stood at "members 10 of 22" for 600 rounds while the plants made nothing)
		for _,f in ipairs(Formation and Formation.list or {}) do
			if f.kind == 'take' and not f.over then use = use + (f.want or 0); end;
		end;
		o.have = (combat + 2 * (Sense.storedByRole.combat or 0)) / 4;
		o.cap = math.max(1, use / 4);
		o.swarm = 0.12;
	end;
	-- mobile AA: one per spot that needs air cover, a second there lets them leapfrog
	-- (one moves while the other covers)
	local aa = Types.Vehicle(Types.turret.aa, Types.base.hover);
	add(aa, {air = math.min(1, 0.4 + Needs.enemyAirShare * 2)});
	if aa and #opts > 0 then
		local spots = 0;
		for _,e in ipairs(Needs.DefendedRegions()) do
			if e.reg.airThreat > 0 or Needs.enemyAirShare >= 0.3 then spots = spots + 1; end;
		end;
		if Mem.posture == 'attack' then spots = spots + 1; end;
		opts[#opts].have = have('aa');
		opts[#opts].cap = math.max(1, spots * 2);
	end;
	add(Types.Vehicle(Types.turret.surveyor, Types.base.heavy) or Types.Vehicle(Types.turret.surveyor, Types.base.hover), {scouting = 1, prospecting = 1});
	if #opts > 0 then opts[#opts].have = have('surveyor'); opts[#opts].cap = 3; end;
	add(Types.Vehicle(Types.turret.radar, Types.base.hover), {scouting = 0.5, defense = 0.3});
	if #opts > 0 then
		-- each further one counts for less, and beyond one per radarPerCombat combat units hardly
		-- at all (a large army used to make every radar look like the first: they drained the minerals)
		opts[#opts].have = have('radar');
		opts[#opts].cap = math.max(1, math.ceil(combat / Cfg.radarPerCombat));
	end;
	-- An army beyond what the ground carries blocks its own paths and those of the builders
	-- (60 heavy units in one region: every move order refused, no port retaken). At the cap
	-- only builders, air defence and scouts are still produced; losses are replaced.
	local army = #Sense.my.combat + #Sense.my.bomb;		-- (ground units: planes don't block paths)
	if army >= Cfg.maxArmy then
		local kept = {};
		for _,o in ipairs(opts) do
			if not (o.serves.army or o.serves.survivalGarrison) or o.serves.survivalBuilder then kept[#kept+1] = o; end;
		end;
		if #kept < #opts then Needs.Note('army at its cap ('..army..' of '..Cfg.maxArmy..'): no more combat units'); end;
		opts = kept;
	end;
	return opts;
end;

function Needs.AdjustVehicleOptions(opts) return opts; end;

function Needs.NextVehicle(kit)
	local cur = Economy.Resources();
	local minerals = cur.storage.minerals or 0;
	local opts = Needs.AdjustVehicleOptions(Needs.VehicleOptions(kit));
	-- (a kind at its cap is no option: left in, the capped builder stayed the top choice round
	-- after round, each time became a tank, and the surveyor behind it was never made)
	if next(Cfg.unitCaps or {}) then
		local keep = {};
		for _,o in ipairs(opts) do
			if not Economy.UnitCapped(o.want) then keep[#keep+1] = o; end;
		end;
		opts = keep;
	end;
	-- (62 surveyors were built and lost in match 151 against 48 tanks: one every surveyorEvery
	-- rounds at most, what dies in between is not replaced at once)
	local function isSurveyor(t) return t and string.find(string.lower(tostring(t.name)), 'surveyor', 1, true) ~= nil; end;
	-- (but an army on a take with no surveyor at all has no eye below ground: then one every 20)
	local every = Cfg.surveyorEvery or 80;
	if #Sense.my.surveyor < (Cfg.takeSurveyors or 2) and Military and Military.SpotRegion and Military.SpotRegion() then every = Cfg.surveyorEveryTake or 20; end;
	-- (nor is one waited 80 rounds for when no mineral site is known at all: match 205 knew none for
	-- 500 rounds with no surveyor from round 200 to 300, stood on 1 to 3 mines and had no army till round 890)
	if #Sense.my.surveyor < (Cfg.prospectSurveyors or 2) and Economy.noMineSite and ((Economy.noMineSite['Minerals']
			and Sense.round - Economy.noMineSite['Minerals'] <= 10) or (Economy.noMineSite['Fuel'] and Sense.round - Economy.noMineSite['Fuel'] <= 10)) then every = math.min(every, Cfg.surveyorEveryProspect or 20); end;
	if Sense.round - (Needs.lastSurveyor or -1000) < every then
		local keep = {};
		for _,o in ipairs(opts) do
			if not isSurveyor(o.want) then keep[#keep+1] = o; end;
		end;
		opts = keep;
	end;
	local best, top = Needs.Choose(opts, 'vehicle', minerals);
	-- no builder at all: nothing can be built or rebuilt. The constructor goes first even if it
	-- isn't affordable yet: anything cheaper bought meanwhile only delays it
	if (Needs.v.survivalBuilder or 0) > 0 and top and top ~= best and top.want == kit.Constructor() then
		Needs.Note('no builder: queueing '..top.name..' first');
		return top.want;
	end;
	-- (minerals lying at the cap and the army below its own: a tank beats nothing. Two matches
	-- stood for 600 rounds with every plant asleep, an army of 20 and the store full)
	if best == nil and minerals >= (Cfg.plantFloorMinerals or 1000) and #Sense.my.combat + #Sense.my.bomb < Cfg.maxArmy then
		local t = kit.Tank();
		if t and not Economy.UnitCapped(t) then
			Needs.Note('produce '..t.name..': '..minerals..' minerals lie unspent and nothing else is wanted');
			return t;
		end;
	end;
	if best == nil then return nil; end;
	-- the constructors are saving for something that matters more: hold back
	local s = Needs.saving;
	if s and Sense.round - s.round <= 1 and best.score < s.score * 0.8 and minerals - best.cost < s.cost
		and Needs.GoalOf(best) == s.goal and minerals < (Cfg.plantFloorMinerals or 1000) then	-- (nothing is saved for at the cap)
		return nil;
	end;
	Needs.Note(string.format('produce %s (%.2f)', best.name, best.score));
	Needs.Charge(best);
	if isSurveyor(best.want) then Needs.lastSurveyor = Sense.round; end;
	return best.want;
end;

-------------------------------------------------------------------------------
-- Logging: the needs every 10 rounds, decisions as they happen (deduplicated per round)
-------------------------------------------------------------------------------

function Needs.Note(msg)
	if Needs.log[msg] == Sense.round then return; end;
	Needs.log[msg] = Sense.round;
	Log('Needs: '..msg);
end;

function Needs.Report()
	if Sense.round % 10 ~= 0 then return; end;
	local nb = 0;
	for _ in pairs(Needs.blind or {}) do nb = nb + 1; end;
	if nb > 0 then Log('Needs: '..nb..' of my regions out of sight'); end;
	Log(string.format('Needs: enemy air share %.2f, artillery risk %.2f, splash %.1f', Needs.enemyAirShare, Needs.artyRisk, Needs.splash));
	local keys = {};
	for k,_ in pairs(Needs.v) do keys[#keys+1] = k; end;
	table.sort(keys);
	local parts = {};
	for _,k in ipairs(keys) do
		if Needs.v[k] > 0 then parts[#parts+1] = string.format('%s %.2f', k, Needs.v[k]); end;
	end;
	Log('Needs: '..table.concat(parts, ', '));
	local bl = {};
	for cause,b in pairs(Needs.blocks) do
		local val = Needs.BlockValue(cause);
		if val > 0 then bl[#bl+1] = string.format('%s %.2f (%s)', cause, val, b.what); end;
	end;
	table.sort(bl);
	if #bl > 0 then Log('Needs blocked: '..table.concat(bl, '; ')); end;
	local gs = {};
	for g,x in pairs(Needs.goalValue or {}) do gs[#gs+1] = string.format('%s %.1f/%d', g, x, math.floor(Needs.account[g] or 0)); end;
	table.sort(gs);
	if #gs > 0 then Log('Needs goals (value/minerals): '..table.concat(gs, ', ')..string.format('; committed %d', math.floor(Needs.committed))); end;
	local ad = {};
	for k,m in pairs(Needs.adapt) do
		if math.abs(m - 1) >= 0.05 then ad[#ad+1] = string.format('%s x%.2f', k, m); end;
	end;
	table.sort(ad);
	if Sense.round % 30 == 0 then
		local ow, owv = Needs.Overwatch();
		local top, topV = nil, 0;
		for id,reg in pairs(Map.regions) do
			local v = Needs.StrategicValue(reg);
			if v > topV then top, topV = id, v; end;
		end;
		Log(string.format('Needs: strategic top region %s (%.1f), overwatch %s (%.1f), home exposed %.0f layers',
			tostring(top), topV, tostring(ow), owv or 0, Mem.homeRegion and Needs.Exposed(Map.regions[Mem.homeRegion]) or 0));
	end;
	if #ad > 0 then Log('Needs learned: '..table.concat(ad, ', ')..'; enemy AA sensed '..tostring(Needs.enemyAA)); end;
end;

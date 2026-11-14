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
	costScale = 400,	-- cost at which an option's score halves
	saving = nil,		-- {score, cost, name}: the best structure we could not afford yet
	log = {},
};

local function clamp(x) return math.max(0, math.min(1, x)); end;

function Needs.Weights()
	local w = {
		survival = 10, energy = 4, fuel = 4, income = 3, spend = 2, defense = 3,
		air = 3, planes = 1, research = 1, scouting = 1.5, builders = 3, army = 1.5,
	};
	for k,v in pairs(Cfg.needWeights or {}) do w[k] = v; end;
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
	if deep > 0 then Nudge('scouting', 1.12 ^ math.min(deep, 4)); end;
	if ground > 0 then Nudge('defense', 1.04 ^ math.min(ground, 4)); end;

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

	v.research = 0;
	if Types.lab and round >= 3 then
		local labs = Economy.Count(Types.lab);
		if labs == 0 then v.research = 0.6;
		elseif labs < Cfg.wantLabs and round >= 15 then v.research = 0.3;
		elseif minerals >= Cfg.richMinerals and labs < Cfg.wantLabsRich then v.research = 0.25; end;
	end;

	local scouts = #Sense.my.surveyor + (Sense.storedByRole.surveyor or 0) + Economy.ScannerTowers();
	v.scouting = scouts == 0 and 0.7 or (Economy.MineShortage() and 0.4 or 0.05);
	if Sense.DeepMatters() and #Sense.my.surveyor < Cfg.maxSurveyorsDeep then v.scouting = math.max(v.scouting, 0.5); end;

	local constructors = #Sense.my.constructor + (Sense.storedByRole.constructor or 0);
	local wantC = round < 40 and Cfg.constructorsEarly or Cfg.constructorsLate;
	v.builders = constructors < 2 and 1 or clamp((wantC - constructors) / wantC);

	local combat = #Sense.my.combat + #Sense.my.bomb + (Sense.storedByRole.combat or 0);
	v.army = clamp(0.35 - combat * 0.02);
	if Mem.posture == 'attack' then v.army = math.max(v.army, 0.3); end;
	v.army = math.max(v.army, v.defense * 0.8);

	Needs.v = v;
end;

-- weight of a need key (survival* share the survival weight)
local function W(w, k)
	if string.sub(k, 1, 8) == 'survival' then return w.survival; end;
	return w[k] or 1;
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
function Needs.Choose(options, what, budget)
	local best, bestS = nil, 0.05;
	local bestAny, bestAnyS = nil, 0;
	for _,o in ipairs(options) do
		o.score = Needs.Score(o.serves, o.cost) * (o.factor or 1);
		if o.score > bestAnyS then bestAny, bestAnyS = o, o.score; end;
		local affordable = budget == nil or (o.cost or 0) <= budget + 50;
		if affordable and o.score > bestS then best, bestS = o, o.score; end;
	end;
	return best, bestAny;
end;

-------------------------------------------------------------------------------
-- Structures
-------------------------------------------------------------------------------

-- Defense candidates come from Economy.DefenseNeed, which claims its pick for the round:
-- collect them without claiming, claim only what is chosen
local function DefenseOption(surplus, homeOnly)
	local saved = {};
	for k,x in pairs(Economy.scannerClaimed) do saved[k] = x; end;
	local need = Economy.DefenseNeed(surplus, homeOnly);
	Economy.scannerClaimed = saved;
	return need;
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
	add(Economy.MineReq('Fuel'), {survivalFuel = 1, fuel = 1, income = 0.3, energy = 0.3}, 'Mine (fuel)');
	if Types.generator and Economy.powerOrders < 2 then
		add({type = Types.generator, kind = 'power'}, {survivalGenerator = 1, energy = 1}, 'Generator');
	end;
	if Types.plant then
		local plants = Economy.Count(Types.plant);
		add({type = Types.plant, kind = 'base'}, {survivalPlant = 1, spend = plants < 3 and 1 or 0.2, army = plants == 0 and 1 or 0.1});
	end;
	if Types.reactor and Types.IsUnlocked(Types.reactor) and Economy.ReactorSiteReady() then
		local uranium = (max.balance.uranium or 0) > 0 or (cur.storage.uranium or 0) > 0;
		if uranium then
			add({type = Types.reactor, kind = 'power'}, {energy = 1.5, spend = 0.3});
		else
			add(Economy.MineReq('Uranium'), {energy = 0.4}, 'Mine (uranium)');
		end;
	end;
	if Types.lab then add({type = Types.lab, kind = 'base'}, {research = 1, spend = 0.3}); end;
	do
		-- the plane factory without the mineral check: saving for it is the scorer's job
		local pf = Types.planeFactory;
		if pf and Types.IsUnlocked(pf) and Economy.Count(pf) < (Cfg.planeFactories or 1) then
			if Economy.FuelMines() >= 1 then
				add({type = pf, kind = 'base'}, {planes = 1, spend = 0.3});
			end;
		end;
	end;
	-- defense: the home region first, then built-up regions, then surplus where the threat is highest
	local home = Sense.round >= 4 and DefenseOption(false, true);
	if home then add(home, {defense = 1.2, air = home.type == Types.aaTower and 1 or 0}); end;
	local reg = DefenseOption(false);
	if reg then add(reg, {defense = 0.9, air = reg.type == Types.aaTower and 1 or 0}); end;
	local extra = DefenseOption(true);
	if extra then add(extra, {defense = 0.6, spend = 0.4}); end;
	if Types.GroundTower() and constructorIndex == 1 then
		for id,_ in pairs(Sense.frontier) do
			local r = Map.regions[id];
			if r.mobileThreat > 0 and Economy.TowersIn(r) < Cfg.wantTowersPerFrontier then
				add({type = Types.GroundTower(), kind = 'tower', region = id}, {defense = 1});
				break;
			end;
		end;
	end;
	if Types.store and Sense.round >= 12 and Types.IsUnlocked(Types.store) and Economy.Count(Types.store) < Cfg.maxStores then
		local want = Economy.StoreRegion();
		if want then add({type = Types.store, kind = 'tower', region = want}, {defense = 0.3, army = 0.3, spend = 0.3}); end;
	end;
	local gt = Types.GroundTower();
	if gt then add({type = gt, kind = 'tower'}, {defense = 0.4, spend = 0.4}, gt.name..' (base)'); end;
	if Types.radarTower and Types.IsUnlocked(Types.radarTower) and Economy.Count(Types.radarTower) < 1 + math.floor(Sense.round / 25) then
		add({type = Types.radarTower, kind = 'tower'}, {scouting = 0.6, defense = 0.2});
	end;
	return opts;
end;

-- Personalities may adjust options here (veto, boost)
function Needs.AdjustStructureOptions(opts) return opts; end;

function Needs.NextStructure(constructorIndex)
	local cur = Economy.Resources();
	local minerals = cur.storage.minerals or 0;
	local opts = Needs.AdjustStructureOptions(Needs.StructureOptions(constructorIndex));
	local best, top = Needs.Choose(opts, 'structure', minerals);
	-- saving up: the top option is out of reach, and nothing affordable comes close
	if top and top ~= best and (best == nil or best.score < top.score * 0.6) then
		Needs.saving = {score = top.score, cost = top.cost, name = top.name, round = Sense.round};
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
	Needs.Note(string.format('build %s (%.2f)', best.name, best.score));
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
	add(kit.Constructor(), {builders = 1, income = 0.2});
	add(kit.Tank(), {defense = 1, army = 1});
	add(Types.Vehicle(Types.turret.aa, Types.base.hover), {air = 1});
	add(Types.Vehicle(Types.turret.surveyor, Types.base.heavy) or Types.Vehicle(Types.turret.surveyor, Types.base.hover), {scouting = 1});
	local combat = #Sense.my.combat;
	if #Sense.my.radar < 1 + math.floor(combat / Cfg.radarPerCombat) then
		add(Types.Vehicle(Types.turret.radar, Types.base.hover), {scouting = 0.5, defense = 0.2});
	end;
	return opts;
end;

function Needs.AdjustVehicleOptions(opts) return opts; end;

function Needs.NextVehicle(kit)
	local cur = Economy.Resources();
	local minerals = cur.storage.minerals or 0;
	local opts = Needs.AdjustVehicleOptions(Needs.VehicleOptions(kit));
	local best = Needs.Choose(opts, 'vehicle', minerals);
	if best == nil then return nil; end;
	-- the constructors are saving for something that matters more: hold back
	local s = Needs.saving;
	if s and Sense.round - s.round <= 1 and best.score < s.score * 0.8 and minerals - best.cost < s.cost then
		return nil;
	end;
	Needs.Note(string.format('produce %s (%.2f)', best.name, best.score));
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
	local keys = {};
	for k,_ in pairs(Needs.v) do keys[#keys+1] = k; end;
	table.sort(keys);
	local parts = {};
	for _,k in ipairs(keys) do
		if Needs.v[k] > 0 then parts[#parts+1] = string.format('%s %.2f', k, Needs.v[k]); end;
	end;
	Log('Needs: '..table.concat(parts, ', '));
	local ad = {};
	for k,m in pairs(Needs.adapt) do
		if math.abs(m - 1) >= 0.05 then ad[#ad+1] = string.format('%s x%.2f', k, m); end;
	end;
	table.sort(ad);
	if #ad > 0 then Log('Needs learned: '..table.concat(ad, ', ')..'; enemy AA sensed '..tostring(Needs.enemyAA)); end;
end;

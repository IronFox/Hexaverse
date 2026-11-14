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
		air = 3, planes = 1, research = 2.5, scouting = 1.5, builders = 3, army = 1.5,
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
		-- what we want most is still locked: research is the way to it
		local pfw = Types.planeFactory;
		if pfw and not Types.IsUnlocked(pfw) and Needs.Weights().planes >= 3 and round >= 8 then
			v.research = math.max(v.research, labs == 0 and 1 or 0.5);
		end;
		-- sitting on minerals with no lab: research is the best use
		if labs == 0 and minerals >= 300 and round >= 8 then v.research = math.max(v.research, 0.9); end;
	end;

	local scouts = #Sense.my.surveyor + (Sense.storedByRole.surveyor or 0) + Economy.ScannerTowers();
	v.scouting = scouts == 0 and 0.7 or (Economy.MineShortage() and 0.4 or 0.05);
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
		if f.name ~= 'Nexus' and f ~= Game.myFaction and f.isHostile ~= false then player = true; end;
	end;
	-- can drones cover my land? (a plane factory, drones researched, and enough of them flying)
	local drones = 0;
	for _,p in ipairs(Sense.my.plane or {}) do if p.type == Types.drone then drones = drones + 1; end; end;
	local canDrones = Types.drone and Types.IsUnlocked(Types.drone) and Types.planeFactory and Economy.Count(Types.planeFactory) > 0;
	Needs.dronesShort = not canDrones or drones < math.ceil(own / 4);
	Needs.deepRisk = Sense.DeepMatters() and 1 or ((player and Sense.round >= 30) and 0.35 or 0.1);

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
	Needs.Note('order for '..name..' came to nothing ('..f.n..'x): weighing it down');
end;

-- A structure order was placed: the pending choice worked out
function Needs.Started()
	Needs.pending = nil;
end;

function Needs.Choose(options, what, budget)
	local best, bestS = nil, Needs.minScore;
	local bestAny, bestAnyS = nil, 0;
	for _,o in ipairs(options) do
		o.score = Needs.Score(o.serves, o.cost) * (o.factor or 1) * Needs.Have(o.have, o.cap, o.swarm) * Needs.FailFactor(o.name);
		if o.score > bestAnyS then bestAny, bestAnyS = o, o.score; end;
		local affordable = budget == nil or (o.cost or 0) <= budget + 50;
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
	add(Economy.MineReq('Fuel'), {survivalFuel = 1, fuel = 1, income = 0.3, energy = 0.3}, 'Mine (fuel)');
	if Types.generator and Economy.powerOrders < 2 then
		add({type = Types.generator, kind = 'power'}, {survivalGenerator = 1, energy = 1}, 'Generator');
	end;
	if Types.plant then
		local plants = Economy.Count(Types.plant);
		add({type = Types.plant, kind = 'base'}, {survivalPlant = 1, spend = 1, army = 0.3});
		opts[#opts].have = plants;
	end;
	if Types.reactor and Types.IsUnlocked(Types.reactor) and Economy.ReactorSiteReady() then
		local uranium = (max.balance.uranium or 0) > 0 or (cur.storage.uranium or 0) > 0;
		if uranium then
			add({type = Types.reactor, kind = 'power'}, {energy = 1.5, spend = 0.3});
		else
			add(Economy.MineReq('Uranium'), {energy = 0.4}, 'Mine (uranium)');
		end;
	end;
	if Types.lab then
		add({type = Types.lab, kind = 'base'}, {research = 1, spend = 0.3});
		opts[#opts].have = Economy.Count(Types.lab);
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
		if want then
			add({type = Types.store, kind = 'tower', region = want, peak = peak},
				{defense = peak and 0.8 or 0.3, army = 0.3, spend = 0.3}, peak and ('Store on peak @'..want) or nil);
			opts[#opts].have = Economy.Count(Types.store);
		end;
	end;
	return opts;
end;

-- Personalities may adjust options here (veto, boost)
function Needs.AdjustStructureOptions(opts) return opts; end;

function Needs.NextStructure(constructorIndex)
	-- the last choice never turned into an order
	if Needs.pending then Needs.Failed(Needs.pending); Needs.pending = nil; end;
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
	if best.key then
		Needs.ordered[best.key] = Needs.ordered[best.key] or {};
		table.insert(Needs.ordered[best.key], Sense.round);
	end;
	Needs.Note(string.format('build %s (%.2f)', best.name, best.score));
	Needs.pending = best.name;
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
	add(kit.Constructor(), {builders = 1, income = 0.2});	-- the builders need already counts them
	local combat = #Sense.my.combat + #Sense.my.bomb;
	-- tanks: as many as the action can use (the attack target's required force, or the
	-- pressure at home), counted in groups of four. Beyond that only a swarm floor:
	-- meat shields and sensor coverage through numbers
	local tank = kit.Tank();
	add(tank, {defense = 1, army = 1, scouting = 0.15});
	if tank and #opts > 0 then
		local o = opts[#opts];
		local pot = Military.UnitPotential();
		local use = math.ceil(Sense.Pressure() / pot) + 2 * #Needs.DefendedRegions();
		local target = Mem.attackTarget or Military.ChooseAttackTarget();
		if target then use = use + Military.RequiredForce(target); end;
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
	add(Types.Vehicle(Types.turret.surveyor, Types.base.heavy) or Types.Vehicle(Types.turret.surveyor, Types.base.hover), {scouting = 1});
	if #opts > 0 then opts[#opts].have = have('surveyor'); end;
	add(Types.Vehicle(Types.turret.radar, Types.base.hover), {scouting = 0.5, defense = 0.3});
	if #opts > 0 then opts[#opts].have = math.floor(have('radar') * Cfg.radarPerCombat / math.max(1, combat)); end;
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

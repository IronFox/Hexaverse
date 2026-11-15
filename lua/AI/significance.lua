--[[
Strategic significance of regions
=================================

One number per region: how much it matters to hold it. The scripts carry out orders; this
judges where they are worth carrying out (where to defend, which sites to take first, where
to send the army, which port to go for).

How it works
------------
* Every few rounds each region is described by a fixed list of numbers (Significance.names):
  what lies in it, how high it is, how far from me and from the enemy, what I and they have
  there, and the same for its neighbours.
* A small network turns the lists into a few values per region (0..1, Significance.outputs):
  worth holding, worth taking, dangerous. It is the hex-sphere counterpart of a convolution:
  every layer applies the same weights at every region, to the region itself and to the
  average of its neighbours, so each layer lets information travel one region further.
  Without trained weights a hand-made estimate stands in, so the values are usable from
  the start and give training something to beat.
* Units decide by reading the values of the regions within their reach
  (Significance.Best), so the network runs once per evaluation however many units there are.
* The lists are logged ("Regions r=N: ...") together with who holds what. A trainer outside
  the game learns from many matches which regions, held or lost, were followed by the reward
  going up or down, and writes new weights.

Weights
-------
Significance.net = {layers = {{ws = {{...}, ...}, wn = {{...}, ...}, b = {...}}, ...}}
(ws: weights for the region itself, wn: for the mean of its neighbours; rows are outputs,
columns inputs.) Hidden layers use tanh, the last one a logistic curve with one output per
entry of Significance.outputs. Set by a variant script, e.g.
	--#require "player.lua"
	Significance.net = { ... };
]]

Significance = {
	every = 10,			-- rounds between evaluations (and log lines); 0 turns it off
	log = true,			-- write the per-region lists to the log
	value = {},			-- region id -> 0..1: worth holding (the first output)
	out = {},			-- region id -> {hold, take, danger}
	outputs = {'hold', 'take', 'danger'},
	features = {},		-- region id -> list of numbers
	static = {},		-- region id -> what rarely changes (resources, heights)
	ports = {},			-- region id -> 'mine' / 'other': Nexus ports seen there
	net = nil,
	names = {
		'minerals', 'fuel', 'height', 'heightVsNeighbors', 'roughness', 'land',
		'hopsHome', 'hopsMine', 'hopsEnemy', 'degree',
		'myStructures', 'myMobiles', 'myStrength', 'myMines', 'held',
		'mobileThreat', 'staticThreat', 'airThreat', 'hostileStructures', 'deaths',
		'seenNow', 'unseenFor', 'portMine', 'portOther', 'isHome', 'round',
	},
};

local function Squash(x, k)		-- 0.. -> 0..1, half at k
	if x == nil or x <= 0 then return 0; end;
	return x / (x + k);
end;

local function Clamp01(x)
	if x ~= x then return 0; end;	-- NaN
	if x < 0 then return 0; elseif x > 1 then return 1; end;
	return x;
end;

--[[
What rarely changes: resources known in the region and the shape of its ground. Refreshed
now and then (prospecting reveals deposits, digging and bombs change the ground).
]]
function Significance.Static(reg)
	local s = Significance.static[reg.id];
	if s ~= nil and Sense.round - s.round < 30 then return s; end;
	s = {round = Sense.round, minerals = 0, fuel = 0, height = 0, rough = 0, land = 0};
	local n, hs, hn, hsum, hsq = #reg.cells, 0, 0, 0, 0;
	for _,cell in ipairs(reg.cells) do
		local okm, m = pcall(Game.Grid.GetNaturalResourceAt, 'Minerals', cell);
		if okm and m and m > 0 then s.minerals = s.minerals + m; end;
		local okf, f = pcall(Game.Grid.GetNaturalResourceAt, 'Fuel', cell);
		if okf and f and f > 0 then s.fuel = s.fuel + f; end;
		local g = Score and Score.ground and Score.ground[cell.index];
		if g and g.h then
			hn = hn + 1;
			hsum = hsum + g.h;
			hsq = hsq + g.h * g.h;
		end;
	end;
	if hn > 0 then
		s.height = hsum / hn;
		s.rough = math.sqrt(math.max(0, hsq / hn - s.height * s.height));
	end;
	s.land = n > 0 and hn / n or 0;
	s.known = hn > 0;
	Significance.static[reg.id] = s;
	return s;
end;

-- Nexus ports are structures like any other: remembered per region once seen
function Significance.Ports()
	local function look(list, whose)
		for _,u in pairs(list or {}) do
			local name = u.type and u.type.name;
			if name and string.find(name, 'Nexus Port', 1, true) and u.location then
				local reg = Map.RegionOfLoc(u.location);
				if reg then Significance.ports[reg.id] = whose; end;
			end;
		end;
	end;
	look(Game.hostileUnits, 'other');
	look(Sense.myStructures, 'mine');
end;

function Significance.Describe()
	local round = Sense.round;
	Significance.Ports();
	local homeHops = Mem.homeRegion and Map.Hops({Mem.homeRegion}) or {};
	local enemyHops = #Sense.enemyRegions > 0 and Map.Hops(Sense.enemyRegions) or {};
	local held = Score and Score.heldByRegion or {};
	local mines = {};
	for _,u in ipairs(Sense.myStructures or {}) do
		if u.type and u.type.name and string.find(u.type.name, 'Mine', 1, true) then
			local reg = Map.RegionOfLoc(u.location);
			if reg then mines[reg.id] = (mines[reg.id] or 0) + 1; end;
		end;
	end;
	-- first pass: the region itself
	local own = {};
	Significance.nbThreat = {};
	for _,reg in ipairs(Map.regions) do
		local s = Significance.Static(reg);
		local port = Significance.ports[reg.id];
		own[reg.id] = {
			minerals = Squash(s.minerals, 10),
			fuel = Squash(s.fuel, 10),
			height = s.height,
			rough = Squash(s.rough, 2),
			land = s.land,
			myStrength = Squash(reg.myStrength, 10),
			threat = Squash((reg.mobileThreat or 0) + (reg.staticThreat or 0), 10),
			hostileStructures = Squash(reg.hostileStructures, 3),
			port = port and 1 or 0,
		};
	end;
	for _,reg in ipairs(Map.regions) do
		local o = own[reg.id];
		local nb = {minerals = 0, fuel = 0, myStrength = 0, threat = 0, hostileStructures = 0, port = 0, height = 0};
		local k = 0;
		for id,_ in pairs(reg.neighbors) do
			local p = own[id];
			if p then
				k = k + 1;
				for key,_ in pairs(nb) do nb[key] = nb[key] + p[key]; end;
			end;
		end;
		if k > 0 then for key,x in pairs(nb) do nb[key] = x / k; end; end;
		local port = Significance.ports[reg.id];
		local never = reg.lastSeen == nil or reg.lastSeen < 0;
		Significance.features[reg.id] = {
			o.minerals, o.fuel,
			Clamp01(0.5 + o.height / 20),
			Clamp01(0.5 + (k > 0 and (o.height - nb.height) or 0) / 6),
			o.rough, o.land,
			Clamp01((homeHops[reg.id] or 8) / 8),
			Clamp01((Sense.hops[reg.id] or 8) / 8),
			Clamp01((enemyHops[reg.id] or 8) / 8),
			Clamp01(k / 8),
			Squash(reg.myStructures, 3), Squash(reg.myMobiles, 3), o.myStrength,
			Squash(mines[reg.id], 2),
			Clamp01((held[reg.id] or 0) / math.max(1, #reg.cells)),
			Squash(reg.mobileThreat, 10), Squash(reg.staticThreat, 10), Squash(reg.airThreat, 10),
			o.hostileStructures, Squash(reg.deaths, 3),
			reg.seenNow and 1 or 0,
			never and 1 or Clamp01((round - reg.lastSeen) / 50),
			port == 'mine' and 1 or 0, port == 'other' and 1 or 0,
			reg.id == Mem.homeRegion and 1 or 0,
			Clamp01(round / 300),
		};
		Significance.nbThreat[reg.id] = nb.threat;
	end;
end;

--[[
The stand-in while nothing is trained.
  hold    my base, my mines, my ports, and resources close to what I have
  take    resources and ports that are not mine yet, and what the enemy has built, the
          closer the better
  danger  what threatens there and next door
]]
function Significance.Estimate(reg, f)
	local F = Significance.index;
	local near = 1 - f[F.hopsMine];
	local mine = math.max(f[F.myStructures], f[F.isHome]);
	local res = math.max(f[F.minerals], 0.6 * f[F.fuel]);
	local hold = 0.35 * mine + 0.15 * f[F.myMines] + 0.25 * f[F.portMine]
		+ 0.20 * res * (0.4 + 0.6 * near) + 0.05 * f[F.heightVsNeighbors] * near;
	local take = (0.40 * res * (1 - f[F.myMines]) + 0.35 * f[F.portOther] + 0.25 * f[F.hostileStructures])
		* (0.3 + 0.7 * near) * (1 - 0.5 * mine);
	local danger = 0.6 * math.max(f[F.mobileThreat], f[F.staticThreat], 0.7 * f[F.airThreat])
		+ 0.3 * (Significance.nbThreat[reg.id] or 0) + 0.1 * f[F.deaths];
	return {Clamp01(hold), Clamp01(take), Clamp01(danger)};
end;

-- all regions at once: layer by layer, each region from itself and the mean of its neighbours
function Significance.Forward(net)
	local x = {};
	for _,reg in ipairs(Map.regions) do x[reg.id] = Significance.features[reg.id]; end;
	local count = #net.layers;
	for li,layer in ipairs(net.layers) do
		local width = #layer.ws[1];
		local mean = {};
		for _,reg in ipairs(Map.regions) do
			local m, k = {}, 0;
			for i = 1,width do m[i] = 0; end;
			for id,_ in pairs(reg.neighbors) do
				local xn = x[id];
				if xn then
					k = k + 1;
					for i = 1,width do m[i] = m[i] + (xn[i] or 0); end;
				end;
			end;
			if k > 1 then for i = 1,width do m[i] = m[i] / k; end; end;
			mean[reg.id] = m;
		end;
		local y = {};
		for _,reg in ipairs(Map.regions) do
			local xs, xn, out = x[reg.id], mean[reg.id], {};
			for j,row in ipairs(layer.ws) do
				local rn = layer.wn[j];
				local sum = layer.b[j] or 0;
				for i = 1,width do
					sum = sum + (row[i] or 0) * (xs[i] or 0) + (rn[i] or 0) * xn[i];
				end;
				if li < count then
					local e = math.exp(-2 * sum);
					out[j] = (1 - e) / (1 + e);	-- tanh
				else
					out[j] = 1 / (1 + math.exp(-sum));
				end;
			end;
			y[reg.id] = out;
		end;
		x = y;
	end;
	return x;
end;

function Significance.Update()
	if Significance.index == nil then
		Significance.index = {};
		for i,name in ipairs(Significance.names) do Significance.index[name] = i; end;
	end;
	if (Significance.every or 0) <= 0 then return; end;
	if Sense.round % Significance.every ~= 0 or Significance.lastRound == Sense.round then return; end;
	Significance.lastRound = Sense.round;
	local timer = Game.MeasureSecondsFromNow and Game.MeasureSecondsFromNow();
	Significance.Describe();
	local net = Significance.net;
	if net and not Significance.netChecked then
		Significance.netChecked = true;
		local first, last = net.layers and net.layers[1], net.layers and net.layers[#net.layers];
		if not (first and first.ws and first.wn and first.ws[1] and #first.ws[1] == #Significance.names
				and last.ws and #last.ws == #Significance.outputs) then
			Log('Significance: weights do not fit ('..#Significance.names..' inputs, '..#Significance.outputs..' outputs). Using the estimate');
			Significance.net = nil;
			net = nil;
		end;
	end;
	local all = net and Significance.Forward(net) or nil;
	local top, topV = nil, -1;
	for _,reg in ipairs(Map.regions) do
		local o = all and all[reg.id] or Significance.Estimate(reg, Significance.features[reg.id]);
		for i = 1,#Significance.outputs do
			if o[i] == nil or o[i] ~= o[i] then o[i] = 0; end;
		end;
		Significance.out[reg.id] = o;
		Significance.value[reg.id] = o[1];
		if o[1] > topV then top, topV = reg.id, o[1]; end;
	end;
	if Significance.log then
		if not Significance.namesLogged then
			Significance.namesLogged = true;
			Log('Region inputs: '..table.concat(Significance.names, ','));
			Log('Region outputs: '..table.concat(Significance.outputs, ','));
			local parts = {};
			for _,reg in ipairs(Map.regions) do
				local nb = {};
				for id,_ in pairs(reg.neighbors) do nb[#nb+1] = id; end;
				table.sort(nb);
				parts[#parts+1] = reg.id..':'..#reg.cells..':'..table.concat(nb, ',');
			end;
			Log('Region graph (id:cells:neighbours): '..table.concat(parts, ';'));
		end;
		-- one line: id:holder:outputs:inputs (holder m = mine, e = enemy seen there, - = neither)
		local parts = {};
		for _,reg in ipairs(Map.regions) do
			local function fmt(list)
				local nums = {};
				for i,v in ipairs(list) do
					-- (always a decimal point, whatever the system's locale writes)
					nums[i] = (v == 0 and '0') or (v == 1 and '1') or (string.gsub(string.format('%.2f', v), ',', '.'));
				end;
				return table.concat(nums, ',');
			end;
			local holder = (Sense.mine[reg.id] and 'm') or ((reg.hostileStructures > 0 or reg.staticThreat > 0) and 'e') or '-';
			parts[#parts+1] = reg.id..':'..holder..':'..fmt(Significance.out[reg.id])..':'..fmt(Significance.features[reg.id]);
		end;
		Log('Regions r='..Sense.round..': '..table.concat(parts, ';'));
	end;
	Significance.Direct();
	Log(string.format('Significance: %s, %d regions, highest region %s (%.2f)%s',
		net and 'network' or 'estimate', #Map.regions, tostring(top), topV,
		timer and string.format(', %.3fs', timer()) or ''));
end;

-- for the scripts: 0..1, or the given default before the first evaluation
function Significance.Of(regOrId, output, default)
	local id = type(regOrId) == 'table' and regOrId.id or regOrId;
	local o = id and Significance.out[id];
	if o == nil then return default or 0; end;
	local k = 1;
	for i,name in ipairs(Significance.outputs) do
		if name == output then k = i; end;
	end;
	return o[k] or default or 0;
end;

--[[
For a unit: the region within maxHops of its own that scores best, where score(reg, hops) is
the caller's judgement built from Significance.Of (e.g. take minus danger, less per hop).
Returns the region and its score, or nil.
]]
function Significance.Best(fromReg, maxHops, score)
	if fromReg == nil then return nil; end;
	local hops = Map.Hops({fromReg.id});
	local best, bestV = nil, nil;
	for _,reg in ipairs(Map.regions) do
		local h = hops[reg.id];
		if h and h <= maxHops then
			local v = score(reg, h);
			if v ~= nil and (bestV == nil or v > bestV) then best, bestV = reg, v; end;
		end;
	end;
	return best, bestV;
end;


--[[
Directions: what the strategic layer hands down
-----------------------------------------------
The layers below (today the scripts, later trained ones) do not read raw values, they get
directions. The format is fixed, so that either side can be retrained without the other
having to learn a new language:

	{kind = 'take' | 'hold' | 'avoid', region = id, priority = 0..1, issued = round}

  take    get this region: build there, or drive out who holds it
  hold    keep this region: defend it, rebuild what is lost there
  avoid   stay out unless ordered in (no building, no lone units)

A few of each at a time (Significance.maxDirections). A direction stays until it is settled:
  take    achieved when the region is mine; failed after Significance.takeRounds
  hold    achieved when still mine after Significance.holdRounds; failed when lost before
  avoid   just lapses with the next evaluation
Both the direction and how it ended are logged ("Direction: ...", "Direction result: ..."),
with what it cost (units lost there). That is the score of the layers below: was the
direction carried out, how fast, at what price. The scripts follow directions only with
Cfg.useDirections; they are issued and settled either way, so matches without it show what
happens to directions nobody acts on.
]]
Significance.maxDirections = 3;
Significance.takeRounds = 40;
Significance.holdRounds = 30;
Significance.directions = {};	-- active ones
Significance.avoid = {};		-- region id -> priority
Significance.failed = {};		-- region id -> round a take there last failed
Significance.taken = {};		-- region id -> round a formation reported it taken (formation.lua)

local function Settle(d, how)
	local reg = Map.regions[d.region];
	Log(string.format('Direction result: %s region %d from round %d: %s at round %d, losses %d',
		d.kind, d.region, d.issued, how, Sense.round, math.max(0, (reg.lost or 0) - d.deaths)));
end;

function Significance.Direct()
	-- settle what is due
	local keep, busy = {}, {};
	for _,d in ipairs(Significance.directions) do
		local mine = Sense.mine[d.region] and true or false;
		local age = Sense.round - d.issued;
		local taken = Significance.taken[d.region];
		-- (a formation needs time to gather before it goes in)
		local limit = Significance.takeRounds * (Cfg.useFormations and 2 or 1);
		if d.kind == 'take' and (mine or (taken and taken >= d.issued)) then Settle(d, 'achieved');
		elseif d.kind == 'take' and age >= limit then
			Settle(d, 'failed');
			Significance.failed[d.region] = Sense.round;
		elseif d.kind == 'hold' and not mine then Settle(d, 'failed');
		elseif d.kind == 'hold' and age >= Significance.holdRounds then Settle(d, 'achieved');
		else
			keep[#keep+1] = d;
			busy[d.region] = true;
		end;
	end;
	Significance.directions = keep;
	-- issue new ones, best first
	local count = {take = 0, hold = 0};
	for _,d in ipairs(keep) do count[d.kind] = count[d.kind] + 1; end;
	local cand = {};
	Significance.avoid = {};
	for _,reg in ipairs(Map.regions) do
		local o = Significance.out[reg.id];
		if o and not busy[reg.id] then
			if Sense.mine[reg.id] then
				-- what is worth much and under threat comes first
				cand[#cand+1] = {kind = 'hold', region = reg.id, priority = o[1] * (0.5 + 0.5 * o[3])};
			else
				-- (what just failed is not tried again at once: something else first)
				local again = Significance.failed[reg.id];
				local damp = (again and Sense.round - again < 100) and 0.3 or 1;
				cand[#cand+1] = {kind = 'take', region = reg.id, priority = o[2] * (1 - 0.5 * o[3]) * damp};
			end;
		end;
		if o and not Sense.mine[reg.id] and o[3] >= 0.6 then Significance.avoid[reg.id] = o[3]; end;
	end;
	table.sort(cand, function(a, b)
		if a.priority ~= b.priority then return a.priority > b.priority; end;
		return a.region < b.region;
	end);
	for _,d in ipairs(cand) do
		if count[d.kind] < Significance.maxDirections and d.priority >= 0.1 then
			count[d.kind] = count[d.kind] + 1;
			d.issued = Sense.round;
			d.deaths = Map.regions[d.region].lost or 0;
			keep[#keep+1] = d;
			Log(string.format('Direction: %s region %d priority %s (round %d)', d.kind, d.region,
				(string.gsub(string.format('%.2f', d.priority), ',', '.')), d.issued));
		end;
	end;
end;

-- priority of the active direction of this kind for a region, or 0
function Significance.Directed(regOrId, kind)
	local id = type(regOrId) == 'table' and regOrId.id or regOrId;
	if id == nil then return 0; end;
	if kind == 'avoid' then return Significance.avoid[id] or 0; end;
	for _,d in ipairs(Significance.directions) do
		if d.region == id and d.kind == kind then return d.priority; end;
	end;
	return 0;
end;

-- the active directions of a kind, highest priority first
function Significance.Directions(kind)
	local out = {};
	for _,d in ipairs(Significance.directions) do
		if d.kind == kind then out[#out+1] = d; end;
	end;
	table.sort(out, function(a, b) return a.priority > b.priority end);
	return out;
end;

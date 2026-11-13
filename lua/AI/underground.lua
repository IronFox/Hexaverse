--[[
Hexaverse AI player: underground personality
=============================================

Plays like player.lua, but retreats underground right away:
* The first terraformer digs a shaft down Cfg.moleDepth layers next to the base,
  then a cavern around the shaft's bottom. Later it tunnels to mine sites.
* All construction happens in the cavern. Sensors can't see through rock, and
  the Nexus does not dig, so a base below ground is hard to find and to reach.
* Surface structures are destroyed once the cavern holds a replacement
  (generators and mines only once the underground ones carry the load).
* Mines may go beneath hostile surface mines ("undermining"). Nexus hubs can't
  be undermined.
* Surveyors and terraformers work together: a surveyor accompanies the digger,
  and the digger never digs into unknown terrain or next to lava.
* Fights underground favor bomb vehicles over tanks.

Select it with Rules/AIPlayers/script = 'underground' (or in a list,
e.g. 'player,underground,air').
]]

--#require "player.lua"

Cfg.logPrefix = 'Underground AI: ';
Cfg.moleDepth = 3;			-- layers below the local surface for the cavern floor
Cfg.moleRings = 2;			-- initial cavern radius in cells around the shaft (1 + 6 + 12 cells)
Cfg.moleMaxRings = 4;		-- expanded when there is no building space left
Cfg.bombShare = 0.5;		-- underground, blasts beat range
Cfg.minesBelowThreat = true;
Cfg.attackMinRound = 80;	-- defend first, attack late
Cfg.roadMinRound = 100000;	-- no roads: they mark the way to the base
Cfg.constructorsEarly = math.max(Cfg.constructorsEarly or 3, 3);
Cfg.planeFactories = 0;		-- planes can't take off from a cave

-- radar and AA towers see nothing through rock: none below ground. Scanner towers do
local baseResolve = Types.Resolve;
function Types.Resolve()
	local first = not Types.resolved;
	baseResolve();
	if first then
		Types.radarTower = nil;
		Types.aaTower = nil;
		if Types.towerByTurret then
			Types.towerByTurret['Radar'] = nil;
			Types.towerByTurret['Anti Aircraft'] = nil;
		end;
	end;
end;

Mole = {
	phase = 'shaft',	-- 'shaft' -> 'cavern' -> 'live'
	shaftCell = nil,	-- unit cell index of the shaft
	level = nil,		-- height of the cavern floor (unit heights)
	digger = nil,		-- id of the digging terraformer
	tunnelGoal = nil,	-- cell index the digger tunnels towards
	rings = Cfg.moleRings,
	waitSurvey = nil,	-- world position a surveyor should look at before we dig on
};

local function D2(a,b) local x,y,z = a.x-b.x, a.y-b.y, a.z-b.z; return x*x+y*y+z*z; end;

local function CellByIndex(idx)
	return Game.Grid.unitCells[idx];
end;

-- Is a location below ground (at or below the cavern level)?
function Mole.IsBelow(loc)
	return Mole.level ~= nil and loc ~= nil and not loc.isSuborbital and loc.height <= Mole.level + 1;
end;

--[[
Terrain check before digging into (cell, height): never next to lava, and never
into unknown terrain (wait for a surveyor to look). Returns 'ok', 'lava' or 'unknown'.
]]
function Mole.DigCheck(cell, h)
	if not (Game.Terrain and Game.Terrain.TranslateUnitCell and Game.Terrain.GetContent) then return 'ok'; end;
	local C = Game.Terrain.Content or {};
	local ok, tc = pcall(Game.Terrain.TranslateUnitCell, cell);
	if not ok or tc == nil then return 'ok'; end;
	local cells = {tc};
	for _,n in ipairs(tc.neighbors or {}) do cells[#cells+1] = n; end;
	local unknown = false;
	for i,c in ipairs(cells) do
		for dh = -1, 2 do
			local ok2, content = pcall(Game.Terrain.GetContent, c, h + dh);
			if ok2 and content then
				if C.Lava and content == C.Lava then return 'lava'; end;
				if i == 1 and C.Unknown and content == C.Unknown and dh >= 0 and dh <= 1 then unknown = true; end;
			end;
		end;
	end;
	return unknown and 'unknown' or 'ok';
end;

-- Cells within n rings of a cell
function Mole.Rings(center, n)
	local out, seen, frontier = {center}, {[center.index]=true}, {center};
	for _ = 1, n do
		local nextF = {};
		for _,c in ipairs(frontier) do
			for _,nb in ipairs(c.neighbors) do
				if not seen[nb.index] then
					seen[nb.index] = true;
					out[#out+1] = nb;
					nextF[#nextF+1] = nb;
				end;
			end;
		end;
		frontier = nextF;
	end;
	return out;
end;

function Mole.LocAt(cell)
	local ok, loc = pcall(Game.Grid.ToLocation, cell, Mole.level);
	if ok then return loc; end;
	return nil;
end;

function Mole.IsOpen(cell)
	local loc = Mole.LocAt(cell);
	if loc == nil then return false; end;
	local ok, clear = pcall(Game.Grid.IsClear, loc);
	return ok and clear;
end;

function Mole.CavernCells()
	local center = Mole.shaftCell and CellByIndex(Mole.shaftCell);
	if center == nil then return {}; end;
	local n = 0;
	local open = {};
	for _,c in ipairs(Mole.Rings(center, Mole.rings)) do
		if Mole.IsOpen(c) then n = n + 1; open[#open+1] = c; end;
	end;
	return open, n;
end;

-- Dig one step from the digger towards a target cell (at cavern level). Returns true if something was scheduled
function Mole.DigToward(u, targetCell)
	local here = u.location.cell;
	if here.index == targetCell.index then return false; end;
	local best, bd = nil, nil;
	local goal = targetCell.center;
	for _,nb in ipairs(here.neighbors) do
		local d = D2(nb.center, goal);
		if bd == nil or d < bd then best, bd = nb, d; end;
	end;
	if best == nil then return false; end;
	local loc = Mole.LocAt(best);
	if loc == nil then return false; end;
	if Mole.IsOpen(best) then
		local ok, path = pcall(u.FindShortestPath, loc, nil, true);
		if ok and path and #path > 0 then u.DirectTo(path[#path]); return true; end;
		return false;
	end;
	local check = Mole.DigCheck(best, Mole.level);
	if check == 'lava' then
		Log('Lava next to cell '..best.index..': not digging there');
		Economy.Blacklist(best.index, 200);
		Mole.tunnelGoal = nil;
		return false;
	elseif check == 'unknown' then
		Mole.waitSurvey = best.center;
		return false;
	end;
	Mole.waitSurvey = nil;
	local ok, err = pcall(u.DigHorizontally, loc);
	if not ok then Log('DigHorizontally failed: '..tostring(err)); return false; end;
	return true;
end;

function Mole.Digger()
	for _,u in ipairs(Sense.my.constructor) do
		if u.id == Mole.digger and u.health > 0 then return u; end;
	end;
	for _,u in ipairs(Sense.my.constructor) do
		if u.turret and u.turret.type.canRemoveTerrain then
			Mole.digger = u.id;
			return u;
		end;
	end;
	return nil;
end;

function Mole.Update()
	local u = Mole.Digger();
	if u == nil or not Move.IsIdle(u) then return; end;

	if Mole.phase == 'shaft' then
		if Mole.shaftCell == nil then
			Mole.shaftCell = u.location.cell.index;
			Mole.level = u.location.height - Cfg.moleDepth;
			Log('Digging in: shaft at cell '..Mole.shaftCell..' down to height '..Mole.level);
		end;
		if u.location.cell.index ~= Mole.shaftCell then
			local loc = Move.GroundAt(u, CellByIndex(Mole.shaftCell));
			if loc then Move.Towards(u, loc.world, loc.cell, false, false); end;
			return;
		end;
		if u.location.height > Mole.level then
			local check = Mole.DigCheck(u.location.cell, u.location.height - 2);
			if check == 'lava' then
				-- lava below: settle for the depth reached
				Mole.level = u.location.height;
				Log('Lava below: cavern at height '..Mole.level);
			elseif check == 'unknown' and #Sense.my.surveyor > 0 then
				Mole.waitSurvey = u.location.world;
				return;
			else
				Mole.waitSurvey = nil;
				local ok, err = pcall(u.DigDown);
				if not ok then Log('DigDown failed: '..tostring(err)); Mole.level = u.location.height; end;
				return;
			end;
		end;
		Mole.phase = 'cavern';
		Log('Shaft done at height '..Mole.level..': digging the cavern');
	end;

	-- cavern: open the next closed cell next to an open one, ring by ring
	local center = CellByIndex(Mole.shaftCell);
	local open = {};
	local want = Mole.Rings(center, Mole.rings);
	local closed = nil;
	for _,c in ipairs(want) do
		if Mole.IsOpen(c) then open[c.index] = true; end;
	end;
	open[Mole.shaftCell] = true;	-- the digger stands at the bottom of the shaft
	for _,c in ipairs(want) do
		if not open[c.index] then
			local bl = Mem.blacklist[c.index];
			if not (bl and bl > Sense.round) then
				for _,nb in ipairs(c.neighbors) do
					if open[nb.index] then closed = c; break; end;
				end;
			end;
		end;
		if closed then break; end;
	end;
	if closed then
		Mole.DigToward(u, closed);
		return;
	end;
	if Mole.phase == 'cavern' then
		Mole.phase = 'live';
		Log('Cavern ready ('..#want..' cells): building below ground');
	end;
	-- live: tunnel to a requested mine site, otherwise widen the cavern when space runs out
	if Mole.tunnelGoal then
		local cell = CellByIndex(Mole.tunnelGoal);
		if cell == nil or Mole.IsOpen(cell) or not Mole.DigToward(u, cell) then
			if cell and Mole.IsOpen(cell) then Log('Tunnel reached cell '..Mole.tunnelGoal); end;
			Mole.tunnelGoal = nil;
		end;
		return;
	end;
	if Mole.noSpace and Mole.rings < Cfg.moleMaxRings then
		Mole.rings = Mole.rings + 1;
		Mole.noSpace = false;
		Log('Widening the cavern to '..Mole.rings..' rings');
	end;
end;

-- Demolish surface structures once the cavern holds replacements. One per round
function Mole.Demolish()
	if Mole.phase ~= 'live' then return; end;
	local below, above = {}, {};
	for _,s in ipairs(Sense.myStructures) do
		local tn = s.type.name;
		if Mole.IsBelow(s.location) then
			below[tn] = (below[tn] or 0) + 1;
		else
			above[tn] = above[tn] or {};
			table.insert(above[tn], s);
		end;
	end;
	local powerBelow = (below[Types.generator and Types.generator.name or ''] or 0) >= 1
		and (below[Types.plant and Types.plant.name or ''] or 0) >= 1;
	for tn,list in pairs(above) do
		local t = list[1].type;
		local replaced;
		if t == Types.generator or t == Types.mine then
			-- keep power and income: only once the cavern carries at least as many
			replaced = (below[tn] or 0) >= #list;
		elseif t.turretType then
			replaced = powerBelow;	-- towers are not rebuilt above; they just give the base away
		else
			replaced = (below[tn] or 0) >= 1;
		end;
		if replaced then
			local s = list[1];
			Log('Demolishing surface '..s.niceName);
			pcall(s.Destroy);
			return;
		end;
	end;
end;

-- Construction: only below ground once the cavern exists. Until then only
-- power and mines on the surface, so the faction doesn't stall
local baseSiteOK = Economy.SiteOK;
function Economy.SiteOK(loc, forMine)
	if not baseSiteOK(loc, forMine) then return false; end;
	if Mole.phase == 'live' or (Mole.phase == 'cavern' and Mole.level) then
		if Mole.IsBelow(loc) then return true; end;
		return false;
	end;
	return forMine or Economy.allowSurface == true;
end;

-- Underground units stand on the cavern floor of a cell, not on the highest ground
local baseGroundAt = Move.GroundAt;
function Move.GroundAt(u, cell)
	if Mole.level and u and u.location and Mole.IsBelow(u.location) then
		if Mole.IsOpen(cell) then
			local loc = Mole.LocAt(cell);
			local ok, stand = pcall(u.CanStandAt, loc);
			if ok and stand ~= false then return loc; end;
		end;
		return nil;
	end;
	return baseGroundAt(u, cell);
end;

local baseNextStructure = Economy.NextStructure;
function Economy.NextStructure(index)
	-- before the cavern is ready, only power: the rest waits for space below
	if Mole.phase ~= 'live' then
		Economy.allowSurface = Economy.Count(Types.generator) < 1;
		if Economy.Count(Types.generator) < 1 then return {type=Types.generator, kind='base'}; end;
		if Economy.FuelMines() < 1 then return Economy.MineReq('Fuel'); end;
		if Economy.Count(Types.mine) < 2 then return Economy.MineReq('Minerals'); end;
		if Mole.phase == 'shaft' then return nil; end;
	end;
	Economy.allowSurface = false;
	return baseNextStructure(index);
end;

local baseAssign = Economy.AssignConstructor;
function Economy.AssignConstructor(u, index)
	if u.id == Mole.digger then return; end;	-- the digger digs
	-- constructors still on the surface head down once the cavern has room
	if Mole.phase ~= 'shaft' and not Mole.IsBelow(u.location) and Move.IsIdle(u) then
		local open = Mole.CavernCells();
		if #open >= 3 then
			local target = open[(index % #open) + 1];
			local loc = Mole.LocAt(target);
			local ok, path = false, nil;
			if loc then ok, path = pcall(u.FindShortestPath, loc, nil, true); end;
			if ok and path and #path > 0 then
				u.DirectTo(path[#path]);
				return;
			end;
		end;
	end;
	local before = Mem.jobs[u.id];
	baseAssign(u, index);
	-- idle below ground with nothing ordered: out of space, or the mine site needs a tunnel
	if Mole.phase == 'live' and Mem.jobs[u.id] == nil and Move.IsIdle(u) and before == nil then
		Mole.noSpace = true;
		if Mole.tunnelGoal == nil and Economy.noMineSite['Minerals'] == Sense.round then
			local cells = {};
			for _,res in ipairs({'Fuel', 'Minerals'}) do
				for _,c in ipairs(Economy.FindMineCells(res, 3)) do cells[#cells+1] = c; end;
			end;
			local center = CellByIndex(Mole.shaftCell);
			local best, bd = nil, nil;
			for _,c in ipairs(cells) do
				local d = D2(c.center, center.center);
				if bd == nil or d < bd then best, bd = c, d; end;
			end;
			if best then
				Mole.tunnelGoal = best.index;
				Log('Tunnelling to mine site at cell '..best.index);
			end;
		end;
	end;
end;

-- One surveyor accompanies the digger: unknown terrain is never dug blind
local baseScouts = Military.Scouts;
function Military.Scouts()
	local escort = nil;
	if Mole.phase ~= 'live' or Mole.tunnelGoal or Mole.waitSurvey then
		escort = Sense.my.surveyor[1];
	end;
	if escort then
		local d = Mole.Digger();
		local goal = Mole.waitSurvey or (d and d.location.world);
		if goal then
			table.remove(Sense.my.surveyor, 1);
			SafeCall('mole escort', function()
				if not Move.IsIdle(escort) then escort.ClearSafeTasks(); end;
				local cell = d and d.location.cell;
				Move.Towards(escort, goal, cell, false, false);
			end);
		end;
	end;
	baseScouts();
	if escort then table.insert(Sense.my.surveyor, 1, escort); end;
end;

local baseConstructors = Economy.Constructors;
function Economy.Constructors()
	SafeCall('Mole', Mole.Update);
	SafeCall('Demolish', Mole.Demolish);
	baseConstructors();
end;

-- a surveyor first: digging needs eyes below ground
local baseNextVehicle = Economy.NextVehicle;
function Economy.NextVehicle()
	local constructors = #Sense.my.constructor + (Sense.storedByRole.constructor or 0);
	if constructors >= 2 and #Sense.my.surveyor == 0 and (Sense.storedByRole.surveyor or 0) == 0 then
		local sv = Types.Vehicle(Types.turret.surveyor, Types.base.heavy) or Types.Vehicle(Types.turret.surveyor, Types.base.hover);
		if sv then return sv; end;
	end;
	return baseNextVehicle();
end;

-- persist the dig state (flat numbers and strings only)
local baseExport = ExportState;
function ExportState()
	local r = baseExport();
	if type(r) == 'table' and Map.built then
		r.molePhase = Mole.phase;
		r.moleShaft = Mole.shaftCell or 0;
		r.moleLevel = Mole.level or -1000;
		r.moleRings = Mole.rings;
	end;
	return r;
end;

local baseApply = ApplyImport;
function ApplyImport()
	local s = Mem.pendingImport;
	if type(s) == 'table' and s.molePhase then
		Mole.phase = s.molePhase;
		Mole.shaftCell = (s.moleShaft or 0) > 0 and s.moleShaft or nil;
		Mole.level = (s.moleLevel or -1000) > -1000 and s.moleLevel or nil;
		Mole.rings = s.moleRings or Cfg.moleRings;
	end;
	baseApply();
end;

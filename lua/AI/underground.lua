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
	sealed = false,		-- shaft plugged: nothing walks in from the surface
	sealTries = 0,
	liveRound = nil,
};
Cfg.moleSealAfter = 15;
Cfg.moleDigTimeout = 6;		-- rounds a single dig order may take
Cfg.moleCavernRounds = 25;	-- after this long, live with the cavern as it is (if it has some room)		-- rounds after the cavern is ready: seal even if units are still up there

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
	-- open space right above an undug cell: digging there would open the cavern to the outside
	if C.Clear then
		for dh = 1, 2 do
			local okc, content = pcall(Game.Terrain.GetContent, tc, h + dh);
			if okc and content == C.Clear and cell.index ~= Mole.shaftCell then return 'breach'; end;
		end;
	end;
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
	if Mole.SurfaceStructureCells()[best.index] and not Mole.IsOpen(best) then
		Log('Not digging beneath own structure at cell '..best.index);
		Economy.Blacklist(best.index, 30);	-- don't undermine our own structures
		return false;
	end;
	local loc = Mole.LocAt(best);
	if loc == nil then Log('No location at cell '..best.index..' height '..tostring(Mole.level)); return false; end;
	if Mole.IsOpen(best) then
		local ok, path = pcall(u.FindShortestPath, loc, nil, true);
		if ok and path and #path > 0 then u.DirectTo(path[#path]); return true; end;
		Log('Open cell '..best.index..' at height '..tostring(Mole.level)..' is not reachable from '..here.index..'/'..tostring(u.location.height));
		Economy.Blacklist(best.index, 10);
		return false;
	end;
	local check = Mole.DigCheck(best, Mole.level);
	if check == 'lava' or check == 'breach' then
		Log((check == 'lava' and 'Lava next to' or 'Open space above')..' cell '..best.index..': not digging there');
		Economy.Blacklist(best.index, 200);
		Mole.tunnelGoal = nil;
		return false;
	elseif check == 'unknown' then
		-- wait a few rounds for a surveyor to look, then dig anyway
		Mole.unknownSince = Mole.unknownSince or Sense.round;
		if Sense.round - Mole.unknownSince < 3 then
			Mole.waitSurvey = best.center;
			return false;
		end;
		Log('Digging blind into cell '..best.index..': no survey');
	end;
	Mole.unknownSince = nil;
	Mole.waitSurvey = nil;
	local ok, err = pcall(u.DigHorizontally, loc);
	if not ok then Log('DigHorizontally failed: '..tostring(err)); return false; end;
	Mole.lastDig = best.index;
	Log(u.niceName..': digging cell '..best.index..' at height '..tostring(loc.height)..' (standing at '..u.location.cell.index..'/'..tostring(u.location.height)..')');
	return true;
end;

--[[
Shaft site: near the digger, but with no own surface structure within the cavern
radius (+1). Digging beneath one undermines it: a start mine that loses its
ground is disabled, and without a mineral mine the faction dies.
]]
function Mole.SurfaceStructureCells()
	local out = {};
	for _,st in ipairs(Sense.myStructures or {}) do
		if not Mole.IsBelow(st.location) then out[st.location.cell.index] = true; end;
	end;
	return out;
end;

function Mole.PickShaftCell(u)
	local taken = Mole.SurfaceStructureCells();
	local keep = Cfg.moleRings + 1;
	local best, bd = nil, nil;
	for _,c in ipairs(Mole.Rings(u.location.cell, 6)) do
		local free = true;
		for _,n in ipairs(Mole.Rings(c, keep)) do
			if taken[n.index] then free = false; break; end;
		end;
		if free then
			local ok, res = pcall(Game.Grid.GetNaturalResourcesAt, c);
			local resCell = ok and res and ((res.minerals or 0) > 0 or (res.fuel or 0) > 0);
			local d = D2(c.center, u.location.cell.center) + (resCell and 1 or 0);
			if bd == nil or d < bd then best, bd = c, d; end;
		end;
	end;
	return best;
end;

-- The closed cavern cell (next to an open one) nearest to a unit, not claimed by another digger
function Mole.NextClosed(u)
	local center = CellByIndex(Mole.shaftCell);
	local open = {};
	local want = Mole.Rings(center, Mole.rings);
	for _,c in ipairs(want) do
		if Mole.IsOpen(c) then open[c.index] = true; end;
	end;
	open[Mole.shaftCell] = true;	-- the digger stands at the bottom of the shaft
	local best, bd = nil, nil;
	local surface = Mole.SurfaceStructureCells();
	for _,c in ipairs(want) do
		if not open[c.index] and not (Mole.digClaims and Mole.digClaims[c.index]) and not surface[c.index] then
			local bl = Mem.blacklist[c.index];
			if not (bl and bl > Sense.round) then
				for _,nb in ipairs(c.neighbors) do
					if open[nb.index] then
						local d = D2(c.center, u.location.cell.center);
						if bd == nil or d < bd then best, bd = c, d; end;
						break;
					end;
				end;
			end;
		end;
	end;
	return best, want;
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
	-- a lone constructor builds the survival essentials before it digs
	Mole.digging = u ~= nil and (#Sense.my.constructor >= 2 or Mole.EssentialsAnywhere());
	if not Mole.digging then return; end;
	-- a dig order that never completes keeps the digger busy forever: drop it after a few rounds
	if not Move.IsIdle(u) then
		Mole.busy = (Mole.busy or 0) + 1;
		if Mole.busy >= Cfg.moleDigTimeout then
			local t = u.tasks and u.tasks[1];
			Log('Digger stuck for '..Mole.busy..' rounds on '..tostring(t and t.name)..' at height '..tostring(u.location.height)..': dropping it');
			if Mole.lastDig then Economy.Blacklist(Mole.lastDig, 40); end;
			u.ClearTasks();
			Mole.busy = 0;
		end;
		return;
	end;
	Mole.busy = 0;

	if Mole.phase == 'shaft' then
		if Mole.shaftCell == nil then
			local c = Mole.PickShaftCell(u);
			if c == nil then
				if Sense.round % 5 == 0 then Log('No free spot for the shaft yet'); end;
				return;
			end;
			Mole.shaftCell = c.index;
			Log('Digging in: shaft at cell '..Mole.shaftCell);
		end;
		if u.location.cell.index == Mole.shaftCell and Mole.level == nil then
			Mole.level = u.location.height - Cfg.moleDepth;
			Log('Shaft goes down to height '..Mole.level);
		end;
		if u.location.cell.index ~= Mole.shaftCell then
			local loc = Move.GroundAt(u, CellByIndex(Mole.shaftCell));
			if loc then Move.Towards(u, loc.world, loc.cell, false, false); end;
			return;
		end;
		if Mole.level and u.location.height > Mole.level then
			local check = Mole.DigCheck(u.location.cell, u.location.height - 2);
			if check == 'lava' then
				-- lava below: settle for the depth reached
				Mole.level = u.location.height;
				Log('Lava below: cavern at height '..Mole.level);
			elseif check == 'unknown' and #Sense.my.surveyor > 0 and Sense.round - (Mole.unknownSince or Sense.round) < 3 then
				Mole.unknownSince = Mole.unknownSince or Sense.round;
				Mole.waitSurvey = u.location.world;
				return;
			else
				Mole.unknownSince = nil;
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
	Mole.digClaims = {};
	local closed, want = Mole.NextClosed(u);
	Mole.cavernSince = Mole.cavernSince or Sense.round;
	if closed and Mole.phase == 'cavern' and Sense.round - Mole.cavernSince >= Cfg.moleCavernRounds then
		local _, n = Mole.CavernCells();
		if n >= 4 then
			Log('Cavern digging takes too long: using the '..n..' open cells');
			closed = nil;
		elseif Sense.round % 10 == 0 then
			Log('Cavern still has only '..n..' open cells after '..(Sense.round - Mole.cavernSince)..' rounds');
		end;
	end;
	if closed then
		Mole.digClaims[closed.index] = true;
		Mole.DigToward(u, closed);
		return;
	end;
	if Mole.phase == 'cavern' then
		Mole.liveRound = Sense.round;
		Mole.phase = 'live';
		Log('Cavern ready ('..#want..' cells): building below ground');
	end;
	-- live: seal the shaft first. An open shaft lets every other faction walk right in
	if not Mole.sealed then
		Mole.Seal(u);
		return;
	end;
	-- then tunnel to a requested mine site, otherwise widen the cavern when space runs out
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

--[[
Seal the shaft: fill its terrain one layer above the cavern floor with
foundation, from a neighboring cavern cell. A unit cell's terrain is its
central terrain cell plus the cells it shares with its neighbors.
Waits until constructors and surveyors are down (or Cfg.moleSealAfter rounds passed).
]]
function Mole.ShaftTerrain(h)
	local C = Game.Terrain.Content or {};
	local ok, tc = pcall(Game.Terrain.TranslateUnitCell, CellByIndex(Mole.shaftCell));
	if not ok or tc == nil then return {}, false; end;
	local out, open = {}, false;
	local cells = {tc};
	for _,n in ipairs(tc.neighbors or {}) do cells[#cells+1] = n; end;
	for i,c in ipairs(cells) do
		local okc, content = pcall(Game.Terrain.GetContent, c, h);
		if okc and (content == C.Clear or content == C.Water) then
			out[#out+1] = Game.Terrain.Pack(c, h);
			if i == 1 then open = true; end;
		end;
	end;
	return out, open;
end;

function Mole.Seal(u)
	local h = Mole.level + 1;
	local cells, open = Mole.ShaftTerrain(h);
	if not open then
		Mole.sealed = true;
		Log('Sealed shaft at cell '..Mole.shaftCell);
		return;
	end;
	-- wait for the ground units still on the surface
	local up = 0;
	for _,role in ipairs({'constructor', 'surveyor'}) do	-- combat units left above guard the surface
		for _,v in ipairs(Sense.my[role] or {}) do
			if not Mole.IsBelow(v.location) then up = up + 1; end;
		end;
	end;
	if up > 0 and Sense.round - (Mole.liveRound or Sense.round) < Cfg.moleSealAfter then
		return;
	end;
	-- stand next to the shaft, inside the cavern
	local shaft = CellByIndex(Mole.shaftCell);
	local nextTo = false;
	for _,nb in ipairs(shaft.neighbors) do
		if nb.index == u.location.cell.index and Mole.IsBelow(u.location) then nextTo = true; end;
	end;
	if not nextTo then
		for _,nb in ipairs(shaft.neighbors) do
			if Mole.IsOpen(nb) then
				local loc = Mole.LocAt(nb);
				local ok, path = pcall(u.FindShortestPath, loc, nil, true);
				if ok and path and #path > 0 then u.DirectTo(path[#path]); return; end;
			end;
		end;
		Log('Cannot reach the shaft to seal it');
		return;
	end;
	Mole.sealTries = Mole.sealTries + 1;
	if Mole.sealTries > 6 then
		if Mole.sealTries == 7 then Log('Sealing the shaft keeps failing: giving up'); end;
		Mole.sealed = true;
		return;
	end;
	local ok, err = pcall(u.BuildBlocks, cells);
	if ok then
		Log('Sealing shaft at cell '..Mole.shaftCell..' ('..#cells..' blocks, '..up..' units left above)');
	else
		Log('BuildBlocks failed: '..tostring(err));
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
	if Mole.phase ~= 'shaft' and Mole.IsBelow(loc) then return true; end;
	-- surface: only what keeps the faction alive until the cavern has its own
	-- (without a mineral mine a faction dies within a few rounds)
	if forMine then return not Mole.EssentialsBelow(); end;
	return Economy.allowSurface == true;
end;

-- Does the cavern have a mineral mine, a fuel mine and a generator of its own?
function Mole.EssentialsBelow()
	local mineral, fuel, gen = false, false, false;
	for _,st in ipairs(Sense.myStructures or {}) do
		if Mole.IsBelow(st.location) then
			if st.type == Types.generator then gen = true;
			elseif st.type == Types.mine then
				local ok, f = pcall(Game.Grid.GetNaturalResourceAt, 'Fuel', st.location.cell);
				if ok and f and f > 0 then fuel = true; else mineral = true; end;
			end;
		end;
	end;
	return mineral and fuel and gen;
end;

function Mole.MineralMines()
	local n = 0;
	for _,st in ipairs(Sense.myStructures or {}) do
		if st.type == Types.mine then
			local ok, m = pcall(Game.Grid.GetNaturalResourceAt, 'Minerals', st.location.cell);
			if ok and m and m > 0 then n = n + 1; end;
		end;
	end;
	return n;
end;

-- Surface essentials: a mineral mine, a fuel mine and a generator
function Mole.EssentialsAnywhere()
	return Mole.MineralMines() >= 1 and Economy.FuelMines() >= 1 and Economy.Count(Types.generator) >= 1;
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
	-- survival first, on the surface if need be: a mineral mine (the faction dies without one),
	-- a fuel mine and a generator. The cavern gets its own later, then these are demolished
	Economy.allowSurface = false;
	if not Mole.EssentialsBelow() then
		if Mole.MineralMines() < 1 then
			local m = Economy.MineReq('Minerals'); if m then return m; end;
		end;
		if Economy.FuelMines() < 1 then
			local m = Economy.MineReq('Fuel'); if m then return m; end;
		end;
		if Economy.Count(Types.generator) < 1 then
			Economy.allowSurface = true;
			return {type=Types.generator, kind='base'};
		end;
	end;
	-- no vehicle plant anywhere: one on the surface for now (the cavern gets its own, then this one goes)
	if Types.plant and Economy.Count(Types.plant) == 0 and Mole.phase ~= 'live' then
		Economy.allowSurface = true;
		return {type=Types.plant, kind='base'};
	end;
	if Mole.phase ~= 'live' then return nil; end;
	return baseNextStructure(index);
end;

local baseAssign = Economy.AssignConstructor;
function Economy.AssignConstructor(u, index)
	if u.id == Mole.digger and Mole.digging then return; end;	-- the digger digs
	-- constructors still on the surface head down once the cavern has room
	if Mole.phase ~= 'shaft' and not Mole.sealed and not Mole.IsBelow(u.location) and Move.IsIdle(u)
		and Mole.EssentialsAnywhere() then
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
	-- below ground before the cavern is ready: help digging
	if Mole.phase == 'cavern' and Mole.IsBelow(u.location) and Move.IsIdle(u) and u.DigHorizontally then
		local c = Mole.NextClosed(u);
		if c then
			Mole.digClaims = Mole.digClaims or {};
			Mole.digClaims[c.index] = true;
			if Mole.DigToward(u, c) then return; end;
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

-- bombs are the only weapon that really works underground: once the plants are
-- below ground, every combat vehicle they build is a bomb (tanks only for the surface guard)
table.insert(Cfg.research, 1, 'Unlock:Bomb');

-- a surveyor first: digging needs eyes below ground
local baseNextVehicle = Economy.NextVehicle;
function Economy.NextVehicle()
	local below = 0;
	for _,p in ipairs(Sense.plants) do
		if Mole.IsBelow(p.location) then below = below + 1; end;
	end;
	Cfg.bombShare = (below > 0 and below == #Sense.plants) and 1.0 or 0.5;
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
		r.moleSealed = Mole.sealed and 1 or 0;
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
		Mole.sealed = (s.moleSealed or 0) == 1;
		Mole.liveRound = Mole.phase == 'live' and -1000 or nil;
	end;
	baseApply();
end;

--[[
Hostile surveyors are the main danger: they look into the ground and reveal the
cavern. They are shot first, and units nearby hunt them down.
]]
local function IsSurveyor(e)
	return e.turret and e.turret.type and e.turret.type.detectsResources;
end;

local baseFire = Military.Fire;
function Military.Fire(v)
	local t = v.turret;
	if t and t.type.canAttack and v.Attack and t.SearchAttackTarget then
		for _,e in pairs(Game.hostileUnits) do
			if e.health > 0 and IsSurveyor(e) and not e.location.isSuborbital then
				local ok, choice = pcall(t.SearchAttackTarget, e.location);
				if ok and choice then
					v.Attack({choice});
					Log(v.niceName..': firing at hostile surveyor '..tostring(e.niceName));
					return true;
				end;
			end;
		end;
	end;
	return baseFire(v);
end;

-- combat units hunt hostile surveyors anywhere near the base before anything else
local baseCombat = Military.Combat;
function Military.Combat()
	local hunted = {};
	local home = Mole.shaftCell and CellByIndex(Mole.shaftCell);
	for _,e in pairs(Game.hostileUnits) do
		if e.health > 0 and IsSurveyor(e) and home and D2(e.location.cell.center, home.center) < 0.02 then
			-- nearest two combat units go for it
			local list = {};
			for _,v in ipairs(Sense.my.combat) do
				if not hunted[v.id] then list[#list+1] = {v=v, d=D2(v.location.world, e.location.world)}; end;
			end;
			table.sort(list, function(a,b) return a.d < b.d end);
			for i = 1, math.min(2, #list) do
				local v = list[i].v;
				hunted[v.id] = true;
				SafeCall('hunt '..v.niceName, function()
					if not Military.Fire(v) or v.motionIgnoresAttack then
						if not Move.IsIdle(v) then v.ClearSafeTasks(); end;
						Move.Towards(v, e.location.world, e.location.cell, false, true);
					end;
				end);
			end;
		end;
	end;
	if next(hunted) == nil then return baseCombat(); end;
	-- the rest as usual
	local keep = Sense.my.combat;
	local rest = {};
	for _,v in ipairs(keep) do if not hunted[v.id] then rest[#rest+1] = v; end; end;
	Sense.my.combat = rest;
	baseCombat();
	Sense.my.combat = keep;
end;

-- Towers below ground only matter once someone has broken in, and by then the base is lost
-- anyway. They are also two layers high: in a one-layer cavern they would have to be built
-- submerged (in a pit dug one layer down), which this script doesn't do. So none below ground:
-- the minerals go into bombs and economy
Cfg.moleMaxTowers = 0;
local function TowerCount()
	local n = 0;
	for _,st in ipairs(Sense.myStructures or {}) do
		if st.turret and st.turret.type.canAttack then n = n + 1; end;
	end;
	return n;
end;
local baseDefenseNeed = Economy.DefenseNeed;
function Economy.DefenseNeed(surplus, homeOnly)
	local need = baseDefenseNeed(surplus, homeOnly);
	if need and need.type and need.type.turretType and need.type.turretType.canAttack
		and TowerCount() >= Cfg.moleMaxTowers then
		return nil;
	end;
	return need;
end;
local baseGroundTower = Types.GroundTower;
function Types.GroundTower()
	if TowerCount() >= Cfg.moleMaxTowers then return nil; end;
	return baseGroundTower();
end;

--[[
Hexaverse AI player: air dominance personality
==============================================

Plays like player.lua, but goes for planes as early as possible:
* Plane factory research first, and plane factories right after the first
  vehicle plant (with the fuel mine they need).
* Most minerals go to planes. The vehicle plants build constructors, scouts
  and a small ground guard, little else.
* Bombers hit artillery, towers and structures from above; raptors hunt hostile
  planes and escort the bombers; a sensor drone keeps the map in view.
* AA towers at home: the opponent's answer to planes is planes.

Select it with Rules/AIPlayers/script = 'air' (or in a list,
e.g. 'player,underground,air').
]]

--#require "player.lua"

Cfg.logPrefix = 'Air AI: ';
Air = {};
Cfg.planePriority = 'early';
Cfg.planeFactoryRound = 0;
Cfg.planeFactoryMines = 1;
Cfg.planeFactories = 1;		-- a second one from round 50 (see below)
Cfg.maxPlanes = 14;
Cfg.planeMinerals = 100;
Cfg.bomberShare = 0.55;
Cfg.wantDrones = 1;
Cfg.groundGuard = 4;			-- combat vehicles kept at home

-- research: planes first, then the rest
table.insert(Cfg.research, 1, 'Unlock:Bomber');
table.insert(Cfg.research, 1, 'Unlock:Plane');

-- Saving up for the plane factory: researched, fuel mine running, minerals short
function Air.Saving()
	local pf = Types.planeFactory;
	return #Sense.planeFactories == 0 and pf ~= nil and Types.IsUnlocked(pf)
		and (Types.IsUnlocked(Types.bomber) or Types.IsUnlocked(Types.raptor))
		and Economy.FuelMines() >= 1
		and (Game.myFaction.currentResources.storage.minerals or 0) < (tonumber(pf.constructionCost) or 200) + 50;
end;

local baseNextStructure = Economy.NextStructure;
function Economy.NextStructure(index)
	Cfg.planeFactories = (Sense.round >= 50 and Economy.Count(Types.mine) >= 5) and 2 or 1;
	local want = baseNextStructure(index);
	-- while saving, constructors build only what earns or powers (mines, generators)
	if want and Air.Saving() and Sense.Pressure() <= 3 and want.kind ~= 'mine'
		and want.type ~= Types.generator and want.type ~= Types.planeFactory
		and not (want.type == Types.plant and Economy.WorkingPlants() == 0) then
		return nil;
	end;
	return want;
end;

-- the ground plants hold back for the plane factory (minerals for it must pile up first),
-- and once it runs, minerals go to planes
local baseNextVehicle = Economy.NextVehicle;
function Economy.NextVehicle()
	local t = baseNextVehicle();
	if t == nil then return t; end;
	local pf = Types.planeFactory;
	if #Sense.planeFactories == 0 then
		local saving = Air.Saving();
		local tt = t.turretType;
		local essential = tt and (tt.canConstruct or tt.detectsResources);
		if saving and not essential and #Sense.my.combat >= 2 and Sense.Pressure() <= 3 then
			return nil;
		end;
		return t;
	end;
	local tt = t.turretType;
	local combat = #Sense.my.combat + #Sense.my.bomb + (Sense.storedByRole.combat or 0);
	local ground = tt and tt.canAttack and tt.trajectoryType ~= 'AAMissile';
	if (ground or (tt and tt.canSelfDetonate)) and combat >= Cfg.groundGuard
		and #Sense.my.plane < Cfg.maxPlanes and Sense.Pressure() <= 0 then
		return nil;
	end;
	return t;
end;

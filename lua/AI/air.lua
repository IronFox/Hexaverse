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

local baseNextStructure = Economy.NextStructure;
function Economy.NextStructure(index)
	Cfg.planeFactories = (Sense.round >= 50 and Economy.Count(Types.mine) >= 5) and 2 or 1;
	return baseNextStructure(index);
end;

-- the ground plants hold back once a plane factory runs: minerals go to planes
local baseNextVehicle = Economy.NextVehicle;
function Economy.NextVehicle()
	local t = baseNextVehicle();
	if t == nil or #Sense.planeFactories == 0 then return t; end;
	local tt = t.turretType;
	local combat = #Sense.my.combat + #Sense.my.bomb + (Sense.storedByRole.combat or 0);
	local ground = tt and tt.canAttack and tt.trajectoryType ~= 'AAMissile';
	if (ground or (tt and tt.canSelfDetonate)) and combat >= Cfg.groundGuard
		and #Sense.my.plane < Cfg.maxPlanes and Sense.Pressure() <= 0 then
		return nil;
	end;
	return t;
end;

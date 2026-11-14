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
Cfg.wantDrones = 3;	-- lots of sensor drones

-- research: planes first, then the rest
table.insert(Cfg.research, 1, 'Unlock:Bomber');
table.insert(Cfg.research, 1, 'Unlock:Raptor');
table.insert(Cfg.research, 1, 'Unlock:PlaneFactory');

-- Weights (see needs.lua): planes matter a lot, the ground army less. Saving for the
-- factory follows from the weights: while it is the top option and not affordable yet,
-- cheaper builds and vehicles that score far lower wait
Cfg.needWeights = {planes = 9, army = 0.8, fuel = 5};

-- a second factory once the economy carries it
local baseUpdate = Needs.Update;
function Needs.Update()
	Cfg.planeFactories = (Sense.round >= 50 and Economy.Count(Types.mine) >= 5) and 2 or 1;
	baseUpdate();
	-- every plane factory burns fuel each round: without spare fuel they shut down
	if #Sense.planeFactories > 0 and (Needs.v.fuel or 0) > 0 then Needs.v.fuel = 1; end;
end;

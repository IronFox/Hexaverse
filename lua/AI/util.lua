
function Sqr(x)
	return x*x;
end;

function QuadraticDistance(u,v)
	return Sqr(u.x - v.x) + Sqr(u.y - v.y) + Sqr(u.z - v.z);
end;


Util = {};

function Util.RoundToInt(x)
	return math.floor(num+.5);
end;

--[[
Projects a location to its suborbital height. No effect if the location already is suborbital
]]
function Util.ToSuborbit(loc --[[Loc]]) --[[Loc]]
	return Game.Grid.ToLocation(loc.cell,Game.Grid.suborbitalHeight);
end;

--[[
Projects a location to ground, using the specified reference location for height
]]
function Util.ToGround(loc --[[Loc]], ref --[[Loc]]) --[[Loc]]
	return Game.Grid.ToLocation(loc.cell,ref.height);
end;


function Util.CanAttackSuborbital(u --[[Unit]]) --bool
	return u.IsInRange(Util.ToSuborbit(u.location).world);
end;

function Util.CanAttackGround(u --[[Unit]], ref --[[Loc]]) --bool
	return u.IsInRange(ToGround(u.location,ref).world);
end;

function Util.GetDamagePotentialPerRound(u --[[Unit]]) --float
	if not u.turret then
		return 0.0;
	end;
	return u.turret.damageLevel * u.turret.attacksPerRound;
end;

function Util.IsGreater(u --[[Unit]], v --[[Unit]]) --bool
	return u.type.constructionCost > v.type.constructionCost or 
	(u.type.constructionCost == v.type.constructionCost and u.id > v.id);
end;


function Util.Vis(qDistance, unitSensorRange)
	local qSensorRange = unitSensorRange * unitSensorRange;
	return {
			isVisibile = qDistance < qSensorRange,
			sensorRange = unitSensorRange,
			quadraticSensorRange = qSensorRange,
			quadraticDistance = qDistance
			};
end;


function Util.CheckVisibility(observer --[[Unit]], observed --[[Loc]]) --[[{isVisibile:bool, sensorRange:float, quadraticSensorRange:float, quadraticDistance:float}]]
	if (observed.isSuborbital) then
		if (observer.location.isSuborbital) then
			local dist = QuadraticDistance(observer.location.world, observed.world);
			return Util.Vis(dist,observer.scanningRange);
		else
			if (observer.suborbitalScanningRange ~= nil) then
				local dist = QuadraticDistance(Util.ToSuborbit(observer.location).world, observed.world);
				return Util.Vis(dist,observer.suborbitalScanningRange);
			end;
			return Util.Vis(1,0);
		end;
	else
		if (observer.location.isSuborbital) then
			
			local distance = QuadraticDistance(observer.location.world, observed.world);
			local rs = Util.Vis(distance,observer.scanningRange);
			if rs.isVisibile then
				return rs;
			end;
			if (observer.groundScanningRange ~= nil) then
				distance = QuadraticDistance(observer.location.world, Util.ToSuborbit(observed).world);
				return Util.Vis(distance,observer.groundScanningRange);
			end;
			return rs;
		else
			local distance = QuadraticDistance(observer.location.world, observed.world);
			return Util.Vis(distance,observer.scanningRange);
		end;
	end;
end;



function Util.IsGhost(u --[[Unit]]) --bool
	return u.health <= 0;
end;

function Util.IsAlive(u --[[Unit]]) --bool
	return not Util.IsGhost(u);
end;

function Util.Push(array, element)
	array[#array+1] = element;
end;

function Util.AddCurrentToReach(reach,unit)
	local node = {
		location = unit.location,
		movementCostsFromRoot = 0,
		predecessor = nil,
		hangar = nil
		};
	for _,n in pairs(reach) do
		if not n.predecessor then
			n.predecessor = node;
		end;
	end;
	Util.Push(reach,node);
end;


--[[
Iterates over all hostile units and hostile contact markers,
and passes each to the given function as only parameter.
If the function returns false (not nil), then iteration is aborted
]]
function Util.ForeachHostileIncludingGhosts(func)
	for _,e in pairs(Game.myMarkers) do
		if e.type == 'HostileContact' and e.unitReference then
			local rs = func(e.unitReference);
			if rs ~= nil and rs == false then
				return;
			end;
		end;
	end;
	for _,e in pairs(Game.hostileUnits) do
		local rs = func(e);
		if rs ~= nil and rs == false then
			return;
		end;
	end;
end;


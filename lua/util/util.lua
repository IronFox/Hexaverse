
function Sqr(x)
	return x*x;
end;

function QuadraticDistance(u,v)
	return Sqr(u.x - v.x) + Sqr(u.y - v.y) + Sqr(u.z - v.z);
end;


Util = {};

function Util.RoundToInt(x)
	return math.floor(x+.5);
end;

--[[ Util.Trim

Returns a trimmed version of the given string.
Argument must be a string
]]
function Util.Trim(s)
   return string.match(s,'^()%s*$') and '' or string.match(s,'^%s*(.*%S)');
end

--[[ Util.Log
Recursively logs an arbitrary object to Game.Log.

@param name Name of the object to log. Must be a string
@param var Object to print. May be anything, including nil and potentially looping table graphs.
@param maxDepth Optional maximum tree depth. No effect iv the given object is not a table. 0 means the argument itself is not expanded, 1 that only the immediate sub-fields are printed, asf.
@param indent Optional indentation string. Used to indent sub-tables but may be used to apply an indentation to the first printout
@param history Should be nil: Map{table->string} to mark visited tables. Created automatically.
@param path Should be nil: String path to the local variable, already including its name
]]
function Util.Log(name --[[string]], var --[[anything]], maxDepth--[[int default nil]], indent --[[string default nil]], history--[[ table[] default nil ]], path--[[string default nil]])
	local t = type(var);
	if maxDepth == nil then
		maxDepth = 1000;
	end;
	if indent == nil then
		indent = '';
	end;
	if t == 'number' or t == 'boolean' then
		Game.Log(indent .. name..': '..tostring(var));
	elseif t == 'string' then
		Game.Log(indent .. name..': "'..tostring(var)..'"');
	elseif t == 'table' then
		if history ~= nil and history[var] then
			Game.Log(indent .. name..': <recurring table '.. history[var].. '>');
		else
			if maxDepth < 1 then
				Game.Log(indent .. name..': <table>');
			else
				if not path then
					path = '[var]';
				end;
				if not history then
					history = {};
				end;
				history[var] = path;
				Game.Log(indent .. name..':');
				for k,v in pairs(var) do
					Util.Log(tostring(k),v, maxDepth-1, indent..'    ', history,path..'.'..tostring(k));
				end;
			end;
		end;
	else
		Game.Log(indent ..name..': <'..t..'>');
	end;
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
	return u.turret.IsInRange(Util.ToSuborbit(u.location).world);
end;

function Util.CanAttackGround(u --[[Unit]], ref --[[Loc]]) --bool
	return u.turret.IsInRange(Util.ToGround(u.location,ref).world);
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
			isVisible = qDistance < qSensorRange,
			sensorRange = unitSensorRange,
			quadraticSensorRange = qSensorRange,
			quadraticDistance = qDistance
		};
end;


function Util.CheckVisibility(observer --[[Unit]], observed --[[Loc]]) --[[{isVisible:bool, sensorRange:float, quadraticSensorRange:float, quadraticDistance:float}]]
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
			if rs.isVisible then
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


--[[
Conceptual notes:

ScriptedType is the central appendix for unit which are not of a built-in type.
The unit is constructed bare, then fed into the requested Behavior constructor.

The unit behavior is referenced in the Unit instance via a new member variable 'scripted'.
This member is not set for built-in types


]]


ScriptedType = {
	--[[unit : Unit ]]
};



function ScriptedType:Instantiate (o, unit)
	o = o or {}   -- create object if user does not provide one
	o.unit = unit;
	setmetatable(o, self)
	self.__index = self
	return o
end;

function ScriptedType:New (name--[[string]], height--[[int]])
	o = {}
	o.typeName = name;
	o.height = height;
	setmetatable(o, self)
	self.__index = self
	return o
end;


--[[
Reacts to a left or right mouse click of the user while a unit with the local behavior is selected
]]
function ScriptedType:DirectTo ( mode --[[CursorMode]], waypoints --[[ Loc[] ]], detailedPath --[[bool]], leftClick --[[bool]] )	--bool
	return false;
end;







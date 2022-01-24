--[[
Conceptual notes:

ScriptedType is the central appendix for unit which are not of a built-in type.
The unit is constructed bare, then fed into the requested Behavior constructor.

The unit behavior is referenced in the Unit instance via a new member variable 'scripted'.
This member is not set for built-in types


]]



function Ternary(condition --[[bool]], ifTrue --[[any]], ifFalse --[[any]])
	if condition then
		return ifTrue;
	end;
	return ifFalse;
end;

ScriptedType = {
	--[[unit : Unit ]]
};



function ScriptedType:Instantiate (o, unit)
	o = o or {}   -- create object if user does not provide one
	o.unit = unit;
	o.commands = {};
	for k,v in pairs(self.commands) do
		o.commands[k] = v;
	end
	setmetatable(o, self)
	self.__index = self
	return o
end;

function ScriptedType:New (name--[[string]], height--[[int]])
	o = {}
	o.typeName = name;
	o.height = height;
	o.commands = {};
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


--[[Table-Type: CmdCost

	int		opCount; -- Cost in operations
	int		resCount; -- Cost in resources
	string	resType;  -- Type of resource needed (see ResVec, case ignored). May be nil if resCount is 0
]]


--[[Table-Type: CmdLabel

	string	brief; -- Text to show when short-hovering
	string	extended; -- Text to also show when long-hovering
]]


CommandFlags = 
{
	None = 0,
	IsEnabled=0x1,
	IsPermaActive=0x2,
	IsFlashing=0x4,
	SoftFlashing=0x8,
	IsInvisible=0x10
};

function ScriptedType:GetCommands()
	return self.commands;
end;


function ScriptedType:AddCommand( name--[[string]], customLabel --[[string/nil]], onClick --[[void(instance)]], getCost --[[CmdCost(instance)]], createLabel --[[CmdLabel(instance)]], getFlags --[[CommandFlags[](instance)]]) -- void
	table.insert(self.commands, 
		{
			name = name,
			customLabel = customLabel,
			onClick = onClick,
			getCost = getCost,
			createLabel = createLabel,
			getFlags = getFlags
		}
	);
end;

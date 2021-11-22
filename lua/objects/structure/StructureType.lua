--#require "../ScriptedType.lua"


StructureType = ScriptedType:New('Structure');

function StructureType:New(name --[[string]], height--[[int]], hasFoundation --[[bool]], isBroad --[[bool]], category --[[string]], meshName --[[string]])
	o = ScriptedType.New(self,name,height);
	o.hasFoundation = hasFoundation;
	o.category = category;
	o.meshName = meshName;
	o.isBroad = isBroad;
	return o;
end;

function StructureType:Instantiate (o, unit --[[Unit]])
	o = ScriptedType:Instantiate(o,unit);
	return o
end

function StructureType:ConnectionPossibleTo ( other --[[Structure]], acrossHighCell --[[Cell]])
	return true;
end;












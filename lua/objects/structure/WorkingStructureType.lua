--#require "StructureType.lua"


WorkingStructureType = StructureType:New('WorkingStructureType', 1, true, true, nil, nil);

function WorkingStructureType:New(name --[[string]], height--[[int]], hasFoundation --[[bool]], isBroad --[[bool]], category --[[string]], meshName --[[string]])
	return StructureType.New(self,name,height, hasFoundation, isBroad, category, meshName);
end;


function WorkingStructureType:Instantiate (o, unit --[[Unit]])
	o = StructureType:Instantiate(o,unit);
	return o;
end;


function WorkingStructureType:CanEnable()
	return true;
end;

function WorkingStructureType:GetResourceVector(balance --[[ResVec]], storageCurrent --[[ResVec]], storageMax --[[ResVec]])
	balance.energy = balance.energy - 10;
end;

function WorkingStructureType:SetResourceStorage(newStorage --[[ResVec]])
	
end;


WorkingStructureType:AddCommand('Enable',nil, 
	function(self)
		self.unit.Enable(not self.unit.isEnabled);
	end,
	function(self)
		return {opCount=0, resCount=0, resType='Minerals'};
	end,
	function(self)
		return {brief = Ternary(self.unit.isEnabled,'Disable','Enable')};
	end,
	function(self)
		return {CommandFlags.IsEnabled, Ternary(self.unit.isEnabled, CommandFlags.IsPermaActive, CommandFlags.None)};
	end
);

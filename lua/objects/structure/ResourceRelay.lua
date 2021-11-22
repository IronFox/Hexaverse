--#require "WorkingStructureType.lua"



ResourceRelayType = WorkingStructureType:New('ResourceRelay', 1, true, true, 'Infrastructure', 'starPort');

ResourceRelayType.briefDescription = '';
ResourceRelayType.textDescription = 'Resource relays link to other relays to transfer resources. An unobstructed path into orbit is required to maintain the connection';


function ResourceRelayType:Instantiate (o, unit --[[Unit]])
	o = WorkingStructureType:Instantiate(o,unit, true, 'Infrastructure');
	o.meshName = 'skyHub.obj';
	return o;
end



Module.DeclareWorkingStructure(ResourceRelayType);




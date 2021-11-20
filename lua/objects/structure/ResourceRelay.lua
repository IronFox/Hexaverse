--#require <../StructureType.lua>



ResourceRelayType = StructureType:New('ResourceRelay', true, 'Infrastructure', 'skyHub.obj');

function ResourceRelayType:Instantiate (o, unit --[[Unit]])
	o = StructureType:Instantiate(o,unit, true, 'Infrastructure');
	o.meshName = 'skyHub.obj';
	return o;
end

ResourceRelayType.briefDescription = '';
ResourceRelayType.textDescription = 'Resource relays link to other relays to transfer resources. An unobstructed path into orbit is required to maintain the connection';



Module.DeclareWorkingStructure(ResourceRelayType);













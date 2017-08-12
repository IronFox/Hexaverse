#ifndef objectPrototypesH
#define objectPrototypesH

#include "types.h"
#include "../stringTable.h"
#include <container/buffer.h>
#include <global_string.h>


namespace Objects
{
	extern Ctr::Buffer0<PStructure>			structurePrototypes;
	namespace Component
	{
		extern Ctr::Buffer0<VehicleBase*>	baseComponents;
		extern Ctr::Buffer0<Turret*>		vehicleTurretComponents,
											allTurretComponents;

	}
	#ifndef NO_SATELLITES
		extern Buffer0<PSatellite>		satellitePrototypes;
	#endif

	String			ToString(const EntityType&);
	EntityType		DecodeType(const String&);


	EntityType		ComponentOrPrototypeToCName(const String&name);
	EntityType		ComponentOrPrototypeToCName(const StringRef&name);
	ConstName		ComponentToCName(const StringRef&name);
	ConstName		ComponentToCName(const String&name);
	String			ComponentOrPrototypeToString(EntityType name);



	PStructure		GetPrototypeByName(EntityType structureTypeName);
	PStructure		GetPrototypeByName(const String&structureTypeName);
	PStructure		GetPrototypeByName(const StringRef&structureTypeName);
	#ifndef NO_SATELLITES
		PSatellite	GetSatellitePrototypeByName(const String&name);
		PSatellite	GetSatellitePrototypeByName(const StringRef&name);
		PSatellite	GetSatellitePrototypeByName(EntityType name);
	#endif

	PWorldEntity	GetNonVehiclePrototypeByName(EntityType typeName);
	PWorldEntity	GetNonVehiclePrototypeByName(const String&typeName);
	PWorldEntity	GetNonVehiclePrototypeByName(const StringRef&typeName);
	namespace Component
	{
		VehicleBase*	GetVehicleBaseByName(ConstName name);
		VehicleBase*	GetVehicleBaseByName(const String&name);
		VehicleBase*	GetVehicleBaseByName(const StringRef&name);
		Turret*			GetTurretByName(const StringRef&name);
		Turret*			GetTurretByName(const String&name);
		Turret*			GetTurretByName(ConstName name);
		Base*			GetByName(const String&name);
		Base*			GetByName(const StringRef&name);
		Base*			GetByName(ConstName name);

		void			ConfigureAll();
		void			DestroyAll();
	}
}




#endif

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



	ConstName		ComponentOrPrototypeToCName(const String&name);
	String			ComponentOrPrototypeToString(ConstName name);



	PStructure		GetPrototypeByName(ConstName structureTypeName);
	PStructure		GetPrototypeByName(const String&structureTypeName);
	#ifndef NO_SATELLITES
		PSatellite	GetSatellitePrototypeByName(const String&name);
		PSatellite	GetSatellitePrototypeByName(ConstName name);
	#endif

	PWorldEntity	GetNonVehiclePrototypeByName(ConstName typeName);
	PWorldEntity	GetNonVehiclePrototypeByName(const String&typeName);
	namespace Component
	{
		VehicleBase*	GetVehicleBaseByName(ConstName name);
		VehicleBase*	GetVehicleBaseByName(const String&name);
		Turret*			GetTurretByName(const String&name);
		Turret*			GetTurretByName(ConstName name);
		Base*			GetByName(const String&name);
		Base*			GetByName(ConstName name);

		void			ConfigureAll();
		void			DestroyAll();
	}
}




#endif

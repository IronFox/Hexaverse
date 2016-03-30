#ifndef objectPrototypesH
#define objectPrototypesH

#include "types.h"
#include "../stringTable.h"
#include <container/buffer.h>
#include <global_string.h>


namespace Objects
{
	extern Buffer0<PStructure>			structurePrototypes;
	namespace Component
	{
		extern Buffer0<VehicleBase*>	baseComponents;
		extern Buffer0<Turret*>			vehicleTurretComponents,
										allTurretComponents;

	}
	extern Buffer0<PSatellite>			satellitePrototypes;



	



	PStructure		GetPrototypeByName(ConstName structureTypeName);
	PStructure		GetPrototypeByName(const String&structureTypeName);
	PSatellite		GetSatellitePrototypeByName(const String&name);
	PSatellite		GetSatellitePrototypeByName(ConstName name);

	PWorldEntity	GetNonVehiclePrototypeByName(ConstName typeName);
	PWorldEntity	GetNonVehiclePrototypeByName(const String&typeName);

	namespace SubOrbital
	{
		extern Buffer0<PPlane>	prototypes;
		PPlane		GetPrototypeByName(ConstName structureTypeName);
		PPlane		GetPrototypeByName(const String&structureTypeName);
	}

	namespace Component
	{
		VehicleBase*	GetVehicleBaseByName(ConstName name);
		VehicleBase*	GetVehicleBaseByName(const String&name);
		Turret*			GetTurretByName(const String&name);
		Turret*			GetTurretByName(ConstName name);

		void			ConfigureAll();
		void			DestroyAll();
	}
}




#endif

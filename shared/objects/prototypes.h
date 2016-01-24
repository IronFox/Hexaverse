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
	namespace Orbital
	{
		extern Buffer0<PShip>			shipPrototypes;
		extern Buffer0<PStation>		stationPrototypes;

		PStation		GetStationPrototypeByName(const String&name);
		PStation		GetStationPrototypeByName(ConstName name);
		PShip			GetShipPrototypeByName(const String&name);
		PShip			GetShipPrototypeByName(ConstName name);
	}

	

	PWorldEntity	GetAnyNonVehiclePrototypeByName(ConstName structureTypeName);
	PWorldEntity	GetAnyNonVehiclePrototypeByName(const String&structureTypeName);

	PStructure		GetPrototypeByName(ConstName structureTypeName);
	PStructure		GetPrototypeByName(const String&structureTypeName);

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

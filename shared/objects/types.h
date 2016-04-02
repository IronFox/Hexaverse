#ifndef objectTypesH
#define objectTypesH

#include <memory>
#include <general/enumeration.h>
#include "../typeDecl.h"
#include "../config.h"

namespace Objects
{
	DECLARE_POINTER_CLASS(BaseCommand)
		DECLARE_POINTER_CLASS(DeployMobileCommand)
		DECLARE_POINTER_CLASS(EntityConstructionCommand)

	DECLARE_POINTER_CLASS(Hangar)
	DECLARE_POINTER_CLASS(MobileEntityProducer)
	DECLARE_POINTER_CLASS(SkyLinkStructure)

	DECLARE_POINTER_CLASS(BaseEntity)
		DECLARE_POINTER_CLASS(WorldEntity)
			DECLARE_POINTER_CLASS(Structure)
				DECLARE_POINTER_CLASS(Connector)
				DECLARE_POINTER_CLASS(Tower)
				DECLARE_POINTER_CLASS(WorkingStructure)
					DECLARE_POINTER_CLASS(SkyPort)
					DECLARE_POINTER_CLASS(VehiclePlant)
					DECLARE_POINTER_CLASS(VehicleStore)
					DECLARE_POINTER_CLASS(Mine)
					DECLARE_POINTER_CLASS(NexusPort)
					DECLARE_POINTER_CLASS(Generator)
					DECLARE_POINTER_CLASS(ReactorCore)
					DECLARE_POINTER_CLASS(MunitionFactory)
					DECLARE_POINTER_CLASS(MunitionStorage)

			DECLARE_POINTER_CLASS(MobileEntity)
				#ifndef NO_SATELLITES
				DECLARE_POINTER_CLASS(Satellite)
					DECLARE_POINTER_CLASS(SkyHub)
					DECLARE_POINTER_CLASS(SkyGuardian)
					DECLARE_POINTER_CLASS(Ragnarok)
				#endif
				DECLARE_POINTER_CLASS(Vehicle)

	namespace Component
	{
		class Base;
		class Turret;
		class VehicleBase;
	}
	DECLARE_POINTER_CLASS(TurretBase);
	//typedef std::shared_ptr<Base>	PVehicleComponent;

	DECLARE_POINTER_CLASS(Faction);

	typedef PFaction		OwnerParameter;


	CONSTRUCT_ENUMERATION7(RemovalType,
		Vanished,
		BeingReplaced,
		ControlledExplosion,
		UncontrolledExplosion,
		EnteredStorage,
		DestroyedInStorage,
		ChangingOwnership
	);

	class Compound;

	DECLARE_POINTER_CLASS(CommandGroup)


	CONSTRUCT_ENUMERATION7(MobilePlacementResult,
		PlacementOkay,
		PlacementOkayFellDown,
		PlacementFailedTemporarilyObstructed,		//!< Something temporary is in the way
		PlacementFailedPermanentlyObstructed,		//!< Something permanently is in the way. This includes mobile entities without motion tasks
		PlacementFailedHarmfulOrBlocked,	//!< Either moving there would hurt or something permanently blocking is in the way
		PlacementFailedMobileDestroyed,	//!< Movement failed, vehicle cannot be returned automatically to any save location, and should be destroyed by the caller
		EnteredStructure
	);

}


#endif

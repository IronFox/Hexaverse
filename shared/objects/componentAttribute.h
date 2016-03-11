#ifndef componentAttributeH
#define componentAttributeH

#include <general/enumeration.h>

namespace Objects
{
	namespace Component
	{
		CONSTRUCT_ENUMERATION11(Attribute, 
			Health,
			ScanningRange,
			CarryWeight,
			AttacksPerRound,
			AttackRange,
			ExplosionRadius,
			DamageLevel,
			MovementSpeed,
			ConstructionSpeed,
			FuelDemand,
			AmmoNeed
			);
	}
}

#endif
#include <global_root.h>
#include "componentAttribute.h"
#include <container/hashtable.h>


namespace Objects
{
	namespace Component
	{
		static IndexTable<String>				map0;
		static StringTable<Attribute::type_t>	map1;
		/*static*/ void		Attribute::Map::Set(type_t t, const String&s)
		{
			map0.Set(t,s);
			map1.Set(s,t);
		}

		Attribute::Map	Attribute::map;

		

		Attribute::Map::Map()
		{
			Set(Health,"Health");
			Set(ScanningRange,"Scanning Range");
			Set(AttacksPerRound,"Attacks per Round");
			Set(AttackRange,"Attack Range");
			Set(ExplosionRadius,"Explosion Radius");
			Set(DamageLevel,"Damage Level");
			Set(MovementSpeed,"Movement Speed");
			Set(ConstructionSpeed,"Construction Speed");
			#ifndef NO_SATELLITES
				Set(FuelDemand,"Fuel Demand");
			#endif
			#ifndef NO_MUNITION
				Set(AmmoNeed,"Ammo Need");
			#endif

			ASSERT_EQUAL__(map0.Count(),N);
			ASSERT_EQUAL__(map1.Count(),N);
		}

		/*static*/ const String&	Attribute::Map::ToString(type_t t)
		{
			const String*rs = map0.QueryPointer(t);
			if (rs)
				return *rs;
			DBG_FATAL__("Trying to convert unexpected attribute integer to string ("+String(t)+")");
			static String empty="";
			return empty;
		}

		/*static*/ bool		Attribute::Map::Decode(const String&s, type_t&t)
		{
			return map1.Query(s,t);
		}

	}
}

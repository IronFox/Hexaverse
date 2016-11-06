#ifndef componentAttributeH
#define componentAttributeH

#include <global_string.h>

namespace Objects
{
	namespace Component
	{
		struct Attribute
		{
			enum type_t
			{
				Health,
				ScanningRange,
				//CarryWeight,
				AttacksPerRound,
				AttackRange,
				ExplosionRadius,
				DamageLevel,
				MovementSpeed,
				ConstructionSpeed,
				#ifndef NO_SATELLITES
					FuelDemand,
				#endif
				#ifndef NO_MUNITION
					AmmoNeed,
				#endif

				N,
				Default = Health
			};

			type_t	value=Default;

			static struct Map
			{
				static void		Set(type_t, const String&);

				/**/				Map();
				static const String&ToString(type_t);
				static bool			Decode(const String&, type_t&);
			}		map;

			/**/	Attribute()	{}
			/**/	Attribute(type_t t):value(t)	{}

			template <typename Primitive>
			bool	Load(Primitive p)
			{
				index_t i = (index_t)p;
				if (i >= N)
					return false;
				value = (type_t)i;
				return true;
			}

			template <typename Primitive>
			static Attribute Reinterpret(Primitive p)
			{
				index_t i = (index_t)p;
				if (i >= N)
					return Attribute();
				return Attribute((type_t)i);
			}

			const String& ToString() const
			{
				return Map::ToString(value);
			}

			bool	Decode(const String&str)
			{
				return Map::Decode(str,value);
			}


			operator type_t() const {return value;}
		};
	}
}

#endif
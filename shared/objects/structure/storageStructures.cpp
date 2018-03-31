#include <global_root.h>
#include "storageStructures.h"
#include "../interface.h"
#include "../gridAndChoices.h"
#include "../components.h"
#include "../../runtimeConfig.h"

namespace Objects
{
	namespace StorageCommon
	{
		void	HandleStorageDestruction(const Grid::LCellIdI&location,const ResourceVector&storage,float&atTime)
		{
			#ifndef CLIENT
				if (!RuntimeConfig::storagesExplodeWhenDestroyed)
					return;

				auto cause = currentExplosionAttack ? currentExplosionAttack->attackFrom : nullptr;
				
				const bool doomsday = RuntimeConfig::doomsdayWeapons != RuntimeConfig::DoomsdayWeapons::Disallow;
				const float bombExplosionRadius = Component::Bomb::GetRadius(false,nullptr);
				const float nukeExplosionRadius = Component::Bomb::GetRadius(doomsday,nullptr);
				const float bombDamage = Component::Bomb::GetDamageLevel(false,nullptr);
				const float nukeDamage = Component::Bomb::GetDamageLevel(doomsday,nullptr);

				const float ff = float(storage.level[Resource::Fuel]) / MaxStorage;
				const float uf = float(storage.level[Resource::Uranium]) / MaxStorage;

				const float myDamage = 
						ff * bombDamage
					+	uf * nukeDamage;
				const float myRadius =
						ff * bombExplosionRadius
					+	uf * nukeExplosionRadius;
				if (myRadius < 2 * Grid::UnitScale)
					return;

				atTime += timeEpsilon;
				ApplicationInterface::SignalChainReactionExplosion(cause,location,myRadius,myDamage, myRadius > bombExplosionRadius, atTime);
				atTime += timeEpsilon;
			#endif
		}
	}
}

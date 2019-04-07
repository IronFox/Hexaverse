#include <global_root.h>
#include "storageStructures.h"
#include "../interface.h"
#include "../gridAndChoices.h"
#include "../components.h"
#include "../../runtimeConfig.h"
#include "../research/globalState.h"

namespace Objects
{
	namespace StorageCommon
	{

		float	GetUncontrolledExplosionRadius(const Resource::Vector&storage, float*damageOut, bool*doomsdayOut)
		{
			const bool doomsday = RuntimeConfig::doomsdayWeapons != RuntimeConfig::DoomsdayWeapons::Disallow;
			const auto&state = Research::globalState.zeroTable;

			const float bombExplosionRadius = Component::Bomb::GetRadius(false,state);
			const float nukeExplosionRadius = Component::Bomb::GetRadius(doomsday,state);
			const float bombDamage = Component::Bomb::GetDamageLevel(false,state);
			const float nukeDamage = Component::Bomb::GetDamageLevel(doomsday,state);

			const float ff = float(storage[Resource::Fuel]) / MaxStorage;
			const float uf = float(storage[Resource::Uranium]) / MaxStorage;

			if (damageOut)
				*damageOut = 
					ff * bombDamage
				+	uf * nukeDamage;
			const float myRadius =
					ff * bombExplosionRadius
				+	uf * nukeExplosionRadius;
			if (myRadius <= 2)
				return 0;
			if (doomsdayOut)
				*doomsdayOut = myRadius > bombExplosionRadius;
			return myRadius;
		}

		void	HandleStorageDestruction(const Grid::LCellIdI&location,const Resource::Vector&storage,float&atTime)
		{
			#ifndef CLIENT
				if (!RuntimeConfig::storagesExplodeWhenDestroyed)
					return;
				
				float myDamage;
				bool doomsday;
				const float myRadius = GetUncontrolledExplosionRadius(storage,&myDamage,&doomsday);
				if (myRadius <= 0)
					return;

				auto cause = currentExplosionAttack ? currentExplosionAttack->attackFrom : nullptr;
				atTime += timeEpsilon;
				ApplicationInterface::SignalChainReactionExplosion(cause,location,myRadius,myDamage, doomsday, atTime);
				atTime += timeEpsilon;
			#endif
		}
	}
}

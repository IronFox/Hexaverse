#include "explosion.h"
#include "../visual/scene/explosions.h"
#include "../objectNetworkHelper.h"

namespace Animation
{

	Explosion::Explosion(Protocol::Explosion&ex):Super(ex.timestamp,ExplosionTime,TUniqueID::Empty())
	{
		explosionCenter = ex.center;
		explosionRadius = ex.radius;
		empExplosion = ex.isEMP;
	}

	/*virtual override*/ void Explosion::Begin()
	{
		#ifdef HAS_WINDOW
			Explosions::InitExplosion(explosionCenter,empExplosion,explosionRadius*4.f,Objects::ExplosionTime(explosionRadius));
		#endif
	}

	/*virtual override*/ void Explosion::End()
	{
		for (index_t i = 0; i < Objects::Marker::all.Count(); i++)
		{
			const Objects::Marker&m = Objects::Marker::all[i];
			if (m.type != Objects::Marker::Type::HostileContact)
				continue;
			if (Vec::quadraticDistance(Grid::ToWorld(m.location),explosionCenter) < M::Sqr(explosionRadius))
			{
				LogEvent(EventSignificance::Common,nullptr,m.GetName()+ " considered near occuring explosion. Dropping marker");
				ObjectHelper::SignalMarkerDeletion(m.id);
				i--;
			}
		}
	}

	/*virtual override*/ String Explosion::Describe() const
	{
		return "Explosion("+ToString(explosionCenter)+" r"+String(explosionRadius)+", emp="+String(empExplosion)+")";
	}
}


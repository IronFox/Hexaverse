#include "explosion.h"
#include "../visual/scene/explosions.h"
#include "../objectNetworkHelper.h"

namespace Animation
{

	Explosion::Explosion(Protocol::Explosion&ex):Super(TUniqueID::Empty())
	{
		explosionCenter = ex.center;
		explosionRadius = ex.radius;
		empExplosion = ex.isEMP;
	}

	/*virtual override*/ void Explosion::Begin( bool instant )
	{
		#ifdef HAS_WINDOW
			Explosions::InitExplosion(explosionCenter,empExplosion,explosionRadius*4.f,Objects::ExplosionTime(explosionRadius));
		#endif
	}

	/*virtual override*/ void Explosion::End()
	{
		Objects::MarkerStore::client.Foreach(Objects::MarkerType::HostileContact,
			[this](const Objects::PMarker&m)
			{
				if (Vec::quadraticDistance(Grid::ToWorld(m->location),explosionCenter) < M::Sqr(explosionRadius))
				{
					LogEvent(EventSignificance::Common,nullptr,m->GetName()+ " considered near occuring explosion. Dropping marker");
					ObjectHelper::SignalMarkerDeletion(m->id);
				}
			});
	}

	/*virtual override*/ String Explosion::Describe() const
	{
		return "Explosion("+ToString(explosionCenter)+" r"+String(explosionRadius)+", emp="+String(empExplosion)+")";
	}
}


#include <global_root.h>
#include "hibernating.h"

#ifndef NO_HIBERNATION

#include "../logEvent.h"
#include "entity.h"
#include "gridAndChoices.h"
#include "rendering.h"


namespace Objects
{

	bool						Hibernating::UpdateHibernation(WorldEntity*self,  bool hibernate, float atTime)
	{
		if (hibernate == isHibernating)
			return false;
		isHibernating = hibernate;
		#ifndef CLIENT
			self->RegisterChange(WorldEntity::HibernationChanged,atTime);
			if (hibernate)
				LogEvent(EventClass::Preset::operationAborted,self,"Uplink bandwidth exhausted: Entering hibernation");
			else
				LogEvent(EventSignificance::Common,self,"Uplink restored. Exiting hibernation");
		#else
			LogEvent(EventSignificance::Debug,self,"Hibernation change ("+String(hibernate)+")");
			if (hibernate)
			{
				#ifdef HAS_WINDOW
					PTurretBase b = self->GetTurretBase();
					if (b && b->turret->continuousRotation)
						Renderer::animatedTurrets.FindAndErase(b);
				#endif
				onlineUplinkTraffic -= self->GetUplinkTraffic();
			}
			else
			{
				#ifdef HAS_WINDOW
					PTurretBase b = self->GetTurretBase();
					if (b && b->turret->continuousRotation)
						Renderer::animatedTurrets << b;
				#endif
				onlineUplinkTraffic += self->GetUplinkTraffic();
			}
		#endif
		return true;

	}


}
#endif
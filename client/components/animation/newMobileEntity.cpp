#include "newMobileEntity.h"
#include <expectedBehavior.h>
#include "delayedMobileInsertion.h"
#include "../visual/hud/hexGrid.h"

namespace Animation
{
	NewMobileEntity::NewMobileEntity(Protocol::NewMobileEntity&ne, const Objects::PMobileEntity&m):Super(ne.state.entityState.timestamp,0,ne.state.entityState.id)
	{
		allocatedTarget = m;
	}

	/*virtual override*/ void NewMobileEntity::Begin()
	{
		EXPECT(allocatedTarget);
		EXPECT1(allocatedTarget->health > 0,allocatedTarget->id);
		Objects::PMobileEntity v = To<Objects::MobileEntity>::DynamicCast(allocatedTarget);
		if (v)
		{
			LogEvent(EventSignificance::Common,allocatedTarget.get(),  "Placing mobile entity (@"+String(allocatedTarget.get())+", health "+String(allocatedTarget->health)+") during animation at time "+String(currentTime)+"/"+ToString(this->time));
			Objects::PWorldEntity existing = Objects::GetEntity(v->id);
			if (existing)
			{
				//could throw an exception here, but observed that if this happens, it happens A LOT in quick succession
				LogEvent(EventClass::Preset::protocolError,existing.get(),"Trying to double-spawn as "+v->GetTypeName()+" (mobile)");
			}
			else
			{
				//MoveSensor(v->preAnimationLocation,v->location,v->turret->scanningRange);
				ASSERT__(!v->isInsertedIntoGrid);
				Objects::PlaceMobileEntity(v,false);
				ASSERT__(!v->isInsertedIntoGrid);
				insertIntoGridLater.Set(v->id);
				#ifdef HAS_WINDOW
					if (!allocatedTarget->isOwnedByMe)
						HUD::HexGrid::RefreshContacts();
				#endif
			}
		}
	}

	/*virtual override*/ D::String		NewMobileEntity::Describe() const
	{
		return "NewMobileEntity("+allocatedTarget->GetNameAndOwner()+")";
	}


}

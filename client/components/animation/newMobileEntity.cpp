#include "newMobileEntity.h"
#include <expectedBehavior.h>
#include "delayedMobileInsertion.h"
#include "../visual/hud/hexGrid.h"
#include "mobileState.h"
#include "../network.h"
#include "../cameraControl.h"

namespace Animation
{
	NewMobileEntity::NewMobileEntity(Protocol::NewMobileEntity&ne):Super(ne.state.entityState.id)
	{
		construct = std::move(ne);
		EXPECT1(construct.state.entityState.health > 0,construct.state.entityState.id);
	}

	/*virtual override*/ void NewMobileEntity::Begin( bool instant )
	{
		{
			Objects::PWorldEntity existing = Objects::GetEntity(construct.state.entityState.id);
			if (existing)
			{
				//could throw an exception here, but observed that if this happens, it happens A LOT in quick succession
				LogEvent(EventClass::Preset::protocolError,existing.get(),"Trying to double-spawn as "+ToString(existing->GetTypeName()));
				return;
			}
		}
		auto v = Objects::SpawnMobileEntity(Network::ResolveFaction(construct.owner),
			construct.state.entityState.id,construct.state.entityState.location,construct.type,false,Network::haveID && Network::myID == construct.owner);

		EXPECT(v);

//		LogEvent(EventSignificance::Common,allocatedTarget.get(),  "Placing mobile entity (@"+String(allocatedTarget.get())+", health "+String(allocatedTarget->health)+") during animation at time "+String(currentTime)+"/"+ToString(this->time));
		
		
		MobileState::Apply(v,construct.state,true, !instant);
		EXPECT1(v->health > 0,v->id);
		ASSERT_EQUAL1__(v->health,construct.state.entityState.health,v->id);
		#ifdef HAS_WINDOW
			if (v->isOwnedByMe && !Network::amLocated)
			{
				Network::amLocated = true;
				Camera::TranslateTo(v->system.w.xyz);
			}
		#endif

		//MoveSensor(v->preAnimationLocation,v->location,v->turret->scanningRange);
		ASSERT__(!v->isInsertedIntoGrid);
		if (v->storedIn.expired())
		{
			Objects::PlaceMobileEntity(v,instant);
			ASSERT_EQUAL__(instant,v->isInsertedIntoGrid);
			if (!instant)
				insertIntoGridLater.Set(v->id);
		}
		#ifdef HAS_WINDOW
			if (!v->isOwnedByMe)
				HUD::HexGrid::RefreshContacts();
		#endif
	}

	/*virtual override*/ D::String		NewMobileEntity::Describe() const
	{
		return "NewMobileEntity("+ToString(construct.state.entityState.id)+",owner="+ToString(construct.owner)+",health="+String(construct.state.entityState.health.value)+")";
	}


}

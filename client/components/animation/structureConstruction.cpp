#include "structureConstruction.h"
#include <expectedBehavior.h>
#include "../visual/hud/hexGrid.h"

namespace Animation
{
	StructureConstruction::StructureConstruction(Protocol::StructureConstruction&s0, const Objects::PStructure&s):Super(s0.state.entityState.timestamp,0,s0.state.entityState.id)
	{
		allocatedTarget = s;
	}

	/*virtual override*/ void StructureConstruction::Begin()
	{
		EXPECT(allocatedTarget);
		EXPECT1(allocatedTarget->health > 0,allocatedTarget->id);
		LogEvent(EventSignificance::Common,allocatedTarget.get(),  "Placing structure entity during animation at time "+String(currentTime)+"/"+ToString(this->time));
		Objects::PWorldEntity existing = Objects::GetEntity(allocatedTarget->id);
		if (existing)
		{
			//could throw an exception here, but observed that if this happens, it happens A LOT in quick succession
			LogEvent(EventClass::Preset::protocolError,existing.get(),"Trying to double-spawn as "+allocatedTarget->GetTypeName()+" (structure)");
		}
		else
		{
			Objects::Place(To<Objects::Structure>::DynamicCast(allocatedTarget));
			#ifdef HAS_WINDOW
				if (!allocatedTarget->isOwnedByMe)
					HUD::HexGrid::RefreshContacts();
			#endif
		}
	}

	/*virtual override*/ D::String		StructureConstruction::Describe() const
	{
		return "StructureConstruction("+allocatedTarget->GetNameAndOwner()+")";
	}


}

#include "structureConstruction.h"
#include <expectedBehavior.h>
#include "../visual/hud/hexGrid.h"
#include "../network.h"
#include "structureState.h"

namespace Animation
{
	StructureConstruction::StructureConstruction(Protocol::StructureConstruction&s0):Super(s0.state.entityState.id)
	{
		construct = std::move(s0);
	}

	/*virtual override*/ void StructureConstruction::Begin()
	{
		{
			Objects::PWorldEntity existing = Objects::GetEntity(construct.state.entityState.id);
			if (existing)
			{
				//could throw an exception here, but observed that if this happens, it happens A LOT in quick succession
				LogEvent(EventClass::Preset::protocolError,existing.get(),"Trying to double-spawn as "+existing->GetTypeName());
				return;
			}
		}
		Objects::PStructure s = Objects::SpawnStructure(Network::ResolveFaction(construct.owner),  
				construct.state.entityState.id,
				construct.state.entityState.location,construct.type, false, Network::haveID && Network::myID == construct.owner);
		ASSERT__(s);
		StructureState::Apply(s,construct.state);
		EXPECT1(s->health > 0,s->id);
		Objects::Place(s);
		#ifdef HAS_WINDOW
			if (!s->isOwnedByMe)
				HUD::HexGrid::RefreshContacts();
		#endif
	}

	/*virtual override*/ D::String		StructureConstruction::Describe() const
	{
		return "StructureConstruction("+ToString(construct.state.entityState.id)+", owner="+ToString(construct.owner)+")";
	}


}

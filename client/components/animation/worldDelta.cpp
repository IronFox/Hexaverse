#include "worldDelta.h"
#include "../visual/scene/world.h"

namespace Animation
{
	WorldDelta::WorldDelta(Protocol::WorldDelta&d):Super(d.timestamp,0,TUniqueID::Empty())
	{
		deltas.Append().swap(d);
	}

	/*virtual override*/ void WorldDelta::Begin()
	{
		foreach (deltas,block)
		{
			BaseGrid::family_t family = block->familyAndLastFlag & 0x7F;
			//bool last = (block->family & 0x80) != 0;
			World::actual.BeginChangeSequence(0).ApplyFamilyDelta(family,block->data0,false,true);
		}
		#ifdef HAS_WINDOW
			World::allowRecompilation = true;
		#endif
	}

	/*virtual override*/ D::String		WorldDelta::Describe() const
	{
		Buffer0<String> deltaIDs;
		foreach (deltas,d)
		{
			BaseGrid::family_t family = d->familyAndLastFlag & 0x7F;
			deltaIDs.Append(String(d->familyAndLastFlag));
		}

		return "WorldDelta("+implode(',',deltaIDs.ToRef())+")";
	}


}

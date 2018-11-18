#include "worldDelta.h"
#include "../visual/scene/world.h"
#include "../network.h"

namespace Animation
{
	WorldDelta::WorldDelta(Protocol::WorldDelta&d):Super(TUniqueID::Empty())
	{
		deltas.Append().swap(d);
		isComplete = (d.familyAndLastFlag & 0x80) != 0;
	}

	/*virtual override*/ void WorldDelta::Begin( bool instant )
	{
		foreach (deltas,block)
		{
			BaseGrid::family_t family = block->familyAndLastFlag & 0x7F;
			//bool last = (block->family & 0x80) != 0;
			World::actual.BeginChangeSequence(0).ApplyFamilyDelta(family,block->data0,false,true);
		}
		#ifdef HAS_WINDOW
			World::allowRecompilation = isComplete && Network::gameHasBegun;
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

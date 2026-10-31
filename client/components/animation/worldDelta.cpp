#include "worldDelta.h"
#include "../visual/scene/world.h"
#include "../network.h"

namespace Animation
{
	WorldDelta::WorldDelta(Protocol::WorldDelta&d):Super(TUniqueID::Empty())
	{
		isComplete = (d.familyAndLastFlag & 0x80) != 0;
		deltas.Append(std::move(d));
	}

	void WorldDelta::Append(Protocol::WorldDelta&&d)
	{
		//DBG_ASSERT__(!isComplete);	//apparently this can mix...
		isComplete = (d.familyAndLastFlag & 0x80) != 0;
		deltas.Append(std::move(d));
	}



	/*virtual override*/ void WorldDelta::Begin( bool instant )
	{
		LogEvent(EventSignificance::Debug,"Terrain: Processing "+String(deltas.Count())+" delta(s)");
		foreach (deltas,block)
		{
			BaseGrid::family_t family = block->familyAndLastFlag & 0x7F;
			//bool last = (block->family & 0x80) != 0;
			World::actual.BeginChangeSequence(0).ApplyFamilyDelta(family,block->data0,false,true);
		}
		#ifdef HAS_WINDOW
			World::allowRecompilation = (!instant || isComplete) && Network::gameHasBegun;
			LogEvent(EventSignificance::Debug,"Terrain: Recompilation allowed set to "+String(World::allowRecompilation)+", with complete="+String(isComplete)+" and gameHasBegun="+String(Network::gameHasBegun));
		#endif
	}

	/*virtual override*/ D::String		WorldDelta::Describe() const
	{
		Vector<String> deltaIDs;
		foreach (deltas,d)
		{
			BaseGrid::family_t family = d->familyAndLastFlag & 0x7F;
			deltaIDs.Append(String(d->familyAndLastFlag));
		}

		return "WorldDelta("+implode(',',deltaIDs.ToRef())+")";
	}


}

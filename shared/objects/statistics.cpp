#include "statistics.h"

namespace Objects
{
	Statistics::Statistics(const Faction*owner):owner(owner)
	{

	}

	void			Statistics::SignalDamageReceivedFrom(const PFaction&source, int level)
	{

	}

	void			Statistics::SignalDamageDealtTo(const PFaction&receiver, int level)
	{

	}
		
	void			Statistics::SignalDestructionOfHostile(const PWorldEntity&)
	{

	}
		
	void			Statistics::SignalLossOf(const PWorldEntity&, const PFaction&causedBy, bool stolen)
	{

	}


}

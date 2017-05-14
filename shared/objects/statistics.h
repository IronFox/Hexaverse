#pragma once

#include "types.h"

namespace Objects
{

	class Statistics
	{
		const Faction*const	owner;
	public:
		/**/			Statistics(const Faction*owner);
		void			SignalDamageReceivedFrom(const PFaction&source, int level);
		void			SignalDamageDealtTo(const PFaction&receiver, int level);
		void			SignalDestructionOfHostile(const PWorldEntity&);
		void			SignalLossOf(const PWorldEntity&, const PFaction&causedBy, bool stolen);
	};

}

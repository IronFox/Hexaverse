#pragma once
#include "../runtimeConfig.h"
#include "types.h"
#include "../grid/types.h"


namespace NexusPresence
{
	
	#ifndef CLIENT
		void Create(bool & placeBases, bool & placeAllPorts, bool &placeSatellites);
	#endif
	void Create(World::Data&world, const Objects::PFaction&nexusFaction, bool & placeBases, bool & placeAllPorts, bool &placeSatellites);
	
}


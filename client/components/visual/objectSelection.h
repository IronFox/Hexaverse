#pragma once
#ifdef HAS_WINDOW

#include <objects/types.h>

namespace Objects
{
	extern PWorldEntity		selectedEntity;
	extern PMobileEntity	selectedMobileEntity;
	extern PVehicle			selectedVehicle;
	extern PMobileEntity	deployingMobileEntity;


	void					Select(const PWorldEntity&entity, bool controlEnabled);
	void					Deselect(bool controlEnabled);

}



#endif

#ifdef HAS_WINDOW
#ifndef objectSelectionH
#define objectSelectionH

#include "vehicle.h"

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
#endif

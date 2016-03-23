#ifdef HAS_WINDOW
#ifndef objectSelectionH
#define objectSelectionH

#include "vehicle.h"

namespace Objects
{
	extern PWorldEntity		selectedEntity;
	extern PMobileEntity	selectedMobileEntity;
	extern PVehicle			selectedVehicle;
	extern bool				techniteIsSelected;
	extern Technites::Technite	selectedTechnite;


	void					Select(const PWorldEntity&entity, bool controlEnabled);
	void					Select(const Technites::Technite&tech, bool controlEnabled);
	void					Deselect(bool controlEnabled);

}



#endif
#endif

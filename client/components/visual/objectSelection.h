#pragma once
#ifdef HAS_WINDOW

#include <objects/types.h>
#include <math/vector.h>

namespace Objects
{
	extern PWorldEntity		selectedEntity;
	extern PMobileEntity	selectedMobileEntity;
	extern PVehicle			selectedVehicle;
	extern PMobileEntity	deployingMobileEntity;


	void					Select(const PWorldEntity&entity, bool controlEnabled);
	bool					Select(const TVec3<>&b, const TVec3<>&d, float&distance,bool controlEnabled);
	void					Deselect(bool controlEnabled);
	PWorldEntity			GetRayIntersection(const TVec3<>&b, const TVec3<>&d, float&distance);

}



#endif

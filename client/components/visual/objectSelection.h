#pragma once
#include <objects/types.h>

#ifdef HAS_WINDOW
#include <math/vector.h>
#include <observer.h>

namespace Objects
{
	extern PWorldEntity		selectedEntity;
	extern PMobileEntity	selectedMobileEntity;
	extern PVehicle			selectedVehicle;
	extern PMobileEntity	deployingMobileEntity;

	extern Observer<>		onSelectionChanged;


	void					Select(const PWorldEntity&entity, bool controlEnabled);
	bool					Select(const TVec3<>&b, const TVec3<>&d, float&distance,bool controlEnabled);
	void					Deselect(bool controlEnabled);
	PWorldEntity			GetRayIntersection(const TVec3<>&b, const TVec3<>&d, float&distance);
	inline bool				IsSelected(const PWorldEntity&e) {return selectedEntity == e;}
}


#else
namespace Objects
{
	inline constexpr bool	IsSelected(const PWorldEntity&) {return false;}
}
#endif

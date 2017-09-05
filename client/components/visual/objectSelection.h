#pragma once
#include <objects/types.h>

#ifdef HAS_WINDOW
#include <math/vector.h>
#include <observer.h>

namespace Objects
{
	using namespace DeltaWorks;

	extern PWorldEntity		selectedEntity;
	extern PMobileEntity	selectedMobileEntity;
	extern PVehicle			selectedVehicle;
	extern PMobileEntity	deployingMobileEntity;

	extern Observable<>		onSelectionChanged;


	void					Select(const PWorldEntity&entity, bool controlEnabled);
	bool					Select(const M::TVec3<>&b, const M::TVec3<>&d, float&distance,bool controlEnabled);
	void					Deselect(bool controlEnabled);
	PWorldEntity			GetRayIntersection(const M::TVec3<>&b, const M::TVec3<>&d, float&distance);
	inline bool				IsSelected(const PWorldEntity&e) {return selectedEntity == e;}




	void					SetShiftMeansQueue(bool);
	bool					ShiftMeansQueue();
	void					SignalShiftPressed();
	void					SignalShiftReleased();
	bool					ShouldQueueCommands();
}


#else
namespace Objects
{
	inline constexpr bool	IsSelected(const PWorldEntity&) {return false;}
}
#endif

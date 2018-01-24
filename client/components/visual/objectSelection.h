#pragma once
#include <objects/types.h>

#ifdef HAS_WINDOW
#include <math/vector.h>
#include <observer.h>
#include <sharedContainer.h>

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



	template <typename T>
	inline void GetIterateableEntity(Objects::PWorldEntity&rs, index_t&i, const SharedContainer<T>&list)
	{
		if (rs)
			return;
		if (i < list.Count())
		{
			rs = list[i];
			return;
		}
		i -= list.Count();
	}

	

	template <typename T>
	inline void FindIterateableEntity(index_t&offset,index_t&rs, const Objects::PWorldEntity&e, const SharedContainer<T>&list)
	{
		if (rs != InvalidIndex)
			return;
		rs = list.GetIndexOf(e);
		if (rs == InvalidIndex)
			offset += list.Count();
		else
			rs += offset;
	}


	count_t			CountIterateableEntities();
	PWorldEntity	GetIterateableEntity(index_t i);
	index_t			FindIterateableEntity(const PWorldEntity&e);
	void			SelectAndFocusNextIdle(bool controlsEnabled);
	void			SelectAndFocusPrev(bool controlsEnabled);
	void			SelectAndFocusNext(bool controlsEnabled);
	bool			IsIdle(const PWorldEntity&e);


}


#else
namespace Objects
{
	inline constexpr bool	IsSelected(const PWorldEntity&) {return false;}
}
#endif

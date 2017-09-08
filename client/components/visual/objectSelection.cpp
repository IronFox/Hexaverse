#ifdef HAS_WINDOW
#include <global_root.h>
#include "objectSelection.h"
#include "objectChoices.h"
#include "objectCommands.h"
#include <objects/interface.h>
#include <objects/gridAndChoices.h>
#include <objects/vehicle.h>
#include <objects/faction.h>
#include <globalEvents.h>

#include "ui.h"

namespace Objects
{

	PVehicle				selectedVehicle;
	PWorldEntity			selectedEntity;
	PMobileEntity			selectedMobileEntity;
	PMobileEntity			deployingMobileEntity;
	Observable<>				onSelectionChanged;
	bool					shiftPressed = false,shiftMeansQueue = true;


	void					SetShiftMeansQueue(bool b)
	{
		shiftMeansQueue = b;
	}

	bool					ShiftMeansQueue()
	{
		return shiftMeansQueue;
	}

	void					SignalShiftPressed()
	{
		bool n = !shiftPressed;
		shiftPressed = true;
		if (shiftMeansQueue && n)
			UI::RefreshButtonState();
	}

	void					SignalShiftReleased()
	{
		bool n = shiftPressed;
		shiftPressed = false;
		if (shiftMeansQueue && n)
			UI::RefreshButtonState();
	}

	bool					ShouldQueueCommands()
	{
		return shiftPressed || !shiftMeansQueue;
	}


	void					Select(const PWorldEntity&entity, bool controlEnabled)
	{
		deployingMobileEntity.reset();
		movementChoices.Clear();
//		bool changed = selectedEntity != entity;
		PWorldEntity e = selectedEntity;
		selectedEntity = entity;
		selectedMobileEntity = To<MobileEntity>::DynamicCast(entity);
		selectedVehicle = To<Vehicle>::DynamicCast(selectedMobileEntity);
		activeCursorMode = CursorMode::Idle;

		if (e != entity)
		{
			if (e)
				e->OnDeselect();
			if (entity)
				entity->OnSelect();
		}
		
		if (e != entity)
			onSelectionChanged();
		GlobalEvents::invalidateObjectCache();
		UpdateChoices(controlEnabled);
	}

	


	void					Deselect(bool controlEnabled)
	{
		deployingMobileEntity.reset();
		movementChoices.Clear();
		PWorldEntity e = selectedEntity;
		selectedEntity.reset();	
		selectedVehicle.reset();
		selectedMobileEntity.reset();
		activeCursorMode = CursorMode::Idle;

		if (e)
			e->OnDeselect();
		Command::RefreshCommands(controlEnabled);
		if (e)
			onSelectionChanged();
		GlobalEvents::invalidateObjectCache();
	}


	PWorldEntity					GetRayIntersection(const M::TVec3<>&b, const M::TVec3<>&d, float&distance)
	{
		PWorldEntity newSel;
		ForeachWorldEntity([b,d,&distance,&newSel](const Objects::PWorldEntity&e)
		{
			if (e->TestIntersection(b, d, distance))
			{
				newSel = e;
			}
		},true);

		return newSel;
	}

	bool					Select(const M::TVec3<>&b, const M::TVec3<>&d, float&distance,bool controlEnabled)
	{
		//if (Compound::Transfer::isActive)
			//return false;

		//selectedEntity.reset();
		PWorldEntity newSel = GetRayIntersection(b,d,distance);

		//bool success = !!selectedEntity;
		if (newSel)
		{
			Select(newSel,controlEnabled);
		}
		#ifdef HAS_WINDOW
			else
				Command::RefreshCommands(controlEnabled);	//in UpdateChoices() otherwise
		#endif
		return !!newSel;
	}
}

#endif

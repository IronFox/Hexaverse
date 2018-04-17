#ifdef HAS_WINDOW
#include <global_root.h>
#include "objectSelection.h"
#include "objectChoices.h"
#include <objects/interface.h>
#include <objects/gridAndChoices.h>
#include <objects/vehicle.h>
#include <objects/faction.h>
#include <globalEvents.h>
#include "scene/vehicleAccessibilityMesh.h"

#include "hud/hexGrid.h"

namespace Objects
{

	PVehicle				selectedVehicle;
	PWorldEntity			selectedEntity;
	PMobileEntity			selectedMobileEntity;
	PMobileEntity			deployingMobileEntity;
	Observable<>			onSelectionChanged;
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
		{
			HUD::HexGrid::RefreshButtonState();
			VehicleAccessibilityMesh::Refresh();
		}

	}

	void					SignalShiftReleased()
	{
		bool n = shiftPressed;
		shiftPressed = false;
		if (shiftMeansQueue && n)
		{
			HUD::HexGrid::RefreshButtonState();
			VehicleAccessibilityMesh::Refresh();
		}
	}

	bool					ShouldQueueCommands()
	{
		return shiftPressed || !shiftMeansQueue || Engine::input.pressed[Key::Alt];
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
		{
			//Debug	2018 04 04. 13:06:57	42	CommandPhase			Hover Artillery	940786dd	<NULL>	527[h=1]	Selected
			LogEvent(EventSignificance::Debug,entity.get(),"Selecting: onSelectionChanged()");	//okay here
			//crashed somewhere after this. Selected artillery was ghost of previous visible, now invisible Nexus unit (still alive)
			//other instances selected structure ghosts. no apparent connection to mobile units
			onSelectionChanged();
			LogEvent(EventSignificance::Debug,entity.get(),"Selecting: GlobalEvents::invalidateObjectCache()");	//new
		}
		GlobalEvents::invalidateObjectCache();
		if (e != entity)
			LogEvent(EventSignificance::Debug,entity.get(),"Selecting: UpdatedChoices("+String(controlEnabled)+")");//new
		UpdateChoices(controlEnabled);
		if (e != entity)
			LogEvent(EventSignificance::Debug,entity.get(),"Selected");//new
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
		if (e)
		{
			LogEvent(EventSignificance::Debug,"Deselected");
			onSelectionChanged();
		}

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
		},Objects::IncludeGhosts::All);

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
				HUD::HexGrid::RefreshButtonState();	//in UpdateChoices() otherwise
		#endif
		return !!newSel;
	}
}

#endif

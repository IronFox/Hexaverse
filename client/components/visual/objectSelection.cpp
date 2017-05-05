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


namespace Objects
{

	PVehicle				selectedVehicle;
	PWorldEntity			selectedEntity;
	PMobileEntity			selectedMobileEntity;
	PMobileEntity			deployingMobileEntity;
	Observable<>				onSelectionChanged;

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
		
		GlobalEvents::invalidateObjectCache();
		if (e != entity)
			onSelectionChanged();
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
		GlobalEvents::invalidateObjectCache();
		if (e)
			onSelectionChanged();
	}


	PWorldEntity					GetRayIntersection(const TVec3<>&b, const TVec3<>&d, float&distance)
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

	bool					Select(const TVec3<>&b, const TVec3<>&d, float&distance,bool controlEnabled)
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

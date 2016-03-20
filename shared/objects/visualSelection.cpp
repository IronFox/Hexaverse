#ifdef HAS_WINDOW
#include <global_root.h>
#include "visualSelection.h"
#include "visualChoices.h"
#include "interface.h"
#include "gridAndChoices.h"

namespace Objects
{

	PVehicle				selectedVehicle;
	PWorldEntity			selectedEntity;
	PMobileEntity			selectedMobileEntity;

	void					Select(const PWorldEntity&entity, bool controlEnabled)
	{
			
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
			
		OnSelectionChange(e == entity);
		UpdateChoices(controlEnabled);
	}

	


	void					Deselect(bool controlEnabled)
	{
		movementChoices.Clear();
		PWorldEntity e = selectedEntity;
		selectedEntity.reset();	
		selectedVehicle.reset();
		selectedMobileEntity.reset();
		activeCursorMode = CursorMode::Idle;

		if (e)
			e->OnDeselect();
		RefreshCommands(controlEnabled);
		OnSelectionChange(!e);
	}
}

#endif

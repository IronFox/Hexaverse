#ifdef HAS_WINDOW
#include <global_root.h>
#include "objectSelection.h"
#include "objectChoices.h"
#include "objectCommands.h"
#include <objects/interface.h>
#include <objects/gridAndChoices.h>
#include <objects/vehicle.h>

namespace Objects
{

	PVehicle				selectedVehicle;
	PWorldEntity			selectedEntity;
	PMobileEntity			selectedMobileEntity;
	PMobileEntity			deployingMobileEntity;

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
			
		ApplicationInterface::OnSelectionChange(e == entity);
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
		RefreshCommands(controlEnabled);
		ApplicationInterface::OnSelectionChange(!e);
	}
}

#endif

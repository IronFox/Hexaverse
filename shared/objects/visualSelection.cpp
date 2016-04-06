#ifdef HAS_WINDOW
#include <global_root.h>
#include "visualSelection.h"
#include "visualChoices.h"
#include "interface.h"
#include "gridAndChoices.h"
#include "../technite.h"

namespace Objects
{

	PVehicle				selectedVehicle;
	PWorldEntity			selectedEntity;
	PMobileEntity			selectedMobileEntity;
	PMobileEntity			deployingMobileEntity;
	bool					techniteIsSelected = false;
	Technites::Technite		selectedTechnite;


	void					Select(const Technites::Technite&tech, bool controlEnabled)
	{
		Deselect(controlEnabled);
		techniteIsSelected = true;
		selectedTechnite = tech;
		OnSelectionChange(false);
		UpdateChoices(controlEnabled);
	}


	void					Select(const PWorldEntity&entity, bool controlEnabled)
	{
		bool wasTechnite = techniteIsSelected; 
		techniteIsSelected = false;
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
			
		OnSelectionChange(e == entity && !wasTechnite);
		UpdateChoices(controlEnabled);
	}

	


	void					Deselect(bool controlEnabled)
	{
		bool wasTechnite = techniteIsSelected; 
		techniteIsSelected = false;
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
		OnSelectionChange(!e && !wasTechnite);
	}
}

#endif

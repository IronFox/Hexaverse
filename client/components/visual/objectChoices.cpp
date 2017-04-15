#ifdef HAS_WINDOW

#include <global_root.h>
#include "objectChoices.h"
#include <objects/entity.h>
#include <objects/gridAndChoices.h>
#include <math/object.h>
#include "objectSelection.h"
#include <objects/interface.h>
#include "objectCommands.h"
#include "terrain.h"

namespace Objects
{

	bool						controlIsEnabled = false;
	Buffer0<Choice>				movementChoices;
	TChoiceVisualization		choiceVis;
	Buffer0<Grid::HCellIdU>		buildCandidates, 
								removeCandidates;
	Buffer0<Grid::HCellIdI>		cellSelection;






	void	ReapplyTrajectoryType()
	{
		if (choiceVis.showTrajectory)
		{
			if (choiceVis.paraboloidTrajectory)
			{
				VisualTerrain::SetParaboloidDisplay(
					Grid::low.cells[choiceVis.arrowFrom.cellLocation.stackID].cornerCenter,
					choiceVis.arrowFrom.radius,
					choiceVis.flatTrajectoryRange,
					float4(1,0,0,0.5f));
			}
			else
			{
				VisualTerrain::SetSphereDisplay(
					choiceVis.arrowFrom.worldLocation,
					choiceVis.flatTrajectoryRange,
					float4(1,0,0,0.5f));
			}
		}
		else
			VisualTerrain::UnsetParaboloidAndSphereDisplay();
	}




	void					UpdateChoices(bool controlEnabled) 
	{
		//activeCursorMode = 

		Command::RefreshCommands(controlEnabled);

		if (controlEnabled)
			GetChoices(selectedEntity,
				deployingMobileEntity, 
				activeCursorMode,movementChoices,choiceVis);
		else
		{
			movementChoices.Clear();
			choiceVis = TChoiceVisualization();
		}
		ReapplyTrajectoryType();
	}





	




	index_t					DetectChoiceIntersection(const TVec3<>&b, const TVec3<>&d, float&distance)
	{
		index_t rs = InvalidIndex;
		foreach(movementChoices, choice)
		{
			float h = Grid::ToRadius(choice->location.height + choice->visualVerticalOffset);
			const auto&c = Grid::low.cells[choice->location.stackID];
			for (int i = 0; i < c.numCorners; i++)
			{
				if (Obj::detectOpticalIntersection(
					c.centerVertex.xyz * h,
					c.cornerVertex[i].xyz * h,
					c.cornerVertex[(i + 1) % c.numCorners].xyz * h,
					b, d, distance
					))
				{
					rs = choice - movementChoices.pointer();
				}

			}

		}
		return rs;
	}

	bool					HandleChoiceIntersection(const TVec3<>&b, const TVec3<>&d, float distance)
	{
		index_t choice = DetectChoiceIntersection(b, d, distance);
		if (choice == InvalidIndex)
			return false;
		PMobileEntity deployment = deployingMobileEntity;
		//VehicleStore::deployingVehicle.reset();
		CursorMode cursorMode = activeCursorMode;
		activeCursorMode = CursorMode::Idle;

		Grid::LCellIdI ch = movementChoices[choice].location;
		if (To<Hangar>::DynamicCast(selectedEntity))
			deployingMobileEntity.reset();

		UpdateChoices(true);


		ApplicationInterface::SignalChoiceIntersection(selectedEntity, ch, cursorMode,deployment);

		return true;
	}



	bool			DisableModes(bool controlEnabled)
	{
		if (activeCursorMode == CursorMode::Idle)
			return false;
		buildCandidates.Clear();
		removeCandidates.Clear();
		activeCursorMode = CursorMode::Idle;
		UpdateChoices(controlEnabled);
		return true;
	}

}




#endif

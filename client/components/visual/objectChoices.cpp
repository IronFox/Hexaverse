#ifdef HAS_WINDOW

#include <global_root.h>
#include "visualObjectChoices.h"
#include <objects/entity.h>
#include <objects/gridAndChoices.h>
#include <math/object.h>
#include "visualObjectSelection.h"
#include <objects/interface.h>
#include "visualObjectCommands.h"

namespace Objects
{

	bool						controlIsEnabled = false;
	Buffer0<Choice>				movementChoices;
	bool						hasArrowCellTarget=false,
								showArrow = false;
	float3						arrowFrom;	//arrow visualization point of origin. Might be the location of a vehicle, or any of the locations in its task queue. Only relevant if showArrow is true
	Grid::LCellIdI				arrowFromCell;
	TrajectoryEndPoint			arrowCellTarget;
	Buffer0<Grid::HCellIdU>		buildCandidates, 
								removeCandidates;
	Buffer0<Grid::HCellIdI>		cellSelection;



	template <bool ControlEnabled>
		void RefreshCommand(const int2&, const PBaseCommand&cmd)
		{
			cmd->Refresh(ControlEnabled);
		}

	void		RefreshCommands(bool controlEnabled)
	{
		controlIsEnabled = controlEnabled;
		globalCommands.Refresh(controlEnabled);
	
		if (selectedEntity)
		{
			selectedEntity->VisitAllCommands(controlEnabled ? RefreshCommand<true> : RefreshCommand<false>,false);
		}
	}



	void					UpdateChoices(bool controlEnabled) 
	{
		//activeCursorMode = 

		RefreshCommands(controlEnabled);

		if (controlEnabled)
			GetChoices(selectedEntity,
				deployingMobileEntity, 
				activeCursorMode,movementChoices,showArrow,hasArrowCellTarget,arrowFrom,arrowFromCell);
		else
		{
			movementChoices.Clear();
			showArrow = false;
			hasArrowCellTarget = false;
		}
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



}




#endif

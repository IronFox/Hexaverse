#pragma once

#include <objects/types.h>
#include <grid/stack.h>
#include <objects/vehicle.h>
#include <container/array.h>
#include <protocol.h>
using namespace DeltaWorks;


namespace ObjectHelper
{
	void	SignalMarkerDeletion(TUniqueID markerID);	//must be copy to prevent weird shit
	void	SignalMarkerCreation(Protocol::Marker&marker);
	void	SignalMarkerTouch(Objects::Marker&marker);
	bool	DirectTo(const Objects::PWorldEntity&v, const ArrayRef<Grid::LCellIdI>&targets, UINT8 flags, Objects::Vehicle::mode_t mode, bool interactive);
	UINT8	AssembleMotionFlags(bool detailedPath, bool leftClick, bool clearSafeTasks);
	UINT8	AssembleMotionFlags(bool detailedPath, bool leftClick);
	bool	DirectTo(const Objects::PWorldEntity&v, const ArrayRef<Grid::LCellIdI>&targets, UINT8 flags, Objects::Vehicle::mode_t mode);

	void	InnerUndoTask(const Objects::PWorldEntity&e, bool force);

	void	SendDirectTo(const Objects::PWorldEntity&v, const ArrayRef<Grid::LCellIdI>&targets, UINT8 flags, Objects::Vehicle::mode_t mode);
	bool	MoveVehicleTo(const Objects::PMobileEntity&v, const Grid::LCellIdI&prev, Grid::LCellIdI target, UINT8 flags);

	bool	CanHit(const Objects::PMobileEntity&m, const Grid::LCellIdI&loc);
	bool	IsIdle(const Objects::PMobileEntity&v);
	bool	IsIdle(const Objects::PStructure&s);
	bool	IsIdle(const Objects::PWorldEntity&e);

	bool	IsTerraformingOperation(ConstName jobName);
	void	SignalNewJob(const Objects::PMobileEntity&v, ConstName jobName, Objects::EntityType parameterName, const Grid::LCellIdI&overrideLocation);


	namespace MarkerCheck
	{
		void	Create();
		bool	DoneThisRound();
	}
}


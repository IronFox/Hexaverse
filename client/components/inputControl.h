#pragma once

#include <objects/types.h>
#include <grid/stack.h>
#include <sharedContainer.h>
#include <objects/vehicle.h>




namespace LocalApplicationInterface
{
	bool	DirectTo(const Objects::PWorldEntity&v, const ArrayRef<Grid::LCellIdI>&targets, UINT8 flags, Objects::Vehicle::mode_t mode, bool interactive);
	UINT8	AssembleMotionFlags(bool detailedPath, bool leftClick, bool clearSafeTasks);
	UINT8	AssembleMotionFlags(bool detailedPath, bool leftClick);
	bool	DirectTo(const Objects::PWorldEntity&v, const ArrayRef<Grid::LCellIdI>&targets, UINT8 flags, Objects::Vehicle::mode_t mode);
}


namespace InputControl
{
	void	SaveScreenshot();
	void	ToggleCameraLock();
	void	ToggleTimeLapse();
	void	ToggleSeeThrough();
	void	ViewDown();
	void	ViewUp();
	void	InnerUndoTask(const Objects::PWorldEntity&e, bool force);

	void	SendDirectTo(const Objects::PWorldEntity&v, const ArrayRef<Grid::LCellIdI>&targets, UINT8 flags, Objects::Vehicle::mode_t mode);
	bool	MoveVehicleTo(const Objects::PMobileEntity&v, const Grid::LCellIdI&prev, Grid::LCellIdI target, UINT8 flags);
	

	bool	CanHit(const Objects::PMobileEntity&m, const Grid::LCellIdI&loc);
	bool	IsIdle(const Objects::PMobileEntity&v);
	bool	IsIdle(const Objects::PStructure&s);
	bool	IsIdle(const Objects::PWorldEntity&e);
	#ifdef HAS_WINDOW



		/**
		Retrieves the low resolution cell id of the cell under the current cursor location
		*/
		bool	GetLowCellUnderCursor(Grid::LCellIdI&result, bool includeWater, bool includeEntities);


		void	ToggleControlGUI();
		void	BeginCameraDrag();
		void	EndCameraDrag(const bool AlsoControlsUnits);

		template <bool AlsoControlsUnits>
		void	EndCameraDrag()	{EndCameraDrag(AlsoControlsUnits);}
		void	UndoTask();
		void	QueryAndSendChatMessage();





		extern bool saveScreenshot,
					saveTransparentScreenshot;

		extern Timer::Time rightMouseDragBegun;
		extern bool rightMouseDragged;
		extern int2 rightMouseDragFrom;


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


		count_t		CountIterateableEntities();

		Objects::PWorldEntity	GetIterateableEntity(index_t i);
		index_t	FindIterateableEntity(const Objects::PWorldEntity&e);

		void	SelectAndFocusNextIdle();
		void	SelectAndFocusPrev();
		void	SelectAndFocusNext();
		void	ResumeBatchConstruction();
		void	AbortBatchConstruction();
		void	OnWheel(short delta);


	#endif

}

#include <global_root.h>
#include "config.h"

#if CELL_LIFE

#include "cellLife.h"


namespace CellLife
{

	bool	LiveCell::DecodeAbsoluteTarget(Grid::HCellIdI & outTarget) const
	{
		TTarget target;
		target.Decompress(compressedTarget);
		if (target.heightDelta < -1 || target.heightDelta > 1)
			return false;
		if (location.stackID >= Grid::high.grid.CellCount)
			return false;
		if (location.height < Grid::CellStack::BottomHeight || location.height > Grid::CellStack::TopHeight)
			return false;
		const auto&n = Grid::high.grid.GetCell(location.stackID).neighborhood;
		if (target.neighborIndex >= n.numNeighbors)
			return false;
		//ASSERT_LESS__(target.neighborIndex,n.numNeighbors);
		outTarget =  Grid::HCellIdI(n.neighborID[ target.neighborIndex],location.height + target.heightDelta);
		return outTarget.height >= Grid::CellStack::BottomHeight && outTarget.height <= Grid::CellStack::TopHeight;
	}


	CellFaction::ExecResult			CellFaction::ExecuteTask(LiveCell&cell)
	{
		if (cell.nextTask == LiveCell::Task::None)
			return ExecResult::NothingToDo;
		Grid::HCellIdI target;
		if (!cell.DecodeAbsoluteTarget(target))
			return ExecResult::BadCommand;
		const auto&targetCell = Grid::actualWorld.cellStack[target.stackID].GetCellI(target.height);

		if (cell.TargetMustBeClear())
		{
			if (targetCell.content != Grid::Content::Clear)
				return ExecResult::TargetConditionNotSuitable;
		}
		if (cell.TargetMustBeMyLiveCell())
		{
			if (targetCell.content != Grid::Content::LiveCell || targetCell.cellFaction != this->gridID)
				return ExecResult::TargetConditionNotSuitable;
		}

		if (cell.TargetMustNotBeMyLiveCell())
		{
			if (targetCell.content == Grid::Content::LiveCell && targetCell.cellFaction == this->gridID)
				return ExecResult::TargetConditionNotSuitable;
		}


		switch (cell.nextTask)
		{
			case LiveCell::Task::GrowTo:
			{
				if (cell.attributes.energy < 5 && cell.attributes.food < 5)
					return ExecResult::CellConditionNotSuitable;
				SpawnCell(target);
				cell.attributes.energy -= 5;
				cell.attributes.food -= 5;
			}
			break;
			case LiveCell::Task::EatMoveTo:
			{
				BYTE prev = targetCell.content;
				Grid::actualWorld.cellStack[target.stackID].MakeLocalHole(target.height);
				cell.attributes.food = (BYTE)std::min(255,(int)cell.attributes.food + Grid::CellStack::Cell::typeStrength[prev]);
				if (prev == Grid::Content::LiveCell)
				{
					PCellFaction f = GetCellFactionByGridID(targetCell.cellFaction);
					ASSERT1__(f.get() != this,f->globalID.ToString());
					f->KillCellByBeingEaten(target,cell.attributes);
				}
			}
			break;
			case LiveCell::Task::TransferEnergyTo:
			{
				index_t targetIndex;
				if (cellMap.Query(target,targetIndex))
				{
					

				}
				else
					FATAL__("Consistency violation: Grid says target cell "+target.ToString()+" contains owned cell, but no such is known"); 
			}
			break;


		}


	}



	void			CellFaction::SplitCell(const Grid::HCellIdI&cellID, const Grid::HCellIdI&newCellLocation, const LifeCell::Attributes&transfer)
	{

	}

	void		Create();
	void		Destroy();





}


#endif

#include <global_root.h>
#include "config.h"

#if TECHNITES

#include "technite.h"


namespace Technites
{
	/*static*/ const Technite::Resources	Technite::SplitCost = Resources(SplitEnergyCost, SplitMatterCost),
											Technite::MoveCost = Resources(MoveEnergyCost, 0),
											Technite::EatCost = Resources(EatEnergyCost, 0),
											Technite::EnergyPackage = Resources(EnergyPackageSize,0),
											Technite::MatterPackage = Resources(0,MatterPackageSize);



	bool	Technite::DecodeAbsoluteTarget(Grid::HCellIdI & outTarget) const
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



	Faction::ExecResult			Faction::GrowTo(Technite&technite, const Grid::HCellIdI&target, const Grid::CellStack::Cell&targetCell)
	{
		ExecResult rs = Eat(technite, target, targetCell);
		if (rs != ExecResult::Success)
			return rs;

		if (!ReduceResources(technite, Technite::SplitCost))
		{
			return ExecResult::TechniteLacksResources;
		}

		SpawnNewTechnite(target);
		return ExecResult::Success;
	}

	Faction::ExecResult			Faction::MoveTo(Technite&technite, const Grid::HCellIdI&target, const Grid::CellStack::Cell&targetCell)
	{
		ExecResult rs = Eat(technite, target, targetCell);
		if (rs != ExecResult::Success)
			return rs;

		if (!ReduceResources(technite, Technite::MoveCost))
		{
			return ExecResult::TechniteLacksResources;
		}

		MoveTechnite(technite, target);
		return ExecResult::Success;
	}
	Faction::ExecResult			Faction::Eat(Technite&technite, const Grid::HCellIdI&target, const Grid::CellStack::Cell&targetCell)
	{
		switch (targetCell.content)
		{
			case Grid::Content::Clear:
				IncreaseResources(technite, EatEntitiesIn(target));
				return ExecResult::Success;
			case Grid::Content::Technite:
			{
				BYTE faction = targetCell.techniteFaction;
				PFaction f = GetFactionOf(faction);
				Technite::Resources resources = f->EatTechnite(target);
				IncreaseResources(technite, resources);
			}
			return ExecResult::Success;
			case Grid::Content::Lava:
				return ExecResult::CannotEatThis;
		}
		if (!ReduceResources(technite, Technite::MoveCost))
		{
			return ExecResult::TechniteLacksResources;
		}
		Technite::Resources	resources;
		Grid::Content newContent = DegradeMatter(target,resources);
		IncreaseResources(technite, resources);
		return newContent == Grid::Content::Clear ? ExecResult::Success : ExecResult::MoreWorkNeeded;
	}

	Faction::ExecResult			Faction::ExecuteTask(Technite&technite)
	{
		if (technite.nextTask == Technite::Task::None)
			return ExecResult::NothingToDo;
		Grid::HCellIdI target;
		if (!technite.DecodeAbsoluteTarget(target))
			return ExecResult::BadCommand;
		const auto&targetCell = Grid::actualWorld.cellStack[target.stackID].GetCellI(target.height);

		switch (technite.nextTask)
		{
			case Technite::Task::GrowTo:
				return GrowTo(technite, target, targetCell);
			case Technite::Task::EatMoveTo:
				return MoveTo(technite, target, targetCell);
			case Technite::Task::TransferEnergyTo:
			{
				Technite*other = GetOwnTechnite(target);
				if (!other)
					return ExecResult::NoTechniteAtDestination;
				if (!ReduceResources(technite, Technite::EnergyPackage))
					return ExecResult::TechniteLacksResources;
				IncreaseResources(*other, Technite::EnergyPackage);
				return ExecResult::Success;
			}
			case Technite::Task::TransferMatterTo:
			{
				Technite*other = GetOwnTechnite(target);
				if (!other)
					return ExecResult::NoTechniteAtDestination;
				if (!ReduceResources(technite, Technite::MatterPackage))
					return ExecResult::TechniteLacksResources;
				IncreaseResources(*other, Technite::MatterPackage);
				return ExecResult::Success;
			}
		}
		return ExecResult::NothingToDo;
	}




	void		Create()
	{

	}
	void		Destroy()
	{


	}





}


#endif

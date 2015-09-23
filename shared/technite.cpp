#include <global_root.h>
#include "config.h"

#if TECHNITES

#include "technite.h"
#include "objects/gridAndChoices.h"
#include "objects/structure.h"
#include "objects/vehicle.h"
#include <general/thread_system.h>

namespace Technites
{
	/*static*/ const Technite::Resources	Technite::SplitCost = Resources(SplitEnergyCost, SplitMatterCost),
		Technite::MoveCost = Resources(MoveEnergyCost, 0),
		Technite::EatCost = Resources(EatEnergyCost, 0),
		Technite::EnergyPackage = Resources(EnergyPackageSize, 0),
		Technite::MatterPackage = Resources(0, MatterPackageSize);

	/*static*/ Buffer0<PFaction>Faction::all;
	/*static*/ AtomicGrid Faction::atomicGrid;
	///*static*/ WorkPipe<Grid::HCellIdI>	Faction::newTechnites;
	///*static*/ WorkPipe<Grid::HCellIdI> Faction::moveInstructions;
	///*static*/ WorkPipe<std::pair<Grid::HCellIdI, Technite::Resources> > Faction::increaseResources;
	/*static*/ PFaction			Faction::gridIDMap[0xFF];

	/*static*/ const BYTE	Technite::MatterYield[NumGroundTypes] =
	{
		0,	//Unknown
		10,	//Granite
		1,	//Earth
		2,	//Grass
		7,	//Rock
		3,	//Sand
		0,	//Snow
		10,	//Foundation
		5,	//Road
		0,	//Lava
		0,	//Technite
		0,	//Water
		0,	//Clear
	};
	/*static*/ const BYTE	Technite::ChangeTypeTo[NumGroundTypes] =
	{
		Grid::Content::Unknown,	//Unknown
		Grid::Content::Rock,	//Granite
		Grid::Content::Clear,	//Earth
		Grid::Content::Earth,	//Grass
		Grid::Content::Earth,	//Rock
		Grid::Content::Earth,	//Sand
		Grid::Content::Clear,	//Snow
		Grid::Content::Rock,	//Foundation
		Grid::Content::Rock,	//Road
		Grid::Content::Lava,	//Lava
		Grid::Content::Technite,	//Technite
		Grid::Content::Water,	//Water
		Grid::Content::Clear,	//Clear
	};


	struct TEaten
	{
		std::atomic<bool>	structureEaten = false,
							mobileEaten = false;
	};

	Array<TEaten>			eatenGrid(Grid::LowSetup::GridType::CellCount * Grid::CellStack::Range);


	static Random random;

	struct FactionRandomizer
	{
		PFaction			target;
		unsigned			sortWeight;

		bool				operator>(const FactionRandomizer&other) const
		{
			return sortWeight > other.sortWeight;
		}
		bool				operator<(const FactionRandomizer&other) const
		{
			return sortWeight < other.sortWeight;
		}

		void				Set(const PFaction&f)
		{
			target = f;
			sortWeight = random.get();
		}

	};


	static index_t atFaction = 0;
	static Array<FactionRandomizer>	processingOrder;


	/*static*/ void				BeginProcessing()
	{
		atFaction = 0;

		Faction::atomicGrid.Import(Grid::actualWorld);



		processingOrder.SetSize(Faction::all.Count());

		for (index_t i = 0; i < Faction::all.Count(); i++)
		{
			PFaction f = Faction::all[i];
			processingOrder[i].Set(f);
			f->ProcessLighting();
		}

		std::sort(processingOrder.begin(),processingOrder.end());
	}


	static bool CheckIsEaten(const Objects::PMobileEntity&m)
	{
		auto&cell = eatenGrid[Objects::CellToGridIndex(m->location)];
		if (!cell.mobileEaten)
			return false;
		cell.mobileEaten = false;
		return true;
	}

	static bool CheckIsEaten(const Objects::PStructure&s)
	{
		auto&cell = eatenGrid[Objects::CellToGridIndex(s->location)];
		if (!cell.structureEaten)
			return false;
		cell.structureEaten = false;
		return true;
	}

	/*static*/ bool				ProcessNext(float t)
	{
		if (atFaction < processingOrder.Count())
		{
			processingOrder[atFaction].target->Process(t);
			atFaction++;
		}

		if (atFaction >= processingOrder.Count())
		{
			Faction::atomicGrid.ExportChanged(Grid::actualWorld, t);

			static Buffer0<Objects::PWorldEntity>	toKill;
			for (const auto&mobile : Objects::vehicles)
			{
				if (CheckIsEaten(mobile))
					toKill << mobile;
			}
			for (const auto&structure : Objects::structures)
			{
				if (CheckIsEaten(structure))
					toKill << structure;
			}

			for (const auto&e : toKill)
				Objects::Remove(e,Objects::RemovalType::UncontrolledExplosion,t,true);
			toKill.Clear();

			return false;
		}
		return true;
	}


	/*static*/ PFaction Faction::GetFactionOf(BYTE factionID)
	{
		return gridIDMap[factionID];
	}

	Technite::Resources		Faction::EatEntitiesIn(const Grid::HCellIdI&target, bool&cantEatThis)
	{
		cantEatThis = false;
		Objects::PWorldEntity	all[4];
		int count = 0;
		const auto mapping = Grid::highToLowMap[target.stackID];
		Objects::PWorldEntity entity;
		for (int k = 0; k < 2; k++)
			if (mapping.lowCells[k] != Grid::THighToLowCell::Empty)
			{
				Grid::LCellIdI at(mapping.lowCells[k],target.height);
				TEaten&eaten = eatenGrid[Objects::CellToGridIndex(at)];
				entity = Objects::GetMobileEntity(at);
				bool ref;
				ref = false;
				if (entity)
					if (!entity->IsIndestructible())
					{
						if (eaten.mobileEaten.compare_exchange_strong(ref,true))
							all[count++] = entity;
					}
					else
						cantEatThis = true;
				entity = Objects::GetStructure(at);
				ref = false;
				if (entity)
					if (!entity->IsIndestructible())
					{
						if (eaten.structureEaten.compare_exchange_strong(ref,true))
							all[count++] = entity;
					}
					else
						cantEatThis = true;
			}

		if (count == 0)
			return Technite::Resources();
		Technite::Resources result;
		for (int i = 0; i < count; i++)
			result.matter += std::max(1,all[i]->GetConstructionCost()/2);
		return result;
	}



	void				AtomicGrid::Import(const Grid::WorldData&data)
	{
		for (index_t i = 0; i < data.cellStack.Count(); i++)
		{
			AtomicGrid::CellStack&to = cellStack[i];
			const Grid::CellStack&from = data.cellStack[i];

			for (UINT32 l = 0; l < Grid::CellStack::Range; l++)
			{
				BYTE fc = from.volumeCell[l].content;
				to.volumeCell[l].content = fc;
			}
		}
		for (index_t i = 0; i < ARRAYSIZE(familiesChanged); i++)
			familiesChanged[i] = false;
	}

	void				AtomicGrid::SignalStackChange(UINT32 stackID)
	{
		familiesChanged[Grid::high.grid.GetCell(stackID).family] = true;
	}

	void				AtomicGrid::ExportChanged(Grid::WorldData&outData, float t)
	{
		for (BYTE family = 0; family < BaseGrid::NumFamilies; family++)
		{
			if (!familiesChanged[family])
				continue;

			Grid::low.family[family].SetOutdated(t);
			Grid::high.family[family].SetOutdated(t);


			index_t offset = Grid::high.grid.GetFamilyOffset(family);
			count_t len = Grid::high.grid.CountFamilyMembers(family);
			for (index_t i = offset; i < offset + len; i++)
			{
				const AtomicGrid::CellStack&from = cellStack[i];
				Grid::CellStack&to = outData.cellStack[i];

				for (UINT32 l = 0; l < Grid::CellStack::Range; l++)
				{
					to.volumeCell[l].content = from.volumeCell[l].content;
				}
			}
		}
	}


	/*static*/ Grid::Content	Faction::GetGlobalGridContent(const Grid::HCellIdI&cellID)
	{
		if (cellID.height < Grid::CellStack::BottomHeight)
			return Grid::Content::Reinterpret(Grid::CellStack::bottom.content);
		if (cellID.height > Grid::CellStack::TopHeight)
			return Grid::Content::Clear;
		UINT32 layer = Grid::CellStack::ToLayer(cellID.height);
		DBG_ASSERT_LESS__(layer, Grid::CellStack::Range);

		return Grid::Content::Reinterpret(Grid::actualWorld.cellStack[cellID.stackID].volumeCell[layer].content);

	}


	/*static*/Grid::Content			Faction::GetLastContent(const Grid::HCellIdI&cellID)
	{
		if (cellID.height < Grid::CellStack::BottomHeight)
			return Grid::Content::Reinterpret(Grid::CellStack::bottom.content);
		if (cellID.height > Grid::CellStack::TopHeight)
			return Grid::Content::Clear;
		UINT32 layer = Grid::CellStack::ToLayer(cellID.height);
		DBG_ASSERT_LESS__(layer, Grid::CellStack::Range);
		
		return Grid::Content::Reinterpret(atomicGrid.cellStack[cellID.stackID].volumeCell[layer].content);
	}
	Grid::Content			Faction::GetLastContent(const Grid::HCellIdU&cellID)
	{
		return Grid::Content::Reinterpret(atomicGrid.cellStack[cellID.stackID].volumeCell[cellID.layer].content);
	}
	Grid::Content			Faction::DegradeMatter(const Grid::HCellIdI&target, Technite::Resources&outYield)
	{
		outYield = Technite::Resources();
		UINT32 layer = Grid::CellStack::ToLayer(target.height);
		ASSERT_LESS__(layer, Grid::CellStack::Range);
		AtomicGrid::CellStack::Cell&cell = atomicGrid.cellStack[target.stackID].volumeCell[layer];
		for (;;)
		{

			BYTE current = cell.content;
			BYTE matterYield = 0;
			BYTE set = Technite::ChangeTypeTo[current];

			matterYield = Technite::MatterYield[current];

			if (set == current)
				return Grid::Content::Reinterpret(set);

			if (cell.content.compare_exchange_weak(current, set))
			{
				atomicGrid.SignalStackChange(target.stackID);

				outYield = Technite::Resources::Resources(0, matterYield);
				return Grid::Content::Reinterpret(set);
			}
		}
	}


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
		outTarget = Grid::HCellIdI(n.neighborID[target.neighborIndex], location.height + target.heightDelta);
		return outTarget.height >= Grid::CellStack::BottomHeight && outTarget.height <= Grid::CellStack::TopHeight;
	}



	Faction::ExecResult			Faction::GrowTo(Technite&technite, const Grid::HCellIdI&target, index_t iteration, WorkerData&data)
	{
		ExecResult rs = Eat(technite, target, iteration,data);
		
		if (rs != ExecResult::Success)
			return rs;

		if (iteration < 2)
			return ExecResult::Again;

		
		if (!ReduceResources(technite, Technite::SplitCost))
		{
			return ExecResult::TechniteLacksResources;
		}
		

		if (!SpawnNewTechnite(target, data))
		{
			//this actually can happen if the local faction recently spawned a new technite there, 
			//which is registered in the grid but not in the faction yet, so it cannot be eaten
			IncreaseResources(technite, Technite::SplitCost);
			return ExecResult::CannotStandThere;
		}
		
		return ExecResult::Success;
	}

	Faction::ExecResult			Faction::MoveTo(Technite&technite, const Grid::HCellIdI&target, index_t iteration, WorkerData&data)
	{
		ExecResult rs = Eat(technite, target, iteration,data);
		
		if (rs != ExecResult::Success)
			return rs;

		
		if (iteration < 3)
			return ExecResult::Again;

		if (!ReduceResources(technite, Technite::MoveCost))
		{
			return ExecResult::TechniteLacksResources;
		}
		

		if (!MoveTechnite(technite, target,data))
		{
			
			IncreaseResources(technite, Technite::MoveCost);
			return ExecResult::CannotStandThere;

		}
		
		return ExecResult::Success;
	}


	index_t					BroadTechniteMap::MapCluster(const Grid::HCellIdI&loc, index_t&outFamily, UINT32&heightRemainder) const
	{
		return MapCluster(Grid::HCellIdU(loc.stackID, Grid::CellStack::ToLayer(loc.height)),outFamily,heightRemainder);
	}
	index_t					BroadTechniteMap::MapCluster(const Grid::HCellIdU&loc, index_t&outFamily, UINT32&heightRemainder) const
	{
		outFamily = Grid::high.grid.GetCell(loc.stackID).family;
		heightRemainder = loc.layer % SectionLayers;
		return outFamily * HeightSections
			+ loc.layer / SectionLayers;
	}

	index_t					BroadTechniteMap::Map(const Grid::HCellIdI&location, Cluster*&outCluster, bool initIfEmpty)
	{
		UINT32 layer = Grid::CellStack::ToLayer(location.height);
		index_t family;
		UINT32 heightRemainder;
		index_t clusterIndex = MapCluster(Grid::HCellIdU(location.stackID, layer), family, heightRemainder);
		//ASSERT_LESS__(heightRemainder, SectionLayers);
		//ASSERT_LESS__(clusterIndex, ARRAYSIZE(cluster));
		outCluster = cluster + clusterIndex;
		count_t range = Grid::high.grid.CountFamilyMembers((BYTE)family);
		index_t inner = (location.stackID - Grid::high.grid.GetFamilyOffset((BYTE)family));
		//ASSERT_LESS__(inner, range);
		if (initIfEmpty && outCluster->volumeCells.IsEmpty())
		{
			outCluster->volumeCells.SetSize(range * SectionLayers);
			outCluster->volumeCells.Fill(Empty);
		}
		return inner + range * heightRemainder;
	}

	index_t					BroadTechniteMap::Map(const Grid::HCellIdI&location, const Cluster*&outCluster) const
	{
		UINT32 layer = Grid::CellStack::ToLayer(location.height);
		index_t family;
		UINT32 heightRemainder;
		index_t clusterIndex = MapCluster(Grid::HCellIdU(location.stackID, layer), family,heightRemainder);
		//ASSERT_LESS__(heightRemainder, SectionLayers);
		//ASSERT_LESS__(clusterIndex, ARRAYSIZE(cluster));
		outCluster = cluster + clusterIndex;
		//const count_t numFamilies = BaseGrid::NumFamilies;
		index_t inner = (location.stackID - Grid::high.grid.GetFamilyOffset((BYTE)family));
		count_t range = Grid::high.grid.CountFamilyMembers((BYTE)family);
		//ASSERT_EQUAL__(family, Grid::high.grid.GetCell(location.stackID).family);
		//ASSERT_LESS__(inner, range);
		return inner + range * heightRemainder;
	}

	void					BroadTechniteMap::Unset(const Grid::HCellIdI& id)
	{
		Cluster*cluster;
		index_t inner = Map(id, cluster,false);
		if (cluster->numEntries == 0)
		{
			FATAL__("Failed to unset " + id.ToString());
			return;
		}
		if (cluster->volumeCells[inner] != Empty)
		{
			cluster->volumeCells[inner] = Empty;
			cluster->numEntries--;
			totalEntries--;
		}
		else
		{
			FATAL__("Failed to unset " + id.ToString());
		}

	}

	void					BroadTechniteMap::Set(const Grid::HCellIdI&id, index_t to)
	{
		Cluster*cluster;
		index_t inner = Map(id, cluster,true);
		if (cluster->volumeCells[inner] == Empty)
		{
			cluster->numEntries++;
			totalEntries++;
		}
		cluster->volumeCells[inner] = (UINT32)to;
	}

	bool					BroadTechniteMap::Query(const Grid::HCellIdI&id, index_t&out) const
	{
		const Cluster*cluster;
		index_t inner = Map(id, cluster);
		if (cluster->numEntries == 0)
		{
			out = InvalidIndex;
			return false;
		}
		UINT32 v = cluster->volumeCells[inner];
		if (v == Empty)
		{
			out = InvalidIndex;
			return false;
		}
		out = (index_t)v;
		return true;
	}

	bool					BroadTechniteMap::IsSet(const Grid::HCellIdI&id) const
	{
		const Cluster*cluster;
		index_t inner = Map(id, cluster);
		if (cluster->numEntries == 0)
			return false;
		UINT32 v = cluster->volumeCells[inner];
		return v != Empty;
	}

	bool					BroadTechniteMap::SetNew(const Grid::HCellIdI&id, index_t to)
	{
		Cluster*cluster;
		index_t inner = Map(id, cluster, true);
		UINT32&v = cluster->volumeCells[inner];
		if (v != Empty)
			return false;
		cluster->numEntries++;
		totalEntries++;
		v = (UINT32)to;
		return true;
	}


	count_t					BroadTechniteMap::Count() const
	{
		return totalEntries;
	}

	Technite::Resources		Faction::EatTechnite(Technite&local, const Grid::HCellIdI&target, bool factionIsActive)
	{
		//	if (!factionIsActive)
		{
			index_t at;
			if (!this->techniteMap.Query(target, at))
			{
				//freshly created technite, most likely
				ASSERT__(factionIsActive);
				//UINT32 layer = Grid::CellStack::ToLayer(target.height);
				//ASSERT_LESS__(layer, Grid::CellStack::Range);
				//atomicGrid.cellStack[target.stackID].volumeCell[layer].content = Grid::Content::Clear;
				//atomicGrid.SignalStackChange(target.stackID);
				return Technite::Resources();	//never been there. weird, but whatever
			}
			

			ASSERT_LESS__(at, technites.Count());
			Technite&tech = technites[at];

			if (!factionIsActive)
			{
				//eating someone else's technites causes a battle
				if (local.resources.energy >= tech.resources.energy)
				{
					//std::cout << local.location.ToString() << " eating remote technite (i have "<<(int)local.resources.energy<<", other has "<<(int)tech.resources.energy<<") " << tech.location.ToString() << std::endl;
					local.resources.energy -= tech.resources.energy;
					//all good tho
				}
				else
				{
					//fail
					tech.resources.energy -= local.resources.energy;
					local.resources.energy = 0;
					//std::cout << local.location.ToString() << " failed eating remote technite (i have " << (int)local.resources.energy << ", other has " << (int)tech.resources.energy << ") " << tech.location.ToString() << ". remote energy decreased to " << (int)tech.resources.energy << std::endl;
					return Technite::Resources();
				}

			}


			if (tech.isDead.Set(false, true))
			{
				//std::cout << local.location.ToString() << " killing off remote technite " << tech.location.ToString() << std::endl;

				if (factionIsActive)
				{
					//std::cout << "setting flag" << std::endl;
					anyDied = true;
				}
				UINT32 layer = Grid::CellStack::ToLayer(target.height);
				ASSERT_LESS__(layer, Grid::CellStack::Range);
				atomicGrid.cellStack[target.stackID].volumeCell[layer].content = Grid::Content::Clear;
				atomicGrid.SignalStackChange(target.stackID);
				
				const Technite::Resources&source = tech.resources;

				return Technite::Resources(source.energy/4, source.energy / 2);
			}
			
			return Technite::Resources();
		}
	}


	Faction::ExecResult			Faction::Eat(Technite&technite, const Grid::HCellIdI&target, index_t iteration, WorkerData&data)
	{
		
		switch (GetLastContent(target))
		{
		case Grid::Content::Clear:	//clear never changes. Once it's empty, it's empty
		{
			bool cantEatThis;
			IncreaseResources(technite, EatEntitiesIn(target,cantEatThis));	//this will be hard to make thread-safe
			
			return cantEatThis ? ExecResult::CannotEatThis : ExecResult::Success;
		}
		case Grid::Content::Technite:
		{
			if (iteration < 1)
				return ExecResult::Again;
			
			BYTE faction = Grid::actualWorld.cellStack[target.stackID].GetCellI(target.height).techniteFaction;	//this should be thread-safe with only one faction being parallely active at any given time
			
			PFaction f = GetFactionOf(faction);
			
			Technite::Resources resources = f->EatTechnite(technite,target, f.get() == this);	//if technite has been eaten in the meantime EatTechnite will return empty resources
			
			IncreaseResources(technite, resources);
			
		}
		return GetLastContent(target) == Grid::Content::Clear ? ExecResult::Success : ExecResult::CannotEatThis;
		case Grid::Content::Lava:
			
			return ExecResult::CannotEatThis;
		}
		if (!ReduceResources(technite, Technite::EatCost))
		{
			
			return ExecResult::TechniteLacksResources;
		}
		Technite::Resources	resources;
		
		Grid::Content newContent = DegradeMatter(target, resources);	//DegradeMatter must be atomic on the specified cell
		
		IncreaseResources(technite, resources);
		
		return newContent == Grid::Content::Clear ? ExecResult::Success : ExecResult::MoreWorkNeeded;
	}


	void					Faction::ProcessLighting(Technite&technite)
	{
		bool lit = true;


		unsigned layer = Grid::CellStack::ToLayer(technite.location.height + 1);
		for (; layer < Grid::CellStack::Range; layer++)
		{
			Grid::Content top = GetLastContent(Grid::HCellIdU(technite.location.stackID, layer));
			if (top != Grid::Content::Clear)
			{
				lit = false;
				break;
			}
		}

		Technite::TState state = technite.DecodeState();
		state.lit = lit;
		technite.compressedState = state.Compress();

		if (lit)
		{
			Technite::Resources::Increase(technite.resources.energy, technite.GetLightEnergyYield());
		}
	}

	void							Faction::CheckState(Technite&technite)
	{

		bool cannotStandHereDies = !ValidTechniteLocation(technite.location);

		Grid::Content content = GetLastContent(technite.location);




		Technite::TState state;
		state.Decompress(technite.compressedState);
		BYTE cost = technite.GetLifeCost();
		if (state.ttl > cost)
		{
			state.ttl -= cost;
		}
		else
			state.ttl = 0;
		technite.compressedState = state.Compress();

		if (content != Grid::Content::Technite
			|| Grid::actualWorld.cellStack[technite.location.stackID].GetCellI(technite.location.height).techniteFaction != this->gridID
			|| state.ttl == 0
			|| cannotStandHereDies
			)
		{
			//std::cout << "Technite died at " << technite.location.ToString() << std::endl;
			//std::cout << " Cannot stand here: " << (cannotStandHereDies ? "true" : "false") << std::endl;
			//std::cout << " Technite content lost: " << (content != Grid::Content::Technite ? "true" : "false") << std::endl;
			//std::cout << " TTL exhausted: " << (state.ttl == 0 ? "true" : "false") << std::endl;
			technite.isDead = true;	//world could have changed by other influences (like explosions)
			anyDied = true;
			
		}
		
	}

	Technite::TaskResult			Faction::ExecuteTask(Technite&technite, index_t iteration, WorkerData&data)
	{
		if (technite.isDead)
			return Technite::TaskResult::NothingToDo;
		Grid::HCellIdI target;
		if (!technite.DecodeAbsoluteTarget(target))
			return Technite::TaskResult::BadCommand;
		//		const auto&targetCell = Grid::actualWorld.cellStack[target.stackID].GetCellI(target.height);


		if (technite.nextTask == Technite::Task::None)
			return Technite::TaskResult::NothingToDo;

		switch (technite.nextTask)
		{
		case Technite::Task::GrowTo:
			return GrowTo(technite, target, iteration,data);
		case Technite::Task::EatMoveTo:
			return MoveTo(technite, target, iteration, data);
		case Technite::Task::TransferEnergyTo:
		{
			if (iteration < 4)
				return ExecResult::Again;
			Technite*other = GetOwnTechnite(target);
			if (!other)
				return Technite::TaskResult::NoTechniteAtDestination;
			if (!ReduceResources(technite, Technite::EnergyPackage))
				return Technite::TaskResult::TechniteLacksResources;
			IncreaseRemoteResources(*other, Technite::EnergyPackage, data);
			return Technite::TaskResult::Success;
		}
		case Technite::Task::TransferMatterTo:
		{
			if (iteration < 4)
				return ExecResult::Again;
			Technite*other = GetOwnTechnite(target);
			if (!other)
				return Technite::TaskResult::NoTechniteAtDestination;
			if (!ReduceResources(technite, Technite::MatterPackage))
				return Technite::TaskResult::TechniteLacksResources;
			IncreaseRemoteResources(*other, Technite::MatterPackage, data);
			return Technite::TaskResult::Success;
		}
		}
		return Technite::TaskResult::NothingToDo;
	}

	//ParallelLoop	loop();


	void		Faction::ProcessLighting()
	{
		Concurrency::parallel_for(index_t(0), technites.Count(), [this](index_t i)
		{
			ProcessLighting(technites[i]);
		});
	}

	void		Faction::Process(float t)
	{
		anyDied = false;

		//std::cout << "processing faction " << (int)gridID << std::endl;
		
		Concurrency::parallel_for(index_t(0), technites.Count(), [this](index_t i)
		{
			CheckState(technites[i]);
			technites[i].taskResult = Technite::TaskResult::Again;
		});
		
		count_t numWorkers = ParallelLoop::global_instance.getWorkerCount();
		static Array<WorkerData>	workerData;
		workerData.SetSize(numWorkers);


		for (index_t iteration = 0; ; iteration++)
		{
			volatile bool anyAgain = false;




			parallelFor(technites.Count(), 64, [this, &anyAgain, iteration](index_t i, index_t workerIndex)
			{
				Technite&tec = technites[i];
				if (tec.isDead || tec.taskResult != Technite::TaskResult::Again)
					return;
				Technite::TaskResult rc = tec.taskResult = ExecuteTask(tec, iteration, workerData[workerIndex]);
				if (rc != Technite::TaskResult::MoreWorkNeeded)
				{
					
					if (rc == ExecResult::Again)
						anyAgain = true;
					else
						tec.nextTask = Technite::Task::None;
				}
			});
			if (!anyAgain)
				break;
		}
		

		if (anyDied)
		{
			//std::cout << "some died" << std::endl;

			static Buffer<Technite>	newField;
			//techniteMap.Clear();
			for (const auto&tec : technites)
				if (!tec.isDead)
				{
					index_t at = newField.Count();
					newField.Append(tec);
					techniteMap.Set(tec.location, at);
				}
				else
				{
					visibleWorld.ChangeSensory(tec.location, t, -1);
					techniteMap.Unset(tec.location);
				}

			newField.swap(technites);
			newField.Clear();
			ASSERT_EQUAL__(technites.Count(), techniteMap.Count());
		}

		
		foreach(workerData, data)
		{
			for (const auto&loc : data->newTechnites)
			{
				if (techniteMap.IsSet(loc))
					continue;

				index_t at = technites.Count();
				Technite tech;
				tech.location = loc;

				//Grid::actualWorld.cellStack[tech.location.stackID].SetLocalTechniteFaction(tech.location.height, this->gridID);
				technites.Append(tech).isDead = false;

				techniteMap.Set(tech.location, at);
				visibleWorld.ChangeSensory(tech.location, t, 1);
			}
			data->newTechnites.Clear();
			ASSERT_EQUAL__(technites.Count(), techniteMap.Count());
		}

		
		foreach(workerData, data)
		{
			for (const auto&inc : data->increaseResources)
			{
				index_t at;
				ASSERT1__(techniteMap.Query(inc.first, at), inc.first.ToString());
				Technite&tec = technites[at];
				tec.resources += inc.second;
			}
			data->increaseResources.Clear();
		}

		foreach(workerData, data)
		{
			for (const auto&loc : data->moveInstructions)
			{
				index_t at;
				ASSERT1__(techniteMap.Query(loc.first, at), loc.first.ToString());
				Technite&tec = technites[at];
				ASSERT__(tec.location == loc.first);
				if (techniteMap.SetNew(loc.second, at))
				{
					tec.location = loc.second;
					techniteMap.Unset(loc.first);
				}
				else
				{
					//this case actually should never happen. so fatal, for now
					FATAL__("Cannot move to reserved location " + loc.second.ToString()+" some other local technite is there");
					index_t other;
					ASSERT__(techniteMap.Query(loc.second, other));
					Technite&otec = technites[other];
					tec.taskResult = Technite::TaskResult::CannotStandThere;
				}
			}
			data->moveInstructions.Clear();
			ASSERT_EQUAL__(technites.Count(), techniteMap.Count());
		}
		
		
		
	}


	bool			Faction::MoveTechnite(Technite&technite, const Grid::HCellIdI&target, WorkerData&data)
	{
		UINT32 fromLayer = Grid::CellStack::ToLayer(technite.location.height),
			toLayer = Grid::CellStack::ToLayer(target.height);
		ASSERT_LESS__(fromLayer, Grid::CellStack::Range);
		ASSERT_LESS__(toLayer, Grid::CellStack::Range);

		AtomicGrid::CellStack::Cell&fromCell = atomicGrid.cellStack[technite.location.stackID].volumeCell[fromLayer];
		AtomicGrid::CellStack::Cell&toCell = atomicGrid.cellStack[target.stackID].volumeCell[toLayer];

		BYTE expect0 = Grid::Content::Clear;
		BYTE expect1 = Grid::Content::Water;
		if (!toCell.content.compare_exchange_strong(expect0, Grid::Content::Technite)
			&&
			!toCell.content.compare_exchange_strong(expect1, Grid::Content::Technite))
			return false;
		atomicGrid.SignalStackChange(target.stackID);
		Grid::actualWorld.cellStack[target.stackID].volumeCell[toLayer].techniteFaction = this->gridID;
		fromCell.content = Grid::Content::Clear;
		atomicGrid.SignalStackChange(technite.location.stackID);
		data.moveInstructions << std::make_pair(technite.location,target);
		//std::cout << "Moving technite from " << technite.location.ToString() << " to " << target.ToString() << std::endl;
		return true;
	}

	Technite* Faction::GetOwnTechnite(const Grid::HCellIdI&target)
	{
		index_t at;
		if (!techniteMap.Query(target, at))
			return nullptr;
		Technite*rs = technites + at;
		if (rs->isDead)
			return nullptr;
		return rs;
	}

	bool			Faction::SpawnNewTechnite(const Grid::HCellIdI&target, WorkerData&data)
	{
		UINT32 toLayer = Grid::CellStack::ToLayer(target.height);
		ASSERT_LESS__(toLayer, Grid::CellStack::Range);

		AtomicGrid::CellStack::Cell&toCell = atomicGrid.cellStack[target.stackID].volumeCell[toLayer];

		BYTE expect0 = Grid::Content::Clear;
		BYTE expect1 = Grid::Content::Water;
		if (!toCell.content.compare_exchange_strong(expect0, Grid::Content::Technite)
			&&
			!toCell.content.compare_exchange_strong(expect1, Grid::Content::Technite))
			return false;
		atomicGrid.SignalStackChange(target.stackID);
		Grid::actualWorld.cellStack[target.stackID].volumeCell[toLayer].techniteFaction = this->gridID;

		//std::cout << "Creating technite at " << target.ToString() << std::endl;

		data.newTechnites << target;
		return true;
	}

	void			Faction::IncreaseResources(Technite&technite, const Technite::Resources&by)
	{
		technite.resources += by;
	}

	bool			Faction::ReduceResources(Technite&technite, const Technite::Resources&by)
	{
		return technite.resources -= by;
	}

	void			Faction::IncreaseRemoteResources(Technite&technite, const Technite::Resources&by, WorkerData&data)
	{
		data.increaseResources << std::make_pair(technite.location, by);
	}



	void		Create()
	{

	}
	void		Destroy()
	{


	}


	/*static*/ PFaction			Faction::Create()
	{
		for (index_t i = 0; i < ARRAYSIZE(gridIDMap); i++)
			if (!gridIDMap[i])
			{
				PFaction faction(new Faction());
				faction->gridID = (BYTE)i;
				gridIDMap[i] = faction;
				Faction::all.Append(faction);
				faction->globalID.Generate();
				faction->knownWorld = Grid::initialWorld;
				return faction;
			}
		return PFaction();
	}

	void Faction::Destroy(float t)
	{
		gridIDMap[gridID].reset();
		all.FindAndErase(shared_from_this());
		bool familyChanged[BaseGrid::NumFamilies] = { false };
		foreach(this->technites, t)
		{
			Grid::actualWorld.cellStack[t->location.stackID].MakeLocalHole(t->location.height);
			familyChanged[Grid::high.grid.GetCell(t->location.stackID).family] = true;
		}
		for (index_t i = 0; i < BaseGrid::NumFamilies; i++)
			if (familyChanged[i])
			{
				Grid::low.family[i].SetOutdated(0);
				Grid::high.family[i].SetOutdated(0);
			}
	}



	/*static*/ bool Faction::ValidTechniteLocation(const Grid::HCellIdI&location)
	{
		if (!Technite::HeldBy(GetLastContent(location - 1)))
		{
			const auto&n = Grid::high.grid.GetCell(location.stackID).neighborhood;
			bool anyGood = false;
			for (int k = 0; k < n.numNeighbors; k++)
			{
				if (Technite::HeldBy(GetLastContent(Grid::HCellIdI(n.neighborID[k], location.height)))
					&&
					Technite::HeldBy(GetLastContent(Grid::HCellIdI(n.neighborID[k], location.height - 1)))
					)
				{
					anyGood = true;
					break;
				}
			}
			return anyGood;
		}
		return true;
	}

	/*static*/ bool Faction::ValidGlobalTechniteLocation(const Grid::HCellIdI&location)
	{
		if (!Technite::HeldBy(GetGlobalGridContent(location - 1)))
		{
			const auto&n = Grid::high.grid.GetCell(location.stackID).neighborhood;
			bool anyGood = false;
			for (int k = 0; k < n.numNeighbors; k++)
			{
				if (Technite::HeldBy(GetGlobalGridContent(Grid::HCellIdI(n.neighborID[k], location.height)))
					&&
					Technite::HeldBy(GetGlobalGridContent(Grid::HCellIdI(n.neighborID[k], location.height - 1)))
					)
				{
					anyGood = true;
					break;
				}
			}
			return anyGood;
		}
		return true;
	}


	index_t					Faction::CreateSingleTechnite(const Grid::HCellIdI&location, float t)
	{
		if (location.height < Grid::CellStack::BottomHeight || location.height > Grid::CellStack::TopHeight)
			return InvalidIndex;

		if (techniteMap.IsSet(location))
			return InvalidIndex;
		if (!ValidGlobalTechniteLocation(location))
			return InvalidIndex;
		Grid::Content content = GetGlobalGridContent(location);
		if (content != Grid::Content::Clear && content != Grid::Content::Water)
			return InvalidIndex;

		index_t at = technites.Count();
		Technite&tech = technites.Append();
		techniteMap.Set(location, at);
		tech.location = location;

		Grid::CellStack&stack = Grid::actualWorld.cellStack[location.stackID];
		ASSERT__(stack.SetLocalToTechniteContent(location.height, this->gridID));

		index_t family = Grid::high.grid.GetCell(location.stackID).family;

		Grid::low.family[family].SetOutdated(t);
		Grid::high.family[family].SetOutdated(t);


		return at;
	}







}


#endif

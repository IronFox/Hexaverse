#include <global_root.h>
#include "config.h"

#if TECHNITES

#include "technite.h"
#include "objects/gridAndChoices.h"
#include "objects/structure.h"
#include "objects/vehicle.h"

namespace Technites
{
	/*static*/ const Technite::Resources	Technite::SplitCost = Resources(SplitEnergyCost, SplitMatterCost),
		Technite::MoveCost = Resources(MoveEnergyCost, 0),
		Technite::EatCost = Resources(EatEnergyCost, 0),
		Technite::EnergyPackage = Resources(EnergyPackageSize, 0),
		Technite::MatterPackage = Resources(0, MatterPackageSize);

	/*static*/ Buffer0<PFaction>Faction::all;
	/*static*/ AtomicGrid Faction::atomicGrid;
	/*static*/ WorkPipe<Grid::HCellIdI>	Faction::newTechnites;
	/*static*/ WorkPipe<Grid::HCellIdI> Faction::moveInstructions;
	/*static*/ WorkPipe<std::pair<Grid::HCellIdI, Technite::Resources> > Faction::increaseResources;
	/*static*/ PFaction			Faction::gridIDMap[0xFF];


	struct TEaten
	{
		std::atomic<bool>	structureEaten = false,
							mobileEaten = false;
	};

	Array<TEaten>			eatenGrid(Grid::LowSetup::GridType::CellCount * Grid::CellStack::Range);




	static index_t atFaction = 0;

	/*static*/ void				Faction::Begin()
	{
		atFaction = 0;

		atomicGrid.Import(Grid::actualWorld);
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

	/*static*/ bool				Faction::ProcessNext(float t)
	{
		if (atFaction < all.Count())
		{
			all[atFaction]->Process(t);
			atFaction++;
		}

		if (atFaction >= all.Count())
		{
			atomicGrid.ExportChanged(Grid::actualWorld, t);

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


	Grid::Content			Faction::GetLastContent(const Grid::HCellIdI&cellID)
	{
		UINT32 layer = Grid::CellStack::ToLayer(cellID.height);
		ASSERT_LESS__(layer, Grid::CellStack::Range);
		
		auto rs = Grid::Content::Reinterpret(atomicGrid.cellStack[cellID.stackID].volumeCell[layer].content);
		
		return rs;
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
			BYTE set = current;

			switch (current)
			{
			case Grid::Content::Clear:
			case Grid::Content::Water:
				return Grid::Content::Reinterpret(current);
			case Grid::Content::Earth:
				set = Grid::Content::Clear;
				matterYield = 1;
				break;
			case Grid::Content::Foundation:
				set = Grid::Content::Rock;
				matterYield = 10;
				break;
			case Grid::Content::Granite:
				set = Grid::Content::Rock;
				matterYield = 10;
				break;
			case Grid::Content::Grass:
				set = Grid::Content::Earth;
				matterYield = 2;
				break;
			case Grid::Content::Road:
				set = Grid::Content::Rock;
				matterYield = 5;
				break;
			case Grid::Content::Rock:
				set = Grid::Content::Earth;
				matterYield = 7;
				break;
			case Grid::Content::Sand:
				set = Grid::Content::Earth;
				matterYield = 3;
				break;
			case Grid::Content::Snow:
				set = Grid::Content::Clear;
				break;
			default:
				return Grid::Content::Reinterpret(current);
			}

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



	Faction::ExecResult			Faction::GrowTo(Technite&technite, const Grid::HCellIdI&target, index_t iteration)
	{
		ExecResult rs = Eat(technite, target, iteration);
		
		if (rs != ExecResult::Success)
			return rs;

		if (iteration < 2)
			return ExecResult::Again;

		
		if (!ReduceResources(technite, Technite::SplitCost))
		{
			return ExecResult::TechniteLacksResources;
		}
		

		SpawnNewTechnite(target);
		
		return ExecResult::Success;
	}

	Faction::ExecResult			Faction::MoveTo(Technite&technite, const Grid::HCellIdI&target, index_t iteration)
	{
		ExecResult rs = Eat(technite, target, iteration);
		
		if (rs != ExecResult::Success)
			return rs;

		
		if (!ReduceResources(technite, Technite::MoveCost))
		{
			return ExecResult::TechniteLacksResources;
		}
		

		if (!MoveTechnite(technite, target))
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
			return;
		if (cluster->volumeCells[inner] != Empty)
		{
			cluster->volumeCells[inner] = Empty;
			cluster->numEntries--;
			totalEntries--;
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

	Technite::Resources		Faction::EatTechnite(const Grid::HCellIdI&target, bool factionIsActive)
	{
		//	if (!factionIsActive)
		{
			index_t at;
			if (!this->techniteMap.Query(target, at))
			{
				UINT32 layer = Grid::CellStack::ToLayer(target.height);
				ASSERT_LESS__(layer, Grid::CellStack::Range);
				atomicGrid.cellStack[target.stackID].volumeCell[layer].content = Grid::Content::Clear;
				atomicGrid.SignalStackChange(target.stackID);
				return Technite::Resources();	//never been there. weird, but whatever
			}
			
			ASSERT_LESS__(at, technites.Count());
			Technite&tech = technites[at];
			if (tech.isDead.Set(false, true))
			{
				
				if (factionIsActive)
					anyDied = true;
				UINT32 layer = Grid::CellStack::ToLayer(target.height);
				ASSERT_LESS__(layer, Grid::CellStack::Range);
				atomicGrid.cellStack[target.stackID].volumeCell[layer].content = Grid::Content::Clear;
				atomicGrid.SignalStackChange(target.stackID);
				
				return technites[at].resources / 2;
			}
			
			return Technite::Resources();
		}
	}


	Faction::ExecResult			Faction::Eat(Technite&technite, const Grid::HCellIdI&target, index_t iteration)
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
			
			Technite::Resources resources = f->EatTechnite(target, f.get() == this);	//if technite has been eaten in the meantime EatTechnite will return empty resources
			
			IncreaseResources(technite, resources);
			
		}
		return ExecResult::Success;
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

	void							Faction::CheckState(Technite&technite)
	{
		//Grid::HCellIdI target;
		//if (!technite.DecodeAbsoluteTarget(target))
		//{
		//	technite.isDead = true;
		//	anyDied = true;
		//	return;
		//}
		
		Grid::Content content = GetLastContent(technite.location);
		if (content != Grid::Content::Technite
			|| Grid::actualWorld.cellStack[technite.location.stackID].GetCellI(technite.location.height).techniteFaction != this->gridID)
		{
			
			technite.isDead = true;	//world could have changed by other influences (like explosions)
			anyDied = true;
			
		}
		
	}

	Technite::TaskResult			Faction::ExecuteTask(Technite&technite, index_t iteration)
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
			return GrowTo(technite, target, iteration);
		case Technite::Task::EatMoveTo:
			return MoveTo(technite, target, iteration);
		case Technite::Task::TransferEnergyTo:
		{
			if (iteration < 3)
				return ExecResult::Again;
			Technite*other = GetOwnTechnite(target);
			if (!other)
				return Technite::TaskResult::NoTechniteAtDestination;
			if (!ReduceResources(technite, Technite::EnergyPackage))
				return Technite::TaskResult::TechniteLacksResources;
			IncreaseRemoteResources(*other, Technite::EnergyPackage);
			return Technite::TaskResult::Success;
		}
		case Technite::Task::TransferMatterTo:
		{
			if (iteration < 3)
				return ExecResult::Again;
			Technite*other = GetOwnTechnite(target);
			if (!other)
				return Technite::TaskResult::NoTechniteAtDestination;
			if (!ReduceResources(technite, Technite::MatterPackage))
				return Technite::TaskResult::TechniteLacksResources;
			IncreaseRemoteResources(*other, Technite::MatterPackage);
			return Technite::TaskResult::Success;
		}
		}
		return Technite::TaskResult::NothingToDo;
	}


	void		Faction::Process(float t)
	{
		anyDied = false;
		
		Concurrency::parallel_for(index_t(0), technites.Count(), [this](index_t i)
		{
			CheckState(technites[i]);
			technites[i].taskResult = Technite::TaskResult::Again;
		});
		

		for (index_t iteration = 0; ; iteration++)
		{
			volatile bool anyAgain = false;
			Concurrency::parallel_for(index_t(0), technites.Count(), [this, &anyAgain, iteration](index_t i)
			{
				Technite&tec = technites[i];
				if (tec.isDead || tec.taskResult != Technite::TaskResult::Again)
					return;
				Technite::TaskResult rc = tec.taskResult = ExecuteTask(tec, iteration);
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
		}
		

		newTechnites.SignalRead();
		for (const auto&loc : newTechnites)
		{
			if (techniteMap.IsSet(loc))
				continue;

			index_t at = technites.Count();
			Technite tech;
			tech.location = loc;

			Grid::actualWorld.cellStack[tech.location.stackID].SetLocalTechniteFaction(tech.location.height, this->gridID);
			UINT32 layer = Grid::CellStack::ToLayer(tech.location.height);
			ASSERT_LESS__(layer, Grid::CellStack::Range);
			atomicGrid.cellStack[tech.location.stackID].volumeCell[layer].content = Grid::Content::Technite;
			atomicGrid.SignalStackChange(tech.location.stackID);
			technites.Append(tech).isDead = false;

			techniteMap.Set(tech.location,at);
			visibleWorld.ChangeSensory(tech.location, t, 1);
		}
		newTechnites.ExitRead();

		
		increaseResources.SignalRead();
		for (const auto&inc : increaseResources)
		{
			index_t at;
			ASSERT1__(techniteMap.Query(inc.first, at), inc.first.ToString());
			Technite&tec = technites[at];
			tec.resources += inc.second;
		}

		increaseResources.ExitRead();

		
		moveInstructions.SignalRead();
		for (const auto&loc : moveInstructions)
		{
			index_t at;
			ASSERT1__(techniteMap.Query(loc, at),loc.ToString());
			Technite&tec = technites[at];
			techniteMap.Unset(loc);
			ASSERT1__(techniteMap.SetNew(tec.location, at), tec.location.ToString());
		}
		moveInstructions.ExitRead();
		
	}


	bool			Faction::MoveTechnite(Technite&technite, const Grid::HCellIdI&target)
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
		moveInstructions << technite.location;//old location
		technite.location = target;
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

	void			Faction::SpawnNewTechnite(const Grid::HCellIdI&target)
	{
		newTechnites << target;
	}

	void			Faction::IncreaseResources(Technite&technite, const Technite::Resources&by)
	{
		technite.resources += by;
	}

	bool			Faction::ReduceResources(Technite&technite, const Technite::Resources&by)
	{
		return technite.resources -= by;
	}

	void			Faction::IncreaseRemoteResources(Technite&technite, const Technite::Resources&by)
	{
		increaseResources << std::make_pair(technite.location, by);
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
		const Grid::CellStack&stack = Grid::actualWorld.cellStack[location.stackID];
		const Grid::CellStack::Cell&self = stack.GetCellI(location.height);
		const Grid::CellStack::Cell&ground = stack.GetCellI(location.height-1);
		return ValidTechniteLocation(ground.content, self.content, self.structureCounter);
	}

	/*static*/ bool Faction::ValidTechniteLocation(BYTE groundContent, BYTE targetContent, BYTE targetStructureLevel)
	{
		return		//!Grid::CellStack::Cell::IsLiquid(groundContent) 
					//&& groundContent != Grid::Content::Clear 
					
					targetContent == Grid::Content::Clear 
					&& targetStructureLevel == 0;
	}


	index_t					Faction::CreateSingleTechnite(const Grid::HCellIdI&location, float t)
	{
		if (location.height < Grid::CellStack::BottomHeight || location.height > Grid::CellStack::TopHeight)
			return InvalidIndex;

		if (techniteMap.IsSet(location))
			return InvalidIndex;
		if (!ValidTechniteLocation(location))
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

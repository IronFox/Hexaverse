#include <global_root.h>
#include "config.h"


#include "technite.h"
#include "objects/gridAndChoices.h"
#include "objects/structure.h"
#include "objects/vehicle.h"
#include "sphereGridHelper.h"
#include <general/thread_system.h>
#include "objects/satellite.h"

#ifndef CLIENT
void	CheckSendTerrain();
bool	ApplyCoverageChange(const Objects::PFaction&faction);
#endif


namespace Technites
{
	bool limitTechniteLife = true;


	const EventClass techniteError = EventClass( EventSignificance::NonFatalError, EventFlavor::ServerPrivate );


	/*static*/ const Technite::Resources	Technite::SplitCost = Resources(SplitEnergyCost, SplitMatterCost),
		Technite::MoveCost = Resources(MoveEnergyCost, 0),
		Technite::GnawAtCost = Resources(GnawAtEnergyCost, 0),
		Technite::ConsumeCost = Resources(ConsumeEnergyCost, 0);
		//Technite::EnergyPackage = Resources(EnergyPackageSize, 0),
		//Technite::MatterPackage = Resources(0, MatterPackageSize);
	/*static*/ float3		Technite::factionColor[0xFF] = {float3()};


	/*static*/ Array<Faction::WorkerData>	Faction::workerData;

	/*static*/ Buffer0<PFaction>Faction::allTechniteFactions;
	/*static*/ AtomicGrid Faction::atomicGrid;
	///*static*/ WorkPipe<Grid::HCellIdU>	Faction::newTechnites;
	///*static*/ WorkPipe<Grid::HCellIdU> Faction::moveInstructions;
	///*static*/ WorkPipe<std::pair<Grid::HCellIdU, Technite::Resources> > Faction::increaseResources;
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
	/*static*/ const BYTE	Technite::DegradeTo[NumGroundTypes] =
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


	#ifndef CLIENT
		static index_t atFaction = 0;
		static Array<FactionRandomizer>	processingOrder;

		static float animationTime;


		float			GetAnimationTime()
		{
			return animationTime;
		}

		void				BeginProcessing(float t0)
		{
			animationTime = t0;
			atFaction = 0;

			//#ifdef _DEBUG
			//	VerifyGrid();	//this might wrongly fail
			//#endif

			Faction::atomicGrid.Import(Grid::actualWorld);



			processingOrder.SetSize(Faction::allTechniteFactions.Count());

			for (index_t i = 0; i < Faction::allTechniteFactions.Count(); i++)
			{
				PFaction f = Faction::allTechniteFactions[i];
				processingOrder[i].Set(f);
				f->ProcessLighting();
				foreach (f->sensors,s)
				{
					f->RegisterSensorRemoval(*s,false,t0,false);
				}
				f->sensors.Clear();
				f->visibility.CheckChanges(t0);
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

		/*static*/ bool				ProcessNext()
		{
			if (atFaction < processingOrder.Count())
			{
				processingOrder[atFaction].target->Process(animationTime);
				atFaction++;

				animationTime += 0.0001f;
			}

			if (atFaction >= processingOrder.Count())
			{
				Faction::atomicGrid.ExportChanged(Grid::actualWorld, animationTime);


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
					Objects::Remove(e,Objects::RemovalType::UncontrolledExplosion,animationTime,true);
				toKill.Clear();

				foreach (Faction::allTechniteFactions,tf)
					(*tf)->VerifyIntegrity();


				return false;
			}
			return true;
		}
	#endif


	#ifdef CLIENT
	void				Technite::CompileTendency(TVec3<>&outDirection, float&outMagnitude, TVec3<>&inOutColor) const
	{
		Grid::HCellIdU dest,loc;
		if (!DecodeAbsoluteTarget(loc,dest))
		{
			Vec::clear(outDirection);
			outMagnitude = 0;
			return;
		}
		if (loc == dest)
		{
			Vec::clear(outDirection);
		}
		else
		{
			Grid::GetWorldDelta(dest,loc,outDirection);
		}
		outMagnitude = 1;
		switch (nextTask)
		{
			case Task::ConsumeSurroundingCell:
				outMagnitude = 80.f;
			break;
			case Task::GnawAtSurroundingCell:
				outMagnitude = 30.f;
			break;
			case Task::GrowTo:
				outMagnitude = 120.f;
			break;
			case Task::TransferEnergyTo:
			case Task::TransferMatterTo:
			case Task::Scan:
				outMagnitude = 60.f;
			break;
			case Task::SelfTransformToType:
				outMagnitude = 127;
			break;
		}
		if (usesColor)
			inOutColor = float3(color)/255.f;
		else
		{
			int totalResources = (int)resources.matter + (int)resources.energy;
			float brightness = 0.5f + (float)resources.energy/64.f;
			float green = 1.f + (float)resources.matter/64.f;
			inOutColor.r = clamp(inOutColor.r * brightness,0,1);
			inOutColor.g = clamp(inOutColor.g * brightness * green,0,1);
			inOutColor.b = clamp(inOutColor.b * brightness,0,1);
		}
	}

	#endif



	/*static*/ PFaction Faction::GetFactionOf(BYTE factionID)
	{
		return gridIDMap[factionID];
	}

	bool					Faction::AnyEntitiesIn(const Grid::HCellIdU&target)
	{
		const auto mapping = Grid::highToLowMap[target.stackID];
		Objects::PWorldEntity entity;
		for (int k = 0; k < 2; k++)
			if (mapping.lowCells[k] != Grid::THighToLowCell::Empty)
			{
				Grid::LCellIdU at(mapping.lowCells[k],target.layer);
				TEaten&eaten = eatenGrid[Objects::CellToGridIndex(at)];
				entity = Objects::GetMobileEntity(at);
				bool ref;
				ref = false;
				if (entity && !eaten.mobileEaten)
					return true;
				entity = Objects::GetStructure(at);
				ref = false;
				if (entity && !eaten.mobileEaten)
					return true;
			}
		return false;
	}

	Technite::Resources		Faction::EatEntitiesIn(const Grid::HCellIdU&target, bool&cantEatThis)
	{
		cantEatThis = false;
		Objects::PWorldEntity	all[4];
		int count = 0;
		const auto mapping = Grid::highToLowMap[target.stackID];
		Objects::PWorldEntity entity;
		UINT32 lower = (UINT32)std::max(0,(int)target.layer-2);
		for (UINT32 layer = lower; layer <= target.layer; layer++)
		{
			for (int k = 0; k < 2; k++)
				if (mapping.lowCells[k] != Grid::THighToLowCell::Empty)
				{
					Grid::LCellIdU at(mapping.lowCells[k],layer);
					TEaten&eaten = eatenGrid[Objects::CellToGridIndex(at)];
					entity = Objects::GetMobileEntity(at);
					bool ref;
					ref = false;
					if (entity && (target.layer - layer) < (UINT32)entity->GetHeight())
						if (!entity->IsIndestructible() && entity->owner.get() != this)
						{
							if (eaten.mobileEaten.compare_exchange_strong(ref,true))
								all[count++] = entity;
						}
						else
							cantEatThis = true;
					entity = Objects::GetStructure(at);
					ref = false;
					if (entity && (target.layer - layer) < (UINT32)entity->GetHeight())
						if (!entity->IsIndestructible() && entity->owner.get() != this)
						{
							if (eaten.structureEaten.compare_exchange_strong(ref,true))
								all[count++] = entity;
						}
						else
							cantEatThis = true;
				}
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

					#ifdef _DEBUG
						if (to.volumeCell[l].content == Grid::Content::Technite)
						{
							PFaction f = Faction::GetFactionOf(to.volumeCell[l].techniteFaction);
							const Grid::HCellIdU loc(i,l);
							if (!f)
							{
								LogEvent(techniteError,"Found homeless technite in "+loc.ToString()+", clearing cell");
								to.volumeCell[l].content = Grid::Content::Clear;
								continue;
							}
							Technite*t = f->GetOwnTechnite(loc);
							if (!t)
							{
								LogEvent(techniteError,"Found abandoned technite of faction "+f->GetLogName()+" in "+loc.ToString()+", clearing cell");
								to.volumeCell[l].content = Grid::Content::Clear;
								continue;
							}
						}
					#endif
				}
			}
		}
	}


	///*static*/ Grid::Content	Faction::GetGlobalGridContent(const Grid::HCellIdU&cellID)
	//{
	//	if (cellID.height < Grid::CellStack::BottomHeight)
	//		return Grid::Content::Reinterpret(Grid::CellStack::bottom.content);
	//	if (cellID.height > Grid::CellStack::TopHeight)
	//		return Grid::Content::Clear;
	//	UINT32 layer = Grid::CellStack::ToLayer(cellID.height);
	//	DBG_ASSERT_LESS__(layer, Grid::CellStack::Range);

	//	return Grid::Content::Reinterpret(Grid::actualWorld.cellStack[cellID.stackID].volumeCell[layer].content);
	//}

	/*static*/ Grid::Content	Faction::GetGlobalGridContent(const Grid::HCellIdU&cellID)
	{
		if (cellID.layer >= Grid::CellStack::Range)
			return Grid::Content::Clear;
		return Grid::Content::Reinterpret(Grid::actualWorld.cellStack[cellID.stackID].volumeCell[cellID.layer].content);
	}


	///*static*/Grid::Content			Faction::GetLastContent(const Grid::HCellIdU&cellID)
	//{
	//	if (cellID.height < Grid::CellStack::BottomHeight)
	//		return Grid::Content::Reinterpret(Grid::CellStack::bottom.content);
	//	if (cellID.height > Grid::CellStack::TopHeight)
	//		return Grid::Content::Clear;
	//	UINT32 layer = Grid::CellStack::ToLayer(cellID.height);
	//	DBG_ASSERT_LESS__(layer, Grid::CellStack::Range);
	//	
	//	return Grid::Content::Reinterpret(atomicGrid.cellStack[cellID.stackID].volumeCell[layer].content);
	//}

	Grid::Content			Faction::GetLastContent(const Grid::HCellIdU&cellID)
	{
		return Grid::Content::Reinterpret(atomicGrid.cellStack[cellID.stackID].volumeCell[cellID.layer].content);
	}
	Grid::Content			Faction::DegradeMatter(const Grid::HCellIdU&source, const Grid::HCellIdU&target, WorkerData&data, index_t iteration, Technite::Resources&outYield, bool destroy)
	{

		outYield = Technite::Resources();
		const UINT32 layer = target.layer;
		ASSERT_LESS__(layer, Grid::CellStack::Range);
		AtomicGrid::CellStack::Cell&cell = atomicGrid.cellStack[target.stackID].volumeCell[layer];
		for (;;)
		{

			BYTE current = cell.content;
			BYTE matterYield = 0;
			BYTE set = Technite::DegradeTo[current];

			matterYield = Technite::MatterYield[current];

			#ifdef _DEBUG
				WorkerData::LoggedEvent&ev = data.events.Append();
				ev.source = source;
				ev.target = target;
				ev.type = WorkerData::LoggedEvent::Type::DegradeMatter;
				ev.iteration = iteration;
				ev.destructive = destroy;
				ev.from = Grid::Content::Reinterpret(current);
				ev.to = Grid::Content::Reinterpret(set);
			#endif

			if (set == current)
			{
				#ifdef _DEBUG
					ev.result = "Success, intended type present";
				#endif
				return Grid::Content::Reinterpret(set);
			}

			if (!destroy)
			{
				#ifdef _DEBUG
					ev.result = "Success, non-destructive";
				#endif
				outYield = Technite::Resources::Resources(0, matterYield/2);
				return Grid::Content::Reinterpret(current);
			}

			if (cell.content.compare_exchange_weak(current, set))
			{
				atomicGrid.SignalStackChange(target.stackID);
				#ifdef _DEBUG
					ev.result = "Success, matter degraded";
				#endif

				outYield = Technite::Resources::Resources(0, matterYield);
				return Grid::Content::Reinterpret(set);
			}
			#ifdef _DEBUG
				ev.result = "Failed, atomic exchange failed";
			#endif
		}
	}

	/*static*/ const char*	Technite::ToString(TaskResult rs)
	{
		#undef CASE
		#define CASE(X)	case TaskResult::X: return #X;
		switch (rs)
		{
			CASE(NothingToDo)
			CASE(BadCommand)
			CASE(CannotEatThis)
			CASE(CannotGnawAtThis)
			CASE(CannotStandThere)
			CASE(TechniteLacksResources)
			CASE(NoTechniteAtDestination)
			CASE(MoreWorkNeeded)
			CASE(OperationWindowMissed)
			CASE(Again)
			CASE(Success)
		}
		return "Unknown task result";
	}

	/*static*/ const char*	Technite::ToString(Task rs)
	{
		#undef CASE
		#define CASE(X)	case Task::X: return #X;
		switch (rs)
		{
			CASE(None)
			CASE(ConsumeSurroundingCell)
			CASE(GnawAtSurroundingCell)
			CASE(GrowTo)
			CASE(TransferEnergyTo)
			CASE(TransferMatterTo)
			CASE(SelfTransformToType)
			CASE(Scan)
		}
		return "Unknown task";
	}


	bool	Technite::DecodeAbsoluteTarget(Grid::HCellIdU & outTarget) const
	{
		Grid::HCellIdU loc;
		return DecodeAbsoluteTarget(loc, outTarget);
	}


	bool	Technite::DecodeAbsoluteTarget(Grid::HCellIdU & loc, Grid::HCellIdU & outTarget) const
	{
		const int heightDelta = target.GetHeightDelta();
		const UINT32 nIndex = target.GetNeighborIndex();

		if (heightDelta < -1 || heightDelta > 1)
			return false;
		loc = location.GetU();
		if (loc.stackID >= Grid::high.grid.CellCount)
			return false;
		if (loc.layer >= Grid::CellStack::Range)
			return false;
		if (loc.layer == 0 && heightDelta < 0)
			return false;
		if (loc.layer+heightDelta >= Grid::CellStack::Range)
			return false;

		if (nIndex == CompressedTarget::SelfNeighbor)
		{
			outTarget = Grid::HCellIdU(loc.stackID, loc.layer + heightDelta);
		}
		else
		{
			const auto&n = Grid::high.grid.GetCell(loc.stackID).neighborhood;
			if (nIndex >= n.numNeighbors)
			{
				return false;
			}
			outTarget = Grid::HCellIdU(n.neighborID[nIndex], loc.layer + heightDelta);
		}
		return outTarget.layer < Grid::CellStack::Range;
	}



	Faction::ExecResult			Faction::GrowTo(Technite&technite, const Grid::HCellIdU&target, index_t iteration, WorkerData&data)
	{
		ExecResult rs = Eat(technite, target, iteration,data, true);
		
		#ifdef _DEBUG
			WorkerData::LoggedEvent&ev = data.events.Append();
			ev.target = target;
			ev.source = technite.location.GetU();
			ev.type = WorkerData::LoggedEvent::Type::GrowTo;
			ev.iteration = iteration;
			ev.destructive = true;
			ev.from = GetLastContent(target);
			ev.to = Grid::Content::Technite;
		#endif

		if (rs != ExecResult::Success)
		{
			#ifdef _DEBUG
				ev.result = "Failed, could not eat content";
			#endif
			return rs;
		}

		if (iteration < 2)
		{
			#ifdef _DEBUG
				ev.result = "Failed, iteration too low";
			#endif
			return ExecResult::Again;
		}
		if (iteration > 2)
		{
			#ifdef _DEBUG
				ev.result = "Failed, iteration too high";
			#endif
			return ExecResult::OperationWindowMissed;
		}

		
		if (!ReduceResources(technite, Technite::SplitCost))
		{
			#ifdef _DEBUG
				ev.result = "Failed, insufficient resources";
			#endif
			return ExecResult::TechniteLacksResources;
		}
		
		#ifdef _DEBUG
			ev.result = "Possibly successful, spawning new technite...";
		#endif

		if (!SpawnNewTechnite(technite.location.GetU(), target, data, iteration))
		{
			//this actually can happen if the local faction recently spawned a new technite there, 
			//which is registered in the grid but not in the faction yet, so it cannot be eaten
			IncreaseResources(technite, Technite::SplitCost);
			return ExecResult::CannotStandThere;
		}
		
		return ExecResult::Success;
	}
/*
	Faction::ExecResult			Faction::MoveTo(Technite&technite, const Grid::HCellIdU&target, index_t iteration, WorkerData&data)
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
*/

	//index_t					BroadTechniteMap::MapCluster(const Grid::HCellIdU&loc, index_t&outFamily, UINT32&heightRemainder) const
	//{
	//	return MapCluster(Grid::HCellIdU(loc.stackID, Grid::CellStack::ToLayer(loc.height)),outFamily,heightRemainder);
	//}
	index_t					BroadTechniteMap::MapCluster(const Grid::HCellIdU&loc, index_t&outFamily, UINT32&heightRemainder) const
	{
		outFamily = Grid::high.grid.GetCell(loc.stackID).family;
		heightRemainder = loc.layer % SectionLayers;
		return outFamily * HeightSections
			+ loc.layer / SectionLayers;
	}

	//index_t					BroadTechniteMap::Map(const Grid::HCellIdU&location, Cluster*&outCluster, bool initIfEmpty)
	//{
	//	return Map(Grid::HCellIdU(location.stackID,Grid::CellStack::ToLayer(location.height)),outCluster,initIfEmpty);
	//}
	index_t					BroadTechniteMap::Map(const Grid::HCellIdU&location, Cluster*&outCluster, bool initIfEmpty)
	{
		index_t family;
		UINT32 heightRemainder;
		index_t clusterIndex = MapCluster(location, family, heightRemainder);
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

	//index_t					BroadTechniteMap::Map(const Grid::HCellIdU&location, const Cluster*&outCluster) const
	//{
	//	return Map(Grid::HCellIdU(location.stackID,Grid::CellStack::ToLayer(location.height)),outCluster);
	//}
	index_t					BroadTechniteMap::Map(const Grid::HCellIdU&location, const Cluster*&outCluster) const
	{
		//UINT32 layer = Grid::CellStack::ToLayer(location.height);
		index_t family;
		UINT32 heightRemainder;
		index_t clusterIndex = MapCluster(location, family,heightRemainder);
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

	index_t					BroadTechniteMap::Unset(const Grid::HCellIdU& id)
	{
		Cluster*cluster;
		index_t inner = Map(id, cluster,false);
		if (cluster->numEntries == 0)
		{
			FATAL__("Failed to unset " + id.ToString());
			return InvalidIndex;
		}
		if (cluster->volumeCells[inner] != Empty)
		{
			index_t rs = cluster->volumeCells[inner];
			cluster->volumeCells[inner] = Empty;
			cluster->numEntries--;
			totalEntries--;
			return rs;
		}
		else
		{
			FATAL__("Failed to unset " + id.ToString());
		}
		return InvalidIndex;
	}
	//void					BroadTechniteMap::Unset(const Grid::HCellIdU& id)
	//{
	//	Unset(Grid::HCellIdU(id.stackID,Grid::CellStack::ToLayer(id.height)));
	//}

	//void					BroadTechniteMap::Set(const Grid::HCellIdU&id, index_t to)
	//{
	//	Set(Grid::HCellIdU(id.stackID,Grid::CellStack::ToLayer(id.height)),to);
	//}
	void					BroadTechniteMap::Set(const Grid::HCellIdU&id, index_t to)
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

	//bool					BroadTechniteMap::Query(const Grid::HCellIdU&id, index_t&out) const
	//{
	//	return Query(Grid::HCellIdU(id.stackID,Grid::CellStack::ToLayer(id.height)),out);
	//}
	bool					BroadTechniteMap::Query(const Grid::HCellIdU&id, index_t&out) const
	{
		if (id == Grid::HCellIdU::Invalid)
			return false;
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

	//bool					BroadTechniteMap::IsSet(const Grid::HCellIdU&id) const
	//{
	//	return IsSet(Grid::HCellIdU(id.stackID,Grid::CellStack::ToLayer(id.height)));
	//}
	bool					BroadTechniteMap::IsSet(const Grid::HCellIdU&id) const
	{
		const Cluster*cluster;
		index_t inner = Map(id, cluster);
		if (cluster->numEntries == 0)
			return false;
		UINT32 v = cluster->volumeCells[inner];
		return v != Empty;
	}

	//bool					BroadTechniteMap::SetNew(const Grid::HCellIdU&id, index_t to)
	//{
	//	return SetNew(Grid::HCellIdU(id.stackID,Grid::CellStack::ToLayer(id.height)),to);
	//}
	bool					BroadTechniteMap::SetNew(const Grid::HCellIdU&id, index_t to)
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

	count_t					BroadTechniteMap::ThoroughCount() const
	{
		count_t rs = 0;
		for (index_t i = 0; i < ClusterCount; i++)
			rs += cluster[i].numEntries;
		ASSERT_EQUAL__(rs,totalEntries);
		return rs;
	}


	Technite::Resources		Faction::EatTechnite(Technite&local, const Grid::HCellIdU&target, index_t iteration, WorkerData&data, bool factionIsActive)
	{
		//this seems to fix the 'terrain content mismatch' issue, but it should not occur
		//return Technite::Resources();//experiment

		#ifdef _DEBUG
			WorkerData::LoggedEvent&ev = data.events.Append();
			ev.target = target;
			ev.source = local.location.GetU();
			ev.type = WorkerData::LoggedEvent::Type::EatTechnite;
			ev.from = GetLastContent(target);
			ev.to = Grid::Content::Clear;
			ev.iteration = iteration;
			ev.targetingOwnTechniteFaction = factionIsActive;
		#endif

		//	if (!factionIsActive)
		{
			index_t at;
			if (!this->techniteMap.Query(target, at))
			{
				//freshly created technite, most likely
				ASSERT__(factionIsActive);

				#ifdef _DEBUG
					ev.result = "Failed, technite unknown";
				#endif


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
					#ifdef _DEBUG
						ev.result = "Success, enough energy to eat hostile technite";
					#endif
				}
				else
				{
					//fail
					tech.resources.energy -= local.resources.energy;
					local.resources.energy = 0;
					//std::cout << local.location.ToString() << " failed eating remote technite (i have " << (int)local.resources.energy << ", other has " << (int)tech.resources.energy << ") " << tech.location.ToString() << ". remote energy decreased to " << (int)tech.resources.energy << std::endl;

					#ifdef _DEBUG
						ev.result = "Failed, insufficient energy to eat hostile technite";
					#endif
					
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
				const UINT32 layer = target.layer;
				ASSERT_LESS__(layer, Grid::CellStack::Range);

				BYTE expect0 = Grid::Content::Technite;

				bool changed = atomicGrid.cellStack[target.stackID].volumeCell[layer].content.compare_exchange_strong(expect0, Grid::Content::Technite);
				if (!changed)
				{
					#ifdef _DEBUG
						ev.result = "Error: Targeted technite died, but grid content lost in transition";
					#endif
					return Technite::Resources();
				}


				atomicGrid.cellStack[target.stackID].volumeCell[layer].content = Grid::Content::Clear;
				atomicGrid.SignalStackChange(target.stackID);
				
				const Technite::Resources&source = tech.resources;
				#ifdef _DEBUG
					ev.result = "Success, targeted technite is now dead";
				#endif

				return Technite::Resources(source.energy/4, source.energy / 2);
			}
			#ifdef _DEBUG
				ev.result = "Failed, targeted technite is already dead";
			#endif

			
			return Technite::Resources();
		}
	}


	#ifndef CLIENT
		static Mutex mutex;
	bool			VerifyIsVisibleAndMatches(const Objects::PFaction&f, const Grid::HCellIdU&cellID)
	{
		const auto&cell = f->visibility.cellStack[cellID.stackID].volumeCell[cellID.layer];
		if (cell.visibility <= 0)
			return false;
		const auto&cell2 = f->currentState.cellStack[cellID.stackID].volumeCell[cellID.layer];
		return cell2.content == Grid::actualWorld.cellStack[cellID.stackID].volumeCell[cellID.layer].content;
		//ASSERT_EQUAL__(cell2.content,Grid::actualWorld.cellStack[cellID.stackID].volumeCell[cellID.layer].content);	//this would be uber fatal
		return true;
	}

	#ifdef _DEBUG
		/*static*/ bool			Faction::VerifyAtomicIsVisibleAndMatches(const Objects::PFaction&f, const Grid::HCellIdU&cellID, const Grid::HCellIdU&techniteIn)
		{
			const auto&cell = f->visibility.cellStack[cellID.stackID].volumeCell[cellID.layer];
			if (cell.visibility <= 0)
			{
				mutex.lock();
				LogEvent(techniteError,"Visibility mismatch in cell "+techniteIn.ToString()+". Expected this cell to be visibile");
				ReportLocation(techniteIn);
				mutex.unlock();
				return false;
			}
			//const auto&cell2 = f->currentState.cellStack[cellID.stackID].volumeCell[cellID.layer];
			//Grid::Content known = Grid::Content::Reinterpret(cell2.content);
			//Grid::Content actual = Grid::Content::Reinterpret(Faction::atomicGrid.cellStack[cellID.stackID].volumeCell[cellID.layer].content);
			//if (known != actual)
			//{
			//	mutex.lock();
			//	LogEvent(EventType::TechniteLogic,nullptr,"Visibility mismatch in cell "+cellID.ToString()+". Expected this cell to contain "+actual.ToString()+", but found"+known.ToString());
			//	ReportLocation(cellID);
			//	mutex.unlock();
			//	return false;
			//}
			return true;
		}
	#endif

	void			VerifyGrid()
	{
		std::atomic<count_t>	abandonedCounter = 0,
								totalCounter = 0;

		bool familyChanged[BaseGrid::NumFamilies] = {false};

		Concurrency::parallel_for(index_t(0),Grid::HighSetup::GridType::CellCount,[&abandonedCounter,&totalCounter,&familyChanged](index_t stackIndex)
		{
			Grid::CellStack&stack = Grid::actualWorld.cellStack[stackIndex];

			bool anyChanged = false;
			for (UINT32 layer = 0; layer < Grid::CellStack::Range; layer++)
			{
				if (stack.volumeCell[layer].content == Grid::Content::Technite)
				{
					totalCounter++;
					PFaction f = Faction::GetFactionOf(stack.volumeCell[layer].techniteFaction);
					const Grid::HCellIdU loc(stackIndex,layer);
					if (!f)
					{
						//LogEvent(EventType::TechniteLogic,nullptr,"Found homeless technite in "+loc.ToString()+", clearing cell");

						stack.MakeLocalHole(Grid::CellStack::ToHeight(layer));
						abandonedCounter++;
						anyChanged = true;
						continue;
					}
					Technite*t = f->GetOwnTechnite(loc);
					if (!t)
					{
						foreach (f->technites,t2)
							if (t2->location.GetU() == loc)
							{
								FATAL__("Map inconsistency found with technite at location "+loc.ToString());
							}

						//LogEvent(EventType::TechniteLogic,nullptr,"Found abandoned technite of faction "+f->GetLogName()+" in "+loc.ToString()+", clearing cell");
						stack.MakeLocalHole(Grid::CellStack::ToHeight(layer));
						abandonedCounter++;
						anyChanged = true;
						continue;
					}

				}
			}
			if (anyChanged)
				familyChanged[Grid::high.grid.GetCell(stackIndex).family] = true;
		});
		for (index_t i = 0; i < ARRAYSIZE(familyChanged); i++)
			if (familyChanged[i])
			{
				Grid::low.family[i].SetOutdated(0.f);
				Grid::high.family[i].SetOutdated(0.f);
			}


		count_t shouldBeTechniteCount = 0;
		foreach (Faction::allTechniteFactions,f)
			shouldBeTechniteCount += (*f)->technites.Count();

		count_t gridTechniteCount = totalCounter - abandonedCounter;
		if (gridTechniteCount < shouldBeTechniteCount)
		{
			LogEvent(techniteError,"Encountered "+String(shouldBeTechniteCount-gridTechniteCount)+" technite ghost(s). Ignoring");
			//ignore this case, won't last long anyways
		}
		else
			ASSERT_EQUAL__(gridTechniteCount,shouldBeTechniteCount);

		if (abandonedCounter > 0)
		{
			LogEvent(techniteError,"Cleared "+String(abandonedCounter)+"/"+String(totalCounter)+" abandoned technite cell(s), "+String(shouldBeTechniteCount)+" technite(s) left");

			
			CheckSendTerrain();

			//RemapFaction(Objects::Faction::nexus);
			//foreach (Faction::all,f)
			//	RemapFaction(f->lock())
		}


		std::atomic<count_t>	visibilityErrors = 0;

		Concurrency::parallel_for(index_t(0),Grid::HighSetup::GridType::CellCount,[&visibilityErrors](index_t stackIndex)
		{
			Grid::CellStack&stack = Grid::actualWorld.cellStack[stackIndex];
			const auto&n = Grid::high.grid.GetCell(stackIndex).neighborhood;
			for (UINT32 layer = 0; layer < Grid::CellStack::Range; layer++)
			{
				if (stack.volumeCell[layer].content == Grid::Content::Technite)
				{
					PFaction f = Faction::GetFactionOf(stack.volumeCell[layer].techniteFaction);
					const Grid::HCellIdU loc(stackIndex,layer);
					Technite*t = f->GetOwnTechnite(loc);

					//live technite must have visibility field of 1 around it
					for (int hd = -1; hd <= 1; hd++)
					{
						if (layer == 0 && hd < 0)
							continue;
						if (layer + hd >= Grid::CellStack::Range)
							continue;
						UINT32 layer2 = layer+hd;
						for (int k = 0; k < n.numNeighbors; k++)
						{
							UINT32 stackID = n.neighborID[k];
							if (!VerifyIsVisibleAndMatches(f,Grid::HCellIdU(stackID,layer2)))
								visibilityErrors++;
						}
						if (!VerifyIsVisibleAndMatches(f,Grid::HCellIdU(stackIndex,layer2)))
							visibilityErrors++;

					}
				}
			}
		});
		if (visibilityErrors > 0)
		{
			LogEvent(techniteError,"Detected "+String(visibilityErrors)+" visibility error(s). Rebuilding visibility grid");

			for (const auto&tf : Faction::allTechniteFactions)
			{
				tf->visibility.FlushAllData(false);
				tf->visibility.LockChangeChecking();
				float t= 0.f;
				foreach (tf->allMobiles,_m)
					_m->lock()->RegisterSensory(t,false);
				foreach (tf->structures,_s)
					_s->lock()->RegisterSensory(t,false);
				#ifndef NO_SATELLITES
					foreach (tf->satellites,_sat)
						_sat->lock()->Objects::WorldEntity::RegisterSensory(t,false);
				#endif
				foreach (tf->technites,tc)
					tf->RegisterTechniteSensory(tc->location.GetU(), t,false);
				tf->visibility.UnlockChangeChecking(0.f);
				ApplyCoverageChange(tf);

				Objects::ForeachWorldEntity([tf,&t](const Objects::PWorldEntity&e)
				{
					tf->CheckGridVisibility(e,t);
				});
			}

			VerifyGrid();

		}

	}


	#ifdef _DEBUG
		void			Faction::VerifyAtomicGrid()
		{
			std::atomic<count_t>	abandonedCounter = 0,
									totalCounter = 0;

			bool familyChanged[BaseGrid::NumFamilies] = {false};


			Concurrency::parallel_for(index_t(0),Grid::HighSetup::GridType::CellCount,[&abandonedCounter,&totalCounter,&familyChanged](index_t stackIndex)
			{
				AtomicGrid::CellStack&stack = atomicGrid.cellStack[stackIndex];
				Grid::CellStack&globalStack = Grid::actualWorld.cellStack[stackIndex];

				bool anyChanged = false;
				for (UINT32 layer = 0; layer < Grid::CellStack::Range; layer++)
				{
					if (stack.volumeCell[layer].content == Grid::Content::Technite)
					{
						totalCounter++;
						PFaction f = Faction::GetFactionOf(globalStack.volumeCell[layer].techniteFaction);
						const Grid::HCellIdU loc(stackIndex,layer);
						if (!f)
						{
							mutex.lock();
							LogEvent(techniteError,"Found homeless technite in "+loc.ToString()+", clearing cell");
							ReportLocation(loc);
							mutex.unlock();

							stack.volumeCell[layer].content = Grid::Content::Clear;
							abandonedCounter++;
							anyChanged = true;
							continue;
						}
						Technite*t = f->GetOwnTechnite(loc);
						if (!t)
						{
							mutex.lock();
							LogEvent(techniteError,"Found abandoned technite of faction "+f->GetLogName()+" in "+loc.ToString()+", clearing cell");
							ReportLocation(loc);
							mutex.unlock();

							stack.volumeCell[layer].content = Grid::Content::Clear;
							abandonedCounter++;
							anyChanged = true;
							continue;
						}
					}
				}
				if (anyChanged)
					atomicGrid.SignalStackChange(stackIndex);
			});


			if (abandonedCounter > 0)
				LogEvent(techniteError,"Cleared "+String(abandonedCounter)+"/"+String(totalCounter)+" abandoned technite cell(s)");


			std::atomic<count_t>	visibilityErrors = 0;

			Concurrency::parallel_for(index_t(0),Grid::HighSetup::GridType::CellCount,[&visibilityErrors](index_t stackIndex)
			{
				AtomicGrid::CellStack&stack = atomicGrid.cellStack[stackIndex];
				Grid::CellStack&globalStack = Grid::actualWorld.cellStack[stackIndex];
				const auto&n = Grid::high.grid.GetCell(stackIndex).neighborhood;
				for (UINT32 layer = 0; layer < Grid::CellStack::Range; layer++)
				{
					if (stack.volumeCell[layer].content == Grid::Content::Technite)
					{
						PFaction f = Faction::GetFactionOf(globalStack.volumeCell[layer].techniteFaction);
						const Grid::HCellIdU loc(stackIndex,layer);
						Technite*t = f->GetOwnTechnite(loc);

						//live technite must have visibility field of 1 around it
						for (int hd = -1; hd <= 1; hd++)
						{
							if (layer == 0 && hd < 0)
								continue;
							if (layer + hd >= Grid::CellStack::Range)
								continue;
							UINT32 layer2 = layer+hd;
							for (int k = 0; k < n.numNeighbors; k++)
							{
								UINT32 stackID = n.neighborID[k];
								if (!VerifyAtomicIsVisibleAndMatches(f,Grid::HCellIdU(stackID,layer2), loc))
									visibilityErrors++;
							}
							if (!VerifyAtomicIsVisibleAndMatches(f,Grid::HCellIdU(stackIndex,layer2), loc))
								visibilityErrors++;

						}
					}
				}
			});
			if (visibilityErrors > 0)
			{
				LogEvent(techniteError,"Detected "+String(visibilityErrors)+" visibility error(s). Rebuilding visibility grid");

				for (const auto&tf : Faction::allTechniteFactions)
				{
					tf->visibility.FlushAllData(false);
					tf->visibility.LockChangeChecking();
					float t = 0.f;
					foreach (tf->allMobiles,_m)
						_m->lock()->RegisterSensory(t,false);
					foreach (tf->structures,_s)
						_s->lock()->RegisterSensory(t,false);
					#ifndef NO_SATELLITES
						foreach (tf->satellites,_sat)
							_sat->lock()->Objects::WorldEntity::RegisterSensory(t,false);
					#endif
					foreach (tf->technites,tc)
						tf->RegisterTechniteSensory(tc->location.GetU(),t,false);
					tf->visibility.UnlockChangeChecking(0.f);
					ApplyCoverageChange(tf);


					Objects::ForeachWorldEntity([tf,&t](const Objects::PWorldEntity&e)
					{
						tf->CheckGridVisibility(e,t);
					});
				}

				VerifyGrid();

			}

		}
	#endif


	#endif


	void			Faction::UnregisterTechniteSensory(const Grid::HCellIdU&tecLoc, float&t, bool checkNow)
	{

		static Grid::HighSetup::TVolumeDescriptor desc;
		Grid::high.GenerateStep2Volume(tecLoc,desc);
		#ifdef CLIENT
			Grid::actualWorld.InsertSensor(desc);
		#else
			visibility.ChangeSensory(desc,t,(short)-1);
			if (checkNow)
				visibility.CheckChanges(t);
			static Array<Objects::PWorldEntity>	keys;
			seeing.exportKeys(keys);
			foreach (keys,k)
			{
				if ((*k)->owner.get() != this && !GridCanSee(*k))
					SignalDisappearance(*k,t,true);
			}
		#endif

	}

	void			Faction::RegisterTechniteSensory(const Grid::HCellIdU&tecLoc, float&t, bool checkNow)
	{

		static Grid::HighSetup::TVolumeDescriptor desc;
		Grid::high.GenerateStep2Volume(tecLoc,desc);
		#ifdef CLIENT
			Grid::actualWorld.RemoveSensor(desc);
		#else
			const auto&low = Grid::highToLowMap[tecLoc.stackID];
			static Grid::LowSetup::TVolumeDescriptor desc0;
			Grid::low.GenerateStep1MultiVolume(low.lowCells,low.lowCells[1] != low.Empty ? 2 : 1,tecLoc.layer,desc0);

			visibility.ChangeSensory(desc,t,(short)1);
			if (checkNow)
				visibility.CheckChanges(t);

			foreach (desc0.coverage,c)
			{
				for (UINT32 l = c->layerBegin; l < c->layerEnd; l++)
				{
					Objects::PWorldEntity e = Objects::GetEntity(Grid::LCellIdI(c->stackID,Grid::CellStack::ToHeight(l)));
					if (e && e->owner.get() != this && GridCanSee(e))
						SignalAppearance(e,t,true,false);
				}
			}
		#endif
	}




	#ifdef CLIENT

		void					Faction::SetTechniteColor(index_t techniteIndex, const TVec3<BYTE>&color)
		{
			Technite&t = technites[techniteIndex];
			t.color = color;
			t.usesColor = true;
			Grid::high.family[Grid::high.grid.GetCell(t.location.GetStackID()).family].SetOutdated(0.f);
		}


		#if defined HAS_WINDOW
		void				Faction::ChangeVision(const Grid::HCellIdU&center, short change)
		{
			bool familyChanged[BaseGrid::NumFamilies] = { false };

			Grid::TemplateHelper::ChangeSensory(center, change, Grid::actualWorld.cellStack.pointer(), familyChanged);

			for (index_t i = 0; i < BaseGrid::NumFamilies; i++)
				if (familyChanged[i])
					Grid::high.family[i].SetOutdated(0.f);
		}
		#endif

		void				Faction::ClearTechnites()
		{
			if (isOwnedByMe)
			{
				#ifdef HAS_WINDOW
					foreach (technites,t)
						ChangeVision(t->location.GetU(),-1);
				#endif
			}

			technites.Clear();
			techniteMap.Clear();
			ASSERT_EQUAL__(techniteMap.ThoroughCount(),technites.Count());

		}

		void				Faction::AppendTechnites(const ArrayData<Technite::TCompactState>&techs)
		{
			ASSERT_EQUAL__(techniteMap.Count(),technites.Count());
			
			foreach (techs,t)
			{
				index_t at = technites.Count();
				Technite&nt = technites.Append();
				nt.ImportState(*t);
				techniteMap.Set(nt.location.GetU(),at);

				index_t test;
				bool success = techniteMap.Query(nt.location.GetU(),test);
				ASSERT__(success);
				ASSERT_EQUAL__(test,at);
				#ifdef HAS_WINDOW
					if (isOwnedByMe)
						ChangeVision(nt.location.GetU(),+1);
				#endif
			}
			ASSERT_EQUAL__(techniteMap.ThoroughCount(),technites.Count());
		}




	#endif

	void					Faction::VerifyIntegrity()
	{
		foreach (technites,t)
		{
			Grid::HCellIdU loc = t->location.GetU();
			index_t at = t - technites.begin();
			index_t test;
			bool success = techniteMap.Query(loc,test);
			ASSERT__(success);
			ASSERT_EQUAL__(test,at);
			const auto&vcell = Grid::actualWorld.cellStack[loc.stackID].volumeCell[loc.layer];
			if (vcell.content == Grid::Content::Technite)
			{
				ASSERT_EQUAL__(vcell.techniteFaction, this->gridID);
			}
			else
			{
				LogEvent(techniteError,this->GetLogName() +": Expected technite content type in cell "+t->location.GetU().ToString()+", but got "+Grid::Content::Reinterpret(vcell.content).ToString());
			}
		}


	}






	Faction::ExecResult			Faction::Eat(Technite&technite, const Grid::HCellIdU&target, index_t iteration, WorkerData&data, bool destroy)
	{
		#ifdef _DEBUG
			WorkerData::LoggedEvent&ev = data.events.Append();
			ev.target = target;
			ev.source = technite.location.GetU();
			ev.type = WorkerData::LoggedEvent::Type::EatCellContent;
			ev.from = GetLastContent(target);
			ev.to = Grid::Content::Clear;
			ev.iteration = iteration;
			ev.destructive = destroy;
		#endif

		switch (GetLastContent(target))
		{
			case Grid::Content::Water:	//water does not block, nor yield anything
			{
				#ifdef _DEBUG
					ev.result = "Success, targeted cell contains water";
				#endif
				return ExecResult::Success;
			}
			break;
			case Grid::Content::Clear:	//clear never changes. Once it's empty, it's empty
			{
				if (destroy)
				{
					bool cantEatThis;
					IncreaseResources(technite, EatEntitiesIn(target,cantEatThis));	//this will be hard to make thread-safe

					#ifdef _DEBUG
						ev.result = cantEatThis? "Failed, cell clear, can't eat contained entities" : "Success, targeted cell is clear";
					#endif
			
					return cantEatThis ? ExecResult::CannotEatThis : ExecResult::Success;
				}
				else
				{
					if (AnyEntitiesIn(target))
					{
						#ifdef _DEBUG
							ev.result = "Failed, cell clear but contains entities. Operation is non-destructive";
						#endif

						return ExecResult::CannotGnawAtThis;
					}
					return ExecResult::NothingToDo;
				}
			}
			case Grid::Content::Technite:
			{
				if (!destroy)
				{
					#ifdef _DEBUG
						ev.result = "Failed, non-destructive, can't eat technites this way";
					#endif
					return ExecResult::CannotGnawAtThis;
				}
				if (iteration < 1)
				{
					#ifdef _DEBUG
						ev.result = "Failed, iteration too low to eat technite";
					#endif
					return ExecResult::Again;
				}
				if (iteration > 1)
				{
					#ifdef _DEBUG
						ev.result = "Failed, iteration too high to eat technite";
					#endif
					return ExecResult::OperationWindowMissed;
				}
			
				BYTE faction = Grid::actualWorld.cellStack[target.stackID].GetCellU(target.layer).techniteFaction;	//this should be thread-safe with only one faction being parallely active at any given time
			
				#ifdef _DEBUG
					ev.result = "Possibly successful, eating technite";
				#endif
				PFaction f = GetFactionOf(faction);
			
				Technite::Resources resources = f->EatTechnite(technite,target, iteration,data, f.get() == this);	//if technite has been eaten in the meantime EatTechnite will return empty resources
			
				IncreaseResources(technite, resources);
			
			}
			return GetLastContent(target) == Grid::Content::Clear ? ExecResult::Success : ExecResult::CannotEatThis;
			case Grid::Content::Lava:
			{
				#ifdef _DEBUG
					ev.result = "Failed, cannot eat lava";
				#endif
				return ExecResult::CannotEatThis;
			}
		}
		if (iteration > 0)
		{
			#ifdef _DEBUG
				ev.result = "Failed, cannot eat regular block after iteration 0";
			#endif
			return ExecResult::OperationWindowMissed;
		}

		if (!ReduceResources(technite, destroy ? Technite::ConsumeCost : Technite::GnawAtCost))
		{
			#ifdef _DEBUG
				ev.result = "Failed, insufficient resources to eat regular cell";
			#endif
			return ExecResult::TechniteLacksResources;
		}
		Technite::Resources	resources;
		
		#ifdef _DEBUG
			ev.result = "Possibly successful, degrading matter";
		#endif

		Grid::Content newContent = DegradeMatter(technite.location.GetU(), target, data, iteration, resources,destroy);	//DegradeMatter must be atomic on the specified cell
		
		IncreaseResources(technite, resources);
		
		return newContent == Grid::Content::Clear || !destroy ? ExecResult::Success : ExecResult::MoreWorkNeeded;
	}


	void					Faction::ProcessLighting(Technite&technite)
	{
		bool lit = true;


		unsigned layer = technite.location.GetLayer()+1;
		//Grid::CellStack::ToLayer(technite.location.height + 1);
		for (; layer < Grid::CellStack::Range; layer++)
		{
			Grid::Content top = GetGlobalGridContent(Grid::HCellIdU(technite.location.GetStackID(), layer));
			if (top != Grid::Content::Clear && top != Grid::Content::Water)
			{
				lit = false;
				break;
			}
		}
		technite.state.SetLit(lit);

		if (lit)
		{
			Technite::Resources::Increase(technite.resources.energy, technite.GetLightEnergyYield());
		}
	}

	void							Faction::CheckState(Technite&technite)
	{

		const Grid::HCellIdU loc = technite.location.GetU();
		bool cannotStandHereDies = !ValidTechniteLocation(loc);

		Grid::Content content = GetLastContent(loc);


		BYTE ttl = technite.state.GetTTL();
		if (limitTechniteLife)
		{
			if (ttl > 0)
				ttl--;
			technite.state.SetTTL(ttl);
		}

		if (content != Grid::Content::Technite
			|| Grid::actualWorld.cellStack[loc.stackID].GetCellU(loc.layer).techniteFaction != this->gridID
			|| ttl == 0
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
		Grid::HCellIdU target;
		if (!technite.DecodeAbsoluteTarget(target))
			return Technite::TaskResult::BadCommand;
		//		const auto&targetCell = Grid::actualWorld.cellStack[target.stackID].GetCellI(target.height);


		if (technite.nextTask == Technite::Task::None)
			return Technite::TaskResult::NothingToDo;

		switch (technite.nextTask)
		{
			case Technite::Task::SelfTransformToType:
			{
				const Grid::HCellIdU target = technite.location.GetU();

				#ifdef _DEBUG
					WorkerData::LoggedEvent&ev = data.events.Append();
					ev.target = target;
					ev.source = ev.target;
					ev.type = WorkerData::LoggedEvent::Type::SelfTransform;
					ev.from = GetLastContent(target);
					ev.to = Grid::Content::Reinterpret(technite.taskParameter);
					ev.iteration = iteration;
					ev.targetingOwnTechniteFaction = true;
				#endif

				ASSERT_EQUAL__(iteration,0);

				if (technite.isDead.Set(false,true))
				{
					anyDied = true;
					if (technite.taskParameter >= ARRAYSIZE(Technite::MatterYield) || Technite::MatterYield[technite.taskParameter] == 0)
					{
						#ifdef _DEBUG
							ev.result = "Failed, bad target type #"+String(technite.taskParameter);
						#endif
						return Technite::TaskResult::BadCommand;
					}
					if (technite.resources.matter < Technite::MatterYield[technite.taskParameter])
					{
						#ifdef _DEBUG
							ev.result = "Failed, not enough resources to change into desired type";
						#endif
						return Technite::TaskResult::TechniteLacksResources;
					}
					BYTE expect = Grid::Content::Technite;
					
					if (atomicGrid.cellStack[target.stackID].volumeCell[target.layer].content.compare_exchange_strong(expect,technite.taskParameter))
					{
						atomicGrid.SignalStackChange(target.stackID);
						#ifdef _DEBUG
							ev.result = "Success";
						#endif
					}
					else
					{
						//this should not happen. transformation will happen in iteration 0, eating technites is not allowed during iteration 0
						#ifdef _DEBUG
							ev.result = "Failed, unable to set cell type to new type";
						#endif
					}

					return Technite::TaskResult::Success;	//dead either way, no need to report this
				}
				#ifdef _DEBUG
					ev.result = "Failed, technite is already dead";
				#endif
				return Technite::TaskResult::NoTechniteAtDestination;
			}
			break;
			case Technite::Task::GrowTo:
				return GrowTo(technite, target, iteration,data);
			case Technite::Task::ConsumeSurroundingCell:
			case Technite::Task::GnawAtSurroundingCell:
				return Eat(technite, target, iteration, data, technite.nextTask == Technite::Task::ConsumeSurroundingCell);
			case Technite::Task::TransferEnergyTo:
			{
				#ifdef _DEBUG
					WorkerData::LoggedEvent&ev = data.events.Append();
					ev.target = target;
					ev.source = technite.location.GetU();
					ev.type = WorkerData::LoggedEvent::Type::TransferEnergy;
					ev.from = GetLastContent(target);
					ev.to = ev.from;
					ev.iteration = iteration;
					ev.targetingOwnTechniteFaction = true;
				#endif


				if (iteration < 4)
				{
					#ifdef _DEBUG
						ev.result = "Failed, iteration too low";
					#endif
					return ExecResult::Again;
				}
				Technite*other = GetOwnTechnite(target);
				if (!other)
				{
					#ifdef _DEBUG
						ev.result = "Failed, cannot locate technite object";
					#endif
					return Technite::TaskResult::NoTechniteAtDestination;
				}
				Technite::Resources	transfer(technite.taskParameter,0);
				if (!ReduceResources(technite,transfer ))
				{
					#ifdef _DEBUG
						ev.result = "Failed, insufficient resources to transfer "+transfer.ToString();
					#endif
					return Technite::TaskResult::TechniteLacksResources;
				}
				IncreaseRemoteResources(*other, transfer, data);
				return Technite::TaskResult::Success;
			}
			case Technite::Task::TransferMatterTo:
			{
				#ifdef _DEBUG
					WorkerData::LoggedEvent&ev = data.events.Append();
					ev.target = target;
					ev.source = technite.location.GetU();
					ev.type = WorkerData::LoggedEvent::Type::TransferMatter;
					ev.from = GetLastContent(target);
					ev.to = ev.from;
					ev.iteration = iteration;
					ev.targetingOwnTechniteFaction = true;
				#endif

				if (iteration < 4)
				{
					#ifdef _DEBUG
						ev.result = "Failed, iteration too low";
					#endif
					return ExecResult::Again;
				}
				Technite*other = GetOwnTechnite(target);
				if (!other)
				{
					#ifdef _DEBUG
						ev.result = "Failed, cannot locate technite object";
					#endif
					return Technite::TaskResult::NoTechniteAtDestination;
				}
				Technite::Resources	transfer(0,technite.taskParameter);
				if (!ReduceResources(technite, transfer))
				{
					#ifdef _DEBUG
						ev.result = "Failed, insufficient resources to transfer "+transfer.ToString();
					#endif
					return Technite::TaskResult::TechniteLacksResources;
				}
				IncreaseRemoteResources(*other, transfer, data);
				return Technite::TaskResult::Success;
			}
			case Technite::Task::Scan:
			{
				Technite::Resources	useEnergy(technite.taskParameter,0);
				if (!ReduceResources(technite, useEnergy))
				{
					return Technite::TaskResult::TechniteLacksResources;
				}
				data.addSensor.Append(std::make_pair (technite.location.GetU(),technite.taskParameter));


				return Technite::TaskResult::Success;
			}
		}
		return Technite::TaskResult::BadCommand;
	}

	//ParallelLoop	loop();

	#ifndef CLIENT

	void		Faction::ProcessLighting()
	{
		Concurrency::parallel_for(index_t(0), technites.Count(), [this](index_t i)
		{
			ProcessLighting(technites[i]);
		});
	}

	#ifdef _DEBUG
		void		Faction::ReportLocation(const Grid::HCellIdU&loc)
		{
			static Buffer<WorkerData::LoggedEvent>	events;
			events.Clear();
			foreach (workerData, data2)
				foreach (data2->events,ev)
					if (ev->target == loc)
					{
						events << *ev;
						events.last().workerIndex = (index_t)(data2-workerData.pointer());
					}
			std::sort(events.begin(),events.end());

			foreach (events, ev) 
				LogEvent(EventClass( EventSignificance::Uncommon, EventFlavor::ServerPrivate ),
					String(ev->stamp)
					+ ": "+String(ev->type.ToString()).subString(0,5)
					+", it="+String(ev->iteration)
					+", worker="+String(ev->workerIndex)
					+", source="+String(ev->source.ToString())
					+", destruct="+String(ev->destructive)
					+", myfact="+String(ev->targetingOwnTechniteFaction)
					+", type0="+String(ev->from.ToString())
					+", type1="+String(ev->to.ToString())
					+", result='"+ev->result+"'"
					);

		}
	#endif
	void		Faction::Process(float t)
	{
		anyDied = false;

		//std::cout << "processing faction " << (int)gridID << std::endl;
		
		Concurrency::parallel_for(index_t(0), technites.Count(), [this](index_t i)
		{
			CheckState(technites[i]);
			technites[i].taskResult = Technite::TaskResult::Again;
		});

		if (anyDied)
		{
			static Buffer<Technite>	newField;
			//techniteMap.Clear();
			for (const auto&tec : technites)
			{
				Grid::HCellIdU loc = tec.location.GetU();	
				if (!tec.isDead)
				{
					index_t at = newField.Count();
					newField.Append(tec);
					techniteMap.Set(loc, at);
				}
				else
				{
					UnregisterTechniteSensory(loc,t);
					//visibility.ChangeTechniteSensory(loc, t, -1);
					techniteMap.Unset(loc);
					atomicGrid.cellStack[loc.stackID].volumeCell[loc.layer].content = Grid::Content::Clear;
					atomicGrid.SignalStackChange(loc.stackID);

					const auto mapping = Grid::highToLowMap[loc.stackID];
					Objects::SignalClusterAndNeighborsChange(Grid::LCellIdI(mapping.lowCells[0],Grid::CellStack::ToHeight(loc.layer)),t);
				}
			}
			newField.swap(technites);
			newField.Clear();
			ASSERT_EQUAL__(technites.Count(), techniteMap.Count());
			anyDied = false;
		}



		
		count_t numWorkers = ParallelLoop::global_instance.getWorkerCount();
		workerData.SetSize(numWorkers);

		index_t iteration = 0;
		for (; ; )
		{
			volatile bool anyAgain = false;




			parallelFor(technites.Count(), 64, [this, &anyAgain, iteration](index_t i, index_t workerIndex)
			{
				Technite&tec = technites[i];
				if (tec.isDead || tec.taskResult != Technite::TaskResult::Again)
					return;
				Technite::TaskResult rc = tec.taskResult = ExecuteTask(tec, iteration, workerData[workerIndex]);
				if (rc != Technite::TaskResult::MoreWorkNeeded && !tec.isDead)
				{
					if (rc == ExecResult::Again)
						anyAgain = true;
					else
						tec.nextTask = Technite::Task::None;
				}
			});
			iteration++;
			if (!anyAgain)
				break;
		}
		

		if (anyDied)
		{
			//std::cout << "some died" << std::endl;

			static Buffer<Technite>	newField;
			//techniteMap.Clear();
			for (const auto&tec : technites)
			{
				Grid::HCellIdU loc = tec.location.GetU();	
				if (!tec.isDead)
				{
					index_t at = newField.Count();
					newField.Append(tec);
					techniteMap.Set(loc, at);
				}
				else
				{
					UnregisterTechniteSensory(loc,t);
					techniteMap.Unset(loc);

					const auto mapping = Grid::highToLowMap[loc.stackID];
					Objects::SignalClusterAndNeighborsChange(Grid::LCellIdI(mapping.lowCells[0],Grid::CellStack::ToHeight(loc.layer)),t);


					#ifdef _DEBUG
					if (GetLastContent(loc) == Grid::Content::Technite)
					{
						Technite*t2 = GetOwnTechnite(loc);
						if (!t2)
						{
							bool found = false;
							foreach(workerData, data)
							{
								if (data->newTechnites.Contains(loc))
								{
									found = true;
									break;
								}
							}
							if (!found)
							{
								LogEvent(techniteError,"Error: leftover technite found in cell "+loc.ToString());
								ReportLocation(loc);

								atomicGrid.cellStack[loc.stackID].volumeCell[loc.layer].content = Grid::Content::Clear;
								atomicGrid.SignalStackChange(loc.stackID);
							}
						}
					}
					#endif

					//BYTE changeTo = Grid::Content::Clear;
				}
			}
			newField.swap(technites);
			newField.Clear();
			ASSERT_EQUAL__(technites.Count(), techniteMap.Count());
		}

		count_t deathByBadSupport = 0,
				deathByLostContent = 0,
				totalNew = 0;

		bool firstLog = true;
		foreach(workerData, data)
		{
			totalNew += data->newTechnites.Count();
			for (const auto&loc : data->newTechnites)
			{
				if (techniteMap.IsSet(loc))
					continue;

				const auto mapping = Grid::highToLowMap[loc.stackID];
				Objects::SignalClusterAndNeighborsChange(Grid::LCellIdI(mapping.lowCells[0],Grid::CellStack::ToHeight(loc.layer)),t);


				if (GetLastContent(loc) != Grid::Content::Technite)
				{
					//this hasn't happened on its own for quite some time. Now this is most likelye due to grid changes caused by the signalling
					//#ifdef _DEBUG
					//	if (firstLog)
					//	{
					//		firstLog = false;

					//		LogEvent(EventType::TechniteLogic,nullptr,"Lost content of new technite at location "+loc.ToString()+", reporting changes...");

					//		ReportLocation(loc);
					//	}		
					//#endif


					deathByLostContent ++;
					continue;
				}

				if (!ValidTechniteLocation(loc))
				{
					atomicGrid.cellStack[loc.stackID].volumeCell[loc.layer].content = Grid::Content::Clear;
					atomicGrid.SignalStackChange(loc.stackID);
					
					deathByBadSupport++;
					continue;
				}

				index_t at = technites.Count();
				Technite tech;
				tech.location = loc;
				tech.state =  Technite::CompressedState(false,Technite::GetInitialTTLAtLayer(loc.layer));

				//Grid::actualWorld.cellStack[tech.location.stackID].SetLocalTechniteFaction(tech.location.height, this->gridID);
				Technite&t1 = technites.Append(tech);
				ProcessLighting(t1);

				DBG_ASSERT_EQUAL__(Grid::actualWorld.cellStack[loc.stackID].volumeCell[loc.layer].techniteFaction,this->gridID);

				techniteMap.Set(loc, at);
				RegisterTechniteSensory(loc,t);


			}
			data->newTechnites.Clear();
			ASSERT_EQUAL__(technites.Count(), techniteMap.Count());



			for (const auto&loc : data->addSensor)
			{
				float radius = sqrt(float(loc.second));
				TSensor	sensor(Grid::ToWorld(loc.first),radius);
				sensors.Append(sensor);
				RegisterSensorCreation(sensor,false,t,false);
			}
			data->addSensor.Clear();


		}

		if (deathByBadSupport)
			LogEvent(techniteError,this->GetLogName()+": Lost "+String(deathByBadSupport)+"/"+String(totalNew)+" new technite(s) due to lack of terrain support");
		if (deathByLostContent)
			LogEvent(techniteError,this->GetLogName()+": Lost "+String(deathByLostContent)+"/"+String(totalNew)+" new technite(s) due to terrain content mismatch");


		
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

		#ifdef _DEBUG
			VerifyAtomicGrid();

			foreach(workerData, data)
			{
				data->events.Clear();
			}
		#endif
		
		visibility.CheckChanges(t);

		
	}

	#endif


	Technite* Faction::GetOwnTechnite(const Grid::HCellIdU&target)
	{
		index_t at;
		if (!techniteMap.Query(target, at))
			return nullptr;
		Technite*rs = technites + at;
		if (rs->isDead)
			return nullptr;
		return rs;
	}

	bool			Faction::SpawnNewTechnite(const Grid::HCellIdU&source, const Grid::HCellIdU&target, WorkerData&data, index_t iteration)
	{
		const UINT32 toLayer = target.layer;
		ASSERT_LESS__(toLayer, Grid::CellStack::Range);

		#ifdef _DEBUG
			WorkerData::LoggedEvent&ev = data.events.Append();
			ev.target = target;
			ev.source = source;
			ev.type = WorkerData::LoggedEvent::Type::SpawnTechnite;
			ev.iteration = iteration;
			ev.destructive = false;
			ev.from = GetLastContent(target);
			ev.to = Grid::Content::Technite;
		#endif


		if (!ValidTechniteLocation(target))
		{
			#ifdef _DEBUG
				ev.result = "Failed, invalid location for technite";
			#endif
			return false;
		}

		AtomicGrid::CellStack::Cell&toCell = atomicGrid.cellStack[target.stackID].volumeCell[toLayer];

		BYTE expect0 = Grid::Content::Clear;
		BYTE expect1 = Grid::Content::Water;
		if (!toCell.content.compare_exchange_strong(expect0, Grid::Content::Technite)
			&&
			!toCell.content.compare_exchange_strong(expect1, Grid::Content::Technite))
		{
			#ifdef _DEBUG
				ev.result = "Failed, unable to atomically change grid type to technite";
			#endif
			return false;
		}
		ASSERT1__(toCell.content==Grid::Content::Technite,toCell.content);
		#ifdef _DEBUG
			ev.to = GetLastContent(target);
			ev.result = "Success, technite created, appending new technite to field";
		#endif
		atomicGrid.SignalStackChange(target.stackID);
		Grid::actualWorld.cellStack[target.stackID].volumeCell[toLayer].techniteFaction = this->gridID;

		//std::cout << "Creating technite at " << target.ToString() << std::endl;

		data.newTechnites << Grid::HCellIdU(target.stackID,toLayer);
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
		data.increaseResources << std::make_pair(technite.location.GetU(), by);
	}



	void		Create()
	{

	}
	void		Destroy()
	{


	}

	/*static*/ PFaction			Faction::CreateFromFile(const TUniqueID&id, BYTE gridID, bool copyInitialWorld/* = false*/)
	{
		ASSERT__(gridID != 0xFF);
		PFaction faction(new Faction(id));
		//if (id != TUniqueID::Empty())
		//	faction->id = id;
		faction->gridID = gridID;
		#ifndef CLIENT
			Technite::factionColor[gridID] = faction->color;
		#endif
		gridIDMap[gridID] = faction;

		Faction::allTechniteFactions.Append(faction);
		#ifndef CLIENT
			Super::all.Append(faction);
			if (copyInitialWorld)
				faction->currentState = Grid::initialWorld;
			else
				faction->currentState.FlushAllData();
		#endif
		return faction;
	}

	/*static*/ PFaction			Faction::Create(bool copyInitialWorld/* = false*/)
	{
		for (index_t i = 0; i < ARRAYSIZE(gridIDMap); i++)
			if (!gridIDMap[i])
			{
				return CreateFromFile(TUniqueID::Empty(),(BYTE)i,copyInitialWorld);
			}
		return PFaction();
	}


	/*virtual override*/ void			Faction::OnKill()
	{
		gridIDMap[gridID].reset();
		allTechniteFactions.FindAndErase(shared_from_this());
		bool familyChanged[BaseGrid::NumFamilies] = { false };
		foreach(this->technites, t)
		{
			UINT32 stackID = t->location.GetStackID();
			Grid::actualWorld.cellStack[stackID].MakeLocalHole(t->location.GetHeight());
			familyChanged[Grid::high.grid.GetCell(stackID).family] = true;
		}
		for (index_t i = 0; i < BaseGrid::NumFamilies; i++)
			if (familyChanged[i])
			{
				Grid::low.family[i].SetOutdated(0);
				Grid::high.family[i].SetOutdated(0);
			}
	}

	void Faction::Destroy(float t)
	{
		#ifdef CLIENT
			allKnown.FindAndErase(shared_from_this());
		#else
			foreach(Super::all, f)
			{
				if (f->lock().get() == this)
				{
					Super::all.erase(f);
					break;
				}
			}
		#endif
		OnKill();
	}



	/*static*/ bool Faction::ValidTechniteLocation(const Grid::HCellIdU&location)
	{
		if (location.layer == 0)
		{
			return Technite::SupportedBy(Grid::Content::Reinterpret(Grid::CellStack::bottom.content));
		}
		if (!Technite::SupportedBy(GetLastContent(location - 1)))
		{
			const auto&n = Grid::high.grid.GetCell(location.stackID).neighborhood;
			bool anyGood = false;
			for (int k = 0; k < n.numNeighbors; k++)
			{
				if (Technite::SupportedBy(GetLastContent(Grid::HCellIdU(n.neighborID[k], location.layer)))
					&&
					Technite::SupportedBy(GetLastContent(Grid::HCellIdU(n.neighborID[k], location.layer - 1)))
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

	/*static*/ bool Faction::ValidGlobalTechniteLocation(const Grid::HCellIdU&location)
	{
		if (location.layer == 0)
		{
			return Technite::SupportedBy(Grid::Content::Reinterpret(Grid::CellStack::bottom.content));
		}
		if (!Technite::SupportedBy(GetGlobalGridContent(location - 1)))
		{
			const auto&n = Grid::high.grid.GetCell(location.stackID).neighborhood;
			bool anyGood = false;
			for (int k = 0; k < n.numNeighbors; k++)
			{
				if (Technite::SupportedBy(GetGlobalGridContent(Grid::HCellIdU(n.neighborID[k], location.layer)))
					&&
					Technite::SupportedBy(GetGlobalGridContent(Grid::HCellIdU(n.neighborID[k], location.layer - 1)))
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


	index_t					Faction::CreateSingleTechnite(const Grid::HCellIdU&location, float t)
	{
		if (location.layer >= Grid::CellStack::Range)
			return InvalidIndex;

		if (techniteMap.IsSet(location))
			return InvalidIndex;
		if (!ValidGlobalTechniteLocation(location))
			return InvalidIndex;
		//Grid::Content content = GetGlobalGridContent(location);
		//if (content != Grid::Content::Clear && content != Grid::Content::Water)
		//	return InvalidIndex;

		index_t at = technites.Count();
		Technite&tech = technites.Append();
		techniteMap.Set(location, at);
		tech.location = location;
		ProcessLighting(tech);

		Grid::CellStack&stack = Grid::actualWorld.cellStack[location.stackID];
		ASSERT__(stack.SetLocalToTechniteContentU(location.layer, this->gridID));

		index_t family = Grid::high.grid.GetCell(location.stackID).family;

		Grid::low.family[family].SetOutdated(t);
		Grid::high.family[family].SetOutdated(t);

		#ifndef CLIENT
			RegisterTechniteSensory(tech.location.GetU(),t);
		#endif

		return at;
	}







}



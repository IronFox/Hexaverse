#include <global_root.h>
#include "worldData.h"
#include <container/array2d.h>

#include "../globalEvents.h"

namespace World
{
	using namespace Grid;

	Data			actual;
	#ifndef CLIENT
		Data		initial;
	#endif


	bool IsRoad(BYTE /*CellStack::Cell::Content::type_t*/ t, bool clearIsRoad)
	{
		return (clearIsRoad && t == Content::Clear) || t == Content::Road || t == Content::Foundation;
	}

	void	FlushAllWorldData()
	{
		#ifndef CLIENT
			//GenerateWorldVolume(config,discovery);
			initial.FlushTerrain();
			initial.FlushAndValidateSensory();
		#endif
		actual.FlushTerrain();
		actual.FlushAndValidateSensory();
	}


	/*virtual*/					Data::~Data()
	{
		if (!IsGlobalInstance() && !application_shutting_down)
		{
			actual.onTerrainChange.Unregister(observerKey);
			observerKey = InvalidIndex;
		}
	}

	void					Data::CopyGridContent(const Data&other)
	{
		for (UINT32 stack = 0; stack < Grid::HighSetup::GridType::CellCount; stack++)
		{
			const auto& fromStack = other.cellStack[stack];
			auto& toStack = cellStack[stack];
			for (unsigned layer = 0; layer < Grid::CellStack::Range; layer++)
			{
				const auto&from = fromStack.volumeCell[layer];
				auto&to = toStack.volumeCell[layer];
				to.content = from.content;
				#ifdef HAS_WINDOW
					to.intLightLevel = from.intLightLevel;
					to.submergence = from.submergence;
				#endif
			}
			#ifdef HAS_WINDOW
				toStack.highestOpaqueCell = fromStack.highestOpaqueCell;
				toStack.lightingChangedTo = fromStack.lightingChangedTo;
				toStack.lowestOpenCell = fromStack.lowestOpenCell;
				toStack.waterRange = fromStack.waterRange;
			#endif
		}

		BaseGrid::FamiliesChanged changed;
		changed.SetAll();
		SignalChange(changed,0,true);
	}



	void	Create()
	{
		FlushAllWorldData();

		Sensor::StaticCreate();

		#ifdef HAS_WINDOW
			actual.UpdateLighting();
		#endif

		GlobalEvents::terrainFlush << FlushAllWorldData;
		GlobalEvents::objectFlush << [](){
			actual.ResetStructureCounters(true);
		};
	}
	
	//void	FlushStackFamily(Ctr::Array<CellStack>&cellStack,  BYTE family)
	//{
	//	const index_t offset = high.grid.GetFamilyOffset(family);
	//	const count_t range = high.grid.CountFamilyMembers(family);
	//	for (index_t i = offset; i < offset+range; i++)
	//		cellStack[i].ZeroInit();		
	//
	//}
		


	bool	Data::ResetStructureCounters(bool resetVisibility, BaseGrid::FamiliesChanged*changedFamilies/*=nullptr*/)
	{
		bool rs= false;
		for (index_t i = 0; i < cellStack.Count(); i++)
		{
			bool changed = cellStack[i].ResetStructureCounters(resetVisibility);
			if (changed)
			{
				rs = true;
				if (changedFamilies != nullptr)
					changedFamilies->Set(Grid::high.grid.GetCell(i).family);
			}
		}
		return rs;
	}


	#ifdef HAS_WINDOW
	bool	Data::UpdateLighting(bool force)
	{
		if (!lightingOutdated && !force)
			return false;
		lightingOutdated = false;
		Timer::Time t0 = timer.Now();

		UINT32 top = 0,bottom = CellStack::Range;
		count_t cnt = 0;
		foreach (cellStack,stack)
		{
			if (stack->lightingChangedTo > stack->lowestOpenCell)
			{
				cnt ++;
				top = std::max<UINT32>(top,stack->lightingChangedTo);
				bottom = std::min<UINT32>(bottom,stack->lowestOpenCell > 0 ? stack->lowestOpenCell-1 : 0);
			}
		}
		if (cnt < 512)
		{
			static Buffer<UINT32>	ids;
			ids.Clear();

			foreach (cellStack,stack)
			{
				if (stack->lightingChangedTo > stack->lowestOpenCell)
				{
					ids << stack - cellStack.pointer();
				}
			}


			UINT32 tp = std::min(top,CellStack::Range-1);
			for (UINT32 h = tp; /*h > 0 && */h >= bottom && h <= tp; h--)
			{
				Concurrency::parallel_for(index_t(0), ids.Count(),[this,h](index_t _i)
				{
					const UINT32 i = ids[_i];
					CellStack&thisStack = cellStack[i];
					if (h > thisStack.lightingChangedTo)
						return;
					int l = 0xFF;
				
					if (h < CellStack::Range-1)
					{
						l = thisStack.volumeCell[h+1].intLightLevel;
						const auto&n = high.grid.GetCell(i).neighborhood;
						for (int j = 0; j < n.numNeighbors; j++)
							l += cellStack[n.neighborID[j]].volumeCell[h+1].intLightLevel;
						l /= 1 + n.numNeighbors;
					}
					thisStack.volumeCell[h].SetIntLightLevel(l);
					BYTE next = h < CellStack::Range-1 ? thisStack.volumeCell[h+1].GetIncreasedSubmergence() : 0;
					thisStack.volumeCell[h].SetSubmergence(next);
				});
			}
			foreach (cellStack,stack)
				stack->lightingChangedTo = 0;

		}
		else
		{
			UINT32 tp = std::min(top,CellStack::Range-1);
			for (UINT32 h = tp; /*h > 0 && */h >= bottom && h <= tp; h--)
			{
				Concurrency::parallel_for(UINT32(0), HighSetup::GridType::CellCount,[this,h](UINT32 i)
				{
					CellStack&thisStack = cellStack[i];
					if (h > thisStack.lightingChangedTo)
						return;
					int l = 0xFF;
				
					if (h < CellStack::Range-1)
					{
						l = thisStack.volumeCell[h+1].intLightLevel;
						const auto&n = high.grid.GetCell(i).neighborhood;
						for (int j = 0; j < n.numNeighbors; j++)
							l += cellStack[n.neighborID[j]].volumeCell[h+1].intLightLevel;
						l /= 1 + n.numNeighbors;
					}
					thisStack.volumeCell[h].SetIntLightLevel(l);
					thisStack.volumeCell[h].SetSubmergence(h < CellStack::Range-1 ? thisStack.volumeCell[h+1].GetIncreasedSubmergence() : 0);
				});
			}
			foreach (cellStack,stack)
				stack->lightingChangedTo = 0;
		}

		LogEvent(EventSignificance::PerformanceMeasurement,  "Updated lighting in "+String(timer.GetSecondsSince(t0))+" seconds");
	
		return true;
	}
/*
		HighSetup::TDistanceField	field;
		void	Data::SignalLightingChange(const HCellIdI&stackID)
		{
			lightingOutdated = true;
			structureChanged = true;
			const UINT r = CellStack::ToLayer(stackID.height)+1;
			high.UpdatePlanarDistanceField(stackID.stackID,r,field);
			for (index_t i = 0; i < HighSetup::TDistanceField::NumEntries; i++)
				if (field.distance[i] != 0xFFFFFFFF)
				{
					cellStack[i].lightingChangedTo = r-field.distance[i];
					SignalFamilyStructureChange(high.grid.GetCell(i).family);
				}
		}

		#ifdef HYBRID_GRID
			void	Data::SignalLightingChange(const LCellIdI&stackID)
			{
				lightingOutdated = true;
				structureChanged = true;
				const UINT r = CellStack::ToLayer(stackID.height) + 2;
				UINT32 higher = low.grid.GetCell(stackID.stackID).higherEquivalent;
				high.UpdatePlanarDistanceField(higher,CellStack::ToLayer(stackID.height),field);
				for (index_t i = 0; i < HighSetup::TDistanceField::NumEntries; i++)
					if (field.distance[i] != 0xFFFFFFFF)
					{
						cellStack[i].lightingChangedTo = std::max<BYTE>( cellStack[i].lightingChangedTo, r-field.distance[i]);
						SignalFamilyStructureChange(high.grid.GetCell(i).family);
					}
			}
		#endif*/

	#endif


	void	Data::Explode(const HCellIdI&id,float radius, float atTime)
	{
		const float3 center = high.cells[id.stackID].centerVertex.xyz * ToRadiusf(0.5f + id.height);
		const float r2 = M::Sqr(radius);
		Ctr::Queue<UINT32> queue;
		
		#ifdef HAS_WINDOW
			Queue<UINT32> changed;
		#endif
		queue << id.stackID;

		M::Box<> b;
		b.SetCenter(center,radius);

		bool processed[HighSetup::GridType::CellCount] = {false};

		ChangeSequence seq = BeginChangeSequence(atTime);

		UINT32 current;
		while (queue >> current)
		{
			if (processed[current])
				continue;
			processed[current] = true;
			seq.SelectStack(current);

			const M::TVec3<>&dir = high.cells[current].centerVertex.xyz;
			M::TVec3<> lower = dir * ToRadius(-CellStack::HalfRange);
			M::TVec3<> upper = dir * ToRadius(CellStack::HalfRange);
			float dummy = Vec::distance(upper,lower);
			if (b.Contains(lower) || b.Contains(upper) || M::Obj::DetectOpticalBoxIntersection(b,lower,(upper-lower)/dummy,dummy))
			{
				bool anyChanged = false;
				for (int h = -CellStack::HalfRange; h < CellStack::HalfRange; h++)
				{
					M::TVec3<> at = dir * ToRadius(h);
					if (Vec::quadraticDistance(center,at) < r2)
					{
						if (seq.Clear(h))
						{
							UINT layer = CellStack::ToLayer(h);
							#ifdef HAS_WINDOW
								cellStack[current].lightingChangedTo = std::max<BYTE>(cellStack[current].lightingChangedTo, layer);
							#endif
							//ASSERT_LESS__(layer,CellStack::Range);

							//SignalLightingChange(HCellIdI(current,h+1));
							anyChanged = true;
						}
					}
				}
				if (anyChanged)
				{
					#ifdef HAS_WINDOW
						changed << current;
					#endif
				}

				const auto&n = high.grid.GetCell(current).neighborhood;
				for (BYTE k = 0; k < n.numNeighbors; k++)
				{
					queue << n.neighborID[k];
				}
			}
		}
		//count_t counter = 0;
		//BYTE smallest = 0xff;
		#ifdef HAS_WINDOW
			while (changed >> current)
			{
				BYTE h = cellStack[current].lightingChangedTo;
				//ASSERT_LESS__(h,CellStack::Range);
				//if (h+1 >= cellStack[current].lowestOpenCell)
				if (!h)
					continue;
				h--;
				//smallest = std::min(smallest,h);
				//counter++;

		
				const auto&n = high.grid.GetCell(current).neighborhood;
				for (BYTE k = 0; k < n.numNeighbors; k++)
				{
					UINT32 nid = n.neighborID[k];
					if (cellStack[nid].lightingChangedTo < h)
					{
						cellStack[nid].lightingChangedTo = h;
						changed << nid;
					}
				}
			}
		#endif
		//ShowMessage(counter);
		//ShowMessage(smallest);

	
	}



	void					Data::ExportDiscovered(Data&target, WorldDiscovery discoveryLevel) const
	{
		for (index_t i = 0; i < HighSetup::GridType::CellCount; i++)
		{
			auto&stack = target.cellStack[i];
			stack = cellStack[i];	//raw copy
			switch (discoveryLevel)
			{
				case WorldDiscovery::None:
					for (unsigned j = 0; j < CellStack::Range; j++)
					{
						CellStack::Cell&c = stack.volumeCell[j];
						if (c.content != Content::Clear)
						{
					
							c.content = Content::Unknown;
							#ifndef CLIENT
							DBG_ASSERT__(c.structureCounter == 0);
							#endif
						}
					}
				break;
				case WorldDiscovery::Water:
					for (unsigned j = 0; j < CellStack::Range; j++)
					{
						CellStack::Cell&c = stack.volumeCell[j];
						if (c.content != Content::Clear && c.content != Content::Water)
						{
							c.content = Content::Unknown;
							#ifndef CLIENT
							DBG_ASSERT__(c.structureCounter == 0);
							#endif
						}
					}
				break;
				case WorldDiscovery::Liquids:
					for (unsigned j = 0; j < CellStack::Range; j++)
					{
						CellStack::Cell&c = stack.volumeCell[j];
						if (c.content != Content::Clear && !c.IsLiquid())
						{
							c.content = Content::Unknown;
							#ifndef CLIENT
							DBG_ASSERT__(c.structureCounter == 0);
							#endif
						}
					}
				break;
			}
			#ifdef HAS_WINDOW
				if (discoveryLevel != WorldDiscovery::FullTerrain)
					stack.UpdateRange();
			#endif
		}
	}





	//void					Data::MapSensorState(UINT32 lowCellCenter, float radius, int refHeight, int groundPenetration, SkySensorState&outState) const
	//{
	//	outState.Clear();
	//	outState.groundPenetration = groundPenetration;


	//	Queue<UINT32>	openCells;
	//	const UINT32 highCenter = Grid::low.grid.GetCell(lowCellCenter).higherEquivalent;
	//	openCells.Push(highCenter);

	//	

	//	const float r = ToRadius(refHeight);
	//	//radius /= r;
	//	const float hr2 = M::Sqr(radius/r);
	//	const M::TVec3<>& center = high.cells[highCenter].centerVertex.xyz;
	//	UINT32 current;
	//	while (openCells >> current)
	//	{
	//		float d2 = Vec::quadraticDistance(high.cells[current].centerVertex.xyz,center);
	//		if (d2 > hr2)
	//			continue;
	//		if (!outState.coverageMap.SetNew(current,SkySensorState::Uninitialized))
	//			continue;
	//		auto&cell = outState.coverage.Append();
	//		cell.downToHeight = SkySensorState::Uninitialized;
	//		cell.hStackID = current;
	//		const auto&n = high.grid.GetCell(current).neighborhood;
	//		for (int k = 0; k < n.numNeighbors; k++)
	//			openCells << n.neighborID[k];
	//	}
	//}

	UINT32						Data::TraceSkySensorLayer(UINT32 highCellCenter, UINT32 startingLayer, int penetration) const
	{
		int penetrationLeft = penetration << 1;	//account for water effect

		const CellStack&stack = cellStack[highCellCenter];
		UINT32 limit = M::Min( startingLayer, CellStack::Range-1);

		#ifdef HAS_WINDOW
			if (stack.waterRange.IsNotEmpty())
			{
				limit = (UINT32)(stack.waterRange.end-1);
				DBG_ASSERT_LESS2__(limit,CellStack::Range, stack.waterRange.start, stack.waterRange.end);
				if (stack.highestOpaqueCell >= stack.lowestOpenCell)
				{
					limit = std::max(limit,(UINT32)stack.highestOpaqueCell);
					DBG_ASSERT_LESS2__(limit,CellStack::Range, stack.lowestOpenCell, stack.highestOpaqueCell);
				}
			}
			else
			{
				if (stack.highestOpaqueCell >= stack.lowestOpenCell)
					limit = stack.highestOpaqueCell;
				DBG_ASSERT_LESS2__(limit,CellStack::Range, stack.lowestOpenCell, stack.highestOpaqueCell);
			}
			limit = M::Min( limit, CellStack::Range-1);
		#endif

		UINT32 layer = limit;
		//stack.highestOpaqueCell




		for (; layer < CellStack::Range; layer--)
		{
			const CellStack::Cell&cell = stack.volumeCell[layer];

			if (cell.content == Content::Clear)
				continue;
			if (cell.content == Content::Water)
				penetrationLeft--;
			else
				penetrationLeft-=2;
			if (penetrationLeft < 0)
				break;
		}
		if (layer > CellStack::Range)
			layer = 0;
		return layer;
	}

	//int						Volume::TraceSkySensorHeight(UINT32 highCellCenter, int penetration) const
	//{
	//	return CellStack::ToHeight(TraceSkySensorLayer(highCellCenter,0xFFFFFFFF,penetration) );
	//}


	//bool					Volume::UpdateSensorState(SkySensorState&state) const
	//{
	//	bool anyChanged = false;
	//	for (auto&c : state.coverage)
	//	{
	//		int h = TraceSkySensor(c.hStackID,state.groundPenetration);
	//		if (h == c.downToHeight)
	//			continue;
	//		c.downToHeight = h;
	//		state.coverageMap.Set(c.hStackID,h);
	//		anyChanged = true;
	//	}
	//	return anyChanged;
	//}



	bool					Data::LowCellHasStructure(const LCellIdI&cell)const
	{
		if (cell.height >= CellStack::HalfRange || cell.height < -CellStack::HalfRange)
			return false;
		#ifdef HYBRID_GRID
			index_t finer = Grid::low.grid.GetCell(cell.stackID).higherEquivalent;
			const auto&c = Grid::high.grid.GetCell(finer);
			return cellStack[finer].GetCellI(cell.height).structureCounter > 0;
		#else
			return cellStack[cell.stackID].GetCellI(cell.height).structureCounter > 0;

		#endif
	}

	BYTE					Data::GetMaxStructureCount() const
	{
		BYTE rs = 0;
		foreach(cellStack, stack)
		{
			for (UINT layer = 0; layer < CellStack::Range; layer++)
			{
				rs = M::Max(rs, stack->volumeCell[layer].structureCounter);
			}
		}
		return rs;
	}

	BYTE					Data::GetMaxStructureCount(const Grid::LCellIdI&cell) const
	{
		if (cell.height > CellStack::TopHeight)
			return 0;
		if (cell.height < CellStack::BottomHeight)
			return 0;
	#ifdef HYBRID_GRID
		index_t finer = Grid::low.grid.GetCell(cell.stackID).higherEquivalent;
		const auto&c = Grid::high.grid.GetCell(finer);
		BYTE rs = cellStack[finer].GetCellI(cell.height).structureCounter;
		for (int j = 0; j < c.neighborhood.numNeighbors; j++)
			rs = M::Max(rs, cellStack[c.neighborhood.neighborID[j]].GetCellI(cell.height).structureCounter);
		return rs;
	#else
		return cellStack[cell.stackID].GetCellI(cell.height).structureCounter;

	#endif

	}

	bool					Data::LowCellIsClear(const LCellIdI&cell)	const
	{
		if (cell.height > CellStack::TopHeight)
			return true;
		if (cell.height < CellStack::BottomHeight)
			return false;
		#ifdef HYBRID_GRID
			index_t finer = Grid::low.grid.GetCell(cell.stackID).higherEquivalent;
			const auto&c = Grid::high.grid.GetCell(finer);
			if (cellStack[finer].GetCellI(cell.height).IsOccupied())
				return false;
			for (int j = 0; j < c.neighborhood.numNeighbors; j++)
				if (cellStack[c.neighborhood.neighborID[j]].GetCellI(cell.height).IsOccupied())
					return false;
			return true;
		#else
			return !cellStack[cell.stackID].GetCellI(cell.height).IsOccupied();

		#endif
	}

	String					Data::LowCellReport(const LCellIdI&cell)	const
	{
		if (cell.height > CellStack::TopHeight)
			return "Clear: Over max height";
		if (cell.height < CellStack::BottomHeight)
			return "Not clear: Below bottom height";
		#ifdef HYBRID_GRID
			index_t finer = Grid::low.grid.GetCell(cell.stackID).higherEquivalent;
			const auto&c = Grid::high.grid.GetCell(finer);
			const auto&vcell = cellStack[finer].GetCellI(cell.height);
			if (vcell.content != Content::Clear)
				return "Not clear: Central stack content is '"+String(Content::Reinterpret(vcell.content).ToString())+"'";
			if (vcell.structureCounter > 1)
				return "Not clear: Central stack struct count is  "+String(vcell.structureCounter);
			for (int j = 0; j < c.neighborhood.numNeighbors; j++)
			{
				const auto&vcell = cellStack[c.neighborhood.neighborID[j]].GetCellI(cell.height);
				if (vcell.content != Content::Clear)
					return "Not clear: Neighbor "+String(j)+" stack content is '"+String(Content::Reinterpret(vcell.content).ToString())+"'";
				if (vcell.structureCounter > 1)
					return "Not clear: Neighbor "+String(j)+" stack struct count is  "+String(vcell.structureCounter);
			}
		#else
			const auto&vcell = cellStack[cell.stackID].GetCellI(cell.height);
			if (vcell.content != Content::Clear)
				return "Not clear: Central stack content is '"+String(Content::Reinterpret(vcell.content).ToString())+"'";
			if (vcell.structureCounter > 1)
				return "Not clear: Central stack struct count is  "+String(vcell.structureCounter);

		#endif
		return "Clear";
	}

	bool					Data::LowCellIsClearOrWater(const LCellIdI&cell)	const
	{
		if (cell.height >= CellStack::HalfRange)
			return true;
		if (cell.height < -CellStack::HalfRange)
			return false;
		#ifdef HYBRID_GRID
			index_t finer = Grid::low.grid.GetCell(cell.stackID).higherEquivalent;
			const auto&c = Grid::high.grid.GetCell(finer);
			if (!cellStack[finer].GetCellI(cell.height).IsClearOrWater())
				return false;
			for (int j = 0; j < c.neighborhood.numNeighbors; j++)
				if (!cellStack[c.neighborhood.neighborID[j]].GetCellI(cell.height).IsClearOrWater())
					return false;
			return true;
		#else
			return cellStack[cell.stackID].GetCellI(cell.height).IsClearOrWater();

		#endif
	}


	bool					Data::LowCellIsClearIgnoreStructure(const LCellIdI&cell)	const
	{
		if (cell.height >= CellStack::HalfRange)
			return true;
		if (cell.height < -CellStack::HalfRange)
			return false;
		#ifdef HYBRID_GRID
			index_t finer = Grid::low.grid.GetCell(cell.stackID).higherEquivalent;
			const auto&c = Grid::high.grid.GetCell(finer);
			if (cellStack[finer].GetCellI(cell.height).content != Content::Clear)
				return false;
			for (int j = 0; j < c.neighborhood.numNeighbors; j++)
				if (cellStack[c.neighborhood.neighborID[j]].GetCellI(cell.height).content != Content::Clear)
					return false;
			return true;
		#else
			return cellStack[cell.stackID].GetCellI(cell.height).content == Content::Clear;
		#endif
	}



	bool	Data::GetStrengthRange(const LCellIdI&cell, Content&outWeakest, Content&outStrongest, bool includeUnknown) const
	{
		#ifdef HYBRID_GRID
			index_t finer = Grid::low.grid.GetCell(cell.stackID).higherEquivalent;
			const auto&c = Grid::high.grid.GetCell(finer);
			outStrongest = outWeakest = Content::Reinterpret((cellStack[finer].GetCellI(cell.height).content));
			bool anyUnknown = false;
			int s0 = CellStack::Cell::typeStrength[outStrongest];
			M::TFloatRange<int> strength = M::FloatRange<int>(s0,s0);
			if (outStrongest == Content::Unknown)
			{
				anyUnknown = true;
				if (!includeUnknown)
					strength = M::MaxInvalidRange<int>();
			}
			for (int j = 0; j < c.neighborhood.numNeighbors; j++)
			{
				Content t = Content::Reinterpret(cellStack[c.neighborhood.neighborID[j]].GetCellI(cell.height).content);
				if (t == Content::Unknown)
				{
					anyUnknown = true;
					if (!includeUnknown)
						continue;
				}
				int strength2 = CellStack::Cell::typeStrength[t];
				if (strength2 < strength.min)
				{
					strength.min = strength2;
					outWeakest = t;
				}
				if (strength2 > strength.max)
				{
					strength.max = strength2;
					outStrongest = t;
				}
			}
			return anyUnknown;
		#else
			//const auto&c = Grid::high.grid.GetCell(cell.stackID);
			outStrongest = outWeakest = Content::Reinterpret((cellStack[cell.stackID].GetCellI(cell.height).content));
			return outWeakest == Content::Unknown;
		#endif
	}


	Data::ChangeSequence::ChangeSequence(ChangeSequence&&other):
			atTime(other.atTime),parent(other.parent),
			selectedStack(other.selectedStack),selectedStackChanged(other.selectedStackChanged),selectedStackIndex(other.selectedStackIndex)
	{
		other.parent = nullptr;
	}


	void				Data::ChangeSequence::SignalChange(UINT32 stackID)
	{
		changed.Set(Grid::high.grid.GetCell(stackID).family);
	}



	Data::ChangeLock::ChangeLock(const ChangeLock&other):parent(other.parent)
	{
		parent->BeginChangeLock();
	}
	Data::ChangeLock::~ChangeLock()
	{
		parent->EndChangeLock();
	}

	
	Data::ChangeLock::ChangeLock(Data*dat):parent(dat)
	{
		parent->BeginChangeLock();
	}


	void			Data::BeginChangeLock()
	{
		ASSERT__(!onChangeLock.isActive);
		onChangeLock.isActive ++;
	}

	void			Data::EndChangeLock()
	{
		ASSERT__(onChangeLock.isActive != 0);
		onChangeLock.isActive --;
		if (onChangeLock.isActive == 0)
		{
			SignalChange(onChangeLock.changed,onChangeLock.lastChangeTime,onChangeLock.terrainChanged);
			onChangeLock = TOnChangeLock();
		}
	}

	void			Data::SignalChange(const BaseGrid::FamiliesChanged&changed, float atTime, bool terrainChanged)
	{
		if (!changed.AnyChanged())
			return;
		if (onChangeLock.isActive)
		{
			onChangeLock.lock.lock();
			{
				onChangeLock.changed.Set(changed);
				onChangeLock.lastChangeTime = std::max(onChangeLock.lastChangeTime,atTime);
				if (terrainChanged)
					onChangeLock.terrainChanged = true;
			}
			onChangeLock.lock.unlock();
			return;
		}

		bool acceptChange = false;
		#ifdef CLIENT
			acceptChange = true;
			#ifdef HAS_WINDOW
				if (terrainChanged)
					lightingOutdated = true;
			#endif
		#else
			acceptChange = !IsGlobalInstance();
		#endif

		foreach (changed,c)
			families[*c].structureOrSensory.Set(atTime);


		if (terrainChanged)
		{
			if (acceptChange)
				InvalidateSensory();	//for the global world this is irrelevant and never checked by the server, but the client will issue sensory updates as a result.
			//the server, on the other hand, will apply this to client-worlds when onTerrainChange() is trigger by World::actual, resulting in the same effect for its clients.
			if (!terrainChangeCascade)
			{
				terrainChangeCascade = true;
				onTerrainChange(*this,changed,atTime);
				terrainChangeCascade = false;
			}
		}
	}

	Data::ChangeSequence::~ChangeSequence()
	{
		if (parent)
		{
			if (selectedStack && selectedStackChanged)
			{
				SignalChange(selectedStackIndex);
			}
			parent->SignalChange(changed,atTime,terrainChanged);
		}
	}




	bool	Data::Clear(const LCellIdI&ch, float atTime)
	{
		ChangeSequence seq = BeginChangeSequence(atTime);
		return seq.Clear(ch);
	}


	bool	Data::ChangeSequence::Clear(const Grid::LCellIdU&at)
	{
		#ifdef HYBRID_GRID
			index_t finer = Grid::low.grid.GetCell(at.stackID).higherEquivalent;
			const auto&c = Grid::high.grid.GetCell(finer);
		#else
			index_t finer = at.stackID;
		#endif

		bool rs = false;
		rs |= Clear(Grid::HCellIdU(finer,at.layer));
		#ifdef HYBRID_GRID
			for (int j = 0; j < c.neighborhood.numNeighbors; j++)
			{
				UINT32 n = c.neighborhood.neighborID[j];
				rs |= Clear(Grid::HCellIdU(n,at.layer));
			}
		#endif
		return rs;
	}

	bool	Data::ChangeSequence::Clear(const Grid::LCellIdI&at)
	{
		if (at.height < Grid::CellStack::BottomHeight)
			return false;
		return Clear(Grid::ToLayered(at));
	}

	bool	Data::ClearNonLava(const LCellIdI&ch, float atTime)
	{
		ChangeSequence seq = BeginChangeSequence(atTime);

		return seq.ClearNonLava(ch);
	}


	bool	Data::ChangeSequence::ClearNonLava(const HCellIdI&c)
	{
		if (c.height < Grid::CellStack::BottomHeight)
			return true;	//no error. just nothing there
		return ClearNonLava(Grid::ToLayered(c));
	}
	
	bool	Data::ChangeSequence::ClearNonLava(const Grid::HCellIdU&c)
	{
		if (c.layer >= Grid::CellStack::Range)
			return true; //no error. just nothing here
		return ChangeConditionally(c,Grid::Content::Clear,Condition::IsNotLava) || parent->cellStack[c.stackID].volumeCell[c.layer].content != Grid::Content::Lava;
	}


	bool	Data::ChangeSequence::ClearNonLava(const LCellIdI&ch)
	{
		#ifdef HYBRID_GRID
			index_t finer = Grid::low.grid.GetCell(ch.stackID).higherEquivalent;
			const auto&c = Grid::high.grid.GetCell(finer);
		#else
			index_t finer = ch.stackID;
		#endif

		bool rs = true;
		{
			if (!ClearNonLava(Grid::HCellIdI(finer,ch.height)))
				rs = false;
		}
		#ifdef HYBRID_GRID
			for (int j = 0; j < c.neighborhood.numNeighbors; j++)
			{
				UINT32 n = c.neighborhood.neighborID[j];
				if (!ClearNonLava(Grid::HCellIdI(n,ch.height)))
					rs = false;
			}
		#endif
		return rs;
	}


	bool	Data::Overwrite(const LCellIdI&ch, Content with,  float atTime)
	{
		ChangeSequence seq = BeginChangeSequence(atTime);
		#ifdef HYBRID_GRID
			index_t finer = Grid::low.grid.GetCell(ch.stackID).higherEquivalent;
			const auto&c = Grid::high.grid.GetCell(finer);
		#else
			index_t finer = ch.stackID;
		#endif
		seq.Change(Grid::HCellIdI(finer,ch.height),with);
		#ifdef HYBRID_GRID
			for (int j = 0; j < c.neighborhood.numNeighbors; j++)
			{
				UINT32 n = c.neighborhood.neighborID[j];
				seq.Change(Grid::HCellIdI(n,ch.height),with);
			}
		#endif
		return seq.AnyChanged();
	}

	#ifdef HAS_WINDOW



		float	Data::GetAverageLightLevel(const LCellIdI&id) const
		{
			#ifdef HYBRID_GRID
				const UINT32 high = Grid::low.grid.GetCell(id.stackID).higherEquivalent;
				const BaseGrid::TCellNeighborhood&n = Grid::high.grid.GetCell(high).neighborhood;
				float h = cellStack[high].GetCellI(id.height).GetLightLevel();
				for (int j = 0; j < n.numNeighbors; j++)
					h += cellStack[n.neighborID[j]].GetCellI(id.height).GetLightLevel();
				return h / (1 + n.numNeighbors);
			#else
				return cellStack[id.stackID].GetCellI(id.height).GetLightLevel();
			#endif
		}

		float	Data::GetHighestLightLevel(const LCellIdI&id) const
		{
			#ifdef HYBRID_GRID
				const UINT32 high = Grid::low.grid.GetCell(id.stackID).higherEquivalent;
				const BaseGrid::TCellNeighborhood&n = Grid::high.grid.GetCell(high).neighborhood;
				float h = cellStack[high].GetCellI(id.height).GetLightLevel();
				for (int j = 0; j < n.numNeighbors; j++)
					h = std::max(h,cellStack[n.neighborID[j]].GetCellI(id.height).GetLightLevel());
				return h;
			#else
				return GetAverageLightLevel(id);
			#endif
		}



	#endif


	bool	Data::IsRoad(const LCellIdI&cell, bool ignoreClearCellsOnEdge) const
	{
		#ifdef HYBRID_GRID
			const UINT32 lowEq = low.grid.GetCell(cell.stackID).higherEquivalent;

			if (!World::IsRoad(cellStack[lowEq].GetCellI(cell.height).content,false))
				return false;
			const auto&n = high.grid.GetCell(lowEq).neighborhood;
			for (int i = 0; i < n.numNeighbors; i++)
				if (!World::IsRoad(cellStack[n.neighborID[i]].GetCellI(cell.height).content,ignoreClearCellsOnEdge))
					return false;
			return true;
		#else
			return Grid::IsRoad(cellStack[cell.stackID].GetCellI(cell.height).content);
		#endif
	}



	void				Data::ChangeSequence::AlterVisibility(const Grid::HCellIdI&c,short changeBy)
	{
		DBG_ASSERT_GREATER_OR_EQUAL__(c.height , Grid::CellStack::BottomHeight);

		AlterVisibility(Grid::ToLayered(c),changeBy);
	}

	void				Data::ChangeSequence::AlterVisibility(const Grid::HCellIdU&c,short changeBy)
	{
		DBG_ASSERT_NOT_NULL__(parent);
		DetailChangeVisibility(parent->cellStack[c.stackID],c.layer,changeBy);
		SignalChange(c.stackID);
	}

	void				Data::ChangeSequence::AlterVisibility(int height,short changeBy)
	{
		DBG_ASSERT_GREATER_OR_EQUAL__(height , Grid::CellStack::BottomHeight);

		AlterVisibilityU(Grid::CellStack::ToLayer(height),changeBy);
	}

	void				Data::ChangeSequence::AlterVisibilityU(unsigned layer,short changeBy)
	{
		DBG_ASSERT_NOT_NULL__(selectedStack);
		DetailChangeVisibility(*selectedStack,layer,changeBy);
		SignalChange(selectedStackIndex);
	}

	/*static*/ void			Data::ChangeSequence::DetailChangeVisibility(Grid::CellStack&stack, unsigned uh, short changeBy)
	{
		stack.volumeCell[uh].AlterVisibility(changeBy);
	}




	bool				Data::ChangeSequence::Change(const Grid::HCellIdI&at, BYTE type)
	{
		if (at.height < Grid::CellStack::BottomHeight)
			return false;
		return Change(Grid::ToLayered(at),type);
	}
	bool				Data::ChangeSequence::Change(const Grid::HCellIdU&at, BYTE type)
	{
		const unsigned uh = at.layer;

		auto&stack = parent->cellStack[at.stackID];
		if (DetailChange(stack,uh,type))
		{
			SignalChange(at.stackID);
			return true;
		}
		return false;
	}

	bool				Data::ChangeSequence::ChangeConditionally(const Grid::HCellIdI&at, /*Grid::Content*/ BYTE type, const FCondition&cond)
	{
		if (at.height < Grid::CellStack::BottomHeight)
			return false;
		return ChangeConditionally(Grid::ToLayered(at),type,cond);
	}

	bool				Data::ChangeSequence::ChangeConditionally(const Grid::HCellIdU&at, /*Grid::Content*/ BYTE type, const FCondition&cond)
	{
		if (DetailChange(parent->cellStack[at.stackID],at.layer,type,cond))
		{
			SignalChange(at.stackID);
			return true;
		}
		return false;
	}



	bool		Data::ChangeSequence::DetailChange(CellStack&stack, unsigned uh, BYTE type)
	{
		if (uh >= Grid::CellStack::Range)
			return false;
		auto&cell = stack.volumeCell[uh];
		if (cell.content == type)
			return false;
		DetailOverwrite(stack,cell, uh,type);
		return true;
	}

	void		Data::ChangeSequence::DetailOverwrite(CellStack&stack, CellStack::Cell&cell, unsigned uh, BYTE newType)
	{
		bool wasSolid = cell.IsOpaque();
		cell.content = newType;
		terrainChanged = true;
			
		#ifdef HAS_WINDOW
			if (!wasSolid && cell.IsOpaque())
			{
				if (stack.lowestOpenCell == (BYTE)uh)
					while (stack.volumeCell[stack.lowestOpenCell].IsOpaque() && stack.lowestOpenCell < (BYTE)CellStack::Range)
						stack.lowestOpenCell++;

				stack.highestOpaqueCell = std::max(stack.highestOpaqueCell, (BYTE)uh);
			}
			else
				if (wasSolid && !cell.IsOpaque())
				{
					stack.lowestOpenCell = std::min(stack.lowestOpenCell, (BYTE)uh);
					if (stack.highestOpaqueCell == (BYTE)uh)
					{
						while (!stack.volumeCell[stack.highestOpaqueCell].IsOpaque() && stack.highestOpaqueCell > 0)
							stack.highestOpaqueCell--;
					}
				}
			stack.DebugCheckRange();
		#endif
	}




	bool		Data::ChangeSequence::DetailChange(CellStack&stack, unsigned uh, BYTE type, const FCondition&cond)
	{
		if (uh >= Grid::CellStack::Range)
			return false;
		auto&cell = stack.volumeCell[uh];
		if (cell.content == type || !cond(cell))
			return false;
		DetailOverwrite(stack,cell, uh,type);
		return true;
	}


	bool				Data::ChangeSequence::ChangeConditionally(int height, /*Grid::Content*/ BYTE type, const FCondition&cond)
	{
		if (height < Grid::CellStack::BottomHeight)
			return false;
		return ChangeConditionallyU(Grid::CellStack::ToLayer(height),type,cond);
	}

	bool				Data::ChangeSequence::ChangeConditionallyU(unsigned layer, /*Grid::Content*/ BYTE type, const FCondition&cond)
	{
		DBG_ASSERT_NOT_NULL__(selectedStack);
		if (DetailChange(*selectedStack,layer,type,cond))
		{
			selectedStackChanged = true;
			return true;
		}
		return false;
	}


	bool				Data::ChangeSequence::ChangeU(unsigned layer, BYTE type)
	{
		DBG_ASSERT_NOT_NULL__(selectedStack);
		if (DetailChange(*selectedStack,layer,type))
		{
			selectedStackChanged = true;
			return true;
		}
		return false;
	}

	bool				Data::ChangeSequence::Change(int height, BYTE type)
	{
		if (height < Grid::CellStack::BottomHeight)
			return false;
		return ChangeU(Grid::CellStack::ToLayer(height),type);
	}

	bool				Data::ChangeSequence::Clear(const Grid::HCellIdI&c)
	{
		return Change(c,Grid::Content::Clear);
	}

	bool				Data::ChangeSequence::Clear(const Grid::HCellIdU&c)
	{
		return Change(c,Grid::Content::Clear);
	}

	bool				Data::ChangeSequence::ClearU(unsigned layer)
	{
		return ChangeU(layer,Grid::Content::Clear);
	}

	bool				Data::ChangeSequence::Clear(int height)
	{
		return Change(height,Grid::Content::Clear);
	}


	Data::StackAccess::~StackAccess()
	{
		#ifdef HAS_WINDOW
			if (stack)
				stack->UpdateRange();
		#endif
	}


	Data::StackAccess			Data::ChangeSequence::AccessStack(UINT32 stackID)
	{
		CellStack&rs = parent->cellStack[stackID];
		#ifdef HAS_WINDOW
			rs.lightingChangedTo = (BYTE)CellStack::Range;
		#endif
		SignalChange(stackID);
		terrainChanged = true; //can't know, assume worst
		return Data::StackAccess(&rs);
	}


	void				Data::ChangeSequence::SelectStack(UINT32 stackID)
	{
		if (selectedStack && selectedStackIndex == stackID)
			return;
		if (selectedStackChanged)
		{
			SignalChange(selectedStackIndex);
			selectedStackChanged = false;
		}

		selectedStack = &parent->cellStack[stackID];
		selectedStackIndex = stackID;
	}
	

	void	Data::ChangeSequence::ApplyFamilyDelta(BaseGrid::family_t family, const Ctr::Array<CellStack::TTypeDeltaBlock>&data0, bool initialization, bool actualWorld)
	{
		ASSERT_LESS__(family, BaseGrid::NumFamilies);
		const index_t offset = high.grid.GetFamilyOffset(family);
		const count_t range = high.grid.CountFamilyMembers(family);

		bool anyChanged = false;
		if (data0.IsNotEmpty())
		{
			index_t at = 0;
			foreach (data0,block)
			{
				for (index_t i = 0; i < block->repitition; i++)
				{
					index_t stackIndex = (at % range) + offset;
					index_t layer = at / range;


					CellStack&stack = parent->cellStack[stackIndex];

					#ifdef _DEBUG
						if (initialization)
						{
							ASSERT_EQUAL__(stack.volumeCell[layer].content, 0);
							ASSERT_EQUAL__(stack.volumeCell[layer].structureCounter, 0);
						}
					#endif
					if (stack.volumeCell[layer].ApplyDelta(block->value))
					{
						anyChanged = true;
						#ifdef HAS_WINDOW
							stack.lightingChangedTo = std::max<BYTE>(stack.lightingChangedTo,(BYTE)(layer+1));
						#endif
					}
				
					at++;
				}
			
			}
			ASSERT_EQUAL__(at, range* CellStack::Range);
		}
	
		if (anyChanged)
		{
			terrainChanged = true;
			changed.Set(family);
		}

		#ifdef CLIENT
			ASSERT__(actualWorld);	//client only knows one world
		#endif
		if (actualWorld)
		{
			#ifdef HAS_WINDOW
				Queue<index_t>	changeQueue;

				if (anyChanged)
				{
					for (index_t i = 0; i < range; i++)
						if (parent->cellStack[i+offset].lightingChangedTo > 0)
						{
							parent->cellStack[i+offset].UpdateRange();
							changeQueue << i+offset;
						}
				}
			#endif


			#ifdef HAS_WINDOW

				index_t current;
				while (changeQueue >> current)
				{
					BYTE h = parent->cellStack[current].lightingChangedTo;
					//ASSERT_LESS__(h,CellStack::Range);
					//if (h+1 >= cellStack[current].lowestOpenCell)
					if (!h)
						continue;
					h--;
					//smallest = std::min(smallest,h);
					//counter++;

		
					const auto&n = high.grid.GetCell(current).neighborhood;
					for (BYTE k = 0; k < n.numNeighbors; k++)
					{
						UINT32 nid = n.neighborID[k];
						if (parent->cellStack[nid].lightingChangedTo < h)
						{
							parent->cellStack[nid].lightingChangedTo = h;
							changeQueue << nid;
						}
					}
				}
			#endif
		}
	}

	

	void	Data::ChangeSequence::ApplyFamilyDelta(BaseGrid::family_t family, const Ctr::Array<CellStack::TTypeDelta>&data0, bool initialization)
	{
		const index_t offset = high.grid.GetFamilyOffset(family);
		const count_t range = high.grid.CountFamilyMembers(family);
		ASSERT_EQUAL__(data0.Count(), range * CellStack::Range);
		//ASSERT_EQUAL__(data1.Count(), range * CellStack::Range);

		#ifdef HAS_WINDOW
			Queue<index_t>	changed;
		#endif
		bool globalChanged = false;
		{
			const CellStack::TTypeDelta*delta = data0.pointer();
			for (index_t i = 0; i < range; i++)
			{
				CellStack&stack = parent->cellStack[i+offset];
				bool anyChanged = false;
				for (index_t j = 0; j < CellStack::Range; j++)
				{
					if (stack.volumeCell[j].ApplyDelta(*delta))
					{
						anyChanged = true;
						#ifdef HAS_WINDOW
							stack.lightingChangedTo = std::max<BYTE>(stack.lightingChangedTo,(BYTE)j+1);
						#endif
					}
					#ifdef _DEBUG
					else
						if (initialization)
							FATAL__("Failed to initialize");
					#endif
					delta ++;
				}
				if (anyChanged)
				{
					globalChanged = true;
					#ifdef HAS_WINDOW
						stack.UpdateRange();
						changed << i + offset;
					#endif
				}
			}
			ASSERT_CONCLUSION(data0,delta);
		}
		if (globalChanged)
		{
			terrainChanged = true;
			this->changed.Set(family);
		}


		#ifdef HAS_WINDOW
			index_t current;
			while (changed >> current)
			{
				BYTE h = parent->cellStack[current].lightingChangedTo;
				if (!h)
					continue;
				h--;
				//smallest = std::min(smallest,h);
				//counter++;

		
				const auto&n = high.grid.GetCell(current).neighborhood;
				for (BYTE k = 0; k < n.numNeighbors; k++)
				{
					UINT32 nid = n.neighborID[k];
					if (parent->cellStack[nid].lightingChangedTo < h)
					{
						parent->cellStack[nid].lightingChangedTo = h;
						changed << nid;
					}
				}
			}
		#endif
	}

	void				Data::DbgCheckWaterRanges() const
	{
		#if defined _DEBUG && defined HAS_WINDOW
			foreach (this->cellStack,s)
				s->DbgCheckWaterRange();
		#endif
	}

	bool				Data::IsGlobalInstance()	const
	{
		if (this == &actual)
			return true;
		#ifndef CLIENT
			if (this == &initial)
				return true;
		#endif
		return false;
	}


	count_t				Data::CountVisibleCells() const
	{
		count_t cnt = 0;
		for (index_t i = 0; i < cellStack.Count(); i++)
		{
			const auto&stack = cellStack[i];
			for (UINT32 k = 0;  k < Grid::CellStack::Range; k++)
			{
				const auto&cell = stack.volumeCell[k];
				if (cell.IsCurrentlyVisible())
					cnt++;
			}
		}
		return cnt;
	}

	void				Data::FlushAndValidateSensory(short initialValue/*=0*/)
	{
		ASSERT__(!PaintingConcurrently());
		bool rs= false;
		for (index_t i = 0; i < cellStack.Count(); i++)
		{
			auto&stack = cellStack[i];
			for (UINT32 k = 0;  k < Grid::CellStack::Range; k++)
			{
				auto&cell = stack.volumeCell[k];
				cell.visibility = initialValue;
			}
		}
		foreach (families,fam)
			fam->structureOrSensory.Set(0);
		this->sensoryValid = true;
	}

	void				Data::ChangeSensory(const Grid::HCellIdU&center, short changeBy, float t)
	{
		if (SensoryIsInvalid())
			return;

		unsigned bottom = center.layer > 0 ? center.layer - 1 : 0;
		unsigned top = std::min(Grid::CellStack::Range-1, center.layer + 1);


		ChangeSequence seq = BeginChangeSequence(t);


		const auto&c = high.grid.GetCell(center.stackID);
		{
			seq.SelectStack(center.stackID);
			for (unsigned h = bottom; h <= top; h++)
				seq.AlterVisibilityU(h,changeBy);
		}

		for (int k = 0; k < c.neighborhood.numNeighbors; k++)
		{
			seq.SelectStack(c.neighborhood.neighborID[k]);
			for (unsigned h = bottom; h <= top; h++)
				seq.AlterVisibilityU(h,changeBy);
		}
	}

	//void				Volume::ChangeSensory(const Grid::HighSetup::TVolumeDescriptor&desc, float t, short changeBy)
	//{
	//	bool familyChanged[BaseGrid::NumFamilies] = { false };
	//	Grid::TemplateHelper::ChangeSensory(desc, changeBy, cellStack.pointer(), familyChanged);
	//	for (index_t i = 0; i < BaseGrid::NumFamilies; i++)
	//		if (familyChanged[i])
	//		{
	//			families[i].changed = true;
	//			//families[i].lastChange
	//		}
	//}

	//void				VisibilityState::ChangeSensory(const LCellIdI&center, float radius, float t, short changeBy)
	//{
	//	bool familyChanged[BaseGrid::NumFamilies] = { false };
	//	Grid::TemplateHelper::ChangeSensory(Grid::ToWorld(center), radius, changeBy, cellStack.pointer(), familyChanged);
	//	for (index_t i = 0; i < BaseGrid::NumFamilies; i++)
	//		if (familyChanged[i])
	//			families[i].changed = true;
	//}
	/*void				Volume::ChangeSensory(const SkySensorState&state, float t, short changeBy)
	{
		bool familyChanged[BaseGrid::NumFamilies] = { false };
		Grid::TemplateHelper::ChangeSensorCoverage(state, changeBy, cellStack.pointer(), familyChanged);
		for (index_t i = 0; i < BaseGrid::NumFamilies; i++)
			if (familyChanged[i])
				families[i].changed = true;
	}*/

	void				Data::RegisterExplosionCrater(const Grid::HCellIdI&center, float radius, float t)
	{
		Sensor(center,0.5f,radius,Sensor::SolidSphere).ApplyTo(*this,0,t);	//causes temporary visibility
	}


	/*static*/ void		Sensor::StaticCreate()
	{
		const count_t numProcessors = Sys::QueryLogicalProcessorsCount();
		perProcessor.SetSize(numProcessors);
		foreach (perProcessor,v)
		{
			v->shadedSphereHelper.SetSize(Grid::HighSetup::GridType::CellCount,Grid::CellStack::Range);
			v->shadedSphereHelper.Fill(0);
		}

	}


	bool			Sensor::Contains(const Grid::LCellIdI&loc) const
	{
		switch (type)
		{
			case SolidSphere:
			case ShadedSphere:
				return M::Sphere<>(worldCenter,radius).Contains(Grid::ToWorld(loc));
			break;
			case UpperHalfSolidSphere:
				return M::Sphere<>(worldCenter, radius).Contains(Grid::ToWorld(loc)) && Grid::CellStack::ToHeight(gridCenter.layer) <= loc.height;
			break;
			case SolidCylinder:
			case ShadedCylinder:
			{
				const float base = Grid::ToRadius(Grid::CellStack::ToHeight(gridCenter.layer));
				const float hr2 = M::Sqr(radius / base);
				if (Vec::quadraticDistance(Grid::low.cells[loc.stackID].cornerCenter,Grid::high.cells[gridCenter.stackID].cornerCenter) > hr2)
					return false;
				return Grid::CellStack::ToLayer( loc.height ) <= gridCenter.layer;
			}
			break;
			default:
				FATAL__("Functionality missing for sensor type #"+String((int)type));
		}
		return false;
	}


	void			Sensor::GetMaxVolume(Grid::LowSetup::TVolumeDescriptor&desc, index_t processorIndex/*=0*/) const
	{
		switch (type)
		{
			case SolidSphere:
			case ShadedSphere:
				Grid::low.GenerateVolume(M::Sphere<>(worldCenter,radius+1.f),perProcessor[processorIndex].lowOp,desc);
			break;
			case SolidCylinder:
			case ShadedCylinder:
				MakeShaftVolume(desc,[](UINT32){return 0;},processorIndex);
			break;
			case UpperHalfSolidSphere:
				Grid::low.GenerateVolume(M::Sphere<>(worldCenter, radius + 1.f), perProcessor[processorIndex].lowOp,desc);
				foreach(desc.coverage, st)
					st->range.start = gridCenter.layer;
			break;
			default:
				FATAL__("Functionality missing for sensor type #"+String((int)type));
		}

	}


	bool		Data::FlushAllChanges()
	{
		bool rs = false;
		foreach (families,fam)
			rs |= fam->Reset();
		return rs;
	}

	void Data::FlushTerrain()
	{
		for (index_t i = 0; i < cellStack.Count(); i++)
		{
			cellStack[i].ZeroInit(false);
		}

		foreach (families,f)
			f->structureOrSensory.Set(0);
		InvalidateSensory();
	}



	
	template <typename T0, typename T1>
		static void Compress(const Ctr::Array<T0>&dts, Ctr::ArrayData<T1>&dest)
		{
			static Ctr::Buffer<T1>	blocks;
			blocks.Clear();
			foreach(dts,dt)
			{
				if (blocks.IsEmpty() || blocks.Last().value != *dt)
				{
					auto&b = blocks.Append();
					b.value = *dt;
					b.repitition = 1;
				}
				else
					blocks.Last().repitition++;
			}
			blocks.CopyToArray(dest);
		}
	template <typename T0, typename T1>
		static void Compress(const Ctr::Array<T0>&dts, Ctr::ArrayData<T1>&dest, bool assertNotNull)
		{
			static Ctr::Buffer<T1>	blocks;
			blocks.Clear();
			foreach(dts,dt)
			{
				#ifndef CLIENT
					if (assertNotNull)
						ASSERT__(dt->IsSet());
				#endif
				if (blocks.IsEmpty() || blocks.Last().value != *dt)
				{
					auto&b = blocks.Append();
					b.value = *dt;
					b.repitition = 1;
				}
				else
					blocks.Last().repitition++;
			}
			blocks.CopyToArray(dest);
		}


	void			Data::GenerateDelta(BaseGrid::family_t familyID, WorldDeltaType::delta_type_t deltaType, Ctr::ArrayData<Grid::CellStack::TTypeDeltaBlock>&delta)
	{
		const index_t offset = Grid::high.grid.GetFamilyOffset(familyID);
		const count_t length = Grid::high.grid.CountFamilyMembers(familyID);

		const bool writeOutput = deltaType != WorldDeltaType::ProgressiveUpdateNoDelta;


		const Grid::CellStack::Cell empty(Grid::Content::Reinterpret(0),0);
		Ctr::Array<Grid::CellStack::TTypeDelta> tdts(writeOutput ? length * Grid::CellStack::Range : 0);
		//Ctr::Array<Grid::CellStack::TTechniteFactionDelta> fdts(length * Grid::CellStack::Range);
		//Ctr::Array<bool> fdtsValid(length * Grid::CellStack::Range);

		bool any = deltaType != WorldDeltaType::ProgressiveUpdate;
		Concurrency::parallel_for(index_t(0),length*Grid::CellStack::Range,
		[&tdts,/*&fdts,&fdtsValid,*/length,&any,
			offset,empty,deltaType,this](index_t i)
		{
			//index_t stackId = offset + (i / Grid::CellStack::Range);
			//index_t layer = i % Grid::CellStack::Range;
			index_t stackID = offset + (i % length);
			index_t layer = i / length;
			switch (deltaType)
			{
				#ifndef CLIENT
					case WorldDeltaType::ProgressiveUpdate:
					{
						bool canSee = true;
//						if (vs)
						{
							auto&cell =  cellStack[stackID].volumeCell[layer];
							canSee = cell.visibility > 0 || cell.temporarilyVisible;
							cell.temporarilyVisible = false;
						}
						if (canSee)
						{
							World::actual.cellStack[stackID].volumeCell[layer].
							GetDeltaFromAndUpdate(cellStack[stackID].volumeCell[layer],tdts[i]/*,fdts[i],fdtsValid[i]*/);
							if (tdts[i].IsSet())
								any = true;
						}
						else
						{
							tdts[i].newType = 0;
						}
					}
					break;
					case WorldDeltaType::ProgressiveUpdateNoDelta:
					{
						auto&cell =  cellStack[stackID].volumeCell[layer];
						bool canSee = cell.visibility > 0 || cell.temporarilyVisible;
						if (canSee)
						{
							const auto&from = World::actual.cellStack[stackID].volumeCell[layer];
							if (from.content != cell.content)
								any = true;
							cell.content = from.content;
							cell.temporarilyVisible = false;
						}
					}
					break;
					case WorldDeltaType::ProgressiveInit:
					{
						bool canSee = true;
						//if (vs)
						{
							auto&cell =  cellStack[stackID].volumeCell[layer];
							canSee = cell.visibility > 0 || cell.temporarilyVisible;
							cell.temporarilyVisible = false;
						}
						const Grid::CellStack::Cell& source = canSee ? World::actual.cellStack[stackID].volumeCell[layer] : World::initial.cellStack[stackID].volumeCell[layer];
						source.GetDeltaFrom(empty,tdts[i]/*,fdts[i],fdtsValid[i]*/);
						cellStack[stackID].volumeCell[layer] = source;
					}
					break;
					case WorldDeltaType::GlobalInitialCopy:
					{
						const Grid::CellStack::Cell& source = World::initial.cellStack[stackID].volumeCell[layer];
						source.GetDeltaFrom(empty,tdts[i]/*,fdts[i],fdtsValid[i]*/);
					}
					break;
				#endif
				case WorldDeltaType::LocalCopy:
				{
					const Grid::CellStack::Cell& source = cellStack[stackID].volumeCell[layer];
					source.GetDeltaFrom(empty,tdts[i]/*,fdts[i],fdtsValid[i]*/);
				}
				break;
				case WorldDeltaType::GlobalCopy:
				{
					const Grid::CellStack::Cell& source = World::actual.cellStack[stackID].volumeCell[layer];
					source.GetDeltaFrom(empty,tdts[i]/*,fdts[i],fdtsValid[i]*/);
				}
				break;
			}
		});

		if (writeOutput)
		{
			if (any)
				Compress(tdts,delta, /*deltaType == FactionCopy || */deltaType == WorldDeltaType::GlobalCopy/* || deltaType == GlobalInitialCopy*/);
			else
			{
				delta.SetSize(1);
				delta.First().repitition = (UINT32)tdts.Count();
				delta.First().value.newType = 0;
			}
		}
	}

	void		Data::CompressFamilyState(BaseGrid::family_t f,  Ctr::ArrayData<Grid::CellStack::TTypeDeltaBlock>&delta)
	{
		ASSERT_LESS__(f,BaseGrid::NumFamilies);

		GenerateDelta(f,WorldDeltaType::LocalCopy,delta);
	}


	bool		Data::ProgressiveUpdate(BaseGrid::family_t f,  Ctr::ArrayData<Grid::CellStack::TTypeDeltaBlock>&delta, float&t, bool noDelta/*=false*/)
	{
		ASSERT__(!IsGlobalInstance());
		ASSERT__(!SensoryIsInvalid());
		ASSERT_LESS__(f,BaseGrid::NumFamilies);
		float t0 = 0;
		if (!families[f].structureOrSensory.Reset(t0))
			return false;	//no changes in structure => no delta

		GenerateDelta(f,noDelta ?  WorldDeltaType::ProgressiveUpdateNoDelta : WorldDeltaType::ProgressiveUpdate,delta);
		if (!noDelta && (delta.Count() > 1 || delta.First().value.IsSet()))
		{
			t = M::Max(t,t0);
			return true;
		}
		return false;
	}

/*
	#ifdef CLIENT
	
		void				Data::InsertSensorCoverage(const SkySensorState&state)
		{
			ChangeSensorCoverage(state,1);
		}
		void				Data::RemoveSensorCoverage(const SkySensorState&state)
		{
			ChangeSensorCoverage(state,-1);
		}


	#endif*/


	void					Data::PaintConcurrently(BasicBuffer<Sensor>&sensors)
	{
		ASSERT__(!PaintingConcurrently());

		concurrentPainting.sensors.swap(sensors);
		concurrentPainting.owner = this;
		concurrentPainting.Start();
	}

	/*virtual override*/ void					Data::ConcurrentPainting::ExecuteRange(index_t begin, index_t end, index_t processorIndex)
	{
		for (index_t i = begin; i < end; i++)
			sensors[i].ApplyTo(*owner,1,0,processorIndex,true);
	}


	/*virtual override*/ void					Data::ConcurrentPainting::ThreadMain()
	{
		bool rs= false;
		for (index_t i = 0; i < owner->cellStack.Count(); i++)
		{
			auto&stack = owner->cellStack[i];
			for (UINT32 k = 0;  k < Grid::CellStack::Range; k++)
			{
				auto&cell = stack.volumeCell[k];
				cell.visibility = 0;
			}
		}
		foreach (owner->families,fam)
			fam->structureOrSensory.Set(0);
		
		workers.Execute(this,sensors.Count(),4);
		
		owner->sensoryValid = true;
	}



	
	/**/					Data::Data()
	{
		#ifdef CLIENT
			cellStack.SetSize(HighSetup::GridType::CellCount);
			FlushTerrain();
		#else
			if (IsGlobalInstance())
			{
				cellStack.SetSize(HighSetup::GridType::CellCount);
				FlushTerrain();
			}
			else
			{
				cellStack = initial.cellStack;
				//for (BaseGrid::family_t fam = 0; fam < BaseGrid::NumFamilies; fam++)
				//	families[fam].structureOrSensory.Set(0);	maybe don't declare this a change

				observerKey = actual.onTerrainChange.Register([this](const Data&,const BaseGrid::FamiliesChanged&fam, float t)
				{
					SignalChange(fam,t,true);
				});
			}
		#endif
	}


	/**/					Data::Data(const Data&other)
	{
		cellStack = other.cellStack;
		ASSERT_EQUAL__(cellStack.Count(),HighSetup::GridType::CellCount);
		if (!IsGlobalInstance())
		{
			observerKey = actual.onTerrainChange.Register([this](const Data&, const BaseGrid::FamiliesChanged&fam, float t)
			{
				SignalChange(fam, t, true);
			});
		}
	}

	void				Data::operator=(const Data&other)
	{
		cellStack = other.cellStack;
	}

	template <typename Setup>
		static void MakeShaftVolumeT(const Setup&setup, typename Setup::TVolumeDescriptor&v, typename Setup::TVolumeDescriptor::Operator&op, UINT32 stackCenter, float radius, UINT32 topLayer, const std::function<UINT32(UINT32 stackID)>&traceFunction)
		{
			v.Clear();

			op.stacksLeft << stackCenter;

			//const float r = worldCenter.GetLength();

			const float base = Grid::ToRadius(Grid::CellStack::ToHeight(topLayer));
			const float hr2 = M::Sqr(radius/base);
			const M::TVec3<>& center = setup.cells[stackCenter].centerVertex.xyz;
			UINT32 current;
			while (op.stacksLeft >> current)
			{
				if (op.stackVisited[current])
					continue;

				float d2 = Vec::quadraticDistance(setup.cells[current].centerVertex.xyz,center);
				if (d2 > hr2)
					continue;
				op.stackVisited[current] = true;

				auto&cell = v.coverage.Append();
				cell.stackID = current;
				cell.range.start = traceFunction(current);
				cell.range.end = M::Min(Grid::CellStack::Range, topLayer+1);
				const auto&n = setup.grid.GetCell(current).neighborhood;
				for (int k = 0; k < n.numNeighbors; k++)
					op.stacksLeft << n.neighborID[k];
			}

			op.ResetVisited(v);

		}

	void			Sensor::MakeShaftVolume(Grid::HighSetup::TVolumeDescriptor&v, const std::function<UINT32(UINT32 stackID)>&traceFunction, index_t processorIndex)	const
	{
		MakeShaftVolumeT(Grid::high, v,perProcessor[processorIndex].op,gridCenter.stackID, radius,gridCenter.layer,traceFunction);
	}


	void			Sensor::MakeShaftVolume(Grid::LowSetup::TVolumeDescriptor&v, const std::function<UINT32(UINT32 stackID)>&traceFunction, index_t processorIndex)	const
	{
		MakeShaftVolumeT(Grid::low, v,perProcessor[processorIndex].lowOp, Grid::low.Locate(worldCenter), radius,gridCenter.layer,traceFunction);
	}

	void			Sensor::ApplyShaftTo(World::Data&v, short change, const std::function<UINT32(UINT32 stackID)>&traceFunction,Data::ChangeSequence&seq, index_t processorIndex)	const
	{
		MakeShaftVolume(perProcessor[processorIndex].volume,traceFunction,processorIndex);
		ApplyVolume(v,change,seq, processorIndex);
	}


	/*static*/ Array<Sensor::TPerProcessor>	Sensor::perProcessor;

	void		Sensor::ApplySolidSphereTo(World::Data&v, short changeBy,Data::ChangeSequence&seq, index_t processorIndex)	const
	{
		Grid::high.GenerateVolume(M::Sphere<>(worldCenter, radius),perProcessor[processorIndex].op, perProcessor[processorIndex].volume);

		ApplyVolume(v, changeBy,seq,processorIndex);
	}


	void		Sensor::ApplyHalfSolidSphereTo(World::Data&v, short changeBy, Data::ChangeSequence&seq, index_t processorIndex)	const
	{
		if (gridCenter.layer < Grid::CellStack::Range)
		{
			Grid::high.GenerateVolume(M::Sphere<>(worldCenter, radius), perProcessor[processorIndex].op, perProcessor[processorIndex].volume);
			auto&cov = perProcessor[processorIndex].volume.coverage;
			foreach(cov, c)
			{
				c->range.start = M::Max(c->range.start, gridCenter.layer);
				c->range.end = M::Max(c->range.end, gridCenter.layer);
			}
			ApplyVolume(v, changeBy, seq, processorIndex);
		}
	}


	/*static*/ void	Sensor::ApplyVolume(World::Data&v, short changeBy,Data::ChangeSequence&seq, index_t processorIndex)
	{

		//#ifdef _DEBUG
		//{
		//	UINT32 crc = CRC32::Sequence().Append(volume.coverage.pointer(),volume.coverage.GetContentSize()).Finish();
		//	LogEvent(EventSignificance::Debug,"Applying sensory change ("+String(changeBy)+"), using descriptor with "+String(volume.coverage.Count())+" stack(s), crc="+IntToHex((int)crc,8));	
		//}

		//#endif


		auto&cov = perProcessor[processorIndex].volume.coverage;
		foreach (cov,c)
		{
			#ifdef _DEBUG
				ASSERT_LESS__(c->stackID,Grid::HighSetup::GridType::CellCount);
				ASSERT_LESS_OR_EQUAL__(c->range.start,c->range.end);
				ASSERT_LESS_OR_EQUAL__(c->range.end,Grid::CellStack::Range);
			#endif
			if (c->range.IsEmpty())
				continue;
			seq.SelectStack(c->stackID);
			for (UINT32 layer = c->range.start; layer < c->range.end; layer++)
			{
				seq.AlterVisibilityU(layer,changeBy);
			}
		}
	}


	float Q_rsqrt( float number )
	{
		INT32 i;
		float x2, y;
		const float threehalfs = 1.5F;

		x2 = number * 0.5F;
		y  = number;
		i  = * ( INT32 * ) &y;                       // evil floating point bit level hacking
		i  = 0x5f3759df - ( i >> 1 );               // what the fuck? 
		y  = * ( float * ) &i;
		y  = y * ( threehalfs - ( x2 * y * y ) );   // 1st iteration
	//	y  = y * ( threehalfs - ( x2 * y * y ) );   // 2nd iteration, this can be removed

		return y;
	}


	void FastNormalize(M::TVec3<>&v)
	{
		float fc = Q_rsqrt(Vec::dot(v));
		Vec::mult(v,fc);
	}


	template <count_t Depth>
		struct Pow
		{
			static float Exec(float x)
			{
				float y = Pow<Depth-1>::Exec(x);
				return y * y;
			}
		};

	template <>
		struct Pow<1>
		{
			static float Exec(float x)
			{
				return x*x;
			}
		};




	void		Sensor::ApplyShadedSphereTo(World::Data&v, short change,Data::ChangeSequence&seq, index_t processorIndex)	const
	{
		enum
		{
			Visited = 0x1,
			Visible = 0x2,
			Translucent = 0x4
		};

		const Grid::HCellIdU center(Grid::high.Locate(worldCenter),M::Min(Grid::CellStack::Range-1, Grid::CellStack::ToLayer( Grid::HeightFromRadius(Vec::length( worldCenter )) )));

		//static Ctr::Array2D<BYTE> user(Grid::HighSetup::GridType::CellCount,Grid::CellStack::Range,0);

		//static Ctr::Buffer0<Grid::HCellIdU>	marked;
		auto&user = perProcessor[processorIndex].shadedSphereHelper;
		auto&marked = perProcessor[processorIndex].shadedSphereMarked;
		marked.Clear();

		{
			auto&cell = user.Get(center.stackID,center.layer);
			cell = Visited | Visible | Translucent;	//is visible
			marked << center;
		}
		Ctr::Queue<Grid::HCellIdU>	work;
		Grid::high.VisitNeighborsOf(center,[&work](const Grid::HCellIdU&c)
		{
			work << c;
		});
		Grid::HCellIdU current;
		const auto centerRadius = Grid::ToRadius(Grid::CellStack::ToHeight(center.layer));
		while (work >> current)
		{
			const auto&s = World::actual.GetStack(current.stackID).volumeCell[current.layer];
			auto&u = user.Get(current.stackID,current.layer);
			if (u&Visited)
				continue;
			//auto&c = v.cellStack[current.stackID].volumeCell[current.layer];
			const M::TVec3<>c3 = Grid::ToWorld(current);
			if (Vec::quadraticDistance(c3,worldCenter) > M::Sqr(radius))
				continue;

			float3 d03 = c3-worldCenter;
			const M::TVec3<>&up = Grid::high.grid.GetCell(current.stackID).center;
			d03 -= up * Vec::dot(d03,up);//bend around sphere
			d03 += up * (Grid::ToRadius(Grid::CellStack::ToHeight(current.layer)) - centerRadius);
		//		Grid::RawMeanLayerHeight * float((int)current.layer - (int)center.layer);
			//d03.Normalize0();
			FastNormalize(d03);
				
			u = Visited;
			marked << current;

			bool translucent = s.IsClearOrWaterIgnoreStructure();

			float visibleWeight=0, totalWeight=0;
			Grid::high.VisitNeighborsOf(current,[&v,&user,&visibleWeight,&totalWeight, &work,current,d03,c3,translucent](const Grid::HCellIdU&next)
			{
				//auto&n = v.cellStack[next.stackID].volumeCell[next.layer];
				auto&nu = user.Get(next.stackID,next.layer);
				M::TVec3<> n3;
				Grid::ToWorld(next,n3);
				M::TVec3<> d3;
				Vec::sub(n3,c3,d3);
				FastNormalize(d3);
				//const float3	d3 = (n3-c3).Normalized();
				const float dot = Vec::dot(d3,d03);
				if (dot > 0)
				{
					//going outwards
					work << next;
				}
				else
				{
					//going inwards
					if (translucent)
					{
						//float w = pow(fabs(dot),64);
						float w = Pow<6>::Exec(fabs(dot));
						totalWeight += w;
						if ((nu & Visible))
							visibleWeight += w;
					}
				}
			});
			if (translucent)
			{
				u |= Translucent;
				if (visibleWeight  > 0.5f * totalWeight)
				{
					u |= Visible;
				}
			}
		}

		foreach (marked,m)
		{
			const auto u = user.Get(m->stackID,m->layer);
//			auto&c = v.cellStack[m->stackID].volumeCell[m->layer];
			bool makeVisible = (u & Visible) != 0;

			if (!makeVisible)
			{
				Grid::high.VisitNeighborsOf(*m,[&v,&makeVisible,&user](const Grid::HCellIdU&next)
				{
					const auto nu = user.Get(next.stackID,next.layer);
					if (nu & Visible)
						makeVisible = true;
				});
			}

			if (makeVisible)
				seq.AlterVisibility(*m,change);
		}

		foreach (marked,m)
			user.Set(m->stackID,m->layer,0);
	}




	void		Sensor::ApplyTo(World::Data&v, short change, float t, index_t processorIndex, bool ignoreTargetSensorValidity/*=false*/)	const
	{
		if (v.SensoryIsInvalid() && !ignoreTargetSensorValidity)
			return;
		Data::ChangeSequence seq = v.BeginChangeSequence(t);
		const Timer::Time t0 = timer.Now();
		switch (type)
		{
			case UpperHalfSolidSphere:
				ApplyHalfSolidSphereTo(v, change, seq,processorIndex);
			break;
			case SolidSphere:
				ApplySolidSphereTo(v,change,seq,processorIndex);
			break;
			case ShadedSphere:
				ApplyShadedSphereTo(v,change,seq,processorIndex);
			break;
			case SolidCylinder:
				ApplyShaftTo(v,change,[](UINT32){return 0;},seq,processorIndex);
			break;
			case ShadedCylinder:
				ApplyShaftTo(v,change,
					[this](UINT32 stackID)
					{
						return actual.TraceSkySensorLayer(stackID,gridCenter.layer,this->terrainPenetration);
					}
					,seq,processorIndex
				);
			break;
			default:
				FATAL__("Unsupported sensor type");
		};

		//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,"Sensory change processed in "+String(timer.GetSecondsSince(t0)));
	}






	bool		Data::GridCanSeeAny(const HCellIdU&base, UINT height) const
	{
		ASSERT__(!this->SensoryIsInvalid());
		const auto&stack = cellStack[base.stackID];
		UINT end = std::min(base.layer + height,CellStack::Range);
		for (UINT layer = base.layer; layer < end; layer++)
			if (stack.volumeCell[layer].IsCurrentlyVisible())
				return true;
		return false;
	}

	bool		Data::GridCanSee(const LCellIdI&center, int height, bool broad) const
	{
		ASSERT__(!this->SensoryIsInvalid());
		UINT32 fine = low.grid.GetCell(center.stackID).higherEquivalent;
		UINT32 layer0 = CellStack::ToLayer(center.height);
		if (GridCanSeeAny(HCellIdU(fine,layer0),(UINT)height))
			return true;
		if (!broad)
			return false;
		const auto&n = high.grid.GetCell(fine).neighborhood;
		for (int i = 0; i < n.numNeighbors; i++)
			if (GridCanSeeAny(HCellIdU(n.neighborID[i],layer0),(UINT)height))
				return true;
		return false;
	}
	
/*
	void		Data::ChangeSensorCoverage(const M::Sphere<>&state, signed char change)
	{
		bool familyChanged[BaseGrid::NumFamilies] = {false};
		TemplateHelper::ChangeSensory(state.center,state.radius,change,cellStack.pointer(),familyChanged);
		for (index_t i = 0; i < BaseGrid::NumFamilies; i++)
			if (familyChanged[i])
				Grid::high.family[i].SetOutdated(0);
	}

	void		Data::ChangeSensorCoverage(const HighSetup::TVolumeDescriptor&state, signed char change)
	{
		bool familyChanged[BaseGrid::NumFamilies] = {false};
		TemplateHelper::ChangeSensory(state,change,cellStack.pointer(),familyChanged);
		for (index_t i = 0; i < BaseGrid::NumFamilies; i++)
			if (familyChanged[i])
				Grid::high.family[i].SetOutdated(0);
	}

	void		Data::ChangeSensorCoverage(const SkySensorState&state, signed char change)
	{
		bool familyChanged[BaseGrid::NumFamilies] = {false};
		TemplateHelper::ChangeSensorCoverage(state,change,cellStack.pointer(),familyChanged);
		for (index_t i = 0; i < BaseGrid::NumFamilies; i++)
			if (familyChanged[i])
				Grid::high.family[i].SetOutdated(0);
	}

	void		Data::ChangeSensory(const LCellIdI&center, float radius, signed char change)
	{
		bool familyChanged[BaseGrid::NumFamilies] = {false};
		TemplateHelper::ChangeSensory(Grid::ToWorld(center),radius,change,cellStack.pointer(),familyChanged);
		for (index_t i = 0; i < BaseGrid::NumFamilies; i++)
			if (familyChanged[i])
				Grid::high.family[i].SetOutdated(0);
		
	}*/


	
	bool		Data::HasSensor(const LCellIdI&cell, float radius) const
	{
		#ifdef DEBUG_SENSORY
			TSensor s;
			s.center = cell;
			s.radius = radius;
			return sensors.Contains(s);	
		#else
			return true;
		#endif
	}

	//void				Data::InsertSensor(const M::Sphere<>&desc)
	//{
	//	ChangeSensorCoverage(desc,1);
	//}

	//void				Data::RemoveSensor(const M::Sphere<>&desc)
	//{
	//	ChangeSensorCoverage(desc,-1);
	//}

	//void				Data::InsertSensor(const HighSetup::TVolumeDescriptor&desc)
	//{
	//	ChangeSensorCoverage(desc,1);
	//}

	//void				Data::RemoveSensor(const HighSetup::TVolumeDescriptor&desc)
	//{
	//	ChangeSensorCoverage(desc,-1);
	//}

}

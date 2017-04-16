#include <global_root.h>
#include "worldData.h"

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
		actual.FlushAllData(true);
		#ifndef CLIENT
			initial.FlushAllData(true);
		#endif


	}

	void	Create()
	{
		#ifndef CLIENT
			//GenerateWorldVolume(config,discovery);
			initial.FlushAllData(true);
			actual.FlushAllData(true);
		#else
			actual.FlushAllData(true);
		#endif


		#ifdef HAS_WINDOW
			actual.UpdateLighting();
		#endif

		GlobalEvents::terrainFlush << FlushAllWorldData;
		GlobalEvents::objectFlush << [](){
			actual.ResetStructureCounters(true);
		};
	}
	
	//void	FlushStackFamily(Array<CellStack>&cellStack,  BYTE family)
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
					changedFamilies->changed[Grid::high.grid.GetCell(i).family] = true;
			}
		}
		return rs;
	}


	#ifdef HAS_WINDOW
	void	Data::UpdateLighting()
	{
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
				Concurrency::parallel_for(index_t(0), HighSetup::GridType::CellCount,[this,h](index_t i)
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
	
	
	}

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
		#endif

	#endif


	void	Data::Explode(const HCellIdI&id,float radius, BaseGrid::FamiliesChanged&changed)
	{
		const float3 center = high.cells[id.stackID].centerVertex.xyz * ToRadiusf(0.5f + id.height);
		const float r2 = sqr(radius);
		Queue<UINT32> queue;
		
		#ifdef HAS_WINDOW
			Queue<UINT32> changed;
		#endif
		queue << id.stackID;

		Box<> b;
		b.SetCenter(center,radius);

		bool processed[HighSetup::GridType::CellCount] = {false};


		UINT32 current;
		while (queue >> current)
		{
			if (processed[current])
				continue;
			processed[current] = true;
			const TVec3<>&dir = high.cells[current].centerVertex.xyz;
			TVec3<> lower = dir * ToRadius(-CellStack::HalfRange);
			TVec3<> upper = dir * ToRadius(CellStack::HalfRange);
			float dummy = Vec::distance(upper,lower);
			if (b.Contains(lower) || b.Contains(upper) || _oDetectOpticalBoxIntersection(b,lower,(upper-lower)/dummy,dummy))
			{
				bool anyChanged = false;
				for (int h = -CellStack::HalfRange; h < CellStack::HalfRange; h++)
				{
					TVec3<> at = dir * ToRadius(h);
					if (Vec::quadraticDistance(center,at) < r2)
					{
						if (cellStack[current].MakeLocalHole(h,*this))
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
					changed.changed[high.grid.GetCell(current).family] = true;
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

	void	Data::Explode(const HCellIdI&id,float radius, float atTime)
	{
		BaseGrid::FamiliesChanged changed;
		Explode(id,radius,changed);
		foreach (changed,c)
		{
			SignalFamilyStructureChange(*c,atTime);
			structureChanged = true;
			#ifdef HAS_WINDOW
				lightingOutdated = true;
			#endif
		}

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
	//	const float hr2 = sqr(radius/r);
	//	const TVec3<>& center = high.cells[highCenter].centerVertex.xyz;
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
		UINT32 limit = vmin( startingLayer, CellStack::Range-1);

		#ifdef HAS_WINDOW
			if (stack.waterRange.IsNotEmpty())
			{
				limit = (UINT32)(stack.waterRange.end-1);
				if (stack.highestOpaqueCell >= stack.lowestOpenCell)
					limit = std::max(limit,(UINT32)stack.highestOpaqueCell);
			}
			else
				if (stack.highestOpaqueCell >= stack.lowestOpenCell)
					limit = stack.highestOpaqueCell;
		#endif
		ASSERT_LESS__(limit,CellStack::Range);

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
			TFloatRange<int> strength = FloatRange<int>(s0,s0);
			if (outStrongest == Content::Unknown)
			{
				anyUnknown = true;
				if (!includeUnknown)
					strength = MaxInvalidRange<int>();
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




	bool	Data::Clear(const LCellIdI&ch, float atTime)
	{
		#ifdef HYBRID_GRID
			index_t finer = Grid::low.grid.GetCell(ch.stackID).higherEquivalent;
			const auto&c = Grid::high.grid.GetCell(finer);
		#else
			index_t finer = ch.stackID;
		#endif
		bool rebuild[BaseGrid::NumFamilies] = { false };

		count_t blobCnt = 0;
		if (cellStack[finer].MakeLocalHole(ch.height,*this))
		{
			TagCluster(rebuild, finer);
			blobCnt++;
		}
		//rebuild[Grid::high.grid.GetCell(finer).family] = true;
		#ifdef HYBRID_GRID
			for (int j = 0; j < c.neighborhood.numNeighbors; j++)
			{
				UINT32 n = c.neighborhood.neighborID[j];
				if (cellStack[n].MakeLocalHole(ch.height,*this))
				{
					TagCluster(rebuild, n);
					blobCnt++;
				}

			}
		#endif
		if (blobCnt > 0)
			for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
				if (rebuild[k])
				{
				//Grid::high.BakeBlockInstruction(Grid::cellStack,k,0,0);
					SignalFamilyStructureChange(k,atTime);
				}
		return blobCnt > 0;
	}

	bool	Data::ClearNonLava(const LCellIdI&ch, float atTime)
	{
		#ifdef HYBRID_GRID
			index_t finer = Grid::low.grid.GetCell(ch.stackID).higherEquivalent;
			const auto&c = Grid::high.grid.GetCell(finer);
		#else
			index_t finer = ch.stackID;
		#endif
		bool rebuild[BaseGrid::NumFamilies] = { false };

		if (cellStack[finer].GetCellI(ch.height).content == Content::Lava)
			return false;
		bool any = false;
		if (cellStack[finer].MakeLocalHole(ch.height,*this))
		{
			TagCluster(rebuild, finer);
			any = true;
		}
		//rebuild[Grid::high.grid.GetCell(finer).family] = true;
		#ifdef HYBRID_GRID
			for (int j = 0; j < c.neighborhood.numNeighbors; j++)
			{
				UINT32 n = c.neighborhood.neighborID[j];
				if (cellStack[n].GetCellI(ch.height).content == Content::Lava)
					return false;
				if (cellStack[n].MakeLocalHole(ch.height,*this))
				{
					TagCluster(rebuild, n);
					any = true;
				}

			}
		#endif
		if (any)
			for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
				if (rebuild[k])
				{
					SignalFamilyStructureChange(k,atTime);
				}
		return true;
	}


	bool	Data::Overwrite(const LCellIdI&ch, Content with,  float atTime)
	{
		#ifdef HYBRID_GRID
			index_t finer = Grid::low.grid.GetCell(ch.stackID).higherEquivalent;
			const auto&c = Grid::high.grid.GetCell(finer);
		#else
			index_t finer = ch.stackID;
		#endif
		bool rebuild[BaseGrid::NumFamilies] = { false };

		count_t blobCnt = 0;
		if (cellStack[finer].LocalOverwrite(ch.height,with,*this))
		{
			TagCluster(rebuild, finer);
			blobCnt++;
		}
		//rebuild[Grid::high.grid.GetCell(finer).family] = true;
		#ifdef HYBRID_GRID
			for (int j = 0; j < c.neighborhood.numNeighbors; j++)
			{
				UINT32 n = c.neighborhood.neighborID[j];
				if (cellStack[n].LocalOverwrite(ch.height,with,*this))
				{
					TagCluster(rebuild, n);
					blobCnt++;
				}

			}
		#endif
		if (blobCnt > 0)
			for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
				if (rebuild[k])
				{
				//Grid::high.BakeBlockInstruction(Grid::cellStack,k,0,0);
					SignalFamilyStructureChange(k,atTime);
				}
		return blobCnt > 0;
	}

	#ifdef HAS_WINDOW

		void	Data::OverrideVisibility(short v)
		{
			foreach (cellStack,stack)
				for (UINT32 l = 0; l < Grid::CellStack::Range; l++)
					stack->volumeCell[l].visibility = v;
		}

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



	void	Data::ApplyFamilyDelta(BYTE family, const Array<CellStack::TTypeDeltaBlock>&data0, bool initialization, bool actualWorld)
	{
		ASSERT_LESS__(family, BaseGrid::NumFamilies);
		const index_t offset = high.grid.GetFamilyOffset(family);
		const count_t range = high.grid.CountFamilyMembers(family);

		#ifdef HAS_WINDOW
			Queue<index_t>	changed;
		#endif
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

					CellStack&stack = cellStack[stackIndex];

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
			InvalidateSensory();

		#ifdef CLIENT
			ASSERT__(actualWorld);	//client only knows one world
		#endif
		if (actualWorld)
		{
			#ifdef HAS_WINDOW
				if (anyChanged)
				{
					for (index_t i = 0; i < range; i++)
						if (cellStack[i+offset].lightingChangedTo > 0)
						{
							cellStack[i+offset].UpdateRange();
							changed << i+offset;
						}
				}
			#endif

			if (anyChanged)
			{
				SignalFamilyStructureChange(family);
			}

			#ifdef HAS_WINDOW

				index_t current;
				while (changed >> current)
				{
					BYTE h = cellStack[current].lightingChangedTo;
					//ASSERT_LESS__(h,CellStack::Range);
					//if (h+1 >= cellStack[current].lowestOpenCell)
					{
						SignalFamilyStructureChange(high.grid.GetCell(current).family);
						lightingOutdated = true;
						structureChanged = true;
					}
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
		}
		//ASSERT_CONCLUSION(data,delta);
	}

	

	void	Data::ApplyFamilyDelta(BYTE family, const Array<CellStack::TTypeDelta>&data0, bool initialization)
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
				CellStack&stack = cellStack[i+offset];
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
		SignalFamilyStructureChange(family);

		if (globalChanged)
			InvalidateSensory();


		#ifdef HAS_WINDOW
			index_t current;
			while (changed >> current)
			{
				BYTE h = cellStack[current].lightingChangedTo;
				//ASSERT_LESS__(h,CellStack::Range);
				//if (h+1 >= cellStack[current].lowestOpenCell)
				{
					SignalFamilyStructureChange(high.grid.GetCell(current).family);
					lightingOutdated = true;
					structureChanged = true;
				}
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
	}



	void				Data::FlushAndValidateSensory()
	{
		bool rs= false;
		for (index_t i = 0; i < cellStack.Count(); i++)
		{
			auto&stack = cellStack[i];
			for (UINT32 k = 0;  k < Grid::CellStack::Range; k++)
			{
				auto&cell = stack.volumeCell[k];
				cell.oldVisible = false;
				cell.visibility = 0;
				cell.temporarilyVisible = false;
			}
		}
		this->sensoryValid = true;
	}

	void				Data::ChangeSensory(const Grid::HCellIdU&center, float t, short changeBy)
	{
		if (SensoryIsInvalid())
			return;
		bool familyChanged[BaseGrid::NumFamilies] = { false };

		unsigned bottom = center.layer > 0 ? center.layer - 1 : 0;
		unsigned top = std::min(Grid::CellStack::Range-1, center.layer + 1);


		const auto&c = high.grid.GetCell(center.stackID);
		{
			auto&stack = cellStack[center.stackID];
			for (unsigned h = bottom; h <= top; h++)
				stack.volumeCell[h].AlterVisibility(changeBy);
		}

		familyChanged[c.family] = true;
		for (int k = 0; k < c.neighborhood.numNeighbors; k++)
		{
			familyChanged[high.grid.GetCell(c.neighborhood.neighborID[k]).family] = true;

			auto&stack = cellStack[c.neighborhood.neighborID[k]];
			for (unsigned h = bottom; h <= top; h++)
				stack.volumeCell[h].AlterVisibility(changeBy);
		}


		for (index_t i = 0; i < BaseGrid::NumFamilies; i++)
			if (familyChanged[i])
				SignalFamilyStructureChange(i,t);
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
		Sensor(center,0.5f,radius,Sensor::SolidSphere).ApplyTo(*this,0,t);
	}


	bool			Sensor::Contains(const Grid::LCellIdI&loc) const
	{
		switch (type)
		{
			case SolidSphere:
			case ShadedSphere:
				return Sphere<>(worldCenter,radius).Contains(Grid::ToWorld(loc));
			break;
			case SolidCylinder:
			case ShadedCylinder:
			{
				const float hr2 = sqr(radius/Grid::VerticalBase);
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


	void			Sensor::GetMaxVolume(Grid::LowSetup::TVolumeDescriptor&desc) const
	{
		switch (type)
		{
			case SolidSphere:
			case ShadedSphere:
				Grid::low.GenerateVolume(Sphere<>(worldCenter,radius+1.f),desc);
			break;
			case SolidCylinder:
			case ShadedCylinder:
				MakeShaftVolume(desc,[](UINT32){return 0;});
			break;
			default:
				FATAL__("Functionality missing for sensor type #"+String((int)type));
		}

	}



	void Data::FlushAllData(bool resetVisibility)
	{
		for (index_t i = 0; i < cellStack.Count(); i++)
		{
			cellStack[i].ZeroInit(resetVisibility);
		}

		for (index_t i = 0; i < ARRAYSIZE(families); i++)
			families[i].Reset();
		InvalidateSensory();
	}



	void		Data::CheckChanges(float t)
	{
		for (BYTE f = 0; f < BaseGrid::NumFamilies; f++)
		{
			if (families[f].changed)
			{
				families[f].changed = false;
				const index_t offset = Grid::high.grid.GetFamilyOffset(f);
				const count_t range = Grid::high.grid.CountFamilyMembers(f);
				bool anyChanged = false;
				Concurrency::parallel_for(offset, offset + range, [this, &anyChanged](index_t i)
				{
					for (unsigned j = 0; j < Grid::CellStack::Range; j++)
					{
						CellStack::Cell&cell1 = cellStack[i].volumeCell[j];
						if (cell1.Changed(true))
							anyChanged = true;
					}
				});
				if (anyChanged)
				{
					families[f].coverageChanged = true;
					families[f].lastChange = t;
				}
			}
		}

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


	
	/**/					Data::Data():cellStack(HighSetup::GridType::CellCount)
	{
		FlushAllData(true);
	}

	template <typename Setup>
		static void MakeShaftVolumeT(const Setup&setup, typename Setup::TVolumeDescriptor&v, typename Setup::TVolumeDescriptor::Operator&op, UINT32 stackCenter, float radius, UINT32 topLayer, const std::function<UINT32(UINT32 stackID)>&traceFunction)
		{
			v.Clear();

			op.stacksLeft << stackCenter;

			//const float r = worldCenter.Length();

			const float hr2 = sqr(radius/Grid::VerticalBase);
			const TVec3<>& center = setup.cells[stackCenter].centerVertex.xyz;
			UINT32 current;
			while (op.stacksLeft >> current)
			{
				if (op.stackVisited[current])
					continue;
				op.stackVisited[current] = true;

				float d2 = Vec::quadraticDistance(setup.cells[current].centerVertex.xyz,center);
				if (d2 > hr2)
					continue;

				auto&cell = v.coverage.Append();
				cell.stackID = current;
				cell.range.end = topLayer+1;
				cell.range.start = traceFunction(current);
				const auto&n = setup.grid.GetCell(current).neighborhood;
				for (int k = 0; k < n.numNeighbors; k++)
					op.stacksLeft << n.neighborID[k];
			}

			op.ResetVisited(v);

		}

	void			Sensor::MakeShaftVolume(Grid::HighSetup::TVolumeDescriptor&v, const std::function<UINT32(UINT32 stackID)>&traceFunction)	const
	{
		MakeShaftVolumeT(Grid::high, v,op,gridCenter.stackID, radius,gridCenter.layer,traceFunction);
	}


	void			Sensor::MakeShaftVolume(Grid::LowSetup::TVolumeDescriptor&v, const std::function<UINT32(UINT32 stackID)>&traceFunction)	const
	{
		MakeShaftVolumeT(Grid::low, v,lowOp, Grid::low.Locate(worldCenter), radius,gridCenter.layer,traceFunction);
	}

	void			Sensor::ApplyShaftTo(World::Data&v, short change, BaseGrid::FamiliesChanged&changed, const std::function<UINT32(UINT32 stackID)>&traceFunction)	const
	{
		MakeShaftVolume(volume,traceFunction);
		ApplyVolume(v,change,changed);
	}


	/*static*/ Grid::HighSetup::TVolumeDescriptor::Operator Sensor::op;
	/*static*/ Grid::LowSetup::TVolumeDescriptor::Operator Sensor::lowOp;
	/*static*/ Grid::HighSetup::TVolumeDescriptor	Sensor::volume;

	void		Sensor::ApplySolidSphereTo(World::Data&v, short changeBy, BaseGrid::FamiliesChanged&familyChanged)	const
	{
		volume.Clear();
		UINT32 centerStack = this->gridCenter.stackID;
		op.stacksLeft << centerStack;

		const float br = Grid::ToRadius(Grid::CellStack::BottomHeight);
		const float tr = Grid::ToRadius(Grid::CellStack::TopHeight);
		UINT32 next;
		while (op.stacksLeft >> next)
		{
			if (op.stackVisited[next])
				continue;
			op.stackVisited[next] = true;
			const TVec3<>&dir = Grid::high.cells[next].cornerCenter;
			TVec2<> distances;
			TVec3<> bottom,top;
			Vec::mult(dir,br,bottom);
			Vec::mult(dir,tr,top);
			Grid::HighSetup::TVolumeDescriptor::TStack&st = volume.coverage.Append();
			st.stackID = next;
			if (!Obj::DetectSphereEdgeIntersection(worldCenter,radius,bottom,top,distances))
			{
				st.range.SetEmpty(0);
				continue;
			}
			
			st.range.start = (UINT32)floor(std::max(distances.x,0.f) * (Grid::CellStack::Range-1));
			st.range.end = 1 + (UINT32)ceil(std::min(distances.y,1.f) * (Grid::CellStack::Range-1));
			const auto&n = Grid::high.grid.GetCell(next).neighborhood;
			for (int i = 0; i < n.numNeighbors; i++)
				op.stacksLeft << n.neighborID[i];
		}
		op.ResetVisited(volume);


		ApplyVolume(v, changeBy,familyChanged);
	}


	/*static*/ void	Sensor::ApplyVolume(World::Data&v, short changeBy, BaseGrid::FamiliesChanged&familyChanged)
	{

		#ifdef _DEBUG
		{
			UINT32 crc = CRC32::getChecksum(volume.coverage.pointer(),volume.coverage.GetContentSize());
			LogEvent(EventSignificance::Debug,"Applying sensory change ("+String(changeBy)+"), using descriptor with "+String(volume.coverage.Count())+" stack(s), crc="+IntToHex((int)crc,8));	
		}

		#endif


		foreach (volume.coverage,c)
		{
			#ifdef _DEBUG
				ASSERT_LESS__(c->stackID,Grid::HighSetup::GridType::CellCount);
				ASSERT_LESS_OR_EQUAL__(c->range.start,c->range.end);
				ASSERT_LESS__(c->range.start,Grid::CellStack::Range);
			#endif
			if (c->range.IsEmpty())
				continue;
			auto&stack = v.cellStack[c->stackID];
			familyChanged.changed[Grid::high.grid.GetCell(c->stackID).family] = true;
			for (UINT32 layer = c->range.start; layer < c->range.end; layer++)
			{
				stack.volumeCell[layer].AlterVisibility(changeBy);
			}
		}
	}


	void		Sensor::ApplyShadedSphereTo(World::Data&v, short change, BaseGrid::FamiliesChanged&familyChanged)	const
	{
		enum
		{
			Visited = 0x1,
			Visible = 0x2,
			Translucent = 0x4
		};

		const Grid::HCellIdU center(Grid::high.Locate(worldCenter),Grid::CellStack::ToLayer( Grid::HeightFromRadius(Vec::length( worldCenter )) ));
		Buffer0<Grid::HCellIdU>	marked;
		{
			auto&cell = v.cellStack[center.stackID].volumeCell[center.layer];
			cell.user = Visited | Visible | Translucent;	//is visible
			marked << center;
		}
		Queue<Grid::HCellIdU>	work;
		Grid::high.VisitNeighborsOf(center,[&work](const Grid::HCellIdU&c)
		{
			work << c;
		});
		Grid::HCellIdU current;
		while (work >> current)
		{
			auto&c = v.cellStack[current.stackID].volumeCell[current.layer];
			if (c.user&Visited)
				continue;
			const TVec3<>c3 = Grid::ToWorld(current);
			if (Vec::quadraticDistance(c3,worldCenter) > sqr(radius))
				continue;

			float3 d03 = c3-worldCenter;
			const TVec3<>&up = Grid::high.grid.GetCell(current.stackID).center;
			d03 -= up * Vec::dot(d03,up);//bend around sphere
			d03 += up * Grid::SingleLayerHeight* float((int)current.layer - (int)center.layer);
			d03.Normalize0();
				
			c.user = Visited;
			marked << current;

			bool translucent = v.cellStack[current.stackID].volumeCell[current.layer].IsClearOrWaterIgnoreStructure();

			float visibleWeight=0, totalWeight=0;
			Grid::high.VisitNeighborsOf(current,[&v,&visibleWeight,&totalWeight, &work,current,d03,c3,translucent](const Grid::HCellIdU&next)
			{
				auto&n = v.cellStack[next.stackID].volumeCell[next.layer];
				const TVec3<> n3 = Grid::ToWorld(next);
				const float3	d3 = (n3-c3).Normalized();
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
						float w = pow(fabs(dot),64);
						totalWeight += w;
						if ((n.user & Visible))
							visibleWeight += w;
					}
				}
			});
			if (translucent)
			{
				c.user |= Translucent;
				if (visibleWeight  > 0.5f * totalWeight)
				{
					c.user |= Visible;
				}
			}
		}

		foreach (marked,m)
		{
			auto&c = v.cellStack[m->stackID].volumeCell[m->layer];
			bool makeVisible = (c.user & Visible) != 0;

			if (!makeVisible)
			{
				Grid::high.VisitNeighborsOf(*m,[&v,&makeVisible](const Grid::HCellIdU&next)
				{
					const auto&n = v.cellStack[next.stackID].volumeCell[next.layer];
					if (n.user & Visible)
						makeVisible = true;
				});
			}

			if (makeVisible)
			{
				c.AlterVisibility(change);
				familyChanged.changed[Grid::high.grid.GetCell(m->stackID).family] = true;
			}
		}

		foreach (marked,m)
			v.cellStack[m->stackID].volumeCell[m->layer].user = 0;
	}




	void		Sensor::ApplyTo(World::Data&v, short change, float t)	const
	{
		if (v.SensoryIsInvalid())
			return;
		BaseGrid::FamiliesChanged familyChanged;

		const Timer::Time t0 = timer.Now();
		switch (type)
		{
			case SolidSphere:
				ApplySolidSphereTo(v,change,familyChanged);
			break;
			case ShadedSphere:
				ApplyShadedSphereTo(v,change,familyChanged);
			break;
			case SolidCylinder:
				ApplyShaftTo(v,change,familyChanged,[](UINT32){return 0;});
			break;
			case ShadedCylinder:
				ApplyShaftTo(v,change,familyChanged,
					[this,&v](UINT32 stackID)
					{
						return v.TraceSkySensorLayer(stackID,gridCenter.layer,this->terrainPenetration);
					}
				);
			break;
		};

		LogEvent(EventSignificance::PerformanceMeasurement,nullptr,"Sensory change processed in "+String(timer.GetSecondsSince(t0)));

		foreach (familyChanged,f)
		{
			v.SignalFamilyStructureChange(*f,t);
		}
	}






	bool		Data::GridCanSeeAny(const HCellIdU&base, UINT height) const
	{
		ASSERT__(this->SensoryIsInvalid());
		const auto&stack = cellStack[base.stackID];
		UINT end = std::min(base.layer + height,CellStack::Range);
		for (UINT layer = base.layer; layer < end; layer++)
			if (stack.volumeCell[layer].IsCurrentlyVisible())
				return true;
		return false;
	}

	bool		Data::GridCanSee(const LCellIdI&center, int height, bool broad) const
	{
		ASSERT__(this->SensoryIsInvalid());
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
	void		Data::ChangeSensorCoverage(const Sphere<>&state, signed char change)
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

	//void				Data::InsertSensor(const Sphere<>&desc)
	//{
	//	ChangeSensorCoverage(desc,1);
	//}

	//void				Data::RemoveSensor(const Sphere<>&desc)
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

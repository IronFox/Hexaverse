#include <global_root.h>
#include "worldData.h"




namespace World
{

	Data			actual;
	#ifndef CLIENT
		Data		initial;
	#endif


	bool IsRoad(BYTE /*CellStack::Cell::Content::type_t*/ t, bool clearIsRoad)
	{
		return (clearIsRoad && t == Content::Clear) || t == Content::Road || t == Content::Foundation;
	}

	void	Create()
	{
		#ifndef CLIENT
			//GenerateWorldVolume(config,discovery);
			initial.FlushAllData();
			actual.FlushAllData();
		#else
			actual.FlushAllData();
		#endif


		#ifdef HAS_WINDOW
			actual.UpdateLighting();
		#endif
	}
	
	void	FlushStackFamily(Array<CellStack>&cellStack,  BYTE family)
	{
		const index_t offset = high.grid.GetFamilyOffset(family);
		const count_t range = high.grid.CountFamilyMembers(family);
		for (index_t i = offset; i < offset+range; i++)
			cellStack[i].ZeroInit();		
	
	}
		
	void	ResetStructureCounters()
	{
		actual.ResetStructureCounters();
	}



	Data::Data()
	{
		familyOutdatedSince.Fill(MaxOutdated);
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
				SetFamilyOutdated(high.grid.GetCell(i).family);
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
						SetFamilyOutdated(high.grid.GetCell(i).family);
					}
			}
		#endif

	#endif


	void	Data::Explode(const HCellIdI&id,float radius, bool*familyChanged)
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
					familyChanged[high.grid.GetCell(current).family] = true;
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
		bool familyChanged[BaseGrid::NumFamilies] = {false};
		Explode(id,radius,familyChanged);
		for (index_t i = 0; i < BaseGrid::NumFamilies; i++)
			if (familyChanged[i])
			{
				SetFamilyOutdated(i,atTime);
				#ifdef HAS_WINDOW
					lightingOutdated = true;
					structureChanged = true;
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

	//UINT32						Volume::TraceSkySensorLayer(UINT32 highCellCenter, UINT32 startingLayer, int penetration) const
	//{
	//	int penetrationLeft = penetration << 1;	//account for water effect

	//	const CellStack&stack = cellStack[highCellCenter];
	//	UINT32 limit = vmin( startingLayer, CellStack::Range-1);

	//	#ifdef HAS_WINDOW
	//		if (stack.waterRange.IsNotEmpty())
	//		{
	//			limit = (UINT32)(stack.waterRange.end-1);
	//			if (stack.highestOpaqueCell >= stack.lowestOpenCell)
	//				limit = std::max(limit,(UINT32)stack.highestOpaqueCell);
	//		}
	//		else
	//			if (stack.highestOpaqueCell >= stack.lowestOpenCell)
	//				limit = stack.highestOpaqueCell;
	//	#endif
	//	ASSERT_LESS__(limit,CellStack::Range);

	//	UINT32 layer = limit;
	//	//stack.highestOpaqueCell




	//	for (; layer < CellStack::Range; layer--)
	//	{
	//		const CellStack::Cell&cell = stack.volumeCell[layer];

	//		if (cell.content == Content::Clear)
	//			continue;
	//		if (cell.content == Content::Water)
	//			penetrationLeft--;
	//		else
	//			penetrationLeft-=2;
	//		if (penetrationLeft < 0)
	//			break;
	//	}
	//	if (layer > CellStack::Range)
	//		layer = 0;
	//	return layer;
	//}

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
					SetFamilyOutdated(k,atTime);
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
					SetFamilyOutdated(k,atTime);
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
					SetFamilyOutdated(k,atTime);
				}
		return blobCnt > 0;
	}

	#ifdef HAS_WINDOW

		//void	Data::OverrideVisibility(short v)
		//{
		//	foreach (cellStack,stack)
		//	for (UINT32 l = 0; l < Grid::CellStack::Range; l++)
		//		stack->volumeCell[l].visible = v;
		//}

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
				SetFamilyOutdated(family);
			}

			#ifdef HAS_WINDOW

				index_t current;
				while (changed >> current)
				{
					BYTE h = cellStack[current].lightingChangedTo;
					//ASSERT_LESS__(h,CellStack::Range);
					//if (h+1 >= cellStack[current].lowestOpenCell)
					{
						SetFamilyOutdated(high.grid.GetCell(current).family);
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

}

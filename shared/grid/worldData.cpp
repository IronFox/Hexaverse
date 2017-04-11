#include <global_root.h>
#include "worldData.h"




namespace World
{

	Data			actual;
	#ifndef CLIENT
		Volume		initial;
	#endif
	
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
						if (cellStack[current].MakeLocalHole(h))
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

	

}

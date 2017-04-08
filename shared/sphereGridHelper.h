#ifndef sphereGridHelperH
#define sphereGridHelperH

#include "sphereGrid.h"

#ifdef _DEBUG
	#include <io/cryptographic_hash.h>

#endif



namespace Grid
{
	namespace TemplateHelper
	{
		template <typename Stack>
		void		ChangeSensorCoverage(const SkySensorState&state, short change, Stack*cellStack, bool*familyChanged)
		{
			Timer::Time t0 = timer.Now();

			Concurrency::parallel_for(index_t(0),state.coverage.Count(),[&state, change, cellStack, familyChanged](index_t i)
			{
				const auto&c = state.coverage[i];
				auto&stack = cellStack[c.hStackID];
				UINT32 downTo = CellStack::ToLayer(c.downToHeight);
				for (UINT32 layer = CellStack::Range-1; layer >= downTo && layer < CellStack::Range; layer--)
				{
					stack.volumeCell[layer].AlterVisibility(change);
				}
				familyChanged[Grid::high.grid.GetCell(c.hStackID).family] = true;
			});

			LogEvent(EventSignificance::PerformanceMeasurement,nullptr,"Sky sensory change ("+String(change)+") processed on "+String(state.coverage.Count())+" stack(s) in "+String(timer.GetSecondsSince(t0)));

		}


		template <typename Stack>
		void		ChangeSensory(const Grid::HighSetup::TVolumeDescriptor&desc, short changeBy, Stack*cellStack, bool*familyChanged)
		{
			#ifdef _DEBUG
			{
				UINT32 crc = CRC32::getChecksum(desc.coverage.pointer(),desc.coverage.GetContentSize());
				LogEvent(EventSignificance::Debug,"Applying sensory change ("+String(changeBy)+"), using descriptor with "+String(desc.coverage.Count())+" stack(s), crc="+IntToHex((int)crc,8));	
			}

			#endif


			foreach (desc.coverage,c)
			{
				#ifdef _DEBUG
					ASSERT_LESS__(c->stackID,HighSetup::GridType::CellCount);
					ASSERT_LESS_OR_EQUAL__(c->range.start,c->range.end);
					ASSERT_LESS__(c->range.start,CellStack::Range);
				#endif
				if (c->range.IsEmpty())
					continue;
				auto&stack = cellStack[c->stackID];
				familyChanged[high.grid.GetCell(c->stackID).family] = true;
				for (UINT32 layer = c->range.start; layer < c->range.end; layer++)
				{
					stack.volumeCell[layer].AlterVisibility(changeBy);
				}
			}
		}


		template <typename Stack>
		void		ChangeSensory(const Grid::HCellIdU&center, short changeBy, Stack*cellStack, bool*familyChanged)
		{
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

		}
		
		template <typename Stack>
		void		ChangeSensory(const TVec3<>&worldCenter, float radius, short change, Stack*cellStack, bool*familyChanged)
		{
			Timer::Time t0 = timer.Now();

			//Math::Box<> test;
			//test.SetCenter(worldCenter,radius);
			//for (BYTE family = 0; family < BaseGrid::NumFamilies; family++)
			//{
			//	if (!Grid::high.planetFamilyBoundingBox[family].Intersects(test))
			//		continue;

			//	const index_t offset = high.grid.GetFamilyOffset(family);
			//	const count_t range = high.grid.CountFamilyMembers(family);
			//	bool anyChanged = false;
			//	Concurrency::parallel_for(offset,offset+range,[cellStack,familyChanged, worldCenter,radius,change,&anyChanged](index_t i)
			//	{
			//		//bool anyChanged = false;
			//		bool anyVisible = false;
			//		Grid::HCellIdU cell((UINT32)i,0U);
			//		{
			//			Grid::HCellIdU aboveTop(cell.stackID, Grid::CellStack::Range);
			//			TVec3<> bottom = Grid::ToWorld(cell),
			//					top = Grid::ToWorld(aboveTop);

			//			anyVisible = Obj::DetectSphereEdgeIntersection(worldCenter,radius,bottom,top);
			//		}
			//		if (!anyVisible)
			//			return;

			//		bool wasIn = false;
			//		for (unsigned j = 0; j < Grid::CellStack::Range; j++)
			//		{
			//			cell.layer = j;
			//			if (Vec::quadraticDistance(Grid::ToWorld(cell),worldCenter) > sqr(radius))
			//			{
			//				if (wasIn)
			//					break;
			//				continue;
			//			}
			//			wasIn = true;
			//			cellStack[i].volumeCell[j].AlterVisibility(change);
			//			anyChanged = true;
			//		}
			//	});
			//	if (anyChanged)
			//		familyChanged[family] = true;

			//}

			enum
			{
				Visited = 0x1,
				Visible = 0x2,
				Translucent = 0x4
			};

			const Grid::HCellIdU center(Grid::high.Locate(worldCenter),Grid::CellStack::ToLayer( Grid::HeightFromRadius(Vec::length( worldCenter )) ));
			Buffer0<Grid::HCellIdU>	marked;
			{
				auto&cell = cellStack[center.stackID].volumeCell[center.layer];
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
				auto&c = cellStack[current.stackID].volumeCell[current.layer];
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

				bool translucent = Grid::actualWorld.cellStack[current.stackID].volumeCell[current.layer].IsClearOrWaterIgnoreStructure();

				float visibleWeight=0, totalWeight=0;
				Grid::high.VisitNeighborsOf(current,[cellStack,&visibleWeight,&totalWeight, &work,current,worldCenter,d03,c3,translucent](const Grid::HCellIdU&next)
				{
					auto&n = cellStack[next.stackID].volumeCell[next.layer];
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
				auto&c = cellStack[m->stackID].volumeCell[m->layer];
				bool makeVisible = (c.user & Visible) != 0;

				if (!makeVisible)
				{
					Grid::high.VisitNeighborsOf(*m,[cellStack,&makeVisible](const Grid::HCellIdU&next)
					{
						const auto&n = cellStack[next.stackID].volumeCell[next.layer];
						if (n.user & Visible)
							makeVisible = true;
					});
				}

				if (makeVisible)
				{
					c.AlterVisibility(change);
					familyChanged[high.grid.GetCell(m->stackID).family] = true;
				}
			}

			foreach (marked,m)
				cellStack[m->stackID].volumeCell[m->layer].user = 0;

			LogEvent(EventSignificance::PerformanceMeasurement,nullptr,"Sensory change processed in "+String(timer.GetSecondsSince(t0)));
		}


	};

}


#endif

#ifndef sphereGridHelperH
#define sphereGridHelperH

#include "sphereGrid.h"

#ifdef _DEBUG
	#include <io/encryption.h>

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
			LogEvent(EventSignificance::PerformanceMeasurement,nullptr,"Sky sensory change processed in "+String(timer.GetSecondsSince(t0)));
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
					ASSERT_LESS_OR_EQUAL__(c->layerBegin,c->layerEnd);
					ASSERT_LESS__(c->layerBegin,CellStack::Range);
				#endif
				if (c->layerEnd == c->layerBegin)
					continue;
				auto&stack = cellStack[c->stackID];
				familyChanged[high.grid.GetCell(c->stackID).family] = true;
				for (UINT32 layer = c->layerBegin; layer < c->layerEnd; layer++)
				{
					stack.volumeCell[layer].AlterVisibility(changeBy);
				}
			}
		}


		template <typename Stack>
		void		ChangeTechniteSensory(const Grid::HCellIdU&center, short changeBy, Stack*cellStack, bool*familyChanged)
		{
			unsigned bottom = center.layer > 1 ? center.layer - 2 : 0;
			unsigned top = std::min(Grid::CellStack::Range-1, center.layer + 2);


			const auto&c = high.grid.GetCell(center.stackID);
			for (int k = 0; k < c.neighborhood.numNeighbors; k++)
			{
				const auto&c2 = high.grid.GetCell(c.neighborhood.neighborID[k]);
				for (int j = 0; j < c2.neighborhood.numNeighbors; j++)
				{
					const UINT32 id = c2.neighborhood.neighborID[j];
					const auto&c3 = high.grid.GetCell(id);
					familyChanged[c3.family] = true;

					auto&stack = cellStack[id];
					for (unsigned h = bottom; h <= top; h++)
						stack.volumeCell[h].AlterVisibility(changeBy);
				}
			}


			//{
			//	auto&stack = cellStack[center.stackID];
			//	for (unsigned h = bottom; h <= top; h++)
			//		stack.volumeCell[h].AlterVisibility(changeBy);
			//}

			//familyChanged[c.family] = true;
			//for (int k = 0; k < c.neighborhood.numNeighbors; k++)
			//{
			//	familyChanged[high.grid.GetCell(c.neighborhood.neighborID[k]).family] = true;

			//	auto&stack = cellStack[c.neighborhood.neighborID[k]];
			//	for (unsigned h = bottom; h <= top; h++)
			//		stack.volumeCell[h].AlterVisibility(changeBy);
			//}

		}
		
		template <typename Stack>
		void		ChangeSensory(const TVec3<>&worldCenter, float radius, short change, Stack*cellStack, bool*familyChanged)
		{
			Timer::Time t0 = timer.Now();

			Math::Box<> test;
			test.SetCenter(worldCenter,radius);
			for (BYTE family = 0; family < BaseGrid::NumFamilies; family++)
			{
				if (!Grid::high.planetFamilyBoundingBox[family].Intersects(test))
					continue;

				const index_t offset = high.grid.GetFamilyOffset(family);
				const count_t range = high.grid.CountFamilyMembers(family);
				bool anyChanged = false;
				Concurrency::parallel_for(offset,offset+range,[cellStack,familyChanged, worldCenter,radius,change,&anyChanged](index_t i)
				{
					//bool anyChanged = false;
					bool anyVisible = false;
					Grid::HCellIdU cell((UINT32)i,0U);
					{
						Grid::HCellIdU aboveTop(cell.stackID, Grid::CellStack::Range);
						TVec3<> bottom = Grid::ToWorld(cell),
								top = Grid::ToWorld(aboveTop);

						anyVisible = Obj::DetectSphereEdgeIntersection(worldCenter,radius,bottom,top);
					}
					if (!anyVisible)
						return;

					bool wasIn = false;
					for (unsigned j = 0; j < Grid::CellStack::Range; j++)
					{
						cell.layer = j;
						if (Vec::quadraticDistance(Grid::ToWorld(cell),worldCenter) > sqr(radius))
						{
							if (wasIn)
								break;
							continue;
						}
						wasIn = true;
						cellStack[i].volumeCell[j].AlterVisibility(change);
						anyChanged = true;
					}
				});
				if (anyChanged)
					familyChanged[family] = true;

			}

			LogEvent(EventSignificance::PerformanceMeasurement,nullptr,"Sensory change processed in "+String(timer.GetSecondsSince(t0)));
			//ShowMessage(timer.GetSecondsSince(t0));



					#ifdef VOXEL_SCANNER
					foreach (Grid::lowCellStack,stack)
						for (unsigned i = 0; i < Grid::CellStack::Range; i++)
						{
							stack->volumeCell[i].visibility = Grid::LowCellStack::Cell::Unchecked;
						}
					if (selectedEntity)
					{

						struct TWalker
						{
							Grid::LCellIdI	id;
							//float			distance;
						};



						Queue<TWalker>	workQueue;
						{
							TWalker&first = workQueue.Push();
							//first.distance = 0;
							first.id = selectedEntity->location;
						}
						const Grid::LCellIdI start = workQueue.Peek().id;

						const TVec3<> root = Grid::low.cells[start.stackID.centerVertex.xyz * Grid::ToRadius(start.height);

						static Buffer<Grid::LCellIdI>	detected;
						detected.Clear();

						float maxDistance = 24;
						count_t cellCount = 0;
						TWalker current;
						TIntRange<index_t>	stepCounts;
						stepCounts.end = 0;
						stepCounts.start = InvalidIndex;

						while (workQueue >> current)
						{
							UINT layer0 = Grid::CellStack::ToLayer(current.id.height);
							Grid::LowCellStack::Cell&cell = Grid::lowCellStack[current.id.stackID.volumeCell[layer0];
							if (cell.visibility != cell.Unchecked)
								continue;	//been here

							const TVec3<> local = Grid::low.cells[current.id.stackID.centerVertex.xyz * Grid::ToRadius(current.id.height);
							const float d2 = Vec::quadraticDistance(local,root);
							if (d2 > sqr(maxDistance))
								continue;

							if (current.id != start)
							{
							

								RayWalker walker(current.id,start,local,root);
								while (walker.Walk());

								stepCounts.Include(walker.cellsTouched);
								if (walker.shaded)
								{
									cell.visibility = cell.Invisible;
									continue;
								}
							}
							detected << current.id;
							cell.visibility = cell.Visible;
							if (Grid::lowCellStack[current.id.stackID.volumeCell[layer0].isFull)
								continue;
							cellCount++;
							TVec3<> localRoot;
							Vec::sub(local,root,localRoot);

							int lower = std::max(current.id.height -1, -Grid::CellStack::HalfRange);
							int upper = std::min(current.id.height+1,Grid::CellStack::HalfRange-1);
							for (int h = lower; h <= upper; h++)
							{
								UINT layer = Grid::CellStack::ToLayer(h);
								const auto&n = Grid::low.grid.GetCell(current.id.cellID).neighborhood;
								for (BYTE k = 0; k < n.numNeighbors; k++)
								{
									Grid::LCellIdI id(n.neighborID[k],h);
									const TVec3<> remote = Grid::low.cells[id.stackID.centerVertex.xyz * Grid::ToRadius(id.height);
									TVec3<> localD;
									Vec::sub(remote,local,localD);

								
									if (current.id != start && Vec::intensity(localD, localRoot) <0)
										continue;
									if (Vec::quadraticDistance(root,remote) < d2)
										continue;
									//float d = current.distance + Vec::distance(remote,local);
									//if (d >= maxDistance)
										//continue;
									TWalker&nd = workQueue.Push();
									//nd.distance = d;
									nd.id = id;
								}
								if (h != current.id.height)
								{
									Grid::LCellIdI id(current.id.cellID,h);
									const TVec3<> remote = Grid::low.cells[id.stackID.centerVertex.xyz * Grid::ToRadius(id.height);
									if (Vec::quadraticDistance(root,remote) >= d2 && (
											current.id == start || Vec::intensity(remote - local, local - root) >= 0))
									{
										TWalker&nd = workQueue.Push();
										//nd.distance = current.distance + Vec::distance(remote,local);
										nd.id = id;
									}
								}
							}
						}
						//ShowMessage("cells: "+String(cellCount)+", ray min: "+String(stepCounts.start)+", max: "+String(stepCounts.end-1));
						foreach (detected,cell)
						{
							//Grid::cellStack[cell->stackID.volumeCell[Grid::CellStack::ToLayer(cell->height)].visible = true;
							Grid::high.family[Grid::low.grid.GetCell(cell->cellID).family].SetOutdated(0);
						}
					}
			
					#endif
			
		}



	};

}


#endif

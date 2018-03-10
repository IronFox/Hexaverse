#include "pathfinding.h"
#include "vehicle.h"
#include "gridAndChoices.h"
#include "faction.h"

namespace Objects{ namespace Pathfinding
{


	
	#ifdef CLIENT

		bool NoHostileVehicleHere(const Grid::LCellIdI&cellID, const OwnerParameter&v)
		{
			PVehicle vh = GetVehicle(cellID);
			return !vh || vh->owner->IsFriendly(v);
		}
		#ifndef NO_SATELLITES
			bool NoHostileSatelliteHere(UINT32 cellID, const OwnerParameter&v)
			{
				PSatellite vh = GetSatellite(cellID);
				return !vh || vh->owner->IsFriendly(v);
			}
		#endif
	#else
		bool NoHostileVehicleHere(const Grid::LCellIdI&cellID, const PFaction&v, float t)
		{
			PVehicle vh = GetVehicle(cellID);
			return !vh || vh->owner->IsFriendly(v) 
					|| !v->CanSee(vh,t)
			;
		}

		#ifndef NO_SATELLITES
			bool NoHostileSatelliteHere(UINT32 cellID, const PFaction&v, float t)
			{
				PSatellite vh = GetSatellite(cellID);
				return !vh || vh->owner->IsFriendly(v) 
						|| !v->CanSee(vh,t)
				;
			}
		#endif
	#endif


	static count_t GetAllWays( const Grid::LCellIdI&location, Grid::LCellIdI*outChoices)
	{
		count_t rs = 0;
		const auto&n = Grid::low.grid.GetCell(location.stackID).neighborhood;

		for (int j = -1; j <= 1; j++)
		{
			if (location.height + j < -Grid::CellStack::HalfRange)
				continue;
			if (location.height + j >= Grid::CellStack::HalfRange)
				continue;
			if (j != 0)
			{
				(*outChoices++) = location + j;
				rs++;			
			}
			else
				for (int i = 0; i < n.numNeighbors; i++)
				{
					const Grid::LCellIdI	cell(n.neighborID[i], location.height + j);

					{
						(*outChoices++) = cell;
						rs++;
					}
				}	
		}
		return rs;
	
	}

	static count_t GetAllSatelliteWays( const UINT32 location, UINT32*outChoices)
	{
		count_t rs = 0;
		const auto&n = Grid::low.grid.GetCell(location).neighborhood;

		{
			for (int i = 0; i < n.numNeighbors; i++)
			{
				{
					(*outChoices++) = n.neighborID[i];
					rs++;
				}
			}	
		}
		return rs;
	}

	static float GetEntityTime(const Objects::WorldEntity&e)
	{
		#ifdef CLIENT
			return 0;
		#else
			return e.entityTime;
		#endif
	}


	bool IsClear(const World::Data&world, const Grid::LCellIdI&location, const PFaction&faction, float t)
	{
		if (!world.LowCellIsClearIgnoreStructure(location))
			return false;	//terrain there

		const auto structCount = World::actual.GetMaxStructureCount(location);
		if (structCount<2)
			return true;	//nothing there, visible or otherwise, or at least it does not block

		#ifdef CLIENT
			return false;
		#else
			if (&world == &World::actual)
				return false; //blocking building there, no point pretending
			auto s = Objects::GetStructure(location);
			if (!s)	//might be in foundation, so look one above
				s = Objects::GetStructure(location+1);
			if (!s)
			{
				LogEvent(EventClass::Preset::internalError,faction,"IsClear(): Grid reports structure count "+String(structCount)+" in "+ToString(location)+", but can't find any structure here, or one above");
				return false;	//better to just avoid it, even if we can't check visibility
			}
			return !faction->CanSee(s,t);//can't see = not there
		#endif
				//!world.GridCanSee(location,1,false);//can't see = not there
	}

	template <bool IgnoreStructuresAtDestination>
	bool ValidDestinationOf( const World::Data&world,const Vehicle&v, const Grid::LCellIdI&location, const Grid::LCellIdI&finalDestination, bool ignoreWater, bool doFloat)
	{
		if (Grid::AffectedByEMP(location))
			return false;
		if (IsClear(world,location,v.owner, GetEntityTime(v)))
			return v.CanStandOn(world,location-1,ignoreWater,doFloat);

		auto hangar = AccessibleHangarHere(location,v);
		if (hangar && (location == finalDestination || hangar == AccessibleHangarHere(finalDestination,v)))
			return true;

		return IgnoreStructuresAtDestination;
	}



	void GetWays(	const World::Data&world,
							const Vehicle*v,  const Grid::LCellIdI&location, 
							TWays&outWays, const Grid::LCellIdI&destination, 
							bool ignoreVehiclesOnDestination, bool ignoreFriendlyVehiclesOnRoute, bool forceRevertible)
	{
		outWays.Clear();
		const auto&n = Grid::low.grid.GetCell(location.stackID).neighborhood;

		int maxWayDown = std::min(location.height + Grid::CellStack::HalfRange, v->GetMaxFallingHeight());
		if (forceRevertible)
			maxWayDown = std::min(maxWayDown,v->GetMaxClimbingHeight());
		bool canWalk[6] = {true,true,true,true,true,true};

		for (int j = v->GetMaxClimbingHeight(); j >= -maxWayDown; j--)
		{
			if (j > 0 && !world.LowCellIsClear(Grid::LCellIdI(location.stackID, location.height+j)))
			{
				outWays.Clear();	//discard any previously found (upper) ways, which are now considered inaccessible. this is only relevant with vehicles that can climb more than 1 height level up
				continue;
			}

			bool anyCanWalk = false;
			for (int i = 0; i < n.numNeighbors; i++)
			{
				if (!canWalk[i])
					continue;
				anyCanWalk = true;
				const Grid::LCellIdI	cell(n.neighborID[i], location.height + j);

				{
					bool okay = cell.height < Grid::CellStack::HalfRange && cell.height >= -Grid::CellStack::HalfRange;
					if (!okay)
						continue;
				}
				bool valid = ValidDestinationOf<false>(world,*v,cell, destination, false, false);
				if (!valid)
				{
					if (j <= 0 && !world.LowCellIsClear(cell))
						canWalk[i] = false;
					continue;
				}


				if (cell==destination)
				{
					if (!ignoreVehiclesOnDestination && !NoVehicleHereExcept(cell,v))
						continue;
				}
				else
				{
					if (!ignoreFriendlyVehiclesOnRoute)
					{
						if (!NoVehicleHereExcept(cell,v))
							continue;
					}
					else
						#ifdef CLIENT
							if (!NoHostileVehicleHere(cell,v->owner))
								continue;
						#else
							if (!NoHostileVehicleHere(cell,v->owner,v->entityTime))
								continue;
						#endif
				}

				//DBG_ASSERT__(!GetStructure(cell));//should be implied by LowCellIsClear (except if it's a connector, then not, so don't check this)
				outWays.Add(cell);
			}
			if (!anyCanWalk)
				break;
		}
	}


	#ifndef NO_SATELLITES
		void GetWays(		const Satellite*v,  UINT32 location, 
								TOrbitalWays&outWays, UINT32 destination, 
								bool ignoreVehiclesOnDestination, bool ignoreFriendlyVehiclesOnRoute)
		{
			outWays.Clear();
			const auto&n = Grid::low.grid.GetCell(location).neighborhood;

			{
				//if (!v->CanClimb(j))	//now redundant
					//continue;
				for (int i = 0; i < n.numNeighbors; i++)
				{
					UINT32	cell = n.neighborID[i];


					if (cell==destination)
					{
						if (!ignoreVehiclesOnDestination && !NoSatelliteHereExcept(cell,v))
							continue;
					}
					else
					{
						if (!ignoreFriendlyVehiclesOnRoute)
						{
							if (!NoSatelliteHereExcept(cell,v))
								continue;
						}
						else
							#ifdef CLIENT
								if (!NoHostileSatelliteHere(cell,v->owner))
									continue;
							#else
								if (!NoHostileSatelliteHere(cell,v->owner,v->motionTime))
									continue;
							#endif
					}

					//DBG_ASSERT__(!GetStructure(cell));//should be implied by LowCellIsClear (except if it's a connector, then not, so don't check this)
					outWays.Add(cell);
				}	
			}
		}
	#endif

	
	void			GetMobileWays(	const World::Data&world,
							const MobileEntity*v,  const Grid::LCellIdI&location, 
							TWays&outWays, const Grid::LCellIdI&destination, 
							bool ignoreVehiclesOnDestination, bool ignoreFriendlyVehiclesOnRoute)
	{
		if (v->IsVehicle())
		{
			GetWays(world,(const Vehicle*)v, location, outWays, destination,ignoreVehiclesOnDestination,ignoreFriendlyVehiclesOnRoute,false);
			return;
		}
		outWays.Clear();
		#ifndef NO_SATELLITES
			if (v->IsSatellite())
			{
				TOrbitalWays ways;
				GetWays((const Satellite*)v,location.stackID,ways,destination.stackID,ignoreVehiclesOnDestination,ignoreFriendlyVehiclesOnRoute);
				for (index_t i = 0; i < ways.numWays; i++)
				{
					outWays.Add(Grid::LCellIdI(ways.way[i],location.height));
				}
				return;
			}
		#endif

		DBG_FATAL__("Unexpected mobile type used to determine ways");
	}



	static count_t GetConstructionWays(const World::Data&world, bool ignoreWater, bool flat, bool canGoDownVertically,
										const Vehicle*v,  const Grid::LCellIdI&location, 
										Grid::LCellIdI*outChoices, const Grid::LCellIdI&destination)
	{
		index_t rs = 0;
		const auto&n = Grid::low.grid.GetCell(location.stackID).neighborhood;


		if (flat)
		{
			for (int i = 0; i < n.numNeighbors; i++)
			{
				const Grid::LCellIdI	cell(n.neighborID[i], location.height);

				bool okay = cell.height < Grid::CellStack::HalfRange && cell.height >= -Grid::CellStack::HalfRange;
				if (okay)
				{
					for (int j = cell.height; j > location.height; j--)
						if (!world.LowCellIsClear(Grid::LCellIdI(location.stackID, j)))
						{
							okay = false;
							break;
						}
				}
				if (!okay)
					continue;

				if ( ValidDestinationOf<true>(world,*v,cell, destination, ignoreWater, true) )
				{
					(*outChoices++) = cell;
					rs++;
				}
			}

			return rs;
		}

		for (int j = -1; j <= 1; j++)
		{
			if (!v->CanClimb(j))
				continue;
			for (int i = 0; i < n.numNeighbors; i++)
			{
				const Grid::LCellIdI	cell(n.neighborID[i], location.height + j);

				bool okay = cell.height < Grid::CellStack::HalfRange && cell.height >= -Grid::CellStack::HalfRange;
				if (okay)
				{
					if (j < 0)
					{
						for (int j = cell.height + 1; j <= location.height; j++)
							if (!world.LowCellIsClear(Grid::LCellIdI(n.neighborID[i], j)))
							{
								okay = false;
								break;
							}
					}
					else
						for (int j = cell.height; j > location.height; j--)
							if (!world.LowCellIsClear(Grid::LCellIdI(location.stackID, j)))
							{
								okay = false;
								break;
							}
				}
				if (!okay)
					continue;

				if ( ValidDestinationOf<true>(world,*v,cell, destination, ignoreWater, false) )
//				if ((cell == destination || world.LowCellIsClear(cell)) && (v->CanStandOn(world,cell-1,ignoreWater)))
				{
					(*outChoices++) = cell;
					rs++;
				}
			}
		}

		if (canGoDownVertically)
		{
			static const int j = -1;
			const Grid::LCellIdI	cell(location.stackID, location.height + j);

			bool okay = cell.height < Grid::CellStack::HalfRange && cell.height >= -Grid::CellStack::HalfRange;
			if (okay)
			{
				if (j < 0)
				{
					for (int j = cell.height + 1; j <= location.height; j++)
						if (!world.LowCellIsClear(Grid::LCellIdI(location.stackID, j)))
						{
							okay = false;
							break;
						}
				}
				else
					for (int j = cell.height; j > location.height; j--)
						if (!world.LowCellIsClear(Grid::LCellIdI(location.stackID, j)))
						{
							okay = false;
							break;
						}
			}
			if ( okay && ValidDestinationOf<true>(world,*v,cell, destination, ignoreWater, false) )
//				if ((cell == destination || world.LowCellIsClear(cell)) && (v->CanStandOn(world,cell-1,ignoreWater)))
			{
				(*outChoices++) = cell;
				rs++;
			}
		}
		return rs;

	}




	

	void AccumulateCellMovementCosts(const World::Data&world, count_t&airCount,count_t&waterCount,count_t&roadCount,const Grid::HCellIdI&cell,count_t multiplier=1)
	{
		const Grid::CellStack::Cell&c = world.GetStack(cell.stackID).GetCellI(cell.height);
		switch (c.content)
		{
			case Grid::Content::Clear:
				airCount += multiplier;
			break;
			case Grid::Content::Water:
				waterCount += multiplier;
			break;
			case Grid::Content::Road:
			case Grid::Content::Foundation:
				roadCount += multiplier;
			break;
		}
	}


	int GetCellMovementCosts(const World::Data&world, const Grid::LCellIdI&cell, const THindranceVector&hindrance, int vehicleSpeed)
	{
		#ifdef HYBRID_GRID
			const UINT32 high = Grid::low.grid.GetCell(cell.stackID).higherEquivalent;
			const auto&n = Grid::high.grid.GetCell(high).neighborhood;
		#else
			const UINT32 high = cell.cellID;
		#endif

		count_t airCount = 0;
		count_t waterCount = 0;
		count_t roadCount = 0;
		

		AccumulateCellMovementCosts(world, airCount,waterCount,roadCount,Grid::HCellIdI(high,cell.height-1),2);
		#ifdef HYBRID_GRID
			for (int i = 0; i < n.numNeighbors; i++)
				AccumulateCellMovementCosts(world, airCount,waterCount,roadCount,Grid::HCellIdI(n.neighborID[i],cell.height-1));
			count_t total = 2 + n.numNeighbors;
		#else
			count_t total = 2;
		#endif

		count_t rest = total - airCount - waterCount - roadCount;

		return M::Round(float(hindrance.road) * float(roadCount) / float(total) + 
						float(hindrance.air) * float(airCount) / float(total) + 
						float(hindrance.water) * float(waterCount) / float(total) + 
						float(hindrance.other) * float(rest) / float(total));


		//float speed = 1.f - hindrance.road * float(roadCount) / float(total)
		///*speed *= 1.f */- hindrance.air * float(airCount) / float(total)
		///*speed *= 1.f */- hindrance.water * float(waterCount) / float(total)
		///*speed *= 1.f */- hindrance.other * float(rest) / float(total);

		//return std::max(1,(int)ceil(  (1.f - speed) * vehicleSpeed));
	}


	bool AppendShortestPath(const Vehicle*vehicle, const Grid::LCellIdI&from,
							const Grid::LCellIdI&to, Ctr::BasicBuffer<PathNode>&oPath,
							bool ignoreFriendlyVehiclesOnRoute, bool failIfLimitReached/*=true*/, bool*outWouldFailDueToLimitReached/*=nullptr*/);

	bool AppendShortestPathI(const Vehicle*vehicle, const Grid::LCellIdI&from,
							const Grid::LCellIdI&to, Ctr::BasicBuffer<PathNode>&oPath, bool failIfLimitReached/*=true*/, bool*outWouldFailDueToLimitReached/*=nullptr*/)
	{
		static Ctr::Buffer0<PathNode>	routeI, route;
		routeI.Clear();
		route.Clear();
		bool hasI = AppendShortestPath(vehicle,from,to,routeI,true,failIfLimitReached,outWouldFailDueToLimitReached);
		if (!hasI)
			return false;
		bool has = AppendShortestPath(vehicle,from,to,route,false,failIfLimitReached,outWouldFailDueToLimitReached);
		//foreach (routeI,r)
		//	ASSERT__(*r != Grid::LCellIdI::Invalid);
		//if (has)
		//	foreach (route,r)
		//		ASSERT__(*r != Grid::LCellIdI::Invalid);
		if (!has)
		{
			oPath.AppendAll(routeI);
			return true;
		}

		count_t lenI = 0, len = 0;
		foreach (routeI,n)
			lenI += n->movementCostsFromPrevNode;
		foreach (route,n)
			len += n->movementCostsFromPrevNode;
		if (len < lenI * 4)	//less than 400% more
		{
			oPath.AppendAll(route);
			return true;
		}

		oPath.AppendAll(routeI);		//else: use the one that ignores friendly vehicles
		return true;

	}

	#ifndef NO_SATELLITES
		bool AppendShortestPath(const Satellite*sat, const Grid::LCellIdI&from,
								const UINT32 to, BasicBuffer<PathNode>&oPath,
								bool ignoreFriendlyVehiclesOnRoute, bool failIfLimitReached/*=true*/, bool*outWouldFailDueToLimitReached/*=nullptr*/);

		bool AppendShortestPathI(const Satellite*sat, const Grid::LCellIdI&from,
								const UINT32 to, BasicBuffer<PathNode>&oPath, bool failIfLimitReached/*=true*/, bool*outWouldFailDueToLimitReached/*=nullptr*/)
		{
			static Buffer0<PathNode>	routeI, route;
			routeI.Clear();
			route.Clear();
			bool hasI = AppendShortestPath(sat,from,to,routeI,true,failIfLimitReached,outWouldFailDueToLimitReached);
			if (!hasI)
				return false;
			bool has = AppendShortestPath(sat,from,to,route,false,failIfLimitReached,outWouldFailDueToLimitReached);
			//foreach (routeI,r)
			//	ASSERT__(*r != Grid::LCellIdI::Invalid);
			//if (has)
			//	foreach (route,r)
			//		ASSERT__(*r != Grid::LCellIdI::Invalid);
			if (!has)
			{
				oPath.Append(routeI);
				return true;
			}

			count_t lenI = 0, len = 0;
			foreach (routeI,n)
				lenI += n->movementCostsFromPrevNode;
			foreach (route,n)
				len += n->movementCostsFromPrevNode;
			if (len < lenI * 4)	//less than 400% more
			{
				oPath.Append(route);
				return true;
			}

			oPath.Append(routeI);		//else: use the one that ignores friendly vehicles
			return true;
		}
	#endif






		
	bool AppendShortestPath(const Vehicle*vehicle, const Grid::LCellIdI&from,
							const Grid::LCellIdI&to, Ctr::BasicBuffer<PathNode>&oPath,
							bool ignoreFriendlyVehiclesOnRoute, bool failIfLimitReached/*=true*/, bool*outWouldFailDueToLimitReached/*=nullptr*/)
	{
		bool inRange = true;
		if (outWouldFailDueToLimitReached)
			(*outWouldFailDueToLimitReached) = false;

		const World::Data&world = 
		#ifdef CLIENT
			World::actual;
		#else
			vehicle->owner->knownWorld;
		#endif
		

		if (!ValidDestinationOf<false>(world,*vehicle,to,to,false, false))
			return false;

		//if (!vehicle->CanStandOn(world,to -1,false) || 
		//	(!world.LowCellIsClear(to) && !VehicleDestinationStructureHere(to) ))// || !NothingHere(destination))
		//		return false;
	//	Timer::Time t0 = timer.Now();
		const THindranceVector&hindrance = vehicle->base->hindrance;
		const int speed = vehicle->GetMovementStepsPerRound();
		TWays	possibilities;

		static const count_t cellCount = Grid::LowSetup::GridType::CellCount * Grid::CellStack::Range;

		struct TNode
		{
			UINT32	backLink, minDistance;
			int		nodeCosts;
		};

		static Ctr::Array<TNode>	buffer(cellCount);
		TNode empty = {0xFFFFFFFF,0xFFFFFFFF,0};
		buffer.Fill(empty);

		Ctr::PriorityQueue<index_t,int>	queue;
		#undef IDOF
		#define IDOF(CELL)	(Grid::CellStack::ToLayer( (CELL).height ) * Grid::LowSetup::GridType::CellCount + (CELL).stackID)
		#undef DESCOF
		#define DESCOF(CELLID)	Grid::LCellIdI((CELLID)%Grid::LowSetup::GridType::CellCount, Grid::CellStack::ToHeight( (UINT32)((CELLID) / Grid::LowSetup::GridType::CellCount)))

		buffer[IDOF(from)].minDistance = 0;

		queue.Push( IDOF(from),0);

		count_t iterations = 0;
		index_t current;
		while (queue.PopLeast(current))
		{
			iterations ++;
			if (iterations > 2000)
			{
				if (failIfLimitReached)	//client sided operation may choose to not implement maximum ranges
				{
					//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
					return false;//let's ignore long ranges
				}
				else
				{
					if (outWouldFailDueToLimitReached)
						(*outWouldFailDueToLimitReached) = true;
					inRange = false;
				}
			}

			Grid::LCellIdI cellID(DESCOF(current));
			TNode&tn = buffer[current];
			if (!tn.nodeCosts)
			{
				tn.nodeCosts = GetCellMovementCosts(world,cellID,hindrance,speed);
			}


			if (cellID == to)
			{
				static Ctr::Buffer<PathNode>	outPath;
				outPath.Clear();

				while (current < cellCount)
				{
					UINT32 next = buffer[current].backLink;
					int nextCosts = next < buffer.Count() ? buffer[next].nodeCosts : 0;
					//GetCellMovementCosts(next, vehicle);
					int hCosts = next < buffer.Count() ?  std::max(0, DESCOF(current).height - DESCOF(next).height ) : 0;
					outPath << PathNode(DESCOF(current), std::max(nextCosts, buffer[current].nodeCosts) + hCosts);
//					outPath.Last().reachedInProcessingLimit = inRange;
					current = next;
				}
				outPath.EraseLast();	//dont need entry point
				outPath.Revert();
				oPath.AppendAll(outPath);
				//ShowMessage("found route with "+String(outPath.Count())+" steps, requiring "+String(iterations)+" iterations" );
				//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
				return true;
			}
			GetWays(world,vehicle,  cellID, possibilities, to, true, ignoreFriendlyVehiclesOnRoute,false);


			for (index_t i = 0; i < possibilities.numWays; i++)
			{
				UINT32 option = IDOF(possibilities.way[i]);
				TNode&n = buffer[option];
				if (!n.nodeCosts)
					n.nodeCosts = GetCellMovementCosts(world,possibilities.way[i],hindrance,speed);
				int cost = std::max(tn.nodeCosts,n.nodeCosts);
				int hCosts = std::max(0, possibilities.way[i].height - cellID.height );
				cost += hCosts;
				if (tn.minDistance + cost < n.minDistance)
				{
					n.minDistance = tn.minDistance + cost;
					n.backLink = (UINT32)current;
					queue.Push(option,n.minDistance);
				}
			}
		}
		//ShowMessage("no route found after "+String(iterations)+" iterations");
		//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
		return false;
	}

	#ifndef NO_SATELLITES
		bool AppendShortestPath(const Satellite*sat, const Grid::LCellIdI&from,
								const UINT32 to, BasicBuffer<PathNode>&oPath,
								bool ignoreFriendlyVehiclesOnRoute
								, bool failIfLimitReached/*=true*/, bool*outWouldFailDueToLimitReached/*=nullptr*/)
		{
			if (outWouldFailDueToLimitReached)
				(*outWouldFailDueToLimitReached) = false;
			const int speed = sat->GetMovementStepsPerRound();
			TOrbitalWays	possibilities;

			static const count_t cellCount = Grid::LowSetup::GridType::CellCount;

			struct TNode
			{
				UINT32	backLink, minDistance;
			};

			static Ctr::Array<TNode>	buffer(cellCount);
			TNode empty = {0xFFFFFFFF,0xFFFFFFFF};
			buffer.Fill(empty);

			PriorityQueue<UINT32,int>	queue;
			//#undef IDOF
			//#define IDOF(CELL)	((CELL).stackID)
			//#undef DESCOF
			//#define DESCOF(CELLID)	Grid::LCellIdI((CELLID), from.height)

			buffer[from.stackID].minDistance = 0;

			queue.Push( from.stackID,0);

			count_t iterations = 0;
			UINT32 current;
			while (queue.PopLeast(current))
			{
				iterations ++;
				if (iterations > 2000)
				{
					if (failIfLimitReached)
					{
						//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
						return false;//let's ignore long ranges
					}
					else
						if (outWouldFailDueToLimitReached)
							(*outWouldFailDueToLimitReached) = true;
				}

				//Grid::LCellIdI stackID(DESCOF(current));
				TNode&tn = buffer[current];


				if (current == to)
				{
					static Buffer<PathNode>	outPath;
					outPath.Clear();

					while (current < cellCount)
					{
						UINT32 next = buffer[current].backLink;
						int nextCosts = next < buffer.Count() ? 1 : 0;
						//GetCellMovementCosts(next, vehicle);
						outPath << PathNode(Grid::LCellIdI(current,sat->location.height), 1);
						current = next;
					}
					outPath.EraseLast();	//dont need entry point
					outPath.Revert();
					oPath.Append(outPath);
					//ShowMessage("found route with "+String(outPath.Count())+" steps, requiring "+String(iterations)+" iterations" );
					//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
					return true;
				}
				GetWays(sat,  current, possibilities, to, true, ignoreFriendlyVehiclesOnRoute);

				for (index_t i = 0; i < possibilities.numWays; i++)
				{
					UINT32 option = possibilities.way[i];
					TNode&n = buffer[option];
					if (tn.minDistance + 1 < n.minDistance)
					{
						n.minDistance = tn.minDistance + 1;
						n.backLink = current;
						queue.Push(option,n.minDistance);
					}
				}
			}
			//ShowMessage("no route found after "+String(iterations)+" iterations");
			//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
			return false;
		}
	#endif

	void ReachabilityMapper::Map(const Vehicle*vehicle, const Grid::LCellIdI&from, FBlockTest isBlocked, UINT32 maxRange/*=0xFFFFFFFF*/, bool forceRevertible/*=false*/)
	{
		const World::Data&world = 
		#ifdef CLIENT
			World::actual;
		#else
			vehicle->owner->knownWorld;
		#endif
		//Timer::Time t0 = timer.Now();
		const THindranceVector&hindrance = vehicle->base->hindrance;
		const int speed = vehicle->GetMovementStepsPerRound();
		TWays	possibilities;

		static const count_t cellCount = Grid::LowSetup::GridType::CellCount * Grid::CellStack::Range;


		buffer.SetSize(cellCount);
		TReachableNode empty = {0xFFFFFFFF,0xFFFFFFFF,0,false,InvalidIndex};
		buffer.Fill(empty);

		Ctr::PriorityQueue<index_t,int>	queue;
		#undef IDOF
		#define IDOF(CELL)	(Grid::CellStack::ToLayer( (CELL).height ) * Grid::LowSetup::GridType::CellCount + (CELL).stackID)
		#undef DESCOF
		#define DESCOF(CELLID)	Grid::LCellIdI((CELLID)%Grid::LowSetup::GridType::CellCount, Grid::CellStack::ToHeight( (UINT32)((CELLID) / Grid::LowSetup::GridType::CellCount)))

		buffer[IDOF(from)].minDistance = 0;

		queue.Push( IDOF(from),0);

		touched.Clear();

		count_t iterations = 0;
		index_t current;
		while (queue.PopLeast(current))
		{
			iterations ++;
			static bool ignoreIterations = false;	//runtime switch
			if (!ignoreIterations && iterations > 1998)
			{
				//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
				break;
			}

			Grid::LCellIdI cellID(DESCOF(current));
			TReachableNode&tn = buffer[current];
			if (!tn.nodeCosts)
			{
				tn.isBlocked = isBlocked && isBlocked(cellID);
				tn.nodeCosts = tn.isBlocked ? 1000000 :
								GetCellMovementCosts(world,cellID,hindrance,speed);
			}
			if (tn.isBlocked)
				continue;

			touched << current;

			GetWays(world,vehicle,  cellID, possibilities, Grid::LCellIdI::Invalid, true, false,forceRevertible);





			for (index_t i = 0; i < possibilities.numWays; i++)
			{
				UINT32 option = IDOF(possibilities.way[i]);
				TReachableNode&n = buffer[option];
				if (!n.nodeCosts)
				{
					n.isBlocked = isBlocked && isBlocked(possibilities.way[i]);
					n.nodeCosts = n.isBlocked ? 1000000 :
									GetCellMovementCosts(world,possibilities.way[i],hindrance,speed);
				}
				if (n.isBlocked)
					continue;

				int cost = std::max(tn.nodeCosts,n.nodeCosts);
				int hCosts = std::max(0, possibilities.way[i].height - cellID.height );
				cost += hCosts;
				if (tn.minDistance + cost < n.minDistance)
				{
					n.minDistance = tn.minDistance + cost;
					n.backLink = (UINT32)current;
					if (n.minDistance < maxRange)
						queue.Push(option,n.minDistance);
				}
			}
		}



		result.clear();
		foreach (touched,t)
		{
			TReachableNode&n = buffer[*t];
			if (n.mapped!=InvalidIndex || !n.minDistance)	//ignore first, and duplicates
				continue;
			n.mapped = result.Count();
			result << MapNode(DESCOF(*t),n.minDistance);
		}

		foreach (touched,t)
		{
			TReachableNode&n = buffer[*t];
			if (n.mapped != InvalidIndex)
			{
				if (n.backLink != 0xFFFFFFFF)
				{
					TReachableNode&pre = buffer[n.backLink];
					result[n.mapped].predecessor = pre.mapped;
					if (pre.mapped == InvalidIndex)
						ASSERT__(DESCOF(n.backLink) == from);
				}
				else
					ASSERT__(DESCOF(*t) == from);
			}
		}


		//ShowMessage("found route with "+String(outPath.Count())+" steps, requiring "+String(iterations)+" iterations" );
		//if (logPerformance)
			//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
	
	}

	static float maxAngleBetweenNeighboringCells = 0.f;


	void Init()
	{
		float max = 0.f;
		for (index_t i = 0; i < Grid::LowSetup::GridType::CellCount; i++)
		{
			const M::TVec3<>&d0 = Grid::low.cells[i].centerVertex.xyz;

			const auto&n = Grid::low.grid.GetCell(i).neighborhood;
			for (int c = 0; c < n.numNeighbors; c++)
				if (n.neighborID[c] > i)
				{
					const M::TVec3<>&d1 = Grid::low.cells[n.neighborID[c]].centerVertex.xyz;

					float a = Vec::angle(d0,d1);
					max = std::max(max,a);
				}

		}
		maxAngleBetweenNeighboringCells = max;
	}


	int EstimateMinimumRemainingCost(const Grid::LCellIdI&from, const Grid::LCellIdI&to, int minHorizontalCost, int minVerticalCost)
	{
		const M::TVec3<>&d0 = Grid::low.cells[from.stackID].centerVertex.xyz;
		const M::TVec3<>&d1 = Grid::low.cells[to.stackID].centerVertex.xyz;
		float a = Vec::angle(d0,d1);
		int hSteps = (int)floor(a / maxAngleBetweenNeighboringCells);
		int vSteps = abs(from.height - to.height);
		return hSteps * minHorizontalCost + vSteps * minVerticalCost;
	}

	
	bool AppendShortestConstructionPath(const Vehicle*vehicle, bool ignoreWater, bool flat, const Grid::LCellIdI&from, Grid::LCellIdI to, Ctr::BasicBuffer<Grid::LCellIdI>&oPath)
	{
		if (flat)
			to.height = from.height;
		if (from == to)
			return true;
		const World::Data&world = 
		#ifdef CLIENT
			World::actual;
		#else
			vehicle->owner->knownWorld;
		#endif
		if (!flat)
			if (!vehicle->CanStandOn(world,to -1,ignoreWater,false) /*|| !Grid::LowCellIsClear(to)*/)// || !NothingHere(destination))
			{
				if (flat)
					to++;
				if (!vehicle->CanStandOn(world,to -1,ignoreWater,false) /*|| !Grid::LowCellIsClear(to)*/)// || !NothingHere(destination))
					return false;
			}

		//Timer::Time t0 = timer.Now();
		Grid::LCellIdI	possibilities[6 * 3 + 2];

		static const count_t cellCount = Grid::LowSetup::GridType::CellCount * Grid::CellStack::Range;

		struct TNode
		{
			UINT32	backLink, minDistance;
		};
		

		static Ctr::Array<TNode>	buffer(cellCount);
		static Ctr::Array<bool> flatBuffer(Grid::LowSetup::GridType::CellCount);
		TNode empty = {0xFFFFFFFF,0xFFFFFFFF};
		buffer.Fill(empty);
		flatBuffer.Fill(false);

		Ctr::PriorityQueue<index_t,int>	queue;
		#undef IDOF
		#define IDOF(CELL)	(Grid::CellStack::ToLayer( (CELL).height ) * Grid::LowSetup::GridType::CellCount + (CELL).stackID)
		#undef DESCOF
		#define DESCOF(CELLID)	Grid::LCellIdI((CELLID)%Grid::LowSetup::GridType::CellCount, Grid::CellStack::ToHeight( (UINT32)((CELLID) / Grid::LowSetup::GridType::CellCount)))

		buffer[IDOF(from)].minDistance = 0;

		queue.Push(IDOF(from),0);

		count_t iterations = 0;
		index_t current;
		while (queue.PopLeast(current))
		{
			iterations ++;
			if (iterations > 2000)
			{
				//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
				return false;//let's ignore long ranges
			}

			Grid::LCellIdI cellID(DESCOF(current));
			TNode&tn = buffer[current];

			flatBuffer[cellID.stackID] = true;
			if (cellID == to)
			{
				static Ctr::Buffer<Grid::LCellIdI>	outPath;
				outPath.Clear();
				if (!world.LowCellIsClear(to))
				{
					current = buffer[current].backLink;	
				}
				while (current < cellCount)
				{
					UINT32 next = buffer[current].backLink;
					//GetCellMovementCosts(next, vehicle);
					outPath << DESCOF(current);
					current = next;
				}
				outPath.EraseLast();	//dont need entry point
				outPath.Revert();
				oPath.AppendAll(outPath);
				//ShowMessage("found route with "+String(outPath.Count())+" steps, requiring "+String(iterations)+" iterations" );
				//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
				return true;
			}
			count_t ways = GetConstructionWays(world, ignoreWater, flat, cellID == from, vehicle, cellID, possibilities, to);
			ASSERT_LESS_OR_EQUAL__(ways,ARRAYSIZE(possibilities));





			for (index_t i = 0; i < ways; i++)
			{
				if (flatBuffer[possibilities[i].stackID] && possibilities[i].stackID != cellID.stackID)
					continue;
				UINT32 option = IDOF(possibilities[i]);
				TNode&n = buffer[option];
				int cost = possibilities[i].stackID != cellID.stackID ? 1 : 0;
				int hCosts = abs(possibilities[i].height - cellID.height );
				cost += hCosts;

				//if (doFloat && possibilities[i].height != cellID.height)
				//{
				//	//cost ++;
				//	if (
				//		(possibilities[i].height > cellID.height && tn.backLink < buffer.Count() && DESCOF(tn.backLink).height < cellID.height)	//avoid vertical shafts up
				//		||
				//		(possibilities[i].height < cellID.height && tn.backLink < buffer.Count() && DESCOF(tn.backLink).height > cellID.height)	//avoid vertical shafts down
				//		)
				//		cost += 100000;
				//	if (possibilities[i].height > cellID.height && possibilities[i] == to)	//avoid going up to destination
				//		cost += 10000;
				//}

				if (tn.minDistance + cost < n.minDistance)
				{
					n.minDistance = tn.minDistance + cost + EstimateMinimumRemainingCost(possibilities[i],to, 1, 1);
					n.backLink = (UINT32)current;
					queue.Push(option,n.minDistance);
				}
			}
		}
		//ShowMessage("no route found after "+String(iterations)+" iterations");
		//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
		return false;
	}

	
	bool AppendShortestDirectPath(const Grid::LCellIdI&from, const Grid::LCellIdI&to, Ctr::BasicBuffer<PathNode>&oPath, bool penaltizeVerticalMotion/*=true*/, bool constrainVerticalMotion/*=true*/)
	{
		//Timer::Time t0 = timer.Now();

		Grid::LCellIdI	possibilities[6 * 3 + 2];

		const M::TIntRange<int> heightRange = M::IntRange(std::min(from.height,to.height),std::max(from.height,to.height)+1);

		static const count_t cellCount = Grid::LowSetup::GridType::CellCount * Grid::CellStack::Range;

		struct TNode
		{
			UINT32	backLink, minDistance;
		};

		static Ctr::Array<TNode>	buffer(cellCount);
		TNode empty = {0xFFFFFFFF,0xFFFFFFFF};
		buffer.Fill(empty);

		Ctr::PriorityQueue<index_t,int>	queue;
		//Queue<index_t>	queue;
		#undef IDOF
		#define IDOF(CELL)	(Grid::CellStack::ToLayer( (CELL).height ) * Grid::LowSetup::GridType::CellCount + (CELL).stackID)
		#undef DESCOF
		#define DESCOF(CELLID)	Grid::LCellIdI((CELLID)%Grid::LowSetup::GridType::CellCount, Grid::CellStack::ToHeight( (UINT32)((CELLID) / Grid::LowSetup::GridType::CellCount)))

		buffer[IDOF(from)].minDistance = 0;

		queue.Push(IDOF(from),0);

		count_t iterations = 0;
		index_t current;
		while (queue.PopLeast(current))
		{
			iterations ++;
			if (iterations > 2000)
			{
				//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
				return false;//let's ignore long ranges
			}

			Grid::LCellIdI cellID(DESCOF(current));
			TNode&tn = buffer[current];


			if (cellID == to)
			{
				static Ctr::Buffer<PathNode>	outPath;
				outPath.Clear();

				while (current < cellCount)
				{
					UINT32 next = buffer[current].backLink;
					//GetCellMovementCosts(next, vehicle);
					outPath << PathNode(DESCOF(current),1);
					current = next;
				}
				outPath.EraseLast();	//dont need entry point
				outPath.Revert();
				oPath.AppendAll(outPath);
				//ShowMessage("found route with "+String(outPath.Count())+" steps, requiring "+String(iterations)+" iterations" );
				//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
				return true;
			}
			count_t ways = GetAllWays(cellID, possibilities);
			DBG_ASSERT_LESS_OR_EQUAL__(ways,ARRAYSIZE(possibilities));





			for (index_t i = 0; i < ways; i++)
			{
				if (constrainVerticalMotion && !heightRange.Contains(possibilities[i].height))
					continue;
				UINT32 option = IDOF(possibilities[i]);
				TNode&n = buffer[option];
				int cost = 1;
				if (penaltizeVerticalMotion && possibilities[i].height != cellID.height)
				{
					//cost ++;
					if (
						(possibilities[i].height > cellID.height && tn.backLink < buffer.Count() && DESCOF(tn.backLink).height < cellID.height)	//avoid vertical shafts up
						||
						(possibilities[i].height < cellID.height && tn.backLink < buffer.Count() && DESCOF(tn.backLink).height > cellID.height)	//avoid vertical shafts down
						)
						cost += 100000;
					if (possibilities[i].height > cellID.height && possibilities[i] == to)	//avoid going up to destination
						cost += 10000;
				}
				if (tn.minDistance + cost < n.minDistance)
				{
					n.minDistance = tn.minDistance + cost;
					n.backLink = (UINT32)current;
					queue.Push(option,n.minDistance);
				}
			}
		}
		//ShowMessage("no route found after "+String(iterations)+" iterations");
		//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
		return false;
	}

	
	bool AppendShortestDirectSatellitePath(const Grid::LCellIdI&from, const UINT32 to, Ctr::BasicBuffer<PathNode>&oPath)
	{
		//Timer::Time t0 = timer.Now();

		UINT32	possibilities[6];

		static const count_t cellCount = Grid::LowSetup::GridType::CellCount;

		struct TNode
		{
			UINT32	backLink, minDistance;
		};

		static Ctr::Array<TNode>	buffer(cellCount);
		TNode empty = {0xFFFFFFFF,0xFFFFFFFF};
		buffer.Fill(empty);

		Ctr::PriorityQueue<UINT32,int>	queue;
		//Queue<index_t>	queue;

		buffer[from.stackID].minDistance = 0;

		queue.Push(from.stackID,0);

		count_t iterations = 0;
		UINT32 current;
		while (queue.PopLeast(current))
		{
			iterations ++;
			if (iterations > 2000)
			{
				//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
				return false;//let's ignore long ranges
			}

			TNode&tn = buffer[current];


			if (current == to)
			{
				static Ctr::Buffer<PathNode>	outPath;
				outPath.Clear();

				while (current < cellCount)
				{
					UINT32 next = buffer[current].backLink;
					//GetCellMovementCosts(next, vehicle);
					outPath << PathNode(Grid::LCellIdI(current,from.height),1);
					current = next;
				}
				outPath.EraseLast();	//dont need entry point
				outPath.Revert();
				oPath.AppendAll(outPath);
				//ShowMessage("found route with "+String(outPath.Count())+" steps, requiring "+String(iterations)+" iterations" );
				//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
				return true;
			}
			count_t ways = GetAllSatelliteWays(current, possibilities);
			DBG_ASSERT_LESS_OR_EQUAL__(ways,ARRAYSIZE(possibilities));





			for (index_t i = 0; i < ways; i++)
			{
				UINT32 option = possibilities[i];
				TNode&n = buffer[option];
				int cost = 1;
				if (tn.minDistance + cost < n.minDistance)
				{
					n.minDistance = tn.minDistance + cost;
					n.backLink = (UINT32)current;
					queue.Push(option,n.minDistance);
				}
			}
		}
		//ShowMessage("no route found after "+String(iterations)+" iterations");
		//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
		return false;
	}


}}

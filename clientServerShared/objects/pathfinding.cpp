#include "pathfinding.h"
#include "vehicle.h"
#include "gridAndChoices.h"
#include "faction.h"

namespace Objects{ namespace Pathfinding
{


	
	#ifdef CLIENT

		bool NoHostileMobileHere(const Grid::LCellIdI&cellID, const OwnerParameter&v, float)
		{
			auto vh = GetMobileEntity(cellID);
			return !vh || vh->owner->IsFriendly(v);
		}
		bool NoForeignMobileHere(const Grid::LCellIdI&cellID, const OwnerParameter&v, float)
		{
			auto vh = GetMobileEntity(cellID);
			return !vh || vh->owner == v;
		}
	#else
		bool NoHostileMobileHere(const Grid::LCellIdI&cellID, const PFaction&v, float t)
		{
			auto vh = GetMobileEntity(cellID);
			return !vh || vh->owner->IsFriendly(v) 
					|| !v->CanSee(vh,t)
			;
		}

		bool NoForeignMobileHere(const Grid::LCellIdI&cellID, const PFaction&v, float t)
		{
			auto vh = GetMobileEntity(cellID);
			return !vh || vh->owner == v
					|| !v->CanSee(vh,t)
			;
		}
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
	bool ValidDestinationOf( const World::Data&world,const MobileEntity&v, const Grid::LCellIdI&location, const Grid::LCellIdI&finalDestination, const PHangar&finalDestinationHangar, RoutingFlags routingFlags, Component::GroundFlags groundFlags)
	{
		if (Grid::AffectedByEMP(location))
			return false;
		if (IsClear(world,location,v.owner, GetEntityTime(v)))
			return !v.IsAffectedByGravity(location) || v.CanStandOn(world,location-1,groundFlags);

		auto hangar = AccessibleHangarHere(location,v);
		if (hangar)
			if (routingFlags & RoutingFlags::AllowAccessibleHangars && (finalDestination == Grid::LCellIdI::Invalid || location == finalDestination || hangar == finalDestinationHangar))
				return true;

		return IgnoreStructuresAtDestination && finalDestination == location;
	}



	void GetWays( const World::Data&world,
							const MobileEntity*v,  const Grid::LCellIdI&location, 
							TWays&outWays, const Grid::LCellIdI&destination, const PHangar&destinationHangar,
							RoutingFlags routingFlags, Component::GroundFlags groundFlags/*=Component::GroundFlags::None*/ )
	{
		outWays.Clear();
		const auto&n = Grid::low.grid.GetCell(location.stackID).neighborhood;

		int maxWayDown = std::min(location.height + Grid::CellStack::HalfRange, v->GetMaxFallingHeight());
		if (routingFlags & RoutingFlags::ForceRevertible)
			maxWayDown = std::min(maxWayDown,v->GetMaxClimbingHeight());
		bool canWalk[6] = {true,true,true,true,true,true};

		const bool ignoreMobilesOnDestination = routingFlags & RoutingFlags::IgnoreMobilesOnDestination;

		if (v->IsPlane())
		{
			if (routingFlags & RoutingFlags::DisallowOffTerrain)
				return;

			if (WorldEntity::IsAboveTerrain(location))
			{
				for (int i = 0; i < n.numNeighbors; i++)
				{
					auto cell = Grid::LCellIdI(n.neighborID[i],location.height);

					if (cell==destination)
					{
						if (!ignoreMobilesOnDestination && !NoStationaryMobileEntityHereExcept(cell,v))
							continue;
					}
					else
					{
						if (routingFlags & RoutingFlags::IgnoreOwnMobilesOnRoute)
						{
							if (!NoForeignMobileHere(cell,v->owner,GetEntityTime(*v)))
								continue;
						}
						else
							if (!NoMobileEntityHereExcept(cell,v))
								continue;
					}
					if (ValidDestinationOf<false>(world,*v,cell, destination, destinationHangar, routingFlags, groundFlags))
						outWays.Add(cell);
				}	
				const auto p = ((Objects::SubOrbital::Plane*)v);
				int h = p->GetLandingHeight(location,*p);
				const auto candidate = Grid::LCellIdI(location.stackID,h);
				if (h != badHeight && ValidDestinationOf<false>(world,*v,candidate, destination, destinationHangar, routingFlags, groundFlags))
					outWays.Add(candidate);
				return;
			}

			const auto fine = Grid::low.grid.GetCell(location.stackID).higherEquivalent;
			const UINT32 base = Grid::CellStack::ToLayer(location.height + v->GetHeight());
			{
				const auto&stack = world.GetStack(fine);
				for (UINT32 layer = base; layer < Grid::CellStack::Range; layer++)
					if (stack.GetCellU(layer).content != Grid::Content::Clear)
						return;	//can't go up
			}
			#ifdef HYBRID_GRID
				const auto&n = Grid::high.grid.GetCell(fine).neighborhood;
				for (int k = 0; k < n.numNeighbors; k++)
				{
					const auto&stack = world.GetStack(n.neighborID[k]);
					for (UINT32 layer = base; layer < Grid::CellStack::Range; layer++)
						if (stack.GetCellU(layer).content != Grid::Content::Clear)
							return;	//can't go up
				}
			#endif
			const auto candidate = Grid::LCellIdI(location.stackID,Objects::SubOrbital::Height);
			if (ValidDestinationOf<false>(world,*v,candidate, destination, destinationHangar, routingFlags, groundFlags))
				outWays.Add(candidate);
			return;	//landed planes are limited to liftoff (if at all)
		}

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
					bool okay = cell.height <= Grid::CellStack::TopHeight && cell.height >= Grid::CellStack::BottomHeight;
					if (!okay)
						continue;
				}
				bool valid = ValidDestinationOf<false>(world,*v,cell, destination, destinationHangar, routingFlags, groundFlags);
				if (!valid)
				{
					if (j <= 0 && !world.LowCellIsClear(cell))
						canWalk[i] = false;
					continue;
				}

				if (cell==destination)
				{
					if (!ignoreMobilesOnDestination && !NoStationaryMobileEntityHereExcept(cell,v))
						continue;
				}
				else
				{
					if (routingFlags & RoutingFlags::IgnoreOwnMobilesOnRoute)
					{
						if (!NoForeignMobileHere(cell,v->owner,GetEntityTime(*v)))
							continue;
					}
					else
						if (!NoMobileEntityHereExcept(cell,v))
							continue;
				}

				//DBG_ASSERT__(!GetStructure(cell));//should be implied by LowCellIsClear (except if it's a connector, then not, so don't check this)
				outWays.Add(cell);
			}
			if (!anyCanWalk)
				break;
		}
	}


	

	static count_t GetConstructionWays( const World::Data&world, bool flat, bool canGoDownVertically, RoutingFlags routingFlags, Component::GroundFlags groundFlags,
										const MobileEntity*v,  const Grid::LCellIdI&location, 
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

				if ( ValidDestinationOf<true>(world,*v,cell, destination, nullptr,routingFlags, groundFlags) )
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

				bool okay = cell.height < Grid::CellStack::HalfRange && cell.height >= -Grid::CellStack::HalfRange && world.LowCellIsClear(cell);
				if (okay)
				{
					if (j < 0)
					{
						for (int k = cell.height + 1; k <= location.height; k++)
							if (!world.LowCellIsClear(Grid::LCellIdI(n.neighborID[i], k)))
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

				if (ValidDestinationOf<true>(world,*v,cell, destination, nullptr, routingFlags, groundFlags))
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
			if (okay && ValidDestinationOf<true>(world,*v,cell, destination, nullptr, routingFlags, groundFlags))
			{
				(*outChoices++) = cell;
				rs++;
			}
		}
		return rs;

	}




	

	void AccumulateCellMovementCosts(const World::Data&world, THindranceVector&outCount, const Grid::HCellIdI&cell,THindranceVector::ScalarType multiplier=1)
	{
		const Grid::CellStack::Cell&c = world.GetStack(cell.stackID).GetCellI(cell.height);
		switch (c.content)
		{
			case Grid::Content::Clear:
				outCount.air += multiplier;
			break;
			case Grid::Content::Water:
				outCount.water += multiplier;
			break;
			case Grid::Content::Road:
			case Grid::Content::Foundation:
				outCount.road += multiplier;
			break;
			case Grid::Content::Crystals:
			case Grid::Content::Trees:
				outCount.fewPlants += multiplier;
			break;
			case Grid::Content::Forest:
			case Grid::Content::CrystalForest:
				outCount.manyPlants += multiplier;
			break;
			default:
				outCount.other += multiplier;
		}
	}


	int GetCellMovementCosts( const World::Data&world, const Grid::LCellIdI&cell, const THindranceVector&hindrance )
	{
		#ifdef HYBRID_GRID
			const UINT32 high = Grid::low.grid.GetCell(cell.stackID).higherEquivalent;
			const auto&n = Grid::high.grid.GetCell(high).neighborhood;
		#else
			const UINT32 high = cell.cellID;
		#endif


		THindranceVector count;
		

		AccumulateCellMovementCosts(world, count, Grid::HCellIdI(high,cell.height-1),2);
		#ifdef HYBRID_GRID
			for (int i = 0; i < n.numNeighbors; i++)
				AccumulateCellMovementCosts(world, count, Grid::HCellIdI(n.neighborID[i],cell.height-1));
		#endif

		return M::Round(float((hindrance * count).Sum()) / count.Sum());
	}


	Result ShortestPath::SelectiveUpdate( const MobileEntity&m, const Grid::LCellIdI&from, const Grid::LCellIdI&to, 
							RoutingFlags routingFlags, Component::GroundFlags groundFlags /*= Component::GroundFlags::None*/,
							bool*outWouldFailDueToLimitReached/*=nullptr*/, FCellCostModifier cellCostModifier /*= nullptr*/)
	{
		if (routingFlags & RoutingFlags::IgnoreOwnMobilesOnRoute)
			return Update(m,from,to,routingFlags,groundFlags,outWouldFailDueToLimitReached,cellCostModifier);

		routeIgnore.Clear();
		Result hasI = Update(m,from,to, routingFlags | RoutingFlags::IgnoreOwnMobilesOnRoute, groundFlags,outWouldFailDueToLimitReached, cellCostModifier);
		if (hasI != Result::DestinationReached)
			return hasI;
		routeIgnore.swap(result);
		Result has = Update(m,from,to,routingFlags, groundFlags ,outWouldFailDueToLimitReached, cellCostModifier);
		if (has != Result::DestinationReached)
		{
			swap(routeIgnore, result);
			return Result::DestinationReached;
		}

		count_t lenI = 0, len = 0;
		foreach (routeIgnore,n)
			lenI += n->movementCostsFromPrevNode;
		foreach (result,n)
			len += n->movementCostsFromPrevNode;
		if (len < lenI * 4)	//less than 400% more
		{
			return Result::DestinationReached;
		}
		swap(routeIgnore, result);	//else: use the one that ignores friendly vehicles
		return Result::DestinationReached;
	}





	ShortestPath	shortestPath;



	Result ShortestPath::Update( const MobileEntity&m, const Grid::LCellIdI&from, const Grid::LCellIdI&to,  
		RoutingFlags routingFlags, Component::GroundFlags groundFlags /*= Component::GroundFlags::None*/,
		bool*outWouldFailDueToLimitReached/*=nullptr*/, FCellCostModifier cellCostModifier /*= nullptr*/)
	{
		result.Clear();
		if (outWouldFailDueToLimitReached)
			(*outWouldFailDueToLimitReached) = false;

		const World::Data&world = 
		#ifdef CLIENT
			World::actual;
		#else
			m.owner->knownWorld;
		#endif
		
		const auto destHangar = AccessibleHangarHere(to,m);

		if (!ValidDestinationOf<false>(world,m,to,to,destHangar,routingFlags, groundFlags))
			return Result::InvalidDestination;
		const THindranceVector hindrance = m.GetGroundHindrance();
		TWays	possibilities;



		TNode empty = {Grid::LCellIdI::Invalid,0xFFFFFFFF,0};
		buffer.Fill(empty);

		PriorityQueue<Grid::LCellIdI,int>	queue;


		buffer[from].minDistance = 0;

		queue.Push( from,0);

		count_t iterations = 0;
		Grid::LCellIdI current;
		while (queue.PopLeast(current))
		{
			iterations ++;
			if (iterations > 2000)
			{
				if (routingFlags & RoutingFlags::IgnoreLimit)	//client sided operation may choose to not implement maximum ranges
				{
					if (outWouldFailDueToLimitReached)
						(*outWouldFailDueToLimitReached) = true;
				}
				else
					return Result::InsufficientIterations;
			}

			const Grid::LCellIdI cellID = current;
			TNode&tn = buffer[current];
			if (!tn.nodeCosts)
			{
				tn.nodeCosts = m.IsAffectedByGravity(cellID) ? GetCellMovementCosts(world,cellID,hindrance) : 1;
				if (cellCostModifier)
				{
					cellCostModifier(cellID, tn.nodeCosts);
					if (tn.nodeCosts <= 0)
					{
						tn.nodeCosts = -1;
						continue;
					}
				}
			}
			elif (tn.nodeCosts == -1)
				continue;


			if (cellID == to)
			{
				CompleteRouting(current, routingFlags & RoutingFlags::IncludeOrigin);
				return Result::DestinationReached;
			}
			GetWays(world,&m,  cellID, possibilities, to, destHangar, routingFlags, groundFlags);


			for (index_t i = 0; i < possibilities.numWays; i++)
			{
				const auto option = possibilities.way[i];
				TNode&n = buffer[option];
				if (!n.nodeCosts)
				{
					n.nodeCosts = GetCellMovementCosts(world, possibilities.way[i], hindrance);
					if (cellCostModifier)
					{
						cellCostModifier(possibilities.way[i], n.nodeCosts);
						if (n.nodeCosts <= 0)
						{
							n.nodeCosts = -1;
							continue;
						}
					}
				}
				elif (n.nodeCosts == -1)
					continue;

				int cost = std::max(tn.nodeCosts,n.nodeCosts);
				int hCosts = std::max(0, possibilities.way[i].height - cellID.height );
				cost += hCosts;
				if (tn.minDistance + cost < n.minDistance)
				{
					n.minDistance = tn.minDistance + cost;
					n.backLink = current;
					queue.Push(option,n.minDistance);
				}
			}
		}
		return Result::NoPath;
	}


	/*
	bool	ReachabilityMapper::CheckIsBlocked(const Grid::LCellIdI&cell, const FBlockTest&isBlocked)
	{
		if (!isBlocked)
			return false;

		auto&b = buffer[cell];
		if (b.blockedTested)
			return b.isBlocked;
		b.blockedTested = true;
		b.isBlocked = isBlocked(cell);
		return b.isBlocked;
	}
	*/

	void	ReachabilityMapper::CheckNodeCosts( TReachableNode&tn, const World::Data&world, const MobileEntity&m, const Grid::LCellIdI&cellID, 
		const FCellCostModifier&costModifier, const THindranceVector&hindrance, RoutingFlags routingFlags )
	{
		if (!tn.nodeCosts)
		{
			if (!m.IsAffectedByGravity(cellID) || routingFlags & RoutingFlags::EdgeCostIsAlwaysOne)
				tn.nodeCosts = 1;
			else
				tn.nodeCosts = GetCellMovementCosts(world,cellID,hindrance);
			tn.unmodifiedCosts = tn.nodeCosts;
			if (costModifier)
			{
				costModifier(cellID,tn.nodeCosts);
				if (tn.nodeCosts <= 0)
				{
					tn.isBlocked = true;
					tn.nodeCosts = std::numeric_limits<int>::max();
				}
			}
		}
	}

	void ReachabilityMapper::Map( const MobileEntity&m, const Grid::LCellIdI&from, RoutingFlags routingFlags, Component::GroundFlags groundFlags, UINT32 maxRange/*=0xFFFFFFFF*/,  const FCellCostModifier&costModifier/*=nullptr*/)
	{
		ASSERT1__(m.owner,m.GetName());

		const World::Data&world = 
		#ifdef CLIENT
			World::actual;
		#else
			m.owner->knownWorld;
		#endif
		//Timer::Time t0 = timer.Now();
		const THindranceVector hindrance = m.GetGroundHindrance();
		const int speed = m.GetMovementStepsPerRound();
		TWays	possibilities;



		buffer.Fill(TReachableNode());

		PriorityQueue<Grid::LCellIdI,int>	queue;

		buffer[from].minDistance = 0;

		queue.Push(from,0);

		touched.Clear();


		count_t iterations = 0;
		Grid::LCellIdI current;
		while (queue.PopLeast(current))
		{
			iterations ++;
			static bool ignoreIterations = false;	//runtime switch
			if (!ignoreIterations && iterations > 1998)
			{
				//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
				break;
			}

			const Grid::LCellIdI cellID(current);
			TReachableNode&tn = buffer[current];
			CheckNodeCosts(tn, world,m, cellID,costModifier,hindrance,routingFlags);
			if (tn.isBlocked)
				continue;

			touched << current;

			GetWays(world,&m,  cellID, possibilities, Grid::LCellIdI::Invalid, nullptr, routingFlags, groundFlags);





			for (index_t i = 0; i < possibilities.numWays; i++)
			{
				const auto option = possibilities.way[i];
				TReachableNode&n = buffer[option];
				CheckNodeCosts(n, world,m,option,costModifier,hindrance,routingFlags);
				if (n.isBlocked)
					continue;

				int cost = std::max(tn.nodeCosts,n.nodeCosts);
				int hCosts = std::max(0, possibilities.way[i].height - cellID.height );
				cost += hCosts;
				if (tn.minDistance + cost < n.minDistance)
				{
					n.minDistance = tn.minDistance + cost;
					n.unmodifiedDistance = tn.unmodifiedDistance + std::max(tn.unmodifiedCosts,n.unmodifiedCosts);
					n.backLink = current;
					if (n.unmodifiedDistance < maxRange)
						queue.Push(option,n.minDistance);
				}
			}
		}



		result.clear();
		foreach (touched,t)
		{
			TReachableNode&n = buffer[*t];
			if (n.mapped!=InvalidIndex || //already been here
				(!n.minDistance && !(routingFlags & RoutingFlags::IncludeOrigin)))	//ignore first, and duplicates
				continue;

			n.mapped = result.Count();
			result << MapNode(*t,n.minDistance);
		}

		foreach (touched,t)
		{
			TReachableNode&n = buffer[*t];
			if (n.mapped != InvalidIndex)
			{
				if (n.backLink != Grid::LCellIdI::Invalid)
				{
					TReachableNode&pre = buffer[n.backLink];
					result[n.mapped].predecessor = pre.mapped;
					if (pre.mapped == InvalidIndex)
						ASSERT__(n.backLink == from);
				}
				else
					ASSERT__(*t == from);
			}
		}
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


	void	ShortestPath::CompleteRouting(const Grid::LCellIdI&dest, bool includeOrigin)
	{
		result.Clear();
		auto current = dest;
		while (current != Grid::LCellIdI::Invalid)
		{
			const auto next = buffer[current].backLink;
			const bool nextIsValid = next != Grid::LCellIdI::Invalid;
			const int nextCosts = nextIsValid ? buffer[next].nodeCosts : 0;
			const int hCosts = nextIsValid ?  std::max(0, current.height - next.height ) : 0;
			result << PathNode(current, std::max(nextCosts, buffer[current].nodeCosts) + hCosts);
			current = next;
		}
		if (!includeOrigin)
			result.EraseLast();
		result.Revert();
	}

	
	Result	ShortestPath::UpdateConstructionPath(
		const MobileEntity&mobile, bool flatPath, const Grid::LCellIdI&from, Grid::LCellIdI to, RoutingFlags routingFlags, Component::GroundFlags groundFlags, const count_t maxIterations /*= 500*/)
	{
		result.Clear();
		if (flatPath)
		{
			to.height = from.height;
			groundFlags |= Component::GroundFlags::ForceFloatOnEverything;
		}
		if (from == to)
			return Result::DestinationReached;
		const World::Data&world = 
		#ifdef CLIENT
			World::actual;
		#else
			mobile.owner->knownWorld;
		#endif
		if (!flatPath && mobile.IsAffectedByGravity(to) && !mobile.CanStandOn(world,to -1,groundFlags))
			return Result::NoPath;

		Grid::LCellIdI	possibilities[6 * 3 + 2];	//all diagonals plus up and down

	

		TNode empty = {Grid::LCellIdI::Invalid,0xFFFFFFFF};
		buffer.Fill(empty);

		PriorityQueue<Grid::LCellIdI,int>	queue;
		
		buffer[from].minDistance = 0;

		queue.Push(from,0);

		count_t iterations = 0;
		Grid::LCellIdI current;
		while (queue.PopLeast(current))
		{
			iterations ++;
			if (iterations > maxIterations)
			{
				//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
				return Result::InsufficientIterations;//let's ignore long ranges
			}

			const Grid::LCellIdI cellID(current);
			TNode&tn = buffer[current];

			if (cellID == to)
			{
				if (!world.LowCellIsClear(to))
				{
					current = buffer[current].backLink;	
				}
				CompleteRouting(current, routingFlags & RoutingFlags::IncludeOrigin);
				return Result::DestinationReached;
			}
			count_t ways = GetConstructionWays(world, flatPath, cellID == from, routingFlags, groundFlags, &mobile, cellID, possibilities, to);
			ASSERT_LESS_OR_EQUAL__(ways,ARRAYSIZE(possibilities));





			for (index_t i = 0; i < ways; i++)
			{
				const auto&p = possibilities[i];
				const auto option = p;
				TNode&n = buffer[option];
				int cost = possibilities[i].stackID != cellID.stackID ? 1 : 0;
				int hCosts = abs(possibilities[i].height - cellID.height );
				cost += hCosts;

				if (tn.minDistance + cost < n.minDistance)
				{
					const auto old = n.minDistance;
					n.minDistance = tn.minDistance + cost + EstimateMinimumRemainingCost(possibilities[i],to, 1, 1);
					n.backLink = current;


					if (!queue.AlterPriority(option,old,n.minDistance))
					{
						queue.Push(option,n.minDistance);
					}
				}
			}
		}
		return Result::NoPath;
	}

	
	bool AppendShortestDirectPath(const Grid::LCellIdI&from, const Grid::LCellIdI&to, Ctr::BasicVector<PathNode>&oPath, bool penaltizeVerticalMotion/*=true*/, bool constrainVerticalMotion/*=true*/)
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
				static Ctr::Vector<PathNode>	outPath;
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

	
	bool AppendShortestDirectSatellitePath(const Grid::LCellIdI&from, const UINT32 to, Ctr::BasicVector<PathNode>&oPath)
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
				static Ctr::Vector<PathNode>	outPath;
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

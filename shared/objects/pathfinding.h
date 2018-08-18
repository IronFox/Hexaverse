#pragma once

#include "../grid/stack.h"
#include "mobileEntity.h"
#include "volumeGrid.h"

namespace Objects
{
	/**
	Everything involving pathfinding of mobile entities
	*/
	namespace Pathfinding
	{
		template <typename T, count_t MaxLevels>
			struct TWaysTemplate
			{
				static const count_t	MaxWays = 6 * MaxLevels;

				T						way[MaxWays];
				count_t					numWays = 0;

				void					Clear()	{numWays = 0;}
				void					Add(const T&way)
				{
					DBG_ASSERT_LESS__(numWays,MaxWays);
					this->way[numWays] = way;
					numWays++;
				}
			};

		/*
		Currently there is no way that, given a specific location, a vehicle can move to two locations above each other in one step.
		So MaxLevels can stay at 1 for TWays, but must be increased to the max number of levels any vehicle can climb up, should that
		be ever more than 1.
		It must also be increased if vehicle types can ever float freely.
		*/
		typedef TWaysTemplate<Grid::LCellIdI,2>	TWays;	//TWays MaxLevels must always be >= 1
		

		
		enum class RoutingFlags
		{
			None = 0x0,
			IgnoreMobilesOnDestination = 0x1,	//!< If any (stationary) mobile is found on final destination, route there anyways. Moving mobiles on destination are always ignored
			IgnoreOwnMobilesOnRoute = 0x2,		//!< Assume my own mobile entities are temporary obstructions and route through
			ForceRevertible = 0x4,				//!< Only assume steps that can be taken in either direction
			IgnoreLimit = 0x8,					//!< Disables the hardcoded step limit to avoid server overload
			AllowAccessibleHangars = 0x10,		//!< Allows accessible hangars on destination
			IncludeOrigin = 0x20,				//!< Includes the origin location into the result
			DisallowOffTerrain = 0x40,			//!< Disallows locations above the terrain
		};


		inline RoutingFlags operator|(RoutingFlags a, RoutingFlags b)
		{
			typedef std::underlying_type<RoutingFlags>::type U;
			return RoutingFlags( U(a) | U(b) );
		}
		inline RoutingFlags operator|=(RoutingFlags&a, RoutingFlags b)
		{
			a = a | b;
			return a;
		}
		inline bool operator&(RoutingFlags a, RoutingFlags b)
		{
			typedef std::underlying_type<RoutingFlags>::type U;
			return ( U(a) & U(b) ) != 0;
		}



		void			GetWays(const World::Data&world,
								const MobileEntity*v,  const Grid::LCellIdI&location, 
								TWays&outWays, const Grid::LCellIdI&destination, const PHangar&destinationHangar,
								RoutingFlags routingFlags, Component::GroundFlags groundFlags=Component::GroundFlags::None);


		enum class Result
		{
			DestinationReached,
			InsufficientIterations,
			NoPath,
			InvalidDestination
		};


		typedef std::function<void(const Grid::LCellIdI&, int&)> FCellCostModifier;

		inline bool		Okay(Result r)	{return r == Result::DestinationReached;}

		/**
		Container structure to calculate the shortest path respecting the particular limitations of the given vehicle.
		Thread safety:
		Should not cause crashes when used asynchronously (one thread per container).
		Inconsistencies may occur if entities or grid are changed during execution.
		*/
		class ShortestPath
		{
		public:
			/**
			Resulting path (if any), not including the origin location
			*/
			Buffer<PathNode>	result;

			/**
			Updates @a result.
			@param mobile Mobile entity to plot for
			@param from Location to plot from (excluded from result)
			@param to Location to plot to (included in result if found)
			@param routingFlags Parameters affecting routing
			@param groundFlags Parameters affecting where the mobile entity can stand
			@param failIfLimitReached Set true to apply server-side work limit of 2000 routing steps (radially, following Dijkstra), false to take until done or no ways left
			@param[out] outWouldFailDueToLimitReached Set if failIfLimitReached is set false, and the limit has been reached (and ignored)
			@param cellCostModifer Custom cell modifier function, that receives the input cell ID, and determined terrain costs, and can alter this value. Must not be set negative
			@return Routing result
			*/
			Result Update(const MobileEntity&mobile, const Grid::LCellIdI&from, const Grid::LCellIdI&to, 
				RoutingFlags routingFlags, Component::GroundFlags groundFlags = Component::GroundFlags::None, bool*outWouldFailDueToLimitReached = nullptr, FCellCostModifier cellCostModifer = nullptr);


			/**
			Smart update function that tries to determine friendly vehicle ignorance by comparing the two options.
			If the route without ignorance is less than 4x longer than the one with ignorance, the non-ignoring will be copied to result.
			Otherwise the ignoring will be copied
			*/
			Result SelectiveUpdate(const MobileEntity&mobile, const Grid::LCellIdI&from, const Grid::LCellIdI&to, 
				RoutingFlags routingFlags, Component::GroundFlags groundFlags = Component::GroundFlags::None, bool*outWouldFailDueToLimitReached = nullptr, FCellCostModifier cellCostModifer = nullptr);


			/**
			Sets the local result to the shortest construction path
			@param mobile Mobile entity to plot for
			@param flatPath Avoid height changes along the path. Destination height is forcefully set to origin height
			@param from Origin location
			@param to Destination location. If @a flatPath is set, to.height is ignored
			@param routingFlags Routing parameters
			@param groundFlags Flags to determine where the mobile entity can stand
			*/
			Result	UpdateConstructionPath(const MobileEntity&mobile, bool flatPath, const Grid::LCellIdI&from, Grid::LCellIdI to, 
				RoutingFlags routingFlags, Component::GroundFlags groundFlags, const count_t maxIterations = 500);

		private:
			struct TNode
			{
				Grid::LCellIdI	backLink;
				UINT32	minDistance;
				int		nodeCosts;
			};
			VolumeGrid<TNode>	buffer;
			Buffer0<PathNode>	routeIgnore;

			void				CompleteRouting(const Grid::LCellIdI&dest, bool includeOrigin);
		};

		extern ShortestPath	shortestPath;


		bool			AppendShortestPathI(const Vehicle*vehicle, const Grid::LCellIdI&from, const Grid::LCellIdI&to, Ctr::BasicBuffer<PathNode>&oPath, bool failIfLimitReached=true, bool*outWouldFailDueToLimitReached=nullptr);
		#ifndef NO_SATELLITES
			bool		AppendShortestPathI(const Satellite*vehicle, const Grid::LCellIdI&from, const UINT32 toCell, BasicBuffer<PathNode>&oPath, bool failIfLimitReached=true, bool*outWouldFailDueToLimitReached=nullptr);
		#endif
		/**
		Attempts to find the shortest direct path to a destination. Does not check if the path is blocked by terrain or entities.
		@param from Source location
		@param to Destination location
		@param oPath Resulting path. The passed path is not cleared before appending new elements
		@param penaltizeVerticalMotion Increases the cost of vertical motion considerably, such that they are chosen only as a last resourt
		@param constrainVerticalMotion Only chooses path nodes whoes height lies in the height range defined by @a from.height and @a to.height. If @a from and @a to lie on the same height, then path finding is restricted to 2d
		*/
		bool			AppendShortestDirectPath(const Grid::LCellIdI&from, const Grid::LCellIdI&to, Ctr::BasicBuffer<PathNode>&oPath, bool penaltizeVerticalMotion=true, bool constrainVerticalMotion=true);
		#ifndef NO_SATELLITES
			bool		AppendShortestDirectSatellitePath(const Grid::LCellIdI&from, const UINT32 toCell, BasicBuffer<PathNode>&oPath);
		#endif


		class ReachabilityMapper
		{
		public:

			struct MapNode
			{
				Grid::LCellIdI	cell;
				int				movementCostsFromRoot=0;
				#ifdef CLIENT
					int			customScore=-1;	//!< Custom variable usable by client
				#endif
				index_t			predecessor=InvalidIndex;

				MapNode()	{}
				MapNode(const Grid::LCellIdI&cell, int costs) : cell(cell),movementCostsFromRoot(costs)	{}

			};
		
			Buffer<MapNode>	result;


			typedef std::function<bool(const Grid::LCellIdI&)>FBlockTest;

			/**/			ReachabilityMapper(bool logPerformance) :logPerformance(logPerformance)	{}
		
			/**
			@brief Maps all reachable cells of the given mobile entity from the specified location.
			Uses known world on client, and client-specific world (derived from the entity parameter) on server.
			Clears this->result before filling it with the new outcome
			@param vehicle Vehicle to use for reference. Must not be NULL. Must have a non-NULL owner
			@param from Specifies the starting location
			@param isBlocked Function callback to determine whether units can move to a given cell. Can be empty, considering all cells non-blocked
			*/
			void			Map(const MobileEntity&vehicle, const Grid::LCellIdI&from, FBlockTest isBlocked, RoutingFlags routingFlags, Component::GroundFlags groundFlags = Component::GroundFlags::None, UINT32 maxRange=0xFFFFFFFF);

		private:
			struct TReachableNode
			{
				Grid::LCellIdI backLink;
				UINT32	minDistance;
				int		nodeCosts;
				bool	isBlocked, blockedTested;
				index_t	mapped;
			};
			bool					logPerformance;
			Buffer<Grid::LCellIdI>		touched;
			VolumeGrid<TReachableNode>	buffer;

			bool					CheckIsBlocked(const Grid::LCellIdI&cell, const FBlockTest&isBlocked);
			void					CheckNodeCosts(TReachableNode&tn, const World::Data&world, const MobileEntity&m, const Grid::LCellIdI&cellID, const FBlockTest&isBlocked, const THindranceVector&vec);
		};



		/**
		Must be called once during start up, after the base grids have been created.
		Updates maxAngleBetweenNeighboringCells, which is required by AppendShortestConstructionPath()
		*/
		void Init();
		


	}
}


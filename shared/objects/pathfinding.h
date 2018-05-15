#pragma once

#include "../grid/stack.h"
#include "mobileEntity.h"

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
		typedef TWaysTemplate<UINT32,1>			TOrbitalWays;	//TOrbitalWays MaxLevels must always be == 1




		void			GetWays(const World::Data&world,
								const Vehicle*v,  const Grid::LCellIdI&location, 
								TWays&outWays, const Grid::LCellIdI&destination, const PHangar&destinationHangar,
								bool ignoreVehiclesOnDestination, bool ignoreFriendlyVehiclesOnRoute, bool forceRevertible);
		#ifndef NO_SATELLITES
			void		GetWays(const Satellite*v,  UINT32 location, 
								TOrbitalWays&outWays, UINT32 destination, 
								bool ignoreVehiclesOnDestination, bool ignoreFriendlyVehiclesOnRoute);
		#endif
		void			GetMobileWays(	const World::Data&world,
								const MobileEntity*v,  const Grid::LCellIdI&location,
								TWays&outWays, const Grid::LCellIdI&destination, const PHangar&destinationHangar, 
								bool ignoreVehiclesOnDestination, bool ignoreFriendlyVehiclesOnRoute);



		enum class Result
		{
			DestinationReached,
			InsufficientIterations,
			NoPath
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
			@param vehicle Vehicle to plot for
			@param from Location to plot from (excluded from result)
			@param to Location to plot to (included in result if found)
			@param ignoreFriendlyVehiclesOnRoute Set true to ignore friendly vehicles, false to route around them
			@param failIfLimitReached Set true to apply server-side work limit of 2000 routing steps (radially, following Dijkstra), false to take until done or no ways left
			@param[out] outWouldFailDueToLimitReached Set if failIfLimitReached is set false, and the limit has been reached (and ignored)
			@param cellCostModifer Custom cell modifier function, that receives the input cell ID, and determined terrain costs, and can alter this value. Must not be set negative
			@return True if the destination was reached, false otherwise
			*/
			bool Update(const Vehicle&vehicle, const Grid::LCellIdI&from,
				const Grid::LCellIdI&to, 
				bool ignoreFriendlyVehiclesOnRoute, bool failIfLimitReached = true, bool*outWouldFailDueToLimitReached = nullptr, FCellCostModifier cellCostModifer = nullptr);


			/**
			Smart update function that tries to determine friendly vehicle ignorance by comparing the two options.
			If the route without ignorance is less than 4x longer than the one with ignorance, the non-ignoring will be copied to result.
			Otherwise the ignoring will be copied
			*/
			bool SelectiveUpdate(const Vehicle&vehicle, const Grid::LCellIdI&from,
				const Grid::LCellIdI&to,
				bool failIfLimitReached = true, bool*outWouldFailDueToLimitReached = nullptr, FCellCostModifier cellCostModifer = nullptr);

		private:
			struct TNode
			{
				UINT32	backLink, minDistance;
				int		nodeCosts;
			};
			Array<TNode>	buffer;
			Buffer0<PathNode>	routeIgnore;
		};

		extern ShortestPath	shortestPath;


		bool			AppendShortestPathI(const Vehicle*vehicle, const Grid::LCellIdI&from, const Grid::LCellIdI&to, Ctr::BasicBuffer<PathNode>&oPath, bool failIfLimitReached=true, bool*outWouldFailDueToLimitReached=nullptr);
		#ifndef NO_SATELLITES
			bool		AppendShortestPathI(const Satellite*vehicle, const Grid::LCellIdI&from, const UINT32 toCell, BasicBuffer<PathNode>&oPath, bool failIfLimitReached=true, bool*outWouldFailDueToLimitReached=nullptr);
		#endif
		Result			AppendShortestConstructionPath(const Vehicle*vehicle, bool ignoreWater, bool flat, const Grid::LCellIdI&from, Grid::LCellIdI to, Ctr::BasicBuffer<Grid::LCellIdI>&oPath, const count_t maxIterations = 500);
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

			/**/			ReachabilityMapper(bool logPerformance, bool cacheIsBlocked=true) :logPerformance(logPerformance),cacheIsBlocked(cacheIsBlocked)	{}
		
			/**
			@brief Maps all reachable cells of the given vehicle from the specified location.
			Uses known world on client, and client-specific world (derived from the vehicle parameter) on server.
			Clears this->result before filling it with the new outcome
			@param vehicle Vehicle to use for reference. Must not be NULL. Must have a non-NULL owner
			@param from Specifies the starting location
			@param isBlocked Function callback to determine whether units can move to a given cell. Can be empty, considering all cells non-blocked
			@param forceRevertible Forces that only steps are considered that are possible in both directions
			*/
			void			Map(const Vehicle*vehicle, const Grid::LCellIdI&from, FBlockTest isBlocked, UINT32 maxRange=0xFFFFFFFF, bool forceRevertible=false, bool allowAccessibleHangars=false, bool ignoreFriendlyVehiclesOnRoute=false);



		private:
			struct TReachableNode
			{
				UINT32	backLink, minDistance;
				int		nodeCosts;
				bool	isBlocked;
				index_t	mapped;
			};
			bool					logPerformance,cacheIsBlocked;
			Buffer<index_t>			touched;
			Array<TReachableNode>	buffer;
			HashTable<Grid::LCellIdI,bool>	blockedCache;

			bool					CheckIsBlocked(const Grid::LCellIdI&cell, const FBlockTest&isBlocked);
		};



		/**
		Must be called once during start up, after the base grids have been created.
		Updates maxAngleBetweenNeighboringCells, which is required by AppendShortestConstructionPath()
		*/
		void Init();
		


	}
}


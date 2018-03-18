#pragma once

#include "../grid/stack.h"
#include "mobileEntity.h"
#include "subOrbital.h"

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
		typedef TWaysTemplate<UINT32,1>			TOrbitalWays,TSubOrbitalWays;	//TOrbitalWays MaxLevels must always be == 1




		void			GetWays(const World::Data&world,
								const Vehicle*v,  const Grid::LCellIdI&location, 
								TWays&outWays, const Grid::LCellIdI&destination, 
								bool ignoreVehiclesOnDestination, bool ignoreFriendlyVehiclesOnRoute, bool forceRevertible);
		#ifndef NO_SATELLITES
			void		GetWays(const Satellite*v,  UINT32 location, 
								TOrbitalWays&outWays, UINT32 destination, 
								bool ignoreVehiclesOnDestination, bool ignoreFriendlyVehiclesOnRoute);
		#endif
		void			GetMobileWays(	const World::Data&world,
								const MobileEntity*v,  const Grid::LCellIdI&location, 
								TWays&outWays, const Grid::LCellIdI&destination, 
								bool ignoreVehiclesOnDestination, bool ignoreFriendlyVehiclesOnRoute);



		enum class Result
		{
			DestinationReached,
			InsufficientIterations,
			NoPath
		};

		inline bool		Okay(Result r)	{return r == Result::DestinationReached;}

		bool			AppendShortestPathI(const Vehicle*vehicle, const Grid::LCellIdI&from, const Grid::LCellIdI&to, Ctr::BasicBuffer<PathNode>&oPath, bool failIfLimitReached=true, bool*outWouldFailDueToLimitReached=nullptr);
		#ifndef NO_SATELLITES
			bool		AppendShortestPathI(const Satellite*vehicle, const Grid::LCellIdI&from, const UINT32 toCell, BasicBuffer<PathNode>&oPath, bool failIfLimitReached=true, bool*outWouldFailDueToLimitReached=nullptr);
		#endif
		Result			AppendShortestConstructionPath(const Vehicle*vehicle, bool ignoreWater, bool flat, const Grid::LCellIdI&from, Grid::LCellIdI to, Ctr::BasicBuffer<Grid::LCellIdI>&oPath, const count_t maxIterations = 2000);
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


		namespace SubOrbital
		{
			using namespace ::Objects::SubOrbital;


			void GetWays(		const Plane*v,  UINT32 location, 
								TSubOrbitalWays&outWays, UINT32 destination, 
								bool ignoreVehiclesOnDestination, bool ignoreFriendlyVehiclesOnRoute);

			bool AppendShortestPath(const Plane*sat, const Grid::LCellIdI&from,
									const Grid::LCellIdI&to, BasicBuffer<PathNode>&oPath,
									bool ignoreFriendlyVehiclesOnRoute, bool failIfLimitReached=true, bool*outWouldFailDueToLimitReached=nullptr);

			bool AppendShortestPathI(const Plane*sat, const Grid::LCellIdI&from,
									const Grid::LCellIdI&to, BasicBuffer<PathNode>&oPath, bool failIfLimitReached=true, bool*outWouldFailDueToLimitReached=nullptr);
			bool AppendShortestDirectPath(const Grid::LCellIdI&from, const UINT32 to, BasicBuffer<PathNode>&oPath);
		}
	


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

			typedef bool (*FBlockTest)(const Grid::LCellIdI&);


			/**/			ReachabilityMapper(bool logPerformance) :logPerformance(logPerformance)	{}
		
			/**
			@brief Maps all server-reachable cells of the given vehicle from the specified location
			@param vehicle Vehicle to use for reference. Must not be NULL
			@param from Specifies the starting location
			@param isBlocked Function callback to determine whether units can move to a given cell. Can be empty, considering all cells non-blocked
			@param forceRevertible Forces that only steps are considered that are possible in both directions
			*/
			void			Map(const Vehicle*vehicle, const Grid::LCellIdI&from, FBlockTest isBlocked, UINT32 maxRange=0xFFFFFFFF, bool forceRevertible=false);



		private:
			struct TReachableNode
			{
				UINT32	backLink, minDistance;
				int		nodeCosts;
				bool	isBlocked;
				index_t	mapped;
			};
			bool					logPerformance;
			Buffer<index_t>			touched;
			Array<TReachableNode>	buffer;
		};



		/**
		Must be called once during start up, after the base grids have been created.
		Updates maxAngleBetweenNeighboringCells, which is required by AppendShortestConstructionPath()
		*/
		void Init();
		


	}
}


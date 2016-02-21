#ifndef objectGridAndChoicesH
#define objectGridAndChoicesH
#include "structure.h"
//#include "vehicle.h"
#include "types.h"
#include "mobileEntity.h"

namespace Objects
{

	namespace Times
	{
		static const float Fall = 0.1f;
		static const float Dig = 0.5f;
		static const float Move = 0.75f;	//movement phase in seconds
		static const float Attack = 0.025f;
		static const float TurretRotation = 0.5f;
	}

	static const count_t MaxStructureTasks = 16;
	static const count_t MaxProductionTasks = MaxStructureTasks,
						MaxFocusPoints = 64;
	static const count_t MaxFocusCount = 64;






	struct TStructureNeighborhood
	{
		PStructure		entry[6*3];
		count_t			numEntries = 0;

		typedef const PStructure*	const_iterator;
		const_iterator	begin() const {return entry;}
		const_iterator	end() const {return entry + numEntries;}

		void			Append(const PStructure&s)
		{
			DBG_ASSERT_LESS__(numEntries,ARRAYSIZE(entry));
			entry[numEntries++] = s;
		}
	};


	/**
	Removes all entities from their respective grids.
	Does not access lists, only grid
	*/
	void			FlushGrids();

	index_t			CellToGridIndex(const Grid::LCellIdU&location);
	/**
	Logs allocated size of entity grids
	*/
	void			LogGridSizes();


	PVehicle		GetVehicle(const Grid::LCellIdI&cellID);
	PStructure		GetStructure(const Grid::LCellIdI&cellID);
	PStructure		GetStructure(const Grid::LCellIdU&cellID);
	PWorldEntity	GetEntity(const Grid::LCellIdI&cellID);
	namespace Orbital
	{
		PShip		GetShip(UINT32 flatCellID);
		PStation	GetStation(UINT32 flatCellID);
	}
	PMobileEntity	GetMobileEntity(const Grid::LCellIdI&cellID);
	PMobileEntity	GetMobileEntity(const Grid::LCellIdU&cellID);


	//void			InjectIntoNeighborhood(const PStructure&s, float atTime);

	void			GetNeighborhood( const OwnerParameter&referenceOwner, bool ignoreOwnership, const Grid::LCellIdI&cellID, TStructureNeighborhood&outNeighborhood);

	/**
	Checks if there is a structure in the given location that can handle a given mobile entity moving in
	*/
	PHangarStructure	MobileDestinationStructureHere(const Grid::LCellIdI&location, const MobileEntity&);



	bool			AppendShortestPathI(const Vehicle*vehicle, const Grid::LCellIdI&from, const Grid::LCellIdI&to, BasicBuffer<PathNode>&oPath, bool failIfLimitReached=true, bool*outWouldFailDueToLimitReached=nullptr);
	bool			AppendShortestPathI(const Orbital::Ship*vehicle, const Grid::LCellIdI&from, const UINT32 toCell, BasicBuffer<PathNode>&oPath, bool failIfLimitReached=true, bool*outWouldFailDueToLimitReached=nullptr);
	bool			AppendShortestConstructionPath(const Vehicle*vehicle, bool ignoreWater, bool flat, const Grid::LCellIdI&from, Grid::LCellIdI to, BasicBuffer<Grid::LCellIdI>&oPath);
	/**
	Attempts to find the shortest direct path to a destination. Does not check if the path is blocked by terrain or entities.
	@param from Source location
	@param to Destination location
	@param oPath Resulting path. The passed path is not cleared before appending new elements
	@param penaltizeVerticalMotion Increases the cost of vertical motion considerably, such that they are chosen only as a last resourt
	@param constrainVerticalMotion Only chooses path nodes whoes height lies in the height range defined by @a from.height and @a to.height. If @a from and @a to lie on the same height, then path finding is restricted to 2d
	*/
	bool			AppendShortestDirectPath(const Grid::LCellIdI&from, const Grid::LCellIdI&to, BasicBuffer<PathNode>&oPath, bool penaltizeVerticalMotion=true, bool constrainVerticalMotion=true);
	bool			AppendShortestDirectOrbitalPath(const Grid::LCellIdI&from, const UINT32 toCell, BasicBuffer<PathNode>&oPath);


	void			GetChoices(WorldEntity&entity, const PMobileEntity&deploying, CursorMode activeCursorMode, BasicBuffer<Grid::LCellIdI>&movementChoices, bool&showArrow, bool&hasArrowCellTarget, TVec3<>&arrowFrom, Grid::LCellIdI&arrowFromCell);
	void			GetChoices(const PWorldEntity&entity, const PMobileEntity&deploying, CursorMode activeCursorMode, BasicBuffer<Grid::LCellIdI>&movementChoices, bool&showArrow, bool&hasArrowCellTarget, TVec3<>&arrowFrom, Grid::LCellIdI&arrowFromCell);

	bool			NothingBlockingHere(const Grid::LCellIdI&cellID, bool checkVehiclesOnClient);


	inline float	ExplosionTime(float size)
					{ return std::max(1.3f,size*size/20.f);}


	static const int badHeight = 0x1FFFFFFF;
	/**
	Attempts to find the lowest possible low-resolution grid cell to put a vehicle in.
	Search starts from the specified reference cell height. If the cell is already empty, search continues down, otherwise up.
	*/
	int				FindApplicableGround(const Grid::LCellIdI&cell, bool canStandOnWater);


	/**
	Determines all spawning choices for a completed vehicle construction
	@param producer Entity that has completed the production
	@param production Base type of the vehicle that was produced
	*/
	void			GetProductionChoices(const WorldEntity*producer,Component::VehicleBase*production,
							BasicBuffer<Grid::LCellIdI>&choices);
	count_t			GetWays(const Grid::WorldData&world,
							const Vehicle*v,  const Grid::LCellIdI&location, 
							Grid::LCellIdI*outChoices, const Grid::LCellIdI&destination, 
							bool ignoreVehiclesOnDestination, bool ignoreFriendlyVehiclesOnRoute);
	count_t			GetWays(const Orbital::Ship*v,  UINT32 location, 
							UINT32*outChoices, UINT32 destination, 
							bool ignoreVehiclesOnDestination, bool ignoreFriendlyVehiclesOnRoute);
	count_t			GetMobileWays(	const Grid::WorldData&world,
							const MobileEntity*v,  const Grid::LCellIdI&location, 
							Grid::LCellIdI*outChoices, const Grid::LCellIdI&destination, 
							bool ignoreVehiclesOnDestination, bool ignoreFriendlyVehiclesOnRoute);

	bool			NoVehicleHere(const Grid::LCellIdI&cellID);
	bool			NoVehicleVisibleHereExcept(const Grid::LCellIdI&cellID, const Vehicle*);

	namespace Orbital
	{
		bool		NoShipHere(const UINT32 cellID);
		bool		NoStationHere(const UINT32 cellID);
		bool		NoStationVisibleHereExcept(UINT32 stackID, const Orbital::Station*v);
		inline bool	NothingHere(const UINT32 cellID) {return NoShipHere(cellID) && NoStationHere(cellID);}
		bool		NothingVisibleHereExcept(const UINT32 cellID, const WorldEntity*);

		void		RemoveShipFromGrid(const PShip&v,UINT32 stackID);
		void		InsertShipIntoGrid(const PShip&v,UINT32 stackID);
	}

	void			RemoveVehicleFromGrid(const PVehicle&v,const Grid::LCellIdI&location);
	void			InsertVehicleIntoGrid(const PVehicle&v,const Grid::LCellIdI&location);
	void			RemoveFromGrid(const PMobileEntity&,const Grid::LCellIdI&location);
	void			InsertIntoGrid(const PMobileEntity&,const Grid::LCellIdI&location);


	inline bool		Failed(MobilePlacementResult rs)
					{
						return rs == MobilePlacementResult::PlacementFailedHarmfulOrBlocked
							|| rs == MobilePlacementResult::PlacementFailedMobileDestroyed
							|| rs == MobilePlacementResult::PlacementFailedTemporarilyObstructed
							|| rs == MobilePlacementResult::PlacementFailedPermanentlyObstructed
							;
					}



	#ifdef CLIENT
		#ifndef NO_HIBERNATION
			extern UINT32	uplinkTraffic,
							onlineUplinkTraffic;
		#endif
		void		MoveGridVehicle(const PVehicle&v, const Grid::LCellIdI&from, const Grid::LCellIdI&to);
		PStructure	SpawnStructure(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&location,ConstName type, bool placeNow, bool isMine);
		PVehicle	SpawnVehicle(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&baseLocation,ConstName base, ConstName turret, bool placeNow, bool isMine);
		namespace Orbital
		{
			PShip	SpawnShip(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&baseLocation,ConstName type, bool placeNow, bool isMine);
		}
		/**
		Selectively spawns either a vehicle or a satellite, depending on the parameters
		In order to spawn a satellite, @a turret must be @a ConstName::Empty, and @base be a valid satellite type name
		*/
		PMobileEntity	SpawnMobileEntity(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&baseLocation,ConstName base, ConstName turret, bool placeNow, bool isMine);
		void		Place( const PStructure&s);
		void		Place( const PVehicle&vehicle, bool insertIntoGrid=true);
		void		Place( const Orbital::PShip&s);
		void		PlaceMobileEntity( const PMobileEntity&m, bool insertIntoGridIfVehicle=true);


	#else
		PVehicle	AssembleVehicle(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&baseLocation, const String&base, const String&turret);
		/**
		Selectively constructs a vehicle or a orbital ship, depending on the given configuration.
		To successfully construct a ship the specified @a turret parameter must be empty, and @a base must be a valid ship type name
		*/
		PMobileEntity	AssembleMobileEntity(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&baseLocation, const String&base, const String&turret);

		bool		GetSentryTarget(const WorldEntity&source, int munitionStorage, const Grid::LCellIdI&sourceLocation, float zOffset, const TurretBase&base, Grid::LCellIdI&outTarget);
		void		SignalClusterChange(const Grid::LCellIdI&location, int verticalTolerance,float atTime,const WorldEntity*except);
		void		SignalVehicleClusterChange(const Grid::LCellIdI&location, int verticalTolerance, float atTime, const WorldEntity*except);
		void		SignalClusterAndNeighborsChange(const Grid::LCellIdI&location, float atTime, const WorldEntity*except = nullptr);
		void		Explode(const Objects::PWorldEntity&attackFrom, const Grid::LCellIdI&traceTo, float flightTime, 
						const Grid::HCellIdI&targetCell, const TVec3<>&center, int explosionRadius, int damageLevel, bool doomsday,
						float&atTime
						);
		void		ExplodeEMP(const Objects::PWorldEntity&attackFrom, const Grid::LCellIdI&traceTo, float flightTime, 
						const Grid::HCellIdI&targetCell, const TVec3<>&center, int explosionRadius, int damageLevel, float&atTime);


		/**
		Checks that all vehicles and structures are stored in their respective grids at their current location.
		Verification failure leads to program termination.
		Executes in linear time, depending on the number of vehicles and structures. Does not test orbital units.
		*/
		void		VerifyGrid();

		/**
		Handles all operations necessary as the result of the user clicking on some visual choice
		@param e Entity to process the choice of
		@param choice World cell the user clicked on
		@param mode Current cursor mode that effects available choices
		@param deployWhat Vehicle being deployed (only if @a e points to a vehicle store)
		*/
		bool		ProcessChoiceIntersection(const PWorldEntity&e, const Grid::LCellIdI&choice,  CursorMode mode, const PMobileEntity&deployWhat);

	#endif

	PVehicle		SpawnVehicle(const OwnerParameter&owner, const Grid::LCellIdI&baseLocation,Component::VehicleBase*base, Component::Turret*turret, bool fillStorage, float&atTime);
	PVehicle		SpawnVehicle(const OwnerParameter&owner, const Grid::LCellIdI&baseLocation,Component::VehicleBase&base, Component::Turret&turret, bool fillStorage, float&atTime);
	void			InsertConstructed(const PMobileEntity&v, float&atTime, bool insertIntoGrid= true, bool setHealthToFull=true);
	void			InsertConstructed(const PStructure&s, float&atTime, bool refreshNetworkNow=false, bool setHealthToFull=true);
	void			RegisterNewOffGrid(const PMobileEntity&v, float&atTime, bool setHealthToFull=true);


	void			Remove( const PWorldEntity&, RemovalType t, float&atTime, bool controlEnabled, bool isRemovedFromGrid=false, bool ignoreIndestructible=false);

	/**
	Must be called once during start up, after the base grids have been created
	*/
	void			InitGrid();




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
		*/
		void			Map(const Vehicle*vehicle, const Grid::LCellIdI&from, FBlockTest isBlocked, UINT32 maxRange=0xFFFFFFFF);



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
}


#endif

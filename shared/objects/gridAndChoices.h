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
	static const count_t MaxProductionTasks = MaxStructureTasks;






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


	extern Array<PVehicle>			vehicleGrid;
	extern Array<PStructure>		structureGrid;

	index_t					CellToGridIndex(const Grid::LCellIdI&location);
	index_t					CellToGridIndex(const Grid::LCellIdU&location);



	PVehicle		GetVehicle(const Grid::LCellIdI&cellID);
	PStructure		GetStructure(const Grid::LCellIdI&cellID);
	PStructure		GetStructure(const Grid::LCellIdU&cellID);
	PWorldEntity	GetEntity(const Grid::LCellIdI&cellID);
	PSatellite		GetSatellite(UINT32 flatCellID);
	PMobileEntity	GetMobileEntity(const Grid::LCellIdI&cellID);
	PMobileEntity	GetMobileEntity(const Grid::LCellIdU&cellID);




	void			InjectIntoNeighborhood(const PStructure&s, float atTime);

	void			GetNeighborhood( const OwnerParameter&referenceOwner, bool ignoreOwnership, const Grid::LCellIdI&cellID, TStructureNeighborhood&outNeighborhood);

	/**
	Checks if there is a structure in the given location that can handle a given mobile entity moving in
	*/
	PHangarStructure	MobileDestinationStructureHere(const Grid::LCellIdI&location, const MobileEntity&);



	bool			AppendShortestPathI(const Vehicle*vehicle, const Grid::LCellIdI&from, const Grid::LCellIdI&to, BasicBuffer<PathNode>&oPath);
	bool			AppendShortestPathI(const Satellite*vehicle, const Grid::LCellIdI&from, const UINT32 toCell, BasicBuffer<PathNode>&oPath);
	bool			AppendShortestConstructionPath(const Vehicle*vehicle, bool ignoreWater, bool doFloat, const Grid::LCellIdI&from, Grid::LCellIdI to, BasicBuffer<Grid::LCellIdI>&oPath);
	bool			AppendShortestDirectPath(const Grid::LCellIdI&from, const Grid::LCellIdI&to, BasicBuffer<PathNode>&oPath, bool penaltizeVerticalMotion=true);
	bool			AppendShortestDirectSatellitePath(const Grid::LCellIdI&from, const UINT32 toCell, BasicBuffer<PathNode>&oPath);


	void			GetChoices(WorldEntity&entity, const PMobileEntity&deploying, CursorMode::mode_t activeCursorMode, BasicBuffer<Grid::LCellIdI>&movementChoices, bool&showArrow, bool&hasArrowCellTarget, TVec3<>&arrowFrom, Grid::LCellIdI&arrowFromCell);
	void			GetChoices(const PWorldEntity&entity, const PMobileEntity&deploying, CursorMode::mode_t activeCursorMode, BasicBuffer<Grid::LCellIdI>&movementChoices, bool&showArrow, bool&hasArrowCellTarget, TVec3<>&arrowFrom, Grid::LCellIdI&arrowFromCell);



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
	count_t			GetWays(const Satellite*v,  UINT32 location, 
							UINT32*outChoices, UINT32 destination, 
							bool ignoreVehiclesOnDestination, bool ignoreFriendlyVehiclesOnRoute);
	count_t			GetMobileWays(	const Grid::WorldData&world,
							const MobileEntity*v,  const Grid::LCellIdI&location, 
							Grid::LCellIdI*outChoices, const Grid::LCellIdI&destination, 
							bool ignoreVehiclesOnDestination, bool ignoreFriendlyVehiclesOnRoute);

	bool			NoVehicleHere(const Grid::LCellIdI&cellID);
	bool			NoVehicleHereExcept(const Grid::LCellIdI&cellID, const Vehicle*);
	bool			NoSatelliteHere(const UINT32 cellID);

	void			RemoveVehicleFromGrid(const PVehicle&v,const Grid::LCellIdI&location);
	void			InsertVehicleIntoGrid(const PVehicle&v,const Grid::LCellIdI&location);


	inline bool		Failed(MobilePlacementResult rs)	{return rs == MobilePlacementResult::PlacementFailedHarmfulOrBlocked || rs == MobilePlacementResult::PlacementFailedObstructed;}



	#ifdef CLIENT
		#ifndef NO_HIBERNATION
			extern UINT32	uplinkTraffic,
							onlineUplinkTraffic;
		#endif
		void		MoveGridVehicle(const PVehicle&v, const Grid::LCellIdI&from, const Grid::LCellIdI&to);
		PStructure	SpawnStructure(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&location,ConstName type, bool placeNow, bool isMine);
		PVehicle	SpawnVehicle(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&baseLocation,ConstName base, ConstName turret, bool placeNow, bool isMine);
		PSatellite	SpawnSatellite(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&baseLocation,ConstName type, bool placeNow, bool isMine);
		/**
		Selectively spawns either a vehicle or a satellite, depending on the parameters
		In order to spawn a satellite, @a turret must be @a ConstName::Empty, and @base be a valid satellite type name
		*/
		PMobileEntity	SpawnMobileEntity(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&baseLocation,ConstName base, ConstName turret, bool placeNow, bool isMine);
		void		Place( const PStructure&s);
		void		Place( const PVehicle&vehicle, bool insertIntoGrid=true);
		void		Place( const PSatellite&s);
		void		PlaceMobileEntity( const PMobileEntity&m, bool insertIntoGridIfVehicle=true);


		#ifdef HAS_WINDOW
			void	InsertSatelliteSensor(Satellite*);
			void	RemoveSatelliteSensor(Satellite*);
		#endif

	#else
		PVehicle	AssembleVehicle(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&baseLocation, const String&base, const String&turret);
		/**
		Selectively constructs a vehicle or a satellite, depending on the given configuration.
		To successfully construct a satellite the specified @a turret parameter must be empty, and @a base must be a valid satellite type name
		*/
		PMobileEntity	AssembleMobileEntity(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&baseLocation, const String&base, const String&turret);

		bool		GetSentryTarget(const WorldEntity&source, int munitionStorage, const Grid::LCellIdI&sourceLocation, float zOffset, const TurretBase&base, Grid::LCellIdI&outTarget);
		void		SignalClusterChange(const Grid::LCellIdI&location, int verticalTolerance,float atTime,const WorldEntity*except);
		void		SignalVehicleClusterChange(const Grid::LCellIdI&location, int verticalTolerance, float atTime, const WorldEntity*except);
		void		SignalClusterAndNeighborsChange(const Grid::LCellIdI&location, float atTime, const WorldEntity*except = nullptr);
		void		Explode(const Objects::PWorldEntity&attackFrom, const Grid::LCellIdI&traceTo, float flightTime, 
						const Grid::HCellIdI&targetCell, const TVec3<>&center, int explosionRadius, int damageLevel, bool doomsday,
						float atTime
						);
		void		ExplodeEMP(const Objects::PWorldEntity&attackFrom, const Grid::LCellIdI&traceTo, float flightTime, 
						const Grid::HCellIdI&targetCell, const TVec3<>&center, int explosionRadius, int damageLevel, float atTime);


		/**
		Checks that all vehicles and structures are stored in their respective grids at their current location.
		Verification failure leads to program termination.
		Executes in linear time, depending on the number of vehicles and structures. Does not test satellites.
		*/
		void		VerifyGrid();

		/**
		Handles all operations necessary as the result of the user clicking on some visual choice
		@param e Entity to process the choice of
		@param choice World cell the user clicked on
		@param mode Current cursor mode that effects available choices
		@param deployWhat Vehicle being deployed (only if @a e points to a vehicle store)
		*/
		bool		ProcessChoiceIntersection(const PWorldEntity&e, const Grid::LCellIdI&choice,  CursorMode::mode_t mode, const PMobileEntity&deployWhat);

	#endif

	PVehicle		SpawnVehicle(const OwnerParameter&owner, const Grid::LCellIdI&baseLocation,Component::VehicleBase*base, Component::Turret*turret, bool fillStorage, float atTime);
	PVehicle		SpawnVehicle(const OwnerParameter&owner, const Grid::LCellIdI&baseLocation,Component::VehicleBase&base, Component::Turret&turret, bool fillStorage, float atTime);
	void			InsertConstructed(const PMobileEntity&v, float atTime, bool insertIntoGrid= true, bool setHealthToFull=true);
	void			InsertConstructed(const PStructure&s, float atTime, bool refreshNetworkNow=false, bool setHealthToFull=true);
	void			RegisterNewOffGrid(const PMobileEntity&v, float atTime, bool setHealthToFull=true);


	void			Remove( const PWorldEntity&, RemovalType t, float atTime, bool controlEnabled, bool isRemovedFromGrid=false, bool ignoreIndestructible=false);

	/**
	Must be called once during start up, after the base grids have been created
	*/
	void			InitGrid();

	//void			DbgCheckSatellites(const PMobileEntity&includeIfSatellite = PMobileEntity());



	class ReachabilityMapper
	{
	public:

		struct MapNode
		{
			Grid::LCellIdI	cell;
			int				movementCostsFromRoot=0;
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

#pragma once

#include "structure.h"
#include "types.h"
#include "mobileEntity.h"
#include "volumeGrid.h"

namespace Objects
{

	namespace Times
	{
		static const float Fall = 0.1f;
		static const float Dig = 0.5f;
		static const float Move = 0.75f;	//movement phase in seconds
		static const float Attack = 0.025f;
		static const float TurretRotation = 0.5f;
		static const float BeamAttack = 0.5f;
	}

	static const count_t	MaxTasks = 16;
	static const count_t	MaxFocusPoints = 16;
	static const count_t	MaxDirectPathLength = 64;






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



	template <typename T>
		class ObjectVolumeGrid : private VolumeGrid<std::shared_ptr<T> >
		{
			typedef VolumeGrid<std::shared_ptr<T> >	Super;
		protected:
			using Super::entries;
		public:
			typedef std::shared_ptr<T>	Entry;
			using Super::CellToGridIndex;

			const Entry&	operator[](const Grid::LCellIdI&loc)	const
			{
				return Super::operator[](loc);
			}

			Super::GetStructureSize;

			void		RemoveObject(const Grid::LCellIdI&location, const Entry&object)
			{
				ASSERT__(object->isInsertedIntoGrid);

				const int h = object->GetHeight();

				for (int z = 0; z < h; z++)
				{
					index_t at = CellToGridIndex(location+z);
					if (entries[at] != object)
					{
						index_t foundAt = InvalidIndex;
						index_t cloneAt = InvalidIndex;
						Entry	cloneIs;
						for (index_t i = 0; i < entries.GetLength(); i++)
						{
							if (entries[i] == object)
							{
								ASSERT__(foundAt == InvalidIndex);
								foundAt = i;
							}
							else if (entries[i] && entries[i]->id == object->id)
							{
								cloneAt = i;
								cloneIs = entries[i];
							}
						}
						if (entries[at])
							LogEvent(EventClass::Preset::error,entries[at].get(),"Found instead of "+object->GetName());
						String clone = cloneAt != InvalidIndex ? " {found clone ("+cloneIs->GetName()+") at "+ToString(Grid::LCellIdI(cloneAt%Grid::LowSetup::GridType::CellCount,Grid::CellStack::ToHeight((UINT32)( cloneAt/Grid::LowSetup::GridType::CellCount))))+"}" : "";
						if (foundAt != InvalidIndex)
						{
							Grid::LCellIdI actualLocation(foundAt%Grid::LowSetup::GridType::CellCount,Grid::CellStack::ToHeight((UINT32)( foundAt/Grid::LowSetup::GridType::CellCount)));
							FATAL__("Trying to remove "+object->GetName()+" from location "+String(at)+"/"+ToString(location)+", but object resides in "+String(foundAt)+"/"+ToString(actualLocation)+clone);
						}
						else
							FATAL__("Trying to remove "+object->GetName()+" from location "+String(at)+"/"+ToString(location)+", but object is not mapped in the grid"+clone);
					}
					LogEvent(EventSignificance::Common,object.get(),"Removed from grid location "+String(at)+"/"+ToString(location));
					entries[at].reset();
				}
				object->isInsertedIntoGrid = false;
			}
	
			void				InsertObject(const Grid::LCellIdI&location, const Entry&object)
			{
				ASSERT1__(!object->isInsertedIntoGrid, object->GetName());
				Grid::AssertIsHorizontallyValid(location,CLOCATION);
				if (location.height != SubOrbital::Height)
					Grid::AssertIsValid(location,CLOCATION);

				const int h = object->GetHeight();
				if (h > 1)
					Grid::AssertIsValid(location+h-1,CLOCATION);
				for (int z = 0; z < h; z++)
				{
					index_t at = CellToGridIndex(location+z);
					auto&existing = entries[at];
					if (existing)
						FATAL__("Trying to register "+object->GetName()+"(@"+ToString(object->location)+",z="+String(z)+") at "+ToString(location)+": Cell occupied by "+existing->GetName()+"(@"+ToString(existing->location)+")");
					LogEvent(EventSignificance::Debug,object.get(),"Inserted into grid location "+String(at)+"/"+ToString(location));
					existing = object;
				}
				object->isInsertedIntoGrid = true;
			}

			void			Flush()
			{
				Super::Fill(nullptr);
			}

		};

	extern ObjectVolumeGrid<MobileEntity>	mobileGrid;
	extern ObjectVolumeGrid<Structure>		structureGrid;



	PMobileEntity	GetMobileEntity(const Grid::LCellIdI&cellID);
	PStructure		GetStructure(const Grid::LCellIdI&cellID);
	PWorldEntity	GetEntity(const Grid::LCellIdI&cellID);


	//void			InjectIntoNeighborhood(const PStructure&s, float atTime);

	void			GetNeighborhood( const OwnerParameter&referenceOwner, bool ignoreOwnership, const Grid::LCellIdI&cellID, TStructureNeighborhood&outNeighborhood);

	/**
	Checks if there is a hangar in the given location that can handle a given mobile entity moving in
	*/
	PHangar			AccessibleHangarHere(const Grid::LCellIdI&location, const MobileEntity&m, const OwnerParameter&mOwner);
	inline PHangar	AccessibleHangarHere( const Grid::LCellIdI&location, const MobileEntity&m)	{return AccessibleHangarHere(location,m,m.owner);}





	struct Choice
	{
		Grid::LCellIdI	location = Grid::LCellIdI::Invalid;
		int			visualVerticalOffset=0;

		/**/		Choice()	{}
		/**/		Choice(const Grid::LCellIdI&loc, int visualOffset):location(loc),visualVerticalOffset(visualOffset)	{}
		bool		operator==(const Choice&other) const {return location == other.location;}
		bool		operator==(const Grid::LCellIdI&other) const {return location == other;}
	};

	struct TChoiceVisualization
	{
		bool			showArrow=false, 
						hasArrowCellTarget=false,
						showTrajectory=false;
		TrajectoryEndPoint	arrowFrom;
		Grid::LCellIdI	arrowFromCell;
		bool			paraboloidTrajectory = false;
		float			flatTrajectoryRange=1;
		TrajectoryEndPoint	arrowCellTarget;
	};

	void			GetChoices(WorldEntity&entity, const PMobileEntity&deploying, CursorMode activeCursorMode, Ctr::BasicBuffer<Choice>&movementChoices, TChoiceVisualization&vis);
	void			GetChoices(const PWorldEntity&entity, const PMobileEntity&deploying, CursorMode activeCursorMode, Ctr::BasicBuffer<Choice>&movementChoices, TChoiceVisualization&vis);



	inline float	ExplosionTime(float size)
					{ return std::max(1.3f,size*size/20.f);}


	static const int badHeight = 0x1FFFFFFF;	//Keep equal to sphereGrid.h Grid::BadHeight
	/**
	Attempts to find the lowest possible low-resolution grid cell to put a vehicle in.
	Search starts from the specified reference cell height. If the cell is already empty, search continues down, otherwise up.
	*/
	int				FindApplicableGround(const Grid::LCellIdI&cell, bool canStandOnWater, bool canStandOnLava);
	int				FindApplicableGround(const World::Data&world, const Grid::LCellIdI&cell, bool canStandOnWater, bool canStandOnLava);

	/**
	Determines all spawning choices for a completed vehicle construction
	@param producer Entity that has completed the production
	@param production Base type of the vehicle that was produced
	*/
	void			GetProductionChoices(const WorldEntity*producer,const PVehicle&production,
							BasicBuffer<Choice>&choices);



	bool			NoMobileEntityHere(const Grid::LCellIdI&cellID);
	bool			NoMobileEntityHereExcept(const Grid::LCellIdI&cellID, const MobileEntity*);
	bool			NoStationaryMobileEntityHereExcept(const Grid::LCellIdI&cellID, const MobileEntity*v);
	#ifndef NO_SATELLITES
		bool		NoSatelliteHere(const UINT32 cellID);
	#endif

	void			RemoveFromGrid(const PWorldEntity&v,const Grid::LCellIdI&location);
	void			InsertIntoGrid(const PWorldEntity&v,const Grid::LCellIdI&location);


	inline bool		Failed(MobilePlacementResult rs)
					{
						return rs == MobilePlacementResult::PlacementFailedHarmfulOrBlocked
							|| rs == MobilePlacementResult::PlacementFailedMobileDestroyed
							|| rs == MobilePlacementResult::PlacementFailedTemporarilyObstructed
							|| rs == MobilePlacementResult::PlacementFailedPermanentlyObstructed
							;
					}

	bool			NothingBlockingHere(const Grid::LCellIdI&cellID, bool checkVehiclesOnClient);
	PWorldEntity	GetBlockingHere(const Grid::LCellIdI&cellID, bool checkVehiclesOnClient);



	#ifdef CLIENT
		#ifndef NO_HIBERNATION
			extern UINT32	uplinkTraffic,
							onlineUplinkTraffic;
		#endif
		PStructure	SpawnStructure(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&location,EntityType type, bool placeNow, bool isOwnedByMe);
		PVehicle	SpawnVehicle(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&baseLocation,EntityType type, bool placeNow, bool isOwnedByMe);
		PMobileEntity	SpawnNonVehicleMobile(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&baseLocation,EntityType type, bool placeNow, bool isOwnedByMe);
		/**
		Selectively spawns either a vehicle or a satellite, depending on the parameters
		In order to spawn a satellite, @a turret must be @a ConstName::Empty, and @base be a valid satellite type name
		*/
		PMobileEntity	SpawnMobileEntity(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&baseLocation,EntityType type, bool placeNow, bool isOwnedByMe);
		void		Place( const PStructure&s);
		void		Place( const PVehicle&vehicle, bool insertIntoGrid=true);
		#ifndef NO_SATELLITES
			void	Place( const PSatellite&s);
		#endif
		void		Place(const SubOrbital::PPlane&s);
		void		PlaceMobileEntity(const PMobileEntity&m, bool insertIntoGrid=true, bool setHealthToFull=true);



	#else
		PVehicle	AssembleVehicle(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&baseLocation, const EntityType&t);
		PVehicle	AssembleVehicle(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&baseLocation, const String&base, const String&turret);
		/**
		Selectively constructs a vehicle or a satellite, depending on the given configuration.
		To successfully construct a satellite the specified @a turret parameter must be empty, and @a base must be a valid satellite type name
		*/
		PMobileEntity	AssembleMobileEntity(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&baseLocation, const String&base, const String&turret);

		bool		GetSentryTarget(WorldEntity&source, int munitionStorage, const TrajectoryEndPoint&sourceLocation, const TurretBase&base, TurretBase::TargetChoice& outTarget);

		/**
		Reference to the currently active explosion attack.
		NULL if not currently exploding.
		*/
		extern const ApplicationInterface::TAttack*	currentExplosionAttack;

		void		Explode(const ApplicationInterface::TAttack& attack, 
						const Grid::HCellIdI&targetCell, const M::TVec3<>&center, float explosionRadius, int damageLevel, bool doomsday,
						float&atTime
						);
		void		ExplodeEMP(const ApplicationInterface::TAttack& attack,
						const Grid::HCellIdI&targetCell, const M::TVec3<>&center, float explosionRadius, int damageLevel, float&atTime);


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
		@param[out] outDeployed If non-null, captures any deployed entity, new or existing as a result of the operation. Set to nullptr if handling failed
		@return true on success
		*/
		bool		ProcessChoiceIntersection(const PWorldEntity&e, const Grid::LCellIdI&choice,  CursorMode mode, const PMobileEntity&deployWhat, PWorldEntity*outDeployed);

	#endif

	/**
	Constructs a new vehicle using the specified parameters, and inserts it into the local scene.
	It receives full health, and a newly generated ID.
	Server-side only: The owner, if specified, is notified during the internal call to InsertConstructed().
	@param owner Owner of the new vehicle. Recorded in the respective member variable of the new entity. May be empty.
	@param baseLocation Position to place the new vehicle at. Must be valid.
		Server-side only:
			If the given location is unsuitable, a better location is looked for vertically via Vehicle::PositionOnTop().
			A critical failure (FATAL__) is triggered if no suitable location was found.
		Client-side only:
			No checks are performed. The location must be completely valid and suitable for this vehicle type.
	@param base Vehicle base to use. Must not be null. Typically taken from Objects::Component::*
	@param turret Turret to use for the vehicle. Must not be null. Typically taken from Objects::Component::*
	@param fillStorage Automatically fill any available vehicle storage to the maximum. Left empty otherwise
	@param[in,out] atTime Time to register the insertion at. The given value will be incremented at least once by timeEpsilon
	@return Pointer to the newly created vehicle. Never null
	*/
	PVehicle		InsertNewVehicle(const OwnerParameter&owner, const Grid::LCellIdI&baseLocation,Component::VehicleBase*base, Component::Turret*turret, bool fillStorage, float&atTime);
	/**
	@copydoc InsertNewVehicle(const OwnerParameter&,const Grid::LCellIdI&,Component::VehicleBase*,Component::Turret*,bool,float&);
	*/
	PVehicle		InsertNewVehicle(const OwnerParameter&owner, const Grid::LCellIdI&baseLocation,Component::VehicleBase&base, Component::Turret&turret, bool fillStorage, float&atTime);
	/**
	Inserts a pre-constructed mobile entity into the scene.
	This function is called automatically by InsertNewVehicle().
	Otherwise only called when a vehicle is inserted into the scene from off-grid storage.
	Invokes OnInsert() on the entity, where freshConstruction is set to setHealthToFull.
	If the entity has previously been registered as off-grid, it will be unregistered from the owner (if any) as such.
	Any containing hangar entity is NOT notified of this operation.
	Updates grid, global lists, and owner registers where necessary.
	Various callback functions are invoked as a result to the insertion, potentially causing complex handling of sensor-appearances,
	both to the owning and any hostile faction(s).
	Client-side, the operation will update scene rendering, server-side various network messages may be caused.
	@param v Mobile entity to insert. Must not be null. A critical failure (FATAL__) is triggered if the entity has previously been inserted
	@param[in,out] atTime Time to register the insertion at. The given value will be incremented at least once by timeEpsilon
	@param insertIntoGrid Set true to insert the vehicle into the respective grid. Currently effective only if the specified entity is a vehicle
	@param setHealthToFull Set true to change entity health to 100%. Health remains unchanged otherwise
	*/
	void			InsertConstructed(const PMobileEntity&v, float&atTime, bool insertIntoGrid= true, bool setHealthToFull=true);
	/**
	Inserts a newly constructed structure entity into the scene.
	Invokes OnInsert() on the entity, where freshConstruction is set to setHealthToFull.
	Updates grid, global lists, and owner registers where necessary.
	Various callback functions are invoked as a result to the insertion, potentially causing complex handling of sensor-appearances,
	both to the owning and any hostile faction(s).
	Client-side the operation will update scene rendering, server-side various network messages may be caused.
	@param s Structure entity to insert. Must not be null. A critical failure (FATAL__) is triggered if the entity has previously been inserted
	@param[in,out] atTime Time to register the insertion at. The given value will be incremented at least once by timeEpsilon
	@param refreshNetworkNow Set true to immediately check changes in compound networks via RefreshCompoundCoverage(), false to simply register changes
	@param setHealthToFull Set true to change entity health to 100%. Health remains unchanged otherwise
	*/
	void			InsertConstructed(const PStructure&s, float&atTime, bool refreshNetworkNow=false, bool setHealthToFull=true);

	/**
	Inserts a newly constructed mobile entity into off-grid storage, implying it has entered storage of some hangar entity.
	Any containing hangar entity is NOT notified of this operation.
	Invokes OnInsert() on the entity.
	Updates global lists, and owner registers where necessary.
	@param v Mobile entity to insert. Must not be null. A critical failure (FATAL__) is triggered if the entity has previously been inserted into the scene or registered
	@param[in,out] atTime Time to register the insertion at. The given value will be incremented at least once by timeEpsilon
	@param setHealthToFull Set true to change entity health to 100%. Health remains unchanged otherwise
	*/
	void			RegisterNewOffGrid(const PMobileEntity&v, float&atTime, bool setHealthToFull=true);


	/**
	@brief Completely removes the specified entity from scene or off-grid storage.

	Invokes OnDestroy() on the entity, unless t is RemovalType::EnteredStorage.
	Updates grid, global lists, and owner registers where necessary.
	Various callback functions are invoked as a result to the removal, potentially causing complex handling of sensor-disappearances,
	both to the owning and any hostile faction(s).
	(Potentially cascading) Explosions may also be triggered by this function.
	Client-side, the operation will update scene rendering, server-side various network messages may be caused.
	@param e Entity to remove. Should exist. Triggers a log-entry and a debug-only critical failure if null.
	Passing an indestructible entity causes the function to fail silently.
	@param t Removal type. Must match the current state of the entity. I.e. RemovalType::DestroyedInStorage must be passed iff the specified entity is currently registered in off-grid storage
	@param[in,out] atTime Time to register the removal at. The given value will be incremented at least once by timeEpsilon
	*/
	void			Remove( const PWorldEntity&e, RemovalType t, float&atTime);

	/**
	@copydoc Remove()

	Unlike Remove(), this function also removes indestructible entities.
	@param e Entity to remove. Should exist. Triggers a log-entry and a debug-only critical failure if null.
	*/
	void			ForceRemove(const PWorldEntity&e, RemovalType t, float&atTime);



}



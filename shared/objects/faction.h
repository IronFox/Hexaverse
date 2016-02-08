#ifndef factionH
#define factionH

#include "entity.h"
#include "types.h"
#include "structureCompound.h"

namespace Objects
{
	//class Faction;
	//typedef std::shared_ptr<Faction>	PFaction;
	//typedef std::weak_ptr<Faction>		WFaction;

	#ifdef WIRELESS_VEHICLES
		static const int			GroundWirelessTransferRange = 6,
									OrbitalWirelessTransferRange = 18;
	#endif

	
	/**
	Persistent scene markers added/removed by the client.
	Markers are visible only to the owning faction, and do not hinder movement or construction.
	Multiple markers can be placed on the same location.
	*/
	class Marker : public BaseEntity
		#ifndef CLIENT
			, public SerializableObject
		#endif
	{
	public:
		CONSTRUCT_ENUMERATION3(Type, 
			EntityDeath,		//friendly entity has died here. usually preserved for 2-5 rounds. AI may preserve them longer to establish statistics 
			HostileContact,		//hostile entity was detected here when radar contact was lost. Usually preserved until radar contact is re-established
			Visited				//marker used by AI to remember previously visited area. referenceEntity is either empty of the mobile that was memorized here
			);

		TUniqueID					referenceEntity = TUniqueID::Empty(),	//!< Usage depends on type
									referenceEntityOwner = TUniqueID::Empty();
		ConstName					referencedType[2];

		Type						type;
		UINT32						createdInRound=0;

		#ifdef CLIENT
			UINT32					age = 0;	//age in rounds, updated during GetOutdated()
		#endif

		static const count_t		MaxMarkersPerFaction = 256;

		/**/						Marker() :BaseEntity(TEmpty())
		{
			referencedType[0] = referencedType[1] = ConstName::Empty;
		}

		void						adoptData(Marker&other)
		{
			referenceEntity = other.referenceEntity;
			memcpy(referencedType,other.referencedType,sizeof(referencedType));
			type = other.type;
			createdInRound = other.createdInRound;

			#ifdef CLIENT
				visualization.swap(other.visualization);
			#endif
			((BaseEntity&)*this) = other;
		}

		#ifndef CLIENT
			virtual	serial_size_t		GetSerialSize(bool export_size) const override;
			virtual	bool				Serialize(IWriteStream&out_stream, bool export_size) const override;
			virtual	bool				Deserialize(IReadStream&in_stream, serial_size_t fixed_size) override;
		#endif

		virtual String				GetTypeName() const override { return "Marker (" + String(type.ToString())+")"; }
		virtual ConstName			GetCTypeName() const override {return ConstName::Type_Marker;}


		static_assert(Objects::Marker::Type::NumberOfPossibilities == 3, "Please adjust marker logic");
		bool						UtilizesReferenceEntity() const { return UtilizesReferenceEntity(type); }
		static bool					UtilizesReferenceEntity(Type type) { return type != Type::Visited; }

		bool						operator==(const Marker&other) const
		{ 
			return id == other.id && location == other.location 
			&& type == other.type && createdInRound == other.createdInRound 
			&& referenceEntity == other.referenceEntity && referencedType[0] == other.referencedType[0] && referencedType[1] == other.referencedType[1];
		}


		#ifdef CLIENT

			void					UpdateAge(UINT32 roundNumber);

			PWorldEntity			visualization;
			//PStructure				structureVisualization;
			//PMobileEntity			mobileVisualization;

			struct TVisited
			{
				TMatrix4<>			system;
				UINT32				age;
			};

			struct TCandidate
			{
				TUniqueID			markerID;
				UINT32				priority;

				bool				operator<(const TCandidate&other) const
				{
					return priority > other.priority;	//biggest first, usually sorted by age
				}
			};

			static void				Create(const TUniqueID&id, Type type, const Grid::LCellIdI&location, UINT32 createdInRound, const TUniqueID&referenceID, const TUniqueID&referenceOwnerID, ConstName referenceType[2], UINT32 currentRound);
			static void				Destroy(const TUniqueID&id, bool controlEnabled);
			static Marker*			FindByReference(const TUniqueID&referenceID);
			static Marker*			Find(const TUniqueID&referenceID);
			static Marker*			Find(const Grid::LCellIdI&);
			static void				GetDeletionCandidates(BasicBuffer<TCandidate>&candidates);
			static void				FlushAll();
			static void				GetOutdated(BasicBuffer<Marker*>&outOudated, UINT32 currentRoundNumber, UINT32 deathOutdatedAfterRounds=3);

			static Buffer0<Marker, Strategy::Adopt>	all;
			static Buffer0<PStructure>	structureGhosts;
			static Buffer0<PMobileEntity>	mobileGhosts;
			static Buffer0<TVisited>	visitedSystems;
			static GenericHashTable<TUniqueID, index_t, TUniqueID>	idMap, referenceIDMap,visitedSystemIDMap;
		#endif
	};





	




	class Faction : public std::enable_shared_from_this<Faction>
	{
	private:
		#ifndef CLIENT
			void		ChangeSensory(const Grid::HighSetup::TVolumeDescriptor&desc, float t, short changeBy);
			void		ChangeSensory(const Grid::SkySensorState&state, float t, short changeBy);
//			void		ReplaceSensory(const Grid::SkySensorState&oldState, const Grid::SkySensorState&newState, float t, short changeBy);
			void		CheckChanges(float t);
		#endif
	public:
		TUniqueID	id;
		String		name;
		bool		online = false;
		float3		color;
		#ifdef CLIENT
			float3	activeEntityColor,
					inactiveEntityColor;
			bool	requestedNewRound = false;
			bool	isMine = false;
			count_t	roundChangeNotifications=0;
		#endif

		#ifdef CONNECTOR_LESS
			Compound	compound;
		#endif

		String		GetName() const
		{
			if (!this)
				return "<NULL>";
			if (name.IsNotEmpty())
				return name;
			return id.ToString();
		}
		String		GetLogName() const
		{
			if (!this)
				return "<NULL>";
			return id.ToString()+":'"+name+"'";
		}


		/**
		Computes a suitable color from a given faction ID.
		The process is guaranteed to return a well saturated, lit color, but may be too slow to use on a per-frame basis.
		No checks for uniqueness are performed at this stage.
		@param[out] outColor Resulting faction color
		@param id Unique faction ID
		@param iteration Passing a different iteration alters the resulting color in a random way
		*/		
		static void IDToColor(TVec3<>&outColor, const TUniqueID&id, int iteration=0);

		void							UpdateID(const TUniqueID&toID, bool recomputeColor=true);

		#ifdef CONNECTOR_LESS
			//index_t		compoundIndex = InvalidIndex;
		#endif

		#ifdef CLIENT
			static Buffer0<PFaction>	allKnown;
			static GenericHashTable<TUniqueID,PFaction,TUniqueID>	idMap;

			static PFaction				mine;


			static PFaction				FindByID(const TUniqueID&id);
			static PFaction				GetOrCreate(const TUniqueID&id);

			/**/						Faction(const TUniqueID&id) {UpdateID(id);}

		#else
			CryptographicID	accessToken;
			Buffer0<WMobileEntity>		allMobiles,	//!< All mobiles registered in the world
										storedMobiles;	//!< All mobile entities registered stored in some other entity. allMobiles and storedMobiles are exclusive of each other
			Buffer0<Orbital::WShip>		orbitalShips;	//overlaps with @a allMobiles
			Buffer0<Orbital::WStation>	orbitalStations;	//overlaps with @a structures
			Buffer0<Orbital::WHub>		orbitalHubs;	//overlaps with @a structures
			Buffer0<WStructure>			structures;

			Buffer0<WTower>				towers;

			static Buffer0<WFaction>	all;
			static PFaction				nexus;

			static PFaction				FindByID(const TUniqueID&);



			Grid::WorldData			currentState;
			Grid::VisibilityState	visibility;

			//FamilyState			global;

			count_t				nexusControl,
								acquiringNexusControl;

			int					deathInRounds;	//remaining rounds to live if nothing changes. <= 0 means inactive

			bool				hasFuelMine,
								hasMineralMine,
								hasMineralContainer,
								hasGenerator,
								hasVehiclePlant,
								hasTerraformer;

			Buffer0<Marker,Strategy::Adopt>		markers;
			GenericHashTable<TUniqueID, index_t, TUniqueID>	markerMap,referenceMap;

			GenericHashTable<PWorldEntity,float,SharedPtrHash>	lostVisualAt,//!< Used for entities that dropped out of contact during the same round
															gainedVisualAt;

			void DropPerRoundData()
			{
				lostVisualAt.Clear();
				gainedVisualAt.Clear();
			}


			struct SeeingReference
			{
				count_t			sightLevel=0;


			};

			GenericHashTable<PWorldEntity, SeeingReference, SharedPtrHash>	seeing;

			/**/		Faction(const TUniqueID&id, const String&name=""):name(name)
			{
				nexusControl = acquiringNexusControl = 0;
				deathInRounds = 0;

				accessToken.Generate();
				UpdateID(id);
			}

			/**
			Completely clears the local state for this faction to be reused in a different game
			*/
			void		FlushData();

			/**
			Checks if the local faction satisfies survival conditions
			*/
			bool		CanLive(String&whyNot) const;
			bool		CanSpawnType(const PWorldEntity&e, String&errorOut)	const;
			bool		CanSpawnFromStorage(const PWorldEntity&e, String&errorOut)	const;
			/**
			Counts the number of mobile entities owned by the local faction that share the specified turret type.
			Operates in O(n), n = number of mobile entities owned by the local faction
			*/
			count_t		CountMobileType(ConstName turretType) const;
			/**
			Counts the number of structures owned by the local faction that share the specified type
			Operates in O(n), n = number of structures owned by the local faction
			*/
			count_t		CountStructureType(ConstName structureType) const;

			//void		SignalCoverageChange(const bool*familiesChanges, float atTime);

			bool		HasActiveMobile(const PMobileEntity&v, bool forceDoNotCheckSatellites=false) const;
			bool		HasMobileInStorage(const PMobileEntity&v) const;
			void		RegisterLoss(const PWorldEntity&);
			void		RegisterStructureLoss(const PWorldEntity&);
			void		RegisterActiveMobileLoss(const PWorldEntity&);
			void		RegisterStoredMobileLoss(const PWorldEntity&);
			void		RegisterGain(const PWorldEntity&);
			void		RegisterStructureGain(const PWorldEntity&);
			void		RegisterActiveMobileGain(const PWorldEntity&);
			void		RegisterStoredMobileGain(const PWorldEntity&);

			bool		CanSee(const PWorldEntity&e, float t)	const;

			bool		GridCanSee(const PWorldEntity&e) const;
			bool		GridCanSee(const Grid::HighSetup::TVolumeDescriptor&) const;

			void		CheckGridVisibility(const PWorldEntity&,float&timeAt);

			void		SignalAppearance(const PWorldEntity&, float&t, bool cascade=true, bool stack=true, index_t incStackBy=1);
			void		SignalDisappearance(const PWorldEntity&, float&t, bool force = false, bool signalConnectedClient=true);

			void		RegisterSensorCreation(const Grid::SkySensorState&state, float&t, bool checkChangesNow, Buffer0<PWorldEntity>*newContacts = nullptr);
			void		RegisterSensorRemoval(const Grid::SkySensorState&state, float&t, bool checkChangesNow);
			void		RegisterSensorCreation(const Sphere<>&, bool isSatellite, float&t, bool checkChangesNow, Buffer0<PWorldEntity>*newContacts = nullptr);
			void		RegisterSensorMotion(const Sphere<>&from, bool isSatellite, const Sphere<>&to, float&t, bool checkChangesNow);
			void		RegisterSensorRemoval(const Sphere<>&, bool isSatellite, float&t, bool checkChangesNow);

			count_t		CountSatelliteCoverage(const TVec3<>&center)	const;
			count_t		CountSatelliteCoverage(const Sphere<>&)	const;

			/**
			Temporarily inserts a spherical sensor as to visualize the impact of a doomsday crater
			*/
			void		RegisterExplosionCrater(const Grid::HCellIdI&center, float radius, float t);

			bool		CanCreateMarker(bool priveleged) const { return markers.Count() < (priveleged ? Marker::MaxMarkersPerFaction*16 : Marker::MaxMarkersPerFaction); }
			Marker&		CreateMarker(const TUniqueID&id, bool priveleged, Marker::Type type, const TUniqueID&referenceID, const TUniqueID&ownerReferenceID);
			Marker*		FindMarker(const TUniqueID&markerID);
			bool		DestroyMarker(const TUniqueID&markerID);
			void		RestoreMarkers(const ArrayData<Marker>&source);
			Marker*		FindReferenceMarker(Marker::Type type, const TUniqueID&);

			#ifdef CONNECTOR_LESS
				/**
				Refreshes the member WorkingStructures of the local compound
				*/
				void	UpdateCompoundCoverage();
			#endif

			static void	Kill(const PFaction&);
		#endif
	};

		//extern Buffer0<POwner>	owners;


	#ifndef CLIENT
		void	ChangeStorage(WorldEntity&e, float atTime, const ResourceVector&max, ResourceVector&storage, Resource res, int&change);
		void	ChangeStorage(WorkingStructure&ws, float atTime, Resource res, int change);
		void	ChangeOwnership( const PWorldEntity&e, const OwnerParameter&newOwner, float&atTime);

	#else
		void	ChangeOwnership( const PWorldEntity&e,const OwnerParameter&newOwner, bool isNowMine);
	#endif

	/**
	Iterates through all factions, including the nexus faction
	*/
	void			ForeachFaction(const std::function<void(const PFaction&)>&func);

	/**
	Iterates through all entities that are located in the world, including markers (markers on client only)
	Stored mobiles are not iterated
	*/
	void			ForeachWorldEntity(const std::function<void(const PWorldEntity&)>&func);
	/**
	Iterates through all entities that are located in the world, including markers (markers on client only)
	Stored mobiles are also iterated
	*/
	void			ForeachWorldEntityIncludingStored(const std::function<void(const PWorldEntity&)>&func);

	/**
	Iterates through all mobile entities that are located in the world.
	Stored mobiles are not iterated
	Note that this function is usually overkill since most loops can easily know whether they target ground
	units or orbital units
	*/
	void			ForeachMobileWorldEntity(const std::function<void(const PMobileEntity&)>&func);

	/**
	Iterates through all mobile entities that are located in the world or in storage.
	Note that this function is usually overkill since most loops can easily know whether they target ground
	units or orbital units
	*/
	void			ForeachMobileEntityIncludingStored(const std::function<void(const PMobileEntity&)>&func);

	/**
	Iterates through all orbital entities excluding stored.
	*/
	void			ForeachOrbitalWorldEntity(const std::function<void(const PWorldEntity&)>&func);
}



#endif

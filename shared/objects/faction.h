#ifndef factionH
#define factionH

#include "entity.h"
#include "types.h"
#include "structureCompound.h"
#include "../stringTable.h"
#include "../roundNumber.h"
#include "../sharedContainer.h"
#include "research.h"
#include "../grid/resources.h"
#include "statistics.h"

namespace Objects
{
	//class Faction;
	//typedef std::shared_ptr<Faction>	PFaction;
	//typedef std::weak_ptr<Faction>		WFaction;


	
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
		RoundNumber					createdInRound;

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
			virtual	serial_size_t	GetSerialSize(bool export_size) const override;
			virtual	void			Serialize(IWriteStream&out_stream, bool export_size) const override;
			virtual	void			Deserialize(IReadStream&in_stream, serial_size_t fixed_size) override;
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
		bool						operator!=(const Marker&other) const {return !operator==(other);}


		#ifdef CLIENT

			void					UpdateAge(const RoundNumber&roundNumber);

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

			static void				Create(const TUniqueID&id, Type type, const Grid::LCellIdI&location, const RoundNumber&createdInRound, const TUniqueID&referenceID, const TUniqueID&referenceOwnerID, ConstName referenceType[2], const RoundNumber& currentRound);
			static void				Destroy(const TUniqueID&id, bool controlEnabled);
			static Marker*			FindByReference(const TUniqueID&referenceID);
			static Marker*			Find(const TUniqueID&referenceID);
			static Marker*			Find(const Grid::LCellIdI&);
			static void				GetDeletionCandidates(BasicBuffer<TCandidate>&candidates);
			static void				FlushAll();
			static void				GetOutdated(BasicBuffer<Marker>&outOudated, const RoundNumber&currentRoundNumber, UINT32 deathOutdatedAfterRounds=3);

			static Buffer0<Marker, Strategy::Adopt>	all;
			static SharedContainer<Structure>		structureGhosts;
			static SharedContainer<MobileEntity>	mobileGhosts;
			static Buffer0<TVisited>	visitedSystems;
			static GenericHashTable<TUniqueID, index_t>	idMap, referenceIDMap,visitedSystemIDMap;
		#endif
	};






	class Faction : public std::enable_shared_from_this<Faction>
	{
	private:
		#ifndef CLIENT
			friend class ResearchState;

			//void		ChangeSensory(const Grid::HighSetup::TVolumeDescriptor&desc, float t, short changeBy);
			//void		ChangeSensory(const Grid::SkySensorState&state, float t, short changeBy);
//			void		ReplaceSensory(const Grid::SkySensorState&oldState, const Grid::SkySensorState&newState, float t, short changeBy);
			/**
			Checks and potentially rebuilds knownWorld sensor state
			*/
			void		CheckSensory(float&t);

		#endif

		count_t			sensorLockLevel=0;

		void			InitID(const TUniqueID&toID, bool nexusFaction);

	public:
		TUniqueID	id;
		String		name;
		bool		online = false;
		float3		color;
		Statistics		statistics;
		ResearchState	researchState;
		#ifdef CLIENT
			float3	activeEntityColor,
					inactiveEntityColor;
			bool	requestedNewRound = false;
			bool	isOwnedByMe = false;
			count_t	roundChangeNotifications=0;
		#endif

		#ifdef ONE_COMPOUND_PER_FACTION
			Compound	compound;
		#endif

		String		GetName() const
		{
			if (!this)
				return "<NULL>";
			if (name.IsNotEmpty())
				return name;
			return ToString(id);
		}
		String		GetLogName() const
		{
			if (!this)
				return "<NULL>";
			return ToString(id)+":'"+name+"'";
		}

		
		struct TSensorLock
		{
			Faction*const owner;

			TSensorLock(Faction*f):owner(f)	{f->sensorLockLevel++;}
			~TSensorLock();
			TSensorLock(const TSensorLock&other):owner(other.owner) {owner->sensorLockLevel++;}
			void operator=(const TSensorLock&) = delete;
		};
		friend struct TSensorLock;

		TSensorLock LockSensorUpdates() {return this;}

#if 0
		/**
		Computes a suitable color from a given faction ID.
		The process is guaranteed to return a well saturated, lit color, but may be too slow to use on a per-frame basis.
		No checks for uniqueness are performed at this stage.
		@param[out] outColor Resulting faction color
		@param id Unique faction ID
		@param iteration Passing a different iteration alters the resulting color in a random way
		*/		
		static void IDToColor(TVec3<>&outColor, const TUniqueID&id, int iteration=0);
#endif /*0*/


		#ifdef ONE_COMPOUND_PER_FACTION
			//index_t		compoundIndex = InvalidIndex;
		#endif

		#ifdef CLIENT
			static Buffer0<PFaction>	allKnown;
			static GenericHashTable<TUniqueID,PFaction>	idMap;

			static PFaction				mine;


			static PFaction				FindByID(const TUniqueID&id);
			static PFaction				GetOrCreate(const TUniqueID&id);

			/**/						Faction(const TUniqueID&id):researchState(this,false),statistics(this) {InitID(id,false);}

		#else

			CryptographicID	accessToken;
			Ctr::Buffer0<WMobileEntity>		allMobiles,	//!< All mobiles registered in the world
										storedMobiles;	//!< All mobile entities registered stored in some other entity. allMobiles and storedMobiles are exclusive of each other
			#ifndef NO_SATELLITES
				Buffer0<WSatellite>		satellites;
				Buffer0<WSkyHub>		skyHubs;	//overlaps with satellites
			#endif
			Ctr::Buffer0<WStructure>			structures;

			Ctr::Buffer0<WTower>				towers;

			static Ctr::Buffer0<WFaction>	all;
			static PFaction				nexus;

			static PFaction				FindByID(const TUniqueID&);
			static PFaction				Create(const TUniqueID&, const String&name = "");


			World::Data				knownWorld;
			Grid::ResourceDiscovery	knownResources;
			

			count_t				nexusControl,
								acquiringNexusControl;

			int					deathInRounds;	//remaining rounds to live if nothing changes. <= 0 means inactive




			enum class UnitClass
			{
				None = 0x0,
				FuelMine = 0x1,
				MineralMine = 0x2,
				MineralContainer = 0x4,
				Generator = 0x8,
				VehiclePlant = 0x10,
				Terraformer = 0x20,
			};

			bool				isDead = false;
			UnitClass			hasUnitClasses = UnitClass::None;

			Ctr::Buffer0<Marker,Strategy::Adopt>		markers;
			Ctr::GenericHashTable<TUniqueID, index_t>	markerMap,referenceMap;

			Ctr::GenericHashTable<PWorldEntity,float,SharedPtrHash>	lostVisualAt,//!< Used for entities that dropped out of contact during the same round
															gainedVisualAt;

			struct SeeingReference
			{
				count_t			sightLevel=0;


			};
			Ctr::GenericHashTable<PWorldEntity, SeeingReference, SharedPtrHash>	seeing;




			static void			RecreateNexus(const TUniqueID&id=TUniqueID::Empty());	//!< Creates a fresh nexus faction and assigns it to @a nexus



			/**/		Faction(const TUniqueID&toID, const String&name="", bool isNexus=false):researchState(this,isNexus), statistics(this), name(name)
			{
				nexusControl = acquiringNexusControl = 0;
				deathInRounds = 0;

				accessToken.Generate();
				InitID(toID,isNexus);
			}


			void DropPerRoundData()
			{
				lostVisualAt.Clear();
				gainedVisualAt.Clear();
			}

			/**
			Checks and potentially rebuilds knownWorld sensor state
			*/
			void		CheckSensory();


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

			bool		HasActiveMobile(const PMobileEntity&v) const;
			bool		HasMobileInStorage(const PMobileEntity&v) const;
			void		RegisterLoss(const PWorldEntity&);
			void		RegisterStructureLoss(const PWorldEntity&);
			void		RegisterActiveMobileLoss(const PWorldEntity&);
			void		RegisterStoredMobileLoss(const PWorldEntity&);
			void		RegisterActiveMobileGain(const PWorldEntity&);

			bool		CanSee(const PWorldEntity&e, float t)	const;

			bool		GridCanSee(const PWorldEntity&e, float &t);
			bool		GridCanSee(const Grid::HighSetup::TVolumeDescriptor&, float &t);

			void		CheckGridVisibility(const PWorldEntity&,float&timeAt);

			/**
			Processes a potentially new contact

			@return True, if the contact was, indeed, new, false otherwise
			*/
			bool		SignalAppearance(const PWorldEntity&, float&t, bool cascade=true, bool stack=true, index_t incStackBy=1);
			void		SignalDisappearance(const PWorldEntity&, float&t, bool force = false, bool signalConnectedClient=true);

			void		RegisterSensorCreation(const World::Sensor&, int sensorHeight, float&t, Ctr::Buffer0<PWorldEntity>*newContacts = nullptr);
			void		RegisterSensorRemoval(const World::Sensor&, int sensorHeight, float&t);
			void		RegisterSensorMotion(const World::Sensor&from, const World::Sensor&to, float&t);

			count_t		CountOffGridCoverage(int channelHeight, const TVec3<>&)	const;
			count_t		CountOffGridCoverage(const Sphere<>&)	const;

			/**
			Temporarily inserts a spherical sensor as to visualize the impact of a doomsday crater
			*/
			void		RegisterExplosionCrater(const Grid::HCellIdI&center, float radius, float t);


			bool		CanCreateMarker(bool priveleged) const { return markers.Count() < (priveleged ? Marker::MaxMarkersPerFaction*16 : Marker::MaxMarkersPerFaction); }
			Marker&		CreateMarker(const TUniqueID&id, bool priveleged, Marker::Type type, const TUniqueID&referenceID, const TUniqueID&ownerReferenceID);
			Marker*		FindMarker(const TUniqueID&markerID);
			bool		DestroyMarker(const TUniqueID&markerID);
			void		RestoreMarkers(const Ctr::ArrayData<Marker>&source);
			Marker*		FindReferenceMarker(Marker::Type type, const TUniqueID&);

			#ifdef ONE_COMPOUND_PER_FACTION
				/**
				Refreshes the member WorkingStructures of the local compound
				*/
				void	UpdateCompoundCoverage();
			#endif
			static void FindFreeColor(float3&outColor);

			static void	Kill(const PFaction&);
		#endif
	};

		//extern Buffer0<POwner>	owners;



	#ifndef CLIENT
		void	ChangeCompoundStorage(Compound&compound, MobileEntity*originatingAt, float atTime, Resource res, int change);

		void	ChangeStorage(WorldEntity&e, float atTime, const ResourceVector&max, ResourceVector&storage, Resource res, int change);
		void	ChangeStorage(WorkingStructure&ws, float atTime, Resource res, int change);
		void	ChangeStorage(MobileEntity&v, float atTime, Resource res, int change);
		//void	MapVisibility2(const PWorldEntity&e, const PWorldEntity&other, const OwnerParameter&checkForOwner, float&atTime);
		void	ChangeOwnership( const PWorldEntity&e, const OwnerParameter&newOwner, float&atTime);

		/**
		Thoroughly checks whether or not entities able to see each other are correctly referenced as such.
		This method has quadratic complexity (where n = number of entities in the world), and should
		not be called too frequently.
		Verification failure leads to program termination.
		*/
//		void	VerifyVisibilityNetwork();
		bool	CanWithdrawFromStorage(const MobileEntity&v, Resource res, int demand);
		int		GetStorage(const MobileEntity&v, Resource res);

	#else
		void	ChangeOwnership( const PWorldEntity&e,const OwnerParameter&newOwner, bool isNowMine);
	#endif

	/**
	Iterates through all factions, including the nexus faction
	*/
	void			ForeachFaction(const std::function<void(const PFaction&)>&func);

	/**
	Iterates through all entities that are located in the world, potentially including ghosts (ghosts on client only)
	Stored mobiles are not iterated
	*/
	void			ForeachWorldEntity(const std::function<void(const PWorldEntity&)>&func, bool includeGhosts);
	/**
	Iterates through all entities that are located in the world, potentially including ghosts (ghosts on client only)
	Stored mobiles are also iterated
	*/
	void			ForeachWorldEntityIncludingStored(const std::function<void(const PWorldEntity&)>&func, bool includeGhosts);

	/**
	Iterates through all mobile entities that are located in the world, potentially including ghosts (ghosts on client only).
	Stored mobiles are not iterated
	Note that this function is usually overkill since most loops can easily know whether they target ground
	units or orbital units
	*/
	void			ForeachMobileWorldEntity(const std::function<void(const PMobileEntity&)>&func, bool includeGhosts);

	/**
	Iterates through all mobile entities that are located in the world or in storage, potentially including ghosts (ghosts on client only).
	Note that this function is usually overkill since most loops can easily know whether they target ground
	units or orbital units
	*/
	void			ForeachMobileEntityIncludingStored(const std::function<void(const PMobileEntity&)>&func, bool includeGhosts);
}

#ifndef CLIENT


inline Objects::Faction::UnitClass	operator|(Objects::Faction::UnitClass a,Objects::Faction::UnitClass b)
{
	typedef Objects::Faction::UnitClass UnitClass;
	typedef std::underlying_type<UnitClass>::type	T;
	return static_cast<UnitClass>( static_cast<T>(a) | static_cast<T>(b) );
}

inline void	operator|=(Objects::Faction::UnitClass&a,Objects::Faction::UnitClass b)
{
	a = a | b;
}

inline bool	operator&(Objects::Faction::UnitClass a,Objects::Faction::UnitClass b)
{
	typedef Objects::Faction::UnitClass UnitClass;
	typedef std::underlying_type<UnitClass>::type	T;
	return (static_cast<T>(a) & static_cast<T>(b) ) != 0;
}
		

#endif

#endif

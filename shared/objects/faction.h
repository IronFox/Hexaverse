#ifndef factionH
#define factionH

#include "entity.h"
#include "entityType.h"
#include "types.h"
#include "structureCompound.h"
#include "../stringTable.h"
#include "../roundNumber.h"
#include "../sharedContainer.h"
#include "research.h"
#include "../grid/resources.h"
#include "statistics.h"
#include <map>

namespace Objects
{
	
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
		EntityType					referencedType;
		

		Type						type;
		RoundNumber					createdInRound;

		#ifdef CLIENT
			UINT32					age = 0;	//age in rounds, updated during GetOutdated()
		#endif

		static const count_t		MaxMarkersPerFaction = 256;

		/**/						Marker() :BaseEntity(TEmpty())
		{}

		void						adoptData(Marker&other)
		{
			referenceEntity = other.referenceEntity;
			referencedType = other.referencedType;
			type = other.type;
			createdInRound = other.createdInRound;

			#ifdef CLIENT
				visualization.swap(other.visualization);
			#endif
			((BaseEntity&)*this) = other;
		}

		#ifndef CLIENT
			virtual	void			Serialize(IWriteStream&outStream) const override;
			virtual	void			Deserialize(IReadStream&inStream) override;
		#endif

		virtual String				GetTypeName() const override { return "Marker (" + String(type.ToString())+")"; }
		virtual EntityType			GetType() const override {return ConstName::Type_Marker;}


		static_assert(Objects::Marker::Type::NumberOfPossibilities == 3, "Please adjust marker logic");
		bool						UtilizesReferenceEntity() const { return UtilizesReferenceEntity(type); }
		static bool					UtilizesReferenceEntity(Type type) { return type != Type::Visited; }

		bool						operator==(const Marker&other) const
		{ 
			return id == other.id && location == other.location 
			&& type == other.type && createdInRound == other.createdInRound 
			&& referenceEntity == other.referenceEntity && referencedType == other.referencedType;
		}
		bool						operator!=(const Marker&other) const {return !operator==(other);}


		#ifdef CLIENT

			void					UpdateAge(const RoundNumber&roundNumber);

			PWorldEntity			visualization;
			//PStructure				structureVisualization;
			//PMobileEntity			mobileVisualization;

			struct TVisited
			{
				M::TMatrix4<>			system;
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

			static void				Create(const TUniqueID&id, Type type, const Grid::LCellIdI&location, const RoundNumber&createdInRound, const TUniqueID&referenceID, const TUniqueID&referenceOwnerID, EntityType referenceType, const RoundNumber& currentRound);
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


	class Alliance : public std::enable_shared_from_this<Alliance>
	{
	public:
		const TUniqueID	id;

		#ifndef CLIENT
			struct TNexusStats
			{
				count_t	acquiringNexusControl=0;
				count_t	nexusControl=0;
			};
		#endif


		struct TConfig
		{
			bool	exposeEntityDetails = false,
					shareSensors = true;

			/**/	TConfig()	{}
			/**/	TConfig(UINT32 flags);
			UINT32	ToFlags() const;
			bool	operator==(const TConfig&other) const {return exposeEntityDetails == other.exposeEntityDetails && shareSensors == other.shareSensors;}
			bool	operator!=(const TConfig&other) const {return !operator==(other);}
			friend String ToString(const TConfig&cfg)
			{
				return String(cfg.exposeEntityDetails ? "See Unit Details":"-")+" "+(cfg.shareSensors ? "Share Sensors":"-");
			}
		}			config;

		/**/				Alliance(const TUniqueID&id);
		virtual				~Alliance();
		void				ForeachMember(const std::function<void(const PFaction&)>&f,bool livingOnly=true)	const;
		void				VisitMembers(const std::function<void(const PFaction&)>&f,bool livingOnly=true)	const	{ForeachMember(f,livingOnly);}
		void				Leave(const PFaction&f);
		void				Leave(Faction*f);
		void				Join(const PFaction&f);
		bool				IsMember(const PFaction&f) const;
		count_t				CountAllMembers() const {return members.Count();}
		count_t				CountLivingMembers() const;
		PFaction			GetMember(index_t i ) const {return members[i].wFaction.lock();}
		#ifndef CLIENT
			TNexusStats		GetNexusStats() const;
			UINT64			GetWinningScore() const;
		#endif

		void				Invite(const PFaction&f);
		void				CancelInvite(const PFaction&f);
		bool				HaveInvited(const PFaction&f);
		void				VisitAllInvitedFactions(const std::function<void(const PFaction&)>&f, bool livingOnly=true) const;

		void				UpdateConfiguration(const TConfig&);
		static PAlliance	FindByID(const TUniqueID&id ){ WAlliance rs; all.Query(id,rs); return rs.lock();}
		static void			Foreach(const std::function<void(const PAlliance&)>&visitor);
		static count_t		Count() {return all.Count();}
		static void			FlushAll() {all.Clear();}
	private:

		struct TMember
		{
			WFaction	wFaction;
			Faction*	rawPointer;
		};

		Buffer0<TMember>						members,invited;
		static HashTable<TUniqueID,WAlliance>	all;

		void				RefreshSensors(bool force);
		index_t				FindInviteOf(const Objects::PFaction&f) const;
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
			void		HandlePotentialAppearance(const Objects::PWorldEntity&e, float&t, bool shareSensors);
			void		HandlePotentialDisappearance(const Objects::PWorldEntity&e, float&t, bool shareSensors);

		#endif

		count_t			sensorLockLevel=0;

		/**
		Inits ID, color, and alliance. Must be called by constructors. Must not be called by anything else
		*/
		void			Setup(const TUniqueID&toID, bool nexusFaction);

		#ifdef CLIENT
			/**/		Faction(const TUniqueID&id):researchState(this,false),statistics(this) {Setup(id,false);}

		#else
			/**/		Faction(const TUniqueID&toID, const String&name="", bool isNexus=false):researchState(this,isNexus), statistics(this), name(name)
			{
				nexusControl = acquiringNexusControl = 0;
				deathInRounds = 0;

				accessToken.Generate();
				Setup(toID,isNexus);
			}
		#endif

	public:
		TUniqueID		id;
		String			name;
		bool			online = false;
		float3			color;
		Statistics		statistics;
		ResearchState	researchState;
		PAlliance		alliance;
		/**
		If set, shares one huge mobile park across all vehicle stores.
		Allows instant teleportation from one vehicle store to another.
		Doing so does not actually increase the store capacity (stores define how many mobiles can be stored there).
		*/
		PMobilePark		sharedPark;
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


		virtual			~Faction();
	
		void			AddEntitySensory(World::Data&worldData, float&t) const;
		void			CollectSensors(Buffer0<World::Sensor>&outSensors, float&t) const;
		
		/**
		Checks whether or not units of the other faction are considered friendly (invalid auto-targets).
		This check is symmetric
		*/
		bool			IsFriendly(const PFaction&other) const {return other.get() == this || other->alliance == alliance;}
		/**
		Checks whether or not units of the other faction receive detailed entity state information such as build queues.
		This check is symmetric
		*/
		bool			ExposesEntityDetailsTo(const PFaction&other) const {return other && this && (other.get() == this || (other->alliance == alliance && alliance->config.exposeEntityDetails));}
		bool			SharingSensorWith(const PFaction&other) const{return other && this && (other.get() == this || (other->alliance == alliance && alliance->config.shareSensors));}

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



#if 0
		/**
		Computes a suitable color from a given faction ID.
		The process is guaranteed to return a well saturated, lit color, but may be too slow to use on a per-frame basis.
		No checks for uniqueness are performed at this stage.
		@param[out] outColor Resulting faction color
		@param id Unique faction ID
		@param iteration Passing a different iteration alters the resulting color in a random way
		*/		
		static void IDToColor(M::TVec3<>&outColor, const TUniqueID&id, int iteration=0);
#endif /*0*/


		#ifdef ONE_COMPOUND_PER_FACTION
			//index_t		compoundIndex = InvalidIndex;
		#endif

		static const String				nexusName;

		#ifdef CLIENT
			static Buffer0<PFaction>	allKnown;
			static GenericHashTable<TUniqueID,PFaction>	idMap;
			#ifdef _DEBUG
				static GenericHashSet<TUniqueID>	deadIDs;
			#endif

			static PFaction				mine;


			static PFaction				FindByID(const TUniqueID&id);
			static PFaction				GetOrCreate(const TUniqueID&id);
			/**
			Retrieves (or creates) the specified faction.
			@param id Faction ID
			@param allianceID Alliance ID to lookup or create
			@param[out] outAllianceChanged Set true if a new faction was created or an existing faction had a new alliance assigned as a result of the operation
			*/
			static PFaction				GetOrCreate(const TUniqueID&id, const TUniqueID&allianceID, bool&outAllianceChanged);

		#else

			CryptographicID				accessToken;
			Ctr::Buffer0<WMobileEntity>	allMobiles,		//!< All mobiles registered in the world
										storedMobiles;	//!< All mobile entities registered stored in some other entity. allMobiles and storedMobiles are exclusive of each other
			#ifndef NO_SATELLITES
				Buffer0<WSatellite>		satellites;
				Buffer0<WSkyHub>		skyHubs;	//overlaps with satellites
			#endif
			Ctr::Buffer0<WStructure>			structures;

			Ctr::Buffer0<WTower>				towers;

			static Ctr::Buffer0<PFaction>	all;
			static PFaction				nexus;

			static PFaction				FindByID(const TUniqueID&);
			static PFaction				Create(const TUniqueID&, const TUniqueID&allianceID, const String&name = "");
			static PFaction				Create(const TUniqueID&, const PAlliance&alliance, const String&name = "");


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

			struct SeeingReference
			{
				WWorldEntity			reference;
				bool					wasVisibleAtBeginning = false;
				std::map<float,bool>	reverseVisibilityChange;	//reverse mapped

				void					swap(SeeingReference&other)
				{
					std::swap(wasVisibleAtBeginning,other.wasVisibleAtBeginning);
					reference.swap(other.reference);
					reverseVisibilityChange.swap(other.reverseVisibilityChange);
				}

				friend void				swap(SeeingReference&a,SeeingReference&b)
				{
					a.swap(b);
				}

				bool					VisibleAt(float t) const;
				void					Appear(const Faction*parent, const PWorldEntity&e, float&t);
				void					Disappear(const Faction*parent, const PWorldEntity&e, float&t);
				bool					CurrentlyVisible() const {return reverseVisibilityChange.empty() ? wasVisibleAtBeginning : reverseVisibilityChange.begin()->second;}

			};
			PointerTable<SeeingReference, Swap>	seeing;


			void VisitVisibleForeignEntities(const std::function<void(const PWorldEntity&e)>&f)	const
			{
				seeing.VisitAllValues([&f](const Objects::Faction::SeeingReference&ref)
				{
					if (ref.CurrentlyVisible())
					{
						DBG_ASSERT__(ref.reverseVisibilityChange.empty());
						auto e = ref.reference.lock();
						if (e)
							f(e);
					}
				});
			};


			void VisitVisibleForeignEntities(float t, const std::function<void(const PWorldEntity&e)>&f)	const
			{
				seeing.VisitAllValues([&f,t](const Objects::Faction::SeeingReference&ref)
				{
					if (ref.VisibleAt(t))
					{
						auto e = ref.reference.lock();
						if (e)
							f(e);
					}
				});
			};

					
			struct TSensorLock
			{
				Faction*const owner;

				TSensorLock(Faction*f):owner(f)	{f->sensorLockLevel++;}
				~TSensorLock();
				TSensorLock(const TSensorLock&other):owner(other.owner) {owner->sensorLockLevel++;}
				void operator=(const TSensorLock&) = delete;
			};
			friend struct TSensorLock;



			static void	RecreateNexus(const TUniqueID&id=TUniqueID::Empty());	//!< Creates a fresh nexus faction and assigns it to @a nexus
			TSensorLock LockSensorUpdates() {return this;}


			void DropPerRoundData()
			{
				seeing.FilterEntries([](const void*p, SeeingReference&seeing)->bool
				{
					if (!seeing.CurrentlyVisible())
						return false;
					seeing.reverseVisibilityChange.clear();
					seeing.wasVisibleAtBeginning= true;
					return true;
				});
			}

			/**
			Checks and potentially rebuilds knownWorld sensor state.
	
			This is the server equivalent of client side ClientGrid::CheckSensory() (gridControl.h)
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
			/**
			Counts the number of mobiles/structures owned by the local faction that provide the specified turret type
			Operates in O(n), n = number of units owned by the local faction
			*/
			count_t		CountTurretType(ConstName turretType) const;

			//void		SignalCoverageChange(const bool*familiesChanges, float atTime);

			bool		HasActiveMobile(const PMobileEntity&v) const;
			bool		HasMobileInStorage(const PMobileEntity&v) const;
			void		RegisterLoss(const PWorldEntity&);
			void		RegisterStructureLoss(const PWorldEntity&);
			void		RegisterActiveMobileLoss(const PWorldEntity&);
			void		RegisterStoredMobileLoss(const PWorldEntity&);
			void		RegisterActiveMobileGain(const PWorldEntity&);

			void		VisitMyEntities(const std::function<void(const PWorldEntity&)>&f, bool includeStored) const;

			bool		CanSee(const PWorldEntity&e, float t)	const;

			bool		GridCanSee(const PWorldEntity&e, float &t);
			bool		GridCanSee(const Grid::HighSetup::TVolumeDescriptor&, float &t);

			void		CheckGridVisibility(const PWorldEntity&,float&timeAt);

			/**
			Processes a potentially new contact

			@return True, if the contact was, indeed, new, false otherwise
			*/
			bool		SignalAppearance(const PWorldEntity&, float&t, bool cascade=true);
			void		SignalDisappearance(const PWorldEntity&, float&t, bool force = false, bool signalConnectedClient=true);

			void		RegisterSensorCreation(const World::Sensor&, int sensorHeight, float&t, Ctr::Buffer0<PWorldEntity>*newContacts = nullptr);
			void		RegisterSensorRemoval(const World::Sensor&, int sensorHeight, float&t);
			void		RegisterSensorMotion(const World::Sensor&from, const World::Sensor&to, float&t);

			count_t		CountOffGridCoverage(int channelHeight, const M::TVec3<>&)	const;
			count_t		CountOffGridCoverage(const M::Sphere<>&)	const;

			count_t		GetWinningScore() const;
			count_t		GetOffensiveCapabilities() const;
			count_t		GetTotalEntityValue() const;

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

		#endif

		/**
		Terminates a faction and all units it posesses.
		Irreversible.
		WARNING: It is highly unusual for the client to call this method, and it is basically ONLY NECESSARY WHEN IN CONFIGURATION MODE.
		Currently does not remove markers
		*/
		static void		Kill(PFaction);

		void			ChangeAlliance(const PAlliance&);
		bool			IsAlliedWith(const PFaction&)const;
		#ifdef CLIENT
			/**
			Checks whether the local faction is allied with the local player's faction.
			Null-pointer sensitive (may be called on a null-object)
			@return true if the local faction exists, the player faction exists, and the two are allied. Also true if the local faction is the player faction
			*/
			bool		IsAlliedWithMe() const;
		#endif


	};

		//extern Buffer0<POwner>	owners;



	#ifndef CLIENT
		void	ChangeCompoundStorage(Compound&compound, MobileEntity*originatingAt, float atTime, Resource res, int change);

		void	ChangeStorage(WorldEntity&e, float atTime, const ResourceVector&max, ResourceVector&storage, Resource res, int change);
		void	ChangeStorage(WorkingStructure&ws, float atTime, Resource res, int change);
		void	ChangeStorage(MobileEntity&v, float atTime, Resource res, int change);
		//void	MapVisibility2(const PWorldEntity&e, const PWorldEntity&other, const OwnerParameter&checkForOwner, float&atTime);
		void	ChangeOwnership( const PWorldEntity&e, const PFaction&newOwner, float&atTime);

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
		void	ChangeOwnership( const PWorldEntity&e,const PFaction&newOwner, bool isNowMine);
	#endif

	/**
	Iterates through all factions, including the nexus faction
	*/
	void			ForeachFaction(const std::function<void(const PFaction&)>&func, bool includeDead=false);

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

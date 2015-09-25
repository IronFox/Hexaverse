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

		TUniqueID					referenceEntity = TUniqueID::Empty();	//!< Usage depends on type
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
				structureVisualization.swap(other.structureVisualization);
				mobileVisualization.swap(other.mobileVisualization);
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

			PStructure				structureVisualization;
			PMobileEntity			mobileVisualization;

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

			static void				Create(const TUniqueID&id, Type type, const Grid::LCellIdI&location, UINT32 createdInRound, const TUniqueID&referenceID, ConstName referenceType[2], UINT32 currentRound);
			static void				Destroy(const TUniqueID&id, bool controlEnabled);
			static Marker*			FindByReference(const TUniqueID&referenceID);
			static Marker*			Find(const TUniqueID&referenceID);
			static void				GetDeletionCandidates(BasicBuffer<TCandidate>&candidates);
			static void				FlushAll();
			static void				GetOutdated(BasicBuffer<TUniqueID>&outOudated, UINT32 currentRoundNumber, UINT32 deathOutdatedAfterRounds=3);

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
			void		ChangeSensory(const Grid::LCellIdI&center, float radius, float t, short changeBy);
			void		ChangeSensory(const Grid::SkySensorState&state, float t, short changeBy);
//			void		ReplaceSensory(const Grid::SkySensorState&oldState, const Grid::SkySensorState&newState, float t, short changeBy);
			void		CheckChanges(float t);
		#endif
	public:
		TUniqueID	id;
		String		name;
		bool		online = false;
		#ifdef CLIENT
			bool	requestedNewRound = false;
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
		#ifdef CONNECTOR_LESS
			//index_t		compoundIndex = InvalidIndex;
		#endif

		#ifdef CLIENT
			static Buffer0<PFaction>	allKnown;
		#else
			CryptographicID	accessToken;
			Buffer0<WMobileEntity>		allMobiles,
										storedMobiles;
			Buffer0<WSatellite>			satellites;
			Buffer0<WSkyHub>			skyHubs;	//overlaps with satellites
			Buffer0<WStructure>			structures;

			static Buffer0<WFaction>	all;
			static PFaction				nexus;


			Grid::WorldData			currentState;
			Grid::VisibilityState	visibility;

			//FamilyState			global;

			count_t				nexusControl,
								acquiringNexusControl;

			int					deathInRounds;	//remaining rounds to live if nothing changes. <= 0 means inactive

			bool				hasMineralGeneratorCycle,	//transient values
								hasVehicleProductionCycle,
								hasTerraformer;

			Buffer0<Marker,Strategy::Adopt>		markers;
			GenericHashTable<TUniqueID, index_t, TUniqueID>	markerMap,referenceMap;

			GenericHashTable<PWorldEntity,float,SharedPtrHash>	lostVisualAt,//!< Used for entities that dropped out of contact during the same round
															gainedVisualAt;

			void DropFrameData()
			{
				lostVisualAt.Clear();
				gainedVisualAt.Clear();
			}

			GenericHashSet<PWorldEntity, SharedPtrHash>	canSee;

			/**/		Faction(const String&name=""):name(name)
			{
				nexusControl = acquiringNexusControl = 0;
				deathInRounds = 0;

				accessToken.Generate();
				id.Generate();
			}

			/**
			Completely clears the local state for this faction to be reused in a different game
			*/
			void		FlushData();

			/**
			Checks if the local faction satisfies survival conditions
			*/
			bool		CanLive() const
			{
				return hasMineralGeneratorCycle
					&& (hasVehicleProductionCycle || hasTerraformer);
			}


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

			bool		SignalAppearance(const PWorldEntity&, float&t, bool cascade=true);
			void		SignalDisappearance(const PWorldEntity&, float&t, bool force = false);

			void		RegisterSensorCreation(const Grid::SkySensorState&state, float t);
			void		RegisterSensorRemoval(const Grid::SkySensorState&state, float t);
			void		RegisterSensorCreation(const Grid::LCellIdI&center, float radius, float t);
			void		RegisterSensorMotion(const Grid::LCellIdI&oldCenter, const Grid::LCellIdI&newCenter, float radius, float t);
			void		RegisterSensorRemoval(const Grid::LCellIdI&center, float radius, float t);

			/**
			Temporarily inserts a spherical sensor as to visualize the impact of a doomsday crater
			*/
			void		RegisterExplosionCrater(const Grid::HCellIdI&center, float radius, float t);

			bool		CanCreateMarker(bool priveleged) const { return markers.Count() < (priveleged ? Marker::MaxMarkersPerFaction*16 : Marker::MaxMarkersPerFaction); }
			Marker&		CreateMarker(const TUniqueID&id, bool priveleged, Marker::Type type, const TUniqueID&referenceID);
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


		#if TECHNITES
			virtual bool	IsTechniteFaction() const { return false; }

		#endif
		virtual void	OnKill() {};

	};

		//extern Buffer0<POwner>	owners;



	#ifndef CLIENT
		void	ChangeCompoundStorage(Compound&compound, MobileEntity*originatingAt, float atTime, Resource res, int change);

		void	ChangeStorage(WorldEntity&e, float atTime, const ResourceVector&max, ResourceVector&storage, Resource res, int change);
		void	ChangeStorage(WorkingStructure&ws, float atTime, Resource res, int change);
		void	ChangeStorage(MobileEntity&v, float atTime, Resource res, int change);
		void	MapVisibility2(const PWorldEntity&e, const PWorldEntity&other, const OwnerParameter&checkForOwner, float&atTime);
		void	ChangeOwnership( const PWorldEntity&e, const OwnerParameter&newOwner, float&atTime);

		/**
		Thoroughly checks whether or not entities able to see each other are correctly referenced as such.
		This method has quadratic complexity (where n = number of entities in the world), and should
		not be called too frequently.
		Verification failure leads to program termination.
		*/
		void	VerifyVisibilityNetwork();
		bool	CanWithdrawFromStorage(const MobileEntity&v, Resource res, int demand);
		int		GetStorage(const MobileEntity&v, Resource res);

	#else
		void	ChangeOwnership( const PWorldEntity&e,const OwnerParameter&newOwner, bool isNowMine);
	#endif


	/**
	Iterates through all entities that are located in the world, including markers (markers on client only)
	Stored mobiles are not iterated
	*/
	void			ForeachWorldEntity(const std::function<void(const PWorldEntity&)>&func);

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
}



#endif

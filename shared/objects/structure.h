#ifndef objectStructureH
#define objectStructureH

#include "entity.h"
#include "entityType.h"
#ifdef HAS_WINDOW
	#include "geometry.h"
#endif

#include <io/cryptographic_hash.h>
#include "../cast.h"
#include "interface.h"
#include "hibernating.h"
#include "hangar.h"
#include "mobileEntityProducer.h"
#include "../roundNumber.h"
#include "../sharedContainer.h"
#include "description.h"

/*
In the context of this game, a structure defines a building, as opposed to a vehicle or meta entity

This file defines all building types and their interaction, operations,
and compounds (connections between buildings).
*/
namespace Objects
{


	struct TStructureNeighborhood;



	/**
	Structure representation class. This class is instanciated exactly once per occurence in the world
	plus once for its prototype.
	*/
	class Structure : public WorldEntity
	{
	public:
		/*
		New concept:
		To better support specifics of towers, hybrid structures (narrow on top of broad) are possible.
		If both broadHeight is specified less than totalHeight, narrowHeight is considered (totalHeight - broadHeight) and to be on top of broadHeight
		*/
		static void					ChangeStructCount(const Grid::LCellIdI&location, int broadHeight, int totalHeight, bool hasFoundation, int changeDirection, float atTime);
		static void					CommonRegisterStructure(const Grid::LCellIdI&location, int broadHeight, int totalHeight, bool hasFoundation, float atTime);
		static void					CommonUnregisterStructure(const Grid::LCellIdI&location, int broadHeight, int totalHeight, bool hasFoundation, float atTime);
	private:

		bool						RemoveConnectionTo(const PStructure&);
		bool						RemoveConnectionTo(Structure*);


	protected:
		BaseAttributeTable*			table;
		typedef WorldEntity			Super;
		/**/						Structure():Super(OwnerParameter(),Grid::LCellIdI(0, 0))
				{}

		/**/						Structure(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&at, bool buildSystem, float time);



	public:
		void						DefineAttributeTable(BaseAttributeTable*t)	{table = t;}

		CONSTRUCT_ENUMERATION5(Category, Resources, Defense, Factories, Infrastructure, Miscellaneous);

		/**
		Structure placement result, returned by CommonCheckStructurePlacement() and CheckPlacement()
		*/
		enum placement_t
		{
			CanPlace,		//!< All okay, nothing obstructing structure placement at the given location
			//LacksCentralEarthFoundation,
			//LacksFullFoundation,
			BadHeight,
			GroundLevelObstruction,	//!< One or more terrain volume cells block structure placement the base level
			UpperLevelObstruction	//!< One or more volume cells block structure placement on levels height+1 and above. This applies only to buildings of height 2+ 
		};

		/**
		Function to check common (theoretical) structure placement at a given location
		@param location to check
		@return Terrain condition at the given location
		*/
		static placement_t			CommonCheckStructurePlacement(const Grid::LCellIdI&location, int broadHeight, int totalHeight);

		static count_t				CountOccurrences(const OwnerParameter&o,EntityType type);


		#ifndef CLIENT


			static void				CommonFixTerrain(const WorldEntity*e, const Grid::LCellIdI&location,  int broadHeight, int totalHeight, bool signalChange, float atTime);
			virtual void			CorrectTerrain(bool signalChange, float atTime)
			{
				CommonFixTerrain(this,location,GetBroadHeight(),GetHeight(),signalChange,atTime);
			}

			virtual bool			DirectTo(CursorMode mode, Ctr::ArrayRef<Grid::LCellIdI> waypoints, bool detailedPath, bool leftClick = false) override	{return false;}

		#endif



		#ifndef ONE_COMPOUND_PER_FACTION
			UINT32					compoundID = 0xFFFFFFFF;	//!< ID of the compound this structure is part of. Do not change (is recomputed automatically). Cannot make private at this point
		#endif

		

		#ifdef HAS_WINDOW

			/**
			Standard connector geometry import. Loaded during StaticSetup()
			*/
			static Geometry			standardConnector;
		#endif



		/**
		Currently not used. Supposed to hold the index section after the local mesh has been embedded into a larger vertex/index-buffer
		*/
		//TIntRange<UINT32>			compiledSection;
	

		/**
		List of all structures, the local structure is connected to. Symmetric. The connected structures must have a link back to the local structure.
		*/
		Ctr::Buffer0<WStructure>				connectedTo;
		#ifdef HAS_WINDOW
			/**
			Indicates whether or not the local structure has changed since the last shape recompilation. Cleared by StructureShape::RecompileConnectors()
			*/
			bool						changed;

			/**
			Type-shared appearance
			*/
			PClientAttachment		shape;
			/**
			Compiled shape of all connectors.
			Only some connections to neighbors are compiled into the local shape.
			Others are compiled by the remote end.
			*/
			PClientAttachment		connectorShape;
		#endif




		/**
		Updates the internal system and inverse matrices. Usually not necessary unless the structure somehow relocated
		*/
		void						UpdateAutomaticSystem();

		bool						IsConnectedTo(const PStructure&)	const;
		bool						IsConnectedTo(const Structure*) const;

		/**
		Retrieves a reference to the compound that this structure is part of.
		Under normal circumstances all structures are part of some compound, but (when ONE_COMPOUND_PER_FACTION is not defined)
		during transition phases this might not be the case.
		When doubt (and with ONE_COMPOUND_PER_FACTION not defined) first invoke RefreshCompoundCoverageIfNeeded()
		*/
		Compound&					GetCompound() const;


		virtual bool				IsStructure() const override {return true;}

		/**
		Indicates that this structure stands atop a foundation (mostly visualization only)
		*/
		virtual bool				HasFoundation() const {return true;}

		static void					StaticSetup();	//!< Invoked once before all other static setup methods are called.
		#ifndef CLIENT
			virtual	void			SetupSystem() override {UpdateAutomaticSystem();}
		#endif
		/**
		Signals that a new neighbor has been constructed in proximity of the local instance.
		Typical behavior is to check validity of a connection (via ConnectionPossibleTo()) and then insert the a new link into @a connectedTo
		Invokes OnNewConnection() on the local and remote instance, if a new connection was established.
		@deprecated
		*/
		//virtual void				SignalNewNeighbor(const PStructure&);
		/**
		Signals that a neighbor in proximity of the local instance has been destroyed.
		Typical behavior is to check for existing connections and remove them from @a connectedTo where necessary
		Invokes OnConnectionLost() on the local instance, if a connection was removed.
		*/
		virtual void				SignalLostNeighbor(const PStructure&, float&atTime);
		/**
		Signalling method invoked by default implementations of SignalEnvironmentChange() and SignalNewNeighbor()
		Invoked whenever a new direct or wireless connection has been established. Called before the new connection is inserted into @a connectedTo
		@param acrossHighCell CellId of the high-resolution cell between the local instance and the specified other instance. May be used to detect terrain state. 0 if the new connection is wireless
		*/
		virtual void				OnNewConnection(const PStructure&, UINT32 acrossHighCell, float atTime) {};
		/**
		Signalling method invoked by default implementations of SignalEnvironmentChange() and SignalLostNeighbor()
		Invoked whenever an existing connection has been lost.
		@param acrossHighCell CellId of the high-resolution cell between the local instance and the specified other instance. May be used to detect terrain state.
		*/
		virtual void				OnConnectionLost(const PStructure&, UINT32 acrossHighCell, float atTime) {};
		/**
		Updates possible connections, and checks if the building survives the terra-forming operation
		*/
		virtual bool				SignalEnvironmentChange(const TStructureNeighborhood&neighborhood, float atTime);
		/**
		Checks whether or not the local structure instance can create a link to the specified second structure instance.
		@param other Pointer to the neighboring structure. Must be an immediate neighbor, but can be one level above or below the local instance
		@param acrossHighCell CellId of the high-resolution cell between the local instance and the specified other instance. May be used to detect terrain intersections. Invalid/0 when not using hybrid grid
		*/
		virtual bool				ConnectionPossibleTo(const PStructure&other, UINT32 acrossHighCell)	{ return false; }
		/**
		Checks if the local structure is currently building anything, and queries its progress
		@param outProgress Building progress, if any. Remains unchanged if not building
		@return true, if the local structure is currently building (should be true regardless of being enabled), false otherwise
		*/
		virtual bool				IsBuilding(std::pair<int,int>&outProgress) const {return false;}

		/**
		Checks whether or not the local structure is an instance of SkyPort.
		Obviously, a dynamic cast would do the same, but at much higher computation cost.
		*/
		virtual bool				IsSkyPort() const {return false;}

		virtual bool				IsSkyLinkStructure() const {return false;}

		#ifndef CLIENT
			/**
			Determines whether or not the local structure will connect to structures of different ownership
			*/
			virtual bool			ConnectsToDifferentOwnership() const { return false;}
		#else
			inline bool				ConnectsToDifferentOwnership() const { return false;}

		#endif

		/**
		Converts the result of placement tests to string
		*/
		static String				ToString(placement_t);

		/**
		Checks if placement of an instance of class at the specified location is possible.
		The configuration of the local instance should not have an influence on the result, only its class should.
		*/
		virtual placement_t			CheckPlacement(const Grid::LCellIdI&location) = 0;

		/**
		Signals that the local structure will be removed. Overrides the WorldEntity method
		Base version also notifies all connected structures of the removal, so Super::OnDestroy()
		should be called by overriding implementations at some point.
		*/
		virtual void				OnDestroy(RemovalType, float&atTime) override;
		virtual void				OnConstructionFinished(float&atTime) override;
		virtual void				PrototypeSetup() override;
		virtual BaseAttributeTable*	GetTable() const override
		{
			ASSERT_NOT_NULL1__(table,GetTypeName());
			return table;
		}


		/**
		Retrieves the height of the local structure that it is broad in. The remainder of GetHeight() is considered narrow.
		The result must be between 0 and GetHeight() - both inclusive.
		Most structures return either 0 or GetHeight(). Only towers feature a mixture of broad and narrow.
		*/
		virtual int					GetBroadHeight() const {return GetHeight();}

		/**
		Erases all connection to other structures in the neighborhood. Since this may cause an inconsistent situation it is
		generally not recommended to call this method manually. OnDestroy() will call this method, as will methods that
		alter the ownership of entities.
		*/
		void						DisconnectFromNeighborhood(float&atTime);

		/**
		Fetches the category of the local structure
		*/
		virtual Category			GetCategory() const = 0;

		#ifdef HAS_WINDOW

		//	#ifndef ONE_COMPOUND_PER_FACTION

			/**
			Retrieves the geometry to use for a specific connection during standard recompilation
			*/
			virtual const Geometry*		GetSourceForConnection(const PStructure&connected)	{return &standardConnector;}

		//	#else
		//	virtual const Geometry*		GetSourceForConnection(const PStructure&connected)	{return nullptr;}

		//	#endif
		#endif

	};




	/**
	Template base class for structures that do not adhere to the WorkingStructure specification, but still
	use some common logic (e.g. automatic localizableShape loading and so forth).
	OtherStructure does, explicitly, not inherit WorkingStructure.
	WorkingStructure provide mechanics to handle resource distribution, and structure enabling/disabling.
	OtherStructure derivatives are expected to be always on.

	@param Derivative Name of the class that inherits from this class
	*/
	template <typename Derivative>
		class OtherStructure : public Structure
		{
			typedef Structure Super;
		protected:
			/**/					OtherStructure<Derivative>(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&location, bool buildSystem, float atTime):Super(table,owner,location,buildSystem,atTime)
			{ 
				#ifdef HAS_WINDOW
					Super::shape = staticShape;
				#endif
			}
			/**/					OtherStructure<Derivative>()
			{ 
				#ifdef HAS_WINDOW
					Super::shape = staticShape;
				#endif		
			}	//global prototype
		public:
			static const std::shared_ptr<Derivative>		globalPrototype;
			#ifdef HAS_WINDOW
				static CommandContainer	commands;

				/**
				Render-ready, object-space instruction to be used for all instances of this structure type
				*/
				static PClientAttachment	staticShape;
				/**
				Intersectable, object-space geometry of all instances of this structure type
				*/
				static Geometry			geometry;
			#endif
									
			virtual void				PrototypeSetup()	override;



			virtual PWorldEntity		CreateNewInstance(const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) override { return PWorldEntity(new Derivative(table,owner, location,atTime)); }
			#ifdef HAS_WINDOW
				virtual bool			TestIntersection(const TVec3<>&b, const TVec3<>&d, float & distance) const override {return Super::TestIntersection(geometry,b,d,distance);}
				virtual void			VisitAllCommands(FCommandVisitor visitor, bool force) override {commands.VisitMembers(visitor,force);};

			#endif
			virtual bool				ConnectionPossibleTo(const PStructure&other, UINT32 acrossHighCell)	override	{return true;}



			virtual placement_t			CheckPlacement(const Grid::LCellIdI&location) override
			{
				return CommonCheckStructurePlacement(location,GetBroadHeight(), GetHeight());
			}
		};

	

	/**
	Minimal structure used to connect distant structures into a single compound.
	Effective only if ONE_COMPOUND_PER_FACTION is not defined.
	Otherwise instances are kept for backwards compatibility, but cannot be newly constructed, and are practically without function
	*/
	class Connector : public OtherStructure<Connector>
	{
	private:
		typedef OtherStructure<Connector>		Super;

		friend class Super;
		/**/						Connector() {}
		/**/						Connector(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime);
	public:


		virtual String				GetTypeName() const override { return "Connector"; };
		virtual EntityType			GetType() const override {return ConstName::Type_Connector;}


		//virtual int					GetConstructionCost() const override {return 5;}
		#ifndef NO_HIBERNATION
			virtual int				GetUplinkTraffic() const override {return 1;}
		#endif

		#ifdef HAS_WINDOW
			virtual void			DescribeType(Description&label, UINT32 descFlags) override
			{
				label.BeginBrief();
				Super::DescribeType(label,descFlags);
				label.MarkBriefEnd();
				label << "Connectors establish links between structures that are too distant from one another to otherwise connect. Connected structures form compounds and share their input/output."
				;
			}
		#endif

		//Structure overrides
		virtual int					GetBroadHeight() const {return 0;}
		virtual bool				HasFoundation() const override {return false;}
		virtual Category			GetCategory() const override {return Category::Infrastructure;}
	};

	/**
	Base interface for structures that perform operations per frame, and directly participate in resource production/consumption.
	While all structures connect and implicitly form compounds, only WorkingStructures are stored in compounds and evaluated during the resource
	cycle.
	WorkingStructures can/will hibernate. Other structures can not.
	*/
	class WorkingStructure : public Structure
		#ifndef NO_HIBERNATION
			, public Hibernating
		#endif
	{
		typedef Structure			Super;
		bool						isEnabled;

		#ifndef CLIENT
			ResourceVector			overproduction;
			friend class Compound;
		#endif

	protected:
		/**/						WorkingStructure() : isEnabled(false)
		{}	//global prototype
		/**/						WorkingStructure(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&location, bool buildSystem, float atTime) 
									: Super(table,owner,location,buildSystem,atTime),isEnabled(false)
		{};

		#ifndef CLIENT
			virtual void			OnEnabledChange() {}
		#endif

	public:
		/**
		Checks whether or not the conditions are given to enable the local structure
		*/
		virtual bool				CanEnable() const	{return true;}
		
		#ifndef CLIENT
			enum force_t
			{
				EnableIfPossible,
				ForceEnable
			};
			/**
			Alters the enabled-state of the local structure
			@param refreshCompounds Causes recalculation of all compounds (if anything changed) and refreshing of the local compound.
									Setting this true may cause the change to fail, if the local compound cannot provide the required resources
			@param force If set to ForceEnable, will enable the structure even if such is not actually possible at this time. Use only during transient situations where compounds are not fully there yet
			@param signalStructure Calls OnEnabledChange() on the local structure if the enable-state changed
			@return true if the local structure could be enabled. Failure may happen if CanEnable() returns false and force is set to EnableIfPossible, 
				or if refreshCompounds is set and the local compounds cannot supply required resources
			*/
			bool					SetEnabled(bool b, float&atTime, bool refreshCompounds=true, force_t force=EnableIfPossible, bool signalStructure=true);
			void					SetEnabledOverride(bool b, bool registerChange=true);
		#else
			void					ApplyEnableChangeFromServer(bool b,bool temporary=false);
		#endif

		bool						IsEnabled() const {return isEnabled;}


		#ifndef NO_HIBERNATION
			/**
			Updates the local hibernation state
			@param hibernate Set true to enter hibernation, false otherwise. If the local structure's hibernation state already matches the given value, then nothing will happen
			*/
			void					Hibernate(bool hibernate, float atTime)
			{
				Hibernating::UpdateHibernation(this,hibernate, atTime);
			}
		#endif


		/**
		Base configuration of the local structure that specifies whether or not the local structure would like
		to be enabled automatically after instantiation in the world.
		Evaluated during InsertConstructed()
		*/
		virtual bool				WouldLikeToEnable() const {return false;}




		/**
		Checks whether or not the local structure could be enabled at this time.
		Returns true, if it already is, otherwise checks whether the local compound resource vector can supply the local structure.
		Can't be const because extensive check temporarly enables the local structure.
		@param ignoreCanEnable If set, CouldEnable() will ignore the result of the local CanEnable() method. This can be useful when determining whether a structure is idle
		*/
		bool						CouldEnable(bool ignoreCanEnable=false);

		#ifndef CLIENT
			static const index_t	FinalLeftoverIteration = 4;	//!< Index of the final iteration during ProcessLeftovers()
			/**
			Forwards oversupply or demand to the local structure. If a resource value is above zero, it indicated oversupply, and the local structure should check to store part or all of it, and change @a v accordingly.
			If the value is negative it indicates overdemand, and the local structure should check to compensate by reducing its own storage, and change @a accordingly.

			@param v Resources to store. If any are stored this vector must be updated accordingly
			@param iteration Update iteration (0 to @a FinalLeftoverIteration ), indicates the storage iteration. High significance demand should be filled at iteration=0 or 1, other at 2 or 3. iteration=4 should always be consumed
			*/
			virtual void			ProcessLeftovers(ResourceVector&v, index_t iteration, float atTime)	{}
			/**
			Signals that a new round has started and production should commence. Tick() is not executed if the local structure is disabled or unstable
			@param v Current resource balance vector. Supply should be added, demand subraced. No checks to the current values should be performed
			*/
			virtual void			Tick(ResourceVector&v);

			virtual void			OnSleepChange(float&t) override;
		#else
			#ifdef HAS_WINDOW
				virtual void		DescribeType(Description&label, UINT32 descFlags) override;
			#endif

		#endif

		/**
		Retrieves the potential resource consumption/generation vector. Ignores whether or not the local structure is enabled
		@param[out] balance Per-round supply and demand (if enabled) should go here. Values can be positive (for supply) or negative (for demand). The specified vector must be set to all 0 when passed to this method.
		@param[out] storageCurrent Current storage state of the local structure. Values must be non-negative. The specified vector must be set to all 0 when passed to this method.
		@param[out] storageMax Maximum storage capacity of the local structure. Values must be non-negative. The specified vector must be set to all 0 when passed to this method.
		*/
		virtual void				GetResourceVector(ResourceVector&balance, ResourceVector&storageCurrent, ResourceVector&storageMax) const = 0;
		/**
		Applies the given ResourceVector as storage state to the local instance.
		Storage is not standardized. This method expects the overriding instance to completely flush its current storage and apply
		the given vector. It may cap to its local maximum though.
		@param vec Storage vector to apply. All values must be non-negative
		*/
		virtual void				SetResourceStorage(const ResourceVector&vec)	{};
		virtual bool				IsNexusPort() const {return false;}
		virtual bool				IsLaboratory() const {return false;}


		//WorldEntity overrides:
		#ifndef NO_HIBERNATION
			virtual const Hibernating*	ToHibernating() const override {return this;}
		#endif
		virtual float				GetActivityLevel() const override {float rs = isEnabled; if (IsSleeping()) rs = (rs + 0.5f) * 0.5f; return rs;}
		virtual bool				IsWorkingStructure() const override {return true;}


		virtual bool				IsMine() const {return false;}
	};

	/**
	Template helper class for WorkingStructures. Most derivatives of WorkingStructure inherit this class to provide a
	globalPrototype instance, and automatically load required resources into type-local static variables.

	@param Derivative Name of the class that inherits from this class
	*/
	template <typename Derivative>
		class WorkingStructureT : public WorkingStructure
		{
			typedef WorkingStructure	Super;
		protected:
			/**/						WorkingStructureT(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime);
			/**/						WorkingStructureT()
			{
				#ifdef HAS_WINDOW
					Super::shape = staticShape;
				#endif
			}	//global prototype


		public:
			static const std::shared_ptr<WorkingStructureT<Derivative> >		globalPrototype;
			#ifdef HAS_WINDOW
				static CommandContainer	commands;

				/**
				Render-ready, object-space instruction to be used for all instances of this structure type
				*/
				static PClientAttachment	staticShape;
				/**
				Intersectable, object-space geometry of all instances of this structure type
				*/
				static Geometry			geometry;
			#endif


			virtual void				PrototypeSetup()	override;


			virtual bool				ConnectionPossibleTo(const PStructure&other, UINT32 acrossHighCell)	override;
			virtual placement_t			CheckPlacement(const Grid::LCellIdI&location) override;
			virtual PWorldEntity		CreateNewInstance(const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) override;

			#ifdef CLIENT
				#ifdef HAS_WINDOW
					virtual bool		TestIntersection(const TVec3<>&b, const TVec3<>&d, float & distance) const override {return Super::TestIntersection(geometry,b,d,distance);}
					virtual void		VisitAllCommands(FCommandVisitor visitor, bool force) override {commands.VisitMembers(visitor,force);};
				#endif
			#endif
		};


	class SkyLinkStructure
	{
	public:
		#ifndef NO_SATELLITES
			WSkyHub					connectedToHub;
		#endif

		virtual WorkingStructure*	CastToStructure() = 0;
	};

	#ifndef NO_SATELLITES
		class SkyPort : public WorkingStructureT<SkyPort>, public MobileEntityProducer, public SkyLinkStructure
		{
		public:
		protected:
			typedef WorkingStructureT<SkyPort>	Super;


			friend class Super;
			/**/						SkyPort():MobileEntityProducer(ConstructionSpeed) {MobileEntityProducer::SetCommandOffset(int2(3,1));}
			/**/						SkyPort(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime)
											:Super(table,owner,location,atTime), MobileEntityProducer(ConstructionSpeed){MobileEntityProducer::SetCommandOffset(int2(3,1));}
		public:
			static const int			ConstructionSpeed = 5;
			static const int			EnergyConsumption = 10;


			//BaseEntity overrides:
			virtual String				GetTypeName() const override { return "Sky Port"; };
			virtual ConstName			GetCTypeName() const override {return ConstName::Type_SkyPort;}

			//WorkingStructure overrides:
			virtual void				GetResourceVector(ResourceVector&v,ResourceVector&, ResourceVector&) const override;

			//WorldEntity overrides:
			virtual int					GetHeight() const override {return 2;}
			virtual void				PrototypeSetup() override;
			//virtual int					GetConstructionCost() const override {return 40;}
			#ifndef NO_HIBERNATION
				virtual int				GetUplinkTraffic() const override {return 10;}
				virtual int				GetUplinkPriority() const override {return 750;};	//not much point going far beyond generators. if they shut off, this thing shuts off, too
			#endif
			virtual bool				AwaitsUserInteraction() const override;

			#ifndef CLIENT
				virtual void			OnSleepChange(float&t) override;
				virtual void			OnEnabledChange() override;

				//virtual void			OnConstructionFinished(float atTime) override;	//has to register with owner	//some day
				//virtual void			OnDestroy(float atTime) override;	//has to unregister with owner	//some day
				virtual void			Tick(ResourceVector&) override;

			#endif
			#ifdef HAS_WINDOW
				virtual float			GetSoundLevel() const {return IsEnabled() ? GetActivityLevel() : 0.f;}
				virtual void			DescribeType(Description&label, UINT32 descFlags) override
				{
					label.BeginBrief();
					Super::DescribeType(label,descFlags);
					//label << " Max Vehicles per Store: "+String(MaxVehiclesPerStore);
					label.MarkBriefEnd();
					label << "Sky ports provide a foundation for each one sky hub. Periodically, "+String(ResourceTag(Resource::Fuel))+"fuel is transferred up, to supply the hub and its satellites."
										" Failure to supply the required "+String(ResourceTag(Resource::Fuel))+" will result in satellite losses. An unobstructed path into orbit is required to maintain the connection";
				}
				virtual void			VisitAllCommands(FCommandVisitor visitor, bool force) override;
				virtual void			OnSelect()	override;
				virtual void			OnDeselect() override;

			#endif
			virtual const MobileEntityProducer*CastToProducer() const override {return this;}

			//MobileEntityProducer overrides
			#ifndef CLIENT
				virtual void			SignalNewProductionEnqueued() override {float t = 0; SetEnabled(true,t);}
				virtual bool			DirectTo(CursorMode, Ctr::ArrayRef<Grid::LCellIdI> waypoints, bool detailedPath, bool leftClick = false) override;
			#endif
			virtual bool				CanProduce(const PWorldEntity&e) const override;


			//Structure overrides:
			virtual bool				WouldLikeToEnable() const;
			virtual bool				IsBuilding(std::pair<int,int>&outProgress) const override
			{
				return MobileEntityProducer::IsBuilding(outProgress);
			}
			virtual bool				IsSkyPort() const override {return true;}
			virtual bool				IsSkyLinkStructure() const override {return true;}
			virtual Category			GetCategory() const override {return Category::Factories;}


			//SkyLinkStructure overrides:
			virtual WorkingStructure*	CastToStructure() override {return this;}


		};
	#endif
	/**
	Resource supplier, and winning condition, NexusPorts are located on the 12 pentagons around the planet.
	In order to capture a NexusPort, one must build any one structure next to it. If the last structure is lost, the NexusPort
	will change ownership back to the Nexus faction. Only then can it be captured by another player.
	When switched on, NexusPorts count down a resistence value until they count as a winning condition.
	Whenever ownership changes, or energy supply is cut, this value resets
	*/
	class NexusPort : public WorkingStructureT<NexusPort>, public SkyLinkStructure
	{
	private:
		typedef WorkingStructureT<NexusPort>			Super;
		typedef NexusPort		Self;
		friend class Super;
		/**/						Self(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) 
									: Super(table,owner,location,atTime),resistence(-1)	{}
		/**/						Self()	{ }	//global prototype

		#ifndef CLIENT
			virtual void			OnEnabledChange() override;
		#endif

		//NaturalResourceVector		naturalResources;
	public:
		static const int			EnergyConsumption = 10;
		int							resistence;
		#ifndef CLIENT
			RoundNumber				lastSpawnRound=RoundNumber(
											0xFFFFFFF0
											);	//last time this nexus spawned nexus units
		#endif
		static int					ResistenceRounds;


		virtual bool				WouldLikeToEnable() const override {return true;}
		virtual bool				ConnectionPossibleTo(const PStructure&other, UINT32 acrossHighCell)	override;
		virtual void				OnNewConnection(const PStructure&other, UINT32 acrossHighCell, float atTime) override;
		virtual void				OnConnectionLost(const PStructure&, UINT32 acrossHighCell, float atTime) override;
		#ifndef CLIENT
			virtual bool			ConnectsToDifferentOwnership() const override;
			void					OnLoad();

			virtual void			Tick(ResourceVector&) override;
			virtual void			OnConstructionFinished(float&atTime)	{Super::OnConstructionFinished(atTime); SetEnabled(false,atTime,false);}
		#else
			#ifdef HAS_WINDOW
				virtual void		DescribeType(Description&label, UINT32 descFlags) override;
			#endif
		#endif

		virtual bool				IsNexusPort() const override {return true;}




		//virtual int					GetConstructionCost() const override {return 0;}
		#ifndef NO_HIBERNATION
			virtual int				GetUplinkTraffic() const override {return 10;}
			virtual int				GetUplinkPriority() const override {return 750;};	//not much point going far beyond generators. if they shut off, this thing shuts off, too
		#endif

		//virtual int					GetMaxHealth() const override {return 1;}

		virtual String				GetTypeName() const override { return "Nexus Port"; };
		virtual EntityType			GetType() const override {return ConstName::Type_NexusPort;}

		virtual bool				IsIndestructible() const	override {return true;};
		virtual void				GetResourceVector(ResourceVector&v, ResourceVector&, ResourceVector&) const override
		{
			if (BaseGrid::IsIsolatedCorner(location.stackID))
			{
				v.level[Resource::Minerals] = 50;	
				v.level[Resource::Fuel] = 55;	
				v.level[Resource::Uranium] = 10;
				v.level[Resource::Energy] = -EnergyConsumption;
			}
			else
			{
				v.level[Resource::Minerals] = 10;
				v.level[Resource::Fuel] = 10;	
				v.level[Resource::Uranium] = 5;
				v.level[Resource::Energy] = -EnergyConsumption;
			}
		}


		//Structure overrides:
		virtual bool				IsSkyLinkStructure() const override {return true;}
		virtual Category			GetCategory() const override {return Category::Resources;}	//not needed

		//SkyLinkStructure overrides:
		virtual WorkingStructure*	CastToStructure() override {return this;}

	};
	
	class Generator : public WorkingStructureT<Generator>
	{
		friend class WorkingStructureT<Generator>;
		/**/						Generator(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) 
									: Super(table,owner,location,atTime)	{}
		/**/						Generator()	{ }	//global prototype
	public:
		static const int FuelCostPerRound = 10;
		typedef WorkingStructureT<Generator>	Super;
		WReactorCore				connectedToReactor;


		int							GetEnergyProduction() const;

		#ifdef HAS_WINDOW
			static Geometry			reactorConnector;
				
			virtual const Geometry*	GetSourceForConnection(const PStructure&connected)	override {return connectedToReactor.lock() == connected ? &reactorConnector : &standardConnector;}
		#endif

		//virtual int					GetConstructionCost() const override {return 40;}
		#ifndef NO_HIBERNATION
			virtual int				GetUplinkTraffic() const override {return 5;}
			virtual int				GetUplinkPriority() const override {return 750;};
		#endif

		virtual bool				WouldLikeToEnable() const override {return false;}


		virtual String				GetTypeName() const override { return "Generator"; };
		virtual EntityType			GetType() const override {return ConstName::Type_Generator;}

		virtual void				GetResourceVector(ResourceVector&v,ResourceVector&, ResourceVector&) const override	{v.level[Resource::Fuel] = -FuelCostPerRound; v.level[Resource::Energy] = GetEnergyProduction(); }

		virtual void				OnNewConnection(const PStructure&other, UINT32 acrossHighCell, float atTime) override;
		virtual void				OnConnectionLost(const PStructure&, UINT32 acrossHighCell, float atTime) override;
		virtual Category			GetCategory() const override {return Category::Resources;}


		#ifdef HAS_WINDOW
			virtual void			DescribeType(Description&label, UINT32 descFlags) override;
		#endif

	};

	class ReactorCore : public WorkingStructureT<ReactorCore>
	{
		typedef WorkingStructureT<ReactorCore>	Super;
		friend class Super;
		/**/						ReactorCore(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) 
									: Super(table,owner,location,atTime)	{}
		/**/						ReactorCore()	{ }	//global prototype
	public:
		static const int	UraniumCost = 5;
		static const int	EnergyOutput = 40;

		//virtual int					GetConstructionCost() const override {return 120;}
		#ifndef NO_HIBERNATION
			virtual int				GetUplinkTraffic() const override {return 15;}
			virtual int				GetUplinkPriority() const override {return 750;};
		#endif

		virtual bool				WouldLikeToEnable() const override {return true;}
		virtual void				GetResourceVector(ResourceVector&v,ResourceVector&, ResourceVector&) const override	{v.level[Resource::Uranium] = -UraniumCost; v.level[Resource::Energy] = EnergyOutput; }

		virtual int					GetHeight() const override {return 2;}
		virtual String				GetTypeName() const override { return "Reactor Core"; };
		virtual EntityType			GetType() const override {return ConstName::Type_ReactorCore;}
		virtual Category			GetCategory() const override {return Category::Resources;}

		#ifdef HAS_WINDOW
			virtual const Geometry*	GetSourceForConnection(const PStructure&connected)	override {PGenerator gen = To<Generator>::DynamicCast(connected); return gen && gen->connectedToReactor.lock().get() == this ? &Generator::reactorConnector : &standardConnector;}

			virtual void			DescribeType(Description&label, UINT32 descFlags) override;
		#endif

		//WorldEntity overrides:
		virtual void				PrototypeSetup()	override;
	};




	

	void	ChangeStorage(WorkingStructure&ws, float atTime, Resource res, int change);	//defined in faction.h/.cpp

	




	extern SharedContainer<Structure>		structures;
	#ifdef CLIENT
		typedef std::pair<WSkyLinkStructure,WWorkingStructure>	TLinkReference;
		extern Buffer0<TLinkReference>		skyLinks;	//also included in structures, but filtered for quicker access
	#endif




	/**
	Recomputes structure-network connections. Usually called automatically where needed.
	*/
	void			RefreshCompoundCoverage( float atTime);

	#ifndef CLIENT
		extern bool		compoundCoverageChanged;
		bool			RefreshCompoundCoverageIfNeeded( float atTime);
	#endif




	

	template <typename Derivative>
	/*static*/ const std::shared_ptr<Derivative>						OtherStructure<Derivative>::globalPrototype(new Derivative());


	template <typename Derivative>
	/*static*/ const std::shared_ptr<WorkingStructureT<Derivative> >		WorkingStructureT<Derivative>::globalPrototype(new Derivative());

	#ifdef HAS_WINDOW
		template <typename Derivative>
		/*static*/ PClientAttachment	WorkingStructureT<Derivative>::staticShape;

		template <typename Derivative>
		/*static*/ PClientAttachment	OtherStructure<Derivative>::staticShape;

		template <typename Derivative>
		/*static*/ Geometry				WorkingStructureT<Derivative>::geometry;
		template <typename Derivative>
		/*static*/ Geometry				OtherStructure<Derivative>::geometry;

		template <typename Derivative>
		/*static*/ CommandContainer		WorkingStructureT<Derivative>::commands;
		template <typename Derivative>
		/*static*/ CommandContainer		OtherStructure<Derivative>::commands;
	#endif



	template <typename Derivative>
		/*virtual override*/ PWorldEntity			WorkingStructureT<Derivative>::CreateNewInstance(const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime)
		{
			return PWorldEntity(new Derivative(table,owner,location,atTime));
		}





	template <typename Derivative>
		/*virtual override*/	void			WorkingStructureT<Derivative>::PrototypeSetup()
		{
			Super::PrototypeSetup();
		}


	template <typename Derivative>
		/*virtual override*/	void			OtherStructure<Derivative>::PrototypeSetup()
		{
			Super::PrototypeSetup();
			#ifdef HAS_WINDOW
				commands.baseCoords = int2(3,0);
			#endif

		}
	
	template <typename Derivative>
		WorkingStructureT<Derivative>::WorkingStructureT(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) : Super(table,owner,location, true, atTime)
		{
			#ifdef HAS_WINDOW
				Super::shape = staticShape;
			#endif
		}




	template <typename Derivative>
		/*virtual override*/ bool				WorkingStructureT<Derivative>::ConnectionPossibleTo(const PStructure&other, UINT32 acrossHighCell)
		{
			return true;
			//return other->location.height == this->location.height;// && dynamic_cast<Mine*>(other.get()) != nullptr;
		}
	template <typename Derivative>
		/*virtual override*/ Structure::placement_t				WorkingStructureT<Derivative>::CheckPlacement(const Grid::LCellIdI&location)
		{
			return CommonCheckStructurePlacement(location,GetBroadHeight(), GetHeight());
		}



	inline PStructure		ToStructure(const PWorldEntity&e)
	{
		if (!e || !e->IsStructure())
			return PStructure();
		return To<Structure>::Cast(e);
	}

	inline PWorkingStructure		ToWorkingStructure(const PWorldEntity&e)
	{
		if (!e || !e->IsWorkingStructure())
			return PWorkingStructure();
		return To<WorkingStructure>::Cast(e);
	}




}





#endif

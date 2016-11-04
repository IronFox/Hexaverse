#ifndef objectStructureH
#define objectStructureH

#include "entity.h"
#ifdef HAS_WINDOW
	#include "geometry.h"
	#include "../../client/ExtRenderer/DirectX11.h"
	#include "visualSelection.h"

#endif

#include <io/encryption.h>
#include "../cast.h"
#include "interface.h"
#include "hibernating.h"
#include "hangar.h"
#include "mobileEntityProducer.h"
#include "../roundNumber.h"

/*
In the context of this game, a structure defines a building, as opposed to a vehicle or meta entity

This file defines all building types and their interaction, operations,
and compounds (connections between buildings).
*/
namespace Objects
{
	#ifdef HAS_WINDOW
		namespace DX11 = Engine::DX11;
	#endif


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
		static void					ChangeStructCount(Grid::WorldData&world, const Grid::LCellIdI&location, int broadHeight, int totalHeight, bool hasFoundation, int changeDirection, float atTime);
		static void					CommonRegisterStructure(Grid::WorldData&world, const Grid::LCellIdI&location, int broadHeight, int totalHeight, bool hasFoundation, float atTime);
		static void					CommonUnregisterStructure(Grid::WorldData&world, const Grid::LCellIdI&location, int broadHeight, int totalHeight, bool hasFoundation, float atTime);
	private:

		bool						RemoveConnectionTo(const PStructure&);
		bool						RemoveConnectionTo(Structure*);


	protected:
		typedef WorldEntity			Super;
		/**/						Structure():Super(OwnerParameter(),Grid::LCellIdI(0, 0))
				#ifdef HAS_WINDOW
					, localizableShape(nullptr)
				#endif
				{}

		/**/						Structure(const OwnerParameter&owner, const Grid::LCellIdI&at, bool buildSystem, float time);



	public:

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

		static count_t				CountOccurrences(const OwnerParameter&o,ConstName type);


		static void					InvalidateTerrain(const Grid::LCellIdI&location, float atTime);

		#ifndef CLIENT


			static void				CommonFixTerrain(const WorldEntity*e, const Grid::LCellIdI&location,  int broadHeight, int totalHeight, bool signalChange, float atTime);
			virtual void			CorrectTerrain(bool signalChange, float atTime)
			{
				CommonFixTerrain(this,location,GetBroadHeight(),GetHeight(),signalChange,atTime);
			}

			virtual bool			DirectTo(CursorMode mode, ArrayRef<Grid::LCellIdI> waypoints, bool detailedPath, bool leftClick = false) override	{return false;}

		#endif



		#ifndef ONE_COMPOUND_PER_FACTION
			UINT32					compoundID = 0xFFFFFFFF;	//!< ID of the compound this structure is part of. Do not change (is recomputed automatically). Cannot make private at this point
		#endif

		

		#ifdef HAS_WINDOW
			static DX11::BaseInstructionBlock	standardCompiledShader, standardLocalizableShader;

			/**
			Standard connector geometry import. Loaded during StaticSetup()
			*/
			static Geometry			standardConnector;
			/**
			Container of the geometry of the local structure. Updated by Recompile()
			*/
			Geometry				localMesh;

		#endif



		/**
		Currently not used. Supposed to hold the index section after the local mesh has been embedded into a larger vertex/index-buffer
		*/
		//TIntRange<UINT32>			compiledSection;
	

		/**
		List of all structures, the local structure is connected to. Symmetric. The connected structures must have a link back to the local structure.
		*/
		Buffer0<WStructure>			connectedTo;
		#ifdef HAS_WINDOW
			/**
			Indicates whether or not the local structure has changed since the last Recompile() call
			*/
			bool						changed;
			/**
			Indicates whether or not any structure has changed since they were all checked/recompiled last time
			*/
			static bool					anyChanged;

			/**
			Temporary structure-specific rendering instruction. Updated by Recompile(). Can be used to render the local structure.
			*/
			DX11::InstructionBlock32	compiledShape,*localizableShape;
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
		#ifdef CLIENT
			#ifdef HAS_WINDOW

				/**
				Causes recompilation of @a localMesh
				Current implementation also uploads this mesh independently to @a shape, fit for rendering
				*/
				virtual void		Recompile();
			#endif
		#else
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
			/**/					OtherStructure<Derivative>(const OwnerParameter&owner, const Grid::LCellIdI&location, bool buildSystem, float atTime):Super(owner,location,buildSystem,atTime)
			{ 
				#ifdef HAS_WINDOW
					Super::localizableShape = &shape;
				#endif
			}
			/**/					OtherStructure<Derivative>()
			{ 
				#ifdef HAS_WINDOW
					Super::localizableShape = &shape;
				#endif		
			}	//global prototype
		public:
			static const std::shared_ptr<Derivative>		globalPrototype;
			#ifdef HAS_WINDOW
				static CommandContainer	commands;

				/**
				Render-ready, object-space instruction to be used for all instances of this structure type
				*/
				static DX11::InstructionBlock32	shape;
				/**
				Intersectable, object-space geometry of all instances of this structure type
				*/
				static Geometry			geometry;
			#endif
									
			virtual void				PrototypeSetup()	override;
			#ifdef HAS_WINDOW
				virtual void			PrototypeLoadGeometry() override;
			#endif



			virtual PWorldEntity		CreateNewInstance(const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) override { return PWorldEntity(new Derivative(owner, location,atTime)); }
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
	Vehicle storage class.
	Instances of this class can permanently store vehicles
	*/
	class VehicleStore : public OtherStructure<VehicleStore>, public Hangar
	{
	public:
		struct Park;
		typedef std::shared_ptr<Park>	PPark;
	private:
		typedef OtherStructure<VehicleStore>		Super;

		/**
		Store-neigbors - directly adjacent VehicleStore instances
		*/
		Buffer0<WVehicleStore>		sneighbors;

		

		void						PropagatePark(const PPark&compound);
		void						MergePark(const PPark&other);



		friend class Super;
		/**/						VehicleStore() {}
		/**/						VehicleStore(const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime);
	public:

		/**
		Shared storage state. Neighboring (also indirectly) VehicleStore instances
		share one Park instance. It tracks Park membership and the number of vehicles currently checked in.
		*/
		struct Park
		{
			Buffer0<WVehicleStore>	members;	//!< All VehicleStore instances that share this Park
			count_t					vehiclesStored = 0;	//!< Total number of vehicles stored in this park

			//TUniqueID				id;

			/**/					Park()
			{
				//id.Generate();
			}


			/**
			Iteratively checks whether or not the local Park instance contains a particular VehicleStore instance
			*/
			bool					Contains(const PVehicleStore&s)	const
			{
				for (const auto&w : members)
					if (w.lock() == s)
						return true;
				return false;
			}
		};


		PPark						park;	//!< Shared state. Should not be set if the local VehicleStore is not connected to any other VehicleStore instances
		#ifdef HAS_WINDOW
			static Geometry			interConnector;


			virtual const Geometry*	GetSourceForConnection(const PStructure&connected)	override {return To<VehicleStore>::DynamicCast(connected) && connected->location.height == location.height ? &interConnector : &standardConnector;}

		#endif
		/**
		Maximum number of vehicles that can be checked into one instance of the local class.
		Neighboring VehicleStores share free slots, and can move vehicles in zero-time (ATM)
		*/
		static const count_t		MaxVehiclesPerStore = 4;



		count_t						CountVehiclesInPark() const {return park? park->vehiclesStored : stored.Count();}
		count_t						GetParkVehicleCapacity() const {return park ? park->members.Count() * MaxVehiclesPerStore : MaxVehiclesPerStore;}



		#ifndef CLIENT
			bool					StoreInPark(const PMobileEntity&v, float&atTime);
		#else
			void					Lose(const PVehicle&v);
		#endif



		/**
		Checks if the local structure (not the park) is full (i.e. cannot store any additional vehicles)
		*/
		bool						LocalIsFull() const {return stored.Count() >= MaxVehiclesPerStore;}


		//Hangar overrides:
		virtual bool				CanStoreType(const MobileEntity&e) const override;
		virtual void				StoreLocallyFromImport(const PMobileEntity&e) override;
		#ifndef CLIENT
			virtual bool			Store(const PMobileEntity&e, float&atTime) override {return e && CanStoreType(*e) && StoreInPark(e,atTime);}
		#else
			virtual void			Lose(const PMobileEntity&v)	override;
		#endif
		virtual bool				SharesStorageWith(const PHangar&other)	const override;
		virtual void				LocalCheckout(const PMobileEntity&v, const Grid::LCellIdI&targetLocation, float&atTime) override;
		virtual count_t				GetMaxStorage() const override {return MaxVehiclesPerStore;}
		const WorldEntity&			ToEntity() const override {return *this;}
		WorldEntity&				ToEntity() override {return *this;}
		virtual void				OnDestroy(RemovalType, float&atTime) override;

		//Structure overrides:
		virtual void				OnNewConnection(const PStructure&, UINT32 acrossHighCell, float atTime) override;
		virtual void				OnConnectionLost(const PStructure&, UINT32 acrossHighCell, float atTime) override;
		virtual PHangar				CastToDestinationForType(const MobileEntity&e) override;
		virtual Category			GetCategory() const override {return Category::Miscellaneous;}


		//WorldEntity overrides:
		virtual bool				IsHangar() const {return true;}
		virtual String				GetTypeName() const override { return "Vehicle Store"; };
		virtual ConstName			GetCTypeName() const override {return ConstName::Type_VehicleStore;}
		virtual int					GetHeight() const override {return 2;}
		virtual void				PrototypeSetup() override;
		virtual int					GetConstructionCost() const override {return 40;}
		#ifndef NO_HIBERNATION
			virtual int				GetUplinkTraffic() const override {return 2;}
		#endif
		#ifdef HAS_WINDOW
			virtual void			PrototypeLoadGeometry() override;
			virtual void			DescribeType(BaseCommand::Label&label, UINT32 descFlags) override
			{
				Super::DescribeType(label,descFlags);
				label << " Max Vehicles per Store: "+String(MaxVehiclesPerStore);
				label.MarkBriefEnd();
				label.WrapAppendText("Vehicle stores can be used to store vehicles which will then enter hibernation until deployed. This structure shares storage with nearby structures of the same type."
									" A vehicle store can deploy any vehicle found within the vehicle store compound.");			
			}
			virtual void			VisitAllCommands(FCommandVisitor visitor, bool force) override;
		#endif


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
		/**/						Connector(const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime);
	public:


		virtual String				GetTypeName() const override { return "Connector"; };
		virtual ConstName			GetCTypeName() const override {return ConstName::Type_Connector;}


		virtual int					GetConstructionCost() const override {return 5;}
		#ifndef NO_HIBERNATION
			virtual int				GetUplinkTraffic() const override {return 1;}
		#endif

		#ifdef HAS_WINDOW
			virtual void			DescribeType(BaseCommand::Label&label, UINT32 descFlags) override
			{
				Super::DescribeType(label,descFlags);
				label.MarkBriefEnd();
				label.WrapAppendText("Connectors establish links between structures that are too distant from one another to otherwise connect. Connected structures form compounds and share their input/output."
				);			
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
		/**/						WorkingStructure() : isEnabled(false)	{ }	//global prototype
		/**/						WorkingStructure(const OwnerParameter&owner, const Grid::LCellIdI&location, bool buildSystem, float atTime) : Super(owner,location,buildSystem,atTime),isEnabled(false)
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
				virtual void		DescribeType(BaseCommand::Label&label, UINT32 descFlags) override;
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
			/**/						WorkingStructureT(const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime);
			/**/						WorkingStructureT()
			{
				#ifdef HAS_WINDOW
					Super::localizableShape = &shape;
				#endif
			}	//global prototype


		protected:

			#ifdef HAS_WINDOW
				struct Cmd
				{
					static void				Enable();
					static bool				IsEnabled();
				};
			#endif
		public:
			static const std::shared_ptr<WorkingStructureT<Derivative> >		globalPrototype;
			#ifdef HAS_WINDOW
				static CommandContainer	commands;

				/**
				Render-ready, object-space instruction to be used for all instances of this structure type
				*/
				static DX11::InstructionBlock32	shape;
				/**
				Intersectable, object-space geometry of all instances of this structure type
				*/
				static Geometry			geometry;
			#endif


			virtual void				PrototypeSetup()	override;
			#ifdef HAS_WINDOW
				virtual void			PrototypeLoadGeometry() override;
			#endif


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
			/**/						SkyPort(const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime):Super(owner,location,atTime), MobileEntityProducer(ConstructionSpeed){MobileEntityProducer::SetCommandOffset(int2(3,1));}
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
			virtual int					GetConstructionCost() const override {return 40;}
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
				virtual void			DescribeType(BaseCommand::Label&label, UINT32 descFlags) override
				{
					Super::DescribeType(label,descFlags);
					//label << " Max Vehicles per Store: "+String(MaxVehiclesPerStore);
					label.MarkBriefEnd();
					label.WrapAppendText("Sky ports provide a foundation for each one sky hub. Periodically, "+String(ResourceTag(Resource::Fuel))+"fuel is transferred up, to supply the hub and its satellites."
										" Failure to supply the required "+String(ResourceTag(Resource::Fuel))+" will result in satellite losses. An unobstructed path into orbit is required to maintain the connection");
				}
				virtual void			VisitAllCommands(FCommandVisitor visitor, bool force) override;
				virtual void			OnSelect()	override;
				virtual void			OnDeselect() override;

			#endif
			virtual const MobileEntityProducer*CastToProducer() const override {return this;}

			//MobileEntityProducer overrides
			#ifndef CLIENT
				virtual void			SignalNewProductionEnqueued() override {float t = 0; SetEnabled(true,t);}
				virtual bool			DirectTo(CursorMode, ArrayRef<Grid::LCellIdI> waypoints, bool detailedPath, bool leftClick = false) override;
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
		/**/						Self(const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) : Super(owner,location,atTime),resistence(-1)	{}
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
				virtual void		DescribeType(BaseCommand::Label&label, UINT32 descFlags) override;
			#endif
		#endif

		virtual bool				IsNexusPort() const override {return true;}




		virtual int					GetConstructionCost() const override {return 0;}
		#ifndef NO_HIBERNATION
			virtual int				GetUplinkTraffic() const override {return 10;}
			virtual int				GetUplinkPriority() const override {return 750;};	//not much point going far beyond generators. if they shut off, this thing shuts off, too
		#endif

		virtual int					GetMaxHealth() const override {return 1;}

		virtual String				GetTypeName() const override { return "Nexus Port"; };
		virtual ConstName			GetCTypeName() const override {return ConstName::Type_NexusPort;}

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


	class PlaneFactory : public WorkingStructureT<PlaneFactory>, public MobileEntityProducer
	{
	public:
	protected:
		typedef WorkingStructureT<PlaneFactory>	Super;


		friend class Super;
		/**/						PlaneFactory():MobileEntityProducer(ConstructionSpeed) {MobileEntityProducer::SetCommandOffset(int2(3,1));}
		/**/						PlaneFactory(const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime):Super(owner,location,atTime), MobileEntityProducer(ConstructionSpeed){MobileEntityProducer::SetCommandOffset(int2(3,1));}

		virtual bool				CanEnable() const override { return MobileEntityProducer::queue.IsNotEmpty(); }
	public:
		static const int			ConstructionSpeed = 20;
		static const int			FuelDemand = 20;
		static const int			EnergyConsumption = 20;

		static Buffer0<ConstName>	prototypes;


		//BaseEntity overrides:
		virtual String				GetTypeName() const override { return "Plane Factory"; };
		virtual ConstName			GetCTypeName() const override {return ConstName::Type_PlaneFactory;}

		//WorkingStructure overrides:
		virtual void				GetResourceVector(ResourceVector&v,ResourceVector&, ResourceVector&) const override;

		//WorldEntity overrides:
		virtual int					GetHeight() const override {return 2;}
		virtual void				PrototypeSetup() override;
		virtual int					GetConstructionCost() const override {return 200;}
		#ifndef NO_HIBERNATION
			virtual int				GetUplinkTraffic() const override {return 10;}
			virtual int				GetUplinkPriority() const override {return 750;};	//not much point going far beyond generators. if they shut off, this thing shuts off, too
		#endif
		virtual bool				AwaitsUserInteraction() const override;

		#ifndef CLIENT
			virtual void			OnSleepChange(float&t) override;

			//virtual void			OnConstructionFinished(float atTime) override;	//has to register with owner	//some day
			//virtual void			OnDestroy(float atTime) override;	//has to unregister with owner	//some day
			virtual void			Tick(ResourceVector&) override;
			virtual void			SignalLastProductionDone(float&t) override {LogEvent(EventSignificance::Common, this, "Queue is empty. Shutting off"); SetEnabled(false,t);}

		#endif
		#ifdef HAS_WINDOW
			virtual float			GetSoundLevel() const {return IsEnabled() ? GetActivityLevel() : 0.f;}
			virtual void			DescribeType(BaseCommand::Label&label, UINT32 descFlags) override
			{
				Super::DescribeType(label,descFlags);
				//label << " Max Vehicles per Store: "+String(MaxVehiclesPerStore);
				label.MarkBriefEnd();
				label.WrapAppendText("Plane factories produce planes");
			}
			virtual void			VisitAllCommands(FCommandVisitor visitor, bool force) override;
		#endif
		virtual const MobileEntityProducer*CastToProducer() const override {return this;}

		//MobileEntityProducer overrides
		#ifndef CLIENT
			virtual void			SignalNewProductionEnqueued() override {float t = 0; SetEnabled(true,t);}
		#endif
		virtual bool				CanProduce(const PWorldEntity&e) const override;


		//Structure overrides:
		virtual bool				IsBuilding(std::pair<int,int>&outProgress) const override
		{
			return MobileEntityProducer::IsBuilding(outProgress);
		}
		virtual Category			GetCategory() const override {return Category::Factories;}
	};
	
	class Generator : public WorkingStructureT<Generator>
	{
		friend class WorkingStructureT<Generator>;
		/**/						Generator(const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) : Super(owner,location,atTime)	{}
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

		virtual int					GetConstructionCost() const override {return 40;}
		#ifndef NO_HIBERNATION
			virtual int				GetUplinkTraffic() const override {return 5;}
			virtual int				GetUplinkPriority() const override {return 750;};
		#endif

		virtual bool				WouldLikeToEnable() const override {return false;}


		virtual String				GetTypeName() const override { return "Generator"; };
		virtual ConstName			GetCTypeName() const override {return ConstName::Type_Generator;}

		virtual void				GetResourceVector(ResourceVector&v,ResourceVector&, ResourceVector&) const override	{v.level[Resource::Fuel] = -FuelCostPerRound; v.level[Resource::Energy] = GetEnergyProduction(); }

		virtual void				OnNewConnection(const PStructure&other, UINT32 acrossHighCell, float atTime) override;
		virtual void				OnConnectionLost(const PStructure&, UINT32 acrossHighCell, float atTime) override;
		virtual Category			GetCategory() const override {return Category::Resources;}


		#ifdef HAS_WINDOW
			virtual void			DescribeType(BaseCommand::Label&label, UINT32 descFlags) override;
			virtual void			PrototypeLoadGeometry() override;
		#endif

	};

	class ReactorCore : public WorkingStructureT<ReactorCore>
	{
		typedef WorkingStructureT<ReactorCore>	Super;
		friend class Super;
		/**/						ReactorCore(const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) : Super(owner,location,atTime)	{}
		/**/						ReactorCore()	{ }	//global prototype
	public:
		static const int	UraniumCost = 5;
		static const int	EnergyOutput = 40;

		virtual int					GetConstructionCost() const override {return 120;}
		#ifndef NO_HIBERNATION
			virtual int				GetUplinkTraffic() const override {return 15;}
			virtual int				GetUplinkPriority() const override {return 750;};
		#endif

		virtual bool				WouldLikeToEnable() const override {return true;}
		virtual void				GetResourceVector(ResourceVector&v,ResourceVector&, ResourceVector&) const override	{v.level[Resource::Uranium] = -UraniumCost; v.level[Resource::Energy] = EnergyOutput; }

		virtual int					GetHeight() const override {return 2;}
		virtual String				GetTypeName() const override { return "Reactor Core"; };
		virtual ConstName			GetCTypeName() const override {return ConstName::Type_ReactorCore;}
		virtual Category			GetCategory() const override {return Category::Resources;}

		#ifdef HAS_WINDOW
			virtual const Geometry*	GetSourceForConnection(const PStructure&connected)	override {PGenerator gen = To<Generator>::DynamicCast(connected); return gen && gen->connectedToReactor.lock().get() == this ? &Generator::reactorConnector : &standardConnector;}

			virtual void			DescribeType(BaseCommand::Label&label, UINT32 descFlags) override;
		#endif

	};




	class VehiclePlant : public WorkingStructureT<VehiclePlant>, public MobileEntityProducer
	{
		friend class WorkingStructureT<VehiclePlant>;

        /**/						VehiclePlant(const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime);
		/**/						VehiclePlant():  MobileEntityProducer(DefaultProductionPerRound) {}//global prototype
	protected:
		virtual bool				CanEnable() const override {return MobileEntityProducer::queue.IsNotEmpty();}

	public:
		typedef WorkingStructureT<VehiclePlant>	Super;



		static const int			EnergyConsumption = 10;
		static const int			DefaultProductionPerRound = 10;


		/**
		All base components known to VehiclePlant.
		Vehicle may use these as bases
		*/
		static Buffer0<Component::VehicleBase*>
									baseComponents;
		/**
		All turret components known to VehiclePlant.
		Effectively a subset of all turret types available.
		Vehicle may use these as turrets.
		*/
		static Buffer0<Component::Turret*>
									turretComponents;


		#ifdef CLIENT
			Component::VehicleBase*	selectedBase;
			Component::Turret*		selectedTurret;
		#endif

		#ifdef CLIENT
			bool					EnqueueSelectedConstruction();
			Component::VehicleBase*	GetProductionBase() const;

			#ifdef HAS_WINDOW
				virtual void		DescribeType(BaseCommand::Label&label, UINT32 descFlags) override;
			#endif

		#else
			bool					AbortConstruction(const TUniqueID&);
			PVehicle				SpawnFinishedProductionAnywhere();
			PVehicle				Enqueue(Component::VehicleBase*, Component::Turret*);
			PVehicle				Enqueue(ConstName baseName, ConstName turretName);
			PVehicle				GetProduction() const;
			//PVehicle				CompleteProduction();
			virtual void			Tick(ResourceVector&) override;
		#endif




		//Structure overrides:
		virtual bool				IsBuilding(std::pair<int,int>&outProgress) const override { return MobileEntityProducer::IsBuilding(outProgress); }
		virtual Category			GetCategory() const override {return Category::Factories;}


		//WorldEntity overrides:
		virtual void				PrototypeSetup()	override;
		virtual int					GetConstructionCost() const override {return 80;}
		#ifndef NO_HIBERNATION
			virtual int				GetUplinkTraffic() const override {return 20;}
		#endif
		//virtual bool				IsVehiclePlant() const override {return true;}
		virtual bool				AwaitsUserInteraction() const override;
		#ifdef HAS_WINDOW
			virtual float			GetSoundLevel() const {return IsEnabled() ? GetActivityLevel() : 0.f;}
		#endif

		//MobileEntityProducer overrides
		#ifndef CLIENT
			virtual void			OnSleepChange(float&t) override;
			virtual void			SignalLastProductionDone(float&t) override {LogEvent(EventSignificance::Common, this, "Queue is empty. Shutting off"); SetEnabled(false,t);}
			virtual void			SignalNewProductionEnqueued() override {float t = 0; SetEnabled(true,t);}
			virtual bool			DirectTo(CursorMode, ArrayRef<Grid::LCellIdI> waypoints, bool detailedPath, bool leftClick = false) override;
		#endif
		virtual bool				CanProduce(const PWorldEntity&e) const override;

		#ifndef NO_HIBERNATION
			//Hibernating overrides:
			virtual int				GetUplinkPriority() const override {return 800;};
		#endif

		//BaseEntity overrides:
		virtual String				GetTypeName() const override { return "Vehicle Plant"; };
		virtual ConstName			GetCTypeName() const override {return ConstName::Type_VehiclePlant;}


		#ifdef HAS_WINDOW
			virtual void			VisitAllCommands(FCommandVisitor visitor, bool force) override;
			virtual void			OnSelect()	override;
			virtual void			OnDeselect() override;
		#endif
		virtual const MobileEntityProducer*CastToProducer() const override {return this;}


		//WorkingStructure overrides:
		virtual void				GetResourceVector(ResourceVector&v,ResourceVector&, ResourceVector&) const override	{v.level[Resource::Energy] = -EnergyConsumption; v.level[Resource::Minerals] = -(int)MobileEntityProducer::productionPerRound;}

	};



	

	void	ChangeStorage(WorkingStructure&ws, float atTime, Resource res, int change);	//defined in faction.h/.cpp

	
	template <class Derivative, index_t LeftoverUpdateIteration=2>
	class StorageStructure : public WorkingStructureT<Derivative>
	{
	protected:
		typedef WorkingStructureT<Derivative>			Super;
		friend class Super;
		/**/						StorageStructure(const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) : Super(owner,location,atTime)	{}
		/**/						StorageStructure()	{ }	//global prototype

	public:
		ResourceVector				storage,maxStorage;

		virtual void				GetResourceVector(ResourceVector&, ResourceVector&storage, ResourceVector&max) const override	{storage = this->storage; max = maxStorage;}
		virtual void				SetResourceStorage(const ResourceVector&store)	override
		{
			this->storage = store;
			this->storage.Cap(maxStorage);
		};

		virtual bool				WouldLikeToEnable() const override {return true;}

		#ifndef CLIENT

			virtual void			ProcessLeftovers(ResourceVector&v, index_t iteration, float atTime) override
			{
				if (iteration != LeftoverUpdateIteration && iteration != FinalLeftoverIteration)
					return;
				for (index_t i = 0; i < Resource::NumberOfPossibilities; i++)
				{
					if (v.level[i] == 0)
						continue;
					if (v.level[i] < 0 )
					{
						if (storage.level[i] > 0)
						{
							int delta = std::min(-v.level[i], storage.level[i]);
							ChangeStorage(*this,atTime,Resource::Reinterpret(i),-delta);

							v.level[i] += delta;
							//storage.level[i] -= delta;
						}
					}
					else
					{
						int capacity = maxStorage.level[i] - storage.level[i];
						if (capacity > 0)
						{
							int delta = std::min(v.level[i], capacity);
							v.level[i] -= delta;

							ChangeStorage(*this,atTime,Resource::Reinterpret(i),delta);
						}	
					}
				}
			}
		#else
			#ifdef HAS_WINDOW
				virtual void			DescribeType(BaseCommand::Label&label, UINT32 descFlags) override
				{
					Super::DescribeType(label,descFlags);
					label.MarkBriefEnd();
					label.WrapAppendText("Storage structures accumulate excess production of the respective resources until they are full."
					" Disabling a storage structure will change its priority such, that excess resources are stored here only, if all other storage capacities are exhausted."
					" Likewise, resource demand will remove resources from disabled storage structures first, such that they tend towards empty."
					" Storage structures do not need "+String(ResourceTag(Resource::Energy))+" to store resources."
					);			
				}
			#endif
		#endif
		virtual int					GetConstructionCost() const override {return 10;}
		#ifndef NO_HIBERNATION
			virtual int				GetUplinkTraffic() const override {return 2;}
			virtual int				GetUplinkPriority() const override {return 1000;};
		#endif
		virtual Category			GetCategory() const override {return Category::Resources;}


	};


	class Mine : public StorageStructure<Mine,3>
	{
	private:
		typedef StorageStructure<Mine,3>			Super;


		friend class WorkingStructureT<Mine>;
		/**/						Mine(const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) : Super(owner,location,atTime)	{maxStorage.level[Resource::Minerals] = MineralStorage; isLinkedToCore = LinkToCore(atTime);}
		/**/						Mine()	{
			maxStorage.level[Resource::Minerals] = MineralStorage;
		}	//global prototype

		bool						isLinkedToCore;
		


	public:
		static const int			EnergyConsumption = 10;
		#ifndef ONE_COMPOUND_PER_FACTION
			static const int		MineralStorage = 10;
		#else
			static const int		MineralStorage = 0;
		#endif

		bool						LinkToCore(float atTime);
		bool						IsLinkedToCore()	const;

		#ifdef CLIENT
			void					SetLinkedToCore(bool, bool controlEnabled);
			#ifdef HAS_WINDOW
				virtual void		DescribeType(BaseCommand::Label&label, UINT32 descFlags) override;
			#endif

		#endif

		NaturalResourceVector		naturalResources;

		float						GetEfficiency() const {return 1.f;/*location.height <= -8 ? 2.f : 1.f;*/}


		virtual bool				WouldLikeToEnable() const override {return true;}
		virtual bool				CanEnable() const override {return IsLinkedToCore();}

		//void						SetNaturalResourceVector(const NaturalResourceVector&res)	{naturalResources = res;}

		virtual void				PrototypeSetup()	override;

		virtual void				GetResourceVector(ResourceVector&v,  ResourceVector&storageCurrent, ResourceVector&storageMax) const override
									{
										Super::GetResourceVector(v,storageCurrent,storageMax); 
										v += naturalResources; 
										v.level[Resource::Energy] += -EnergyConsumption;
									}

		virtual int					GetConstructionCost() const override {return 40;}
		#ifndef NO_HIBERNATION
			virtual int				GetUplinkTraffic() const override {return 10;}
			virtual int				GetUplinkPriority() const override {return naturalResources.fuel*5 + naturalResources.minerals + naturalResources.uranium;};
		#endif


		virtual String				GetTypeName() const override { return "Mine"; };
		virtual ConstName			GetCTypeName() const override {return ConstName::Type_Mine;}


		virtual bool				SignalEnvironmentChange(const TStructureNeighborhood&neighborhood, float atTime) override
		{
			#ifndef CLIENT
				bool linked = IsLinkedToCore();
				if (linked != isLinkedToCore)
				{
					if (linked)
					{
						this->SetEnabled(false,atTime,false);
					}
					else
						RegisterChange(this->EnableChanged,atTime);
					isLinkedToCore = linked;
				}
			#endif
			return Super::SignalEnvironmentChange(neighborhood,atTime);

		}


		virtual bool				IsMine() const {return true;}
		virtual Category			GetCategory() const override {return Category::Resources;}
	};



	class MunitionFactory : public StorageStructure<MunitionFactory,3>
	{
		typedef MunitionFactory		Self;
		typedef StorageStructure<Self,3>	Super;
		friend class Super;
		friend class WorkingStructureT<Self>;


		static const int LocalMunitionStorage = 4;
		/**/						Self(const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) : Super(owner,location,atTime)
		{
			#ifndef NO_MUNITION
				maxStorage.level[Resource::Munitions] = LocalMunitionStorage;
			#endif
			#ifndef CLIENT
				SetEnabled(false,atTime,false,ForceEnable);
			#endif
		}
		/**/						Self()
		{
			#ifndef NO_MUNITION
				maxStorage.level[Resource::Munitions] = LocalMunitionStorage;
			#endif
		}	//global prototype
	protected:

	public:


		static const int			EnergyConsumption = 10,
									MineralConsumption = 10,
									MunitionOutput = 2;

		virtual void				GetResourceVector(ResourceVector&v,ResourceVector&storage, ResourceVector&max) const override
		{
			v.level[Resource::Energy] = -EnergyConsumption; 
			v.level[Resource::Minerals] = -MineralConsumption; 
			#ifndef NO_MUNITION
				v.level[Resource::Munitions] = MunitionOutput; 
			#endif
			Super::GetResourceVector(v,storage,max);
		}
		virtual Category			GetCategory() const override {return Category::Resources;}

		virtual int					GetConstructionCost() const override {return 40;}
		#ifndef NO_HIBERNATION
			virtual int				GetUplinkTraffic() const override {return 5;}
			virtual int				GetUplinkPriority() const override {return 400;};
		#endif


		virtual String				GetTypeName() const override { return "Munition Factory"; };
		virtual ConstName			GetCTypeName() const override {return ConstName::Type_MunitionFactory;}


		#ifdef HAS_WINDOW
			virtual void			DescribeType(BaseCommand::Label&label, UINT32 descFlags) override
			{
				Super::DescribeType(label,descFlags);
				label.MarkBriefEnd();
				#ifdef NO_MUNITION
					label.WrapAppendText("Enabled munition factories continuiously produce munition to supply connected offensive turrets. This is a deprecated building type."
				#else
					label.WrapAppendText("Enabled munition factories continuiously produce "+String(ResourceTag(Resource::Munitions))+" to supply connected offensive turrets."
				#endif
				);			
			}
		#endif
	};




	class FuelContainer : public StorageStructure<FuelContainer>
	{
		typedef StorageStructure<FuelContainer>	Super;
		friend class Super;
		friend class WorkingStructureT<FuelContainer>;
		/**/						FuelContainer(const OwnerParameter&owner, const Grid::LCellIdI&location,float atTime) : Super(owner,location,atTime)	{maxStorage.level[Resource::Fuel] = 400;}
		/**/						FuelContainer()	{maxStorage.level[Resource::Fuel] = 400; }	//global prototype
	public:
		virtual String				GetTypeName() const override { return "Fuel Container"; };
		virtual ConstName			GetCTypeName() const override {return ConstName::Type_FuelContainer;}

	};


	class MineralContainer : public StorageStructure<MineralContainer>
	{
		typedef StorageStructure<MineralContainer>	Super;
		friend class Super;
		friend class WorkingStructureT<MineralContainer>;
		/**/						MineralContainer(const OwnerParameter&owner, const Grid::LCellIdI&location,float atTime) : Super(owner,location,atTime)	{maxStorage.level[Resource::Minerals] = 400;}
		/**/						MineralContainer()	{maxStorage.level[Resource::Minerals] = 400; }	//global prototype
	public:
		virtual String				GetTypeName() const override { return "Mineral Container"; };
		virtual ConstName			GetCTypeName() const override {return ConstName::Type_MineralContainer;}

	};


	class UraniumStorage : public StorageStructure<UraniumStorage>
	{
		typedef StorageStructure<UraniumStorage>	Super;
		friend class Super;
		friend class WorkingStructureT<UraniumStorage>;
		/**/						UraniumStorage(const OwnerParameter&owner, const Grid::LCellIdI&location,float atTime) : Super(owner,location,atTime)	{maxStorage.level[Resource::Uranium] = 400;}
		/**/						UraniumStorage()	{maxStorage.level[Resource::Uranium] = 400; }	//global prototype
	public:
		virtual String				GetTypeName() const override { return "Uranium Storage"; };
		virtual ConstName			GetCTypeName() const override {return ConstName::Type_UraniumStorage;}

	};

	
	class MunitionStorage : public StorageStructure<MunitionStorage>
	{
		typedef MunitionStorage		Self;

		typedef StorageStructure<Self>	Super;
		friend class Super;
		friend class WorkingStructureT<Self>;
		/**/						Self(const OwnerParameter&owner, const Grid::LCellIdI&location,float atTime) : Super(owner,location,atTime)	
		{
			#ifndef NO_MUNITION
				maxStorage.level[Resource::Munitions] = 400;
			#endif
		}
		/**/						Self()
		{
			#ifndef NO_MUNITION
				maxStorage.level[Resource::Munitions] = 400; 
			#endif
		}	//global prototype
	public:
		virtual String				GetTypeName() const override { return "Munition Storage"; };
		virtual ConstName			GetCTypeName() const override {return ConstName::Type_MunitionStorage;}

	};








	/**
	Stationary defence building
	All towers feature turrets on their top, which define their functionality. Towers and satellites are the only
	WorldEntity's that can fire automatically during movement phase. They are therefore particularly effective against approaching
	vehicles, and must typically be disabled via EMP first.
	*/
	class Tower : public WorkingStructure, public TurretBase
	{
	private:
		static bool					setup;
		typedef WorkingStructure	Super;
		friend class Super;
		friend void Create();
		//friend class WorkingStructureT<Tower>;
		/**/						Tower(Component::Turret*turret) : TurretBase(turret)
		{
			#ifdef HAS_WINDOW
				Super::localizableShape = &shape;
			#endif
		}
		/**/						Tower(Component::Turret*turret, const OwnerParameter&owner, const Grid::LCellIdI&location, float time) : Super(owner,location,true,time),TurretBase(turret)
		{
			#ifdef HAS_WINDOW
				Super::localizableShape = &shape;
			#endif
		}

		bool						WouldLikeToEnable() const override {return true;}


	public:

		static Buffer0<PTower>		globalPrototypes;

		ResourceVector				storage;


		struct AttackTask
		{
			//TUniqueID				id;
			Grid::LCellIdI			target;

			#ifdef CLIENT
				#ifdef HAS_WINDOW
					TMatrix4<>					blastSystem;
					DX11::InstructionBlock32	blastVisualization,
												trajectoryVisualization;
				#endif
			#else
				bool				autoTarget;
			#endif

		};

		virtual void				SetResourceStorage(const ResourceVector&storage)override
		{
			this->storage = storage;
			this->storage.Cap(this->GetMaxStorageCapacity());
		};

		static float				foundationHeight;	//unscaled height of the base geometry, and vertical offset to position turret
		static float				GetEffectiveFoundationHeight() {return foundationHeight * Grid::UnitScale;}
		virtual PTurretBase			GetTurretBase(float&zOffset) override { zOffset = GetEffectiveFoundationHeight(); return To<Tower>::Cast(shared_from_this());}
		virtual PConstTurretBase	GetTurretBase(float&zOffset) const override { zOffset = GetEffectiveFoundationHeight(); return To<const Tower>::Cast(shared_from_this());}

		virtual int					GetHeight() const override {return 2;}


		float						attackActionLeft=1.f;

		#ifdef CLIENT
			Buffer0<AttackTask>			attackQueue;

			#ifdef HAS_WINDOW
				static CommandContainer	commands;
				/**
				Render-ready, object-space instruction to be used for all instances of this structure type
				*/
				static DX11::InstructionBlock32	shape;
				/**
				Intersectable, object-space geometry of all instances of this structure type
				*/
				static Geometry			geometry;
			#endif

			void					AppendTask(const Grid::LCellIdI&attackTarget);
		#else
			Queue<AttackTask>		attackQueue;


			void					BeginAttackPhase()	{}

			bool					AttackNext(const Grid::LCellIdI&target, bool autoTarget);
			bool					ClearTasks();
			bool					DropLastTask();

			virtual void			Attack(float&)	override;
			virtual bool			FillAttackQueue() override;
			bool					WouldAttack(const PWorldEntity&e, Grid::LCellIdI&outTarget, float entityTime)	const;
			bool					IsIdleButCouldAttack(float atTime) const {return attackQueue.IsEmpty() && HasAmmoForShot(atTime);}


			virtual bool			DirectTo(CursorMode, ArrayRef<Grid::LCellIdI> waypoints, bool detailedPath, bool leftClick = false) override;

		#endif
		bool						HasAmmoForShot(float atTime) const;
		bool						HasAmmoForShot() override;



		#ifndef ONE_COMPOUND_PER_FACTION
			ResourceVector			GetMaxStorageCapacity()	const	{return turret->baseStorageCapacity * 2;};
		#else
			ResourceVector			GetMaxStorageCapacity()	const	{return ResourceVector();};
		#endif



		virtual void				GetResourceVector(ResourceVector&, ResourceVector&storage, ResourceVector&max) const override	{storage = this->storage; max = GetMaxStorageCapacity();}



		virtual placement_t			CheckPlacement(const Grid::LCellIdI&location) override
		{
			return CommonCheckStructurePlacement(location,2,true);
		}

		virtual PWorldEntity				CreateNewInstance(const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) override {return PWorldEntity(new Tower(turret,owner,location,atTime));};


		virtual float				GetMaxScanningRange() const override	{return turret->CanAttack() ? turret->scanningRange - 2.f : turret->scanningRange + 2.f;};	//returns the effective scanning range of the local entity
		virtual int					GetMaxHealth() const override {return int(ceil(float(Super::GetMaxHealth()) * turret->healthMultiplier));}
		virtual void				PrototypeSetup()	override;
		virtual String				GetTypeName() const override { return turret->typeName+" Tower"; };
		virtual ConstName			GetCTypeName() const override {return turret->constTowerName;}

		virtual int					GetConstructionCost() const override {return 40 + turret->productionCosts;}
		#ifndef NO_HIBERNATION
			virtual int				GetUplinkTraffic() const override {return turret->uplinkTraffic;}
			virtual int				GetUplinkPriority() const override {return 1000;}
		#endif

		virtual bool				ConnectionPossibleTo(const PStructure&other, UINT32 acrossHighCell)	override	{return true;}

		#ifndef CLIENT

			virtual void			OnConstructionFinished(float&atTime) override;
			virtual void			ProcessLeftovers(ResourceVector&v, index_t iteration, float atTime) override;
		#else
			#ifdef HAS_WINDOW
				virtual void		VisitAllCommands(FCommandVisitor visitor, bool force) override;

				virtual bool		TestIntersection(const TVec3<>&b, const TVec3<>&d, float & distance) const override;
			#endif
		#endif

		//WorldEntity overrides:
		virtual bool				IsTower() const override {return true;}

		//structure overrides:
		virtual int					GetBroadHeight() const override {return std::max(1,GetHeight()-1);}
		virtual Category			GetCategory() const override {return Category::Defense;}


		//turret base overrides:
		virtual bool				NextTaskIsAttack() const override {return health > 0 && attackQueue.IsNotEmpty();}
		virtual float				GetAttackActionLeft() const override {return attackActionLeft;};
		virtual float				GetFlatAttackRange() const override {return turret->horizontalAttackRange > 0 ? EMPModify(float(turret->horizontalAttackRange) + 2.f,2.f) : 0.f;}

	};



	















	extern Buffer0<PStructure>			structures;
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
		/*static*/ DX11::InstructionBlock32	WorkingStructureT<Derivative>::shape;

		template <typename Derivative>
		/*static*/ DX11::InstructionBlock32	OtherStructure<Derivative>::shape;

		template <typename Derivative>
		/*static*/ Geometry				WorkingStructureT<Derivative>::geometry;
		template <typename Derivative>
		/*static*/ Geometry				OtherStructure<Derivative>::geometry;

		template <typename Derivative>
		/*static*/ CommandContainer	WorkingStructureT<Derivative>::commands;
		template <typename Derivative>
		/*static*/ CommandContainer	OtherStructure<Derivative>::commands;
	#endif



	template <typename Derivative>
		/*virtual override*/ PWorldEntity			WorkingStructureT<Derivative>::CreateNewInstance(const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) { return PWorldEntity(new Derivative(owner,location,atTime)); }



	#ifdef HAS_WINDOW
		template <typename Derivative>
			/*virtual override*/	void			WorkingStructureT<Derivative>::PrototypeLoadGeometry()
			{
				Super::PrototypeLoadGeometry();

				#ifdef HAS_WINDOW
					commands.baseCoords = int2(3,0);
				#endif


				geometry.Clear();
				String name = GetTypeName();
				name.set(0, Template::tolower(name.firstChar()));
				name.replace(' ',"");
				geometry.LoadFromObj(PathString("objects/"+name+".obj"));

				if (shape.shader.IsEmpty())
					(DX11::BaseInstructionBlock&)shape = standardLocalizableShader;
				shape.vbo.CreateFromStructs(geometry.GetLOD(0).vertices);
				shape.ibo.Create(geometry.GetLOD(0).indices);
			}


		template <typename Derivative>
			/*virtual override*/	void			OtherStructure<Derivative>::PrototypeLoadGeometry()
			{
				Super::PrototypeLoadGeometry();

				geometry.Clear();
				String name = GetTypeName();
				name.set(0, Template::tolower(name.firstChar()));
				name.replace(' ',"");
				geometry.LoadFromObj(PathString("objects/"+name+".obj"));

				if (shape.shader.IsEmpty())
					(DX11::BaseInstructionBlock&)shape = standardLocalizableShader;
				shape.vbo.CreateFromStructs(geometry.GetLOD(0).vertices);
				shape.ibo.Create(geometry.GetLOD(0).indices);
			}

	#endif

	template <typename Derivative>
		/*virtual override*/	void			WorkingStructureT<Derivative>::PrototypeSetup()
		{
			Super::PrototypeSetup();

			#ifdef HAS_WINDOW
				commands.baseCoords = int2(3,0);

				commands.Add(new IconCommand("Enable",&Cmd::Enable,
				[](bool controlEnabled)->BaseCommand::Flags
				{
					BaseCommand::Flags rs = BaseCommand::Flags::None;
					WorkingStructureT<Derivative>*div = dynamic_cast<WorkingStructureT<Derivative>*>(selectedEntity.get());
					if (controlEnabled)
					{
						if (div && div->CanEnable())
						{
							rs |= BaseCommand::Flags::IsEnabled;
						}
					}
					if (div && div->IsEnabled())
						rs |= BaseCommand::Flags::IsPermaActive;
					return rs;
				}
				,NoCosts
				));
			#endif

			String name = GetTypeName();
			name.set(0, Template::tolower(name.firstChar()));
			name.replace(' ',"");
			#ifdef HAS_WINDOW
				geometry.LoadFromObj(PathString("objects/"+name+".obj"));

				if (shape.shader.IsEmpty())
					(DX11::BaseInstructionBlock&)shape = standardLocalizableShader;
				shape.vbo.CreateFromStructs(geometry.GetLOD(0).vertices);
				shape.ibo.Create(geometry.GetLOD(0).indices);
			#endif
		}


	template <typename Derivative>
		/*virtual override*/	void			OtherStructure<Derivative>::PrototypeSetup()
		{
			Super::PrototypeSetup();
			#ifdef HAS_WINDOW
				commands.baseCoords = int2(3,0);
			#endif

			String name = GetTypeName();
			name.set(0, Template::tolower(name.firstChar()));
			name.replace(' ',"");
			#ifdef HAS_WINDOW
				geometry.LoadFromObj(PathString("objects/"+name+".obj"));

				if (shape.shader.IsEmpty())
					(DX11::BaseInstructionBlock&)shape = standardLocalizableShader;
				shape.vbo.CreateFromStructs(geometry.GetLOD(0).vertices);
				shape.ibo.Create(geometry.GetLOD(0).indices);
			#endif
		}
	
	template <typename Derivative>
		WorkingStructureT<Derivative>::WorkingStructureT(const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) : Super(owner,location, true, atTime)
		{
			#ifdef CLIENT
				#ifdef HAS_WINDOW
					Super::localizableShape = &shape;
				#endif
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

	#ifdef HAS_WINDOW

		template <typename Derivative>
			/*static*/ void				WorkingStructureT<Derivative>::Cmd::Enable()
			{
				WorkingStructureT<Derivative>*s2 = dynamic_cast<WorkingStructureT<Derivative>*>(selectedEntity.get());
				if (s2)
				{
					ApplicationInterface::SignalEnableStructure(*s2, !s2->IsEnabled());
					//s2->SetEnabled(!s2->IsEnabled());
				}
			}
		template <typename Derivative>
			/*static*/ bool				WorkingStructureT<Derivative>::Cmd::IsEnabled()
			{
				WorkingStructureT<Derivative>*s2 = dynamic_cast<WorkingStructureT<Derivative>*>(selectedEntity.get());
				return s2 && s2->IsEnabled();
			};




	#endif


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

	inline PTower		ToTower(const PWorldEntity&e)
	{
		PWorkingStructure ws = ToWorkingStructure(e);
		if (!ws || !ws->IsTower())
			return PTower();
		return To<Tower>::Cast(ws);
	}

	inline PMine		ToMine(const PWorldEntity&e)
	{
		PWorkingStructure ws = ToWorkingStructure(e);
		if (!ws || !ws->IsMine())
			return PMine();
		return To<Mine>::Cast(ws);
	}

}





#endif

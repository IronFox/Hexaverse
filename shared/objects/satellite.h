#ifndef objectsSatteliteH
#define objectsSatteliteH

#include "../config.h"

#ifndef NO_SATELLITES

/*
* single sphere layer that hosts all space objects around planets
* no storage facilities
* every object in space demands fuel per round, regardless of motion, collected every round
	+ fuel demand increases quadratically with distance from next SkyHub
	+ objects lacking fuel degrade in height once
	+ failure to restore their height leads to object loss
* SkyHub and SkyPort are responsible for transfering resources from and to the planet surface
* SkyHub and SkyPort can exist independently and build each other
* connected/supplied SkyHubs do not require fuel
	disconnected/unsupplied SkyHubs must be supplied by other SkyHubs,
	and fall under the distance-rules as any other satellite
* hibernating sattelites consume fuel and maintain/restore their altitude, 
	but cannot move or execute other instructions


*/


#include "entity.h"
#include "../cast.h"
#include "hibernating.h"
#include "mobileEntity.h"
#include "interface.h"
#include "mobileEntityProducer.h"
#include "../sharedContainer.h"

namespace Objects
{
	#ifdef HAS_WINDOW
		extern DX11::BaseInstructionBlock	stdVehicleShader;	//don't want to include rendering.h here
	#endif


	/**
	@brief Base sattelite entity

	Satellites under normal operation maintain a standard height 0, which is translated into 
	Satellite::BaseHeight during loading, and re-converted during storing.
	Satellites in positional decline move to Satellite::DeclineHeight. Any loaded height <0 is
	considered declined height, regardless of actual value
	In the context of this game, the term satellite also applies to space ships in general.
	More station-like structures move very slowly, while space ships can move much faster

	*/
	class Satellite : public MobileEntity
	{
	protected:
		typedef MobileEntity	Super;
		/**/					Satellite(const OwnerParameter&owner, Component::Turret*turret, const Grid::LCellIdI&location):Super(owner,turret,location)
								{}




		struct THub
		{
			WSkyHub				hub;
			float distance;

			float operator<(const THub&other) const
			{
				return distance < other.distance;
			}
		};
		#ifndef ONE_COMPOUND_PER_FACTION
			static Buffer0<THub>hubs;	//ordered for references, closest first

			/**
			Updates @hubs to the local configuration. Since all suppliers provide the same in connector-less mode
			keeping a full list is not necessary in this case
			*/
			void				UpdateHubMap();
		#else
			/**
			Fetches the closest connected SkyHub (that is not this), belonging to the same faction
			@param[out] quadraticDistance Quadratic distance to the found SkyHub.
			@return Found SkyHub instance or NULL, if the local faction does not own (or is not known to have)
			any other connected SkyHubs
			*/
			PSkyHub				GetClosestExternalSupplier(float&quadraticDistance);
		#endif


		PSkyLinkStructure		RemoteSupplyFuel(float&atTime);

		#ifdef HAS_WINDOW
			/**
			Cache value meant to accelerate client processes (e.g. the UI).
			Stores the last calculated extra amount of fuel needed for the local
			satellite instance. Referenced and updated by GetExtraFuelDemand().
			Negative means uninitialized.
			Can be 0, if the satellite consumes only its base demand
			*/
			int					lastExtraFuelDemand = -1;
		#endif

	public:
		static const int		BaseResidenceHeight = Grid::CellStack::HalfRange * 5;
		static const int		DeclinedResidenceHeight = BaseResidenceHeight - 4;

		static bool				IsSatelliteHeight(int h)	{return h == BaseResidenceHeight || h == DeclinedResidenceHeight;}

		static count_t			CountTurretOccurrences(const OwnerParameter&,Component::Turret*);


		/**
		Sensor state of the shaft-sensor that scans the ground beneath the satellite.
		Currently all satellites carry one such, even if they cannot actually scan the ground.
		Due to their ongoing fuel supply it is not to be expected that a lot of satellites
		roam the skies.
		*/
		Grid::SkySensorState	skySensorState;

		/**
		Determines the factor that is to be applied to the fuel demand of a satellite at
		a given (quadratic) distance from the next connected SkyHub.
		@param quadraticDistanceFromSource Quadratic distance to the nearest supplying SkyHub
		@return Factor to apply to the result of GetFuelConsumptionPerRound().
		The returned value is always 1.0f or greater.
		*/
		static float			FuelFactorAtDistance(float quadraticDistanceFromSource);


		int						GetGroundHeight() const;
		static int				GetGroundHeight(const Grid::LCellIdI&location);

		#ifdef HAS_WINDOW
			/**
			Visualization instruction responsible for visualizing the core component of the local satellite
			References a static object unique to each satellite class.
			Kinda like a vtable pointer for satellite rendering instructions
			*/
			DX11::InstructionBlock32	*baseRenderingInstruction;
		#endif

		#ifndef CLIENT
			/**
			Adjust to ground changes. Should be called once per round to account for 
			@return true, if changes were detected and applied, false otherwise
			*/
			bool				RefreshGroundSensor(float&atTime);
			bool				AttackNext(Grid::LCellIdI target, bool autoTarget);
			virtual void		MoveSensory(const Grid::LCellIdI&oldLocation, const Grid::LCellIdI&newLocation, float&atTime, bool checkChangesNow/*=true*/, Buffer0<PWorldEntity>*newContacts/* = nullptr*/) override;
		#endif

		#ifdef HAS_WINDOW
			/**
			Retrieves the amount of fuel that the local satellite (at least) consumes in excess of its
			base fuel demand. This is a client-sided guess. It can be wrong.
			@return extra fuel demand, depending on its distance to the nearest connected SkyHub.
					Slow at first, cached for high-frequency querying. 0, if no extra fuel is
					needed. Always non-negative.
			*/
			int					GetExtraFuelDemand();

			void				FlushFuelDemandCache() {lastExtraFuelDemand = -1;}
		#endif

		/**
		Retrieves the base amount of fuel this satellite requires to preserve its orbit.
		The returned value is further modified by the distance to the next SkyHub or possible
		SkyPort connection status.
		The return value is expected to be independent of the state of the local instance.
		*/
		virtual int				GetFuelConsumptionPerRound() const;
		virtual String			DescribeFuelConsumptionPerRound() const;
		virtual int				GetBaseFuelConsumptionPerRound() const {return GetConstructionCost()/4;}
		/**
		Tests whether or not the local instance currently requires fuel at all.
		SkyHubs do not require fuel when hovering atop an enabled SkyPort, and will then return false.
		*/
		virtual bool			RequiresFuel() const {return true;}

		//new overridables:
		#ifndef CLIENT
			virtual bool		ExtCanHit(const Grid::LCellIdI&attackFrom,const Grid::LCellIdI&attackTarget,Grid::LCellIdI&actual) const {return false;}
			virtual bool		WouldAttack(const PWorldEntity&target, Grid::LCellIdI&outTarget, float entityTime)	{return false;}
			virtual bool		IsSentry() const	{return false;}
			/**
			Performs all per-round operations the local satellite can. The base version handles fueling of the local satellite.
			Any overriding class should call the base version of this method and return its result, unless fuel processing should not commence for whatever reason
			@return true, if the local satellite was successfully supplied with fuel, false if declination should happen
			*/
			virtual bool		ProcessRound(float&atTime);
		#endif
		virtual bool			IsSkyHub() const {return false;}

		//TurretBase overrides:
		virtual float			GetFlatAttackRange() const override;
		#ifndef CLIENT
			virtual bool		FrontQueuePossibleAttacks()override;
		#endif

		//WorldEntity overrides:
		virtual bool			IsSatellite() const override {return true;}
		virtual PTurretBase		GetTurretBase(float&zOffset) override { zOffset = 0.f; return To<MobileEntity>::Cast(shared_from_this());}
		virtual PConstTurretBase	GetTurretBase(float&zOffset) const override { zOffset = 0.f; return To<const MobileEntity>::Cast(shared_from_this());}
		virtual bool			CanSpawnInGrid() const override {return false;}
		virtual bool			CanSpawnAboveGrid(int&outHeight) const {outHeight = BaseResidenceHeight; return true;}

		#ifdef HAS_WINDOW
			virtual void		DescribeType(Command::Base::Label&outResult, UINT32 descFlags) override;
			//virtual void		RegisterWorldSensory() override;
			//virtual void		UnregisterWorldSensory() override;
//			virtual void		MoveWorldSensory(const Grid::LCellIdI&from, const Grid::LCellIdI&to) override;
			virtual void		VisitAllCommands(FCommandVisitor visitor,bool force) override {turret->VisitAllCommands(visitor,force);};

		#endif

		//#ifndef CLIENT
			virtual void		RegisterSensory(const Grid::LCellIdI&loc, float&atTime, bool checkChangesNow=true, Buffer0<PWorldEntity>*newContacts = nullptr) override;
			virtual void		UnregisterSensory(const Grid::LCellIdI&loc, float&atTime, bool checkChangesNow=true) override;
		//#endif

		//MobileEntity overrides:
		virtual void			TranslateLocationForExport() override
		{
			if (location.height < BaseResidenceHeight)
				location.height = -1;
			else
				location.height = 0;
		}
		virtual void			TranslateLocationFromExport() override
		{
			if (location.height == 0)
				location.height = BaseResidenceHeight;
			else
				location.height = DeclinedResidenceHeight;
		}
		#ifndef ONE_COMPOUND_PER_FACTION
			virtual ResourceVector		GetMaxStorageCapacity()	const override
			{
				return turret->baseStorageCapacity;
			}
		#endif

		virtual int				GetMovementStepsPerRound() const override;
		virtual String			DescribeMovementStepsPerRound()	const override;
		virtual bool			MotionIgnoresAttack() const override {return false;}
		virtual float			GetPathSize() const override {return 1.f;}
		#ifndef CLIENT
			bool				DirectTo(mode_t mode, Ctr::ArrayRef<Grid::LCellIdI> waypoints, bool detailedPath, bool leftClick = false) override;
			MobilePlacementResult	PlaceExactlyAt(const Grid::LCellIdI&, float timeCost, float minTimeOffset, bool signalEvent=true, bool isCreated=true, bool failIsFatal=false) override;
		#endif

		//WorldEntity overrides:
		virtual void			PrototypeSetup() override;
	};




	template <typename Derivative>
		class SatelliteTemplate : public Satellite
		{
		protected:
			typedef Satellite		Super;
			/**/					SatelliteTemplate<Derivative>(const OwnerParameter&owner, Component::Turret*turret, const Grid::LCellIdI&location):Super(owner,turret,location)
			{
				#ifdef HAS_WINDOW
					Super::baseRenderingInstruction = &shape;
				#endif		

			}
			/**/					SatelliteTemplate<Derivative>(Component::Turret*turret) : Super(OwnerParameter(),turret,Grid::LCellIdI(0, 0))
			{ 
				#ifdef HAS_WINDOW
					Super::baseRenderingInstruction = &shape;
				#endif		
			}	//global prototype

		public:
			static const std::shared_ptr<Derivative>	globalPrototype;
			#ifdef HAS_WINDOW
				static CommandContainer	commands;
				/**
				Render-ready, object-space instruction to be used for all instances of this structure type
				*/
				static DX11::InstructionBlock32	shape;
				/**
				Intersectable, object-space geometry of all instances of this structure type
				*/
				static Geometry		geometry;
			#endif

			virtual Component::Turret*
									GetTurretType() const=0;

			//WorldEntity overrides:
			virtual void			PrototypeSetup() override;
			virtual PWorldEntity	CreateNewInstance(const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) override {return PWorldEntity(new Derivative(owner, location)); }
			#ifdef HAS_WINDOW
				virtual void		PrototypeLoadGeometry() override;

				virtual bool		TestIntersection(const TVec3<>&b, const TVec3<>&d, float & distance) const override;
				virtual void		VisitAllCommands(FCommandVisitor visitor,bool force) override {Super::VisitAllCommands(visitor,force); commands.VisitMembers(visitor,force);};
			#endif

		};

	template <typename Derivative>
	/*static*/ const std::shared_ptr<Derivative>	SatelliteTemplate<Derivative>::globalPrototype(new Derivative());

	#ifdef HAS_WINDOW
		template <typename Derivative>
			/*static*/ CommandContainer		SatelliteTemplate<Derivative>::commands;
		template <typename Derivative>
			/*static*/ DX11::InstructionBlock32	SatelliteTemplate<Derivative>::shape;
		template <typename Derivative>
			/*static*/ Geometry		SatelliteTemplate<Derivative>::geometry;

		template <typename Derivative>
		/*virtual overrider*/ bool			SatelliteTemplate<Derivative>::TestIntersection(const TVec3<>&_b, const TVec3<>&_d, float & distance) const
		{
		// {return Super::TestIntersection(geometry,b,d,distance);}
			TVec3<> b,d;//,test;
			Mat::Transform(inverse,_b,b);
			Mat::rotate(inverse,_d,d);
			
			return geometry.TestIntersection(b,d,distance) | 	//binary or - must evaluate both
					turret->TestIntersection(0.f, turretAngleX,turretAngleY,b,d,distance);
		}

	#endif




	template <typename Derivative>
		/*virtual override*/	void			SatelliteTemplate<Derivative>::PrototypeSetup()
		{
			Super::PrototypeSetup();
			turret = GetTurretType();
			#ifdef HAS_WINDOW
				commands.baseCoords = int2(4,0)+Command::Base::coordOffset;
			#endif
		}

	#ifdef HAS_WINDOW
		template <typename Derivative>
			/*virtual override*/	void		SatelliteTemplate<Derivative>::PrototypeLoadGeometry()
			{
				geometry.Clear();
				String name = GetTypeName();
				name.set(0, Template::tolower(name.FirstChar()));
				name.FindAndReplace(' ',"");
				geometry.LoadFromObj(PathString("objects/"+name+".obj"));
				if (shape.shader.IsEmpty())
					(DX11::BaseInstructionBlock&)shape = stdVehicleShader;
				shape.vbo.CreateFromStructs(geometry.GetLOD(0).vertices);
				shape.ibo.Create(geometry.GetLOD(0).indices);

			}
	#endif



	class SkyHub : public SatelliteTemplate<SkyHub>, public MobileEntityProducer
	{
		static Buffer0<ConstName>	supportsProduction;
	protected:
		typedef SatelliteTemplate<SkyHub>	Super;
		friend class Super;

		static const int		ProductionSpeed = 10;

		/**/					SkyHub(const OwnerParameter&owner, const Grid::LCellIdI&location):Super(owner,GetTurretType(),location),MobileEntityProducer(ProductionSpeed)
								{MobileEntityProducer::SetCommandOffset(int2(3,1));}
		/**/					SkyHub():Super(GetTurretType()),MobileEntityProducer(ProductionSpeed)
								{MobileEntityProducer::SetCommandOffset(int2(3,1));}	//global prototype

		static int2				AllocateProductionSlot();

		static void				RegisterProduction(const PWorldEntity&);
		virtual bool			CanProduce(const PWorldEntity&e) const override {return supportsProduction.Contains(e->GetCTypeName());}

		#ifdef HAS_WINDOW

			struct MyCmd
			{
				static bool		CanBuildStructure();
				static void		Build(const PWorldEntity&what);

			};
		#endif

	public:
		WSkyLinkStructure		connectedTo;

		/**
		Updates @a connectedTo if necessary
		*/
		void					UpdateConnections();


		static bool				CanProduce(const Satellite*satPrototype);



		bool					GainGroundConnection(const PSkyLinkStructure&to);
		void					LoseGroundConnection();

		static int				GetStructureBuildHeight(const Grid::LCellIdI&satelliteLocation);
		int						GetStructureBuildHeight() const;


		//Satellite overrides:
		virtual bool			IsSkyHub() const override {return true;}
		virtual bool			RequiresFuel() const {return connectedTo.expired();}


		#ifndef CLIENT
			virtual bool		ProcessRound(float&atTime) override;
		#endif


		//BaseEntity overrides:
		String					GetTypeName() const override {return "Sky Hub";}
		ConstName				GetCTypeName() const override {return ConstName::Type_SkyHub;}


		//Hibernating overrides:
		#ifndef NO_HIBERNATION
			virtual int			GetUplinkPriority() const override {return 500;}
		#endif


		//SatelliteTemplate overrides:
		virtual Component::Turret*
								GetTurretType() const override	{return &Component::skyHubTurret;}

		//MobileEntity overrides:
		virtual Grid::LCellIdI	GetConstructionLocation(const Grid::LCellIdI&focus) const {return Grid::LCellIdI(focus.stackID,GetStructureBuildHeight(focus));}


		//WorldEntity overrides:
		virtual int				GetConstructionCost() const override {return 50;}
		#ifndef NO_HIBERNATION
			virtual int			GetUplinkTraffic() const override {return 10;}
		#endif
		virtual void			OnDestroy(RemovalType t, float&atTime) override {SyncTime(atTime); Super::OnDestroy(t,atTime); LoseGroundConnection();}
		virtual void			OnConstructionFinished(float&atTime) override;
		virtual bool			AwaitsUserInteraction() const override {return MobileEntityProducer::AwaitsUserInteraction();}
		#ifdef HAS_WINDOW
			virtual void		VisitAllCommands(FCommandVisitor visitor,bool force) override;
			virtual void		OnSelect() override;
			virtual void		OnDeselect() override;
		#endif
		virtual const MobileEntityProducer*CastToProducer() const override {return this;}
		virtual void			PrototypeSetup() override;


	};


	class SkyGuardian : public SatelliteTemplate<SkyGuardian>
	{
	protected:
		typedef SatelliteTemplate<SkyGuardian>	Super;
		friend class Super;

		/**/					SkyGuardian(const OwnerParameter&owner, const Grid::LCellIdI&location):Super(owner,GetTurretType(),location)
								{}
		/**/					SkyGuardian():Super(GetTurretType())
								{}	//global prototype
	public:

		#ifndef CLIENT
			//Satellite overrides:
			virtual bool		WouldAttack(const PWorldEntity&target, Grid::LCellIdI&outTarget, float entityTime)	override;
			virtual bool		ExtCanHit(const Grid::LCellIdI&attackFrom,const Grid::LCellIdI&attackTarget,Grid::LCellIdI&actual) const override;
			virtual bool		IsSentry() const override {return true;}
		#endif




		//BaseEntity overrides:
		virtual String			GetTypeName() const override {return "Sky Guardian";}
		virtual ConstName		GetCTypeName() const override {return ConstName::Type_SkyGuardian;}

		//SatelliteTemplate overrides:
		virtual Component::Turret*
								GetTurretType() const override	{return &Component::protonCannon;}

		//WorldEntity overrides:
		virtual int				GetConstructionCost() const override {return 15;}
		#ifndef NO_HIBERNATION
			virtual int			GetUplinkTraffic() const override {return 4;}
		#endif

		//Hibernating overrides:
		#ifndef NO_HIBERNATION
			virtual int			GetUplinkPriority() const override {return 900;}
		#endif


	};

	class Ragnarok : public SatelliteTemplate<Ragnarok>
	{
	protected:
		typedef SatelliteTemplate<Ragnarok>	Super;
		friend class Super;

		/**/					Ragnarok(const OwnerParameter&owner, const Grid::LCellIdI&location):Super(owner,GetTurretType(),location)
								{}
		/**/					Ragnarok():Super(GetTurretType())
								{}	//global prototype
	public:

		//BaseEntity overrides:
		virtual String			GetTypeName() const override {return "Ragnarök";}
		virtual ConstName		GetCTypeName() const override {return ConstName::Type_Ragnarok;}

		//SatelliteTemplate overrides:
		virtual Component::Turret*
								GetTurretType() const override	{return &Component::ragnarok;}

		//WorldEntity overrides:
		virtual int				GetConstructionCost() const override {return 100;}
		#ifndef NO_HIBERNATION
			virtual int			GetUplinkTraffic() const override {return 50;}

			//Hibernating overrides:
			virtual int			GetUplinkPriority() const override {return 890;}
		#endif

	};


	class StarPort : public SatelliteTemplate<StarPort>
	{
	protected:
		typedef SatelliteTemplate<StarPort>	Super;
		friend class Super;

		/**/					StarPort(const OwnerParameter&owner, const Grid::LCellIdI&location):Super(owner,GetTurretType(),location)
								{}
		/**/					StarPort():Super(GetTurretType())
								{}	//global prototype
	public:
		static const int		ProductionSpeed = 5;


		//BaseEntity overrides:
		virtual String			GetTypeName() const override {return "Star Port";}
		virtual ConstName		GetCTypeName() const override {return ConstName::Type_StarPort;}

		//SatelliteTemplate overrides:
		virtual Component::Turret*
								GetTurretType() const override	{return &Component::starPort;}

		//WorldEntity overrides:
		virtual int				GetConstructionCost() const override {return 200;}
		#ifndef NO_HIBERNATION
			virtual int			GetUplinkTraffic() const override {return 5;}

			//Hibernating overrides:
			virtual int			GetUplinkPriority() const override {return 1000;}
		#endif

	};





	extern SharedContainer<Satellite>	satellites;
	//extern bool					satellitesAreLocked;

}

#endif
#endif

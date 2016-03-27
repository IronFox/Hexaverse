#ifndef objectsSubOrbitalH
#define objectsSubOrbitalH

#include "entity.h"
#include "../cast.h"
#include "hibernating.h"
#include "mobileEntity.h"
#include "interface.h"
#include "mobileEntityProducer.h"
#include "vehicle.h"

namespace Objects
{
	#ifdef HAS_WINDOW
		extern DX11::BaseInstructionBlock	stdVehicleShader;	//don't want to include rendering.h here
	#endif

	namespace SubOrbital
	{
		static const int			Height = Grid::CellStack::TopHeight +2;
		static const UINT			Layer = (UINT)(Height + Grid::CellStack::HalfRange);

		class Plane : public Vehicle
		{
		protected:
			typedef Vehicle			Super;
			/**/					Plane(const OwnerParameter&owner, Component::Turret*turret, const Grid::LCellIdI&location):Super(owner,location,&Component::SubOrbital::planeBase,turret)
									{}

			struct Detail
			{
				static int			WalkStack(const Grid::CellStack&stack, bool failOnStructureCount);

			};

		public:
			static count_t			CountTurretOccurrences(const OwnerParameter&,Component::Turret*);


			/**
			Sensor state of the shaft-sensor that scans the ground beneath the plane.
			Currently all planes carry one such, even if they cannot actually scan the ground.
			*/
			Grid::SkySensorState	skySensorState;

			int						GetCentralGroundHeight() const;
			static int				GetCentralGroundHeight(const Grid::LCellIdI&location);
			int						GetLandingHeight() const	{return GetLandingHeight(location);}
			static int				GetLandingHeight(const Grid::LCellIdI&location);

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
				bool				AttackNext(Grid::LCellIdI target, bool autoTarget);
				bool				RefreshGroundSensor(float&atTime, bool checkChangesNow=true, Buffer0<PWorldEntity>*newContacts = nullptr);
			#endif

			//new overridables:
			#ifndef CLIENT
				virtual bool		ExtCanHit(const Grid::LCellIdI&attackFrom,const Grid::LCellIdI&attackTarget,Grid::LCellIdI&actual) const {return false;}
				virtual bool		ProcessRound(float&atTime);
			#endif

			//TurretBase overrides:
			virtual float			GetFlatAttackRange() const override {return EMPModify(turret->horizontalAttackRange,1.f);}
			virtual bool			HasAmmoForShot() const override;
			#ifndef CLIENT
				virtual bool		FillAttackQueue()override;
			#endif

			//WorldEntity overrides:
			virtual bool			IsGroundBoundVehicle() const {return location.height <= Grid::CellStack::TopHeight;}	//landed
			virtual bool			IsPureVehicle() const override {return false;}
			virtual bool			IsPlane() const override {return true;}
			virtual PTurretBase		GetTurretBase(float&zOffset) override { zOffset = 0.f; return To<MobileEntity>::Cast(shared_from_this());}
			virtual PConstTurretBase	GetTurretBase(float&zOffset) const override { zOffset = 0.f; return To<const MobileEntity>::Cast(shared_from_this());}

			#ifdef HAS_WINDOW
				virtual void		DescribeType(BaseCommand::Label&outResult, UINT32 descFlags) override;
				virtual void		VisitAllCommands(FCommandVisitor visitor,bool force) override {turret->VisitAllCommands(visitor,force);};
				virtual float		GetSoundLevel() const {return IsGroundBoundVehicle() ? 0.f : GetActivityLevel()*0.25f;}	//save 75% for motion
			#endif

			virtual void			RegisterSensory(const Grid::LCellIdI&loc, float&atTime, bool checkChangesNow=true, Buffer0<PWorldEntity>*newContacts = nullptr) override;
			virtual void			UnregisterSensory(const Grid::LCellIdI&loc, float&atTime, bool checkChangesNow=true) override;
			#ifndef CLIENT
				virtual void		MoveSensory(const Grid::LCellIdI&oldLocation, const Grid::LCellIdI&newLocation, float&atTime, bool checkChangesNow/*=true*/, Buffer0<PWorldEntity>*newContacts/* = nullptr*/) override;
			#endif

			//MobileEntity overrides:
			#ifndef ONE_COMPOUND_PER_FACTION
				virtual ResourceVector		GetMaxStorageCapacity()	const override
				{
					return turret->baseStorageCapacity;
				}
			#endif

			virtual bool			MotionIgnoresAttack() const override {return true;}
			//virtual float			GetPathSize() const override {return 0.6f;}
			#ifndef CLIENT
				bool				DirectTo(mode_t mode, ArrayRef<Grid::LCellIdI> waypoints, bool detailedPath, bool leftClick = false) override;
				MobilePlacementResult	PlaceExactlyAt(const Grid::LCellIdI&, float timeCost, float minTimeOffset, bool signalEvent=true, bool isCreated=true, bool failIsFatal=false) override;
			#endif
		};




		template <typename Derivative>
			class Template : public Plane
			{
			protected:
				typedef Plane		Super;
				/**/					Template<Derivative>(const OwnerParameter&owner, Component::Turret*turret, const Grid::LCellIdI&location):Super(owner,turret,location)
				{
					#ifdef HAS_WINDOW
						Super::baseRenderingInstruction = &shape;
					#endif		

				}
				/**/					Template<Derivative>(Component::Turret*turret) : Super(OwnerParameter(),turret,Grid::LCellIdI(0, 0))
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
		/*static*/ const std::shared_ptr<Derivative>	Template<Derivative>::globalPrototype(new Derivative());

		#ifdef HAS_WINDOW
			template <typename Derivative>
				/*static*/ CommandContainer		Template<Derivative>::commands;
			template <typename Derivative>
				/*static*/ DX11::InstructionBlock32	Template<Derivative>::shape;
			template <typename Derivative>
				/*static*/ Geometry		Template<Derivative>::geometry;

			template <typename Derivative>
			/*virtual overrider*/ bool			Template<Derivative>::TestIntersection(const TVec3<>&_b, const TVec3<>&_d, float & distance) const
			{
			// {return Super::TestIntersection(geometry,b,d,distance);}
				TVec3<> b,d;//,test;
				Mat::transform(inverse,_b,b);
				Mat::rotate(inverse,_d,d);
			
				return geometry.TestIntersection(b,d,distance) | 	//binary or - must evaluate both
						turret->TestIntersection(0.f, turretAngleX,turretAngleY,b,d,distance);
			}

		#endif




		template <typename Derivative>
			/*virtual override*/	void			Template<Derivative>::PrototypeSetup()
			{
				turret = GetTurretType();
				#ifdef HAS_WINDOW
					commands.baseCoords = int2(4,0)+BaseCommand::coordOffset;
				#endif
			}

		#ifdef HAS_WINDOW
			template <typename Derivative>
				/*virtual override*/	void		Template<Derivative>::PrototypeLoadGeometry()
				{
					geometry.Clear();
					String name = GetTypeName();
					name.set(0, ::Template::tolower(name.firstChar()));
					name.replace(' ',"");
					geometry.LoadFromObj(PathString("objects/"+name+".obj"));
					geometry.Scale(vehicleScale);
					if (shape.shader.IsEmpty())
						(DX11::BaseInstructionBlock&)shape = stdVehicleShader;
					shape.vbo.CreateFromStructs(geometry.GetLOD(0).vertices);
					shape.ibo.Create(geometry.GetLOD(0).indices);

				}
		#endif





		class Raptor : public Template<Raptor>
		{
		protected:
			typedef Template<Raptor>	Super;
			friend class Super;

			/**/					Raptor(const OwnerParameter&owner, const Grid::LCellIdI&location):Super(owner,GetTurretType(),location)
									{}
			/**/					Raptor():Super(GetTurretType())
									{}	//global prototype
		public:

			#ifndef CLIENT
				//Satellite overrides:
				virtual bool		ExtCanHit(const Grid::LCellIdI&attackFrom,const Grid::LCellIdI&attackTarget,Grid::LCellIdI&actual) const override;
			#endif


			//MobileEntity overrides:
			virtual int				GetMovementStepsPerRound() const override
			{
				return (int)ceil(EMPModify(10.f,1.f));
			}


			//BaseEntity overrides:
			virtual String			GetTypeName() const override {return "Raptor";}
			virtual ConstName		GetCTypeName() const override {return ConstName::Type_Raptor;}

			//SatelliteTemplate overrides:
			virtual Component::Turret*
									GetTurretType() const override	{return &Component::SubOrbital::raptor;}

			//WorldEntity overrides:
			virtual int				GetConstructionCost() const override {return 80;}
			virtual int				GetMaxHealth() const {return 5;}

			#ifndef NO_HIBERNATION
				virtual int			GetUplinkTraffic() const override {return 4;}
			#endif

			//Hibernating overrides:
			#ifndef NO_HIBERNATION
				virtual int			GetUplinkPriority() const override {return 900;}
			#endif
		};

		class CargoPlane : public Template<CargoPlane>, public HangarStructure
		{
		protected:
			typedef Template<CargoPlane>	Super;
			friend class Super;

			/**/					CargoPlane(const OwnerParameter&owner, const Grid::LCellIdI&location):Super(owner,GetTurretType(),location)
									{}
			/**/					CargoPlane():Super(GetTurretType())
									{}	//global prototype
		public:
			struct TStored
			{
				PMobileEntity		entity;
				#ifdef HAS_WINDOW
					DeployMobileCommand	deployCommand;
				#endif

				/**/				TStored(const PMobileEntity&e):entity(e)	{}

				bool				operator==(const PMobileEntity&e) const {return entity == e;}
			};

			Buffer0<TStored>		stored;
			static const count_t	MaxStorage = 4;

			//MobileEntity overrides:
			virtual int				GetMovementStepsPerRound() const override
			{
				return (int)ceil(EMPModify(8.f,1.f));
			}


			//BaseEntity overrides:
			virtual String			GetTypeName() const override {return "Cargo Plane";}
			virtual ConstName		GetCTypeName() const override {return ConstName::Type_CargoPlane;}

			//SatelliteTemplate overrides:
			virtual Component::Turret*
									GetTurretType() const override	{return &Component::SubOrbital::cargoPlane;}

			//WorldEntity overrides:
			virtual int				GetConstructionCost() const override {return 40;}
			virtual int				GetMaxHealth() const {return 15;}
			#ifdef HAS_WINDOW
				virtual void		OnSelect()	override {deployingMobileEntity.reset();};
				virtual void		OnDeselect() override {deployingMobileEntity.reset();}
			#endif
			virtual PHangarStructure	CastToDestinationForType(const MobileEntity&e) override;

			#ifndef NO_HIBERNATION
				virtual int			GetUplinkTraffic() const override {return 4;}
			#endif

			//Hibernating overrides:
			#ifndef NO_HIBERNATION
				virtual int			GetUplinkPriority() const override {return 900;}
			#endif

			//HangarStructure overrides:
			virtual const WorldEntity&	ToEntity() const override {return *this;}
			virtual WorldEntity&		ToEntity() override {return *this;}

			virtual bool	CanStoreType(const MobileEntity&e) const override {return e.IsVehicle();}
			virtual bool	HasStoredLocally(const PMobileEntity&e) const override {return stored.Contains(e);}
			virtual void	StoreLocallyFromImport(const PMobileEntity&e) override {ASSERT2__(e->IsVehicle(),GetName(),e->GetName()); stored << e;}
			virtual bool	Store(const PMobileEntity&e, float&atTime) override;
		};


		extern Buffer0<PPlane>		planes;
	}
}

#endif

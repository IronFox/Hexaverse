#ifndef objectsSubOrbitalH
#define objectsSubOrbitalH

#include "entity.h"
#include "../cast.h"
#include "hibernating.h"
#include "mobileEntity.h"
#include "interface.h"
#include "mobileEntityProducer.h"
#include "vehicle.h"
#include "hangar.h"


namespace Objects
{

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
		#ifdef HAS_WINDOW
			/**
			Type-shared appearance
			*/
			PClientAttachment		shape;
		#endif
			static count_t			CountTurretOccurrences(const OwnerParameter&,Component::Turret*);


			int						GetCentralGroundHeight() const;
			static int				GetCentralGroundHeight(const Grid::LCellIdI&location);
			int						GetLandingHeight() const	{return GetLandingHeight(location);}
			static int				GetLandingHeight(const Grid::LCellIdI&location);


			#ifndef CLIENT
				/**
				Adjust to ground changes. Should be called once per round to account for 
				@return true, if changes were detected and applied, false otherwise
				*/
				bool				AttackNext(Grid::LCellIdI target, bool autoTarget);
				bool				AttackFirst(Grid::LCellIdI target, bool autoTarget);
				virtual bool		WouldAttack(const PWorldEntity&e, TurretBase::TargetChoice&outTarget, float entityTime) {return false;}
			#endif

			//new overridables:
			#ifndef CLIENT
				virtual bool		ExtCanHit(const Grid::LCellIdI&attackFrom,const Grid::LCellIdI&attackTarget, TargetChoice&outExtResult) const {return false;}
				virtual bool		ProcessRound(float&atTime);
			#endif
			float					GetMaxAirSpeed() const {return 100.f / turret->weight.GetValue(GetResearchState());};

			//TurretBase overrides:
			virtual float			GetFlatAttackRange() const override;
			virtual String			DescribeFlatAttackRange() const override;
			#ifndef CLIENT
				virtual bool		FrontQueueAutoAttack(TurretBase::TargetChoice&outChoice) override;
			#endif

			//WorldEntity overrides:
			virtual int				GetMaxHealth() const override {return turret->healthMultiplier.GetValue(GetResearchState());}
			virtual String			DescribeMaxHealth() const override {return turret->healthMultiplier.Describe(GetResearchState());}
			virtual int				GetConstructionCost() const override {return turret->productionCosts.GetValue(GetResearchState());}
			virtual bool			IsGroundBoundVehicle() const {return location.height <= Grid::CellStack::TopHeight;}	//landed
			virtual bool			IsPureVehicle() const override {return false;}
			virtual bool			IsPlane() const override {return true;}
			virtual PTurretBase		GetTurretBase(float&zOffset) override { zOffset = 0.f; return To<MobileEntity>::Cast(shared_from_this());}
			virtual PConstTurretBase	GetTurretBase(float&zOffset) const override { zOffset = 0.f; return To<const MobileEntity>::Cast(shared_from_this());}
			virtual bool			IsUnlockedAtState(const Lua::Reference&researchState) const	{return turret->isUnlocked.GetValue(researchState);}

			#ifdef HAS_WINDOW
				virtual void		DescribeType(Description&outResult, UINT32 descFlags) override;
				virtual float		GetSoundLevel() const {return IsGroundBoundVehicle() ? 0.f : GetActivityLevel()*0.25f;}	//save 75% for motion
			#endif

			virtual void			VisitSensors(const Grid::LCellIdI&loc, const std::function<void(const World::Sensor&)>&visitor, bool appearance = false) const override;
			virtual bool			CanSpawnAboveGrid(int&outHeight) const override {outHeight = Height; return true;}
			virtual float3			GetScanningCenter(const Grid::LCellIdI&loc) const override;
			

			//MobileEntity overrides:
			virtual int				GetMovementStepsPerRound() const override
			{
				return (int)ceil(EMPModify(GetMaxAirSpeed(),1.f));
			}
			#ifndef ONE_COMPOUND_PER_FACTION
				virtual ResourceVector		GetMaxStorageCapacity()	const override
				{
					return turret->baseStorageCapacity;
				}
			#endif

			virtual bool			MotionIgnoresAttack() const override {return true;}
			//virtual float			GetPathSize() const override {return 0.6f;}
			#ifndef CLIENT
				virtual bool		DirectTo(CursorMode, MutableArrayRef<Grid::LCellIdI> waypoints, bool detailedPath, bool leftClick = false) override;
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
						ASSERT__(staticShape);
						Super::shape = staticShape;
					#endif		

				}
				/**/					Template<Derivative>(Component::Turret*turret) : Super(OwnerParameter(),turret,Grid::LCellIdI(0, 0))
				{ 
					#ifdef HAS_WINDOW
					//	ASSERT__(staticShape);
						Super::shape = staticShape;
					#endif		
				}	//global prototype

			public:
				static const std::shared_ptr<Derivative>	globalPrototype;
				#ifdef HAS_WINDOW
	

					static CommandContainer		commands;
					static PClientAttachment	staticShape;
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
					virtual bool		TestIntersection(const M::TVec3<>&b, const M::TVec3<>&d, float & distance) const override;
					virtual const CC*	GetCommands(CC&dynamicStorage) const override
					{
						dynamicStorage.Append(Super::GetCommands(dynamicStorage));
						dynamicStorage.Append(commands);
						return &dynamicStorage;
					}
				#endif

			};

		template <typename Derivative>
		/*static*/ const std::shared_ptr<Derivative>	Template<Derivative>::globalPrototype(new Derivative());

		#ifdef HAS_WINDOW
			template <typename Derivative>
				/*static*/ CommandContainer		Template<Derivative>::commands;
			template <typename Derivative>
				/*static*/ PClientAttachment	Template<Derivative>::staticShape;
			template <typename Derivative>
				/*static*/ Geometry		Template<Derivative>::geometry;

			template <typename Derivative>
			/*virtual overrider*/ bool			Template<Derivative>::TestIntersection(const M::TVec3<>&_b, const M::TVec3<>&_d, float & distance) const
			{
			// {return Super::TestIntersection(geometry,b,d,distance);}
				M::TVec3<> b,d;//,test;
				Mat::Transform(inverse,_b,b);
				Mat::Rotate(inverse,_d,d);
			
				return geometry.TestIntersection(b,d,distance) | 	//binary or - must evaluate both
						turret->TestIntersection(0.f, turretAngleX,turretAngleY,b,d,distance);
			}

		#endif




		template <typename Derivative>
			/*virtual override*/	void			Template<Derivative>::PrototypeSetup()
			{
				turret = GetTurretType();
			}





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
				virtual bool		ExtCanHit(const Grid::LCellIdI&attackFrom,const Grid::LCellIdI&attackTarget, TargetChoice&outExtResult) const override;
				virtual bool		WouldAttack(const PWorldEntity&e, TurretBase::TargetChoice&outTarget, float entityTime) override;
			#endif


			//BaseEntity overrides:
			virtual String			GetTypeName() const override {return "Raptor";}
			virtual EntityType		GetType() const override {return ConstName::Type_Raptor;}

			//SatelliteTemplate overrides:
			virtual Component::Turret*
									GetTurretType() const override	{return &Component::SubOrbital::raptor;}

			//WorldEntity overrides:
			#ifndef NO_HIBERNATION
				virtual int			GetUplinkTraffic() const override {return 4;}
			#endif
			virtual bool			IsSentry() const override {return true;}

			//Hibernating overrides:
			#ifndef NO_HIBERNATION
				virtual int			GetUplinkPriority() const override {return 900;}
			#endif
		};

		class CargoPlane : public Template<CargoPlane>, public Hangar
		{
		protected:
			typedef Template<CargoPlane>	Super;
			friend class Super;

			/**/					CargoPlane(const OwnerParameter&owner, const Grid::LCellIdI&location):Super(owner,GetTurretType(),location)
									{}
			/**/					CargoPlane():Super(GetTurretType())
									{}	//global prototype
		public:
			static const count_t	MaxStorage = 4;

			//BaseEntity overrides:
			virtual String			GetTypeName() const override {return "Cargo Plane";}
			virtual EntityType		GetType() const override {return ConstName::Type_CargoPlane;}

			//SatelliteTemplate overrides:
			virtual Component::Turret*
									GetTurretType() const override	{return &Component::SubOrbital::cargoPlane;}

			//WorldEntity overrides:
			virtual bool			IsHangar() const {return true;}
			virtual PHangar			CastToDestinationForType(const MobileEntity&e) override;
			virtual void			OnDestroy(RemovalType, float&) override;
			#ifdef HAS_WINDOW
				virtual const CC*	GetCommands(CC&dynamicStorage) const override;
			#endif
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
			virtual count_t				GetLoad() const override { return this->stored.Count(); }
			virtual bool	CanStoreType(const MobileEntity&e) const override {return e.IsVehicle() && !e.IsHangar();}
			virtual void	StoreLocallyFromImport(const PMobileEntity&e) override;
			#ifndef CLIENT
				virtual bool	Store(const PMobileEntity&e, float&atTime) override;
			#endif
			virtual count_t	GetMaxStorage() const override {return MaxStorage;}
		};


		class SensorDrone : public Template<SensorDrone>
		{
		protected:
			typedef Template<SensorDrone>	Super;
			friend class Super;

			/**/					SensorDrone(const OwnerParameter&owner, const Grid::LCellIdI&location):Super(owner,GetTurretType(),location)
									{}
			/**/					SensorDrone():Super(GetTurretType())
									{}	//global prototype
		public:



			//BaseEntity overrides:
			virtual String			GetTypeName() const override {return "Sensor Drone";}
			virtual EntityType		GetType() const override {return ConstName::Type_SensorDrone;}

			//SatelliteTemplate overrides:
			virtual Component::Turret*
									GetTurretType() const override	{return &Component::SubOrbital::sensorDrone;}

			//WorldEntity overrides:
			#ifndef NO_HIBERNATION
				virtual int			GetUplinkTraffic() const override {return 4;}
			#endif

			//Hibernating overrides:
			#ifndef NO_HIBERNATION
				virtual int			GetUplinkPriority() const override {return 900;}
			#endif
		};


		extern SharedContainer<Plane>		planes;
	}
}

#endif

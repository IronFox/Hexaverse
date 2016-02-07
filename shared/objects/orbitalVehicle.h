#ifndef objectsOrbitalVehicleH
#define objectsOrbitalVehicleH


/*
* a small sphere layer window that hosts all space objects around planets
* every object in space demands fuel per round, withdrawn from local storage every round.
	+ Can be withdrawn from nearby satellites, but never to below than that satellite needs per round
	+ objects lacking fuel degrade in height once
	+ failure to restore their height leads to object loss
	+ motion costs extra-fuel. some satellites are very expensive to move

* SkyGates are responsible for transfering resources from and to the planet surface
	+ SkyGates automatically attempt to supply the satellite currently connected. This is independent of the following:
	+ Additional commands allow to transfer 10 units of either resource per round up to the satellite. These are modular switches.
* Likewise any satellite has commands to transfer 10 units of either resource per round downwards to connected SkyGates.



Satellite types:
	* Ragnarök:
		Pure offensive. Limited visual range. Low health. Slow. High fuel and uranium storage. Cannot unload resources.
	* SkyHub:
		High storage capacity for all resource types.
		High cost for motion.
		Can build other satellites.
		Can build SkyGate



* SkyGates require fuel and 
* SkyHub and SkyGate can exist independently and build each other
* Satellites cannot hibernate (must change this, if i ever re-enable hibernation)


*/


#include "entity.h"
#include "../cast.h"
#include "hibernating.h"
#include "mobileEntity.h"
#include "interface.h"
#include "mobileEntityProducer.h"
#include "orbitalStructure.h"

namespace Objects
{
	#ifdef HAS_WINDOW
		extern DX11::BaseInstructionBlock	stdVehicleShader;	//don't want to include rendering.h here
	#endif

	namespace Orbital
	{
		/**
		List of all living ships.
		Does NOT overlap with Objects::vehicles
		*/
		extern Buffer0<PShip>			ships;

		/**
		@brief Base sattelite entity

		Satellites under normal operation maintain a standard height 0, which is translated into 
		Satellite::BaseHeight during loading, and re-converted during storing.
		Satellites in positional decline move to Satellite::DeclineHeight. Any loaded height <0 is
		considered declined height, regardless of actual value
		In the context of this game, the term satellite also applies to space ships in general.
		More station-like structures move very slowly, while space ships can move much faster

		*/
		class Ship : public MobileEntity
		{
		protected:
			typedef MobileEntity	Super;
			/**/					Ship(const OwnerParameter&owner, Component::Turret*turret, const Grid::LCellIdI&location):Super(owner,turret,location)
									{}


		public:
			static const int		BaseResidenceHeight = Orbital::BaseHeight;
			static const int		DeclinedResidenceHeight = Orbital::DeclinedHeight;


			static count_t			CountTurretOccurrences(const OwnerParameter&,Component::Turret*);


			/**
			Sensor state of the shaft-sensor that scans the ground beneath the satellite.
			Currently all satellites carry one such, even if they cannot actually scan the ground.
			Due to their ongoing fuel supply it is not to be expected that a lot of satellites
			roam the skies.
			*/
			Grid::SkySensorState	skySensorState;



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
			#endif

			virtual void			GetUpkeep(ResourceVector&v) const override {Super::GetUpkeep(v); v.level[Resource::Fuel] = GetConstructionCost()/40;}

			//new overridables:
			#ifndef CLIENT
				virtual bool		ExtCanHit(const Grid::LCellIdI&attackFrom,const Grid::LCellIdI&attackTarget,Grid::LCellIdI&actual) const {return false;}
				virtual bool		WouldAttack(const PWorldEntity&target, Grid::LCellIdI&outTarget, float entityTime)	{return false;}
			#endif

			//TurretBase overrides:
			virtual float			GetFlatAttackRange() const override {return EMPModify(turret->horizontalAttackRange,1.f);}
			virtual bool			HasAmmoForShot() override;
			#ifndef CLIENT
				virtual bool		FillAttackQueue()override;
			#endif

			//WorldEntity overrides:
			virtual bool			IsOrbital() const override {return true;}
			virtual PTurretBase		GetTurretBase(float&zOffset) override { zOffset = 0.f; return To<MobileEntity>::Cast(shared_from_this());}
			virtual PConstTurretBase	GetTurretBase(float&zOffset) const override { zOffset = 0.f; return To<const MobileEntity>::Cast(shared_from_this());}

			#ifdef HAS_WINDOW
				virtual void		DescribeType(BaseCommand::Label&outResult, UINT32 descFlags) override;
				virtual void		VisitAllCommands(FCommandVisitor visitor) override {turret->VisitAllCommands(visitor);};
			#endif

			virtual void			RegisterSensory(const Grid::LCellIdI&loc, float&atTime, bool checkChangesNow=true, Buffer0<PWorldEntity>*newContacts = nullptr) override;
			virtual void			UnregisterSensory(const Grid::LCellIdI&loc, float&atTime, bool checkChangesNow=true) override;


			//MobileEntity overrides:
			#ifndef CONNECTOR_LESS
				virtual ResourceVector		GetMaxStorageCapacity()	const override
				{
					return turret->baseStorageCapacity;
				}
			#endif
			virtual int				GetOperationSpeed(Component::operation_t op) const override {return turret->operationSpeed[op];}
			virtual bool			MotionIgnoresAttack() const override {return false;}
			virtual float			GetPathSize() const override {return 1.f;}
			#ifndef CLIENT
				bool				DirectTo(mode_t mode, ArrayRef<Grid::LCellIdI> waypoints, bool detailedPath, bool leftClick = false) override;
				MobilePlacementResult	PlaceExactlyAt(const Grid::LCellIdI&, float timeCost, float minTimeOffset, bool signalEvent=true, bool isCreated=true, bool failIsFatal=false) override;
			#endif
		};




		template <typename Derivative>
			class ShipTemplate : public Ship
			{
			protected:
				typedef Ship			Super;
				/**/					ShipTemplate<Derivative>(const OwnerParameter&owner, Component::Turret*turret, const Grid::LCellIdI&location):Super(owner,turret,location)
				{
					#ifdef HAS_WINDOW
						Super::baseRenderingInstruction = &shape;
					#endif		

				}
				/**/					ShipTemplate<Derivative>(Component::Turret*turret) : Super(OwnerParameter(),turret,Grid::LCellIdI(0, 0))
				{ 
					#ifdef HAS_WINDOW
						Super::baseRenderingInstruction = &shape;
					#endif		
				}	//global prototype

			public:
				static const std::shared_ptr<Derivative>	globalPrototype;
				#ifdef HAS_WINDOW
					static Buffer0<IconCommand>		iconCommands;

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
					virtual void		VisitAllCommands(FCommandVisitor visitor) override {Super::VisitAllCommands(visitor); foreach (iconCommands,cmd) visitor(*cmd);};
				#endif

			};

		template <typename Derivative>
		/*static*/ const std::shared_ptr<Derivative>	ShipTemplate<Derivative>::globalPrototype(new Derivative());

		#ifdef HAS_WINDOW
			template <typename Derivative>
				/*static*/ Buffer0<IconCommand>		ShipTemplate<Derivative>::iconCommands;
			template <typename Derivative>
				/*static*/ DX11::InstructionBlock32	ShipTemplate<Derivative>::shape;
			template <typename Derivative>
				/*static*/ Geometry		ShipTemplate<Derivative>::geometry;

			template <typename Derivative>
			/*virtual overrider*/ bool			ShipTemplate<Derivative>::TestIntersection(const TVec3<>&_b, const TVec3<>&_d, float & distance) const
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
			/*virtual override*/	void			ShipTemplate<Derivative>::PrototypeSetup()
			{
				turret = GetTurretType();

			}

		#ifdef HAS_WINDOW
			template <typename Derivative>
				/*virtual override*/	void		ShipTemplate<Derivative>::PrototypeLoadGeometry()
				{
					geometry.Clear();
					String name = GetTypeName();
					name.set(0, Template::tolower(name.firstChar()));
					name.replace(' ',"");
					geometry.LoadFromObj(PathString("objects/"+name+".obj"));
					if (shape.shader.IsEmpty())
						(DX11::BaseInstructionBlock&)shape = stdVehicleShader;
					shape.vbo.CreateFromStructs(geometry.GetLOD(0).vertices);
					shape.ibo.Create(geometry.GetLOD(0).indices);

				}
		#endif




		class Ragnarok : public ShipTemplate<Ragnarok>
		{
		protected:
			typedef ShipTemplate<Ragnarok>	Super;
			friend class Super;

			/**/					Ragnarok(const OwnerParameter&owner, const Grid::LCellIdI&location):Super(owner,GetTurretType(),location)
									{}
			/**/					Ragnarok():Super(GetTurretType())
									{}	//global prototype
		public:
			//MobileEntity overrides:
			virtual int				GetMovementStepsPerRound() const override {return 2;}

			//BaseEntity overrides:
			virtual String			GetTypeName() const override {return "Ragnarök";}
			virtual ConstName		GetCTypeName() const override {return ConstName::ShipType_Ragnarok;}

			//SatelliteTemplate overrides:
			virtual Component::Turret*
									GetTurretType() const override	{return &Component::Orbital::ragnarok;}

			//WorldEntity overrides:
			virtual int				GetConstructionCost() const override {return 100;}
			#ifndef NO_HIBERNATION
				virtual int			GetUplinkTraffic() const override {return 50;}

				//Hibernating overrides:
				virtual int			GetUplinkPriority() const override {return 890;}
			#endif

		};


		class Engineer : public ShipTemplate<Engineer>
		{
		protected:
			typedef ShipTemplate<Engineer>	Super;
			friend class Super;

			/**/					Engineer(const OwnerParameter&owner, const Grid::LCellIdI&location):Super(owner,GetTurretType(),location)
									{}
			/**/					Engineer():Super(GetTurretType())
									{}	//global prototype
		public:
			//MobileEntity overrides:
			virtual int				GetMovementStepsPerRound() const override {return 5;}

			//BaseEntity overrides:
			virtual String			GetTypeName() const override {return "Orbital Engineer";}
			virtual ConstName		GetCTypeName() const override {return ConstName::ShipType_Engineer;}

			//SatelliteTemplate overrides:
			virtual Component::Turret*
									GetTurretType() const override	{return &Component::Orbital::engineer;}

			//WorldEntity overrides:
			virtual int				GetConstructionCost() const override {return GetTurretType()->productionCosts + 20;}
			#ifndef NO_HIBERNATION
				virtual int			GetUplinkTraffic() const override {return GetTurretType()->uplinkTraffic;}

				//Hibernating overrides:
				virtual int			GetUplinkPriority() const override {return GetTurretType()->uplinkPriority;}
			#endif

		};





	}
}

#endif

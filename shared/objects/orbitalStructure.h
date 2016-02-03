#ifndef ObjectsOrbitalStructureH
#define ObjectsOrbitalStructureH

#include "types.h"
#include "structure.h"
#include "mobileEntityProducer.h"

namespace Objects
{
	namespace Orbital
	{
		/**
		List of all living stations.
		Also contained by Objects::structures
		*/
		extern Buffer0<PStation>stations;

		static const int		BaseHeight = Grid::CellStack::HalfRange * 5;
		static const int		DeclinedHeight = BaseHeight - 4;


		class Station : public Tower
		{
			typedef Tower		Super;
		protected:
			/**/				Station(Component::Turret*turret) : Super(turret)
			{}
			/**/				Station(Component::Turret*turret, const OwnerParameter&owner, const Grid::LCellIdI&location, float time) : Super(turret,owner,location,false,time)
			{
				UpdateOrbitalSystem();
			}

		public:

			/**
			Sensor state of the shaft-sensor that scans the ground beneath the satellite.
			Currently all satellites carry one such, even if they cannot actually scan the ground.
			Due to their ongoing fuel supply it is not to be expected that a lot of satellites
			roam the skies.
			*/
			Grid::SkySensorState	skySensorState;


			/**
			Updates the local system matrix
			*/
			void				UpdateOrbitalSystem();
			static int			GetStructureBuildHeight(const Grid::LCellIdI&satelliteLocation);
			int					GetStructureBuildHeight() const;

			//WorldEntity overrides:
			virtual void		RegisterSensory(const Grid::LCellIdI&loc, float&atTime, bool checkChangesNow=true, Buffer0<PWorldEntity>*newContacts = nullptr) override;
			virtual void		UnregisterSensory(const Grid::LCellIdI&loc, float&atTime, bool checkChangesNow=true) override;
			#ifndef CLIENT
				virtual	void	SetupSystem() override {UpdateOrbitalSystem();}
			#endif
			virtual PTurretBase			GetTurretBase(float&zOffset) override { zOffset = 0.f; return To<Station>::Cast(shared_from_this());}
			virtual PConstTurretBase	GetTurretBase(float&zOffset) const override { zOffset = 0.f; return To<const Station>::Cast(shared_from_this());}
			virtual bool		IsOrbital() const override {return true;}
			virtual bool		HasFoundation() const override {return false;}
			virtual void		GetUpkeep(ResourceVector&v) const override {Super::GetUpkeep(v); v.level[Resource::Fuel] = CeilDiv(GetConstructionCost(),4);}


			#ifndef CLIENT
				virtual void	CorrectTerrain(bool signalChange, float atTime, bool deep) override	{};
				virtual void	OnConstructionFinished(float&atTime) override	{Super::OnConstructionFinished(atTime);};
				/**
				Adjust to ground changes. Should be called once per round to account for 
				@return true, if changes were detected and applied, false otherwise
				*/
				bool			RefreshGroundSensor(float&atTime);
			#endif

			//new overridables:
			virtual Component::Turret*GetTurretType() const  = 0;
			virtual bool		IsHub() const {return false;}
		};


		template<typename Derivative>
			class FloatingStructureT : public Station
			{
				typedef Station	Super;
			protected:
				/**/				FloatingStructureT(Component::Turret*turret) : Super(turret)
				{
					#ifdef HAS_WINDOW
						Super::localizableShape = &shape;
					#endif
				}
				/**/				FloatingStructureT(Component::Turret*turret, const OwnerParameter&owner, const Grid::LCellIdI&location, float time) : Super(turret,owner,location,time)
				{
					#ifdef HAS_WINDOW
						Super::localizableShape = &shape;
					#endif
				}
			public:
				static const std::shared_ptr<Derivative>		globalPrototype;
				#ifdef HAS_WINDOW
					static Buffer0<IconCommand>	commands;
					static Buffer0<EntityConstructionCommand>		constructionCommands;


					/**
					Render-ready, object-space instruction to be used for all instances of this structure type
					*/
					static DX11::InstructionBlock32	shape;
					/**
					Intersectable, object-space geometry of all instances of this structure type
					*/
					static Geometry			geometry;
				#endif

				virtual PWorldEntity		CreateNewInstance(const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) override {return PWorldEntity(new Derivative(owner,location,atTime));};

				virtual void				PrototypeSetup()	override;
				#ifdef HAS_WINDOW
					virtual void			PrototypeLoadGeometry() override;
				#endif

				#ifndef CLIENT
				#else
					#ifdef HAS_WINDOW
						virtual bool		TestIntersection(const TVec3<>&b, const TVec3<>&d, float & distance) const override;
						virtual void		VisitAllCommands(FCommandVisitor visitor) override {foreach (commands,cmd) visitor(*cmd);};
					#endif
				#endif
			};
		
		
			template <typename Derivative>
				/*virtual override*/	void			FloatingStructureT<Derivative>::PrototypeSetup()
				{
					Super::PrototypeSetup();
				}


		
		
			template <typename Derivative>
				/*static*/ const std::shared_ptr<Derivative>		FloatingStructureT<Derivative>::globalPrototype(new Derivative());

			#ifdef HAS_WINDOW
				template <typename Derivative>
					/*static*/ DX11::InstructionBlock32	FloatingStructureT<Derivative>::shape;


				template <typename Derivative>
					/*static*/ Geometry				FloatingStructureT<Derivative>::geometry;
				template <typename Derivative>
					/*static*/ Buffer0<IconCommand>		FloatingStructureT<Derivative>::commands;
				template <typename Derivative>
					/*static*/ Buffer0<EntityConstructionCommand>		FloatingStructureT<Derivative>::constructionCommands;
			#endif

		
			#ifdef HAS_WINDOW


				template <typename Derivative>
					/*virtual override*/ bool			FloatingStructureT<Derivative>::TestIntersection(const TVec3<>&_b, const TVec3<>&_d, float & distance) const
					{
						TVec3<> b,d;//,test;
						Mat::transform(inverse,_b,b);
						Mat::rotate(inverse,_d,d);
			
						return WorldEntity::TestIntersection(geometry, b,d,distance) | turret->TestIntersection(0.f, turretAngleX,turretAngleY,b,d,distance);	//binary or - must evaluate both
					}

				template <typename Derivative>
					/*virtual override*/	void			FloatingStructureT<Derivative>::PrototypeLoadGeometry()
					{
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
		
		
		


			class Hub : public FloatingStructureT<Hub>, public EntityFactory
			{
			protected:
				typedef FloatingStructureT<Hub>	Super;
				friend class Super;

				static const int		ProductionSpeed = 10;
				static const int		EnergyConsumption = 10;

				/**/					Hub(const OwnerParameter&owner, const Grid::LCellIdI&location, float time):Super(GetTurretType(),owner,location,time),EntityFactory(ProductionSpeed) {}
				/**/					Hub():Super(GetTurretType()),EntityFactory(ProductionSpeed){}	//global prototype

				static int2				AllocateProductionSlot();

				static void				RegisterProduction(const PShip&);
				//static void				RegisterProduction(const PStructure&);

				#ifdef HAS_WINDOW
					static Buffer<EntityConstructionCommand>	entityConstructionCommands;

					struct MyCmd
					{
						static bool		CanBuild(const PStructure&what, const PWorldEntity&who);
						static void		Build(const PShip&what, const PWorldEntity&who);
					};
				#endif

			public:
				WSkyLinkStructure	connectedToLink;


				/**
				Updates @a connectedTo if necessary
				@return true if anything changed
				*/
				bool				UpdateConnections();
				bool				GainGroundConnection(const PSkyLinkStructure&to);
				void				LoseGroundConnection();


				#ifndef CLIENT
					virtual void		Tick(ResourceVector&v) override;
				#endif

				//Station overrides:
				virtual bool			IsHub() const override {return true;}

				//WorkingStructure overrides:
				virtual void			GetResourceVector(ResourceVector&v,ResourceVector&, ResourceVector&) const override	{v.level[Resource::Energy] = -EnergyConsumption; v.level[Resource::Minerals] = -(int)EntityFactory::productionPerRound;}

				//BaseEntity overrides:
				virtual String			GetTypeName() const override {return "Sky Hub";}
				virtual ConstName		GetCTypeName() const override {return ConstName::StationType_Hub;}


				//Hibernating overrides:
				#ifndef NO_HIBERNATION
					virtual int			GetUplinkPriority() const override {return 500;}
				#endif


				//SatelliteTemplate overrides:
				virtual Component::Turret*
										GetTurretType() const override	{return &Component::Orbital::hub;}

				////MobileEntity overrides:
				//virtual Grid::LCellIdI	GetConstructionLocation(const Grid::LCellIdI&focus) const {return Grid::LCellIdI(focus.stackID,GetStructureBuildHeight(focus));}

				//WorldEntity overrides:
				virtual int				GetConstructionCost() const override {return 100;}
				#ifndef NO_HIBERNATION
					virtual int			GetUplinkTraffic() const override {return 10;}
				#endif
				virtual void			OnDestroy(float&atTime) override {LoseGroundConnection();Super::OnDestroy(atTime);}
				virtual bool			AwaitsUserInteraction() const override {return EntityFactory::AwaitsUserInteraction();}
				#ifdef HAS_WINDOW
					virtual void		VisitAllCommands(FCommandVisitor visitor) override;
					virtual void		OnSelect() override;
					virtual void		OnDeselect() override;
				#endif
				virtual const EntityFactory*CastToFactory() const override {return this;}
				virtual void			PrototypeSetup() override;
				#ifndef CLIENT
					virtual void		OnConstructionFinished(float&atTime) override;
				#endif
				virtual void			VisitAllConnected(const std::function<void(const PStructure&)>&visitor) const override;

			};
		
		
		
		
			class Guardian : public FloatingStructureT<Guardian>
			{
			protected:
				typedef FloatingStructureT<Guardian>	Super;
				friend class Super;

				/**/					Guardian(const OwnerParameter&owner, const Grid::LCellIdI&location, float time):Super(GetTurretType(),owner,location, time)
										{}
				/**/					Guardian():Super(GetTurretType())
										{}	//global prototype
			public:


				//BaseEntity overrides:
				virtual String			GetTypeName() const override {return "Sky Guardian";}
				virtual ConstName		GetCTypeName() const override {return ConstName::StationType_Guardian;}

				//SatelliteTemplate overrides:
				virtual Component::Turret*
										GetTurretType() const override	{return &Component::Orbital::protonCannon;}

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



			class StarPort : public FloatingStructureT<StarPort>
			{
			protected:
				typedef FloatingStructureT<StarPort>	Super;
				friend class Super;

				/**/					StarPort(const OwnerParameter&owner, const Grid::LCellIdI&location, float time):Super(GetTurretType(),owner,location, time)
										{}
				/**/					StarPort():Super(GetTurretType())
										{}	//global prototype
			public:
				static const int		ProductionSpeed = 5;

				//BaseEntity overrides:
				virtual String			GetTypeName() const override {return "Star Port";}
				virtual ConstName		GetCTypeName() const override {return ConstName::StationType_StarPort;}

				//SatelliteTemplate overrides:
				virtual Component::Turret*
										GetTurretType() const override	{return &Component::Orbital::starPort;}

				//WorldEntity overrides:
				virtual int				GetConstructionCost() const override {return 200;}
				#ifndef NO_HIBERNATION
					virtual int			GetUplinkTraffic() const override {return 5;}

					//Hibernating overrides:
					virtual int			GetUplinkPriority() const override {return 1000;}
				#endif

			};
		
		
	}
	
	
	
	
	
}





#endif

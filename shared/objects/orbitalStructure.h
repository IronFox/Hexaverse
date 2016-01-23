#ifndef ObjectsOrbitalStructureH
#define ObjectsOrbitalStructureH

#include "types.h"
#include "structure.h"
#include "mobileEntityProducer.h"

namespace Objects
{
	namespace Orbital
	{
		extern Buffer0<PStation>stations;

		static const int		BaseHeight = Grid::CellStack::HalfRange * 5;
		static const int		DeclinedHeight = BaseHeight - 4;


		class Station : public Tower
		{
			typedef Tower		Super;
		protected:
			/**/				Station(Component::Turret*turret) : Super(turret)
			{}
			/**/				Station(Component::Turret*turret, const OwnerParameter&owner, const Grid::LCellIdI&location, float time) : Super(turret,owner,location,time)
			{}

		public:

			/**
			Sensor state of the shaft-sensor that scans the ground beneath the satellite.
			Currently all satellites carry one such, even if they cannot actually scan the ground.
			Due to their ongoing fuel supply it is not to be expected that a lot of satellites
			roam the skies.
			*/
			Grid::SkySensorState	skySensorState;


			virtual void		RegisterSensory(const Grid::LCellIdI&loc, float&atTime, bool checkChangesNow=true, Buffer0<PWorldEntity>*newContacts = nullptr) override;
			virtual void		UnregisterSensory(const Grid::LCellIdI&loc, float&atTime, bool checkChangesNow=true) override;



			static int			GetStructureBuildHeight(const Grid::LCellIdI&satelliteLocation);
			int					GetStructureBuildHeight() const;


			virtual bool		IsOrbital() const override {return true;}
			virtual bool		HasFoundation() const override {return false;}

			virtual void		GetUpkeep(ResourceVector&v) const override {Super::GetUpkeep(v); v.level[Resource::Fuel] = GetConstructionCost()/4;}

			virtual Component::Turret*GetTurretType() const  = 0;

			virtual bool		IsHub() const {return false;}

			#ifndef CLIENT
				virtual void	CorrectTerrain(bool signalChange, float atTime, bool deep) override	{};
				virtual void	OnConstructionFinished(float atTime) override	{};
				/**
				Adjust to ground changes. Should be called once per round to account for 
				@return true, if changes were detected and applied, false otherwise
				*/
				bool			RefreshGroundSensor(float&atTime);
			#endif
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
						virtual bool		TestIntersection(const TVec3<>&b, const TVec3<>&d, float & distance) const override {return Super::TestIntersection(geometry,b,d,distance);}
						virtual void		VisitAllCommands(FCommandVisitor visitor) override {foreach (commands,cmd) visitor(*cmd);};
					#endif
				#endif
			};
		
		
		
		
			#ifdef HAS_WINDOW
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
		
		
		


			class Hub : public FloatingStructureT<Hub>, public MobileEntityProducer
			{
			protected:
				typedef FloatingStructureT<Hub>	Super;
				friend class Super;

				static const int		ProductionSpeed = 10;

				/**/					Hub(const OwnerParameter&owner, const Grid::LCellIdI&location, float time):Super(GetTurretType(),owner,location,time),MobileEntityProducer(ProductionSpeed) {}
				/**/					Hub():Super(GetTurretType()),MobileEntityProducer(ProductionSpeed){}	//global prototype

				static int2				AllocateProductionSlot();

				static void				RegisterProduction(const PShip&);
				static void				RegisterProduction(const PStructure&);

				#ifdef HAS_WINDOW
					static Buffer<StructureConstructionCommand>	structureConstructionCommands;
					static Buffer<SatelliteConstructionCommand>	satelliteConstructionCommands;

					struct MyCmd
					{
						static bool		CanBuild(const PStructure&what, const PWorldEntity&who);
						static void		Build(const PSatellite&what, const PWorldEntity&who);
					};
				#endif

			public:
				WSkyLinkStructure	connectedToLink;


				/**
				Updates @a connectedTo if necessary
				*/
				void				UpdateConnections();
				bool				GainGroundConnection(const PSkyLinkStructure&to);
				void				LoseGroundConnection();


				#ifndef CLIENT
					virtual void		Tick(ResourceVector&v) override;
				#endif

				//Station overrides:
				virtual bool			IsHub() const override {return true;}


				//BaseEntity overrides:
				virtual String			GetTypeName() const override {return "Sky Hub";}
				virtual ConstName		GetCTypeName() const override {return ConstName::Type_SkyHub;}


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
				virtual void			OnDestroy(float atTime) override {LoseGroundConnection();}
				virtual bool			AwaitsUserInteraction() const override {return MobileEntityProducer::AwaitsUserInteraction();}
				#ifdef HAS_WINDOW
					virtual void		VisitAllCommands(FCommandVisitor visitor) override;
					virtual void		OnSelect() override;
					virtual void		OnDeselect() override;
				#endif
				virtual const MobileEntityProducer*CastToProducer() const override {return this;}
				virtual void			PrototypeSetup() override;
				#ifndef CLIENT
					virtual void		OnConstructionFinished(float atTime) override;
				#endif
				virtual void			VisitAllConnected(const std::function<void(const PStructure&)>&visitor) const override;

			};
		
		
		
		
		
		
	}
	
	
	
	
	
}





#endif

#ifndef objectVehicleComponentsH
#define objectVehicleComponentsH

#ifdef HAS_WINDOW
	#include "visualCommands.h"
	#include "geometry.h"
//	#include "entity.h"
#endif

#include "../sphereGrid.h"
#include "resource.h"
#include "cursorMode.h"
#include "types.h"


namespace Objects
{
	static const float			vehicleScale = 0.01f * WorldScale;
	inline void NoCosts(const PWorldEntity&, int&mineralCostOut, int&timeCostOut){mineralCostOut = 0; timeCostOut = 0;}


	struct THindranceVector
	{
		int			road,
					air,
					water,
					other;
	};


	CONSTRUCT_ENUMERATION3(
		TurretTrajectory,
			InvalidTrajectory,
			ReachesTarget,
			MidFlightImpact
	);
	static const float g = 9.80665f;

	CONSTRUCT_ENUMERATION3(TrajectoryType, Artillery, Missile, VerticalBeam);


	#ifdef HAS_WINDOW
		inline bool					VehicleCanBeDirected()  {return (activeCursorMode == CursorMode::Idle);}
		inline bool					VehicleCanBeLeftClickDirected() {
			return (
			activeCursorMode == CursorMode::BuildConnectorChainMode 
			|| activeCursorMode == CursorMode::BuildRoadChainMode
			|| activeCursorMode == CursorMode::BuildBridgeMode
			|| activeCursorMode == CursorMode::BuildWallChainMode
			|| activeCursorMode == CursorMode::AttackMode 
			|| activeCursorMode == CursorMode::DigTunnelMode);}
	#endif


	/**
	Components of vehicles, towers, and satellites
	*/
	namespace Component
	{
		enum operation_t
		{
			Construct,
			Repair,
			TerrainAdd,
			TerrainRemove,

			OperationCount
		};

		class Base 
		{
		protected:
			/**/							Base():productionCosts(1),initiative(100)	{}

			struct Detail
			{
				static void					TraceArtilleryTrajectory(float velocity, const TVec3<>&source, const TVec3<>&dest, Buffer0<TVec4<> >&outSteps);
				static void					TraceMissileTrajectory(float range, const TVec3<>&source, const TVec3<>&dest, Buffer0<TVec4<> >&outSteps);
				static TurretTrajectory		TraceArtilleryTrajectory(float velocity, const Grid::WorldData&world, const Grid::LCellIdI&source, float zOffset, const Grid::LCellIdI&dest, Buffer0<TVec4<> >&outSteps, TVec3<>&impactAt, Grid::HCellIdI&impactCell);
				static TurretTrajectory		TraceMissileTrajectory(float range, const Grid::WorldData&world, const Grid::LCellIdI&source, float zOffset, const Grid::LCellIdI&dest, Buffer0<TVec4<> >&outSteps, TVec3<>&impactAt, Grid::HCellIdI&impactCell);
				static bool					ArtilleryCanHit(float flatRange, float horizontalDistance, float verticalDistance);
				static bool					MissileCanHit(float flatRange, const TVec3<>&originNormal, const TVec3<>&origin, const TVec3<>&destination);
			};
		public:
			#ifdef HAS_WINDOW
	//			typedef Objects::IconCommand	Command;
				Buffer0<IconCommand>			iconCommands;
				Buffer0<StructureConstructionCommand>			structureConstructionCommands;

				Array<std::pair<Geometry,DX11::InstructionBlock32> >	parts;
			#endif

			String							typeName,shortName;
			ConstName						constName;
			int								productionCosts,
											initiative;

			void							FinishCreation(bool scaleToVehicleSize=true);

			void							GetSubSystem(index_t geometryIndex, float angle, TMatrix4<>&outSystem)	const;

			#ifdef HAS_WINDOW
				typedef std::function<void(EntityCommand&cmd)>	FCommandVisitor;

				void						VisitAllCommands(FCommandVisitor visitor);
			#endif

			bool							TestIntersection(float zOffset, float x, float y, const TVec3<>&b, const TVec3<>&d, float & distance);

			#ifdef HAS_WINDOW
				void						DescribeType(BaseCommand::Label&label, bool compound);
			#endif

		};

	

		/**
		Description structure for vehicle, tower, and satellite turrets (although generally called rings when applied to satellites)
		*/
		class Turret : public Base
		{
		public:
			ConstName						constTowerName;

			float							healthMultiplier = 1.f;
			Resource						ammoType;
			int								operationSpeed[OperationCount],
											weight,					//turret weight
											damageLevel,
											attacksPerRound,
											ammoNeedPerShot,
											uplinkTraffic = 5,
											uplinkPriority = 950;	//before towers and storage structures

			#ifndef CONNECTOR_LESS
				ResourceVector				baseStorageCapacity;
			#endif
			bool							ignoresVehicleMotion = false,	//indicates that the vehicle can fire and move at 100% each in the same round
											dealsEMPDamage = false,
											detectsResources = false,
											continuousRotation = false,	//indicates that the turret should rotate continuously, regardless of server turret rotation commands
											requiresStationaryVehicle = false,	//cannot fire if vehicle moved last round
											isSatelliteRing = false,
											isDoomsday = false;	//server may reject their deployment

			float							explosionRadius = 0.f;
			TrajectoryType					trajectoryType;
			//int								fuelSupplyRadius = 0;	//!< Satellites only (in fact, SkyHub only): Maximum radius that friendly satellites will be supplied in



			//enum trajectory_t
			//{
			//	None,
			//	Arc,
			//	Linear,
			//	Missile,
			//}								trajectory;
			float							horizontalAttackRange = 0,	//geometric firing range
											scanningRange = 10,	//geometric scanning range
											groundScanningRange = -1.f,//>0 indicates that this turret can scan from orbit down to the ground (effective only if used by a satellite)
											groundScannerPenetration = 1,	//number of blocks the ground-scanner can penetrate into solid ground (effective only if used by a satellite)
											continuousRotationSpeed = 1;	//angles per second rotation speed, effective only, if continuousRotation is set true



			/**/							Turret():weight(1),damageLevel(0),attacksPerRound(0),ammoNeedPerShot(1),
												#ifdef NO_MUNITION
													ammoType(Resource::Minerals)
												#else
													ammoType(Resource::Munitions)
												#endif
												{
													memset(operationSpeed,0,sizeof(operationSpeed));
												}

			static void						TraceTrajectory(TrajectoryType, float flatRange, const TVec3<>&source, const TVec3<>&dest, Buffer0<TVec4<> >&outSteps);
			static TurretTrajectory			TraceTrajectory(TrajectoryType, float flatRange, const Grid::WorldData&world, const Grid::LCellIdI&source, float zOffset, const Grid::LCellIdI&dest, Buffer0<TVec4<> >&outSteps, TVec3<>&impactAt, Grid::HCellIdI&impactCell);
			static TurretTrajectory			TraceTrajectory(TrajectoryType type, float flatRange, const Grid::WorldData&world, const Grid::LCellIdI&source, float zOffset, const Grid::LCellIdI&dest, TVec3<>&impactAt)
			{
				Buffer0<TVec4<> > outSteps;
				Grid::HCellIdI impactCell;
				return TraceTrajectory(type, flatRange,world,source,zOffset, dest,outSteps,impactAt,impactCell);
			}

		

			static float					MaxRangeToVelocity(float range)
			{
				return sqrt(range*g);
			}


			static float					VelocityToMaxVerticalHeight(float v)
			{
				return sqr(v) / (2.f * g);

			}

			static float					MaxRangeAtHeight(float horizontalRange, float verticalDistance)
			{
				float q = sqr(horizontalRange) - 2.f * horizontalRange * verticalDistance;
				if (q < 0)
					return - 1.f;
				return sqrt(q);
			}

			static float					MaxHeightAtHDistance(float horizontalRange, float horizontalDistance)
			{
				return -0.5f / horizontalRange * sqr(horizontalDistance) + horizontalRange * 0.5f;
			}

			static float					MaxHeightSlopeAtHDistance(float horizontalRange, float horizontalDistance)
			{
				return -horizontalDistance / horizontalRange;
			}

			static float					VelocityToMaxRangeAtHeight(float v, float verticalDistance)
			{
				verticalDistance *= -1.f;
				float velocity2 = v*v;
				float t = 2.f * g * verticalDistance;
				float q0 = (t + velocity2) / (t + 2.f * velocity2);
				if (q0 < 0)
					return -1.f;
				//ASSERT_GREATER_OR_EQUAL__(q0,0);
				float q = sqrt(q0);
				if (q > 1.f || q < -1.f)
					return -1.f;
				float a = acos(q);

				float q2 = velocity2 * sqr(sin(a)) + 2.f * g * verticalDistance;
				ASSERT_GREATER_OR_EQUAL__(q2,0);
				q = sqrt(q2);

				return v * cos(a) / g * (v * sin(a) + q);
		
			}
			struct TAngles
			{
				int		numAngles = 0;
				float	idealAngle[2];
			};

			static TAngles					GetAngleForDistance(float v, const TVec3<>&origin, const TVec3<>&dest);
			static TAngles					GetAngleForDistance(float v, const Grid::LCellIdI&origin, float zOffset, const TVec3<>&dest);
			static TAngles					GetAngleForDistance(float v, const TVec3<>&origin, const Grid::LCellIdI&dest);
			static TAngles					GetAngleForDistance(float v, const Grid::LCellIdI&origin, float zOffset, const Grid::LCellIdI&dest);
			static TAngles					GetAngleForDistance(float v, const TVec3<>&originDir, float originH, const TVec3<>&destDir, float destH);
			static TAngles					GetAngleForDistance(float v, float horizontalDistance, float verticalDistance)
			{
				//verticalDistance = -verticalDistance;
				//v *= (1.f + TypeInfo<float>::error);
				float velocity2 = sqr(v);
				TAngles rs;
				float q0 = sqr(velocity2) - g  * (g * sqr(horizontalDistance) + 2.f * verticalDistance * velocity2 );
				if (q0 < 0)
					return rs;

				float q = sqrt(q0);
				float a0 = atan((velocity2 - q) / (g * horizontalDistance));
				float a1 = atan((velocity2 + q) / (g * horizontalDistance));

				if (a0 > 0)
					rs.idealAngle[rs.numAngles++] = a0;
				if (a1 > 0 && (rs.numAngles == 0 || !similar(a0,a1)))
					rs.idealAngle[rs.numAngles++] = a1;
				return rs;
			}

			//static bool					CanHit(float flatRange, const TVec3<>&origin, const TVec3<>&dest);
			static bool						CanHit(TrajectoryType t, float flatRange, const Grid::LCellIdI&origin, float zOffset, const TVec3<>&dest);
			//static bool					CanHit(float flatRange, const TVec3<>&origin, const Grid::LCellIdI&dest);
			static bool						CanHit(TrajectoryType t, float flatRange, const Grid::LCellIdI&origin, float zOffset, const Grid::LCellIdI&dest);
			static bool						CanHit(TrajectoryType t, float flatRange, const TVec3<>&originDir, float originH, const TVec3<>&destDir, float destH);



			void							BuildBlastSystem(const Grid::LCellIdI&location, TMatrix4<>&system) const;
			void							BuildBlastSystem(const TVec3<>&location, TMatrix4<>&system) const;

			bool							CanAttack() const {return attacksPerRound > 0;}
			bool							CanDo(operation_t op) const {return operationSpeed[op] > 0;}
			bool							CanRepair() const {return CanDo(Repair);}
			bool							CanConstruct() const {return CanDo(Construct);}
			//bool							IsStandardOffensive() const
			//{
			//	return CanAttack() && 
			//			#ifdef NO_MUNITION
			//				ammoNeedPerShot == 0
			//			#else
			//				ammoType == Resource::Munition
			//			#endif
			//			;
			//}

			#ifdef HAS_WINDOW
				static float				CompileTrajectory(const Buffer0<TVec4<> >&v, bool schematic, Geometry&outGeo, float widthScale=1.f);
				void						DescribeType(BaseCommand::Label&label, bool compound, const String&parentType, const OwnerParameter&owner);
			#endif

			/**
			Checks whether or not the local turret could ever possibly consume resources
			*/
			bool							ConsumesResources(ResourceFlagSet&res) const;
		};




		class VehicleBase : public Base
		{
		public:
	
			int								maxClimbUp = 1,
											minClimbDown = -1;
			float							height = 1.f;

			#ifndef CONNECTOR_LESS
				int							storageMultiplier = 1;
			#endif

			int								rangeModifier = 0,
											operationSpeedMultiplier = 1,
											baseMovementSpeed,		//ideal movement speed (if not encumbered)
											carryingCapacity,		//turret weight tolerance before movement speed is reduced
											baseHealth;			//!< Base health level
			bool							ignoresVehicleAttacks;
			THindranceVector				hindrance;

			//warning: none is broken in pathfinding
			CONSTRUCT_ENUMERATION3(Requirement, None, AnyBlock ,Water);

			Requirement						requirements;
			bool							moveOnWater;
	
			/**/							VehicleBase():baseHealth(2),ignoresVehicleAttacks(false)
			{
				hindrance.road = 1;
				hindrance.air = 2;
				hindrance.water = 5;
				hindrance.other = 4;
			}

			bool							CanClimb(int inclination) const { if (inclination < 0) return inclination >= minClimbDown; return inclination <= maxClimbUp;}

			/**
			Global flag that states whether or not any base should be able to stand on unknown blocks.
			Usually this is fine, but for AI-units this can prove fatal.
			Make sure this variable is only ever temporarily false
			*/
			static bool						canStandOnUnknown	/*=true*/;

			bool							CanStandOn(const Grid::WorldData&world, const Grid::LCellIdI&, bool ignoreWater, bool doFloat);

			#ifdef HAS_WINDOW
				void						DescribeType(BaseCommand::Label&label, bool compound);
			#endif

	
		};




	
		namespace Hoverbase
		{
			void			Configure(VehicleBase&);
		}
		namespace Heavybase
		{
			void			Configure(VehicleBase&);
		}
		namespace Catbase
		{
			void			Configure(VehicleBase&);
		}
		extern VehicleBase	hoverbase, 
							heavybase,
							catbase;



		namespace Terraformer // : public Vehicle
		{
			#ifdef HAS_WINDOW
			namespace Cmd
			{

		
				bool	BuildFoundationJob(const Vehicle&v);


				void	ToggleDigMode(const PWorldEntity&);

				void	DigDown(const PWorldEntity&);
				//bool	CanDigDown(const PWorldEntity&);


				template <ConstName Command>
					void	IssueCommand(const PWorldEntity&v)
					{
						PMobileEntity vehicle = To<MobileEntity>::DynamicCast(v);
						if (!vehicle)
							return;
						//if (vehicle->GetLastPointOfOrigin().height+1 >= Grid::CellStack::TopHeight)	//not the place of the client to decide
						//{
						//	LogEvent(EventClass::Preset::commandRejected
						//	return;
						//}
						SignalNewJob(vehicle, Command,ConstName::Empty);					
					}

				void	BuildRoad(const PWorldEntity&);


				bool	NotBuilding(const PWorldEntity&e);

				void	ClearAbove(const PWorldEntity&);
				bool	ClearAboveIsActive(const PWorldEntity&);

				void	BuildBlock(const PWorldEntity&v);
				bool	BuildBlockIsActive(const PWorldEntity&);

				void	RemoveBlock(const PWorldEntity&v);
				bool	RemoveBlockIsActive(const PWorldEntity&e);

				void	CommitOperation(const PWorldEntity&);
				bool	CommitIsPossibleC(const PWorldEntity&, bool);
				BaseCommand::Flags CommitCheck(const PWorldEntity&, bool);
		

	//				bool	AbortIsPossible(const PWorldEntity&);

				bool	ConstructionIsPossible(const PWorldEntity&e);
				void	Construct(const PStructure&p, const PWorldEntity&);
			};
			#endif



			void		Configure(Turret&);
			//void DefineTurret(Base&component);
		};


	
		extern Turret		terraformer,mechanic,
							artillery,launcher,emp,armored,
							munitionTransporter,
							radar,resourceScanner;
		namespace Orbital
		{
			extern Turret	protonCannon,
							hub,
							starPort,
							ragnarok,
							engineer;
		}

	
		namespace Artillery
		{
			void		Configure(Turret&);
		};

		namespace Launcher
		{
			void		Configure(Turret&);
		};
	
		namespace EMP
		{
			void		Configure(Turret&);
		};

		namespace Orbital
		{
			/**
			ProtonCannons are used by satellites, and (re)use the missile trajectory
			*/
			namespace Guardian
			{
				void		Configure(Turret&);
			};

			/**
			Used only by the SkyHub satellite. Provides its logic
			*/
			namespace Hub
			{
				void		Configure(Turret&);
			};

			namespace Ragnarok
			{
				void		Configure(Turret&);
			};

			namespace StarPort
			{
				void		Configure(Turret&);
			};

			namespace Engineer
			{
				void		Configure(Turret&);
			}
		}

		namespace Armored
		{
			void		Configure(Turret&);
		}


		namespace Mechanic
		{
			void		Configure(Turret&);
		};

	
		namespace MunitionTransporter
		{
			void		Configure(Turret&);
		};

	
		namespace Radar
		{
			void		Configure(Turret&);
		};

	
		namespace ResourceScanner
		{
			void		Configure(Turret&);
		};




	








	}

	inline float3 ToWorld(const TVec4<>&trajectoryPoint)
	{
		return trajectoryPoint.xyz * trajectoryPoint.w;
	}

}



#endif

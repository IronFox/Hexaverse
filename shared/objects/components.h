#ifndef objectVehicleComponentsH
#define objectVehicleComponentsH

#ifdef HAS_WINDOW
	#include "visualCommands.h"
	#include "geometry.h"
	#include <al/openal.h>
//	#include "entity.h"
#endif

#include "../sphereGrid.h"
#include "resource.h"
#include "cursorMode.h"
#include "types.h"
#include "componentAttribute.h"
#include "luaAttribute.h"


namespace Objects
{
	static const constexpr float			vehicleScale = 0.01f * Grid::UnitScale * WorldScale;
	inline void NoCosts(int&mineralCostOut, Resource&resOut, int&timeCostOut){mineralCostOut = 0; resOut=Resource::Minerals; timeCostOut = 0;}


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

	inline bool					CursorModeExpectsLeftClick(CursorMode activeCursorMode)
	{
		return (
		activeCursorMode == CursorMode::BuildConnectorChainMode 
		|| activeCursorMode == CursorMode::BuildRoadChainMode
		|| activeCursorMode == CursorMode::BuildBridgeMode
		|| activeCursorMode == CursorMode::BuildWallChainMode
		|| activeCursorMode == CursorMode::AttackMode 
		|| activeCursorMode == CursorMode::DigTunnelMode
		|| activeCursorMode == CursorMode::PlaceStructureMode
		);
	}

	#ifdef HAS_WINDOW
		inline bool					VehicleCanBeDirected()  {return (activeCursorMode == CursorMode::Idle);}
		inline bool					CursorModeExpectsLeftClick() {return CursorModeExpectsLeftClick(activeCursorMode);}
	#endif


	/**
	Source or destination point of a trajectory.
	Can be constructed either from 3d coordinates, or cell IDs
	*/
	struct TrajectoryEndPoint
	{
		Grid::LCellIdI	cellLocation;
		float3			worldLocation;
		float			radius=0;	//!< Distance from planet center

		/**/			TrajectoryEndPoint()	{}
		/**/			TrajectoryEndPoint(const TVec3<>&worldLocation);
		/**/			TrajectoryEndPoint(const Grid::LCellIdI&, float zOffset);
		explicit		TrajectoryEndPoint(const Grid::LCellIdI&);

		String			ToString() const;
	};

	/**
	Components of vehicles, towers, and satellites
	*/
	namespace Component
	{





		enum operation_t
		{
			Construct,
			Nuclear,
			Repair,
			TerrainAdd,
			TerrainRemove,

			OperationCount
		};


		const char*			OperationToString(operation_t);
		Lua::Reference		ResearchStateOf(const OwnerParameter&);



		template <typename T, int EmptyDefault=0>
			class LuaAttribute : public Objects::LuaAttribute<T>
			{
			public:
				T						operator()(const OwnerParameter&p) const
				{
					if (this->IsEmpty())
						return (T)EmptyDefault;
					return this->GetValue(ResearchStateOf(p));
				}

				//String		Describe(const OwnerParameter&p) const
				//{
				//	return std::move(Objects::LuaAttribute<T>::Describe(ResearchStateOf(p)));
				//}

			};

		class Base 
		{
		protected:
			/**/							Base():initiative(100)	{}

			struct Detail
			{
				static void					TraceAnimationArtilleryTrajectory(float velocity,  float attackAngle, const TrajectoryEndPoint&attackerPosition, const TrajectoryEndPoint&explosionCenter,Buffer0<TVec4<> >&outSteps);
				static void					TraceAnimationMissileTrajectory(float range, const TrajectoryEndPoint&source, const TrajectoryEndPoint&dest, Buffer0<TVec4<> >&outSteps);
				static TurretTrajectory		TraceArtilleryTrajectory(float velocity, const Grid::WorldData&decisionWorld, const Grid::WorldData&actualWorld, const TrajectoryEndPoint&source,  const TrajectoryEndPoint&dest, Buffer0<TVec4<> >&outSteps, TVec3<>&impactAt, Grid::HCellIdI&impactCell, float&outAngle);
				static TurretTrajectory		TraceMissileTrajectory(float range, const Grid::WorldData&actualWorld, const TrajectoryEndPoint&source, const TrajectoryEndPoint&dest, Buffer0<TVec4<> >&outSteps, TVec3<>&impactAt, Grid::HCellIdI&impactCell);
				static bool					ArtilleryCanHit(float flatRange, float horizontalDistance, float verticalDistance, float&outAngle);
				static bool					ArtilleryCanHit(float flatRange, float horizontalDistance, float verticalDistance);
				static bool					MissileCanHit(float flatRange, const TrajectoryEndPoint&origin, const TrajectoryEndPoint&destination);
			};


		public:
			#ifdef HAS_WINDOW
				CommandContainer			commands;

				Array<std::pair<Geometry,DX11::InstructionBlock32> >	parts;
			#endif

			
			String							typeName,shortName;
			ConstName						constName;
			int								initiative;
			LuaAttribute<int>				productionCosts /*=1*/;
			LuaAttribute<bool,true>			isUnlocked;
											


			void							FinishCreation(bool scaleToVehicleSize=true);

			void							GetSubSystem(index_t geometryIndex, float angle, TMatrix4<>&outSystem)	const;
			virtual bool					IsTurret() const {return false;}
			virtual bool					IsVehicleBase() const {return false;}

			#ifdef HAS_WINDOW
				//typedef std::function<void(EntityCommand&cmd)>	FCommandVisitor;

				void						VisitAllCommands(FCommandVisitor visitor, bool force);
			#endif

			bool							TestIntersection(float zOffset, float x, float y, const TVec3<>&b, const TVec3<>&d, float & distance);

			#ifdef HAS_WINDOW
				void						DescribeType(BaseCommand::Label&label,bool compound, const OwnerParameter&);
			#endif

		};


		typedef LuaAttribute<float>		FFunc;
		typedef LuaAttribute<int>		IFunc;


	

		/**
		Description structure for vehicle, tower, and satellite turrets (although generally called rings when applied to satellites)
		*/
		class Turret : public Base
		{
			typedef Base					Super;
		public:
			enum class MountType
			{
				Vehicle,
				TowerOnly,
				Satellite,
			};


			ConstName						constTowerName;

			IFunc							healthMultiplier;
			Resource						ammoType;
			FixedArray<IFunc,OperationCount>operationSpeed;
			FFunc							weight	/*=1*/,					//turret weight
											damageLevel;
			IFunc							attacksPerRound,
											ammoNeedPerShot;

			#ifndef ONE_COMPOUND_PER_FACTION
				ResourceVector				baseStorageCapacity;
			#endif
			bool							ignoresVehicleMotion = false,	//indicates that the vehicle can fire and move at 100% each in the same round
											dealsEMPDamage = false,
											detectsResources = false,
											continuousRotation = false,	//indicates that the turret should rotate continuously, regardless of server turret rotation commands
											requiresStationaryVehicle = false,	//cannot fire if vehicle moved last round
											isDoomsday = false;	//server may reject their deployment

			MountType						mountType = MountType::Vehicle;
			FFunc							explosionRadius;
			TrajectoryType					trajectoryType;
			//int								fuelSupplyRadius = 0;	//!< Satellites only (in fact, SkyHub only): Maximum radius that friendly satellites will be supplied in



			//enum trajectory_t
			//{
			//	None,
			//	Arc,
			//	Linear,
			//	Missile,
			//}								trajectory;
			FFunc							horizontalAttackRange,	//geometric firing range
											scanningRange /*= 10*/,	//geometric scanning range
											groundScanningRange /*= -1.f*/,//>0 indicates that this turret can scan from orbit down to the ground (effective only if used by a satellite)
											groundScannerPenetration /*= 1*/;	//number of blocks the ground-scanner can penetrate into solid ground (effective only if used by a satellite)
			float							continuousRotationSpeed = 1;	//angles per second rotation speed, effective only, if continuousRotation is set true



			/**/							Turret():
												#ifdef NO_MUNITION
													ammoType(Resource::Minerals)
												#else
													ammoType(Resource::Munitions)
												#endif
												{
													//baseOperationSpeed.Fill(0);
												}

			static int						GetGroundHeight(const Grid::LCellIdI&location);
			//static void						TraceTrajectory(TrajectoryType, float flatRange, const TVec3<>&source, const TVec3<>&dest, Buffer0<TVec4<> >&outSteps);
			static TurretTrajectory			TraceTrajectory(TrajectoryType, float flatRange, const Grid::WorldData&visibleWorld, const Grid::WorldData&actualWorld, const TrajectoryEndPoint&source, const TrajectoryEndPoint&dest, Buffer0<TVec4<> >&outSteps, TVec3<>&impactAt, Grid::HCellIdI&impactCell, float&outAngle);
			static TurretTrajectory			TraceTrajectory(TrajectoryType type, float flatRange, const Grid::WorldData&visibleWorld, const Grid::WorldData&actualWorld, const TrajectoryEndPoint&source, const TrajectoryEndPoint&dest, TVec3<>&impactAt)
			{
				Buffer0<TVec4<> > outSteps;
				Grid::HCellIdI impactCell;
				float angle;
				return TraceTrajectory(type, flatRange,visibleWorld, actualWorld,source,dest,outSteps,impactAt,impactCell,angle);
			}
			virtual bool					IsTurret() const override {return true;}
			static void						TraceAnimationTrajectory(TrajectoryType, float flatRange, float attackAngle, const TrajectoryEndPoint&attackerPosition, const TrajectoryEndPoint&explosionCenter,Buffer0<TVec4<> >&outTrajectory);


			float							GetOperationSpeed(operation_t op, const OwnerParameter&owner) const;
			int								GetDamageLevel(const OwnerParameter&owner) const;
			float							GetExplosionRadius(const OwnerParameter&owner) const;
			int								GetAttacksPerRound(const OwnerParameter&owner) const;
			int								GetAmmoNeedPerShot(const OwnerParameter&owner) const;
			float							GetHorizontalAttackRange(const OwnerParameter&owner, const FFunc&bonus=FFunc()) const;
			float							GetScanningRange(const OwnerParameter&owner, int bonus=0) const;
			float							GetGroundScanningRange(const OwnerParameter&owner) const;

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

			static TAngles					GetAngleForDistance(float v, const TrajectoryEndPoint&origin, const TrajectoryEndPoint&dest);
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

			static bool						CanHit(TrajectoryType t, float flatRange, const TrajectoryEndPoint&origin, const TrajectoryEndPoint&dest, float&outAngle);
			static bool						CanHit(TrajectoryType t, float flatRange, const TrajectoryEndPoint&origin, const TrajectoryEndPoint&dest);



			void							BuildBlastSystem(const Grid::LCellIdI&location, float radius, TMatrix4<>&system) const;
			void							BuildBlastSystem(const TVec3<>&location, float radius, TMatrix4<>&system) const;

			bool							CanAttack() const {return attacksPerRound.IsSet();}
			bool							CanDo(operation_t op) const {return operationSpeed[op].IsSet();}
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
				enum geometryType_t
				{
					Schematic,
					Animation,
					SmokeTrail
				};
				static float				CompileTrajectory(const Buffer0<TVec4<> >&v, geometryType_t type, Geometry&outGeo, float widthScale=1.f);
				void						DescribeType(BaseCommand::Label&label, bool compound, const String&parentType, const OwnerParameter&owner);
			#endif

			/**
			Checks whether or not the local turret could ever possibly consume resources
			*/
			bool							ConsumesResources(ResourceFlagSet&res, const OwnerParameter&) const;
			void							FinishCreation(bool scaleToVehicleSize=true)
			{
				//float fc = (Grid::UnitScale-1.f)*0.5f + 1.f;
				Super::FinishCreation(scaleToVehicleSize);
			}

		};




		class VehicleBase : public Base
		{
		public:
	
			int								maxClimbUp = 1,
											minClimbDown = -1;
			float							height = 1.f;

			#ifndef ONE_COMPOUND_PER_FACTION
				int							storageMultiplier = 1;
			#endif

			FFunc							turretRangeModifier /*= 0*/,
											operationSpeedMultiplier /*= 1*/,
											movementSpeed,		//ideal movement speed (if not encumbered)
											carryingCapacity;		//turret weight tolerance before movement speed is reduced
			IFunc							health/*=2*/;			//!< Base health level
			bool							ignoresVehicleAttacks;
			THindranceVector				hindrance;

			/**
			Mobile job acceleration per operation type.
			These are exponents applied to certain jobs. A value of 0 leaves the original speed unchanged,
			such that it will consume the entire round.
			Increased values each halve the percentage of a round required to advance the respective job by one.
			A value of one will allow the vehicle to advance this job twice per round.
			With a maximum action value of 128, using an exponent of 6 would allow the vehicle to continue a job 128 times each round.
			A value of 7 or more will (theoretically) allow an infinite number of executions per round.
			*/
			FixedArray<BYTE,OperationCount>	acceleration;

			#ifdef HAS_WINDOW
				OpenAL::Wave				ambientSound;	//sound used for ambience (25% volume, normal pitch), or motion (100% volume, increased pitch)
			#endif

			//warning: none is broken in pathfinding
			CONSTRUCT_ENUMERATION3(Requirement, None, AnyBlock ,Water);

			Requirement						requirements;
			bool							moveOnWater;
	
			/**/							VehicleBase():ignoresVehicleAttacks(false)
			{
				hindrance.road = 1;
				hindrance.air = 2;
				hindrance.water = 5;
				hindrance.other = 4;
				acceleration.Fill(0);
			}



			float							GetBaseMovementSpeed(const OwnerParameter&owner) const;
			float							GetCarryingCapacity(const OwnerParameter&owner) const;
			float							GetBaseHealth(const OwnerParameter&owner) const;


			bool							CanClimb(int inclination) const { if (inclination < 0) return inclination >= minClimbDown; return inclination <= maxClimbUp;}

			virtual bool					IsVehicleBase() const override {return true;}

			/**
			Global flag that states whether or not any base should be able to stand on unknown blocks.
			Usually this is fine, but for AI-units this can prove fatal.
			Make sure this variable is only ever temporarily false
			*/
			static bool						canStandOnUnknown	/*=true*/;

			bool							CanStandOn(const Grid::WorldData&world, const Grid::LCellIdI&, bool ignoreWater, bool doFloat);

			#ifdef HAS_WINDOW
				void						DescribeType(BaseCommand::Label&label, bool compound, const OwnerParameter&);
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



		namespace Terraformer
		{
			extern Buffer<PStructure> canBuild;

			#ifdef HAS_WINDOW
			namespace Cmd
			{

		
				bool	BuildFoundationJob(const Vehicle&v);


				//void	ToggleDigMode();

				void	DigDown();
				//bool	CanDigDown();


				template <ConstName Command>
					void	IssueCommand()
					{
						if (!selectedMobileEntity)
							return;
						ApplicationInterface::SignalNewJob(selectedMobileEntity, Command,ConstName::Empty);					
					}

				void	BuildRoad();
				bool	NotBuilding();

				//void	ClearAbove();
				//bool	ClearAboveIsActive();

				void	BuildBlock();
				//bool	BuildBlockIsActive();

				void	RemoveBlock();
				//bool	RemoveBlockIsActive();

				void	CommitOperation();
				bool	CommitIsPossibleC(bool);
				BaseCommand::Flags CommitCheck(bool);
		
				bool	ConstructionIsPossible();
				void	Construct(const PStructure&p);
			};
			#endif



			void		Configure(Turret&);
		};

		namespace Bomb
		{
			#ifdef HAS_WINDOW
			namespace Cmd
			{
				template <ConstName DetonationSize>
					void	IssueCommand()
					{
						PMobileEntity vehicle = selectedMobileEntity;
						if (!vehicle)
							return;
						ApplicationInterface::SignalNewJob(vehicle, ConstName::Job_SelfDetonate,DetonationSize);					
					}

			};
			#endif


			void		Configure(Turret&);


			float		GetRadius(bool doomsday, const OwnerParameter&);
			int			GetDamageLevel(bool doomsday, const OwnerParameter&);
		};

	
		extern Turret	terraformer,mechanic,
						artillery,launcher,emp,armored,
						bomb,
						munitionTransporter,
						radar,resourceScanner
						#ifndef NO_SATELLITES
							,protonCannon,skyHubTurret,ragnarok,starPort
						#endif
						;

	
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

		#ifndef NO_SATELLITES
			/**
			ProtonCannons are used by satellites, and (re)use the missile trajectory
			*/
			namespace SkyGuardian
			{
				void		Configure(Turret&);
			};

			/**
			Used only by the SkyHub satellite. Provides its logic
			*/
			namespace SkyHubTurret
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
		#endif

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

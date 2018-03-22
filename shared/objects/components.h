#pragma once

#include "../grid/worldData.h"
#include "resource.h"
#include "cursorMode.h"
#include "types.h"
#include "componentAttribute.h"
#include "luaAttribute.h"
#include "description.h"
#include "entityCommand.h"
#include "clientAttachment.h"
#include "geometry.h"
#ifdef HAS_WINDOW
	#include <al/openal.h>
#endif

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
		TraceResult,
			InvalidTrajectory,
			ReachesTarget,
			MidFlightImpact
	);

	struct ImpactDetails
	{
		TraceResult			result;
		M::TVec3<>			impactAt;	//!< World location of any detected impact. Valid only if result!=TrajectoryResult::InvalidTrajectory
		Grid::HCellIdI		impactCell;	//!< High resolution cell location of any detected impact. Valid only if result!=TrajectoryResult::InvalidTrajectory
		float				artilleryAngle=0;	//!< Used artillery angle for this impact. Valid only if result!=TrajectoryResult::InvalidTrajectory, and used weapon is artillery
		/**
		Geometric distance to the given destination.
		Valid only if result != TrajectoryResult::InvalidTrajectory
		*/
		float				distanceToTarget=0;

		friend String		ToString(const ImpactDetails&det)
		{
			if (det.result == TraceResult::InvalidTrajectory)
				return "No Impact";
			return String(det.result.ToString())+": @"+ToString(det.impactAt)+"/"+ToString(det.impactCell)+", dist="+String(det.distanceToTarget);
		}

		operator TraceResult() const {return result;}
		bool				operator==(const TraceResult&rs) const {return result == rs;}
		bool				operator!=(const TraceResult&rs) const {return result != rs;}

		bool				operator!=(const ImpactDetails&other) const	{return !operator==(other);}
		bool				operator==(const ImpactDetails&other) const
		{
			return result == other.result
					&& Vec::equal(impactAt,other.impactAt)
					&& artilleryAngle == other.artilleryAngle
					&& distanceToTarget == other.distanceToTarget;
		}

		void				AssertEquality(const ImpactDetails&other) const
		{
			ASSERT2__(result == other.result,result.ToString(),other.result.ToString());
			ASSERT2__(Vec::equal(impactAt,other.impactAt),impactAt,other.impactAt);
			ASSERT_EQUAL__(artilleryAngle,other.artilleryAngle);
			ASSERT_EQUAL__(distanceToTarget,other.distanceToTarget);
		}
	};



	static const float g = 9.80665f;

	CONSTRUCT_ENUMERATION4(TrajectoryType, Artillery, Missile, VerticalBeam,VerticalBeamUp);

	/**
	Determines whether the given trajectory is a vertical beam trajectory.
	If true, it fires only up and down, and cannot affect neighboring stacks except via explosion radius
	*/
	inline bool IsBeam(TrajectoryType t)	{return t == TrajectoryType::VerticalBeam || t == TrajectoryType::VerticalBeamUp;}

	inline bool					CursorModeExpectsLeftClick(CursorMode activeCursorMode)
	{
		switch (activeCursorMode)
		{
			case CursorMode::BuildConnectorChainMode:
			case CursorMode::BuildRoadChainMode:
			case CursorMode::BuildBridgeMode:
			case CursorMode::BuildHighWallChainMode:
			case CursorMode::BuildLowWallChainMode:
			case CursorMode::AttackMode:
			case CursorMode::DigTunnelMode:
			case CursorMode::PlaceStructureMode:
				return true;
		}
		return false;
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
		/**/			TrajectoryEndPoint(const M::TVec3<>&worldLocation);
		/**/			TrajectoryEndPoint(const Grid::LCellIdI&, float zOffset);
		explicit		TrajectoryEndPoint(const Grid::LCellIdI&);

		friend String	ToString(const TrajectoryEndPoint&);
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
		Lua::Reference		ResearchStateOf(const PFaction&);
		Lua::Reference		ResearchStateOf(const OwnerParameter&);



		template <typename T, typename Filter, int EmptyDefault=0>
			class LuaAttribute : public Objects::LuaAttribute<T,Filter>
			{
			public:
				T						operator()(const OwnerParameter&p) const
				{
					if (this->IsEmpty())
						return (T)EmptyDefault;
					return this->GetValue(ResearchStateOf(p));
				}
				T						operator()(const Lua::Reference&researchState) const
				{
					if (this->IsEmpty())
						return (T)EmptyDefault;
					return this->GetValue(researchState);
				}
			};

		class Base 
		{
		protected:
			/**/							Base():initiative(100)	{}

			struct Detail
			{
				static void					TraceAnimationArtilleryTrajectory(float velocity,  float attackAngle, const TrajectoryEndPoint&attackerPosition, const TrajectoryEndPoint&explosionCenter,Ctr::Buffer0<M::TVec4<> >&outSteps);
				static void					TraceAnimationMissileTrajectory(float range, const TrajectoryEndPoint&source, const TrajectoryEndPoint&dest, Ctr::Buffer0<M::TVec4<> >&outSteps);
				static ImpactDetails		TraceArtilleryTrajectory(float velocity, const World::Data&decisionWorld, const World::Data&actualWorld, bool traceInDecisionWorld, const TrajectoryEndPoint&source,  const TrajectoryEndPoint&dest, Ctr::Buffer0<M::TVec4<> >&outSteps);
				static ImpactDetails		TraceMissileTrajectory(float range, const World::Data&actualWorld, const TrajectoryEndPoint&source, const TrajectoryEndPoint&dest, Ctr::Buffer0<M::TVec4<> >&outSteps);
				static bool					ArtilleryCanHit(float flatRange, float horizontalDistance, float verticalDistance, float&outAngle);
				static bool					ArtilleryCanHit(float flatRange, float horizontalDistance, float verticalDistance);
				static bool					MissileCanHit(float flatRange, const TrajectoryEndPoint&origin, const TrajectoryEndPoint&destination);
			};


		public:

			/**
			Custom attachment to the local component. Not used by server.
			*/
			PClientAttachment				attachment;
			CommandContainer				commands;
			#ifdef HAS_WINDOW
				Ctr::Array<Geometry>		parts;
				Engine::OpenAL::Wave		ambientSound;	//sound used for ambience (25% volume, normal pitch), or motion (100% volume, increased pitch)
			#endif


			
			String							typeName,shortName;
			ConstName						constName;
			int								initiative;
			LuaAttribute<int,NoFilter>			productionCosts /*=1*/;
			LuaAttribute<bool,NoFilter,true>	isUnlocked;
											



			void							GetSubSystem(index_t geometryIndex, float angle, M::TMatrix4<>&outSystem)	const;
			virtual bool					IsTurret() const {return false;}
			virtual bool					IsVehicleBase() const {return false;}

			

			bool							TestIntersection(float zOffset, float x, float y, const M::TVec3<>&b, const M::TVec3<>&d, float & distance);

			void							DescribeType(Description&,bool compound, const OwnerParameter&);
		};


		typedef LuaAttribute<float,NoFilter>		FFunc;
		typedef LuaAttribute<int,NoFilter>			IFunc;
		typedef LuaAttribute<float,UnitScaleFilter>	FScaledFunc;


	

		/**
		Description structure for vehicle, tower, and satellite turrets (although generally called rings when applied to satellites)
		*/
		class Turret : public Base
		{
			typedef Base					Super;
		public:
			CONSTRUCT_ENUMERATION4(MountType,
				Vehicle,
				TowerOnly,
				Satellite,
				Plane
			);


			IFunc							healthMultiplier;
			Resource						ammoType;
			Ctr::FixedArray<IFunc,OperationCount>operationSpeed;
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
											isScanner = false,	//indicates a turret explicitly designed to scan for hostile units or terrain changes
											continuousRotation = false,	//indicates that the turret should rotate continuously, regardless of server turret rotation commands
											requiresStationaryVehicle = false,	//cannot fire if vehicle moved last round
											isDoomsday = false;	//server may reject their deployment

			MountType						mountType = MountType::Vehicle;
			FScaledFunc						explosionRadius;
			TrajectoryType					trajectoryType;
			//int								fuelSupplyRadius = 0;	//!< Satellites only (in fact, SkyHub only): Maximum radius that friendly satellites will be supplied in



			//enum trajectory_t
			//{
			//	None,
			//	Arc,
			//	Linear,
			//	Missile,
			//}								trajectory;
			FScaledFunc						horizontalAttackRange,	//geometric firing range
											scanningRange /*= 10*/,	//geometric scanning range
											subOrbitalScanningRange /*=-1*/,	//>0 indicates that this turret features an additional scanning bubble in sub orbit
											groundScanningRange /*= -1.f*/;//>0 indicates that this turret can scan from orbit down to the ground (effective only if used by a satellite)
			FFunc							groundScannerPenetration /*= 1*/;	//number of blocks the ground-scanner can penetrate into solid ground (effective only if used by a satellite)
			float							continuousRotationSpeed = 1;	//angles per second rotation speed, effective only, if continuousRotation is set true


			static Timer::Time				timeSpentTracing;
			static count_t					traceCount;

			/**/							Turret():
												#ifdef NO_MUNITION
													ammoType(Resource::Minerals)
												#else
													ammoType(Resource::Munitions)
												#endif
												{
													//baseOperationSpeed.Fill(0);
												}

			static int						GetGroundHeight(const World::Data&world, const Grid::LCellIdI&location);
			//static void						TraceTrajectory(TrajectoryType, float flatRange, const M::TVec3<>&source, const M::TVec3<>&dest, Buffer0<M::TVec4<> >&outSteps);
			static ImpactDetails			TraceTrajectory(TrajectoryType, float flatRange, const World::Data&visibleWorld, const World::Data&actualWorld, bool traceInActualWorld, const TrajectoryEndPoint&source, const TrajectoryEndPoint&dest, Ctr::Buffer0<M::TVec4<> >&outSteps);
			static ImpactDetails			TraceTrajectory(TrajectoryType type, float flatRange, const World::Data&visibleWorld, const World::Data&actualWorld, bool traceInActualWorld, const TrajectoryEndPoint&source, const TrajectoryEndPoint&dest)
			{
				Ctr::Buffer0<M::TVec4<> > outSteps;
				return TraceTrajectory(type, flatRange,visibleWorld, actualWorld,traceInActualWorld,source,dest,outSteps);
			}
			virtual bool					IsTurret() const override {return true;}
			static void						TraceAnimationTrajectory(TrajectoryType, float flatRange, float attackAngle, const TrajectoryEndPoint&attackerPosition, const TrajectoryEndPoint&explosionCenter,Ctr::Buffer0<M::TVec4<> >&outTrajectory);


			float							GetOperationSpeed(operation_t op, const OwnerParameter&owner) const;
			int								GetDamageLevel(const PFaction&owner) const;
			float							GetExplosionRadius(const PFaction&owner) const;
			int								GetAttacksPerRound(const PFaction&owner) const;
			int								GetAmmoNeedPerShot(const PFaction&owner) const;
			float							GetHorizontalAttackRange(const PFaction&owner, const FFunc&bonus=FFunc()) const;
			float							GetScanningRange(const PFaction&owner, int bonus=0) const;
			float							GetGroundScanningRange(const PFaction&owner) const;

			static float					MaxRangeToVelocity(float range)
			{
				return sqrt(range*g);
			}


			static float					VelocityToMaxVerticalHeight(float v)
			{
				return M::Sqr(v) / (2.f * g);

			}

			static float					MaxRangeAtHeight(float horizontalRange, float verticalDistance)
			{
				float q = M::Sqr(horizontalRange) - 2.f * horizontalRange * verticalDistance;
				if (q < 0)
					return - 1.f;
				return sqrt(q);
			}

			static float					MaxHeightAtHDistance(float horizontalRange, float horizontalDistance)
			{
				return -0.5f / horizontalRange * M::Sqr(horizontalDistance) + horizontalRange * 0.5f;
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

				float q2 = velocity2 * M::Sqr(sin(a)) + 2.f * g * verticalDistance;
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
				float velocity2 = M::Sqr(v);
				TAngles rs;
				float q0 = M::Sqr(velocity2) - g  * (g * M::Sqr(horizontalDistance) + 2.f * verticalDistance * velocity2 );
				if (q0 < 0)
					return rs;

				float q = sqrt(q0);
				float a0 = atan((velocity2 - q) / (g * horizontalDistance));
				float a1 = atan((velocity2 + q) / (g * horizontalDistance));

				if (a0 > 0)
					rs.idealAngle[rs.numAngles++] = a0;
				if (a1 > 0 && (rs.numAngles == 0 || !M::Similar(a0,a1)))
					rs.idealAngle[rs.numAngles++] = a1;
				return rs;
			}

			static bool						CanHit(TrajectoryType t, float flatRange, const TrajectoryEndPoint&origin, const TrajectoryEndPoint&dest, float&outAngle);
			static bool						CanHit(TrajectoryType t, float flatRange, const TrajectoryEndPoint&origin, const TrajectoryEndPoint&dest);



			void							BuildBlastSystem(const Grid::LCellIdI&location, float radius, M::TMatrix4<>&system) const;
			void							BuildBlastSystem(const M::TVec3<>&location, float radius, M::TMatrix4<>&system) const;

			bool							CanAttack() const {return attacksPerRound.IsFunctionOrConstNonZero();}
			bool							CanDealDamage() const {return damageLevel.IsFunctionOrConstNonZero();}
			bool							CanDo(operation_t op) const {return operationSpeed[op].IsFunctionOrConstNonZero();}
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
			#endif
			void							DescribeType(Description&, bool compound, bool includeName, const String&parentType, const OwnerParameter&owner);


			enum geometryType_t
			{
				Schematic,
				Animation,
				SmokeTrail
			};

			static float					CompileTrajectory(const Ctr::Buffer0<M::TVec4<> >&v, geometryType_t type, Geometry&outGeo, float widthScale=1.f);

			/**
			Checks whether or not the local turret could ever possibly consume resources
			*/
			bool							ConsumesResources(ResourceFlagSet&res, const PFaction&) const;

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

			FScaledFunc						turretRangeModifier /*= 0*/;
			FFunc							operationSpeedMultiplier /*= 1*/,
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
			Ctr::FixedArray<BYTE,OperationCount>	acceleration;


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

			bool							CanStandOn(const World::Data&world, const Grid::LCellIdI&, bool ignoreWater, bool doFloat);

			void							DescribeType(Description&, bool compound, bool includeName, const OwnerParameter&);

	
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
			void		Configure(Turret&);

			bool		CanBuild(const PStructure&proto);

		};

		namespace Bomb
		{


			void		Configure(Turret&);


			float		GetRadius(bool doomsday, const PFaction&);
			int			GetDamageLevel(bool doomsday, const PFaction&);
		};

	
		extern Turret	terraformer,mechanic,
						artillery,launcher,rockets,emp,armored,
						bomb,
						munitionTransporter,
						radar,resourceScanner
						#ifndef NO_SATELLITES
							,protonCannon,skyHubTurret,ragnarok,starPort
						#endif
						;

		namespace SubOrbital
		{
			extern Turret		raptor,bomber,cargoPlane,sensorDrone;
			extern VehicleBase	planeBase;
		}

	
		namespace Artillery
		{
			void		Configure(Turret&);
		};

		namespace Launcher
		{
			void		Configure(Turret&);
		};

		namespace Rockets
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



		namespace SubOrbital
		{
			#ifdef HAS_WINDOW
			namespace Cmd
			{
				void	Land();
				void	TakeOff();
			}
			#endif

			namespace Raptor
			{
				void	Configure(Turret&);
			}

			namespace Bomber
			{
				void	Configure(Turret&);
			}

			namespace CargoPlane
			{
				void	Configure(Turret&);
			}

			namespace SensorDrone
			{
				void	Configure(Turret&);
			}

			namespace Base
			{
				void	Configure(VehicleBase&);
			}
		}

	








	}

	inline float3 ToWorld(const M::TVec4<>&trajectoryPoint)
	{
		return trajectoryPoint.xyz * trajectoryPoint.w;
	}

}



#include <global_root.h>
#include "components.h"
#include "systems.h"
#include "vehicle.h"
#include "structure.h"
#include "../cast.h"
#include "jobs.h"
#include "prototypes.h"
#include "interface.h"
#include "satellite.h"
#include "subOrbital.h"
#include "gridAndChoices.h"
#include "../runtimeConfig.h"
#include "faction.h"
#include "../grid/worldData.h"
#include "structure/storageStructures.h"
#include "subOrbital.h"

namespace Objects
{
	Timer::Time				Component::Turret::timeSpentTracing = 0;
	count_t					Component::Turret::traceCount=0;


	/**/			TrajectoryEndPoint::TrajectoryEndPoint(const M::TVec3<>&worldLocation)
	{
		radius = Vec::length(worldLocation);
		cellLocation.stackID = Grid::low.Locate(worldLocation,0);
		cellLocation.height = (Grid::HeightFloorFromRadius(radius));
		this->worldLocation = worldLocation;

	//	LogEvent(EventSignificance::Uncommon,"from world: "+ToString());
	}

	/**/			TrajectoryEndPoint::TrajectoryEndPoint(const Grid::LCellIdI&coords, float zOffset)
	{
		radius = zOffset + Grid::ToRadiusf(0.05f + coords.height);
		worldLocation = Grid::low.cells[coords.stackID].centerVertex.xyz * radius;
		cellLocation = coords;
	//	LogEvent(EventSignificance::Uncommon,"from coords+z("+String(zOffset)+"): "+ToString());
	}

	/**/			TrajectoryEndPoint::TrajectoryEndPoint(const Grid::LCellIdI&coords)
	{
		radius = Grid::ToRadiusf(0.05f + coords.height);
		worldLocation = Grid::low.cells[coords.stackID].centerVertex.xyz * radius;
		cellLocation = coords;
	//	LogEvent(EventSignificance::Uncommon,"from coords: "+ToString());
	}


	String			ToString(const TrajectoryEndPoint&endPoint)
	{
		using StringConversion::ToString;
		return ToString(endPoint.worldLocation)+"/"+ToString(endPoint.cellLocation)+" r="+ToString(endPoint.radius);
	}



	namespace Component
	{
		Turret			terraformer,mechanic,
						artillery,launcher,rockets,emp,armored,
						antiAircraft,
						bomb,
						munitionTransporter,
						radar,resourceScanner
						#ifndef NO_SATELLITES
							,protonCannon,skyHubTurret,ragnarok,starPort
						#endif
						;


		VehicleBase		hoverbase,
						heavybase,
						catbase;


		namespace SubOrbital
		{
			Turret		raptor,bomber,cargoPlane,sensorDrone;
			VehicleBase	planeBase;
		}
		const char*				OperationToString(operation_t op)
		{
			#undef ECASE
			#define ECASE(X)	case X: return #X;

			switch (op)
			{
				ECASE(Construct)
				ECASE(Nuclear)
				ECASE(Repair)
				ECASE(TerrainAdd)
				ECASE(TerrainRemove)
			}
			return "undefined";
		}


		const Research::Attributes::Table&	ResearchStateOf(const OwnerParameter&owner)
		{
			return owner ? owner->researchState.attributeTable : Research::globalState.zeroTable;
		}


		float							VehicleBase::GetBaseMovementSpeed(const OwnerParameter&owner) const
		{
			return movementSpeed.GetValue(ResearchStateOf(owner));
		}
		float							VehicleBase::GetCarryingCapacity(const OwnerParameter&owner) const
		{
			return carryingCapacity.GetValue(ResearchStateOf(owner));
			//return owner ? owner->researchState.GetFactorFor(*this,Attribute::CarryWeight) * baseCarryingCapacity : baseCarryingCapacity;
		}
		float							VehicleBase::GetBaseHealth(const OwnerParameter&owner) const
		{
			return health.GetValue(ResearchStateOf(owner));
		}



		float							Turret::GetOperationSpeed(operation_t op, const OwnerParameter&owner) const
		{
			return operationSpeed[op](owner);
		}

		int							Turret::GetDamageLevel(const PFaction&owner) const
		{
			return damageLevel(owner);
		}

		float							Turret::GetExplosionRadius(const PFaction&owner) const
		{
			return  explosionRadius(owner);
		}

		int								Turret::GetAttacksPerRound(const PFaction&owner) const
		{
			return  attacksPerRound(owner);
		}		

		int								Turret::GetAmmoNeedPerShot(const PFaction&owner) const
		{
			return ammoNeedPerShot(owner);
		}

		float							Turret::GetHorizontalAttackRange(const PFaction&owner, const FFunc&bonus/*=FFunc()*/) const
		{
			if (horizontalAttackRange.IsEmpty())
				return 0;
			return (horizontalAttackRange(owner) + bonus(owner));
		}

		float							Turret::GetScanningRange(const PFaction&owner, int bonus/*=0*/) const
		{
			return (scanningRange(owner) + bonus);
		}

		float							Turret::GetGroundScanningRange(const PFaction&owner) const
		{
			return groundScanningRange(owner);
		}



		void		Turret::BuildBlastSystem(const M::TVec3<>&location, float radius, M::TMatrix4<>&system) const
		{
			BuildSystem(location,radius,radius,system);
		}


		void		Turret::BuildBlastSystem(const Grid::LCellIdI&location, float radius, M::TMatrix4<>&system)	const
		{
			BuildSystem(location,radius,radius,system);
		}


		bool							Turret::ConsumesResources(ResourceFlagSet&res, const PFaction&owner) const
		{
			bool rs = false;
			if (this->mountType == Component::Turret::MountType::Satellite)
			{
				res |= Resource::Fuel;
				rs = true;
			}
			if (CanAttack())
			{
				int demand = GetAmmoNeedPerShot(owner);
				if ( demand > 0)
				{
					res |= ammoType;
					rs = true;
				}
			}
			if (CanDo(Component::Construct) || CanDo(Component::Repair) || CanDo(Component::TerrainAdd))
			{
				res |= Resource::Minerals;
				rs = true;
			}
			if (CanDo(Component::Nuclear))
			{
				res |= Resource::Uranium;
				rs = true;
			}
			return rs;
		}


		#ifdef HAS_WINDOW
			/*static*/ float			Turret::CompileTrajectory(const Buffer0<M::TVec4<> >&v, geometryType_t type, Geometry&geo, float widthScale/*=1.f*/)
			{
				bool schematic = type == Schematic;
				bool smoke = type == SmokeTrail;
				if (v.IsNotEmpty())
				{
					M::TVec3<> n;
					float3 front = v.First().xyz * v.First().w;
					float3 back = v.Last().xyz * v.Last().w;
					Vec::cross(front,back-front,n);
					Vec::normalize(n);
					n*= 0.5f * widthScale;
					if (!smoke)
						n*= v.First().w / Grid::MeanPlanetRadius;;
					float d = 0;
					geo.SetVertexOffsetToCurrent();
					M::TVec3<> lastB;
					float innerWhite = schematic ? 1.f : 1.1f;
					float innerOpacity = schematic ? 1.5f : 1.1f;
					float outerWhite = schematic ? 1.0f : 0.f;
					float outerOpacity = schematic ? 0.5f : 0.f;

					float topGreen = smoke ? 0.5f : outerWhite;
					float bottomGreen = smoke ? 0.5f : outerWhite;
					float rightGreen = smoke ? 1.f : outerWhite;
					float leftGreen = smoke ? 0.f : outerWhite;
					float centerGreen = smoke ? 0.5f : innerWhite;

					float lengthScale = 1.f;
					if (smoke)
					{
						for (index_t i = 0; i < v.Count(); i++)
						{
							const M::TVec4<>&_b = v[i];
							M::TVec3<>b = _b.xyz * _b.w;


							if (i)
								d += Vec::distance(lastB,b);
							lastB = b;
						}
						lengthScale = 1.f / d;
						d = 0.f;
					}



					for (index_t i = 0; i < v.Count(); i++)
					{
						const M::TVec4<>&_b = v[i];
						M::TVec3<>b = _b.xyz * _b.w;
						M::TVec3<> upNormal;
						if (i > 0)
						{
							if (schematic)
							{
								geo.MakeQuad(0,1,4,3);
								geo.MakeQuad(1,2,5,4);
								//geo.MakeQuad(0,4,6,2);
								//geo.MakeQuad(2,6,5,1);
								//geo.MakeQuad(1,5,7,3);
								//geo.MakeQuad(3,7,4,0);
							}
							else
							{
								geo.MakeQuad(0,1,6,5);
								geo.MakeQuad(1,2,7,6);
								geo.MakeQuad(1,4,9,6);
								geo.MakeQuad(3,1,6,8);
							}
							if (smoke)
							{
								Vec::cross(n,b-lastB,upNormal);
								Vec::normalize(upNormal);
							}

							d += Vec::distance(lastB,b) * lengthScale;
						}
						else
						{
							if (smoke && v.Count() > 1)
							{
								const M::TVec4<>&_b2 = v[1];
								M::TVec3<>b2 = _b2.xyz * _b2.w;

								Vec::cross(n,b2-b,upNormal);
								Vec::normalize(upNormal);
							}
						}
						float oo = outerOpacity;
						float io = innerOpacity;
						if (schematic)
						{
							float fc = 1.f - M::Sqr((float(i) - (v.Count()/2)) / float(v.Count()/2));
							oo *= fc;
							io *= fc;
						}

						geo.SetVertexOffsetToCurrent();
						{
							geo.MakeVertex(b + n,n,float4(oo,rightGreen,d,0.5f));	//red controls opacity [0,1], green controls whiteness or U (smoke) in [0,1], blue user for distance metric, alpha controls V
							//if (!schematic)
							geo.MakeVertex(b,_b.xyz,float4(io,centerGreen,d,0.5f));
							geo.MakeVertex(b - n,-n,float4(oo,leftGreen,d,0.5f));
							if (!schematic)
							{
								if (smoke)
								{
									geo.MakeVertex(b +upNormal*0.5f*widthScale,upNormal,float4(oo,topGreen,d,1.f));
									geo.MakeVertex(b -upNormal*0.5f*widthScale,-upNormal,float4(oo,bottomGreen,d,0.f));
								}
								else
								{
									geo.MakeVertex(b +_b.xyz*0.5f*widthScale,_b.xyz,float4(oo,topGreen,d,1.f));
									geo.MakeVertex(b -_b.xyz*0.5f*widthScale,-_b.xyz,float4(oo,bottomGreen,d,0.f));
								}
							}
						}
						lastB = b;
					}
					return d;
				}
				return 0;
			}
		#endif


		///*static*/ bool						Turret::CanHit(float flatRange, const M::TVec3<>&origin, const M::TVec3<>&dest)
		//{
		//	float hDest = Vec::length(dest);
		//	float hOrigin = Vec::length(origin);
		//	return CanHit(flatRange, origin /hOrigin,hOrigin,dest/hDest,hDest);
		//}




		/*static*/ int				Turret::GetGroundHeight( const World::Data&world, const Grid::LCellIdI&location )
		{
			UINT32 fine = Grid::low.grid.GetCell(location.stackID).higherEquivalent;
			return world.GetStack(fine).FindTopClearOverNonClear(true,false);
		}

		/*static*/ int				Turret::GetCeilingHeight( const World::Data&world, const Grid::LCellIdI&location )
		{
			UINT32 fine = Grid::low.grid.GetCell(location.stackID).higherEquivalent;
			return world.GetStack(fine).FindNextClearBeneathNonClear(true,false,Grid::CellStack::ToLayer(location.height));
		}



		/*static*/ bool						Turret::CanHit( TrajectoryType t, float flatRange, const TrajectoryEndPoint&origin, const TrajectoryEndPoint&dest, float&outAngle )
		{
			if (t == TrajectoryType::Artillery)
				return Detail::ArtilleryCanHit(flatRange,Vec::distance(origin.worldLocation,dest.worldLocation),dest.radius - origin.radius,outAngle);

			outAngle = 0;
			return CanHit(t,flatRange,origin,dest);
		}
	

		/*static*/ bool						Turret::CanHit( TrajectoryType t, float flatRange, const TrajectoryEndPoint&origin, const TrajectoryEndPoint&dest )
		{
			switch (t)
			{
				case TrajectoryType::VerticalBeam:
				{
					if (dest.cellLocation.stackID != origin.cellLocation.stackID)
						return false;
					return dest.cellLocation.height >= GetGroundHeight(World::actual, origin.cellLocation)-1;
				}
				case TrajectoryType::AAMissile:
				{
					//let's define this as a cone, so that mid-way explosions are valid
					const float h = Grid::ToRadius(Objects::SubOrbital::Height);
					const float r = flatRange;
					const float h2 = sqrt(M::Sqr(h) - M::Sqr(r));
					const auto&axis = Grid::low.cells[origin.cellLocation.stackID].cornerCenter;
					const float h2d = Vec::dot(axis,dest.worldLocation - origin.worldLocation);
					if (h2d < 0)
						return false;
					const float rd = h2d * r / (h2 - origin.radius);
					return Vec::quadraticDistance(origin.worldLocation + axis * h2d,dest.worldLocation) <= M::Sqr(rd);
				}
				case TrajectoryType::Artillery:
					return Detail::ArtilleryCanHit(flatRange,Vec::distance(origin.worldLocation,dest.worldLocation),dest.radius - origin.radius);
				case TrajectoryType::Missile:
					return Detail::MissileCanHit(flatRange,origin,dest);
				default:
					FATAL__("Incompatible trajectory type "+String(t.ToString()));
			}
			return false;
		}

		/*static*/ bool						Turret::Detail::ArtilleryCanHit(float flatRange, float horizontalDistance, float verticalDistance, float&outAngle)
		{
			if (verticalDistance > Turret::MaxHeightAtHDistance(flatRange,horizontalDistance))
				return false;
			auto a = Turret::GetAngleForDistance(Turret::MaxRangeToVelocity(flatRange),horizontalDistance,verticalDistance);
			if (a.numAngles == 0)
			{
				flatRange += 0.1f;
				return ArtilleryCanHit(flatRange,horizontalDistance,verticalDistance,outAngle);
			}
			outAngle = a.idealAngle[0];
			return true;
		}
		/*static*/ bool						Turret::Detail::ArtilleryCanHit(float flatRange, float horizontalDistance, float verticalDistance)
		{
			return (verticalDistance <= Turret::MaxHeightAtHDistance(flatRange,horizontalDistance));
		}


		/*static*/ bool						Turret::Detail::MissileCanHit(float flatRange, const TrajectoryEndPoint&origin, const TrajectoryEndPoint&destination)
		{
			return M::Sqr(flatRange) >= Vec::quadraticDistance(origin.worldLocation,destination.worldLocation);
		}



		/*static*/ Turret::TAngles					Turret::GetAngleForDistance(float v, const TrajectoryEndPoint&origin, const TrajectoryEndPoint&dest)
		{
			return GetAngleForDistance(v,Vec::distance(origin.worldLocation,dest.worldLocation*origin.radius/dest.radius),dest.radius - origin.radius);
		}



		
		/*static*/ void							Turret::TraceAnimationTrajectory(TrajectoryType t, float flatRange,  float attackAngle, const TrajectoryEndPoint&from,const TrajectoryEndPoint&to, Ctr::Buffer0<M::TVec4<> >&outSteps)
		{
			outSteps.Clear();

			switch (t)
			{
				case TrajectoryType::Artillery:
					Detail::TraceAnimationArtilleryTrajectory(Turret::MaxRangeToVelocity(flatRange),attackAngle, from, to, outSteps);
				break;
				case TrajectoryType::Missile:
				case TrajectoryType::AAMissile:
					Detail::TraceAnimationMissileTrajectory(flatRange,from, to, outSteps);
				break;
				case TrajectoryType::VerticalBeam:
				{
					UINT32 stackIndex = from.cellLocation.stackID;
					int h = GetGroundHeight(World::actual, Grid::LCellIdI(stackIndex,0));
					if (h == badHeight)
						return;
					float4 p(Grid::low.cells[stackIndex].cornerCenter,Grid::ToRadius(h));
					outSteps.Append(p);
				}
				break;
				default:
					FATAL__("Incompatible trajectory type "+String(t.ToString()));
			}
		}

		/*static*/ void							Turret::Detail::TraceAnimationMissileTrajectory(float range, const TrajectoryEndPoint&here, const TrajectoryEndPoint&there, Ctr::Buffer0<M::TVec4<> >&outSteps)
		{
			float d2 = Vec::quadraticDistance(here.worldLocation,there.worldLocation);
			const float3 up = (here.worldLocation + there.worldLocation).Normalized0();
			float distance = sqrt(d2);


			const float h = there.radius - here.radius;

		

			const int resolution = std::max<int>(2,distance*2);

			float lastH = here.radius;
			M::TVec3<> lastB = here.worldLocation;
			for (int i = 0; i <= resolution; i++)
			{
				float t01 = float(i) / float(resolution);
			
				float fy = here.radius + (t01 * h);
				M::TVec3<> b;
				Vec::interpolate(here.worldLocation,there.worldLocation,t01,b);
				Vec::normalize(b);
				outSteps << float4(b,fy);

			}
		}


		/*static*/ void							Turret::Detail::TraceAnimationArtilleryTrajectory(float velocity,  float angle, const TrajectoryEndPoint&source, const TrajectoryEndPoint&dest,Ctr::Buffer0<M::TVec4<> >&outSteps)
		{
			const float sn = sin(angle),
						cs = cos(angle);


			const float3 up = (source.worldLocation + dest.worldLocation).Normalized0();
			float distance = Vec::distance(source.worldLocation,dest.worldLocation);


			const float h = dest.radius - source.radius;

			const float tEnd = (velocity * sn + sqrt(M::Sqr(velocity*sn) - 2.f * g * h)) / g;


			const int resolution = std::max<int>(2,distance*4);

			float lastH = source.radius;
			M::TVec3<> lastB = source.worldLocation;
			for (int i = 0; i <= resolution; i++)
			{
				float t01 = float(i) / float(resolution);
				const float t = t01 * tEnd;
				float fy = source.radius + (velocity * t*sn - 0.5f * g * M::Sqr(t));
				M::TVec3<> b;
				Vec::interpolate(source.worldLocation,dest.worldLocation,t01,b);
				Vec::normalize(b);
				outSteps << float4(b,fy);

			}
		}


		/*static*/ ImpactDetails			Turret::TraceTrajectory(TrajectoryType t, float flatRange, const World::Data&visibleWorld, const World::Data&actualWorld, bool traceInActualWorld, const TrajectoryEndPoint&source, const TrajectoryEndPoint&dest, Ctr::Buffer0<M::TVec4<> >&outSteps)
		{
			outSteps.Clear();
			if (!Turret::CanHit(t,flatRange,source,dest))
				return ImpactDetails{};

			switch (t)
			{
				case TrajectoryType::Artillery:
					return Detail::TraceArtilleryTrajectory(Turret::MaxRangeToVelocity(flatRange),visibleWorld,actualWorld, !traceInActualWorld,source, dest, outSteps);
				break;
				case TrajectoryType::Missile:
				case TrajectoryType::AAMissile:
					return Detail::TraceMissileTrajectory(flatRange,traceInActualWorld ? actualWorld : visibleWorld,source, dest, outSteps);
				break;
				case TrajectoryType::VerticalBeam:
				{
					ImpactDetails rs;
					int h = GetGroundHeight(traceInActualWorld ? actualWorld : visibleWorld,source.cellLocation);
					if (h == badHeight)
						return rs;
					outSteps.Clear();
					float4 p(Grid::low.cells[source.cellLocation.stackID].cornerCenter,Grid::ToRadius(h));
					outSteps.Append(p);
					rs.result = TraceResult::ReachesTarget;
					rs.impactAt = p.xyz * p.w;
					rs.impactCell = Grid::HCellIdI(Grid::low.grid.GetCell(source.cellLocation.stackID).higherEquivalent,h);
					return rs;
				}
				break;
				default:
					FATAL__("Incompatible trajectory type "+String(t.ToString()));
			}
			return ImpactDetails{};

		}

		static float	PreTraceArtillery(float velocity, float angle, const TrajectoryEndPoint&here, const TrajectoryEndPoint&there, float distance, const World::Data&world)
		{
			float sn = sin(angle),
					cs = cos(angle);


			const int resolution = std::max<int>(2,std::max(distance*2.f,distance * sn * 5.f));

			float delta = there.radius - here.radius;
			float tEnd = (velocity * sn + sqrt(M::Sqr(velocity*sn) - 2.f * g * delta)) / g;




			//const float3 up = (here + there).Normalized0();

			#ifdef HYBRID_GRID
				UINT32 cellAt = Grid::low.grid.GetCell(here.cellLocation.stackID).higherEquivalent;
			#else
				UINT32 cellAt = source.cellID;
			#endif

			
			float lastH = here.radius;
			M::TVec3<> lastB = here.worldLocation;
			for (int i = 0; i <= resolution; i++)
			{
				float t01 = float(i) / float(resolution);
				const float t = t01 * tEnd;
				float fy = here.radius + (velocity * t*sn - 0.5f * g * M::Sqr(t));
				M::TVec3<> b;
				Vec::interpolate(here.worldLocation,there.worldLocation,t01,b);
				Vec::normalize(b);
//				outSteps << float4(b,fy);

				cellAt = Grid::high.Locate(b,cellAt);
				int h = (Grid::HeightFloorFromRadius(fy));
				if (world.GetStack(cellAt).GetCellI(h).IsOccupied())
				{
					float3 impactAt = b * (fy);
					return Vec::quadraticDistance(impactAt, there.worldLocation);
				}
			}
			return 0;
		}

		/*static*/ ImpactDetails			Turret::Detail::TraceArtilleryTrajectory(float velocity, const World::Data&decisionWorld, const World::Data&actualWorld, bool traceInDecisionWorld, const TrajectoryEndPoint&source, const TrajectoryEndPoint&dest, Ctr::Buffer0<M::TVec4<> >&outSteps)
		{
			Turret::TAngles angles = Turret::GetAngleForDistance(velocity,source,dest);
			if (angles.numAngles == 0)
			{
				if (velocity < 50.f)
				{
					//LogEvent(EventClass::Preset::internalError,"Unable to determine feasible angles for artillery trajectory at v="+String(velocity)+", source="+ToString(source)+", dest="+ToString(dest)+". Increasing velocity and retrying.");
					velocity = velocity > 0 ? velocity * 1.1f : 1.f;	//should not be zero, but it's better to handle this case here anyway. Otherwise we're causing a stack overflow in no-time
					return TraceArtilleryTrajectory(velocity,decisionWorld,actualWorld,traceInDecisionWorld,source, dest,outSteps);
				}

				return ImpactDetails{};
			}

			Turret::traceCount++;
			const Timer::Time t0 = timer.Now();
		/*	const float h0 = zOffset + Grid::ToRadiusf(0.05f + source.height);
			const float h1 = Grid::ToRadiusf(0.05f + dest.height);
			const float3 there = Grid::low.cells[dest.stackID].centerVertex.xyz * h1;
			const float3 here = Grid::low.cells[source.stackID].centerVertex.xyz * h0;
*/
			//const float3 up = (here + there).Normalized0();
			float distance = Vec::distance(source.worldLocation,dest.worldLocation);

		

			const float h = dest.radius - source.radius;
			int k = 0;
			float leastDistance = std::numeric_limits<float>::max();
			for (int j = 0; j < angles.numAngles; j++)
			{
				float d2 = PreTraceArtillery(velocity,angles.idealAngle[j], source,dest,distance,decisionWorld);
				LogEvent(EventSignificance::Debug,ToString(source)+"->"+ToString(dest)+ ": Angle "+String(j)+"/"+String(angles.numAngles)+" "+String(angles.idealAngle[j])+" lead to distance "+String(d2));
				if (d2 < leastDistance)
				{
					leastDistance = d2;
					k = j;
				}
			}




			{
				ImpactDetails rs;
				outSteps.Clear();
				float angle = rs.artilleryAngle = angles.idealAngle[k];
				LogEvent(EventSignificance::Debug,"Using angle "+String(k)+"/"+String(angles.numAngles)+" "+String(angle));
				float sn = sin(angle),
						cs = cos(angle);


				const int resolution = std::max<int>(2,std::max(distance*2.f,distance * sn * 5.f));

				float tEnd = (velocity * sn + sqrt(M::Sqr(velocity*sn) - 2.f * g * h)) / g;



				rs.impactAt = dest.worldLocation;
				const auto impactAtHCell = Grid::ToHigh(dest.cellLocation);
				#ifdef HYBRID_GRID
					rs.impactCell = Grid::HCellIdI(Grid::low.grid.GetCell(dest.cellLocation.stackID).higherEquivalent,dest.cellLocation.height);
				#else
					rs.impactCell = dest.cellLocation;
				#endif

				//const float3 up = (here + there).Normalized0();

				#ifdef HYBRID_GRID
					UINT32 cellAt = Grid::low.grid.GetCell(source.cellLocation.stackID).higherEquivalent;
				#else
					UINT32 cellAt = source.cellLocation.stackID;
				#endif

			
				float lastH = source.radius;
				M::TVec3<> lastB = source.worldLocation;
				for (int i = 0; i <= resolution; i++)
				{
					float t01 = float(i) / float(resolution);
					const float t = t01 * tEnd;
					float fy = source.radius + (velocity * t*sn - 0.5f * g * M::Sqr(t));
					M::TVec3<> b;
					Vec::interpolate(source.worldLocation,dest.worldLocation,t01,b);
					Vec::normalize(b);
					outSteps << float4(b,fy);

					cellAt = Grid::high.Locate(b,cellAt);
					int h = (Grid::HeightFloorFromRadius(fy));

					const auto&actualCell = actualWorld.GetStack(cellAt).GetCellI(h);
					bool impact = false;
					if (traceInDecisionWorld)
					{
						const auto&decisionCell = decisionWorld.GetStack(cellAt).GetCellI(h);
						if (decisionCell.content != Grid::Content::Clear)
							impact = true;
						elif (decisionCell.TerrainIsVisible())
						{
							if (actualCell.structureCounter > 1)
								impact = true;
						}
					}
					else
						impact = actualCell.IsOccupied();


					if (impact)
					{
						rs.impactAt = b * (fy);
						rs.impactCell = Grid::HCellIdI(cellAt,h);
						rs.distanceToTarget = Vec::distance(rs.impactAt,dest.worldLocation);
						rs.result = rs.distanceToTarget < 0.1f || impactAtHCell == Grid::HCellIdI(cellAt,h) ? TraceResult::ReachesTarget : TraceResult::MidFlightImpact;
						
						Turret::timeSpentTracing += timer.Now() - t0;
						return rs;
					}
				}

				rs.impactCell = Grid::HCellIdI(
					#ifdef HYBRID_GRID
						Grid::low.grid.GetCell(dest.cellLocation.stackID).higherEquivalent
					#else
						dest.stackID

					#endif
				,dest.cellLocation.height);
				float3 last = outSteps.Last().xyz * outSteps.Last().w;
				rs.impactAt = dest.worldLocation;//last;//Grid::ToWorld(rs.impactCell);
				//rs.distanceToTarget = Vec::distance(rs.impactAt,dest.worldLocation);
				rs.result = TraceResult::ReachesTarget;
				Turret::timeSpentTracing += timer.Now() - t0;
				return rs;
			}
		}

		/*static*/ ImpactDetails			Turret::Detail::TraceMissileTrajectory( float range, const World::Data&world, const TrajectoryEndPoint&source, const TrajectoryEndPoint&dest, Ctr::Buffer0<M::TVec4<> >&outSteps )
		{
		/*

			const float h0 = zOffset + Grid::ToRadiusf(0.05f + source.height);
			const float h1 = Grid::ToRadiusf(0.05f + dest.height);
			const float3 there = Grid::low.cells[dest.stackID].centerVertex.xyz * h1;
			const float3 here = Grid::low.cells[source.stackID].centerVertex.xyz * h0;
		*/
			float d2 = Vec::quadraticDistance(source.worldLocation,dest.worldLocation);

			Turret::traceCount++;
			const Timer::Time t0 = timer.Now();

			const float3 up = (source.worldLocation + dest.worldLocation).Normalized0();
			float distance = sqrt(d2);

			const float h = dest.radius - source.radius;

			const int resolution = std::max<int>(2,distance*2);
			UINT32 cellAt = Grid::low.grid.GetCell(source.cellLocation.stackID).higherEquivalent;
			const auto impactAtHCell = Grid::ToHigh(dest.cellLocation);

			float lastH = source.radius;
			M::TVec3<> lastB = source.worldLocation;
			for (int i = 0; i <= resolution; i++)
			{
				float t01 = float(i) / float(resolution);
			
				float fy = source.radius + (t01 * h);
				M::TVec3<> b;
				Vec::interpolate(source.worldLocation,dest.worldLocation,t01,b);
				Vec::normalize(b);
				outSteps << float4(b,fy);

				cellAt = Grid::high.Locate(b,cellAt);
				int h = (Grid::HeightFloorFromRadius(fy));
				if (world.GetStack(cellAt).GetCellI(h).IsOccupied())
				{
					ImpactDetails rs;
					rs.impactAt = b * (fy);
					rs.impactCell = Grid::HCellIdI(cellAt,h);
					rs.distanceToTarget = Vec::distance(rs.impactAt,dest.worldLocation);
					rs.result = rs.distanceToTarget < 0.1f || impactAtHCell == Grid::HCellIdI(cellAt,h) ? TraceResult::ReachesTarget : TraceResult::MidFlightImpact;
					Turret::timeSpentTracing += timer.Now() - t0;
					return rs;
				}
			}
			ImpactDetails rs;
			rs.impactCell = Grid::HCellIdI(
				#ifdef HYBRID_GRID
					Grid::low.grid.GetCell(dest.cellLocation.stackID).higherEquivalent
				#else
					dest.cellLocation.stackID
				#endif
			,dest.cellLocation.height);

			float3 last = outSteps.Last().xyz * outSteps.Last().w;
			rs.impactAt = dest.worldLocation;//Grid::ToWorld(rs.impactCell);
			//rs.distanceToTarget = Vec::distance(rs.impactAt,dest.worldLocation);
			rs.result = TraceResult::ReachesTarget;
			Turret::timeSpentTracing += timer.Now() - t0;
			return rs;
		}









		#ifdef CLIENT


			bool		VehicleNotBusy(const PWorldEntity&e)
			{
				PMobileEntity v = To<MobileEntity>::DynamicCast(e);
				return v && !v->HasTasks();
			}

		#endif



		bool Unlockable(const PStructure&proto)
		{
			return proto->IsUnlocked() || proto->CanBeUnlocked();
		}

		bool			Terraformer::CanBuild(const PStructure&proto)
		{
			return !proto->IsIndestructible()
					#ifdef ONE_COMPOUND_PER_FACTION
						&& !To<Connector>::DynamicCast(proto)
					#endif
					#ifdef NO_MUNITION
						&& !To<MunitionFactory>::DynamicCast(proto) && !To<MunitionStorage>::DynamicCast(proto)
					#endif
				&& Unlockable(proto)
			;
		}


		void			Terraformer::Configure(Turret&component)
		{
			#ifdef HAS_WINDOW		
				component.parts.SetSize(1);

				component.parts.First().LoadFromObj("objects/terraformer.obj");
			#endif

			component.typeName = "Terraformer";
			component.shortName = "former";
			component.constName = ConstName::Turret_Terraformer;

			//component.productionCosts = 40;
			#ifndef ONE_COMPOUND_PER_FACTION
				component.baseStorageCapacity.level[Resource::Minerals] = 200;
			#endif
		}



		float		Bomb::GetRadius(bool doomsday, const PFaction& owner)
		{
			//return doomsday ? 7*Grid::UnitScale : artillery.explosionRadius*2;
			return bomb.GetExplosionRadius(owner) * (doomsday ? 2 : 1);
		}

		int			Bomb::GetDamageLevel(bool doomsday, const PFaction& owner)
		{
			//return doomsday ? 50 : artillery.damageLevel*2;
			return bomb.GetDamageLevel(owner) * (doomsday ? 5 : 1);
		}

	
		void			Bomb::Configure(Turret&component)
		{

			#ifdef HAS_WINDOW
				

				component.parts.SetSize(1);

				component.parts.First().LoadFromObj("objects/bomb.obj");
			#endif

			component.typeName = "Bomb";
			component.shortName = "bomb";
			component.constName = ConstName::Turret_Bomb;

			//component.productionCosts = 40;
			#ifndef ONE_COMPOUND_PER_FACTION
				component.baseStorageCapacity.level[Resource::Uranium] = VehicleJob::DoomsdaySelfDetonationCosts;
			#endif
			component.continuousRotation = true;
			component.continuousRotationSpeed = 0.125f;
		}


	


	
		void			Artillery::Configure(Turret&component)
		{
			#ifdef HAS_WINDOW		
				component.parts.SetSize(1);
				component.parts.First().LoadFromObj("objects/tank.obj");
			#endif


			component.typeName = "Artillery";
			component.shortName = " Artillery";
			component.constName = ConstName::Turret_Artillery;

			#if !defined ONE_COMPOUND_PER_FACTION && !defined NO_MUNITION
				component.baseStorageCapacity.level[Resource::Munitions] = 24;
			#endif

			component.initiative = 80;
		}


		void			Launcher::Configure(Turret&component)
		{
			#ifdef HAS_WINDOW		
				component.parts.SetSize(1);
				component.parts.First().LoadFromObj("objects/launcher.obj");
			#endif

			component.mountType = Turret::MountType::TowerOnly;
			component.typeName = "Missile Launcher";
			component.shortName = " Launcher";
			component.constName = ConstName::Turret_Launcher;
			component.attacksMovingHostilesDuringMotionPhase = true;

			#if !defined ONE_COMPOUND_PER_FACTION && !defined NO_MUNITION
				component.baseStorageCapacity.level[Resource::Munitions] = 24;
			#endif

			component.trajectoryType = TrajectoryType::Missile;

			component.initiative = 70;
		}

		void			Rockets::Configure(Turret&component)
		{
			#ifdef HAS_WINDOW		
				component.parts.SetSize(1);
				component.parts.First().LoadFromObj("objects/rockets.obj");
			#endif

			component.mountType = Turret::MountType::TowerOnly;
			component.typeName = "Rocket Launcher";
			component.shortName = " Rockets";
			component.constName = ConstName::Turret_Rockets;
			component.attacksMovingHostilesDuringMotionPhase = true;

			#if !defined ONE_COMPOUND_PER_FACTION && !defined NO_MUNITION
				component.baseStorageCapacity.level[Resource::Munitions] = 20;
			#endif

			component.trajectoryType = TrajectoryType::Missile;
		}


		void			Armored::Configure(Turret&component)
		{
			#ifdef HAS_WINDOW		
				component.parts.SetSize(1);
				component.parts.First().LoadFromObj("objects/armored.obj");
			#endif


			component.typeName = "Cannon";
			component.shortName = "tank";
			component.constName = ConstName::Turret_Cannon;

			#if !defined ONE_COMPOUND_PER_FACTION && !defined NO_MUNITION
				component.baseStorageCapacity.level[Resource::Munitions] = 20;
			#endif

			component.independentAttackAndMotion = true;
		}

		void			AntiAircraft::Configure(Turret&component)
		{
			#ifdef HAS_WINDOW		
				component.parts.SetSize(1);
				component.parts.First().LoadFromObj("objects/antiAircraft.obj");
			#endif


			component.typeName = "AntiAircraft";
			component.shortName = " AA";
			component.constName = ConstName::Turret_AntiAircraft;
			component.isScanner = true;
			component.trajectoryType = TrajectoryType::AAMissile;

			#if !defined ONE_COMPOUND_PER_FACTION && !defined NO_MUNITION
				component.baseStorageCapacity.level[Resource::Munitions] = 20;
			#endif
			component.initiative = 80;
			component.independentAttackAndMotion = false;
			component.requiresStationaryVehicle = true;
			component.attacksMovingHostilesDuringMotionPhase = true;
		}

	
		void			EMP::Configure(Turret&component)
		{
			#ifdef HAS_WINDOW		
				component.parts.SetSize(1);
				component.parts.First().LoadFromObj("objects/emp.obj");
			#endif


			component.typeName = "EMP";
			component.shortName = " EMP";
			component.constName = ConstName::Turret_EMP;

			#ifndef ONE_COMPOUND_PER_FACTION
				component.baseStorageCapacity.level[Resource::Uranium] = 150;
			#endif
		
			component.dealsEMPDamage = true;
			component.initiative = 30;
			component.ammoType = Resource::Uranium;
		}


		#ifndef NO_SATELLITES
			#ifdef HAS_WINDOW		
				namespace Ragnarok
				{
					namespace MyCmd
					{
						void		Obliterate()
						{
							ApplicationInterface::SignalTargetLessAttack(selectedEntity);
						}
					}
				}
			#endif

			void			Ragnarok::Configure(Turret&component)
			{
				#ifdef HAS_WINDOW		
					CellCoords offset = ConfigureStandardCommands(component);

					#ifndef ONE_COMPOUND_PER_FACTION
						component.commands.Add( new IconCommand("Withdraw Uranium",&CommonCmd::ToggleMode<CursorMode::RestockMode>, &CommonCmd::ModeCheck<CursorMode::RestockMode>,NoCosts));
						component.commands.Add( new IconCommand("Deposit Uranium",&CommonCmd::ToggleMode<CursorMode::DepositMode>, &CommonCmd::ModeCheck<CursorMode::DepositMode>,NoCosts));
					#endif

					component.commands.Add( new IconCommand("Obliterate",&MyCmd::Obliterate, &CommonCmd::CommonButtonCheck,NoCosts));

					component.parts.SetSize(1);
					component.parts.First().first.LoadFromObj("objects/ragnarökRing.obj");
				#endif


				component.typeName = "Ragnarök";
				component.shortName = " Ragnarök";
				component.constName = ConstName::Turret_Ragnarok;
				component.constTowerName = ConstName::Empty;	//not used

				component.mountType = Turret::MountType::Satellite;
				component.isDoomsday = true;

				component.continuousRotation = true;
				component.continuousRotationSpeed = -0.25f;


				component.productionCosts = 1;	//irrelevant
		
				component.weight = 1;	//irrelevant
				component.healthMultiplier = 1;	//irrelevant
				component.baseHorizontalAttackRange = 0;
				component.baseScanningRange = 1;

				component.baseExplosionRadius = 10;
				component.baseDamageLevel = 100;
				component.dealsEMPDamage = false;
				component.baseAttacksPerRound = 1;
				component.initiative = 10;
				#ifdef _DEBUG
					component.baseAmmoNeedPerShot = 0;//1000;
				#else
					component.baseAmmoNeedPerShot = 2000;
				#endif
				#ifndef ONE_COMPOUND_PER_FACTION
					component.baseStorageCapacity.level[Resource::Uranium] = component.baseAmmoNeedPerShot;
				#endif
				component.ammoType = Resource::Uranium;
				component.requiresStationaryVehicle = true;
				component.uplinkTraffic = 50;
				component.trajectoryType = TrajectoryType::VerticalBeam;


				Research::FactionState::RegisterDependency(component,emp);
				Research::FactionState::RegisterDependency(component,SkyPort::globalPrototype);
				Research::FactionState::RegisterUnlockCost(component,100);
				Research::FactionState::RegisterResearch(component,Attribute::DamageLevel);
				Research::FactionState::RegisterResearch(component,Attribute::ExplosionRadius);
				Research::FactionState::RegisterResearch(component,Attribute::ScanningRange);
				if (component.baseAmmoNeedPerShot > 0)
					Research::FactionState::RegisterResearch(component,Attribute::AmmoNeed);

				component.FinishCreation(false);
			}


	
			void			SkyGuardian::Configure(Turret&component)
			{
				#ifdef HAS_WINDOW		
					CellCoords offset = ConfigureStandardCommands(component);

					component.commands.Add( new IconCommand("Attack",&CommonCmd::ToggleMode<CursorMode::AttackMode>, &CommonCmd::ModeCheck<CursorMode::AttackMode>,NoCosts));

					component.parts.SetSize(1);
					component.parts.First().first.LoadFromObj("objects/skyGuardianRing.obj");
				#endif


				component.typeName = "Sky Guardian";
				component.shortName = " Guardian";
				component.constName = ConstName::Turret_ProtonCannon;
				component.constTowerName = ConstName::Empty;	//not used

				component.mountType = Turret::MountType::Satellite;

				component.productionCosts = 1;	//irrelevant

		
				component.weight = 1;	//irrelevant
				component.healthMultiplier = 1;	//irrelevant
				component.baseHorizontalAttackRange = 30;
				component.baseScanningRange = 10;

				component.baseExplosionRadius = 2;	//in space, 2 is 1
				component.baseDamageLevel = 5;
				component.dealsEMPDamage = false;
				component.baseAttacksPerRound = 1;
				component.initiative = 60;
				component.baseAmmoNeedPerShot = 0;//10;
				component.ammoType = Resource::Uranium;
				component.requiresStationaryVehicle = true;
				component.uplinkTraffic = 10;
				component.trajectoryType = TrajectoryType::Missile;


				Research::FactionState::RegisterDependency(component,emp);
				Research::FactionState::RegisterDependency(component,SkyPort::globalPrototype);
				Research::FactionState::RegisterUnlockCost(component,10);
				Research::FactionState::RegisterResearch(component,Attribute::AttackRange);
				Research::FactionState::RegisterResearch(component,Attribute::AttacksPerRound);
				Research::FactionState::RegisterResearch(component,Attribute::DamageLevel);
				Research::FactionState::RegisterResearch(component,Attribute::ExplosionRadius);
				Research::FactionState::RegisterResearch(component,Attribute::ScanningRange);
				if (component.baseAmmoNeedPerShot > 0)
					Research::FactionState::RegisterResearch(component,Attribute::AmmoNeed);

				component.FinishCreation(false);
			}

			void			SkyHubTurret::Configure(Turret&component)
			{
				#ifdef HAS_WINDOW		
					CellCoords offset = ConfigureStandardCommands(component);
					offset++;//remain on lower level
					component.commands.Add( new IconCommand("Repair",&CommonCmd::ToggleMode<CursorMode::RepairMode>, &CommonCmd::ModeCheck<CursorMode::RepairMode>,NoCosts));

					component.parts.SetSize(1);
					component.parts.First().first.LoadFromObj("objects/skyHubTurret.obj");
			
				#endif


				component.typeName = "Sky Hub";
				component.shortName = " Hub";
				component.constName = ConstName::Turret_SkyHub;
				component.constTowerName = ConstName::Empty;	//not used

				component.mountType = Turret::MountType::Satellite;
				component.baseOperationSpeed[Repair] = 10;
				component.baseOperationSpeed[Construct] = 5;
				//component.fuelSupplyRadius = 20;

				component.continuousRotation = true;
				component.continuousRotationSpeed = 0.25f;

				component.productionCosts = 1;	//irrelevant
		
				component.weight = 1;	//irrelevant
				component.healthMultiplier = 1;	//irrelevant
				component.baseScanningRange = 35;	//temporary. Once orbital scanners are introduced, reduce this value again
				component.baseGroundScanningRange = 10;

				component.baseAttacksPerRound = 0;
				component.initiative = 60;
				component.uplinkTraffic = 20;

				Research::FactionState::RegisterResearch(component,Attribute::ConstructionSpeed);
				Research::FactionState::RegisterResearch(component,Attribute::ScanningRange);
				if (component.baseAmmoNeedPerShot > 0)
					Research::FactionState::RegisterResearch(component,Attribute::AmmoNeed);

				component.FinishCreation(false);
			}

			void			StarPort::Configure(Turret&component)
			{
				#ifdef HAS_WINDOW		
					CellCoords offset = ConfigureStandardCommands(component);
					offset++;//remain on lower level
					component.commands.Add( new IconCommand("Repair",&CommonCmd::ToggleMode<CursorMode::RepairMode>, &CommonCmd::ModeCheck<CursorMode::RepairMode>,NoCosts));

					component.parts.SetSize(1);
					component.parts.First().first.LoadFromObj("objects/starPortRing.obj");
			
				#endif


				component.typeName = "Star Port";
				component.shortName = " Port";
				component.constName = ConstName::Turret_StarPort;
				component.constTowerName = ConstName::Empty;	//not used

				component.mountType = Turret::MountType::Satellite;

				component.continuousRotation = true;
				component.continuousRotationSpeed = -0.05f;

				component.productionCosts = 1;	//irrelevant
				component.baseOperationSpeed[Repair] = 10;
		
				component.weight = 1;	//irrelevant
				component.healthMultiplier = 1;	//irrelevant
				component.baseScanningRange = 10;
				component.baseGroundScanningRange = 0;

				component.baseAttacksPerRound = 0;
				component.initiative = 30;
				component.uplinkTraffic = 1;	//irrelevant

				Research::FactionState::RegisterResearch(component,Attribute::ScanningRange);
				if (component.baseAmmoNeedPerShot > 0)
					Research::FactionState::RegisterResearch(component,Attribute::AmmoNeed);

				component.FinishCreation(false);
			}

		#endif

		void			MunitionTransporter::Configure(Turret&component)
		{
			#ifdef HAS_WINDOW		

				component.parts.SetSize(1);
				component.parts.First().LoadFromObj("objects/munitionTransporter.obj");
			#endif


			component.typeName = "Munition Transporter";
			component.shortName = " Ammo Truck";
			component.constName = ConstName::Turret_MunitionTransporter;

			#if !defined ONE_COMPOUND_PER_FACTION && !defined NO_MUNITION
				component.baseStorageCapacity.level[Resource::Munitions] = 100;
			#endif
		}

		void			Mechanic::Configure(Turret&component)
		{
			#ifdef HAS_WINDOW		
				component.parts.SetSize(1);
				component.parts.First().LoadFromObj("objects/constructor.obj");
			#endif


			component.typeName = "Mechanic";
			component.shortName = "mechanic";
			component.constName = ConstName::Turret_Mechanic;

			#ifndef ONE_COMPOUND_PER_FACTION
				component.baseStorageCapacity.level[Resource::Minerals] = 100;
			#endif
		}


		void			Radar::Configure(Turret&component)
		{
			#ifdef HAS_WINDOW		
				component.parts.SetSize(1);
				component.parts.First().LoadFromObj("objects/radar.obj");
			#endif


			component.typeName = "Radar";
			component.shortName = "radar";
			component.constName = ConstName::Turret_Radar;

			component.isScanner = true;
			component.continuousRotation = true;
			component.continuousRotationSpeed = 2.f;
		}

	
	
		void			ResourceScanner::Configure(Turret&component)
		{
			#ifdef HAS_WINDOW		
				component.parts.SetSize(1);
				component.parts.First().LoadFromObj("objects/resourceScanner.obj");
			#endif


			component.typeName = "Surveyor";
			component.shortName = "surveyor";
			component.constName = ConstName::Turret_Surveyor;

			component.isScanner = true;
			component.continuousRotation = true;
			component.continuousRotationSpeed = -1.f;

			component.detectsResources = true;
		}



		namespace SubOrbital
		{
			namespace Base
			{
				void	Configure(VehicleBase&component)
				{
					#ifdef HAS_WINDOW
						component.ambientSound = Engine::OpenAL::Load("resources/objectSounds/plane.wav");
					#endif
					component.shortName = "Airborne";
					component.constName = ConstName::VehicleBase_Plane;

					component.carryingCapacity.SetInvariant(100);
					component.requirements = VehicleBase::Requirement::AnyBlock;
					component.moveOnWater = true;
					component.minClimbDown = -1;	//-int(Grid::CellStack::Range);
					component.operationSpeedMultiplier.SetInvariant(1);
					component.turretRangeModifier.SetInvariant(0);
					#ifndef ONE_COMPOUND_PER_FACTION
						component.storageMultiplier = 1;
					#endif
				}
			}


			void			Raptor::Configure(Turret&component)
			{
			

				component.typeName = "Raptor";
				component.shortName = " Raptor";
				component.constName = ConstName::Type_Raptor;	//let's see if i can use the same name here

				component.mountType = Turret::MountType::Plane;

		
				component.dealsEMPDamage = false;
				component.initiative = 100;
				component.ammoType = Resource::Uranium;
				component.requiresStationaryVehicle = false;
				component.independentAttackAndMotion = true;
				component.trajectoryType = TrajectoryType::Missile;

			}

			void			Bomber::Configure(Turret&component)
			{
			

				component.typeName = "Bomber";
				component.shortName = " Bomber";
				component.constName = ConstName::Type_Bomber;

				component.mountType = Turret::MountType::Plane;

		
				component.dealsEMPDamage = false;
				component.initiative = 90;
				component.ammoType = Resource::Uranium;
				component.requiresStationaryVehicle = false;
				component.independentAttackAndMotion = true;
				component.trajectoryType = TrajectoryType::VerticalBeam;

			}

			void			SensorDrone::Configure(Turret&component)
			{
				component.typeName = "Sensor Drone";
				component.shortName = " Sensor";
				component.constName = ConstName::Type_SensorDrone;

				component.mountType = Turret::MountType::Plane;
				component.initiative = 110;
			}




			void			CargoPlane::Configure(Turret&component)
			{
				component.typeName = "Cargo Plane";
				component.shortName = " Cargo";
				component.constName = ConstName::Type_CargoPlane;	//let's see if i can use the same name here

				component.mountType = Turret::MountType::Plane;
				component.initiative = 60;
			}


		}


		void			Hoverbase::Configure(VehicleBase&component)
		{
		

			static const float staticHeight = 0.4743f;

			#ifdef HAS_WINDOW		
				component.parts.SetSize(1);

				component.parts.First().LoadFromObj("objects/hoverbase.obj");
				component.height = component.parts.First().GetBoundingBox().z.max;
				ASSERT2__(M::Similar(staticHeight,component.height),staticHeight,component.height);

				component.ambientSound = Engine::OpenAL::Load("resources/objectSounds/hoverbase.wav");

			#else
				component.height = staticHeight;
			#endif

			//MakeBox(component.parts.First().first, M::Box<>(-0.5f, -1.f, 0.f, 0.5f, 1.f, 0.35f));
			component.typeName = "Hoverbase";
			component.shortName = "Hover";
			component.constName = ConstName::VehicleBase_HoverBase;

			//component.productionCosts = 40;
			component.requirements = VehicleBase::Requirement::AnyBlock;
			component.moveOnWater = true;
			component.minClimbDown = -5;	//-int(Grid::CellStack::Range);
			#ifndef ONE_COMPOUND_PER_FACTION
				component.storageMultiplier = 1;
			#endif

			//component.unmodulatedMovementSpeed = 20;
			//component.baseCarryingCapacity = 1;
			//component.unmodulatedHealth = 4;

			component.acceleration[TerrainAdd] = 1;
			component.acceleration[TerrainRemove] = 1;

			component.height *= vehicleScale;
		}

		void			Heavybase::Configure(VehicleBase&component)
		{
		
			static const float staticHeight = 0.4743f;

			#ifdef HAS_WINDOW		
				component.parts.SetSize(1);

				component.parts.First().LoadFromObj("objects/heavyBase.obj");
				component.height = component.parts.First().GetBoundingBox().z.max;
				ASSERT2__(M::Similar(staticHeight,component.height),staticHeight,component.height);

				component.ambientSound = Engine::OpenAL::Load("resources/objectSounds/heavybase.wav");
			#else
				component.height = staticHeight;
			#endif


			//MakeBox(component.parts.First().first, M::Box<>(-0.5f, -1.f, 0.f, 0.5f, 1.f, 0.35f));
			component.typeName = "Heavy Base";
			component.shortName = "Heavy";
			component.constName = ConstName::VehicleBase_HeavyBase;

			//component.productionCosts = 80;
			component.requirements = VehicleBase::Requirement::AnyBlock;	//none is broken...
			component.moveOnWater = false;
			component.minClimbDown = -2;
			#ifndef ONE_COMPOUND_PER_FACTION
				component.storageMultiplier = 2;
			#endif
			component.hindrance.road = 1;
			component.hindrance.air = 3;
			component.hindrance.water = 4;
			component.hindrance.other = 4;

			component.acceleration[TerrainAdd] = 2;
			component.acceleration[TerrainRemove] = 2;

			component.height *= vehicleScale;
		}

		void			Catbase::Configure(VehicleBase&component)
		{
		
			static const float staticHeight = 0.4743f;

			#ifdef HAS_WINDOW		
				component.parts.SetSize(1);

				component.parts.First().LoadFromObj("objects/catBase.obj");
				component.height = component.parts.First().GetBoundingBox().z.max;
				ASSERT2__(M::Similar(staticHeight,component.height),staticHeight,component.height);

				component.ambientSound = Engine::OpenAL::Load("resources/objectSounds/catbase.wav");
			#else
				component.height = staticHeight;
			#endif


			//MakeBox(component.parts.First().first, M::Box<>(-0.5f, -1.f, 0.f, 0.5f, 1.f, 0.35f));
			component.typeName = "Catamaran Base";
			component.shortName = "Cat";
			component.constName = ConstName::VehicleBase_CatBase;

			//component.productionCosts = 100;
			component.requirements = VehicleBase::Requirement::Water;	//none is broken...
			component.moveOnWater = true;
			component.minClimbDown = -2;
			#ifndef ONE_COMPOUND_PER_FACTION
				component.storageMultiplier = 2;
			#endif
			//component.operationSpeedMultiplier = 2;
			//component.rangeModifier = 1;

			//component.unmodulatedMovementSpeed = 8;
			//component.baseCarryingCapacity = 15;
			//component.unmodulatedHealth = 12;

			component.hindrance.road = 10;
			component.hindrance.air = 2;
			component.hindrance.water = 1;
			component.hindrance.other = 10;

			component.acceleration[TerrainAdd] = 2;
			component.acceleration[TerrainRemove] = 2;

			component.height *= vehicleScale;
		}




		#ifdef HAS_WINDOW
		
			void			Base::DescribeType(Command::Base::Label&label, bool compound, const OwnerParameter&owner)
			{
				if (!compound)
				{
					label <<"Cost: "+String(ResourceTag(Resource::Minerals))+productionCosts.Describe(Objects::Component::ResearchStateOf(owner));
					label <<"Initiative: "+String(initiative);
				}
			}



			void VehicleBase::DescribeType(Command::Base::Label&outResult, bool compound, bool includeName, const OwnerParameter&owner)
			{
				outResult.BeginBrief();
				const auto& researchState = ResearchStateOf(owner);
				if (includeName && typeName.IsNotEmpty())
					outResult << typeName + " (Base)";
				Base::DescribeType(outResult,compound,owner);
				outResult.BeginBrief();
				if (movementSpeed.GetValue(researchState) > 0)
					outResult << "Can climb "+String(maxClimbUp)+" height level"+(maxClimbUp==1?"":"s")+" up";
			
				outResult << "Can safely fall down "+String(-minClimbDown)+" height level"+(minClimbDown==-1?"":"s");
				#ifndef ONE_COMPOUND_PER_FACTION
					if (storageMultiplier != 1)
						outResult <<"Storage capacity multiplied by "+String(storageMultiplier);
				#endif
				if (!compound)
				{
					float mult = operationSpeedMultiplier.GetValue(researchState);
					if (mult != 1)
						outResult << "Operation speed multiplied by "+operationSpeedMultiplier.Describe(researchState);

					outResult <<"Carrying capacity: "+carryingCapacity.Describe(researchState);
					outResult<<"Base speed: "+ movementSpeed.Describe(researchState);
					if (requirements == Requirement::Water)
						outResult.GetLastLine()+= " (naval)";
					else
						if (moveOnWater)
							outResult.GetLastLine() += " (supported by water)";
					outResult<<"Base health: "+health.Describe(researchState);
					float rangeMod = turretRangeModifier.GetValue(researchState);
					if (rangeMod != 0)
						outResult<<"Turret range modifier: "+turretRangeModifier.Describe(researchState);
				}
				if (ignoresVehicleAttacks)
					outResult<<"Can move independently of attack action";
			}




			void Turret::DescribeType(Command::Base::Label&outResult, bool compound, bool includeName, const String&parentType, const OwnerParameter&owner)
			{
				outResult.BeginBrief();
				const String upperName = parentType.IsNotEmpty() ? parentType+"-Turret" : "Turret";
				const String lowerName = upperName.CopyToLowerCase();
				if (includeName)
				{
					const String nameAppend = isDoomsday ? ", DOOMSDAY" : "";
					outResult << typeName+" ("+upperName+nameAppend+")";
				}
				else
					if (isDoomsday)
						outResult << "DOOMSDAY";

				if (!compound && RuntimeConfig::UnitLimits::AnySet())
				{
					count_t cnt = 0, limit = 0;
					cnt = Vehicle::CountTurretOccurrences(owner,this)
						#ifndef NO_SATELLITES
						+
						Satellite::CountTurretOccurrences(owner,this)
						#endif
						;
					limit = RuntimeConfig::UnitLimits::Get(constName);
					if (limit != 0xFFFF)
						outResult <<"Deployed: "+String(cnt)+"/"+String(limit);
				}

				const auto& researchState = ResearchStateOf(owner);

				Base::DescribeType(outResult,compound,owner);
				outResult.BeginBrief();
				if (!compound)
				{
					outResult <<"Weight: "+weight.Describe(researchState);
					outResult <<"Base scanning range: "+scanningRange.Describe(researchState);
					if (subOrbitalScanningRange.IsFunctionOrConstNonZero())
						outResult <<"Sub-orbital scanning range: "+subOrbitalScanningRange.Describe(researchState);
					float healthMult = healthMultiplier.GetValue(researchState);
					if (healthMult != 1)
						outResult <<"Health: x"+healthMultiplier.Describe(researchState);
					//if (RuntimeConfig::hibernationSystemEnabled)
					//{
					//	outResult <<""+String(TrafficResourceTag())+String(uplinkTraffic)+", Priority: "+String(uplinkPriority);
					//}
				}
				if (CanAttack())
				{
					if (!compound)
						outResult <<"Offensive "+lowerName;
					if (parentType != "Tower")
					{
						if (independentAttackAndMotion)
							outResult <<"Can attack independently of movement action";
						elif (requiresStationaryVehicle)
							outResult <<"Vehicle must remain stationary to fire";
					}
					if (!compound)
						outResult <<"Base horizontal attack range: "+horizontalAttackRange.Describe(researchState)+" ("+trajectoryType.ToString()+")";

					int need = ammoNeedPerShot.GetValue(researchState);
					if (need == 0)
						outResult <<"Does not require ammunition";
					else
						outResult <<"Consumes "+String(ResourceTag(ammoType))+ammoNeedPerShot.Describe(researchState)+" per shot";
					int count = attacksPerRound.GetValue(researchState);
					outResult <<""+attacksPerRound.Describe(researchState)+" attack"+(count == 1 ? "" : "s")+" per round";
				}
				if (explosionRadius.IsFunctionOrConstNonZero())
				{
					outResult <<"Explosion radius: "+explosionRadius.Describe(researchState);
					if (this == &bomb)
						outResult.GetLastLine() +=" ("+String(ResourceTag(Resource::Uranium))+": *2)";
						//bomb.GetExplosionRadius(owner) : artillery.GetExplosionRadius(owner)*2;
				}
				if (damageLevel.IsFunctionOrConstNonZero())
				{
					if (dealsEMPDamage)
						outResult <<"EMP damage: "+damageLevel.Describe(researchState);
					else
						outResult <<"Damage: "+damageLevel.Describe(researchState);
					if (this == &bomb)
						outResult.GetLastLine() +=" ("+String(ResourceTag(Resource::Uranium))+": *5)";
				}

				if (CanConstruct() && !compound)
					outResult <<"Construction: Converts "+String(ResourceTag(Resource::Minerals))+operationSpeed[Construct].Describe(researchState)+" per round";
				if ((CanDo(TerrainRemove) || CanDo(TerrainAdd)) && !compound)
					outResult <<"Terraforming: Can shape the surrounding terrain";
				if (CanRepair())
					outResult <<"Can repair other vehicles/structures";
				if (detectsResources)
					outResult <<"Scans into the ground, and detects natural resources";
				#ifndef ONE_COMPOUND_PER_FACTION
					if (!compound && baseStorageCapacity.IsNotEmpty())
					{
						outResult <<"Base storage capacity:";
						for (index_t i = 0; i < Resource::NumberOfPossibilities; i++)
						{
							if (baseStorageCapacity.level[i])
							{
								outResult <<" "+String(ResourceTag(i))+String(baseStorageCapacity.level[i]);
							}
						}
					}
				#endif
				outResult.MarkBriefEnd();
				if (CanConstruct())
					outResult << ("This "+lowerName+" type can build structures in valid locations. It requires "+String(ResourceTag(Resource::Minerals))+" to initiate construction.");
				if (CanRepair())
					outResult << ("This "+lowerName+" type can repair other structures and vehicles. It requires "+String(ResourceTag(Resource::Minerals))+" to initiate repair.");
				if (detectsResources)
					outResult << ("This "+lowerName+" type scans deep into the ground, and detects all naturally occuring resources within its scanning range. It can be automated if mobile.");
				if (CanAttack())
				{
					if (trajectoryType == TrajectoryType::AAMissile)
					{
						if (subOrbitalScanningRange.IsFunctionOrConstNonZero())
							outResult << ("This "+lowerName+" type can detect and attack sub-orbital planes in flight.");
						else
							outResult << ("This "+lowerName+" type can attack sub-orbital planes in flight.");
					}
					else
						outResult << ("This "+lowerName+" type can attack objects in range.");
					if (dealsEMPDamage)
						outResult << ("This "+lowerName+" deals EMP damage. EMP damage temporarily reduces attack and scanning range, movement speed, and disallows construction operations. Does not cause any permanent damage.");
				}
				else
				{
					if (subOrbitalScanningRange.IsFunctionOrConstNonZero())
						outResult << ("This "+lowerName+" type can detect sub-orbital planes in flight.");
					if (isScanner)
						outResult << ("The "+this->typeName + " "+lowerName+" is especially suited for detecting hostile entities. Such turret types are manadatory in order to see hostile forces approaching, as well as their attacks. Offensive turrets usually lack the necessary scanning range to see where they are shooting and depend on this turret type to reside in the vicinity.");
				}

				if (attacksMovingHostilesDuringMotionPhase)
				{
					if (mountType != MountType::TowerOnly)
						outResult << ("The "+this->typeName + " "+lowerName+" intercepts hostile mobile units during their movement phase even if itself mounted on a mobile unit. It is thus particularly well suited to establish an adhoc defense perimeter, or to persistently defend strategically important regions if mounted on a tower.");
					else
						outResult << ("The "+this->typeName + " "+lowerName+" intercepts hostile mobile units during their movement phase. It is thus particularly well suited to defend strategically important regions.");
				}

				if (this == &bomb)
					outResult << "This "+lowerName+" type can self-detonate in a large explosion. The vehicle is lost in the process. The explosion can be caused explicitly or by sufficient external damage."
					"Bombs are very powerful and can trigger the explosion of neighboring bombs. An optional "+String(ResourceTag(Resource::Uranium))+"-driven DOOMSDAY explosion provides an extended explosion radius but will notify hostile factions of the explosion.";
				//if (isDoomsday)
				//{
				//	outResult.WrapAppendText("This "+lowerName+" type is classified as Doomsday weaponry. Careful with this one.");
				//}
			}

	
			//void					Terraformer::Cmd::BuildWall( const PWorldEntity&v)
			//{
			//	if (v->location.height <= -Grid::CellStack::HalfRange)
			//		return;
			//	PVehicle vehicle = To<Vehicle>::DynamicCast(v);
			//	if (!vehicle)
			//		return;
			//	SignalNewJob(vehicle, ConstName::Job_BuildWall,ConstName::Empty);
			//	//Vehicle::PJob job(new Job::BuildFoundation(*vehicle));
			//	//vehicle->RegisterNewJob("", job,nullptr);
			//}





		#endif


		/*static*/ bool					VehicleBase::canStandOnUnknown=true;

		bool							VehicleBase::CanStandOn(const World::Data&world, const Grid::LCellIdI&cell, bool ignoreWater, bool doFloat)
		{
			if (requirements == Requirement::None)
				return true;

			Grid::Content weakest,strongest;
			bool anyUnknown = world.GetStrengthRange(cell,weakest,strongest,canStandOnUnknown);
			if (strongest == Grid::Content::Unknown && !canStandOnUnknown)
				return false;
			//if (!canStandOnUnknown && anyUnknown)
			//	return false;

	//								moveOnWater

			//if (t == Grid::Content::Clear)
				//return false;
			const int waterStrength = Grid::CellStack::Cell::typeStrength[Grid::Content::Water];
			const int strongestStr = Grid::CellStack::Cell::typeStrength[strongest];

			switch (requirements)
			{
				case Requirement::AnyBlock:
				{
					if (strongestStr > waterStrength)
						return true;
					if (strongestStr == waterStrength)
						return moveOnWater || ignoreWater;
					//if (strongest == Grid::Content::Clear)
					//	return doFloat;
					return doFloat;	//air or lava
				}
				case Requirement::Water:
					if (anyUnknown && !canStandOnUnknown)
						return false;
					//if (strongestStr > waterStrength)
					//	return false;
					//if (strongest == Grid::Content::Lava)
					//	return false;
					//if (strongest == Grid::Content::Clear)
					//	return doFloat;
					return strongest == Grid::Content::Water || doFloat;
				break;
				//case Requirement::CentralBlock:
				//{
				//	BYTE t = world.cellStack[Grid::low.grid.GetCell(cell.cellID).higherEquivalent].GetCellI(cell.height).content;
				//	return t != Grid::Content::Clear && (moveOnWater || t != Grid::Content::Water);
				//}
				/*case Requirement::FullBase:
				{
					return weakest != Grid::Content::Clear && (moveOnWater || weakest != Grid::Content::Water);
				}
				case Requirement::RoadOrFoundation:
					return weakest == Grid::Content::Foundation || weakest == Grid::Content::Road;*/
			}
			FATAL__("Unknown requirement "+String(requirements.ToString()));
			return false;
	
		}

		bool							Base::TestIntersection(float zOffset, float x, float y, const M::TVec3<>&_b, const M::TVec3<>&_d, float & distance)
		{
			#ifdef HAS_WINDOW
				bool rs = false;
				M::TMatrix4<> system1,system2,inverse;
				M::TVec3<> b,d;
				if (parts.Count() > 0)
				{
					GetSubSystem(0,x,system1);
					//system1.w.xyz += system1.z.xyz * zOffset;
					system1.w.z = zOffset;
					Mat::invertSystem(system1,inverse);
					Mat::Transform(inverse,_b,b);
					Mat::rotate(inverse,_d,d);
					rs |= parts.First().TestIntersection(b,d,distance);
				}
				if (parts.Count() > 1)
				{
					GetSubSystem(1,y,system2);
					Mat::transformSystem(system1,system2);
					Mat::invertSystem(system2,inverse);
					Mat::Transform(inverse,_b,b);
					Mat::rotate(inverse,_d,d);
					rs |= parts[1].TestIntersection(b,d,distance);
				}
				return rs;
			#else
				return false;
			#endif
	
		}


		void							Base::GetSubSystem(index_t geometryIndex, float angle, M::TMatrix4<>&outSystem)	const
		{
			Mat::Eye(outSystem);
			if (angle == 0)
				return;
			const float cs = cos(angle);
			const float sn = sin(angle);
			switch (geometryIndex)
			{
				case 0:
					outSystem.x.x = cs;
					outSystem.x.y = sn;
					outSystem.y.x = -sn;
					outSystem.y.y = cs;
				break;
				case 1:
					outSystem.y.y = cs;
					outSystem.y.z = sn;
					outSystem.z.y = -sn;
					outSystem.z.z = cs;
				break;		
			}
		}




	}
}
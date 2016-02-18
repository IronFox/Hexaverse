#include <global_root.h>
#include "components.h"
#include "systems.h"
#include "vehicle.h"
#include "structure.h"
#include "../cast.h"
#include "jobs.h"
#include "prototypes.h"
#include "interface.h"
#include "visualSelection.h"
#include "visualChoices.h"
#include "rendering.h"
#include "satellite.h"
#include "gridAndChoices.h"
#include "../runtimeConfig.h"

namespace Objects
{
	namespace Component
	{
		Turret			terraformer,mechanic,
						artillery,launcher,emp,armored,
						munitionTransporter,
						radar,resourceScanner,
						protonCannon,skyHubTurret,ragnarok,starPort;


		VehicleBase		hoverbase,
						heavybase,
						catbase;








		void		Turret::BuildBlastSystem(const TVec3<>&location, TMatrix4<>&system) const
		{
			BuildSystem(location,explosionRadius,explosionRadius,system);
		}


		void		Turret::BuildBlastSystem(const Grid::LCellIdI&location, TMatrix4<>&system)	const
		{
			BuildSystem(location,explosionRadius,explosionRadius,system);
		}



		#ifdef HAS_WINDOW
			/*static*/ float			Turret::CompileTrajectory(const Buffer0<TVec4<> >&v, geometryType_t type, Geometry&geo, float widthScale/*=1.f*/)
			{
				bool schematic = type == Schematic;
				bool smoke = type == SmokeTrail;
				if (v.IsNotEmpty())
				{
					TVec3<> n;
					float3 front = v.first().xyz * v.first().w;
					float3 back = v.last().xyz * v.last().w;
					Vec::cross(front,back-front,n);
					Vec::normalize(n);
					n*= 0.5f * widthScale;
					if (!smoke)
						n*= v.first().w / Grid::VerticalBase;;
					float d = 0;
					geo.SetVertexOffsetToCurrent();
					TVec3<> lastB;
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
							const TVec4<>&_b = v[i];
							TVec3<>b = _b.xyz * _b.w;


							if (i)
								d += Vec::distance(lastB,b);
							lastB = b;
						}
						lengthScale = 1.f / d;
						d = 0.f;
					}



					for (index_t i = 0; i < v.Count(); i++)
					{
						const TVec4<>&_b = v[i];
						TVec3<>b = _b.xyz * _b.w;
						TVec3<> upNormal;
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
								const TVec4<>&_b2 = v[1];
								TVec3<>b2 = _b2.xyz * _b2.w;

								Vec::cross(n,b2-b,upNormal);
								Vec::normalize(upNormal);
							}
						}
						float oo = outerOpacity;
						float io = innerOpacity;
						if (schematic)
						{
							float fc = 1.f - sqr((float(i) - (v.Count()/2)) / float(v.Count()/2));
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


		///*static*/ bool						Turret::CanHit(float flatRange, const TVec3<>&origin, const TVec3<>&dest)
		//{
		//	float hDest = Vec::length(dest);
		//	float hOrigin = Vec::length(origin);
		//	return CanHit(flatRange, origin /hOrigin,hOrigin,dest/hDest,hDest);
		//}

		/*static*/ bool						Turret::CanHit(TrajectoryType t, float flatRange, const Grid::LCellIdI&origin, float zOffset, const TVec3<>&dest)
		{
			if (t == TrajectoryType::VerticalBeam)
			{
				return Satellite::GetGroundHeight(origin) != badHeight;
			}
			float hDest = Vec::length(dest);
			return CanHit(	t, flatRange,
							Grid::low.cells[origin.stackID].centerVertex.xyz,Grid::ToRadius(origin.height) + zOffset,
							dest / hDest, hDest
							);
		}
	
		///*static*/ bool						Turret::CanHit(float flatRange, const TVec3<>&origin, const Grid::LCellIdI&dest)
		//{
		//	float hOrigin = Vec::length(origin);
		//	return CanHit(	flatRange,
		//					origin / hOrigin, hOrigin,
		//					Grid::low.cells[dest.stackID].centerVertex.xyz,Grid::ToRadius(dest.height)
		//					);
		//}

		/*static*/ bool						Turret::CanHit(TrajectoryType t, float flatRange, const Grid::LCellIdI&origin, float zOffset,const Grid::LCellIdI&dest)
		{
			if (t == TrajectoryType::VerticalBeam)
			{
				return Satellite::GetGroundHeight(origin) != badHeight;
			}
			return CanHit(	t, flatRange,
							Grid::low.cells[origin.stackID].centerVertex.xyz,Grid::ToRadius(origin.height) + zOffset,
							Grid::low.cells[dest.stackID].centerVertex.xyz,Grid::ToRadius(dest.height)
							);
		}

		/*static*/ bool						Turret::CanHit(TrajectoryType t, float flatRange, const TVec3<>&originDir, float originH, const TVec3<>&destDir, float destH)
		{
			switch (t)
			{
				case TrajectoryType::Artillery:
				{
					float3 p0 = originDir * originH,
							p1 = destDir * originH;
					return Detail::ArtilleryCanHit(flatRange,Vec::distance(p0,p1),destH - originH);
				}
				case TrajectoryType::Missile:
				{
					return Detail::MissileCanHit(flatRange,originDir,originDir*originH,destDir*destH);
				}
				default:
					FATAL__("Incompatible trajectory type "+String(t.ToString()));
			}
			return false;
		}

		/*static*/ bool						Turret::Detail::ArtilleryCanHit(float flatRange, float horizontalDistance, float verticalDistance)
		{
			return verticalDistance <= Turret::MaxHeightAtHDistance(flatRange,horizontalDistance);
		}

		/*static*/ bool						Turret::Detail::MissileCanHit(float flatRange, const TVec3<>&originNormal, const TVec3<>&origin, const TVec3<>&destination)
		{
			return sqr(flatRange) >= Vec::quadraticDistance(origin,destination);
		}



		/*static*/ Turret::TAngles					Turret::GetAngleForDistance(float v, const TVec3<>&originDir, float originH, const TVec3<>&destDir, float destH)
		{
			float3 p0 = originDir * originH,
					p1 = destDir * originH;
			return GetAngleForDistance(v,Vec::distance(p0,p1),destH - originH);
		}


		/*static*/ Turret::TAngles					Turret::GetAngleForDistance(float v, const Grid::LCellIdI&origin, float zOffset, const TVec3<>&dest)
		{
			float hDest = Vec::length(dest);
			return GetAngleForDistance(	v,
										Grid::low.cells[origin.stackID].centerVertex.xyz,Grid::ToRadius(origin.height) + zOffset,
										dest / hDest, hDest
										);
		}

		/*static*/ Turret::TAngles					Turret::GetAngleForDistance(float v, const Grid::LCellIdI&origin, float zOffset, const Grid::LCellIdI&dest)
		{
		//	float hDest = Vec::length(dest);
			return GetAngleForDistance(	v,
										Grid::low.cells[origin.stackID].centerVertex.xyz,Grid::ToRadius(origin.height) + zOffset,
										Grid::low.cells[dest.stackID].centerVertex.xyz,Grid::ToRadius(dest.height)
										);
		}

		/*static*/ Turret::TAngles					Turret::GetAngleForDistance(float v, const TVec3<>&origin, const TVec3<>&dest)
		{
			float hDest = Vec::length(dest);
			float hOrigin = Vec::length(origin);
			return GetAngleForDistance(	v,
										origin / hOrigin, hOrigin,
										dest / hDest, hDest
										);
		}


		/*static*/ Turret::TAngles					Turret::GetAngleForDistance(float v, const TVec3<>&origin, const Grid::LCellIdI&dest)
		{
			float hOrigin = Vec::length(origin);
			return GetAngleForDistance(	v,
										origin / hOrigin, hOrigin,
										Grid::low.cells[dest.stackID].centerVertex.xyz,Grid::ToRadius(dest.height)
										);
		}

		/*static*/ void							Turret::TraceTrajectory(TrajectoryType t, float flatRange, const TVec3<>&source, const TVec3<>&dest, Buffer0<TVec4<> >&outSteps)
		{
			outSteps.Clear();

			switch (t)
			{
				case TrajectoryType::Artillery:
					Detail::TraceArtilleryTrajectory(Turret::MaxRangeToVelocity(flatRange),source, dest, outSteps);
				break;
				case TrajectoryType::Missile:
					Detail::TraceMissileTrajectory(flatRange,source, dest, outSteps);
				break;
				case TrajectoryType::VerticalBeam:
				{
					UINT32 stackIndex = Grid::low.Locate(dest);
					int h = Satellite::GetGroundHeight(Grid::LCellIdI(stackIndex,0));
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

		/*static*/ void							Turret::Detail::TraceMissileTrajectory(float range, const TVec3<>&source, const TVec3<>&dest, Buffer0<TVec4<> >&outSteps)
		{
			float d2 = Vec::quadraticDistance(source,dest);
			if (d2 > sqr(range))
			{
				LogEvent(EventClass::Preset::error,"TraceMissileTrajectory(): Distance to target ("+String(sqrt(d2))+") exceeds maximum range ("+String(range)+") of missile turret. Ignoring for now.");
				//DBG_FATAL__("Trajectory out of range: "+String(sqrt(d2))+">="+String(range));
				//return;
			}
		

			const float h0 = Vec::length(source);
			const float3 here = source;
			const float h1 = Vec::length(dest);
			const float3 there = dest;

			const float3 up = (here + there).Normalized0();
			float distance = Vec::distance(here,there);


			const float h = h1 - h0;

		

			const int resolution = std::max<int>(2,distance*2);

			float lastH = h0;
			TVec3<> lastB = here;
			for (int i = 0; i <= resolution; i++)
			{
				float t01 = float(i) / float(resolution);
			
				float fy = h0 + (t01 * h);
				TVec3<> b;
				Vec::interpolate(here,there,t01,b);
				Vec::normalize(b);
				outSteps << float4(b,fy);

			}
		}


		/*static*/ void							Turret::Detail::TraceArtilleryTrajectory(float velocity, const TVec3<>&source, const TVec3<>&dest, Buffer0<TVec4<> >&outSteps)
		{
			Turret::TAngles angles = Turret::GetAngleForDistance(velocity,source,dest);
			if (angles.numAngles == 0)
			{
				if (velocity < 50.f)
				{
					LogEvent(EventClass::Preset::error,"Unable to determine feasible angles for artillery trajectory at v="+String(velocity)+", source="+Vec::toString(source)+", dest="+Vec::toString(dest)+". Increasing velocity and retrying.");
					velocity = velocity > 0 ? velocity * 1.1f : 1.f;	//should not be zero, but it's better to handle this case here anyway. Otherwise we're causing a stack overflow in no-time
					TraceArtilleryTrajectory(velocity,source,dest,outSteps);
					return;
				}

				DBG_FATAL__("Unable to determine feasible angles for artillery trajectory at v="+String(velocity)+", source="+Vec::toString(source)+", dest="+Vec::toString(dest));
				return;
			}
		
			float angle = angles.idealAngle[angles.numAngles-1];

			const float sn = sin(angle),
						cs = cos(angle);

			const float h0 = Vec::length(source);
			const float3 here = source;
			const float h1 = Vec::length(dest);
			const float3 there = dest;

			const float3 up = (here + there).Normalized0();
			float distance = Vec::distance(here,there);


			const float h = h1 - h0;

			const float tEnd = (velocity * sn + sqrt(sqr(velocity*sn) - 2.f * g * h)) / g;


			const int resolution = std::max<int>(2,distance*4);

			float lastH = h0;
			TVec3<> lastB = here;
			for (int i = 0; i <= resolution; i++)
			{
				float t01 = float(i) / float(resolution);
				const float t = t01 * tEnd;
				float fy = h0 + (velocity * t*sn - 0.5f * g * sqr(t));
				TVec3<> b;
				Vec::interpolate(here,there,t01,b);
				Vec::normalize(b);
				outSteps << float4(b,fy);

			}
		}
		/*static*/ TurretTrajectory			Turret::TraceTrajectory(TrajectoryType t, float flatRange, const Grid::WorldData&world, const Grid::LCellIdI&source, float zOffset, const Grid::LCellIdI&dest, Buffer0<TVec4<> >&outSteps, TVec3<>&impactAt, Grid::HCellIdI&impactCell)
		{
			outSteps.Clear();

			switch (t)
			{
				case TrajectoryType::Artillery:
					return Detail::TraceArtilleryTrajectory(Turret::MaxRangeToVelocity(flatRange),world,source, zOffset, dest, outSteps,impactAt, impactCell);
				break;
				case TrajectoryType::Missile:
					return Detail::TraceMissileTrajectory(flatRange,world,source, zOffset, dest, outSteps,impactAt, impactCell);
				break;
				case TrajectoryType::VerticalBeam:
				{
					int h = Satellite::GetGroundHeight(source);
					if (h == badHeight)
						return TurretTrajectory::InvalidTrajectory;
					outSteps.Clear();
					float4 p(Grid::low.cells[source.stackID].cornerCenter,Grid::ToRadius(h));
					outSteps.Append(p);
					impactAt = p.xyz * p.w;
					impactCell = Grid::HCellIdI(Grid::low.grid.GetCell(source.stackID).higherEquivalent,h);
					return TurretTrajectory::ReachesTarget;
				}
				break;
				default:
					FATAL__("Incompatible trajectory type "+String(t.ToString()));
			}
			return TurretTrajectory::InvalidTrajectory;

		}

		/*static*/ TurretTrajectory			Turret::Detail::TraceArtilleryTrajectory(float velocity, const Grid::WorldData&world, const Grid::LCellIdI&source, float zOffset, const Grid::LCellIdI&dest, Buffer0<TVec4<> >&outSteps, TVec3<>&impactAt, Grid::HCellIdI&impactCell)
		{
			Turret::TAngles angles = Turret::GetAngleForDistance(velocity,source,zOffset,dest);
			if (angles.numAngles == 0)
			{
				if (velocity < 50.f)
				{
					LogEvent(EventClass::Preset::error,"Unable to determine feasible angles for artillery trajectory at v="+String(velocity)+", source="+source.ToString()+", dest="+dest.ToString()+". Increasing velocity and retrying.");
					velocity = velocity > 0 ? velocity * 1.1f : 1.f;	//should not be zero, but it's better to handle this case here anyway. Otherwise we're causing a stack overflow in no-time
					return TraceArtilleryTrajectory(velocity,world,source,zOffset, dest,outSteps,impactAt,impactCell);
				}

				return TurretTrajectory::InvalidTrajectory;
			}

			const float h0 = zOffset + Grid::ToRadiusf(0.05f + source.height);
			const float h1 = Grid::ToRadiusf(0.05f + dest.height);
			const float3 there = Grid::low.cells[dest.stackID].centerVertex.xyz * h1;
			const float3 here = Grid::low.cells[source.stackID].centerVertex.xyz * h0;

			//const float3 up = (here + there).Normalized0();
			float distance = Vec::distance(here,there);

		

			const float h = h1 - h0;


			for (int k = angles.numAngles-1; k < angles.numAngles; k--)
			{
				outSteps.Clear();
				float angle = angles.idealAngle[k];

				float sn = sin(angle),
						cs = cos(angle);


				const int resolution = std::max<int>(2,std::max(distance*2.f,distance * sn * 5.f));

				float tEnd = (velocity * sn + sqrt(sqr(velocity*sn) - 2.f * g * h)) / g;



				impactAt = there;
				#ifdef HYBRID_GRID
					impactCell = Grid::HCellIdI(Grid::low.grid.GetCell(dest.stackID).higherEquivalent,dest.height);
				#else
					impactCell = dest;
				#endif

				//const float3 up = (here + there).Normalized0();

				#ifdef HYBRID_GRID
					UINT32 cellAt = Grid::low.grid.GetCell(source.stackID).higherEquivalent;
				#else
					UINT32 cellAt = source.cellID;
				#endif

			
				bool loop = false;
				float lastH = h0;
				TVec3<> lastB = here;
				for (int i = 0; i <= resolution; i++)
				{
					float t01 = float(i) / float(resolution);
					const float t = t01 * tEnd;
					float fy = h0 + (velocity * t*sn - 0.5f * g * sqr(t));
					TVec3<> b;
					Vec::interpolate(here,there,t01,b);
					Vec::normalize(b);
					outSteps << float4(b,fy);

					cellAt = Grid::high.Locate(b,cellAt);
					int h = (Grid::FloorFromRadius(fy));
					if (world.cellStack[cellAt].GetCellI(h).IsOccupied())
					{
						if (k == 0)
						{
							impactAt = b * (fy);
							impactCell = Grid::HCellIdI(cellAt,h);
							return TurretTrajectory::MidFlightImpact;
						}
						loop = true;
						break;
					}
				}
				if (loop)
					continue;

				impactCell = Grid::HCellIdI(
					#ifdef HYBRID_GRID
						Grid::low.grid.GetCell(dest.stackID).higherEquivalent
					#else
						dest.stackID

					#endif
				,dest.height);
				impactAt = Grid::ToWorld(impactCell);
				float3 last = outSteps.last().xyz * outSteps.last().w;
	//			ASSERT2__(Vec::similar(impactAt,last),Vec::toString(impactAt),last.ToString());
				return TurretTrajectory::ReachesTarget;
			}
			return TurretTrajectory::InvalidTrajectory;
		}

		/*static*/ TurretTrajectory			Turret::Detail::TraceMissileTrajectory(float range, const Grid::WorldData&world, const Grid::LCellIdI&source, float zOffset, const Grid::LCellIdI&dest, Buffer0<TVec4<> >&outSteps, TVec3<>&impactAt, Grid::HCellIdI&impactCell)
		{
		

			const float h0 = zOffset + Grid::ToRadiusf(0.05f + source.height);
			const float h1 = Grid::ToRadiusf(0.05f + dest.height);
			const float3 there = Grid::low.cells[dest.stackID].centerVertex.xyz * h1;
			const float3 here = Grid::low.cells[source.stackID].centerVertex.xyz * h0;
		
			float d2 = Vec::quadraticDistance(here,there);
			if (d2 > sqr(range))
				return TurretTrajectory::InvalidTrajectory;


			const float3 up = (here + there).Normalized0();
			float distance = sqrt(d2);


			const float h = h1 - h0;

		

			const int resolution = std::max<int>(2,distance*2);
			UINT32 cellAt = Grid::low.grid.GetCell(source.stackID).higherEquivalent;

			float lastH = h0;
			TVec3<> lastB = here;
			for (int i = 0; i <= resolution; i++)
			{
				float t01 = float(i) / float(resolution);
			
				float fy = h0 + (t01 * h);
				TVec3<> b;
				Vec::interpolate(here,there,t01,b);
				Vec::normalize(b);
				outSteps << float4(b,fy);

				cellAt = Grid::high.Locate(b,cellAt);
				int h = (Grid::FloorFromRadius(fy));
				if (world.cellStack[cellAt].GetCellI(h).IsOccupied())
				{
					impactAt = b * (fy);
					impactCell = Grid::HCellIdI(cellAt,h);
					return TurretTrajectory::MidFlightImpact;
				}
			}
			impactCell = Grid::HCellIdI(
				#ifdef HYBRID_GRID
					Grid::low.grid.GetCell(dest.stackID).higherEquivalent
				#else
					dest.stackID

				#endif
			,dest.height);
			impactAt = Grid::ToWorld(impactCell);
			float3 last = outSteps.last().xyz * outSteps.last().w;
	//			ASSERT2__(Vec::similar(impactAt,last),Vec::toString(impactAt),last.ToString());
			return TurretTrajectory::ReachesTarget;
		}



		void	Base::FinishCreation(bool scaleToVehicleSize/*=true*/)
		{
			#ifdef HAS_WINDOW		
				foreach (parts,p)
				{
					if (scaleToVehicleSize)
						p->first.Scale(vehicleScale);
					(DX11::BaseInstructionBlock&)p->second = stdVehicleShader;
					p->second.vbo.CreateFromStructs(p->first.GetLOD(0).vertices);
					p->second.ibo.Create(p->first.GetLOD(0).indices);
				}
			#endif
		}

		#ifdef HAS_WINDOW		
			void							Base::VisitAllCommands(FCommandVisitor visitor)
			{	
				foreach (iconCommands,cmd) 
					visitor(*cmd);
				foreach (structureConstructionCommands,cmd) 
					visitor(*cmd);
			}
		#endif







		#ifdef CLIENT


			bool		VehicleNotBusy(const PWorldEntity&e)
			{
				PMobileEntity v = To<MobileEntity>::DynamicCast(e);
				return v && !v->HasTasks();
			}

		#endif



		#ifdef HAS_WINDOW

			struct CellCoords : public int2
			{

				/**/		CellCoords(int x, int y):int2(x,y)
				{}


				CellCoords	operator++(int)
				{
					CellCoords rs = *this;
					if (y == 0)
						y = 1;
					else
					{
						x++;
						y = 0;
					}
					return rs;
				}
				CellCoords&	operator++()
				{
					if (y == 0)
						y = 1;
					else
					{
						x++;
						y = 0;
					}
					return *this;
				}

			};


			CellCoords ConfigureStandardCommands(Turret&component)
			{
				component.iconCommands.Append(IconCommand(int2(4,0), "Abort", &VehicleCmd::AbortOperation, &CommonCmd::CommonButtonCheck,NoCosts));
				component.iconCommands.Append(IconCommand(int2(4,1), "Undo", &VehicleCmd::UndoOperation, &CommonCmd::CommonButtonCheck,NoCosts));
				component.iconCommands.Append(IconCommand(int2(5,1), "Sleep", &VehicleCmd::Sleep, &VehicleCmd::SleepCheck,NoCosts));
				component.iconCommands.Append(IconCommand(int2(5,0), "Demolish",&SignalRemoval,&CommonCmd::CommonButtonCheck,NoCosts));
				return CellCoords(6,0);
			}
		#endif


		void			Terraformer::Configure(Turret&component)
		{

			#ifdef HAS_WINDOW
				CellCoords offset = ConfigureStandardCommands(component);

				component.iconCommands.Append(IconCommand(offset++, "Dig Horizontally", &Cmd::ToggleDigMode
					, &CommonCmd::ModeCheck<CursorMode::DigMode>
					,[](const PWorldEntity&e, int&mineralCostOut, int&timeCostOut)
					{
						mineralCostOut = 0;
						ASSERT__(e->IsVehicle());
						PVehicle v = To<Vehicle>::Cast(e);
						timeCostOut = v->GetOperationRounds(VehicleJob::DigHorizontalMineralCosts,TerrainRemove);
					}
					));
				component.iconCommands.Append(IconCommand(offset++, "Dig Down", &Cmd::DigDown, 
					&CommonCmd::IdleCheck
					,[](const PWorldEntity&e, int&mineralCostOut, int&timeCostOut)
					{
						mineralCostOut = 0;
						ASSERT__(e->IsVehicle());
						PVehicle v = To<Vehicle>::Cast(e);
						timeCostOut = v->GetOperationRounds(VehicleJob::DigDownMineralCosts,TerrainRemove);
					}
				));
				component.iconCommands.Append(IconCommand(offset++, "Clear Above", &Cmd::ClearAbove
					, &CommonCmd::ModeCheck<CursorMode::ClearAboveMode>
					,[](const PWorldEntity&e, int&mineralCostOut, int&timeCostOut)
					{
						mineralCostOut = 0;
						ASSERT__(e->IsVehicle());
						PVehicle v = To<Vehicle>::Cast(e);
						timeCostOut = v->GetOperationRounds(VehicleJob::ClearAboveMineralCosts,TerrainRemove);
					}
				));

				component.iconCommands.Append(IconCommand(offset++, "Build Wall Up", &Cmd::IssueCommand<ConstName::Job_BuildWallUp>
					,&CommonCmd::IdleCheck
					,[](const PWorldEntity&e, int&mineralCostOut, int&timeCostOut)
					{
						mineralCostOut = VehicleJob::BuildWallMineralCosts;
						ASSERT__(e->IsVehicle());
						PVehicle v = To<Vehicle>::Cast(e);
						timeCostOut = v->GetOperationRounds(mineralCostOut,TerrainAdd);
					}
				));
				component.iconCommands.Append(IconCommand(offset++, "Build Horizontal Wall", &Cmd::IssueCommand<ConstName::Job_BuildHorizontalWall>
					,&CommonCmd::IdleCheck
					,[](const PWorldEntity&e, int&mineralCostOut, int&timeCostOut)
					{
						mineralCostOut = VehicleJob::BuildWallMineralCosts;
						ASSERT__(e->IsVehicle());
						PVehicle v = To<Vehicle>::Cast(e);
						timeCostOut = v->GetOperationRounds(mineralCostOut,TerrainAdd);
					}
				));
				component.iconCommands.Append(IconCommand(offset++, "Build Road", &Cmd::BuildRoad
					,&CommonCmd::IdleCheck
					,[](const PWorldEntity&e, int&mineralCostOut, int&timeCostOut)
					{
						mineralCostOut = VehicleJob::BuildRoadMineralCosts;
						ASSERT__(e->IsVehicle());
						PVehicle v = To<Vehicle>::Cast(e);
						timeCostOut = v->GetOperationRounds(mineralCostOut,TerrainAdd);
					}
				));

				#ifndef ONE_COMPOUND_PER_FACTION
					component.iconCommands.Append(IconCommand(offset++, "Withdraw Resources",&CommonCmd::ToggleMode<CursorMode::RestockMode>, &CommonCmd::ModeCheck<CursorMode::RestockMode>,NoCosts));
					component.iconCommands.Append(IconCommand(offset++, "Deposit Resources",&CommonCmd::ToggleMode<CursorMode::DepositMode>, &CommonCmd::ModeCheck<CursorMode::DepositMode>,NoCosts));

					component.iconCommands.Append(IconCommand(offset++, "Build Connectors to", &CommonCmd::ToggleMode<CursorMode::BuildConnectorChainMode>, &CommonCmd::ModeCheck<CursorMode::BuildConnectorChainMode>,NoCosts));
				#else

				#endif
				component.iconCommands.Append(IconCommand(offset++, "Build Road to", &CommonCmd::ToggleMode<CursorMode::BuildRoadChainMode>, &CommonCmd::ModeCheck<CursorMode::BuildRoadChainMode>, NoCosts));
				component.iconCommands.Append(IconCommand(offset++, "Build Bridge to", &CommonCmd::ToggleMode<CursorMode::BuildBridgeMode>, &CommonCmd::ModeCheck<CursorMode::DigTunnelMode>,NoCosts));
				component.iconCommands.Append(IconCommand(offset++, "Dig Tunnel to", &CommonCmd::ToggleMode<CursorMode::DigTunnelMode>, &CommonCmd::ModeCheck<CursorMode::DigTunnelMode>,NoCosts));
				component.iconCommands.Append(IconCommand(offset++, "Build Wall to", &CommonCmd::ToggleMode<CursorMode::BuildWallChainMode>, &CommonCmd::ModeCheck<CursorMode::BuildWallChainMode>,NoCosts));

				offset++;offset++;

				component.iconCommands.Append(IconCommand(offset++, "Build Blocks", &Cmd::BuildBlock, &CommonCmd::ModeCheck<CursorMode::BuildBlockMode>,NoCosts));
				component.iconCommands.Append(IconCommand(offset++, "Commit", &Cmd::CommitOperation, &Cmd::CommitCheck
					,[](const PWorldEntity&e, int&mineralCostOut, int&timeCostOut)
					{
	//					if (activeCursorMode == CursorMode::BuildBlockMode)
						{
							mineralCostOut = (int)cellSelection.Count();
						}
						ASSERT__(e->IsVehicle());
						PVehicle v = To<Vehicle>::Cast(e);
						timeCostOut = v->GetOperationRounds(mineralCostOut,activeCursorMode != CursorMode::BuildBlockMode ? TerrainRemove : TerrainAdd);
						if (activeCursorMode != CursorMode::BuildBlockMode)
							mineralCostOut = 0;
					}
				));
				component.iconCommands.Append(IconCommand(offset++, "Remove Blocks", &Cmd::RemoveBlock, &CommonCmd::ModeCheck<CursorMode::RemoveBlockMode>,NoCosts));

				


				Buffer<PStructure> canBuild;
		
				foreach (structurePrototypes,p)
					if (!(*p)->IsIndestructible())
						#ifdef ONE_COMPOUND_PER_FACTION
							if (!To<Connector>::DynamicCast(*p))
						#endif
						#ifdef NO_MUNITION
							if (!To<MunitionFactory>::DynamicCast(*p) && !To<MunitionStorage>::DynamicCast(*p))
						#endif
						canBuild << *p;
				//canBuild << Mine::globalPrototype;
				//canBuild << Connector::globalPrototype;
				//canBuild << Generator::globalPrototype;
				//canBuild << VehiclePlant::globalPrototype;
				//canBuild << FuelContainer::globalPrototype;
				//canBuild << MineralContainer::globalPrototype;
				int at = 0;

				foreach(canBuild, b)
				{
					PStructure s = *b;
					component.structureConstructionCommands.Append(StructureConstructionCommand(int2(5 + (at%9),2+at/9), /*"Build " + */(*b)->GetTypeName(), *b,std::bind(&Cmd::Construct, *b, std::placeholders::_1),
					//[s](const PWorldEntity&e)->bool
					//{
					//	PVehicle v = To<Vehicle>::DynamicCast(e);
					//	return v && Cmd::ConstructionIsPossible(e) && s->CanPlace(v->GetLastPointOfOrigin());
					//},
					&CommonCmd::IdleCheck));
					at++;
				}
			#endif

			//component.parts.setSize(1);

			//component.parts.first().first.LoadFromObj("objects/constructor.obj");
	/*
			MakeBox(component.parts.first().first, Box<>(-0.5f, -1.f, 0.f, 0.5f, 1.f, 0.15f));
			component.parts.first().first.UpdateBoundingBox();*/
			//component.typeName = "Constructor";
			//component.shortName = "structor";
			//component.productionCosts = 20;
			//component.baseStorageCapacity.level[Resource::Minerals] = 200;
			//component.constructionSpeed = 5;
		

			#ifdef HAS_WINDOW		
				component.parts.setSize(1);

				component.parts.first().first.LoadFromObj("objects/terraformer.obj");
			#endif
	/*
			MakeBox(component.parts.first().first, Box<>(-0.5f, -1.f, 0.f, 0.5f, 1.f, 0.15f));
			component.parts.first().first.UpdateBoundingBox();*/
			component.typeName = "Terraformer";
			component.shortName = "former";
			component.constName = ConstName::Turret_Terraformer;

			component.productionCosts = 40;
			#ifndef ONE_COMPOUND_PER_FACTION
				component.baseStorageCapacity.level[Resource::Minerals] = 200;
			#endif
			component.operationSpeed[Construct] = 10;
			component.operationSpeed[TerrainRemove] = 10;
			component.operationSpeed[TerrainAdd] = 10;
			component.healthMultiplier = 1;
			component.scanningRange = 4;
			component.uplinkTraffic = 30;
			component.uplinkPriority = 200;	//fairly early

		
			component.weight = 2;					//turret weight


			component.FinishCreation();
		}


	


	
		void			Artillery::Configure(Turret&component)
		{
			#ifdef HAS_WINDOW		
				CellCoords offset = ConfigureStandardCommands(component);

				#ifndef ONE_COMPOUND_PER_FACTION
					component.iconCommands.Append(IconCommand(offset++, "Withdraw Munitions",&CommonCmd::ToggleMode<CursorMode::RestockMode>, &CommonCmd::ModeCheck<CursorMode::RestockMode>,NoCosts));
					component.iconCommands.Append(IconCommand(offset++, "Deposit Munitions",&CommonCmd::ToggleMode<CursorMode::DepositMode>, &CommonCmd::ModeCheck<CursorMode::DepositMode>,NoCosts));
				#endif
	
				component.iconCommands.Append(IconCommand(offset++, "Attack",&CommonCmd::ToggleMode<CursorMode::AttackMode>, &CommonCmd::ModeCheck<CursorMode::AttackMode>,NoCosts));

				component.parts.setSize(1);
				component.parts.first().first.LoadFromObj("objects/tank.obj");
			#endif


			component.typeName = "Artillery";
			component.shortName = " Artillery";
			component.constName = ConstName::Turret_Artillery;
			component.constTowerName = ConstName::Type_ArtilleryTower;

			component.productionCosts = 25;
			#if !defined ONE_COMPOUND_PER_FACTION && !defined NO_MUNITION
				component.baseStorageCapacity.level[Resource::Munitions] = 24;
			#endif
		
			component.weight = 3;
			component.healthMultiplier = 1.5f;
			component.horizontalAttackRange = 7;
			component.scanningRange = 4;

			#ifndef HYBRID_GRID
				component.explosionRadius = 3.5f;
			#else
				component.explosionRadius = 2;
			#endif
			component.damageLevel = 7;
			component.attacksPerRound = 1;
			component.initiative = 80;
			#ifdef NO_MUNITION
				component.ammoNeedPerShot = 0;
			#else
				component.ammoNeedPerShot = 6;
			#endif

			component.FinishCreation();
		}


		void			Launcher::Configure(Turret&component)
		{
			#ifdef HAS_WINDOW		
				CellCoords offset = ConfigureStandardCommands(component);

				#ifndef ONE_COMPOUND_PER_FACTION
					component.iconCommands.Append(IconCommand(offset++, "Withdraw Munitions",&CommonCmd::ToggleMode<CursorMode::RestockMode>, &CommonCmd::ModeCheck<CursorMode::RestockMode>,NoCosts));
					component.iconCommands.Append(IconCommand(offset++, "Deposit Munitions",&CommonCmd::ToggleMode<CursorMode::DepositMode>, &CommonCmd::ModeCheck<CursorMode::DepositMode>,NoCosts));
				#endif
	
				component.iconCommands.Append(IconCommand(offset++, "Attack",&CommonCmd::ToggleMode<CursorMode::AttackMode>, &CommonCmd::ModeCheck<CursorMode::AttackMode>,NoCosts));

				component.parts.setSize(1);
				component.parts.first().first.LoadFromObj("objects/launcher.obj");
			#endif


			component.typeName = "Missile Launcher";
			component.shortName = " Launcher";
			component.constName = ConstName::Turret_Launcher;
			component.constTowerName = ConstName::Type_LauncherTower;

			component.productionCosts = 40;
			#if !defined ONE_COMPOUND_PER_FACTION && !defined NO_MUNITION
				component.baseStorageCapacity.level[Resource::Munitions] = 24;
			#endif
		
			component.weight = 13;
			component.healthMultiplier = 1.5f;
			component.horizontalAttackRange = 8;
			component.scanningRange = 4;

			#ifndef HYBRID_GRID
				component.explosionRadius = 1.5f;
			#else
				component.explosionRadius = 1;
			#endif
			component.trajectoryType = TrajectoryType::Missile;
			component.damageLevel = 10;
			component.attacksPerRound = 1;
			component.initiative = 70;
			#ifdef NO_MUNITION
				component.ammoNeedPerShot = 0;
			#else
				component.ammoNeedPerShot = 8;
			#endif

			component.FinishCreation();
		}


		void			Armored::Configure(Turret&component)
		{
			#ifdef HAS_WINDOW		
				CellCoords offset = ConfigureStandardCommands(component);

				#ifndef ONE_COMPOUND_PER_FACTION
					component.iconCommands.Append(IconCommand(offset++, "Withdraw Munitions",&CommonCmd::ToggleMode<CursorMode::RestockMode>, &CommonCmd::ModeCheck<CursorMode::RestockMode>,NoCosts));
					component.iconCommands.Append(IconCommand(offset++, "Deposit Munitions",&CommonCmd::ToggleMode<CursorMode::DepositMode>, &CommonCmd::ModeCheck<CursorMode::DepositMode>,NoCosts));
				#endif
	
				component.iconCommands.Append(IconCommand(offset++, "Attack",&CommonCmd::ToggleMode<CursorMode::AttackMode>, &CommonCmd::ModeCheck<CursorMode::AttackMode>,NoCosts));

				component.parts.setSize(1);
				component.parts.first().first.LoadFromObj("objects/armored.obj");
			#endif


			component.typeName = "Cannon";
			component.shortName = "tank";
			component.constName = ConstName::Turret_Cannon;
			component.constTowerName = ConstName::Type_CannonTower;

			component.productionCosts = 15;
			#if !defined ONE_COMPOUND_PER_FACTION && !defined NO_MUNITION
				component.baseStorageCapacity.level[Resource::Munitions] = 20;
			#endif
		
			component.weight = 3;
			component.healthMultiplier = 2;
			component.horizontalAttackRange = 5;
			component.scanningRange = 5;

			component.explosionRadius = 1;
			component.damageLevel = 3;
			component.attacksPerRound = 2;
			#ifdef NO_MUNITION
				component.ammoNeedPerShot = 0;
			#else
				component.ammoNeedPerShot = 2;
			#endif

			component.ignoresVehicleMotion = true;


			component.FinishCreation();
		}


	
		void			EMP::Configure(Turret&component)
		{
			#ifdef HAS_WINDOW		
				CellCoords offset = ConfigureStandardCommands(component);

				#ifndef ONE_COMPOUND_PER_FACTION
					component.iconCommands.Append(IconCommand(offset++, "Withdraw Uranium",&CommonCmd::ToggleMode<CursorMode::RestockMode>, &CommonCmd::ModeCheck<CursorMode::RestockMode>,NoCosts));
					component.iconCommands.Append(IconCommand(offset++, "Deposit Uranium",&CommonCmd::ToggleMode<CursorMode::DepositMode>, &CommonCmd::ModeCheck<CursorMode::DepositMode>,NoCosts));
				#endif

				component.iconCommands.Append(IconCommand(offset++, "Attack",&CommonCmd::ToggleMode<CursorMode::AttackMode>, &CommonCmd::ModeCheck<CursorMode::AttackMode>,NoCosts));

				component.parts.setSize(1);
				component.parts.first().first.LoadFromObj("objects/emp.obj");
			#endif


			component.typeName = "EMP";
			component.shortName = " EMP";
			component.constName = ConstName::Turret_EMP;
			component.constTowerName = ConstName::Type_EMPTower;

			component.productionCosts = 100;
			#ifndef ONE_COMPOUND_PER_FACTION
				component.baseStorageCapacity.level[Resource::Uranium] = 150;
			#endif
		
			component.weight = 5;
			component.healthMultiplier = 1;
			component.horizontalAttackRange = 17;
			component.scanningRange = 3;

			component.explosionRadius = 3;
			component.damageLevel = 5;
			component.dealsEMPDamage = true;
			component.attacksPerRound = 1;
			component.initiative = 30;
			component.ammoNeedPerShot = 50;
			component.ammoType = Resource::Uranium;
			component.requiresStationaryVehicle = true;
			component.uplinkTraffic = 10;


			component.FinishCreation();
		}


		#ifdef HAS_WINDOW		
			namespace Ragnarok
			{
				namespace MyCmd
				{
					void		Obliterate(const PWorldEntity&entity)
					{
						SignalTargetLessAttack(entity);
					}
				}
			}
		#endif

		void			Ragnarok::Configure(Turret&component)
		{
			#ifdef HAS_WINDOW		
				CellCoords offset = ConfigureStandardCommands(component);

				#ifndef ONE_COMPOUND_PER_FACTION
					component.iconCommands.Append(IconCommand(offset++, "Withdraw Uranium",&CommonCmd::ToggleMode<CursorMode::RestockMode>, &CommonCmd::ModeCheck<CursorMode::RestockMode>,NoCosts));
					component.iconCommands.Append(IconCommand(offset++, "Deposit Uranium",&CommonCmd::ToggleMode<CursorMode::DepositMode>, &CommonCmd::ModeCheck<CursorMode::DepositMode>,NoCosts));
				#endif

				component.iconCommands.Append(IconCommand(offset++, "Obliterate",&MyCmd::Obliterate, &CommonCmd::CommonButtonCheck,NoCosts));

				component.parts.setSize(1);
				component.parts.first().first.LoadFromObj("objects/ragnarökRing.obj");
			#endif


			component.typeName = "Ragnarök";
			component.shortName = " Ragnarök";
			component.constName = ConstName::Turret_Ragnarok;
			component.constTowerName = ConstName::Empty;	//not used

			component.isSatelliteRing = true;
			component.isDoomsday = true;

			component.continuousRotation = true;
			component.continuousRotationSpeed = -0.25f;


			component.productionCosts = 1;	//irrelevant
		
			component.weight = 1;	//irrelevant
			component.healthMultiplier = 1;	//irrelevant
			component.horizontalAttackRange = 0;
			component.scanningRange = 1;

			component.explosionRadius = 10;
			component.damageLevel = 100;
			component.dealsEMPDamage = false;
			component.attacksPerRound = 1;
			component.initiative = 10;
			#ifdef _DEBUG
				component.ammoNeedPerShot = 0;//1000;
			#else
				component.ammoNeedPerShot = 2000;
			#endif
			#ifndef ONE_COMPOUND_PER_FACTION
				component.baseStorageCapacity.level[Resource::Uranium] = component.ammoNeedPerShot;
			#endif
			component.ammoType = Resource::Uranium;
			component.requiresStationaryVehicle = true;
			component.uplinkTraffic = 50;
			component.trajectoryType = TrajectoryType::VerticalBeam;


			component.FinishCreation(false);
		}


	
		void			SkyGuardian::Configure(Turret&component)
		{
			#ifdef HAS_WINDOW		
				CellCoords offset = ConfigureStandardCommands(component);

				#ifndef ONE_COMPOUND_PER_FACTION
					component.iconCommands.Append(IconCommand(offset++, "Withdraw Uranium",&CommonCmd::ToggleMode<CursorMode::RestockMode>, &CommonCmd::ModeCheck<CursorMode::RestockMode>,NoCosts));
					component.iconCommands.Append(IconCommand(offset++, "Deposit Uranium",&CommonCmd::ToggleMode<CursorMode::DepositMode>, &CommonCmd::ModeCheck<CursorMode::DepositMode>,NoCosts));
				#endif

				component.iconCommands.Append(IconCommand(offset++, "Attack",&CommonCmd::ToggleMode<CursorMode::AttackMode>, &CommonCmd::ModeCheck<CursorMode::AttackMode>,NoCosts));

				component.parts.setSize(1);
				component.parts.first().first.LoadFromObj("objects/skyGuardianRing.obj");
			#endif


			component.typeName = "Sky Guardian";
			component.shortName = " Guardian";
			component.constName = ConstName::Turret_ProtonCannon;
			component.constTowerName = ConstName::Empty;	//not used

			component.isSatelliteRing = true;

			component.productionCosts = 1;	//irrelevant
			#ifndef ONE_COMPOUND_PER_FACTION
				component.baseStorageCapacity.level[Resource::Uranium] = 30;
			#endif
		
			component.weight = 1;	//irrelevant
			component.healthMultiplier = 1;	//irrelevant
			component.horizontalAttackRange = 30;
			component.scanningRange = 10;

			component.explosionRadius = 2;	//in space, 2 is 1
			component.damageLevel = 5;
			component.dealsEMPDamage = false;
			component.attacksPerRound = 1;
			component.initiative = 60;
			component.ammoNeedPerShot = 0;//10;
			component.ammoType = Resource::Uranium;
			component.requiresStationaryVehicle = true;
			component.uplinkTraffic = 10;
			component.trajectoryType = TrajectoryType::Missile;


			component.FinishCreation(false);
		}

		void			SkyHubTurret::Configure(Turret&component)
		{
			#ifdef HAS_WINDOW		
				CellCoords offset = ConfigureStandardCommands(component);
				offset++;//remain on lower level
				component.iconCommands.Append(IconCommand(offset++, "Repair",&CommonCmd::ToggleMode<CursorMode::RepairMode>, &CommonCmd::ModeCheck<CursorMode::RepairMode>,NoCosts));

				component.parts.setSize(1);
				component.parts.first().first.LoadFromObj("objects/skyHubTurret.obj");
			
			#endif


			component.typeName = "Sky Hub";
			component.shortName = " Hub";
			component.constName = ConstName::Turret_SkyHub;
			component.constTowerName = ConstName::Empty;	//not used

			component.isSatelliteRing = true;
			component.operationSpeed[Repair] = 10;
			component.operationSpeed[Construct] = 5;
			//component.fuelSupplyRadius = 20;

			component.continuousRotation = true;
			component.continuousRotationSpeed = 0.25f;

			component.productionCosts = 1;	//irrelevant
		
			component.weight = 1;	//irrelevant
			component.healthMultiplier = 1;	//irrelevant
			component.scanningRange = 35;	//temporary. Once orbital scanners are introduced, reduce this value again
			component.groundScanningRange = 10;

			component.attacksPerRound = 0;
			component.initiative = 60;
			component.uplinkTraffic = 20;

			component.FinishCreation(false);
		}

		void			StarPort::Configure(Turret&component)
		{
			#ifdef HAS_WINDOW		
				CellCoords offset = ConfigureStandardCommands(component);
				offset++;//remain on lower level
				component.iconCommands.Append(IconCommand(offset++, "Repair",&CommonCmd::ToggleMode<CursorMode::RepairMode>, &CommonCmd::ModeCheck<CursorMode::RepairMode>,NoCosts));

				component.parts.setSize(1);
				component.parts.first().first.LoadFromObj("objects/starPortRing.obj");
			
			#endif


			component.typeName = "Star Port";
			component.shortName = " Port";
			component.constName = ConstName::Turret_StarPort;
			component.constTowerName = ConstName::Empty;	//not used

			component.isSatelliteRing = true;

			component.continuousRotation = true;
			component.continuousRotationSpeed = -0.05f;

			component.productionCosts = 1;	//irrelevant
			component.operationSpeed[Repair] = 10;
		
			component.weight = 1;	//irrelevant
			component.healthMultiplier = 1;	//irrelevant
			component.scanningRange = 10;
			component.groundScanningRange = 0;

			component.attacksPerRound = 0;
			component.initiative = 30;
			component.uplinkTraffic = 1;	//irrelevant

			component.FinishCreation(false);
		}

		void			MunitionTransporter::Configure(Turret&component)
		{
			#ifdef HAS_WINDOW		
				CellCoords offset = ConfigureStandardCommands(component);

				component.iconCommands.Append(IconCommand(offset++, "Withdraw Munitions",&CommonCmd::ToggleMode<CursorMode::RestockMode>, &CommonCmd::ModeCheck<CursorMode::RestockMode>,NoCosts));
				component.iconCommands.Append(IconCommand(offset++, "Deposit Munitions",&CommonCmd::ToggleMode<CursorMode::DepositMode>, &CommonCmd::ModeCheck<CursorMode::DepositMode>,NoCosts));

				component.parts.setSize(1);
				component.parts.first().first.LoadFromObj("objects/munitionTransporter.obj");
			#endif


			component.typeName = "Munition Transporter";
			component.shortName = " Ammo Truck";
			component.constName = ConstName::Turret_MunitionTransporter;

			component.productionCosts = 5;
			#if !defined ONE_COMPOUND_PER_FACTION && !defined NO_MUNITION
				component.baseStorageCapacity.level[Resource::Munitions] = 100;
			#endif
		
			component.weight = 4;
			component.healthMultiplier = 1;
			component.scanningRange = 3;

			//component.explosionRadius = 1;
			//component.damageLevel = 3;
			//component.attacksPerRound = 2;
			//component.ammoNeedPerShot = 2;

			//component.ignoresVehicleMotion = true;


			component.FinishCreation();
		}

		void			Mechanic::Configure(Turret&component)
		{
			#ifdef HAS_WINDOW		
				CellCoords offset = ConfigureStandardCommands(component);

				#ifndef ONE_COMPOUND_PER_FACTION
					component.iconCommands.Append(IconCommand(offset++, "Withdraw Resources",&CommonCmd::ToggleMode<CursorMode::RestockMode>, &CommonCmd::ModeCheck<CursorMode::RestockMode>,NoCosts));
					component.iconCommands.Append(IconCommand(offset++, "Deposit Resources",&CommonCmd::ToggleMode<CursorMode::DepositMode>, &CommonCmd::ModeCheck<CursorMode::DepositMode>,NoCosts));
				#endif

				component.iconCommands.Append(IconCommand(offset++, "Repair",&CommonCmd::ToggleMode<CursorMode::RepairMode>, &CommonCmd::ModeCheck<CursorMode::RepairMode>,NoCosts));

				component.parts.setSize(1);
				component.parts.first().first.LoadFromObj("objects/constructor.obj");
			#endif


			component.typeName = "Mechanic";
			component.shortName = "mechanic";
			component.constName = ConstName::Turret_Mechanic;
			component.constTowerName = ConstName::Type_MechanicTower;

			component.productionCosts = 30;
			#ifndef ONE_COMPOUND_PER_FACTION
				component.baseStorageCapacity.level[Resource::Minerals] = 100;
			#endif
		
			component.weight = 2;
			component.healthMultiplier = 1;
			component.scanningRange = 3;
			component.operationSpeed[Repair] = 10;
			component.uplinkTraffic = 10;



			component.FinishCreation();
		}


		void			Radar::Configure(Turret&component)
		{
			#ifdef HAS_WINDOW		
				CellCoords offset = ConfigureStandardCommands(component);

				component.parts.setSize(1);
				component.parts.first().first.LoadFromObj("objects/radar.obj");
			#endif


			component.typeName = "Radar";
			component.shortName = "radar";
			component.constName = ConstName::Turret_Radar;
			component.constTowerName = ConstName::Type_RadarTower;

			component.productionCosts = 10;
		
			component.weight = 2;
			component.healthMultiplier = 1;
			component.scanningRange = 12;
			component.continuousRotation = true;
			component.continuousRotationSpeed = 2.f;
			component.uplinkTraffic = 10;


			component.FinishCreation();
		}

	
	
		void			ResourceScanner::Configure(Turret&component)
		{
			#ifdef HAS_WINDOW		
				CellCoords offset = ConfigureStandardCommands(component);
				component.iconCommands.Append(IconCommand(offset++, "Automate", &VehicleCmd::Automate, &VehicleCmd::AutomatedCheck,NoCosts));

				component.parts.setSize(1);
				component.parts.first().first.LoadFromObj("objects/resourceScanner.obj");
			#endif


			component.typeName = "Surveyor";
			component.shortName = "surveyor";
			component.constName = ConstName::Turret_Surveyor;

			component.productionCosts = 10;
			component.continuousRotation = true;
			component.continuousRotationSpeed = -1.f;

			component.weight = 1;
			component.healthMultiplier = 1;
			component.detectsResources = true;
			component.scanningRange = 4;
			component.uplinkPriority = 180;	//fairly early



			component.FinishCreation();
		}



		void			Hoverbase::Configure(VehicleBase&component)
		{
		

			static const float staticHeight = 0.4743f;

			#ifdef HAS_WINDOW		
				component.parts.setSize(1);

				component.parts.first().first.LoadFromObj("objects/hoverbase.obj");
				component.height = component.parts.first().first.GetBoundingBox().z.max;
				ASSERT2__(similar(staticHeight,component.height),staticHeight,component.height);

				component.ambientSound = OpenAL::Load("resources/objectSounds/hoverBase.wav");

			#else
				component.height = staticHeight;
			#endif

			//MakeBox(component.parts.first().first, Box<>(-0.5f, -1.f, 0.f, 0.5f, 1.f, 0.35f));
			component.typeName = "Hoverbase";
			component.shortName = "Hover";
			component.constName = ConstName::VehicleBase_HoverBase;

			component.productionCosts = 40;
			component.requirements = VehicleBase::Requirement::AnyBlock;
			component.moveOnWater = true;
			component.minClimbDown = -int(Grid::CellStack::Range);
			#ifndef ONE_COMPOUND_PER_FACTION
				component.storageMultiplier = 1;
			#endif

			component.baseMovementSpeed = 20;
			component.carryingCapacity = 1;
			component.baseHealth = 4;


			component.FinishCreation();

			#ifdef CLIENT	
				component.height *= vehicleScale;
			#endif
		}

		void			Heavybase::Configure(VehicleBase&component)
		{
		
			static const float staticHeight = 0.4743f;

			#ifdef HAS_WINDOW		
				component.parts.setSize(1);

				component.parts.first().first.LoadFromObj("objects/heavyBase.obj");
				component.height = component.parts.first().first.GetBoundingBox().z.max;
				ASSERT2__(similar(staticHeight,component.height),staticHeight,component.height);
			#else
				component.height = staticHeight;
			#endif


			//MakeBox(component.parts.first().first, Box<>(-0.5f, -1.f, 0.f, 0.5f, 1.f, 0.35f));
			component.typeName = "Heavy Base";
			component.shortName = "Heavy";
			component.constName = ConstName::VehicleBase_HeavyBase;

			component.productionCosts = 80;
			component.requirements = VehicleBase::Requirement::AnyBlock;	//none is broken...
			component.moveOnWater = false;
			component.minClimbDown = -2;
			//component.minClimbDown = -int(Grid::CellStack::Range);
			#ifndef ONE_COMPOUND_PER_FACTION
				component.storageMultiplier = 2;
			#endif
			component.operationSpeedMultiplier = 2;

			component.baseMovementSpeed = 15;
			component.carryingCapacity = 10;
			component.baseHealth = 12;

			component.hindrance.road = 1;
			component.hindrance.air = 10;
			component.hindrance.water = 10;
			component.hindrance.other = 4;


			component.FinishCreation();

			#ifdef CLIENT	
				component.height *= vehicleScale;
			#endif
		}

		void			Catbase::Configure(VehicleBase&component)
		{
		
			static const float staticHeight = 0.4743f;

			#ifdef HAS_WINDOW		
				component.parts.setSize(1);

				component.parts.first().first.LoadFromObj("objects/catBase.obj");
				component.height = component.parts.first().first.GetBoundingBox().z.max;
				ASSERT2__(similar(staticHeight,component.height),staticHeight,component.height);
			#else
				component.height = staticHeight;
			#endif


			//MakeBox(component.parts.first().first, Box<>(-0.5f, -1.f, 0.f, 0.5f, 1.f, 0.35f));
			component.typeName = "Catamaran Base";
			component.shortName = "Cat";
			component.constName = ConstName::VehicleBase_CatBase;

			component.productionCosts = 100;
			component.requirements = VehicleBase::Requirement::Water;	//none is broken...
			component.moveOnWater = true;
			component.minClimbDown = -2;
			//component.minClimbDown = -int(Grid::CellStack::Range);
			#ifndef ONE_COMPOUND_PER_FACTION
				component.storageMultiplier = 2;
			#endif
			component.operationSpeedMultiplier = 2;
			component.rangeModifier = 1;

			component.baseMovementSpeed = 8;
			component.carryingCapacity = 15;
			component.baseHealth = 12;

			component.hindrance.road = 10;
			component.hindrance.air = 2;
			component.hindrance.water = 1;
			component.hindrance.other = 10;


			component.FinishCreation();

			#ifdef CLIENT	
				component.height *= vehicleScale;
			#endif
		}




		#ifdef HAS_WINDOW
			bool	NotModifyBlockMode(const Vehicle&v)
			{
				return activeCursorMode != CursorMode::BuildBlockMode && activeCursorMode != CursorMode::RemoveBlockMode;
			}

			/*static*/ bool Terraformer::Cmd::NotBuilding(const PWorldEntity&e)
			{
				PMobileEntity v = To<MobileEntity>::DynamicCast(e);
				return v && (!v->HasTasks() || !v->myTasks.Peek().IsJob());
			}

			/*static*/ void Terraformer::Cmd::Construct(const PStructure&p, const PWorldEntity&v)
			{
				PMobileEntity vehicle = To<MobileEntity>::DynamicCast(v);
				if (!vehicle)
					return;
				SignalNewJob(vehicle, ConstName::Job_Construct, p->GetCTypeName());
				//auto loc = vehicle->GetLastPointOfOrigin();
				///*if (!p->CanPlace(loc))
				//{
				//	LogEvent("Unable to place " + p->GetTypeName() + " here");
				//	return;
				//}*/

				//CursorMode::PJob job(new VehicleJob::Construct(*vehicle,p));

				//vehicle->RegisterNewJob(p->GetTypeName(),job,&loc);
		
			}



			void Terraformer::Cmd::ToggleDigMode(const PWorldEntity&e)
			{
				CommonCmd::ToggleMode(e,CursorMode::DigMode);
				}


			void Terraformer::Cmd::ClearAbove(const PWorldEntity&e)
			{
				CommonCmd::ToggleMode(e, CursorMode::ClearAboveMode);
			}

			bool Terraformer::Cmd::ClearAboveIsActive(const PWorldEntity&e)
			{
				return CommonCmd::IsModeActive(e,CursorMode::ClearAboveMode);
			}
	
			bool Terraformer::Cmd::BuildBlockIsActive(const PWorldEntity&e)
			{
				return CommonCmd::IsModeActive(e,CursorMode::BuildBlockMode);
			}



			bool Terraformer::Cmd::RemoveBlockIsActive(const PWorldEntity&e)	
			{
				return CommonCmd::IsModeActive(e,CursorMode::RemoveBlockMode);
			}
	 


			bool Terraformer::Cmd::CommitIsPossibleC(const PWorldEntity&e, bool controlEnabled)
			{
				return controlEnabled && 
					(CommonCmd::IsModeActive(e,CursorMode::BuildBlockMode) || CommonCmd::IsModeActive(e,CursorMode::RemoveBlockMode));
			}

			BaseCommand::Flags Terraformer::Cmd::CommitCheck(const PWorldEntity&e, bool controlEnabled)
			{
				return CommitIsPossibleC(e,controlEnabled) ? BaseCommand::Flags::IsEnabled : BaseCommand::Flags::None;
			}

	
		/*	bool VehicleCmd::AbortIsPossible(const PWorldEntity&e)
			{
				PVehicle v = To<Vehicle>::DynamicCast(e);
				return (activeCursorMode != CursorMode::Idle || (v && v->tasks.IsNotEmpty()));
			}*/

			bool Terraformer::Cmd::ConstructionIsPossible(const PWorldEntity&e)
			{
				return CommonCmd::IsModeActive(e,CursorMode::Idle) && NotBuilding(e); 
			}
		
	
			/*static*/ void					Terraformer::Cmd::DigDown(const PWorldEntity&v)
			{
				if (v->location.height < -Grid::CellStack::HalfRange+2)
					return;
				PVehicle vehicle = To<Vehicle>::DynamicCast(v);
				if (!vehicle)
					return;
				SignalNewJob(vehicle, ConstName::Job_DigDown, ConstName::Empty);

				//Grid::LCellIdI target = vehicle->GetLastPointOfOrigin()-1;
				//
				//CursorMode::PJob job(new VehicleJob::DigDown(*vehicle));

				//vehicle->RegisterNewJob("",job,&target);
			}

			void			Base::DescribeType(BaseCommand::Label&label, bool compound)
			{
			//	label << typeName;
				if (!compound)
					label << " Cost: "+String(ResourceTag(Resource::Minerals))+String(productionCosts);
				label.MarkBriefEnd();
				if (!compound)
					label << " Initiative: "+String(initiative);
			}



			void VehicleBase::DescribeType(BaseCommand::Label&outResult, bool compound)
			{
				outResult << typeName + " (Base)";
				Base::DescribeType(outResult,compound);
				outResult << " Can climb "+String(maxClimbUp)+" height level(s) up";
			
				//if ( minClimbDown > -int(Grid::CellStack::Range))
				outResult << " Can safely fall down "+String(-minClimbDown)+" height level(s)";
				#ifndef ONE_COMPOUND_PER_FACTION
					if (storageMultiplier != 1)
						outResult << " Storage capacity multiplied by "+String(storageMultiplier);
				#endif
				if (!compound)
				{
					if (operationSpeedMultiplier != 1)
						outResult << " Operation speed multiplied by "+String(operationSpeedMultiplier);
					outResult<<" Base speed: "+String(baseMovementSpeed);
					if (requirements == Requirement::Water)
						outResult.lines.last()+= " (naval)";
					else
						if (moveOnWater)
							outResult.lines.last()+= " (moves over water)";
					outResult<<" Base health: "+String(baseHealth);
					outResult<<" Turret carrying capacity: "+String(carryingCapacity);
					if (rangeModifier != 0)
						outResult<<" Turret range modifier: "+String(rangeModifier);
				}
				if (ignoresVehicleAttacks)
					outResult<<" Can move independently of attack action";
			}




			void Turret::DescribeType(BaseCommand::Label&outResult, bool compound, const String&parentType, const OwnerParameter&owner)
			{
				const String upperName = parentType.IsNotEmpty() ? parentType+"-Turret" : "Turret";
				const String nameAppend = isDoomsday ? ", DOOMSDAY" : "";
				const String lowerName = upperName.copyToLowerCase();
				outResult << typeName+" ("+upperName+nameAppend+")";

				if (!compound && RuntimeConfig::UnitLimits::AnySet())
				{
					count_t cnt = 0, limit = 0;
					cnt = Vehicle::CountTurretOccurrences(owner,this)
						+
						Satellite::CountTurretOccurrences(owner,this);
					limit = RuntimeConfig::UnitLimits::Get(constName);
					if (limit != 0xFFFF)
						outResult << " Deployed: "+String(cnt)+"/"+String(limit);
				}


				Base::DescribeType(outResult,compound);
				if (!compound)
				{
					outResult << " Weight: "+String(weight);
					outResult << " Base scanning range: "+String(scanningRange);
					if (healthMultiplier != 1)
						outResult << " Health: x"+String(healthMultiplier);
					if (RuntimeConfig::hibernationSystemEnabled)
					{
						outResult << " "+String(TrafficResourceTag())+String(uplinkTraffic)+", Priority: "+String(uplinkPriority);
					}
				}
				if (CanAttack())
				{
					if (!compound)
						outResult << " Offensive "+lowerName;
					if (ignoresVehicleMotion)
						outResult << " Can attack independently of movement action";
					elif (requiresStationaryVehicle)
						outResult << " Vehicle must remain stationary to fire";
					if (!compound)
						outResult << " Base horizontal attack range: "+String(horizontalAttackRange);
					if (ammoNeedPerShot == 0)
						outResult << "Does not require ammunition";
					else
						outResult << " Consumes "+String(ResourceTag(ammoType))+String(ammoNeedPerShot)+" per shot";
					outResult << " Explosion radius: "+String(explosionRadius);
					if (dealsEMPDamage)
						outResult << " EMP damage: "+String(damageLevel);
					else
						outResult << " Damage: "+String(damageLevel);
					outResult << " "+String(attacksPerRound)+" attack(s) per round";
				}

				if (CanConstruct() && !compound)
					outResult << " Construction: Converts "+String(ResourceTag(Resource::Minerals))+String(operationSpeed[Construct])+" per round";
				if ((CanDo(TerrainRemove) || CanDo(TerrainAdd)) && !compound)
					outResult << " Terraforming: Can shape the surrounding terrain";
				if (CanRepair())
					outResult <<" Can repair other vehicles/structures";
				if (detectsResources)
					outResult <<  " Detects natural resources";
				#ifndef ONE_COMPOUND_PER_FACTION
					if (!compound && baseStorageCapacity.IsNotEmpty())
					{
						outResult << " Base storage capacity:";
						for (index_t i = 0; i < Resource::NumberOfPossibilities; i++)
						{
							if (baseStorageCapacity.level[i])
							{
								outResult << "  "+String(ResourceTag(i))+String(baseStorageCapacity.level[i]);
							}
						}
					}
				#endif
				outResult.MarkBriefEnd();
				outResult << "";
				if (CanConstruct())
					outResult.WrapAppendText("This "+lowerName+" type can build structures in valid locations. It requires "+String(ResourceTag(Resource::Minerals))+" to initiate construction.");
				if (CanRepair())
					outResult.WrapAppendText("This "+lowerName+" type can repair other structures and vehicles. It requires "+String(ResourceTag(Resource::Minerals))+" to initiate repair.");
				if (detectsResources)
					outResult.WrapAppendText("This "+lowerName+" type detects all naturally occuring resources within its scanning range. It can be automated.");
				if (CanAttack())
				{
					outResult.WrapAppendText("This "+lowerName+" type can attack objects in range. It must typically be accompanied by a radar tower or vehicle.");
					if (dealsEMPDamage)
						outResult.WrapAppendText("This "+lowerName+" deals EMP damage. EMP damage temporarily reduces scanning range. Does not cause any permanent damage.");
				}
				else
					if (scanningRange > 10)
						outResult.WrapAppendText("This "+lowerName+" type is especially suited for detecting hostile entities. Such turret types are manadatory in order to see hostile forces approaching, as well as their attacks. Offensive turrets usually lack the necessary scanning range to see where they are shooting and depend on this turret type to reside in the vicinity.");
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


			void					Terraformer::Cmd::BuildRoad(const PWorldEntity&v)
			{
				if (v->location.height <= -Grid::CellStack::HalfRange)
					return;
				PVehicle vehicle = To<Vehicle>::DynamicCast(v);
				if (!vehicle)
					return;
				SignalNewJob(vehicle, ConstName::Job_BuildRoad,ConstName::Empty);
				//Vehicle::PJob job(new Job::BuildRoad(*vehicle));
				//vehicle->RegisterNewJob("", job,nullptr);
			}





			void	Terraformer::Cmd::RemoveBlock(const PWorldEntity&e)
			{
				PVehicle v = To<Vehicle>::DynamicCast(e);
				if (!v)
					return;
				CommonCmd::ToggleMode(e,CursorMode::RemoveBlockMode);

				cellSelection.Clear();
				removeCandidates.Clear();

				if (activeCursorMode == CursorMode::RemoveBlockMode)
				{
					ASSERT__(v);
					Grid::high.UpdatePlanarDistanceField(
						#ifdef HYBRID_GRID
							Grid::low.grid.GetCell(v->GetLastPointOfOrigin().stackID).higherEquivalent
							, 8, Objects::vehicleDistanceField);
						#else
							v->GetLastPointOfOrigin().stackID

							, 4, Objects::vehicleDistanceField);
						#endif
					for (UINT32 i = 0; i < (UINT32)Grid::high.grid.CellCount; i++)
					{
						if (Objects::vehicleDistanceField.distance[i] <= 3)
						{
							UINT32 at = v->location.height + Grid::CellStack::HalfRange;
							UINT32 min = std::max(at,2U)-1;
							for (UINT32 j = min; j <= at + 1; j++)
							{
								if (Grid::actualWorld.cellStack[i].volumeCell[j].IsSolid())
								{
									//if (j != at || Grid::vehicleDistanceField.distance[i] > 1)
									{
										removeCandidates << Grid::HCellIdU(i, j);
									}

								}

							}

						}

					}


				}

				if (v == selectedEntity)
					UpdateChoices(true);
			}


			void	Terraformer::Cmd::BuildBlock(const PWorldEntity&e)
			{
				PVehicle v = To<Vehicle>::DynamicCast(e);
				if (!v)
					return;
				CommonCmd::ToggleMode(e,CursorMode::BuildBlockMode);
				cellSelection.Clear();
				buildCandidates.Clear();
				const int height = v->GetLastPointOfOrigin().height;

				if (activeCursorMode == CursorMode::BuildBlockMode)
				{
					ASSERT__(v);
					Grid::high.UpdatePlanarDistanceField(
						#ifdef HYBRID_GRID
							Grid::low.grid.GetCell(
								v->GetLastPointOfOrigin().stackID).higherEquivalent
							, 8,
						#else
							v->GetLastPointOfOrigin().stackID

							, 4,
						#endif
						 Objects::vehicleDistanceField);
					for (UINT32 i = 0; i < (UINT32)Grid::high.grid.CellCount; i++)
					{
						if (Objects::vehicleDistanceField.distance[i] <= 3)
						{
							UINT32 at = height + Grid::CellStack::HalfRange;
							for (UINT32 j = at - 1; j <= at + 1; j++)
							{
								if (!Grid::actualWorld.cellStack[i].volumeCell[j].IsOccupied())
								{
									if (j != at || Objects::vehicleDistanceField.distance[i] > 1)
									{
										buildCandidates << Grid::HCellIdU(i, j);
									}

								}

							}

						}

					}


				}

				if (v == selectedEntity)
					UpdateChoices(true);
			}

			void					Terraformer::Cmd::CommitOperation(const PWorldEntity&e)
			{
				PVehicle v = To<Vehicle>::DynamicCast(e);
				if (!v)
					return;
				Array<Grid::HCellIdI>	selection;
				Objects::cellSelection.MoveToArray(selection,true);
				removeCandidates.Clear();
				buildCandidates.Clear();

				SignalTerrainModification(v,activeCursorMode,selection );
				//Vehicle::PJob job(new Job::AlterTerrain(*v,activeCursorMode,selection));

				//v->RegisterNewJob("",job,nullptr);
				activeCursorMode = CursorMode::Idle;
			}




		#endif


		/*static*/ bool					VehicleBase::canStandOnUnknown=true;

		bool							VehicleBase::CanStandOn(const Grid::WorldData&world, const Grid::LCellIdI&cell, bool ignoreWater, bool doFloat)
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
					if (strongest == Grid::Content::Clear)
						return doFloat;
					return false;	//lava
				}
				case Requirement::Water:
					if (anyUnknown && !canStandOnUnknown)
						return false;
					//if (strongestStr > waterStrength)
					//	return false;
					//if (strongest == Grid::Content::Lava)
					//	return false;
					if (strongest == Grid::Content::Clear)
						return doFloat;
					return strongest == Grid::Content::Water;
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

		bool							Base::TestIntersection(float zOffset, float x, float y, const TVec3<>&_b, const TVec3<>&_d, float & distance)
		{
			#ifdef HAS_WINDOW
				bool rs = false;
				TMatrix4<> system1,system2,inverse;
				TVec3<> b,d;
				if (parts.Count() > 0)
				{
					GetSubSystem(0,x,system1);
					//system1.w.xyz += system1.z.xyz * zOffset;
					system1.w.z = zOffset;
					Mat::invertSystem(system1,inverse);
					Mat::transform(inverse,_b,b);
					Mat::rotate(inverse,_d,d);
					rs |= parts.first().first.TestIntersection(b,d,distance);
				}
				if (parts.Count() > 1)
				{
					GetSubSystem(1,y,system2);
					Mat::transformSystem(system1,system2);
					Mat::invertSystem(system2,inverse);
					Mat::transform(inverse,_b,b);
					Mat::rotate(inverse,_d,d);
					rs |= parts[1].first.TestIntersection(b,d,distance);
				}
				return rs;
			#else
				return false;
			#endif
	
		}


		void							Base::GetSubSystem(index_t geometryIndex, float angle, TMatrix4<>&outSystem)	const
		{
			Mat::eye(outSystem);
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
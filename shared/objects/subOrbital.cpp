#include <global_root.h>
#include "subOrbital.h"
#include "gridAndChoices.h"
#include "faction.h"

namespace Objects
{

	namespace SubOrbital
	{
		SharedContainer<Plane>	planes;

		/*static*/ count_t			Plane::CountTurretOccurrences(const OwnerParameter&o,Component::Turret*t)
		{
			count_t rs = 0;
			planes.Foreach([&rs,o,t](const PPlane&m)
			{
				if (m->owner == o && m->turret == t)
					rs++;
			});
			return rs;
		}


		/*virtual override*/ float3			Plane::GetScanningCenter(const Grid::LCellIdI&loc) const
		{
			return Grid::ToWorld(loc);
		}




		/*virtual override*/ void		Plane::VisitSensors(const Grid::LCellIdI&loc, const std::function<void(const World::Sensor&)>&visitor, bool appearance /*= false*/) const
		{
			float3 scanningCenter = GetScanningCenter(loc);
			float range2 = GetScanningRange();
			if (loc.height > Grid::CellStack::TopHeight)
			{
				float range = turret->groundScanningRange.GetValue(GetResearchState());
				if (range > 0)
				{
					range = EMPModify(range, 2);
					visitor(World::Sensor(scanningCenter, range, World::Sensor::ShadedCylinder, (int)turret->groundScannerPenetration.GetValue(GetResearchState())));
				}
				if (range2 == range)
					visitor(World::Sensor(scanningCenter, range2,World::Sensor::UpperHalfSolidSphere));
				else
					Super::VisitSensors(loc, visitor, appearance);
			}
			else
			{
				visitor(World::Sensor(scanningCenter, range2*0.5f, World::Sensor::ShadedSphere));
			//	Super::VisitSensors(loc, visitor, appearance);
			}
		}


		
		#ifdef HAS_WINDOW



			/*virtual override*/ void Plane::DescribeType(Description&outResult, UINT32 descFlags)
			{
				Super::DescribeType(outResult,descFlags);
				String speed = "Speed: "+String(this->GetMovementStepsPerRound()) + " (aerial)";

				outResult << speed;
				int opSpeed = GetOperationSpeed(Component::Construct);
				if (opSpeed)
					outResult << "Effective construction speed: "+String(ResourceTag(Resource::Minerals))+String(opSpeed)+" per round";

				const String fuel (ResourceTag(Resource::Fuel));

				int demand = 0;
				if (demand)
				{
					outResult << "Consumes"+fuel+String(demand)+" per round";
				}



				outResult.MarkBriefEnd();
				outResult<< "Suborbital planes move above the regular terrain. They can scan the surface, and are invisible to ordinary radar.";

				if (IsHangar())
					outResult<< "This plane can quickly transport other mobile entities to remote locations. Land atop mobile units, or have them move on the same location as the plane to load.";
			}
		


		#endif





		/*static*/ int				Plane::GetCentralGroundHeight(const Grid::LCellIdI&location)
		{
			UINT32 fine = Grid::low.grid.GetCell(location.stackID).higherEquivalent;
			return Detail::WalkStack(World::actual.GetStack(fine),false);
		}


		int						Plane::GetCentralGroundHeight() const
		{
			return GetCentralGroundHeight(location);
		}


		/*static*/ int			Plane::Detail::WalkStack(const Grid::CellStack&stack, bool failOnStructureCount)
		{
			UINT32 h;
			for (h = Grid::CellStack::Range-1; h < Grid::CellStack::Range; h--)
			{
				const Grid::CellStack::Cell&c = stack.volumeCell[h];
				if (c.content != Grid::Content::Clear)
					break;
			}
			h++;
			if (h >= Grid::CellStack::Range-1)
				return badHeight;
			if (failOnStructureCount)
			{
				const Grid::CellStack::Cell&c = stack.volumeCell[h];
				if (c.structureCounter > 1)
					return badHeight;
			}

			return Grid::CellStack::ToHeight(h);
		
		}


		/*static*/ int				Plane::GetLandingHeight(const Grid::LCellIdI&satelliteLocation)
		{
			#ifdef HYBRID_GRID
				UINT32 finer = Grid::low.grid.GetCell(satelliteLocation.stackID).higherEquivalent;
				const auto&c = Grid::high.grid.GetCell(finer);
			#else
				UINT32 finer = who->location.stackID;
			#endif
			int h = Detail::WalkStack(World::actual.GetStack(finer),true);
			if (h == badHeight)
				return badHeight;
			#ifdef HYBRID_GRID
				for (int j = 0; j < c.neighborhood.numNeighbors; j++)
				{
					int h2 = Detail::WalkStack(World::actual.GetStack(c.neighborhood.neighborID[j]),true);
					if (h2 == badHeight)
						return badHeight;
					h = std::max(h2,h);
				}
			#endif
			return h <= Grid::CellStack::TopHeight ? h : badHeight;
		}





		#ifndef CLIENT


			/*virtual override*/ bool		Plane::ProcessRound(float&atTime)
			{
				return true;
			}

			/*virtual override*/ MobilePlacementResult	Plane::PlaceExactlyAt(const Grid::LCellIdI&cell, float timeCost, float minTimeOffset, bool signalEvent/*=true*/, bool isCreated/*=true*/, bool failIsFatal/*=false*/)
			{
			//	UINT32 cell = _cell.stackID;
				if (health <= 0)
				{
					if (failIsFatal)
					{
						LogEvent(EventClass::Preset::fatal,this,"Dead");
						FATAL__("Critical abort");
					}
					return MobilePlacementResult::PlacementFailedHarmfulOrBlocked;
				}
			
				//VerifyVisibilityNetwork();
				SyncTime(minTimeOffset);
				
				if (cell.height <= Grid::CellStack::TopHeight && !World::actual.LowCellIsClear(cell))
				{
					PHangar s = MobileDestinationStructureHere(cell,*this);
					if (s && s->Store(To<Vehicle>::Cast(shared_from_this()),entityTime))
						return MobilePlacementResult::EnteredStructure;

					if (failIsFatal)
					{
						PStructure st = GetStructure(cell);
						LogEvent(EventClass::Preset::fatal,this,"Target cell ("+ToString(cell)+") is obstructed by structure "+(st ? st->GetName(): "<>")+": "+World::actual.LowCellReport(cell));
						FATAL__("Critical abort");
					}

					return MobilePlacementResult::PlacementFailedHarmfulOrBlocked;
				}

				if (location != cell) 
				{
					PMobileEntity v = GetMobileEntity(cell);
					if (v)
					{
						bool isGood = false;
						if (v->owner == owner)
						{
							PHangar s = v->CastToDestinationForType(*this);
							PHangar t = CastToDestinationForType(*v);
							if (!s != !t)
							{
								if (s && s->Store(To<Vehicle>::Cast(shared_from_this()),entityTime))
									return MobilePlacementResult::EnteredStructure;
								else if (t && t->Store(v,entityTime))
									isGood = true;
							}
						}
						if (!isGood)
						{
							if (failIsFatal)
							{
								LogEvent(EventClass::Preset::fatal,this,"Target cell ("+ToString(cell)+") is obstructed by "+v->GetName());
								FATAL__("Critical abort");
							}
							if (!v->HasMotionTasks() || v->ConstructingHere(v->location))
								return MobilePlacementResult::PlacementFailedPermanentlyObstructed;
							else
								return MobilePlacementResult::PlacementFailedTemporarilyObstructed;
						}
					}
				}
				const Grid::LCellIdI old = location;

				#ifdef _DEBUG
					bool hadTasks = HasTasks();
				#endif
			
				location = cell;
	
				MobilePlacementResult result = MobilePlacementResult::PlacementOkay;
				if (old != location)
				{
					if (isCreated)
					{
						if (old.height != Height)
							RemoveVehicleFromGrid(To<Vehicle>::Cast(shared_from_this()),old);
						if (location.height != Height)
							InsertVehicleIntoGrid(To<Vehicle>::Cast(shared_from_this()),location);

						if (constructionRoundCounter)
						{
							constructionRoundCounter = 0;
							RegisterChange(WorldEntity::TasksChanged,entityTime);
						}
					}
					Super::HandleMotion(result,signalEvent,old,timeCost);
				}
				#ifdef _DEBUG
					ASSERT__(Failed(result) || hadTasks == HasTasks());
				#endif
				return result;

			}

	
			bool	Plane::DirectTo(mode_t mode, ArrayRef<Grid::LCellIdI> waypoints, bool detailedPath, bool leftClick /*= false*/)
			{
				switch (mode)
				{
					case CursorMode::Idle:
						return PlotCourse(waypoints,detailedPath);
					break;
					case CursorMode::DigTunnelMode:
						return false;

					case CursorMode::BuildConnectorChainMode:
						if (!GetOperationSpeed(Component::Construct))
							return false;

						if (leftClick)
						{
							/*if (!Grid::LowCellIsClear(destination))
								destination ++;*/
							foreach (waypoints,way)
								if (myTasks.Count() < MaxProductionTasks)
									RegisterNewJob("", PVehicleJob(new VehicleJob::Sequence(*this,mode)),way);
								else
									break;
						}
						else
							PlotCourse(waypoints,detailedPath);
					break;
					case CursorMode::AttackMode:
						if (!CanAttack())
							return false;

						if (leftClick)
						{
							for (const auto&destination : waypoints)
							{
								if (destination != location || turret->trajectoryType == TrajectoryType::VerticalBeam)
								{
									if (turret->trajectoryType != TrajectoryType::VerticalBeam)
									{
										Grid::LCellIdI pointOfOrigin = GetLastPointOfOrigin();
										//attack logic here
						
										if (!CanHitFrom(pointOfOrigin,TrajectoryEndPoint( destination )))
										{
											LogEvent(EventClass::Preset::commandRejected,this,"Destination out of range");
											return false;
										}
									}
									if (!AttackNext(destination,false))
										return false;
								}
								else
									LogEvent(EventClass::Preset::commandRejected,this,"Attempting to attack own location");
							}
						}
						else
							PlotCourse(waypoints,detailedPath);
					break;
				}
				return true;

			}
		#endif


		/*virtual override*/ PHangar	CargoPlane::CastToDestinationForType(const MobileEntity&e)
		{
			if (CanStoreType(e))
			{
				return To<CargoPlane>::Cast(shared_from_this());
			}
			return PHangar();
		}
	
		/*virtual override*/ void	CargoPlane::StoreLocallyFromImport(const PMobileEntity&e)
		{
			ASSERT2__(e->IsVehicle(),GetName(),e->GetName()); 
			float t = 0.f;
			ASSERT__(Hangar::StoreLocally(e,t,false,To<CargoPlane>::Cast(shared_from_this())));

		}
		/*virtual override*/ void	CargoPlane::OnDestroy(RemovalType type, float&t)
		{
			Super::OnDestroy(type,t);
			Hangar::OnDestroy(t);
		}

		/*virtual override*/ String	Plane::DescribeFlatAttackRange() const
		{
			return cache.horizontalAttackRange.DescribeEMPModified(*this,turret->horizontalAttackRange,LuaAttribute<float,NoFilter>());
		}

	
		/*virtual override*/ float	Plane::GetFlatAttackRange() const
		{
			return cache.horizontalAttackRange.GetEMPModified(*this,turret->horizontalAttackRange,LuaAttribute<float,NoFilter>());
			//EMPModify(turret->GetHorizontalAttackRange(owner,base->turretRangeModifier),1.f);
		}


		#ifndef CLIENT
		/*virtual override*/ bool	CargoPlane::Store(const PMobileEntity&e, float&atTime)
		{
			return Hangar::StoreLocally(e,atTime,true,To<CargoPlane>::Cast(shared_from_this()));
		}
		#endif

		#ifdef HAS_WINDOW
			/*virtual override*/ void		CargoPlane::VisitAllCommands(FCommandVisitor visitor, bool force)
			{
				Super::VisitAllCommands(visitor,force);
				if (storeOffset > 0 || force)
					cmdLeft->VisitMembers(visitor,force);
				index_t at = 0;
				foreach (stored,s2)
				{
					if (at >= storeOffset || force)
					{
						int2 coords;
						if (at >= storeOffset)
							coords = cmdLeftCoords + int2(at - storeOffset+1 ,0);
						s2->deployCommand->VisitMembers(visitor,force,coords);
					}

					at++;
					if (at >= storeOffset + MaxVisibleStoreItems && !force)
						break;
				}
				if (stored.Count() - storeOffset > MaxVisibleStoreItems || force)
					cmdRight->VisitMembers(visitor,force);
			}

		#endif

		#ifndef CLIENT

			/*virtual override*/ bool					Plane::FrontQueuePossibleAttacks()
			{
				if (!turret->CanAttack() || !HasAmmoForShot())
				{
					//LogEvent(EventClass::Preset::commandRejected,this,"Insufficient ammunition");
					return false;
				}

				{
					Grid::LCellIdI target;

					const ResourceVector&storage = 
						#ifndef ONE_COMPOUND_PER_FACTION
							this->currentStorage
						#else
							this->owner->compound.total.storage
						#endif
					;

					bool result = false;
					while (		(!NextTaskIsAttack() || (int)myTasks.GetOldest().foci.Count() < CountPossibleAttacks())   
						&& GetSentryTarget(*this, storage.level[turret->ammoType], TrajectoryEndPoint( location),*this, target))
					{
						AttackFirst(target,true);
						result = true;
					}
					return result;


					//if (GetSentryTarget(*this, storage.level[turret->ammoType], TrajectoryEndPoint( location ),*this, target))
					//{
					//	AttackNext(target,true);
					//	return true;
					//}
				}
				//else
					//return false;
				return false;

			}



			/*virtual override*/ bool			Raptor::ExtCanHit(const Grid::LCellIdI&attackFrom,const Grid::LCellIdI&attackTarget,Grid::LCellIdI&actual) const
			{
				return TurretBase::ExtCanHit(owner,TrajectoryEndPoint( attackFrom ),TrajectoryEndPoint( attackTarget ),actual);
			}



			/*virtual override*/ bool	Plane::AttackNext(Grid::LCellIdI target, bool autoTarget)
			{
				ASSERT__(!TasksAreLocked());
				if (!CanAttack())
					return false;
				if (myTasks.Count() >= MaxStructureTasks)
				{
					LogEvent(EventClass::Preset::commandRejected,this,"Max queue length ("+String(MaxStructureTasks)+") reached");
					return false;
				}
				Grid::LCellIdI pointOfOrigin = GetLastPointOfOrigin();
				if (target == pointOfOrigin && turret->trajectoryType != TrajectoryType::VerticalBeam)
				{
					LogEvent(EventClass::Preset::commandRejected,this,"Destination too close");
					return false;
				}
				if (turret->trajectoryType != TrajectoryType::VerticalBeam)
				{
					//Grid::LCellIdI pointOfOrigin = location+1;


					//float targetDistance = Grid::GetGeometricDistance(pointOfOrigin,target);

					if (!CanHitFrom(pointOfOrigin,TrajectoryEndPoint( target )))
					{
						LogEvent(EventClass::Preset::commandRejected,this,"Destination out of range");
						return false;
					}
				}

				if (!HasTasks() || !myTasks.GetNewest().IsMode(CursorMode::AttackMode))
				{
					myTasks << Task("Attack Position",target,false,Task::Mode,CursorMode::AttackMode);
					myTasks.GetNewest().Replot(pointOfOrigin,this,FocusChangeType::FullChange,true);
					RegisterChange(TasksChanged,-1);
				}
				else
				{
					//if (!tasks.GetNewest().foci.Contains(destination))	//irrelevant. attack same location as often as you want
					{
						myTasks.GetNewest().foci.Push().target = target;
						myTasks.GetNewest().Replot(pointOfOrigin,this,FocusChangeType::FocusAppended,true);
						RegisterChange(TasksChanged,-1);
					}
				}
				return true;

			}

			/*virtual override*/ bool	Plane::AttackFirst(Grid::LCellIdI target, bool autoTarget)
			{
				ASSERT__(!TasksAreLocked());
				if (!CanAttack())
					return false;
				if (myTasks.Count() >= MaxStructureTasks)
				{
					LogEvent(EventClass::Preset::commandRejected,this,"Max queue length ("+String(MaxStructureTasks)+") reached");
					return false;
				}
				Grid::LCellIdI pointOfOrigin = location;
				if (target == pointOfOrigin && turret->trajectoryType != TrajectoryType::VerticalBeam)
				{
					LogEvent(EventClass::Preset::commandRejected,this,"Destination too close");
					return false;
				}
				if (turret->trajectoryType != TrajectoryType::VerticalBeam)
				{
					//Grid::LCellIdI pointOfOrigin = location+1;


					//float targetDistance = Grid::GetGeometricDistance(pointOfOrigin,target);

					if (!CanHitFrom(pointOfOrigin,TrajectoryEndPoint( target )))
					{
						LogEvent(EventClass::Preset::commandRejected,this,"Destination out of range");
						return false;
					}
				}

				if (!HasTasks() || !myTasks.GetOldest().IsMode(CursorMode::AttackMode))
				{
					myTasks.PushFront(Task("Attack Position",target,false,Task::Mode,CursorMode::AttackMode));
					myTasks.GetOldest().Replot(pointOfOrigin,this,FocusChangeType::FullChange,true);
					RegisterChange(TasksChanged,-1);
				}
				else
				{
					//if (!tasks.GetNewest().foci.Contains(destination))	//irrelevant. attack same location as often as you want
					{
						myTasks.GetOldest().foci.PushFront().target = target;
						myTasks.GetOldest().Replot(pointOfOrigin,this,FocusChangeType::FocusAppended,true);
						RegisterChange(TasksChanged,-1);
					}
				}
				return true;

			}

		#endif

	}

}

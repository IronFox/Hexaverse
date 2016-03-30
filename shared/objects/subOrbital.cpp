#include <global_root.h>
#include "subOrbital.h"
#include "gridAndChoices.h"
#include "faction.h"

namespace Objects
{

	namespace SubOrbital
	{
		Buffer0<PPlane>	planes;

		/*static*/ count_t			Plane::CountTurretOccurrences(const OwnerParameter&o,Component::Turret*t)
		{
			count_t rs = 0;
			for (const auto&m : planes)
				if (m->owner == o && m->turret == t)
					rs++;
			return rs;
		}




		/*virtual override*/ void		Plane::RegisterSensory(const Grid::LCellIdI&loc, float&atTime, bool checkChangesNow/*=true*/, Buffer0<PWorldEntity>*newContacts /*= nullptr*/)
		{
			Super::RegisterSensory(loc,atTime,checkChangesNow && turret->groundScanningRange <= 0, newContacts);
			#ifdef CLIENT
				if (isOwnedByMe)
					Grid::visibleSubOrbitGrid.ChangeSensory(loc,GetScanningRange(),+1);
			#endif
			if (turret->groundScanningRange > 0.f)
			{
				float range = EMPModify(turret->groundScanningRange,2);
				Grid::actualWorld.MapSensorState(loc.stackID,range,loc.height,turret->groundScannerPenetration,skySensorState);
				Grid::actualWorld.UpdateSensorState(skySensorState);
				#ifdef CLIENT
					if (isOwnedByMe)
					{
						LogEvent(EventSignificance::Debug,this,"Registering sky sensory centered on "+loc.ToString()+", range="+String(range));
						Grid::actualWorld.InsertSensorCoverage(skySensorState);
					}
				#else
					owner->RegisterSensorCreation(skySensorState,atTime,checkChangesNow,newContacts);
				#endif
			}
		}

		/*virtual override*/ void		Plane::UnregisterSensory(const Grid::LCellIdI&loc, float&atTime, bool checkChangesNow/*=true*/)
		{
			Super::UnregisterSensory(loc,atTime,checkChangesNow && turret->groundScanningRange <= 0);
			#ifdef CLIENT
				if (isOwnedByMe)
					Grid::visibleSubOrbitGrid.ChangeSensory(loc,GetScanningRange(),-1);
			#endif
			if (turret->groundScanningRange > 0.f)
			{
				#ifdef CLIENT
					if (isOwnedByMe)
					{
						LogEvent(EventSignificance::Debug,this,"Unregistering sky sensory, hopefully centered on "+loc.ToString());
						Grid::actualWorld.RemoveSensorCoverage(skySensorState);
					}
				#else
					owner->RegisterSensorRemoval(skySensorState,atTime,checkChangesNow);
				#endif
			}
		}


		
		#ifdef HAS_WINDOW



			/*virtual override*/ void Plane::DescribeType(BaseCommand::Label&outResult, UINT32 descFlags)
			{
				Super::DescribeType(outResult,descFlags);
				outResult << "";
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
				outResult<<"";
				outResult<< "Sub orbital planes move above the regular terrain. They can scan the surface, and are invisible to ordinary radar."
					;
			}
		


		#endif





		/*static*/ int				Plane::GetCentralGroundHeight(const Grid::LCellIdI&location)
		{
			UINT32 fine = Grid::low.grid.GetCell(location.stackID).higherEquivalent;
			return Detail::WalkStack(Grid::actualWorld.cellStack[fine],false);
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
				index_t finer = Grid::low.grid.GetCell(satelliteLocation.stackID).higherEquivalent;
				const auto&c = Grid::high.grid.GetCell(finer);
			#else
				index_t finer = who->location.stackID;
			#endif
			int h = Detail::WalkStack(Grid::actualWorld.cellStack[finer],true);
			if (h == badHeight)
				return badHeight;
			#ifdef HYBRID_GRID
				for (int j = 0; j < c.neighborhood.numNeighbors; j++)
				{
					int h2 = Detail::WalkStack(Grid::actualWorld.cellStack[c.neighborhood.neighborID[j]],true);
					if (h2 == badHeight)
						return badHeight;
					h = std::max(h2,h);
				}
			#endif
			return h <= Grid::CellStack::TopHeight ? h : badHeight;
		}





		#ifndef CLIENT
			bool					Plane::RefreshGroundSensor(float&atTime, bool checkChangesNow/*=true*/, Buffer0<PWorldEntity>*newContacts/* = nullptr*/)
			{
				if (turret->groundScanningRange > 0.f)
				{
					Grid::SkySensorState state = skySensorState;
					if (Grid::actualWorld.UpdateSensorState(skySensorState))
					{
						LogEvent(EventSignificance::Common,this,"Ground-sensor change detected");

						owner->RegisterSensorCreation(skySensorState,atTime,false, newContacts);
						atTime += timeEpsilon;
						owner->RegisterSensorRemoval(state,atTime,checkChangesNow);
						atTime += timeEpsilon;
						return true;
					}

				}
				return false;
			}

			/*virtual override*/ void		Plane::MoveSensory(const Grid::LCellIdI&oldLocation, const Grid::LCellIdI&newLocation, float&atTime, bool checkChangesNow/*=true*/, Buffer0<PWorldEntity>*newContacts/* = nullptr*/)
			{
				if (oldLocation == newLocation)
					return;
				Super::RegisterSensory(location,motionTime,false,newContacts);
				Super::UnregisterSensory(oldLocation,motionTime,false);
				if (turret->groundScanningRange > 0)
				{
					ASSERT__(newLocation == location);
					Grid::SkySensorState state = skySensorState;
					LogEvent(EventSignificance::Common,this,"Processing ground-sensor motion");
					float range = EMPModify(turret->groundScanningRange,2);
					Grid::actualWorld.MapSensorState(newLocation.stackID,range,newLocation.height,turret->groundScannerPenetration,skySensorState);
					Grid::actualWorld.UpdateSensorState(skySensorState);

					owner->RegisterSensorCreation(skySensorState,atTime,false, newContacts);
					atTime += timeEpsilon;
					owner->RegisterSensorRemoval(state,atTime,checkChangesNow);
					atTime += timeEpsilon;
				}
			}


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
				
				if (cell.height <= Grid::CellStack::TopHeight && !Grid::actualWorld.LowCellIsClear(cell))
				{
					PHangar s = MobileDestinationStructureHere(cell,*this);
					if (s && s->Store(To<Vehicle>::Cast(shared_from_this()),motionTime))
						return MobilePlacementResult::EnteredStructure;

					if (failIsFatal)
					{
						PStructure st = GetStructure(cell);
						LogEvent(EventClass::Preset::fatal,this,"Target cell ("+cell.ToString()+") is obstructed by structure "+(st ? st->GetName(): "<>")+": "+Grid::actualWorld.LowCellReport(cell));
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
								if (s && s->Store(To<Vehicle>::Cast(shared_from_this()),motionTime))
									return MobilePlacementResult::EnteredStructure;
								else if (t && t->Store(v,motionTime))
									isGood = true;
							}
						}
						if (!isGood)
						{
							if (failIsFatal)
							{
								LogEvent(EventClass::Preset::fatal,this,"Target cell ("+cell.ToString()+") is obstructed by "+v->GetName());
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
							RegisterChange(WorldEntity::TasksChanged,motionTime);
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
								if (destination.stackID != location.stackID || turret->trajectoryType == TrajectoryType::VerticalBeam)
								{
									if (turret->trajectoryType != TrajectoryType::VerticalBeam)
									{
										Grid::LCellIdI pointOfOrigin = GetLastPointOfOrigin();
										//attack logic here
						
										if (!CanHitFrom(pointOfOrigin,destination))
										{
											LogEvent(EventClass::Preset::commandRejected,this,"Destination out of range");
											return false;
										}
									}
									if (!AttackNext(destination,false))
										return false;
								}
								else
									LogEvent(EventClass::Preset::commandRejected,this,"Attempting to attack current location");
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
					cmdLeft.VisitMembers(visitor,force);
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
					cmdRight.VisitMembers(visitor,force);
			}

		#endif

		#ifndef CLIENT

			/*virtual override*/ bool					Plane::FillAttackQueue()
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

					if (GetSentryTarget(*this, storage.level[turret->ammoType], location,0.f,*this, target))
					{
						AttackNext(target,true);
						return true;
					}
				}
				//else
					//return false;
				return false;

			}



			/*virtual override*/ bool			Raptor::ExtCanHit(const Grid::LCellIdI&attackFrom,const Grid::LCellIdI&attackTarget,Grid::LCellIdI&actual) const
			{
				return TurretBase::ExtCanHit(owner,attackFrom,0.f,attackTarget,actual);
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
				if (target.stackID == pointOfOrigin.stackID && turret->trajectoryType != TrajectoryType::VerticalBeam)
				{
					LogEvent(EventClass::Preset::commandRejected,this,"Destination too close");
					return false;
				}
				if (turret->trajectoryType != TrajectoryType::VerticalBeam)
				{
					//Grid::LCellIdI pointOfOrigin = location+1;


					//float targetDistance = Grid::GetGeometricDistance(pointOfOrigin,target);

					if (!CanHitFrom(pointOfOrigin,target))
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
		#endif

	}

}

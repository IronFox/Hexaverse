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
						Grid::actualWorld.RemoveSensorCoverage(skySensorState);
					}
				#else
					owner->RegisterSensorRemoval(skySensorState,atTime,checkChangesNow);
				#endif
			}
		}

		#ifndef CLIENT


			bool					Plane::RefreshGroundSensor(float&atTime)
			{
				if (turret->groundScanningRange > 0.f)
				{
					Grid::SkySensorState state = skySensorState;
					if (Grid::actualWorld.UpdateSensorState(skySensorState))
					{
						LogEvent(EventSignificance::Common,this,"Ground-sensor change detected");

						owner->RegisterSensorCreation(skySensorState,atTime,false);
						atTime += timeEpsilon;
						owner->RegisterSensorRemoval(state,atTime,true);
						atTime += timeEpsilon;
						return true;
					}

				}
				return false;
			}


		#endif

		
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
				outResult<< "Sub orbital planes hover high above the surface on a singular residual height level."
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



			/*virtual override*/ bool		Plane::ProcessRound(float&atTime)
			{
				return true;
			}

			/*virtual override*/ MobilePlacementResult	Plane::PlaceExactlyAt(const Grid::LCellIdI&_cell, float timeCost, float minTimeOffset, bool signalEvent/*=true*/, bool isCreated/*=true*/, bool failIsFatal/*=false*/)
			{
				UINT32 cell = _cell.stackID;
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

				if (location.stackID != cell) 
				{
					PPlane v = GetPlane(cell);
					if (v)
					{
						if (failIsFatal)
						{
							LogEvent(EventClass::Preset::fatal,this,"Target cell ("+String(cell)+") is obstructed by plane");
							FATAL__("Critical abort");
						}
						if (!v->HasMotionTasks() || v->ConstructingHere(v->location))
							return MobilePlacementResult::PlacementFailedPermanentlyObstructed;
						else
							return MobilePlacementResult::PlacementFailedTemporarilyObstructed;
					}
				}
				const Grid::LCellIdI old = location;

				#ifdef _DEBUG
					bool hadTasks = HasTasks();
				#endif
			
				location = _cell;
	
				MobilePlacementResult result = MobilePlacementResult::PlacementOkay;
				if (old != location)
				{
					if (isCreated)
					{
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



		/*virtual override*/ bool					Plane::HasAmmoForShot() const
		{
		#ifdef ONE_COMPOUND_PER_FACTION
			const Compound&compound = owner->compound;
			return compound.total.storage.level[turret->ammoType] >= turret->ammoNeedPerShot;
		#else
			return currentStorage.level[turret->ammoType] >= turret->ammoNeedPerShot;
		#endif
		}

	
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

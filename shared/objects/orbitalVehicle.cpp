#include <global_root.h>
#include "orbitalVehicle.h"
#include "gridAndChoices.h"
#include "faction.h"

namespace Objects
{
	namespace Orbital
	{
		Buffer0<PShip>	ships;
		//bool					satellitesAreLocked = false;


		/*static*/ count_t			Ship::CountTurretOccurrences(const OwnerParameter&o,Component::Turret*t)
		{
			count_t rs = 0;
			for (const auto&m : ships)
				if (m->owner == o && m->turret == t)
					rs++;
			return rs;
		}



		///*virtual override*/ bool				Ship::CanSee(const PWorldEntity&other)	const
		//{
		//	if (Super::CanSee(other))
		//		return true;
		//	if (!turret->groundScanningRange)
		//		return false;
		//	UINT32 higher = Grid::low.grid.GetCell(other->location.stackID).higherEquivalent;
		//	int h;
		//	if (skySensorState.coverageMap.Query(higher,h))
		//	{
		//		return h <= other->location.height;
		//	}
		//	return false;
		//}


		///*virtual override*/ bool				Ship::CanSee(const TVec3<>&center, float radius)	const
		//{
		//	if (Super::CanSee(center,radius))
		//		return true;

		//	if (!turret->groundScanningRange)
		//		return false;
		//	
		//	UINT32 higher = Grid::low.grid.GetCell(location.stackID).higherEquivalent;
		//	int h;
		//	if (!skySensorState.coverageMap.Query(higher,h))
		//		return false;	//this would be a bug... ah, whatever

		//	float r0 = Grid::ToRadius(h);
		//	const TVec3<>&n = Grid::low.cells[location.stackID].cornerCenter;
		//	float vertical = Vec::dot(n,center) - r0;
		//	if (vertical + radius < 0.f)
		//		return false;
		//	TVec3<> base = n*vertical;
		//	float d2 = Vec::quadraticDistance(base,center);
		//	float range = EMPModify(turret->groundScannerPenetration,2);
		//	return d2 <= sqr(range+radius);
		//}


		/*virtual override*/ void		Ship::RegisterSensory(const Grid::LCellIdI&loc, float&atTime, bool checkChangesNow/*=true*/, Buffer0<PWorldEntity>*newContacts /*= nullptr*/)
		{
			Super::RegisterSensory(loc,atTime,checkChangesNow && turret->groundScanningRange <= 0, newContacts);
			#ifdef CLIENT
				if (isMine)
					Grid::visibleOrbitGrid.ChangeSensory(loc,GetScanningRange(),+1);
			#endif
			if (turret->groundScanningRange > 0.f)
			{
				float range = EMPModify(turret->groundScanningRange,2);
				Grid::actualWorld.MapSensorState(loc.stackID,range,loc.height,turret->groundScannerPenetration,skySensorState);
				Grid::actualWorld.UpdateSensorState(skySensorState);
				#ifdef CLIENT
					if (isMine)
					{
						Grid::actualWorld.InsertSensorCoverage(skySensorState);
					}
				#else
					owner->RegisterSensorCreation(skySensorState,atTime,checkChangesNow,newContacts);
				#endif
			}
		}

		/*virtual override*/ void		Ship::UnregisterSensory(const Grid::LCellIdI&loc, float&atTime, bool checkChangesNow/*=true*/)
		{
			Super::UnregisterSensory(loc,atTime,checkChangesNow && turret->groundScanningRange <= 0);
			#ifdef CLIENT
				if (isMine)
					Grid::visibleOrbitGrid.ChangeSensory(loc,GetScanningRange(),-1);
			#endif
			if (turret->groundScanningRange > 0.f)
			{
				#ifdef CLIENT
					if (isMine)
					{
						Grid::actualWorld.RemoveSensorCoverage(skySensorState);
					}
				#else
					owner->RegisterSensorRemoval(skySensorState,atTime,checkChangesNow);
				#endif
			}
		}

		#ifndef CLIENT


			bool					Ship::RefreshGroundSensor(float&atTime)
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

	
			/*virtual override*/ void Ship::DescribeType(BaseCommand::Label&outResult, UINT32 descFlags)
			{
				Super::DescribeType(outResult,descFlags);
				outResult << "";
				String speed = "Speed: "+String(this->GetMovementStepsPerRound()) + " (orbital)";

				outResult << speed;
				int opSpeed = GetOperationSpeed(Component::Construct);
				if (opSpeed)
					outResult << "Effective construction speed: "+String(ResourceTag(Resource::Minerals))+String(opSpeed)+" per round";

				const String fuel (ResourceTag(Resource::Fuel));


				outResult.MarkBriefEnd();
				outResult<<"";
				outResult<< "Satellites hover high above the surface on a singular residual height level. They constantly consume "+fuel+"fuel to preserve their height."
					"Short term failure to supply the required amount of "+fuel+" will result in height decline. Extended periods of starvation will result in satellite loss.";
			}
		


		#endif






		/*static*/ int				Ship::GetGroundHeight(const Grid::LCellIdI&location)
		{
			UINT32 fine = Grid::low.grid.GetCell(location.stackID).higherEquivalent;
			return Grid::actualWorld.cellStack[fine].FindTopNonClear(false);
		}


		int						Ship::GetGroundHeight() const
		{
			return GetGroundHeight(location);
		}


		#ifdef HAS_WINDOW

		#endif





		#ifdef ONE_COMPOUND_PER_FACTION
			#if defined HAS_WINDOW || !defined CLIENT
				PSkyHub	Ship::GetClosestExternalSupplier(float&quadraticDistance)
				{
					float distance = std::numeric_limits<float>::max();
					PSkyHub found;
					#ifdef CLIENT
						for (const auto&port : skyLinks)
							if (port.second->owner == owner && port.first->connectedToHub)
							{
								PSkyHub hub = port.first->connectedToHub;
								if (hub.get() != this)
								{
									float d2 = Vec::quadraticDistance(hub->system.w.xyz,system.w.xyz);
									if (d2 < distance)
									{
										distance = d2;
										found = hub;
									}
								}
							}
					#else
						for (const auto&sat : owner->skyHubs)
						{
							PSkyHub hub = sat.lock();
							if (hub && hub.get() != this && !hub->connectedTo.expired())
							{
								float d2 = Vec::quadraticDistance(hub->system.w.xyz,system.w.xyz);
								if (d2 < distance)
								{
									distance = d2;
									found = hub;
								}
							}
						}
					#endif
					quadraticDistance = distance;
					return found;
				}
			#endif
		#else

		#endif


		#ifndef CLIENT




			/*virtual override*/ MobilePlacementResult	Ship::PlaceExactlyAt(const Grid::LCellIdI&_cell, float timeCost, float minTimeOffset, bool signalEvent/*=true*/, bool isCreated/*=true*/, bool failIsFatal/*=false*/)
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
					Orbital::PShip s =  Orbital::GetShip(cell);
					Orbital::PStation st = Orbital::GetStation(cell);
					if (s || st)
					{
						if (failIsFatal)
						{
							LogEvent(EventClass::Preset::fatal,this,"Target cell ("+String(cell)+") is obstructed by orbital");
							FATAL__("Critical abort");
						}
						if (st)
							return MobilePlacementResult::PlacementFailedPermanentlyObstructed;
						else
						{
							if (!s->HasMotionTasks() || s->ConstructingHere(s->location))
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
			
				location = _cell;
	

				MobilePlacementResult result = MobilePlacementResult::PlacementOkay;
				if (old != location)
				{
					if (isCreated)
					{
						Orbital::RemoveShipFromGrid(To<Ship>::Cast(shared_from_this()),old.stackID);
						Orbital::InsertShipIntoGrid(To<Ship>::Cast(shared_from_this()),location.stackID);

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

	
			bool	Ship::DirectTo(mode_t mode, ArrayRef<Grid::LCellIdI> waypoints, bool detailedPath, bool leftClick /*= false*/)
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


		#if 0

		/*virtual override*/ bool					Guardian::CanHitFrom(const Grid::LCellIdI&origin, const TVec3<>&dest) const
		{
			return Turret::CanHit(TrajectoryType::Missile,FlatRange,location,0.f,dest);

		}
		/*virtual override*/ bool					Guardian::CanHitFrom(const Grid::LCellIdI&origin, const Grid::LCellIdI&dest) const
		{
			return Turret::CanHit(TrajectoryType::Missile,FlatRange,location,0.f,dest);

		}

		/*virtual override*/ bool					Guardian::ExtCanHit(const Grid::LCellIdI&attackFrom,const Grid::LCellIdI&attackTarget,Grid::LCellIdI&actual) const
		{
			//const float velocity = Turret::MaxRangeToVelocity(GetFlatAttackRange());
			const float range = GetFlatAttackRange();
			//const float extRange = range + (turret->explosionRadius);
			//const float range2 = sqr(range);
			//const float extRange2 = sqr(extRange);
			//const TVec3<> source3 = Grid::ToWorld(source);
			TVec3<> dest3 = Grid::ToWorld(destIn);
			//float distance2 = Vec::quadraticDistance(source3,dest3);
			//if (Turret::GetAngleForDistance(velocity,source,destIn))
			//if (distance2 > extRange2)
			//	return false;
			const float explosionRadius = turret->explosionRadius;
			const float explosionRadius2 = sqr(explosionRadius);
			if (Turret::CanHit(turret->trajectoryType, range,source,zOffset,destIn)
				&& CheckArea(myFaction,destIn,explosionRadius))
			{
				destOut = destIn;
				return true;
			}

			if (!Turret::CanHit(turret->trajectoryType, range+explosionRadius*2,source,zOffset,destIn))
				return false;

			AreaHelper	helper;
			//queue << destIn;
			{
				const auto&n = Grid::low.grid.GetCell(destIn.stackID).neighborhood;
				for (int i = 0; i < n.numNeighbors; i++)
					helper << Grid::LCellIdI(n.neighborID[i],destIn.height);
				helper << destIn + 1;
				helper << destIn - 1;
			}
			Grid::LCellIdI current;
			while (helper >> current)
			{
				TVec3<> world = Grid::ToWorld(current);
				float fromDest2 = Vec::quadraticDistance(dest3,world);
				if (fromDest2 > sqr(turret->explosionRadius))
					continue;
				if (!Grid::actualWorld.LowCellIsClear(current) || 
					!Turret::CanHit(turret->trajectoryType, range,source,zOffset,current))
					continue;
				//float fromSource2 = Vec::quadraticDistance(source3,world);
				//if (fromSource2 > extRange2)
					//continue;



				if (CheckArea(myFaction,current,explosionRadius))
				{
					destOut = current;
					return true;
				}
				{
					const auto&n = Grid::low.grid.GetCell(current.stackID).neighborhood;
					for (int i = 0; i < n.numNeighbors; i++)
						helper << Grid::LCellIdI(n.neighborID[i],current.height);
					helper << current + 1;
					helper << current - 1;
				}
			}
			return false;



		}

		#endif /*0*/


		/*virtual override*/ bool					Ship::HasAmmoForShot()
		{
		#ifdef ONE_COMPOUND_PER_FACTION
			const Compound&compound = owner->compound;
			return compound.total.storage.level[turret->ammoType] >= turret->ammoNeedPerShot;
		#else
			return currentStorage.level[turret->ammoType] >= turret->ammoNeedPerShot;
		#endif
		}

	
		#ifndef CLIENT
			/*virtual override*/ bool					Ship::FillAttackQueue()
			{
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



			///*virtual override*/ bool			Guardian::ExtCanHit(const Grid::LCellIdI&attackFrom,const Grid::LCellIdI&attackTarget,Grid::LCellIdI&actual) const
			//{
			//	return TurretBase::ExtCanHit(owner,attackFrom,0.f,attackTarget,actual);
			//}



			/*virtual override*/ bool	Ship::AttackNext(Grid::LCellIdI target, bool autoTarget)
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


			///*virtual override*/ bool Guardian::WouldAttack(const PWorldEntity&target, Grid::LCellIdI&outTarget, float entityTime)
			//{
			//	if (!HasAmmoForShot() && empDamageLevel == 0)
			//	{
			//		//LogEvent(EventClass::Preset::commandRejected,this,"Insufficient ammunition");
			//		return false;
			//	}

			//	static Buffer0<TVec4<> > trajectory;

			//	TVec3<> iat;
			//	Grid::HCellIdI ic;


			//	//float d2 = Grid::GetQuadraticGeometricDistance(location+1,target->location);

			//	return		//owner->ExtCanSee(target,entityTime)
			//			/*&&	*/
			//				turret->CanAttack() 
			//			&& timeAt <= entityTime 
			//			&& CountPossibleAttacks() > 0 
			//			&& ExtCanHit(location,target->location,outTarget)
			//			//&& d2 > sqr(turret->explosionRadius)
			//			&& TraceTrajectory(owner->currentState, target->location,trajectory,iat,ic)==TurretTrajectory::ReachesTarget;
			//}


		


		#endif


	}

}

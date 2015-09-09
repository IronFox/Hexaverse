#include <global_root.h>
#include "vehicle.h"
#include "rendering.h"
#include "prototypes.h"
#include "systems.h"
#include "gridAndChoices.h"
#if !defined CLIENT || defined CONNECTOR_LESS
	#include "faction.h"
#endif

namespace Objects
{
	Buffer0<PVehicle>				vehicles;

	#ifndef CLIENT
		bool						newVehiclesHaveFullAmmo = true;

	#endif

	Grid::THighDistanceField		vehicleDistanceField;

	/*void						ForeachVehicle(const std::function<void(const PVehicle&)>&f)
	{
		for (const auto&v : vehicles)
			f(v);
		for (const auto&v : storedVehicles)
			f(v);
	}
*/


	/*static*/ count_t			Vehicle::CountTurretOccurrences(const OwnerParameter&o,Component::Turret*t)
	{
		count_t rs = 0;
		for (const auto&m : vehicles)
			if (m->owner == o && m->turret == t)
				rs++;
		return rs;
	}



	
	int						Vehicle::GetConstructionSpeed() const
	{
		return base->constructionSpeedMultiplier * turret->constructionSpeed;
	}


	

	#ifndef NO_HIBERNATION
		/*virtual override*/ int				Vehicle::GetUplinkTraffic() const
		{
			return turret->uplinkTraffic + 5;

		}
	#endif

	/*static*/ int					Vehicle::GetPrice(Component::VehicleBase*base, Component::Turret*turret)
	{
		return (turret ? turret->productionCosts : 0) + (base ? base->productionCosts : 0);
	}




	
	#ifndef CLIENT


		/*virtual override*/ void		Vehicle::OnConstructionFinished(float atTime)
		{
			if (turret->CanAttack())
			{
				//leave munitions here. it seems fair that a new tank starts with full ammo, but an emp-tank should not start with full uranium...
				#if !defined CONNECTOR_LESS && !defined NO_MUNITION
					if (newVehiclesHaveFullAmmo)
						ChangeStorage(*this,atTime,Resource::Munitions,GetMaxStorageCapacity().level[Resource::Munitions] - currentStorage.level[Resource::Munitions]);
				#endif

				//
				//currentStorage.level[Resource::Munitions] = GetMaxStorageCapacity().level[Resource::Munitions];
				//LogEvent(EventType::MinorEntityProcess,this,"Munition storage changed to "+String(currentStorage.level[Resource::Munitions]));
			}
		}

		/*virtual override*/ bool Vehicle::FillAttackQueue()
		{
			//index_t freeSlots = attackQueue.Count() < t->GetAttacksPerRound() <  t->GetAttacksPerRound() - t->attackQueue.Count() : 0;
			//if (attackQueue.Count() < CountPossibleAttacks())
			{
				Grid::LCellIdI target;

				const ResourceVector&storage = 
					#ifndef CONNECTOR_LESS
						this->currentStorage
					#else
						this->owner->compound.total.storage
					#endif
				;

				if (GetSentryTarget(*this, storage.level[turret->ammoType], location,base->height,*this, target))
				{
					EnqueueAttack(target);
					return true;
				}
			}
			//else
				//return false;
			return false;

		}





	#endif

	
	#ifndef CONNECTOR_LESS

	ResourceVector			Vehicle::GetMaxStorageCapacity()	const
	{
		ResourceVector rs = turret->baseStorageCapacity;
		rs *= base->storageMultiplier;
		return rs;
	}

	#endif


	/*virtual override*/int			Vehicle::GetMovementStepsPerRound()	const
	{
		return (int)ceil(EMPModify(
				float(base->baseMovementSpeed) 
				* Grid::powi(0.75f,std::max(0, turret->weight - base->carryingCapacity)),
				1.f));
	}

	

	#ifdef CLIENT
	#else
		void Vehicle::PositionOnTop(const Grid::LCellIdI&cell, bool signalEvent)
		{
			int h = FindApplicableGround(cell,base->moveOnWater);
			ASSERT__(h != badHeight);
			MobilePlacementResult rs = PlaceExactlyAt(Grid::LCellIdI(cell.stackID, h),0,0,signalEvent,false);
			ASSERT_EQUAL__(rs, MobilePlacementResult::PlacementOkay);
			ClearAllTasks(true,CLOCATION);
		}


		/*virtual override*/ MobilePlacementResult Vehicle::PlaceExactlyAt(const Grid::LCellIdI&cell, float timeCost, float minTimeOffset, bool signalEvent/*=true*/, bool isCreated/*=true*/, bool failIsFatal/*=false*/)
		{
			if (health <= 0)
			{
				if (failIsFatal)
				{
					LogEvent(EventType::Fatal,this,"Dead");
					FATAL__("Critical abort");
				}
				return MobilePlacementResult::PlacementFailedHarmfulOrBlocked;
			}
			
			//VerifyVisibilityNetwork();
			timeAt = std::max(minTimeOffset,timeAt);
			if (!Grid::actualWorld.LowCellIsClear(cell))
			{
				PHangarStructure s = MobileDestinationStructureHere(cell,*this);
				if (s)
				{
					PVehicleStore st = To<VehicleStore>::DynamicCast(s);
					ASSERT1__(st,s->ToEntity().GetName());
					if (st && st->StoreInPark(To<Vehicle>::Cast(shared_from_this()),timeAt))
					{
						return MobilePlacementResult::EnteredStructure;
					}
				}
				if (failIsFatal)
				{
					LogEvent(EventType::Fatal,this,"Target cell ("+cell.ToString()+") is obstructed by structure");
					FATAL__("Critical abort");
				}

				return MobilePlacementResult::PlacementFailedHarmfulOrBlocked;
			}
			if ((location != cell && !NoVehicleHere(cell)))
			{
				if (failIsFatal)
				{
					LogEvent(EventType::Fatal,this,"Target cell ("+cell.ToString()+") is obstructed by vehicle");
					FATAL__("Critical abort");
				}
				return MobilePlacementResult::PlacementFailedObstructed;
			}
			const Grid::LCellIdI old = location;

			#ifdef _DEBUG
				bool hadTasks = HasTasks();
			#endif
			
			location = cell;
			int fallingHeight = 0;
			while (location.height-fallingHeight > -Grid::CellStack::HalfRange 
					&& Grid::actualWorld.LowCellIsClear(Grid::LCellIdI(location.stackID, location.height - fallingHeight - 1))
					&& NoVehicleHere(Grid::LCellIdI(location.stackID, location.height - fallingHeight - 1)))
				fallingHeight ++;
			//if (fallingHeight > 0)
			{
				timeCost += Times::Fall * fallingHeight;
				location = location - fallingHeight;
				if (!CanClimb(-fallingHeight) || !CanStandOn(Grid::actualWorld,location-1,false,false))
				{
					if (cell.stackID != old.stackID)	//at least some horizontal motion
					{
						location = old;
						PlaceExactlyAt(location,0,minTimeOffset,signalEvent,isCreated);
						if (failIsFatal)
						{
							LogEvent(EventType::Fatal,this,"Target cell ("+cell.ToString()+") is unsuitable for placement");
							FATAL__("Critical abort");
						}

						return MobilePlacementResult::PlacementFailedHarmfulOrBlocked;
					}
					if (isCreated)
					{
						RemoveVehicleFromGrid(To<Vehicle>::Cast(shared_from_this()),old);
						//RemoveFromGrid(vehicleGrid,old,To<Vehicle>::Cast(shared_from_this()));
						//InsertIntoGrid(vehicleGrid,location,To<Vehicle>::Cast(shared_from_this()));

						if (timeCost > 0)
							SignalMotion(To<Vehicle>::Cast(shared_from_this()),old,location,timeAt,timeCost);

						timeAt += timeCost;
						//RemoveFromGrid(vehicleGrid,location,To<Vehicle>::Cast(shared_from_this()));
					}
					LogEvent(EventType::CommandRejected,this, "Unable to relocate here. Vehicle destroyed.");
					Remove(shared_from_this(),RemovalType::UncontrolledExplosion,timeAt,false,true);

					if (failIsFatal)
					{
						LogEvent(EventType::Fatal,this,"Target cell ("+cell.ToString()+") is unsuitable for placement. Destruction event");
						FATAL__("Critical abort");
					}

					return MobilePlacementResult::PlacementFailedHarmfulOrBlocked;
				}

			}
			MobilePlacementResult result = MobilePlacementResult::PlacementOkay;
			if (old != location)
			{
				if (isCreated)
				{
					RemoveVehicleFromGrid(To<Vehicle>::Cast(shared_from_this()),old);
					InsertVehicleIntoGrid(To<Vehicle>::Cast(shared_from_this()),location);

					if (constructionRoundCounter)
					{
						constructionRoundCounter = 0;
						RegisterChange(WorldEntity::TasksChanged,timeAt);
					}
				}
				if (fallingHeight > 0)
				{
					result = MobilePlacementResult::PlacementOkayFellDown;
				}
				this->UpdateSystem();
				if (signalEvent)
				{

					static GenericHashSet<OwnerParameter,OwnerHash>	checkStillBeeingSeenBy;
					static Buffer0<PWorldEntity> checkCanStillSee;
					checkCanStillSee.Clear();
					checkStillBeeingSeenBy.Clear();

					float oldTime = timeAt;
					timeAt += timeCost;

					for (index_t i = 0; i < seeing.Count(); i++)
					{
						const PWorldEntity&other = seeing[i];
	
						if (!CanSee(other))
						{
							ASSERT__(other->seenBy.FindAndErase(shared_from_this()));


							bool found = false;
							foreach (other->seenBy,s2)
							{
								if ((*s2)->owner == owner)
								{
									found = true;
									break;
								}
							}
							if (!found)
							{
								PMobileEntity v = To<MobileEntity>::DynamicCast(other);
								if (v)
									timeAt = std::max(timeAt, v->timeAt);
								owner->SignalDisappearance(other,timeAt);
							}

							seeing.Erase(i--);
						}
					}
					for (index_t i = 0; i < seenBy.Count(); i++)
					{
						const PWorldEntity&other = seenBy[i];
	
						if (!other->CanSee(shared_from_this()))
						{
							ASSERT__(other->seeing.FindAndErase(shared_from_this()));
							checkStillBeeingSeenBy.set(other->owner);
							seenBy.Erase(i--);
						}
					}

			
					foreach (seenBy,s)
						checkStillBeeingSeenBy.Unset((*s)->owner);

				
					checkStillBeeingSeenBy.VisitAllKeys([this,timeCost](const OwnerParameter&id)
					{
						//SignalDisappearanceToHostileFaction(id,shared_from_this(),timeAt+timeCost);
						id->SignalDisappearance(shared_from_this(),timeAt);
					});


					foreach (vehicles,_v)
						ProcessVisibilityOf(*_v,timeAt);
					foreach (structures,v)
					{
						if (!ProcessVisibilityOfS(*v,timeAt))
						{
							float zOffset;
							PTurretBase base = (*v)->GetTurretBase(zOffset);
							//float velocity = Turret::MaxRangeToVelocity(base->GetFlatAttackRange());
							//float range = base->GetFlatAttackRange();
							//float r2 = sqr(base->GetAttackRange() + base->turret->explosionRadius);
							for (WMobileEntity w : owner->allMobiles)
							{
								PMobileEntity v2 = w.lock();
								for (index_t i = 0; i < v2->myTasks.Count(); i++)
								{
									const Task&task = v2->myTasks[i];
									bool badTask = false;
									if (task.type == Task::MoveTo)
									{
										for (const auto&node : task.path)
										{
											//TVec3<> world = Grid::ToWorld(node.cell);
											
											Grid::LCellIdI actual;
											if (base->ExtCanHit(OwnerParameter(),(*v)->location,zOffset,node.cell,actual))
											{
												badTask = true;
												break;
											}

										}
									}
									if (badTask)
									{
										LogEvent(EventType::EntityProcess,v2.get(),"Dangerous movement into range of "+(*v)->GetNiceName()+" detected. Dropping task(s).");
										if (v2.get() == this)
										{
											ClearAllTasks(true,CLOCATION);
											result = MobilePlacementResult::PlacementFailedHarmfulOrBlocked;
										}
										else
										{
											while (v2->myTasks.Count() > i)
												v2->myTasks.EraseBack();
										}
										break;
									}

								}
							}


					
						}
					}


		
					//#ifdef _DEBUG
						//VerifyVisibilityNetwork();
					//#endif
					owner->RegisterSensorMotion(old, location,GetScanningRange(), timeAt);


					if (timeCost > 0)
					{
						SignalMotion(To<Vehicle>::Cast(shared_from_this()),old,location,oldTime,timeCost);
					}
					else
					{
						#ifndef CLIENT
							LogEvent(EventType::MinorEntityProcess,this,"Time cost is "+String(timeCost)+". Warping vehicle at time "+String(oldTime));
						#endif
						RegisterChange(PositionChanged,oldTime);
					}
					//timeAt += timeCost;

					Grid::LCellIdI loc;
					foreach (towers, t)
						if ((*t)->WouldAttack(shared_from_this(),loc,timeAt))
						{
							if ((*t)->AttackNext(loc,true))
							{
								LogEvent(EventType::EntityProcess,t->get(),"Auto-targeting moving "+GetNiceName()+ " at location "+loc.ToString());

								(*t)->Attack(timeAt);
								if (health <= 0)
									return MobilePlacementResult::PlacementFailedHarmfulOrBlocked;
							}
						}
					#ifndef CLIENT
						LogEvent(EventType::MinorEntityProcess,this,"Motion completed");
					#endif
				}
				else
				{
					timeAt += timeCost;
					#ifndef CLIENT
						LogEvent(EventType::MinorEntityProcess,this,"Silent motion completed");
					#endif
				}

			}
			#ifdef _DEBUG
				ASSERT__(Failed(result) || hadTasks == HasTasks());
			#endif
			return result;
		}


	#endif



	/*virtual override*/bool				Vehicle::HasAmmoForShot() const 
	{
	#ifdef CONNECTOR_LESS
		const Compound&compound = owner->compound;
		return compound.total.storage.level[turret->ammoType] >= turret->ammoNeedPerShot;
	#else
		return currentStorage.level[turret->ammoType] >= turret->ammoNeedPerShot;
	#endif
	}
	
	#ifdef HAS_WINDOW

		void Vehicle::DescribeType(BaseCommand::Label&outResult, UINT32 descFlags)
		{
			Super::DescribeType(outResult,descFlags);
			outResult << "";
			base->DescribeType(outResult,true);
			outResult << "";

			if (turret->weight > base->carryingCapacity)
				outResult << "Overweight: "+String(turret->weight - base->carryingCapacity);
			String speed = "Effective speed: "+String(this->GetMovementStepsPerRound());
			if (GetMovementStepsPerRound() != base->baseMovementSpeed)
				speed += " /"+String(this->base->baseMovementSpeed);

			outResult << speed;
			if (base->requirements == Component::VehicleBase::Requirement::Water)
				outResult.lines.last()+= " (naval)";
			else
				if (base->moveOnWater)
					outResult.lines.last() += " (moves over water)";
			if (turret->constructionSpeed)
				outResult << "Effective construction speed: "+String(ResourceTag(Resource::Minerals))+String(this->GetConstructionSpeed())+" per round";

			#ifndef CONNECTOR_LESS
				if (turret->baseStorageCapacity.IsNotEmpty())
				{
					outResult << "Effective storage capacity:";
					ResourceVector vec = this->GetMaxStorageCapacity();
					for (index_t i = 0; i < Resource::NumberOfPossibilities; i++)
					{
						if (vec.level[i])
						{
							outResult << " "+String(ResourceTag(i))+String(vec.level[i]);
						}
					}
				}
			#endif

		}


	#endif




	#ifndef CLIENT



		void	Vehicle::EnqueueAttack(const Grid::LCellIdI&destination)
		{
			Grid::LCellIdI pointOfOrigin = GetLastPointOfOrigin();
			ASSERT__(!TasksAreLocked());
			if (!HasTasks() || !myTasks.GetNewest().IsMode(CursorMode::AttackMode))
			{
				myTasks << Task("Attack Position",destination,Task::Mode,CursorMode::AttackMode);
				myTasks.GetNewest().Recompile(pointOfOrigin,this);
				RegisterChange(TasksChanged,-1);
			}
			else
			{
				//if (!tasks.GetNewest().foci.Contains(destination))	//irrelevant. attack same location as often as you want
				{
					myTasks.GetNewest().foci << destination;
					myTasks.GetNewest().Recompile(pointOfOrigin,this);
					RegisterChange(TasksChanged,-1);
				}
			}
		}


		bool	Vehicle::DirectTo(mode_t mode, Grid::LCellIdI destination, bool leftClick/* = false*/)
		{
			switch (mode)
			{
				case CursorMode::Idle:
					return PlotCourse(destination);
				break;
				case CursorMode::DigTunnelMode:
					if (!turret->constructionSpeed)
						return false;
					if (leftClick)
					{
						destination--;
						if (destination != location)
						{
							Grid::LCellIdI pointOfOrigin = GetLastPointOfOrigin();
							Buffer0<PathNode> path;
							if (!AppendShortestDirectPath(pointOfOrigin,destination,path))
								LogEvent(EventType::CommandRejected,this,"Unable to plot path to destination");
					
							Grid::LCellIdI c = pointOfOrigin;
							foreach (path,node)
							{
								if (GetStructure(node->cell))
									break;

								if (node->cell.height == c.height)
									RegisterNewJob("", PVehicleJob(new VehicleJob::DigHorizontal(*this)),&node->cell);
								else
								{
									if (node->cell.height < c.height)
									{
										RegisterNewJob("", PVehicleJob(new VehicleJob::DigDown(*this)),&node->cell);
									}
									else
									{
										RegisterNewJob("", PVehicleJob(new VehicleJob::ClearAbove(*this)),&node->cell);
										++node;
										if (node != path.end())
											RegisterNewJob("", PVehicleJob(new VehicleJob::ClearAbove(*this)),&node->cell);
										else
											break;
										ASSERT__(!TasksAreLocked());

										myTasks << Task("Move To",node->cell,Task::MoveTo);
										RegisterChange(TasksChanged,-1);
										myTasks.GetNewest().Recompile(pointOfOrigin, this);
									}

								}

								c = node->cell;
							}
						}
					}
					else
						PlotCourse(destination);

				break;

				case CursorMode::BuildConnectorChainMode:
				case CursorMode::BuildRoadChainMode:
				case CursorMode::BuildWallChainMode:
				case CursorMode::BuildBridgeMode:
					if (!turret->constructionSpeed)
						return false;

					if (leftClick)
					{
						/*if (!Grid::LowCellIsClear(destination))
							destination ++;*/
						RegisterNewJob("", PVehicleJob(new VehicleJob::Sequence(*this,mode)),&destination);
					}
					else
						PlotCourse(destination);
				break;
				case CursorMode::AttackMode:
					if (!turret->CanAttack())
						return false;

					if (leftClick)
					{
						if (destination != location)
						{
							Grid::LCellIdI pointOfOrigin = GetLastPointOfOrigin();

							if (!CanHitFrom(pointOfOrigin,destination))
							{
								LogEvent(EventType::CommandRejected,this,"Destination out of range");
								return false;
							}

							EnqueueAttack(destination);
						}
					}
					else
						PlotCourse(destination);
				break;
			}
			return true;

		}

		//void	Vehicle::DirectTo(Grid::LCellIdI destination, bool leftClick /*= false*/)
		//{
		//	DirectTo(activeCursorMode, destination, leftClick);
		//	if (activeCursorMode != Idle)
		//	{
		//		activeCursorMode = Idle;
		//		UpdateChoices();
		//	}
		//}




	#endif




	
	#ifdef HAS_WINDOW



		/*virtual overrider*/ bool			Vehicle::TestIntersection(const TVec3<>&_b, const TVec3<>&_d, float & distance) const
		{
			TVec3<> b,d;//,test;
			Mat::transform(inverse,_b,b);
			Mat::rotate(inverse,_d,d);
			
			return base->TestIntersection(0,0,0,b,d,distance) | turret->TestIntersection(this->base->height, turretAngleX,turretAngleY,b,d,distance);	//binary or - must evaluate both
		}

	#endif

}


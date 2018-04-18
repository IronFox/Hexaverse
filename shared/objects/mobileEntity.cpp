#include <global_root.h>
#include "mobileEntity.h"
#include "faction.h"
#include "gridAndChoices.h"
#include "vehicle.h"
#include "satellite.h"
#include "systems.h"
#include "prototypes.h"
#include "../runtimeConfig.h"
#include "structure/tower.h"
#include "pathfinding.h"


namespace Objects
{
	SharedContainer<MobileEntity>			storedMobileEntities;

	#ifndef CLIENT
		class BuildChecker
		{
		public:
			/**/	BuildChecker(MobileEntity*e):entity(e),wasBuilding(e->IsBuilding())
			{}

			/**/	~BuildChecker()
			{
				const bool isBuilding = entity->IsBuilding();
				if (!wasBuilding && isBuilding)
					entity->RegisterChange(WorldEntity::StartedBuilding,entity->entityTime);
				elif (wasBuilding && !isBuilding)
					entity->RegisterChange(WorldEntity::StoppedBuilding,entity->entityTime);
			}

		private:
			MobileEntity*const	entity;
			const bool			wasBuilding;
		};

		/*static*/ const MobileEntity* MobileEntity::executingJob = nullptr;
	#endif


	void						CheckMobileEntitySanity()
	{
		ForeachMobileWorldEntity([](const Objects::PMobileEntity&m)
		{
			ASSERT1__(m->isInserted,m->GetName());
			ASSERT1__(m->IsAlive(),m->GetName());
			ASSERT1__(GetEntity(m->id) == m, m->GetName());
			ASSERT1__(!storedMobileEntities.Contains(m), m->GetName());
			ASSERT1__(m->storedIn.expired(), m->GetName());
			#ifndef CLIENT
				ASSERT2__(!m->owner->HasMobileInStorage(m),m->GetName(),m->owner->GetName());
				ASSERT2__(m->owner->HasActiveMobile(m),m->GetName(),m->owner->GetName());
			#endif
		},Objects::IncludeGhosts::None);
		
		storedMobileEntities.Foreach([](const Objects::PMobileEntity&m)
		{
			ASSERT1__(!m->isInserted,m->GetName());
			ASSERT1__(!m->isInsertedIntoGrid,m->GetName());
			ASSERT1__(m->IsAlive(),m->GetName());
			ASSERT1__(GetEntity(m->id) == m, m->GetName());
			if (m->IsVehicle())
				ASSERT1__(!vehicles.Contains(To<Vehicle>::Cast(m)),m->GetName());
			PHangar hangar = m->storedIn.lock();
			//PVehicleStore storedIn = To<VehicleStore>::DynamicCast( v->storedIn.lock());
			ASSERT1__(hangar,m->GetName());
			ASSERT2__(hangar->HasStoredLocally(m),m->GetName(),hangar->ToEntity().GetName());

			#ifndef CLIENT
				ASSERT2__(m->owner->HasMobileInStorage(m),m->GetName(),m->owner->GetName());
				ASSERT2__(!m->owner->HasActiveMobile(m),m->GetName(),m->owner->GetName());
			#endif
		});

	}


	/*virtual overrride*/ float					MobileEntity::GetAttackExplosionRadius() const
	{
		if (!turret->CanAttack())
			return 0.f;
		return turret->explosionRadius.GetValue(GetResearchState());
	}


	/*virtual override*/ void			MobileEntity::VisitSensors(const Grid::LCellIdI&loc, const std::function<void(const World::Sensor&)>&visitor, bool appearance /*= false*/) const
	{
		if (turret->detectsResources)
		{
			const auto center = GetScanningCenter(loc);
			float range = GetScanningRange();

			visitor(World::Sensor(center, range, World::Sensor::UpperHalfSolidSphere));
			visitor(World::Sensor(center, range, World::Sensor::ShadedCylinder,20));
		}
		else
			Super::VisitSensors(loc, visitor, appearance);
	}


	/*virtual override*/ float		MobileEntity::GetScanningRange() const
	{
		return EMPModify(turret->scanningRange.GetValue(GetResearchState()),2);
	}

	/*virtual override*/ String		MobileEntity::DescribeScanningRange() const
	{
		return turret->scanningRange.Describe(GetResearchState());
	}


	/*static*/ void MobileEntity::UpdateSystem(M::TMatrix4<>&inOutSys, M::TMatrix4<>*outInverse, const Grid::LCellIdI&location)
	{
		ASSERT_LESS__(location.stackID, Grid::LowSetup::GridType::CellCount);
		M::TVec3<> oldLocation = inOutSys.w.xyz;
		const M::TVec3<>&up = Grid::low.cells[location.stackID].cornerCenter;
		//centerVertex.xyz;
		M::TVec3<> y, x,z;
		inOutSys.w.xyz = up * Grid::ToRadiusf(float(location.height) /*+ 0.025f*/);
		z = oldLocation- inOutSys.w.xyz;

		if (Vec::zero(z))
		{
			z = float3(1,1,1);
			if (Vec::similar(z,up))
				z = float3(-1,1,1);
		}
		Vec::cross(up, z, x);
		Vec::normalize0(x);
		Vec::cross(x, up, y);
		Vec::normalize0(y);
		inOutSys.y.xyz = y;
		inOutSys.z.xyz = up;
		Vec::cross(inOutSys.y.xyz, inOutSys.z.xyz, inOutSys.x.xyz);
		Mat::resetBottomRow(inOutSys);
		if (outInverse)
			Mat::invertSystem(inOutSys, *outInverse);

		float hScale = GetScaleAtHeight(location.height);
		if (hScale < 1)
		{
			inOutSys.x.xyz *= hScale;
			inOutSys.y.xyz *= hScale;
			inOutSys.z.xyz *= hScale;
			if (outInverse)
			{
				outInverse->x.xyz /= hScale;
				outInverse->y.xyz /= hScale;
				outInverse->z.xyz /= hScale;
				outInverse->w.xyz /= hScale;
			}
		}
	}
	
	void MobileEntity::UpdateSystem()
	{
		UpdateSystem(system,&inverse, location);

		#ifdef HAS_WINDOW
			ambientSound.SetPosition(system.w.xyz);
			ambientSound.SetVelocity(float3());
			activitySound.SetPosition(system.w.xyz);
			activitySound.SetVelocity(float3());
		#endif

	}

	float MobileEntity::ContinuationsToRounds(int continuationsToJobCompletion, Component::operation_t op) const
	{
		int sub = (1<<GetConstructionAcceleration(op));
		return float(continuationsToJobCompletion) / sub;
	}



	UINT32					MobileEntity::GetOperationContinuations(int jobCosts, Component::operation_t op) const
	{
		if (!jobCosts)
			return 0;
		jobCosts = abs(jobCosts);
		int speed = GetOperationSpeed(op);
		if (!speed)
			return 0xFFFFFFFF;
		int rs = jobCosts / speed;
		if (jobCosts % speed)
			rs ++;
		return rs;
	}

	UINT32					MobileEntity::GetOperationCompletionRounds(int jobCosts, Component::operation_t op) const
	{
		return M::CeilDiv<UINT32>(GetOperationContinuations(jobCosts,op),1 << GetConstructionAcceleration(op));
	}



	std::pair<int,int>					MobileEntity::GetBuildingProgress() const
	{
		if (!HasTasks())
			return std::pair<int,int>(0,1);
		UINT32 total = 0;
		for (index_t i = 0; i < myTasks.Count(); i++)
		{
			const auto&t = myTasks[i];
			if (!t.job || t.job->continuationsToJobCompletion < 0)
				break;
			total += t.job->GetConstructionActionCostPerContinuation(*this) * t.job->continuationsToJobCompletion;
			if (t.job->movesToFocusWhenDone)
				break;
		}
		
		if (constructionActionCounter + total == 0)
			return std::pair<int,int>(0,1);

		int begin = (int)constructionActionCounter;
		int end = (int)(constructionActionCounter + total);
		return std::pair<int,int>(M::CeilDiv<int>(begin,MaxConstructionActionLeft),M::CeilDiv<int>(end,MaxConstructionActionLeft));
	}


	bool MobileEntity::HasUnsafeTask() const
	{
		return constructionActionCounter  > 0 
				&& myTasks.IsNotEmpty() 
				&& myTasks.Peek().IsJob() 
				&& myTasks.Peek().job->resourceCostLeft > 0;
	}

	#ifndef CLIENT

		/*virtual override*/ void		MobileEntity::OnDestroy(RemovalType t, float&atTime)
		{
			if (t == RemovalType::UncontrolledExplosion && RuntimeConfig::bombsExplodeWhenDestroyed && turret == &Component::bomb)
			{
				ApplicationInterface::SignalChainReactionExplosion(shared_from_this(), location, Component::Bomb::GetRadius(false,owner),Component::Bomb::GetDamageLevel(false,owner),false,atTime);
			}

		}

		void			MobileEntity::UnlockTasks()
		{
			ASSERT_GREATER_OR_EQUAL__(privateLockCounter,0);
			privateLockCounter --;
			if (!privateLockCounter)
			{
				if (flushTasksOnUnlock)
				{
					ClearAllTasks(true,CLOCATION);
					flushTasksOnUnlock = false;
				}
			}
		}



		MobileEntity::TaskResult			MobileEntity::HandleNormalTask()
		{
			if (!HasTasks())
				return TaskResult::AllDoneForThisRound;	//erase from motion queue
			Task&task = LockedPeekTask();
			jobStarted = false;

			const World::Data&world = owner->knownWorld;
			bool isObstructed = false;
			//if ( focus != location)
			{
				switch (task.type)
				{
					case Task::MoveTo:
					{
						bool dropRemaining = false;
						const int stepsPerRound = GetMovementStepsPerRound() * movementActionLeft;
						int remaining = stepsPerRound;
						//while (task.foci.IsNotEmpty())
						{
							//Grid::LCellIdI focus = task.foci.peek();
							if (!task.UpdatePath(this,location,FocusChangeType::SituationChanged))
							{
								bool changedAny = false;
								foreach (task.foci,f)
									if (!world.LowCellIsClear(f->target))
									{
										f->target ++;
										//location ++;
										bool abort = true;
										if (world.LowCellIsClear(f->target) )
											changedAny = true;
									}
								if (changedAny)
								{
									if (!task.UpdatePath(this,location,FocusChangeType::SituationChanged))
									{
										LogEvent(isAutomated ? EventClass::Preset::common : EventClass::Preset::operationAborted,this,"Route to destination lost. Motion aborted.");
										this->UnlockTasks();
										this->ClearAllTasks(true, CLOCATION);
										return TaskResult::AllDoneForThisRound;
									}
									LogEvent(EventSignificance::Common,this,"Revised path due to terrain changes.");
								}
								else
								{
									LogEvent(isAutomated ? EventClass::Preset::common : EventClass::Preset::operationAborted,this,"Route to destination lost. Motion aborted.");
									this->UnlockTasks();
									ClearAllTasks(true, CLOCATION);
									return TaskResult::AllDoneForThisRound;
									//hasMoved = true;
									//LogEvent(EventClass::Preset::commandRejected,this,"Route to destination blocked. Motion delayed.");
									//return false;
								}
							}
							//while (!Grid::LowCellIsClear(focus))
								//focus++;
							//if (!CanStandOn(focus -1) || !FindShortestPath(this,focus,task.path))
							//{
							//	//movementDestination = location;
							//	LogEvent("Route to destination lost. Motion delayed.");
							//}
							//else
							//{
							#ifdef CLIENT
								this->turretAngleX = 0;
							#endif
					
							task.path.Revert();
							task.path.EraseLast();	//current location
							Grid::LCellIdI newLocation = location;
							int cost = task.path.IsNotEmpty() ? task.path.Last().movementCostsFromPrevNode : 0;
							ASSERT__(HasTasks());	//just to be sure...
							while (task.path.IsNotEmpty() && (remaining >= cost || (!hasMoved && remaining > 0)))
							{
								ASSERT__(HasTasks());	//STILL HAPPENS!!!!!!!!
								Grid::LCellIdI previousLocation = location;
								float change = float(cost) / float(GetMovementStepsPerRound());
								bool stop = false;
								MobilePlacementResult rs = this->PlaceExactlyAt(task.path.Last().cell,Times::Move * change,0);
								switch (rs)
								{
									case MobilePlacementResult::PlacementOkay:
									{
										ASSERT__(HasTasks());
										if (task.path.Last().cell == task.foci.Peek().target)
											task.foci.EraseFront();

										remaining-=task.path.Last().movementCostsFromPrevNode;
										if (!AttackAndMotionAreIndependent())
											attackActionLeft -= change;
										movementActionLeft -= change;
										attackActionLeft = std::max(attackActionLeft,0.f);	//can happen
										movementActionLeft = std::max(movementActionLeft,0.f);
										hasMoved = true;
										if (AttackIsStationary())
											attackActionLeft = 0;

										RegisterChange(ActionChanged,entityTime);
										task.path.EraseLast();
										cost = task.path.IsNotEmpty() ? task.path.Last().movementCostsFromPrevNode : 1;
									}
									break;
									case MobilePlacementResult::EnteredHangar:
									{
										dropRemaining = true;
										stop = true;
										//should have relocated, so completeOnMove should finish. If not, AutoMoveComplete() will be triggered later and fail
									}
									break;
									case MobilePlacementResult::PlacementOkayFellDown:
									{
										LogEvent(EventClass::Preset::operationAborted,this,"Falling event interrupted motion.");
										stop = true;
									}
									break;
									case MobilePlacementResult::PlacementFailedHarmfulOrBlocked:
									case MobilePlacementResult::PlacementFailedPermanentlyObstructed:
										if (task.path.GetLength() <= 1 || (task.foci.IsNotEmpty() && task.foci.Peek().isDetailed))
											dropRemaining = true;
										LogEvent( this->isAutomated ? EventClass::Preset::common :  EventClass::Preset::operationAborted,this,"Permanent or harmful obstruction detected: Unable to complete movement.");
										//bleed into next case:
									case MobilePlacementResult::PlacementFailedTemporarilyObstructed:
									{
										isObstructed = true;
										if (rs == MobilePlacementResult::PlacementFailedTemporarilyObstructed)
											LogEvent( this->isAutomated ? EventClass::Preset::common :  EventClass::Preset::operationAborted,this,"Temporary obstruction detected: Unable to complete movement.");
										if (IsDeadOrGhost())
										{
											LogEvent(EventSignificance::Common,this,"Is dead.");
											if (completeOnMove)
											{
												CompleteOnMove(previousLocation);
											}
											this->UnlockTasks();
											return TaskResult::AllDoneForThisRound;
										}

										stop = true;
									}
									break;
									default:
										FATAL__("Unexpected placement result "+String(rs));
								}

								if (completeOnMove && location != previousLocation)
								{
									if (!CompleteOnMove(previousLocation))
									{
										UnlockTasks();
										return TaskResult::AllDoneForThisRound;
									}
								}

								if (stop)
									break;
							}
							if (task.path.IsEmpty() || dropRemaining)
							{
								this->UnlockTasks();
								BuildChecker checker(this);
								if (dropRemaining)
								{
									this->ClearAllTasks(true,CLOCATION);
									remaining = true;
								}
								else
								{
									this->EraseFirstTask();
									//tasks.EraseFront();
									if (HasTasks() && To<VehicleJob::PathJob>::DynamicCast(myTasks.Peek().job))
									{
										myTasks.Peek().Replot(location,this,FocusChangeType::SituationChanged,true);
									}
								}
								#ifndef CLIENT
									RegisterChange(TasksChanged,entityTime);
								#endif
								return remaining == 0 || isObstructed ? TaskResult::AllDoneForThisRound : TaskResult::FinishedCanDoMore;
							}
							else
							{
								if (HasTasks())
								{
									if (remaining < task.path.Last().movementCostsFromPrevNode)
									{
										remaining = 0;
										if (!AttackAndMotionAreIndependent())
											attackActionLeft = 0;
										movementActionLeft = 0;
										RegisterChange(ActionChanged,entityTime);
									}

									hasMoved = true;
									task.path << PathNode(location,0);
									task.path.Revert();
								
									task.Replot(location,this,FocusChangeType::SituationChanged,true);
									#ifdef CLIENT
										if (selectedEntity.get() == this)
											UpdateChoices();
									#endif
								}
								this->UnlockTasks();
								return remaining == 0 || isObstructed ? TaskResult::AllDoneForThisRound : TaskResult::FinishedCanDoMore;
							}
						}
					}
					break;
					case Task::Mode:
					{
						if (task.mode == CursorMode::AttackMode)
						{
							FATAL__("should not be here");
						}
					}
					break;
				}
			}	
			this->UnlockTasks();
			return TaskResult::AllDoneForThisRound;
		}

		/*virtual override*/ void			MobileEntity::SignalTerrainChange(float atTime)
		{
			if (executingJob == this)
			{
				/*
				If we are here then this method was called as a result of terrain change which in turn is the result of some job executed by this entity.
				We know job execution has not finished yet, which means the terrain may be temporarily inconsistent for this entity.
				We assume that placement will be handled by the job itself and not do anything here as it may invalidate the operation
				*/
				return;
			}
			Super::SignalTerrainChange(atTime);
			if (IsDeadOrGhost())
				return;
			CheckGround(atTime);	//check if mobile entity should fall
		}


		void			MobileEntity::CheckGround(float atTime)
		{
			HandleNow(PlaceExactlyAt(location,0, atTime));
		}


		void			MobileEntity::HandleNow(MobilePlacementResult rs)
		{
			if (rs == MobilePlacementResult::PlacementFailedMobileDestroyed)
			{
				LogEvent(EventSignificance::Common,this,"Handling mobile entity destruction.");
				jobStarted = false;
				ClearAllTasks(true, CLOCATION);
				unstable = false;
				if (isInserted)	//otherwise already killed by Finish() operation
					Remove(shared_from_this(),RemovalType::UncontrolledExplosion,entityTime);
			}
		}


		bool			MobileEntity::CompleteOnMove(const Grid::LCellIdI&referenceLocation)
		{
			LogEvent(EventSignificance::Common,this,"Completing on-move job '"+completeOnMove->GetName()+ "' at t="+String(entityTime));
			VehicleJob::BaseJob::continue_t rs = completeOnMove->Finish(*this,referenceLocation);
			completeOnMove.reset();
			unstable = false;
			if (rs == VehicleJob::BaseJob::TerminalFailureVehicleDestroyed)
			{
				LogEvent(EventSignificance::Common,this,"Terminal failure. Remaining tasks dropped. Mobile entity destroyed.");
				jobStarted = false;
				ClearAllTasks(true, CLOCATION);
				unstable = false;
				if (isInserted)	//otherwise already killed by Finish() operation
					Remove(shared_from_this(),RemovalType::UncontrolledExplosion,entityTime);
				return false;
			}
			if (rs == VehicleJob::BaseJob::TerminalFailure)
			{
				LogEvent(EventSignificance::Common,this,"Terminal failure. Remaining tasks dropped.");
				jobStarted = false;
				ClearAllTasks(true, CLOCATION);
				unstable = false;
				return false;
			}
			return true;
		}
	


		/*virtual override*/ void		MobileEntity::FlushAutoAttacks()
		{
			while (myTasks.IsNotEmpty() && myTasks.Peek().IsMode(CursorMode::AttackMode) && myTasks.Peek().name.BeginsWith("Auto "))
				myTasks.EraseFront();
		}

		/*virtual override*/ bool		MobileEntity::AttackNext(const Grid::LCellIdI&target, bool autoTarget)
		{
			ASSERT__(!TasksAreLocked());
			if (!CanAttack())
				return false;
			Grid::LCellIdI pointOfOrigin = GetLastPointOfOrigin();
			if (target == pointOfOrigin && !IsBeam(turret->trajectoryType))
			{
				LogEvent(EventClass::Preset::commandRejected,this,"Destination too close");
				return false;
			}
			if (!IsBeam(turret->trajectoryType))
			{
				//Grid::LCellIdI pointOfOrigin = location+1;


				//float targetDistance = Grid::GetGeometricDistance(pointOfOrigin,target);

				if (!CanHitFrom(pointOfOrigin,TrajectoryEndPoint( target )))
				{
					LogEvent(EventClass::Preset::commandRejected,this,"Destination out of range");
					return false;
				}
			}

			const String prefix = autoTarget ? "Auto " : "";

			if (!HasTasks() || !myTasks.GetNewest().IsMode(CursorMode::AttackMode) || !myTasks.GetNewest().name.BeginsWith(prefix))
			{
				if (myTasks.Count() >= MaxTasks)
				{
					LogEvent(EventClass::Preset::commandRejected,this,"Max queue length ("+String(MaxTasks)+") reached");
					return false;
				}

				auto copy = target;
				myTasks << Task(prefix + "Attack Position",copy,false,Task::Mode,CursorMode::AttackMode);
				myTasks.GetNewest().Replot(pointOfOrigin,this,FocusChangeType::FullChange,true);
				RegisterChange(TasksChanged,-1);
			}
			else
			{
				if (myTasks.GetNewest().foci.Count() >= MaxFocusPoints)
				{
					LogEvent(EventClass::Preset::commandRejected,this,"Max focus point count ("+String(MaxFocusPoints)+") reached");
					return false;
				}

				//if (!tasks.GetNewest().foci.Contains(destination))	//irrelevant. attack same location as often as you want
				{
					myTasks.GetNewest().foci.Push().target = target;
					myTasks.GetNewest().Replot(pointOfOrigin,this,FocusChangeType::FocusAppended,true);
					RegisterChange(TasksChanged,-1);
				}
			}
			return true;
		}



		void MobileEntity::HandleMotion( MobilePlacementResult&result, bool signalEvent, const Grid::LCellIdI&oldLocation, float timeCost )
		{
//			MobilePlacementResult result;
			this->UpdateSystem();
			if (signalEvent)
			{

				//static GenericHashSet<OwnerParameter,OwnerHash>	checkStillBeeingSeenBy;
				//static Buffer0<PWorldEntity> checkCanStillSee;
				//checkCanStillSee.Clear();
				//checkStillBeeingSeenBy.Clear();

				float oldTime = entityTime;
				entityTime += timeCost;

				static Ctr::Buffer0<PWorldEntity> newContacts;
				newContacts.Clear();
				this->MoveSensoryToCurrentLocation(entityTime,&newContacts);

				//stop motion into attack range of new contacts:
				foreach (newContacts,c)
				{
					float zOffset;
					PTurretBase b = (*c)->GetTurretBase(zOffset);
					if (!b)
						continue;
					const bool movingAttack = b->AttacksMovingHostilesDuringMotionPhase();
					if (b->turret->CanAttack() && !b->turret->dealsEMPDamage)
					{
						LogEvent(EventClass::Preset::common,this,"Processing new hostile regular-offensive contact "+(*c)->GetName()+"...");
						owner->VisitMyActiveMobileEntities([movingAttack,zOffset,c,this,&result,b](const Objects::PMobileEntity&v2)
						{
							if (v2->turret->CanAttack() && v2->turret->independentAttackAndMotion && !movingAttack)
								return;	//otherwise this entity is a hunter, with low sensor range, plus probably able to tank the damage and/or counter-attack

							for (index_t i = 0; i < v2->myTasks.Count(); i++)
							{
								const Task&task = v2->myTasks[i];
								bool badTask = false;
								if (task.type == Task::MoveTo)
								{
									for (const auto&node : task.path)
									{
										//M::TVec3<> world = Grid::ToWorld(node.cell);
										//float zOffset;
										//const auto&base = m->GetTurretBase(zOffset);
										TurretBase::TargetChoice choice;
										if (b->ExtCanHit((*c)->owner, TrajectoryEndPoint( (*c)->location,zOffset), TrajectoryEndPoint(node.cell), choice))
										{
											badTask = true;
											break;
										}

									}
								}
								if (badTask)
								{
									LogEvent(EventSignificance::Common,v2.get(),"Dangerous movement into range of "+(*c)->GetName()+" at "+ToString((*c)->location)+" detected.");
									LogEvent(EventSignificance::Uncommon,v2.get(),"Dangerous movement into range of "+(*c)->GetNiceName()+" detected.");
									BuildChecker checker(this);
									if (v2.get() == this)
									{
										ClearAllTasks(true,CLOCATION);
										result = MobilePlacementResult::PlacementFailedHarmfulOrBlocked;
									}
									else
									{
										LogEvent(EventSignificance::Common,v2.get(),"Truncating tasks to "+String(i));
										bool anyChanged = false;
										while (v2->myTasks.Count() > i)	//must be greater. task 1 is bad => queue max length is 1 (task 0 is assumed good)
										{
											v2->myTasks.EraseBack();
											anyChanged = true;
										}
										if (anyChanged)
										{
											entityTime += timeEpsilon;
											v2->RegisterChange(TasksChanged,entityTime);
										}
									}
									break;
								}

							}
						});
					}
				};


		

				if (timeCost > 0)
				{
					ApplicationInterface::SignalMotion(To<Vehicle>::Cast(shared_from_this()),oldLocation,location,oldTime,timeCost);
				}
				else
				{
					#ifndef CLIENT
						LogEvent(EventSignificance::Common,this,"Time cost is "+String(timeCost)+". Warping mobile entity at time "+String(oldTime));
					#endif
					RegisterChange(PositionChanged,oldTime);
				}
				//motionTime += timeCost;

				const auto _this = shared_from_this();
				ForeachFaction([this,_this](const PFaction&f)
				{
					f->CheckGridVisibility(_this,entityTime);
				});

				bool doBreak = false;
				ForeachFaction([this,&doBreak,_this](const PFaction&f)
				{
					if (doBreak)
						return;
					if (f == owner)
						return;
					if (!f->CanSee(_this,entityTime))
						return;

					TurretBase::TargetChoice choice;
		
					if (!doBreak)
					{
						f->VisitMyAutoFiringTurrets([_this,&choice,this,&doBreak](const PTurretBase&t)
						{
							if (doBreak)
								return;
							if (t && t->WouldAutoAttack(_this,choice,entityTime))
							{
								if (t->AttackNext(choice.cellTarget,true))
								{
									LogEvent(EventSignificance::Debug,&t->ToEntity(),"Auto-targeting moving "+GetName()+" (located at "+ToString(location)+", =>"+ToString(Grid::ToWorld(location))+",sys="+ToString(system.w.xyz)+") via closest viable attack target "+ToString(choice.cellTarget)+" at t="+String(entityTime));
									LogEvent(EventSignificance::Important,&t->ToEntity(),"Auto-targeting moving "+GetNiceName());

									t->Attack(entityTime);
									LogEvent(EventSignificance::Debug,&t->ToEntity(),"t is now "+String(entityTime));
									if (IsDeadOrGhost())
									{
										doBreak = true;
										return;
									}
										
								}
							}
						});
					}
				});
				if (doBreak)
				{
					result = MobilePlacementResult::PlacementFailedHarmfulOrBlocked;
					#ifndef CLIENT
						LogEvent(EventSignificance::Common,this,"Motion failed. Dead");
					#endif
					return;
				}

				#ifndef CLIENT
					LogEvent(EventSignificance::Common,this,"Motion completed");
				#endif
			}
			else
			{
				entityTime += timeCost;
				#ifndef CLIENT
					LogEvent(EventSignificance::Common,this,"Silent motion completed");
				#endif
			}

		}


		MobileEntity::TaskResult MobileEntity::AutoMoveComplete()
		{
			if (!this->storedIn.expired())
			{
				LogEvent(EventClass::Preset::operationAborted,this,"Unable to finish '"+completeOnMove->GetName()+"'. Now stored in "+storedIn.lock()->ToEntity().GetName()+". Aborting operation");
				completeOnMove = nullptr;
				return TaskResult::AllDoneForThisRound;
			}
			if (numTriedPlottingOnMoveAutoMotion < 5)
			{
				Pathfinding::TWays	ways;

				GetMobileWays(owner->knownWorld, this,  location,ways, Grid::LCellIdI::Invalid, nullptr,false,false);
				//random first to maybe not make the same mistake twice
				for (index_t i = 0; i < ways.numWays; i++)
				{
					index_t target = entityRandom.NextIndex(ways.numWays);
					if (PlotFrontCourse(ways.way[target],true))
					{
						numTriedPlottingOnMoveAutoMotion ++;
						LogEvent(EventSignificance::Common,this,"Automatically moving to finish '"+completeOnMove->GetName()+"'");
						return TaskResult::TryingSomething;
					}
				}
				//iterative, in case random missed some option
				for (index_t i = 0; i < ways.numWays; i++)
					if (PlotFrontCourse(ways.way[i],true))
					{
						numTriedPlottingOnMoveAutoMotion ++;
						LogEvent(EventSignificance::Common,this,"Automatically moving to finish '"+completeOnMove->GetName()+"'");
						return TaskResult::TryingSomething;
					}
			}
			LogEvent(EventClass::Preset::operationAborted,this,"Unable to automatically relocate. Move manually to finish '"+completeOnMove->GetName()+"'");
			return TaskResult::AllDoneForThisRound;
		}


		MobileEntity::TaskResult			MobileEntity::PerformNonAttack()
		{
			BuildChecker checker(this);

			if (!HasTasks())
			{
				if (completeOnMove)
					return AutoMoveComplete();
				return TaskResult::AllDoneForThisRound;
			}
			
			if (myTasks.Peek().job)
			{

				if (this->empDamageLevel)
				{
					LogEvent(EventClass::Preset::operationAborted,this,"EMP damage: Unable to continue job at this time.");
					return TaskResult::AllDoneForThisRound;
				}
				RegisterChange(TasksChanged,entityTime);
				if (!jobStarted)
				{
					//jobStarted = true;
					//if (To<Job::PathJob>::DynamicCast(task.job))
					//	task.Recompile(location,this);
					//ASSERT__(!completeOnMove || tasks.IsEmpty() || !tasks.Peek().job);

					while (HasTasks() && myTasks.Peek().job)
					{
						auto job = myTasks.Peek().job;
						VehicleJob::BaseJob::begin_t b = job->BeginHere(*this,myTasks.Peek().GetFocus(location),!!completeOnMove);
						job->UpdateEstimations(*this);	//accomodate situational changes
						bool staticDeltaContainer = true;
						switch (b)
						{
							case VehicleJob::BaseJob::Delayed:
								LogEvent(EventSignificance::Common,this,"Delayed Job");
							break;
							case VehicleJob::BaseJob::Started:
								LogEvent(EventSignificance::Common,this,"Started '"+myTasks.Peek().job->GetName()+"'");
								staticDeltaContainer = false;
								jobStarted = true;
								//movementActionLeft = 0;
							break;
							case VehicleJob::BaseJob::CannotStartAbort:
							{
								LogEvent(EventSignificance::Common,this,"Cannot start '"+myTasks.Peek().job->GetName()+"'. Skipping");
								PVehicleJob parent = myTasks.Peek().job->parent.lock();
								EraseFirstTask();
								if (parent)
								{
									while (myTasks.IsNotEmpty() && myTasks.Peek().job != parent)
									{
										LogEvent(EventSignificance::Common,this,"Cannot continue offspring '"+myTasks.Peek().name+"'. Skipping");
										EraseFirstTask();
									}
									if (myTasks.IsNotEmpty())
									{
										LogEvent(EventSignificance::Common,this,"Cannot continue parent '"+myTasks.Peek().name+"'. Skipping");
										EraseFirstTask();
									}
									else
										LogEvent(EventClass::Preset::error,this,"Cannot find parent job '"+parent->GetName()+"'");
								}
								RegisterChange(TasksChanged,entityTime);

								if (completeOnMove && !HasTasks())
									return AutoMoveComplete();

							}
							break;
						}
						if (!staticDeltaContainer)
							break;
					};

					if (!HasTasks())
						return TaskResult::AllDoneForThisRound;
					if (myTasks.Peek().IsMode(CursorMode::AttackMode))
						return TaskResult::FinishedCanDoMore;

					if (!myTasks.Peek().job)
					{
						LogEvent(EventSignificance::Common,this,"Executing normal task");
						return HandleNormalTask();
					}
				}
				if (HasTasks())
				{
					PVehicleJob job = myTasks.Peek().job;
					if (job)
					{
						const UINT32 cost = job->GetConstructionActionCostPerContinuation(*this);
						if (cost <= constructionActionLeft)
						{
							ASSERT__(jobStarted);	//supposedly it can happen that this is false
							if (IsSleeping() && job->DelayedBySleep())
							{
								LogEvent(EventSignificance::Common,this,"Job delayed: '"+job->GetName()+"'. Entity is sleeping");
								return TaskResult::AllDoneForThisRound;
							}
							else
							{
								constructionActionLeft -= cost;
								constructionActionCounter += cost;
								/**
								Unable to determine why terminal is ever set true.
								Please note that the BuildRoad job should continue despite this condition evaluating to true
								*/
								//bool terminal = !job->movesToFocusWhenDone && job->parent.expired();

								executingJob = this;
								auto rs = job->Continue(*this,myTasks.Peek().GetFocus(location),true);
								executingJob = nullptr;
								switch (rs)
								{
									case VehicleJob::BaseJob::ContinueJob:
										LogEvent(EventSignificance::Common,this,"Job continued: '"+job->GetName()+"' now at "+String(constructionActionCounter)+", "+String(job->continuationsToJobCompletion)+" action left");
									break;
									case VehicleJob::BaseJob::JobDelayed:
										constructionActionCounter -= cost;
										//LogEvent(EventSignificance::Common,this,"Job delayed: '"+job->GetName()+"' now at "+String(constructionActionCounter)+", "+String(job->continuationsToJobCompletion)+" action left");
									break;
									case VehicleJob::BaseJob::JobDone:
										LogEvent(EventSignificance::Common,this,"Job ended: '"+job->GetName()+"'");

										EraseFirstTask();
										RegisterChange(TasksChanged,entityTime);
										while (HasTasks() && myTasks.Peek().job && myTasks.Peek().job->IsImplicitlyDone(*this,myTasks.Peek().GetFocus(location)))
										{
											LogEvent(EventSignificance::Common,this,"Job has implicitly ended: '"+myTasks.Peek().job->GetName()+"'");

											EraseFirstTask();
										}
										if (HasTasks() && myTasks.Peek().job)
										{
											LogEvent(EventSignificance::Common,this,"Next front job: '"+myTasks.Peek().job->GetName()+"'");
										}
										foreach (myTasks,t)
											if (t->job)
												t->job->UpdateEstimations(*this);
										jobStarted = false;

									return TaskResult::FinishedCanDoMore;
									case VehicleJob::BaseJob::TerminalFailureVehicleDestroyed:
										LogEvent(EventSignificance::Common,this,"Terminal failure. Remaining tasks dropped. Vehicle destroyed.");
										jobStarted = false;
										ClearAllTasks(true, CLOCATION);
										unstable = false;
										if (isInserted)	//otherwise already killed by Continue() operation
											Remove(shared_from_this(),RemovalType::UncontrolledExplosion,entityTime);
									return TaskResult::AllDoneForThisRound;
									case VehicleJob::BaseJob::TerminalFailure:
										LogEvent(EventSignificance::Common,this,"Terminal failure. Remaining tasks dropped.");
										jobStarted = false;
										ClearAllTasks(true, CLOCATION);
										unstable = false;
									return TaskResult::AllDoneForThisRound;
								}
								return constructionActionLeft == 0 /*|| terminal*/ ? TaskResult::AllDoneForThisRound : TaskResult::FinishedCanDoMore;
							}
						}
					}
				}
				return TaskResult::AllDoneForThisRound;
			}
			else
			{
				if (HasTasks() && myTasks.Peek().IsMode(CursorMode::AttackMode))
					return TaskResult::AllDoneForThisRound;

				RegisterChange(TasksChanged,entityTime);
				return HandleNormalTask();
			}
			
		}

		void					MobileEntity::RegisterNewJob(const String&localizableShapeIdentifier, const PVehicleJob&job, const Grid::LCellIdI*destination)
		{
			/*{
				if (job->resourceCostLeft > this->currentStorage.level[Resource::Minerals])
				{
					LogEvent(GetName()+": Unable to comply. Insufficient minerals left in storage.");
					return;
				}
			}*/

			ASSERT__(!TasksAreLocked());
			BuildChecker checker(this);
			Grid::LCellIdI origin = this->GetLastPointOfOrigin();
		
			RegisterChange(TasksChanged,entityTime);

			if (myTasks.IsEmpty())
			{
				constructionActionCounter = 0;
			}

			if (IsSleeping() && job->DelayedBySleep())
			{
				float t = 0.f;
				SetSleep(false,t);
			}

			Task&task = myTasks.Push();
			task.type = Task::Job;
			task.name = job->GetName();
			if (destination)
				task.foci.Push().target = *destination;
			task.job = job; 
			if (!task.Replot(origin,this,FocusChangeType::FullChange,true))
			{
				LogEvent(EventClass::Preset::operationAborted,this, "Destination unreachable or out of range for pathfinding. Unable to enqueue job.");
				myTasks.EraseBack();
				return;
			}
			LogEvent(EventSignificance::Common,this,"Enqueued new job '"+job->GetName()+"'");
		}
	
		void					MobileEntity::RegisterNewFrontJob(const String&localizableShapeIdentifier, const PVehicleJob&job, const PVehicleJob&parent, const Grid::LCellIdI*destination)
		{
			ASSERT__(!TasksAreLocked());

			BuildChecker checker(this);

			if (IsSleeping() && job->DelayedBySleep())
			{
				float t = 0.f;
				SetSleep(false,t);
			}


			RegisterChange(TasksChanged,entityTime);
			Task&task = myTasks.PushFront();
			task.type = Task::Job;
			task.name = job->GetName();
			if (destination)
				task.foci.Push().target = *destination;
			ASSERT__(parent);
			job->parent = parent;

			task.job = job; 
			task.Replot(location,this,FocusChangeType::FullChange,true);

			LogEvent(EventSignificance::Common,this,"Enqueued new front-job '"+job->GetName()+"'");
		}


		//void MobileEntity::ProcessVisibilityOf(const PWorldEntity&e, float&t)
		//{
		//	if (e.get() == this || e->owner == owner)
		//		return;
		//	if (CanSee(e) && !seeing.Contains(e))
		//	{
		//		seeing << e;
		//		(e)->seenBy << shared_from_this();
		//		//SignalAppearanceToHostileFaction(owner,e,t);
		//		owner->SignalAppearance(e,t);
		//	}
		//	if (e->CanSee(shared_from_this()) && !seenBy.Contains(e))
		//	{
		//		seenBy << e;
		//		(e)->seeing << shared_from_this();
		//		//SignalAppearanceToHostileFaction(e->owner,shared_from_this(),t);
		//		e->owner->SignalAppearance(shared_from_this(),t);
		//	}


		//}


		//bool MobileEntity::ProcessVisibilityOfS(const PWorldEntity&e, float&t)
		//{
		//	if (e.get() == this || e->owner == owner)
		//		return true;
		//	bool rs = true;
		//	if (CanSee(e) && !seeing.Contains(e))
		//	{
		//		seeing << e;
		//		(e)->seenBy << shared_from_this();
		//		//SignalAppearanceToHostileFaction(owner,e,t);
		//		
		//		if (owner->SignalAppearance(e,t)
		//			&& e->GetTurretBase() 
		//			&& !owner->FindReferenceMarker(Marker::Type::HostileContact,e->id))
		//		{
		//			rs = false;
		//		}
		//	}
		//	if (e->CanSee(shared_from_this()) && !seenBy.Contains(e))
		//	{
		//		seenBy << e;
		//		(e)->seeing << shared_from_this();
		//		//SignalAppearanceToHostileFaction(e->owner,shared_from_this(),t);
		//		e->owner->SignalAppearance(shared_from_this(),t);
		//	}

		//	return rs;
		//}


		void			MobileEntity::EraseFirstTask()
		{
			ASSERT__(!TasksAreLocked());
			BuildChecker checker(this);
			myTasks.EraseFront();
		}

		bool			MobileEntity::PlotCourse( MutableArrayRef<Grid::LCellIdI> destination, bool detailedPath, bool virtualPlatformMode/*=false*/ )
		{
			const World::Data&world = owner->knownWorld;
			if (IsVehicle())
			{
				for (index_t i = 0; i < destination.GetLength(); i++)
				{
					Grid::LCellIdI&dest = destination[i];

					while (!world.LowCellIsClear(dest) && !AccessibleHangarHere(dest,*this))
						dest++;
					while (world.LowCellIsClear(dest-1) || AccessibleHangarHere(dest-1,*this))
						dest--;
					if (!virtualPlatformMode && !((Vehicle*)this)->CanStandOn(world,dest -1,false,false))
						return false;
				}
			}

			Grid::LCellIdI pointOfOrigin = GetLastPointOfOrigin();

			while (destination.IsNotEmpty() && destination.First() == pointOfOrigin)
			{
				destination = destination.SubRef(1);
			}
			if (destination.IsEmpty())
			{
				return false;
			}

			if (!HasTasks() || myTasks.GetNewest().type != Task::MoveTo)
			{
				if (myTasks.Count() >= Objects::MaxTasks)
				{
					LogEvent(EventClass::Preset::commandRejected,this,"Max queue length ("+String(Objects::MaxTasks)+") reached. Task rejected");
					return false;
				}

				ASSERT__(!TasksAreLocked());
				myTasks << Task("Move To",destination,detailedPath, Task::MoveTo);
				if (!myTasks.GetNewest().Replot(pointOfOrigin, this,FocusChangeType::FullChange,true))
				{
					LogEvent(EventClass::Preset::commandRejected,this,"Unable to plot path to destination. Task rejected.");
					myTasks.EraseBack();
					return false;
				}
				RegisterChange(TasksChanged,-1);
			}
			else
			{
				if (myTasks.GetNewest().foci.IsNotEmpty())
				{
					if (myTasks.GetNewest().foci.Count() >= Objects::MaxFocusPoints)
					{
						LogEvent(EventClass::Preset::commandRejected,this,"Max focus point count ("+String(Objects::MaxFocusPoints)+") reached. Task rejected");
						return false;
					}
					const auto&match = myTasks.GetNewest().foci.GetNewest();
					while (destination.IsNotEmpty() && destination.First() == match.target)
					{
						destination = destination.SubRef(1);
					}
				}
				if (destination.IsEmpty())
				{
					return false;
				}

				{
					count_t offset = myTasks.GetNewest().foci.Count();
					bool failed = false;
					for (index_t i = 0; i < destination.GetLength(); i++)
					{
						if (myTasks.GetNewest().foci.Count() >= MaxFocusPoints)
						{
							failed = true;
							break;
						}
						Task::Focus&f = myTasks.GetNewest().foci.Push();
						f.target = destination[i];
						f.isDetailed = detailedPath;
						f.isIntermediate = i+1 < destination.GetLength();

						if (!myTasks.GetNewest().Replot(GetPrevPointOfOrigin(), this,FocusChangeType::FocusAppended,true))
						{
							failed = true;
							break;
						}
					}
					if (failed)
					{
						LogEvent(EventClass::Preset::commandRejected,this,"Unable to plot path to destination. Task rejected.");
						while (myTasks.GetNewest().foci.Count() > offset)
							myTasks.GetNewest().foci.EraseBack();
						return false;
					}
					RegisterChange(TasksChanged,-1);
				}
			}

			return true;
		}

		bool	MobileEntity::PlotFrontCourse(MutableArrayRef<Grid::LCellIdI> destination, bool detailedPath)
		{
			const World::Data&world = owner->knownWorld;

			if (IsVehicle())
			{
				for (auto&dest : destination)
				{
					while (!world.LowCellIsClear(dest))
						dest++;
					if (!((Vehicle*)this)->CanStandOn(world,dest -1,false,false))
						return false;
				}
			}

			while (destination.IsNotEmpty() && destination.First() == location)
			{
				destination = destination.SubRef(1);
			}
			if (destination.IsEmpty())
			{
				return false;
			}

			{
				BuildChecker checker(this);
				ASSERT__(!TasksAreLocked());

				myTasks.PushFront(Task("Move To",destination,detailedPath,Task::MoveTo));
				myTasks.GetOldest().Replot(location, this, FocusChangeType::FullChange,true);
				RegisterChange(TasksChanged,entityTime);
			}

			return true;
		}
		

	
	
		bool		MobileEntity::DropLastTask()
		{
			if (!HasTasks())
				return false;
			ASSERT__(!TasksAreLocked());
			BuildChecker checker(this);
			if (myTasks.GetNewest().type == Task::MoveTo)
			{
				myTasks.GetNewest().foci.EraseBack();
				myTasks.GetNewest().Replot(GetPrevPointOfOrigin(),this,FocusChangeType::LastFocusDropped,false);
				while (myTasks.GetNewest().foci.IsNotEmpty() && myTasks.GetNewest().foci.GetNewest().isIntermediate)
				{
					myTasks.GetNewest().foci.EraseBack();
					myTasks.GetNewest().Replot(GetPrevPointOfOrigin(),this,FocusChangeType::LastFocusDropped,false);
				}
				if (myTasks.GetNewest().foci.IsEmpty())
					myTasks.EraseBack();
				else
					myTasks.GetNewest().Replot(GetPrevPointOfOrigin(),this,FocusChangeType::NoChange,true);
			}
			else
			{
				if (myTasks.GetNewest().IsMode(CursorMode::AttackMode) && myTasks.GetNewest().foci.Count() > 1)
				{
					myTasks.GetNewest().foci.EraseBack();
					myTasks.GetNewest().Replot(GetPrevPointOfOrigin(),this,FocusChangeType::LastFocusDropped,true);
				}
				else
					myTasks.EraseBack();
			}
			RegisterChange(TasksChanged,entityTime);
			if (!HasTasks())
				jobStarted = false;
			return true;
		}


		bool		MobileEntity::ClearAllTasks(bool isReaction, const TCodeLocation&caller)
		{
			LogEvent(EventSignificance::Debug,this,"ClearAllTasks("+String(isReaction)+", "+String(caller)+")");
			if (TasksAreLocked())
			{
				if (flushTasksOnUnlock)
					return false;
				entityTime += timeEpsilon;
				flushTasksOnUnlock = true;
				LogEvent(EventSignificance::Debug,this," ...set to clear on unlock");
				return true;
			}
				
			if (myTasks.IsNotEmpty() || completeOnMove)
			{
				BuildChecker checker(this);
				entityTime += timeEpsilon;
				jobStarted = false;
				if (!isReaction)
					completeOnMove.reset();
				RegisterChange(TasksChanged,entityTime);
				//tasksChanged = true;
				myTasks.Clear();
				LogEvent(EventSignificance::Debug,this," ...cleared");
				return true;
			}
			return false;
		}



		/*virtual override*/ void			MobileEntity::ClearSafeTasks()
		{
			BuildChecker checker(this);
			if (HasUnsafeTask())
			{
				if (myTasks.Count() == 1)
					return;//avoid signalling change
				while (myTasks.Count() > 1)
					myTasks.EraseBack();
			}
			else
				myTasks.Clear();
			RegisterChange(TasksChanged,entityTime);
		}

		void	MobileEntity::OverrideExecuteAttackNow(const Grid::LCellIdI target, float explosionSize, int damageLevel, bool dealsEMPDamage, bool isDoomsday, float&attackTime)
		{
			if (isDoomsday && RuntimeConfig::doomsdayWeapons == RuntimeConfig::DoomsdayWeapons::Disallow)
			{
				LogEvent(EventClass::Preset::commandRejected,this,"Cannot execute attack: Doomsday weapons are not allowed by server rule");
				return;
			}

			LogEvent(EventSignificance::Debug,this,"Setting local time ("+String(entityTime)+") to attack time="+String(attackTime));
			SyncTime(attackTime,timeEpsilon);

			float3 dir = Grid::low.cells[target.stackID].centerVertex.xyz * Grid::ToRadius(target.height) - this->GetWorldPosition();
			float3 innerDir;
			Mat::rotate(this->inverse,dir,innerDir);
			Buffer0<M::TVec4<> > trajectory;

			const auto rs = TraceTrajectory(TrajectoryEndPoint(target),true,trajectory);
			if (rs != TraceResult::InvalidTrajectory)
			{
				float change = 1.f / float(GetAttacksPerRound());
				attackActionLeft -= change;
				if (!turret->independentAttackAndMotion)
					movementActionLeft -= change;
				RegisterChange(ActionChanged,entityTime);

				float flightTime;

				if (!IsBeam(turret->trajectoryType))
				{
					float len = 0;
					for (index_t i = 1; i < trajectory.GetLength(); i++)
						len += Vec::distance(ToWorld(trajectory[i-1]),ToWorld(trajectory[i]));

					flightTime =  Times::Attack * len;
				
				
					{
						//float delta = std::max(fabs(angles.x - turretAngleX), fabs(angles.y - turretAngleY));
						float rotTime = Times::TurretRotation/**delta*/;
						ApplicationInterface::SignalTurretRotation(To<Vehicle>::Cast(shared_from_this()),(ToWorld(trajectory[1]) - ToWorld(trajectory.First())).Normalized0(),rotTime);
						entityTime += rotTime;

					//	M::TVec3<> t0;
					//	Grid::HCellIdI t1;
					//	if (RecompileTrajectory(t0,t1))
					//	{
					//		Grid::Explode(t1,10.f);
					//	}

					}
				}
				else
					flightTime =  Times::BeamAttack + timeEpsilon;// Times::Attack * 10;;

				ApplicationInterface::TAttack at;
				at.attackFrom = To<MobileEntity>::Cast(shared_from_this());
				at.attackAngle = rs.artilleryAngle;
				at.flightTime = flightTime;
				at.isAutoAttack = false;
				at.traceTo = rs.impactAt;

				ApplicationInterface::SignalAttack(at);
				ChangeStorage(*this,entityTime,turret->ammoType,-turret->GetAmmoNeedPerShot(owner));
				
				entityTime += flightTime;

				if (dealsEMPDamage)
					ExplodeEMP(at,rs.impactCell,rs.impactAt,explosionSize, damageLevel,entityTime);
				else
					Explode(at,rs.impactCell,rs.impactAt,explosionSize, damageLevel,isDoomsday, entityTime);
			}

		}


		/*virtual override*/ void				MobileEntity::Attack(float&attackTime)
		{
			//ASSERT1__(NextTaskIsAttack(),this->id.ToString());
			if (!NextTaskIsAttack())	//got tired of this
				return;
			float change = 1.f / float(GetAttacksPerRound());
			ASSERT_LESS_OR_EQUAL__(change,attackActionLeft);
			if (!CanAttack())
				return;


			Task&task = LockedPeekTask();

			OverrideExecuteAttackNow(task.foci.Pop().target, GetAttackExplosionRadius(), (int)floor(turret->GetDamageLevel(owner)),turret->dealsEMPDamage, turret->isDoomsday, attackTime);

			if (task.foci.IsEmpty())
			{
				UnlockTasks();
				EraseFirstTask();
			}
			else
			{
				task.Replot(location,this,FocusChangeType::FirstFocusDropped,true);
				UnlockTasks();
			}
			RegisterChange(TasksChanged,entityTime);
			//stateChangedAt = attackTime;	//problematic
			attackTime = entityTime;
		}




	#else
		void MobileEntity::Relocate(const Grid::LCellIdI&toLocation, bool updateSystem)
		{
			if (toLocation == location)
				return;
			LogEvent(EventSignificance::Common,this,"Relocating to "+ToString(toLocation)+" ("+String(updateSystem)+")");
			static Grid::HighSetup::TVolumeDescriptor desc;
			float t = 0.f;
			if (owner->SharesSensorsWithMe())
			{
				UnregisterSensory(t);
			}
			if (IsVehicle())
			{
				Objects::RemoveVehicleFromGrid(To<Objects::Vehicle>::Cast(shared_from_this()),location);
			}
			location = toLocation;
			if (IsVehicle())
			{
				Objects::InsertVehicleIntoGrid(To<Objects::Vehicle>::Cast(shared_from_this()),location);
			}
			if (owner->SharesSensorsWithMe())
			{
				RegisterSensory(t);
			}
			#ifdef THOROUGLY_DEBUG_SENSORY
				if (owner->SharesSensorsWithMe())
					VerifySensory();
			#endif

			if (updateSystem)
				UpdateSystem();
		}

	#endif


	bool	MobileEntity::Task::AppendPathSegment( const MobileEntity*v, const Grid::LCellIdI&prev, const Task::Focus&target )
	{
		if (v->IsVehicle())
		{
			//would be faster, but too complicated to extract cost
			//if (target.isExplicit)
			//{
			//	Grid::LCellIdI	possibilities[6 * 3];
			//	count_t numWays = Objects::GetWays(v->owner->knownWorld,(const Objects::Vehicle*)v,prev,possibilities,target.target,true,true);
			//	bool found = false;
			//	for (index_t i = 0; i < numWays; i++)
			//		if (possibilities[i] == target.target)
			//		{
			//			found = true;
			//			break;
			//		}
			//	if (!found)
			//		return false;
			//	path << target.target;
			//}
			//else
			{
				if (!Pathfinding::AppendShortestPathI((const Vehicle*)v,prev,target.target,path))
					return false;
			}
		}
		else
		{
			#ifndef NO_SATELLITES
				if (v->IsSatellite())
				{
					count_t offset = path.Count();
					if (!AppendShortestPathI((const Satellite*)v,prev,target.target.stackID,path))
						return false;
					#ifndef CLIENT
						if (target.isDetailed && path.Count() - offset != 1)
						{
							path.Truncate(offset);
							return false;
						}
					#endif
				}
				else
			#endif
				FATAL__("Routing not implemented for "+v->GetTypeName());
		}

		return true;
	}

	bool MobileEntity::Task::UpdatePath( const MobileEntity*v, const Grid::LCellIdI&pointOfOrigin, FocusChangeType type )
	{
		if (type == FocusChangeType::NoChange)
			return true;
		auto it = foci.end();
		if (!job)
		{
			bool newPath = false;
			if (path.IsEmpty())
			{
				type = FocusChangeType::FullChange;
			}
			switch (type)
			{
				case FocusChangeType::FullChange:
				case FocusChangeType::SituationChanged:
				case FocusChangeType::FirstFocusDropped:
					newPath = true;
				break;
				case FocusChangeType::FirstSegmentChanged:
				{
					if (foci.IsEmpty())
					{
						newPath = true;
					}
					else
					{					
						auto it = path.begin();
						const auto nextFocus = foci.Peek();
						while (it != path.end() && it->cell != nextFocus.target)
							++it;
						if (it == path.end())
						{
							newPath = true;
						}
						else
						{
							++it;
							if (it == path.end())
							{
								newPath = true;
							}
							else
							{
								index_t offset = it-path.begin();
								Array<PathNode>	nodes = path.SubRef(offset);
								path.Clear();
								path << PathNode(v->location,0);
								if (!AppendPathSegment(v,v->location,nextFocus))
								{
									newPath = true;
								}
								else
								{
									path.AppendAll(nodes);
								}
							}

						}
					}
				}
				break;
				case FocusChangeType::LastFocusDropped:
				{
					if (foci.IsNotEmpty())
					{
						Grid::LCellIdI lookFor = foci.GetNewest().target;
						while (path.IsNotEmpty() && path.Last().cell != lookFor)
							path.EraseLast();
					}
					else
						path.Clear();
					if (path.IsEmpty())
						newPath = true;
				}
				break;
				case FocusChangeType::FocusAppended:
				{
					const Grid::LCellIdI&prev = path.Last().cell;
					if (!AppendPathSegment(v,prev,foci.GetNewest()))
						return false;
				}
				break;
			}
			if (newPath)
			{
				path.Clear();
				path << PathNode(pointOfOrigin,0);
				foreach (foci,f)
				{
					const Grid::LCellIdI&prev = f == foci.begin() ? pointOfOrigin : it->target;
					if (!AppendPathSegment(v,prev,*f))
						return false;
					it = f;
				}
			}
		}
		else
		{
			path.Clear();
			path << PathNode(pointOfOrigin,0);
			if (v->IsVehicle())
			{
				if (!Pathfinding::AppendShortestDirectPath(pointOfOrigin,foci.Peek().target,path,false))
				{
					return false;	//this would be weird, but happens
				}
			}
			else
			{
				#ifndef NO_SATELLITES
					if (v->IsSatellite())
					{
						if (!AppendShortestDirectSatellitePath(pointOfOrigin,foci.Peek().target.stackID,path))
							return false;
					}
					else
				#endif
					FATAL__("Routing not implemented for "+v->GetTypeName());

			}
		}
		#ifdef _DEBUG
			static StringBuffer strPlottedCourse;
			strPlottedCourse.Clear();
			foreach (path,node)
			{
				strPlottedCourse << ' ' << node->cell.stackID<<'.'<<node->cell.height;
			}
			LogEvent(EventSignificance::Debug,v,strPlottedCourse.ToStringRef());
		#endif
		return true;
	}
	

	
	bool					MobileEntity::Task::Replot( const Grid::LCellIdI&pointOfOrigin, const MobileEntity*parent, FocusChangeType t, bool recompileVisualization )
	{
		switch (type)
		{
			case Job:
			{
				if (!job || !job->movesToFocusWhenDone || foci.IsEmpty())
					break;
				std::shared_ptr<VehicleJob::PathJob> pjob = To<VehicleJob::PathJob>::DynamicCast(this->job);
				//if (foci.IsNotEmpty() || type == MoveTo)

				if (pjob)
				{
					if (foci.IsNotEmpty() && pjob->UpdatePath(*parent, pointOfOrigin, foci.Peek().target))
					{
						path.Clear();
						path << PathNode(pointOfOrigin,0);
						foreach(pjob->path,n)
						{
							path << PathNode(*n,1);
						}
						pjob->pathChanged = false;
					}
					#ifndef CLIENT
					else
						return false;
					#endif

					break;
				}
			}
			case MoveTo:
				//if (!job || job->movesToFocusWhenDone)
				{
					if (!UpdatePath(parent,pointOfOrigin,t))
						return false;
				}
			break;
			case Mode:

			break;
		}
		if (recompileVisualization)
			onVisualizationOutdated(*parent,*this,pointOfOrigin);
		return true;
	}

	Observable<const MobileEntity&, MobileEntity::Task&,const Grid::LCellIdI&>	MobileEntity::Task::onVisualizationOutdated;



	bool					MobileEntity::HasMotionTasks() const
	{
		if (!HasTasks())
			return false;
		foreach (myTasks,t)
		{
			if (t->ContainsMovement())
				return true;
		}
		return false;
	}



	bool					MobileEntity::ConstructingHere(const Grid::LCellIdI&loc) const
	{
		if (!HasTasks())
			return false;
		Grid::LCellIdI at = location;
		foreach (myTasks,t)
		{
			if (t->IsJob() && (at == loc || at+1 == loc || at-1 == loc))
			{
				#ifdef CLIENT
					if (t->name.BeginsWithIgnoreCase("construct "))
				#else
					if (To<VehicleJob::Construct>::DynamicCast(t->job))
				#endif
				return true;
			}
			if (t->ContainsMovement())
				at = t->foci.GetNewest().target;
		}
		return false;
	}


	Grid::LCellIdI			MobileEntity::GetLastPointOfOrigin() const
	{
		if (!HasTasks())
			return location;
		index_t at = myTasks.Count()-1;
		while (at < myTasks.Count() && !myTasks[at].ContainsMovement())
			at--;
		if (at >= myTasks.Count() || myTasks[at].foci.IsEmpty())
			return location;
		return myTasks[at].foci.GetNewest().target;
	}


	Grid::LCellIdI			MobileEntity::GetPrevPointOfOrigin() const
	{
		if (!HasTasks())
			return location;
		index_t at = myTasks.Count()-1;
		while (at < myTasks.Count() && !myTasks[at].ContainsMovement())
			at--;
		if (at >= myTasks.Count())
			return location;
		if (at == myTasks.Count()-1)
		{
			at --;
			while (at < myTasks.Count() && !myTasks[at].ContainsMovement())
				at--;
			if (at >= myTasks.Count())
				return location;
		}

		return myTasks[at].ContainsMovement() ? myTasks[at].foci.GetNewest().target : location;
	}




	

	int MobileEntity::GetCurrentMineralConsumption() const
	{
		if (myTasks.IsNotEmpty() && myTasks.Peek().job)
			return std::min(myTasks.Peek().job->resourceCostLeft,GetOperationSpeed(myTasks.Peek().job->GetOperationAffinity())) ;
		return 0;
	}


	/*virtual override*/bool				MobileEntity::HasAmmoForShot() 
	{
		int ammoNeed = turret->GetAmmoNeedPerShot(owner);
		if (!ammoNeed)
			return true;

		#ifdef ONE_COMPOUND_PER_FACTION
			const Compound&compound = owner->compound;
			return compound.total.storage.level[turret->ammoType] >= ammoNeed;
		#else
			return currentStorage.level[turret->ammoType] >= ammoNeed;
		#endif
	}

}


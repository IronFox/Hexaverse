#include <global_root.h>
#include "mobileEntity.h"
#include "faction.h"
#include "gridAndChoices.h"
#include "rendering.h"
#include "vehicle.h"
#include "satellite.h"
#include "systems.h"
#include "prototypes.h"

namespace Objects
{
	Buffer0<PMobileEntity>			storedMobileEntities;






	void						CheckMobileEntitySanity()
	{
		for (const auto&v : vehicles)
		{
			ASSERT1__(v->isInserted,v->GetName());
			ASSERT1__(v->health > 0,v->GetName());
			ASSERT1__(GetEntity(v->id) == v, v->GetName());
			ASSERT1__(!storedMobileEntities.Contains(v), v->GetName());
			ASSERT1__(v->storedIn.expired(), v->GetName());
			#ifndef CLIENT
				ASSERT2__(!v->owner->HasMobileInStorage(v),v->GetName(),v->owner->GetName());
				ASSERT2__(v->owner->HasActiveMobile(v),v->GetName(),v->owner->GetName());
			#endif
		}
		for (const auto&v : satellites)
		{
			ASSERT1__(v->isInserted,v->GetName());
			ASSERT1__(!v->isInsertedIntoGrid,v->GetName());	//satellites are never inserted into grid
			ASSERT1__(v->health > 0,v->GetName());
			ASSERT1__(GetEntity(v->id) == v, v->GetName());
			ASSERT1__(!storedMobileEntities.Contains(v), v->GetName());
			ASSERT1__(v->storedIn.expired(), v->GetName());
			#ifndef CLIENT
				ASSERT2__(!v->owner->HasMobileInStorage(v),v->GetName(),v->owner->GetName());
				ASSERT2__(v->owner->HasActiveMobile(v,true),v->GetName(),v->owner->GetName());
				ASSERT2__(v->owner->HasActiveMobile(v),v->GetName(),v->owner->GetName());
			#endif
		}
		for (const auto&v : storedMobileEntities)
		{
			ASSERT1__(!v->isInserted,v->GetName());
			ASSERT1__(!v->isInsertedIntoGrid,v->GetName());
			ASSERT1__(v->health > 0,v->GetName());
			ASSERT1__(GetEntity(v->id) == v, v->GetName());
			ASSERT1__(!vehicles.Contains(v),v->GetName());
			PHangarStructure hangar = v->storedIn.lock();
			//PVehicleStore storedIn = To<VehicleStore>::DynamicCast( v->storedIn.lock());
			ASSERT1__(hangar,v->GetName());
			ASSERT2__(hangar->HasStoredLocally(v),v->GetName(),hangar->ToEntity().GetName());

			#ifndef CLIENT
				ASSERT2__(v->owner->HasMobileInStorage(v),v->GetName(),v->owner->GetName());
				ASSERT2__(!v->owner->HasActiveMobile(v),v->GetName(),v->owner->GetName());
			#endif
		}

	}





	void MobileEntity::UpdateSystem()
	{
		ASSERT_LESS__(location.stackID, Grid::LowSetup::GridType::CellCount);
		TVec3<> oldLocation = system.w.xyz;
		const TVec3<>&up = Grid::low.cells[location.stackID].cornerCenter;
		//centerVertex.xyz;
		TVec3<> y, x,z;
		system.w.xyz = up * Grid::ToRadiusf(float(location.height) /*+ 0.025f*/);
		z = oldLocation- system.w.xyz;
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
		system.y.xyz = y;
		system.z.xyz = up;
		Vec::cross(system.y.xyz, system.z.xyz, system.x.xyz);
		Mat::resetBottomRow(system);

		Mat::invertSystem(system, inverse);
	}

	std::pair<int,int>					MobileEntity::GetBuildingProgress() const
	{
		if (!HasTasks())
			return std::pair<int,int>(0,1);
		int total = 0;
		for (index_t i = 0; i < myTasks.Count(); i++)
		{
			const auto&t = myTasks[i];
			if (!t.job || t.job->roundsToJobCompletion < 0)
				break;
			total += t.job->roundsToJobCompletion;
			if (t.job->movesToFocusWhenDone)
				break;
		}
		return constructionRoundCounter + total > 0 ? std::pair<int,int>(constructionRoundCounter,constructionRoundCounter + total) : std::pair<int,int>(0,1);
	}


	#ifndef CLIENT

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



		bool					MobileEntity::HandleNormalTask()
		{
			if (!HasTasks())
				return true;	//erase from motion queue
			Task&task = LockedPeekTask();
			jobStarted = false;

			const Grid::WorldData&world = owner->currentState;

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
							if (!task.UpdatePath(this,location))
							{
								bool changedAny = false;
								foreach (task.foci,f)
									if (!world.LowCellIsClear(*f))
									{
										(*f) ++;
										//location ++;
										bool abort = true;
										if (world.LowCellIsClear(*f) )
											changedAny = true;
									}
								if (changedAny)
								{
									if (!task.UpdatePath(this,location))
									{
										LogEvent(EventType::CommandRejected,this,"Route to destination lost. Motion aborted.");
										this->UnlockTasks();
										this->ClearAllTasks(true, CLOCATION);
										return true;
									}
									LogEvent(EventType::EntityProcess,this,"Revised path due to terrain changes.");
								}
								else
								{
									LogEvent(EventType::CommandRejected,this,"Route to destination lost. Motion aborted.");
									this->UnlockTasks();
									ClearAllTasks(true, CLOCATION);
									return true;
									//hasMoved = true;
									//LogEvent(EventType::CommandRejected,this,"Route to destination blocked. Motion delayed.");
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
							int cost = task.path.IsNotEmpty() ? task.path.last().movementCostsFromPrevNode : 0;
							ASSERT__(HasTasks());	//just to be sure...
							while (task.path.IsNotEmpty() && (remaining >= cost || (!hasMoved && remaining > 0)))
							{
								ASSERT__(HasTasks());	//STILL HAPPENS!!!!!!!!
								Grid::LCellIdI previousLocation = location;
								float change = float(cost) / float(GetMovementStepsPerRound());
								bool stop = false;
								MobilePlacementResult rs = this->PlaceExactlyAt(task.path.last().cell,Times::Move * change,0);
								switch (rs)
								{
									case MobilePlacementResult::PlacementOkay:
									{
										ASSERT__(HasTasks());
										if (task.path.last().cell == task.foci.Peek())
											task.foci.EraseFront();

										remaining-=task.path.last().movementCostsFromPrevNode;
										if (!AttackIgnoresMotion())
											attackActionLeft -= change;
										movementActionLeft -= change;
										attackActionLeft = std::max(attackActionLeft,0.f);	//can happen
										movementActionLeft = std::max(movementActionLeft,0.f);
										hasMoved = true;
										if (AttackIsStationary())
											attackActionLeft = 0;

										RegisterChange(ActionChanged,timeAt);
										task.path.EraseLast();
										cost = task.path.IsNotEmpty() ? task.path.last().movementCostsFromPrevNode : 1;

										if (completeOnMove)
										{
											if (!CompleteOnMove(previousLocation))
											{
												UnlockTasks();
												return true;
											}
										}
									}
									break;
									case MobilePlacementResult::EnteredStructure:
									{
										dropRemaining = true;
										stop = true;
									}
									break;
									case MobilePlacementResult::PlacementOkayFellDown:
									{
										LogEvent(EventType::EntityProcessFailed,this,"Falling event interrupted motion.");
										stop = true;
									}
									break;
									case MobilePlacementResult::PlacementFailedHarmfulOrBlocked:
										this->ClearAllTasks(true, CLOCATION);
									case MobilePlacementResult::PlacementFailedObstructed:
									{
										LogEvent(EventType::EntityProcessFailed,this,"Unable to complete movement.");
										if (health <= 0)
										{
											LogEvent(EventType::MinorEntityProcess,this,"Is dead.");
											if (completeOnMove)
											{
												CompleteOnMove(previousLocation);
											}
											this->UnlockTasks();
											return true;
										}
										//if (completeOnMove)
										//{
										//	unstable = true;
										//	RegisterChange(TasksChanged,timeAt);
										//}

										stop = true;
									}
									break;
									default:
										FATAL__("Unexpected placement result "+String(rs));
								}
								if (stop)
									break;
							}
							if (task.path.IsEmpty() || dropRemaining)
							{
								this->UnlockTasks();
								this->EraseFirstTask();
								//tasks.EraseFront();
								if (HasTasks() && To<VehicleJob::PathJob>::DynamicCast(myTasks.Peek().job))
									myTasks.Peek().Recompile(location,this);
								#ifndef CLIENT
									RegisterChange(TasksChanged,timeAt);
								#endif
								return remaining == 0;
							}
							else
							{
								if (HasTasks())
								{
									if (remaining < task.path.last().movementCostsFromPrevNode)
									{
										remaining = 0;
										if (!AttackIgnoresMotion())
											attackActionLeft = 0;
										movementActionLeft = 0;
										RegisterChange(ActionChanged,timeAt);
									}

									hasMoved = true;
									task.path << PathNode(location,0);
									task.path.Revert();
								
									task.Recompile(location,this);
									#ifdef CLIENT
										if (selectedEntity.get() == this)
											UpdateChoices();
									#endif
								}
								this->UnlockTasks();
								return remaining == 0;
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
			return true;
		}



		bool			MobileEntity::CompleteOnMove(const Grid::LCellIdI&referenceLocation)
		{
			LogEvent(EventType::EntityProcess,this,"Completing on-move job '"+completeOnMove->GetName()+ "'");
			VehicleJob::BaseJob::continue_t rs = completeOnMove->Finish(*this,referenceLocation);
			completeOnMove.reset();
			unstable = false;
			if (rs == VehicleJob::BaseJob::TerminalFailureVehicleDestroyed)
			{
				LogEvent(EventType::EntityProcess,this,"Terminal failure. Remaining tasks dropped. Vehicle destroyed.");
				jobStarted = false;
				ClearAllTasks(true, CLOCATION);
				unstable = false;
				Remove(shared_from_this(),RemovalType::UncontrolledExplosion,timeAt,false,false);
				return false;
			}
			if (rs == VehicleJob::BaseJob::TerminalFailure)
			{
				LogEvent(EventType::EntityProcess,this,"Terminal failure. Remaining tasks dropped.");
				jobStarted = false;
				ClearAllTasks(true, CLOCATION);
				unstable = false;
				return false;
			}
			return true;
		}
	



		bool			MobileEntity::PerformNonAttack()
		{
			if (!HasTasks())
			{
				if (completeOnMove)
				{
					if (!hasTriedPlottingOnMoveAutoMotion)
					{
						Grid::LCellIdI	ways[6 * 3];

						count_t numWays = GetMobileWays(owner->currentState, this,  location, ways, Grid::LCellIdI::Invalid, false,false);
						ASSERT_LESS__(numWays,ARRAYSIZE(ways));
						//random first to maybe not make the same mistake twice
						for (index_t i = 0; i < numWays; i++)
						{
							static Random random;
							index_t target = random.getIndex(numWays-1);
							if (PlotCourse(ways[target],false))
							{
								hasTriedPlottingOnMoveAutoMotion = true;
								LogEvent(EventType::MinorEntityProcess,this,"Automatically moving to finish '"+completeOnMove->GetName()+"'");
								return false;	//retry
							}
						}
						//iterative, in case random missed some option
						for (index_t i = 0; i < numWays; i++)
							if (PlotCourse(ways[i],false))
							{
								hasTriedPlottingOnMoveAutoMotion = true;
								LogEvent(EventType::MinorEntityProcess,this,"Automatically moving to finish '"+completeOnMove->GetName()+"'");
								return false;	//retry
							}
					}
					LogEvent(EventType::EntityProcessFailed,this,"Unable to automatically relocate. Move manually to finish '"+completeOnMove->GetName()+"'");
				}
				return true;
			}
			bool rs = false;
			//LockTasks();

			//Task&task = myTasks.Peek();
			RegisterChange(TasksChanged,timeAt);

			if (myTasks.Peek().job)
			{
				if (this->empDamageLevel)
				{
					LogEvent(EventType::EntityProcessFailed,this,"EMP damage: Unable to continue job at this time.");
					return true;
				}
				//const Grid::LCellIdI loc = task.foci.IsNotEmpty() ? task.foci.Peek() : location;
				if (!jobStarted)
				{
					//jobStarted = true;
					//if (To<Job::PathJob>::DynamicCast(task.job))
					//	task.Recompile(location,this);
					//ASSERT__(!completeOnMove || tasks.IsEmpty() || !tasks.Peek().job);

					while (HasTasks() && myTasks.Peek().job)
					{
						VehicleJob::BaseJob::begin_t b = myTasks.Peek().job->BeginHere(*this,myTasks.Peek().GetFocus(location),!!completeOnMove);
						bool cont = true;
						switch (b)
						{
							case VehicleJob::BaseJob::Delayed:
								LogEvent(EventType::EntityProcess,this,"Delayed Job");
							break;
							case VehicleJob::BaseJob::Started:
								LogEvent(EventType::MinorEntityProcess,this,"Started '"+myTasks.Peek().job->GetName()+"'");
								cont = false;
								jobStarted = true;
								//movementActionLeft = 0;
							break;
							case VehicleJob::BaseJob::CannotStartAbort:
								LogEvent(EventType::MinorEntityProcess,this,"Cannot start '"+myTasks.Peek().job->GetName()+"'. Skipping");
								EraseFirstTask();
								RegisterChange(TasksChanged,timeAt);
							break;
						}
						if (!cont)
							break;
					};

					if (!HasTasks())
						return true;
					if (myTasks.Peek().IsMode(CursorMode::AttackMode))
						return false;

					if (!myTasks.Peek().job)
					{
						LogEvent(EventType::MinorEntityProcess,this,"Executing normal task");
						return HandleNormalTask();
					}
				}
				if (!hasConstructedThisRound && HasTasks() && myTasks.Peek().job)
				{
					hasConstructedThisRound = true;
					constructionRoundCounter ++;
					ASSERT__(jobStarted);	//supposedly it can happen that this is false
					PVehicleJob job = myTasks.Peek().job;
					switch (job->Continue(*this,myTasks.Peek().GetFocus(location),!hasMoved))
					{
						case VehicleJob::BaseJob::ContinueJob:
						break;
						case VehicleJob::BaseJob::JobDone:
							LogEvent(EventType::MinorEntityProcess,this,"Job ended: '"+job->GetName()+"'");

							EraseFirstTask();
							RegisterChange(TasksChanged,timeAt);
							while (HasTasks() && myTasks.Peek().job && myTasks.Peek().job->IsImplicitlyDone(*this,myTasks.Peek().GetFocus(location)))
							{
								LogEvent(EventType::MinorEntityProcess,this,"Job has implicitly ended: '"+myTasks.Peek().job->GetName()+"'");

								EraseFirstTask();
							}
							if (HasTasks() && myTasks.Peek().job)
							{
								LogEvent(EventType::EntityProcess,this,"Next front job: '"+myTasks.Peek().job->GetName()+"'");
							}
							jobStarted = false;

						return false;
						case VehicleJob::BaseJob::TerminalFailureVehicleDestroyed:
							LogEvent(EventType::EntityProcess,this,"Terminal failure. Remaining tasks dropped. Vehicle destroyed.");
							jobStarted = false;
							ClearAllTasks(true, CLOCATION);
							unstable = false;

							Remove(shared_from_this(),RemovalType::UncontrolledExplosion,timeAt,false,false);
						return false;
						case VehicleJob::BaseJob::TerminalFailure:
							LogEvent(EventType::EntityProcess,this,"Terminal failure. Remaining tasks dropped.");
							jobStarted = false;
							ClearAllTasks(true, CLOCATION);
							unstable = false;
						return false;
					}
				}
				return true;
			}
			else
			{
				if (HasTasks() && myTasks.Peek().IsMode(CursorMode::AttackMode))
					return false;

				return HandleNormalTask();
			}
			return true;
		}

		void					MobileEntity::RegisterNewJob(const String&localizableShapeIdentifier, const PVehicleJob&job, const Grid::LCellIdI*destination)
		{
			/*{
				if (job->mineralCostLeft > this->currentStorage.level[Resource::Minerals])
				{
					LogEvent(GetName()+": Unable to comply. Insufficient minerals left in storage.");
					return;
				}
			}*/

			ASSERT__(!TasksAreLocked());

			Grid::LCellIdI origin = this->GetLastPointOfOrigin();
		
			RegisterChange(TasksChanged,timeAt);

			if (myTasks.IsEmpty())
			{
				constructionRoundCounter = 0;
			}

			Task&task = myTasks.Push();
			task.type = Task::Job;
			task.name = job->GetName();
			if (destination)
				task.foci << *destination;
			#ifdef CLIENT
				if (localizableShapeIdentifier.IsNotEmpty())
				{
					foreach (structurePrototypes,p)
						if ((*p)->GetTypeName() == localizableShapeIdentifier)
						{
							((DX11::BaseInstructionBlock&) task.localizableShape) = jobShader;

							task.localizableShape.vbo = (*p)->localizableShape->vbo;
							task.localizableShape.ibo = (*p)->localizableShape->ibo;
						}
				}
			#endif
			task.job = job; 
			if (!task.Recompile(origin,this))
			{
				LogEvent(EventType::EntityProcessFailed,this, "Failed to compile task. Unable to enqueue job.");
				myTasks.EraseBack();
				return;
			}
			#ifdef CLIENT
				if (selectedEntity.get() == this)
					UpdateChoices();
			#else
				LogEvent(EventType::MinorEntityProcess,this,"Enqueued new job '"+job->GetName()+"'");
			#endif
		}
	
		void					MobileEntity::RegisterNewFrontJob(const String&localizableShapeIdentifier, const PVehicleJob&job, const Grid::LCellIdI*destination)
		{
			ASSERT__(!TasksAreLocked());

			RegisterChange(TasksChanged,timeAt);
			Task&task = myTasks.PushFront();
			task.type = Task::Job;
			task.name = job->GetName();
			if (destination)
				task.foci << *destination;
			#ifdef CLIENT
				if (localizableShapeIdentifier.IsNotEmpty())
				{
					foreach (structurePrototypes,p)
						if ((*p)->GetTypeName() == localizableShapeIdentifier)
						{
							((DX11::BaseInstructionBlock&) task.localizableShape) = jobShader;

							task.localizableShape.vbo = (*p)->localizableShape->vbo;
							task.localizableShape.ibo = (*p)->localizableShape->ibo;
						}
				}
			#endif

			task.job = job; 
			task.Recompile(location,this);

			#ifdef CLIENT
				if (selectedEntity.get() == this)
					UpdateChoices();
			#else
				LogEvent(EventType::MinorEntityProcess,this,"Enqueued new front-job '"+job->GetName()+"'");
			#endif
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
			myTasks.EraseFront();
		}

		bool			MobileEntity::PlotCourse(Grid::LCellIdI destination, bool virtualPlatformMode/*=false*/)
		{
			const Grid::WorldData&world = owner->currentState;
			if (IsVehicle())
			{
				while (!world.LowCellIsClear(destination) && !MobileDestinationStructureHere(destination,*this))
					destination++;
				while (world.LowCellIsClear(destination-1) || MobileDestinationStructureHere(destination-1,*this))
					destination--;
				if (!virtualPlatformMode && !((Vehicle*)this)->CanStandOn(world,destination -1,false,false))
					return false;
			}

			Grid::LCellIdI pointOfOrigin = GetLastPointOfOrigin();

			if (destination == pointOfOrigin)
			{
				return false;
			}

			if (!HasTasks() || myTasks.GetNewest().type != Task::MoveTo)
			{
				ASSERT__(!TasksAreLocked());
				myTasks << Task("Move To",destination,Task::MoveTo);
				if (!myTasks.GetNewest().Recompile(pointOfOrigin, this))
				{
					LogEvent(EventType::CommandRejected,this,"Unable to plot path to destination. Task rejected.");
					myTasks.EraseBack();
					return false;
				}
				RegisterChange(TasksChanged,-1);
			}
			else
			{
				if (myTasks.GetNewest().foci.IsEmpty() || myTasks.GetNewest().foci.GetNewest() != destination)
				{
					myTasks.GetNewest().foci << destination;
					if (!myTasks.GetNewest().Recompile(GetPrevPointOfOrigin(), this))
					{
						LogEvent(EventType::CommandRejected,this,"Unable to plot path to destination. Task rejected.");
						myTasks.GetNewest().foci.EraseBack();
						return false;
					}
					RegisterChange(TasksChanged,-1);
				}
			}

			return true;
		}

		bool	MobileEntity::PlotFrontCourse(Grid::LCellIdI destination)
		{
			const Grid::WorldData&world = owner->currentState;

			if (IsVehicle())
			{
				while (!world.LowCellIsClear(destination))
					destination++;
				if (!((Vehicle*)this)->CanStandOn(world,destination -1,false,false))
					return false;
			}

			if (destination == location)
			{
				return false;
			}

			{
				ASSERT__(!TasksAreLocked());

				myTasks.PushFront(Task("Move To",destination,Task::MoveTo));
				myTasks.GetOldest().Recompile(location, this);
				RegisterChange(TasksChanged,timeAt);
			}

			return true;
		}
		

	
	
		void		MobileEntity::DropLastTask()
		{
			if (!HasTasks())
				return;
			ASSERT__(!TasksAreLocked());
			if (myTasks.GetNewest().type == Task::MoveTo && myTasks.GetNewest().foci.Count() > 1)
			{
				myTasks.GetNewest().foci.EraseBack();
				myTasks.GetNewest().Recompile(GetPrevPointOfOrigin(),this);
			}
			else
			{
				if (myTasks.GetNewest().IsMode(CursorMode::AttackMode) && myTasks.GetNewest().foci.Count() > 1)
				{
					myTasks.GetNewest().foci.EraseBack();
					myTasks.GetNewest().Recompile(GetPrevPointOfOrigin(),this);
				}
				else
					myTasks.EraseBack();
			}
			RegisterChange(TasksChanged,timeAt);
		}


		/*virtual override*/ void				MobileEntity::Attack(float&attackTime)
		{
			//ASSERT1__(NextTaskIsAttack(),this->id.ToString());
			if (!NextTaskIsAttack())	//got tired of this
				return;
			float change = 1.f / float(GetAttacksPerRound());
			ASSERT_LESS_OR_EQUAL__(change,attackActionLeft);
			if (!HasAmmoForShot())
				return;

			LogEvent(EventType::Debug,this,"Setting local time ("+String(timeAt)+") to attack time="+String(attackTime));
			timeAt = attackTime;

			Task&task = LockedPeekTask();

			const Grid::LCellIdI target = task.foci.Pop();
			float3 dir = Grid::low.cells[target.stackID].centerVertex.xyz * Grid::ToRadius(target.height) - this->system.w.xyz;
			float3 innerDir;
			Mat::rotate(this->inverse,dir,innerDir);
			TVec3<> t0;
			Grid::HCellIdI t1;
			Buffer0<TVec4<> > trajectory;

							
			if (TraceTrajectory(Grid::actualWorld, target,trajectory,t0,t1))
			{
				float change = 1.f / float(GetAttacksPerRound());
				attackActionLeft -= change;
				if (!turret->ignoresVehicleMotion)
					movementActionLeft -= change;
				RegisterChange(ActionChanged,timeAt);

				float flightTime;

				if (turret->trajectoryType != TrajectoryType::VerticalBeam)
				{
					float len = 0;
					for (index_t i = 1; i < trajectory.length(); i++)
						len += Vec::distance(ToWorld(trajectory[i-1]),ToWorld(trajectory[i]));

					flightTime =  Times::Attack * len;
				
				
					{
						//float delta = std::max(fabs(angles.x - turretAngleX), fabs(angles.y - turretAngleY));
						float rotTime = Times::TurretRotation/**delta*/;
						SignalTurretRotation(To<Vehicle>::Cast(shared_from_this()),(ToWorld(trajectory[1]) - ToWorld(trajectory.first())).Normalized0(),rotTime);
						timeAt += rotTime;

					//	TVec3<> t0;
					//	Grid::HCellIdI t1;
					//	if (RecompileTrajectory(t0,t1))
					//	{
					//		Grid::Explode(t1,10.f);
					//	}

					}
				}
				else
					flightTime =  timeEpsilon;// Times::Attack * 10;;


				SignalAttack(To<Vehicle>::Cast(shared_from_this()),target,flightTime);
				ChangeStorage(*this,timeAt,turret->ammoType,-turret->ammoNeedPerShot);
				
				timeAt += flightTime;

				if (turret->dealsEMPDamage)
					ExplodeEMP(To<Vehicle>::Cast(shared_from_this()),target,flightTime,
								t1, t0, turret->explosionRadius, turret->damageLevel,timeAt);
				else
					Explode(To<Vehicle>::Cast(shared_from_this()),target,flightTime,
							t1, t0, turret->explosionRadius, turret->damageLevel,turret->isDoomsday, timeAt);
			}

			if (task.foci.IsEmpty())
			{
				UnlockTasks();
				EraseFirstTask();
			}
			else
			{
				task.Recompile(location,this);
				UnlockTasks();
			}
			RegisterChange(TasksChanged,timeAt);
			//stateChangedAt = attackTime;	//problematic
			attackTime = timeAt;
		}




	#else
		void MobileEntity::Relocate(const Grid::LCellIdI&toLocation, bool updateSystem)
		{
			if (toLocation == location)
				return;
			#ifdef HAS_WINDOW
				static Grid::HighSetup::TVolumeDescriptor desc;
				float t = 0.f;
				if (isMine)
				{
					UnregisterSensory(t);
				}
			#endif
			location = toLocation;
			#ifdef HAS_WINDOW
				if (isMine)
				{
					RegisterSensory(t);
				}
				#ifdef THOROUGLY_DEBUG_SENSORY
					if (isMine)
						VerifySensory();
				#endif
			#endif

			if (updateSystem)
				UpdateSystem();
		}

	#endif

	bool MobileEntity::Task::UpdatePath(const MobileEntity*v, const Grid::LCellIdI&pointOfOrigin)
	{
		path.Clear();
		path << PathNode(pointOfOrigin,0);
		auto it = foci.end();
		if (!job)
		{
			foreach (foci,f)
			{
				const Grid::LCellIdI&prev = f == foci.begin() ? pointOfOrigin : *it;
				if (v->IsVehicle())
				{
					if (!AppendShortestPathI((const Vehicle*)v,prev,*f,path))
						return false;
				}
				else
				{
					if (v->IsSatellite())
					{
						if (!AppendShortestPathI((const Satellite*)v,prev,f->stackID,path))
							return false;
					}
					else
						FATAL__("Routing not implemented for "+v->GetTypeName());

				}
				it = f;
			}
		}
		else
		{
			if (v->IsVehicle())
			{
				if (!AppendShortestDirectPath(pointOfOrigin,foci.Peek(),path,false))
				{
					return false;	//this would be weird, but happens
				}
			}
			else
			{
				if (v->IsSatellite())
				{
					if (!AppendShortestDirectSatellitePath(pointOfOrigin,foci.Peek().stackID,path))
						return false;
				}
				else
					FATAL__("Routing not implemented for "+v->GetTypeName());

			}
		}
		return true;
	}
	

	
	bool					MobileEntity::Task::Recompile(const Grid::LCellIdI&pointOfOrigin, const MobileEntity*parent)
	{
		switch (type)
		{
			case Job:
			{
				std::shared_ptr<VehicleJob::PathJob> job = To<VehicleJob::PathJob>::DynamicCast(this->job);
				//if (foci.IsNotEmpty() || type == MoveTo)

				if (!job || job->movesToFocusWhenDone)
				{
					if (job)
					{
						if (job->UpdatePath(*parent, pointOfOrigin, foci.Peek()))
						{
							path.Clear();
							path << PathNode(pointOfOrigin,0);
							foreach(job->path,n)
							{
								path << PathNode(*n,1);
							}
							#ifdef HAS_WINDOW
								RecompilePath(1);
							#endif
							job->pathChanged = false;
						}
						#ifndef CLIENT
						else
							return false;
						#endif
					}
					break;
				}
			}
			case MoveTo:
				//if (!job || job->movesToFocusWhenDone)
				{
					if (!UpdatePath(parent,pointOfOrigin))
						return false;
					#ifdef HAS_WINDOW
						RecompilePath(parent->GetMovementStepsPerRound(),parent->GetPathSize());
					#endif
				}
				
			break;
			case Mode:
				switch (mode)
				{
					case CursorMode::AttackMode:
					{
						#ifdef HAS_WINDOW		
							static Buffer0<TVec4<> > trajectory;
							Geometry geo;
								localizableShapes.Clear();
							foreach (foci,f)
							{
								trajectory.Clear();
								TVec3<> dummy0;
								Grid::HCellIdI dummy1;
								if (parent->TraceTrajectoryFrom(Grid::actualWorld, pointOfOrigin,*f,trajectory,dummy0,dummy1))
								{
									Component::Turret::CompileTrajectory(trajectory,true,geo,0.25f);

									Shape&shape = localizableShapes.Append();
									((DX11::BaseInstructionBlock&) shape.instruction) = jobShader;
									shape.instruction.vbo = sphereVBO[0];
									shape.instruction.ibo = sphereIBO[0];
									shape.depthTest = true;

									BuildSystem(trajectory.last().xyz * trajectory.last().w,parent->GetAttackExplosionRadius(),  parent->GetAttackExplosionRadius(),shape.system);
								}
							}
							if (visualization.shader.IsEmpty())
								(DX11::BaseInstructionBlock&)visualization = pathShader;
							visualization.vbo.CreateFromStructs(geo.GetLOD(0).vertices);
							visualization.ibo.Create(geo.GetLOD(0).indices);
						#endif
					}
					break;
				}
			break;
		}
		return true;
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
		return myTasks[at].foci.GetNewest();
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

		return myTasks[at].ContainsMovement() ? myTasks[at].foci.GetNewest() : location;
	}



	#ifdef HAS_WINDOW


	void					MobileEntity::Task::SetLocalizableShape(ConstName structureIdentifier, const Grid::LCellIdI&location)
	{
		PStructure p = 	GetPrototypeByName(structureIdentifier);

		if (p)
		{
			localizableShapes.SetSize(1,true);
			Shape&shape = localizableShapes.first();
			((DX11::BaseInstructionBlock&) shape.instruction) = jobShader;

			shape.instruction.vbo = p->localizableShape->vbo;
			shape.instruction.ibo = p->localizableShape->ibo;
			shape.depthTest = true;

			BuildStructureSystem(location,shape.system);
			return;
		}
		localizableShapes.Clear();
	}

	void					MobileEntity::Task::RecompilePath(int stepsPerRound, float structureSize/*=vehicleScale*/)
	{
		Geometry geo;

		struct TNode
		{
			float3	dir,
					normal;
			bool	stopHere;
		};

		Array<TNode>		nodes(path.length());
		
		int remaining = stepsPerRound;

		for (index_t i = 0; i+1 < path.Count(); i++)
		{
			TNode	&na = nodes[i],
					&nb = nodes[i+1];

			const Grid::LCellIdI&a = path[i].cell,
								&b = path[i+1].cell;
			if (remaining >= path[i+1].movementCostsFromPrevNode)
			{
				remaining -= path[i+1].movementCostsFromPrevNode;
				na.stopHere = false;
			}
			else
			{
				remaining = stepsPerRound;
				remaining -= path[i+1].movementCostsFromPrevNode;
				na.stopHere = true;
			}
			float3 p0 = Grid::low.cells[a.stackID].centerVertex.xyz * Grid::ToRadius(a.height);
			float3 p1 = Grid::low.cells[b.stackID].centerVertex.xyz * Grid::ToRadius(b.height);
			TVec3<> n,d = p1 - p0;
			Vec::cross(p0,d,n);
			na.normal += n;
			nb.normal += n*0.9f;
			na.dir += d;
			nb.dir += d*0.9f;
		}
		foreach (nodes, n)
		{
			Vec::setLen0(n->dir,structureSize);
			Vec::setLen0(n->normal,structureSize);
		}



		for (index_t i = 0; i+1 < path.Count(); i++)
		{
			TNode	&na = nodes[i],
					&nb = nodes[i+1];

			const Grid::LCellIdI&a = path[i].cell,
								&b = path[i+1].cell;
			float3 d0 = Grid::low.cells[a.stackID].centerVertex.xyz;
			float3 p0 = d0  * Grid::ToRadiusf(float(a.height)+0.5f);
			float3 d1 = Grid::low.cells[b.stackID].centerVertex.xyz;
			float3 p1 = d1 * Grid::ToRadiusf(float(b.height)+0.5f);

			float3 c0 = p0 + na.dir;
			float3 c1 = p1 - nb.dir;


			bool edge = i+2 < nodes.Count() && nodes[i+1].stopHere ;

			//!((i+1)% stepsPerRound);

			//TVec3<> d = p1 - p0;
			//if (a.height == b.height)

			static const int Res = 8;
			for (int j = (i!=0); j <= Res; j++)
			{
				float fj = float(j) / float(Res);
				TVec3<> p,n,d;

				float op = i > 0 ? 1.f : fj;
				
				Vec::resolveBezierCurvePoint(p0,c0,c1,p1,fj,p);
				//Vec::resolveBezierCurveAxis(p0,c0,c1,p1,fj,d);
				//Vec::cross(p,d,n);
				Vec::interpolate(na.normal,nb.normal,fj,n);
				Vec::setLen(n,structureSize);
				if (j > 0)
				{
					geo.MakeQuad(0,1,4,3);
					geo.MakeQuad(1,2,5,4);

				}
				//if (!i)
				//{
				//	geo.MakeVertex(p0 + direction[i],d0,float3(0));
				//	geo.MakeVertex(p0 - direction[i],d0,float3(1));
				//}

				geo.SetVertexOffsetToCurrent();

				{
					geo.MakeVertex(p + n,d1,float3(op,1,1));
					if (j < Res || !edge)
						geo.MakeVertex(p,d1,float3(0));
					else
						geo.MakeVertex(p,d1,float3(op,1,1)*0.7f);
					geo.MakeVertex(p - n,d1,float3(op,1,1));
				}
				if (i+2 == path.Count() && fj > 0.5f)
				{
					//geo.SetVertexOffsetToCurrent();
					//geo.MakeVertex(p + n*2.f,d1,float3(0));
					//geo.MakeVertex(p - n*2.f,d1,float3(1));
					break;
				}
			}
		}
		if (path.Count() > 1)
		{
			const Grid::LCellIdI&b = path.last().cell;

			float3 d1 = Grid::low.cells[b.stackID].centerVertex.xyz;
			float3 p1 = d1 * Grid::ToRadiusf(float(b.height)+0.5f);

			//float s = std::max(0.f,2.f - (fj-0.5f)*4.f);
			geo.MakeVertex(p1,d1,float3(1));
			geo.MakeTriangle(0,1,3);
			geo.MakeTriangle(1,2,3);
		}

		//geo.Scale(vehicleScale);
		if (visualization.shader.IsEmpty())
			(DX11::BaseInstructionBlock&)visualization = pathShader;
		visualization.vbo.CreateFromStructs(geo.GetLOD(0).vertices);
		visualization.ibo.Create(geo.GetLOD(0).indices);
	}
	#endif

	


}


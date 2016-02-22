#include <global_root.h>
#include "mobileEntity.h"
#include "faction.h"
#include "gridAndChoices.h"
#include "rendering.h"
#include "vehicle.h"
#include "satellite.h"
#include "systems.h"
#include "prototypes.h"
#include "../runtimeConfig.h"

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


	#ifdef WIRELESS_VEHICLES


		#ifndef ONE_COMPOUND_PER_FACTION
			bool				MobileResourceEnvironment::ContainsCompound(UINT32 compoundID)	const
			{
				foreach (this->sourceCompounds,c)
					if (c->first == compoundID)
						return true;
				return false;
			}

			void				MobileResourceEnvironment::AddCompound(const PStructure&ref)
			{
				if (!ContainsCompound(ref->compoundID))	//we assume the number of compounds is very low here (usually 0..2), so this should be okay in regards to performance
					sourceCompounds << std::make_pair(ref->compoundID,ref);
			}

			void	MobileResourceEnvironment::GetOwnCompoundsIn(const OwnerParameter&p, const Grid::LowSetup::TVolumeDescriptor&desc)
			{
				foreach (desc.coverage,c)
				{
					for (UINT l = c->layerBegin; l < c->layerEnd; l++)
					{
						PStructure s = GetStructure(Grid::LCellIdU(c->stackID,l));
						if (!s || s->owner != p)
							continue;
						if (s->compoundID == Compound::InvalidNetwork)
							continue;
						AddCompound(s);
					}
				}
			}

		#endif

		MobileResourceEnvironment::TCacheID::TCacheID(const MobileEntity&m)
		{
			round = RuntimeConfig::roundNumber;
			stackID = (UINT16)m.location.stackID;
			layerID = (UINT8)Grid::CellStack::ToLayer(m.location.height);
			phase = (UINT8)RuntimeConfig::phase;
			#ifndef ONE_COMPOUND_PER_FACTION
				numCompounds = (UINT16)compounds.Count();
			#else
				numCompounds = 1;
			#endif
		}

		MobileResourceEnvironment::TCacheID::TCacheID(const MobileEntity&m, const Grid::LCellIdI&loc)
		{
			round = RuntimeConfig::roundNumber;
			stackID = (UINT16)loc.stackID;
			layerID = (UINT8)Grid::CellStack::ToLayer(loc.height);
			phase = (UINT8)RuntimeConfig::phase;
			#ifndef ONE_COMPOUND_PER_FACTION
				numCompounds = (UINT16)compounds.Count();
			#else
				numCompounds = 1;
			#endif
		}

		#ifndef CLIENT

			/*static*/ void MobileResourceEnvironment::ChangeCompoundStorage(Compound&compound, MobileEntity*originatingAt, float atTime, Resource res, int&change)
			{
				ResourceVector demand;

				WithdrawOverproduction(compound,originatingAt,res,change);
				if (change == 0)
					return;
				demand.level[res] = change;
				compound.ChangeStorage(demand,atTime);
				change = demand.level[res];
			}




			/*static*/ void	MobileResourceEnvironment::ForceChangeCompoundStorage(Compound&compound, MobileEntity*originatingAt, float atTime, Resource res, int change)
			{
				int original = change;
				ChangeCompoundStorage(compound,originatingAt,atTime,res,change);
				if (change != 0)
					FATAL__(originatingAt->GetName()+": Unable to change compound storage by "+String(original)+" unit(s) of "+res.ToString()+". "+String(change)+" unit(s) remaining");
			}

		#endif

	
		void	MobileResourceEnvironment::CheckRefresh(const MobileEntity&m, const Grid::LCellIdI&loc)
		{
			#ifndef ONE_COMPOUND_PER_FACTION

				TCacheID	check(m,loc);
				if (cacheID == check)
					return;
				cacheID = check;
				#ifdef WIRELESS_VEHICLES
					sourceCompounds.Clear();
					if (m.IsVehicle())
					{
						#ifdef CLIENT
							for (index_t i = 0; i < skyLinks.Count(); i++)
							{
								TLinkReference&ref = skyLinks[i];
								PSkyLinkStructure link = ref.first.lock();
								PStructure st = ref.second.lock();
								if (!link || !st)
								{
									LogEvent(EventClass::Preset::internalError,&m, "Link reference lost");
									skyLinks.Erase(i);
									i--;
									continue;
								}
								if (st->owner == m.owner && !link->connectedToHub.expired())
									AddCompound(st);
							}
						#else
							foreach (m.owner->skyHubs,h)
							{
								PSkyHub hub = h->lock();
								if (hub)
								{
									PSkyLinkStructure link = hub->connectedTo.lock();
									if (link)
									{
										AddCompound(To<Structure>::Cast( link->CastToStructure()->shared_from_this() ) );
									}
								}
							}
						#endif
					}
				#endif
			#endif

		}


		void	MobileResourceEnvironment::CheckRefresh(const MobileEntity&m)
		{
			return CheckRefresh(m,m.location);
		}
	#endif

	bool		MobileResourceEnvironment::CanWithdraw(const MobileEntity&owner, Resource res, int demand)	const
	{
		if (demand == 0)
			return true;
		return GetStorage(owner,res) >= demand;
	}
		
	#ifndef CLIENT
		/**
		Attempts to withdraw the specified amount of the specified resource from the specified compound's overproduction this round
		@param compound Compound to attempt to withdraw overproduction from
		@param originatingAt For reference only: mobile entity that is attempting the withdrawal
		@param res Resource to withdraw
		@param[inout] change Change value. Must be negative for withdrawal. Positive or zero values are ignored. The resulting value in change represents how much was not withdrawn.
		If @a change is 0 after the call has finished, then all was withdrawn
		*/
		/*static*/ void	MobileResourceEnvironment::WithdrawOverproduction(Compound&compound, MobileEntity*originatingAt, Resource res, int&change)
		{
			if (change >= 0)
				return;
			ResourceVector overproduction = compound.GetOverproduction();
			if (overproduction.level[res] > 0)
			{
				int production = overproduction.level[res];
				int overproductionTransfer = std::min(production,-change);
				LogEvent(EventSignificance::Common,originatingAt,String(res.ToString())+ " overproduction ("+String(production)+") redirected for demand ("+String(change)+"): "+String(overproductionTransfer)+" transferred");
				change += overproductionTransfer;
				compound.ForceWithdrawOverproduction(res,overproductionTransfer);
//				compound.overproduction.level[res]-= overproductionTransfer;
			}
		}

		void	MobileResourceEnvironment::ChangeStorage(MobileEntity&owner, Resource res, int&change)
		{
			if (change == 0)
				return;
			#ifndef ONE_COMPOUND_PER_FACTION
				#ifdef WIRELESS_VEHICLES
					foreach (sourceCompounds,c)
					{
						Compound&compound = Objects::compounds[c->first];
						WithdrawOverproduction(compound,&owner,res,change);
						if (change == 0)
							return;
					}
					ResourceVector demand;
					foreach (sourceCompounds,c)
					{
						Compound&compound = Objects::compounds[c->first];
						demand.level[res] = change;
						compound.ChangeStorage(demand,owner.motionTime);
						change = demand.level[res];
						if (change == 0)
							return;
					}
					Objects::ChangeStorage(owner,owner.motionTime,owner.GetMaxStorageCapacity(),owner.currentStorage,res,change);
					if (!change)
						return;
				#else
					Objects::ChangeStorage(owner,owner.timeAt,owner.GetMaxStorageCapacity(),owner.currentStorage,res,change);
				#endif
			#else
				ChangeCompoundStorage(owner.owner->compound, &owner, owner.timeAt,res,change);
			#endif
		}

		void	MobileResourceEnvironment::ForceChangeStorage(MobileEntity&owner, Resource res, int change)
		{
			int original = change;
			ChangeStorage(owner,res,change);
			if (change != 0)
				FATAL__(owner.GetName()+": Unable to change surrounding storage by "+String(original)+" unit(s) of "+res.ToString()+". "+String(change)+" unit(s) remaining");
		}
	#endif

	int			MobileResourceEnvironment::GetStorage(const MobileEntity&owner, Resource res)	const
	{
		#ifndef ONE_COMPOUND_PER_FACTION
			#ifdef WIRELESS_VEHICLES
				int rs = owner.currentStorage.level[res];
				foreach (sourceCompounds,c)
					rs += Objects::compounds[c->first].total.storage.level[res] 
						#ifndef CLIENT
							+ Objects::compounds[c->first].GetOverproduction(res)
						#endif
						;
				return rs;
			#else
				return owner.currentStorage.level[res];
			#endif
		#else
			const Compound&compound = owner.owner->compound;
			return compound.total.storage.level[res] 
				#ifndef CLIENT
					+ compound.GetOverproduction(res)
				#endif
				;
		#endif
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

		#ifdef HAS_WINDOW
			ambientSound.SetPosition(system.w.xyz);
			ambientSound.SetVelocity(float3());
			activitySound.SetPosition(system.w.xyz);
			activitySound.SetVelocity(float3());
		#endif


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
										return true;
									}
									LogEvent(EventSignificance::Common,this,"Revised path due to terrain changes.");
								}
								else
								{
									LogEvent(isAutomated ? EventClass::Preset::common : EventClass::Preset::operationAborted,this,"Route to destination lost. Motion aborted.");
									this->UnlockTasks();
									ClearAllTasks(true, CLOCATION);
									return true;
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
										if (task.path.last().cell == task.foci.Peek().target)
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

										RegisterChange(ActionChanged,motionTime);
										task.path.EraseLast();
										cost = task.path.IsNotEmpty() ? task.path.last().movementCostsFromPrevNode : 1;
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
										LogEvent(EventClass::Preset::operationAborted,this,"Falling event interrupted motion.");
										stop = true;
									}
									break;
									case MobilePlacementResult::PlacementFailedHarmfulOrBlocked:
										this->ClearAllTasks(true, CLOCATION);
									case MobilePlacementResult::PlacementFailedPermanentlyObstructed:
										dropRemaining = true;
										//bleed into next case:
									case MobilePlacementResult::PlacementFailedTemporarilyObstructed:
									{
										LogEvent(EventClass::Preset::operationAborted,this,"Unable to complete movement.");
										if (health <= 0)
										{
											LogEvent(EventSignificance::Common,this,"Is dead.");
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
										//	RegisterChange(TasksChanged,motionTime);
										//}

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
										return true;
									}
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
									myTasks.Peek().Replot(location,this,FocusChangeType::SituationChanged,true);
								#ifndef CLIENT
									RegisterChange(TasksChanged,motionTime);
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
										RegisterChange(ActionChanged,motionTime);
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
			LogEvent(EventSignificance::Common,this,"Completing on-move job '"+completeOnMove->GetName()+ "' at t="+String(motionTime));
			VehicleJob::BaseJob::continue_t rs = completeOnMove->Finish(*this,referenceLocation);
			completeOnMove.reset();
			unstable = false;
			if (rs == VehicleJob::BaseJob::TerminalFailureVehicleDestroyed)
			{
				LogEvent(EventSignificance::Common,this,"Terminal failure. Remaining tasks dropped. Vehicle destroyed.");
				jobStarted = false;
				ClearAllTasks(true, CLOCATION);
				unstable = false;
				Remove(shared_from_this(),RemovalType::UncontrolledExplosion,motionTime,false,false);
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
	


		void MobileEntity::HandleMotion(MobilePlacementResult&result, bool signalEvent, const Grid::LCellIdI&oldLocation, float timeCost)
		{
//			MobilePlacementResult result;
			this->UpdateSystem();
			if (signalEvent)
			{

				//static GenericHashSet<OwnerParameter,OwnerHash>	checkStillBeeingSeenBy;
				//static Buffer0<PWorldEntity> checkCanStillSee;
				//checkCanStillSee.Clear();
				//checkStillBeeingSeenBy.Clear();

				float oldTime = motionTime;
				motionTime += timeCost;

				static Buffer0<PWorldEntity> newContacts;
				newContacts.Clear();
				this->RegisterSensory(location,motionTime,false,&newContacts);
				this->UnregisterSensory(oldLocation,motionTime,true);
				foreach (newContacts,c)
				{
					float zOffset;
					PTurretBase b = (*c)->GetTurretBase(zOffset);
					if (!b)
						continue;
					if (b->turret->CanAttack())
					{
						LogEvent(EventClass::Preset::common,this,"Processing new hostile contact "+(*c)->GetName()+"...");
						for (WMobileEntity w : owner->allMobiles)
						{
							PMobileEntity v2 = w.lock();
							if (!v2)
								continue;

							for (index_t i = 0; i < v2->myTasks.Count(); i++)
							{
								const Task&task = v2->myTasks[i];
								bool badTask = false;
								if (task.type == Task::MoveTo)
								{
									for (const auto&node : task.path)
									{
										//TVec3<> world = Grid::ToWorld(node.cell);
										//float zOffset;
										//const auto&base = m->GetTurretBase(zOffset);
										Grid::LCellIdI actual;
										if (b->ExtCanHit((*c)->owner,(*c)->location,zOffset,node.cell, actual))
										{
											badTask = true;
											break;
										}

									}
								}
								if (badTask)
								{
									LogEvent(EventSignificance::Common,v2.get(),"Dangerous movement into range of "+(*c)->GetName()+" at "+(*c)->location.ToString()+" detected.");
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
											motionTime += timeEpsilon;
											v2->RegisterChange(TasksChanged,motionTime);
										}
									}
									break;
								}

							}
						}
					}
				};


		

				if (timeCost > 0)
				{
					SignalMotion(To<Vehicle>::Cast(shared_from_this()),oldLocation,location,oldTime,timeCost);
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
					f->CheckGridVisibility(_this,motionTime);
				});

				bool doBreak = false;
				ForeachFaction([this,&doBreak,_this](const PFaction&f)
				{
					if (doBreak)
						return;
					if (!f->CanSee(_this,motionTime))
						return;
					Grid::LCellIdI loc;
					if (IsSatellite())
					{
						foreach (f->satellites, _s)
						{
							PSatellite s = _s->lock();
							if (s && s->WouldAttack(_this,loc,motionTime))
							{
								if (s->AttackNext(loc,true))
								{
									LogEvent(EventSignificance::Debug,s.get(),"Auto-targeting moving "+GetName()+" (located at "+location.ToString()+") via closest viable attack target "+loc.ToString()+" at t="+String(motionTime));
									LogEvent(EventSignificance::Important,s.get(),"Auto-targeting moving "+GetNiceName());

									s->Attack(motionTime);
									if (health <= 0)
									{
										doBreak = true;
										return;
									}
										
								}
							}
						}

					}
					else
					{
						foreach (f->towers, _t)
						{
							PTower t = _t->lock();
							if (t && t->WouldAttack(_this,loc,motionTime))
							{
								if (t->AttackNext(loc,true))
								{
									LogEvent(EventSignificance::Debug,t.get(),"Auto-targeting moving "+GetName()+" (located at "+location.ToString()+") via closest viable attack target "+loc.ToString()+" at t="+String(motionTime));
									LogEvent(EventSignificance::Important,t.get(),"Auto-targeting moving "+GetNiceName());

									t->Attack(motionTime);
									LogEvent(EventSignificance::Debug,t.get(),"t is now "+String(motionTime));
									if (health <= 0)
									{
										doBreak = true;
										return;
									}
										
								}
							}
						}
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
				motionTime += timeCost;
				#ifndef CLIENT
					LogEvent(EventSignificance::Common,this,"Silent motion completed");
				#endif
			}

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
							index_t target = entityRandom.getIndex(numWays-1);
							if (PlotCourse(ways[target],true,false))
							{
								hasTriedPlottingOnMoveAutoMotion = true;
								LogEvent(EventSignificance::Common,this,"Automatically moving to finish '"+completeOnMove->GetName()+"'");
								return false;	//retry
							}
						}
						//iterative, in case random missed some option
						for (index_t i = 0; i < numWays; i++)
							if (PlotCourse(ways[i],true, false))
							{
								hasTriedPlottingOnMoveAutoMotion = true;
								LogEvent(EventSignificance::Common,this,"Automatically moving to finish '"+completeOnMove->GetName()+"'");
								return false;	//retry
							}
					}
					LogEvent(EventClass::Preset::operationAborted,this,"Unable to automatically relocate. Move manually to finish '"+completeOnMove->GetName()+"'");
				}
				return true;
			}
			bool rs = false;
			//LockTasks();

			//Task&task = myTasks.Peek();
			RegisterChange(TasksChanged,motionTime);

			if (myTasks.Peek().job)
			{
				if (this->empDamageLevel)
				{
					LogEvent(EventClass::Preset::operationAborted,this,"EMP damage: Unable to continue job at this time.");
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
								LogEvent(EventSignificance::Common,this,"Delayed Job");
							break;
							case VehicleJob::BaseJob::Started:
								LogEvent(EventSignificance::Common,this,"Started '"+myTasks.Peek().job->GetName()+"'");
								cont = false;
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
								RegisterChange(TasksChanged,motionTime);
							}
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
						LogEvent(EventSignificance::Common,this,"Executing normal task");
						return HandleNormalTask();
					}
				}
				if (!hasConstructedThisRound && HasTasks() && myTasks.Peek().job)
				{
					ASSERT__(jobStarted);	//supposedly it can happen that this is false
					PVehicleJob job = myTasks.Peek().job;
					if (IsSleeping())
					{
						LogEvent(EventSignificance::Common,this,"Job delayed: '"+job->GetName()+"'. Entity is sleeping");
					}
					else
					{
						hasConstructedThisRound = true;
						constructionRoundCounter ++;
						switch (job->Continue(*this,myTasks.Peek().GetFocus(location),!hasMoved))
						{
							case VehicleJob::BaseJob::ContinueJob:
							break;
							case VehicleJob::BaseJob::JobDelayed:
								constructionRoundCounter --;
							break;
							case VehicleJob::BaseJob::JobDone:
								LogEvent(EventSignificance::Common,this,"Job ended: '"+job->GetName()+"'");

								EraseFirstTask();
								RegisterChange(TasksChanged,motionTime);
								while (HasTasks() && myTasks.Peek().job && myTasks.Peek().job->IsImplicitlyDone(*this,myTasks.Peek().GetFocus(location)))
								{
									LogEvent(EventSignificance::Common,this,"Job has implicitly ended: '"+myTasks.Peek().job->GetName()+"'");

									EraseFirstTask();
								}
								if (HasTasks() && myTasks.Peek().job)
								{
									LogEvent(EventSignificance::Common,this,"Next front job: '"+myTasks.Peek().job->GetName()+"'");
								}
								jobStarted = false;

							return false;
							case VehicleJob::BaseJob::TerminalFailureVehicleDestroyed:
								LogEvent(EventSignificance::Common,this,"Terminal failure. Remaining tasks dropped. Vehicle destroyed.");
								jobStarted = false;
								ClearAllTasks(true, CLOCATION);
								unstable = false;

								Remove(shared_from_this(),RemovalType::UncontrolledExplosion,motionTime,false,false);
							return false;
							case VehicleJob::BaseJob::TerminalFailure:
								LogEvent(EventSignificance::Common,this,"Terminal failure. Remaining tasks dropped.");
								jobStarted = false;
								ClearAllTasks(true, CLOCATION);
								unstable = false;
							return false;
						}
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
		
			RegisterChange(TasksChanged,motionTime);

			if (myTasks.IsEmpty())
			{
				constructionRoundCounter = 0;
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

			RegisterChange(TasksChanged,motionTime);
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
			myTasks.EraseFront();
		}

		bool			MobileEntity::PlotCourse(ArrayRef<Grid::LCellIdI> destination, bool detailedPath, bool virtualPlatformMode/*=false*/)
		{
			const Grid::WorldData&world = owner->currentState;
			if (IsVehicle())
			{
				for (index_t i = 0; i < destination.length(); i++)
				{
					Grid::LCellIdI&dest = destination[i];

					while (!world.LowCellIsClear(dest) && !MobileDestinationStructureHere(dest,*this))
						dest++;
					while (world.LowCellIsClear(dest-1) || MobileDestinationStructureHere(dest-1,*this))
						dest--;
					if (!virtualPlatformMode && !((Vehicle*)this)->CanStandOn(world,dest -1,false,false))
						return false;
				}
			}

			Grid::LCellIdI pointOfOrigin = GetLastPointOfOrigin();

			while (destination.IsNotEmpty() && destination.First() == pointOfOrigin)
			{
				destination = ArrayRef<Grid::LCellIdI>(destination.GetPointer() +1 ,destination.GetLength()-1);
			}
			if (destination.IsEmpty())
			{
				return false;
			}

			if (!HasTasks() || myTasks.GetNewest().type != Task::MoveTo)
			{
				if (myTasks.Count() >= Objects::MaxProductionTasks)
				{
					LogEvent(EventClass::Preset::commandRejected,this,"Max queue length ("+String(Objects::MaxProductionTasks)+") reached. Task rejected");
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
						destination = ArrayRef<Grid::LCellIdI>(destination.GetPointer() +1 ,destination.GetLength()-1);
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
						if (myTasks.GetNewest().foci.Count() >= MaxFocusCount)
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

		bool	MobileEntity::PlotFrontCourse(ArrayRef<Grid::LCellIdI> destination, bool detailedPath)
		{
			const Grid::WorldData&world = owner->currentState;

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
				destination = ArrayRef<Grid::LCellIdI>(destination.GetPointer() +1 ,destination.GetLength()-1);
			}
			if (destination.IsEmpty())
			{
				return false;
			}

			{
				ASSERT__(!TasksAreLocked());

				myTasks.PushFront(Task("Move To",destination,detailedPath,Task::MoveTo));
				myTasks.GetOldest().Replot(location, this, FocusChangeType::FullChange,true);
				RegisterChange(TasksChanged,motionTime);
			}

			return true;
		}
		

	
	
		bool		MobileEntity::DropLastTask()
		{
			if (!HasTasks())
				return false;
			ASSERT__(!TasksAreLocked());
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
			RegisterChange(TasksChanged,motionTime);
			return true;
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

			LogEvent(EventSignificance::Debug,this,"Setting local time ("+String(motionTime)+") to attack time="+String(attackTime));
			SyncTime(attackTime,timeEpsilon);

			Task&task = LockedPeekTask();

			const Grid::LCellIdI target = task.foci.Pop().target;
			float3 dir = Grid::low.cells[target.stackID].centerVertex.xyz * Grid::ToRadius(target.height) - this->GetWorldPosition();
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
				RegisterChange(ActionChanged,motionTime);

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
						motionTime += rotTime;

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


				SignalAttack(To<Vehicle>::Cast(shared_from_this()),target,flightTime,false);

				resourceEnvironment.CheckRefresh(*this);
				resourceEnvironment.ForceChangeStorage(*this, turret->ammoType,-turret->ammoNeedPerShot);
				
				motionTime += flightTime;

				if (turret->dealsEMPDamage)
					ExplodeEMP(To<Vehicle>::Cast(shared_from_this()),target,flightTime,
								t1, t0, turret->explosionRadius, turret->damageLevel,motionTime);
				else
					Explode(To<Vehicle>::Cast(shared_from_this()),target,flightTime,
							t1, t0, turret->explosionRadius, turret->damageLevel,turret->isDoomsday, motionTime);
			}

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
			RegisterChange(TasksChanged,motionTime);
			//stateChangedAt = attackTime;	//problematic
			attackTime = motionTime;
		}




	#else
		void MobileEntity::Relocate(const Grid::LCellIdI&toLocation, bool updateSystem)
		{
			if (toLocation == location)
				return;
			LogEvent(EventSignificance::Common,this,"Relocating to "+toLocation.ToString()+" ("+String(updateSystem)+")");
			static Grid::HighSetup::TVolumeDescriptor desc;
			float t = 0.f;
			if (isMine)
			{
				UnregisterSensory(t);
			}
			if (IsVehicle())
			{
				Objects::RemoveVehicleFromGrid(To<Objects::Vehicle>::Cast(shared_from_this()),location);
			}
			preAnimationLocation = location = toLocation;
			if (IsVehicle())
			{
				Objects::InsertVehicleIntoGrid(To<Objects::Vehicle>::Cast(shared_from_this()),location);
			}
			if (isMine)
			{
				RegisterSensory(t);
			}
			#ifdef THOROUGLY_DEBUG_SENSORY
				if (isMine)
					VerifySensory();
			#endif

			if (updateSystem)
				UpdateSystem();
		}

	#endif


	bool	MobileEntity::Task::AppendPathSegment(const MobileEntity*v, const Grid::LCellIdI&prev, const Task::Focus&target)
	{
		if (v->IsVehicle())
		{
			//would be faster, but too complicated to extract cost
			//if (target.isExplicit)
			//{
			//	Grid::LCellIdI	possibilities[6 * 3];
			//	count_t numWays = Objects::GetWays(v->owner->currentState,(const Objects::Vehicle*)v,prev,possibilities,target.target,true,true);
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
				if (!AppendShortestPathI((const Vehicle*)v,prev,target.target,path))
					return false;
			}
		}
		else
		{
			if (v->IsSatellite())
			{
				//would be faster, but too complicated to extract cost
				//if (target.isExplicit)
				//{
				//	UINT32	possibilities[6 * 3];
				//	count_t numWays = Objects::GetWays((const Objects::Satellite*)v,prev.stackID,possibilities,target.target.stackID,true,true);
				//	bool found = false;
				//	for (index_t i = 0; i < numWays; i++)
				//		if (possibilities[i] == target.target.stackID)
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
			}
			else
				FATAL__("Routing not implemented for "+v->GetTypeName());
		}

		return true;
	}

	bool MobileEntity::Task::UpdatePath(const MobileEntity*v, const Grid::LCellIdI&pointOfOrigin, FocusChangeType type)
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
								ArrayRef<PathNode> ref = path.ToRef();
								ref = ArrayRef<PathNode>(ref.GetPointer() + offset,ref.Count()-offset);
								Array<PathNode>	nodes = ref;
								path.Clear();
								path << PathNode(v->location,0);
								if (!AppendPathSegment(v,v->location,nextFocus))
								{
									newPath = true;
								}
								else
								{
									path.Append(nodes);
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
						while (path.IsNotEmpty() && path.last().cell != lookFor)
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
					const Grid::LCellIdI&prev = path.last().cell;
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
				if (!AppendShortestDirectPath(pointOfOrigin,foci.Peek().target,path,false))
				{
					return false;	//this would be weird, but happens
				}
			}
			else
			{
				if (v->IsSatellite())
				{
					if (!AppendShortestDirectSatellitePath(pointOfOrigin,foci.Peek().target.stackID,path))
						return false;
				}
				else
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
			LogEvent(EventSignificance::Common,v,strPlottedCourse.ToStringRef());
		#endif
		return true;
	}
	

	
	bool					MobileEntity::Task::Replot(const Grid::LCellIdI&pointOfOrigin, const MobileEntity*parent, FocusChangeType t, bool recompileVisualization)
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
						#ifdef HAS_WINDOW
							if (recompileVisualization)
								RecompilePath(1);
						#endif
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
					#ifdef HAS_WINDOW
						if (recompileVisualization)
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
							if (recompileVisualization)
							{
								static Buffer0<TVec4<> > trajectory;
								Geometry geo;
									localizableShapes.Clear();
								foreach (foci,f)
								{
									trajectory.Clear();
									TVec3<> dummy0;
									Grid::HCellIdI dummy1;
									if (parent->TraceTrajectoryFrom(Grid::actualWorld, pointOfOrigin,f->target,trajectory,dummy0,dummy1))
									{
										Component::Turret::CompileTrajectory(trajectory,Component::Turret::Schematic,geo,0.25f);

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
							}
						#endif
					}
					break;
				}
			break;
		}
		return true;
	}


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
				return true;
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

	

	int MobileEntity::GetCurrentMineralConsumption() const
	{
		if (myTasks.IsNotEmpty() && myTasks.Peek().job)
			return std::min(myTasks.Peek().job->mineralCostLeft,GetOperationSpeed(myTasks.Peek().job->GetOperationAffinity())) ;
		return 0;
	}


}


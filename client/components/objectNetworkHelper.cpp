#include "objectNetworkHelper.h"

#include <protocol.h>
#include <objects/structure/mine.h>
#include <objects/structure/laboratory.h>
#include <objects/pathfinding.h>
#include "visual/objectChoices.h"
#include "visual/commandDefinitions.h"
#include "visual/hud/objectCommands.h"
#include "visual/objectSelection.h"
#include "visual/hud/dialog.h"
#include "clientRuntimeConfig.h"
#include "network.h"
#include "visual/tutorial.h"

namespace ObjectHelper
{

	bool IsTerraformingOperation(ConstName jobName)
	{
		switch (jobName)
		{
			case ConstName::Job_AlterTerrain:
			case ConstName::Job_BuildHorizontalWall:
			case ConstName::Job_BuildRoad:
			case ConstName::Job_BuildWallUp:
			case ConstName::Job_ClearAbove:
			case ConstName::Job_DigDown:
			case ConstName::Job_DigHorizontal:
			case ConstName::Job_Sequence:
				return true;

		}
		return false;
	}


	void	SignalNewJob(const Objects::PMobileEntity&v, ConstName jobName, Objects::EntityType parameterName, const Grid::LCellIdI&overrideLocation)
	{
		const Grid::LCellIdI targetLocation = overrideLocation != Grid::LCellIdI::Invalid ? overrideLocation : v->GetLastPointOfOrigin();
		#ifdef HAS_WINDOW
			if (RuntimeConfig::tutorialMode && IsTerraformingOperation(jobName))
			{
				HUD::Dialog::ShowMessage("Terraforming is not permitted in this tutorial. You may do so freely on a regular server.");
				return;
			}


			if (jobName == ConstName::Job_Construct && parameterName != ConstName::Type_Connector)
			{
				Objects::NaturalResourceVector	res;
				if (res.SampleWorld(targetLocation.stackID))
				{
					const auto& proto = Objects::Prototypes::structures.Find(parameterName);
					ASSERT1__(proto,ToString(parameterName));
					String name = proto->GetTypeName();
					TUniqueID id = v->id;
					if (parameterName != ConstName::Type_Mine && res.IsNotEmpty())
					{
						HUD::Dialog::Confirm(v->GetNiceName()+" will construct a "+name+" on a location with known natural resources, which could be harvested by a Mine. Do you wish to continue building the "+name+"?",
							[id,jobName,parameterName,targetLocation]()
							{
								Protocol::StandardJob	job;
								job.ownerID = id;
								job.jobName = jobName;
								job.parameterName = parameterName;	
								Network::Channels::signalNewJob.SendTo(Network::client,job);
							},
							[id,jobName,parameterName]()->HUD::Dialog::CheckResult
							{
								if (!Objects::GetEntity(id) || !Network::client.IsConnected())
									return HUD::Dialog::CheckResult::Abort;
								if (RuntimeConfig::phase != RuntimeConfig::Phase::CommandPhase)
									return HUD::Dialog::CheckResult::OkayNotRightNow;
								return HUD::Dialog::CheckResult::OkayContinue;
							}
						);
						return;
					}
					if (parameterName == ConstName::Type_Mine && res.IsEmpty())
					{
						HUD::Dialog::Confirm(v->GetNiceName()+" will construct a Mine on a location that has no natural resources. Building a mine here seems like a waste of resources. Do you wish to continue building the Mine?",
							[id,jobName,parameterName,targetLocation]()
							{
								Protocol::StandardJob	job;
								job.ownerID = id;
								job.jobName = jobName;
								job.parameterName = parameterName;
								Network::Channels::signalNewJob.SendTo(Network::client,job);
							},
							[id,jobName,parameterName]()->HUD::Dialog::CheckResult
							{
								if (!Objects::GetEntity(id) || !Network::client.IsConnected())
									return HUD::Dialog::CheckResult::Abort;
								if (RuntimeConfig::phase != RuntimeConfig::Phase::CommandPhase)
									return HUD::Dialog::CheckResult::OkayNotRightNow;
								return HUD::Dialog::CheckResult::OkayContinue;
							}
						);
						return;
					}
				}

			}
		#endif
		Protocol::StandardJob	job;
		job.ownerID = v->id;
		job.jobName = jobName;
		job.parameterName = parameterName;	
		Network::Channels::signalNewJob.SendTo(Network::client,job);

	}


	void							SignalMarkerDeletion(TUniqueID markerID)	//must be copy to prevent weird shit
	{
		Network::Channels::destroyMarker.SendTo(Network::client, markerID);
		Objects::Marker::Destroy(markerID,Network::ControlEnabled());
	}

	void	SignalMarkerCreation(Protocol::Marker&marker)
	{
		marker.createdInRound = RuntimeConfig::roundNumber;
		Network::Channels::createMarker.SendTo(Network::client, marker);
		Network::Receiver::CreateMarker(marker);	//one-way
	}



	void	SignalMarkerTouch(Objects::Marker&marker)
	{
		marker.createdInRound = RuntimeConfig::roundNumber;
		marker.UpdateAge(RuntimeConfig::roundNumber);
		Protocol::TouchedMarker m;
		m.id = marker.id;
		m.newRoundNumber = RuntimeConfig::roundNumber;
		Network::Channels::touchMarker.SendTo(Network::client,m);
	}



	bool	DirectTo( const Objects::PWorldEntity&v, const ArrayRef<Grid::LCellIdI>&targets, UINT8 flags, Objects::Vehicle::mode_t mode, bool interactive )
	{
		if (!v)
			return false;
		DBG_ASSERT__(!RuntimeConfig::aiMode || !Network::newRoundRequested);
		const bool moveTo = mode == Objects::CursorMode::Idle && !(flags & Protocol::DirectFlags::LeftClick);

		if (v->IsMobile() && moveTo)
		{
			Objects::PMobileEntity me = To<Objects::MobileEntity>::Cast(v);

			#ifdef HAS_WINDOW
			#ifndef NO_SATELLITES
				if (interactive && me->IsSatellite() && !me->HasMotionTasks())
				{
					Objects::PSatellite sat = To<Objects::Satellite>::Cast(me);
					if (sat->IsSkyHub())
					{
						Objects::PSkyHub hub = To<Objects::SkyHub>::Cast(me);
						if (!hub->connectedTo.expired())
						{
							TUniqueID vid = v->id;
							Array<Grid::LCellIdI> pathCopy = targets;
							Grid::LCellIdI sourceLoc = me->GetLastPointOfOrigin();
							Grid::LCellIdI originLoc = me->location;
							HUD::Dialog::Confirm(me->GetNiceName()+" is currently tethered to "+hub->connectedTo.lock()->CastToStructure()->GetNiceName()+". The link keeps it afloat for free, and supplies fuel to distribute to other nearby satellites."
											" Moving the satellite will sever this link, and require it to be supplied by another hub satellite. Please confirm.",[vid,pathCopy,detailedPath,leftClick,mode]()
											{
												DirectTo(Objects::GetEntity(vid),pathCopy,detailedPath,leftClick,mode,false);
											},[vid,sourceLoc,originLoc]()->HUD::Dialog::CheckResult
											{
												Objects::PWorldEntity e = Objects::GetEntity(vid);
												if (!e || !e->IsMobile())
													return HUD::Dialog::CheckResult::Abort;
												Objects::PMobileEntity me = To<Objects::MobileEntity>::Cast(e);
												if (e->location != originLoc || me->GetLastPointOfOrigin() != sourceLoc)
													return HUD::Dialog::CheckResult::Abort;
												if (!RuntimeConfig::IsInAnyCommandPhase() || Objects::Faction::mine->requestedNewRound)
													return HUD::Dialog::CheckResult::OkayNotRightNow;
												return HUD::Dialog::CheckResult::OkayContinue;
											});
							return false;
						}
					}
				}
			#endif
			#endif

			if (!(flags & Protocol::DirectFlags::Detailed))
			{
				#ifdef HAS_WINDOW
					Grid::LCellIdI start = Objects::Commands::GetPointOfOrigin(*me);
				#else
					Grid::LCellIdI start = me->GetLastPointOfOrigin();
				#endif
				foreach (targets,t)
				{
					if (!MoveVehicleTo(me,start,*t,flags))
						return false;
					start = *t;
				}
			}
			else
				SendDirectTo(v,targets,flags,Objects::CursorMode::Idle);
		}
		else
		{
			SendDirectTo(v,targets,flags,mode);
		}

		#ifdef HAS_WINDOW
			Objects::DisableModes(Network::ControlEnabled());
			if (moveTo)
				Tutorial::ShowMessage("Move To");
		#endif
		return true;
	}


	UINT8	AssembleMotionFlags(bool detailedPath, bool leftClick, bool clearSafeTasks)
	{
		UINT8 flags = 0;
		if (detailedPath)
			flags |= Protocol::DirectFlags::Detailed;
		if (leftClick)
			flags |= Protocol::DirectFlags::LeftClick;
		if (clearSafeTasks)
			flags |= Protocol::DirectFlags::ClearSafeTasks;
		return flags;
	}

	
	UINT8	AssembleMotionFlags(bool detailedPath, bool leftClick)
	{
		UINT8 flags = 0;
		if (detailedPath)
			flags |= Protocol::DirectFlags::Detailed;
		if (leftClick)
			flags |= Protocol::DirectFlags::LeftClick;
		#ifdef HAS_WINDOW
			if (!Objects::ShouldQueueCommands())
				flags |= Protocol::DirectFlags::ClearSafeTasks;
		#endif
		return flags;
	}

	bool	DirectTo(const Objects::PWorldEntity&v, const ArrayRef<Grid::LCellIdI>&targets, UINT8 flags, Objects::Vehicle::mode_t mode)
	{
		return DirectTo(v, targets, flags, mode, true);
	}


	void SendDirectTo(const Objects::PWorldEntity&v, const ArrayRef<Grid::LCellIdI>&targets, UINT8 flags, Objects::Vehicle::mode_t mode)
	{
		Protocol::EntityDirectCommand	cmd;
		cmd.id = v->id;
		cmd.mode = (UINT16)mode;
		cmd.path = targets;
		cmd.directFlags = flags;
		Network::Channels::entityDirectCommand.SendTo(Network::client,cmd);
	}


	bool	MoveVehicleTo( const Objects::PMobileEntity&v, const Grid::LCellIdI&prev, Grid::LCellIdI target, UINT8 flags )
	{
		if (prev == target)
			return true;
		Buffer0<Objects::PathNode>		path;
		{
			bool tooLong;
			bool foundPath;

			if (v->IsPlane())
				foundPath = Objects::Pathfinding::SubOrbital::AppendShortestPathI((const Objects::SubOrbital::Plane*)v.get(),prev,target,path,false,&tooLong);
			else if (v->IsGroundBoundVehicle())
				foundPath = Objects::Pathfinding::AppendShortestPathI((const Objects::Vehicle*)v.get(),prev,target,path,false,&tooLong);
			#ifndef NO_SATELLITES
				else if (v->IsSatellite())
					foundPath = Objects::AppendShortestPathI((const Objects::Satellite*)v.get(),prev,target.stackID,path,false,&tooLong);
			#endif
			else
				FATAL__("Routing not implemented for "+v->GetTypeName());
		
			if (!foundPath)
			{
				if (!World::actual.LowCellIsClearIgnoreStructure(target) && target.height+1 <= Grid::CellStack::TopHeight)
				{
					target++;
					return MoveVehicleTo(v,prev,target,flags);
				}
				LogEvent(EventClass::Preset::commandRejected,v.get(),"Unable to find route to destination");
				//SendDirectTo(v,target,false,false,Objects::CursorMode::Idle);	//maybe some error in the client
				return false;
			}
			if (!tooLong)
				SendDirectTo(v,target,flags,Objects::CursorMode::Idle);	//all good
			else
			{
				LogEvent(EventSignificance::Common,v.get(),"Splitting long path into parts");
				static Buffer0<Grid::LCellIdI>	waypoints;
				waypoints.Clear();
				bool open = true;
				for (index_t i = 0; i < path.GetLength(); i++)
				{
					if (!((i+1)%5))
					{
						waypoints << path[i].cell;
						//SendDirectTo(v,path[i].cell,false,Objects::CursorMode::Idle);
						open = false;
					}
					else
						open = true;
				}

				if (open)
					waypoints << path.Last().cell;
					//SendDirectTo(v,path.Last().cell,false,Objects::CursorMode::Idle);
				SendDirectTo(v,waypoints.ToRef(),flags,Objects::CursorMode::Idle);
			}
		}
		return true;
	}


	

	bool	CanHit(const Objects::PMobileEntity&m, const Grid::LCellIdI&loc)
	{
		return m->CanHit(Objects::TrajectoryEndPoint( loc ));
	//			if (source.ExtCanHit(source.owner, Objects::TrajectoryEndPoint( source.location,source.base->height),Objects::TrajectoryEndPoint( candidate.location,0 ),target))

	}


	bool	IsIdle(const Objects::PMobileEntity&v)
	{
		//okay, clarification:
		//0: if dead: NOT IDLE (duh)
		if (v->IsDeadOrGhost())
			return false;	//yes, this can happen if badly called
		//1: if hibernating: NOT IDLE, EVER
		if (v->IsHibernating())
			return false;
		//2: if automated, then some other logic takes care of everything. NOT IDLE
		if (v->isAutomated)
			return false;

		//3: if it already IS doing something, anything really, then it is NOT IDLE
		if (v->myTasks.IsNotEmpty())	//if it already is doing normal vehicle stuff, then it's not IDLE (duh)
			return false;
		const Objects::MobileEntityProducer*p =  v->CastToProducer();
		if (p && p->queue.IsNotEmpty())	//if it's a factory and has any task, then it's not IDLE
			return false;


		//4: if it's SLEEPING then it's not idle, UNLESS the local entity can attack, and has something to attack:
		if (v->IsSleeping())
		{
			#if 0
			if (v->CanAttack() && v->HasAmmoForShot() && v->CountPossibleAttacks())	//check that its turret can attack, there's ammo for the shot, and it has any shots left this round
			{
				bool emp = v->turret->dealsEMPDamage;
				if (v->IsVehicle())
				{
					bool canAttackAny = false;
					Objects::ForeachWorldEntity([emp,&canAttackAny,v](const Objects::PWorldEntity&other)
					{
						if (!canAttackAny && (!emp || !other->empDamageLevel))
							if (!other->isOwnedByMe && !other->IsIndestructible() && CanHit(v,other->location))
								canAttackAny = true;
					},false);
					if (canAttackAny)
						return true;
				}
				else
					#ifndef NO_SATELLITES
					if (v->IsSatellite())
					{
						for (auto other : Objects::satellites)
							if (!other->isOwnedByMe && !other->IsIndestructible() && CanHit(v,other->location))
								if (!emp || !other->empDamageLevel)
									return true;
					}
					else
					#endif
					{
						FATAL__("Unknown mobile entity type "+v->GetTypeName());
					}
			}
			#endif
			return false;	//cannot attack or has nothing to attack: NOT IDLE since it is SLEEPING
		}

		//not sleeping, no tasks, not producing, not automated, not hibernating:
		return true;	//yes, IDLE
	}


	bool	IsIdle(const Objects::PStructure&s)
	{
		if (s->IsDeadOrGhost())
			return false;
		#ifndef NO_HIBERNATION
			if (s->ToHibernating()->IsHibernating())
				return false;
		#endif

		if (!s->IsWorkingStructure() || s->IsSleeping())
			return false;
		Objects::PWorkingStructure ws = To<Objects::WorkingStructure>::Cast(s);

		
		//Objects::PVehiclePlant vp = To<Objects::VehiclePlant>::DynamicCast(s);
		if (s->CastToProducer())
			return (s->AwaitsUserInteraction() || /*vp->queue.IsEmpty() || */
			(!ws->IsEnabled()
			&&
			ws->CouldEnable(true)
			)
			);
		Objects::PMine mine = To<Objects::Mine>::DynamicCast(s);
		if (mine)
			return !mine->IsEnabled() && mine->CouldEnable();

		auto lab = To<Objects::Laboratory>::DynamicCast(s);
		if (lab)
		{
			if ((lab->IsEnabled() || lab->CouldEnable()) && lab->owner->researchState.IsIdle())
				return true;
			return !lab->IsEnabled() && lab->CouldEnable();
		}

		if (To<Objects::NexusPort>::DynamicCast(s))
			return !ws->IsEnabled() && ws->CouldEnable();

		#ifndef NO_SATELLITES
			if (s->IsSkyLinkStructure())
			{
				Objects::PSkyLinkStructure st = To<Objects::SkyLinkStructure>::DynamicCast(s);	//around a inheritance corner, prolly required to use dynamic cast here
				if (!st->connectedToHub.expired())
				{
					return !To<Objects::WorkingStructure>::Cast(s)->IsEnabled();
				}
				if (s->IsSkyPort())
				{
					Objects::PSkyPort p = To<Objects::SkyPort>::Cast(s);
					if ((p->queue.IsEmpty() || !p->IsEnabled()) && p->CouldEnable(true))
						return true;
				}
			}
		#endif
		return false;

	}

	bool	IsIdle(const Objects::PWorldEntity&e)
	{
		if (e->IsStructure())
			return IsIdle(To<Objects::Structure>::Cast(e));
		if (e->IsMobile())
			return IsIdle(To<Objects::MobileEntity>::Cast(e));
		return false;
	}

	void	InnerUndoTask(const Objects::PWorldEntity&e, bool force)
	{
		#ifdef HAS_WINDOW
			LogEvent(EventSignificance::Debug,Objects::selectedEntity.get(),"InnerUndoTask()");
		#endif
		if (!e)
			return;
		if (e->IsMobile())
		{
			#ifdef HAS_WINDOW
				if (Objects::CommonCmd::IsModeActive(Objects::CursorMode::BuildBlockMode) || Objects::CommonCmd::IsModeActive(Objects::CursorMode::RemoveBlockMode))
				{
					Objects::cellSelection.EraseLast();
					return;
				}
			#endif
			Objects::PMobileEntity m = To<Objects::MobileEntity>::Cast(e);
			if (m->HasTasks())
			{
				#ifdef HAS_WINDOW
				if (!force)
				{
					Objects::PMobileEntity v = m;
					if (v->constructionActionCounter > 0 
						&& v->myTasks.Count() == 1
						&& v->myTasks.GetNewest().IsJob() 
						&& v->myTasks.GetNewest().job->resourceCostLeft > 0)
					{
						TUniqueID id = v->id;
						HUD::Dialog::Confirm("Are you sure you wish to clear the current production task? The invested resources will be lost.",[id]()
						{
							InnerUndoTask(Objects::GetEntity(id),true);
						},
						[id]()->HUD::Dialog::CheckResult
						{
							if (!Network::client.IsConnected() || !Objects::GetEntity(id))
								return HUD::Dialog::CheckResult::Abort;
							if (RuntimeConfig::phase != RuntimeConfig::Phase::CommandPhase || Network::newRoundRequested)
								return HUD::Dialog::CheckResult::OkayNotRightNow;
							return HUD::Dialog::CheckResult::OkayContinue;
						},true);
						return;
					}
					else
					{
			
					}
		
				}
				#endif

				//SignalDropLastTask(v);
				//v->DropLastTask();
				bool isJob = m->myTasks.GetNewest().IsJob();
				Network::Channels::dropTasks.SendTo(Network::client, Protocol::DropTasks( m->id, Protocol::DropMode::LastTask));
				if (isJob && m->myTasks.Count() > 1 
					&& m->myTasks[m->myTasks.Count() - 2].type == Objects::MobileEntity::Task::MoveTo)
				{
					Network::Channels::dropTasks.SendTo(Network::client, Protocol::DropTasks( e->id, Protocol::DropMode::LastTask));
				}
			}
		}
		else
			Network::Channels::dropTasks.SendTo(Network::client, Protocol::DropTasks( e->id, Protocol::DropMode::LastTask));
	}


}

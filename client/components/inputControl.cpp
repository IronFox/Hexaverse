#include "inputControl.h"
#include <objects/vehicle.h>
#include <objects/mobileEntity.h>
#include <objects/mobileEntityProducer.h>
#include <objects/structure/laboratory.h>
#include <objects/structure/mine.h>
#include <objects/faction.h>
#include "visual/hud/dialog.h"
#include "network.h"
#include "cameraControl.h"
#include "visual/objectSelection.h"
#include "visual/objectChoices.h"
#include "visual/hud/hexGrid.h"
#include "timelapse.h"
#include "visual/scene/terrain.h"
#include "visual/tutorial.h"
#include "clientRuntimeConfig.h"
#include "visual/commandDefinitions.h"

void	OnTutorialsChanged();











namespace LocalApplicationInterface
{
	bool	DirectTo(const Objects::PWorldEntity&v, const ArrayRef<Grid::LCellIdI>&targets, UINT8 flags, Objects::Vehicle::mode_t mode, bool interactive)
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
					if (!InputControl::MoveVehicleTo(me,start,*t,flags))
						return false;
					start = *t;
				}
			}
			else
				InputControl::SendDirectTo(v,targets,flags,Objects::CursorMode::Idle);
		}
		else
		{
			InputControl::SendDirectTo(v,targets,flags,mode);
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
}





namespace InputControl
{
	void SendDirectTo(const Objects::PWorldEntity&v, const ArrayRef<Grid::LCellIdI>&targets, UINT8 flags, Objects::Vehicle::mode_t mode)
	{
		Protocol::EntityDirectCommand	cmd;
		cmd.id = v->id;
		cmd.mode = (UINT16)mode;
		cmd.path = targets;
		cmd.directFlags = flags;
		Network::Channels::entityDirectCommand.SendTo(Network::client,cmd);
	}


	bool	MoveVehicleTo(const Objects::PMobileEntity&v, const Grid::LCellIdI&prev, Grid::LCellIdI target, UINT8 flags)
	{
		if (prev == target)
			return true;
		Buffer0<Objects::PathNode>		path;
		{
			bool tooLong;
			bool foundPath;

			if (v->IsVehicle())
			{
				foundPath = Objects::AppendShortestPathI((const Objects::Vehicle*)v.get(),prev,target,path,false,&tooLong);
			}
			else
			{
				#ifndef NO_SATELLITES
					if (v->IsSatellite())
						foundPath = Objects::AppendShortestPathI((const Objects::Satellite*)v.get(),prev,target.stackID,path,false,&tooLong);
					else
				#endif
					FATAL__("Routing not implemented for "+v->GetTypeName());
			}
			if (!foundPath)
			{
				if (!World::actual.LowCellIsClearIgnoreStructure(target) && target.height+1 <= Grid::CellStack::TopHeight)
				{
					target++;
					return MoveVehicleTo(v,prev,target,flags);
				}
				LogEvent(EventClass::Preset::commandRejected,v.get(),"Unable to find route to designated destination");
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
					if (v->constructionRoundCounter > 0 
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
						});
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




	#ifdef HAS_WINDOW
		Timer::Time rightMouseDragBegun;
		bool rightMouseDragged = false;
		int2 rightMouseDragFrom;

		bool GetLowCellUnderCursor(Grid::LCellIdI&result, bool includeWater, bool includeEntities)
		{
			float3 b,d;
			Camera::scene.ScreenToVector(Engine::mouse.location.windowRelative * 2.f - 1.f,b,d);
			if (!Vec::zero(d))
			{
				Vec::normalize(d);

				float distance = std::numeric_limits<float>::max();


				bool rs = false;

				if (includeEntities)
				{
					Objects::ForeachWorldEntity([&result,&rs,b,d,&distance](const Objects::PWorldEntity&v)
					{
						if (v->TestIntersection(b,d,distance))
						{
							result = v->location;
							rs = true;
						}
					},true);
				}

				#ifndef NO_SATELLITES
				if (Camera::SkyGridIsVisible())
				{
					index_t stackID = Grid::low.TestGridIntersection(b, d, Grid::visibleOrbitGrid.orbit, distance);
					if (stackID != InvalidIndex && Vec::dot(Grid::low.cells[stackID].centerVertex.xyz,d) < 0.f)
					{
						result = Grid::LCellIdI((UINT32)stackID,Grid::FromRadius(Grid::visibleOrbitGrid.orbit));
						return true;
					}
				}
				#endif

				{
					M::TVec3<> normal;
					if (VisualTerrain::TestHullIntersection(b, d, distance, normal, includeWater))
					{
						//if (test)
							//return false;
						float3 p = b + d * distance;
						float height = p.GetLength();
						distance = std::numeric_limits<float>::max();
						index_t stackID = VisualTerrain::TestLowGridIntersection(b, d, height, distance);
						if (stackID != InvalidIndex)
						{
							result = Grid::LCellIdI((UINT32)stackID,Grid::HeightFromRadius(height));
							return true;
						}
					}
				}
				return rs;
			}
			return false;
		}




		void ToggleControlGUI()
		{
			HUD::showControlsAndContacts = !HUD::showControlsAndContacts;
			HUD::HexGrid::Configure(Objects::selectedEntity);
			OnTutorialsChanged();
		}

		void BeginCameraDrag()
		{
			Tutorial::lastDelayingEvent = timer.Now();
			rightMouseDragged = false;
			rightMouseDragBegun = timer.Now();
			rightMouseDragFrom = Engine::mouse.location.absolute;
		}

		void EndCameraDrag(const bool AlsoControlsUnits)
		{
			Tutorial::lastDelayingEvent = timer.Now();

			if (!AlsoControlsUnits)
				return;



			if (timer.GetSecondsSince(rightMouseDragBegun) < 0.2f && !rightMouseDragged)
			{
				if (Objects::DisableModes(Network::ControlEnabled()))
				{
					OnTutorialsChanged();
					return;
				}

				Grid::LCellIdI	target;
				if (Objects::selectedEntity)
				{
					Objects::PTurretBase b = Objects::selectedEntity->GetTurretBase();
					if (b && b->turret->CanAttack() && b->turret->trajectoryType != Objects::TrajectoryType::VerticalBeam)
					{
						float3 b, d;
						Camera::scene.ScreenToVector(Engine::mouse.location.windowRelative * 2.f - 1.f, b, d);
						if (!Vec::zero(d))
						{
							Vec::normalize(d);

							float distance = std::numeric_limits<float>::max();

							Objects::PWorldEntity possibleTarget = Objects::GetRayIntersection(b,d,distance);
							if (possibleTarget && !possibleTarget->isOwnedByMe && Objects::selectedEntity->CanHit( Objects::TrajectoryEndPoint( possibleTarget->location )))
							{
								LocalApplicationInterface::DirectTo(Objects::selectedEntity,possibleTarget->location,
								LocalApplicationInterface::AssembleMotionFlags(false,true),
									Objects::CursorMode::AttackMode);
								return;
							}
						}
						if (Objects::selectedEntity->IsTower())
						{
							if (GetLowCellUnderCursor(target,true/*Objects::selectedVehicle && Objects::selectedVehicle->base->moveOnWater*/,true))
							{
								LocalApplicationInterface::DirectTo(Objects::selectedEntity,target,
								LocalApplicationInterface::AssembleMotionFlags(false,true),
									Objects::CursorMode::AttackMode);
								return;
							}
						}
					}
					else
					{
						if (Objects::selectedEntity->CastToProducer() && !Objects::selectedMobileEntity)
						{
							if (GetLowCellUnderCursor(target,true/*Objects::selectedVehicle && Objects::selectedVehicle->base->moveOnWater*/,false))
							{
								ApplicationInterface::DirectTo(Objects::selectedEntity,target,false,false);
								return;
							}
						}
					}
				
		
				}

				if (Objects::selectedMobileEntity 
					&& Objects::VehicleCanBeDirected()	//TODO: rename/relocate for mobile entities
				)
				{

					if (GetLowCellUnderCursor(target,true/*Objects::selectedVehicle && Objects::selectedVehicle->base->moveOnWater*/,true))
					{
						//Grid::LCellIdI	target((UINT32)cellID,Grid::FromRadius(height));
						int h;
						if (!Objects::selectedMobileEntity->CanSpawnInGrid() && Objects::selectedMobileEntity->CanSpawnAboveGrid(h))
							target.height = h;
						LocalApplicationInterface::DirectTo(Objects::selectedMobileEntity,target, LocalApplicationInterface::AssembleMotionFlags(false,false),Objects::activeCursorMode);
					}
				}
			}

		}


		void	UndoTask()
		{
			if (Objects::DisableModes(Network::ControlEnabled()))
			{
				OnTutorialsChanged();
				return;
			}

			InnerUndoTask(Objects::selectedEntity,false);
		}

		void	QueryAndSendChatMessage()
		{
			if (HUD::WindowStack::IsModal())
				return ;
		
			HUD::Dialog::Query("Please enter message","",Network::allowedChatCharacters,[](const String&candidate)->bool
				{
					return candidate.GetLength() <= 256;
				},
				Network::BroadcastChatMessage
			);


		}














		bool saveScreenshot= false,saveTransparentScreenshot = false;

		void SaveScreenshot()
		{
			saveScreenshot =true;
			saveTransparentScreenshot = Engine::input.pressed[Key::Shift];
		}

		void ToggleCameraLock()
		{
			if (TimeLapse::enabled)
			{
				LogEvent(EventClass::Preset::error,"Cannot alter camera lock while time-lapse is active");
				return;
			}
			if (!Camera::locked)
			{
				HUD::Dialog::Confirm("Enabling camera lock will disallow all changes to the active perspective until unlocked."
								"Press F4 again to unlock. Commence camera lock?",[](){Camera::locked=true;},
								[]()->HUD::Dialog::CheckResult {if (!Network::client.IsConnected()) return HUD::Dialog::CheckResult::Abort; return HUD::Dialog::CheckResult::OkayContinue;});
			}
			else
				Camera::locked = false;
		}


		void ToggleTimeLapse()
		{
			if (TimeLapse::enabled)
			{
				TimeLapse::enabled = false;
				Camera::locked = false;
				HUD::WindowStack::SignalSituationChange();
				LogEvent(EventClass::Preset::uncommon,"Time lapse disabled");
			}
			else
			{
				if (!Network::amSpectator || !Network::client.IsConnected())
				{
					LogEvent(EventClass(EventSignificance::Important,EventFlavor::UserError),"Time lapse recording cannot be enabled if the local client is not configured as spectator");
				}
				else
				{
					PathString outDirName = PathString(Logging::TimeStamp(true));
					if (TimeLapse::customBasePath.IsNotEmpty())
					{
						FileSystem::Folder f = FileSystem::CreateDirectory(TimeLapse::customBasePath);
						if (f.IsValidLocation())
							outDirName = f.FindFile(outDirName,false)->GetLocation();
						else
						{
							LogEvent( EventClass::Preset::commandRejected , "Unable to find folder '"+String(TimeLapse::customBasePath)+". Defaulting to default Documents folder");
							outDirName = RuntimeConfig::ConfigFile("timelapses/"+outDirName);
						}
					}
					else
						outDirName = RuntimeConfig::ConfigFile("timelapses/"+outDirName);

					int framesPerSecond = (int)ceil(1.f / std::max(0.001f,TimeLapse::frameDelaySeconds));
					HUD::Dialog::Confirm("Timelapse mode will capture roughly "+String(framesPerSecond)+" frame(s) per second during animation phase, but remain inactive during all other phases. The camera will be locked in place.\nProceed?",
					[outDirName]()
					{
						TimeLapse::runningNumber = 0;
						TimeLapse::enabled = true;
						Camera::locked = true;
						TimeLapse::basePath = outDirName;
						FileSystem::CreateDirectory(outDirName);
						LogEvent(EventClass::Preset::uncommon,"Time lapse enabled. Writing to "+String(outDirName));
						HUD::Dialog::PermanentMessage("Time lapse recording is enabled. Camera is locked. Press F2 to end time lapse recording",[](){return TimeLapse::enabled ? HUD::Dialog::CheckResult::OkayContinue : HUD::Dialog::CheckResult::Abort;});
					});
				}
			}
		}



		void ToggleSeeThrough()
		{
			bool enable = !VisualTerrain::SeeThrough::IsEnabled();
			VisualTerrain::SeeThrough::Update(enable, VisualTerrain::SeeThrough::LocateCamera());
			if (enable)
				Tutorial::ShowMessage("Clip Terrain");
		}

		void ViewDown()
		{
			if (VisualTerrain::SeeThrough::GetFocusPoint().height > -Grid::CellStack::HalfRange)
			{
				VisualTerrain::SeeThrough::Update(true, VisualTerrain::SeeThrough::LocateCamera()-1);
				Objects::UpdateChoices(Network::ControlEnabled());
				Tutorial::ShowMessage("Clip Terrain");
			}
		}

		void ViewUp()
		{
			if (VisualTerrain::SeeThrough::GetFocusPoint().height < Grid::CellStack::HalfRange)
			{
				VisualTerrain::SeeThrough::Update(true, VisualTerrain::SeeThrough::LocateCamera() + 1);
				Objects::UpdateChoices(Network::ControlEnabled());
				Tutorial::ShowMessage("Clip Terrain");
			}
		}



		count_t		CountIterateableEntities()
		{
			return
				Objects::vehicles.Count() 
				#ifndef NO_SATELLITES
					+ Objects::satellites.Count() 
				#endif
				+ Objects::structures.Count();
		}

		Objects::PWorldEntity	GetIterateableEntity(index_t i)
		{
			Objects::PWorldEntity rs;
			GetIterateableEntity(rs,i,Objects::vehicles);
			#ifndef NO_SATELLITES
				GetIterateableEntity(rs,i,Objects::satellites);
			#endif
			GetIterateableEntity(rs,i,Objects::structures);
			return rs;
		}
	
		index_t	FindIterateableEntity(const Objects::PWorldEntity&e)
		{
			index_t rs = InvalidIndex, offset=0;
			FindIterateableEntity(offset,rs,e,Objects::vehicles);
			#ifndef NO_SATELLITES
				FindIterateableEntity(offset,rs,e,Objects::satellites);
			#endif
			FindIterateableEntity(offset,rs,e,Objects::structures);
			return rs;

		}

		void	SelectAndFocusNextIdle()
		{
			if (HUD::Dialog::ConfirmCurrent(true))
				return;

			const count_t total = CountIterateableEntities();
			if ((total == 0) || !Network::haveID)
				return;
			index_t from = FindIterateableEntity(Objects::selectedEntity);
			if (from == InvalidIndex)
				from = 0;

			index_t to = from;
			do
			{
				to ++;
				if (to >= total)
					to = 0;

				Objects::PWorldEntity v = GetIterateableEntity(to);
				if (v->isOwnedByMe && IsIdle(v))
				{
					Objects::Select(v,Network::ControlEnabled());
					Camera::TranslateTo(Objects::selectedEntity->system.w.xyz);
					return;
				}
			}
			while (to != from);

			HUD::HexGrid::RefreshButtonState(); //just in case
		}




	
		void	SelectAndFocusPrev()
		{
			const count_t total = CountIterateableEntities();
			if ((total == 0) || !Network::haveID)
				return;
			index_t from = FindIterateableEntity(Objects::selectedEntity);
			if (from == InvalidIndex)
				from = 0;
			
			index_t to = from;
			do
			{
				if (to > 0)
				{
					to --;
				}
				else
					to = total-1;

				Objects::PWorldEntity v = GetIterateableEntity(to);

				if (v->owner == Objects::Faction::mine)
				{
					Objects::Select(v,Network::ControlEnabled());
					Camera::TranslateTo(Objects::selectedEntity->system.w.xyz);
					return;
				}
			}
			while (to != from);


		}

		void	SelectAndFocusNext()
		{
			const count_t total = CountIterateableEntities();
			if ((total == 0) || !Network::haveID)
				return;
			index_t from = FindIterateableEntity(Objects::selectedEntity);
			if (from == InvalidIndex)
				from = 0;

			index_t to = from;
			do
			{
				to ++;
				if (to >= total)
					to = 0;

				Objects::PWorldEntity v = GetIterateableEntity(to);

				if (v->owner == Objects::Faction::mine)
				{
					Objects::Select(v,Network::ControlEnabled());
					Camera::TranslateTo(Objects::selectedEntity->system.w.xyz);
					return;
				}
			}
			while (to != from);
		}



		void	ResumeBatchConstruction()
		{

			if (Objects::selectedMobileEntity && 
				Objects::selectedMobileEntity->turret->CanConstruct() &&
				Objects::activeStructureToPlace &&
				(Objects::activeCursorMode == Objects::CursorMode::Idle
				|| Objects::activeCursorMode == Objects::CursorMode::PlaceStructureMode
				)
			)
			{
				bool newly = Objects::activeCursorMode == Objects::CursorMode::Idle;
				Objects::choiceVis.arrowFromCell = Objects::selectedMobileEntity->GetLastPointOfOrigin();
				Objects::choiceVis.arrowFrom = Grid::low.cells[Objects::choiceVis.arrowFromCell.stackID].centerVertex.xyz * ( Grid::ToRadius(Objects::choiceVis.arrowFromCell.height));
				Objects::choiceVis.showArrow = true;
				Objects::activeCursorMode = Objects::CursorMode::PlaceStructureMode;
				if (newly)
					HUD::HexGrid::RefreshButtonState();
			}

		}

		void	AbortBatchConstruction()
		{
			if (Objects::activeCursorMode == Objects::CursorMode::PlaceStructureMode)
			{
				Objects::activeCursorMode = Objects::CursorMode::Idle;
				Objects::choiceVis.showArrow = false;
				HUD::HexGrid::RefreshButtonState();
			}
		}

		void	OnWheel(short delta)
		{
			if (HUD::WindowStack::OnMouseWheel(delta) == HUD::Component::WheelRs::NotContained)
				Camera::Event::WheelAction(delta);
		}



	#endif

}



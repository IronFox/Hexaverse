#include "inputControl.h"
//#include <objects/vehicle.h>
//#include <objects/mobileEntity.h>
//#include <objects/mobileEntityProducer.h>
//#include <objects/structure/laboratory.h>
//#include <objects/structure/mine.h>
#include <objects/faction.h>
//#include "visual/hud/dialog.h"
#include "network.h"
#include "cameraControl.h"
//#include "visual/objectSelection.h"
#include "visual/objectChoices.h"
//#include "visual/hud/hexGrid.h"
#include "timelapse.h"
#include "visual/scene/terrain.h"
#include "visual/tutorial.h"
//#include "clientRuntimeConfig.h"
//#include "visual/commandDefinitions.h"
#include "visual/hud/resourceGauges.h"
#include "visual/hud/hexGrid.h"
//#include "animation.h"
#include "visual/hud/menu.h"
//#include "localApplicationInterface.h"
#include "objectNetworkHelper.h"

void	OnTutorialsChanged();
















namespace InputControl
{





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
								ObjectHelper::DirectTo(Objects::selectedEntity,possibleTarget->location,
								ObjectHelper::AssembleMotionFlags(false,true),
									Objects::CursorMode::AttackMode);
								return;
							}
						}
						if (Objects::selectedEntity->IsTower())
						{
							if (GetLowCellUnderCursor(target,true/*Objects::selectedVehicle && Objects::selectedVehicle->base->moveOnWater*/,true))
							{
								ObjectHelper::DirectTo(Objects::selectedEntity,target,
									ObjectHelper::AssembleMotionFlags(false,true),
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
						ObjectHelper::DirectTo(Objects::selectedMobileEntity,target, ObjectHelper::AssembleMotionFlags(false,false),Objects::activeCursorMode);
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

			ObjectHelper::InnerUndoTask(Objects::selectedEntity,false);
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
				if (v->isOwnedByMe && ObjectHelper::IsIdle(v))
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


		void	AbortMode()
		{
			Camera::StopAutoRotation();
			if (HUD::WindowStack::CloseTopModal())
			{
				HUD::Dialog::QueryNicknameIfEmpty();
				return;
			}
			if (Objects::DisableModes(Network::ControlEnabled()))
				return;



			if (!HUD::WindowStack::IsVisible(HUD::Menu::main))
			{
				if (!HUD::showControlsAndContacts)
					InputControl::ToggleControlGUI();	
				HUD::WindowStack::Show(HUD::Menu::main);
			}
	
			HUD::WindowStack::SignalSituationChange();
		}

		void	ReallyStartNewFaction()
		{
			if (Network::haveFaction)
				Network::AbandonCurrentFaction();

			//technite menu goes here

			//Network::StartNewFaction();
		}
		void	StartNewFaction()
		{
			if (Network::haveFaction)
			{
				HUD::Dialog::Confirm("You already are in control of a faction. If you start a new faction, your previous one will be abandoned. This process cannot be reversed. Continue?",
							ReallyStartNewFaction,
							[](){return Network::client.IsConnected() && Network::haveFaction ? HUD::Dialog::CheckResult::OkayContinue : HUD::Dialog::CheckResult::Abort;});
			}
			else
				ReallyStartNewFaction();

		}
		void ToggleResourceView()
		{
			HUD::ResourceGauges::resourcesAsList = !HUD::ResourceGauges::resourcesAsList;
		}

		/**
		Primary entry point used when clicking anywhere on the window
		*/
		void Select()
		{
			Tutorial::lastDelayingEvent = timer.Now();

			if (HUD::WindowStack::OnMouseDown() || HUD::HexGrid::HandleClick())
				return;
			if (M::Rect<>(0.f,0.97f,0.1f,1.f).Contains(Engine::mouse.location.windowRelative))
			{
				HUD::Dialog::QueryNickname();
				return;
			}

			//int oldVehicleHeight = Grid::SeeThrough::GetFocusPoint().height;
			//if (Objects::selectedEntity)
			//	oldVehicleHeight = Objects::selectedEntity->location.height;

	


			if (Objects::selectedMobileEntity && Objects::CursorModeExpectsLeftClick())
			{
				Grid::LCellIdI	target;
				bool includeWater = true;

				if (InputControl::GetLowCellUnderCursor(target,includeWater, Objects::activeCursorMode == Objects::CursorMode::AttackMode))
				{
					int h;
					if (!Objects::selectedEntity->CanSpawnInGrid() && Objects::selectedEntity->CanSpawnAboveGrid(h))
						target.height = h;

					if (Objects::activeCursorMode == Objects::CursorMode::PlaceStructureMode)
					{
						if (Objects::activeStructureToPlace)
						{
							const auto placement = Objects::activeStructureToPlace->CheckPlacement(target);
							if (placement != Objects::Structure::CanPlace)
							{
								LogEvent(EventClass::Preset::commandRejected,Objects::selectedMobileEntity.get(),"Cannot place "+Objects::activeStructureToPlace->GetTypeName()+" here ("+Objects::Structure::ToString(placement)+")");
							}
							elif (Objects::selectedMobileEntity->ConstructingHere(target))
							{
								LogEvent(EventClass::Preset::commandRejected,Objects::selectedMobileEntity.get(),"Can't build here, already trying to build something here");
							}
							else
								if (ObjectHelper::DirectTo(Objects::selectedMobileEntity,target,0,Objects::CursorMode::Idle,false))
								{
									ObjectHelper::SignalNewJob(Objects::selectedMobileEntity,ConstName::Job_Construct,Objects::activeStructureToPlace->GetType(),target);
									Objects::choiceVis.arrowFromCell = target;
								}
						}
						else
						{
							LogEvent(EventClass::Preset::internalError,"Cursor is in structure placement mode, but Objects::activeStructureToPlace is null");
							DBG_FATAL__("stop");
						}
					}
					else
						ApplicationInterface::DirectTo(Objects::selectedMobileEntity,target,false,true);
					return;
				}
			}
			else
				if (Objects::selectedEntity && Objects::activeCursorMode == Objects::CursorMode::AttackMode)
				{
					float h;
					Objects::PTurretBase base = Objects::selectedEntity->GetTurretBase(h);
					if (base)
					{
						Grid::LCellIdI	target;
						if (InputControl::GetLowCellUnderCursor(target,true,true))
						{
							ApplicationInterface::DirectTo(Objects::selectedEntity,target,false,true);
							return;
						}
					}
				}

			//selectedId = InvalidIndex;
			{
				float3 b,d;
				Camera::scene.ScreenToVector(Engine::mouse.location.windowRelative * 2.f - 1.f,b,d);
				if (!Vec::zero(d))
				{
					Vec::normalize(d);

					float distance = std::numeric_limits<float>::max();


					bool handledChoice = Objects::HandleChoiceIntersection(b, d, distance);
					if (!handledChoice)
					{
						M::TVec3<> normal;
						bool selectionDone = false;
						if (Objects::CommonCmd::IsModeActive(Objects::CursorMode::BuildBlockMode) || Objects::CommonCmd::IsModeActive(Objects::CursorMode::RemoveBlockMode))
						{
							if (VisualTerrain::TestHullIntersection(b, d, distance, normal,false))
							{
								if (Objects::selectedVehicle)
								{
									float3 p = b + d * distance;
									float r = p.GetLength();
									float3 d = p / r;
									UINT32 stackID;
									int h;
									if (Objects::CommonCmd::IsModeActive(Objects::CursorMode::BuildBlockMode))
									{
										if (Vec::dot(d, normal) > 0.5f)	//flat selection
										{
											stackID = Grid::high.Locate(d);
											h = Grid::HeightFromRadius(r);
										}
										else
										{
											stackID = Grid::high.Locate(d + normal*0.001f);
											h = Grid::HeightFloorFromRadius(r);
										}
									}
									else
										if (Vec::dot(d, normal) > 0.5f)	//flat selection
										{
											stackID = Grid::high.Locate(d);
											h = int(floor(Grid::HeightFromRadiusf(r)-0.5f));
										}
										else
										{
											stackID = Grid::high.Locate(d - normal*0.01f);
											h = Grid::HeightFloorFromRadius(r);
										}
									String activity = "build";
									if (Objects::CommonCmd::IsModeActive(Objects::CursorMode::RemoveBlockMode))
										activity = "remove";
									Grid::LCellIdI origin = Objects::selectedVehicle->GetLastPointOfOrigin();
									if (h < Grid::CellStack::BottomHeight)
									{
										LogEvent(EventSignificance::NonFatalError, Objects::selectedEntity.get(), "Cannot "+activity+" here: Cannot alter bottom-layer");
									}
									elif (abs(h - origin.height) > 1)
									{
										LogEvent(EventSignificance::NonFatalError, Objects::selectedEntity.get(), "Cannot "+activity+" here: Height difference too big: Selected="+String(h)+", vehicle at: "+String(origin.height));
									}
									elif(Objects::vehicleDistanceField.distance[stackID] > 3)
										LogEvent(EventSignificance::NonFatalError, Objects::selectedEntity.get(), "Cannot " + activity + " here: Too far away from vehicle");
									elif(Objects::CommonCmd::IsModeActive(Objects::CursorMode::BuildBlockMode) && h == origin.height && Objects::vehicleDistanceField.distance[stackID] <= 1)
										LogEvent(EventSignificance::NonFatalError, Objects::selectedEntity.get(), "Cannot " + activity + " here: Too close to vehicle");
									else
									{
										Grid::HCellIdI	cell = Grid::HCellIdI(stackID, h);
										index_t current = Objects::cellSelection.GetIndexOf(cell);
										if (current == InvalidIndex)
											Objects::cellSelection << cell;
										else
											Objects::cellSelection.Erase(current);
									}
								}
								else
									FATAL__("vehicle selection lost");
								selectionDone = true;
							}
						}
						if (!selectionDone)
							if (Objects::Select(b, d, distance, Network::ControlEnabled()))
							{
								selectionDone = true;
							}
							else
							{
								distance = std::numeric_limits<float>::max();

								if (VisualTerrain::TestHullIntersection(b, d, distance, normal,false))
								{
									float3 p = b + d * distance;
									float height = p.GetLength();

									distance = std::numeric_limits<float>::max();

									index_t stackID = VisualTerrain::TestLowGridIntersection(b, d, height, distance);
									if (stackID != InvalidIndex)
									{
										Grid::LCellIdI	target((UINT32)stackID,Grid::HeightFromRadius(height));
										if (target.height < Grid::CellStack::HalfRange && target.height >= -Grid::CellStack::HalfRange)
										{
											Objects::PWorldEntity e = Objects::GetEntity(target);
											Objects::Select(e,Network::ControlEnabled());
											selectionDone = true;
											#ifdef _DEBUG
												if (Engine::input.pressed[Key::Shift])
													ShowMessage(ToString(target));
											#endif
										}
									}
								}
							}
						if (!selectionDone)
							Objects::Deselect(Network::ControlEnabled());
						//else
							//ShowMessage("nothing");
					}

				}
			}
		}

	#endif

}



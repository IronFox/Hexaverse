#include <global_root.h>
#include "commandDefinitions.h"
#include "hud/objectCommands.h"
#include <objects/structure.h>
#include "objectSelection.h"
#include <objects/research.h>
#include <objects/gridAndChoices.h>
#include <objects/vehicle.h>
#include <objects/prototypes.h>
#include <objects/structure/laboratory.h>
#include <objects/structure/mine.h>
#include <objects/structure/vehiclePlant.h>
#include <objects/faction.h>
#include <objects/structure/planeFactory.h>
#include <objects/subOrbital.h>
#include <objects/structure/tower.h>
#include "../clientRuntimeConfig.h"
#include "../network.h"
#include "hud/dialog.h"
#include "objectChoices.h"
#include "scene/terrain.h"
#include "scene/objectRendering.h"
#include "../cameraControl.h"
#include "hud/menu.h"
#include "hud/hexGrid.h"
#include "../objectNetworkHelper.h"
#include "scene/vehicleAccessibilityMesh.h"


namespace Objects
{
	namespace Commands
	{
		void DefineResourceTransfer(Component::Turret&component, Resource res);
		void DefineOffensive(Command::Container&commands,Objects::TrajectoryType t);
		void DefineOffensive(Component::Turret&component);


		Grid::LCellIdI	GetPointOfOrigin(const Objects::MobileEntity&e)
		{
			if (ShouldQueueCommands())
				return e.GetLastPointOfOrigin();
			return e.location;
		}

		namespace Detail
		{






			/**
			Relay-functor to convert a single boolean function or variable to Objects::CommandFlags.
			Depending on the returned boolean value, flag compositions will either be 
			Objects::CommandFlags::IsEnabled | Objects::CommandFlags::IsPermaActive 
			or
			Objects::CommandFlags::IsEnabled
			*/
			class GlobalPermaSwitch
			{
				std::function<bool()>	getState,enabled;
			public:
				/**/	GlobalPermaSwitch(const std::function<bool()>&f,const std::function<bool()>&enabled=std::function<bool()>()):getState(f),enabled(enabled)
				{}
				/**/	GlobalPermaSwitch(bool&b,const std::function<bool()>&enabled=std::function<bool()>()):getState([&b](){return b;}),enabled(enabled)
				{}


				Objects::CommandFlags	operator()(bool)
				{
					Objects::CommandFlags rs= getState() ? Objects::CommandFlags::IsPermaActive : Objects::CommandFlags::None;
					if (!enabled || enabled())
						rs |= Objects::CommandFlags::IsEnabled;
					return rs;
				}


			};

			static void				EnableWorkingStructure()
			{
				Objects::WorkingStructure*s2 = dynamic_cast<Objects::WorkingStructure*>(selectedEntity.get());
				if (s2)
				{
					ApplicationInterface::SignalEnableStructure(*s2, !s2->IsEnabled());
					//s2->SetEnabled(!s2->IsEnabled());
				}
			}

			static bool				WorkingStructureIsEnabled()
			{
				Objects::WorkingStructure*s2 = dynamic_cast<Objects::WorkingStructure*>(selectedEntity.get());
				return s2 && s2->IsEnabled();
			};



			static bool				EnqueueSelectedConstruction(VehiclePlant&plant)
			{
				if (plant.selectedBase && plant.selectedTurret)
				{
					static Buffer0<Objects::Choice>	choices;
					Objects::GetProductionChoices(&plant,plant.GetSelectionPrototype(),choices);
					static bool ignoreWarnings = false;
					if (choices.IsNotEmpty() || ignoreWarnings)
						ApplicationInterface::SignalConstructionBegin(plant.id, {plant.selectedBase->constName, plant.selectedTurret->constName});
					else
					{


						#ifdef HAS_WINDOW					
							String waterNotice;
							if (!plant.selectedBase->moveOnWater)
								waterNotice = " Note that this vehicle type cannot stand on water. ";
							TUniqueID myID = plant.id;
							EntityType type = {plant.selectedBase->constName,plant.selectedTurret->constName};
							HUD::Dialog::Confirm("Cannot find valid spawning locations around '"+plant.GetNiceName()+"' for '"+Vehicle::GetTypeName(plant.selectedBase,plant.selectedTurret)+"'. "+waterNotice+"Are you sure you wish to start this production?",
									[type,myID]()
									{
										ApplicationInterface::SignalConstructionBegin(myID, type);
										//ignoreWarnings = true;
									}
									,
									[myID]()->HUD::Dialog::CheckResult
									{
										PVehiclePlant plant = To<VehiclePlant>::DynamicCast(GetEntity(myID));
										if (!plant || plant->IsDeadOrGhost())
											return HUD::Dialog::CheckResult::Abort;
										if (!Network::ControlEnabled())
											return HUD::Dialog::CheckResult::OkayNotRightNow;
										return HUD::Dialog::CheckResult::OkayContinue;
									}
								);
						#else
							ApplicationInterface::SignalConstructionBegin(id, selectedBase->constName, selectedTurret->constName);
						#endif
					}
				}
				return false;
			}
	

		}

		void DefineVehiclePlant(CommandContainer&commands)
		{
			DefineWorkingStructure(commands);

			Command::PIconCommand newEnable {new Command::IconCommand("Enable",
			[]()
			{

				VehiclePlant*s2 = dynamic_cast<VehiclePlant*>(selectedEntity.get());
				if (s2)
				{
					if (!s2->IsEnabled() && !s2->CanEnable())
					{
						ApplicationInterface::SignalUserMessage(s2->GetNiceName()+" is switched off because its queue is empty. It will attempt to automatically enable once you enqueue at least one production job.");
						return;
					}
					ApplicationInterface::SignalEnableStructure(*s2, !s2->IsEnabled());
				}
			}
			,[](bool controlEnabled)
			{
				Objects::CommandFlags rs = Objects::CommandFlags::None;
				VehiclePlant*div = dynamic_cast<VehiclePlant*>(selectedEntity.get());
				if (controlEnabled && div != nullptr)
					rs |= Objects::CommandFlags::IsEnabled;
				if (Detail::WorkingStructureIsEnabled())
					rs |= Objects::CommandFlags::IsPermaActive;
				return rs;
			}
			,NoCosts
			)};
			newEnable->customLabelBuilder = [](Command::Base::Label&outLabel)
			{
				if (selectedEntity && selectedEntity->IsWorkingStructure() && To<WorkingStructure>::Cast(selectedEntity)->IsEnabled())
					outLabel << "Disable";
				else
					outLabel << "Enable";
			};


			commands.ReplaceLocalMember([](const Command::PBase&cmd) {return cmd->name == "Enable";},newEnable);
				


			//commands.AppendRow();
			foreach (VehiclePlant::baseComponents,comp)
			{
				Component::VehicleBase* c = *comp;
				commands.Add(new Command::EntityConstructionCommand(c->typeName,c,nullptr,PWorldEntity(),[c]()
				{
					VehiclePlant*plant = dynamic_cast<VehiclePlant*>(selectedEntity.get());
					if (plant)
						plant->selectedBase = c;
				}
				,[c](bool controlEnabled)
				{
					Objects::CommandFlags rs = CommonCmd::CommonButtonCheck(controlEnabled);
					VehiclePlant*plant = dynamic_cast<VehiclePlant*>(selectedEntity.get());
					if (plant && plant->selectedBase == c)
						rs |= Objects::CommandFlags::IsPermaActive;
					if (!plant || !c->isUnlocked.GetValue(plant->GetResearchState()))
						rs = Objects::CommandFlags::None;
					return rs;
				}));
				To<Objects::Command::VisualCommand>::Cast(commands.GetLast())->RasterizeImage();
			}

			commands.AppendRow();
			foreach (VehiclePlant::turretComponents,comp)
			{
				Component::Turret* c = *comp;
				commands.Add(new Command::EntityConstructionCommand(c->typeName,nullptr,c,PWorldEntity(),[c]()
				{
					VehiclePlant*plant = dynamic_cast<VehiclePlant*>(selectedEntity.get());
					if (plant)
					{
						plant->selectedTurret = c;

						if (plant->selectedBase && plant->selectedTurret)
							Detail::EnqueueSelectedConstruction(*plant);
						else
						{
							if (!plant->selectedTurret)
								LogEvent(EventClass::Preset::internalError,plant,"No Turret selected for construction");
							else
								LogEvent(EventClass::Preset::internalError,plant,"No base selected for construction");
						}
					}
				},
				[c](bool controlEnabled)
				{
					if (!controlEnabled)
						return Objects::CommandFlags::None;
					VehiclePlant*plant = dynamic_cast<VehiclePlant*>(selectedEntity.get());
					if (plant)
					{
						Objects::CommandFlags rs = Objects::CommandFlags::IsEnabled;
						if (!c->isUnlocked.GetValue(plant->GetResearchState()))
							rs = Objects::CommandFlags::None;

						return rs;
					}
					else return Objects::CommandFlags::None;
				},
				[c](int&mineralCostOut, Resource&costTypeOut, int&timeCostOut)
				{
					costTypeOut = Resource::Minerals;
					PVehiclePlant p = To<VehiclePlant>::DynamicCast(selectedEntity);
					ASSERT__(p);
					if (!p->selectedBase)
					{
						mineralCostOut = 0;
						timeCostOut = 0;
						return;
					}
					mineralCostOut = Vehicle::GetPrice(p->selectedBase,c,p->GetResearchState());
					timeCostOut = p->ProductionRounds(mineralCostOut,false);
				}));
				To<Objects::Command::VisualCommand>::Cast(commands.GetLast())->RasterizeImage();
			}
		
		}


		void DefinePlaneFactory(CommandContainer&commands)
		{
			DefineWorkingStructure(commands);
			commands.VisitLocalMembers([](const Command::PBase&cmd)
			{
				if (cmd->name == "Enable")
				{
					(*cmd) = Command::IconCommand("Enable",
						[]()
					{

						PlaneFactory*s2 = dynamic_cast<PlaneFactory*>(selectedEntity.get());
						if (s2)
						{
							if (!s2->IsEnabled() && !s2->CanEnable())
							{
								ApplicationInterface::SignalUserMessage(s2->GetNiceName() + " is switched off because its queue is empty. It will attempt to automatically enable once you enqueue at least one production job using the 'Construct Selected' button. You must first select both a foundation and a turret, however.");
								return;
							}
							ApplicationInterface::SignalEnableStructure(*s2, !s2->IsEnabled());
						}
					}
						, [](bool controlEnabled)
					{
						Objects::CommandFlags rs = Objects::CommandFlags::None;
						PlaneFactory*div = dynamic_cast<PlaneFactory *> (selectedEntity.get());
						if (controlEnabled && div != nullptr)
							rs |= Objects::CommandFlags::IsEnabled;
						if (Detail::WorkingStructureIsEnabled())
							rs |= Objects::CommandFlags::IsPermaActive;
						return rs;
					}
						, NoCosts
						);
				}
			});


			foreach(Objects::Prototypes::planes, proto)
			{
				auto p = *proto;
				Command::PEntityConstructionCommand cmd(new Command::EntityConstructionCommand("Construct " + p->GetTypeName(), nullptr, nullptr, p, [p]()
					{
						PPlaneFactory factory = To<PlaneFactory>::DynamicCast(selectedEntity);
						if (!factory)
							return;
						ApplicationInterface::SignalConstructionBegin(factory->id, p->GetType());
					}
					,
					[p](bool controlEnabled)
					{
						if (!controlEnabled)
							return CommandFlags::None;
						PlaneFactory*plant = dynamic_cast<PlaneFactory*>(selectedEntity.get());
						if (plant)
						{
							CommandFlags rs = p->turret->isUnlocked.GetValue(selectedEntity->GetResearchState()) ? CommandFlags::IsEnabled : CommandFlags::None;
							return rs;
						}
						return CommandFlags::None;
					}
				));

				commands.Add(cmd);
				cmd->RasterizeImage();
			}
		}


		void OnAttackButtonPress()
		{
			if (!Objects::selectedEntity)
				return;
			auto base = Objects::selectedEntity->GetTurretBase();
			if (!base)
				return;
			if (base->turret->trajectoryType == Objects::TrajectoryType::AAMissile)
				return;	//no manual attack for now
			if (IsBeam(base->turret->trajectoryType))
			{
				Grid::LCellIdI loc = Objects::selectedEntity->location;
				if (ShouldQueueCommands() && Objects::selectedMobileEntity)
					loc = Objects::selectedMobileEntity->GetLastPointOfOrigin();
				ObjectHelper::DirectTo(Objects::selectedEntity, loc, ObjectHelper::AssembleMotionFlags(false,true,!ShouldQueueCommands()), Objects::CursorMode::AttackMode);
				return;
			}
			else
				CommonCmd::ToggleMode<CursorMode::AttackMode>();
		}


		void DefineOffensive(Command::Container&commands,Objects::TrajectoryType t)
		{
			if (t == Objects::TrajectoryType::AAMissile)
				return;	//no manual attack for now

			String text = "Attack";
			if (IsBeam(t))
				text = "Fire Beam";
			auto cmd = new Command::IconCommand(text,&OnAttackButtonPress, &CommonCmd::ModeCheck<CursorMode::AttackMode>,NoCosts);
			cmd->name += " (CTRL)";
			commands.Add(cmd);
		}

		void DefineOffensive(Component::Turret&component)
		{
			#ifndef NO_MUNITION
				if (component.ammoNeedPerShot.IsFunctionOrConstNonZero())
					DefineResourceTransfer(component,component.ammoType);
			#endif
			DefineOffensive(component.commands,component.trajectoryType);
		}

		void DefineTower(Objects::Tower::Prototype&proto)
		{
			if (proto.turret->CanAttack())
				DefineOffensive(proto.commands,proto.turret->trajectoryType);
		}

		void DefineMine(CommandContainer&commands)
		{
			DefineWorkingStructure(commands);
			commands.Add( new Command::IconCommand("Restore Core Link",[]()
			{
				PMine mine = ToMine(selectedEntity);
				if (mine)
				{
					ApplicationInterface::SignalRestoreCoreLink(mine->id);
				}
			},
			[](bool controlEnabled)
			{
				if (!controlEnabled)
					return Objects::CommandFlags::None;
				PMine mine = ToMine(selectedEntity);
				return mine && !mine->IsLinkedToCore() ? Objects::CommandFlags::IsEnabled : Objects::CommandFlags::None;
			},
			NoCosts
			));
		}

		void DefineWorkingStructure(CommandContainer & commands)
		{
			//commands.baseCoords = int2(3,0);


			Command::PIconCommand cmd (new Command::IconCommand("Enable",&Detail::EnableWorkingStructure,
			[](bool controlEnabled)->Objects::CommandFlags
			{
				Objects::CommandFlags rs = Objects::CommandFlags::None;
				Objects::WorkingStructure*div = dynamic_cast<Objects::WorkingStructure*>(selectedEntity.get());
				if (controlEnabled)
				{
					if (div && div->CanEnable())
					{
						rs |= Objects::CommandFlags::IsEnabled;
					}
				}
				if (div && div->IsEnabled())
					rs |= Objects::CommandFlags::IsPermaActive;
				return rs;
			}
			,NoCosts
			));

			cmd->customLabelBuilder = [](Command::Base::Label&outLabel)
			{
				if (selectedEntity && selectedEntity->IsWorkingStructure() && To<WorkingStructure>::Cast(selectedEntity)->IsEnabled())
					outLabel << "Disable";
				else
					outLabel << "Enable";
			};
			commands.Add(cmd);
		}

		static void BuildCommand(CommandContainer&parent, const Research::PTopic&t)
		{
			Command::PResearchCommand rs(new Command::ResearchCommand(t));
			parent.Add(rs);
			foreach (t->children,c)
			{
				BuildCommand(*rs,*c);
			}
			if (t->componentReference || t->prototypeReference)
				rs->RasterizeImage();
		}

		static count_t			researchCommandOffset = InvalidIndex, researchQueueOffset = InvalidIndex;


		static	void	ConfigureResearchQueueButtons(CommandContainer&commands)
		{
			commands.TruncateRow(0,researchQueueOffset);
			if (Faction::mine && !Faction::mine->researchState.IsIdle())
			{
				foreach (Faction::mine->researchState.GetResearchQueue(),r)
				{
					Command::PResearchAbortCommand rs(new Command::ResearchAbortCommand(*r,r == Faction::mine->researchState.GetResearchQueue().begin()));
					if ((*r)->componentReference || (*r)->prototypeReference)
						rs->RasterizeImage();
					commands.Add(rs);
				}
			}

			commands.Refresh(Network::ControlEnabled());
		}

		static	void	ConfigureResearchButtons(CommandContainer&commands)
		{
			if (researchCommandOffset == InvalidIndex)
				researchCommandOffset = commands.CountCommands();
			else
				commands.TruncateRow(0,researchCommandOffset);
			foreach (Research::globalState.rootResearch,res)
			{
				BuildCommand(commands,*res);
			}
			commands.AddSpace();
			researchQueueOffset = commands.CountCommands();
			ConfigureResearchQueueButtons(commands);
		}


		void	ReconfigureResearchButtons()
		{
			ConfigureResearchButtons(Laboratory::commands);
		}
		void	ReconfigureResearchQueueButtons()
		{
			ConfigureResearchQueueButtons(Laboratory::commands);
		}

		void	DefineLaboratory(CommandContainer&commands)
		{
			DefineWorkingStructure(commands);
			ConfigureResearchButtons(commands);
		}

		void TerraformerConstruct(const PStructure&p)
		{
			PMobileEntity vehicle = selectedMobileEntity;
			if (!vehicle || !vehicle->turret->CanConstruct())
				return;
			if (activeCursorMode != CursorMode::PlaceStructureMode)
			{
				//bool directPlace = !Engine::input.pressed[Key::Ctrl] && vehicle->myTasks.IsNotEmpty() && vehicle->myTasks.GetNewest().type == MobileEntity::Task::MoveTo;
				//if (directPlace)
				//{
				//	const Grid::LCellIdI last = vehicle->GetLastPointOfOrigin();
				//	const Grid::LCellIdI prev = vehicle->GetPrevPointOfOrigin();
				//	const auto&n = Grid::low.grid.GetCell(last.stackID).neighborhood;
				//	
				//	for (int k = 0; k < n.numNeighbors; k++)
				//		if (n.neighborID[k] == prev.stackID)
				//		{
				//			directPlace = false;
				//			break;
				//		}
				//}

				//if (!directPlace)
				{
					activeCursorMode = CursorMode::PlaceStructureMode;
					choiceVis.showArrow = true;
					choiceVis.arrowFromCell = GetPointOfOrigin(*vehicle);
					choiceVis.arrowFrom = Grid::low.cells[choiceVis.arrowFromCell.stackID].centerVertex.xyz * ( Grid::ToRadius(choiceVis.arrowFromCell.height));

					if (!ShouldQueueCommands())
						ApplicationInterface::SignalDropSafeTasks(vehicle);
				}
				//else
					//ApplicationInterface::SignalNewJob(vehicle, ConstName::Job_Construct, p->GetCTypeName());	
						//this still works, but seemed unintuitive rather quickly
				activeStructureToPlace = p;
			}
			else
			{
				activeCursorMode = CursorMode::Idle;
				choiceVis.showArrow = false;
			}


			//SignalNewJob(vehicle, ConstName::Job_Construct, p->GetCTypeName());
		}

		namespace Detail
		{
			namespace SubOrbitalCmd
			{
				Objects::CommandFlags CheckGridVisibility(bool)
				{
					return Objects::CommandFlags::IsEnabled |
						(Camera::SubOrbitalGridIsVisible() ? Objects::CommandFlags::IsPermaActive : Objects::CommandFlags::None);
				}

				void	Land()
				{
					if (!selectedMobileEntity || !selectedMobileEntity->IsPlane() || GetPointOfOrigin(*selectedMobileEntity).height <= Grid::CellStack::TopHeight)
						return;
					Objects::SubOrbital::PPlane plane = To<Objects::SubOrbital::Plane>::Cast(selectedMobileEntity);
					auto loc = GetPointOfOrigin(*plane);
					int h = plane->GetLandingHeight(loc,*plane);
					if (h != badHeight)
						ApplicationInterface::DirectTo(selectedMobileEntity, Grid::LCellIdI(loc.stackID, h), false, false);
				}

				CommandFlags LandCheck(bool controlEnabled)
				{
					if (!controlEnabled || !selectedMobileEntity || !selectedMobileEntity->IsPlane() || GetPointOfOrigin(*selectedMobileEntity).height <= Grid::CellStack::TopHeight)
						return CommandFlags::None;
					Objects::SubOrbital::PPlane plane = To<Objects::SubOrbital::Plane>::Cast(selectedMobileEntity);
					auto loc = GetPointOfOrigin(*selectedMobileEntity);
					int h = plane->GetLandingHeight(loc,*plane);
					if (h != badHeight)
						return CommandFlags::IsEnabled;
					return CommandFlags::None;
				}

				void	TakeOff()
				{
					if (!selectedMobileEntity || !selectedMobileEntity->IsPlane() || GetPointOfOrigin(*selectedMobileEntity).height > Grid::CellStack::TopHeight)
						return;
					ApplicationInterface::DirectTo(selectedMobileEntity, Grid::LCellIdI(GetPointOfOrigin(*selectedMobileEntity).stackID, Objects::SubOrbital::Height), false, false);
				}

				CommandFlags TakeOffCheck(bool controlEnabled)
				{
					if (!controlEnabled || !selectedMobileEntity || !selectedMobileEntity->IsPlane() || GetPointOfOrigin(*selectedMobileEntity).height > Grid::CellStack::TopHeight)
						return CommandFlags::None;
					return CommandFlags::IsEnabled;
				}
			}
		


			//struct CellCoords : public int2
			//{

			//	/**/		CellCoords(int x, int y):int2(x,y)
			//	{}


			//	CellCoords	operator++(int)
			//	{
			//		CellCoords rs = *this;
			//		if (y == 0)
			//			y = 1;
			//		else
			//		{
			//			x++;
			//			y = 0;
			//		}
			//		return rs;
			//	}
			//	CellCoords&	operator++()
			//	{
			//		if (y == 0)
			//			y = 1;
			//		else
			//		{
			//			x++;
			//			y = 0;
			//		}
			//		return *this;
			//	}

			//};


			bool	NotModifyBlockMode(const Vehicle&v)
			{
				return activeCursorMode != CursorMode::BuildBlockMode && activeCursorMode != CursorMode::RemoveBlockMode;
			}

			/*static*/ bool TerraformerNotBuilding()
			{
				PMobileEntity v = selectedMobileEntity;
				return v && (!v->HasTasks() || !v->myTasks.Peek().IsJob());
			}


/*

			void Terraformer::Cmd::ToggleDigMode()
			{
				CommonCmd::ToggleMode(CursorMode::DigMode);
			}
*/

			//void Terraformer::Cmd::ClearAbove()
			//{
			//	CommonCmd::ToggleMode(CursorMode::ClearAboveMode);
			//}

			//bool Terraformer::Cmd::ClearAboveIsActive(const PWorldEntity&e)
			//{
			//	return CommonCmd::IsModeActive(e,CursorMode::ClearAboveMode);
			//}
	
			//bool Terraformer::Cmd::BuildBlockIsActive(const PWorldEntity&e)
			//{
			//	return CommonCmd::IsModeActive(e,CursorMode::BuildBlockMode);
			//}



			//bool Terraformer::Cmd::RemoveBlockIsActive(const PWorldEntity&e)	
			//{
			//	return CommonCmd::IsModeActive(e,CursorMode::RemoveBlockMode);
			//}
	 


			bool TerraformerCommitIsPossibleC(bool controlEnabled)
			{
				return controlEnabled && 
					(CommonCmd::IsModeActive(CursorMode::BuildBlockMode) || CommonCmd::IsModeActive(CursorMode::RemoveBlockMode));
			}

			Objects::CommandFlags TerraformerCommitCheck(bool controlEnabled)
			{
				return TerraformerCommitIsPossibleC(controlEnabled) ? Objects::CommandFlags::IsEnabled : Objects::CommandFlags::None;
			}

	
		/*	bool VehicleCmd::AbortIsPossible(const PWorldEntity&e)
			{
				PVehicle v = To<Vehicle>::DynamicCast(e);
				return (activeCursorMode != CursorMode::Idle || (v && v->tasks.IsNotEmpty()));
			}*/

			bool TerraformerConstructionIsPossible()
			{
				return CommonCmd::IsModeActive(CursorMode::Idle) && TerraformerNotBuilding(); 
			}
		
	
			/*static*/ void					TerraformerDigDown()
			{
				PVehicle vehicle = To<Vehicle>::DynamicCast(selectedMobileEntity);
				if (!vehicle)
					return;
				if (vehicle->location.height < Grid::CellStack::BottomHeight+1)
					return;

				if (!Objects::ShouldQueueCommands())
					ApplicationInterface::SignalDropSafeTasks(vehicle);
				ApplicationInterface::SignalNewJob(vehicle, ConstName::Job_DigDown, Objects::EntityType());
			}

			template <ConstName Command>
				void	IssueCommand()
				{
					if (!selectedMobileEntity)
						return;
					if (!Objects::ShouldQueueCommands())
						ApplicationInterface::SignalDropSafeTasks(selectedMobileEntity);
					ApplicationInterface::SignalNewJob(selectedMobileEntity, Command,Objects::EntityType());
				}

			template <ConstName Command, ConstName Parameter>
				void	IssueParameterizedCommand()
				{
					PMobileEntity vehicle = selectedMobileEntity;
					if (!vehicle)
						return;
					if (!ShouldQueueCommands())
						ApplicationInterface::SignalDropSafeTasks(vehicle);
					ApplicationInterface::SignalNewJob(vehicle, Command,Parameter);					
				}

			template <ConstName DetonationSize>
				void	IssueDetonationCommand()
				{
					IssueParameterizedCommand<ConstName::Job_SelfDetonate,DetonationSize>();
				}


			void					TerraformerBuildRoad()
			{
				PVehicle vehicle = To<Vehicle>::DynamicCast(selectedMobileEntity);
				if (!vehicle)
					return;
				if (vehicle->location.height <= Grid::CellStack::BottomHeight)
					return;
				if (!Objects::ShouldQueueCommands())
					ApplicationInterface::SignalDropSafeTasks(vehicle);
				ApplicationInterface::SignalNewJob(vehicle, ConstName::Job_BuildRoad,Objects::EntityType());
			}





			void	TerraformerRemoveBlock()
			{
				PVehicle v = To<Vehicle>::DynamicCast(selectedMobileEntity);
				if (!v)
					return;
				CommonCmd::ToggleMode(CursorMode::RemoveBlockMode);

				cellSelection.Clear();
				removeCandidates.Clear();

				if (activeCursorMode == CursorMode::RemoveBlockMode)
				{
					ASSERT__(v);
					Grid::high.UpdatePlanarDistanceField(
						#ifdef HYBRID_GRID
							Grid::low.grid.GetCell(GetPointOfOrigin(*v).stackID).higherEquivalent
							, 8, Objects::vehicleDistanceField);
						#else
							GetPointOfOrigin(*v).stackID

							, 4, Objects::vehicleDistanceField);
						#endif
					for (UINT32 i = 0; i < (UINT32)Grid::high.grid.CellCount; i++)
					{
						if (Objects::vehicleDistanceField.distance[i] <= 3)
						{
							UINT32 at = v->location.height + Grid::CellStack::HalfRange;
							UINT32 min = std::max(at,2U)-1;
							const auto&stack = World::actual.GetStack(i);
							for (UINT32 j = min; j <= at + 1; j++)
							{
								if (stack.volumeCell[j].IsSolid())
								{
									//if (j != at || Grid::vehicleDistanceField.distance[i] > 1)
									{
										removeCandidates << Grid::HCellIdU(i, j);
									}

								}

							}

						}

					}


				}

				if (v == selectedEntity)
					UpdateChoices(true);
			}


			void	TerraformerBuildBlock()
			{
				PVehicle v = To<Vehicle>::DynamicCast(selectedMobileEntity);
				if (!v)
					return;
				CommonCmd::ToggleMode(CursorMode::BuildBlockMode);
				cellSelection.Clear();
				buildCandidates.Clear();
				const int height = GetPointOfOrigin(*v).height;

				if (activeCursorMode == CursorMode::BuildBlockMode)
				{
					ASSERT__(v);
					Grid::high.UpdatePlanarDistanceField(
						#ifdef HYBRID_GRID
							Grid::low.grid.GetCell(
								GetPointOfOrigin(*v).stackID).higherEquivalent
							, 8,
						#else
							GetPointOfOrigin(*v).stackID

							, 4,
						#endif
						 Objects::vehicleDistanceField);
					for (UINT32 i = 0; i < (UINT32)Grid::high.grid.CellCount; i++)
					{
						if (Objects::vehicleDistanceField.distance[i] <= 3)
						{
							UINT32 at = height + Grid::CellStack::HalfRange;
							const auto&stack = World::actual.GetStack(i);
							for (UINT32 j = at - 1; j <= at + 1; j++)
							{
								const auto&cell = stack.volumeCell[j];
								if (!cell.IsOccupied() || cell.IsLiquid())
								{
									if (j != at || Objects::vehicleDistanceField.distance[i] > 1)
									{
										buildCandidates << Grid::HCellIdU(i, j);
									}

								}

							}

						}

					}


				}

				if (v == selectedEntity)
					UpdateChoices(true);
			}

			void					TerraformerCommitOperation()
			{
				PVehicle v = To<Vehicle>::DynamicCast(selectedMobileEntity);
				if (!v)
					return;
				Array<Grid::HCellIdI>	selection;
				Objects::cellSelection.MoveToArray(selection,true);
				removeCandidates.Clear();
				buildCandidates.Clear();

				ApplicationInterface::SignalTerrainModification(v,activeCursorMode,selection );

				activeCursorMode = CursorMode::Idle;
			}

		}

		void	DefinePlane(Objects::Component::Turret&component)
		{
			component.commands.Add(new Command::IconCommand("Take Off", &Detail::SubOrbitalCmd::TakeOff, &Detail::SubOrbitalCmd::TakeOffCheck, NoCosts));
			component.commands.Add(new Command::IconCommand("Land", &Detail::SubOrbitalCmd::Land, &Detail::SubOrbitalCmd::LandCheck, NoCosts));
		}

		void	DefineSensorDrone(Objects::Component::Turret&component)
		{
			DefinePlane(component);
		}

		void	DefineRaptor( Objects::Component::Turret&component )
		{
			DefinePlane(component);
			DefineOffensive(component);
		}

		void	DefineBomber( Objects::Component::Turret&component)
		{
			DefinePlane(component);
			DefineOffensive(component);
			component.commands.Add( new Command::IconCommand("Obliterate", &Detail::IssueParameterizedCommand<ConstName::Job_DoomsdayBeamAttack, ConstName::DetonationSize_Doomsday>
				, &CommonCmd::IdleCheck
				,[](int&resourceCostOut, Resource&costTypeOut, int&timeCostOut)
				{
					ASSERT__(selectedMobileEntity);
					resourceCostOut = Component::SubOrbital::Bomber::doomsdayBeamAttackCosts.GetValue(selectedMobileEntity->GetResearchState());
					costTypeOut = Resource::Uranium;
					timeCostOut = selectedMobileEntity->GetOperationCompletionRounds(resourceCostOut,Component::Nuclear);
				}
				));
		}

		void	DefineCargoPlane(Objects::Component::Turret&component)
		{
			DefinePlane(component);
		}

		void	DefineTerraformer(Objects::Component::Turret&component)
		{
			Command::PCommandGroup	buildGroup = Command::AddChild(component.commands,"Build"),
							terraformGroup = Command::AddChild(component.commands,"Terraform"),
							terraformRemoveGround = Command::AddChild(*terraformGroup,"Remove Ground"),
							terraformAddGround = Command::AddChild(*terraformGroup,"Add Ground"),
							structureGroup[Structure::Category::N];

			#ifndef ONE_COMPOUND_PER_FACTION
				PCommandGroup	transferGroup = component.commands.AddChild("Transfer");
			#endif

			for (index_t i = 0; i < Structure::Category::N; i++)
			{
				#ifdef ONE_COMPOUND_PER_FACTION
					if (i == Structure::Category::Infrastructure)
						continue;
				#endif
				structureGroup[i] =	Command::AddChild(*buildGroup,Structure::Category::Reinterpret(i).ToString());
			}

			


			terraformRemoveGround->Add( new Command::IconCommand("Dig Horizontally", &CommonCmd::ToggleMode<CursorMode::DigMode>
				, &CommonCmd::ModeCheck<CursorMode::DigMode>
				,[](int&mineralCostOut, Resource&costTypeOut, int&timeCostOut)
				{
					costTypeOut = Resource::Minerals;
					mineralCostOut = 0;
					ASSERT__(selectedMobileEntity);
					//PVehicle v = To<Vehicle>::Cast(e);
					timeCostOut = selectedMobileEntity->GetOperationCompletionRounds(VehicleJob::DigHorizontalMineralCosts,Component::TerrainRemove);
				}
				));
			terraformRemoveGround->Add( new Command::IconCommand("Dig Down", &Detail::TerraformerDigDown, 
				&CommonCmd::IdleCheck
				,[](int&mineralCostOut, Resource&costTypeOut, int&timeCostOut)
				{
					costTypeOut = Resource::Minerals;
					mineralCostOut = 0;
					ASSERT__(selectedMobileEntity);
//						PVehicle v = To<Vehicle>::Cast(selectedMobileEntity);
					timeCostOut = selectedMobileEntity->GetOperationCompletionRounds(VehicleJob::DigDownMineralCosts,Component::TerrainRemove);
				}
			));
			terraformRemoveGround->Add( new Command::IconCommand("Clear Above", &CommonCmd::ToggleMode<CursorMode::ClearAboveMode>
				, &CommonCmd::ModeCheck<CursorMode::ClearAboveMode>
				,[](int&mineralCostOut, Resource&costTypeOut, int&timeCostOut)
				{
					costTypeOut = Resource::Minerals;
					mineralCostOut = 0;
					ASSERT__(selectedMobileEntity);
//						PVehicle v = To<Vehicle>::Cast(e);
					timeCostOut = selectedMobileEntity->GetOperationCompletionRounds(VehicleJob::ClearAboveMineralCosts,Component::TerrainRemove);
				}
			));

			terraformAddGround->Add( new Command::IconCommand("Build Wall Up", &Detail::IssueCommand<ConstName::Job_BuildWallUp>
				,&CommonCmd::IdleCheck
				,[](int&mineralCostOut, Resource&costTypeOut, int&timeCostOut)
				{
					costTypeOut = Resource::Minerals;
					mineralCostOut = 0;//NoCosts;//VehicleJob::MinBuildWallMineralCosts;
					ASSERT__(selectedMobileEntity);
					//PVehicle v = To<Vehicle>::Cast(e);
					timeCostOut = 0;//selectedMobileEntity->GetOperationRounds(mineralCostOut,Component::TerrainAdd);
				}
			));
			terraformAddGround->Add( new Command::IconCommand("Build Horizontal Wall", &Detail::IssueCommand<ConstName::Job_BuildHorizontalWall>
				,&CommonCmd::IdleCheck
				,[](int&mineralCostOut, Resource&costTypeOut, int&timeCostOut)
				{
					costTypeOut = Resource::Minerals;
					mineralCostOut = VehicleJob::MinBuildWallMineralCosts;
					ASSERT__(selectedMobileEntity);
					//PVehicle v = To<Vehicle>::Cast(e);
					timeCostOut = selectedMobileEntity->GetOperationCompletionRounds(mineralCostOut,Component::TerrainAdd);
				}
			));

			Command::PBase buildRoad(new Command::IconCommand("Build Road", &Detail::TerraformerBuildRoad
				,&CommonCmd::IdleCheck
				,[](int&mineralCostOut, Resource&costTypeOut, int&timeCostOut)
				{
					costTypeOut = Resource::Minerals;
					mineralCostOut = VehicleJob::MinBuildRoadMineralCosts;
					ASSERT__(selectedMobileEntity);
					//PVehicle v = To<Vehicle>::Cast(e);
					timeCostOut = selectedMobileEntity->GetOperationCompletionRounds(mineralCostOut,Component::TerrainAdd);
				}
			));

			terraformAddGround->Add( buildRoad );
			if (structureGroup[Structure::Category::Infrastructure])
				structureGroup[Structure::Category::Infrastructure]->Add(buildRoad);

			#ifndef ONE_COMPOUND_PER_FACTION
				transferGroup->Add(new IconCommand("Withdraw Resources",&CommonCmd::ToggleMode<CursorMode::RestockMode>, &CommonCmd::ModeCheck<CursorMode::RestockMode>,NoCosts));
				transferGroup->Add(new IconCommand("Deposit Resources",&CommonCmd::ToggleMode<CursorMode::DepositMode>, &CommonCmd::ModeCheck<CursorMode::DepositMode>,NoCosts));

				structureGroup[Structure::Category::Infrastructure]->Add(new IconCommand("Build Connectors to", &CommonCmd::ToggleMode<CursorMode::BuildConnectorChainMode>, &CommonCmd::ModeCheck<CursorMode::BuildConnectorChainMode>,NoCosts));
			#else

			#endif

			Command::PBase buildRoadTo(new Command::IconCommand("Build Road to", &CommonCmd::ToggleMode<CursorMode::BuildRoadChainMode>, &CommonCmd::ModeCheck<CursorMode::BuildRoadChainMode>, NoCosts));
			buildRoadTo->name += " (P)";
			Command::PBase buildBridgeTo( new Command::IconCommand("Build Bridge to", &CommonCmd::ToggleMode<CursorMode::BuildBridgeMode>, &CommonCmd::ModeCheck<CursorMode::BuildBridgeMode>,NoCosts) );

			terraformAddGround->Add( buildRoadTo);
			terraformAddGround->Add( buildBridgeTo);
			if (structureGroup[Structure::Category::Infrastructure])
			{
				structureGroup[Structure::Category::Infrastructure]->Add(buildRoadTo);
				structureGroup[Structure::Category::Infrastructure]->Add(buildBridgeTo);
			}

			terraformRemoveGround->Add( new Command::IconCommand("Dig Tunnel to", &CommonCmd::ToggleMode<CursorMode::DigTunnelMode>, &CommonCmd::ModeCheck<CursorMode::DigTunnelMode>,NoCosts));
			terraformAddGround->Add( new Command::IconCommand("Build Low Wall to", &CommonCmd::ToggleMode<CursorMode::BuildLowWallChainMode>, &CommonCmd::ModeCheck<CursorMode::BuildLowWallChainMode>,NoCosts));
			terraformAddGround->Add( new Command::IconCommand("Build High Wall to", &CommonCmd::ToggleMode<CursorMode::BuildHighWallChainMode>, &CommonCmd::ModeCheck<CursorMode::BuildHighWallChainMode>,NoCosts));


			terraformAddGround->Add( new Command::IconCommand("Build Blocks", &Detail::TerraformerBuildBlock, &CommonCmd::ModeCheck<CursorMode::BuildBlockMode>,NoCosts));
			terraformAddGround->Add( new Command::IconCommand("Commit", &Detail::TerraformerCommitOperation, &Detail::TerraformerCommitCheck
				,[](int&mineralCostOut, Resource&costTypeOut, int&timeCostOut)
				{
					costTypeOut = Resource::Minerals;
					mineralCostOut = (int)cellSelection.Count();
					ASSERT__(selectedMobileEntity);
					//PVehicle v = To<Vehicle>::Cast(e);
					timeCostOut = selectedMobileEntity->GetOperationCompletionRounds(mineralCostOut,activeCursorMode != CursorMode::BuildBlockMode ? Component::TerrainRemove : Component::TerrainAdd);
					if (activeCursorMode != CursorMode::BuildBlockMode)
						mineralCostOut = 0;
				}
			));
			terraformRemoveGround->Add( new Command::IconCommand("Remove Blocks", &Detail::TerraformerRemoveBlock, &CommonCmd::ModeCheck<CursorMode::RemoveBlockMode>,NoCosts));
			terraformRemoveGround->Add( new Command::IconCommand("Commit", &Detail::TerraformerCommitOperation, &Detail::TerraformerCommitCheck
				,[](int&mineralCostOut, Resource&costTypeOut, int&timeCostOut)
				{
					costTypeOut = Resource::Minerals;
					mineralCostOut = (int)cellSelection.Count();
					ASSERT__(selectedMobileEntity);
					//PVehicle v = To<Vehicle>::Cast(e);
					timeCostOut = selectedMobileEntity->GetOperationCompletionRounds(mineralCostOut,activeCursorMode != CursorMode::BuildBlockMode ? Component::TerrainRemove : Component::TerrainAdd);
					if (activeCursorMode != CursorMode::BuildBlockMode)
						mineralCostOut = 0;
				}
			));

				


		


			int at = 0;

			ASSERT__(Prototypes::structures.IsNotEmpty());
			foreach(Prototypes::structures, b)
				if (Component::Terraformer::CanBuild(*b))
				{
					PStructure s = *b;
					String caption = (*b)->GetTypeName();

					Key::Name k = FindTerraformerShortcutKey(*b);
					if (k != Key::Undefined)
						caption += " ("+String(resolveKeyName(k))+")";

					auto cmd = new Command::EntityConstructionCommand(caption, nullptr,nullptr,*b,std::bind(&TerraformerConstruct, *b),
					[s](bool controlsEnabled)
					{
						bool constructingMe = (activeCursorMode == CursorMode::PlaceStructureMode
								&& activeStructureToPlace == s);
						bool unlocked = selectedEntity && s->GetTable()->isUnlocked.GetValue(selectedEntity->GetResearchState());
						Objects::CommandFlags rs = controlsEnabled && 
							(activeCursorMode == CursorMode::Idle
							|| constructingMe
							) && unlocked
							? Objects::CommandFlags::IsEnabled : Objects::CommandFlags::None;
						if (constructingMe)
							rs |= Objects::CommandFlags::IsPermaActive;
						return rs;
					}
					);
					structureGroup[s->GetCategory()]->Add(cmd);
					cmd->RasterizeImage();
				}
		}


		void	DefineBomb(Component::Turret&component)
		{
			component.commands.Add(new Command::IconCommand("Self Detonate", &Detail::IssueDetonationCommand<ConstName::DetonationSize_Regular>
				, &CommonCmd::IdleCheck
				,[](int&resourceCostOut, Resource&costTypeOut, int&timeCostOut)
				{
					ASSERT__(selectedMobileEntity);
					resourceCostOut = Component::Bomb::regularUraniumCost.GetValue(selectedMobileEntity->GetResearchState());
					costTypeOut = Resource::Uranium;
					//PVehicle v = To<Vehicle>::Cast(e);
					timeCostOut = selectedMobileEntity->GetOperationCompletionRounds(resourceCostOut,Component::Nuclear);
				}
				));

			component.commands.Add(new Command::IconCommand("Obliterate", &Detail::IssueDetonationCommand<ConstName::DetonationSize_Doomsday>
				, &CommonCmd::IdleCheck
				,[](int&resourceCostOut, Resource&costTypeOut, int&timeCostOut)
				{
					ASSERT__(selectedMobileEntity);
					resourceCostOut = Component::Bomb::doomsdayUraniumCost.GetValue(selectedMobileEntity->GetResearchState());
					costTypeOut = Resource::Uranium;
					//PVehicle v = To<Vehicle>::Cast(e);
					timeCostOut = selectedMobileEntity->GetOperationCompletionRounds(resourceCostOut,Component::Nuclear);
				}
				));

			#ifndef ONE_COMPOUND_PER_FACTION
				component.commands.Add( new IconCommand("Withdraw Uranium",&CommonCmd::ToggleMode<CursorMode::RestockMode>, &CommonCmd::ModeCheck<CursorMode::RestockMode>,NoCosts));
				component.commands.Add( new IconCommand("Deposit Uranium",&CommonCmd::ToggleMode<CursorMode::DepositMode>, &CommonCmd::ModeCheck<CursorMode::DepositMode>,NoCosts));
			#endif
		}

		void DefineResourceTransfer(Component::Turret&component, Resource res)
		{
			#ifndef ONE_COMPOUND_PER_FACTION
				String resource = component.ammoType.ToString();
				component.commands.Add( new IconCommand("Withdraw "+resource,&CommonCmd::ToggleMode<CursorMode::RestockMode>, &CommonCmd::ModeCheck<CursorMode::RestockMode>,NoCosts));
				component.commands.Add( new IconCommand("Deposit "+resource,&CommonCmd::ToggleMode<CursorMode::DepositMode>, &CommonCmd::ModeCheck<CursorMode::DepositMode>,NoCosts));
			#endif

		}

		void DefineMechanic(Component::Turret&component)
		{
			DefineResourceTransfer(component,Resource::Minerals);
			component.commands.Add( new Command::IconCommand("Repair",&CommonCmd::ToggleMode<CursorMode::RepairMode>, &CommonCmd::ModeCheck<CursorMode::RepairMode>,NoCosts));
		}

		void DefineAutomatable(Component::Turret&component)
		{
			component.commands.Add( new Command::IconCommand("Automate", &VehicleCmd::Automate, &VehicleCmd::AutomatedCheck,NoCosts));
		}

		void DefineHangar()
		{
	
			//Hangar::cmdLeftCoords = int2(4,1) + Command::Base::coordOffset;
			//Hangar::cmdRightCoords = Hangar::cmdLeftCoords + int2(Hangar::MaxVisibleStoreItems+1,0);

			//Hangar::cmdLeft.reset(new Command::IconCommand("Left",
			//	[]()
			//	{
			//		if (selectedEntity && selectedEntity->IsHangar())
			//		{
			//			PHangar h = To<Hangar>::DynamicCast(selectedEntity);
			//			if (h && Hangar::storeOffset > 0)
			//			{
			//				if (Hangar::storeOffset > Hangar::MaxVisibleStoreItems)
			//					Hangar::storeOffset -= Hangar::MaxVisibleStoreItems;
			//				else
			//					Hangar::storeOffset = 0;
			//				Command::RefreshCommands(true);
			//			}
			//		}
			//	},
			//	[](bool controlEnabled)
			//	{
			//		return Hangar::storeOffset > 0 && controlEnabled
			//			 ? Objects::CommandFlags::IsEnabled : Objects::CommandFlags::None;
			//	},
			//	NoCosts));

			//Hangar::cmdRight.reset( new Command::IconCommand("Right",
			//	[]()
			//	{
			//		if (selectedEntity && selectedEntity->IsHangar())
			//		{
			//			PHangar h = To<Hangar>::DynamicCast(selectedEntity);
			//			if (!h)
			//				return;
			//			if (h->GetLoad() <= Hangar::storeOffset)
			//				Hangar::storeOffset = 0;
			//			else
			//			{
			//				count_t remaining = h->GetLoad() - (Hangar::storeOffset);
			//				if (remaining > Hangar::MaxVisibleStoreItems)
			//					Hangar::storeOffset += Hangar::MaxVisibleStoreItems;
			//				//else
			//					//storeOffset = s->CountVehiclesInPark() - MaxVehiclesPerStore;
			//			}
			//			Command::RefreshCommands(true);
			//		}
			//	},
			//	[](bool controlEnabled)
			//	{
			//		if (selectedEntity && selectedEntity->IsHangar())
			//		{
			//			PHangar h = To<Hangar>::DynamicCast(selectedEntity);
			//			return h && h->GetLoad() > Hangar::storeOffset + Hangar::MaxVisibleStoreItems
			//			  && controlEnabled
			//			   ? Objects::CommandFlags::IsEnabled : Objects::CommandFlags::None;
			//		}
			//		return Objects::CommandFlags::None;
			//	},
			//	NoCosts));
		}

					
		
		void DefineItemHandlers()
		{
			Hangar::onStored << [](Hangar&h,Hangar::TStored&item)
			{
				WMobileEntity w = item.entity;
				Command::PVisualCommand cmd(new Command::DeployMobileCommand(w.lock(),
					[w]()
					{
						//focus = To<VehicleStore>::DynamicCast(storeEntity).get();
						if (deployingMobileEntity)
							deployingMobileEntity.reset();
						else
							deployingMobileEntity = w.lock();
					},
					[w](bool controlEnabled)
					{
						Objects::CommandFlags rs = CommonCmd::CommonButtonCheck(controlEnabled);
						if (selectedEntity->IsHangar())
						{
							if (deployingMobileEntity && deployingMobileEntity == w.lock() && deployingMobileEntity->IsAlive())
								rs |= Objects::CommandFlags::IsPermaActive;
						}
						return rs;
					}
					));
				cmd->RasterizeImage();

				item.deployCommand = cmd;
			};
			MobileEntityProducer::onAppendTask << [](MobileEntityProducer&p, MobileEntityProducer::WorkItem&item)
			{
				index_t index = p.queue.Count()-1;
				auto id = item.id;

				std::shared_ptr<Command::EntityConstructionCommand> cmd(
					new Command::EntityConstructionCommand(
						item.queueName,item.base,item.turret,item.prototype,[id,index]()
				{
					Objects::PMobileEntityProducer p = To<Objects::MobileEntityProducer>::DynamicCast(Objects::selectedEntity);
					ASSERT__(p);
					if (!index && p->productionDone)
					{
						WWorldEntity e = selectedEntity;
						HUD::Dialog::Confirm("Confirm cancellation of production job "+p->queue.First().name+". Material has been invested into this production, which will be lost.",
						[e,id]()
						{
							PWorldEntity e1 = e.lock();
							if (e1)
								ApplicationInterface::SignalConstructionAbort(e1->id,id);
						}
						,
						[e,p,id]()->HUD::Dialog::CheckResult
						{
							PWorldEntity e1 = e.lock();
							if (!e1 || e1->IsDeadOrGhost())
								return HUD::Dialog::CheckResult::Abort;
							if (p->queue.IsEmpty() || p->queue.First().id != id)
								return HUD::Dialog::CheckResult::Abort;
							if (!Network::ControlEnabled())
								return HUD::Dialog::CheckResult::OkayNotRightNow;
							return HUD::Dialog::CheckResult::OkayContinue;
						}
						);

					}
					else
						ApplicationInterface::SignalConstructionAbort(selectedEntity->id,id);	//lossless

				},CommonCmd::CommonButtonCheck));

				item.commandToTerminate.second = cmd;

				item.commandToTerminate.first = p.GetCommandOffset() + int2(index,0);
				if (!index)
				{
					cmd->roundsLeft = item.roundsToCompletion;
					cmd->mineralsLeft = item.cost;
					if (index == 0)
						cmd->mineralsLeft -= p.productionDone;
				}
				else
				{
					cmd->roundsLeft = 0xFFFFFFFF;
					cmd->mineralsLeft = 0xFFFFFFFF;
				}

				cmd->RasterizeImage();

			};

		}


		bool					TerraformerCommitOperation()
		{
			if (Detail::TerraformerCommitIsPossibleC(Network::ControlEnabled()))
			{
				Detail::TerraformerCommitOperation();
				return true;
			}
			return false;
		}


		void DefineGlobalCommands()
		{
			using namespace Command;

			globalCommands.Add(new GlobalCommand("End Round",
			[]()
			{
				if (ApplicationInterface::HasSignalledRoundEnd())
					ApplicationInterface::SignalUndoRoundEnd();
				else
					ApplicationInterface::SignalRoundEnd();
			}, 
			[](bool controlEnabled)->Objects::CommandFlags
			{
				Objects::CommandFlags rs = Objects::CommandFlags::None;
				if (ApplicationInterface::CanChangeRoundState())
					rs |= Objects::CommandFlags::IsEnabled;
				if (ApplicationInterface::HasSignalledRoundEnd())
					rs |= Objects::CommandFlags::IsPermaActive;
				else
					if (ApplicationInterface::ShouldSignalRoundEnd())
						rs |= Objects::CommandFlags::IsFlashing;
				return rs;
			}));

			globalCommands.Add(new GlobalCommand("Auto End Round",
			[]()
			{
				if (!RuntimeConfig::autoNewRound && ApplicationInterface::ShouldSignalRoundEnd())
					ApplicationInterface::SignalRoundEnd();
				RuntimeConfig::autoNewRound = !RuntimeConfig::autoNewRound;
			},
			[](bool controlEnabled)->Objects::CommandFlags
			{
				Objects::CommandFlags rs = Objects::CommandFlags::None;
				rs |= Objects::CommandFlags::IsEnabled;
				if (RuntimeConfig::autoNewRound)
					rs |= Objects::CommandFlags::IsPermaActive;
				return rs;
			}));


			viewCommands = AddChild(globalCommands,"View");

			viewCommands->Add( new GlobalCommand("Clip Terrain", &GlobalCmd::ToggleSeeThrough
					,Detail::GlobalPermaSwitch(VisualTerrain::SeeThrough::IsEnabled)
				))->name += " (End)";
			viewCommands->Add( new GlobalCommand("Clip-View Down", &GlobalCmd::SeeThroughDown
					,[](bool)->Objects::CommandFlags{return GlobalCmd::SeeThroughIsActive() ? Objects::CommandFlags::IsEnabled : Objects::CommandFlags::None;}
				))->name += " (Page Down)";
			viewCommands->Add( new GlobalCommand("Clip-View Up", &GlobalCmd::SeeThroughUp
					,[](bool)->Objects::CommandFlags{return GlobalCmd::SeeThroughIsActive() ? Objects::CommandFlags::IsEnabled : Objects::CommandFlags::None;}
				))->name += " (Page Up)";

			viewCommands->Add( new GlobalCommand("Hide Objects", &GlobalCmd::ToggleObjectView
					,Detail::GlobalPermaSwitch([](){return !Objects::objectView;})
				));


			viewCommands->Add( new GlobalCommand("Show Resources", []()
					{
						resourceView = !resourceView;
						VisualTerrain::Invalidate(false);
					}
				#ifdef _DEBUG
					, Detail::GlobalPermaSwitch(resourceView)
				#else
					,Detail::GlobalPermaSwitch(resourceView,[](){return !RuntimeConfig::tutorialMode;})
				#endif
				));

			viewCommands->Add( new GlobalCommand("Show Ranges", []()
					{
						showFriendlyRanges = !showFriendlyRanges;
					}
					, Detail::GlobalPermaSwitch(showFriendlyRanges)
				));
			viewCommands->Add( new GlobalCommand("Show Hostile Ranges", []()
					{
						showHostileRanges = !showHostileRanges;
					}
					,Detail::GlobalPermaSwitch(showHostileRanges)
				));
			viewCommands->Add( new GlobalCommand("Show Sensor Ranges", []()
					{
						showSensorRanges = !showSensorRanges;
					}
					, Detail::GlobalPermaSwitch(showSensorRanges)
				));
			viewCommands->Add( new GlobalCommand("Show Mobile Reach", []()
					{
						VehicleAccessibilityMesh::visible = !VehicleAccessibilityMesh::visible;
					}
					, Detail::GlobalPermaSwitch(VehicleAccessibilityMesh::visible)
				));

			viewCommands->Add(new GlobalCommand("Show Sky Grid",
					&Camera::ToggleOrbit, &Detail::SubOrbitalCmd::CheckGridVisibility
			))->name += " (Home)";

		}
		
		void RebuildTerraformerButtons()
		{
			Component::terraformer.commands.Clear();
			if (Objects::Research::globalState.allTopics.IsNotEmpty())
				ASSERT__(Component::terraformer.CanConstruct());
			DefineTerraformer(Component::terraformer);
			if (Objects::selectedEntity && Objects::selectedEntity->IsVehicle() && To<Vehicle>::Cast(Objects::selectedEntity)->turret == &Component::terraformer)
				HUD::HexGrid::Configure(Objects::selectedEntity);
		}

		
		void	VisitTerraformerShortcuts(const std::function<void(Key::Name,const Objects::PStructure&)>&visitor)
		{
			visitor(Key::M,Objects::Prototypes::structures.Find(ConstName::Type_Mine));
			visitor(Key::G,Objects::Prototypes::structures.Find(ConstName::Type_Generator));
			visitor(Key::F,Objects::Prototypes::structures.Find(ConstName::Type_VehiclePlant));
			visitor(Key::V,Objects::Prototypes::structures.Find(ConstName::Type_VehiclePlant));
			visitor(Key::R,Objects::Prototypes::structures.Find(Objects::EntityType(ConstName::Type_Tower,ConstName::Turret_Radar)));
			visitor(Key::L,Objects::Prototypes::structures.Find(ConstName::Type_Laboratory));
		}

		Key::Name	FindTerraformerShortcutKey(const Objects::PStructure&proto)
		{
			Key::Name rs = Key::Undefined;
			VisitTerraformerShortcuts([&rs, proto](Key::Name k, const Objects::PStructure&s)
			{
				if (s == proto)
					rs = k;
			});
			return rs;
		}



		void	DefineAll()
		{
			DefineItemHandlers();
			Research::globalState.onScriptChange << ReconfigureResearchButtons << RebuildTerraformerButtons;
			Research::globalState.onQueueChange << [](Faction*f){ if (f == Faction::mine.get()) ReconfigureResearchQueueButtons();};

			DefineMine(Objects::Mine::commands);
			DefineVehiclePlant(Objects::VehiclePlant::commands);
			DefinePlaneFactory(Objects::PlaneFactory::commands);
			DefineWorkingStructure(Objects::MineralContainer::commands);
			DefineWorkingStructure(Objects::FuelContainer::commands);
			DefineWorkingStructure(Objects::UraniumStorage::commands);
			DefineWorkingStructure(Objects::Generator::commands);
			DefineWorkingStructure(Objects::ReactorCore::commands);
			DefineWorkingStructure(Objects::NexusPort::commands);
			DefineLaboratory(Objects::Laboratory::commands);
			DefineHangar();

			DefineTerraformer(Component::terraformer);
			DefineBomb(Component::bomb);
			DefineOffensive(Component::artillery);
			DefineOffensive(Component::launcher);
			DefineOffensive(Component::armored);
			DefineOffensive(Component::antiAircraft);
			DefineOffensive(Component::emp);

			#ifndef NO_MUNITION
				DefineResourceTransfer(Component::munitionTransporter,Resource::Munitions);
			#endif
			DefineMechanic(Component::mechanic);
			DefineAutomatable(Component::resourceScanner);

			DefineRaptor(Component::SubOrbital::raptor);
			DefineBomber(Component::SubOrbital::bomber);
			DefineSensorDrone(Component::SubOrbital::sensorDrone);
			DefineCargoPlane(Component::SubOrbital::cargoPlane);


			foreach (Objects::Component::vehicleTurretComponents,tc)
				(*tc)->commands.VisitLocalMembers([](const Command::PBase&cmd){ To<Command::VisualCommand>::Cast(cmd)->RasterizeImage();});
			foreach (Objects::Component::baseComponents,bc)
				(*bc)->commands.VisitLocalMembers([](const Command::PBase&cmd){To<Command::VisualCommand>::Cast(cmd)->RasterizeImage();});


			foreach (Objects::Tower::globalPrototypes,p)
			{
				DefineTower(**p);
			}

			DefineGlobalCommands();

		}

	}





}

#include <global_root.h>
#include "commandDefinitions.h"
#include "objectCommands.h"
#include <objects/structure.h>
#include "objectSelection.h"
#include <objects/research.h>
#include <objects/gridAndChoices.h>
#include <objects/vehicle.h>
#include <objects/structure/laboratory.h>
#include <objects/structure/mine.h>
#include <objects/structure/vehiclePlant.h>
#include "dialog.h"
#include "objectChoices.h"


namespace Objects
{
	namespace Commands
	{
		namespace Detail
		{
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
					Objects::GetProductionChoices(&plant,plant.selectedBase,choices);
					static bool ignoreWarnings = false;
					if (choices.IsNotEmpty() || ignoreWarnings)
						ApplicationInterface::SignalConstructionBegin(plant.id, plant.selectedBase->constName, plant.selectedTurret->constName);
					else
					{


						#ifdef HAS_WINDOW					
							String waterNotice;
							if (!plant.selectedBase->moveOnWater)
								waterNotice = " Note that this vehicle type cannot stand on water. ";
							TUniqueID myID = plant.id;
							ConstName baseName = plant.selectedBase->constName;
							ConstName turretName = plant.selectedTurret->constName;
							Dialog::Confirm("Cannot find valid spawning locations around '"+plant.GetNiceName()+"' for '"+Vehicle::GetTypeName(plant.selectedBase,plant.selectedTurret)+"'. "+waterNotice+"Are you sure you wish to start this production?",
									[baseName,turretName,myID]()
									{
										ApplicationInterface::SignalConstructionBegin(myID, baseName, turretName);
										//ignoreWarnings = true;
									}
									,
									[myID]()->Dialog::CheckResult
									{
										PVehiclePlant plant = To<VehiclePlant>::DynamicCast(GetEntity(myID));
										if (!plant || plant->health <= 0)
											return Dialog::CheckResult::Abort;
										if (!Objects::controlIsEnabled)
											return Dialog::CheckResult::OkayNotRightNow;
										return Dialog::CheckResult::OkayContinue;
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
			commands.VisitMembers([](const int2&coords,const PBaseCommand&cmd)
			{
				if (cmd->name == "Enable")
				{
					(*cmd) = IconCommand("Enable",
						[]()
						{

							VehiclePlant*s2 = dynamic_cast<VehiclePlant*>(selectedEntity.get());
							if (s2)
							{
								if (!s2->IsEnabled() && !s2->CanEnable())
								{
									ApplicationInterface::SignalUserMessage(s2->GetNiceName()+" is switched off because its queue is empty. It will attempt to automatically enable once you enqueue at least one production job using the 'Construct Selected' button. You must first select both a foundation and a turret, however.");
									return;
								}
								ApplicationInterface::SignalEnableStructure(*s2, !s2->IsEnabled());
							}
						}
						,[](bool controlEnabled)
						{
							BaseCommand::Flags rs = BaseCommand::Flags::None;
							VehiclePlant*div = dynamic_cast<VehiclePlant*>(selectedEntity.get());
							if (controlEnabled && div != nullptr)
								rs |= BaseCommand::Flags::IsEnabled;
							if (Detail::WorkingStructureIsEnabled())
								rs |= BaseCommand::Flags::IsPermaActive;
							return rs;
						}
						,NoCosts
					);
				}
			},true);


			commands.Add(int2(4,0),false, new IconCommand("Construct Selected",[]()
				{
					VehiclePlant*plant = dynamic_cast<VehiclePlant*>(selectedEntity.get());
					if (plant)
					{
						if (plant->selectedBase && plant->selectedTurret)
							plant->EnqueueSelectedConstruction();
						else
						{
							if (plant->selectedBase)
								ApplicationInterface::SignalUserMessage(plant->GetNiceName()+" needs to know what turret you want to use in order to enqueue a new production job. Please select one in the top row.");
							elif (plant->selectedTurret)
								ApplicationInterface::SignalUserMessage(plant->GetNiceName()+" needs to know what foundation you want to use in order to enqueue a new production job. Please select one in the bottom row.");
							else
								ApplicationInterface::SignalUserMessage(plant->GetNiceName()+" needs to know what turret and foundation you want to use in order to enqueue a new production job. Please select one in each top and bottom rows.");
						}
					}
				},
				[](bool controlEnabled)
				{
					if (!controlEnabled)
						return BaseCommand::Flags::None;
					VehiclePlant*plant = dynamic_cast<VehiclePlant*>(selectedEntity.get());
					if (plant)
					{
						BaseCommand::Flags rs = BaseCommand::Flags::IsEnabled;
						if (plant->selectedBase && plant->selectedTurret && plant->queue.IsEmpty() /*&& !plant->IsSleeping()*/)
							rs |= BaseCommand::Flags::IsFlashing | BaseCommand::Flags::SoftFlashing;

						return rs;
					}
					else return BaseCommand::Flags::None;
				},
				[](int&mineralCostOut, Resource&costTypeOut, int&timeCostOut)
				{
					costTypeOut = Resource::Minerals;
					PVehiclePlant p = To<VehiclePlant>::DynamicCast(selectedEntity);
					ASSERT__(p);
					if (!p->selectedBase || !p->selectedTurret)
					{
						mineralCostOut = 0;
						timeCostOut = 0;
						return;
					}
					mineralCostOut = Vehicle::GetPrice(p->selectedBase,p->selectedTurret,p->owner);
					timeCostOut = p->ProductionRounds(mineralCostOut,false);
				}
			));


			int at = 0;
			//vehicleConstructionCommands << VehicleConstructionCommand("Base:",[](const PWorldEntity&){},AlwaysFalse<PWorldEntity const&>,AlwaysFalse<PWorldEntity const&>);
			foreach (VehiclePlant::baseComponents,comp)
			{
				Component::VehicleBase* c = *comp;
				commands.Add(int2(5+at,0),false,new EntityConstructionCommand(c->typeName,c,nullptr,PWorldEntity(),[c]()
				{
					VehiclePlant*plant = dynamic_cast<VehiclePlant*>(selectedEntity.get());
					if (plant)
						plant->selectedBase = c;
				}
				,[c](bool controlEnabled)
				{
					BaseCommand::Flags rs = CommonCmd::CommonButtonCheck(controlEnabled);
					VehiclePlant*plant = dynamic_cast<VehiclePlant*>(selectedEntity.get());
					if (plant && plant->selectedBase == c)
						rs |= BaseCommand::Flags::IsPermaActive;
					if (!plant || !c->isUnlocked(plant->owner))
						rs = BaseCommand::Flags::None;
					return rs;
				}));
				To<Objects::VisualCommand>::Cast(commands.GetLast().second)->RasterizeImage();
				at++;
			}
			at = 0;
			//commands << Command("Turret:",[](const PWorldEntity&){},AlwaysFalse<PWorldEntity const&>,AlwaysFalse<PWorldEntity const&>);
			foreach (VehiclePlant::turretComponents,comp)
			{
				Component::Turret* c = *comp;
				commands.Add(int2(3+at,1),false,new EntityConstructionCommand(c->typeName,nullptr,c,PWorldEntity(),[c]()
				{
					VehiclePlant*plant = dynamic_cast<VehiclePlant*>(selectedEntity.get());
					if (plant)
						plant->selectedTurret = c;
				}
				,[c](bool controlEnabled)
				{
					BaseCommand::Flags rs = CommonCmd::CommonButtonCheck(controlEnabled);
					VehiclePlant*plant = dynamic_cast<VehiclePlant*>(selectedEntity.get());
					if (plant)
					{
						if (plant->selectedTurret == c)
							rs |= BaseCommand::Flags::IsPermaActive;
						elif (!plant->selectedTurret)
							rs |= BaseCommand::Flags::IsFlashing | BaseCommand::Flags::SoftFlashing;
					}
					if (!plant || !c->isUnlocked(plant->owner))
						rs = BaseCommand::Flags::None;
					return rs;
				}));
				To<Objects::VisualCommand>::Cast(commands.GetLast().second)->RasterizeImage();
				at++;
			}
		
		}

		void DefineMine(CommandContainer&commands)
		{
			DefineWorkingStructure(commands);
			commands.Add( new IconCommand("Restore Core Link",[]()
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
					return BaseCommand::Flags::None;
				PMine mine = ToMine(selectedEntity);
				return mine && !mine->IsLinkedToCore() ? BaseCommand::Flags::IsEnabled : BaseCommand::Flags::None;
			},
			NoCosts
			));
		}

		void DefineWorkingStructure(CommandContainer & commands)
		{
			commands.baseCoords = int2(3,0);

			commands.Add(new IconCommand("Enable",&Detail::EnableWorkingStructure,
			[](bool controlEnabled)->BaseCommand::Flags
			{
				BaseCommand::Flags rs = BaseCommand::Flags::None;
				Objects::WorkingStructure*div = dynamic_cast<Objects::WorkingStructure*>(selectedEntity.get());
				if (controlEnabled)
				{
					if (div && div->CanEnable())
					{
						rs |= BaseCommand::Flags::IsEnabled;
					}
				}
				if (div && div->IsEnabled())
					rs |= BaseCommand::Flags::IsPermaActive;
				return rs;
			}
			,NoCosts
			));
		}

		static void BuildCommand(CommandContainer&parent, const PResearchTopic&t)
		{
			PResearchCommand rs(new ResearchCommand(t));
			parent.Add(rs);
			foreach (t->children,c)
			{
				BuildCommand(*rs,*c);
			}
			if (t->componentReference || t->prototypeReference)
				rs->RasterizeImage();
		}

		static count_t			researchCommandOffset = InvalidIndex;

		static	void	ConfigureResearchButtons(CommandContainer&commands)
		{
			if (researchCommandOffset == InvalidIndex)
				researchCommandOffset = commands.Count();
			else
				commands.Truncate(researchCommandOffset);
			foreach (ResearchState::globalState.rootResearch,res)
			{
				BuildCommand(commands,*res);
			}
		}
		void	ReconfigureResearchButtons()
		{
			ConfigureResearchButtons(Laboratory::commands);
		}

		void	DefineLaboratory(CommandContainer&commands)
		{
			DefineWorkingStructure(commands);
			ConfigureResearchButtons(commands);
		}


		namespace Detail
		{
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


			void ConfigureStandardOffset(Objects::Component::Turret&component)
			{
				component.commands.baseCoords = int2(3,0);
			}


			bool	NotModifyBlockMode(const Vehicle&v)
			{
				return activeCursorMode != CursorMode::BuildBlockMode && activeCursorMode != CursorMode::RemoveBlockMode;
			}

			/*static*/ bool TerraformerNotBuilding()
			{
				PMobileEntity v = selectedMobileEntity;
				return v && (!v->HasTasks() || !v->myTasks.Peek().IsJob());
			}

			/*static*/ void TerraformerConstruct(const PStructure&p)
			{
				PMobileEntity vehicle = selectedMobileEntity;
				if (!vehicle)
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
						showArrow = true;
						arrowFromCell = vehicle->GetLastPointOfOrigin();
						arrowFrom = Grid::low.cells[arrowFromCell.stackID].centerVertex.xyz * ( Grid::ToRadius(arrowFromCell.height));

						if (!Engine::input.pressed[Key::Shift])
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
					showArrow = false;
				}


				//SignalNewJob(vehicle, ConstName::Job_Construct, p->GetCTypeName());
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

			BaseCommand::Flags TerraformerCommitCheck(bool controlEnabled)
			{
				return TerraformerCommitIsPossibleC(controlEnabled) ? BaseCommand::Flags::IsEnabled : BaseCommand::Flags::None;
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
				ApplicationInterface::SignalNewJob(vehicle, ConstName::Job_DigDown, ConstName::Empty);
			}

			template <ConstName Command>
				void	IssueCommand()
				{
					if (!selectedMobileEntity)
						return;
					ApplicationInterface::SignalNewJob(selectedMobileEntity, Command,ConstName::Empty);					
				}



			void					TerraformerBuildRoad()
			{
				PVehicle vehicle = To<Vehicle>::DynamicCast(selectedMobileEntity);
				if (!vehicle)
					return;
				if (vehicle->location.height <= Grid::CellStack::BottomHeight)
					return;
				ApplicationInterface::SignalNewJob(vehicle, ConstName::Job_BuildRoad,ConstName::Empty);
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
							Grid::low.grid.GetCell(v->GetLastPointOfOrigin().stackID).higherEquivalent
							, 8, Objects::vehicleDistanceField);
						#else
							v->GetLastPointOfOrigin().stackID

							, 4, Objects::vehicleDistanceField);
						#endif
					for (UINT32 i = 0; i < (UINT32)Grid::high.grid.CellCount; i++)
					{
						if (Objects::vehicleDistanceField.distance[i] <= 3)
						{
							UINT32 at = v->location.height + Grid::CellStack::HalfRange;
							UINT32 min = std::max(at,2U)-1;
							for (UINT32 j = min; j <= at + 1; j++)
							{
								if (World::actual.cellStack[i].volumeCell[j].IsSolid())
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
				const int height = v->GetLastPointOfOrigin().height;

				if (activeCursorMode == CursorMode::BuildBlockMode)
				{
					ASSERT__(v);
					Grid::high.UpdatePlanarDistanceField(
						#ifdef HYBRID_GRID
							Grid::low.grid.GetCell(
								v->GetLastPointOfOrigin().stackID).higherEquivalent
							, 8,
						#else
							v->GetLastPointOfOrigin().stackID

							, 4,
						#endif
						 Objects::vehicleDistanceField);
					for (UINT32 i = 0; i < (UINT32)Grid::high.grid.CellCount; i++)
					{
						if (Objects::vehicleDistanceField.distance[i] <= 3)
						{
							UINT32 at = height + Grid::CellStack::HalfRange;
							for (UINT32 j = at - 1; j <= at + 1; j++)
							{
								if (!World::actual.cellStack[i].volumeCell[j].IsOccupied())
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

		void	DefineTerraformer(Objects::Component::Turret&component)
		{
			Detail::ConfigureStandardOffset(component);


			PCommandGroup	buildGroup = component.commands.AddChild("Build"),
							terraformGroup = component.commands.AddChild("Terraform"),
							terraformRemoveGround = terraformGroup->AddChild("Remove Ground"),
							terraformAddGround = terraformGroup->AddChild("Add Ground"),
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
				structureGroup[i] =	buildGroup->AddChild(Structure::Category::Reinterpret(i).ToString());
			}

			


			terraformRemoveGround->Add( new IconCommand("Dig Horizontally", &CommonCmd::ToggleMode<CursorMode::DigMode>
				, &CommonCmd::ModeCheck<CursorMode::DigMode>
				,[](int&mineralCostOut, Resource&costTypeOut, int&timeCostOut)
				{
					costTypeOut = Resource::Minerals;
					mineralCostOut = 0;
					ASSERT__(selectedMobileEntity);
					//PVehicle v = To<Vehicle>::Cast(e);
					timeCostOut = selectedMobileEntity->GetOperationRounds(VehicleJob::DigHorizontalMineralCosts,Component::TerrainRemove);
				}
				));
			terraformRemoveGround->Add( new IconCommand("Dig Down", &Detail::TerraformerDigDown, 
				&CommonCmd::IdleCheck
				,[](int&mineralCostOut, Resource&costTypeOut, int&timeCostOut)
				{
					costTypeOut = Resource::Minerals;
					mineralCostOut = 0;
					ASSERT__(selectedMobileEntity);
//						PVehicle v = To<Vehicle>::Cast(selectedMobileEntity);
					timeCostOut = selectedMobileEntity->GetOperationRounds(VehicleJob::DigDownMineralCosts,Component::TerrainRemove);
				}
			));
			terraformRemoveGround->Add( new IconCommand("Clear Above", &CommonCmd::ToggleMode<CursorMode::ClearAboveMode>
				, &CommonCmd::ModeCheck<CursorMode::ClearAboveMode>
				,[](int&mineralCostOut, Resource&costTypeOut, int&timeCostOut)
				{
					costTypeOut = Resource::Minerals;
					mineralCostOut = 0;
					ASSERT__(selectedMobileEntity);
//						PVehicle v = To<Vehicle>::Cast(e);
					timeCostOut = selectedMobileEntity->GetOperationRounds(VehicleJob::ClearAboveMineralCosts,Component::TerrainRemove);
				}
			));

			terraformAddGround->Add( new IconCommand("Build Wall Up", &Detail::IssueCommand<ConstName::Job_BuildWallUp>
				,&CommonCmd::IdleCheck
				,[](int&mineralCostOut, Resource&costTypeOut, int&timeCostOut)
				{
					costTypeOut = Resource::Minerals;
					mineralCostOut = VehicleJob::BuildWallMineralCosts;
					ASSERT__(selectedMobileEntity);
					//PVehicle v = To<Vehicle>::Cast(e);
					timeCostOut = selectedMobileEntity->GetOperationRounds(mineralCostOut,Component::TerrainAdd);
				}
			));
			terraformAddGround->Add( new IconCommand("Build Horizontal Wall", &Detail::IssueCommand<ConstName::Job_BuildHorizontalWall>
				,&CommonCmd::IdleCheck
				,[](int&mineralCostOut, Resource&costTypeOut, int&timeCostOut)
				{
					costTypeOut = Resource::Minerals;
					mineralCostOut = VehicleJob::BuildWallMineralCosts;
					ASSERT__(selectedMobileEntity);
					//PVehicle v = To<Vehicle>::Cast(e);
					timeCostOut = selectedMobileEntity->GetOperationRounds(mineralCostOut,Component::TerrainAdd);
				}
			));

			PBaseCommand buildRoad(new IconCommand("Build Road", &Detail::TerraformerBuildRoad
				,&CommonCmd::IdleCheck
				,[](int&mineralCostOut, Resource&costTypeOut, int&timeCostOut)
				{
					costTypeOut = Resource::Minerals;
					mineralCostOut = VehicleJob::BuildRoadMineralCosts;
					ASSERT__(selectedMobileEntity);
					//PVehicle v = To<Vehicle>::Cast(e);
					timeCostOut = selectedMobileEntity->GetOperationRounds(mineralCostOut,Component::TerrainAdd);
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

			PBaseCommand buildRoadTo(new IconCommand("Build Road to", &CommonCmd::ToggleMode<CursorMode::BuildRoadChainMode>, &CommonCmd::ModeCheck<CursorMode::BuildRoadChainMode>, NoCosts));
			PBaseCommand buildBridgeTo( new IconCommand("Build Bridge to", &CommonCmd::ToggleMode<CursorMode::BuildBridgeMode>, &CommonCmd::ModeCheck<CursorMode::BuildBridgeMode>,NoCosts) );

			terraformAddGround->Add( buildRoadTo);
			terraformAddGround->Add( buildBridgeTo);
			if (structureGroup[Structure::Category::Infrastructure])
			{
				structureGroup[Structure::Category::Infrastructure]->Add(buildRoadTo);
				structureGroup[Structure::Category::Infrastructure]->Add(buildBridgeTo);
			}

			terraformRemoveGround->Add( new IconCommand("Dig Tunnel to", &CommonCmd::ToggleMode<CursorMode::DigTunnelMode>, &CommonCmd::ModeCheck<CursorMode::DigTunnelMode>,NoCosts));
			terraformAddGround->Add( new IconCommand("Build Wall to", &CommonCmd::ToggleMode<CursorMode::BuildWallChainMode>, &CommonCmd::ModeCheck<CursorMode::BuildWallChainMode>,NoCosts));


			terraformAddGround->Add( new IconCommand("Build Blocks", &Detail::TerraformerBuildBlock, &CommonCmd::ModeCheck<CursorMode::BuildBlockMode>,NoCosts));
			terraformAddGround->Add( new IconCommand("Commit", &Detail::TerraformerCommitOperation, &Detail::TerraformerCommitCheck
				,[](int&mineralCostOut, Resource&costTypeOut, int&timeCostOut)
				{
					costTypeOut = Resource::Minerals;
					mineralCostOut = (int)cellSelection.Count();
					ASSERT__(selectedMobileEntity);
					//PVehicle v = To<Vehicle>::Cast(e);
					timeCostOut = selectedMobileEntity->GetOperationRounds(mineralCostOut,activeCursorMode != CursorMode::BuildBlockMode ? Component::TerrainRemove : Component::TerrainAdd);
					if (activeCursorMode != CursorMode::BuildBlockMode)
						mineralCostOut = 0;
				}
			));
			terraformRemoveGround->Add( new IconCommand("Remove Blocks", &Detail::TerraformerRemoveBlock, &CommonCmd::ModeCheck<CursorMode::RemoveBlockMode>,NoCosts));
			terraformRemoveGround->Add( new IconCommand("Commit", &Detail::TerraformerCommitOperation, &Detail::TerraformerCommitCheck
				,[](int&mineralCostOut, Resource&costTypeOut, int&timeCostOut)
				{
					costTypeOut = Resource::Minerals;
					mineralCostOut = (int)cellSelection.Count();
					ASSERT__(selectedMobileEntity);
					//PVehicle v = To<Vehicle>::Cast(e);
					timeCostOut = selectedMobileEntity->GetOperationRounds(mineralCostOut,activeCursorMode != CursorMode::BuildBlockMode ? Component::TerrainRemove : Component::TerrainAdd);
					if (activeCursorMode != CursorMode::BuildBlockMode)
						mineralCostOut = 0;
				}
			));

				


		


			int at = 0;

			foreach(Component::Terraformer::canBuild, b)
			{
				PStructure s = *b;
				structureGroup[s->GetCategory()]->Add(new EntityConstructionCommand(/*"Build " + */(*b)->GetTypeName(), nullptr,nullptr,*b,std::bind(&Detail::TerraformerConstruct, *b),
				[s](bool controlsEnabled)
				{
					bool constructingMe = (activeCursorMode == CursorMode::PlaceStructureMode
							&& activeStructureToPlace == s);
					bool unlocked = selectedEntity && s->GetTable()->isUnlocked.GetValue(selectedEntity->GetResearchState());
					BaseCommand::Flags rs = controlsEnabled && 
						(activeCursorMode == CursorMode::Idle
						|| constructingMe
						) && unlocked
						? BaseCommand::Flags::IsEnabled : BaseCommand::Flags::None;
					if (constructingMe)
						rs |= BaseCommand::Flags::IsPermaActive;
					return rs;
				}
				));
			}
		}


		void	DefineBomb(Component::Turret&component)
		{
			Detail::ConfigureStandardOffset(component);

			component.commands.Add( new IconCommand("Self Detonate", &Detail::IssueCommand<ConstName::DetonationSize_Regular>
				, &CommonCmd::IdleCheck
				,[](int&resourceCostOut, Resource&costTypeOut, int&timeCostOut)
				{
					resourceCostOut = 0;
					ASSERT__(selectedMobileEntity);
					//PVehicle v = To<Vehicle>::Cast(e);
					timeCostOut = selectedMobileEntity->GetOperationRounds(VehicleJob::RegularSelfDetonationCosts,Component::Nuclear);
				}
				));

			component.commands.Add( new IconCommand("Obliterate", &Detail::IssueCommand<ConstName::DetonationSize_Doomsday>
				, &CommonCmd::IdleCheck
				,[](int&resourceCostOut, Resource&costTypeOut, int&timeCostOut)
				{
					resourceCostOut = VehicleJob::DoomsdaySelfDetonationCosts;
					costTypeOut = Resource::Uranium;
					ASSERT__(selectedMobileEntity);
					//PVehicle v = To<Vehicle>::Cast(e);
					timeCostOut = selectedMobileEntity->GetOperationRounds(VehicleJob::DoomsdaySelfDetonationCosts,Component::Nuclear);
				}
				));

			#ifndef ONE_COMPOUND_PER_FACTION
				component.commands.Add( new IconCommand("Withdraw Uranium",&CommonCmd::ToggleMode<CursorMode::RestockMode>, &CommonCmd::ModeCheck<CursorMode::RestockMode>,NoCosts));
				component.commands.Add( new IconCommand("Deposit Uranium",&CommonCmd::ToggleMode<CursorMode::DepositMode>, &CommonCmd::ModeCheck<CursorMode::DepositMode>,NoCosts));
			#endif
		}

		void DefineResourceTransfer(Component::Turret&component, Resource res)
		{
			Detail::ConfigureStandardOffset(component);
			#ifndef ONE_COMPOUND_PER_FACTION
				String resource = component.ammoType.ToString();
				component.commands.Add( new IconCommand("Withdraw "+resource,&CommonCmd::ToggleMode<CursorMode::RestockMode>, &CommonCmd::ModeCheck<CursorMode::RestockMode>,NoCosts));
				component.commands.Add( new IconCommand("Deposit "+resource,&CommonCmd::ToggleMode<CursorMode::DepositMode>, &CommonCmd::ModeCheck<CursorMode::DepositMode>,NoCosts));
			#endif

		}

		void DefineOffensive(Component::Turret&component)
		{
			Detail::ConfigureStandardOffset(component);
			#ifndef NO_MUNITION
				DefineResourceTransfer(component,component.ammoType);
			#endif
	
			component.commands.Add( new IconCommand("Attack",&CommonCmd::ToggleMode<CursorMode::AttackMode>, &CommonCmd::ModeCheck<CursorMode::AttackMode>,NoCosts));
		}

		void DefineMechanic(Component::Turret&component)
		{
			DefineResourceTransfer(component,Resource::Minerals);
			component.commands.Add( new IconCommand("Repair",&CommonCmd::ToggleMode<CursorMode::RepairMode>, &CommonCmd::ModeCheck<CursorMode::RepairMode>,NoCosts));
		}

		void DefineAutomatable(Component::Turret&component)
		{
			Detail::ConfigureStandardOffset(component);

			component.commands.Add( new IconCommand("Automate", &VehicleCmd::Automate, &VehicleCmd::AutomatedCheck,NoCosts));
		}

		void	DefineAll()
		{
			DefineMine(Objects::Mine::commands);
			DefineVehiclePlant(Objects::VehiclePlant::commands);
			DefineWorkingStructure(Objects::MineralContainer::commands);
			DefineWorkingStructure(Objects::FuelContainer::commands);
			DefineWorkingStructure(Objects::UraniumStorage::commands);
			DefineLaboratory(Objects::Laboratory::commands);

			DefineTerraformer(Component::terraformer);
			DefineBomb(Component::bomb);
			DefineOffensive(Component::artillery);
			DefineOffensive(Component::launcher);
			DefineOffensive(Component::armored);
			DefineOffensive(Component::emp);

			#ifndef NO_MUNITION
				DefineResourceTransfer(Component::munitionTransporter,Resource::Munitions);
			#endif
			DefineMechanic(Component::mechanic);
			DefineAutomatable(Component::resourceScanner);


		}

	}
}

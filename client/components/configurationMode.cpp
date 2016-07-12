#include <global_root.h>
#include "configurationMode.h"
#include "world.h"
#include "ui.h"
#include <objects/visualCommands.h>
#include "network.h"
#include "objectStorage.h"

namespace ConfigurationMode
{
	namespace Detail
	{
		Objects::Storage serverObjects;
		bool isActive = false;
		Grid::ExtendedPlanetConfig	config, refConfig;
		UINT32 seed = 0;
		Protocol::GameConfig	gameConfig;
		Random rng;

		class GridState
		{
		public:
			Grid::WorldData	copy;
			static GridState	*current;
			bool			isLoaded = false;

			bool			IsLoadedOrActive() const
			{
				return isLoaded || current == this;
			}
			bool			IsActive() const
			{
				return current == this;
			}

			void			Import()
			{
				ASSERT__(!isLoaded);
				ASSERT__(!current || current == this);
				isLoaded = true;
				copy.swap(Grid::actualWorld);
				current = nullptr;
				Grid::actualWorld.lightingOutdated = true;
				Grid::SetAllFamiliesOutdated();	//force rebuild
			};

			void			Export()
			{
				ASSERT__(isLoaded);
				ASSERT_IS_NULL__(current);
				//ASSERT__(copy.cellStack.first().volumeCell[0].content != Grid::Content::Clear);
				isLoaded = false;
				copy.swap(Grid::actualWorld);
				current = this;
				Grid::actualWorld.lightingOutdated = true;
				Grid::SetAllFamiliesOutdated();	//force rebuild
			}

		};
		/*static*/ GridState	*GridState::current = nullptr;

		GridState serverWorld[Grid::WorldDiscovery::N], clientWorld[Grid::WorldDiscovery::N];
		bool created = false,
				flashUploadButton = false;
		bool clientWorldLoaded = false, clientWorldBuilt = false, anyWorldLoaded = false;
		Objects::PCommandGroup mountainGroup,extendedCoreGroup,liquidGroup,coreGroup,ruleGroup;
		Buffer0<std::pair<int2,Objects::PBaseCommand> > buttons; 

		class MaterialDialog
		{
		public:
			GUIElements::PWindow	window;
			std::function<void(BYTE)>	onSelect;
			
			void				Create(bool allowUnknown, bool allowLiquids);
			void				SetCurrent(BYTE type);
		};

		MaterialDialog		fullScope, noUnknown, noLiquids;


		void RefreshButtonStates()
		{
			foreach (buttons,b)
				b->second->Refresh(true);
		}

		bool	showDiscovery = false;


		void ActivateDiscovery(bool forceRebuild, GridState*range)
		{
			if (GridState::current)
				GridState::current->Import();
			GridState&st = range[Grid::WorldDiscovery::FullTerrain];
			if (gameConfig.worldDiscovery == (BYTE)Grid::WorldDiscovery::FullTerrain || !showDiscovery)
				st.Export();
			else
			{
				//ASSERT__(st.copy.cellStack.first().volumeCell[0].content != Grid::Content::Clear);
				GridState&st2 = range[Grid::WorldDiscovery::Reinterpret(gameConfig.worldDiscovery)];
				if (forceRebuild || !st2.IsLoadedOrActive())
				{
					//ASSERT__(st.copy.cellStack.first().volumeCell[0].content != Grid::Content::Clear);
					st.copy.ExportDiscovered(st2.copy,Grid::WorldDiscovery::Reinterpret(gameConfig.worldDiscovery));
					//ASSERT__(st.copy.cellStack.first().volumeCell[0].content != Grid::Content::Clear);
					//ASSERT__(st2.copy.cellStack.first().volumeCell[0].content != Grid::Content::Clear);
					st2.isLoaded = true;
				}
				st2.Export();
			}
			Grid::SetAllFamiliesOutdated();	//force rebuild
			World::allowRecompilation = true;
			RefreshButtonStates();
		}

		void Update(bool forceRebuild = true)
		{
			config.SetSeed(seed);

			if (isActive)
			{
				//Grid::actualWorld.FlushAllData();
				if (serverObjects.IsEmpty())
					serverObjects.Import(true);
				forceRebuild |= !clientWorldBuilt;
				clientWorldBuilt = true;
				if (GridState::current)
					GridState::current->Import();
				clientWorldLoaded = true;
				anyWorldLoaded = true;
				GridState&st = clientWorld[Grid::WorldDiscovery::FullTerrain];
				if (forceRebuild)
				{
					st.isLoaded = true;
					st.copy.BuildWorld(config);
					st.copy.OverrideVisibility(1);
//					ASSERT__(st.copy.cellStack.first().volumeCell[0].content != Grid::Content::Clear);
				}
				ActivateDiscovery(forceRebuild, clientWorld);
			}
		}


		void				MaterialDialog::Create(bool allowUnknown, bool allowLiquids)
		{
			window.reset(new GUIElements::Window());
			//window->autoHeight = false;
			for (index_t i  = 0; i < Grid::Content::NumberOfPossibilities; i++)
			{
				if (!allowUnknown && i == Grid::Content::Unknown)
					continue;
				if (i == Grid::Content::Technite)
					continue;
				if (Grid::CellStack::Cell::IsLiquid((BYTE)i) && !allowLiquids)
					continue;

				String name = Grid::Content::Reinterpret(i).ToString();
				if (i == Grid::Content::Unknown)
					name = "Natural";
				GUIElements::PButton button(new GUIElements::Button(name));
				button->backgroundTone = Grid::CellStack::Cell::typeColor[i];
				Vec::abs(button->backgroundTone);
				float max = Vec::max(button->backgroundTone);
				if (max > 0.5f)
					button->backgroundTone /= max;
				button->SetColor(float4(button->backgroundTone,1.f));
				button->fillBackground = true;
				float b = 0.2f * (window->components.Count()/4);
				float x = 0.25f * (window->components.Count()%4);
				float x2 = 0.25f * (3-(window->components.Count()%4));
				button->SetLocation(Math::TFloatRect(float2(),float2(0.15f,0.15f)));
				button->anchorConfig.Left().Set(x);
				button->anchorConfig.Right().Set(x2);
				if (window->components.Count()>=4)
					button->anchorConfig.Top().Set(window->components[4*(window->components.Count()/4-1)], 0.01f);
				else
					button->anchorConfig.Top().Set(0.f);
				//if ((i%4)!=0)
				//{
				//	button->anchorConfig.Left().SetLastOf(window,0.01f);
				//}
				button->SetOnPress([this,i]()
				{
					Dialog::Hide(window);
					this->onSelect((BYTE)i);
				});
				window->components << button;
			}

			{
				GUIElements::PButton button(new GUIElements::Button("Abort"));
				float b = 0.2f * ((window->components.Count()/4)+1);
				button->SetLocation(Math::TFloatRect(float2(),float2(1.f,0.2f)));
//				button->anchorConfig.Top().Set(b);
				button->anchorConfig.Left().Set(0);
				button->anchorConfig.Right().Set(0);
				button->anchorConfig.Top().SetLastOf(window, 0.01f);
			
				button->SetOnPress([this]()
				{
					Dialog::Hide(window);
				});
				window->components << button;
			}
		}


		void				MaterialDialog::SetCurrent(BYTE type)
		{

		}



		/*
		struct TLayerConfig
		{
			int		dampening = 2;
			int		baseHeight = 0;	//center terrain height
			float	heightScale = 1.f;	//vertical scale of noise.
			//1.0f is pretty much the most you should set with normal noise frequency
			float	frequency = 1.f;	//noise frequency factor
		};

		*/

		void	LoadClientWorld()
		{
			if (clientWorldLoaded)
				return;
			Update(false);
			clientWorldLoaded = true;
			LogEvent(EventClass(EventSignificance::Important ,EventFlavor::Gain),"Now viewing local planet version. Objects and nexus bases are hidden!");
		}

		void	LoadServerWorld(bool forceFullDiscovery)
		{
			if (!clientWorldLoaded && anyWorldLoaded)
				return;
			clientWorldLoaded = false; 
			flashUploadButton = false;
			anyWorldLoaded = true;
			if (GridState::current)
				GridState::current->Import();
			if (forceFullDiscovery)
			{
				auto&st = serverWorld[Grid::WorldDiscovery::FullTerrain];
				if (st.isLoaded)
					st.Export();
				else
					GridState::current = &st;	//link, whatever is loaded
			}
			else
				ActivateDiscovery(false,serverWorld);
		}

		void AppendRuleButton(const Objects::PCommandGroup&grp,const String&niceName, const String&iconName,const String&configName, UINT32 flag, const std::function<void(bool)>&onChange)
		{
			Objects::PBaseCommand cmd(new Objects::CustomIconCommand(niceName,iconName,[flag,onChange]()
			{
				if (gameConfig.gameFlags & flag)
				{
					gameConfig.gameFlags &= ~flag;
					onChange(false);
				}
				else
				{
					gameConfig.gameFlags |= flag;
					onChange(true);
				}
				Network::Channels::gameConfig.SendTo(Network::client,gameConfig);
				//LoadClientWorld();
			},
			[flag](bool)->Objects::BaseCommand::Flags
			{
				typedef Objects::BaseCommand::Flags F;

				F result = F::IsEnabled;
				
				if (gameConfig.gameFlags & flag)
					result |= F::IsPermaActive;
				return result;
			}));
			grp->Add(cmd);
			onChange(!!(gameConfig.gameFlags&flag));
		}

		void AppendRuleButton(const Objects::PCommandGroup&grp,const String&niceName, const String&configName, UINT32 flag)
		{
			AppendRuleButton(grp,niceName,niceName,configName,flag,[](bool){});
		}

		void	RefreshDiscovery()
		{
			if (clientWorldLoaded)
				Update(false);
			else
				ActivateDiscovery(false,serverWorld);
		}

		void AppendDiscoveryButton(const Objects::PCommandGroup&grp,Grid::WorldDiscovery disc)
		{
			Objects::PBaseCommand cmd(new Objects::CustomIconCommand(disc.ToString(),String("Discovery ")+disc.ToString(),[disc]()
			{
				if (gameConfig.worldDiscovery == (BYTE)disc && showDiscovery)
					return;
				showDiscovery = true;
				gameConfig.worldDiscovery = (BYTE)disc;
				Network::Channels::gameConfig.SendTo(Network::client,gameConfig);
				RefreshDiscovery();
			},
			[disc](bool)->Objects::BaseCommand::Flags
			{
				typedef Objects::BaseCommand::Flags F;

				F result = F::IsEnabled;
				
				if (gameConfig.worldDiscovery == (BYTE)disc)
					result |= F::IsPermaActive;
				return result;
			}));
			grp->Add(cmd);

		}


		void AppendGoalButton(const Objects::PCommandGroup&grp,RuntimeConfig::GameGoal goal)
		{
			Objects::PBaseCommand cmd(new Objects::CustomIconCommand(goal.ToString(),String("Goal ")+goal.ToString(),[goal]()
			{
				if (gameConfig.gameGoal == (BYTE)goal)
					return;
				RuntimeConfig::gameGoal = goal;
				gameConfig.gameGoal = (BYTE)goal;
				Network::Channels::gameConfig.SendTo(Network::client,gameConfig);
			},
			[goal](bool)->Objects::BaseCommand::Flags
			{
				typedef Objects::BaseCommand::Flags F;

				F result = F::IsEnabled;
				
				if (gameConfig.gameGoal == (BYTE)goal)
					result |= F::IsPermaActive;
				return result;
			}));
			grp->Add(cmd);
		}
			

		void MakeIntButtons(const Objects::PCommandGroup&group, int&value, const int&defValue, int min, int max)
		{
			Objects::PBaseCommand reset(new Objects::CustomIconCommand("Reset this setting to server default","Undo",[&value,&defValue]()
			{
				value = defValue;
				Update();
			},[](bool){return Objects::BaseCommand::Flags::IsEnabled;}));
			Objects::PBaseCommand decrease(new Objects::IconCommand("Decrease",[&value,min]()
			{
				if (value > min)
				{
					value--;
					Update();
				}
			},[&value,min](bool){return value > min ? Objects::BaseCommand::Flags::IsEnabled : Objects::BaseCommand::Flags::None;}));
			Objects::PBaseCommand increase(new Objects::IconCommand("Increase",[&value,max]()
			{
				if (value < max)
				{
					value++;
					Update();
				}
			},[&value,max](bool){return value < max ? Objects::BaseCommand::Flags::IsEnabled : Objects::BaseCommand::Flags::None;}));

			Objects::PBaseCommand decreaseFast(new Objects::IconCommand("Decrease Fast",[&value,min,max]()
			{
				if (value > min)
				{
					value-=(max - min)/4;
					value = std::max(min,value);
					Update();
				}
			},[&value,min](bool){return value > min ? Objects::BaseCommand::Flags::IsEnabled : Objects::BaseCommand::Flags::None;}));
			Objects::PBaseCommand increaseFast(new Objects::IconCommand("Increase Fast",[&value,min,max]()
			{
				if (value < max)
				{
					value+=(max - min)/4;
					value = std::min(max,value);
					Update();
				}
			},[&value,max](bool){return value < max ? Objects::BaseCommand::Flags::IsEnabled : Objects::BaseCommand::Flags::None;}));

			Objects::PBaseCommand setMin(new Objects::IconCommand("Set Min",[&value,min]()
			{
				if (value > min)
				{
					value = min;
					Update();
				}
			},[&value,min](bool){return value > min ? Objects::BaseCommand::Flags::IsEnabled : Objects::BaseCommand::Flags::None;}));
			Objects::PBaseCommand setMax(new Objects::IconCommand("Set Max",[&value,max]()
			{
				if (value < max)
				{
					value = max;
					Update();
				}
			},[&value,max](bool){return value < max ? Objects::BaseCommand::Flags::IsEnabled : Objects::BaseCommand::Flags::None;}));


			group->Add(reset);
			group->Add(decrease);
			group->Add(increase);
			group->Add(decreaseFast);
			group->Add(increaseFast);
			group->Add(setMin);
			group->Add(setMax);
		}

		void MakeFloatButtons(const Objects::PCommandGroup&group, float&value, const float&defValue)
		{
			float min = value / 10.f;
			float max = value * 10.f;
			Objects::PBaseCommand reset(new Objects::CustomIconCommand("Reset this setting to server default","Undo",[&value,&defValue]()
			{
				value = defValue;
				Update();
			},[](bool){return Objects::BaseCommand::Flags::IsEnabled;}));
			Objects::PBaseCommand decrease(new Objects::IconCommand("Decrease",[&value,min]()
			{
				if (value > min)
				{
					value /= 1.2f;
					value = std::max(value,min);
					Update();
				}
			},[&value,min](bool){return value > min ? Objects::BaseCommand::Flags::IsEnabled : Objects::BaseCommand::Flags::None;}));
			Objects::PBaseCommand increase(new Objects::IconCommand("Increase",[&value,max]()
			{
				if (value < max)
				{
					value *= 1.2f;
					value = std::min(value,max);
					Update();
				}
			},[&value,max](bool){return value < max ? Objects::BaseCommand::Flags::IsEnabled : Objects::BaseCommand::Flags::None;}));

			group->Add(reset);
			group->Add(decrease);
			group->Add(increase);
		}

		Objects::PCommandGroup MakeLayer(const String&name, Grid::ExtendedPlanetConfig::TLayerConfig&layer,const Grid::ExtendedPlanetConfig::TLayerConfig&refLayer)
		{
			Objects::PCommandGroup group(new Objects::CommandGroup(name));
			group->baseCoords = int2(1);
			MakeIntButtons(group->AddChild("Base Height"),layer.baseHeight,refLayer.baseHeight, Grid::CellStack::BottomHeight,Grid::CellStack::TopHeight-1);
			MakeIntButtons(group->AddChild("Dampening"),layer.dampening,refLayer.dampening, 0,Grid::CellStack::HalfRange);
			MakeFloatButtons(group->AddChild("Height Scale"),layer.heightScale, refLayer.heightScale);
			MakeFloatButtons(group->AddChild("Frequency"),layer.frequency, refLayer.frequency);
			return group;
		}


		template <typename T=BYTE>
		void MakeMaterialChoice(const Objects::PCommandGroup&group, T&value, const T&defValue, MaterialDialog&dialog)
		{
			Objects::PBaseCommand reset(new Objects::CustomIconCommand("Reset this material choice to server default","Undo",[&value,defValue]()
			{
				value = defValue;
				Update();
			},[](bool){return Objects::BaseCommand::Flags::IsEnabled;}));
			Objects::PBaseCommand choose(new Objects::IconCommand("Choose Material",[&value,&dialog]()
			{
				dialog.SetCurrent(value);
				dialog.onSelect = [&value](BYTE v)
				{
					value = v;
					Update();
				};
				Dialog::Show(dialog.window); 
			},
			[](bool){return Objects::BaseCommand::Flags::IsEnabled;}
			));

			group->Add(reset);
			group->Add(choose);
		}



		void	Create()
		{
			if (created)
				return;
			created = true;
			fullScope.Create(true,true);
			noLiquids.Create(false,false);
			noUnknown.Create(false,true);

			coreGroup.reset(new Objects::CommandGroup("Core"));
			coreGroup->baseCoords = int2(1);
			ruleGroup.reset(new Objects::CommandGroup("Rules"));
			ruleGroup->baseCoords = int2(1);
			/*			ClosedGame = 0x1,
			BasesRequired = 0x2,
			StartBases = 0x4,
			NexusAI = 0x8,
			EMPZones = 0x10,
			ConfigurationMode = 0x20,

			*/

			AppendRuleButton(ruleGroup,"Closed Game","closedGame",Protocol::GameFlag::ClosedGame);
			AppendRuleButton(ruleGroup,"Start-Bases","startBases",Protocol::GameFlag::StartBases);
			AppendRuleButton(ruleGroup,"Bases Required","basedRequired",Protocol::GameFlag::BasesRequired);
			AppendRuleButton(ruleGroup,"Enable Nexus AI", "Nexus/enableAI",Protocol::GameFlag::NexusAI);
			AppendRuleButton(ruleGroup,"EMP Zones", "Energy","empZones",Protocol::GameFlag::EMPZones,[](bool set)
			{
				if (set)
				{
					Grid::empZonesUnlocked = 0x1;
					Grid::empZonesLocked = ~0x1;
				}
				else
				{
					Grid::empZonesUnlocked = 0xffff;
					Grid::empZonesLocked = 0x0;
				}
				RuntimeConfig::empZones = set;
				Grid::SetAllFamiliesOutdated(0);
			});
			Objects::PCommandGroup goals = ruleGroup->AddChild("Game Goal");
			for (index_t i = 0; i < RuntimeConfig::GameGoal::N; i++)
				AppendGoalButton(goals,RuntimeConfig::GameGoal::Reinterpret(i));
			Objects::PCommandGroup disc = ruleGroup->AddChild("World Discovery");
			for (index_t i = 0; i < Grid::WorldDiscovery::N; i++)
				AppendDiscoveryButton(disc,Grid::WorldDiscovery::Reinterpret(i));
			disc->onExpand = []()
			{
				showDiscovery = true;
				RefreshDiscovery();
			};
			disc->onCollapse = []()
			{
				showDiscovery = false;
				RefreshDiscovery();
			};

			MakeMaterialChoice(coreGroup,config.innerCoreType,refConfig.innerCoreType,noUnknown);
			mountainGroup = MakeLayer("Mountains",config.mountains,refConfig.mountains);
			extendedCoreGroup = MakeLayer("Extended Core",config.extendedCore,refConfig.extendedCore);
			MakeMaterialChoice(extendedCoreGroup->AddChild("Type","Extended Core Content"),config.extendedCoreType,refConfig.extendedCoreType, noUnknown);
			MakeMaterialChoice(mountainGroup->AddChild("Type","Terrain Content"),config.innerType,refConfig.innerType, fullScope);
			MakeMaterialChoice(mountainGroup->AddChild("Top Type", "Terrain Top" ),config.topType,refConfig.topType, fullScope);
			MakeIntButtons(mountainGroup->AddChild("Snow Height"),config.snowHeight,refConfig.snowHeight,Grid::CellStack::BottomHeight,Grid::CellStack::TopHeight-1);

			{
				liquidGroup.reset(new Objects::CommandGroup("Oceans"));
				liquidGroup->baseCoords = int2(1);
				MakeMaterialChoice(liquidGroup->AddChild("Ocean Content","Ocean Type"),config.liquidCoverage,refConfig.liquidCoverage,noUnknown);
				MakeIntButtons(liquidGroup->AddChild("Level","Base Height"),config.liquidHeight,refConfig.liquidHeight,Grid::CellStack::BottomHeight,Grid::CellStack::TopHeight-1);
				MakeMaterialChoice(liquidGroup->AddChild("Ocean Outline"),config.liquidOutline,refConfig.liquidOutline,noLiquids);
			}


			int at = 0;
			buttons << std::make_pair(int2(at++,0),
						Objects::PBaseCommand(new Objects::CustomIconCommand("Accept current server planet and config, and finish","Commit",[]()
						{
							
							if (clientWorldLoaded)
							{
								Dialog::Confirm("Upload changes and begin game?",[]()
								{
									refConfig = config;
									RefreshButtonStates();
									Network::Channels::planetConfig.SendTo(Network::client,config);
									Network::client.SendSignal(Protocol::ChannelID::EndConfigurationMode);
								},
								[](){return Detail::isActive ? Dialog::CheckResult::OkayContinue : Dialog::CheckResult::Abort;});
							}
							else
								Dialog::Confirm("Accept current state and begin game?",[]()
								{
									Network::client.SendSignal(Protocol::ChannelID::EndConfigurationMode);
								},
								[](){return Detail::isActive ? Dialog::CheckResult::OkayContinue : Dialog::CheckResult::Abort;});
							//Update();
						},
						[](bool){return Objects::BaseCommand::Flags::IsEnabled;}
						))
					);
			buttons << std::make_pair(int2(at++,0),
						Objects::PBaseCommand(new Objects::CustomIconCommand("Commit changes and rebuild server planet","Upload to Server",[]()
						{
							refConfig = config;
							RefreshButtonStates();
							Network::Channels::planetConfig.SendTo(Network::client,config);
							//Update();
						},
						[](bool){
						return (flashUploadButton ? Objects::BaseCommand::Flags::SoftFlashing : Objects::BaseCommand::Flags::None) |  Objects::BaseCommand::Flags::IsEnabled;
						}
						))
					);
			buttons	<< std::make_pair(int2(at++,0),
						Objects::PBaseCommand(new Objects::CustomIconCommand("Reset to server config and rebuild local planet","Download from Server",[]()
						{
							config = refConfig;
							Update();
						},
						[](bool){return Objects::BaseCommand::Flags::IsEnabled;}
						))
					);
			buttons << std::make_pair(int2(at++,0),
						Objects::PBaseCommand(new Objects::CustomIconCommand("View server version","View Server Version",[]()
						{
							if (clientWorldLoaded)
							{
								LoadServerWorld(false);
								RestoreServerObjects();
							}
							else
							{
								LoadClientWorld();
							}
							//Update();
						},
						[](bool){
						return (!clientWorldLoaded ? Objects::BaseCommand::Flags::IsPermaActive : Objects::BaseCommand::Flags::None) | Objects::BaseCommand::Flags::IsEnabled;
						}
						))
					);
			buttons	<< std::make_pair(int2(at++,0),
						Objects::PBaseCommand(new Objects::CustomIconCommand("Randomize seeds and rebuild using current config","Randomize",[]()
						{
							RandomizeSeed();
						},
						[](bool){return Objects::BaseCommand::Flags::IsEnabled;}
						))
					);
			buttons	<< std::make_pair(int2(at++,0),mountainGroup);
			buttons	<< std::make_pair(int2(at++,0),extendedCoreGroup);
			buttons	<< std::make_pair(int2(at++,0),coreGroup);
			buttons	<< std::make_pair(int2(at++,0),liquidGroup);
			buttons	<< std::make_pair(int2(at++,0),ruleGroup);
			;
		}

		void	Destroy()
		{

		}
	}

	void	RandomizeSeed()
	{
		using namespace Detail;
		seed = rng.getSeed();
		Update();
	}

	void	Update(const Grid::ExtendedPlanetConfig&cfg)
	{
		using namespace Detail;
		seed = cfg.lastSeed;
		config = cfg;
		refConfig = cfg;
		if (clientWorldLoaded)
			Update();
	}

	void	SetGameConfig(UINT32 gameFlags, RuntimeConfig::GameGoal goal, Grid::WorldDiscovery disc)
	{
		Detail::gameConfig.gameFlags = gameFlags;
		Detail::gameConfig.gameGoal = (BYTE)goal;
		Detail::gameConfig.worldDiscovery = (BYTE)disc;
	}


	void	SetActive(bool newActive)
	{
		using namespace Detail;
		if (isActive == newActive)
			return;
		isActive = newActive;
		if (!isActive)
		{
			clientWorldBuilt = false;
			showDiscovery = false;
			serverObjects.Clear();
		}
		Create();
		UI::HexGrid::Configure(Objects::selectedEntity);

	}

	bool	IsActive()
	{
		using namespace Detail;
		return isActive;
	}

	bool	ReconfigureVisualCommands()
	{
		using namespace Detail;
		if (!isActive)
			return false;
		UI::HexGrid::ClearCommands();
		foreach (buttons,b)
			b->second->VisitMembers(UI::HexGrid::VisitAddCommand,false,b->first);
		return true;
	}


	void	RestoreServerWorld()
	{
		Detail::LoadServerWorld(true);
	}
	void	RestoreServerObjects()
	{
		using namespace Detail;
		if (serverObjects.IsNotEmpty())
		{
			serverObjects.Export();
			Grid::actualWorld.OverrideVisibility(1);
		}
	}


}

#ifdef HAS_WINDOW

#include <global_root.h>
#include <string/cli.h>
#include "configurationMode.h"
#include "world.h"
#include "ui.h"
#include <objects/visualCommands.h>
#include "network.h"
#include "objectStorage.h"
#include "../version.h"

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
		StringTable<String>		declarations;

		const String& FindDeclaration(const String& path)
		{
			if (!path.beginsWith('/'))
				return FindDeclaration('/'+path);
			static String empty = "";
			const String*ptr = declarations.QueryPointer(path);
			if (ptr)
				return *ptr;
			return empty;
		}

		void ParseDeclarations(const XML::Node& context, const String&path)
		{
			String str;
			Array<String>	lines;
			foreach(context.children,ch)
			{
				if (ch->name == "context" && ch->Query("name",str))
					ParseDeclarations(*ch,path+'/'+str);
				else if (ch->name == "variable" && ch->Query("name",str))
				{
					String base = ch->inner_content;
					explode('\n',base,lines);
					foreach(lines,ln)
						ln->trimThis();
					
					declarations.Set(path+'/'+str,implode('\n',lines));
				}
			}
		}


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
				Grid::SetAllFamiliesOutdated();	//force rebuild
			}

		};
		/*static*/ GridState	*GridState::current = nullptr;

		GridState serverWorld[Grid::WorldDiscovery::N], clientWorld[Grid::WorldDiscovery::N];
		bool created = false,
				flashUploadButton = false;
		bool clientWorldLoaded = false, clientWorldBuilt = false, anyWorldLoaded = false;
		Objects::PCommandGroup mountainGroup,extendedCoreGroup,liquidGroup,coreGroup,ruleGroup,copyPasteGroup;
		Buffer0<std::pair<int2,Objects::PBaseCommand> > buttons; 

		class MaterialDialog
		{
		public:
			GUI::PWindow	window;
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
			window.reset(new GUI::Window());
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
				GUI::PButton button(new GUI::Button(name));
				button->backgroundTone = Grid::CellStack::Cell::typeColor[i];
				Vec::abs(button->backgroundTone);
				float max = Vec::max(button->backgroundTone);
				if (max > 0.5f)
					button->backgroundTone /= max;
				button->SetColor(float4(button->backgroundTone,1.f));
				button->fillBackground = true;
				float b = 0.2f * (window->CountComponents()/4);
				float x = 0.25f * (window->CountComponents()%4);
				float x2 = 0.25f * (3-(window->CountComponents()%4));
				button->SetLocation(Math::TFloatRect(float2(),float2(0.15f,0.15f)));
				button->anchorConfig.Left().Set(x);
				button->anchorConfig.Right().Set(x2);
				if (window->CountComponents()>=4)
					button->anchorConfig.Top().Set(window->GetComponents()[4*(window->CountComponents()/4-1)], 0.01f);
				else
					button->anchorConfig.Top().Set(0.f);
				//if ((i%4)!=0)
				//{
				//	button->anchorConfig.Left().SetLastOf(window,0.01f);
				//}
				button->SetOnPress([this,i]()
				{
					GUI::Hide(window);
					this->onSelect((BYTE)i);
				});
				window->AddComponent(button);
			}

			{
				GUI::PButton button(new GUI::Button("Abort"));
				float b = 0.2f * ((window->CountComponents()/4)+1);
				button->SetLocation(Math::TFloatRect(float2(),float2(1.f,0.2f)));
//				button->anchorConfig.Top().Set(b);
				button->anchorConfig.Left().Set(0);
				button->anchorConfig.Right().Set(0);
				button->anchorConfig.Top().SetLastOf(window, 0.01f);
			
				button->SetOnPress([this]()
				{
					GUI::Hide(window);
				});
				window->AddComponent( button );
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

		/**
		Activates client world view
		*/
		void	LoadClientWorld(bool forceRebuild=false)
		{
			if (clientWorldLoaded)
			{
				if (forceRebuild)
					Update(true);
				return;
			}
			Update(forceRebuild);
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
			Objects::PIconCommand cmd(new Objects::CustomIconCommand(niceName,iconName,[flag,onChange]()
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
			String help = FindDeclaration(configName);
			if (help.IsNotEmpty())
				cmd->customLabel << help;
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
			

		Objects::PCommandGroup MakeIntButtons(const Objects::PCommandGroup&group, int&value, const int&defValue, int min, int max)
		{
			Objects::PIconCommand reset(new Objects::CustomIconCommand("Reset","Undo",[&value,&defValue]()
			{
				value = defValue;
				Update();
			},[](bool){return Objects::BaseCommand::Flags::IsEnabled;}));
			reset->customLabel << "Resets this setting to the current server value";
			Objects::PIconCommand decrease(new Objects::IconCommand("Decrease",[&value,min]()
			{
				if (value > min)
				{
					value--;
					Update();
				}
			},[&value,min](bool){return value > min ? Objects::BaseCommand::Flags::IsEnabled : Objects::BaseCommand::Flags::None;}));
			decrease->customLabel << "Decreases this setting by 1";
			Objects::PIconCommand increase(new Objects::IconCommand("Increase",[&value,max]()
			{
				if (value < max)
				{
					value++;
					Update();
				}
			},[&value,max](bool){return value < max ? Objects::BaseCommand::Flags::IsEnabled : Objects::BaseCommand::Flags::None;}));
			increase->customLabel << "Increases this setting by 1";

			Objects::PIconCommand decreaseFast(new Objects::IconCommand("Decrease Fast",[&value,min,max]()
			{
				if (value > min)
				{
					value-=(max - min)/4;
					value = std::max(min,value);
					Update();
				}
			},[&value,min](bool){return value > min ? Objects::BaseCommand::Flags::IsEnabled : Objects::BaseCommand::Flags::None;}));
			decreaseFast->customLabel << "Decreases this setting by "+String((max - min)/4);
			Objects::PIconCommand increaseFast(new Objects::IconCommand("Increase Fast",[&value,min,max]()
			{
				if (value < max)
				{
					value+=(max - min)/4;
					value = std::min(max,value);
					Update();
				}
			},[&value,max](bool){return value < max ? Objects::BaseCommand::Flags::IsEnabled : Objects::BaseCommand::Flags::None;}));
			increaseFast->customLabel << "Decreases this setting by "+String((max - min)/4);

			Objects::PIconCommand setMin(new Objects::IconCommand("Set Min",[&value,min]()
			{
				if (value > min)
				{
					value = min;
					Update();
				}
			},[&value,min](bool){return value > min ? Objects::BaseCommand::Flags::IsEnabled : Objects::BaseCommand::Flags::None;}));
			setMin->customLabel << "Sets this setting to the minimum possible value ("+String(min)+")";


			Objects::PIconCommand setMax(new Objects::IconCommand("Set Max",[&value,max]()
			{
				if (value < max)
				{
					value = max;
					Update();
				}
			},[&value,max](bool){return value < max ? Objects::BaseCommand::Flags::IsEnabled : Objects::BaseCommand::Flags::None;}));
			setMax->customLabel << "Sets this setting to the maximum possible value ("+String(max)+")";


			group->Add(reset);
			group->Add(decrease);
			group->Add(increase);
			group->Add(decreaseFast);
			group->Add(increaseFast);
			group->Add(setMin);
			group->Add(setMax);
			return group;
		}

		Objects::PCommandGroup MakeFloatButtons(const Objects::PCommandGroup&group, float&value, const float&defValue)
		{
			float min = value / 10.f;
			float max = value * 10.f;
			Objects::PIconCommand reset(new Objects::CustomIconCommand("Reset","Undo",[&value,&defValue]()
			{
				value = defValue;
				Update();
			},[](bool){return Objects::BaseCommand::Flags::IsEnabled;}));
			reset->customLabel << "Resets this setting to the current server value";
			Objects::PIconCommand decrease(new Objects::IconCommand("Decrease",[&value,min]()
			{
				if (value > min)
				{
					value /= 1.2f;
					value = std::max(value,min);
					Update();
				}
			},[&value,min](bool){return value > min ? Objects::BaseCommand::Flags::IsEnabled : Objects::BaseCommand::Flags::None;}));
			decrease->customLabel << "Decreases this setting by 20%";
			Objects::PIconCommand increase(new Objects::IconCommand("Increase",[&value,max]()
			{
				if (value < max)
				{
					value *= 1.2f;
					value = std::min(value,max);
					Update();
				}
			},[&value,max](bool){return value < max ? Objects::BaseCommand::Flags::IsEnabled : Objects::BaseCommand::Flags::None;}));
			increase->customLabel << "Increases this setting by 20%";

			group->Add(reset);
			group->Add(decrease);
			group->Add(increase);
			return group;
		}

		Objects::PCommandGroup MakeLayer(const String&name, Grid::ExtendedPlanetConfig::TLayerConfig&layer,const Grid::ExtendedPlanetConfig::TLayerConfig&refLayer)
		{
			Objects::PCommandGroup group(new Objects::CommandGroup(name));
			group->baseCoords = int2(1);
			MakeIntButtons(group->AddChild("Base Height"),layer.baseHeight,refLayer.baseHeight, Grid::CellStack::BottomHeight,Grid::CellStack::TopHeight-1)
			->customLabel << "Defines the central height of this attribute. The effective altitude stretches symmetrically up and down from this value."
			;
			MakeIntButtons(group->AddChild("Dampening"),layer.dampening,refLayer.dampening, 0,Grid::CellStack::HalfRange)
			->customLabel << "Draws height values closer to the base height. Creates plateaus at central height level"
			;
			MakeFloatButtons(group->AddChild("Height Scale"),layer.heightScale, refLayer.heightScale)
			->customLabel << "Scales extremes. The heigher this value the further the resulting heightmap will reach up and down"
			;
			MakeFloatButtons(group->AddChild("Frequency"),layer.frequency, refLayer.frequency)
			->customLabel << "Scales the frequency of peaks. The heigher this value the higher the number of mountains at reduced horizontal extend each."
			;
			return group;
		}


		template <typename T=BYTE>
		void MakeMaterialChoice(const Objects::PCommandGroup&group, T&value, const T&defValue, MaterialDialog&dialog)
		{
			Objects::PIconCommand reset(new Objects::CustomIconCommand("Reset","Undo",[&value,defValue]()
			{
				value = defValue;
				Update();
			},[](bool){return Objects::BaseCommand::Flags::IsEnabled;}));
			reset->customLabel << "Resets this material to the current server value";
			Objects::PBaseCommand choose(new Objects::IconCommand("Choose Material",[&value,&dialog]()
			{
				dialog.SetCurrent(value);
				dialog.onSelect = [&value](BYTE v)
				{
					value = v;
					Update();
				};
				GUI::Show(dialog.window); 
			},
			[](bool){return Objects::BaseCommand::Flags::IsEnabled;}
			));

			group->Add(reset);
			group->Add(choose);
		}


		namespace Clipboard
		{
			Buffer0<String>	warnings;

			void	ExportConfig(Config::Context&sub, const Grid::ExtendedPlanetConfig::TLayerConfig&layer)
			{
				sub.Define("baseHeight",layer.baseHeight);
				sub.Define("noiseHeightScale",layer.heightScale);
				sub.Define("frequency",layer.frequency);
				sub.Define("dampening",layer.dampening);
			}

			void	ExportPlanet()
			{
				/*
				Important:
				Keep this in sync with server/components/reflection.cpp
				*/
				Configuration cfg;
				cfg.Define("atmosphereDensity",config.atmosphereColor.a);
				cfg.Define("innerCoreType",Grid::Content::Reinterpret(config.innerCoreType).ToString());
				{
					Config::Context&sub = cfg.DefineContext("ExtendedCore");
					sub.Define("type",Grid::Content::Reinterpret(config.extendedCoreType).ToString());
					ExportConfig(sub,config.extendedCore);
				}
					
				{
					Config::Context&sub = cfg.DefineContext("Mountains");
					sub.Define("type",Grid::Content::Reinterpret(config.type).ToString());
					sub.Define("snowHeight",config.snowHeight);
					ExportConfig(sub,config.mountains);
				}

				{
					Config::Context&sub = cfg.DefineContext("Liquid");
					sub.Define("type",Grid::Content::Reinterpret(config.liquidCoverage).ToString());
					sub.Define("level",config.liquidHeight);
					sub.Define("outline",Grid::Content::Reinterpret(config.liquidOutline).ToString());
				}
				

				{
					Config::Context&sub = cfg.DefineContext("Resources");
					for (index_t i = 0; i < Objects::Resource::NumberOfPossibilities; i++)
					{
						Objects::Resource res = Objects::Resource::Reinterpret(i);
						Config::Context&sub2 = sub.DefineContext(res.ToString());
							sub2.Define("density",Grid::Resource::density[i]);
							sub2.Define("max",Grid::Resource::maximum[i]);
					}
				}

				StringBuffer buffer;
				cfg.SaveToStringBuffer(buffer,PRODUCT_NAME);
				buffer<< '\0';
				System::copyToClipboard(Engine::context.getWindow(),buffer.pointer());
			}

			template <typename T>
			void	Query(const Config::Context&ctx, const char*name, T&var)
			{
				String rs;
				if (!ctx.QueryString(name,rs))
				{
					warnings << "Unable to query '"+ctx.name+"/"+name+"'";
					return;
				}
				if (!convert(rs.c_str(),rs.length(),var))
				{
					warnings << "Unable to convert '"+ctx.name+"/"+name+"' = '"+rs+"' to "+TypeInfo<T>::name;
					return;
				}
			}

			template <typename T, typename T2>
			void	QueryMappedEnum(const Config::Context&ctx, const char*name, T2&var)
			{
				String rs;
				if (!ctx.QueryString(name,rs))
				{
					warnings << "Unable to query '"+ctx.name+"/"+name+"'";
					return;
				}
				T e;

				if (!e.Decode(rs.c_str()))
				{
					warnings << "Unable to convert '"+ctx.name+"/"+name+"' = '"+rs+"' to "+T::GetName();
					return;
				}
				var = (T2)e;
			}


			void	ImportConfig(const Config::Context&sub, Grid::ExtendedPlanetConfig::TLayerConfig&layer)
			{
				Query(sub,"baseHeight",layer.baseHeight);
				Query(sub,"noiseHeightScale",layer.heightScale);
				Query(sub,"frequency",layer.frequency);
				Query(sub,"dampening",layer.dampening);
			}

			void	ImportPlanet()
			{
				/*
				Important:
				Keep this in sync with server/components/reflection.cpp
				*/
				char buffer[0x10000];
				if (System::getFromClipboardIfText(Engine::context.getWindow(),buffer,sizeof(buffer)))
				{
					Configuration cfg;
					StringFile file(CM_STRIP_COMMENTS|CM_RECORD_COMMENTS|CM_STRIP_LINE_START_COMMENTS);
					
					file.LinkSourceData(buffer,strlen(buffer));
					if (cfg.LoadFromFile(file))
					{
						warnings.Clear();
						Query(cfg,"atmosphereDensity",config.atmosphereColor.a);
						QueryMappedEnum<Grid::Content>(cfg,"innerCoreType",config.innerCoreType);

						if (Config::Context*sub = cfg.GetContext("ExtendedCore"))
						{
							QueryMappedEnum<Grid::Content>(*sub,"type",config.extendedCoreType);
							ImportConfig(*sub,config.extendedCore);
						}
						else
							warnings << "Unable to fetch 'ExtendedCore'";
					
						if (Config::Context*sub = cfg.GetContext("Mountains"))
						{
							QueryMappedEnum<Grid::Content>(*sub,"type",config.type);
							Query(*sub,"snowHeight",config.snowHeight);
							ImportConfig(*sub,config.mountains);
						}
						else
							warnings << "Unable to fetch 'Mountains'";

						if (Config::Context*sub = cfg.GetContext("Liquid"))
						{
							QueryMappedEnum<Grid::Content>(*sub,"type",config.liquidCoverage);
							Query(*sub,"level",config.liquidHeight);
							QueryMappedEnum<Grid::Content>(*sub,"outline",config.liquidOutline);
						}
						else
							warnings << "Unable to fetch 'Liquid'";
				

						if (Config::Context*sub = cfg.GetContext("Resources"))
						{
							for (index_t i = 0; i < Objects::Resource::NumberOfPossibilities; i++)
							{
								Objects::Resource res = Objects::Resource::Reinterpret(i);
								if (Config::Context*sub2 = sub->GetContext(res.ToString()))
								{
									Query(*sub2,"density",Grid::Resource::density[i]);
									Query(*sub2,"max",Grid::Resource::maximum[i]);
								}
								else
									warnings << "Unable to fetch '"+sub->name+"/"+res.ToString()+"'";
							}
						}
						else
							warnings << "Unable to fetch 'Resources'";

						if (warnings.IsNotEmpty())
						{
							Dialog::ShowMessage("Importing encountered warnings:\n"+implode('\n',warnings.pointer(),warnings.Count()));
						}
						LoadClientWorld(true);
					}
					else
						Dialog::ShowMessage("Planet configuration could not be parsed from clipboard:\n"+cfg.GetError());
				}
				else
					Dialog::ShowMessage("Planet configuration could not be loaded from clipboard. Chances are, you did not copy it to clipboard first");

			}
		}


		void	Create()
		{
			if (created)
				return;
			created = true;

			XML::Container declarations;
			declarations.LoadFromFile("../server/config/declaration.xml");
			ParseDeclarations(declarations.root_node,"");


			fullScope.Create(true,true);
			noLiquids.Create(false,false);
			noUnknown.Create(false,true);

			coreGroup.reset(new Objects::CommandGroup("Core"));
			coreGroup->baseCoords = int2(1);
			coreGroup->customLabel << "The inner planetary core represents the center-most sphere content";
			ruleGroup.reset(new Objects::CommandGroup("Rules"));
			ruleGroup->baseCoords = int2(1);
			ruleGroup->customLabel << "All common game rules";
			copyPasteGroup.reset(new Objects::CommandGroup("Copy/Paste"));
			copyPasteGroup->baseCoords = int2(1);
			copyPasteGroup->customLabel << "Copy/paste current configuration to clipboard";

			{
				Objects::PIconCommand cmd;

				cmd.reset(new Objects::IconCommand("Copy planet to clipboard (CTRL+C)",Clipboard::ExportPlanet,
				[](bool)->Objects::BaseCommand::Flags { return Objects::BaseCommand::Flags::IsEnabled;}));
				copyPasteGroup->Add(cmd);

				cmd.reset(new Objects::IconCommand("Paste planet from clipboard (CTRL+V)",Clipboard::ImportPlanet,
				[](bool)->Objects::BaseCommand::Flags { return Objects::BaseCommand::Flags::IsEnabled;}));
				copyPasteGroup->Add(cmd);
			}



			/*			ClosedGame = 0x1,
			BasesRequired = 0x2,
			StartBases = 0x4,
			NexusAI = 0x8,
			EMPZones = 0x10,
			ConfigurationMode = 0x20,

			*/

			AppendRuleButton(ruleGroup,"Closed Game","Rules/closedGame",Protocol::GameFlag::ClosedGame);
			AppendRuleButton(ruleGroup,"Start-Bases","Rules/startBases",Protocol::GameFlag::StartBases);
			AppendRuleButton(ruleGroup,"Bases Required","Rules/basesRequired",Protocol::GameFlag::BasesRequired);
			AppendRuleButton(ruleGroup,"Enable Nexus AI", "Rules/Nexus/enableAI",Protocol::GameFlag::NexusAI);
			AppendRuleButton(ruleGroup,"Simulate Liquids", "World/simulateLiquids",Protocol::GameFlag::SimulateLiquids);
			AppendRuleButton(ruleGroup,"EMP Zones", "Energy","Rules/empZones",Protocol::GameFlag::EMPZones,[](bool set)
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
			goals->customLabel << FindDeclaration("/Rules/gameGoal");
			for (index_t i = 0; i < RuntimeConfig::GameGoal::N; i++)
				AppendGoalButton(goals,RuntimeConfig::GameGoal::Reinterpret(i));
			Objects::PCommandGroup disc = ruleGroup->AddChild("Initial Discovery","View");
			disc->customLabel << FindDeclaration("/World/worldDiscovery");
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
			mountainGroup->customLabel << "Defines the extend and content of the mountain layer. The mountain layer stretches from the bottom up, where not overwritten by the extended planet core.";
			extendedCoreGroup = MakeLayer("Extended Core",config.extendedCore,refConfig.extendedCore);
			extendedCoreGroup->customLabel << "Defines the extend and content of the extended core layer. The extended core stretches from the bottom up, potentially overwriting content defined by the mountain layer.";
			MakeMaterialChoice(extendedCoreGroup->AddChild("Type","Terrain Content"),config.extendedCoreType,refConfig.extendedCoreType, noUnknown);
			MakeMaterialChoice(mountainGroup->AddChild("Type","Terrain Content"),config.type,refConfig.type, fullScope);
//			MakeMaterialChoice(mountainGroup->AddChild("Top Type", "Terrain Top" ),config.topType,refConfig.topType, fullScope);
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
			buttons	<< std::make_pair(int2(at++,0),copyPasteGroup);
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
		RestoreServerWorld();
		using namespace Detail;
		if (serverObjects.IsNotEmpty())
		{
			serverObjects.Export();
			Grid::actualWorld.OverrideVisibility(1);
		}
	}


	void	OnCopy()
	{
		if (Detail::isActive)
			Detail::Clipboard::ExportPlanet();
	}


	void	OnPaste()
	{
		if (Detail::isActive)
			Detail::Clipboard::ImportPlanet();
	}
}

#endif

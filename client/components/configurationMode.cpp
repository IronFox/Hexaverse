#ifdef HAS_WINDOW

#include <global_root.h>
#include <string/cli.h>
#include <grid/resources.h>
#include "configurationMode.h"
#include "visual/scene/world.h"
#include "visual/scene/terrain.h"
#include "visual/hud/windowStack.h"
#include "visual/hud/hexGrid.h"
#include "visual/hud/objectCommands.h"

#include "network.h"
#include "objectStorage.h"
#include "gridControl.h"
#include "../version.h"
#include "clientEvents.h"

namespace ConfigurationMode
{
	namespace Detail
	{
		Objects::Storage serverObjects;
		bool isActive = false;
		Grid::ExtendedPlanetConfig	config, serverConfig;
		UINT32 seed = 0,serverSeed=0;
		Protocol::GameConfig	gameConfig;
		Random rng;
		StringTable<String>		declarations;
		Array<String>			planetList;
		Timer::Time				lastLocalChange=timer.Now();

		index_t					sentVersion = 0,
								lastServerVersion = 0,
								localVersion = 0;
		/**
		Disallows any planet modifications until the 
		*/
		index_t					lockedAwaitConfig=0;

		Buffer0<Objects::Command::PVisualCommand>	numberCommands;	//!< All commands rendering numbers

		using namespace HUD::Component;
		PWindow			planetListWindow;

		const String& FindDeclaration(const String& path)
		{
			if (!path.BeginsWith('/'))
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
						ln->TrimThis();
					
					declarations.Set(path+'/'+str,implode('\n',lines));
				}
			}
		}



		class GridState
		{
		public:
			World::Data	copy;
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
				copy.CopyGridContent(World::actual);
				//copy.swap(World::actual);
				current = nullptr;
				VisualTerrain::Invalidate(); //force rebuild
			};

			void			Export()
			{
				ASSERT__(isLoaded);
				ASSERT_IS_NULL__(current);
				//ASSERT__(copy.cellStack.First().volumeCell[0].content != Grid::Content::Clear);
				isLoaded = false;
				World::actual.CopyGridContent(copy);
				current = this;
				VisualTerrain::Invalidate(); //force rebuild
			}

		};
		/*static*/ GridState	*GridState::current = nullptr;

		GridState serverWorld[Grid::WorldDiscovery::N], clientWorld[Grid::WorldDiscovery::N];
		bool created = false,
				flashUploadButton = false;
		bool clientWorldLoaded = false, clientWorldBuilt = false, anyWorldLoaded = false;
		Objects::Command::PCommandGroup mountainGroup,extendedCoreGroup,liquidGroup,coreGroup,ruleGroup,copyPasteGroup;
		Buffer0<std::pair<int2,Objects::Command::PBase> > buttons; 

		class MaterialDialog
		{
		public:
			PWindow	window;
			std::function<void(BYTE)>	onSelect;
			Buffer0<std::pair<index_t, PButton> >	materialButtons;
			
			void				Create(bool allowUnknown, bool allowLiquids);
			void				SetText(BYTE type);
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
				//ASSERT__(st.copy.cellStack.First().volumeCell[0].content != Grid::Content::Clear);
				GridState&st2 = range[Grid::WorldDiscovery::Reinterpret(gameConfig.worldDiscovery)];
				if (forceRebuild || !st2.IsLoadedOrActive())
				{
					//ASSERT__(st.copy.cellStack.First().volumeCell[0].content != Grid::Content::Clear);
					st.copy.ExportDiscovered(st2.copy,Grid::WorldDiscovery::Reinterpret(gameConfig.worldDiscovery));
					//ASSERT__(st.copy.cellStack.First().volumeCell[0].content != Grid::Content::Clear);
					//ASSERT__(st2.copy.cellStack.First().volumeCell[0].content != Grid::Content::Clear);
					st2.isLoaded = true;
				}
				st2.Export();
			}
			VisualTerrain::Invalidate(); //force rebuild
			World::allowRecompilation = true;

			if (clientWorldLoaded)
			{
				config.GenerateAtmosphereColor();
				ClientGrid::Apply(config);
			}
			else
				ClientGrid::Apply(serverConfig);

			RefreshButtonStates();
		}

		void Update(bool forceRebuild = true)
		{
			if (lockedAwaitConfig)
				return;

			lastLocalChange = timer.Now();
			if (localVersion == sentVersion)
				localVersion++;
			config.SetSeed(seed);

			if (isActive)
			{
				//World::actual.FlushAllData();
				if (serverObjects.IsEmpty())
				{
					serverObjects.Import(true);
					HUD::HexGrid::RefreshContacts();
				}
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
					config.BuildWorld(st.copy);
					st.copy.FlushAndValidateSensory(1);
//					ASSERT__(st.copy.cellStack.First().volumeCell[0].content != Grid::Content::Clear);
				}
				ActivateDiscovery(forceRebuild, clientWorld);
			}
		}


		void				MaterialDialog::Create(bool allowUnknown, bool allowLiquids)
		{
			materialButtons.Clear();
			window = Window::Create();
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
				PButton button(new Button(name));
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
				button->SetLocation(M::TFloatRect(float2(),float2(0.15f,0.15f)));
				{
					auto cfg = button->GetAnchorConfig();
					cfg.Left().Set(x);
					cfg.Right().Set(x2);
					if (window->CountComponents()>=4)
						cfg.Top().Set(window->GetComponents()[4*(window->CountComponents()/4-1)], 0.01f);
					else
						cfg.Top().Set(0.f);
					button->SetAnchorConfig(cfg);
				}
				button->SetHorizontalPadding(0);
				//if ((i%4)!=0)
				//{
				//	button->anchorConfig.Left().SetLastOf(window,0.01f);
				//}
				button->SetTriggerFunction([this,i]()
				{
					HUD::WindowStack::Hide(window);
					this->onSelect((BYTE)i);
				});
				window->AddComponent(button);
				materialButtons << std::make_pair(i,button);
			}

			{
				PButton button(new Button("Abort"));
				float b = 0.2f * ((window->CountComponents()/4)+1);
				button->SetLocation(M::TFloatRect(float2(),float2(1.f,0.2f)));
//				button->anchorConfig.Top().Set(b);
				{
					auto cfg = button->GetAnchorConfig();
					cfg.Left().Set(0);
					cfg.Right().Set(0);
					cfg.Top().SetLastOf(window, 0.01f);
					button->SetAnchorConfig(cfg);
				}
			
				button->SetTriggerFunction([this]()
				{
					HUD::WindowStack::Hide(window);
				});
				window->AddComponent( button );
			}
		}


		void				MaterialDialog::SetText(BYTE type)
		{
			foreach (materialButtons,b)
			{
				//String name = Grid::Content::Reinterpret(b->first).ToString();
				//if (b->first == Grid::Content::Unknown)
					//name = "Natural";
				if (b->first == type)
				{
					b->second->SetAlwaysDown(true);
					//b->second->SetText("["+name+"]");
				}
				else
					b->second->SetAlwaysDown(false);
					//b->second->SetText(name);
			}
		}

		void UploadToServer()
		{
			config.versionCounter = (UINT32)sentVersion;
			serverConfig = config;
			serverSeed = seed;
			RefreshButtonStates();
			Network::Channels::planetConfig.SendTo(Network::client,config);
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

		template <typename T>
		void SetHint(Objects::Command::CustomLabel<T>&grp, const String&hint)
		{
			if (hint.IsEmpty())
				return;
			grp.customLabel << grp.name;
			grp.customLabel.MarkBriefEnd();
			grp.customLabel << hint.TrimRef();
		}


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

		void		RebuildPlanetList()
		{
			planetListWindow->ClearComponents();
			planetListWindow->SetWidth(0.7f);
			planetListWindow->SetAutomaticHeight(true);
			planetListWindow->SetMaxAutoHeight(0.7f);
			planetListWindow->fillBackground = false;
			PComponent label = planetListWindow->AddNewComponent(new Label("Planets",1.2f));
			{
				auto cfg = label->GetAnchorConfig();
				cfg.Top().Set(0);
				cfg.Left().Set(0);
				label->SetAnchorConfig(cfg);
			}
			foreach (planetList,p)
			{
				index_t myIndex = p - planetList.pointer();
				PButton button(new Button(*p,[myIndex]()
				{
					Network::Channels::loadPlanetByName.SendTo(Network::client, Protocol::LoadPlanetByName( planetList[myIndex],(UINT32)++sentVersion));
					HUD::WindowStack::Hide(planetListWindow);
					LoadServerWorld(false);
					lockedAwaitConfig = sentVersion;
				}));
				{
					auto cfg = button->GetAnchorConfig();
					cfg.Top().SetLastOf(planetListWindow,0.02f);
					cfg.axis[0].min.Set(0);
					cfg.axis[0].max.Set(0);
					button->SetAnchorConfig(cfg);
				}
				planetListWindow->AddComponent(button);
			}

			planetListWindow->FullAutoLayout(CLOCATION);
		}


		void AppendRuleButton(const Objects::Command::PCommandGroup&grp,const String&niceName, const String&iconName,const String&configName, UINT32 flag, const std::function<void(bool)>&onChange)
		{
			Objects::Command::PIconCommand cmd(new Objects::Command::CustomIconCommand(niceName,iconName,[flag,onChange]()
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
			[flag](bool)->Objects::CommandFlags
			{
				typedef Objects::CommandFlags F;

				F result = F::IsEnabled;
				
				if (gameConfig.gameFlags & flag)
					result |= F::IsPermaActive;
				return result;
			}));
			grp->Add(cmd);
			onChange(!!(gameConfig.gameFlags&flag));
			String help = FindDeclaration(configName);
			SetHint(*cmd, help);
		}

		void AppendRuleButton(const Objects::Command::PCommandGroup&grp,const String&niceName, const String&configName, UINT32 flag)
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

		void AppendDiscoveryButton(const Objects::Command::PCommandGroup&grp,Grid::WorldDiscovery disc)
		{
			Objects::Command::PBase cmd(new Objects::Command::CustomIconCommand(disc.ToString(),String("Discovery ")+disc.ToString(),[disc]()
			{
				if (gameConfig.worldDiscovery == (BYTE)disc && showDiscovery)
					return;
				showDiscovery = true;
				gameConfig.worldDiscovery = (BYTE)disc;
				Network::Channels::gameConfig.SendTo(Network::client,gameConfig);
				RefreshDiscovery();
			},
			[disc](bool)->Objects::CommandFlags
			{
				typedef Objects::CommandFlags F;

				F result = F::IsEnabled;
				
				if (gameConfig.worldDiscovery == (BYTE)disc)
					result |= F::IsPermaActive;
				return result;
			}));
			grp->Add(cmd);

		}


		void AppendGoalButton(const Objects::Command::PCommandGroup&grp,RuntimeConfig::GameGoal goal)
		{
			Objects::Command::PBase cmd(new Objects::Command::CustomIconCommand(goal.ToString(),String("Goal ")+goal.ToString(),[goal]()
			{
				if (gameConfig.gameGoal == (BYTE)goal)
					return;
				RuntimeConfig::gameGoal = goal;
				gameConfig.gameGoal = (BYTE)goal;
				UploadToServer();	//make sure server planet is up-to-date
				Network::Channels::gameConfig.SendTo(Network::client,gameConfig);
			},
			[goal](bool)->Objects::CommandFlags
			{
				typedef Objects::CommandFlags F;

				F result = F::IsEnabled;
				
				if (gameConfig.gameGoal == (BYTE)goal)
					result |= F::IsPermaActive;
				return result;
			}));
			grp->Add(cmd);
		}
			

		Objects::Command::PCommandGroup MakeIntButtons(const Objects::Command::PCommandGroup&group, int&value, const int&defValue, int min, int max)
		{
			Objects::Command::PVisualCommand number(new HUD::HexGrid::Custom::ValueReferenceCommand<int>(group->name+" ["+String(min)+".."+String(max)+"]",value));
			numberCommands << number;
			Objects::Command::PIconCommand decrease(new Objects::Command::IconCommand("Decrease",[&value,min,number]()
			{
				if (value > min)
				{
					value--;
					number->RasterizeImage();
					Update();
				}
			},[&value,min](bool){return value > min ? Objects::CommandFlags::IsEnabled : Objects::CommandFlags::None;}));
			SetHint(*decrease, "Decreases this setting by 1");
			Objects::Command::PIconCommand increase(new Objects::Command::IconCommand("Increase",[&value,max,number]()
			{
				if (value < max)
				{
					value++;
					number->RasterizeImage();
					Update();
				}
			},[&value,max](bool){return value < max ? Objects::CommandFlags::IsEnabled : Objects::CommandFlags::None;}));
			SetHint(*increase,"Increases this setting by 1");

			Objects::Command::PIconCommand decreaseFast(new Objects::Command::IconCommand("Decrease Fast",[&value,min,max,number]()
			{
				if (value > min)
				{
					value-=(max - min)/4;
					value = std::max(min,value);
					number->RasterizeImage();
					Update();
				}
			},[&value,min](bool){return value > min ? Objects::CommandFlags::IsEnabled : Objects::CommandFlags::None;}));
			SetHint(*decreaseFast, "Decreases this setting by "+String((max - min)/4));
			Objects::Command::PIconCommand increaseFast(new Objects::Command::IconCommand("Increase Fast",[&value,min,max,number]()
			{
				if (value < max)
				{
					value+=(max - min)/4;
					value = std::min(max,value);
					number->RasterizeImage();
					Update();
				}
			},[&value,max](bool){return value < max ? Objects::CommandFlags::IsEnabled : Objects::CommandFlags::None;}));
			SetHint(*increaseFast, "Decreases this setting by "+String((max - min)/4));

			Objects::Command::PIconCommand setMin(new Objects::Command::IconCommand("Set Min",[&value,min,number]()
			{
				if (value > min)
				{
					value = min;
					number->RasterizeImage();
					Update();
				}
			},[&value,min](bool){return value > min ? Objects::CommandFlags::IsEnabled : Objects::CommandFlags::None;}));
			SetHint(*setMin, "Sets this setting to the minimum possible value ("+String(min)+")");


			Objects::Command::PIconCommand setMax(new Objects::Command::IconCommand("Set Max",[&value,max,number]()
			{
				if (value < max)
				{
					value = max;
					number->RasterizeImage();
					Update();
				}
			},[&value,max](bool){return value < max ? Objects::CommandFlags::IsEnabled : Objects::CommandFlags::None;}));
			SetHint(*setMax, "Sets this setting to the maximum possible value ("+String(max)+")");

			group->Add(number);
			group->Add(decrease);
			group->Add(increase);
			group->Add(decreaseFast);
			group->Add(increaseFast);
			group->Add(setMin);
			group->Add(setMax);
			return group;
		}

		Objects::Command::PCommandGroup MakeFloatButtons(const Objects::Command::PCommandGroup&group, float&value, const float&defValue)
		{
			float min = value / 10.f;
			float max = value * 10.f;
			Objects::Command::PVisualCommand number(new HUD::HexGrid::Custom::ValueReferenceCommand<float>(group->name+" ["+String(min)+" - "+String(max)+"]",value));
			numberCommands << number;
			//Objects::Command::PIconCommand reset(new Objects::Command::CustomIconCommand("Reset","Undo",[&value,&defValue]()
			//{
			//	value = defValue;
			//	Update();
			//},[](bool){return Objects::CommandFlags::IsEnabled;}));
			//reset->customLabel << "Resets this setting to the current server value";
			Objects::Command::PIconCommand decrease(new Objects::Command::IconCommand("Decrease",[&value,min,number]()
			{
				if (value > min)
				{
					value /= 1.2f;
					value = std::max(value,min);
					number->RasterizeImage();
					Update();
				}
			},[&value,min](bool){return value > min ? Objects::CommandFlags::IsEnabled : Objects::CommandFlags::None;}));
			SetHint(*decrease, "Decreases this setting by 20%");
			Objects::Command::PIconCommand increase(new Objects::Command::IconCommand("Increase",[&value,max,number]()
			{
				if (value < max)
				{
					value *= 1.2f;
					value = std::min(value,max);
					number->RasterizeImage();
					Update();
				}
			},[&value,max](bool){return value < max ? Objects::CommandFlags::IsEnabled : Objects::CommandFlags::None;}));
			SetHint(*increase, "Increases this setting by 20%");

			group->Add(number);
			group->Add(decrease);
			group->Add(increase);
			return group;
		}

		Objects::Command::PCommandGroup MakeLayer(const String&name, Grid::ExtendedPlanetConfig::TLayerConfig&layer,const Grid::ExtendedPlanetConfig::TLayerConfig&refLayer)
		{
			namespace Command = Objects::Command;
			Objects::Command::PCommandGroup group(new Command::CommandGroup(name));
			//group->baseCoords = int2(1);
			SetHint(*MakeIntButtons(Command::AddChild(*group,"Base Height"),layer.baseHeight,refLayer.baseHeight, Grid::CellStack::BottomHeight,Grid::CellStack::TopHeight-1),
			"Defines the central height of this attribute. The effective altitude stretches symmetrically up and down from this value.")
			;
			SetHint(*MakeIntButtons(Command::AddChild(*group,"Dampening"),layer.dampening,refLayer.dampening, 0,Grid::CellStack::HalfRange),
			"Draws height values closer to the base height. Creates plateaus at central height level")
			;
			SetHint(*MakeFloatButtons(Command::AddChild(*group,"Height Scale"),layer.heightScale, refLayer.heightScale),
			"Scales extremes. The heigher this value the further the resulting heightmap will reach up and down")
			;
			SetHint(*MakeFloatButtons(Command::AddChild(*group,"Frequency"),layer.frequency, refLayer.frequency),
			"Scales the frequency of peaks. The heigher this value the higher the number of mountains at reduced horizontal extent each.")
			;
			return group;
		}


		template <typename T=BYTE>
		void MakeMaterialChoice(const Objects::Command::PCommandGroup&group, T&value, const T&defValue, MaterialDialog&dialog)
		{
/*			Objects::Command::PIconCommand reset(new Objects::Command::CustomIconCommand("Reset","Undo",[&value,defValue]()
			{
				value = defValue;
				Update();
			},[](bool){return Objects::CommandFlags::IsEnabled;}));
			reset->customLabel << "Resets this material to the current server value";
			*/
			Objects::Command::PBase choose(new Objects::Command::IconCommand("Choose Material",[&value,&dialog]()
			{
				dialog.SetText(value);
				dialog.onSelect = [&value](BYTE v)
				{
					value = v;
					Update();
				};
				HUD::WindowStack::Show(dialog.window); 
			},
			[](bool){return Objects::CommandFlags::IsEnabled;}
			));

//			group->Add(reset);
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
							HUD::Dialog::ShowMessage("Importing encountered warnings:\n"+implode('\n',warnings.pointer(),warnings.Count()));
						}
						LoadClientWorld(true);
					}
					else
						HUD::Dialog::ShowMessage("Planet configuration could not be parsed from clipboard:\n"+cfg.GetError());
				}
				else
					HUD::Dialog::ShowMessage("Planet configuration could not be loaded from clipboard. Chances are, you did not copy it to clipboard first");

			}
		}


		void	Create()
		{
			if (created)
				return;
			created = true;


			planetListWindow = Window::Create();
			RebuildPlanetList();

			XML::Container declarations;
			declarations.LoadFromFile("../server/config/declaration.xml");
			ParseDeclarations(declarations.root_node,"");


			fullScope.Create(true,true);
			noLiquids.Create(false,false);
			noUnknown.Create(false,true);

			namespace Command = Objects::Command;
			coreGroup.reset(new Objects::Command::CommandGroup("Core"));
			//coreGroup->baseCoords = int2(1);
			SetHint(*coreGroup,"The inner planetary core represents the center-most sphere content");
			ruleGroup.reset(new Objects::Command::CommandGroup("Rules"));
			//ruleGroup->baseCoords = int2(1);
			SetHint(*ruleGroup,"All common game rules");


			copyPasteGroup.reset(new Objects::Command::CommandGroup("Copy/Paste"));
			//copyPasteGroup->baseCoords = int2(1);
			SetHint(*copyPasteGroup,"Copy/paste current configuration to/from clipboard");

			{
				Objects::Command::PIconCommand cmd;

				cmd.reset(new Objects::Command::CustomIconCommand("Copy planet to clipboard (CTRL+C)","Copy/Paste",Clipboard::ExportPlanet,
				[](bool)->Objects::CommandFlags { return Objects::CommandFlags::IsEnabled;}));
				copyPasteGroup->Add(cmd);

				cmd.reset(new Objects::Command::CustomIconCommand("Paste planet from clipboard (CTRL+V)","Paste",Clipboard::ImportPlanet,
				[](bool)->Objects::CommandFlags { return Objects::CommandFlags::IsEnabled;}));
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
				VisualTerrain::Invalidate(); //force rebuild
			});
			Objects::Command::PCommandGroup goals = AddChild(*ruleGroup,"Game Goal");
			SetHint(*goals,FindDeclaration("/Rules/gameGoal"));
			for (index_t i = 0; i < RuntimeConfig::GameGoal::N; i++)
				AppendGoalButton(goals,RuntimeConfig::GameGoal::Reinterpret(i));
			Objects::Command::PCommandGroup disc = AddChild(*ruleGroup,"Initial Discovery","View");
			SetHint(*disc,FindDeclaration("/World/worldDiscovery"));
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

			MakeMaterialChoice(coreGroup,config.innerCoreType,serverConfig.innerCoreType,noUnknown);
			mountainGroup = MakeLayer("Mountains",config.mountains,serverConfig.mountains);
			SetHint(*mountainGroup,"Defines the extent and content of the mountain layer. The mountain layer stretches from the bottom up, where not overwritten by the extended planet core.");
			extendedCoreGroup = MakeLayer("Extended Core",config.extendedCore,serverConfig.extendedCore);
			SetHint(*extendedCoreGroup, "Defines the extent and content of the extended core layer. The extended core stretches from the bottom up, potentially overwriting content defined by the mountain layer.");
			MakeMaterialChoice(AddChild(*extendedCoreGroup,"Type","Terrain Content"),config.extendedCoreType,serverConfig.extendedCoreType, noUnknown);
			MakeMaterialChoice(AddChild(*mountainGroup,"Type","Terrain Content"),config.type,serverConfig.type, fullScope);
//			MakeMaterialChoice(mountainGroup->AddChild("Top Type", "Terrain Top" ),config.topType,refConfig.topType, fullScope);
			MakeIntButtons(AddChild(*mountainGroup,"Snow Height"),config.snowHeight,serverConfig.snowHeight,Grid::CellStack::BottomHeight,Grid::CellStack::TopHeight-1);

			{
				liquidGroup.reset(new Objects::Command::CommandGroup("Oceans"));
				//liquidGroup->baseCoords = int2(1);
				MakeMaterialChoice(AddChild(*liquidGroup,"Ocean Content","Ocean Type"),config.liquidCoverage,serverConfig.liquidCoverage,noUnknown);
				MakeIntButtons(AddChild(*liquidGroup,"Level","Base Height"),config.liquidHeight,serverConfig.liquidHeight,Grid::CellStack::BottomHeight,Grid::CellStack::TopHeight-1);
				MakeMaterialChoice(AddChild(*liquidGroup,"Ocean Outline"),config.liquidOutline,serverConfig.liquidOutline,noLiquids);
			}


			int at = 0;
			buttons << std::make_pair(int2(at++,0),
						Objects::Command::PBase(new Objects::Command::CustomIconCommand("Accept current server planet and config, and finish","Commit",[]()
						{
							
							if (clientWorldLoaded)
							{
								HUD::Dialog::Confirm("Upload changes and begin game?",[]()
								{
									UploadToServer();
									Network::client.SendSignal(Protocol::ChannelID::EndConfigurationMode);
								},
								[](){return Detail::isActive ? HUD::Dialog::CheckResult::OkayContinue : HUD::Dialog::CheckResult::Abort;});
							}
							else
								HUD::Dialog::Confirm("Accept current state and begin game?",[]()
								{
									Network::client.SendSignal(Protocol::ChannelID::EndConfigurationMode);
								},
								[](){return Detail::isActive ? HUD::Dialog::CheckResult::OkayContinue : HUD::Dialog::CheckResult::Abort;});
							//Update();
						},
						[](bool){return Objects::CommandFlags::IsEnabled;}
						))
					);
			//buttons << std::make_pair(int2(at++,0),
			//			Objects::Command::PBase(new Objects::Command::CustomIconCommand("Commit changes and rebuild server planet","Upload to Server",[]()
			//			{
			//				UploadToServer();
			//				//Update();
			//			},
			//			[](bool){
			//			return (flashUploadButton ? Objects::CommandFlags::SoftFlashing : Objects::CommandFlags::None) |  Objects::CommandFlags::IsEnabled;
			//			}
			//			))
			//		);
			//buttons	<< std::make_pair(int2(at++,0),
			//			Objects::Command::PBase(new Objects::Command::CustomIconCommand("Reset to server config and rebuild local planet","Download from Server",[]()
			//			{
			//				config = serverConfig;
			//				seed = serverSeed;
			//				Update();
			//			},
			//			[](bool){return Objects::CommandFlags::IsEnabled;}
			//			))
			//		);
			//buttons << std::make_pair(int2(at++,0),
			//			Objects::Command::PBase(new Objects::Command::CustomIconCommand("View server version","View Server Version",[]()
			//			{
			//				if (clientWorldLoaded)
			//				{
			//					LoadServerWorld(false);
			//					RestoreServerObjects();
			//				}
			//				else
			//				{
			//					LoadClientWorld();
			//				}
			//				//Update();
			//			},
			//			[](bool){
			//			return (!clientWorldLoaded ? Objects::CommandFlags::IsPermaActive : Objects::CommandFlags::None) | Objects::CommandFlags::IsEnabled;
			//			}
			//			))
			//		);
			buttons	<< std::make_pair(int2(at++,0),
						Objects::Command::PBase(new Objects::Command::CustomIconCommand("Randomize seeds and rebuild using current config","Randomize",[]()
						{
							RandomizeSeed();
						},
						[](bool){return Objects::CommandFlags::IsEnabled;}
						))
					);
			buttons	<< std::make_pair(int2(at++,0),
						Objects::Command::PBase(new Objects::Command::CustomIconCommand("Load Planet Preset","Planet Presets",[]()
						{
							if (!HUD::WindowStack::IsVisible(planetListWindow))
								HUD::WindowStack::Show(planetListWindow);
							else
								HUD::WindowStack::Hide(planetListWindow);
						},
						[](bool)
						{
							Objects::CommandFlags rs = Objects::CommandFlags::IsEnabled;
							//if (IsVisible(planetListWindow))
							//	rs |= Objects::CommandFlags::IsPermaActive;
							return rs;
						}
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
		seed = rng.NextSeed();
		Update();
	}

	UINT32	GetServerSeed()
	{
		return Detail::seed;
	}


	void	Update(const Grid::ExtendedPlanetConfig&cfg)
	{
		using namespace Detail;

		lastServerVersion = cfg.versionCounter;
		if (localVersion < lastServerVersion)
			localVersion = lastServerVersion;
		if (sentVersion < lastServerVersion)
			sentVersion = lastServerVersion;

		bool mustLoad = false;
		if (lockedAwaitConfig == cfg.versionCounter)
		{
			mustLoad = true;
			ASSERT_EQUAL__(lastServerVersion,sentVersion);
			lockedAwaitConfig = 0;
			LogEvent(EventSignificance::Important,"Received updated world config. Unlocking");
		}
		
		if (lastServerVersion == sentVersion)
		{
			if (RestoreServerWorld())
			{
				seed = cfg.lastSeed;
				if (!clientWorldBuilt)
				{
					config = cfg;
					foreach (numberCommands,n)
						(*n)->RasterizeImage();
				}
				serverConfig = cfg;
				if (mustLoad)
					config = cfg;
				//if (clientWorldLoaded)
				//	Update();
				//else
				ClientGrid::Apply(cfg);
				World::allowRecompilation = false;
			}
			else
				ASSERT__(!mustLoad);
		}
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
		localVersion = sentVersion = lastServerVersion = lockedAwaitConfig = 0;
		Detail::Create();
		HUD::HexGrid::Configure(Objects::selectedEntity);

	}

	bool	ViewingServerVersion()
	{
		using namespace Detail;
		return !clientWorldLoaded;
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
		HUD::HexGrid::ClearCommands();
		foreach (buttons,b)
			HUD::HexGrid::AddCommand(b->first+int2(2,1),b->second);
		return true;
	}

	void	SignalServerWorldUpdateCompleted()
	{
		using namespace Detail;
		if (!isActive)
			return;

		ASSERT__(!clientWorldLoaded);

		Detail::LoadServerWorld(true);
		for (index_t i = 0; i< Grid::WorldDiscovery::N; i++)
		{
			clientWorld[i] = serverWorld[i];
			clientWorld[i].isLoaded = false;
		}
	}

	bool	RestoreServerWorld()
	{
		if (!Detail::isActive)
			return true;
		if (Detail::lockedAwaitConfig)
			return false;
		if (Detail::lastServerVersion != Detail::localVersion)
			return false;
		Detail::LoadServerWorld(true);
		//World::allowRecompilation = false;
		return true;
	}
	void	RestoreServerObjects()
	{
		RestoreServerWorld();
		using namespace Detail;
		if (serverObjects.IsNotEmpty())
		{
			serverObjects.Export();
			World::actual.FlushAndValidateSensory(1);
		}
		HUD::HexGrid::RefreshContacts();
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

	void	Tick()
	{
		if (Detail::isActive && Detail::localVersion > Detail::sentVersion && timer.GetSecondsSince(Detail::lastLocalChange) > 1)
		{
			Detail::sentVersion = Detail::localVersion;
			Detail::UploadToServer();
		}
	}



	void	UpdatePlanetList(Array<String>&fleetingData)
	{
		if (Detail::isActive)
		{
			Detail::planetList.adoptData(fleetingData);
			Detail::RebuildPlanetList();
		}
	}


	void	Create()
	{
		GlobalEvents::frameBegin << Tick;
	}

}

#endif

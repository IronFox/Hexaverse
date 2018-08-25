#include "menu.h"
#include "../tutorial.h"
#include "common.h"
#include <serverProcess.h>
#include "../../network.h"
#include "../scene/shadedRenderInstruction.h"
#include "../../clientRuntimeConfig.h"
#include "../../../version.h"
#include "../../audio/audioInterface.h"
#include "../../audio/music.h"
#include "../../audio/newRoundSound.h"
#include "../objectSelection.h"
#include "guiStatistics.h"
#include "../scene/explosions.h"
#include "guiFactionTable.h"
#include <io/monitoredProcess.h>
#include "../../configurationMode.h"
#include "../../io.h"
#include "../../cameraControl.h"
#include "../../scriptedAI.h"
#include "hexGrid.h"
#include "../commandDefinitions.h"
#include "../autoDocumentation.h"

bool	IsFullScreen();
void	SetFullScreen(bool isFull);
bool	ValidProfileCharacter(char c);



namespace HUD
{

namespace Menu
{


	/*virtual override*/ void				Menu::OnScreenSizeOrUIScaleChange()
	{
		LogEvent(EventClass::Preset::gui,"Menu["+String(this)+"/"+myTitle+"]::OnScreenSizeOrUIScaleChange()");
		Super::OnScreenSizeOrUIScaleChange();
	}

	HUD::Component::PButton				Menu::AddCommand(const String&name, const std::function<void()>&execute, const std::function<bool()>&check /*= std::function<bool()>()*/, bool autoHide/*=true*/)
	{
		HUD::Component::PButton button(new HUD::Component::Button(name,
			[execute,this,autoHide]()
			{
				if (autoHide)
					WindowStack::CloseAllUpTo(To<HUD::Component::Window>::Cast(this->shared_from_this()));
				execute();
			},
			check
		));
		if (mainPanel->CountComponents() > 0)
		{
			To<HUD::Component::Button>::Cast(mainPanel->GetLastChild())->SetCutCorners(M::Quad<bool>(true,false,true,false));
			button->SetCutCorners(M::Quad<bool>(true,true,true,false));
		}
		mainPanel->AnchorBelowPrevious(CLOCATION,button,0.f,HUD::Component::Distance::GameWindow( Super::components.IsEmpty() ? 0.f : 0.015f ));

		LogEvent(EventClass::Preset::gui,"Menu["+String(this)+"/"+myTitle+"]::AddCommand("+name+", ...)");
		button->SetBackgroundTone((float3)0.7f);
		button->SetBackgroundOpacity(0.9f);
		button->SetEdgeColor(float4(button->GetBackgroundTone(),0.95f));
		button->SetEdgeOpacity(1.f);
		button->SetEdges(true);
		button->SetVerticalPadding(0.25f);
		button->SetEnabledTextAlwaysLit(true);
		return button;
	}

	void		Menu::Setup(float initialAbsoluteWidth, const String&title, bool escapeCloses)
	{
		canBeImplicitlyClosed = escapeCloses;
		Super::ClearComponents();
		Super::SetEdges(false);
		Super::SetBackgroundOpacity(0.65f);
		Super::SetBackgroundTone(Super::GetBackgroundTone() * 1.5f);
		hintLabel.reset();
		myTitle = title;
		Super::Anchor().CenterHorizontally(initialAbsoluteWidth);

		if (title.IsNotEmpty())
		{
			HUD::Component::PLabel label(new HUD::Component::Label());
			label->SetText(title);
			label->SetVerticalPadding( 0.1f );
			label->SetBackground(true);
			Super::AnchorBelowPrevious(CLOCATION,label,0.005f,0.005f);
			label->SetCutCorners(M::Quad<bool>(true,false,true,true));
			label->SetVerticalPadding(0.2f,0);
			label->SetBackgroundTone((float3)0.75f);
			label->SetBackgroundOpacity(0.9f);
			label->SetEnabledTextAlwaysLit(true);
		}

		if (mainPanel)
			mainPanel->ClearComponents();
		else
		{
			mainPanel.reset(new HUD::Component::Panel());
			mainPanel->SetEdges(false);
			mainPanel->SetBackground(false);
			mainPanel->SetAutomaticHeight(true);
			mainPanel->SetMaxAutoHeight(0.5f);
		}

		Super::AnchorBelowPrevious(CLOCATION,mainPanel,0.005f,0.005f);



		if (escapeCloses)
		{
			hintLabel.reset(new HUD::Component::Label());
			hintLabel->SetLocalFontScale(0.75f);
			hintLabel->SetText("Press Escape to close");
			hintLabel->SetVerticalPadding( 0.1f );
			Super::AnchorBelowPrevious(CLOCATION,hintLabel,0.005f,0.f);
		}
		else
			mainPanel->SetAutoHeightSpacing(0.01f);
		LogEvent(EventClass::Preset::gui,"Menu["+String(this)+"]::Setup("+String(initialAbsoluteWidth)+","+myTitle+")");
	}




	PMenu	main,
			help,
			login,
			game,
			startServer,
			saveGames,
			launch;
	Component::PWindow	options;


	namespace Configuration
	{

		PathString	outConfigPath;

		DeltaWorks::Configuration*config;

		void	Assign(DeltaWorks::Configuration&cfg, const PathString&userConfigPath)
		{
			config = &cfg;
			outConfigPath = userConfigPath;
		}


		void	Update(const String&path, const String&value)
		{
			config->Define(path,value);
			config->SaveToFile(outConfigPath,PRODUCT_NAME);
		}

	}


	static const float MinTextWidth=0.2f;	//measured in multiples of a whole screen width


	static Component::PLabel AddOption(const String&name, const Component::PComponent&c)
	{
		LogEvent(EventClass::Preset::gui,"CreateOptionsMenu()["+String(options.get())+"].AddOption("+name+", ...)");

		Component::PPanel p(new Component::Panel());
		p->Anchor().Top();
		Component::PLabel l(new Component::Label(name));
		p->AddComponent(l,CLOCATION);
		p->AddComponent(c,CLOCATION);

		l->SetPadding( 0 );



		l->SetBackground(false);
		l->alignCenter = false;
		l->Anchor().CenterVertically().Left(0,MinTextWidth);
		c->Anchor().FillHorizontally(MinTextWidth,0.01f).CenterVertically();

		p->SetAutomaticHeight(true);
		p->SetBackground(false);

		options->AnchorBelowPrevious(CLOCATION,p);
		return l;
	}

	static Component::PSlider AddOptionSlider(const String&name, float min, float max, float slide, float initial, const std::function<void(float&)>&onChange, const std::function<String()>&toString)
	{
		Component::PSlider s(new Component::Slider());
		s->Setup(initial-min,max-min, slide);
		auto label = AddOption(name,s);
		std::weak_ptr<Component::Label> w = label;
		s->SetOnChange([onChange,toString,w,name,min](float&f0)
		{
			float f1 = f0 + min;
			LogEvent(EventSignificance::Common,"Slider change '"+name+"' -> "+String(f1));
			onChange(f1);
			LogEvent(EventSignificance::Common,"Slider change '"+name+"' done -> "+String(f1));
			f0 = f1 - min;
			const auto s = w.lock();
			if (s)
				s->SetText(name+" ("+toString()+")");
		});
		label->SetText(name+" ("+toString()+")");
		return s;
	}

	static Component::PCheckbox AddCheckBox(const String&name,bool initial, const std::function<void(bool)>&onChange)
	{	
		Component::PCheckbox c(new Component::Checkbox("",initial,[onChange,name](bool b)
		{
			LogEvent(EventSignificance::Common,"Checkbox change '"+name+"' -> "+String(b));
			onChange(b);	
			LogEvent(EventSignificance::Common,"Checkbox change '"+name+"' done");
		}));
		AddOption(name,c);
		return c;
	}

	static void CreateOptionsMenu()
	{
		auto menu = options = Component::Window::Create();
		LogEvent(EventClass::Preset::gui,"CreateOptionsMenu(): "+String(menu.get()));
		menu->logFullAutoLayout = true;
		menu->Anchor().FillHorizontally(0.1f,0.1f).Top(0.1f);
		menu->AnchorBelowPrevious(CLOCATION,new Component::Label("Options",2.f));
		
		AddOptionSlider("UI Scale", 0.5f, 2.f, 0.125f,UI::GetScale(), [](float&q)
		{
			q = M::Round(q * 8.f)*0.125f;
			UI::SetScale(q);
			Configuration::Update("runtime/uiScale",q);
		},
		[]()->String
		{
			return String(M::Round(UI::GetScale() * 100))+"%";
		}
		)->SetupMarkers(0.25f,true);


		AddOptionSlider("Shadow Quality", 0, ShadedInstruction::Quality::N-1, 1, ShadedInstruction::Quality::N-1 - ShadedInstruction::chosenQuality, [](float&q)
			{
				auto old = ShadedInstruction::chosenQuality;
				ShadedInstruction::chosenQuality.Load(ShadedInstruction::Quality::N-1 -M::Round(q));
				q = ShadedInstruction::Quality::N-1 - ShadedInstruction::chosenQuality;
				if (old != ShadedInstruction::chosenQuality)
				{
					Configuration::Update("runtime/shadowQuality",ShadedInstruction::chosenQuality.ToString());
					GlobalEvents::shadowQualityChanged();
				}
			},
			[]() -> String
			{
				return ShadedInstruction::chosenQuality.ToString();
			}
		);
		const auto modes = display.GetMSAAModes();
		if (modes.Count() > 1)
		{
			AddOptionSlider("Anti-Aliasing", 0, modes.Count()-1, 1,display.GetCurrentMSAAMode(), [](float&q)
			{
				auto old = display.GetCurrentMSAAMode();
				const index_t mode = Math::Max(0.f,M::Round(q));
				if (old != mode)
				{
					LogEvent(EventSignificance::Common,"MSAA change to mode "+String(mode)+" (x"+String(display.GetMSAARate(mode))+ ")");
					display.SetMSAAMode(mode);
					Configuration::Update("runtime/fsaa",display.GetCurrentMSAARate());
				}
				q = display.GetCurrentMSAAMode();
			},
			[]()->String
			{
				BYTE samples = display.GetCurrentMSAARate();
				return samples > 1 ? String(samples)+"x" : "off";
			}
			);
		}


		AddCheckBox("Vertical Sync",display.IsVSyncActive(),[](bool enabled)
		{
			bool old = display.IsVSyncActive();
			if (old != enabled)
			{
				display.SetVSyncActive(enabled);
				Configuration::Update("runtime/vsync",enabled ? 1 : 0);
			}
		});

		{
			Component::PCheckbox box = AddCheckBox("Full Screen (Alt+Enter)",IsFullScreen(),[](bool enabled)
			{
				bool old = IsFullScreen();
				if (old != enabled)
					SetFullScreen(enabled);
			});

			GlobalEvents::onFullscreenChange << [box](bool fullscreen)
			{
				box->SetChecked(fullscreen,false);
			};
		}

		
		static const float minParticles = 500,
							maxParticles = 100000;
		static const float expBase = 1.5f;
		static const float	minExponent = floor(log(minParticles) / log(expBase)),
							maxExponent = ceil(log(maxParticles) / log(expBase));
		const float		current = log(Explosions::maxConcurrentParticles) / log(expBase);
		
		AddOptionSlider("Max Explosion Particles", minExponent, maxExponent, 2, current, [](float&q)
			{
				count_t nVal = pow(expBase,q);
				if (Explosions::maxConcurrentParticles != nVal)
				{
					Explosions::maxConcurrentParticles = nVal;
					Configuration::Update("runtime/maxExplosionParticles",Explosions::maxConcurrentParticles);
				}
			},
			[]() -> String
			{
				return Explosions::maxConcurrentParticles;
			}
		);

		AddCheckBox("Terrain Camera Intersection Correction (slow)",Camera::terrainIntersectionTest,[](bool enabled)
		{
			bool old = Camera::terrainIntersectionTest;
			if (old != enabled)
			{
				Camera::terrainIntersectionTest = enabled;
				Configuration::Update("runtime/terrainCameraIntersectionCorrection",enabled);
			}
		});

		if (AudioInterface::IsActive())
		{
			AddOptionSlider("Sound Volume", 0, 1.f, 0.25f,AudioInterface::GetMasterVolume(), [](float&q)
			{
				auto old = AudioInterface::GetMasterVolume();
				if (old != q)
				{
					AudioInterface::SetMasterVolume(q);
					Configuration::Update("runtime/soundVolume",q);
				}
			},
			[]()->String
			{
				return String(M::Round(AudioInterface::GetMasterVolume() * 100))+"%";
			}
			)->SetupMarkers(0.25f,false);

			AddCheckBox("Mute sound when tabbed out",AudioInterface::MutingOnWindowFocusLoss(), [](bool enabled)
			{
				auto old = AudioInterface::MutingOnWindowFocusLoss();
				if (old != enabled)
				{
					AudioInterface::SetMuteOnWindowFocusLoss(enabled);
					Configuration::Update("runtime/muteOnWindowFocusLoss",enabled);
				}
			});

			AddCheckBox("Play sound on round begin",NewRoundSound::IsEnabled(), [](bool enabled)
			{
				auto old = NewRoundSound::IsEnabled();
				if (old != enabled)
				{
					NewRoundSound::SetEnabled(enabled);
					Configuration::Update("runtime/playSoundOnNewRound",enabled);
				}
			});

			AddOptionSlider("Music Volume", 0, 1.f, 0.25f,Music::GetVolume(), [](float&q)
			{
				auto old = Music::GetVolume();
				if (old != q)
				{
					Music::SetVolume(q);
					Configuration::Update("runtime/musicVolume",q);
				}
			},
			[]()->String
			{
				return String(M::Round(Music::GetVolume() * 100))+"%";
			}
			)->SetupMarkers(0.25f,false);
			
			AddCheckBox("Mute music during gameplay",!Music::PlayMusicDuringGameplay(),[](bool enabled)
			{
				bool old = !Music::PlayMusicDuringGameplay();
				if (old != enabled)
				{
					Music::SetPlayMusicDuringGameplay(!enabled);
					Configuration::Update("runtime/musicDuringGameplay",!enabled);
				}
			});


		}

		AddCheckBox("Auto-Queue Unit Commands",!Objects::ShiftMeansQueue(),[](bool enabled)
		{
			bool old = !Objects::ShiftMeansQueue();
			if (old != enabled)
			{
				Objects::SetShiftMeansQueue(!enabled);
				Configuration::Update("runtime/alwaysQueueUnitCommands",enabled);
			}
		});

		AddCheckBox("Debug Tools for Scripted AI", ScriptedAI::visualDebugActive, [](bool enabled)
		{
			bool old = ScriptedAI::visualDebugActive;
			if (old != enabled)
			{
				ScriptedAI::visualDebugActive = enabled;
				Configuration::Update("runtime/aiDebugMode", enabled);

				Objects::Commands::RedefineGlobalCommands();
				HUD::HexGrid::Configure(Objects::selectedEntity);
			}
		});

		{
			Component::PButton button(new Component::Button("OK",[](){WindowStack::Hide(options);}));
			//button->LockIdealWidth();
			options->AnchorBelowPrevious(CLOCATION,button);
			button->SetCenteredWidth(HUD::Component::Distance::GameWindow( 0.5f ));
		}

		options->SetAutoHeightSpacing(0.02f);

		options->SetAutomaticHeight(true);
		options->FullAutoLayout(CLOCATION);
	}

	static void CreateGameMenu()
	{
		game = Menu::Create();
		game->Setup(0.3f,"Game",true);

		game->AddCommand("Change Nickname",HUD::Dialog::QueryNickname);
		game->AddCommand("Abandon faction",Network::AbandonCurrentFaction,[](){return Network::amOnline && Network::haveFaction && RuntimeConfig::phase != RuntimeConfig::Phase::EndOfGame;});
		game->AddCommand("Show factions (Tab)", FactionTable::Show,[](){return Network::amOnline;});
		game->AddCommand("Show statistics", Statistics::Show,[](){return Network::amOnline && RuntimeConfig::phase == RuntimeConfig::Phase::EndOfGame;});
		game->AddCommand("Save game as...",[]()
		{
			Dialog::Query("Please specify a name","","ABCDEFGHIJKLMNOPQRSTUVWXZYabcdefghijklmnopqrstuvwxyz0123456789 ",[](const String&candidate)->bool{return candidate.Length() > 3 && candidate.Length() < 16 && !candidate.BeginsWith(' ') && !candidate.EndsWith(' ');},
				[](const String&name)
			{
				PathString location = RuntimeConfig::sharedConfig.Lookup("saves/"+name+RuntimeConfig::savFileExtension);
				if (FileSystem::IsFile(location))
				{
					Dialog::Confirm("Savegame '"+name+"' already exists. Replace?",[name]()
					{
						if (Network::client.IsConnected() && Network::client.IsLocalhost())
						{
							Network::Channels::SaveAs.SendTo(Network::client,name);
						}
					});
				}
				else
					if (Network::client.IsConnected() && Network::client.IsLocalhost())
					{
						Network::Channels::SaveAs.SendTo(Network::client,name);
					}


			});

		},[](){return Network::client.IsConnected() && Network::client.IsLocalhost();});
	}

	static void	CreateHelpMenu()
	{
		auto menu = help = Menu::Create();
		menu->Setup(0.3f,"Help",true);
		menu->AddCommand("View Online Manual",[]()
		{
			OpenDocumentExternally("https://www.hexaverse.net/html.php");
		});

		menu->AddCommand("Show Basic Controls",
			[]()
			{
				Tutorial::ShowMessage("Controls",true,true);
			});
		menu->AddCommand("Revisit All Tutorial Messages",
			[]()
			{
				HUD::Dialog::Confirm("You are about to reset the game's memory of tutorial messages you have been shown. All messages will be shown again where considered appropriate.\n"
								"Continue?",
					[]()
					{
						Tutorial::FlushMemory();
					});
			});
		menu->AddCommand("Start Interactive Tutorial",
			Tutorial::StartTutorialServer,
			[](){return !Network::client.IsConnected() && !Network::client.IsAttemptingToConnect();});
		
		menu->AddCommand("Report Bug",
			[]()
			{
				SetFullScreen(false);
				Logging::CallBugReporter();
			}	
		);

		menu->AddCommand("View Changelog",[]()
		{
			FileSystem::File f;
			if (FileSystem::Folder().FindFile("..\\changelog.txt",f))
				OpenDocumentExternally(f.GetLocation());
		});

		menu->AddCommand("View Third Party Sources",[]()
		{
			FileSystem::File f;
			if (FileSystem::Folder().FindFile("..\\info.txt",f))
				OpenDocumentExternally(f.GetLocation());
		});

		menu->AddCommand("About", []()
		{
			String text = PRODUCT_NAME " v" PRODUCT_VERSION "\n"
				"Compiled "+String(__DATE__)+" "+String(__TIME__) +  "\n"
				"(c) 2014-"+String(__DATE__).GetLastWord()+" Stefan Elsen\n"
				"www.hexaverse.net"
			;
			//static const String line = "(C) Stefan Elsen; v" PRODUCT_VERSION "; Compiled "+String(__DATE__)+" "+String(__TIME__);


			Dialog::ShowImageMessage("about.png",text);

		});


		/*{
			float2 margin = help->GetOutlineMargin();
			margin.y *= 10.f;
			help->SetOutlineMargin(margin);
		}*/
		//help->dbgTrack = true;
		//help->FullAutoLayout();
	}

	static void CreateMainMenu()
	{
		main = Menu::Create();
		main->Setup(0.25f,"Main",true);

		main->AddCommand("Return to Server List",
		[]()
		{
			ConfigurationMode::SetActive(false);
			Network::client.Disconnect();
		},
		[](){return Network::client.IsConnected() || ConfigurationMode::IsActive();});
		main->AddCommand("Game...",[](){WindowStack::Show(game);},std::function<bool()>(),false);
		main->AddCommand("Launch...",[](){WindowStack::Show(launch);},std::function<bool()>(),false);
		
		main->AddCommand("Help...",
			[]()
			{
				WindowStack::Show(help);
			},
			std::function<bool()>(),
			false
			);
		main->AddCommand("Options",
			[]()
			{
				WindowStack::Show(options);
			});

		#ifdef _DEBUG
			main->AddCommand("Update Automatic Documentation...",
			[]()
			{
				if (!Network::client.IsConnected() || !Network::gameHasBegun)
					Dialog::ShowMessage("Please connect to a server to continue");
				else
				{
					Dialog::Query("Enter target documentation directory (must exist)","../documentation","abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_ ",[](const String&str)
					{
						return FileSystem::IsDirectory(PathString(str));
					},
					[](const String&str)
					{
						if (!Network::client.IsConnected() || !Network::gameHasBegun)
							Dialog::ShowMessage("Please connect to a server to continue");
						else
							AutoDocumentation::Run(FileSystem::Folder().FindDirectory(PathString(str)));
					});
				}

			});

		#endif

		main->AddCommand("Quit to Desktop",[](){HUD::Dialog::Confirm("Return to Windows?",[](){application_shutting_down = true;},[](){return HUD::Dialog::CheckResult::OkayContinue;});});
		//main->FullAutoLayout();
	}

	static void CreateLaunchMenu()
	{
		auto menu = launch = Menu::Create();
		menu->Setup(0.3f,"Launch...",true);

		menu->AddCommand("Local server control console",[]()
		{
			SetFullScreen(false);
			MonitoredProcess	process(false);
			if (Network::amOnline && Network::client.IsLocalhost())
				process.Start("../ControlConsole/","ControlConsole" PLATFORM_TARGET_NAME_EXTENSION_STR  ".exe",PathString(" -p "+String(Network::connectionBasePort)));	//start and abandon
			else
				process.Start("../ControlConsole/","ControlConsole" PLATFORM_TARGET_NAME_EXTENSION_STR ".exe","");	//start and abandon, without port parameter since we don't know any
		});

		menu->AddCommand("New client with custom profile",[]()
		{
			Dialog::Query("Please enter a profile name to continue (empty for none).\n"
				"Servers allow one faction per profile, enabling multiple factions controlled by each one instance of the same Hexaverse installation.\n"
				"Make sure to not reuse the same profile for more than one open client instance at a time."
				,"",
				"abcdefghijklmnopqrstuvwxyz_0123456789",[](const StringRef&str){return true;},[](const String&str)
			{
				SetFullScreen(false);
				MonitoredProcess	process(false);
				PathString parameters = PathString("profile="+str);
				if (Network::client.IsConnected())
					parameters += PathString(" serverURL="+ToString(Network::lastServerAddress));


				count_t len = 0;
				Buffer<PathString::char_t> execPath; 
				do
				{
					execPath.AppendRow(MAX_PATH);
					len = GetModuleFileNameW(nullptr, execPath.GetPointer(), (DWORD)execPath.GetLength());
				}
				while (len >= execPath.GetLength());

				process.Start("./",execPath.GetPointer(),parameters);
			});
		});

		menu->AddCommand("Server Manager",[]()
		{
			SetFullScreen(false);
			MonitoredProcess	process(false);
			process.Start("../ServerManager","ServerManager.exe","");
		});


	}


	static void AddFactionCreationChoice(const String&line, UINT32 typeFlags, UINT32 supportFactionFlag)
	{
		login->AddCommand(line,
		[typeFlags,supportFactionFlag]()
		{
			if (Network::HaveFactionKeyForCurrentWorld() && typeFlags != Protocol::FactionType::SpectatorFaction)
			{
				HUD::Dialog::Confirm("A faction key is known for this world ("+Network::worldIDString+"). If you create a new faction, this key will be lost, and control of your old faction will irreversible be abandoned. Please confirm.",
				[typeFlags](){
					Network::AttemptFactionLogin(true,typeFlags);
					WindowStack::Hide(login);
				},[supportFactionFlag]()
				{
					if (!Network::amOnline || Network::haveFaction || !(supportFactionFlag & Network::gameFlags))
						return HUD::Dialog::CheckResult::Abort;
					if (RuntimeConfig::phase != RuntimeConfig::Phase::CommandPhase)
						return HUD::Dialog::CheckResult::OkayNotRightNow;
					return HUD::Dialog::CheckResult::OkayContinue;
				});
			}
			else
			{
				Network::AttemptFactionLogin(true,typeFlags);
				WindowStack::Hide(login);
			}
		},[supportFactionFlag](){return Network::amOnline && !Network::haveFaction && (Network::gameFlags & supportFactionFlag) 
			&& (
					RuntimeConfig::IsInAnyCommandPhase() 
						|| RuntimeConfig::phase  == RuntimeConfig::Phase::AnimationPhase 
						|| RuntimeConfig::phase  == RuntimeConfig::Phase::EndOfGame
				)
			;},false);
	}

	static void CreateLoginMenu()
	{
		login = Menu::Create();
		login->Setup(0.25f,"Login",false); 	//once visible, must explicitly close

		login->AddCommand("Continue existing faction",[](){Network::AttemptFactionLogin(false,Protocol::FactionType::ContinueOnly);},
		[]()
		{
			return Network::amOnline && !Network::haveFaction 
			#ifndef _DEBUG
			&& Network::HaveFactionKeyForCurrentWorld() 
			#endif
			&& RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase;
		}
		);
		AddFactionCreationChoice("Spectate",Protocol::FactionType::SpectatorFaction, Protocol::GameFlag::SupportSpectatorFactions);
		AddFactionCreationChoice("Create new faction",Protocol::FactionType::RegularFaction, Protocol::GameFlag::SupportRegularFactions);
	}


	struct TSaveGame
	{
		String title;
		PathString path;
		FileSystem::ftime_t time;

		friend void swap(TSaveGame&a, TSaveGame&b)
		{
			a.title.swap(b.title);
			a.path.swap(b.path);
			std::swap(a.time,b.time);
		}
	};

	static void LookupSaveGames(Buffer0<TSaveGame>&saves,FileSystem::FolderScan scan, const String&path)
	{
		FileSystem::File f;
		while (scan.NextFile(RuntimeConfig::savFileExtension,f))
		{
			String path2 = path.IsNotEmpty() ? path +"/" : "";
			auto&e = saves.Append();
			e.title = path2 + String(f.GetInnerName());
			e.path = f.GetLocation();
			e.time = f.GetModificationTime();
		}

		scan.Rewind();
		while (scan.NextFolder(f))
		{
			String path2 = path.IsNotEmpty() ? path +"/" : "";
			LookupSaveGames(saves,f,path2 + String(f.GetInnerName()));
		}
	}


	static bool RefreshSaveGames()
	{
		if (!saveGames)
			saveGames = Menu::Create();
		auto menu = saveGames;
		menu->Setup(0.25f,"Start Server with Saved Game",true);

		Buffer0<TSaveGame> saves;
		LookupSaveGames(saves, FileSystem::Folder(RuntimeConfig::sharedConfig.Lookup("saves")), "");
		if (saves.IsEmpty())
			return false;
		foreach (saves,s)
		{
			PathString p = s->path;
			time_t t = s->time;
			struct tm *tm = localtime(&t);
			char date[64];
			strftime(date, sizeof(date), "%Y-%m-%d %H:%M", tm);

			menu->AddCommand(s->title+"\n"+date,[p]()
			{
				ServerProcess::Begin(true,false,"-l \""+p+"\"");
				HUD::WindowStack::SignalSituationChange();
			},[](){return !Network::amOnline;});
		}
		return true;
	}


	static void CreateStartServerMenu()
	{
		auto menu = startServer = Menu::Create();
		menu->Setup(0.25f,"Start Server",true);
		menu->AddCommand("Configure visually...",[](){ConfigurationMode::SetActive(true);},[](){return !Network::amOnline;});
		menu->AddCommand("Start using current configuration",
			[]()
		{
			ServerProcess::Begin(true,false);
			HUD::WindowStack::SignalSituationChange();
		},[](){return !Network::amOnline;});
		menu->AddCommand("Load saved game...",
			[]()
		{
			if (RefreshSaveGames())
				WindowStack::Show(saveGames);
			else
				Dialog::ShowMessage("Apparently, there are no save games to load. Please try again later.");
		});
		menu->AddCommand("Start Interactive Tutorial",
			Tutorial::StartTutorialServer,
			[](){return !Network::client.IsConnected() && !Network::client.IsAttemptingToConnect();});
	}

	void Create()
	{
		CreateOptionsMenu();
		CreateHelpMenu();
		CreateMainMenu();
		CreateLaunchMenu();
		CreateLoginMenu();
		CreateGameMenu();
		CreateStartServerMenu();

		GlobalEvents::applicationTerminate << []()
		{
			main.reset();
			launch.reset();
			help.reset();
			login.reset();
			options.reset();
			startServer.reset();
		};
	}

}
}

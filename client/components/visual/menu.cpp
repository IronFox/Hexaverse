#include "menu.h"
#include "tutorial.h"
#include "../serverProcess.h"
#include "../network.h"
#include "../localInterface.h"
#include "../clientEvents.h"
#include "shadedRenderInstruction.h"
#include "../clientRuntimeConfig.h"
#include "../../version.h"
#include "../audioInterface.h"
#include "objectSelection.h"
#include "guiStatistics.h"

bool	IsFullScreen();
void	SetFullScreen(bool isFull);



namespace Menu
{
	Dialog::PMenu	main,help,login,game;
	GUI::PWindow	options;


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


	static GUI::PLabel AddOption(const String&name, const GUI::PComponent&c)
	{
		GUI::PPanel p(new GUI::Panel());
		GUI::PLabel l(new GUI::Label(name));
		p->AddComponent(l);
		p->AddComponent(c);

		l->SetVerticalPadding( 0 );
		l->fillBackground = false;
		l->alignCenter = false;
		{
			auto mod = l->ModifyAnchorConfig();
			//mod.Top().Set(0);
			mod.Top().SetAuto();
			mod.Bottom().SetAuto();
			mod.Left().Set(0);

		}
		l->SetSize(float2(MinTextWidth,0));
		c->AnchorTop(0);
		{
			auto mod = c->ModifyAnchorConfig();
			mod.Left().Set(MinTextWidth,GUI::TAnchor::RelativeToScreen);
			mod.Right().Set(0.01f);
			mod.Top().SetAuto();
			mod.Bottom().SetAuto();
		}

		p->SetAutomaticHeight(true);
		p->fillBackground = false;

		options->AnchorBelowPrevious(p);
		return l;
	}

	static GUI::PSlider AddOptionSlider(const String&name, float min, float max, float slide, float initial, const std::function<void(float&)>&onChange, const std::function<String()>&toString)
	{
		GUI::PSlider s(new GUI::Slider());
		s->Setup(initial-min,max-min, slide);
		auto label = AddOption(name,s);
		std::weak_ptr<GUI::Label> w = label;
		s->SetOnChange([onChange,toString,w,name,min](float&f0)
		{
			float f1 = f0 + min;
			onChange(f1);
			f0 = f1 - min;
			const auto s = w.lock();
			if (s)
				s->SetText(name+" ("+toString()+")");
		});
		label->SetText(name+" ("+toString()+")");
		return s;
	}

	static GUI::PCheckbox AddCheckBox(const String&name,bool initial, const std::function<void(bool)>&onChange)
	{	
		GUI::PCheckbox c(new GUI::Checkbox("",initial,onChange));
		AddOption(name,c);
		return c;
	}

	static void CreateOptionsMenu()
	{
		options = GUI::Window::Create();
		options->SetAutoCenter(false);
		options->FillHorizontally(0.1f);
		options->AnchorTop(0.1f);
		options->AnchorBelowPrevious(new GUI::Label("Options",2.f));
		
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
				display.SetMSAAMode(Math::Max(0.f,M::Round(q)));
				q = display.GetCurrentMSAAMode();
				if (old != display.GetCurrentMSAAMode())
					Configuration::Update("runtime/fsaa",display.GetCurrentMSAARate());
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
			GUI::PCheckbox box = AddCheckBox("Full Screen (Alt+Enter)",IsFullScreen(),[](bool enabled)
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


		if (AudioInterface::isActive)
			AddOptionSlider("Sound Level", 0, 1.f, 0.25f,AudioInterface::GetMasterVolume(), [](float&q)
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

		AddCheckBox("Auto-Queue Unit Commands",!Objects::ShiftMeansQueue(),[](bool enabled)
		{
			bool old = !Objects::ShiftMeansQueue();
			if (old != enabled)
			{
				Objects::SetShiftMeansQueue(!enabled);
				Configuration::Update("runtime/alwaysQueueUnitCommands",enabled);
			}
		});

		{
			GUI::PButton button(new GUI::Button("OK",[](){GUI::Hide(options);}));
			//button->LockIdealWidth();
			options->AnchorBelowPrevious(button);
			button->SetCenteredWidth(0.5f,GUI::TAnchor::RelativeToScreen);
		}

		options->SetAutoHeightSpacing(0.02f);

		options->SetAutomaticHeight(true);
		options->FullAutoLayout();
	}

	static void CreateGameMenu()
	{
		game = Dialog::Menu::Create();
		game->Setup(0.3f,"Game");

		game->AddCommand("Change Nickname",LocalApplicationInterface::QueryNickname);
		game->AddCommand("Abandon faction",Network::AbandonCurrentFaction,[](){return Network::amOnline && Network::haveFaction && RuntimeConfig::phase != RuntimeConfig::Phase::EndOfGame;});
		game->AddCommand("Show statistics", GUI::Statistics::Show,[](){return Network::amOnline && RuntimeConfig::phase == RuntimeConfig::Phase::EndOfGame;});

		//game->FullAutoLayout();
	}

	static void	CreateHelpMenu()
	{
		help = Dialog::Menu::Create();
		help->Setup(0.3f,"Help");
		help->AddCommand("Show Basic Controls",
			[]()
			{
				Tutorial::ShowMessage("Controls",true,true);
			});
		help->AddCommand("Revisit All Tutorial Messages",
			[]()
			{
				Dialog::Confirm("You are about to reset the game's memory of tutorial messages you have been shown. All messages will be shown again where considered appropriate.\n"
								"Continue?",
					[]()
					{
						Tutorial::FlushMemory();
					});
			});
		help->AddCommand("Start Interactive Tutorial",
			[]()
			{
				ServerProcess::StartTutorialServer();
			},
			[](){return !Network::client.IsConnected() && !Network::client.IsAttemptingToConnect();});

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
		main = Dialog::Menu::Create();
		main->Setup(0.25f,"Main");

		main->AddCommand("Disconnect",
		[]()
		{
			Network::client.Disconnect();
		},
		[](){return Network::client.IsConnected();});
		main->AddCommand("Game",[](){GUI::Show(game);},std::function<bool()>(),false);
		
		main->AddCommand("Help",
			[]()
			{
				GUI::Show(help);
			},
			std::function<bool()>(),
			false
			);
		main->AddCommand("Options",
			[]()
			{
				GUI::Show(options);
			});

		main->AddCommand("Quit to Desktop",[](){Dialog::Confirm("Return to Windows?",[](){application_shutting_down = true;},[](){return Dialog::CheckResult::OkayContinue;});});
		//main->FullAutoLayout();
	}


	static void AddFactionCreationChoice(const String&line, UINT32 typeFlags, UINT32 supportFactionFlag)
	{
		login->AddCommand(line,
		[typeFlags,supportFactionFlag]()
		{
			if (Network::HaveFactionKeyForCurrentWorld() && typeFlags != Protocol::FactionType::SpectatorFaction)
			{
				Dialog::Confirm("A faction key is known for this world ("+Network::worldIDString+"). If you create a new faction, this key will be lost, and control of your old faction will irreversible be abandoned. Please confirm.",
				[typeFlags](){
					Network::AttemptFactionLogin(true,typeFlags);
					GUI::Hide(login);
				},[supportFactionFlag]()
				{
					if (!Network::amOnline || Network::haveFaction || !(supportFactionFlag & Network::gameFlags))
						return Dialog::CheckResult::Abort;
					if (RuntimeConfig::phase != RuntimeConfig::Phase::CommandPhase)
						return Dialog::CheckResult::OkayNotRightNow;
					return Dialog::CheckResult::OkayContinue;
				});
			}
			else
			{
				Network::AttemptFactionLogin(true,typeFlags);
				GUI::Hide(login);
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
		login = Dialog::Menu::Create();
		login->Setup(0.25f,"Login");
		login->canBeImplicitlyClosed = false;	//once visible, must explicitly close

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


	void Create()
	{
		CreateOptionsMenu();
		CreateHelpMenu();
		CreateMainMenu();
		CreateLoginMenu();
		CreateGameMenu();

		GlobalEvents::applicationTerminate << []()
		{
			main.reset();
			help.reset();
			login.reset();
			options.reset();
		};
	}

}

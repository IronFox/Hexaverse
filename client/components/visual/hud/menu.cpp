#include "menu.h"
#include "../tutorial.h"
#include "common.h"
#include "../../serverProcess.h"
#include "../../network.h"
#include "../../localInterface.h"
#include "../scene/shadedRenderInstruction.h"
#include "../../clientRuntimeConfig.h"
#include "../../../version.h"
#include "../../audio/audioInterface.h"
#include "../../audio/music.h"
#include "../objectSelection.h"
#include "guiStatistics.h"
#include "../scene/explosions.h"
#include "factionTable.h"

bool	IsFullScreen();
void	SetFullScreen(bool isFull);



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
				execute();
				if (autoHide)
					WindowStack::CloseAllModalBeneath(To<HUD::Component::Window>::Cast(this->shared_from_this()));
			},
			check
		));
		Super::RemoveComponent(hintLabel);
		Super::AnchorBelowPrevious(button,0.f,HUD::Component::Distance::GameWindow( Super::components.IsEmpty() ? 0.f : 0.015f ));
		if (hintLabel)
			Super::AnchorBelowPrevious(hintLabel,0.015f,0.015f);

		LogEvent(EventClass::Preset::gui,"Menu["+String(this)+"/"+myTitle+"]::AddCommand("+name+", ...)");
		return button;
	}

	void		Menu::Setup(float initialAbsoluteWidth, const String&title, bool escapeCloses)
	{
		canBeImplicitlyClosed = escapeCloses;
		Super::ClearComponents();
		hintLabel.reset();
		myTitle = title;
		if (Super::DoesAutoCenter())
			SetWidth(initialAbsoluteWidth);
		else
			Super::SetCenteredWidth(HUD::Component::Distance::GameWindow( initialAbsoluteWidth ));
		Super::fillBackground = false;

		if (title.IsNotEmpty())
		{
			HUD::Component::PLabel label(new HUD::Component::Label());
			label->SetText(title);
			label->SetVerticalPadding( 0.1f );
			Super::AnchorBelowPrevious(label,0.015f,0.f);
		}

		if (escapeCloses)
		{
			hintLabel.reset(new HUD::Component::Label());
			hintLabel->SetLocalFontScale(0.75f);
			hintLabel->SetText("Press Escape to close");
			hintLabel->SetVerticalPadding( 0.1f );
			Super::AnchorBelowPrevious(hintLabel,0.015f,0.f);
		}
		LogEvent(EventClass::Preset::gui,"Menu["+String(this)+"]::Setup("+String(initialAbsoluteWidth)+","+myTitle+")");
	}




	PMenu	main,help,login,game;
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
		p->AddComponent(l);
		p->AddComponent(c);

		l->SetPadding( 0 );



		l->fillBackground = false;
		l->alignCenter = false;
		{
			auto mod = l->Anchor();
			//mod.Top().Set(0);
			mod.CenterVertically().Left(0,MinTextWidth);
		}
		c->AnchorTop(0);
		{
			auto mod = c->Anchor();
			mod.Left(HUD::Component::Distance::GameWindow( MinTextWidth ));
			mod.Right(0.01f);
			mod.CenterVertically();
		}

		p->SetAutomaticHeight(true);
		p->fillBackground = false;

		options->AnchorBelowPrevious(p);
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
		options = Component::Window::Create();
		LogEvent(EventClass::Preset::gui,"CreateOptionsMenu(): "+String(options.get()));
		options->logFullAutoLayout = true;
		options->SetAutoCenter(false);
		options->FillHorizontally(0.1f);
		options->AnchorTop(0.1f);
		options->AnchorBelowPrevious(new Component::Label("Options",2.f));
		
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

		if (AudioInterface::isActive)
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

		{
			Component::PButton button(new Component::Button("OK",[](){WindowStack::Hide(options);}));
			//button->LockIdealWidth();
			options->AnchorBelowPrevious(button);
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

		game->AddCommand("Change Nickname",LocalApplicationInterface::QueryNickname);
		game->AddCommand("Abandon faction",Network::AbandonCurrentFaction,[](){return Network::amOnline && Network::haveFaction && RuntimeConfig::phase != RuntimeConfig::Phase::EndOfGame;});
		game->AddCommand("Show factions (Tab)", FactionTable::Show,[](){return Network::amOnline;});
		game->AddCommand("Show statistics", Statistics::Show,[](){return Network::amOnline && RuntimeConfig::phase == RuntimeConfig::Phase::EndOfGame;});

		//game->FullAutoLayout();
	}

	static void	CreateHelpMenu()
	{
		help = Menu::Create();
		help->Setup(0.3f,"Help",true);
		help->AddCommand("Show Basic Controls",
			[]()
			{
				Tutorial::ShowMessage("Controls",true,true);
			});
		help->AddCommand("Revisit All Tutorial Messages",
			[]()
			{
				HUD::Dialog::Confirm("You are about to reset the game's memory of tutorial messages you have been shown. All messages will be shown again where considered appropriate.\n"
								"Continue?",
					[]()
					{
						Tutorial::FlushMemory();
					});
			});
		help->AddCommand("Start Interactive Tutorial",
			ServerProcess::StartTutorialServer,
			[](){return !Network::client.IsConnected() && !Network::client.IsAttemptingToConnect();});
		
		help->AddCommand("Report Bug",
			[]()
			{
				SetFullScreen(false);
				Logging::CallBugReporter();
			}	
		);

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

		main->AddCommand("Disconnect",
		[]()
		{
			Network::client.Disconnect();
		},
		[](){return Network::client.IsConnected();});
		main->AddCommand("Game",[](){WindowStack::Show(game);},std::function<bool()>(),false);
		
		main->AddCommand("Help",
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

		main->AddCommand("Quit to Desktop",[](){HUD::Dialog::Confirm("Return to Windows?",[](){application_shutting_down = true;},[](){return HUD::Dialog::CheckResult::OkayContinue;});});
		//main->FullAutoLayout();
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
}

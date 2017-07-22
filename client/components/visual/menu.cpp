#include "menu.h"
#include "tutorial.h"
#include "../serverProcess.h"
#include "../network.h"
#include "../localInterface.h"
#include "../clientEvents.h"
#include "shadedRenderInstruction.h"

namespace Menu
{
	Dialog::PMenu	main,help,login;
	GUI::PWindow	options;


	namespace Configuration
	{
		void	Update(const String&path, const String&value)
		{}

	}


	static const float MinTextWidth=0.2f;	//measured in multiples of a whole screen width

	static void AddOptionSlider(const String&name, float min, float max, float initial, const std::function<void(float&)>&onChange, const std::function<String()>&toString)
	{

		GUI::PSlider s(new GUI::Slider());
		s->Setup(initial,max, 1.f,MinTextWidth);
		std::weak_ptr<GUI::Slider> w = s;
		s->SetOnChange([onChange,toString,w,name](float&f)
		{
			onChange(f);
			const auto s = w.lock();
			if (s)
				s->SetText(name+" ("+toString()+")");
		});
		s->SetText(name+" ("+toString()+")");
		options->AnchorBelowPrevious(s);
	}

	static void AddCheckBox(const String&name,bool initial, const std::function<void(bool)>&onChange)
	{
		GUI::PPanel p(new GUI::Panel());
		GUI::PLabel l(new GUI::Label(name));
		GUI::PCheckbox c(new GUI::Checkbox("",initial,onChange));
		p->AddComponent(l);
		p->AddComponent(c);

		l->extraSpacing = 0;
		l->fillBackground = false;
		l->alignCenter = false;
		l->anchorConfig.Top().Set(0);
		l->anchorConfig.Left().Set(0);
		l->SetSize(float2(MinTextWidth,0));
		c->AnchorTop(0);
		//c->extraSpacing = 0.1f;
		c->anchorConfig.Left().Set(MinTextWidth,GUI::TAnchor::RelativeToScreen);

		p->autoHeight = true;
		p->fillBackground = false;

		options->AnchorBelowPrevious(p);
	}

	static void CreateOptionsMenu()
	{
		options.reset(new GUI::Window());
		options->FillHorizontally(0.1f);
		options->AnchorTop(0.1f);
		options->AnchorBelowPrevious(new GUI::Label("Options",2.f));
		AddOptionSlider("Shadow Quality", 0, ShadedInstruction::Quality::N-1, ShadedInstruction::Quality::N-1 - ShadedInstruction::chosenQuality, [](float&q)
			{
				auto old = ShadedInstruction::chosenQuality;
				ShadedInstruction::chosenQuality.Load(ShadedInstruction::Quality::N-1 -Round(q));
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
			AddOptionSlider("Anti-Aliasing", 0, modes.Count()-1, display.GetCurrentMSAAMode(), [](float&q)
			{
				auto old = display.GetCurrentMSAAMode();
				display.SetMSAAMode(Math::Max(0.f,Round(q)));
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
		options->autoHeight = true;
		options->FullAutoLayout();
	}

	static void	CreateHelpMenu()
	{
		help.reset(new Dialog::Menu(0.3f,"Help"));
		help->AddCommand("Show Basic Controls",
			[]()
			{
				Tutorial::ShowMessage("Controls",true,true);
			});
		help->AddCommand("Revisit All Tutorial Messages",
			[]()
			{
				Dialog::Confirm("You are about to reset the game's memory on which tutorial messages you have so far seen. All messages will be shown again where considered approprote.\n"
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

		{
			float2 margin = help->GetOutlineMargin();
			margin.y *= 10.f;
			help->SetOutlineMargin(margin);
		}
		help->FullAutoLayout();
	}

	static void CreateMainMenu()
	{
		main.reset(new Dialog::Menu(0.25f,"Main"));
		main->AddCommand("Change Nickname",LocalApplicationInterface::QueryNickname);
		main->AddCommand("Abandon faction",Network::AbandonCurrentFaction,[](){return Network::amOnline && Network::haveFaction;});
		main->AddCommand("Disconnect",
			[]()
			{
				Network::client.Disconnect();
			},
			[](){return Network::client.IsConnected();});
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

		main->AddCommand("Quit to Desktop",[](){Dialog::Confirm("Really quit? Any active faction will be preserved server-sided for you to continue later on.",[](){application_shutting_down = true;},[](){return Dialog::CheckResult::OkayContinue;});});
		main->FullAutoLayout();
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
		login.reset(new Dialog::Menu(0.25f,"Login"));
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

		GlobalEvents::applicationTerminate << []()
		{
			main.reset();
			help.reset();
			login.reset();
			options.reset();
		};
	}

}

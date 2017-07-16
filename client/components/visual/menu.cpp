#include "menu.h"
#include "tutorial.h"
#include "../serverProcess.h"
#include "../network.h"
#include "../localInterface.h"
#include "../clientEvents.h"

namespace Menu
{
	Dialog::PMenu	main,help,login;

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
		CreateHelpMenu();
		CreateMainMenu();
		CreateLoginMenu();

		GlobalEvents::applicationTerminate << []()
		{
			main.reset();
			help.reset();
			login.reset();
		};
	}

}

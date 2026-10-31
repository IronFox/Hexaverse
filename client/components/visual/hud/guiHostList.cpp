#include <global_root.h>
#include "guiHostList.h"
#include "windowStack.h"
#include "../../network.h"
#include "dialog.h"
#include "../../../version.h"
#include <io/string_file.h>
#include <runtimeConfig.h>
#include "../../clientRuntimeConfig.h"
#include <serverProcess.h>
#include <network/steamInterface.h>
#include "../../audio/music.h"
#include "../../clientEvents.h"
#include "menu.h"
#include "../tutorial.h"
#include "../display.h"

#ifdef HAS_WINDOW

namespace HUD{namespace HostList{

	using namespace Component;
	using namespace Time::Literals;

	//class VisualAttachment: public Network::ServerInfo::VisualAttachment
	//{
	//public:
	//	PLabel			headLine;	//game/server ids, optionally (continue)
	//	PButtonPanel	buttonPanel;
	//};

	const float4 incompatibleColor(1,0.6,0.55,1);

	static const float VisualTabWidth[] = {0.2f,0.25f,0.1f,0.1f,0.25f};

	PWindow		serverChoice,changelog;
	PTabBar		serverTabBar;
	PPanel		headerRow;


	bool	IsVisible()
	{
		return WindowStack::IsVisible(serverChoice);
	}

	void	TickRefresh()
	{
		static Time::Stamp	lastCheck = Time::Stamp() - 10_s;
		if (lastCheck.CheckIfOutdatedAndUpdate(1_s))
		{
			if (display.IsMinimized() || !IsVisible())
				return;
			Update();
		}
	}

	static void RefreshVerticalChangelogLayout()
	{
		float bottom = 0.1f * UI::GetScale();
		float top = 0.15f * UI::GetScale();
		changelog->Anchor().Right().Bottom(bottom);
		changelog->SetMaxAutoHeight(0.75f-bottom-top);
	}

	void	Create()
	{
		GlobalEvents::frameBegin << TickRefresh;
			


		serverChoice = Window::Create();
		serverChoice->Anchor().Top().CenterHorizontally(0.9f);
		serverChoice->canBeImplicitlyClosed = false;

		serverTabBar.reset(new TabBar());
		serverChoice->AddComponent(serverTabBar,CLOCATION);
		serverTabBar->FillHorizontally(0);
		serverTabBar->Anchor().Top();
		serverTabBar->SetCornerStyle(HUD::Component::TabBar::CornerStyle::Compact);

		try
		{
			StringFile source(L"../changelog.txt",CM_RETURN_EMPTY_LINES);
			changelog = Window::Create();
			changelog->canBeImplicitlyClosed = false;
			changelog->isModal = false;
			changelog->SetWidth(HUD::Component::Distance(0.4f));
			//changelog->SetAutomaticHeight(false);
			//changelog->SetHeight(0.6f);
			changelog->SetBackground(false);
			changelog->SetEdges(false);
			changelog->SetDarkenedOutline(false);
			RefreshVerticalChangelogLayout();
			count_t cnt = 0;
			String line;
			while (source >> line)
			{
				if (line.TrimRef().IsEmpty() && cnt > 6)
					break;
				cnt ++;
				PLabel label{new Label{line,0.7f}};
				label->Anchor().FillHorizontally();
				label->SetAlign(HUD::Component::HorizontalTextAlign::Left);
				label->SetColor(float4(1,1,1,0.6f));
				label->SetVerticalPadding(0.01f);
				changelog->AnchorBelowPrevious(CLOCATION,label,0,0);
			}

			GlobalEvents::uiScaleChanged << RefreshVerticalChangelogLayout;
		}
		catch (...)
		{
			changelog.reset();
		}	//don't care, just don't show

		headerRow.reset(new Panel());
		headerRow->FillHorizontally(0);
		headerRow->Anchor().Beneath(serverTabBar);
		headerRow->SetBackground(false);
		headerRow->SetAutomaticHeight(true);
		headerRow->SetBackground(false);
		
		CheckVisibility(true);
	}

	void	CheckVisibility(bool doShow)
	{
		HUD::WindowStack::VerifyIntegrity(CLOCATION);

		Music::SignalServerListShown(doShow);

		if (doShow)
		{
			bool update = false;
			if (!changelog)
			{
				update = WindowStack::ShowBottom(serverChoice);
			}
			else
			{
				update = WindowStack::ShowBottom(changelog);
				if (update)
				{
					WindowStack::ShowBottom(serverChoice);
					WindowStack::ShowBottom(changelog);
				}
			}
			if (update)
				Update();
		}
		else
		{
			WindowStack::Hide(serverChoice);
			if (changelog)
				WindowStack::Hide(changelog);
		}
		HUD::WindowStack::VerifyIntegrity(CLOCATION);
	}


	PLabel	MakeHeaderLabel(const String&text, float zoom, index_t tabIndex)
	{
		PLabel label (new Label());
		label->SetAlign(HUD::Component::HorizontalTextAlign::Left);
		label->Anchor().Top(Distance::GameWindow( 0.01f )).Left( Distance::GameWindow(0.01f) );
		label->SetWidth(VisualTabWidth[std::min(tabIndex,ARRAYSIZE(VisualTabWidth))]);
		label->SetText(text);
		label->SetLocalFontScale(zoom);
		label->SetVerticalPadding( 0);
		return label;
	};


	PLabel	AddHeader(const PPanel&context, const String&name, const float4&color, const Distance&verticalSpacing = Distance())
	{
		const index_t tabIndex = context->CountComponents();
		PLabel label=MakeHeaderLabel(name/*+"["+String(tabIndex)+"]"*/,1.1f,tabIndex);
		label->Anchor().Top(verticalSpacing).RightOfLastOf(context,0.f);
		context->AddComponent(label,CLOCATION);
		label->SetColor(color);
		//label->fillBackground = true;
		return label;
	}

	PLabel	AddMainHeader(const String&name, const float4&color=float4(1))
	{
		return AddHeader(headerRow,name,color);
	}

	static String CompileRules(const Protocol::ServerInfo&info)
	{
		String result;
		if (info.gameInfo.gameConfig.gameFlags & Protocol::GameFlag::TutorialMode)
			result += "Tutorial Mode";
		else if (info.gameInfo.gameConfig.gameFlags & Protocol::GameFlag::GameHasEnded)
			result += "Game Has Ended";
		else
		{
			switch (info.gameInfo.gameConfig.gameGoal)
			{
				case RuntimeConfig::GameGoal::ConquerNexus:
					result="Conquer Nexus";
				break;
				case RuntimeConfig::GameGoal::None:
					result="No Goal";
				break;
			}
			if (info.gameInfo.gameConfig.gameFlags & Protocol::GameFlag::PrivateServer)
				result += ", Private";
			if (info.gameInfo.gameConfig.gameFlags & Protocol::GameFlag::ClosedGame)
				result += ", Closed Game";
			if (info.gameInfo.gameConfig.gameFlags & Protocol::GameFlag::EMPZones)
				result += ", EMP Zones";
			if (info.gameInfo.gameConfig.gameFlags & Protocol::GameFlag::BasesRequired)
				result += ", Require Bases";
			if (info.gameInfo.gameConfig.gameFlags & Protocol::GameFlag::StartBases)
				result += ", Start Bases";
			if (info.gameInfo.gameConfig.gameFlags & Protocol::GameFlag::NexusAI)
				result += ", Nexus AI";
			if (info.gameInfo.gameConfig.gameFlags & Protocol::GameFlag::SupportSpectatorFactions)
				result += ", Spectator";
			if (info.gameInfo.gameConfig.gameFlags & Protocol::GameFlag::SupportRegularFactions)
				result += ", Regular";
		}
		//String result = String("Goal: ")+Protocol::GameGoal::Reinterpret(info.gameGoal).ToString();
		return result;


	}


	static void ConnectToHost(const Network::ServerAddress&id)
	{
		Network::ServerInfo*info = nullptr;
		foreach (Network::servers,srv)
		{
			if (srv->HasAddress( id ))
			{
				info = srv;
				break;
			}
		}
		if (!info)
			return;
		index_t best = 0;
				
		info->selected = true;
		info->Connect();


	}

	static void SetTop(const PComponent&c, float dist)
	{
		c->Anchor().Top(Distance::GameWindow(dist));
	}

	PButtonPanel ConstructPanel(Network::ServerInfo&info)
	{




		float4 color(1);
		if (!info.rightVersion || info.protocolIncompatible)
			color = incompatibleColor;
		else
		{
		
			if (info.IsPrivate()
				|| 
				info.HasNonSteamAddress()
				)
			{
				color.rgb = float3(0.8,0.85,1)*1.2f;
			}
		}
		PButtonPanel p;

		//if (info.visualAttachment)
		//{
		//	To<VisualAttachment>::Cast(info.visualAttachment)->buttonPanel->ClearComponents();
		//}
		//else
		{
			//auto attach = new VisualAttachment();
			//info.visualAttachment.reset(attach);

			//attach->buttonPanel.reset(new ButtonPanel());
			//auto p = attach->buttonPanel;
			p = PButtonPanel(new ButtonPanel());

			p->Anchor().FillHorizontally().Top();
			p->SetAutomaticHeight(true);
			bool versionMatch = info.rightVersion;

			const auto id = info.lobbyID;
			const bool isTutorialMode = (info.GetPrimaryAddress().info.gameInfo.gameConfig.gameFlags & Protocol::GameFlag::TutorialMode) != 0;
			const bool requireLocalhost = isTutorialMode;

			p->SetCheck([versionMatch,id,requireLocalhost]()
			{
				if (requireLocalhost)
				{
					Network::ServerInfo*info = nullptr;
					foreach (Network::servers,srv)
					{
						if (srv->lobbyID == id)
						{
							info = srv;
							break;
						}
					}
					if (!info || !info->HasLocalhost())
						return false;
				}

				return versionMatch && !Network::client.IsAttemptingToConnect() && !Network::client.IsConnected();
			});

			p->SetTriggerFunction([id]()
			{
				foreach(Network::servers,serv)
					if (serv->lobbyID == id)
					{
						ConnectToHost(serv->GetPrimaryAddress());
					}
			});


			

			{
				String text;
				//color = incompatibleColor * 1.3f;
				if (!info.protocolIncompatible)
				{
					text = " "+info.GetWorldID().ToNiceString();
				}
				else
				{
					text = " "+info.GetWorldID().ToNiceString();
					text += "\n (Version Mismatch)";
				}
				PLabel headLine = MakeHeaderLabel(text,1.1f,0);
				headLine->SetColor(color);
				SetTop(headLine,0.01f);
				//attach->headLine = headLine;

				p->AddComponent(headLine,CLOCATION);

			}
		}



		const auto&primary = info.GetPrimaryAddress();

		//bool incompatible = !info.rightVersion || info.protocolIncompatible;
		{
			if (info.ownerName.IsEmpty())
				SteamInterface::TryGetUsernameNow(info.ownerID, info.ownerName);
			String owner = info.ownerName;
			#ifdef USE_STEAM_OWNER_AUTHENTICATION
				if (!info.authenticated)
					owner += '?';
			#endif
			auto ocolor = color;
			if (SteamInterface::IsFriend(info.ownerID))
				ocolor = float3(0.65,1,0.7);


			AddHeader(p,owner,ocolor,0.01f);
			AddHeader(p,String(primary.info.gameInfo.numFactionsOnline) + "/" + String(primary.info.gameInfo.numFactionsCreated),color,0.01f);
			AddHeader(p,primary.info.gameInfo.roundNumber.ToString(),color,0.01f);
			PLabel l = AddHeader(p,CompileRules(primary.info),color,0.01f);
			l->SetLocalFontScale(l->GetLocalFontScale() * 0.7f);



		}

		if (!info.rightVersion && !info.protocolIncompatible)
		{
			PLabel label = MakeHeaderLabel("Server identifies as '"+String(primary.info.hostInfo.version)+'\'',0.7f,0);
			label->SetColor(incompatibleColor);
			label->Anchor().BeneathLastOf(p,Distance::GameWindow( 0.005f ));
			p->AddComponent( label ,CLOCATION);
					
			label = MakeHeaderLabel("Client expected '" SERVER_NAME "\'",0.7f,0);
			label->SetColor(incompatibleColor);
			label->Anchor().BeneathLastOf(p,Distance::GameWindow( 0.005f ));
			p->AddComponent( label ,CLOCATION);
		}

		p->SetAutoHeightSpacing(0.01f);
		p->SetBackgroundTone(color.rgb * 0.75f);

		return p;
	}


	static const String allowedHostChars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-.:";

	bool CheckHostName(const UTF32StringRef&url)
	{
		if (url.IsEmpty())
			return false;

		//could add more here. if i knew what

		return true;
	}


	PPanel	serverListPanel;

	void	ConfigureButton( const HUD::Component::PButton&b )
	{
		b->LockToIdealWidth();
		b->SetEdgeVisibility(b->GetCutCorners());
	}

	namespace Filter
	{
		bool	TCPFilter(const Network::ServerInfo&info)
		{
			if (!Network::ForcingTCPConnections())
				return true;
			return info.lastPing != 0;
		}

		bool	All(const Network::ServerInfo&info)
		{
			return TCPFilter(info);
		}
		bool	Friends(const Network::ServerInfo&info)
		{
			return TCPFilter(info) && SteamInterface::IsFriend(info.ownerID) && !SteamInterface::IsLocalUser(info.ownerID);
		}
		bool	Visited(const Network::ServerInfo&info)
		{
			return TCPFilter(info) && Network::DidVisit(info.GetWorldID());
		}
		bool	TCPNonLocal(const Network::ServerInfo&info)
		{
			return info.HasNonSteamAddress() && !info.HasLocalhost();
		}
		bool	Local(const Network::ServerInfo&info)
		{
			return TCPFilter(info) && (info.HasLocalhost() || SteamInterface::IsLocalUser(info.ownerID));
		}

	};


	void	Update( )
	{

		if (serverChoice->HasComponent(mousePressedComponent))
			return;	//can't activate right now
		WindowStack::VerifyIntegrity(CLOCATION);

		const bool isActive = serverChoice->IsActive();
		if (isActive)
			serverChoice->SetActive(false);

		if (serverTabBar->IsEmpty())
		{
			serverTabBar->AddTab("All","Show all found/known servers");
			serverTabBar->AddTab("Friends","List servers hosted by friends");
			serverTabBar->AddTab("Visited","List servers you previously connected to");
			serverTabBar->AddTab("Remote TCP","List all manually added TCP servers");
			serverTabBar->AddTab("Local","List all servers running on this machine");
			serverTabBar->SetOnTabChange(Update);
		}

		bool (*filter)(const Network::ServerInfo&) = nullptr;

		switch (serverTabBar->GetActiveTabIndex())
		{
			case 0:	//All
				filter = Filter::All;
			break;
			case 1:	//Friends
				filter = Filter::Friends;
			break;
			case 2:	//Visited
				filter = Filter::Visited;
			break;
			case 3:	//TCP
				filter = Filter::TCPNonLocal;
			break;
			case 4:	//Local
				filter = Filter::Local;
			break;
		}

		serverChoice->ClearComponents();
		serverChoice->AddComponent(serverTabBar,CLOCATION);

		serverChoice->SetBackground(false);
		serverChoice->isModal = false;
		SetTop(serverChoice,0.1f);
		serverChoice->SetMaxAutoHeight(0.8f);

		serverChoice->AddComponent(headerRow,CLOCATION);
		if (!headerRow->HasComponents())
		{
			AddMainHeader("WorldID");
			AddMainHeader("Owner");
			AddMainHeader("Online");
			AddMainHeader("Round");
			AddMainHeader("Rules");
		}

		//bool first =true;
		
		if (!serverListPanel)
		{
			serverListPanel.reset(new Panel());
			serverListPanel->Anchor().FillHorizontally().Top();
			serverListPanel->SetAutomaticHeight(true);
			serverListPanel->SetMaxAutoHeight(0.5f);
			serverListPanel->SetBackground(false);
		}
		serverListPanel->ClearComponents();
		serverListPanel->Anchor().BeneathAllOf(serverChoice);
		serverChoice->AddComponent( serverListPanel,CLOCATION );


		static Buffer0<Network::ServerIPAddress>	list;
		Network::ServerDiscovery::ListFailedAttempts(list);
		foreach (list,failed)
		{
			PPanel p(new Panel());
			p->Anchor()
				.BeneathLastOf(serverListPanel,Distance::GameWindow( 0.015f ))
				.FillHorizontally();
			//p->SetAutomaticHeight(true);
			p->SetBackground(false);

			PPanel pb(new Panel());
			//pb->autoHeight = true;
			p->AddComponent( pb ,CLOCATION);
			pb->Anchor().FillVertically().Left();
			
			

			PLabel label = MakeHeaderLabel("Still trying to reach '"+ToString(*failed)+"'...",0.7f,0);
//			label->anchorConfig.Left().Set(0.1f,false);
			label->SetColor(1,0.75f,0.75f,0.9f);
			label->Anchor().Top(Distance::GameWindow( 0.01f )).FillHorizontally();//.Right(Distance::GameWindow( 0.01f ));
			pb->SetCutCorners(false);
			pb->AddComponent( label,CLOCATION );
			Network::ServerIPAddress url = *failed;
			PButton forget(new Button("Forget this host","Removes this host from the list of queried TCP game servers",
			[url]()
			{
				HUD::Dialog::Confirm("'"+ToString(url)+"' will be removed from the list of actively checked hosts. Please confirm.",
				[url]()
				{
					if (Network::ServerDiscovery::TryRemoveHost(url))
					{
						Update();
						Network::SaveHosts();
					}
				},
				[url]()
				{
					return Network::ServerDiscovery::ListeningButCantConnectTo(url) ? HUD::Dialog::CheckResult::OkayContinue : HUD::Dialog::CheckResult::Abort;
				}
				);
			
			}));
			forget->SetLocalFontScale(forget->GetLocalFontScale() * 0.9f);
			forget->ScaleVerticalPadding(0.5f);
			forget->LockToIdealWidth();
			forget->SetCutCorners(false);
			forget->Anchor().Top().Right();
		
			p->AddComponent( forget ,CLOCATION);
			pb->Anchor().FillHBetween(nullptr,forget,Distance(),Distance(10,Distance::Type::AbsolutePixels));

			serverListPanel->AddComponent( p ,CLOCATION);
		}

		for (index_t i = 0; i < Network::servers.Count(); i++)
		{
			auto&info = Network::servers[i];

			if (!filter(info))
				continue;

			PButtonPanel p = ConstructPanel(info);
			p->SetBackgroundTone((float3)0.5f);

			p->Anchor().BeneathLastOf(serverListPanel,Distance::GameWindow( 0.015f ));
			serverListPanel->AddComponent( p ,CLOCATION);
		}

		//count_t openAttempts = 0;





		if (Network::servers.IsEmpty())
		{
			PPanel p(new Panel());
			p->Anchor().BeneathAllOf(serverChoice, Distance::GameWindow(0.025f ));
			serverChoice->AddComponent( p ,CLOCATION);
			p->SetBackground(false);

			PLabel label = MakeHeaderLabel(" No Servers Found",0.7f,0);
			label->Anchor().Left(Distance::GameWindow( 0.1f ));
			label->SetColor(1,1,1,0.7f);
			p->AddComponent( label ,CLOCATION);
			p->SetAutomaticHeight(true);
			p->SetAutoHeightSpacing(p->GetAutoHeightSpacing() * 4.f);

		};


		{
			PCheckbox b(new Checkbox("Poll Servers from Steam",Network::SteamDiscoveryIsEnabled(),[](bool enabled)
			{
				Network::SetSteamDiscovery(enabled);
			}));
			{
				auto mod = b->Anchor();
				mod.BeneathLastOf(serverChoice,0.025f);
				mod.Left();
			}


			b->LockToIdealWidth();

			serverChoice->AddComponent( b,CLOCATION );
		}
		
		{
			PCheckbox b(new Checkbox("Force TCP Connection",Network::ForcingTCPConnections(),[](bool enabled)
			{
				Network::ForceTCPConnections(enabled);
			}));
			{
				auto mod = b->Anchor();
				mod.BeneathLastOf(serverChoice,0.025f);
				mod.Left();
			}


			b->LockToIdealWidth();

			serverChoice->AddComponent( b,CLOCATION );
		}

		{
			PButton b(new Button("Refresh list",[]()
			{
				Network::RefreshServerListNow();
			},
			[]()->bool {return !Network::client.IsAttemptingToConnect() && !Network::client.IsConnected();}
			));
			{
				auto mod = b->Anchor();
				mod.BeneathLastOf(serverChoice,0.025f);
				mod.Left();
			}

			b->LockToIdealWidth();
			b->SetCutCorners({true,true,false,true});
			{
				auto cp = b->GetPadding();
				cp.left = 3;
				b->SetPadding(cp);
			}
			ConfigureButton(b);

			serverChoice->AddComponent( b,CLOCATION );
		}		





		PButton addHostButton;
		{
			PButton b(new Button("Add new host...","Queries for a host name or IP address to begin querying. Preserved across game restarts",[]()
			{
				HUD::Dialog::Query("Enter a new host address to attempt connection to.","",nullptr,CheckHostName,
				[](const String&url)
				{
					try
					{
						if (Network::ServerDiscovery::RegisterNewHost(0, SteamInterface::GetUserID(),Network::ServerAddress(url,0,Protocol::ServerInfo()),false,false))
							Network::SaveHosts();
					}
					catch (const std::exception&ex)
					{
						HUD::Dialog::ShowMessage(ex.what());
					}
				});
			
			},
			[]()->bool {return !Network::client.IsAttemptingToConnect() && !Network::client.IsConnected();}
			));
			{
				auto mod = b->Anchor();
				mod.axis[AnchorAxis::Vertical] = serverChoice->GetLastChild()->GetAnchorConfig().GetVAxis();
				mod.RightOfLastOf(serverChoice,0.01f);
			}
			b->LockToIdealWidth();
			b->SetCutCorners(M::Quad<bool>(false,true,false,true));
			ConfigureButton(b);

			serverChoice->AddComponent( b ,CLOCATION);
			
			addHostButton = b;
		}

		{
			PButton start(new Button("Start local server...","Opens a menu to start a local server, load save games, or begin a tutorial session"));
			PButton stop(new Button("Stop local server","Closes down the first detected locally running server. Not recommended when using the server manager"));
			bool running = ServerProcess::IsRunning();
			stop->SetEnabled(running);
			start->SetEnabled(!running);
			start->SetCutCorners(M::Quad<bool>(false,true,false,true));
			stop->SetCutCorners(M::Quad<bool>(false,true,true,true));
			WButton wstart = start;
			WButton wstop = stop;
			start->SetTriggerFunction([wstart,wstop]()
			{
				HUD::WindowStack::Show(HUD::Menu::startServer);
/*
				ServerProcess::Begin(true,false);
				PButton b = wstart.lock();
				if (b)
					b->SetEnabled(false);
				b = wstop.lock();
				if (b)
					b->SetEnabled(true);*/
			});
			stop->SetTriggerFunction([wstart,wstop]()
			{
				ServerProcess::End();
				Network::RefreshServerListNow();
				PButton b = wstart.lock();
				if (b)
					b->SetEnabled(true);
				b = wstop.lock();
				if (b)
					b->SetEnabled(false);
			});
			start->SetCheck(
			[]()->bool {return !Network::client.IsAttemptingToConnect() && !Network::client.IsConnected() && !ServerProcess::IsRunning();}
			);
			stop->SetCheck(
			[]()->bool {return !Network::client.IsAttemptingToConnect() && !Network::client.IsConnected() && ServerProcess::IsRunning();}
			);
			{
				auto mod = start->Anchor();

				mod.GetVAxis() = addHostButton->GetAnchorConfig().GetVAxis();
				mod.RightOf(addHostButton,0.01f);
			}
			{
				auto mod = stop->Anchor();
				mod.GetVAxis() = addHostButton->GetAnchorConfig().GetVAxis();
				mod.RightOf(start,0.01f);
			}

			{
				auto cp = stop->GetPadding();
				cp.right = 3;
				stop->SetPadding(cp);
			}
			ConfigureButton(start);
			ConfigureButton(stop);
			serverChoice->AddComponent(start,CLOCATION);
			serverChoice->AddComponent(stop,CLOCATION);
		}
		

		serverChoice->FullAutoLayout(CLOCATION);
		if (isActive)
		{
			serverChoice->SetActive(isActive);
			WindowStack::OverrideSignalMouseMotion();
		}
		
		HostList::CheckVisibility(!Network::client.IsConnected());
	}
}}
#endif

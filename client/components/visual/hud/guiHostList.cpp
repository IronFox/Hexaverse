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
#include "../../audio/music.h"
#include "../../clientEvents.h"
#include "menu.h"

#ifdef HAS_WINDOW

namespace HUD{namespace HostList{

	using namespace Component;

	//class VisualAttachment: public Network::ServerInfo::VisualAttachment
	//{
	//public:
	//	PLabel			headLine;	//game/server ids, optionally (continue)
	//	PButtonPanel	buttonPanel;
	//};

	const float4 incompatibleColor(1,0.6,0.55,1);

	static const float VisualTabWidth[] = {0.3f,0.15f,0.1f,0.25f};

	PWindow		serverChoice,changelog;
	PPanel		headerRow;


	bool	IsVisible()
	{
		return WindowStack::IsVisible(serverChoice);
	}

	void	TickRefresh()
	{
		static Timer::Time	lastCheck = timer.Now() - timer.ToTicks(10);
		if (timer.CheckIfOutdatedAndUpdate(lastCheck,1.f))
		{
			if (display.IsMinimized() || !IsVisible())
				return;
			Update();
		}
	}

	void	Create()
	{
		GlobalEvents::frameBegin << TickRefresh;
			


		serverChoice = Window::Create();
		serverChoice->Anchor().Top().CenterHorizontally(0.9f);
		serverChoice->canBeImplicitlyClosed = false;

		try
		{
			StringFile source("../changelog.txt",CM_RETURN_EMPTY_LINES);
			changelog = Window::Create();
			changelog->Anchor().Right().Bottom(0.1f);
			changelog->canBeImplicitlyClosed = false;
			changelog->SetWidth(HUD::Component::Distance(0.4f));
			//changelog->SetAutomaticHeight(false);
			//changelog->SetHeight(0.6f);
			changelog->fillBackground = false;
			changelog->renderBorder = false;
			changelog->darkenOutline = false;
			changelog->SetMaxAutoHeight(0.5f);
			count_t cnt = 0;
			String line;
			while (source >> line)
			{
				if (line.TrimRef().IsEmpty())
					break;
				PLabel label{new Label{line,0.7f}};
				label->Anchor().FillHorizontally();
				label->alignCenter = false;
				label->SetColor(float4(1,1,1,0.6f));
				label->SetVerticalPadding(0.01f);
				changelog->AnchorBelowPrevious(CLOCATION,label,0,0);
			}
		}
		catch (...)
		{
			changelog.reset();
		}	//don't care, just don't show

		headerRow.reset(new Panel());
		headerRow->FillHorizontally(0);
		headerRow->Anchor().Top();
		headerRow->SetAutomaticHeight(true);
		headerRow->SetBackgroundTransparent();
		
		Update();
	}

	bool	SetVisibility(bool doShow)
	{

		Music::SignalServerListShown(doShow);

		if (doShow)
		{
			if (!changelog)
			{
				if (WindowStack::ShowBottom(serverChoice))
					return true;
			}
			else
			{
				bool change = WindowStack::ShowBottom(changelog);
				if (change)
				{
					WindowStack::ShowBottom(serverChoice);
					WindowStack::ShowBottom(changelog);
					return true;
				}
			}
		}
		else
		{
			WindowStack::Hide(serverChoice);
			if (changelog)
				WindowStack::Hide(changelog);
		}
		return false;
	}

	void	CheckVisibility(bool doShow)
	{
		if (SetVisibility(doShow))
			Update();
	}



	PLabel	MakeHeaderLabel(const String&text, float zoom, index_t tabIndex)
	{
		PLabel label (new Label());
		label->alignCenter = false;
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

	static float PingOf(const Network::Host&p)
	{
		if (p.address.host == "localhost")
			return -1;
		return p.ping;
	}

	static const Network::Host&	GetTargetHost(const Network::ServerInfo&info)
	{
		float ping = std::numeric_limits<float>::max();
		const Network::Host* best = nullptr;

		foreach (info.addresses,addr)
		{
			float p = PingOf(*addr);
			if (p < ping)
			{
				ping = p;
				best = addr;
			}
		}
		ASSERT_NOT_NULL__(best);
		return *best;
	}

	static void SetTop(const PComponent&c, float dist)
	{
		c->Anchor().Top(Distance::GameWindow(dist));
	}

	PButtonPanel ConstructPanel(Network::ServerInfo&info)
	{

		const Network::Host&host = GetTargetHost(info);



		float4 color(1);
		if (!info.rightVersion || info.protocolIncompatible)
			color = incompatibleColor;
		PButtonPanel p;// = To<VisualAttachment>::Cast(info.visualAttachment)->buttonPanel;

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

			TUniqueID id = info.id;
			const bool isTutorialMode = (host.serverInfo.gameInfo.gameConfig.gameFlags & Protocol::GameFlag::TutorialMode) != 0;
			const bool requireLocalhost = isTutorialMode;

			p->SetCheck([versionMatch,id,requireLocalhost]()
			{
				if (requireLocalhost)
				{
					Network::ServerInfo*info = nullptr;
					foreach (Network::servers,srv)
					{
						if (srv->id == id)
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
				Network::ServerInfo*info = nullptr;
				foreach (Network::servers,srv)
				{
					if (srv->id == id)
					{
						info = srv;
						break;
					}
				}
				if (!info)
					return;
				index_t best = 0;
				float bestPing = PingOf(info->addresses.First());
				for (index_t i = 1; i < info->addresses.Count(); i++)
				{
					float ping = PingOf(info->addresses[i]);
					if (ping < bestPing)
					{
						bestPing = ping;
						best = i;
					}
				}
				info->selected = true;


				Network::ConnectTo(info->addresses[best]);
			});


			
			{
				String text;
				//float4 color(1,1,1,1);
				if (info.rightVersion)
				{
					text = " "+info.id.ToNiceString()+" / "+host.serverInfo.gameInfo.worldID.ToNiceString();
					if (info.haveToken)
						text += " (continue)";
				}
				else
				{
					//color = incompatibleColor * 1.3f;
					if (info.protocolIncompatible)
					{
						text = " <Unknown> (Protocol Incompatible)";
					}
					else
					{
						text = " "+info.id.ToNiceString()+" / "+host.serverInfo.gameInfo.worldID.ToNiceString();
						text += "\n (Version Mismatch)";
					}
				}
				PLabel headLine = MakeHeaderLabel(text,1.1f,0);
				headLine->SetColor(color);
				SetTop(headLine,0.01f);
				//attach->headLine = headLine;

				p->AddComponent(headLine,CLOCATION);

			}
		}




		bool incompatible = !info.rightVersion || info.protocolIncompatible;
		{
			AddHeader(p,String(host.serverInfo.gameInfo.numFactionsOnline) + "/" + String(host.serverInfo.gameInfo.numFactionsCreated),color,Distance::GameWindow(0.01f));
			AddHeader(p,host.serverInfo.gameInfo.roundNumber.ToString(),color,Distance::GameWindow(0.01f));
			PLabel l = AddHeader(p,CompileRules(host.serverInfo),color,Distance::GameWindow(0.01f));
			l->SetLocalFontScale(l->GetLocalFontScale() * 0.9f);

		}


		//0.7f * 1.5f = ~1
		//color set to 70% opacity
		bool first = true;
		foreach (info.addresses,addr)
		{
			PLabel label = MakeHeaderLabel(ToString(addr->address)+ " (ping "+String((int)ceil(addr->ping*1000))+')',0.7f,0);
			if (info.rightVersion && !info.protocolIncompatible)
				label->SetColor(float4(1,1,1,0.7f));
			else
				label->SetColor(incompatibleColor);
			{
				auto cfg = label->Anchor();
				cfg.Left(0.05f);
				if (first)
				{
					first = false;
					cfg.BeneathSomeOf(p,[p](const PComponent&c)->bool{return c != p->GetReferences().Last();});
				}
				else
				{
					cfg.BeneathLastOf(p);
				}
			}
			p->AddComponent( label, CLOCATION );
		}

		if (!info.rightVersion && !info.protocolIncompatible)
		{
			PLabel label = MakeHeaderLabel("Server identifies as '"+String(host.serverInfo.hostInfo.version)+'\'',0.7f,0);
			label->SetColor(incompatibleColor);
			label->Anchor().BeneathLastOf(p,Distance::GameWindow( 0.005f ));
			p->AddComponent( label ,CLOCATION);
					
			label = MakeHeaderLabel("Client expected '" SERVER_NAME "\'",0.7f,0);
			label->SetColor(incompatibleColor);
			label->Anchor().BeneathLastOf(p,Distance::GameWindow( 0.005f ));
			p->AddComponent( label ,CLOCATION);
		}

		p->SetAutoHeightSpacing(0.01f);
		p->backgroundTone = color.rgb * 0.75f;

		return p;
	}


	static const String allowedHostChars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-.:";

	bool CheckHostName(const String&url)
	{
		if (url.IsEmpty())
			return false;

		//could add more here. if i knew what

		return true;
	}


	PPanel	serverListPanel;

	void	Update( )
	{

		if (serverChoice->HasComponent(mousePressedComponent))
			return;	//can't activate right now
		WindowStack::VerifyIntegrity(CLOCATION);

		const bool isActive = serverChoice->IsActive();
		if (isActive)
			serverChoice->SetActive(false);




		serverChoice->ClearComponents();

		serverChoice->SetBackgroundTransparent();
		serverChoice->isModal = false;
		SetTop(serverChoice,0.1f);
		serverChoice->SetMaxAutoHeight(0.8f);

		serverChoice->AddComponent(headerRow,CLOCATION);
		if (!headerRow->HasComponents())
		{
			AddMainHeader("HostID/WorldID");
			AddMainHeader("Online");
			AddMainHeader("Round");
			AddMainHeader("Rules");
		}
		bool attemptingConnection = Network::client.IsAttemptingToConnect();

		//bool first =true;
		
		if (!serverListPanel)
		{
			serverListPanel.reset(new Panel());
			serverListPanel->Anchor().FillHorizontally().Top();
			serverListPanel->SetAutomaticHeight(true);
			serverListPanel->SetMaxAutoHeight(0.5f);
			serverListPanel->SetBackgroundTransparent();
		}
		serverListPanel->ClearComponents();
		serverListPanel->Anchor().BeneathAllOf(serverChoice);
		serverChoice->AddComponent( serverListPanel,CLOCATION );


		static Buffer0<Network::ServerAddress>	list;
		Network::ServerDiscovery::ListFailedAttempts(list);
		foreach (list,failed)
		{
			PPanel p(new Panel());
			p->Anchor()
				.BeneathLastOf(serverListPanel,Distance::GameWindow( 0.015f ))
				.FillHorizontally();
			//p->SetAutomaticHeight(true);
			p->SetBackgroundTransparent();

			PPanel pb(new Panel());
			//pb->autoHeight = true;
			p->AddComponent( pb ,CLOCATION);
			pb->Anchor().FillVertically().Left();
			
			

			PLabel label = MakeHeaderLabel("Still trying to reach '"+ToString(*failed)+"'...",0.7f,0);
//			label->anchorConfig.Left().Set(0.1f,false);
			label->SetColor(1,0.75f,0.75f,0.9f);
			label->Anchor().Top(Distance::GameWindow( 0.01f )).FillHorizontally();//.Right(Distance::GameWindow( 0.01f ));
			pb->AddComponent( label,CLOCATION );
			Network::ServerAddress url = *failed;
			PButton forget(new Button("Forget this host",
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
			forget->Anchor().Top().Right();
		
			p->AddComponent( forget ,CLOCATION);
			pb->Anchor().FillHBetween(nullptr,forget,Distance(),Distance(10,Distance::Type::AbsolutePixels));

			serverListPanel->AddComponent( p ,CLOCATION);
		}

		for (index_t i = 0; i < Network::servers.Count(); i++)
		{
			auto&info = Network::servers[i];

			PButtonPanel p = ConstructPanel(info);

			p->Anchor().BeneathLastOf(serverListPanel,Distance::GameWindow( 0.015f ));
			serverListPanel->AddComponent( p ,CLOCATION);
		}

		//count_t openAttempts = 0;





		if (Network::servers.IsEmpty())
		{
			PPanel p(new Panel());
			p->Anchor().BeneathAllOf(serverChoice, Distance::GameWindow(0.025f ));
			serverChoice->AddComponent( p ,CLOCATION);
			p->SetBackgroundTransparent();

			PLabel label = MakeHeaderLabel(" No Servers Found",0.7f,0);
			label->Anchor().Left(Distance::GameWindow( 0.1f ));
			label->SetColor(1,1,1,0.7f);
			p->AddComponent( label ,CLOCATION);
			p->SetAutomaticHeight(true);
			p->SetAutoHeightSpacing(p->GetAutoHeightSpacing() * 4.f);

		};


		PButton addHostButton;
		{
			PButton b(new Button("Add new host...",[]()
			{
				HUD::Dialog::Query("Enter a new host address to attempt connection to.","",allowedHostChars,CheckHostName,
				[](const String&url)
				{
					try
					{
						if (Network::ServerDiscovery::RegisterNewHost(Network::ServerAddress(url),false,false))
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
				mod.BeneathLastOf(serverChoice, Distance::GameWindow( 0.025f )).Left();
			}
			b->LockToIdealWidth();

			serverChoice->AddComponent( b ,CLOCATION);
			
			addHostButton = b;
		}
		

		{
			PButton start(new Button("Start local server..."));
			PButton stop(new Button("Stop local server"));
			bool running = ServerProcess::IsRunning();
			stop->SetEnabled(running);
			start->SetEnabled(!running);
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
				mod.RightOf(addHostButton,0.02f);
			}
			{
				auto mod = stop->Anchor();
				mod.GetVAxis() = addHostButton->GetAnchorConfig().GetVAxis();
				mod.RightOf(start,0.02f);
			}

			serverChoice->AddComponent(start,CLOCATION);
			serverChoice->AddComponent(stop,CLOCATION);
		}
		

		serverChoice->FullAutoLayout(CLOCATION);
		if (isActive)
		{
			serverChoice->SetActive(isActive);
			WindowStack::OverrideSignalMouseMotion();
		}
		
		HostList::SetVisibility(!Network::client.IsConnected());
	}
}}
#endif

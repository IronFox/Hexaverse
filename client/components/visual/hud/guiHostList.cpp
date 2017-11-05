#include <global_root.h>
#include "guiHostList.h"
#include "windowStack.h"
#include "../../network.h"
#include "dialog.h"
#include "../../../version.h"
#include <io/string_file.h>
#include <runtimeConfig.h>
#include "../../clientRuntimeConfig.h"
#include "../../serverProcess.h"
#include "../../audio/music.h"
#include "../../clientEvents.h"


#ifdef HAS_WINDOW

namespace HUD{namespace HostList{

	using namespace Component;

	class VisualAttachment: public Network::ServerInfo::VisualAttachment
	{
	public:
		PLabel			headLine;	//game/server ids, optionally (continue)
		PButtonPanel	buttonPanel;
	};

	const float4 incompatibleColor(1,0.6,0.55,1);

	static const float VisualTabWidth[] = {0.3f,0.15f,0.1f,0.25f};

	PWindow		serverChoice;
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
		serverChoice->SetAutoCenter(false);
		serverChoice->SetCenteredWidth(0.9f,TAnchor::RelativeToScreen);
		serverChoice->canBeImplicitlyClosed = false;

		headerRow.reset(new Panel());
		headerRow->FillHorizontally(0);
		headerRow->ModifyAnchorConfig().Top().Set(0);
		headerRow->fillBackground = false;
		headerRow->SetAutomaticHeight(true);
		
		Update();
	}

	bool	SetVisibility(bool doShow)
	{

		Music::SignalServerListShown(doShow);

		if (doShow)
		{
			if (WindowStack::ShowBottom(serverChoice))
				return true;
		}
		else
			WindowStack::Hide(serverChoice);
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
		label->SetWidth(VisualTabWidth[std::min(tabIndex,ARRAYSIZE(VisualTabWidth))]);
		{
			auto cfg = label->GetAnchorConfig();
			cfg.Top().Set(0.01f);
			cfg.Left().Set(0.01f);
			label->SetAnchorConfig(cfg);
		}
		label->SetText(text);
		label->SetLocalFontScale(zoom);
		label->SetVerticalPadding( 0);
		return label;
	};


	PLabel	AddHeader(const PPanel&context, const String&name, const float4&color, float verticalSpacing = 0.f, TAnchor::distanceType_t verticalSpacingIsRelative = TAnchor::RelativeToScreen)
	{
		const index_t tabIndex = context->CountComponents();
		PLabel label=MakeHeaderLabel(name/*+"["+String(tabIndex)+"]"*/,1.1f,tabIndex);
		{
			auto cfg = label->GetAnchorConfig();
			cfg.Top().Set(verticalSpacing,verticalSpacingIsRelative);
			cfg.Left().SetLastOf(context,0.f);
			label->SetAnchorConfig(cfg);
		}
		context->AddComponent(label);
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
		if (info.gameInfo.gameConfig.gameFlags & Protocol::GameFlag::ConfigurationMode)
			result += "Interactive Configuration Mode";
		else if (info.gameInfo.gameConfig.gameFlags & Protocol::GameFlag::TutorialMode)
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
		auto cfg = c->GetAnchorConfig();
		cfg.Top().Set(dist,TAnchor::RelativeToScreen);
		c->SetAnchorConfig(cfg);
	}

	PButtonPanel ConstructPanel(Network::ServerInfo&info)
	{

		const Network::Host&host = GetTargetHost(info);



		float4 color(1);
		if (!info.rightVersion || info.protocolIncompatible)
			color = incompatibleColor;


		if (info.visualAttachment)
		{
			To<VisualAttachment>::Cast(info.visualAttachment)->buttonPanel->ClearComponents();
		}
		else
		{
			auto attach = new VisualAttachment();
			info.visualAttachment.reset(attach);

			attach->buttonPanel.reset(new ButtonPanel());
			auto p = attach->buttonPanel;
			p->FillHorizontally(0);
			p->SetAutomaticHeight(true);
			bool versionMatch = info.rightVersion;

			TUniqueID id = info.id;
			const bool isConfigurationMode = (host.serverInfo.gameInfo.gameConfig.gameFlags & Protocol::GameFlag::ConfigurationMode) != 0;
			const bool isTutorialMode = (host.serverInfo.gameInfo.gameConfig.gameFlags & Protocol::GameFlag::TutorialMode) != 0;
			const bool requireLocalhost = isConfigurationMode || isTutorialMode;

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
				attach->headLine = headLine;
			}
		}


		PButtonPanel p = To<VisualAttachment>::Cast(info.visualAttachment)->buttonPanel;
		p->AddComponent(To<VisualAttachment>::Cast(info.visualAttachment)->headLine);


		bool incompatible = !info.rightVersion || info.protocolIncompatible;
		{
			AddHeader(p,String(host.serverInfo.gameInfo.numFactionsOnline) + "/" + String(host.serverInfo.gameInfo.numFactionsCreated),color,0.01f,TAnchor::RelativeToScreen);
			AddHeader(p,host.serverInfo.gameInfo.roundNumber.ToString(),color,0.01f,TAnchor::RelativeToScreen);
			PLabel l = AddHeader(p,CompileRules(host.serverInfo),color,0.01f,TAnchor::RelativeToScreen);
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
			auto cfg = label->GetAnchorConfig();
			cfg.Left().Set(0.05f);
			if (first)
			{
				first = false;
				cfg.Top().SetSomeOf(p,0.f,[p](const PComponent&c)->bool{return c != p->GetComponents().last();});
			}
			else
			{
				cfg.Top().SetLastOf(p,0.f,TAnchor::RelativeToScreen);
			}
			label->SetAnchorConfig(cfg);
			p->AddComponent( label );
		}

		if (!info.rightVersion && !info.protocolIncompatible)
		{
			PLabel label = MakeHeaderLabel("Server identifies as '"+String(host.serverInfo.hostInfo.version)+'\'',0.7f,0);
			label->SetColor(incompatibleColor);
			label->ModifyAnchorConfig().Top().SetLastOf(p,0.005f,TAnchor::RelativeToScreen);
			p->AddComponent( label );
					
			label = MakeHeaderLabel("Client expected '" SERVER_NAME "\'",0.7f,0);
			label->SetColor(incompatibleColor);
			label->ModifyAnchorConfig().Top().SetLastOf(p,0.005f,TAnchor::RelativeToScreen);
			p->AddComponent( label );
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

	void	Update()
	{

		if (serverChoice->HasComponent(mousePressedComponent))
			return;	//can't activate right now
		WindowStack::VerifyIntegrity(CLOCATION);

		const bool isActive = serverChoice->IsActive();
		if (isActive)
			serverChoice->SetActive(false);




		serverChoice->ClearComponents();

		serverChoice->fillBackground = false;
		serverChoice->isModal = false;
		serverChoice->SetAutoCenter(false);
		SetTop(serverChoice,0.1f);
		serverChoice->SetMaxAutoHeight(0.8f);

		serverChoice->AddComponent(headerRow);
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
			serverListPanel->FillHorizontally(0);
			serverListPanel->SetAutomaticHeight(true);
			serverListPanel->SetMaxAutoHeight(0.5f);
			serverListPanel->fillBackground = false;
		}
		serverListPanel->ClearComponents();
		serverListPanel->ModifyAnchorConfig().Top().SetAllOf(serverChoice,0);
		serverChoice->AddComponent( serverListPanel );


		static Buffer0<Network::ServerAddress>	list;
		Network::ServerDiscovery::ListFailedAttempts(list);
		foreach (list,failed)
		{
			PPanel p(new Panel());
			{
				auto cfg = p->GetAnchorConfig();
				cfg.Top().SetLastOf(serverListPanel,0.015f,TAnchor::RelativeToScreen);
				cfg.Left().Set(0.f);
				cfg.Right().Set(0.f);
				p->SetAnchorConfig(cfg);
			}
			p->SetAutomaticHeight(true);
			p->fillBackground = false;

			PPanel pb(new Panel());
			//pb->autoHeight = true;
			p->AddComponent( pb );
			{
				auto cfg = pb->GetAnchorConfig();
				cfg.Top().Set(0.f);
				cfg.Bottom().Set(0.f);
				cfg.Left().Set(0.f);
				pb->SetAnchorConfig(cfg);
			}
			
			

			PLabel label = MakeHeaderLabel("Still trying to query '"+ToString(*failed)+"'...",0.7f,0);
//			label->anchorConfig.Left().Set(0.1f,false);
			label->SetColor(1,0.75f,0.75f,0.9f);
			{
				auto cfg = label->GetAnchorConfig();
				cfg.Top().Set(0.01f,TAnchor::RelativeToScreen);
				cfg.Right().Set(0.01f,TAnchor::RelativeToScreen);
				label->SetAnchorConfig(cfg);
			}
			pb->AddComponent( label );
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
			{
				auto cfg = forget->ModifyAnchorConfig();
				cfg.Top().Set(0.f);
				cfg.Right().Set(0.f);
			}

			p->AddComponent( forget );

			{
				auto cfg = pb->ModifyAnchorConfig();
				cfg.Right().Set(forget,10,TAnchor::AbsolutePixels);
			}

			serverListPanel->AddComponent( p );
		}

		for (index_t i = 0; i < Network::servers.Count(); i++)
		{
			auto&info = Network::servers[i];

			PButtonPanel p = ConstructPanel(info);

			p->ModifyAnchorConfig().Top().SetLastOf(serverListPanel,0.015f,TAnchor::RelativeToScreen);
			serverListPanel->AddComponent( p );
		}

		//count_t openAttempts = 0;





		if (Network::servers.IsEmpty())
		{
			PButtonPanel p(new ButtonPanel());
			p->ModifyAnchorConfig().Top().SetAllOf(serverChoice,0.025f,TAnchor::RelativeToScreen);

			PLabel label = MakeHeaderLabel(" No Servers Found",0.7f,0);
			label->ModifyAnchorConfig().Left().Set(0.1f,TAnchor::RelativeToScreen);
			label->SetColor(1,1,1,0.7f);
			p->AddComponent( label );
			p->SetAutomaticHeight(true);
			p->SetAutoHeightSpacing(p->GetAutoHeightSpacing() * 4.f);

			serverChoice->AddComponent( p );
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
				auto mod = b->ModifyAnchorConfig();
				mod.Top().SetLastOf(serverChoice,0.025f,TAnchor::RelativeToScreen);
				mod.Left().Set(0.f);
			}
			b->LockToIdealWidth();

			serverChoice->AddComponent( b );
			
			addHostButton = b;
		}
		

		{
			PButton start(new Button("Start local server"));
			PButton stop(new Button("Stop local server"));
			bool running = ServerProcess::IsRunning();
			stop->SetEnabled(running);
			start->SetEnabled(!running);
			WButton wstart = start;
			WButton wstop = stop;
			start->SetTriggerFunction([wstart,wstop]()
			{
				ServerProcess::Begin(true);
				PButton b = wstart.lock();
				if (b)
					b->SetEnabled(false);
				b = wstop.lock();
				if (b)
					b->SetEnabled(true);
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
				auto mod = start->ModifyAnchorConfig();
				mod.Top() = addHostButton->GetAnchorConfig().Top();
				mod.Left().Set(addHostButton,0.02f);
			}
			{
				auto mod = stop->ModifyAnchorConfig();
				mod.Top() = addHostButton->GetAnchorConfig().Top();
				mod.Left().Set(start,0.02f);
			}

			serverChoice->AddComponent(start);
			serverChoice->AddComponent(stop);
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

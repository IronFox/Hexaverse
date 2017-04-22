#include <global_root.h>
#include "guiHostList.h"
#include "guiElements.h"
#include "../network.h"
#include "dialog.h"
#include "../../version.h"
#include <io/string_file.h>
#include <runtimeConfig.h>
#include "../clientRuntimeConfig.h"
#include "../serverProcess.h"




#ifdef HAS_WINDOW


	const float4 incompatibleColor(1,0.6,0.55,1);

	static const float VisualTabWidth = 0.2f;

	GUI::PLabel	MakeTabLabel(const String&text, float zoom)
	{
		using namespace GUI;
		PLabel label (new GUI::Label());
		label->alignCenter = false;
		label->SetLocation(Rect<>(0,0,VisualTabWidth,0.f));
		label->anchorConfig.Top().Set(0.01f);
		label->anchorConfig.Left().Set(0.01f);
		label->SetText(text);
		label->zoom = zoom;
		label->extraSpacing = 0;
		return label;
	};


	GUI::PLabel	AddTab(const GUI::PPanel&context, const String&name, bool incompatible, float verticalSpacing = 0.f, GUI::TAnchor::distanceType_t verticalSpacingIsRelative = GUI::TAnchor::RelativeToScreen)
	{
		using namespace GUI;
		PLabel label=MakeTabLabel(name,1.1f);
		label->anchorConfig.Top().Set(verticalSpacing,verticalSpacingIsRelative);
		if (context->HasComponents())
			label->anchorConfig.Left().Set(context->GetComponents().last(),0.f);
		else
			label->anchorConfig.Left().Set(0.f);
		context->AddComponent(label);
		if (incompatible)
			label->SetColor(incompatibleColor);
		return label;
	}

	GUI::PLabel	AddTab(const String&name, bool incompatible)
	{
		return AddTab(Dialog::serverChoice,name,incompatible);
	}

	static String CompileRules(const Protocol::ServerInfo&info)
	{
		String result;
		if (info.gameInfo.gameConfig.gameFlags & Protocol::GameFlag::ConfigurationMode)
			result += "Interactive Configuration Mode";
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

	GUI::PButtonPanel ConstructPanel(Network::ServerInfo&info)
	{
		using namespace GUI;

		const Network::Host&host = GetTargetHost(info);


		if (info.buttonPanel)
		{
			info.buttonPanel->ClearComponents();
		}
		else
		{
			info.buttonPanel.reset(new ButtonPanel());
			info.buttonPanel->anchorConfig.Left().Set(0.f);
			info.buttonPanel->anchorConfig.Right().Set(0.f);
			info.buttonPanel->autoHeight = true;
			bool versionMatch = info.rightVersion;

			TUniqueID id = info.id;
			const bool isConfigurationMode = (host.serverInfo.gameInfo.gameConfig.gameFlags & Protocol::GameFlag::ConfigurationMode) != 0;

			info.buttonPanel->SetCheck([versionMatch,id,isConfigurationMode]()
			{
				if (isConfigurationMode)
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

			info.buttonPanel->SetOnPress([id]()
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

		}


		PButtonPanel p = info.buttonPanel;

		if (!info.headLine)
		{
			String text;
			float4 color(1,1,1,1);
			if (info.rightVersion)
			{
				text = " "+info.id.ToNiceString()+" / "+host.serverInfo.gameInfo.worldID.value.ToNiceString();
				if (info.haveToken)
					text += " (continue)";
			}
			else
			{
				color = incompatibleColor * 1.3f;
				if (info.protocolIncompatible)
				{
					text = " <Unknown> (Protocol Incompatible)";
				}
				else
				{
					text = " "+info.id.ToNiceString()+" / "+host.serverInfo.gameInfo.worldID.value.ToNiceString();
					text += "\n (Version Mismatch)";
				}
			}
			PLabel headLine = MakeTabLabel(text,1.1f);
			headLine->SetColor(color);
			headLine->anchorConfig.Top().Set(0.01f,GUI::TAnchor::RelativeToScreen);
			info.headLine = headLine;
		}

		p->AddComponent(info.headLine);


		bool incompatible = !info.rightVersion || info.protocolIncompatible;
		{
			AddTab(p,String(host.serverInfo.gameInfo.numFactionsOnline) + "/" + String(host.serverInfo.gameInfo.numFactionsCreated),incompatible,0.01f,GUI::TAnchor::RelativeToScreen);
			AddTab(p,host.serverInfo.gameInfo.roundNumber.value.ToString(),incompatible,0.01f,GUI::TAnchor::RelativeToScreen);
			PLabel l = AddTab(p,CompileRules(host.serverInfo),incompatible,0.01f,GUI::TAnchor::RelativeToScreen);
			l->zoom *= 0.9f;

		}


		//0.7f * 1.5f = ~1
		//color set to 70% opacity
		bool first = true;
		foreach (info.addresses,addr)
		{
			PLabel label = MakeTabLabel(addr->address.ToString()+ " (ping "+String((int)ceil(addr->ping*1000))+')',0.7f);
			if (info.rightVersion && !info.protocolIncompatible)
				label->SetColor(float4(1,1,1,0.7f));
			else
				label->SetColor(incompatibleColor);
			label->anchorConfig.Left().Set(0.05f);
			if (first)
			{
				first = false;
				label->anchorConfig.Top().SetSomeOf(p,0.f,[p](const PComponent&c)->bool{return c != p->GetComponents().last();});
			}
			else
			{
				label->anchorConfig.Top().SetLastOf(p,0.f,GUI::TAnchor::RelativeToScreen);
			}
			p->AddComponent( label );
		}

		if (!info.rightVersion && !info.protocolIncompatible)
		{
			PLabel label = MakeTabLabel("Server identifies as '"+String(host.serverInfo.hostInfo.value.version)+'\'',0.7f);
			label->SetColor(incompatibleColor);
			label->anchorConfig.Top().SetLastOf(p,0.005f,GUI::TAnchor::RelativeToScreen);
			p->AddComponent( label );
					
			label = MakeTabLabel("Client expected '" SERVER_NAME "\'",0.7f);
			label->SetColor(incompatibleColor);
			label->anchorConfig.Top().SetLastOf(p,0.005f,GUI::TAnchor::RelativeToScreen);
			p->AddComponent( label );
		}

		p->autoHeightSpacing = 0.01f;
		if (!info.rightVersion || info.protocolIncompatible)
			p->backgroundTone = incompatibleColor.rgb * 0.75f;
		else
			p->backgroundTone = float3(1);
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


	GUI::PPanel	serverListPanel;

	void	UpdateServerChoice()
	{
		using namespace GUI;
		
		if (Dialog::serverChoice->HasComponent(mousePressedComponent))
			return;	//can't activate right now
		GUI::VerifyIntegrity(CLOCATION);

		const bool isActive = Dialog::serverChoice->IsActive();
		if (isActive)
			Dialog::serverChoice->SetActive(false);
		Dialog::serverChoice->ClearComponents();

		Dialog::serverChoice->fillBackground = false;
		Dialog::serverChoice->isModal = false;
		Dialog::serverChoice->autoCenter = false;
		Dialog::serverChoice->anchorConfig.Top().Set(0.1f);
		Dialog::serverChoice->maxHeight = 0.8f;

		AddTab("HostID/WorldID",false);
		AddTab("Factions online",false);
		AddTab("Round Number",false);
		AddTab("Rules",false);

		bool attemptingConnection = Network::client.IsAttemptingToConnect();

		//bool first =true;
		
		if (!serverListPanel)
		{
			serverListPanel.reset(new GUI::Panel());
			serverListPanel->anchorConfig.axis[0].min.Set(0);
			serverListPanel->anchorConfig.axis[0].max.Set(0);
			serverListPanel->autoHeight = true;
			serverListPanel->maxHeight = 0.5f;
			serverListPanel->fillBackground = false;
		}
		serverListPanel->ClearComponents();
		serverListPanel->anchorConfig.axis[1].max.SetAllOf(Dialog::serverChoice,0);
		Dialog::serverChoice->AddComponent( serverListPanel );


		static Buffer0<Network::ServerAddress>	list;
		Network::ServerDiscovery::ListFailedAttempts(list);
		foreach (list,failed)
		{
			PPanel p(new Panel());
			p->anchorConfig.Top().SetLastOf(serverListPanel,0.015f,GUI::TAnchor::RelativeToScreen);

			p->anchorConfig.Left().Set(0.f);
			p->anchorConfig.Right().Set(0.f);
			p->autoHeight = true;
			p->fillBackground = false;

			PPanel pb(new Panel());
			//pb->autoHeight = true;
			p->AddComponent( pb );
			pb->anchorConfig.Top().Set(0.f);
			pb->anchorConfig.Bottom().Set(0.f);
			pb->anchorConfig.Left().Set(0.f);
			
			

			PLabel label = MakeTabLabel("Still trying to query '"+failed->ToString()+"'...",0.7f);
//			label->anchorConfig.Left().Set(0.1f,false);
			label->SetColor(1,0.75f,0.75f,0.9f);
			label->anchorConfig.Top().Set(0.01f,GUI::TAnchor::RelativeToScreen);
			label->anchorConfig.Right().Set(0.01f,GUI::TAnchor::RelativeToScreen);
			pb->AddComponent( label );
			Network::ServerAddress url = *failed;
			PButton forget(new Button("Forget this host",
			[url]()
			{
				Dialog::Confirm("'"+url.ToString()+"' will be removed from the list of actively checked hosts. Please confirm.",
				[url]()
				{
					if (Network::ServerDiscovery::TryRemoveHost(url))
					{
						UpdateServerChoice();
						Network::SaveHosts();
					}
				},
				[url]()
				{
					return Network::ServerDiscovery::ListeningButCantConnectTo(url) ? Dialog::CheckResult::OkayContinue : Dialog::CheckResult::Abort;
				}
				);
			
			}));
			forget->zoom *= 0.9f;
			forget->extraSpacing *= 0.5f;
			forget->LockIdealWidth();
			forget->anchorConfig.Top().Set(0.f);
			forget->anchorConfig.Right().Set(0.f);

			p->AddComponent( forget );

			pb->anchorConfig.Right().Set(forget,10,GUI::TAnchor::AbsolutePixels);


			serverListPanel->AddComponent( p );
		}

		for (index_t i = 0; i < Network::servers.Count(); i++)
		{
			auto&info = Network::servers[i];

			PButtonPanel p = ConstructPanel(info);

			p->anchorConfig.Top().references.Clear();
			p->anchorConfig.Top().SetLastOf(serverListPanel,0.015f,GUI::TAnchor::RelativeToScreen);
			serverListPanel->AddComponent( p );
		}

		//count_t openAttempts = 0;





		if (Network::servers.IsEmpty())
		{
			PButtonPanel p(new ButtonPanel());
			p->anchorConfig.Top().SetAllOf(Dialog::serverChoice,0.025f,GUI::TAnchor::RelativeToScreen);

			PLabel label = MakeTabLabel(" No Servers Found",0.7f);
			label->anchorConfig.Left().Set(0.1f,GUI::TAnchor::RelativeToScreen);
			label->SetColor(1,1,1,0.7f);
			p->AddComponent( label );
			p->autoHeight = true;
			p->autoHeightSpacing *= 4.f;

			Dialog::serverChoice->AddComponent( p );
		};


		PButton addHostButton;
		{
			PButton b(new Button("Add new host...",[]()
			{
				Dialog::Query("Enter a new host address to attempt connection to.","",allowedHostChars,CheckHostName,
				[](const String&url)
				{
					try
					{
						if (Network::ServerDiscovery::RegisterNewHost(Network::ServerAddress(url),false,false))
							Network::SaveHosts();
					}
					catch (const std::exception&ex)
					{
						Dialog::ShowMessage(ex.what());
					}
				});
			
			},
			[]()->bool {return !Network::client.IsAttemptingToConnect() && !Network::client.IsConnected();}
			));
			b->anchorConfig.Top().SetLastOf(Dialog::serverChoice,0.025f,GUI::TAnchor::RelativeToScreen);
			b->anchorConfig.Left().Set(0.f);
			b->LockIdealWidth();

			Dialog::serverChoice->AddComponent( b );
			
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
			start->SetOnPress([wstart,wstop]()
			{
				ServerProcess::Begin(true);
				PButton b = wstart.lock();
				if (b)
					b->SetEnabled(false);
				b = wstop.lock();
				if (b)
					b->SetEnabled(true);
			});
			stop->SetOnPress([wstart,wstop]()
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
			start->anchorConfig.Top() = addHostButton->anchorConfig.Top();
			stop->anchorConfig.Top() = addHostButton->anchorConfig.Top();
			start->anchorConfig.Left().Set(addHostButton,0.02f);
			stop->anchorConfig.Left().Set(start,0.02f);

			Dialog::serverChoice->AddComponent(start);
			Dialog::serverChoice->AddComponent(stop);
		}
		

		Dialog::serverChoice->FullAutoLayout();
		if (isActive)
		{
			Dialog::serverChoice->SetActive(isActive);
			GUI::OverrideSignalMouseMotion();
		}
		
		Dialog::CheckServersAreShown(!Network::client.IsConnected());
	}

#endif

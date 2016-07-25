#include <global_root.h>
#include "guiHostList.h"
#include "guiElements.h"
#include "network.h"
#include "dialog.h"
#include "../version.h"
#include <io/string_file.h>
#include "../../shared/runtimeConfig.h"
#include "clientRuntimeConfig.h"
#include "serverProcess.h"

void LoadHosts()
{
	StringFile hosts;
	if (hosts.Open(RuntimeConfig::ConfigFile("hosts.txt")))
	{
		String host;
		while (hosts >> host)
			Network::ServerDiscovery::RegisterNewHost(Network::ServerAddress(host),false, false);
	}
}

void	SaveHosts()
{
	StringFile hosts;
	if (hosts.Create(RuntimeConfig::ConfigFile("hosts.txt")))
	{
		static Buffer0<Network::ServerAddress>	 urls;
		Network::ServerDiscovery::ListNonAutoAttempts(urls);
		foreach (urls,url)
			hosts << url->host << ':' << url->basePort << nl;
		hosts.Close();
	}
}


#ifdef HAS_WINDOW


	const float4 incompatibleColor(1,0.6,0.55,1);

	static const float VisualTabWidth = 0.2f;

	GUIElements::PLabel	MakeTabLabel(const String&text, float zoom)
	{
		using namespace GUIElements;
		PLabel label (new GUIElements::Label());
		label->alignCenter = false;
		label->SetLocation(Rect<>(0,0,VisualTabWidth,0.f));
		label->anchorConfig.Top().Set(0.01f);
		label->anchorConfig.Left().Set(0.01f);
		label->SetText(text);
		label->zoom = zoom;
		label->extraSpacing = 0;
		return label;
	};


	GUIElements::PLabel	AddTab(const GUIElements::PPanel&context, const String&name, bool incompatible, float verticalSpacing = 0.f, GUIElements::TAnchor::distanceType_t verticalSpacingIsRelative = GUIElements::TAnchor::RelativeToScreen)
	{
		using namespace GUIElements;
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

	GUIElements::PLabel	AddTab(const String&name, bool incompatible)
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

	static float PingOf(const std::pair<Network::ServerAddress,float>&p)
	{
		if (p.first.host == "localhost")
			return -1;
		return p.second;
	}

	GUIElements::PButtonPanel ConstructPanel(Network::ServerInfo&info)
	{
		using namespace GUIElements;
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
			info.buttonPanel->SetCheck([versionMatch]()
			{
				return versionMatch && !Network::client.IsAttemptingToConnect() && !Network::client.IsConnected();
			});

			TUniqueID id = info.id;
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
				float bestPing = PingOf(info->addresses.first());
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

				Network::serverID = info->id;
				Network::serverIDString = Network::serverID.ToNiceString();
				Network::worldID = info->info.gameInfo.worldID;
				Network::worldIDString = Network::worldID.ToNiceString();
				Network::gameFlags = info->info.gameInfo.gameConfig.gameFlags;
				RuntimeConfig::gameGoal = RuntimeConfig::GameGoal::Reinterpret(info->info.gameInfo.gameConfig.gameGoal);

				Network::ConnectTo(info->addresses[best].first);
			});

		}


		PButtonPanel p = info.buttonPanel;

		if (!info.headLine)
		{
			String text;
			float4 color(1,1,1,1);
			if (info.rightVersion)
			{
				text = " "+info.info.hostInfo.value.hostID.ToNiceString()+" / "+info.info.gameInfo.worldID.value.ToNiceString();
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
					text = " "+info.info.hostInfo.value.hostID.ToNiceString()+" / "+info.info.gameInfo.worldID.value.ToNiceString();
					text += "\n (Version Mismatch)";
				}
			}
			PLabel headLine = MakeTabLabel(text,1.1f);
			headLine->SetColor(color);
			headLine->anchorConfig.Top().Set(0.01f,GUIElements::TAnchor::RelativeToScreen);
			info.headLine = headLine;
		}

		p->AddComponent(info.headLine);


		bool incompatible = !info.rightVersion || info.protocolIncompatible;
		{
			AddTab(p,String(info.info.gameInfo.numFactionsOnline) + "/" + String(info.info.gameInfo.numFactionsCreated),incompatible,0.01f,GUIElements::TAnchor::RelativeToScreen);
			AddTab(p,info.info.gameInfo.roundNumber.value.ToString(),incompatible,0.01f,GUIElements::TAnchor::RelativeToScreen);
			PLabel l = AddTab(p,CompileRules(info.info),incompatible,0.01f,GUIElements::TAnchor::RelativeToScreen);
			l->zoom *= 0.9f;

		}


		//0.7f * 1.5f = ~1
		//color set to 70% opacity
		bool first = true;
		foreach (info.addresses,addr)
		{
			PLabel label = MakeTabLabel(addr->first.ToString()+ " (ping "+String((int)ceil(addr->second*1000))+')',0.7f);
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
				label->anchorConfig.Top().SetLastOf(p,0.f,GUIElements::TAnchor::RelativeToScreen);
			}
			p->AddComponent( label );
		}

		if (!info.rightVersion && !info.protocolIncompatible)
		{
			PLabel label = MakeTabLabel("Server identifies as '"+String(info.info.hostInfo.value.version)+'\'',0.7f);
			label->SetColor(incompatibleColor);
			label->anchorConfig.Top().SetLastOf(p,0.005f,GUIElements::TAnchor::RelativeToScreen);
			p->AddComponent( label );
					
			label = MakeTabLabel("Client expected '" SERVER_NAME "\'",0.7f);
			label->SetColor(incompatibleColor);
			label->anchorConfig.Top().SetLastOf(p,0.005f,GUIElements::TAnchor::RelativeToScreen);
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




	 void	UpdateServerChoice()
	 {
		using namespace GUIElements;
		
		if (Dialog::serverChoice->HasComponent(mousePressedComponent))
			return;	//can't activate right now
		Dialog::VerifyIntegrity(CLOCATION);

		const bool isActive = Dialog::serverChoice->IsActive();
		if (isActive)
			Dialog::serverChoice->Deactivate();
		Dialog::serverChoice->ClearComponents();

		Dialog::serverChoice->fillBackground = false;
		Dialog::serverChoice->isModal = false;
		Dialog::serverChoice->autoCenter = false;
		Dialog::serverChoice->anchorConfig.Top().Set(0.1f);


		AddTab("HostID/WorldID",false);
		AddTab("Factions online",false);
		AddTab("Round Number",false);
		AddTab("Rules",false);

		bool attemptingConnection = Network::client.IsAttemptingToConnect();

		bool first =true;

		for (index_t i = 0; i < Network::servers.Count(); i++)
		{
			auto&info = Network::servers[i];

			PButtonPanel p = ConstructPanel(info);

			p->anchorConfig.Top().references.Clear();
			if (first)
			{
				first = false;
				p->anchorConfig.Top().SetAllOf(Dialog::serverChoice,0.015f,GUIElements::TAnchor::RelativeToScreen);
			}
			else
				p->anchorConfig.Top().SetLastOf(Dialog::serverChoice,0.015f,GUIElements::TAnchor::RelativeToScreen);
			Dialog::serverChoice->AddComponent( p );
		}

		//count_t openAttempts = 0;
		static Buffer0<Network::ServerAddress>	list;
		Network::ServerDiscovery::ListFailedAttempts(list);

		foreach (list,failed)
		{
			PPanel p(new Panel());
			if (first)
			{
				first = false;
				p->anchorConfig.Top().SetAllOf(Dialog::serverChoice,0.025f,GUIElements::TAnchor::RelativeToScreen);
			}
			else
				p->anchorConfig.Top().SetLastOf(Dialog::serverChoice,0.025f,GUIElements::TAnchor::RelativeToScreen);

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
			label->anchorConfig.Top().Set(0.01f,GUIElements::TAnchor::RelativeToScreen);
			label->anchorConfig.Right().Set(0.01f,GUIElements::TAnchor::RelativeToScreen);
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
						SaveHosts();
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

			pb->anchorConfig.Right().Set(forget,10,GUIElements::TAnchor::AbsolutePixels);


			Dialog::serverChoice->AddComponent( p );
		}


		if (Network::servers.IsEmpty())
		{
			PButtonPanel p(new ButtonPanel());
			p->anchorConfig.Top().SetAllOf(Dialog::serverChoice,0.025f,GUIElements::TAnchor::RelativeToScreen);

			PLabel label = MakeTabLabel(" No Servers Found",0.7f);
			label->anchorConfig.Left().Set(0.1f,GUIElements::TAnchor::RelativeToScreen);
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
							SaveHosts();
					}
					catch (const std::exception&ex)
					{
						Dialog::ShowMessage(ex.what());
					}
				});
			
			},
			[]()->bool {return !Network::client.IsAttemptingToConnect() && !Network::client.IsConnected();}
			));
			b->anchorConfig.Top().SetLastOf(Dialog::serverChoice,0.025f,GUIElements::TAnchor::RelativeToScreen);
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
			Dialog::serverChoice->Activate();

		
		Dialog::CheckServersAreShown(!Network::client.IsConnected());
	}

#endif

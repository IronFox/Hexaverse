#include <global_root.h>
#include "guiHostList.h"
#include "guiElements.h"
#include "network.h"
#include "dialog.h"
#include "../version.h"
#include <io/string_file.h>

void LoadHosts()
{
	StringFile hosts;
	if (hosts.open("./config/hosts.txt"))
	{
		String host;
		while (hosts >> host)
			Network::ServerDiscovery::RegisterNewHost(host,false, false);
	}
}

void	SaveHosts()
{
	StringFile hosts;
	if (hosts.create("./config/hosts.txt"))
	{
		static Buffer0<String>	 urls;
		Network::ServerDiscovery::ListNonAutoAttempts(urls);
		foreach (urls,url)
			hosts << *url << nl;
		hosts.close();
	}
}


#ifdef HAS_WINDOW


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


	GUIElements::PLabel	AddTab(const GUIElements::PPanel&context, const String&name, float verticalSpacing = 0.f, GUIElements::TAnchor::distanceType_t verticalSpacingIsRelative = GUIElements::TAnchor::RelativeToScreen)
	{
		using namespace GUIElements;
		PLabel label=MakeTabLabel(name,1.1f);
		label->anchorConfig.Top().Set(verticalSpacing,verticalSpacingIsRelative);
		if (context->components.IsNotEmpty())
			label->anchorConfig.Left().Set(context->components.last(),0.f);
		else
			label->anchorConfig.Left().Set(0.f);
		context->components.Append(label);
		return label;
	}

	GUIElements::PLabel	AddTab(const String&name)
	{
		return AddTab(Dialog::serverChoice,name);
	}

	static String CompileRules(const Protocol::ServerInfo&info)
	{
		String result;
		switch (info.gameGoal)
		{
			case Objects::RuntimeConfig::GameGoal::ConquerNexus:
				result="Conquer Nexus";
			break;
			case Objects::RuntimeConfig::GameGoal::EMPConquerNexus:
				result="Conquer Nexus, EMP-Zones";
			break;
			case Objects::RuntimeConfig::GameGoal::None:
				result="No Goal";
			break;
		}
		if (info.gameFlags & Protocol::GameFlag::ClosedGame)
			result += ", Closed Game";
		if (info.gameFlags & Protocol::GameFlag::BasesRequired)
			result += ", Require Bases";
		if (info.gameFlags & Protocol::GameFlag::StartBases)
			result += ", Start Bases";
		if (info.gameFlags & Protocol::GameFlag::NexusAI)
			result += ", Nexus AI";
		if (info.gameFlags & Protocol::GameFlag::SupportRegularFactions)
			result += ", Regular";
		if (info.gameFlags & Protocol::GameFlag::SupportTechniteFactions)
			result += ", Technite";
		if (info.gameFlags & Protocol::GameFlag::SupportHybridFactions)
			result += ", Hybrid";

		//String result = String("Goal: ")+Protocol::GameGoal::Reinterpret(info.gameGoal).ToString();
		return result;


	}


	GUIElements::PButtonPanel ConstructPanel(Network::ServerInfo&info)
	{
		using namespace GUIElements;
		if (info.buttonPanel)
		{
			info.buttonPanel->components.Clear();
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
				return versionMatch && !Network::client.IsAttemptingToConnect();
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
				float bestPing = info->urls.first().second;
				for (index_t i = 1; i < info->urls.Count(); i++)
					if (info->urls[i].second < bestPing)
					{
						bestPing = info->urls[i].second;
						best = i;
					}
				info->selected = true;

				Network::serverID = info->id;
				Network::serverIDString = Network::serverID.ToNiceString();
				Network::worldID = info->info.worldID;
				Network::worldIDString = Network::worldID.ToNiceString();
				Network::gameFlags = info->info.gameFlags;
				Objects::RuntimeConfig::gameGoal = Objects::RuntimeConfig::GameGoal::Reinterpret(info->info.gameGoal);
				Network::client.ConnectAsync(info->urls[best].first + ":"+String(SERVICE_PORT));
			});

		}


		PButtonPanel p = info.buttonPanel;

		if (!info.headLine)
		{
			String text;
			float4 color(1,1,1,1);
			if (info.rightVersion)
			{
				text = " "+info.info.hostInfo.value.hostID.ToNiceString()+" / "+info.info.worldID.value.ToNiceString();
				if (info.haveToken)
					text += " (continue)";
			}
			else
			{
				color = float4(1,0.4,0.4,0.8) * 1.3f;
				if (info.protocolIncompatible)
				{
					text = " <Unknown> (Protocol Incompatible)";
				}
				else
				{
					text = " "+info.info.hostInfo.value.hostID.ToNiceString()+" / "+info.info.worldID.value.ToNiceString();
					text += "\n (Version Mismatch)";
				}
			}
			PLabel headLine = MakeTabLabel(text,1.1f);
			headLine->SetColor(color);
			headLine->anchorConfig.Top().Set(0.01f,GUIElements::TAnchor::RelativeToScreen);
			info.headLine = headLine;
		}

		p->components << info.headLine;



		{
			AddTab(p,String(info.info.numPlayers)/* + "/" + String(info.info.numClients)*/,0.01f,GUIElements::TAnchor::RelativeToScreen);
			AddTab(p,String(info.info.roundNumber),0.01f,GUIElements::TAnchor::RelativeToScreen);
			PLabel l = AddTab(p,CompileRules(info.info),0.01f,GUIElements::TAnchor::RelativeToScreen);
			l->zoom *= 0.9f;

		}


		//0.7f * 1.5f = ~1
		//color set to 70% opacity
		bool first = true;
		foreach (info.urls,url)
		{
			PLabel label = MakeTabLabel(url->first+ " (ping "+String((int)ceil(url->second*1000))+')',0.7f);
			label->SetColor(float4(1,1,1,0.7f));
			label->anchorConfig.Left().Set(0.05f);
			if (first)
			{
				first = false;
				label->anchorConfig.Top().SetSomeOf(p,0.f,[p](const PComponent&c)->bool{return c != p->components.last();});
			}
			else
			{
				label->anchorConfig.Top().Set(p->components.last(),0.f,GUIElements::TAnchor::RelativeToScreen);
			}
			p->components << label;
		}

		if (!info.rightVersion && !info.protocolIncompatible)
		{
			PLabel label = MakeTabLabel("Server identifies as '"+String(info.info.hostInfo.value.version)+'\'',0.7f);
			label->SetColor(1,0.5,0.5,0.7f);
			label->anchorConfig.Top().Set(p->components.last(),0.005f,GUIElements::TAnchor::RelativeToScreen);
			p->components << label;
					
			label = MakeTabLabel("Client expected '" SERVER_NAME "\'",0.7f);
			label->SetColor(1,0.5,0.5,0.7f);
			label->anchorConfig.Top().Set(p->components.last(),0.005f,GUIElements::TAnchor::RelativeToScreen);
			p->components << label;
		}

		p->autoHeightSpacing = 0.01f;
		
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
			

		const bool isActive = Dialog::serverChoice->IsActive();
		if (isActive)
			Dialog::serverChoice->Deactivate();
		Dialog::serverChoice->components.Clear();

		Dialog::serverChoice->fillBackground = false;
		Dialog::serverChoice->isModal = false;
		Dialog::serverChoice->autoCenter = false;
		Dialog::serverChoice->anchorConfig.Top().Set(0.1f);


		AddTab("HostID/WorldID");
		AddTab("Factions");
		AddTab("Round Number");
		AddTab("Rules");

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
				p->anchorConfig.Top().Set(Dialog::serverChoice->components.last(),0.015f,GUIElements::TAnchor::RelativeToScreen);
			Dialog::serverChoice->components << p;
		}

		//count_t openAttempts = 0;
		static Buffer0<String>	list;
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
			p->components << pb;
			pb->anchorConfig.Top().Set(0.f);
			pb->anchorConfig.Bottom().Set(0.f);
			pb->anchorConfig.Left().Set(0.f);
			
			

			PLabel label = MakeTabLabel("Still trying to query '"+*failed+"'...",0.7f);
//			label->anchorConfig.Left().Set(0.1f,false);
			label->SetColor(1,0.75f,0.75f,0.9f);
			label->anchorConfig.Top().Set(0.01f,GUIElements::TAnchor::RelativeToScreen);
			label->anchorConfig.Right().Set(0.01f,GUIElements::TAnchor::RelativeToScreen);
			pb->components << label;
			String url = *failed;
			PButton forget(new Button("Forget this host",
			[url]()
			{
				Dialog::Confirm("'"+url+"' will be removed from the list of actively checked hosts. Please confirm.",
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

			p->components << forget;

			pb->anchorConfig.Right().Set(forget,10,GUIElements::TAnchor::AbsolutePixels);


			Dialog::serverChoice->components << p;
		}


		if (Network::servers.IsEmpty())
		{
			PButtonPanel p(new ButtonPanel());
			p->anchorConfig.Top().SetAllOf(Dialog::serverChoice,0.025f,GUIElements::TAnchor::RelativeToScreen);

			PLabel label = MakeTabLabel(" No Servers Found",0.7f);
			label->anchorConfig.Left().Set(0.1f,GUIElements::TAnchor::RelativeToScreen);
			label->SetColor(1,1,1,0.7f);
			p->components << label;
			p->autoHeight = true;
			p->autoHeightSpacing *= 4.f;

			Dialog::serverChoice->components << p;
		};


		{
			PButton b(new Button("Add new host...",[]()
			{
				Dialog::Query("Enter a new host address to attempt connection to.","",allowedHostChars,CheckHostName,
				[](const String&url)
				{
					if (Network::ServerDiscovery::RegisterNewHost(url,false,false))
						SaveHosts();
				});
			
			}));
			b->anchorConfig.Top().SetLastOf(Dialog::serverChoice,0.025f,GUIElements::TAnchor::RelativeToScreen);
			b->anchorConfig.Left().Set(0.f);
			b->LockIdealWidth();

			Dialog::serverChoice->components << b;
		}
		

		Dialog::serverChoice->FullAutoLayout();
		if (isActive)
			Dialog::serverChoice->Activate();

		
		Dialog::CheckServersAreShown(!Network::client.IsConnected());
	}

#endif

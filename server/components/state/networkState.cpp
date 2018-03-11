#include <global_root.h>
#include "networkState.h"

#include <protocol.h>
#include <objects.h>
#include <runtimeConfig.h>
#include "../journalSaveGame.h"
#include "../networkChannels.h"
#include "../aiProcess.h"
#include "../serverLogEvent.h"
#include "../udpBroadcast.h"
#include "worldState.h"
#include <globalEvents.h>



#undef StartService

extern TwisterRandom random;


void TerminateNow(bool isMain);


namespace ApplicationInterface
{
	void	SignalAllianceInvite(const Objects::PAlliance&a, const Objects::PFaction&f,BYTE type)
	{
		Channels::AllianceInvite.SendTo(Network::server,Protocol::AllianceInvite(a->id,f->id,type),Channels::Level::Faction);
	}

	void	SignalAllianceMembershipChange(const Objects::PFaction&owner)
	{
		if (!owner->isDead)
			Channels::identifyFaction.SendTo(::Network::server,Network::Identify(owner),Channels::Level::Faction);
		SignalFactionsChanged();	//for statistical purposes
	}

	void	SignalAllianceConfigChange(const Objects::PAlliance&alliance)
	{
		Protocol::AllianceConfigChange ch;
		ch.allianceID = alliance->id;
		ch.flags = alliance->config.ToFlags();
		alliance->ForeachMember([ch](const Objects::PFaction&f)
		{
			Network::PClient client;
			if (Network::playerMap.Query(f,client))
			{
				Channels::AllianceConfigChange.SendTo(client->peer.lock(),ch);
			}
		});
	}


}


namespace Network
{
	Ctr::Buffer<PClient>	activePlayersAndSpectators,spectators;
	Ctr::GenericHashTable<Objects::PFaction,PClient,Objects::OwnerHash>	playerMap;
	Ctr::GenericHashTable<CryptographicID,Objects::PFaction>		tokenMap;


	namespace Heartbeat
	{
		SOCKET s = INVALID_SOCKET;
		Timer::Time lastBeat = timer.Now();

		class Loop : public Sys::ThreadObject
		{
		public:
			virtual void ThreadMain()
			{
				BYTE buffer[1024];
				while (s != INVALID_SOCKET)
				{
					sockaddr_in addr;
					int size = (int)sizeof (addr);
					int bytes = recvfrom(s,(char*)buffer, sizeof(buffer), 0, (SOCKADDR *) & addr, &size);
					//LogEvent(EventSignificance::Debug,"Received heartbeat from "+TCP::ToString((const sockaddr_storage&)addr,size));
					if (bytes <= 0)
					{
						if (s != INVALID_SOCKET)
							FATAL__("Heartbeat receive failure. Socket lost");
					}
					lastBeat = timer.Now();
				}
			}
		};
		Loop	loopThread;

		void	Stop()
		{
			if (s != INVALID_SOCKET)
			{
				SOCKET s1 = s;
				s = INVALID_SOCKET;
				closesocket(s1);
			}
		}

		void	Start(UINT16 basePort)
		{
			//LogEvent(EventSignificance::Uncommon,"Starting heartbeat receiver on port "+String(HEARTBEAT_PORT));
			GlobalEvents::applicationTerminate << Stop;
			ASSERT__(Net::initNet());

			s = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
			ASSERT_NOT_EQUAL__(s,INVALID_SOCKET);
			

			SOCKADDR_IN me;
			memset(&me,0,sizeof(me));
			me.sin_family=AF_INET;
			me.sin_port=htons(basePort + PortOffset::Heartbeat);;
			me.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
			LogEvent(EventSignificance::Uncommon,"Binding heartbeat receiver to "+TCP::ToString((const sockaddr_storage&)me,sizeof(me)));
			int rc=::bind(s,(SOCKADDR*)&me,sizeof(SOCKADDR_IN));
			if (rc != 0)
			{
				closesocket(s);
				s = INVALID_SOCKET;
				FATAL__("Unable to bind UDP heartbeat socket. Chances are, a server is already running.");
				return;
			}
			loopThread.Start();
			lastBeat = timer.Now();
		}





		void	Check()
		{
			if (s == INVALID_SOCKET)
				return;

			static const float timeOut = 3;
			if (timer.GetSecondsSince(lastBeat) > timeOut)
			{
				LogEvent(EventClass::Preset::fatal,"No heartbeat received in "+String(timeOut)+" seconds. Shutting down");

				application_shutting_down = true;	//prevent certain operations in Close()
				try
				{
					PathString out = RuntimeConfig::sharedConfig.Lookup("saves/no-heartbeat-shutdown.hsf");
					Logging::WriteEvent(EventClass::Preset::uncommon,"Trying to save game to "+String(out),nullptr,nullptr,nullptr);
					FileIO::Save(out,RuntimeConfig::Phase::CommandPhase);
					Logging::WriteEvent(EventClass::Preset::uncommon,"Success",nullptr,nullptr,nullptr);
				}
				catch (...)
				{
					Logging::WriteEvent(EventClass::Preset::error,"Failed to save to file",nullptr,nullptr,nullptr);
				}

				TerminateNow(true);
			}
		}

	}

	

	namespace InformationProvider
	{
		TCP::Server		server;
		bool			created = false;

		void	OnPing(Protocol::Ping&ping, TCP::Peer&peer);

		TCP::ObjectSender<Protocol::ServerInfo,Protocol::InfoChannel::ServerInfo>	infoSender;
		TCP::Channel<Protocol::Ping,0,Protocol::InfoChannel::Ping>	pingChannel(OnPing);

		void	OnPing(Protocol::Ping&ping, TCP::Peer&peer)
		{
			pingChannel.SendTo(peer,ping);
		}



		class Updater : public Sys::ThreadObject
		{
		public:
			volatile bool	doEnd = false;

			void	ThreadMain() override
			{
				Protocol::ServerInfo infoGlobal,infoLocal;

				while (!UDPBroadCast::PacketIsDone())
				{
					Sleep(500);
				}

				while (!application_shutting_down && !doEnd)
				{
					Sleep(1000);
					infoGlobal.hostInfo = UDPBroadCast::packet;
					infoLocal.hostInfo = UDPBroadCast::packet;

					Game::Network::AssembleGameInfo(infoGlobal.gameInfo,false);
					Game::Network::AssembleGameInfo(infoLocal.gameInfo,true);

					server.VisitAllPeers([infoGlobal,infoLocal](const TCP::PPeer&p)
					{
						if (p->IsLocalhost())
							infoSender.SendTo(p,infoLocal);
						else
							infoSender.SendTo(p,infoGlobal);
					});
					//infoSender.SendTo(server,info);
				}
				server.EndService();
			}
		}		updater;


		void		Start(UINT16 basePort)
		{
			if (!created)
			{
				created = true;
				server.InstallChannel(pingChannel);
			}

			UINT16 port = basePort + PortOffset::Info;

			if (!server.StartService(port))
				FATAL__("Unable to register information server. Chances are, a server is already running on port "+String(port)+".")
			else
			{
				LogEvent(EventSignificance::Uncommon,"Info server started on port "+String(port));
				updater.doEnd = false;
				updater.start();
			}



		}

		void		Stop()
		{
			updater.doEnd = true;
			updater.awaitCompletion();
		}

		bool	IsOnline()
		{
			return server.IsOnline();
		}


	}


	namespace Config
	{
		UINT16		sessionPort = DEFAULT_SERVER_BASE_PORT + PortOffset::Session;
	}

	namespace Session
	{
		bool	isStarted=false;
	}


	TUniqueID		serverID;
	TCP::Server		server;


	static void StartServer(bool logStateAfterRestart)
	{
		if (server.IsOnline())
		{
			if (server.GetPort() == Config::sessionPort)
				return;
			server.EndService();
		}
		server.OnDoneResolving([logStateAfterRestart]()
		{
			if (server.StartService(Config::sessionPort))
			{
				LogEvent(EventClass::Preset::network,"Session server started on port "+String(Config::sessionPort));
				if (logStateAfterRestart)
					LogServerState();
			}
			else
				FATAL__("Unable to start service in port "+String(Config::sessionPort)+" ("+server.GetError(true)+"). Chances are, a server is already running on that port.")
		});
	}

	void		Tick()
	{
		server.Resolve();
		Heartbeat::Check();
	}




	void ChangeUserLevel(TCP::Peer&peer, UserLevel newLevel)
	{
		if (peer.userLevel == (UINT32)newLevel)
			return;
		peer.userLevel = (UINT32)newLevel;
		LogEvent(EventSignificance::Debug,peer,"User level changed to "+String(peer.userLevel));
	}

	void		BuildConfiguredWorld()
	{
		bool started = Network::CloseSession(false,false);
		if (started)
			World::worldID.Generate();

		World::UpdateEMPZones(0);
		World::BuildConfiguredWorld(random);

		if (started)
			Network::RestartSession(false);
		else
			World::Network::CheckAllFactionCoverageAndSendAsNeeded();
	}

	Protocol::IdentifyFaction Identify(const Objects::PFaction & f)
	{
		Protocol::IdentifyFaction	id;
		id.id = f->id;
		id.allianceID = f->alliance->id;
		id.name = f->name;
		id.color = f->color;
		return id;
	}


	String						Client::ConvertToString() const
	{
		return (faction ? ToString(faction->id) : (isSpectator ? "[spectator]" : "[no faction]"))+":'"+name+"'";
	}

	bool		Client::CanSee(const Objects::PWorldEntity&e, float t)	const
	{
		return isSpectator || (faction && faction->CanSee(e,t));
	}


	static String AllianceStatus(const Objects::PAlliance&a, const Objects::Alliance::TNexusStats&stats)
	{
		if (!a)
			return "Game Over";
		return "Conquering Nexus Ports: "+String(stats.acquiringNexusControl)+", conquered: "+String(stats.nexusControl)+"/"
			+ String(Game::Config::GetTargetNexusCount());
	}

	static String FactionWarning(const Objects::PFaction&f, const String&warningReason)
	{
		if (!f)
			return "Game Over";
		if (Game::Config::basesRequired && f->deathInRounds > 0)
			return warningReason+": Faction death in "+(f->deathInRounds > 1 ? String(f->deathInRounds)+" rounds" : "ONE round");
		return "";
	}


	void Client::SendProgressUpdate(const String&warningReason, const Objects::Alliance::TNexusStats&stats)
	{
		Protocol::GameProgress progress;
		const bool dead = !faction || faction->isDead;
		progress.gameOver = dead || RuntimeConfig::phase == RuntimeConfig::Phase::EndOfGame;
		auto winner = Game::winner.lock();
		progress.gameWon = faction && faction->alliance == winner;
		progress.status = RuntimeConfig::gameGoal == RuntimeConfig::GameGoal::None ? "" : AllianceStatus(faction ? faction->alliance : nullptr,stats);
		progress.warning = dead ? "Game Over" : FactionWarning(faction,warningReason);


		auto ppeer = peer.lock();
		if (!ppeer)
			return;
		{
			auto&peer = *ppeer;
			if (progress.gameOver && faction && RuntimeConfig::phase != RuntimeConfig::Phase::EndOfGame)
			{
				//send only statistics of local faction
				Game::Network::SendStatisticsTo(peer,faction);
			}


			if (RuntimeConfig::phase == RuntimeConfig::Phase::EndOfGame)
			{
				//game over: send statistics of all factions
				Objects::ForeachFaction([&peer](const Objects::PFaction&f)
				{
					Game::Network::SendStatisticsTo(peer,f);
				});
			}

			Channels::gameProgress.SendTo(ppeer,progress);
		}
	}


	void		StartHeartbeat(UINT16 basePort)
	{
		Heartbeat::Start(basePort);
	}


	void		Start(UINT16 basePort)
	{
		SetupServer();
		StartServer(false);
		Session::isStarted = true;

		InformationProvider::Start(basePort);
	}

	void		Stop()
	{
		Network::server.StopService();
		InformationProvider::Stop();
	}


	void	SetupServer()
	{
		static bool setup = false;
		if (setup)
			return;
		setup = true;

		server.InstallChannel(Channels::hello);
		server.InstallChannel(Channels::entityDirectCommand);
		server.InstallChannel(Channels::choiceIntersection);
		server.InstallChannel(Channels::dropTasks);
		server.InstallChannel(Channels::signalNewJob);
		server.InstallChannel(Channels::signalTerrainMod);
		server.InstallChannel(Channels::constructionAbort);
		server.InstallChannel(Channels::constructionBegin);
		server.InstallChannel(Channels::enableStructure);
		server.InstallChannel(Channels::clientFactionRequest);
		server.InstallChannel(Channels::removeEntity);
		server.InstallChannel(Channels::restoreCoreLink);
		server.InstallChannel(Channels::automate);
		server.InstallChannel(Channels::sleep);
		server.InstallChannel(Channels::createMarker);
		server.InstallChannel(Channels::destroyMarker);
		server.InstallChannel(Channels::touchMarker);
		server.InstallChannel(Channels::changeMyName);
		server.InstallChannel(Channels::identifyFactionRecv);
		server.InstallChannel(Channels::queueResearch);
		server.InstallChannel(Channels::ChatMessage);
		server.InstallChannel(Channels::requestResource);
		server.InstallChannel(Channels::tutorialAt);

		server.InstallChannel(Channels::AllianceInvite);
		server.InstallChannel(Channels::ClientVote);
		server.InstallChannel(Channels::AllianceConfigChange);

		server.InstallChannel(Channels::SaveAs);

		server.OpenSignalChannel(Protocol::ChannelID::RequestRoundEnd,(UINT32)UserLevel::CommonFaction);
		server.OpenSignalChannel(Protocol::ChannelID::RequestContinueRound,(UINT32)UserLevel::CommonFaction);
		server.OpenSignalChannel(Protocol::ChannelID::Ping,0);
		server.OpenSignalChannel(Protocol::ChannelID::RequestEverything,(UINT32)UserLevel::Spectator);
		server.OpenSignalChannel(Protocol::ChannelID::LeaveAlliance,(UINT32)UserLevel::CommonFaction);
		
		server.onSignal = Receiver::OnSignal;
		server.SetAsync(false);
		server.onEvent = Receiver::OnEvent;
		server.onIgnorePackage = Receiver::OnIgnorePackage;
		server.onDeserializationFailed = Receiver::OnDeserializationFailed;


		GlobalEvents::applicationTerminate << []
		{
			server.Shutdown();

		};
	}

	bool FactionIsActive(const Objects::PFaction&f)
	{
		foreach (activePlayersAndSpectators,a)
			if (!(*a)->isSpectator && (*a)->faction == f)
				return true;
		return false;
	}


	bool AnyHumanPlayersActive()
	{
		count_t activePlayers = activePlayersAndSpectators.Count() - spectators.Count();
		return activePlayers > 1 || (activePlayers == 1 && !FactionIsActive(Objects::Faction::nexus));
	}

	void HandleConnectionLoss(PClient client)
	{
		//State::clients.FindAndErase(client);
		activePlayersAndSpectators.FindAndErase(client);
		spectators.FindAndErase(client);
		if (client->faction)
		{
			Journal::currentRound.LogFactionOffline(client->faction);

			playerMap.Unset(client->faction);
			client->faction->online = false;
			Channels::factionOffline.SendTo(server,client->faction->id,Channels::Level::Faction);
		}
	}





	void	RestartSession(bool logStateAfterRestart)
	{
		LogEvent(EventSignificance::Important,"Restarting session.");
		#ifndef NO_HIBERNATION
			UpdateHibernation();
		#endif

		Objects::ForeachFaction([](const Objects::PFaction&f)
		{
			f->DropPerRoundData();	//make sure any remnant timing information from previous operations is dropped
		});



		server.FlushPendingEvents();
		StartServer(logStateAfterRestart);

		Game::Network::HandleSessionRestart();

		Game::ResetFactionStartMineralValue();

//		Objects::VerifyVisibilityNetwork();
		Objects::CheckMobileEntitySanity();
		Objects::VerifyGrid();

		Session::isStarted = true;

		World::Network::CheckAllFactionCoverageAndSendAsNeeded();
	}


	bool		SuspendSession()
	{
		bool isOnline = server.IsOnline();
		if (!isOnline)
			return false;
		LogEvent(EventSignificance::Important,"Suspending session.");

		server.Shutdown();

		activePlayersAndSpectators.Clear();
		spectators.Clear();
		playerMap.Clear();

		Game::Network::HandleSessionSuspension();


		return true;

	}


	bool		SessionIsStarted()
	{
		return Session::isStarted;
	}


	bool		CloseSession(bool preserveSpectators, bool sendGridChangesToPreservedSpectators/*=true*/)
	{
		bool isOnline = Session::isStarted;
		LogEvent(EventSignificance::Important,isOnline ? "Closing session." : "Not online: Flushing session data.");

		if (isOnline)
		{
			Session::isStarted = false;
			if (preserveSpectators)
			{
				Game::Config::factionSupportFlags = Protocol::GameFlag::SupportSpectatorFactions;
				Ctr::Buffer0<TCP::PPeer>	dump;
				server.VisitAllPeers([&dump](const TCP::PPeer&p)
				{
					if (p->userLevel < (UINT32)UserLevel::Spectator)
						dump << p;
				});
				foreach (dump,d)
				{
					(*d)->DisconnectPeer();
				}
			}
			else
				server.Shutdown();

			if (!preserveSpectators)
			{
				activePlayersAndSpectators.Clear();
				spectators.Clear();
			}
			else
			{
				for (index_t i = 0; i < activePlayersAndSpectators.Count(); i++)
				{
					if (!activePlayersAndSpectators[i]->isSpectator)
						activePlayersAndSpectators.Erase(i--);
				}

			}
		}
		playerMap.Clear();


		RuntimeConfig::doomsdayWeapons = RuntimeConfig::DoomsdayWeapons();	//load default value
		tokenMap.Clear();

		World::Network::HandleSessionClosure();
		Game::Network::HandleSessionClosure();
		FileIO::Network::HandleSessionClosure();
		RuntimeConfig::roundNumber = 0;
		RuntimeConfig::phase = RuntimeConfig::Phase::CommandPhase;
		RuntimeConfig::tutorialMode = false;

		if (spectators.IsNotEmpty())
		{
			Objects::ForeachWorldEntityIncludingStored([](const Objects::PWorldEntity&e)
			{
				Protocol::DestroyEntity d;
				d.id = e->id;
				d.type = e->isInsertedIntoGrid ? Objects::RemovalType::Vanished : Objects::RemovalType::DestroyedInStorage;
				d.timestamp.rightNow = true;
				Channels::destroyEntity.SendTo(server,d);
			},false);

		}

		GlobalEvents::FlushAll();

		if ((preserveSpectators || !isOnline) && sendGridChangesToPreservedSpectators)
			World::Network::CheckAllFactionCoverageAndSendAsNeeded();

		return isOnline;

	}

	void LogConfig(const String&msg)
	{
		Logging::WriteEvent(EventClass::Preset::configurationPrint,msg,nullptr,nullptr,nullptr);
	}

	void		LogServerState()
	{
		LogEvent(EventSignificance::Important,Network::DescribeStatus());
		Network::DescribeConfiguration([](const String&line){LogEvent(EventClass::Preset::configurationPrint, line);});
		FileIO::RefreshWindowTitle();
	}

	String		DescribeStatus()
	{
		if (server.IsOnline())
			return "Server "+ToString(Network::serverID)+"/"+UDPBroadCast::packet.version+" is online.";
		return "Server "+ToString(Network::serverID)+"/"+UDPBroadCast::packet.version+" is created but offline.";
	}


	void		DescribeConfiguration(const std::function<void(const String&)>&logConfig)
	{
		//Logging::WriteEvent(EventSignificance::Important,
		//FileIO::RefreshWindowTitle();

		{
			logConfig("AI is configured as follows:");
			if (Game::Config::Nexus::enableAI)
			{
				logConfig(" -Nexus-AI enabled");
				logConfig(" -ThreatEstimationFactor=" + String(Game::Config::Nexus::threatEstimationFactor));
				//LogConfig(" -NexusTurrets=" + implode(',', State::Config::Nexus::vehicleTurretNames));
			}
			else
				logConfig(" -Disabled");

			logConfig("This game is configured as follows:");
			#ifndef NO_HIBERNATION
				logConfig(" -Uplink traffic per faction is limited to "+String(State::Config::uplinkBandwidth));
			#endif

			logConfig(" -Factions: "+Game::Config::FactionSupportFlagString());

			if (World::Config::simulateLiquids)
				logConfig(" -Liquids: Expanding.");
			else
				logConfig(" -Liquids: Static.");
			switch (RuntimeConfig::gameGoal)
			{
				case RuntimeConfig::GameGoal::None:
					logConfig(" -Goal: None.");
				break;
				case RuntimeConfig::GameGoal::ConquerNexus:
					static_assert(BaseGrid::IsolatedCornerCount==3,"Unexpected count");
					logConfig(" -Goal: Connect and enable 2+ Nexus Ports.");
				break;
			}
			if (Game::Config::closedGame)
				logConfig(" -Closed: Joining will be restricted to first round and access tokens. Game can be won.");
			else
				logConfig(" -Open: Players can join any time. Game is endless.");
			if (Game::Config::Nexus::bases.havePlaced)
				logConfig(" -Nexus-Bases: Nexus bases have been randomly created during world creation");
			else
				logConfig(" -No Nexus-Bases: Nexus bases have not been created");

			switch (RuntimeConfig::doomsdayWeapons)
			{
				case RuntimeConfig::DoomsdayWeapons::Conceal:
					logConfig(" -Doomsday weapons: Treated like normal weapons (must be in sensor range to see)");
				break;
				case RuntimeConfig::DoomsdayWeapons::Disallow:
					logConfig(" -Doomsday weapons: Forbidden to fire (can still be built, however)");
				break;
				case RuntimeConfig::DoomsdayWeapons::Expose:
					logConfig(" -Doomsday weapons: Doomsday explosions are made visible to all factions");
				break;
				default:
					logConfig(" -Doomsday weapons: "+String(RuntimeConfig::doomsdayWeapons.ToString()));
				break;
			}

			if (Game::Config::roundTimer > 0)
				logConfig(" -Round Timer: Rounds beyond the first may last at most "+String(Game::Config::roundTimer)+" seconds.");
			else
				logConfig(" -No Round Timer: Rounds end when all players requested so.");
			if (Game::Config::basesRequired)
				logConfig(" -Bases Required: Factions that do not meet the station requirements die after five rounds.");
			else
				logConfig(" -No Survival Conditions: Factions live forever (although their vehicles/structures might not).");
			if (Game::Config::startBases)
				logConfig(" -Bases: New factions are given minimal bases.");
			else
				logConfig(" -Flexibility: New factions are given vehicles, but no structures.");
			if (Objects::newVehiclesHaveFullAmmo)
				logConfig(" -Ammo: New vehicles using standard ammo are given full ammo.");
			else
				logConfig(" -No Ammo: New vehicles have zero ammo.");
			if (Game::Config::exposeActionLeftToHostileFactions)
				logConfig(" -Expose Vehicle Initiative: Vehicle initiative is exposed to hostile factions.");
			else
				logConfig(" -Private Vehicle Action: Vehicle movement is not exposed to hostile factions.");
			if (RuntimeConfig::empZones)
				logConfig(" -EMP Zones: Vehicles entering residual EMP fields will suffer extensive EMP damage.");
			else
				logConfig(" -No EMP Zones: The entire planet is freely accessible.");

			if (FileIO::Config::saveEveryNRounds > 0)
				logConfig(" -Auto-saves will be created every "+String(FileIO::Config::saveEveryNRounds)+" round(s).");
			if (FileIO::Config::saveWhenAllQuit)
				logConfig(" -Auto-saves will be created when all quit.");
		}


	}



	void InvalidOperation(TCP::Peer&peer)
	{
		PClient client = To<Client>::Cast(peer.attachment);
		Logging::WriteEvent(EventClass::Preset::unexpectedBehavior, "Invalid operation performed",&peer,nullptr,client && client->faction ? client->faction : nullptr);
		if (client)
		{
			//clients.FindAndErase(client);
			activePlayersAndSpectators.FindAndErase(client);
			spectators.FindAndErase(client);
			client->isSpectator = false;
			if (client->faction)
			{
				playerMap.Unset(client->faction);
				client->faction->online = false;
				Channels::factionOffline.SendTo(server,client->faction->id,2);
			}

			peer.attachment.reset();
		}
		peer.DisconnectPeer();
	}



	bool CanControl(const Client&c, const Objects::WorldEntity&e)
	{
		if (!c.faction || c.faction->isDead)
		{
			LogEvent(EventClass::Preset::commandRejected,c.peer.lock(),"Unable to comply: Your faction has ended");
			return false;
		}
		if (c.faction != e.owner)
			return false;
		if (c.wantNewRound)
		{
			ASSERT__(c.faction == e.owner);
			LogEvent(EventClass::Preset::commandRejected,&e,"Unable to comply: You have ended your round");
			return false;
		}
		return true;
	}
	bool CanControl(const PClient&c, const Objects::PWorldEntity&e)
	{
		return CanControl(*c,*e);
	}
	bool CanControl(const PClient&c, const Objects::WorldEntity&e)
	{
		return CanControl(*c,e);
	}
	bool PeerCanControl(TCP::Peer&peer, const Objects::WorldEntity&e)
	{
		if (!peer.attachment)
			return false;
		return CanControl(To<Client>::Cast(peer.attachment),e);
	}
	bool PeerCanControl(TCP::Peer&peer, const Objects::PWorldEntity&e)
	{
		return PeerCanControl(peer,*e);
	}


	bool CanSeeAsOwner(const Client&c, const Objects::WorldEntity&e)
	{
		return c.faction == e.owner || c.isSpectator || (e.owner->IsAlliedWith(c.faction) && c.faction->alliance->config.exposeEntityDetails);
	}
	bool CanSeeAsOwner(const PClient&c, const Objects::PWorldEntity&e)
	{
		return CanSeeAsOwner(*c,*e);
	}
	bool CanSeeAsOwner(const PClient&c, const Objects::WorldEntity&e)
	{
		return CanSeeAsOwner(*c,e);
	}


	bool CanSee(const Client&c, const Objects::PWorldEntity&e, float t)
	{
		return c.CanSee(e,t);
		//return /*c.id == e.owner*/ true;	//placeholder
	}
	bool CanSee(const PClient&c, const Objects::PWorldEntity&e, float t)
	{
		return CanSee(*c,e,t);
	}

}

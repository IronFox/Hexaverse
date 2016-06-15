#include <global_root.h>
#include "networkState.h"

#include <protocol.h>
#include <objects.h>
#include <runtimeConfig.h>
#include "../journalSaveGame.h"
#include "../networkChannels.h"
#include "../aiProcess.h"
#include "../serverLogEvent.h"
#include "worldState.h"


#undef StartService



namespace Network
{
	Buffer<PClient>	activePlayersAndSpectators,spectators;
	GenericHashTable<Objects::OwnerParameter,PClient,Objects::OwnerHash>	playerMap;
	GenericHashTable<CryptographicID,Objects::PFaction,CryptographicID>		tokenMap;


	namespace Config
	{
		UINT16		sessionPort = DEFAULT_SERVER_BASE_PORT + PortOffset::Session;
	}


	TUniqueID		serverID;
	TCP::Server		server;


	static void StartServer()
	{
		if (server.IsOnline())
		{
			if (server.GetPort() == Config::sessionPort)
				return;
			server.EndService();
		}
		if (server.StartService(Config::sessionPort))
		{
			LogEvent(EventClass::Preset::network,"Session server started on port "+String(Config::sessionPort));
		}
		else
			FATAL__("Unable to start service in port "+String(Config::sessionPort)+" ("+server.GetError(true)+"). Chances are, a server is already running on that port.")

	}



	void ChangeUserLevel(TCP::Peer&peer, UserLevel newLevel)
	{
		if (peer.userLevel == (UINT32)newLevel)
			return;
		peer.userLevel = (UINT32)newLevel;
		LogEvent(EventSignificance::Debug,peer,"User level changed to "+String(peer.userLevel));
	}

	String						Client::ToString() const
	{
		return (faction ? faction->id.ToString() : (isSpectator ? "[spectator]" : "[no faction]"))+":'"+name+"'";
	}

	bool		Client::CanSee(const Objects::PWorldEntity&e, float t)	const
	{
		return isSpectator || (faction && faction->CanSee(e,t));
	}


	static String FactionStatus(const Objects::PFaction&f)
	{
		if (!f)
			return "Game Over";
		return "Nexus Ports enabled: "+String(f->acquiringNexusControl)+", conquered: "+String(f->nexusControl)+"/"
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


	void Client::SendProgressUpdate(bool dead, const String&warningReason)
	{
		Protocol::GameProgress progress;
		progress.gameOver = dead || RuntimeConfig::phase == RuntimeConfig::Phase::EndOfGame;
		progress.gameWon = Game::winner == faction && !!Game::winner;
		progress.status = FactionStatus(faction);
		progress.warning = dead ? "Game Over" : FactionWarning(faction,warningReason);
		Channels::gameProgress.SendTo(peer.lock(),progress);
	}




	void		Start()
	{
		SetupServer();
		StartServer();
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
		server.InstallChannel(Channels::dropLastTask);
		server.InstallChannel(Channels::dropAllTasks);
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
		server.InstallChannel(Channels::identifyUserRecv);
		server.InstallChannel(Channels::chatMessage);

		server.OpenSignalChannel(Protocol::ChannelID::RequestRoundEnd,(UINT32)UserLevel::CommonFaction);
		server.OpenSignalChannel(Protocol::ChannelID::RequestContinueRound,(UINT32)UserLevel::CommonFaction);
		server.OpenSignalChannel(Protocol::ChannelID::Ping,0);
		server.OpenSignalChannel(Protocol::ChannelID::RequestEverything,(UINT32)UserLevel::Spectator);
		
		server.onSignal = Receiver::OnSignal;
		server.SetAsync(false);
		server.onEvent = Receiver::OnEvent;
		server.onIgnorePackage = Receiver::OnIgnorePackage;
		server.onDeserializationFailed = Receiver::OnDeserializationFailed;
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
			Channels::factionOffline.SendTo(server,client->faction->id,2);
		}
	}



	namespace Session
	{
		bool	isStarted=false;


	}

	void	RestartSession()
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
		StartServer();

		Game::Network::HandleSessionRestart();

		Game::ResetFactionStartMineralValue();

//		Objects::VerifyVisibilityNetwork();
		Objects::CheckMobileEntitySanity();
		Objects::VerifyGrid();

		Session::isStarted = true;

		World::Network::SendChangedFamilies();
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


	bool		CloseSession(bool preserveSpectators)
	{
		bool isOnline = Session::isStarted;
		LogEvent(EventSignificance::Important,isOnline ? "Closing session." : "Flushing session data.");

		if (isOnline)
		{
			Session::isStarted = false;
			if (preserveSpectators)
			{
				Game::Config::factionSupportFlags = Protocol::GameFlag::SupportSpectatorFactions;
				Buffer0<TCP::PPeer>	dump;
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

		if (spectators.IsNotEmpty())
		{
			Objects::ForeachWorldEntityIncludingStored([](const Objects::PWorldEntity&e)
			{
				Protocol::DestroyEntity d;
				d.id = e->id;
				d.type = e->isInsertedIntoGrid ? Objects::RemovalType::Vanished : Objects::RemovalType::DestroyedInStorage;
				d.timestamp.rightNow = true;
				Channels::destroyEntity.SendTo(server,d);
			});

		}

		Objects::FlushAllData();
		Grid::FlushAllData();

		World::Network::SendChangedFamilies();

		return isOnline;

	}




	void InvalidOperation(TCP::Peer&peer)
	{
		PClient client = To<Client>::Cast(peer.attachment);
		Logging::WriteEvent(EventClass::Preset::unexpectedBehavior, "Invalid operation performed",&peer,nullptr);
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
		return c.faction == e.owner || c.isSpectator;
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

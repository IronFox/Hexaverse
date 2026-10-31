#include <grid/resources.h>
#include <io/string_file.h>

#include "network.h"
#include "../version.h"
#include "animation.h"
#include "clientRuntimeConfig.h"
#include "visual/scene/world.h"
#include "cameraControl.h"
#include "visual/hud/hexGrid.h"
#include "visual/tutorial.h"
#include "visual/hud/dialog.h"
#include "visual/hud/menu.h"
#include "visual/hud/guiStatistics.h"
#include "ai/vehicles.h"
#include "clientRuntimeConfig.h"
#include "timelapse.h"
#include <network/steamInterface.h>
#include <network/net.h>
#include "configurationMode.h"
#include "gridControl.h"
#include "visual/objectChoices.h"
#include "visual/scene/terrain.h"
#include "visual/hud/guiFactionTable.h"
#include "clientEvents.h"
#include "visual/hud/guiHostList.h"
#include "visual/hud/eventLog.h"
#include "voting.h"
#include "objectNetworkHelper.h"
#include "playerControlBegin.h"
#include "ai/scriptedAI.h"
#include <expectedBehavior.h>
#include <passwordMatch.h>
#include <objects/nexus/nexusThreatEstimation.h>

#include "animation/worldDelta.h"
#include "animation/mobileState.h"
#include "animation/newMobileEntity.h"
#include "animation/structureState.h"
#include "animation/structureConstruction.h"
#include "animation/destroyEntity.h"
#include "animation/mobileEntityMotion.h"
#include "animation/trajectory.h"
#include "animation/explosion.h"
#include "animation/turretRotation.h"
#include "animation/resourceUpdate.h"
#include "animation/operationLacksResources.h"

using namespace Time::Literals;

namespace Network
{
	struct SteamServer
	{
		ServerAddress address;
		UINT64 ownerID,lobbyID;
	};

	Buffer0<SteamServer>
					steamServers;

	bool			haveFactionLoaded = false,
					haveFactionOnServer = false,
					haveID = false,
					amSpectator = false,
					amOnline=false,
					gameHasBegun=false;
	#ifdef HAS_WINDOW
		bool		amLocated=false;
		UINT64		attemptingToConnect = 0;
	#endif

	TUniqueID		myID,worldID;
	UINT32				gameFlags = 0;
	UINT16			connectionBasePort = DEFAULT_SERVER_BASE_PORT;

	CryptographicID	passwordChallenge;
	String			myIDString,worldIDString;
	INT32			uplinkBandwidth = -1;
	ServerAddress	lastServerAddress;
	UINT64			lastLobbyID=0;
	UINT64			autoConnectToLobby=0;

	
	bool			serverRequiresPassword = false;

	Protocol::GameProgress	gameProgress;
	TUniqueID				winningAllianceID = TUniqueID::Empty();


	float			animationTimeFactor = 1,
					roundTimer = 0;
	Time::Stamp		phaseStarted;
	bool			newRoundRequested = false;
	UINT64			connectingToSteam = 0;


	HashSet<TUniqueID>	visitedWorldMap;
	bool CanSendFactionRequests()
	{
		return		Network::amOnline
					&&
					(	RuntimeConfig::IsInAnyCommandPhase() 
						|| RuntimeConfig::phase  == RuntimeConfig::Phase::AnimationPhase 
						|| RuntimeConfig::phase  == RuntimeConfig::Phase::EndOfGame
					)
				;
	}
	

	bool IsValidNickname(const String&candidate)
	{
		if (candidate.GetLength() < 4)
			return false;
		bool rs = Package::IsValid(candidate,16);
		return rs
				&& !candidate.Contains("  ") && !candidate.Contains('\t') && !candidate.Contains('\n') && !candidate.Contains('\r') && !candidate.EndsWith(' ') && !candidate.BeginsWith(' ');
	}

	bool	IsValidNickname(const UTF32StringRef&candidate)
	{
		if (candidate.GetLength() < 4)
			return false;
		bool rs = Package::IsValid(candidate,32);
		return rs;
	}

	void	SetSteamPersona()
	{
		String name = SteamInterface::GetUserNickName();
		if (IsValidNickname(name))
		{
			Network::chosenNickname = name;
			Network::customNickname = false;
		}
		else
			throw Except::Program::DataConsistencyFault("Steam user name is not valid for " PRODUCT_NAME". You must manually choose a nickname");;

	}

	void	ResetNickname()
	{
		try
		{
			SetSteamPersona();
			FileSystem::UnlinkFile(RuntimeConfig::myConfig.Lookup("userName.txt"));
			LogEvent(EventClass::Preset::uncommon,"Nickname reset to Steam alias '"+Network::chosenNickname+"'");
			if (Network::client.IsConnected())
				Network::Channels::changeMyName.SendTo(Network::client,Network::chosenNickname);
		}
		catch (const std::exception&ex)
		{
			LogEvent(EventClass::Preset::error,ex.what());
		}
	}

	void ConnectToLobby(UINT64 lobbyID)
	{
		if (lobbyID == Network::lastLobbyID && Network::client.IsConnected())
		{
			LogEvent(EventSignificance::Uncommon,"Already connected to lobby. Ignoring event");
			return;
		}
		if (Network::ForcingTCPConnections())
		{
			LogEvent(EventSignificance::NonFatalError,"Cannot connect to lobby; TCP connections are enforced");
			return;
		}
		Network::client.Disconnect();
		SteamInterface::RegisterExternalLobby(lobbyID);
		Network::RefreshServerListNow();
		Network::autoConnectToLobby = lobbyID;
		#ifdef HAS_WINDOW
			attemptingToConnect = lobbyID;
			HUD::Dialog::PermanentMessage("Attempting to connect. Please wait.",[lobbyID]()
			{
				return !Network::client.IsConnected() && SteamInterface::LobbyIsRegistered(lobbyID) ? HUD::Dialog::CheckResult::OkayContinue : HUD::Dialog::CheckResult::Abort;
			});
		#endif
	
	}


	void RegisterVisited(const TUniqueID&worldID)
	{
		if (visitedWorldMap.SetNew(worldID))
			SaveHosts();
	}

	bool DidVisit(const TUniqueID&worldID)
	{
		return visitedWorldMap.IsSet(worldID);
	}
	
	void LoadHosts()
	{
		try
		{
			StringFile visitedWorlds;
			visitedWorlds.Open(RuntimeConfig::myConfig.Lookup("visitedWorlds.txt"));
			String visited;
			while (visitedWorlds >> visited)
			{
				TUniqueID id;
				if (id.Decode(visited))
					visitedWorldMap.Set(id);
			}
		}
		catch (...)
		{}

		try
		{
			StringFile hosts;
			hosts.Open(RuntimeConfig::myConfig.Lookup("hosts.txt"));
			String host;
			while (hosts >> host)
				Network::ServerDiscovery::RegisterNewHost(0, SteamInterface::GetUserID(), Network::ServerAddress(host,0, Protocol::ServerInfo()),false, false);
		}
		catch (...)
		{}
	}

	void	SaveHosts()
	{
		try
		{
			StringFile visited;
			visited.Create(RuntimeConfig::myConfig.Lookup("visitedWorlds.txt"));
			static Array<TUniqueID>	 worlds;
			visitedWorldMap.ExportKeys(worlds);
			foreach (worlds,world)
				visited << world->ToFullString() << nl;
			visited.Close();
		}
		catch (const std::exception&ex)
		{
			LogException(nullptr,ex);
		}
		try
		{
			StringFile hosts;
			hosts.Create(RuntimeConfig::myConfig.Lookup("hosts.txt"));
			static Buffer0<Network::ServerIPAddress>	 urls;
			ServerDiscovery::ListNonAutoAttempts(urls);
			foreach (urls,url)
				hosts << url->host << ':' << url->basePort << nl;
			hosts.Close();
		}
		catch (const std::exception&ex)
		{
			LogException(nullptr,ex);
		}
	}

	void		SendChatMessage(const String&msg, const Objects::PFaction&receiver)
	{
		if (!receiver)
			return;
		if (msg.TrimRef().GetLength() > 0)
		{
			if (client.IsConnected())
				Channels::ChatMessage.SendTo(client,Protocol::ChatMessage(msg,receiver->id));
		}
	}

	void		SendChatMessage(const String&msg, const Objects::PAlliance&receiver)
	{
		if (!receiver)
			return;
		if (msg.TrimRef().GetLength() > 0)
		{
			if (client.IsConnected())
				Channels::ChatMessage.SendTo(client,Protocol::ChatMessage(msg,receiver->id));
		}
	}

	void		BroadcastChatMessage(const String&msg)
	{
		if (msg.TrimRef().GetLength() > 0)
		{
			if (client.IsConnected())
				Channels::ChatMessage.SendTo(client,Protocol::ChatMessage(msg,TUniqueID::Empty()));
		}
	}



	void			LoadGameConfig(const Protocol::GameConfig&cfg)
	{
		gameFlags = cfg.gameFlags;
		RuntimeConfig::gameGoal = RuntimeConfig::GameGoal::Reinterpret(cfg.gameGoal);
	}


	void			ServerInfo::Connect() const
	{
		client.Disconnect();

		if (AI::Script::IsRunning())
		{
			LogEvent(EventClass::Preset::scriptError,"Cannot connect right now. Scripted AI still busy");
			#ifdef HAS_WINDOW
				HUD::Dialog::ShowMessage("Unable to connect to the requested server right now. Your scripted AI is still busy. Please wait until it completes.");
			#endif
			return;
		}
		const auto&addr = GetPrimaryAddress();
		if (addr.host.IsEmpty() &&!addr.steamID)
		{
			
			return;
		}
		autoConnectToLobby = 0;
		lastServerAddress = addr;
		lastLobbyID = lobbyID;


		RuntimeConfig::tutorialMode = (GetPrimaryAddress().info.gameInfo.gameConfig.gameFlags & Protocol::GameFlag::TutorialMode) != 0;

		RuntimeConfig::phase = RuntimeConfig::Phase::Uninitialized;
		GlobalEvents::phaseChange();
		gameHasBegun = false;

		Network::worldID = addr.info.gameInfo.worldID;
		Network::worldIDString = Network::worldID.ToNiceString();
		Network::gameFlags = addr.info.gameInfo.gameConfig.gameFlags;
		RuntimeConfig::gameGoal = RuntimeConfig::GameGoal::Reinterpret(addr.info.gameInfo.gameConfig.gameGoal);

		if (addr.steamID  && !ForcingTCPConnections())
		{
			client.ConnectToSteam(addr.steamID);
			connectingToSteam = addr.steamID;
		}
		else
		{
			client.ConnectAsync(addr.host + ":"+String(addr.basePort + PortOffset::Session));
			connectingToSteam = 0;
		}
		//if (lobbyID != 0)
		//{
		//	SteamInterface::JoinLobby(lobbyID);
		//}
		#ifdef HAS_WINDOW
			HUD::WindowStack::SignalSituationChange();
		#endif

		worldID = addr.info.gameInfo.worldID;
		worldIDString = worldID.ToNiceString();

		connectionBasePort = addr.basePort;
	}




	Objects::OwnerParameter ResolveFaction(const TUniqueID&id, const TUniqueID&allianceID)
	{
		bool allianceChanged = false;
		auto rs = Objects::Faction::GetOrCreate(id,allianceID, allianceChanged);
		if (allianceChanged)
			GlobalEvents::onAllianceChange(rs);
		return rs;
	}

	Objects::OwnerParameter ResolveFaction(const TUniqueID&id)
	{
		return Objects::Faction::GetOrCreate(id);
	}


	static PathString AccessFilename()
	{
		return RuntimeConfig::myConfig.Lookup("worldAccess.m32");
	}

	namespace ServerDiscovery
	{


		void OnServerInfo(Protocol::ServerInfo&info, TCP::Peer&peer);
		void OnPing(Protocol::Ping&ping, TCP::Peer&peer);

		TCP::Channel<Protocol::ServerInfo,0,Protocol::InfoChannel::ServerInfo>	serverInfoChannel(OnServerInfo);
		TCP::Channel<Protocol::Ping,0,Protocol::InfoChannel::Ping>	pingChannel(OnPing);

		class HostContactAttempt : public Sys::ThreadObject, public TCP::Peer::Attachment, public std::enable_shared_from_this<HostContactAttempt>
		{
		public:
			TCP::Client		client;
			ServerIPAddress	address;
			bool			hasContact = false,
							hasInfo = false,
							rightVersion = false,
							persistent = false,
							autoAttemptHideIfFailing = false,
							protocolIncompatible = false;
			Protocol::ServerInfo	lastInfo;
			float			lastPing;
			volatile bool	doQuit = false;
			//Semaphore		sem;

			/**/			HostContactAttempt():/*sem(0),*/lastPing(0)	{}


			static void		OnEvent(TCP::event_t ev, TCP::Peer&peer);

			static void		OnDeserializationFailed(UINT32, TCP::Peer&peer, const String&exception);

			void			ThreadMain() override
			{
				client.SetAsync(true);
				client.onEvent = OnEvent;
				client.attachment = shared_from_this();
				client.InstallChannel(serverInfoChannel);
				client.InstallChannel(pingChannel);
				client.onDeserializationFailed = OnDeserializationFailed;

				while (!application_shutting_down && !doQuit)
				{
					hasContact = false;
					lastPing = 0;
					hasInfo = false;
					while (!application_shutting_down && !doQuit)
					{
						if (client.Connect(address.host + ":"+String(address.basePort + PortOffset::Info)))
						{
							break;
						}
						Sleep(1000);
					}
					if (doQuit)
						return;
					hasContact = true;
					while (!doQuit && !application_shutting_down && client.IsConnected())
					{
						pingChannel.SendTo(client,Time::Stamp().ToTicks());
						Sleep(200);
						if (client.GetLastPackageTime().ElapsedSeconds() > 5.f)
							client.Disconnect();
					}
					//if (!application_shutting_down)
						//sem.enter();
				}
			}

			//void			Release()
			//{
			//	sem.release();
			//}


		};
		typedef std::shared_ptr<HostContactAttempt>	PHostContactAttempt;

		Buffer0<PHostContactAttempt>	attempts,waste;

		void	ListFailedAttempts( Buffer0<ServerIPAddress,Swap>&outList )
		{
			outList.Clear();
			foreach (attempts,a)
			{
				if (!(*a)->autoAttemptHideIfFailing && !(*a)->hasContact)
					outList << (*a)->address;
			}

		}

		void	ListNonAutoAttempts(Buffer0<ServerIPAddress>&outList)
		{
			outList.Clear();
			foreach (attempts,a)
			{
				if (!(*a)->autoAttemptHideIfFailing)
					outList << (*a)->address;
			}
		}


		bool	TryRemoveHost( const ServerIPAddress&addr )
		{
			if (client.IsAttemptingToConnect() || client.IsConnected())
				return false;
			foreach (attempts,a)
			{
				if ((*a)->address == addr)
				{
					if ((*a)->persistent)
						(*a)->autoAttemptHideIfFailing = true;
					else
					{
						(*a)->doQuit = true;
						waste << *a;
						attempts.erase(a);
					}
					#ifdef HAS_WINDOW
						HUD::WindowStack::SignalSituationChange();
					#endif
					return true;
				}
			}
			return false;
		}


		bool	ListeningButCantConnectTo(const ServerIPAddress&addr)
		{
			if (client.IsAttemptingToConnect() || client.IsConnected())
				return false;
			foreach (attempts,a)
			{
				if ((*a)->address == addr)
				{
					return !(*a)->hasContact || !(*a)->rightVersion || (*a)->protocolIncompatible;
				}

			}
			return false;

		}

		void OnServerInfo(Protocol::ServerInfo&info, TCP::Peer&peer)
		{
			PHostContactAttempt me = To<HostContactAttempt>::Cast(peer.attachment);
			me->lastInfo = info;
			info.hostInfo.version[ARRAYSIZE(info.hostInfo.version)-1] = 0;
			me->hasInfo = true;
			me->rightVersion = !strcmp(info.hostInfo.version, SERVER_NAME);
		}


		/*static*/ void		HostContactAttempt::OnDeserializationFailed(UINT32, TCP::Peer&peer, const String&exception)
		{
			PHostContactAttempt me = To<HostContactAttempt>::Cast(peer.attachment);
			if (me)
				me->protocolIncompatible = true;
		}


		void OnPing(Protocol::Ping&ping, TCP::Peer&peer)
		{
			PHostContactAttempt me = To<HostContactAttempt>::Cast(peer.attachment);
			me->lastPing = Time::Stamp::FromTicks(ping).ElapsedSeconds();
		}


		/*static*/ void		HostContactAttempt::OnEvent(TCP::event_t ev, TCP::Peer&peer)
		{
			PHostContactAttempt me = To<HostContactAttempt>::Cast(peer.attachment);
			switch (ev)
			{
				case TCP::Event::ConnectionLost:
					//me->Release();
					if (me)
						me->protocolIncompatible = false;	//can't know for next connection
				break;
			}

		}

		void MapNonSteamServer(const ServerIPAddress&addr, const Protocol::ServerInfo&info, bool protocolIncompatible, bool rightVersion, float lastPing)
		{
			index_t at = InvalidIndex;
			ServerAddress addr2(addr,info);
			foreach (servers,serv)
			{
				if (serv->HasAddress(addr2))
				{
					at = serv - servers.pointer();
					serv->ownerID = info.hostInfo.ownerID;
					serv->lobbyID = info.hostInfo.lobbyID;
					break;
				}
				if (serv->ownerID == info.hostInfo.ownerID 
					&& !serv->HasNonSteamAddress() 
					&& serv->IsWorldID(info.gameInfo.worldID))
				{
					at = serv - servers.pointer();
					serv->addresses << addr2;
					break;
				}					
			}			
			if (at == InvalidIndex)
			{
				at = servers.Count();
				servers.Append().addresses << addr2;
				servers.Last().ownerID = info.hostInfo.ownerID;
				servers.Last().lobbyID = info.hostInfo.lobbyID;
				servers.Last().protocolIncompatible = protocolIncompatible;
				servers.Last().rightVersion = rightVersion && !protocolIncompatible;
				if (info.hostInfo.lobbyID)
					SteamInterface::RegisterExternalLobby(info.hostInfo.lobbyID);
			}
			else
				servers[at].UpdateNonSteamAddress(addr2);
			servers[at].lastPing = lastPing;
			//servers[at].info = info;
		}

		void MapSteamServer(const SteamServer&s)
		{
			/*
			Removed verification because:
			Lobby has only own owner (guaranteed).
			Owner sets game server id (guaranteed).
			Therefor lobby owner -> game server id.
			Lobby owner could register a bad server:
				* Doesn't exist, but timeouts are somehow updated: Connection always fails
				* Owned by someone else, meaning all lobby data is wrong. Corrected during ClientFactionResponse (after logging in/creating faction)
					=> Could create many lobbies pointing to some server thus overloading it, however, verification would do the same
			Assumed low probability scenario. If ever wrong: reactivate the following, OR compare lobby data with server data, send actual data upon first connect
			*/
			#ifdef USE_STEAM_OWNER_AUTHENTICATION
				auto authenticationState = SteamNetwork::GetAuthenticationState(s.address.steamID,s.ownerID);
				if (authenticationState == SteamNetwork::AuthenticationState::AuthenticationFailed)
					return;
				bool authenticated = authenticationState == SteamNetwork::AuthenticationState::Authenticated;
			#endif
			
			
			index_t at = InvalidIndex;
			foreach (servers,serv)
			{
				if (serv->HasAddress(s.address))
				{
					at = serv - servers.pointer();
					serv->ownerID = s.ownerID;
					serv->lobbyID = s.lobbyID;
					break;
				}
				if (serv->ownerID == s.ownerID 
					&& !serv->HasSteamAddress() 
					&& serv->IsWorldID(s.address.info.gameInfo.worldID))
				{
					at = serv - servers.pointer();
					serv->addresses << s.address;
					break;
				}					
			}


			if (at == InvalidIndex)
			{
				at = servers.Count();
				servers.Append().addresses << s.address;
				servers.Last().protocolIncompatible = false;
				servers.Last().rightVersion = true;
				servers.Last().ownerID = s.ownerID;
				servers.Last().lobbyID = s.lobbyID;
			}
			else
				servers[at].UpdateSteamAddress(s.address);
			#ifdef USE_STEAM_OWNER_AUTHENTICATION
				servers[at].authenticated |= authenticated;
			#endif
		}


		bool	RegisterNewHost( UINT64 lobbyID, UINT64 ownerID,const ServerAddress&host, bool probablyRightVersion, bool automaticallyDiscovered, bool makePersistent )
		{
			foreach (attempts,a)
				if ((*a)->address == host)
				{
					if (!automaticallyDiscovered)
						(*a)->autoAttemptHideIfFailing = false;
					if (makePersistent)
						(*a)->persistent = true;
					return false;	//already there
				}
			if (host.steamID)
			{
				
				index_t at=InvalidIndex;

				for (index_t i = 0; i < steamServers.Count(); i++)
				{
					if (steamServers[i].address == host)
					{
						at = i;
						break;
					}
				}
				if (at == InvalidIndex)
				{
					at = steamServers.Count();
					steamServers.Append().address = host;
					steamServers.Last().ownerID = ownerID;
					steamServers.Last().lobbyID = lobbyID;
				}
				steamServers[at].address = host;	//might have new info

				MapSteamServer(steamServers[at]);
			}
			else
			{
				PHostContactAttempt&attempt = attempts.Append();
				attempt.reset(new HostContactAttempt());
				attempt->address = host;
				attempt->rightVersion = probablyRightVersion;
				attempt->autoAttemptHideIfFailing = automaticallyDiscovered;
				attempt->persistent = makePersistent;
			
				attempt->Start();
			}
			return true;
		}


	}


	static void ApplyPlanetConfig(const Grid::TSharedPlanetConfig&config)
	{
		ClientGrid::Apply(config);
	}


	static void		FlushFaction(bool alsoFlushWorld)
	{
		#ifdef HAS_WINDOW
			TimeLapse::enabled = false;
			Camera::locked = false;
		#endif

		//dbgRoundNumber = 0xFFFFFFFF;
		haveFactionLoaded = false;

		roundTimer = -1;

		GlobalEvents::FlushObjects();
		Network::winningAllianceID = TUniqueID::Empty();
		Network::gameProgress.gameOver = false;
		Network::gameProgress.gameWon = false;
		Network::gameProgress.status = "";
		Network::gameProgress.warning = "";
		Network::newRoundRequested = false;
		Objects::Faction::idMap.Clear();
		Objects::Faction::allKnown.Clear();
		Objects::Faction::mine.reset();

		Animation::Flush();

		if (alsoFlushWorld)
		{
			GlobalEvents::terrainFlush();
			RuntimeConfig::gameGoal = RuntimeConfig::GameGoal();
			ApplyPlanetConfig(Grid::TSharedPlanetConfig());
		}

		#ifdef HAS_WINDOW
			HUD::HexGrid::EventLog::Clear();
			HUD::HexGrid::RefreshContacts();

			amLocated = false;
			if (alsoFlushWorld)
				Camera::Reset();
		#endif
	}

	#ifdef HAS_WINDOW
		void QueryPasswordAndLogin(Protocol::FactionOperation::op_t op, UINT32 factionTypes)
		{
			HUD::Dialog::QueryPassword("Please provide the configured server password",
			[factionTypes,op](const UTF32StringRef&input)
			{
				StringEncoding::UTF32::ToUTF8(input, RuntimeConfig::serverPassword);
				AttemptFactionLogin(op,factionTypes,RuntimeConfig::serverPassword);
			},[]()->HUD::Dialog::CheckResult
			{
				if (!client.IsConnected())
					return HUD::Dialog::CheckResult::Abort;
				if (!RuntimeConfig::IsInAnyCommandPhase())
					return HUD::Dialog::CheckResult::OkayNotRightNow;
				return HUD::Dialog::CheckResult::OkayContinue;
			}
			);
		}


		void AttemptFactionLogin(Protocol::FactionOperation::op_t op, UINT32 factionTypes)
		{
			if (serverRequiresPassword && RuntimeConfig::serverPassword.IsEmpty())
				QueryPasswordAndLogin(op,factionTypes);
			else
				AttemptFactionLogin(op,factionTypes,RuntimeConfig::serverPassword);
		}

	#else
		void AttemptFactionLogin(Protocol::FactionOperation::op_t op, UINT32 factionTypes)
		{
			AttemptFactionLogin(op,factionTypes,RuntimeConfig::serverPassword);
		}
	#endif


	namespace Receiver
	{
		bool			eventsLocked = true;
		 

		void	GameConfig(Protocol::GameConfig&cfg)
		{
			Network::LoadGameConfig(cfg);
			LogEvent(EventClass::Preset::important,"Game config updated");
		}
		void	SendHello(UINT64 userID, const ArrayRef<BYTE>&key)
		{
			Protocol::Hello hello;
			hello.clientType = RuntimeConfig::aiMode ? Protocol::ClientType::AIClient : Protocol::ClientType::RegularClient;
			hello.steamID = userID;
			hello.ticket = key;
		/*	#ifdef HAS_WINDOW
				hello.steamID = SteamInterface::GenerateNewAccessToken(hello.ticket);
			#else
				hello.steamID = 0;
				RuntimeConfig::aiAccessToken.Export(hello.ticket);
			#endif
				*/
			//if (RuntimeConfig::aiMode || customNickname)
			hello.nickname = RuntimeConfig::aiMode ? Objects::Faction::nexusName : chosenNickname;
			if (RuntimeConfig::aiMode)
				chosenNickname = Objects::Faction::nexusName;
			//state.state = 0U;
			LogEvent(EventSignificance::Common,"Sending hello to server ("+String(hello.clientType)+","+String(userID)+",'"+hello.nickname+"')");
			Channels::hello.SendTo(client,hello);	//elevate client state
			

			//#ifdef HAS_WINDOW
			//	Dialog::Show(Dialog::loginMenu);
			//	GUI::HostList::CheckVisibility(false);
			//#endif

		}

		void	OnServerHello(Protocol::ServerHello&hello)
		{
			if (hello.version != SERVER_NAME)
			{
				LogEvent(EventClass::Preset::error,"Server version mismatch (expected "+String(SERVER_NAME)+", but got "+hello.version+")");
				client.Disconnect();
				return;
			}

			if (RuntimeConfig::aiMode)
			{
				Array<BYTE> key;
				RuntimeConfig::aiAccessToken.Export(key);
				SendHello(0,key);
				return;
			}

			if (hello.useCryptographicID)
			{
				SteamInterface::RequestNewCryptographicAccessToken([](UINT64 steamID, const ArrayRef<BYTE>&key,const String&error)
				{
					if (steamID == 0)
					{
						LogEvent(EventClass::Preset::error,"Unable to generate access token ("+error+")");
						client.Disconnect();
						return;
					}
					SendHello(steamID,key);
				});
			}
			else
			{
				Array<BYTE> key;
				UINT64 steamID = SteamInterface::GenerateNewAccessToken(key);
				if (steamID)
				{
					SendHello(steamID,key);

				}
				else
				{
					LogEvent(EventClass::Preset::error,"Unable to generate access token");
					client.Disconnect();
					return;
				}
			}
		}

		void	OnHelloResponse(Protocol::HelloResponse&response)
		{
			if (response.success)
			{
				LogEvent(EventClass::Preset::common, nullptr, "Status confirmed ('"+response.errorMessage+"')");
				passwordChallenge = response.passwordChallenge;
				if (passwordChallenge.IsEmpty())
				{
					LogEvent(EventClass::Preset::networkError,"Server password challenge is empty. Disconnecting.");
					client.Disconnect();
					return;
				}
				const bool gameEndSpectatorMode = (response.gameFlags & Protocol::GameFlag::GameHasEnded) != 0;
				serverRequiresPassword = (response.gameFlags & Protocol::GameFlag::RequireServerPassword) != 0;
				haveFactionOnServer = response.hasFaction;

				if (RuntimeConfig::aiMode)
					AttemptFactionLogin(Protocol::FactionOperation::ForceContinue,Protocol::FactionType::NexusFaction);
				elif (gameEndSpectatorMode)
					AttemptFactionLogin(Protocol::FactionOperation::Spectate,Protocol::FactionType::RegularFaction);
				elif (RuntimeConfig::tutorialMode)
					AttemptFactionLogin(Protocol::FactionOperation::ForceContinue,Protocol::FactionType::RegularFaction);
				else
				{
					#ifdef HAS_WINDOW
						HUD::WindowStack::Show(HUD::Menu::login);
					#endif
				}
				SteamInterface::AdvertiseLobby(lastLobbyID);
				#ifdef HAS_WINDOW
					HUD::HostList::CheckVisibility(false);
				#endif
			}
			else
			{
				LogEvent(EventClass::Preset::networkError,"Server rejected hello ('"+response.errorMessage+"')");
				client.Disconnect();
			}
		}

		void	OnWorldDelta(Protocol::WorldDelta&delta)
		{
			ASSERT__(!eventsLocked);

			auto existing = To<Animation::WorldDelta>::Cast(Animation::FindFirstScheduledEventAtTime(delta.timestamp,[](const auto&ev){return ev->IsWorldDelta();}));
			if (existing)
			{
				existing->Append(std::move(delta));
				return;
			}
			Animation::SmartSchedule<Animation::WorldDelta>(delta.timestamp,delta);
		}




		void OnMobileState(Protocol::MobileState&vs)
		{
			ASSERT__(!eventsLocked);
			#ifdef HAS_WINDOW
				DBG_ASSERT__(!ConfigurationMode::IsActive());
			#endif

			EXPECT2(vs.entityState.health>0,vs.entityState.health,vs.entityState.id);

			#ifdef _DEBUG
				if (!vs.entityState.timestamp.rightNow)
				{
					auto existing = Animation::FindFirstScheduledEventAtTime(vs.entityState.timestamp,[vs](const auto&ev){return ev->targetEntityID == vs.entityState.id && ev->IsMobileState();});
					if (existing)
					{
						auto entity = Objects::GetEntity(existing->targetEntityID);
						if (entity)
							LogEvent(EventSignificance::NonFatalError, entity.get(), "Got multiple mobile entity updates on this entity at time " + ToString(vs.entityState.timestamp));
						else
							LogEvent(EventSignificance::NonFatalError,"Got multiple mobile entity updates on entity "+ToString(existing->targetEntityID)+", at time "+ToString(vs.entityState.timestamp));
						FAIL_EXPECT("Multiple mobile entity updates detected");
					}
				}
			#endif
			Animation::SmartSchedule<Animation::MobileState>(vs.entityState.timestamp, vs);
		}


		void	OnNewMobileEntity(Protocol::NewMobileEntity&vc)
		{
			ASSERT__(!eventsLocked);
			ASSERT__(Network::haveID || Network::amSpectator);
			#ifdef HAS_WINDOW
				DBG_ASSERT__(!ConfigurationMode::IsActive());
			#endif
			Animation::SmartSchedule<Animation::NewMobileEntity>(vc.state.entityState.timestamp, vc);
		}

		

		void	OnNewStructure(Protocol::StructureConstruction&sc)
		{
			ASSERT__(!eventsLocked);
			#ifdef HAS_WINDOW
				DBG_ASSERT__(!ConfigurationMode::IsActive());
			#endif
			Animation::SmartSchedule<Animation::StructureConstruction>(sc.state.entityState.timestamp, sc);
		}

		void OnStructureState(Protocol::StructureState&vs)
		{
			ASSERT__(!eventsLocked);
			#ifdef HAS_WINDOW
				DBG_ASSERT__(!ConfigurationMode::IsActive());
			#endif
			Animation::SmartSchedule<Animation::StructureState>(vs.entityState.timestamp,vs);
		}

		void DestroyEntity(Protocol::DestroyEntity&de)
		{
			ASSERT__(!eventsLocked);
			#ifdef HAS_WINDOW
				DBG_ASSERT__(!ConfigurationMode::IsActive());
			#endif
			Animation::SmartSchedule<Animation::DestroyEntity>(de.timestamp,de);
		}

		void ControlMessage(Protocol::ControlMessage&msg)
		{

			Objects::OwnerParameter f;
			Objects::PWorldEntity	e;
			if (msg.focus.IsNotEmpty())
			{
				f = Objects::Faction::FindByID(msg.focus);
				if (!f)
				{
					e = Objects::GetEntity(msg.focus);
				}
			}

			Logging::Origin origin = msg.isPrivate ? Logging::Origin::ServerPrivate : Logging::Origin::ServerPublic;

			auto cls = EventClass( EventSignificance::Reinterpret(msg.significance),  EventFlavor::Reinterpret(msg.flavor));
			if (f)
				LogEvent(cls ,f,msg.message,origin);
			else
				LogEvent(cls ,e.get(),msg.message,origin);

			if (cls.flavor == EventFlavor::ChatMessage)
			{
				#ifdef HAS_WINDOW
					HUD::FactionTable::SignalChatMessage(f,msg.message);
				#endif
				AI::Script::SignalChatMessage(f,msg.message);
			}

		}




		void	ClientFactionResponse(Protocol::ClientFactionResponse&cr)
		{
			ASSERT__(!eventsLocked);

			ApplyPlanetConfig(cr.planetConfig);

			myID = TUniqueID::Empty();
			Objects::Faction::mine.reset();
			myIDString = ToString(myID);

			Network::gameFlags = cr.info.gameConfig.gameFlags;

			RuntimeConfig::nexusBases = (Network::gameFlags & Protocol::GameFlag::NexusHasBases) != 0;
			RuntimeConfig::nexusPortsOnAllPoles = (Network::gameFlags & Protocol::GameFlag::NexusPortsOnAllPoles) != 0;
			RuntimeConfig::empZones = (Network::gameFlags & Protocol::GameFlag::EMPZones) != 0;
			RuntimeConfig::gameGoal = RuntimeConfig::GameGoal::Reinterpret(cr.info.gameConfig.gameGoal);
			RuntimeConfig::phase = RuntimeConfig::Phase::Reinterpret(cr.info.activePhase);
			RuntimeConfig::tutorialMode = (Network::gameFlags & Protocol::GameFlag::TutorialMode) != 0;
			#ifdef HAS_WINDOW
				ConfigurationMode::SetGameConfig(Network::gameFlags, RuntimeConfig::gameGoal, Grid::WorldDiscovery::Reinterpret( cr.info.gameConfig.worldDiscovery ));
				World::allowRecompilation = false;
				ConfigurationMode::SetActive(false);
			#endif
			gameHasBegun = false;

			haveFactionLoaded = cr.startFactionTypes != 0;
			if (haveFactionLoaded)
			{
				RegisterVisited(worldID);
				haveFactionOnServer = true;
				amSpectator = cr.operation == Protocol::FactionOperation::Spectate;
				#ifdef HAS_WINDOW
					Objects::resourceView = !amSpectator;
					HUD::HexGrid::RefreshButtonState();
				#endif


				#ifdef HAS_WINDOW
					HUD::WindowStack::Hide(HUD::Menu::login);
					//if (ConfigurationMode::IsActive())
					//	HUD::HexGrid::Configure
					//	HUD::WindowStack::Show(HUD::Dialog::configMenu);
				#endif
			}
			else
			{
				amSpectator = cr.operation == Protocol::FactionOperation::Spectate;
				if (amSpectator)
				{
					RegisterVisited(worldID);
					#ifdef HAS_WINDOW
						Objects::resourceView = false;
						HUD::HexGrid::RefreshButtonState();
						HUD::WindowStack::Hide(HUD::Menu::login);
					#endif
				}
				else
				{
					LogEvent(EventClass::Preset::uncommon, nullptr, "Server response: Faction void.");
					FlushFaction(true);
					#ifdef HAS_WINDOW
						HUD::WindowStack::Show(HUD::Menu::login);
					#endif
				}
			}

			if (!amSpectator)
				Network::Channels::changeMyName.SendTo(Network::client,chosenNickname);

			#ifdef HAS_WINDOW
				if (amSpectator)
				{
					World::actual.FlushAndValidateSensory(1);

				}
			#endif
		}


		void	PhaseChange(Protocol::PhaseChange&c);

		void	MobileEntityMotion(Protocol::MobileEntityMotion&m)
		{
			ASSERT__(!eventsLocked);

			if (Animation::AlterScheduledEvent(
				[&m](const auto&ev, const auto&t)
				{
					if (ev->targetEntityID != m.id 
						|| !ev->IsMobileEntityMotion() 
						|| !To<Animation::MobileEntityMotion>::Cast(ev)->EndsAt(m.fromLocation))
						return false;

					auto vanish = Animation::FindScheduledEvent(
					[&m, ev,t](const auto&other, const auto&tOther)
					{
						return other->targetEntityID == m.id
							&& tOther.Intersects(M::Range(t.max,m.time.begin.atTime))
							&& other->IsEntityDestruction();
					});
					return !vanish;
				},
				[&m](const auto&ev, auto&t){To<Animation::MobileEntityMotion>::Cast(ev)->Append(m,t);}
				))
				return;

			Animation::SmartSchedule<Animation::MobileEntityMotion>(m.time, m);
		}

		void	VagueAttackTrajectory(Protocol::VagueAttackTrajectory&a)
		{
			ASSERT__(!eventsLocked);
			Animation::SmartSchedule<Animation::Trajectory>(a.time, a);
		}

		void	PreciseAttackTrajectory(Protocol::PreciseAttackTrajectory&a)
		{
			ASSERT__(!eventsLocked);
			Animation::SmartSchedule<Animation::Trajectory>(a.time, a);
		}

		void	Explosion(Protocol::Explosion&ex)
		{
			ASSERT__(!eventsLocked);
			Animation::SmartSchedule<Animation::Explosion>(Protocol::TimeWindow(ex.timestamp,Animation::Explosion::ExplosionTime), ex);
		}

		void	EntityTurretRotation(Protocol::EntityTurretRotation&rot)
		{
			ASSERT__(!eventsLocked);
			Animation::SmartSchedule<Animation::TurretRotation>(rot.time, rot);
		}


		void	ResourceUpdate(Protocol::ResourceUpdate&u)
		{
			ASSERT__(!eventsLocked);
			Animation::SmartSchedule<Animation::ResourceUpdate>(u.timestamp, u);
		}

		void	GameProgress(Protocol::GameProgress&progress)
		{
			Network::gameProgress.swap(progress);
		}

		void	CreateMarker(Protocol::Marker&marker)
		{
			Objects::MarkerType type;
			if (!type.Load(marker.markerType))
				throw Except::Program::DataConsistencyFault(CLOCATION,"Unable to load parse marker type #"+String(marker.markerType));
			
			Objects::MarkerStore::client.Create(type,marker.state,false);
			GlobalEvents::invalidateObjectCache();
		}

		void	MarkerCreationFailedLimitReached(Protocol::Marker&marker);	//must re-send, must know channels, cannot be implemented here

		void	DestroyMarker(NET(TUniqueID)&mid)
		{
			if (Objects::MarkerStore::client.Destroy(mid))
				GlobalEvents::invalidateObjectCache();
		}

		void	IdentifyFaction(Protocol::IdentifyFaction&id);
		void	FactionOnline(NET(TUniqueID)&);
		void	FactionOffline(NET(TUniqueID)&);
		void	FactionRequestedNewRound(NET(TUniqueID)&);
		void	FactionContinuedNewRound(NET(TUniqueID)&);
		void	LockEmpZones(NET(UINT16)&);
		void	UplinkBandwidth(NET(INT32)&v)
		{
			uplinkBandwidth = v;
			RuntimeConfig::hibernationSystemEnabled = v > 0;
			GlobalEvents::invalidateObjectCache();
		}

		void	TutorialAt(NET(INT32)&at)
		{
			#ifdef HAS_WINDOW
				Tutorial::SetProgress(at);
			#endif
		}


		void	UnitLimits(Protocol::UnitLimits&limits)
		{
			RuntimeConfig::UnitLimits::Import(limits);
			GlobalEvents::invalidateObjectCache();
		}

		void	PlanetConfig(Grid::ExtendedPlanetConfig&cfg)
		{
			#ifdef HAS_WINDOW
				ConfigurationMode::Update(cfg);
			#endif
		}

		void	OperationLacksResources( Protocol::OperationLacksResources&res )
		{
			ASSERT__(!eventsLocked);
			#ifdef HAS_WINDOW
				DBG_ASSERT__(!ConfigurationMode::IsActive());
				Animation::SmartSchedule<Animation::OperationLacksResources>(res.timestamp,res);		
			#endif
		}

		enum class ScriptChangeFlags
		{
			Empty = 0x0,
			HaveNewTopics = 0x1,
			HaveMyAttributes = 0x2,
			HaveZeroAttributes = 0x4,
			HaveMyResearchFactionState = 0x8,

			ShouldLoad = HaveNewTopics | HaveMyAttributes | HaveZeroAttributes | HaveMyResearchFactionState

		};

		inline ScriptChangeFlags	operator|(ScriptChangeFlags a,ScriptChangeFlags b)
		{
			typedef std::underlying_type<ScriptChangeFlags>::type	T;
			return static_cast<ScriptChangeFlags>( static_cast<T>(a) | static_cast<T>(b) );
		}

		inline void	operator|=(ScriptChangeFlags&a,ScriptChangeFlags b)
		{
			a = a | b;
		}

		inline bool	operator&(ScriptChangeFlags a,ScriptChangeFlags b)
		{
			typedef std::underlying_type<ScriptChangeFlags>::type	T;
			return (static_cast<T>(a) & static_cast<T>(b) ) != 0;
		}
		
		static ScriptChangeFlags scriptChangeFlags = ScriptChangeFlags::Empty;


		void	CheckScriptChangeFlags()
		{
			if (scriptChangeFlags == ScriptChangeFlags::ShouldLoad)
			{
				scriptChangeFlags = ScriptChangeFlags::Empty;
				#ifdef HAS_WINDOW
					Objects::Command::CommandGroup::CollapseAll();	//some groups are reconfigured as a result of onScriptChange()
				#endif
				Objects::Research::globalState.onScriptChange();
			}
		}


		void	ResearchFactionState(Protocol::ResearchFactionState&state)
		{
			Objects::PFaction f = ResolveFaction(state.ownerID);
			LogEvent(EventSignificance::Common,"Received research state for faction "+f->GetLogName());
			EXPECT_EQUAL(Objects::Research::globalState.allTopics.Count(),state.progressVector.Length());
			f->researchState.topicProgressVector.adoptData(state.progressVector);
			f->researchState.topicProgressChanged = true;
			#ifdef HAS_WINDOW
				if (RuntimeConfig::tutorialMode && f == Objects::Faction::mine)
					Tutorial::SignalResearchChange();
			#endif

			if (f->isOwnedByMe && f->researchState.topicProgressVector.Count() == Objects::Research::globalState.allTopics.Count())
			{
				scriptChangeFlags |= ScriptChangeFlags::HaveMyResearchFactionState;

				CheckScriptChangeFlags();
			}

		}

		static index_t ImportIndex(const Protocol::SerialUnsigned&v)
		{
			if (v.value == 0)
				return InvalidIndex;
			return v.value - 1;
		}

		//research program updates must always preceed research states
		void	ResearchProgram(Protocol::ResearchProgram&program)
		{
			Objects::Research::FactionState::BeginServerTopicUpdate();
			foreach (program,prog)
				Objects::Research::FactionState::DeclareServerTopic(ImportIndex(prog->parentTopic), prog->name,prog->iconName, prog->maxProgress,ImportIndex(prog->dependentTopic));


			scriptChangeFlags = ScriptChangeFlags::HaveNewTopics; //must wait for state. Flush all other updates
		}


		void	ResearchQueue(Protocol::ResearchQueue&research)
		{
			if (Objects::Faction::mine)
			{
				Objects::Faction::mine->researchState.RestoreResearchQueue(research);
				//HUD::Dialog::SignalSituationChange();
				#ifdef HAS_WINDOW
					if (Objects::selectedEntity && Objects::selectedEntity->IsWorkingStructure() && To<Objects::WorkingStructure>::Cast(Objects::selectedEntity)->IsLaboratory())
						HUD::HexGrid::Configure(Objects::selectedEntity);
					HUD::HexGrid::RefreshButtonState();
					if (RuntimeConfig::tutorialMode)
						Tutorial::SignalResearchChange();
					GlobalEvents::invalidateObjectCache();
				#endif
			}
		}


		template <typename T>
			static void Import(Protocol::ResearchAttributeArray<T>&source, Objects::Research::Attributes::TypeMap<T>&dest)
			{
				dest.attributes.SetSize(source.Count());	//don't assert, just resize. might change
				for (index_t i = 0; i < dest.attributes.Count(); i++)
				{
					auto&src = source[i];
					auto&dst = dest.attributes[i];
					dst.value = src.value;
					dst.description.adoptData(src.description);
					dst.flags = src.flags;
				}
			}

		template <typename T>
			static void Import(Protocol::ResearchAttributeDeltaArray<T>&source, Objects::Research::Attributes::TypeMap<T>&dest)
			{
				foreach (source,src)
				{
					auto&dst = Expect::SafeGet(dest.attributes,src->index,CLOCATION);
					dst.value = src->value;
					dst.description.adoptData(src->description);
					dst.flags = src->flags;
				}
			}


		void	T2Globals( Protocol::T2Globals&globals )
		{
			Objects::Research::globalState.t2.upgradeCostQuantity = globals.upgradeCostQuantity;
			Objects::Research::globalState.t2.upgradeCostType = Objects::Resource::ID(globals.upgradeCostTypeResourceIndex);
		}

		void	ResearchAttributeVector( Protocol::ResearchAttributeVector&attributes )
		{
			Objects::Research::Attributes::Table*table0 = nullptr;
			bool isMine = false;
			const bool zeroTable = attributes.faction.IsEmpty();
			if (zeroTable)
				table0 = &Objects::Research::globalState.zeroTable;
			else
			{
				auto f = ResolveFaction(attributes.faction);
				if (f->isOwnedByMe)
					isMine = true;
				table0 = &f->researchState.attributeTable;
			}
			auto&table = *table0;

			Import(attributes.boolAttributes,table.bools);
			Import(attributes.intAttributes,table.ints);
			Import(attributes.floatAttributes,table.floats);


			if (isMine)
			{
				scriptChangeFlags |= ScriptChangeFlags::HaveMyAttributes;

				World::actual.InvalidateSensory();
			}
			elif (zeroTable)
				scriptChangeFlags |= ScriptChangeFlags::HaveZeroAttributes;

			CheckScriptChangeFlags();

		}

		void	ResearchAttributeDeltaVector(Protocol::ResearchAttributeDeltaVector&attributes)
		{
			Objects::Research::Attributes::Table*table0 = nullptr;
			bool isMine = false;
			if (attributes.faction.IsEmpty())
				table0 = &Objects::Research::globalState.zeroTable;
			else
			{
				auto f = ResolveFaction(attributes.faction);
				isMine = f->isOwnedByMe;
				table0 = &f->researchState.attributeTable;
			}
			auto&table = *table0;

			Import(attributes.boolAttributes,table.bools);
			Import(attributes.intAttributes,table.ints);
			Import(attributes.floatAttributes,table.floats);

			if (isMine)
				World::actual.InvalidateSensory();
		}



		void	WinningAlliance(TUniqueID&id)
		{
			winningAllianceID = id;
			#ifdef HAS_WINDOW
				HUD::Statistics::Update();

				const auto winningAlliance = Objects::Alliance::FindByID(winningAllianceID);
				if (winningAlliance)
				{
					if (Objects::Faction::mine && Objects::Faction::mine->alliance == winningAlliance)
					{
						if (winningAlliance->CountLivingMembers() > 1)
							HUD::Dialog::ShowMessage("Congratulations!\nYour alliance has won this game!");
						else
							HUD::Dialog::ShowMessage("Congratulations!\nYou have won this game!");
					}
					else
					{
						Buffer0<String> names;
						winningAlliance->VisitMembers([&names](const Objects::PFaction&f)
						{
							names << f->GetName();
						});
						if (names.Count() != 1)
							HUD::Dialog::ShowMessage(implode(", ",names.ToRef())+" have won this game!");
						else
							HUD::Dialog::ShowMessage(names.First()+" has won this game!");
					}
				}

			#endif
		}



		namespace Statistics
		{
			static Objects::WFaction target;

			void	BeginSequence(Protocol::BeginStatisticsSequence&begin)
			{
				if (!target.expired())
				{
					LogEvent(EventClass::Preset::networkError,"Sequence violation. Statistics sequence on "+target.lock()->GetLogName()+" should have been ended");
					DBG_FATAL__("Sequence violation");
					EndSequence();
				}

				Objects::PFaction f = ResolveFaction(begin.factionID,begin.allianceID);
				f->name = begin.factionName;
				f->isDead = begin.factionIsDead;
				f->color = begin.color;

				LogEvent(EventClass::Preset::common,"Starting statistics sequence on "+f->GetLogName());
				target = f;
				
				f->statistics.BeginLoading();
			}

			void	EndSequence()
			{
				auto f = target.lock();
				target.reset();
				if (f)
				{
					f->statistics.EndLoading(false);
					#ifdef HAS_WINDOW
						HUD::Statistics::Update();
					#endif
				}
			}

			void	Round(Objects::Statistics::Round&r)
			{
				auto f = target.lock();
				if (!f)
					throw Except::Program::DataConsistencyFault(CLOCATION,"Unable to load round statistics: Not in sequence or sequence faction has meanwhile expired");
				f->statistics.Load(std::move(r));
			}
			void	FactionInteraction(Objects::Statistics::FactionInteraction&i)
			{
				auto f = target.lock();
				if (!f)
					throw Except::Program::DataConsistencyFault(CLOCATION,"Unable to load faction interaction statistics: Not in sequence or sequence faction has meanwhile expired");
				f->statistics.Load(std::move(i));
			}
			void	EntityType(Objects::Statistics::EntityType&t)
			{
				auto f = target.lock();
				if (!f)
					throw Except::Program::DataConsistencyFault(CLOCATION,"Unable to load entity type statistics: Not in sequence or sequence faction has meanwhile expired");
				f->statistics.Load(std::move(t));
			}
		}

		void	BeginStatisticsSequence(Protocol::BeginStatisticsSequence&begin)
		{
			Statistics::BeginSequence(begin);
		}

		void MyFaction(Protocol::MyFaction&faction)
		{
			ASSERT__(!eventsLocked);

			haveID = true;
			amSpectator = false;
			myID = faction.factionID;
			Objects::Faction::mine = ResolveFaction(myID,faction.alliance.allianceID);
			Objects::Faction::mine->alliance->UpdateConfiguration(faction.alliance.flags);
			myIDString = ToString(myID);
			Objects::Faction::mine->isOwnedByMe = true;
			#ifdef HAS_WINDOW
				HUD::FactionTable::Update();
			#endif
		}

		void AllianceConfigChange(Protocol::AllianceConfigChange&ch)
		{
			ASSERT__(!eventsLocked);
			Objects::PAlliance alliance = Objects::Alliance::FindByID(ch.allianceID);
			if (alliance)
			{
				alliance->config = ch.flags;
				#ifdef HAS_WINDOW
					HUD::FactionTable::Update();
				#endif
			}
			else
				LogEvent(EventClass::Preset::internalError,"Can't find alliance "+ToString(ch.allianceID)+" for config update");

		}

		void	VoteCreation(Protocol::VoteCreation&vote)
		{
			Vote::Create(vote.voteID,vote.remainingSeconds,vote.phrase);
		}

		void	AllianceInvite(Protocol::AllianceInvite&invite)
		{
			auto alliance = Objects::Alliance::FindByID(invite.allianceID);
			auto faction = Objects::Faction::FindByID(invite.factionID);
			if (!alliance)
			{
				LogEvent(EventClass::Preset::internalError,"Can't find alliance "+ToString(invite.allianceID)+" for invite");
				return;
			}
			if (!faction)
			{
				LogEvent(EventClass::Preset::internalError,"Can't find faction "+ToString(invite.factionID)+" for alliance invite");
				return;
			}
			switch (invite.type)
			{
				case Protocol::AllianceInviteType::Invite:
					alliance->Invite(faction);
				break;
				case Protocol::AllianceInviteType::Cancel:
				case Protocol::AllianceInviteType::Accept:
				case Protocol::AllianceInviteType::Reject:
					alliance->CancelInvite(faction);
				break;
				default:
					LogEvent(EventClass::Preset::internalError,"Unexpected invite type for alliance "+ToString(alliance->id)+" and faction "+faction->GetLogName());
			}
			#ifdef HAS_WINDOW
				HUD::FactionTable::Update();
			#endif
		}

		void	VoteResult(Protocol::VoteResult&result)
		{
			auto v = Vote::FindByID(result.voteID);
			if (!v)
			{
				LogEvent(EventSignificance::NonFatalError,"Unable to find vote "+ToString(result.voteID));
				return;
			}
			Vote::TOutcome outcome;
			outcome.eligibleToVote = result.eligibleToVote;
			outcome.quorum = result.quorum;
			outcome.result = (Protocol::VoteResultType::T)result.resultType;
			outcome.votedAgainst = result.votedAgainst;
			outcome.votedInFavor = result.votedInFavor;

			v->Close(outcome);
		}

		void	KillFaction(Protocol::KillFaction&kill)
		{
			Objects::PFaction f = Objects::Faction::FindByID(kill);
			if (f)
			{
				const bool isMine = f == Objects::Faction::mine;
				bool shouldBeGone = f->alliance->CountLivingMembers() == 1;
				const TUniqueID aid = f->alliance->id;
				const Objects::WAlliance wa = f->alliance;
				Objects::Faction::Kill(f);
				ASSERT__(!f->alliance);
				if (shouldBeGone)
				{
					ASSERT1__(wa.expired(),wa.lock()->CountLivingMembers());
					ASSERT__(!Objects::Alliance::FindByID(aid));
				}
				#ifdef HAS_WINDOW
					HUD::FactionTable::Update();
				#endif
				if (isMine)
				{
					haveFactionLoaded = false;
					haveFactionOnServer = false;
					haveID = false;
					LogEvent(EventClass(EventSignificance::ClientFatal,EventFlavor::Loss),"Your faction has been annihilated");
				}
			}
		}

		void	ChoiceResponse(Protocol::ChoiceResponse&resp)
		{
			AI::Script::SignalChoiceResponse(resp.requestID,resp.deployedID);
		}

		void	ScriptedAIState(Protocol::ScriptedAIState&state)
		{
			LogEvent(EventSignificance::Common,"Received script state for '"+state.scriptName+"' of "+String(state.dataVector.GetLength())+" byte(s)");
			AI::Script::Resume(state.scriptName,state.dataVector);
		}

		void	NexusAIHints(Protocol::NexusAIHints&state)
		{
			LogEvent(EventSignificance::Common,"Received update to nexus-hostile presences");

			Objects::NexusThreat::SetEstimationFactor(state.estimationFactor);

			
			AI::SetMinRoundForHeavyVehicleBases(state.minRoundForHeavyVehicleBases);
			AI::SetMinRoundForPlanes(state.minRoundForPlanes);

			for (int i = 0; i < 12; i++)
				Objects::NexusThreat::UpdatePole(i,Objects::NexusThreat::TLevel(),false,false);

			foreach (state.bases,pole)
			{
				Objects::NexusThreat::TLevel lvl;
				lvl.air.mobile = pole->airMobileThreatLevel;
				lvl.air.stationary = pole->airStationaryThreatLevel;
				lvl.ground.mobile = pole->groundMobileThreatLevel;
				lvl.ground.stationary = pole->groundStationaryThreatLevel;
				lvl.highestConstructorSignificance = pole->highestConstructorSignificance;
				lvl.constructorSignificanceSum = pole->constructorSignificanceSum;
				Objects::NexusThreat::UpdatePole(pole->poleIndex,lvl,pole->deployHeavyBases,pole->hasBase);
			}
		}

		void	ResourceTable(Protocol::ResourceTable&table)
		{
			Objects::Resource::lastTable = std::move(Objects::Resource::table);
			Objects::Resource::table = std::move(table);
			GlobalEvents::resourceTableUpdated();
		}

		void	AllianceMarkerText(Protocol::AllianceMarkerText&text)
		{
			Objects::PMarker m = Objects::MarkerStore::client.FindByID(text.markerID);
			if (!m)
				return;	//going to ignore this one because client might have deleted its own marker already
			Objects::PAllianceMarker am = To<Objects::AllianceMarker>::DynamicCast(m);
			if (!am)
				throw Except::IO::Network::ProtocolViolation(CLOCATION, "Trying to update text of non-alliance-marker "+m->Describe());

			am->text = text.markerText;
		}

	}


	namespace Channels
	{
		#undef DECLARE_CHANNEL
		#define	DECLARE_CHANNEL(NAME)	TCP::Channel<Protocol::NAME,0,Protocol::ChannelID::NAME>	NAME(Receiver::NAME);
		#undef DECLARE_OBJECT_SENDER
		#define	DECLARE_OBJECT_SENDER(NAME)	TCP::ObjectSender<Protocol::NAME,Protocol::ChannelID::NAME>	NAME;

		TCP::Channel<Protocol::ControlMessage,0,Protocol::ChannelID::ControlMessage>				controlMessage(Receiver::ControlMessage);

		TCP::Channel<Protocol::ClientFactionResponse,0,Protocol::ChannelID::ClientFaction>			clientFactionResponse(Receiver::ClientFactionResponse);

		TCP::Channel<Protocol::HelloResponse,0,Protocol::ChannelID::Hello>							helloResponse(Receiver::OnHelloResponse);
		TCP::Channel<Protocol::ServerHello,0,Protocol::ChannelID::ServerHello>						serverHello(Receiver::OnServerHello);
		TCP::Channel<Protocol::WorldDelta,0,Protocol::ChannelID::WorldDelta>						worldData(Receiver::OnWorldDelta);
		TCP::Channel<Protocol::NewMobileEntity,0,Protocol::ChannelID::NewMobileEntity>				newMobileEntity(Receiver::OnNewMobileEntity);
		TCP::Channel<Protocol::MobileState,0,Protocol::ChannelID::MobileState>						mobileStateUpdate(Receiver::OnMobileState);
		TCP::Channel<Protocol::StructureConstruction,0,Protocol::ChannelID::InstantiateStructure>	newStructure(Receiver::OnNewStructure);
		TCP::Channel<Protocol::StructureState,0,Protocol::ChannelID::UpdateStructure>				structureStateUpdate(Receiver::OnStructureState);
		TCP::Channel<Protocol::DestroyEntity,0,Protocol::ChannelID::DestroyEntity>					destroyEntity(Receiver::DestroyEntity);

		TCP::Channel<Protocol::PhaseChange,0,Protocol::ChannelID::PhaseChange>						phaseChange(Receiver::PhaseChange);
		TCP::Channel<Protocol::MobileEntityMotion,0,Protocol::ChannelID::MobileEntityMotion>		mobileEntityMotion(Receiver::MobileEntityMotion);
		TCP::Channel<Protocol::VagueAttackTrajectory,0,Protocol::ChannelID::VagueAttackTrajectory>			vagueAttackTrajectory(Receiver::VagueAttackTrajectory);
		TCP::Channel<Protocol::PreciseAttackTrajectory,0,Protocol::ChannelID::PreciseAttackTrajectory>		preciseAttackTrajectory(Receiver::PreciseAttackTrajectory);
		TCP::Channel<Protocol::Explosion,0,Protocol::ChannelID::Explosion>							explosion(Receiver::Explosion);
		TCP::Channel<Protocol::EntityTurretRotation,0,Protocol::ChannelID::EntityTurretRotation>	entityTurretRotation(Receiver::EntityTurretRotation);
		TCP::Channel<Protocol::ResourceUpdate,0,Protocol::ChannelID::ResourceUpdate>				resourceUpdate(Receiver::ResourceUpdate);
		TCP::Channel<Protocol::GameProgress,0,Protocol::ChannelID::GameProgress>					gameProgress(Receiver::GameProgress);

		TCP::Channel<Protocol::Marker, 0, Protocol::ChannelID::CreateMarker>						createMarker(Receiver::CreateMarker);
		TCP::Channel<Protocol::Marker, 0, Protocol::ChannelID::MarkerCreationFailedLimitReached>	markerCreationFailedLimitReached(Receiver::MarkerCreationFailedLimitReached);
		TCP::Channel<NET(TUniqueID), 0, Protocol::ChannelID::DestroyMarker>							destroyMarker(Receiver::DestroyMarker);

		TCP::Channel<Protocol::IdentifyFaction, 0, Protocol::ChannelID::IdentifyFaction>			identifyFaction(Receiver::IdentifyFaction);
		TCP::Channel<NET(TUniqueID), 0, Protocol::ChannelID::FactionOnline>							factionOnline(Receiver::FactionOnline);
		TCP::Channel<NET(TUniqueID), 0, Protocol::ChannelID::FactionOffline>						factionOffline(Receiver::FactionOffline);
		TCP::Channel<NET(TUniqueID), 0, Protocol::ChannelID::FactionRequestedNewRound>				factionRequestedNewRound(Receiver::FactionRequestedNewRound);
		TCP::Channel<NET(TUniqueID), 0, Protocol::ChannelID::FactionContinuedRound>					factionContinuedRound(Receiver::FactionContinuedRound);
		TCP::Channel<NET(UINT16), 0, Protocol::ChannelID::LockEMPZones>								lockEmpZones(Receiver::LockEmpZones);
		TCP::Channel<NET(INT32), 0, Protocol::ChannelID::UplinkBandwidth>							uplinkBandwidth(Receiver::UplinkBandwidth);
		TCP::Channel<Protocol::UnitLimits, 0, Protocol::ChannelID::UnitLimits>						unitLimits(Receiver::UnitLimits);
		TCP::Channel<Protocol::OperationLacksResources, 0, Protocol::ChannelID::OperationLacksResources>			operationLacksResources(Receiver::OperationLacksResources);
		TCP::Channel<Protocol::GameConfig,0,Protocol::ChannelID::GameConfig>						gameConfig(Receiver::GameConfig);

		TCP::Channel<NET(INT32),0,Protocol::ChannelID::TutorialAt>									tutorialAt(Receiver::TutorialAt);
		TCP::Channel<TUniqueID,0,Protocol::ChannelID::WinningAlliance>								winningAlliance(Receiver::WinningAlliance);

		namespace Statistics
		{
			TCP::Channel<Objects::Statistics::Round, 0, Protocol::ChannelID::RoundStatistics>		round(Receiver::Statistics::Round);
			TCP::Channel<Objects::Statistics::EntityType, 0, Protocol::ChannelID::EntityTypeStatistics>	entityType(Receiver::Statistics::EntityType);
			TCP::Channel<Objects::Statistics::FactionInteraction, 0, Protocol::ChannelID::FactionInteractionStatistics>	factionInteraction(Receiver::Statistics::FactionInteraction);

			DECLARE_CHANNEL(BeginStatisticsSequence);
		}


		TCP::ObjectSender<Protocol::ClientFactionRequest,Protocol::ChannelID::ClientFaction>		clientFactionRequest;
		TCP::ObjectSender<Protocol::TouchedMarker,Protocol::ChannelID::TouchMarker>					touchMarker;
		TCP::ObjectSender<Protocol::Hello,Protocol::ChannelID::Hello>								hello;
		TCP::ObjectSender<Protocol::EntityDirectCommand, Protocol::ChannelID::EntityDirectCommand>	entityDirectCommand;
		TCP::ObjectSender<Protocol::ChoiceIntersection, Protocol::ChannelID::ChoiceIntersection>	choiceIntersection;
		TCP::ObjectSender<Protocol::DropTasks, Protocol::ChannelID::DropTasks>						dropTasks;
		TCP::ObjectSender<Protocol::StandardJob, Protocol::ChannelID::SignalNewJob>					signalNewJob;
		TCP::ObjectSender<Protocol::TerrainMod, Protocol::ChannelID::SignalTerrainMod>				signalTerrainMod;
		TCP::ObjectSender<Protocol::ConstructionAbort, Protocol::ChannelID::ConstructionAbort>		constructionAbort;
		TCP::ObjectSender<Protocol::ConstructionBegin, Protocol::ChannelID::ConstructionBegin>		constructionBegin;
		TCP::ObjectSender<Protocol::EnableStructure, Protocol::ChannelID::EnableStructure>			enableStructure;
		TCP::ObjectSender<Protocol::EntityStateUpdate,Protocol::ChannelID::Automate>				automate;
		TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::RestoreCoreLink>						restoreCoreLink;
		TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::RemoveEntity>						removeEntity;
		TCP::ObjectSender<Protocol::EntityStateUpdate, Protocol::ChannelID::Sleep>					sleep;
		TCP::ObjectSender<Protocol::ClientName, Protocol::ChannelID::ChangeMyName>					changeMyName;

		DECLARE_CHANNEL(ResourceTable);

		DECLARE_CHANNEL(MyFaction);
		DECLARE_CHANNEL(AllianceConfigChange);
		DECLARE_CHANNEL(VoteCreation);
		DECLARE_CHANNEL(VoteResult);
		DECLARE_OBJECT_SENDER(ClientVote);
		DECLARE_CHANNEL(AllianceInvite);
		DECLARE_OBJECT_SENDER(ChatMessage);
		DECLARE_OBJECT_SENDER(SaveAs);
		DECLARE_CHANNEL(KillFaction);

		DECLARE_CHANNEL(ResearchProgram);
		DECLARE_CHANNEL(ResearchFactionState);
		DECLARE_CHANNEL(ResearchQueue);
		DECLARE_OBJECT_SENDER(QueueResearch);
		DECLARE_CHANNEL(T2Globals);
		DECLARE_CHANNEL(ResearchAttributeVector);
		DECLARE_CHANNEL(ResearchAttributeDeltaVector);

		DECLARE_CHANNEL(ChoiceResponse);

		DECLARE_CHANNEL(ScriptedAIState);
		DECLARE_CHANNEL(NexusAIHints);

		DECLARE_OBJECT_SENDER(EntityTrigger);

		DECLARE_CHANNEL(AllianceMarkerText);

		DECLARE_OBJECT_SENDER(TransferEntityOwnership);
		DECLARE_OBJECT_SENDER(MobilizeStructure);
		DECLARE_OBJECT_SENDER(SettleMobile);
	}


	SteamNetwork::HybridClient		client;
	//SOCKET			broadcastListenSocket = INVALID_SOCKET;

	String			chosenNickname;
	bool			customNickname;
	//Timer::Time		lastPong = timer.Now();
	


	void			AbandonCurrentFaction()
	{
		if (haveFactionLoaded && client.IsConnected())
		{
			Network::Channels::changeMyName.SendTo(Network::client,Network::chosenNickname+" (abandoned)");

			Protocol::ClientFactionRequest ready;
			ready.startFactionTypes = 0;
			ready.operation = Protocol::FactionOperation::AbandonCurrent;
			LogEvent(EventClass::Preset::uncommon, nullptr, "Abandoning current faction");
			Channels::clientFactionRequest.SendTo(client,ready);

			#ifdef HAS_WINDOW
				HUD::WindowStack::Show(HUD::Menu::login);
			#endif
		}
	}


	void		AttemptFactionLogin( Protocol::FactionOperation::op_t op, UINT32 factionTypes, const String&serverPassword )
	{
		Protocol::ClientFactionRequest ready;
		ready.startFactionTypes = factionTypes;
		ready.operation = op;
		CryptographicID accessToken;
		if (serverPassword.IsNotEmpty())
		{
			Password::Hash(passwordChallenge,serverPassword,ready.hashedServerPassword);
			LogEvent(EventClass::Preset::common,"Using server password '"+serverPassword+"' with challenge '"+passwordChallenge.ToFullString()+"': '"+Bin2Str::ConvertPOD(ready.hashedServerPassword));
		}
		else
		{
			ready.hashedServerPassword = Password::Hashed::Empty;
		}

		if (RuntimeConfig::aiMode)
		{
			LogEvent(EventClass::Preset::common, nullptr, "Attempting to log in to " + ToString(worldID) + " as AI");
		}
		else
		{
			switch (op)
			{
				case Protocol::FactionOperation::ForceNew:
					LogEvent(EventClass::Preset::common, nullptr, "Forcing new faction in world " + ToString(worldID));
				break;
				case Protocol::FactionOperation::ForceContinue:
					LogEvent(EventClass::Preset::common, nullptr, "Forcing faction resumption in world " + ToString(worldID));
				break;
			}
		}


		Channels::clientFactionRequest.SendTo(client,ready);
	}



	void		OnEvent(TCP::event_t ev, TCP::Peer&)
	{
		#ifdef HAS_WINDOW
			attemptingToConnect = 0;
		#endif
		switch (ev)
		{
			case TCP::Event::ConnectionEstablished:
			{
				Receiver::eventsLocked = false;


				#ifdef HAS_WINDOW
					World::allowRecompilation = false;	//until game begins
					Tutorial::HideMessage("Servers");
					Tutorial::lastDelayingEvent.Update();
					if (RuntimeConfig::tutorialMode)
						Tutorial::SignalConnectedToTutorialServer();
				#endif

				amOnline = true;

				LogEvent(EventClass::Preset::uncommon, nullptr, "Connection established to "+client.ToString());
				//if (connectingToSteam)
				//	SteamNetwork::AdvertiseServer(connectingToSteam);

				//lastPong = timer.Now();


				GlobalEvents::serverConnectionEstablished();
			}
			break;
			case TCP::Event::ConnectionFailed:
			{
				LogEvent(EventClass::Preset::networkError, nullptr, "Connection attempt failed");
				#ifdef HAS_WINDOW
					World::allowRecompilation = true;
					HUD::WindowStack::SignalSituationChange();

				#endif
				RemoveServer(lastServerAddress,lastLobbyID);
				RefreshServerListNow();
				GlobalEvents::serverConnectionFailed();
			}
			break;
			case TCP::Event::ConnectionLost:
			{
				Receiver::scriptChangeFlags = Receiver::ScriptChangeFlags::Empty;
				RuntimeConfig::tutorialMode = false;
				if (!RuntimeConfig::aiMode)
					RuntimeConfig::serverAddress = ServerAddress();	//prevent any automatic reconnect
				
				SteamInterface::InvalidateLastAccessToken();
				//SteamInterface::LeaveLobby();
				SteamInterface::AdvertiseLobby(0);


				String err = client.GetError(true);

				LogEvent(EventClass( EventSignificance::Uncommon, EventFlavor::Loss ), nullptr, err.IsNotEmpty() ? err : "Connection closed or lost");
				amOnline = false;
				haveFactionOnServer = false;

				serverRequiresPassword = false;
				RuntimeConfig::serverPassword = "";

				FlushFaction(true);
				#ifdef HAS_WINDOW
					World::allowRecompilation = true;
					HUD::WindowStack::CloseAll();
				#endif



				if (RuntimeConfig::aiMode && RuntimeConfig::serverAddress.host.IsNotEmpty())
					application_shutting_down = true;
				#ifdef HAS_WINDOW
					HUD::Dialog::QueryNicknameIfEmpty();
				#endif

				Receiver::eventsLocked = true;


				#ifdef HAS_WINDOW
					ConfigurationMode::SetActive(false);
					HUD::WindowStack::Hide(HUD::Menu::login);
					HUD::HostList::CheckVisibility(true);
					HUD::WindowStack::SignalSituationChange();
				#endif
				RemoveServer(lastServerAddress,lastLobbyID);
				RefreshServerListNow();
				GlobalEvents::serverConnectionLost();

			}
			break;
		}
	}

	void		OnSignal(UINT32 signal, TCP::Peer&origin)
	{
		switch (signal)
		{
			case Protocol::ChannelID::EndStatisticsSequence:
				Receiver::Statistics::EndSequence();
			break;
			case Protocol::ChannelID::BeginGame:
				#ifdef HAS_WINDOW
					World::allowRecompilation = true;
				#endif
				gameHasBegun = true;
				LogEvent(EventClass::Preset::uncommon,nullptr,"Game has begun");
				if (RuntimeConfig::IsInAnyCommandPhase())
					if (Animation::AllDone())
					{
						LogEvent(EventSignificance::Debug,"Begin Game. Animation done. OnPlayerControlBegin");
						PlayerControlBegin::OnPlayerControlBegin();
					}
					else
						LogEvent(EventSignificance::Debug,"Begin Game. Animation NOT done");
				GlobalEvents::beginGame();
			break;
			case Protocol::ChannelID::Pong:
				//lastPong = timer.Now();
				#ifdef HAS_WINDOW
					HUD::WindowStack::SignalSituationChange();
				#endif
				#ifdef _DEBUG
					//LogEvent(EventSignificance::Debug,nullptr,"Got pong from server");
				#endif
			break;
		}
	}

	void OnIgnorePackage(UINT32 channel,UINT32 size,TCP::Peer&origin)
	{
		LogEvent(EventClass::Preset::networkError,origin.ToString()+": Ignored package on channel #"+String(channel)+" of size "+String(size));
		//DBG_FATAL__("Ignored Package");

	}
	void OnIgnoredException(const TCodeLocation&loc, const std::exception&ex)
	{
		LogEvent(EventClass::Preset::networkError,String(loc)+": Ignored(TCP): "+ex.what());
		DBG_FATAL__("Ignored Exception: "+String(ex.what()));
	}

	void		Update();

	void		Setup()
	{
		LoadHosts();

		client.InstallChannel(Channels::serverHello);
		client.InstallChannel(Channels::helloResponse);
		client.InstallChannel(Channels::worldData);
		client.InstallChannel(Channels::newMobileEntity);
		client.InstallChannel(Channels::mobileStateUpdate);
		client.InstallChannel(Channels::newStructure);
		client.InstallChannel(Channels::structureStateUpdate);
		client.InstallChannel(Channels::destroyEntity);
		client.InstallChannel(Channels::controlMessage);
		client.InstallChannel(Channels::clientFactionResponse);
		client.InstallChannel(Channels::MyFaction);
		client.InstallChannel(Channels::KillFaction);
		client.InstallChannel(Channels::AllianceConfigChange);
		client.InstallChannel(Channels::AllianceInvite);
		client.InstallChannel(Channels::ResourceTable);
		client.InstallChannel(Channels::VoteCreation);
		client.InstallChannel(Channels::VoteResult);

		client.InstallChannel(Channels::phaseChange);
		client.InstallChannel(Channels::mobileEntityMotion);
		client.InstallChannel(Channels::vagueAttackTrajectory);
		client.InstallChannel(Channels::preciseAttackTrajectory);
		client.InstallChannel(Channels::explosion);
		client.InstallChannel(Channels::entityTurretRotation);
		client.InstallChannel(Channels::resourceUpdate);
		client.InstallChannel(Channels::gameProgress);

		client.InstallChannel(Channels::createMarker);
		client.InstallChannel(Channels::markerCreationFailedLimitReached);
		client.InstallChannel(Channels::destroyMarker);
		client.InstallChannel(Channels::identifyFaction);
		client.InstallChannel(Channels::factionOnline);
		client.InstallChannel(Channels::factionOffline);
		client.InstallChannel(Channels::factionRequestedNewRound);
		client.InstallChannel(Channels::factionContinuedRound);

		client.InstallChannel(Channels::lockEmpZones);
		client.InstallChannel(Channels::uplinkBandwidth);
		client.InstallChannel(Channels::unitLimits);
		client.InstallChannel(Channels::operationLacksResources);
		client.InstallChannel(Channels::gameConfig);

		client.InstallChannel(Channels::tutorialAt);
		client.InstallChannel(Channels::winningAlliance);

		client.InstallChannel(Channels::Statistics::BeginStatisticsSequence);
		client.InstallChannel(Channels::Statistics::entityType);
		client.InstallChannel(Channels::Statistics::round);
		client.InstallChannel(Channels::Statistics::factionInteraction);

		client.InstallChannel(Channels::ResearchProgram);
		client.InstallChannel(Channels::ResearchFactionState);
		client.InstallChannel(Channels::ResearchQueue);
		client.InstallChannel(Channels::T2Globals);
		client.InstallChannel(Channels::ResearchAttributeVector);
		client.InstallChannel(Channels::ResearchAttributeDeltaVector);

		client.InstallChannel(Channels::ChoiceResponse);

		client.InstallChannel(Channels::ScriptedAIState);
		client.InstallChannel(Channels::NexusAIHints);

		client.InstallChannel(Channels::AllianceMarkerText);

		client.OpenSignalChannel(Protocol::ChannelID::Pong,0);
		client.OpenSignalChannel(Protocol::ChannelID::BeginGame,0);
		client.OpenSignalChannel(Protocol::ChannelID::BeginEverything,0);
		client.OpenSignalChannel(Protocol::ChannelID::EndEverything,0);
		client.OpenSignalChannel(Protocol::ChannelID::EndStatisticsSequence,0);


		client.SetAsync(false);
		client.onEvent = OnEvent;
		client.onSignal = OnSignal;
		client.onIgnorePackage = OnIgnorePackage;
		TCP::onIgnoredException = OnIgnoredException;
		client.onDeserializationFailed = [](UINT32 channel, TCP::Peer&origin, const String&exceptionMessage)
		{
			LogEvent(EventSignificance::NonFatalError,"Failed to deserialize package on network channel "+String(channel)+": "+exceptionMessage);
		};

		ASSERT__(Net::initNet());
		#if 0
		if (RuntimeConfig::udpListen)
		{
			broadcastListenSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if (broadcastListenSocket == INVALID_SOCKET)
			{
				String error = String(Net::GetLastError());
				LogEvent(EventClass::Preset::operationAborted, nullptr, "Unable to initiate UDP listening for servers on the local area network: " + error + " (unable to create socket). Listening will be disabled.");
			}
			else
			{
				sockaddr_in me;
				memset(&me, 0, sizeof(me));

				me.sin_family = AF_INET;
				me.sin_port = htons(BROADCAST_PORT);
				me.sin_addr.s_addr = ADDR_ANY;

				int rc = ::bind(broadcastListenSocket, (const sockaddr*)&me, (int)sizeof(me));
				if (rc != 0)
				{
					String error = String(Net::GetLastError());
					LogEvent(EventClass::Preset::operationAborted,nullptr,"Unable to initiate UDP listening for servers on the local area network: "+error+" (maybe another instance of " PRODUCT_NAME " is running already?). Listening will be disabled." );
					RuntimeConfig::udpListen = false;
					closesocket(broadcastListenSocket);
				}
				else
				{
					u_long dontBlock = 1;
					rc = ioctlsocket(broadcastListenSocket, FIONBIO, &dontBlock);	//set socket to non-blocking
					ASSERT_EQUAL__(rc, 0);
				}
			}
		}
		#endif /*0*/

		GlobalEvents::frameBegin << Update;


		/*several components need this: so w/e*/
		if (Network::ServerDiscovery::RegisterNewHost(0, SteamInterface::GetUserID(),Network::ServerAddress("localhost",0,Protocol::ServerInfo()),false,true))
			Network::SaveHosts();

	}


	//class ServerAddress
	//{
	//public:
	//	sockaddr_storage	addr;
	//	int					addrLen;
	//	Timer::Time			lastSignal;
	//	TUniqueID			id;

	//};


	//Buffer0<ServerAddress>	servers;


	/*explicit*/			ServerAddress::ServerAddress(const String&url,UINT64 steamID, const Protocol::ServerInfo&info)
	{
		this->steamID = steamID;
		this->info = info;
		index_t portAt = url.Find(':');
		if (portAt != InvalidIndex)
		{
			host = url.subStringRef(0,portAt);
			StringRef conv = url.SubStringRef(portAt+1);
			if (!convert(conv.pointer(),conv.GetLength(),basePort))
			{
				throw Except::Program::ParameterFault(CLOCATION, "Unable to parse port component '" + String(conv) + "' of '"+url+"'");
			}
		}
		else
		{
			host = url;
			basePort = DEFAULT_SERVER_BASE_PORT;
		}
	}


	Buffer0<ServerInfo,Swap>	servers;
	

	Time::Stamp		lastPing,lastServerTableUpdate;
	Time::Stamp		lastServerProbe = Time::Stamp() - 50_s;
	bool			steamDiscovery=true;
	bool			forceTCP=false;

	void	SteamLobbyLost(UINT64 lobbyID)
	{
		if (Network::autoConnectToLobby == lobbyID)
		{
			LogEvent( EventClass(EventSignificance::Important, EventFlavor::Loss), "Failed to auto-connect to lobby #"+String(lobbyID));
			Network::autoConnectToLobby = 0;
			#ifdef HAS_WINDOW
				HUD::WindowStack::SignalSituationChange();
				HUD::Dialog::ShowMessage("Failed to connect to lobby #"+String(attemptingToConnect)+". Steam reports that it does not exist");
				attemptingToConnect = 0;
			#endif

		}
	}

	void		Update()
	{
		if (!client.IsConnected() && (RuntimeConfig::serverAddress.host.IsNotEmpty() || autoConnectToLobby))
			ConnectToConfiguredServerIfAvailable();

		SteamNetwork::ResolveGlobalEvents();
		client.Resolve();

		if (steamDiscovery && !client.IsConnected() && lastServerProbe.CheckIfOutdatedAndUpdate(30_s))
		{
			steamServers.Clear();
			SteamInterface::ProbeServers([](UINT64 lobbyID, UINT64 serverID,UINT64 ownerID, const String&addr0, const Protocol::ServerInfo &info)
			{
				ServerAddress addr(addr0,serverID,info);
				ServerDiscovery::RegisterNewHost(lobbyID, ownerID,addr,true,true);
			},
			[](UINT64 lobbyID, const String&string, SteamInterface::ServerMatchError err)
			{
				switch (err)
				{
					case SteamInterface::ServerMatchError::VersionMismatch:
					case SteamInterface::ServerMatchError::StampOutdated:
					break;
					default:
						LogEvent(EventSignificance::Common,"Steam lobby error on "+SteamInterface::IDToString(lobbyID)+": "+string);
					break;
				}
				#ifdef HAS_WINDOW
					if (lobbyID == attemptingToConnect)
					{
						switch (err)
						{
							case SteamInterface::ServerMatchError::StampOutdated:
								HUD::Dialog::ShowMessage("Unable to connect.\nLast known time stamp is outdated by "+string+" seconds. This server probably quit");
							break;
							case SteamInterface::ServerMatchError::VersionMismatch:
								HUD::Dialog::ShowMessage("Unable to connect.\nServer reports to be '"+string+"', but local version expected '"+String(SERVER_NAME)+"'");
							break;
							default:
								HUD::Dialog::ShowMessage("Connection attempt failed due to unexpected or missing server data:\n"+string+"\nIf you are certain this should not happen, and the problem persists, you may have to restart Steam");
							break;
						}
					}
					attemptingToConnect = false;
				#endif
			});
		}
		#if 0
		if (broadcastListenSocket != INVALID_SOCKET)
		{
			Protocol::THostBroadcastPacket	packet;
			for (;;)
			{
				sockaddr_storage from;
				memset(&from, 0, sizeof(from));
				socklen_t len = (socklen_t)sizeof(from);
				int bytes = recvfrom(broadcastListenSocket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&from, &len);
				if (bytes == sizeof(packet))
				{
					packet.version[ARRAYSIZE(packet.version) - 1] = 0;
					//if (!strcmp(packet.version, SERVER_NAME))	//connect anyways
					{
						bool found = false;
						ServerAddress addr;
						addr.host = TCP::ToString(from,len,false);
						if (addr.host == "127.0.0.1" || addr.host == "::1")
							addr.host = "localhost";
						addr.basePort = packet.serverBasePort;
						foreach(ServerDiscovery::attempts, sv)
						{
							if ((*sv)->address == addr)
							{
								found = true;
								break;
							}
						}
						if (!found)
						{
							ServerDiscovery::RegisterNewHost(addr, !strcmp(packet.version, SERVER_NAME),true);
						}
					}
				}
				else
					if (bytes <= 0)
						break;
					else
						FATAL__("weird");
			}
		}
		#endif

		Time::Stamp t;
		if (t -  lastPing > 2_s)
		{
			lastPing = t;
			if (client.IsConnected())
			{
				static const Time::Delta maxPong = 
					#ifdef _DEBUG
						15_s;
					#else
						10_s;
					#endif
				Time::Delta age = t - client.GetLastPackageTime();
				if (age > maxPong)
				{
					LogEvent(EventClass::Preset::uncommon,nullptr,"Server has not responded for "+String(age.ToSeconds())+"/"+String(maxPong.ToSeconds())+" second(s). Disconnecting");
					client.Disconnect();
				}
				else
				{
					client.SendSignal(Protocol::ChannelID::Ping);
					//#ifdef _DEBUG
					//	LogEvent(EventSignificance::Debug,nullptr,"ping");
					//#endif
				}
			}

		}

		if (lastServerTableUpdate.CheckIfOutdatedAndUpdate(0.5_s) && !client.IsConnected() && !client.IsAttemptingToConnect())
		{
			#ifdef DEBUG_SERVER_LIST
				if (servers.IsEmpty())
				{
					Random rng;
					for (index_t i = 0; i < 100; i++)
					{
						auto&s = servers.Append();
						s.addresses << std::make_pair(ServerAddress("localhost"),rng.getFloat(1,200));
						s.id.Generate();
						s.info.gameInfo.activePhase = (BYTE)rng.get(RuntimeConfig::Phase::N-1);
						s.info.gameInfo.gameConfig.gameFlags = Protocol::GameFlag::AllFactionSupport;
						s.info.gameInfo.gameConfig.gameGoal = (BYTE)rng.get(RuntimeConfig::GameGoal::N -1);
						s.info.gameInfo.gameConfig.worldDiscovery = (BYTE)rng.get(Grid::WorldDiscovery::N -1);
						s.info.gameInfo.numFactionsCreated = M::Sqr(rng.get(0,16));
						s.info.gameInfo.numFactionsOnline = rng.get(s.info.gameInfo.numFactionsCreated);
						s.info.gameInfo.roundNumber = M::Sqr(M::Sqr(rng.get(8)));
						s.info.gameInfo.worldID.Generate();
						s.info.hostInfo.hostID = s.id;
						s.info.hostInfo.serverBasePort = DEFAULT_SERVER_BASE_PORT;
						s.rightVersion = true;
						s.selected = false;
					}
				}
			#else

				servers.Clear();
				//serverMap.Clear();


				foreach (ServerDiscovery::attempts,_a)
				{
					const auto&a = **_a;
					if (a.hasContact && (a.hasInfo || a.protocolIncompatible))
						ServerDiscovery::MapNonSteamServer(a.address,a.lastInfo,a.protocolIncompatible,a.rightVersion,a.lastPing);
				}
				foreach (steamServers,serv)
				{
					ServerDiscovery::MapSteamServer(*serv);
				}

				//LogEvent(EventClass::Preset::important,String(steamServers.Count())+ " steam server(s) resulting in "+String(servers.Count())+" server(s)");

			#endif
		}
	
	}

	void		Shutdown()
	{
		client.Disconnect();
		//closesocket(broadcastListenSocket);
		//broadcastListenSocket = INVALID_SOCKET;

	}


	bool		ControlEnabled( )
	{
		return client.IsConnected() && RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase && Animation::AllDone() && !newRoundRequested;
	}


}




void	Network::Receiver::PhaseChange(Protocol::PhaseChange&c)
{
	ASSERT__(!eventsLocked);

	switch (c.newPhase)
	{
		case RuntimeConfig::Phase::AnimationPhase:
			#ifdef HAS_WINDOW
				Objects::Renderer::ClearSmokeTrails();
				HUD::HexGrid::EventLog::Clear();
				if (TimeLapse::enabled)
					InputControl::saveScreenshot = true;
			#endif
			PlayerControlBegin::pauseAutomaticRoundProgression = false;	//events that undo this happen during the animation phase
			if (RuntimeConfig::aiMode)
				AI::ClearPending();	//whichever is left, cannot be computed
			Animation::atTime = 0.f;
			Objects::ForeachMobileWorldEntity([](const Objects::PMobileEntity&m)
			{
				m->isInMovementStream = false;	//just in case
			},Objects::IncludeGhosts::None);
			if (c.timeFactor > 0)
				animationTimeFactor = c.timeFactor;
		break;
		case RuntimeConfig::Phase::CommandPhase:
			roundTimer = c.timeFactor;
			RuntimeConfig::roundNumber = c.roundNumber;
			RuntimeConfig::phase.Load(c.newPhase);

			Objects::ForeachMobileWorldEntity([](const Objects::PWorldEntity&e)
			{
				if (!e->isOwnedByMe)
					return;
				Objects::PMarker m = Objects::MarkerStore::client.FindByLocation(Objects::MarkerType::EntityDeath, e->location);
				if (m)
					ObjectHelper::SignalMarkerDeletion(m->id);
			},Objects::IncludeGhosts::None);

			#ifdef HAS_WINDOW
				Objects::structures.Foreach([](const Objects::PStructure&s)
				{
					if (s->isOwnedByMe && s->IsWorkingStructure() )
					{
						Objects::PWorkingStructure ws = To<Objects::WorkingStructure>::Cast(s);
						if (ws->AwaitsUserInteraction())
							Tutorial::ShowMessage("Interaction");
					}
				});

				if (TimeLapse::enabled)
					InputControl::saveScreenshot = true;
			#endif
		break;
	}
	#ifdef HAS_WINDOW
		Tutorial::lastDelayingEvent.Update();
	#endif
	RuntimeConfig::phase.Load(c.newPhase);
	GlobalEvents::phaseChange();
	if (RuntimeConfig::IsInAnyCommandPhase())
		newRoundRequested = false;
	RuntimeConfig::roundNumber = c.roundNumber;
	phaseStarted.Update();
	#ifdef HAS_WINDOW
		Objects::UpdateChoices(ControlEnabled());
		HUD::HexGrid::RefreshContacts();
	#endif

	//while (Objects::Network::records.IsNotEmpty() && timer.GetSecondsSince(Objects::Network::records.Peek().recorded) > 10)
	//	Objects::Network::records.EraseFront();
	LogEvent(EventSignificance::Common,nullptr,"Server phase changed to "+String(RuntimeConfig::phase.ToString())+" ("+String(c.timeFactor)+") @"+String(Animation::atTime));
	if (RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase)
	{
		LogEvent(EventClass::Preset::important, nullptr, "Begin round " + RuntimeConfig::roundNumber.ToString());


	}
	if (gameHasBegun)
		for (const auto&f : Objects::Faction::allKnown)
		{
			f->requestedNewRound = false;
			f->roundChangeNotifications = 0;
		};
	if (RuntimeConfig::IsInAnyCommandPhase())
	{
		if (gameHasBegun)
		{
			if (Animation::AllDone())
			{
				LogEvent(EventSignificance::Debug,"Command phase. Game has begun. Animation done. OnPlayerControlBegin");
				PlayerControlBegin::OnPlayerControlBegin();
			}
			else
			{
				LogEvent(EventSignificance::Debug,"Command phase. Game has begun. Animation NOT done");
			}
		}
		else
			LogEvent(EventSignificance::Debug,"Command phase. Game has NOT begun");
	}
	else
		if (RuntimeConfig::phase == RuntimeConfig::Phase::EndOfGame)
		{
			#ifdef HAS_WINDOW
				HUD::Statistics::Update();
				HUD::Statistics::Show();
			#endif
		}

	#ifdef HAS_WINDOW
		HUD::WindowStack::SignalSituationChange();
	#endif
}


void	Network::Receiver::LockEmpZones(NET(UINT16)&zones)
{
	Grid::empZonesLocked = zones;
	Grid::empZonesUnlocked = ~zones;

	#ifdef HAS_WINDOW
		VisualTerrain::Invalidate();
	#endif
	#ifndef NO_SATELLITES
		Grid::visibleOrbitGrid.Invalidate();
	#endif
}


void	Network::Receiver::FactionOnline(NET(TUniqueID)&id)
{
	Objects::OwnerParameter faction = Network::ResolveFaction(id);
	faction->online = true;
	#ifdef HAS_WINDOW
		HUD::FactionTable::Update();
	#endif
}
void	Network::Receiver::FactionOffline(NET(TUniqueID)&id)
{
	Objects::OwnerParameter faction = Objects::Faction::FindByID(id);
	if (faction)
		faction->online = false;
	#ifdef HAS_WINDOW
		HUD::FactionTable::Update();
	#endif
}
void	Network::Receiver::FactionRequestedNewRound(NET(TUniqueID)&id)
{
	Objects::OwnerParameter faction = Network::ResolveFaction(id);
	faction->requestedNewRound = true;
	faction->roundChangeNotifications++;
	if (!faction->isOwnedByMe)
		LogEvent(faction->roundChangeNotifications < 3 ? EventSignificance::Uncommon : EventSignificance::Common,nullptr,faction->GetName()+" is done thinking");
	#ifdef HAS_WINDOW
		HUD::FactionTable::Update();
		HUD::HexGrid::RefreshButtonState();
	#endif
}

void	Network::Receiver::FactionContinuedRound(NET(TUniqueID)&id)
{
	Objects::OwnerParameter faction = Network::ResolveFaction(id);
	faction->requestedNewRound = false;
	faction->roundChangeNotifications++;
	if (!faction->isOwnedByMe)
		LogEvent(faction->roundChangeNotifications < 3 ? EventSignificance::Uncommon : EventSignificance::Common,nullptr,faction->GetName()+" is thinking some more");
	#ifdef HAS_WINDOW
		HUD::FactionTable::Update();
		HUD::HexGrid::RefreshButtonState();
	#endif
}



void	Network::Receiver::IdentifyFaction(Protocol::IdentifyFaction&id)
{
	Objects::OwnerParameter faction = Network::ResolveFaction(id.id, id.allianceID);
	faction->name = id.name;
	faction->color = id.color;
	#ifdef HAS_WINDOW
		HUD::FactionTable::Update();
	#endif
	Objects::MarkerStore::client.SignalFactionColorChange(faction);
}


void	Network::Receiver::MarkerCreationFailedLimitReached(Protocol::Marker&marker)
{
	bool anyDeleted = false;
	Buffer0<Objects::MarkerStore::TCandidate>	candidates;
	Objects::MarkerStore::client.GetDeletionCandidates(candidates);
	ASSERT__(candidates.IsNotEmpty());
	foreach(candidates,m)
		ObjectHelper::SignalMarkerDeletion(m->markerID);
	Network::Channels::createMarker.SendTo(Network::client,marker);
	//SignalMarkerCreation(marker);	//can't - would create marker once more
}


bool		Network::ConnectToConfiguredServerIfAvailable()
{
	if (Network::client.IsConnected() || Network::client.IsAttemptingToConnect())
		return false;
	static Time::Stamp lastAttempt = Time::Stamp()-10_s;
	static count_t delayCounter = 10;
	if (++delayCounter < 3 || lastAttempt.Elapsed() < 5_s)
		return false;	//make sure there is enough time to rebuild the server list in-between connection attempts to a ghost-server that is configured via runtime parameter
	foreach(Network::servers, srv)
	{
		if (srv->rightVersion && ((autoConnectToLobby && srv->lobbyID == autoConnectToLobby) || (RuntimeConfig::serverAddress.host.IsNotEmpty() && srv->HasAddress(RuntimeConfig::serverAddress) )))
		{
			index_t best = 0;
			
			srv->selected = true;
			srv->Connect();


			lastAttempt.Update();
			delayCounter = 0;
			return true;
		}

	}
	return false;
}

void					Network::RemoveServer(const ServerAddress&a, UINT64 lobbyID)
{
	foreach (steamServers,serv)
	{
		if (serv->address == a || serv->lobbyID == lobbyID)
		{
			steamServers.erase(serv);
			break;
		}
	}

	foreach (servers,serv)
	{
		if ((serv->addresses.FindAndErase( a) && serv->addresses.IsEmpty())
			||
			serv->lobbyID == lobbyID)
		{
			servers.erase(serv);
			break;
		}
	}
	//ServerDiscovery::TryRemoveHost(a);
	SteamInterface::UnregisterExternalLobby(lobbyID);

	#ifdef HAS_WINDOW
		HUD::HostList::Update();
	#endif
}

void					Network::RefreshServerListNow()
{
	lastServerProbe = Time::Stamp() - 50_s;
}


bool					Network::SteamDiscoveryIsEnabled()
{
	return steamDiscovery;
}

void					Network::SetSteamDiscovery(bool enableDiscovery)
{
	steamDiscovery = enableDiscovery;
	if (!enableDiscovery)
		steamServers.Clear();
	else
		RefreshServerListNow();
	#ifdef HAS_WINDOW
		HUD::HostList::Update();
	#endif
}


bool					Network::ForcingTCPConnections()
{
	return forceTCP;
}

void					Network::ForceTCPConnections(bool doForce)
{
	forceTCP = doForce;
	#ifdef HAS_WINDOW
		HUD::HostList::Update();
	#endif
}



void	Network::SignalProtocolDisruption(const String&message)
{
	LogEvent(EventClass::Preset::error,message);
	if (client.IsConnected())
	{
		#ifdef HAS_WINDOW
			HUD::Dialog::ShowMessage("Protocol disruption detected. Disconnecting from server.");
		#endif
		client.Disconnect();
	}
}



#include <global_root.h>
#include "net.h"

#include <net/tcp.h>
#include "steamInterface.h"
#include <ports.h>

#include "../protocol.h"
#include "../../client/version.h"
#include  "../runtimeConfig.h"

#ifdef HAS_STEAM_API
	#include <steam/steam_api.h>
	#include <steam/steam_gameserver.h>
	#include <steam/isteamnetworkingsockets.h>
	#include <steam/isteamnetworkingutils.h>
#endif

#ifndef CLIENT
#include "../../server/components/udpBroadcast.h"
#endif


using namespace Time::Literals;

namespace SteamInterface
{
	#ifdef HAS_STEAM_API
		String	ToString(EBeginAuthSessionResult rs);

		String	ToString(EAuthSessionResponse rs);

		String ToString(EResult rs);
		String ToString(ESteamNetworkingConnectionState);
	#endif
}

namespace SteamNetwork
{
	/**
	Virtual port on which servers answer authentication queries (see GetAuthenticationState()).
	Game connections use the steam channel passed to Server::StartService() and HybridClient::ConnectToSteam() as virtual port.
	*/
	static const int AuthVirtualPort = 1000;
	#ifdef HAS_STEAM_API

		/**
		Largest message ISteamNetworkingSockets accepts.
		Larger packages are split by SteamPeer::SendToSteam() and reassembled by SteamPeer::AssembleSteamMessage()
		*/
		static const UINT32 MaxSteamMessageSize = k_cbMaxSteamNetworkingSocketsMessageSizeSend;
		/**
		Largest package accepted for reassembly. Protects against bogus size headers
		*/
		static const UINT32 MaxAssembledSize = 256*1024*1024;
		static const int SendFlags = k_nSteamNetworkingSend_ReliableNoNagle;

		class Network
		{
			ISteamNetworkingSockets*	access;

			struct Static
			{
				Sys::SpinLock		lock;
				volatile count_t	depth=0;
				std::thread::id lockThread;
				//TCodeLocation	loc;
			};
			static Static	global;
		public:
			/**
			@param server True to access the game server interface (SteamGameServerNetworkingSockets()),
				false to access the user interface (SteamNetworkingSockets())
			*/
			explicit		Network(bool server)
			{
				Lock(/*loc*/);
				access = server ? SteamGameServerNetworkingSockets() : SteamNetworkingSockets();
				if (!access)
					Unlock();
			}

			/**/			~Network()
			{
				if (access)
					Unlock();
			}

			static void		Lock(/*const TCodeLocation&loc*/)
			{
				if (std::this_thread::get_id() == global.lockThread)
				{
					global.depth++;
					return;
				}
				global.lock.lock();
				global.lockThread = std::this_thread::get_id();
				global.depth++;
			//	global.loc = loc;

			}
			static void		Unlock()
			{
				ASSERT__ (std::this_thread::get_id() == global.lockThread);



				global.depth--;
				if (global.depth > 0)
					return;

				global.lockThread = std::thread::id();
				global.lock.unlock();

			}

			ISteamNetworkingSockets* operator->() const
			{
				if (!access)
					throw Except::Program::MemberNotFound("Steam Network Access N/A");
				return access;
			}

			operator bool () const {return access != nullptr;}

		};
		Network::Static Network::global;


		class Callback
		{
		public:
			void OnLobbyResponse( LobbyCreated_t *pResult, bool ioError);
			CCallResult<Callback, LobbyCreated_t> callCreateLobbyResponse;

		};
		std::shared_ptr<Callback>	callback;


		static HSteamListenSocket				authListenSocket = k_HSteamListenSocket_Invalid;
		static HashTable<UINT32,Server*>		listenServers;		//listen socket -> server
		static HashTable<UINT32,HybridClient*>	clientConnections;	//outgoing game connection -> client
		static HashTable<UINT32,UINT64>			authConnections;	//outgoing authentication query -> server Steam ID

		static const int NumConnectionOptions = 2;
		/**
		Fills @a options with the configuration of all Steam listen sockets and connections.
		Sets the status change callback, which Steam invokes during SteamAPI_RunCallbacks() or SteamGameServer_RunCallbacks()
		*/
		static void	GetConnectionOptions(SteamNetworkingConfigValue_t (&options)[NumConnectionOptions]);


		Vector<SteamEventHandler*>	globalDispatch;
		Sys::SpinLock dispatchLock;

		void	RegisterHandler(SteamEventHandler*h)
		{
			dispatchLock.lock();
			ASSERT__(!globalDispatch.Contains(h));
			globalDispatch.Append(h);
			dispatchLock.unlock();
		}

		void	UnregisterHandler(SteamEventHandler*h)
		{
			dispatchLock.lock();
			ASSERT__(globalDispatch.FindAndErase(h));
			dispatchLock.unlock();
		}

	#endif



	/*virtual override*/ String				SteamPeer::AddressToString(bool includePort) const
	{
		#ifdef HAS_STEAM_API
			if (steamID)
				return CSteamID(steamID).Render();
		#endif
		return Super::AddressToString(includePort);
	}
	
	void				SteamPeer::DisconnectPeer()
	{
		#ifdef HAS_STEAM_API
			if (steamChannel != -1)
			{
				TCP_TRY
				{
					if (steamConnection != k_HSteamNetConnection_Invalid)
					{
						clientConnections.Unset(steamConnection);
						Network net(steamServerSide);
						if (net)
							net->CloseConnection(steamConnection,0,"Disconnected",true);	//linger: deliver pending reliable data first
					}
				}
				TCP_CATCH
				steamChannel = -1;
				owner->HandleEvent(TCP::Event::ConnectionLost,*this);
				owner->OnDisconnect(this,TCP::Event::ConnectionLost);
				steamID = 0;
				steamConnection = k_HSteamNetConnection_Invalid;
				steamIncomingSize = steamIncomingFill = 0;
				return;
			}
		#endif
		Super::DisconnectPeer();

	}

	bool				SteamPeer::SendToSteam(const void*data, size_t dataSize)
	{
		#ifdef HAS_STEAM_API
			if (steamConnection == k_HSteamNetConnection_Invalid)
				return false;
			Network net(steamServerSide);
			if (!net)
				return false;
			const BYTE*at = (const BYTE*)data;
			do
			{
				const UINT32 chunk = (UINT32)(dataSize < MaxSteamMessageSize ? dataSize : MaxSteamMessageSize);
				if (net->SendMessageToConnection(steamConnection,at,chunk,SendFlags,nullptr) != k_EResultOK)
					return false;
				at += chunk;
				dataSize -= chunk;
			}
			while (dataSize > 0);
			return true;
		#else
			return false;
		#endif
	}

	bool				SteamPeer::AssembleSteamMessage(const BYTE*&data, UINT32&size)
	{
		#ifdef HAS_STEAM_API
			if (steamIncomingSize)
			{
				if (steamIncomingFill + size > steamIncomingSize)
				{
					//Fragment does not fit. Drop the incomplete package and let the caller report the fragment
					steamIncomingSize = steamIncomingFill = 0;
					return true;
				}
				memcpy(steamIncoming.pointer()+steamIncomingFill,data,size);
				steamIncomingFill += size;
				if (steamIncomingFill < steamIncomingSize)
					return false;
				data = steamIncoming.pointer();
				size = steamIncomingSize;
				steamIncomingSize = steamIncomingFill = 0;
				return true;
			}
			if (size == MaxSteamMessageSize)
			{
				//SendToSteam() splits packages into messages of exactly MaxSteamMessageSize bytes, followed by the remainder
				const UINT32 innerSize = ((const UINT32*)data)[1];
				if (innerSize <= MaxAssembledSize - 8 && innerSize + 8 > size)
				{
					steamIncomingSize = innerSize + 8;
					if (steamIncoming.size() < steamIncomingSize)
						steamIncoming.SetSize(steamIncomingSize);
					memcpy(steamIncoming.pointer(),data,size);
					steamIncomingFill = size;
					return false;
				}
			}
		#endif
		return true;
	}



	/*virtual override*/ void				Server::OnDisconnect(const TCP::Peer*p, TCP::event_t e)
	{
		if (ignoreDisconnection)
			return;
		const SteamPeer*sp = dynamic_cast<const SteamPeer*>(p);
		if (!sp)
		{
			Super::OnDisconnect(p,e);
			return;
		}

		PSteamPeer*p2=nullptr;

		foreach (steamPeers,p3)
			if (p3->get() == sp)
			{
				p2 = p3;
				break;
			}

		if (!p2)
			return;
		PSteamPeer mapped;
		if (steamMap.Query((*p2)->GetSteamID(),mapped) && mapped.get() == sp)	//a reconnect of the same user may have replaced it already
			steamMap.Unset((*p2)->GetSteamID());
		steamConnectionMap.Unset((*p2)->steamConnection);
		TCP_TRY
		{
			(*p2)->Destroy();
		}
		TCP_CATCH

		TCP_TRY
		{
			(*p2)->Dissolve();
		}
		TCP_CATCH
		steamPeers.erase(p2);
	}

	/*virtual override*/ void	Server::SendPackage(const TCP::Package&object, unsigned minUserLevel)
	{
		if (IsShuttingDown())
		{
			return;
		}
		Super::SendPackage(object,minUserLevel);

		#ifdef HAS_STEAM_API
			Vector<PSteamPeer> dump;
			for (const auto&p : steamPeers)
			{
				if (p->userLevel < minUserLevel)
					continue;
				if (!p->SendToSteam(object.GetData()))
					dump << p;
			}
			for (const auto&p : dump)
				p->DisconnectPeer();
		#endif
	}

	
	/*virtual override*/ void	Server::SendPackage(const TCP::PPeer&exclude, const TCP::Package&object, unsigned minUserLevel)
	{
		if (IsShuttingDown())
		{
			return;
		}
		Super::SendPackage(exclude,object,minUserLevel);

		#ifdef HAS_STEAM_API
			Vector<PSteamPeer> dump;
			for (const auto&p : steamPeers)
			{
				if (p->userLevel < minUserLevel || p == exclude)
					continue;
				if (!p->SendToSteam(object.GetData()))
					dump << p;
			}
			for (const auto&p : dump)
				p->DisconnectPeer();
		#endif
	}

		
	bool				Server::StartService(int steamChannel, UINT16 port, USHORT rootPort, bool limitToLocalhost/*=false*/, USHORT*outPort/*=nullptr*/)
	{
		return ExtStartService(steamChannel,port,rootPort,Protocol::IPv4,limitToLocalhost,outPort);

	}


	static UINT16 gamePort = 0;
	#ifdef HAS_STEAM_API
		CSteamID lobbyID;

		static count_t serversSetup = 0;

		void	LeaveLobby()
		{
			if (lobbyID != CSteamID())
			{
				if (SteamMatchmaking())
				{
					LogEvent(EventSignificance::Common,"Steam: Invalidating and leaving lobby "+String(lobbyID.Render()));
					SteamMatchmaking()->SetLobbyData( lobbyID, "version", "" );
					SteamMatchmaking()->SetLobbyType(lobbyID,k_ELobbyTypePrivate);
					SteamMatchmaking()->SetLobbyJoinable(lobbyID,false);
					SteamMatchmaking()->SetLobbyMemberLimit(lobbyID,0);
					SteamMatchmaking()->LeaveLobby(lobbyID);
					SteamInterface::RegisterLeftLobby(lobbyID.ConvertToUint64());
				}
				else
					LogEvent(EventClass::Preset::internalError,"Steam: Unable to leave lobby "+String(lobbyID.Render())+": SteamMatchmaking() returned null");

				lobbyID = CSteamID();
			}
			else
				LogEvent(EventSignificance::Common,"Steam: Cannot leave lobby: None created");
		}

		bool	CreateLobby()
		{
			if (!serversSetup || !SteamMatchmaking() || !callback)
				return false;
			#ifndef USE_CRYPTOGRAPHIC_LOGIN
				LeaveLobby();
				UINT64	oldLobby;

				LogEvent(EventSignificance::Common,"Steam: Creating new private lobby");
				SteamAPICall_t rc = SteamMatchmaking()->CreateLobby(k_ELobbyTypePrivate, 1000);
				callback->callCreateLobbyResponse.Set(rc, callback.get(), &Callback::OnLobbyResponse);
			#endif
			return true;
		}

		bool	SetupServer(USHORT basePort)
		{
			if (!serversSetup)
			{
				LogEvent(EventSignificance::Common,"Steam: SetupServer("+String(basePort)+")");
				Network::Lock(/*CLOCATION*/);
				if ( !SteamGameServer_Init( INADDR_ANY, basePort + PortOffset::SteamGame, basePort + PortOffset::SteamInfo, eServerModeAuthentication, PRODUCT_VERSION ) )
				{
					FATAL__( "SteamGameServer_Init() failed" );
				//	Alert( "Fatal Error", "Steam must be running to play this game (SteamAPI_Init() failed).\n" );
					Network::Unlock();
					return false;
				}
				gamePort = basePort;// + PortOffset::Session;
				callback.reset(new Callback());
				SteamGameServer()->SetDedicatedServer(true);
				SteamGameServer()->SetProduct(PRODUCT_NAME);
				SteamGameServer()->SetGameDescription(PRODUCT_NAME);
				SteamGameServer()->SetModDir(PRODUCT_NAME);
				//SteamGameServer()->SetGameData("123445489153871");
				SteamGameServer()->LogOnAnonymous();
				SteamGameServer()->SetAdvertiseServerActive(true);

				SteamNetworkingUtils()->InitRelayNetworkAccess();	//one utils interface serves both the user and the game server
				{
					Network net(true);
					if (net)
					{
						SteamNetworkingConfigValue_t options[NumConnectionOptions];
						GetConnectionOptions(options);
						authListenSocket = net->CreateListenSocketP2P(AuthVirtualPort,NumConnectionOptions,options);
					}
				}

				Network::Unlock();
			}
			else
				LogEvent(EventSignificance::Common,"Steam: SetupServer("+String(basePort)+") repeated "+String(serversSetup+1)+" times");

			serversSetup++;
			return true;
		}


		void	ShutdownServer()
		{
			ASSERT_GREATER_OR_EQUAL__(serversSetup,0);
			serversSetup--;
			if (!serversSetup)
			{
				Network::Lock(/*CLOCATION*/);
				if (SteamGameServer())
				{
					callback.reset();

					{
						Network net(true);
						if (net && authListenSocket != k_HSteamListenSocket_Invalid)
							net->CloseListenSocket(authListenSocket);
						authListenSocket = k_HSteamListenSocket_Invalid;
					}
					SteamGameServer()->SetAdvertiseServerActive(false);
					SteamGameServer()->LogOff();
					LeaveLobby();
					SteamGameServer_Shutdown( );
				}
				Network::Unlock();
			}
		}




	#else
		bool	CreateLobby()
		{
			return false;
		}

		void	LeaveLobby()
		{}



	#endif

	void					HandleSessionClosure()
	{
		LeaveLobby();
	}

	bool				Server::ExtStartService(int steamChannel, USHORT port, USHORT rootPort, Protocol proto, bool limitToLocalhost/*=false*/, USHORT*outPort/*=nullptr*/)
	{
	
		EndService();
		if (!Super::ExtStartService(port,proto, limitToLocalhost,outPort))
			return false;


		#ifdef HAS_STEAM_API
			SetupServer(rootPort);
			this->steamChannel = steamChannel;
			{
				Network net(true);
				if (net)
				{
					SteamNetworkingConfigValue_t options[NumConnectionOptions];
					GetConnectionOptions(options);
					steamPollGroup = net->CreatePollGroup();
					steamListenSocket = net->CreateListenSocketP2P(steamChannel,NumConnectionOptions,options);
					listenServers.Set(steamListenSocket,this);
				}
				else
					LogEvent(EventClass::Preset::networkError,"Steam: Game server networking is not available. Steam clients cannot connect");
			}
			if (IsAsync())
				RegisterHandler(this);
		#endif

		return true;
	}

	#ifdef HAS_STEAM_API

		static Protocol::GameInfo serverInfo;
		static ELobbyType	lobbyType = k_ELobbyTypePrivate;

		static String ToString(ELobbyType t)
		{
			#undef ECASE
			#define ECASE(X) case X: return #X;
			switch (t)
			{
				ECASE(k_ELobbyTypePrivate);
				ECASE(k_ELobbyTypeFriendsOnly);
				ECASE(k_ELobbyTypePublic);
				ECASE(k_ELobbyTypeInvisible);
			}

			return "Unknown lobby type";
		}


		static void	UpdateServerInfo()
		{
			if (!SteamMatchmaking()->SetLobbyData(lobbyID,"gameGoal",RuntimeConfig::gameGoal.ToString()))
				LogEvent(EventClass::Preset::networkError,"Failed to update gameGoal on Steam lobby");
			if (!SteamMatchmaking()->SetLobbyData(lobbyID,"roundNumber",serverInfo.roundNumber.ToString().c_str()))
				LogEvent(EventClass::Preset::networkError,"Failed to update roundNumber on Steam lobby");
			if (!SteamMatchmaking()->SetLobbyData(lobbyID,"gameFlags",String(serverInfo.gameConfig.gameFlags).c_str()))
				LogEvent(EventClass::Preset::networkError,"Failed to update gameFlags on Steam lobby");
			if (!SteamMatchmaking()->SetLobbyData(lobbyID,"numFactionsOnline",String(serverInfo.numFactionsOnline).c_str()))
				LogEvent(EventClass::Preset::networkError,"Failed to update numFactionsOnline on Steam lobby");
			if (!SteamMatchmaking()->SetLobbyData(lobbyID,"numFactionsCreated",String(serverInfo.numFactionsCreated).c_str()))
				LogEvent(EventClass::Preset::networkError,"Failed to update numFactionsCreated on Steam lobby");
			if (!SteamMatchmaking()->SetLobbyData(lobbyID,"worldID",serverInfo.worldID.ToFullString().c_str()))
				LogEvent(EventClass::Preset::networkError,"Failed to update worldID on Steam lobby");
			if (!SteamMatchmaking()->SetLobbyType(lobbyID,lobbyType))
				LogEvent(EventClass::Preset::networkError,"Failed to update type of Steam lobby "+String(lobbyID.Render()));
			else
				LogEvent(EventClass::Preset::common,"Type of Steam lobby updated on lobby "+String(lobbyID.Render())+" to "+ToString(lobbyType));
		}


		void UpdateLobbyStamp()
		{
			SteamMatchmaking()->SetLobbyData(lobbyID,"stamp",String(SteamUtils()->GetServerRealTime()).c_str());
		}
		void Callback::OnLobbyResponse( LobbyCreated_t *pResult, bool ioError)
		{
			if (pResult->m_eResult == k_EResultOK)
			{
				lobbyID = CSteamID(pResult->m_ulSteamIDLobby);
				#ifndef CLIENT
					UDPBroadCast::UpdateLobbyID(pResult->m_ulSteamIDLobby);
				#endif
				LogEvent(EventSignificance::Uncommon,"Lobby "+String(lobbyID.Render())+" created");
				UINT32 ip=0;
				for (int i = 0; i < 1000;i++)
				{
					const SteamIPAddress_t publicIP = SteamGameServer()->GetPublicIP();
					if (publicIP.m_eType == k_ESteamIPTypeIPv4)
						ip = publicIP.m_unIPv4;
					if (ip)
						break;
					Sleep(10);
				}
				SteamMatchmaking()->SetLobbyGameServer(lobbyID,ip,gamePort,SteamGameServer()->GetSteamID());
				SteamMatchmaking()->SetLobbyData(lobbyID,"version",SERVER_NAME);
				SteamMatchmaking()->SetLobbyJoinable(lobbyID,true);
				//SteamMatchmaking()->SetLobbyMemberLimit(lobbyID,true);

				//if (SteamUser())
				//	SteamMatchmaking()->SetLobbyData(lobbyID,"owner",String(SteamUser()->GetSteamID().ConvertToUint64()).c_str());
				UpdateLobbyStamp();
				UpdateServerInfo();

				SteamInterface::RegisterJoinedLobby(pResult->m_ulSteamIDLobby);


				//if (SteamUser())
				//	if (SteamMatchmaking()->InviteUserToLobby(lobbyID,SteamUser()->GetSteamID()))
				//		LogEvent(EventSignificance::Uncommon,"Owner invite sent to new server");
				//	else
				//		LogEvent(EventClass::Preset::networkError,"Failed to send owner invite to "+String(SteamUser()->GetSteamID().Render()));

			}
			else
				LogEvent(EventClass::Preset::networkError,"Lobby creation failed ("+SteamInterface::ToString(pResult->m_eResult)+")");
		}

	#endif

	void					UpdateGameState(const Protocol::GameInfo&info,bool createLobbyIfNecessary)
	{
		#ifdef HAS_STEAM_API

			serverInfo = info;
			String logMsg;
			if ((info.gameConfig.gameFlags & Protocol::GameFlag::PrivateServer))
			{
				lobbyType = k_ELobbyTypePrivate;
				logMsg = "Changing lobby visibility to private";
			}
			else
			{
				lobbyType = k_ELobbyTypePublic;
				logMsg = "Changing lobby visibility to public";
			}
			if (SteamGameServer() && lobbyID.IsValid())
			{
				LogEvent(EventSignificance::Common,logMsg);
				UpdateServerInfo();
			}
			else
			{
				logMsg += " (set for later application; lobby currently doesn't exist)";
				LogEvent(EventSignificance::Common,logMsg);

				if (createLobbyIfNecessary)
					CreateLobby();
			}

		#endif
	}

	void				Server::EndService()
	{
		Super::EndService();

		#ifdef HAS_STEAM_API
			if (steamChannel>=0)
			{
				if (IsAsync())
					UnregisterHandler(this);
				Network net(true);
				ignoreDisconnection = true;
				foreach (steamPeers,cl)
				{
					if (net && (*cl)->steamConnection != k_HSteamNetConnection_Invalid)
						net->CloseConnection((*cl)->steamConnection,0,"Server shutting down",true);
					(*cl)->steamConnection = k_HSteamNetConnection_Invalid;
					TCP_TRY
					{
						(*cl)->Destroy();
					}
					TCP_CATCH
					TCP_TRY
					{
						(*cl)->Dissolve();
					}
					TCP_CATCH
				}
				ignoreDisconnection = false;

				steamPeers.Clear();
				steamMap.Clear();
				steamConnectionMap.Clear();

				listenServers.Unset(steamListenSocket);
				if (net)
				{
					net->CloseListenSocket(steamListenSocket);
					net->DestroyPollGroup(steamPollGroup);
				}
				steamListenSocket = k_HSteamListenSocket_Invalid;
				steamPollGroup = k_HSteamNetPollGroup_Invalid;

				ShutdownServer();
				steamChannel = -1;
			}
			
		#endif
	}

	TCP::PPeer			Server::GetSharedPointerOfClient(TCP::Peer*p)
	{
		const SteamPeer*sp = dynamic_cast<const SteamPeer*>(p);
		if (!sp)
		{
			return Super::GetSharedPointerOfClient(p);
		}

		foreach (steamPeers,p3)
			if (p3->get() == sp)
			{
				return *p3;
			}
		return TCP::PPeer();
	}

	/*virtual override*/ void		Server::HandleSignal(UINT32 signal, TCP::Peer&sender)
	{
		if (!dynamic_cast<SteamPeer*>(&sender))
		{
			Super::HandleSignal(signal,sender);
			return;
		}
		if (onSignal)
			onSignal(signal,sender);
	}

	/*virtual override*/ void		Server::HandleObject(TCP::RootChannel*receiver, TCP::Peer&sender, const TCP::PDispatchable&object)
	{
		if (!object)
			return;
		if (!dynamic_cast<SteamPeer*>(&sender))
		{
			Super::HandleObject(receiver,sender,object);
			return;
		}
		receiver->Handle(object,sender.LinkFromThis());
	}

	/*virtual override*/ void		Server::HandleEvent(TCP::event_t ev, TCP::Peer&sender)
	{
		if (ev == TCP::Event::None)
			return;
		if (!dynamic_cast<SteamPeer*>(&sender))
		{
			Super::HandleEvent(ev,sender);
			return;
		}
		if (onEvent)
			onEvent(ev,sender);
	}

	/*virtual override*/ bool		SteamPeer::SendSignal(UINT32 channel)
	{
		#ifdef HAS_STEAM_API
			if (steamID)
			{
				return SendToSteam(&channel,sizeof(channel));
			}
		#endif
		return Super::SendSignal(channel);
	}

	/*virtual override*/ void				SteamPeer::SendPackage(const TCP::Package&object, unsigned minUserLevel/*=0*/)
	{
		#ifdef HAS_STEAM_API
			if (steamID)
			{
				if (!SendToSteam(object.GetData()))
					DisconnectPeer();
			}
		#endif
		Super::SendPackage(object,minUserLevel);
	}
	#ifdef HAS_STEAM_API

		/*struct TAuthRec
		{
			HAuthTicket handle;
			Timer::Time	created;
		};
*/
		//GenericHashTable<UINT64,TAuthRec>	authenticationRecords;

		struct TClientAuthRecord
		{
			AuthenticationState	state;
			Time::Stamp	created;
			UINT64		userID;

		};
		HashTable<UINT64,TClientAuthRecord> authenticationState;

		/**
		Routes Steam connection status changes to the listen socket or connection they belong to
		*/
		struct SteamConnectionDispatch
		{
			static void OnStatusChanged(SteamNetConnectionStatusChangedCallback_t*p)
			{
				LogEvent(EventClass::Preset::debug,"Steam connection "+String((unsigned)p->m_hConn)+" ("+String(p->m_info.m_szConnectionDescription)+"): "+SteamInterface::ToString(p->m_info.m_eState));
				const HSteamListenSocket listenSocket = p->m_info.m_hListenSocket;
				if (listenSocket != k_HSteamListenSocket_Invalid)
				{
					if (listenSocket == authListenSocket)
					{
						HandleAuthenticationQuery(*p);
						return;
					}
					Server*server;
					if (listenServers.Query(listenSocket,server))
						server->HandleSteamConnectionStatus(*p);
					else
					{
						Network net(true);
						if (net)
							net->CloseConnection(p->m_hConn,0,"Server not running",false);
					}
					return;
				}
				UINT64 serverID;
				if (authConnections.Query(p->m_hConn,serverID))
				{
					HandleAuthenticationReply(*p,serverID);
					return;
				}
				HybridClient*client;
				if (clientConnections.Query(p->m_hConn,client))
					client->HandleSteamConnectionStatus(*p);
			}

			/**
			Server side of GetAuthenticationState(): replies with the Steam ID of the local user, then closes the connection
			*/
			static void HandleAuthenticationQuery(const SteamNetConnectionStatusChangedCallback_t&p)
			{
				Network net(true);
				if (!net)
					return;
				switch (p.m_info.m_eState)
				{
					case k_ESteamNetworkingConnectionState_Connecting:
						if (net->AcceptConnection(p.m_hConn) != k_EResultOK)
							net->CloseConnection(p.m_hConn,0,"Accept failed",false);
					break;
					case k_ESteamNetworkingConnectionState_Connected:
					{
						UINT64 id = SteamUser() ? SteamUser()->GetSteamID().ConvertToUint64() : 0;
						net->SendMessageToConnection(p.m_hConn,&id,sizeof(id),k_nSteamNetworkingSend_Reliable,nullptr);
						net->CloseConnection(p.m_hConn,0,"Authentication reply sent",true);
					}
					break;
					case k_ESteamNetworkingConnectionState_ClosedByPeer:
					case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
						net->CloseConnection(p.m_hConn,0,nullptr,false);
					break;
				}
			}

			/**
			Client side of GetAuthenticationState()
			*/
			static void HandleAuthenticationReply(const SteamNetConnectionStatusChangedCallback_t&p, UINT64 serverID)
			{
				switch (p.m_info.m_eState)
				{
					case k_ESteamNetworkingConnectionState_ClosedByPeer:
					case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
					{
						Network net(false);
						if (!net)
							return;
						if (!ReceiveAuthenticationReply(net,p.m_hConn,serverID))
							LogEvent(EventClass::Preset::networkError,"Steam: Authentication query to server "+String(CSteamID(serverID).Render())+" closed without reply ("+String(p.m_info.m_szEndDebug)+")");
						net->CloseConnection(p.m_hConn,0,nullptr,false);
						authConnections.Unset(p.m_hConn);
					}
					break;
				}
			}

			/**
			Reads the server's reply to an authentication query, if available
			@return true if the reply was received
			*/
			static bool	ReceiveAuthenticationReply(Network&net, HSteamNetConnection connection, UINT64 serverID)
			{
				bool received = false;
				SteamNetworkingMessage_t*message;
				while (!received && net->ReceiveMessagesOnConnection(connection,&message,1) > 0)
				{
					if (message->m_cbSize >= 8)
					{
						const UINT64 userID = *(const UINT64*)message->m_pData;
						TClientAuthRecord*rec = authenticationState.QueryPointer(serverID);
						if (rec)
						{
							rec->state = AuthenticationState::Authenticated;
							rec->userID = userID;
							LogEvent(EventClass::Preset::debug,"Authenticated user "+String(CSteamID(userID).Render())+" on server "+String(CSteamID(serverID).Render()));
						}
						received = true;
					}
					message->Release();
				}
				return received;
			}
		};

		static void	GetConnectionOptions(SteamNetworkingConfigValue_t (&options)[NumConnectionOptions])
		{
			options[0].SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,(void*)&SteamConnectionDispatch::OnStatusChanged);
			options[1].SetInt32(k_ESteamNetworkingConfig_SendBufferSize,32*1024*1024);	//room for large packages, which are queued as several messages
		}

	#endif

	AuthenticationState	GetAuthenticationState(UINT64 serverID, UINT64 expectUserID)
	{
		#ifdef HAS_STEAM_API
			TClientAuthRecord rs;
			if (authenticationState.Query(serverID,rs))
			{
				if (rs.state == AuthenticationState::Authenticated && rs.userID != expectUserID)
					return AuthenticationState::AuthenticationFailed;
				if (rs.state == AuthenticationState::Pending && rs.created.ElapsedSeconds()>10)
				{
					rs.state = AuthenticationState::AuthenticationFailed;
					authenticationState.Set(serverID,rs);
				}
				return rs.state;
			}

			Network net(false);
			if (net)
			{
				SteamNetworkingIdentity identity;
				identity.SetSteamID64(serverID);
				SteamNetworkingConfigValue_t options[NumConnectionOptions];
				GetConnectionOptions(options);
				const HSteamNetConnection connection = net->ConnectP2P(identity,AuthVirtualPort,NumConnectionOptions,options);
				if (connection != k_HSteamNetConnection_Invalid)
				{
					authConnections.Set(connection,serverID);
					rs.created.Update();
					rs.state = AuthenticationState::Pending;
					rs.userID = 0;
					authenticationState.Set(serverID,rs);
					return rs.state;
				}
			}
		#endif
		return AuthenticationState::Pending;
	}


	void				AdvertiseServer(UINT64 serverID)
	{
		#ifdef HAS_STEAM_API
			if (SteamUser())
				SteamUser()->AdvertiseGame(serverID,0,0);
		#endif
	}

	void				ResolveGlobalEvents()
	{
	
		#ifdef HAS_STEAM_API

			if (SteamGameServer())
			{
				SteamGameServer_RunCallbacks();
				if (lobbyID != CSteamID())
				{
					static Time::Stamp lastLobbyPing;
					if (lastLobbyPing.ElapsedSeconds()>10)
					{
						UpdateLobbyStamp();
						lastLobbyPing.Update();
					}
				}
			}
			if (globalDispatch.IsNotEmpty())
			{
				dispatchLock.lock();
				foreach (globalDispatch,d)
					(**d).HandleSteamPackages();
				dispatchLock.unlock();
			}
	
			if (authConnections.IsNotEmpty())
			{
				Network net(false);
				if (net)
				{
					Vector<std::pair<UINT32,UINT64> > pending;
					authConnections.VisitAllEntries([&pending](UINT32 connection, const UINT64&serverID)
					{
						pending << std::make_pair(connection,serverID);
					});
					for (const auto&p : pending)
						if (SteamConnectionDispatch::ReceiveAuthenticationReply(net,p.first,p.second))
						{
							net->CloseConnection(p.first,0,nullptr,false);
							authConnections.Unset(p.first);
						}
				}
			}
	
		#endif
	}

	void				Server::Resolve()
	{
		Super::Resolve();
		HandleSteamPackages();
	}


	void				Server::HandleSteamPackages()
	{
		#ifdef HAS_STEAM_API
			if (steamPollGroup == k_HSteamNetPollGroup_Invalid)
				return;
			Network net(true);
			if (!net)
				return;
			SteamNetworkingMessage_t*messages[32];
			for (;;)
			{
				const int count = net->ReceiveMessagesOnPollGroup(steamPollGroup,messages,ARRAYSIZE(messages));
				if (count <= 0)
					break;
				for (int i = 0; i < count; i++)
				{
					PSteamPeer peer;
					if (steamConnectionMap.Query(messages[i]->m_conn,peer))
						HandleSteamMessage(*peer,(const BYTE*)messages[i]->m_pData,(UINT32)messages[i]->m_cbSize);
					messages[i]->Release();
				}
			}
		#endif
	}

	#ifdef HAS_STEAM_API
	void				Server::HandleSteamMessage(SteamPeer&peer, const BYTE*data, UINT32 size)
	{
		if (!peer.AssembleSteamMessage(data,size))
			return;
		if (size < 4)
			return;
		const UINT32 channel = (*(const UINT32*)data);
		const UINT32 innerSize = size >= 8 ? (*((const UINT32*)data+1)) : 0;
		if (size >= 8 && innerSize +8 != size)
		{
			if (onDeserializationFailed)
				onDeserializationFailed(channel,peer,"Content declares package to be of size "+String(innerSize+8)+", but steam reports "+String(size)+" bytes.");
			return;
		}
		peer.SignalPackage();
		if (size > 8 || !HandleIncomingSignal(channel, peer))
		{
			if (size >= 8)
			{
				MemReadStream read(data+8,size-8);
				HandleIncomingPackage(channel,peer,read);
			}
		}
	}

	void				Server::HandleSteamConnectionStatus(const SteamNetConnectionStatusChangedCallback_t&p)
	{
		Network net(true);
		if (!net)
			return;
		const HSteamNetConnection connection = p.m_hConn;
		switch (p.m_info.m_eState)
		{
			case k_ESteamNetworkingConnectionState_Connecting:
				if (net->AcceptConnection(connection) != k_EResultOK || !net->SetConnectionPollGroup(connection,steamPollGroup))
				{
					LogEvent(EventClass::Preset::networkError,"Steam: Failed to accept connection from "+String(p.m_info.m_identityRemote.GetSteamID().Render()));
					net->CloseConnection(connection,0,"Accept failed",false);
				}
			break;
			case k_ESteamNetworkingConnectionState_Connected:
			{
				const UINT64 remoteID = p.m_info.m_identityRemote.GetSteamID64();
				PSteamPeer peer;
				if (steamMap.QueryAndUnset(remoteID,peer))
					peer->DisconnectPeer();	//same user reconnected. Drop the old connection
				peer.reset(new SteamPeer(this,remoteID,steamChannel));
				peer->steamConnection = connection;
				peer->steamServerSide = true;
				steamPeers << peer;
				steamMap.Set(remoteID,peer);
				steamConnectionMap.Set(connection,peer);
				HandleEvent(TCP::Event::ConnectionEstablished,*peer);
			}
			break;
			case k_ESteamNetworkingConnectionState_ClosedByPeer:
			case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
			{
				HandleSteamPackages();	//dispatch anything received before the connection closed
				PSteamPeer peer;
				if (steamConnectionMap.Query(connection,peer))
					peer->DisconnectPeer();
				else
					net->CloseConnection(connection,0,nullptr,false);
			}
			break;
		}
	}
	#endif

	
	bool	HybridClient::Connect(const String&url)
	{
		steamID = 0;
		if (attempt.IsRunning())
		{
			attempt.Join();
			return !socketAccess->IsClosed();
		}
		ConnectAsync(url);
		attempt.Join();
		ASSERT__(!attempt.IsRunning());
		return !socketAccess->IsClosed();
	}
	
	void	HybridClient::ConnectAsync(const String&url)
	{
		steamID = 0;
		ASSERT__(!IsSelf());
		if (attempt.IsRunning())
		{
			if (!attempt.IsDone())
				return;
			attempt.Join();
		}
			
		attempt.connect_target = url;
//		attempt.client = this;
		attempt.Start();
	}

	void	HybridClient::ConnectToSteam(UINT64 id, int channel)
	{
		if (attempt.IsRunning())
		{
			if (!attempt.IsDone())
				return;
			attempt.Join();
		}
		Disconnect();
		steamID = id;
		steamChannel = channel;
		steamServerSide = false;
		state = Connecting;

		#ifdef HAS_STEAM_API
			if (IsAsync())
				RegisterHandler(this);
			{
				Network net(false);
				if (net)
				{
					SteamNetworkingIdentity identity;
					identity.SetSteamID64(id);
					SteamNetworkingConfigValue_t options[NumConnectionOptions];
					GetConnectionOptions(options);
					steamConnection = net->ConnectP2P(identity,channel,NumConnectionOptions,options);
				}
			}
			if (steamConnection != k_HSteamNetConnection_Invalid)
				clientConnections.Set(steamConnection,this);
			else
			{
				steamFailurePending = true;
				pendingSteamFailure = "Unable to connect to "+String(CSteamID(id).Render())+" via Steam";
			}
		#else
			steamFailurePending = true;
			pendingSteamFailure = "Steam is not available";
		#endif

		SignalPackage();
	}

	void	HybridClient::HandleSteamConnectionFailure(const String&reason)
	{
		if (state == Connected)
		{
			SetError(reason);
			DisconnectPeer();
		}
		else
			if (state == Connecting)
			{
				#ifdef HAS_STEAM_API
					if (steamConnection != k_HSteamNetConnection_Invalid)
					{
						clientConnections.Unset(steamConnection);
						Network net(false);
						if (net)
							net->CloseConnection(steamConnection,0,nullptr,false);
						steamConnection = k_HSteamNetConnection_Invalid;
					}
				#endif
				HandleEvent(TCP::Event::ConnectionFailed,*this);
				state = Idle;
			}
	}

	/*virtual override*/	void		HybridClient::OnDisconnect(const Peer*, TCP::event_t event)
	{
		#ifdef HAS_STEAM_API
			if (IsAsync())
				UnregisterHandler(this);
		#endif
		state = Idle;
	}

	void		HybridClient::Disconnect()
	{
		if (IsResolving())
		{
			Dispatcher::SignalPostResolutionTermination();
			return;
		}
		bool wasAttempting = IsAttemptingToConnect();
		bool wasConnected = IsConnected();
		eventLock.Block(CLOCATION);
		attempt.Join();
		DisconnectPeer();
		Join();
		eventLock.Unblock(CLOCATION);
		Dispatcher::FlushPendingEvents();
		state = Idle;
		steamFailurePending = false;
		if (wasConnected)
			HandleEvent(TCP::Event::ConnectionLost,*this);
		elif (wasAttempting)
			HandleEvent(TCP::Event::ConnectionFailed,*this);
	}


	void				HybridClient::Resolve()
	{
		TCP::Connection::Resolve();
		HandleSteamPackages();
	}

	void				HybridClient::HandleSteamPackages()
	{
		if (steamFailurePending)
		{
			steamFailurePending = false;
			HandleSteamConnectionFailure(pendingSteamFailure);
			return;
		}
		#ifdef HAS_STEAM_API
			const HSteamNetConnection connection = steamConnection;
			if (connection == k_HSteamNetConnection_Invalid)
				return;
			Network net(false);
			if (!net)
				return;
			SteamNetworkingMessage_t*messages[32];
			for (;;)
			{
				const int count = net->ReceiveMessagesOnConnection(connection,messages,ARRAYSIZE(messages));
				if (count <= 0)
					break;
				for (int i = 0; i < count; i++)
				{
					if (steamConnection == connection)	//handling a message may disconnect
						HandleSteamMessage((const BYTE*)messages[i]->m_pData,(UINT32)messages[i]->m_cbSize);
					messages[i]->Release();
				}
				if (steamConnection != connection)
					break;
			}
		#endif
	}

	#ifdef HAS_STEAM_API
	void				HybridClient::HandleSteamMessage(const BYTE*data, UINT32 size)
	{
		if (!AssembleSteamMessage(data,size))
			return;
		if (size < 4)
			return;
		const UINT32 channel = (*(const UINT32*)data);
		const UINT32 innerSize = size >= 8 ? (*((const UINT32*)data+1)) : 0;
		if (size >= 8 && innerSize + 8 != size)
		{
			if (onDeserializationFailed)
				onDeserializationFailed(channel,*this,"Package header declares package size="+String(innerSize+8)+", but Steam reports "+String(size)+" bytes");
			return;
		}
		if (state != Connected)
		{
			if (onIgnorePackage)
				onIgnorePackage(channel,size,*this);
			return;
		}

		SignalPackage();
		if (size > 8 || !HandleIncomingSignal(channel, *this))
		{
			if (size >= 8)
			{
				MemReadStream read(data+8,size-8);
				HandleIncomingPackage(channel,*this,read);
			}
		}
	}

	void				HybridClient::HandleSteamConnectionStatus(const SteamNetConnectionStatusChangedCallback_t&p)
	{
		if (p.m_hConn != steamConnection)
			return;
		switch (p.m_info.m_eState)
		{
			case k_ESteamNetworkingConnectionState_Connected:
				if (state == Connecting)
				{
					state = Connected;
					HandleEvent(TCP::Event::ConnectionEstablished,*this);
				}
			break;
			case k_ESteamNetworkingConnectionState_ClosedByPeer:
			case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
				HandleSteamPackages();	//dispatch anything the server sent before closing
				if (p.m_hConn == steamConnection)
					HandleSteamConnectionFailure(String(p.m_info.m_szEndDebug));
			break;
		}
	}
	#endif


}

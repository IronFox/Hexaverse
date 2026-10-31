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
		String ToString(ESNetSocketState);
	#endif
}

namespace SteamNetwork
{
	static const UINT16 Hello = 255,
						HelloAck = 254,
						Bye=256,
						ServerAuthRequest=257;
	#ifdef HAS_STEAM_API

	

		class Network
		{
			ISteamNetworking*	access;

			struct Static
			{
				Sys::SpinLock		lock;
				volatile count_t	depth=0;
				std::thread::id lockThread;
				//TCodeLocation	loc;
			};
			static Static	global;
		public:
			/**/			Network( /*const TCodeLocation&loc*/)
			{
				Lock(/*loc*/);
				if (SteamGameServerNetworking())
				{
					access = SteamGameServerNetworking();
				}
				else
					access = SteamNetworking();
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

			ISteamNetworking* operator->() const
			{
				if (!access)
					throw Except::Program::MemberNotFound("Steam Network Access N/A");
				return access;
			}

			operator bool () const {return access != nullptr;}

		};
		Network::Static Network::global;
		

		EP2PSend sendType = k_EP2PSendReliable;
		class Callback
		{
		public:

			Callback():onSessionRequest(this,&Callback::OnSessionRequest)
							,onSocketStatus(this,&Callback::OnSocketStatus)

			{
				onSessionRequest.SetGameserverFlag();
				onSocketStatus.SetGameserverFlag();
			}

			STEAM_CALLBACK( Callback, OnSessionRequest, P2PSessionRequest_t, onSessionRequest );
			STEAM_CALLBACK( Callback, OnSocketStatus, SocketStatusCallback_t, onSocketStatus);


			void OnLobbyResponse( LobbyCreated_t *pResult, bool ioError);
			CCallResult<Callback, LobbyCreated_t> callCreateLobbyResponse;

		};
		std::shared_ptr<Callback>	callback;

		
		void Callback::OnSessionRequest(P2PSessionRequest_t *pParam)
		{
			
			bool rs = Network(/*CLOCATION*/)->AcceptP2PSessionWithUser( pParam->m_steamIDRemote );
			LogEvent(EventClass::Preset::uncommon,"SessionRequest: "+String(pParam->m_steamIDRemote.Render())+": "+String(rs));
		}

		void Callback::OnSocketStatus(SocketStatusCallback_t*p)
		{
			{
				ESNetSocketState state = (ESNetSocketState)p->m_eSNetSocketState;
				LogEvent(EventSignificance::Uncommon,SteamInterface::ToString(state)+": "+p->m_steamIDRemote.Render());


			}
		}

		
		Buffer0<SteamEventHandler*>	globalDispatch;
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
					Network net;
					if (net)
					{
						net->SendP2PPacket(steamID,&Bye,2,k_EP2PSendReliable,steamChannel);
						Sleep(1);
						net->CloseP2PChannelWithUser(steamID,steamChannel);
					}
				}
				TCP_CATCH
				steamChannel = -1;
				owner->HandleEvent(TCP::Event::ConnectionLost,*this);
				owner->OnDisconnect(this,TCP::Event::ConnectionLost);
				steamID = 0;
				return;
			}
		#endif
		Super::DisconnectPeer();

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
		steamMap.Unset((*p2)->GetSteamID());
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

	static bool SendTo(UINT64 steamID, int steamChannel,const void*data, size_t dataSize)
	{
		if (steamID == 0)
			return false;
		#ifdef HAS_STEAM_API
		Network net/*(CLOCATION)*/;
		return net && net->SendP2PPacket(steamID,data,(UINT32)dataSize,sendType,steamChannel);
		#else
		return false;
		#endif
	}
	
	static bool SendTo(UINT64 steamID, int steamChannel,const ArrayRef<BYTE>&data)
	{
		return SendTo(steamID,steamChannel,data.pointer(),data.size());
	}

	/*virtual override*/ void	Server::SendPackage(const TCP::Package&object, unsigned minUserLevel)
	{
		if (IsShuttingDown())
		{
			return;
		}
		Super::SendPackage(object,minUserLevel);

		#ifdef HAS_STEAM_API
			Buffer0<PSteamPeer> dump;
			for (const auto&p : steamPeers)
			{
				if (p->userLevel < minUserLevel)
					continue;
				if (!SendTo(p->GetSteamID(),steamChannel,object.GetData()))
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
			Buffer0<PSteamPeer> dump;
			for (const auto&p : steamPeers)
			{
				if (p->userLevel < minUserLevel || p == exclude)
					continue;
				if (!SendTo(p->GetSteamID(),steamChannel,object.GetData()))
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
				if ( !SteamGameServer_Init( INADDR_ANY, basePort + PortOffset::Steam, basePort + PortOffset::SteamGame, basePort + PortOffset::SteamInfo, eServerModeAuthentication, PRODUCT_VERSION ) )
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
				SteamGameServer()->EnableHeartbeats(true);

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

					SteamGameServer()->EnableHeartbeats(false);
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
			if (IsAsync())
				RegisterHandler(this);
			//socket = SteamGameServerNetworking()->CreateListenSocket(steamChannel,0,0,true);
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
					ip = SteamGameServer()->GetPublicIP();
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
				ignoreDisconnection = true;
				foreach (steamPeers,cl)
				{
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
				return SendTo(steamID,steamChannel,&channel,sizeof(channel));
			}
		#endif
		return Super::SendSignal(channel);
	}

	/*virtual override*/ void				SteamPeer::SendPackage(const TCP::Package&object, unsigned minUserLevel/*=0*/)
	{
		#ifdef HAS_STEAM_API
			if (steamID)
			{
				if (!SendTo(steamID,steamChannel,object.GetData()))
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

			Network net;
			if (net && net->SendP2PPacket(serverID,&ServerAuthRequest,2,k_EP2PSendReliable,-1))
			{
				rs.created.Update();
				rs.state = AuthenticationState::Pending;
				rs.userID = 0;
				authenticationState.Set(serverID,rs);
				return rs.state;
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
	
			/*if (authenticationRecords.IsNotEmpty())
			{
				static Timer::Time lastCheck = timer.Now();
				if (timer.CheckIfOutdatedAndUpdate(lastCheck,10))
				{
					static Buffer0<UINT64> toUnset;
					Timer::Time now = timer.Now();
					authenticationRecords.VisitAllEntries([now](UINT64 id, TAuthRec&rec)
					{
						if (timer.ToSeconds(now - rec.created) > 20)
						{
							if (SteamUser())
							{
								LogEvent(EventSignificance::Uncommon,"Timeout reached: Canceling ticket "+String((unsigned)rec.handle));
								SteamUser()->CancelAuthTicket(rec.handle);
							}
							toUnset << id;
						}
					});
					Network net;
					foreach (toUnset,u)
					{
						authenticationRecords.Unset(*u);
						if (net)
							net->CloseP2PChannelWithUser(CSteamID(*u),-1);
					}
					toUnset.Clear();
				}

			}*/
			Network net/*(CLOCATION)*/;
			if (net)
			{
				//char *pchRecvBuf = NULL;
				uint32 cubMsgSize;
				CSteamID steamIDRemote;
				static	Array<BYTE>	inBuffer;


				while ( net->IsP2PPacketAvailable( &cubMsgSize,-1) )
				{
					if (cubMsgSize > inBuffer.size())
						inBuffer.SetSize(cubMsgSize);

					// see if there is any data waiting on the socket
					if ( !net->ReadP2PPacket( inBuffer.pointer(), cubMsgSize, &cubMsgSize, &steamIDRemote,-1 ) )
						break;
					if (cubMsgSize >= 8)
					{
						net->CloseP2PChannelWithUser(steamIDRemote,-1);
						UINT64 userID = *(UINT64*)(inBuffer.pointer());
						TClientAuthRecord*rec = authenticationState.QueryPointer(steamIDRemote.ConvertToUint64());
						if (rec)
						{
							rec->state = AuthenticationState::Authenticated;
							rec->userID = userID;
							LogEvent(EventClass::Preset::debug,"Authenticated user "+String(CSteamID(userID).Render())+" on server "+String(steamIDRemote.Render()));
						}
						
/*
						SteamInterface::Authenticate(userID,
									ArrayRef<BYTE>(inBuffer.pointer()+8,cubMsgSize-8),nullptr,
									[steamIDRemote,userID](bool success, const String&err)
									{
										TClientAuthRecord*rec = authenticationState.QueryPointer(steamIDRemote.ConvertToUint64());
										if (rec)
										{
											if (success)
											{
												rec->state = AuthenticationState::Authenticated;
												rec->userID = userID;
												LogEvent(EventClass::Preset::debug,"Authenticated user "+String(CSteamID(userID).Render())+" on server "+String(steamIDRemote.Render()));
											}
											else
											{
												rec->state = AuthenticationState::AuthenticationFailed;
												LogEvent(EventClass::Preset::networkError,"Failed to authenticate user "+String(CSteamID(userID).Render())+" on server "+String(steamIDRemote.Render())+" ("+err+")");
											}
										}
									});*/
						continue;
					}
					if (cubMsgSize == 2 && *(UINT16*)inBuffer.pointer() == ServerAuthRequest)
					{
					/*	Array<BYTE> package;
						if (SteamUser())
						{

							TAuthRec rec;
							if (authenticationRecords.QueryAndUnset(steamIDRemote.ConvertToUint64(),rec))
							{
								LogEvent(EventSignificance::Uncommon,"Re-authenticate: Canceling ticket "+String((unsigned)rec.handle));
								SteamUser()->CancelAuthTicket(rec.handle);
							}
							Array<BYTE> ticket;
							UINT32 actualSize = 0;
							ticket.SetSize(234);
							int cnt = 0;
							HAuthTicket currentTicket;
							for(;;)
							{
								currentTicket = SteamUser()->GetAuthSessionTicket(ticket.pointer(),(int)ticket.size(),&actualSize);
								if (currentTicket != k_HAuthTicketInvalid)
									break;
								if (++cnt > 10)
									break;
								ticket.SetSize(actualSize);
							}
							if (currentTicket != k_HAuthTicketInvalid)
							{
								rec.created = timer.Now();
								rec.handle = currentTicket;
								authenticationRecords.Set(steamIDRemote.ConvertToUint64(),rec);
								ticket.resizePreserveContent(actualSize);

								package.SetSize(ticket.length()+8);
								package.insertCopy(8,ticket);
								*(UINT64*)package.pointer() = SteamUser()->GetSteamID().ConvertToUint64();
							}
						}
						net->SendP2PPacket(steamIDRemote,package.pointer(),package.size(),k_EP2PSendReliable,-1);*/
						UINT64 id = SteamUser() ? SteamUser()->GetSteamID().ConvertToUint64() : 0;
						net->SendP2PPacket(steamIDRemote,&id,8,k_EP2PSendReliable,-1);
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
			Network net/*(CLOCATION)*/;
			if (net)
			{
				//char *pchRecvBuf = NULL;
				uint32 cubMsgSize;
				CSteamID steamIDRemote;
				static	Array<BYTE>	inBuffer;


				while ( net->IsP2PPacketAvailable( &cubMsgSize,steamChannel) )
				{
					if (cubMsgSize > inBuffer.size())
						inBuffer.SetSize(cubMsgSize);

					// see if there is any data waiting on the socket
					if ( !net->ReadP2PPacket( inBuffer.pointer(), cubMsgSize, &cubMsgSize, &steamIDRemote,steamChannel ) )
						break;
					if (cubMsgSize < 4)//probably a Hello
					{
						if (cubMsgSize == 2)
						{
							switch(*(UINT16*)inBuffer.pointer())
							{
								case HelloAck:
								{
									SendTo(steamIDRemote.ConvertToUint64(),steamChannel,&HelloAck,2);
									PSteamPeer peer;
									if (!steamMap.Query(steamIDRemote.ConvertToUint64(),peer))
									{
										peer.reset(new SteamPeer(this,steamIDRemote.ConvertToUint64(),steamChannel));
										steamPeers << peer;
										steamMap.Set(steamIDRemote.ConvertToUint64(),peer);
										HandleEvent(TCP::Event::ConnectionEstablished,*peer);
									}
								}
								break;
								case Hello:
								{
									SendTo(steamIDRemote.ConvertToUint64(),steamChannel,&Hello,2);
									PSteamPeer peer;
									if (steamMap.QueryAndUnset(steamIDRemote.ConvertToUint64(),peer))
										peer->DisconnectPeer();
								}
								break;
								case Bye:
								{
									PSteamPeer peer;
									if (steamMap.QueryAndUnset(steamIDRemote.ConvertToUint64(),peer))
										peer->DisconnectPeer();
								}
								break;
							}
						}
						continue;
					}
					const UINT32 channel = (*(UINT32*)inBuffer.pointer());
					const UINT32 innerSize = cubMsgSize >= 8 ? (*((UINT32*)inBuffer.pointer()+1)) : 0;
					
					PSteamPeer peer;
					if (!steamMap.Query(steamIDRemote.ConvertToUint64(),peer))
					{
						continue;
					}
					if (cubMsgSize >= 8 && innerSize +8 != cubMsgSize)
					{
						if (onDeserializationFailed)
							onDeserializationFailed(channel,*peer,"Content declares package to be of size "+String(innerSize+8)+", but steam reports "+String(cubMsgSize)+" bytes.");
						continue;
					}
					peer->SignalPackage();
					if (cubMsgSize > 8 || !HandleIncomingSignal(channel, *peer))
					{
						if (cubMsgSize >= 8)
						{
							MemReadStream read(inBuffer.pointer()+8,cubMsgSize-8);
							HandleIncomingPackage(channel,*peer,read);
						}
					}
				}
			}
		#endif
	}

	
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

		SteamInterface::RegisterConnectionFailure(steamID,[this](const String& reason)
		{
			HandleSteamConnectionFailure(reason);
		});
		#ifdef HAS_STEAM_API
			if (IsAsync())
				RegisterHandler(this);
		#endif
		SendTo(steamID,channel,&Hello,2);
		state = HelloSent;
		lastControlMessage.Update();
		numResents = 0;

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
			if (state == HelloSent || state == HelloAckSent )
			{
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
		if (wasConnected)
			HandleEvent(TCP::Event::ConnectionLost,*this);
		elif (wasAttempting)
			HandleEvent(TCP::Event::ConnectionFailed,*this);
	}

	
	void				HybridClient::Resolve()
	{
		TCP::Connection::Resolve();
		HandleSteamPackages();
		if (state != Idle && state != Connected && lastControlMessage.CheckIfOutdatedAndUpdate(1_s))
		{
			numResents ++;
			if (numResents > 10)
			{
				HandleSteamConnectionFailure("Retried sending control message 10 times. Giving up");
				return;
			}
			
			switch (state)
			{
				case HelloSent:
					SendTo(steamID,steamChannel,&Hello,2);
				break;
				case HelloAckSent:
					SendTo(steamID,steamChannel,&HelloAck,2);
				break;
			}
		}
	}

	void				HybridClient::HandleSteamPackages()
	{
		#ifdef HAS_STEAM_API

			Network net/*(CLOCATION)*/;
			if (net)
			{
				//char *pchRecvBuf = NULL;
				uint32 cubMsgSize;
				CSteamID steamIDRemote;
				static	Array<BYTE>	inBuffer;
				while ( net->IsP2PPacketAvailable( &cubMsgSize,steamChannel) )
				{
					if (cubMsgSize > inBuffer.size())
						inBuffer.SetSize(cubMsgSize);

					// see if there is any data waiting on the socket
					if ( !net->ReadP2PPacket( inBuffer.pointer(), cubMsgSize, &cubMsgSize, &steamIDRemote,steamChannel ) )
						break;
					if (steamIDRemote != CSteamID(steamID))
					{
						if (onIgnorePackage)
							onIgnorePackage(0,cubMsgSize,*this);
						continue;
					}
					if (cubMsgSize < 4)
					{
						if (cubMsgSize == 2)
						{
							switch(*(UINT16*)inBuffer.pointer())
							{
								case HelloAck:
								{
									if (state == HelloAckSent)
									{
										state = Connected;
										HandleEvent(TCP::Event::ConnectionEstablished,*this);
									}
								}
								break;
								case Hello:
								{
									SendTo(steamIDRemote.ConvertToUint64(),steamChannel,&HelloAck,2);
									lastControlMessage.Update();
									if (state == HelloSent)
									{
										state = HelloAckSent;
										numResents = 0;
									}
								}
								break;
								case Bye:
								{
									Disconnect();
								}
								break;
							}
						}
						continue;
					}
					const UINT32 channel = (*(UINT32*)inBuffer.pointer());
					const UINT32 innerSize = cubMsgSize >= 8 ? (*((UINT32*)inBuffer.pointer()+1)) : 0;
					if (cubMsgSize >= 8 && innerSize + 8 != cubMsgSize)
					{
						if (onDeserializationFailed)
							onDeserializationFailed(channel,*this,"Package header declares package size="+String(innerSize+8)+", but Steam reports "+String(cubMsgSize)+" bytes");
						continue;
					}
					if (state != Connected)
					{
						if (onIgnorePackage)
							onIgnorePackage(channel,cubMsgSize,*this);
						continue;
					}
					
					SignalPackage();
					if (cubMsgSize > 8 || !HandleIncomingSignal(channel, *this))
					{
						if (cubMsgSize >= 8)
						{
							MemReadStream read(inBuffer.pointer()+8,cubMsgSize-8);
							HandleIncomingPackage(channel,*this,read);
						}
					}
				}
			}
		#endif
	}


}

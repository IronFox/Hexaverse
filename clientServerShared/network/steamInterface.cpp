#include <global_root.h>
#include "steamInterface.h"
#include "../globalEvents.h"
#include "net.h"


/* Connection behavior


client -> list lobbies (lobby callback)
lobby callback -> GetLobbyOwner(lobby): join lobby if unknown (lobby joined callback)
lobby joined callback -> fallback to <lobby callback>
lobby callback -> return lobby to client
client -> connect to server of lobby (on connect)
on connect -> verify owner of server (on token)
on token -> compare server owner to lobby owner


*/



#ifdef HAS_STEAM_API

	#include <steam/steam_api.h>
	#include <steam/steam_gameserver.h>
	#include <steam/steamencryptedappticket.h>
#endif
#include <logEvent.h>
#include <container/hashtable.h>
#include <container/queue.h>
#include <general/timer.h>
#include "../runtimeConfig.h"


#include "protocol.h"
#include "../client/version.h"

#ifdef CLIENT
namespace Network
{
	void ConnectToLobby(UINT64);
}
#endif

#ifdef HAS_STEAM_API
const char * CSteamID::Render() const
{
	static char szSteamID[64];
	const char*type = "";	
	switch(m_steamid.m_comp.m_EAccountType)
	{
	case k_EAccountTypeInvalid:
		return "Inv";
	case k_EAccountTypeIndividual:
		type = "I";
		break;
	case k_EAccountTypeMultiseat:
		type = "MultiSeat";
		break;
	case k_EAccountTypeGameServer:
		type = "S";
		break;
	case k_EAccountTypeAnonGameServer:
		type = "AS";
		break;
	case k_EAccountTypePending:
		type = "Pending";
		break;
	case k_EAccountTypeContentServer:
		type = "CS";
		break;
	case k_EAccountTypeClan:
		type = "C";
		break;
	case k_EAccountTypeChat:
		type = "Chat";
		break;
	case k_EAccountTypeConsoleUser:
		type = "ConsoleUser";
		break;
	case k_EAccountTypeAnonUser:
		type = "AU";
		break;
	}
	sprintf_s(szSteamID, sizeof(szSteamID), "[%s:%08X]", type,m_steamid.m_comp.m_unAccountID);
	return szSteamID;
}

#endif


namespace SteamInterface
{
	FOnRegisteredLobbyInvalid onInvalidCallback;

	#ifdef HAS_STEAM_API

		#undef CASE
		#define CASE(X)	case X: return #X;
		String	ToString(EBeginAuthSessionResult rs)
		{
			switch (rs)
			{
				CASE(k_EBeginAuthSessionResultOK)
				CASE(k_EBeginAuthSessionResultInvalidTicket)				// Ticket is not valid.
				CASE(k_EBeginAuthSessionResultDuplicateRequest)			// A ticket has already been submitted for this steamID
				CASE(k_EBeginAuthSessionResultInvalidVersion)			// Ticket is from an incompatible interface version
				CASE(k_EBeginAuthSessionResultGameMismatch)				// Ticket is not for this game
				CASE(k_EBeginAuthSessionResultExpiredTicket)				// Ticket has expired
			}
			return "Unknown EBeginAuthSessionResult ("+String((unsigned)rs)+")";
		}

		String	ToString(EAuthSessionResponse rs)
		{
			switch (rs)
			{
				CASE(k_EAuthSessionResponseOK)							// Steam has verified the user is online, the ticket is valid and ticket has not been reused.
				CASE(k_EAuthSessionResponseUserNotConnectedToSteam)		// The user in question is not connected to steam
				CASE(k_EAuthSessionResponseNoLicenseOrExpired)			// The license has expired.
				CASE(k_EAuthSessionResponseVACBanned)					// The user is VAC banned for this game.
				CASE(k_EAuthSessionResponseLoggedInElseWhere)			// The user account has logged in elsewhere and the session containing the game instance has been disconnected.
				CASE(k_EAuthSessionResponseVACCheckTimedOut)				// VAC has been unable to perform anti-cheat checks on this user
				CASE(k_EAuthSessionResponseAuthTicketCanceled)			// The ticket has been canceled by the issuer
				CASE(k_EAuthSessionResponseAuthTicketInvalidAlreadyUsed)	// This ticket has already been used, it is not valid.
				CASE(k_EAuthSessionResponseAuthTicketInvalid)			// This ticket is not from a user instance currently connected to steam.
				CASE(k_EAuthSessionResponsePublisherIssuedBan)			// The user is banned for this game. The ban came via the web api and not VAC
			}
			return "Unknown EAuthSessionResponse ("+String((unsigned)rs)+")";
		}
		String ToString(EP2PSessionError err)
		{
			switch (err)
			{
				CASE(k_EP2PSessionErrorNone)
				CASE(k_EP2PSessionErrorNotRunningApp)
				CASE(k_EP2PSessionErrorNoRightsToApp)
				CASE(k_EP2PSessionErrorDestinationNotLoggedIn)
				CASE(k_EP2PSessionErrorTimeout)
			}
			return "Unknown EP2PSessionError ("+String((unsigned)err)+")";
		}

		String ToString(EChatRoomEnterResponse resp)
		{
			switch (resp)
			{
				CASE(k_EChatRoomEnterResponseSuccess)
				CASE(k_EChatRoomEnterResponseDoesntExist)
				CASE(k_EChatRoomEnterResponseNotAllowed)
				CASE(k_EChatRoomEnterResponseFull)
				CASE(k_EChatRoomEnterResponseError)
				CASE(k_EChatRoomEnterResponseBanned)
				CASE(k_EChatRoomEnterResponseLimited)
				CASE(k_EChatRoomEnterResponseClanDisabled)
				CASE(k_EChatRoomEnterResponseCommunityBan)
				CASE(k_EChatRoomEnterResponseMemberBlockedYou)
				CASE(k_EChatRoomEnterResponseYouBlockedMember)
			}
			return "Unknown EChatRoomEnterResponse ("+String((unsigned)resp)+")";
		}

		String ToString(EResult rs)
		{
			switch (rs)
			{
				CASE(k_EResultOK)
				CASE(k_EResultFail)
				CASE(k_EResultNoConnection)
				CASE(k_EResultInvalidPassword)
				CASE(k_EResultLoggedInElsewhere)
				CASE(k_EResultInvalidProtocolVer)
				CASE(k_EResultInvalidParam)
				CASE(k_EResultFileNotFound)
				CASE(k_EResultBusy)
				CASE(k_EResultInvalidState)
				CASE(k_EResultInvalidName)
				CASE(k_EResultInvalidEmail)
				CASE(k_EResultDuplicateName)
				CASE(k_EResultAccessDenied)
				CASE(k_EResultTimeout)
				CASE(k_EResultBanned)
				CASE(k_EResultAccountNotFound)
				CASE(k_EResultInvalidSteamID)
				CASE(k_EResultServiceUnavailable)
				CASE(k_EResultNotLoggedOn)
				CASE(k_EResultPending)
				CASE(k_EResultEncryptionFailure)
				CASE(k_EResultInsufficientPrivilege)
				CASE(k_EResultLimitExceeded)
				CASE(k_EResultRevoked)
				CASE(k_EResultExpired)
				CASE(k_EResultAlreadyRedeemed)
				CASE(k_EResultDuplicateRequest)
				CASE(k_EResultAlreadyOwned)
				CASE(k_EResultIPNotFound)
				CASE(k_EResultPersistFailed)
				CASE(k_EResultLockingFailed)
				CASE(k_EResultLogonSessionReplaced)
				CASE(k_EResultConnectFailed)
				CASE(k_EResultHandshakeFailed)
				CASE(k_EResultIOFailure)
				CASE(k_EResultRemoteDisconnect)
				CASE(k_EResultShoppingCartNotFound)
				CASE(k_EResultBlocked)
				CASE(k_EResultIgnored)
				CASE(k_EResultNoMatch)
				CASE(k_EResultAccountDisabled)
				CASE(k_EResultServiceReadOnly)
				CASE(k_EResultAccountNotFeatured)
				CASE(k_EResultAdministratorOK)
				CASE(k_EResultContentVersion)
				CASE(k_EResultTryAnotherCM)
				CASE(k_EResultPasswordRequiredToKickSession)
				CASE(k_EResultAlreadyLoggedInElsewhere)
				CASE(k_EResultSuspended)
				CASE(k_EResultCancelled)
				CASE(k_EResultDataCorruption)
				CASE(k_EResultDiskFull)
				CASE(k_EResultRemoteCallFailed)
				CASE(k_EResultPasswordUnset)
				CASE(k_EResultExternalAccountUnlinked)
				CASE(k_EResultPSNTicketInvalid)
				CASE(k_EResultExternalAccountAlreadyLinked)
				CASE(k_EResultRemoteFileConflict)
				CASE(k_EResultIllegalPassword)
				CASE(k_EResultSameAsPreviousValue)
				CASE(k_EResultAccountLogonDenied)
				CASE(k_EResultCannotUseOldPassword)
				CASE(k_EResultInvalidLoginAuthCode)
				CASE(k_EResultAccountLogonDeniedNoMail)
				CASE(k_EResultHardwareNotCapableOfIPT)
				CASE(k_EResultIPTInitError)
				CASE(k_EResultParentalControlRestricted)
				CASE(k_EResultFacebookQueryError)
				CASE(k_EResultExpiredLoginAuthCode)
				CASE(k_EResultIPLoginRestrictionFailed)
				CASE(k_EResultAccountLockedDown)
				CASE(k_EResultAccountLogonDeniedVerifiedEmailRequired)
				CASE(k_EResultNoMatchingURL)
				CASE(k_EResultBadResponse)
				CASE(k_EResultRequirePasswordReEntry)
				CASE(k_EResultValueOutOfRange)
				CASE(k_EResultUnexpectedError)
				CASE(k_EResultDisabled)
				CASE(k_EResultInvalidCEGSubmission)
				CASE(k_EResultRestrictedDevice)
				CASE(k_EResultRegionLocked)
				CASE(k_EResultRateLimitExceeded)
				CASE(k_EResultAccountLoginDeniedNeedTwoFactor)
				CASE(k_EResultItemDeleted)
				CASE(k_EResultAccountLoginDeniedThrottle)
				CASE(k_EResultTwoFactorCodeMismatch)
				CASE(k_EResultTwoFactorActivationCodeMismatch)
				CASE(k_EResultAccountAssociatedToMultiplePartners)
				CASE(k_EResultNotModified)
				CASE(k_EResultNoMobileDevice)
				CASE(k_EResultTimeNotSynced)
				CASE(k_EResultSmsCodeFailed)
				CASE(k_EResultAccountLimitExceeded)
				CASE(k_EResultAccountActivityLimitExceeded)
				CASE(k_EResultPhoneActivityLimitExceeded)
				CASE(k_EResultRefundToWallet)
				CASE(k_EResultEmailSendFailure)
				CASE(k_EResultNotSettled)
				CASE(k_EResultNeedCaptcha)
				CASE(k_EResultGSLTDenied)
				CASE(k_EResultGSOwnerDenied)
				CASE(k_EResultInvalidItemType)
			};
			return "Unknown EResult ("+String((unsigned)rs)+")";
		}

		String ToString(ESNetSocketState st)
		{
			switch(st)
			{
				CASE(k_ESNetSocketStateInvalid)			
				CASE(k_ESNetSocketStateConnected)
				CASE(k_ESNetSocketStateInitiated)
				CASE(k_ESNetSocketStateLocalCandidatesFound)
				CASE(k_ESNetSocketStateReceivedRemoteCandidates)
				CASE(k_ESNetSocketStateChallengeHandshake)
				CASE(k_ESNetSocketStateDisconnecting)
				CASE(k_ESNetSocketStateLocalDisconnect)
				CASE(k_ESNetSocketStateTimeoutDuringConnect)
				CASE(k_ESNetSocketStateRemoteEndDisconnected)
				CASE(k_ESNetSocketStateConnectionBroken)
			}
			return "Unknown ESNetSocketState ("+String((unsigned)st)+")";
		}

		#undef CASE



		

	HAuthTicket currentTicket = k_HAuthTicketInvalid;
	
	HashTable<UINT64,FOnUserName>	nameCallbacks;
	HashTable<UINT64, FAuthResult>	ticketCallbacks;
	Queue<FAppTicketResponse>				appTicketResponseCallbacks;

	HashTable<UINT64,Buffer0<FOnSessionConnectionFailure> > failureMap;
	IndexTable<FOnSocketStatusChange> socketMonitor;

	Queue<UINT64>		lobbyJoinQueue;
	UINT64				joiningLobby = 0;
	Time::Stamp			lastLobbyJoinAttempt;

	HashSet<UINT64>	joinedLobbies;

	Queue<std::pair<UINT64,Time::Stamp> > checkTicks;
	#endif

	void	RegisterConnectionFailure(UINT64 remoteID,const FOnSessionConnectionFailure&f)
	{
		#ifdef HAS_STEAM_API
			failureMap.Set(remoteID).Append(f);
		#endif
	}

	void	RegisterSocketStatusChange(UINT32 socketID, const FOnSocketStatusChange&f)
	{
		#ifdef HAS_STEAM_API
			socketMonitor.Set(socketID,f);
		#endif
	}

	void	UnregisterSocketStatusChange(UINT32 socketID)
	{
		#ifdef HAS_STEAM_API
			socketMonitor.Unset(socketID);
		#endif
	}

	void InvalidateLastAccessToken()
	{
		#ifdef HAS_STEAM_API
			if (currentTicket !=k_HAuthTicketInvalid)
			{
				LogEvent(EventSignificance::Debug,"InvalidateLastAccessToken: Canceling ticket "+String((unsigned)currentTicket));
				SteamUser()->CancelAuthTicket(currentTicket);
				currentTicket = k_HAuthTicketInvalid;
			}
		#endif
	}


	UINT64 GenerateNewAccessToken(Array<BYTE>&out)
	{
		#ifdef HAS_STEAM_API
			InvalidateLastAccessToken();
			UINT32 actualSize = 0;
			out.SetSize(1024);
			int cnt = 0;
			for(;;)
			{
				currentTicket = SteamUser()->GetAuthSessionTicket(out.pointer(),(int)out.size(),&actualSize);
				if (currentTicket != k_HAuthTicketInvalid)
					break;
				if (++cnt > 10)
					return 0;
				out.SetSize(actualSize);
			}
			out.ResizePreserveContent(actualSize);
			LogEvent(EventSignificance::Debug,"GenerateNewAccessToken: Created ticket "+String((unsigned)currentTicket));
			return SteamUser()->GetSteamID().ConvertToUint64();
		#else
			out.Clear();
			return 0;
		#endif
	}

	
	
	#ifdef HAS_STEAM_API
	class Callback
	{
	public:

		Callback():onName(this,&Callback::OnName)
				,onTicketResponse(this,&Callback::OnTicketResponse)
				/*,onAppTicketResponse(this,&Callback::OnAppTicketResponse)*/
				,onConnectionFailed(this,&Callback::OnConnectionFailed)
				,onSocketStatus(this,&Callback::OnSocketStatus)
				,onLobbyData(this,&Callback::OnLobbyData)
				,onGameLobbyJoinRequested(this,&Callback::OnGameLobbyJoinRequested)
				{}

		STEAM_CALLBACK( Callback, OnName, PersonaStateChange_t, onName );
		STEAM_CALLBACK( Callback, OnTicketResponse, ValidateAuthTicketResponse_t, onTicketResponse );
		//STEAM_CALLBACK( Callback, OnAppTicketResponse, EncryptedAppTicketResponse_t, onAppTicketResponse );
		STEAM_CALLBACK( Callback, OnConnectionFailed,P2PSessionConnectFail_t, onConnectionFailed );
		STEAM_CALLBACK( Callback, OnSocketStatus, SocketStatusCallback_t, onSocketStatus);
		STEAM_CALLBACK( Callback, OnLobbyData, LobbyDataUpdate_t, onLobbyData);
		STEAM_CALLBACK( Callback, OnGameLobbyJoinRequested, GameLobbyJoinRequested_t, onGameLobbyJoinRequested);

		
		
		
		void OnAppTicketResponse( EncryptedAppTicketResponse_t *pResult, bool ioError);
		void OnLobbyMatchList( LobbyMatchList_t *pResult, bool ioError);
		CCallResult<Callback, EncryptedAppTicketResponse_t> callEncryptedAppTicketResponse;
		CCallResult<Callback,LobbyMatchList_t> lobbyMatchList;
		void OnLobbyEnter( LobbyEnter_t *pResult, bool ioError);
		CCallResult<Callback, LobbyEnter_t> callLobbyEnter;
	}	*callback = nullptr;



	void Callback::OnSocketStatus(SocketStatusCallback_t*p)
	{
		FOnSocketStatusChange*f = socketMonitor.QueryPointer(p->m_hSocket);
		if (f)
		{
			ESNetSocketState state = (ESNetSocketState)p->m_eSNetSocketState;

			SocketState s = SocketState::Disconnected;
			switch (state)
			{
				case k_ESNetSocketStateConnected:
					s = SocketState::Connected;
				break;
				case k_ESNetSocketStateInitiated:
				case k_ESNetSocketStateLocalCandidatesFound:
				case k_ESNetSocketStateReceivedRemoteCandidates:
				case k_ESNetSocketStateChallengeHandshake:
					s = SocketState::AttemptingConnection;
				break;
			}
			(*f)(p->m_steamIDRemote.ConvertToUint64(), s,ToString(state));

		}
	}

	void Callback::OnConnectionFailed(P2PSessionConnectFail_t*p)
	{
		Buffer0<FOnSessionConnectionFailure>*list = failureMap.QueryPointer(p->m_steamIDRemote.ConvertToUint64());
		if (list)
		{
			String err = ToString((EP2PSessionError)p->m_eP2PSessionError);
			foreach (*list,h)
				(*h)(err);
			failureMap.Unset(p->m_steamIDRemote.ConvertToUint64());
		}
	}

	void Callback::OnAppTicketResponse(EncryptedAppTicketResponse_t *pParam, bool ioError)
	{
		FAppTicketResponse f;
		if (appTicketResponseCallbacks.Pop(f))
		{

			if (pParam->m_eResult == k_EResultOK && !ioError)
			{
				UINT32 actualSize = 0;
				Array<BYTE> out;
				out.SetSize(1024);
				int cnt = 0;
				for(;;)
				{
					bool success = SteamUser()->GetEncryptedAppTicket(out.pointer(),(int)out.size(),&actualSize);
					if (success)
					{
						break;
					}
					if (++cnt > 10)
					{
						f(0,ArrayRef<BYTE>(),"Max retries reached");
						return;
					}
					out.SetSize(actualSize);
				}
				out.ResizePreserveContent(actualSize);
				f(SteamUser()->GetSteamID().ConvertToUint64(),out,"");
				return;
			}
			f(0,ArrayRef<BYTE>(),ToString(pParam->m_eResult));
		}
		else
			LogEvent(EventClass::Preset::debug,"Received app ticket response ("+ToString(pParam->m_eResult)+"), but no handler is waiting");
	}

	void	ProcessLobbyMatch(const CSteamID&id, const FOnServerMatch&matchCallback)
	{

		LogEvent(EventClass::Preset::debug,"Lobby: "+String(id.Render()));
		UINT8 serverIP[4];
		UINT16 port;
		CSteamID dest;
		SteamMatchmaking()->GetLobbyGameServer(id,(UINT32*)serverIP,&port,&dest);
		if (!dest.BAnonGameServerAccount())
		{
			LogEvent(EventClass::Preset::internalError,"Steam: Server ("+IDToString(dest.ConvertToUint64())+") of lobby "+IDToString(id.ConvertToUint64())+" is not an anonymous game server account");
			return;
		}

		swp(serverIP[0],serverIP[3]);
		swp(serverIP[1],serverIP[2]);
		String ip = String(serverIP[0])+'.'+String(serverIP[1])+'.'+String(serverIP[2])+'.'+String(serverIP[3]);
		//LogEvent(EventClass::Preset::uncommon,ip+":"+String(port)+'/'+dest.Render());
		//LogEvent(EventClass::Preset::uncommon,SteamMatchmaking()->GetLobbyData(id,"stamp"));

		if (matchCallback)
		{
			GetLobbyOwner(id.ConvertToUint64(),[dest,port,id,ip,matchCallback](UINT64 lobbyOwner)
			{
				Protocol::ServerInfo info;
				info.hostInfo.serverBasePort = port;
				const char*version = SteamMatchmaking()->GetLobbyData(id,"version");
				size_t copy = std::min(sizeof(info.hostInfo.version)-1,strlen(version));
				memcpy(info.hostInfo.version,version,copy);
				info.hostInfo.version[copy] = 0;
				const char*strWorldID = SteamMatchmaking()->GetLobbyData(id,"worldID");
				if (info.gameInfo.worldID.Decode(strWorldID))
				{
					info.gameInfo.activePhase = 0;//changes too quickly
					convert(SteamMatchmaking()->GetLobbyData(id,"gameFlags"),info.gameInfo.gameConfig.gameFlags);
					RuntimeConfig::GameGoal goal;
					goal.Decode(SteamMatchmaking()->GetLobbyData(id,"gameGoal"));
					info.gameInfo.gameConfig.gameGoal = (BYTE)goal;
					info.gameInfo.numFactionsCreated = 0;
					info.gameInfo.numFactionsOnline = 0;
					convert(SteamMatchmaking()->GetLobbyData(id,"numFactionsCreated"),info.gameInfo.numFactionsCreated);
					convert(SteamMatchmaking()->GetLobbyData(id,"numFactionsOnline"),info.gameInfo.numFactionsOnline);
					convert(SteamMatchmaking()->GetLobbyData(id,"roundNumber"),info.gameInfo.roundNumber.main);
					matchCallback(id.ConvertToUint64(),dest.ConvertToUint64(),lobbyOwner, ip+':'+String(port),info);
				}
				else
				{
					LogEvent(EventClass::Preset::internalError,"Steam: Unable to decode world ID '"+String(strWorldID)+"', returned by lobby "+IDToString(id.ConvertToUint64()));
				}
			});
		}
	}


	FOnServerMatch	matchCallback;
	FOnServerMatchError matchErrorCallback;

	void Callback::OnLobbyMatchList( LobbyMatchList_t *pResult, bool ioError)
	{
		if (!ioError)
		{
			const UINT32 cnt = pResult->m_nLobbiesMatching;
			LogEvent(EventClass::Preset::common,"Found "+String(cnt)+" lobby match(es)");
			for (UINT32 i = 0; i < cnt; i++)
			{
				CSteamID id = SteamMatchmaking()->GetLobbyByIndex(i);
				ProcessLobbyMatch(id,matchCallback);

			}

		}
		else
			LogEvent(EventClass::Preset::networkError,"IO error while fetching lobby match list");

		matchCallback = nullptr;
		matchErrorCallback = nullptr;
	}


	struct LobbyOwnerMapEntry
	{
		UINT64		ownerID;
		Time::Stamp	recordedAt;

		/**/		LobbyOwnerMapEntry(UINT64 ownerID=0):ownerID(ownerID)	{}
	};
	HashTable<UINT64,LobbyOwnerMapEntry>	lobbyOwnerMap;
	HashTable<UINT64,FLobbyCallback> lobbyCallbacks;


	void	Join(UINT64 lobbyID)
	{
		joiningLobby = lobbyID;
		lastLobbyJoinAttempt.Update();

		SteamAPICall_t rc = SteamMatchmaking()->JoinLobby(CSteamID(lobbyID));
		callback->callLobbyEnter.Set( rc, callback, &Callback::OnLobbyEnter );
		LogEvent(EventClass::Preset::debug,"Steam: Attempting to join lobby: "+IDToString(lobbyID));
	}

	void	JoinNextLobby()
	{
		UINT64 nextLobby;
		if (lobbyJoinQueue.Pop(nextLobby))
		{
			Join(nextLobby);
		}
		else
			joiningLobby = 0;

	}



	void Callback::OnLobbyEnter(LobbyEnter_t*p, bool ioError)
	{
		JoinNextLobby();

		FLobbyCallback f;
		if (ioError)
		{
			//lobbyOwnerMap.Set(p->m_ulSteamIDLobby,0); //retry later?
			LogEvent(EventClass::Preset::networkError,"Steam: Trying to join lobby "+String(CSteamID( p->m_ulSteamIDLobby ).Render())+" has failed (io error)");
			return;
		}
		EChatRoomEnterResponse resp = (EChatRoomEnterResponse)p->m_EChatRoomEnterResponse;
		if (resp != k_EChatRoomEnterResponseSuccess)
		{
			LogEvent(EventClass::Preset::networkError,"Steam: Trying to join lobby "+String(CSteamID( p->m_ulSteamIDLobby ).Render())+" has failed ("+ToString(resp)+")");
			if (resp == k_EChatRoomEnterResponseFull || resp == k_EChatRoomEnterResponseYouBlockedMember)
			{
				//GetLobbyOwner(p->m_ulSteamIDLobby,f); //retry later? - no point hammering
				return;
			}
			lobbyOwnerMap.Set(p->m_ulSteamIDLobby,0);
			return;
		}
	
		UINT64 lobbyOwner = SteamMatchmaking()->GetLobbyOwner(p->m_ulSteamIDLobby).ConvertToUint64();

		SteamMatchmaking()->LeaveLobby(p->m_ulSteamIDLobby);
		if (lobbyOwner == GetUserID())
			FATAL__("Joined own lobby");

		LogEvent(EventClass::Preset::network,"Retrieved owner "+String(CSteamID( lobbyOwner ).Render())+ " of lobby "+String(CSteamID(p->m_ulSteamIDLobby).Render()));

		lobbyOwnerMap.Set(p->m_ulSteamIDLobby,lobbyOwner);
		if (!lobbyCallbacks.QueryAndUnset(p->m_ulSteamIDLobby,f))
			return;
		f(lobbyOwner);
	}

	



	void	RegisterLeftLobby(UINT64 id)
	{
		joinedLobbies.Unset(id);
		LogEvent(EventClass::Preset::network,"Steam: Left lobby "+String(CSteamID(id).Render()));
	}


	void	RegisterJoinedLobby(UINT64 id)
	{
		joinedLobbies.Set(id);
		LogEvent(EventClass::Preset::network,"Steam: Joined lobby "+String(CSteamID(id).Render()));
	}




	void	GetLobbyOwner(UINT64 lobbyID,const FLobbyCallback&onResult)
	{
		LobbyOwnerMapEntry rs;
		if (lobbyOwnerMap.Query(lobbyID,rs))
		{
			if (rs.recordedAt.ElapsedSeconds() < 120)	//let's go with 2 minutes
			{
				onResult(rs.ownerID);
				return;
			}
			lobbyOwnerMap.Unset(lobbyID);
		}
		if (lobbyCallbacks.IsSet(lobbyID))
		{
			LogEvent(EventClass::Preset::debug,"Steam: Already attempting to query owner of "+IDToString(lobbyID)+". Ignoring request");
			return;	//ignore
		}
		CSteamID lobbyOwner = SteamMatchmaking()->GetLobbyOwner(lobbyID);
		if (lobbyOwner.IsValid())
		{
			lobbyOwnerMap.Set(lobbyID,lobbyOwner.ConvertToUint64());
			onResult(lobbyOwner.ConvertToUint64());
			return;
		}

		lobbyCallbacks.Set(lobbyID,onResult);

		if (joiningLobby)
		{
			LogEvent(EventClass::Preset::debug,"Steam: Queueing up "+IDToString(lobbyID)+" in order to determine owner");
			lobbyJoinQueue.Push(lobbyID);
			return;
		}
		Join(lobbyID);
	}


	class ServerListSink : public ISteamMatchmakingServerListResponse
	{
		// Server has responded ok with updated data
		virtual void ServerResponded( HServerListRequest hRequest, int iServer ) override
		{
			gameserveritem_t*details = SteamMatchmakingServers()->GetServerDetails(hRequest,iServer);

			LogEvent(EventSignificance::Uncommon , "Server list entry: "+ String(details->m_NetAdr.GetQueryAddressString()));
		}

	// Server has failed to respond
		virtual void ServerFailedToRespond( HServerListRequest hRequest, int iServer ) override
		{

		}

	// A list refresh you had initiated is now 100% completed
		virtual void RefreshComplete( HServerListRequest hRequest, EMatchMakingServerResponse response ) override
		{
			SteamMatchmakingServers()->ReleaseRequest(hRequest);
			LogEvent(EventSignificance::Uncommon , "Server list reached end");
		}

	}	serverListSink;
	#else


	
	bool	IsInLobby()
	{
		return false;
	}
	UINT64	GetCurrentLobbyID()
	{
		return 0;
	}

	void	LeaveLobby()
	{
	}
	void	JoinLobby(UINT64 lobbyID)
	{
	}

	#endif

	bool IsLocalUser(UINT64 id)
	{
		#ifdef HAS_STEAM_API
			return (SteamUser() && SteamUser()->GetSteamID() == id);
		#endif
		return false;
	}

	bool IsFriend(UINT64 id)
	{
		#ifdef HAS_STEAM_API
			if (SteamUser() && SteamUser()->GetSteamID() == id)
				return true;
			if (SteamFriends())
				return SteamFriends()->HasFriend(id,k_EFriendFlagImmediate|k_EFriendFlagClanMember);
		#endif
		return false;
	}

	void RequestNewCryptographicAccessToken(const FAppTicketResponse&f)
	{
		#ifdef HAS_STEAM_API
			UINT64 id = SteamUser()->GetSteamID().ConvertToUint64();
			SteamAPICall_t rc = SteamUser()->RequestEncryptedAppTicket(&id,8);
			appTicketResponseCallbacks << f;
			callback->callEncryptedAppTicketResponse.Set( rc, callback, &Callback::OnAppTicketResponse );
		#else
			f(0,ArrayRef<BYTE>(),"Steam API not available");
		#endif
	}

	Buffer0<UINT64>	externalLobbies;
	FOnServerMatch	externalCallback;

	bool LobbyIsRegistered(UINT64 id)
	{
		return externalLobbies.Contains(id);
	}

	void	RegisterExternalLobby(UINT64 lobbyID)
	{
		if (!externalLobbies.Contains(lobbyID))
			externalLobbies << lobbyID;
	}

	void	UnregisterExternalLobby(UINT64 lobbyID)
	{
		externalLobbies.FindAndErase(lobbyID);
	}

	#ifdef HAS_STEAM_API
		void Callback::OnLobbyData(LobbyDataUpdate_t*p)
		{
			if (!externalLobbies.Contains(p->m_ulSteamIDLobby))
				return;
			bool remove = true;
			String error = "";
			ServerMatchError errorEnum;
			if (p->m_bSuccess)
			{
				UINT32 t = SteamUtils()->GetServerRealTime();
				const char*stamp = SteamMatchmaking()->GetLobbyData(CSteamID(p->m_ulSteamIDLobby),"stamp");
				const char*version = SteamMatchmaking()->GetLobbyData(CSteamID(p->m_ulSteamIDLobby),"version");
				UINT32 t2;
				if (stamp && convert(stamp,t2))
				{
					if (t2 >= t-60)
					{
						if (version && !strcmp(version,SERVER_NAME))
						{
							ProcessLobbyMatch(p->m_ulSteamIDLobby,externalCallback);
							remove = false;
						}
						else
						{
							error = String(version);
							errorEnum = ServerMatchError::VersionMismatch;
						}
					}
					else
					{
						error = String(t-t2);
						errorEnum = ServerMatchError::StampOutdated;
					}
				}
				else
				{
					error = "Failed to parse timestamp";
					errorEnum = ServerMatchError::DataError;
				}
			}
			else
			{
				error = "Unable to fetch data";
				errorEnum = ServerMatchError::DataError;
			}

			if (remove)
			{
				externalLobbies.FindAndErase(p->m_ulSteamIDLobby);
				if (matchErrorCallback)
					matchErrorCallback(p->m_ulSteamIDLobby,error,errorEnum);
			}
		}
	#endif


	void	AdvertiseLobby(UINT64 lobbyID)
	{
		#ifdef HAS_STEAM_API
			if (SteamFriends())
			{
				if (lobbyID)
					SteamFriends()->SetRichPresence("connect",("+connect_lobby " + String(lobbyID)).c_str());
				else
					SteamFriends()->SetRichPresence("connect",nullptr);
			}

		#endif
	}


	void ProbeServers(const FOnServerMatch&onMatch, const FOnServerMatchError&onError)
	{
		#ifdef HAS_STEAM_API
			if (matchCallback)
				return;
			externalCallback = matchCallback = onMatch;
			matchErrorCallback = onError;
			UINT32 t = SteamUtils()->GetServerRealTime();
			#ifndef _DEBUG
				SteamMatchmaking()->AddRequestLobbyListNumericalFilter("stamp",t-60,k_ELobbyComparisonEqualToOrGreaterThan);
				SteamMatchmaking()->AddRequestLobbyListStringFilter("version",SERVER_NAME,k_ELobbyComparisonEqual);
			#endif
			SteamMatchmaking()->AddRequestLobbyListNearValueFilter("numFactionsOnline",2);
			SteamAPICall_t rc = SteamMatchmaking()->RequestLobbyList();
			callback->lobbyMatchList.Set( rc, callback, &Callback::OnLobbyMatchList );

			count_t dispatchedRequests = 0;
			Buffer0<UINT64> drop;
			foreach (externalLobbies,l)
				if (SteamMatchmaking()->RequestLobbyData(CSteamID(*l)))
					dispatchedRequests ++;
				else
				{
					LogEvent(EventSignificance::NonFatalError,"Failed to query data about Steam lobby "+String(*l));
					drop.Append(*l);
				}
			foreach (drop,l)
			{
				UnregisterExternalLobby(*l);
				if (onInvalidCallback)
					onInvalidCallback(*l);
			}
			

			if (dispatchedRequests)
				LogEvent(EventSignificance::Common,"Dispatched "+String(dispatchedRequests)+" external lobby data request(s)");

			//MatchMakingKeyValuePair_t *pairs[1];
			////pairs[0] = new MatchMakingKeyValuePair_t("gamedataand","123445489153871");
			//LogEvent(EventSignificance::Uncommon,"Starting steam server search for app ID "+String(SteamUtils()->GetAppID()));
			//SteamMatchmakingServers()->RequestInternetServerList(SteamUtils()->GetAppID(),pairs,0,&serverListSink);

			//SteamMatchmakingServers()->RequestLANServerList(468350,&serverListSink);
		#endif


	}

	void	Destroy()
	{
		#ifdef HAS_STEAM_API
			LogEvent(EventSignificance::Important,"Shutting down Steam API");
			SteamNetwork::HandleSessionClosure();
			InvalidateLastAccessToken();
			if (SteamMatchmaking())
			{
				if (joiningLobby)
					SteamMatchmaking()->LeaveLobby(joiningLobby);
				joiningLobby = 0;
				joinedLobbies.VisitAllKeys([](UINT64 key)
				{
					SteamMatchmaking()->LeaveLobby(key);
				});
			}
			joinedLobbies.Clear();
			lobbyCallbacks.Clear();

			delete callback;
			callback = nullptr;

			SteamGameServer_Shutdown();
			SteamAPI_Shutdown();
		#endif
	}

	void	Resolve()
	{
		#ifdef HAS_STEAM_API
			while (checkTicks.IsNotEmpty() && checkTicks.Peek().second.ElapsedSeconds() > 5.f)
			{
				FAuthResult f;
				UINT64 id = checkTicks.Pop().first;
				if (ticketCallbacks.QueryAndUnset(id,f))
				{
					LogEvent(EventClass::Preset::debug,"Closing remaining open ticked response for "+String(id));
					f(false,"k_EAuthSessionResponseAuthTicketCanceled");
				}
				else
					LogEvent(EventClass::Preset::debug,"Closing remaining open ticked response for "+String(id)+" (no handler set)");
			}

			SteamAPI_RunCallbacks();


			if (joiningLobby && lastLobbyJoinAttempt.ElapsedSeconds() > 5)
			{
				LogEvent(EventClass::Preset::debug,"Steam: Failed to join lobby in time: "+IDToString(joiningLobby));
				lobbyCallbacks.Unset(joiningLobby);
				JoinNextLobby();
			}

		#endif
	}



	#ifdef HAS_STEAM_API
	

		
		void Callback::OnName(PersonaStateChange_t *pParam)
		{
			//LogEvent(EventClass::Preset::uncommon,"Name response: "+String(pParam->m_nChangeFlags)+" for "+ String(pParam->m_ulSteamID));
			if (pParam->m_nChangeFlags & (k_EPersonaChangeName | k_EPersonaChangeNickname))
			{
				FOnUserName f;
				if (nameCallbacks.QueryAndUnset(pParam->m_ulSteamID,f))
					FetchUsername(pParam->m_ulSteamID,f);
				else
					LogEvent(EventClass::Preset::debug,"Name response lost: no handler");
			}
			//else
				//LogEvent(EventClass::Preset::uncommon,"...ignoring");
			
		}


		void Callback::OnTicketResponse(ValidateAuthTicketResponse_t *pParam)
		{
			FAuthResult f;
			LogEvent(EventClass::Preset::debug,"Ticket response: "+ToString(pParam->m_eAuthSessionResponse)+" for "+ String(pParam->m_SteamID.ConvertToUint64())+ "/"+String(pParam->m_OwnerSteamID.ConvertToUint64()));
			if (pParam->m_eAuthSessionResponse == k_EAuthSessionResponseAuthTicketCanceled)
			{
				checkTicks.Push(std::make_pair(pParam->m_SteamID.ConvertToUint64(),Time::Stamp()));
				LogEvent(EventClass::Preset::debug,"...ignoring");
				return;
			}
			if (ticketCallbacks.QueryAndUnset(pParam->m_SteamID.ConvertToUint64(),f))
			{
				f(	   pParam->m_eAuthSessionResponse == k_EAuthSessionResponseOK 
					|| pParam->m_eAuthSessionResponse == k_EAuthSessionResponseVACBanned
					|| pParam->m_eAuthSessionResponse == k_EAuthSessionResponseNoLicenseOrExpired	//this seems to happen from time to time. Let's ignore this for now
					,ToString(pParam->m_eAuthSessionResponse));
			}
			else
				LogEvent(EventClass::Preset::debug,"Ticket response lost: no handler");
		}
	#endif

	void	Authenticate(UINT64 steamID, const ArrayRef<BYTE>&ticket, const String*cryptographicSecret, const FAuthResult&onResult)
	{
		#ifdef HAS_STEAM_API
			if (cryptographicSecret)
			{
				const String key= *cryptographicSecret;
				Array<BYTE> bkey(key.Length()/2);
				Array<BYTE> decrypted(ticket.Length()*2);
				UINT32 len;
				Bin2Str::Convert(bkey.pointer(),bkey.Length(),key);
				bool good = false;
				for (int i = 0; i < 10; i++)
				{
					len = (UINT32)decrypted.Length();
					if (SteamEncryptedAppTicket_BDecryptTicket(ticket.pointer(),(UINT32)ticket.GetLength(),
					decrypted.pointer(),&len,bkey.pointer(),(int)bkey.Length()))
					{
						//decrypted.resizePreserveContent(len);
						good = true;
						break;
					}
					decrypted.SetSize(len);
				}
				bkey.Clear();

				if (!good)
				{
					onResult(false,"Ticket decryption failed");
					return;
				}
				if (!SteamEncryptedAppTicket_BIsTicketForApp(decrypted.pointer(),len,SteamUtils()->GetAppID()))
				{
					onResult(false,"This ticket is for App# "+String(SteamEncryptedAppTicket_GetTicketAppID(decrypted.pointer(),len))+", expected App# "+String(SteamUtils()->GetAppID())+")");
					return;
				}
				time_t now = time(nullptr);
				INT64 delta = (INT64)now - (INT64)SteamEncryptedAppTicket_GetTicketIssueTime(decrypted.pointer(),len);
				if (abs(delta) > 180)
				{
					onResult(false,"This ticket is out of time");
					return;
				}
				CSteamID id;
				SteamEncryptedAppTicket_GetTicketSteamID (decrypted.pointer(),len,&id);

				if (id.ConvertToUint64() != steamID)
				{
					onResult(false,"This ticket is for someone else");
					return;
				}
				decrypted.Clear();
 				{
					onResult(true,"");
					return;
				}
 
 				return;
			}
			EBeginAuthSessionResult rs = SteamUser() ?
											SteamUser()->BeginAuthSession(ticket.pointer(),(int)ticket.Length(),CSteamID(steamID))
											: 
											(SteamGameServer()?
												SteamGameServer()->BeginAuthSession(ticket.pointer(),(int)ticket.Length(),CSteamID(steamID))
												:k_EBeginAuthSessionResultGameMismatch
												);
			if (rs == k_EBeginAuthSessionResultOK)
			{
				LogEvent(EventSignificance::Debug,"Authenticating "+String(steamID)+"...");
				ticketCallbacks.Set(steamID,onResult);
				//if (onUserName)
					//FetchUsername(steamID,onUserName);
				return;
			}
			onResult(false,ToString(rs));
		#else
			onResult(false,"Steam API not available in this build");
		#endif
	}

	void	SignalConnectionLoss(UINT64 clientSteamAccountID)
	{
		#ifdef HAS_STEAM_API
		if(SteamUser())
			SteamUser()->EndAuthSession(CSteamID(clientSteamAccountID));
		else if (SteamGameServer())
			SteamGameServer()->EndAuthSession(CSteamID(clientSteamAccountID));
		#endif
	}


	//-----------------------------------------------------------------------------
	// Purpose: callback hook for debug text emitted from the Steam API
	//-----------------------------------------------------------------------------
	extern "C" void __cdecl SteamAPIDebugTextHook( int nSeverity, const char *pchDebugText )
	{
		// if you're running in the debugger, only warnings (nSeverity >= 1) will be sent
		// if you add -debug_steamapi to the command-line, a lot of extra informational messages will also be sent
		::OutputDebugStringA( pchDebugText );
		
		if ( nSeverity >= 1 )
		{
			// place to set a breakpoint for catching API errors
			int x = 3;
			x = x;
			if (strcmp(pchDebugText,"[S_API WARN] CancelAuthTicket called on nonexistent ticket"))
				LogEvent(EventClass::Preset::error,pchDebugText);
			else
				LogEvent(EventSignificance::Debug,pchDebugText);

		}
		else
			LogEvent(EventSignificance::Debug,pchDebugText);
	}

	void SignalWindowResize(const Resolution&newRes)
	{
		#ifdef HAS_STEAM_API
			if (SteamUtils())
				SteamUtils()->SetOverlayNotificationInset(0,newRes.height/10);
		#endif
	}

	bool Create( UINT32 appID , FOnRegisteredLobbyInvalid onInvalidCallback )
	{
		SteamInterface::onInvalidCallback = onInvalidCallback;
		#ifdef HAS_STEAM_API
			SetEnvironmentVariableA("SteamAppId",String(appID).c_str());
		if ( SteamAPI_RestartAppIfNecessary( /*k_uAppIdInvalid*/appID ) )
		{
			// if Steam is not running or the game wasn't started through Steam, SteamAPI_RestartAppIfNecessary starts the 
			// local Steam client and also launches this game again.
			
			// Once you get a public Steam AppID assigned for this game, you need to replace k_uAppIdInvalid with it and
			// removed steam_appid.txt from the game depot.
			LogEvent(EventClass:: Preset::uncommon,"Steam requests application restart");
			return false;
		}
		
		//#ifdef CLIENT

			// Initialize SteamAPI, if this fails we bail out since we depend on Steam for lots of stuff.
			// You don't necessarily have to though if you write your code to check whether all the Steam
			// interfaces are NULL before using them and provide alternate paths when they are unavailable.

			//
			// This will also load the in-game steam overlay dll into your process.  That dll is normally
			// injected by steam when it launches games, but by calling this you cause it to always load,
			// even when not launched via steam.
			if ( !SteamAPI_Init() )
			{
				LogEvent(EventClass::Preset::fatal,"Steam must be running to play this game (SteamAPI_Init() failed).");
				ErrMessage( "Steam must be running to play this game (SteamAPI_Init() failed)." );
			//	Alert( "Fatal Error", "Steam must be running to play this game (SteamAPI_Init() failed).\n" );
				return false;
			}
		/*#else
			if (!SteamGameServer_Init( uint32 unIP, uint16 usSteamPort, uint16 usGamePort, uint16 usQueryPort, EServerMode eServerMode, const char *pchVersionString );
			{
				FATAL__( "SteamGameServer_Init() failed" );
			}
		#endif*/
			SteamUtils()->SetOverlayNotificationPosition(k_EPositionBottomRight);
		// set our debug handler
		SteamClient()->SetWarningMessageHook( &SteamAPIDebugTextHook );
		
		callback = new Callback();
		// Ensure that the user has logged into Steam. This will always return true if the game is launched
		// from Steam, but if Steam is at the login prompt when you run your game from the debugger, it
		// will return false.
		//if ( !SteamUser()->BLoggedOn() )
		//{
		//	OutputDebugStringA( "Steam user is not logged in\n" );
		//	//Alert( "Fatal Error", "Steam user must be logged in to play this game (SteamUser()->BLoggedOn() returned false).\n" );
		//	return false;
		//}
			GlobalEvents::applicationTerminate << SteamInterface::Destroy;
		#endif	
		return true;
	}

	
	bool CreateServer(UINT32 appID, UINT16 basePort)
	{
		if (!Create(appID, nullptr))	//can work without but will not have online session authentication, and require that other authentication mechanism
			return false;


		return true;
	}


	UINT64 GetUserID()
	{
		#ifdef HAS_STEAM_API
			if (SteamUser ())
				return SteamUser()->GetSteamID().ConvertToUint64();
		#endif
		return 0;
	}

	
	String GetUserNickName()
	{
		#ifdef HAS_STEAM_API
			if (!SteamFriends()->RequestUserInformation( SteamUser()->GetSteamID(), true ) )
			{
				return SteamFriends()->GetFriendPersonaName( SteamUser()->GetSteamID() );
			}
		#endif
		throw Except::Program::UnsupportedRequest("Steam user name not (yet) available");
	}


	bool	TryGetUsernameNow(UINT64 steamID, String&outName)
	{
		#ifdef HAS_STEAM_API
			CSteamID id(steamID);
			if (!SteamFriends()->RequestUserInformation( id, true ) )
			{
				outName = SteamFriends()->GetFriendPersonaName( id );
				return true;
			}
		#endif
		return false;

	}


	void	FetchUsername(UINT64 steamID,const FOnUserName&onName)
	{
		#ifdef HAS_STEAM_API
			CSteamID id(steamID);
			if (!SteamFriends()->RequestUserInformation( id, true ) )
			{
				onName(steamID, SteamFriends()->GetFriendPersonaName( id ));
			}
			else
				nameCallbacks.Set(steamID,onName);

		#endif
	}

	#ifdef HAS_STEAM_API
	
		void Callback::OnGameLobbyJoinRequested(GameLobbyJoinRequested_t*p)
		{
			#ifdef CLIENT
				String name;
				if (TryGetUsernameNow(p->m_steamIDFriend.ConvertToUint64(),name))
					LogEvent(EventClass::Preset::important,"Received game lobby invite for lobby "+String(p->m_steamIDLobby.Render())+" (by "+String(p->m_steamIDFriend.Render())+ " '"+name+ "')");
				else
					LogEvent(EventClass::Preset::important,"Received game lobby invite for lobby "+String(p->m_steamIDLobby.Render())+" (by "+String(p->m_steamIDFriend.Render())+ ")");
				Network::ConnectToLobby(p->m_steamIDLobby.ConvertToUint64());
			#endif
		}

	#endif

	void	InviteFriend(UINT64 lobbyID)
	{
		#ifdef HAS_STEAM_API
			if (SteamFriends() && lobbyID)
				SteamFriends()->ActivateGameOverlayInviteDialog(CSteamID(lobbyID));
		#endif
	}




	String	IDToString(UINT64 steamID)
	{
		#ifdef HAS_STEAM_API
			CSteamID id(steamID);
			if (!id.IsValid())
				return "Invalid";
			return id.Render();
		#else
			return String(steamID);
		#endif


	}

}

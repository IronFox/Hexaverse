#ifndef componentSteamH
#define componentSteamH

#include <global_string.h>
#include <functional>
#include <math/resolution.h>
#include "../protocol.h"


#if defined HAS_WINDOW || !defined CLIENT
	#define HAS_STEAM_API
#endif

namespace SteamInterface
{
	using namespace DeltaWorks;

	typedef std::function<void(UINT64)>	FOnRegisteredLobbyInvalid;

	bool	Create(UINT32 appID, FOnRegisteredLobbyInvalid onInvalidCallback);
	bool	CreateServer(UINT32 appID, UINT16 basePort);

	enum class SocketState
	{
		Disconnected,
		AttemptingConnection,
		Connected,
	};

	enum class ServerMatchError
	{
		DataError,
		StampOutdated,
		VersionMismatch
	};
	
	typedef std::function<void(bool, const String&)>	FAuthResult;
	typedef std::function<void(UINT64, const String&)>	FOnUserName;
	typedef std::function<void(UINT64, const ArrayRef<BYTE>&,const String&)> FAppTicketResponse;
	/**
	Lobby+server match callback.
	Since only the owner can set the game server, and the owner is guaranteed by steam, further verification appears unnecessary.

	@param lobbyID Steam ID of the retrieved lobby. Guaranteed correct by steam
	@param serverID Steam ID of the server associated with the lobby, if any. Apparently guaranteed by steam, to be set by the owner via SetLobbyGameServer()
	@param ownerID Steam ID of the lobby owning account. Guaranteed correct through connection with the lobby, however possibly outdated due to caching. Added two minutes cache time
	
	*/
	typedef std::function<void(UINT64 lobbyID, UINT64 serverID, UINT64 ownerID,const String&addr, const Protocol::ServerInfo &info)> FOnServerMatch;
	typedef std::function<void(UINT64 lobbyID, const String&error, ServerMatchError)> FOnServerMatchError;
	typedef std::function<void(const String&)> FOnSessionConnectionFailure;
	typedef std::function<void(UINT64, SocketState,const String&)> FOnSocketStatusChange;

	UINT64	GetUserID();
	String	GetUserNickName();
	UINT64	GenerateNewAccessToken(Array<BYTE>&out);
	void	InvalidateLastAccessToken();
	void	RequestNewCryptographicAccessToken(const FAppTicketResponse&f);

	void	Authenticate(UINT64 steamID, const ArrayRef<BYTE>&ticket, const String*cryptographicSecret, const FAuthResult&onResult);
	void	SignalConnectionLoss(UINT64 clientSteamAccountID);
	void	FetchUsername(UINT64 steamID,const FOnUserName&onName);
	bool	TryGetUsernameNow(UINT64 steamID, String&outName);
	void	Resolve();

	bool	IsLocalUser(UINT64 steamID);
	bool	IsFriend(UINT64 steamID);
	String	IDToString(UINT64 steamID);
	void	RegisterConnectionFailure(UINT64 remoteID,const FOnSessionConnectionFailure&);
	void	RegisterSocketStatusChange(UINT32 socketID, const FOnSocketStatusChange&);
	void	UnregisterSocketStatusChange(UINT32 socketID);

	typedef std::function<void(UINT64)> FLobbyCallback;

	void	GetLobbyOwner(UINT64 lobbyID,const FLobbyCallback&);


	/**
	Scans for matching lobbies to be used as servers. Asynchronous.
	*/
	void	ProbeServers(const FOnServerMatch&, const FOnServerMatchError&);



	//callback functions only necessary when directly manipulating the Steam API without using local functions
	void	RegisterJoinedLobby(UINT64 lobbyID);
	void	RegisterLeftLobby(UINT64 lobbyID);


	/**
	Updates user rich presence to allow others to join the local server.
	@param lobbyID Lobby to advertise to friends. Set 0 to disable advertisement
	*/
	void	AdvertiseLobby(UINT64 lobbyID);

	/**
	Begins friend invitation process
	*/
	void	InviteFriend(UINT64 lobbyID);

	/**
	Registers a new externally known lobby to be considered during the next call to ProbeServers()
	*/
	void	RegisterExternalLobby(UINT64 lobbyID);
	void	UnregisterExternalLobby(UINT64 lobbyID);
	bool	LobbyIsRegistered(UINT64 id);
	void	SignalWindowResize(const Resolution&newRes);
	
}



#endif

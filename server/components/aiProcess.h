#ifndef aiProcessH
#define aiProcessH

#include <uniqueID.h>
#include <functional>

/**
Manages the Console Client processes that run Lua scripted AIs.
There is at most one process per faction access token: one for the Nexus faction,
plus one per AI player faction.
*/
namespace AIProcess
{
	/**
	Starts the Nexus AI client, unless it is already running
	*/
	void			Begin(const CryptographicID&accessToken, USHORT serverBasePort, const std::function<String()>&asyncGetServerPassword);

	/**
	Starts an AI player client for the given access token, unless one is already running for it.
	@param accessToken Token the client logs in with. For a faction that does not exist yet, this is a token reserved via Game::ReserveAIPlayerToken()
	@param asyncGetExtraParameters Called (asynchronously) each time the process is (re)started. Returns additional client parameters, e.g. aiPlayerName='AI 1' aiNewFaction=true
	*/
	void			BeginPlayer(const CryptographicID&accessToken, USHORT serverBasePort, const std::function<String()>&asyncGetServerPassword, const std::function<String()>&asyncGetExtraParameters);

	/**
	Terminates the process associated with the given token, if any, and stops restarting it
	*/
	void			EndOne(const CryptographicID&accessToken);

	/**
	Terminates all AI processes (Nexus and players)
	*/
	void			End();

	/**
	Terminates all AI player processes, but keeps the Nexus AI running
	*/
	void			EndPlayers();
}





#endif

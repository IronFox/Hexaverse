#ifndef runtimeConfigComponentH
#define runtimeConfigComponentH

#include <io/file_system.h>

#include <uniqueID.h>
#include <runtimeConfig.h>
#include <general/timer.h>

namespace Network
{
	class ServerAddress;

}

namespace RuntimeConfig
{
	extern bool aiMode;
	extern CryptographicID aiAccessToken;
	/**
	Set (non-empty) if this AI client plays a regular faction (server-launched AI player) rather than the Nexus faction.
	Used as faction name
	*/
	extern String aiPlayerName;
	/**
	True if this AI player client should create its faction (first launch) rather than continue an existing one
	*/
	extern bool aiNewFaction;
	inline bool IsAIPlayer() {return aiMode && aiPlayerName.IsNotEmpty();}
	extern String serverPassword;
	extern bool udpListen;
	extern Time::Stamp				programStart;

	extern Network::ServerAddress	serverAddress;

	extern String					profilePrefix, profileName;

	extern bool						nexusBases,	nexusPortsOnAllPoles;

	void							ClientInit();

	PathString						GetUserNameFile();

	namespace FromFile
	{
		extern int		capFPS;
		extern bool		alwaysDay;
	}

}



#endif

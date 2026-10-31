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
	extern String serverPassword;
	//extern bool udpListen;
	extern Time::Stamp				programStart;

	extern Network::ServerAddress	serverAddress;

	extern UINT64					connectToSteamLobby;

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

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
	extern bool udpListen;
	extern Timer::Time				programStart;

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

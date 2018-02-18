#ifndef runtimeConfigComponentH
#define runtimeConfigComponentH

#include <io/file_system.h>

#include <uniqueID.h>
#include <runtimeConfig.h>

namespace Network
{
	class ServerAddress;

}

namespace RuntimeConfig
{
	extern bool aiMode;
	extern CryptographicID aiAccessToken;
	extern bool udpListen;

	extern Network::ServerAddress	serverAddress;
	extern FileSystem::File			documentationRootFolder;

	extern String					profilePrefix;

	extern bool						nexusBases,	nexusPortsOnAllPoles;

	void					ClientInit();


	namespace FromFile
	{
		extern int		capFPS;
		extern bool		alwaysDay;
	}

}



#endif

#ifndef runtimeConfigComponentH
#define runtimeConfigComponentH

#include <io/file_system.h>

#include "../../shared/uniqueID.h"
#include "../../shared/runtimeConfig.h"

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
	extern FileSystem::File			documentationTargetFolder,imageTargetFolder;

	void					ClientInit();


	namespace FromFile
	{
		extern int		capFPS;
		extern bool		alwaysDay;
	}

}



#endif

#include <global_root.h>
#include "clientRuntimeConfig.h"
#include "network.h"



namespace RuntimeConfig
{
	bool aiMode = false;
	CryptographicID aiAccessToken;
	bool udpListen = true;
	bool nexusBases=false,	nexusPortsOnAllPoles=false;

	Network::ServerAddress	serverAddress;
	FileSystem::File		documentationRootFolder;

	String					profilePrefix;


	void	ClientInit()
	{
		InitConfigDir(aiMode ? "client/ai" : "client");


		phase = Phase::Uninitialized;
	}




	namespace FromFile
	{
		int		capFPS = -1;
		bool	alwaysDay = false;
	}

}
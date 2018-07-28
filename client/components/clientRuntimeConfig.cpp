#include <global_root.h>
#include "clientRuntimeConfig.h"
#include "network.h"



namespace RuntimeConfig
{
	bool aiMode = false;
	CryptographicID aiAccessToken;
	String serverPassword;
	bool udpListen = true;
	bool nexusBases=false,	nexusPortsOnAllPoles=false;

	Network::ServerAddress	serverAddress;

	String					profilePrefix,profileName;


	void	ClientInit()
	{
		myConfig.InitConfigDir(aiMode ? "aiClient" : "client");
		sharedConfig.InitConfigDir("shared");


		phase = Phase::Uninitialized;
	}


	PathString				GetUserNameFile()
	{
		return RuntimeConfig::myConfig.Lookup(RuntimeConfig::profilePrefix+ "userName.txt");
	}




	namespace FromFile
	{
		int		capFPS = -1;
		bool	alwaysDay = false;
	}

}
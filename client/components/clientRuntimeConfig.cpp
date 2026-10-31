#include <global_root.h>
#include "clientRuntimeConfig.h"
#include "network.h"



namespace RuntimeConfig
{
	bool aiMode = false;
	CryptographicID aiAccessToken;
	String serverPassword;
//	bool udpListen = true;
	bool nexusBases=false,	nexusPortsOnAllPoles=false;
	Time::Stamp				programStart;

	Network::ServerAddress	serverAddress;
	UINT64					connectToSteamLobby = 0;


	void	ClientInit()
	{
		myConfig.InitConfigDir(aiMode ? "aiClient" : "client");
		sharedConfig.InitConfigDir("shared");


		phase = Phase::Uninitialized;
	}


	PathString				GetUserNameFile()
	{
		return RuntimeConfig::myConfig.Lookup("userName.txt");
	}




	namespace FromFile
	{
		int		capFPS = -1;
		bool	alwaysDay = false;
	}

}
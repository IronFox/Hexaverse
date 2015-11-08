#include <global_root.h>
#include "clientRuntimeConfig.h"
#include "network.h"



namespace RuntimeConfig
{
	bool aiMode = false;
	CryptographicID aiAccessToken;
	String serverURL;
	bool udpListen = true;


	void	InitConfigDir()
	{
		InitConfigDir(aiMode ? "client/ai" : "client/player");
	}




	namespace FromFile
	{
		int		capFPS = -1;
		bool	alwaysDay = false;
	}

}
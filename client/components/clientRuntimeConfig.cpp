#include <global_root.h>
#include "clientRuntimeConfig.h"
#include "network.h"



namespace RuntimeConfig
{
	bool aiMode = false;
	CryptographicID aiAccessToken;
	String serverURL;
	bool udpListen = true;


	void	ClientInit()
	{
		InitConfigDir(aiMode ? "client/ai" : "client/player");


		phase = Phase::Uninitialized;
	}




	namespace FromFile
	{
		int		capFPS = -1;
		bool	alwaysDay = false;
	}

}
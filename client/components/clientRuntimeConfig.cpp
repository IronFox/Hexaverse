#include <global_root.h>
#include "clientRuntimeConfig.h"
#include "network.h"

namespace RuntimeConfig
{
	bool aiMode = false;
	CryptographicID aiAccessToken;
	String serverURL;
	bool udpListen = true;

	String configPath = "client";

	String ConfigDir()
	{
		if (aiMode)
			return "./config/ai";
		return "./config/player";
	}



	namespace FromFile
	{
		int		capFPS = -1;
		bool	alwaysDay = false;
	}

}
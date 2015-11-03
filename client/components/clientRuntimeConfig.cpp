#include <global_root.h>
#include "clientRuntimeConfig.h"
#include "network.h"

namespace RuntimeConfig
{
	bool aiMode = false;
	CryptographicID aiAccessToken;
	String serverURL;
	bool udpListen = true;




	namespace FromFile
	{
		int		capFPS = -1;
		bool	alwaysDay = false;
	}

}
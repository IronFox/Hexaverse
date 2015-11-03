#ifndef runtimeConfigComponentH
#define runtimeConfigComponentH

#include "../../shared/uniqueID.h"
#include "../../shared/runtimeConfig.h"

namespace RuntimeConfig
{
	extern bool aiMode;
	extern CryptographicID aiAccessToken;
	extern String serverURL;
	extern bool udpListen;




	namespace FromFile
	{
		extern int		capFPS;
		extern bool		alwaysDay;
	}

}



#endif

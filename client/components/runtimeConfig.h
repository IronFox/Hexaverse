#ifndef runtimeConfigComponentH
#define runtimeConfigComponentH

#include "../../shared/uniqueID.h"

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

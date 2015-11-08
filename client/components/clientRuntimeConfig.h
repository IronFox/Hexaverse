#ifndef runtimeConfigComponentH
#define runtimeConfigComponentH

#include <io/file_system.h>

#include "../../shared/uniqueID.h"
#include "../../shared/runtimeConfig.h"

namespace RuntimeConfig
{
	extern bool aiMode;
	extern CryptographicID aiAccessToken;
	extern String serverURL;
	extern bool udpListen;



	void					InitConfigDir();


	namespace FromFile
	{
		extern int		capFPS;
		extern bool		alwaysDay;
	}

}



#endif

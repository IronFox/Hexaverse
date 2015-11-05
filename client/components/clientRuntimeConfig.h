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

	String		ConfigDir();

	inline String ConfigFile(const String&base)	{return ConfigDir() + FOLDER_SLASH + base;}



	namespace FromFile
	{
		extern int		capFPS;
		extern bool		alwaysDay;
	}

}



#endif

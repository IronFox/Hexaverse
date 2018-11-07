#ifndef aiProcessH
#define aiProcessH

#include <uniqueID.h>
#include <functional>

namespace AIProcess
{
	void			Begin(const CryptographicID&accessToken, USHORT serverBasePort, const std::function<String()>&asyncGetServerPassword);
	void			End();
}





#endif

#ifndef remoteControlH
#define remoteControlH

#include "../../shared/protocol.h"

namespace RemoteControl
{
	void	Start(UINT16 serverBasePort);
	void	Destroy();
	void	Resolve();
}



#endif

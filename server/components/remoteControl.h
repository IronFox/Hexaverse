#ifndef remoteControlH
#define remoteControlH

#include "../shared/protocol.h"

namespace RemoteControl
{
	void	Start(Protocol::ChannelID::id_t waitForPacket = Protocol::ChannelID::Unused);
	void	Destroy();

}



#endif

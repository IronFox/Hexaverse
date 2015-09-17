#ifndef udpBroadcastH
#define udpBroadcastH

#include "../../shared/protocol.h"


namespace UDPBroadCast
{
	extern Protocol::THostBroadcastPacket	packet;

	bool		Start();
	void		Terminate();
	void		Update();
}



#endif

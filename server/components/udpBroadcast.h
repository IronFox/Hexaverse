#ifndef udpBroadcastH
#define udpBroadcastH

#include "../../shared/protocol.h"


namespace UDPBroadCast
{
	extern Protocol::THostBroadcastPacket	packet;

	bool		Start(UINT16 basePort);
	void		Terminate();
	void		Update();
	bool		IsStarted();
}



#endif

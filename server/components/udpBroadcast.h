#ifndef udpBroadcastH
#define udpBroadcastH

#include "../../shared/protocol.h"


namespace UDPBroadCast
{
	extern Protocol::THostBroadcastPacket	packet;

	/**
	Creates the UDP broadcast packet. Must be called once when the program starts even if UDP broadcast will never be used
	*/
	void		Create(UINT16 basePort);
	/**
	Starts UDP broadcasting relative to the specified base port
	*/
	bool		Start();
	/**
	Ends UDP broadcasting
	*/
	void		Terminate();
	/**
	Resends any UDP broadcasts, if UDP broadcasting has been started and timing is appropriate. Does nothing otherwise
	*/
	void		Update();
	/**
	Checks if UDP broadcasting is enabled
	*/
	bool		IsStarted();
}



#endif

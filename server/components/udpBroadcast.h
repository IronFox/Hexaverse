#ifndef udpBroadcastH
#define udpBroadcastH

#include <protocol.h>


namespace UDPBroadCast
{
	extern Protocol::THostBroadcastPacket	packet;

	/**
	Creates the UDP broadcast packet. Must be called once when the program starts even if UDP broadcast will never be used
	*/
	void		Create(UINT16 basePort, const TUniqueID&serverID);
	/**
	Starts UDP broadcasting relative to the specified base port
	*/
	bool		Start();
	/**
	Ends UDP broadcasting
	*/
	void		Stop();
	/**
	Resends any UDP broadcasts, if UDP broadcasting has been started and timing is appropriate. Does nothing otherwise
	*/
	void		Update();
	/**
	Checks if UDP broadcasting is enabled
	*/
	bool		IsStarted();

	/**
	Checks whether or not the local packet variable is completed and can be used.
	Network::InformationProvider::Updater thread queries this every half second until true is returned
	@return true if @a packet is complete. Always true on master branch
	*/
	bool		PacketIsDone();
}



#endif

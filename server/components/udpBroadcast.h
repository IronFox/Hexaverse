#ifndef udpBroadcastH
#define udpBroadcastH

#include <protocol.h>


namespace UDPBroadCast
{
	extern Protocol::THostBroadcastPacket	packet;

	/**
	Creates the UDP broadcast packet. Must be called once when the program starts even if UDP broadcast will never be used
	*/
	void		Create(UINT16 basePort, const TUniqueID&serverID, UINT64 ownerID);
	void		UpdateLobbyID(UINT64 lobbyID);
	#if 0
	/**
	Starts UDP broadcasting relative to the specified base port
	*/
	bool		Start();
	/**
	Ends UDP broadcasting
	*/
	void		Stop();
	#endif
	/**
	Resends any UDP broadcasts, if UDP broadcasting has been started and timing is appropriate. Does nothing otherwise
	*/
	void		Update();
	#if 0
	/**
	Checks if UDP broadcasting is enabled
	*/
	bool		IsStarted();
	#endif

	/**
	Checks whether or not the local packet variable is completed and can be used.
	Network::InformationProvider::Updater thread queries this every half second until true is returned
	@return true if @a packet is complete. Always true on master branch
	*/
	bool		PacketIsDone();
}



#endif

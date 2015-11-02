#ifndef serverLogEventH
#define serverLogEventH

#include "../../shared/protocol.h"


extern bool		inTestMode /*= false*/;

namespace Logging
{
	/**
	Writes an event to the console and/or log file
	*/
	void	WriteEvent(EventType type, const String&message, TCP::Peer*peer, const Objects::WorldEntity*e);

	/**
	Sends an event to a peer
	*/
	void	SendEntityEvent(TCP::Peer*toPeer,EventType type, const Objects::WorldEntity*e, const String&msg);
}

void		LogEvent(EventType t, TCP::Peer&p, const String&msg);
void		LogEvent(EventType t, TCP::Peer&p, const Objects::PWorldEntity&e, const String&msg);




#endif

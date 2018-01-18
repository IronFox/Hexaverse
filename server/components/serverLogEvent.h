#ifndef serverLogEventH
#define serverLogEventH

#include "../../shared/protocol.h"


extern bool		inTestMode /*= false*/;

namespace Logging
{
	/**
	Writes an event to the console and/or log file
	*/
	void	WriteEvent(const EventClass&type, String message, TCP::Peer*peer, const Objects::WorldEntity*e, const Objects::PFaction&owner, Origin origin = Logging::Origin::Local);

	/**
	Sends an event to a peer
	*/
	void	SendEntityEvent( TCP::Peer*toPeer,const EventClass&type, const Objects::WorldEntity*e, const Objects::PFaction&owner, const String&msg, Logging::Privacy privacy=Logging::Privacy::Auto);
}

void		LogEvent(const EventClass&t, TCP::Peer&p, const String&msg);
void		LogEvent(const EventClass&t, TCP::Peer&p, const Objects::PWorldEntity&e, const String&msg);

inline void		LogEvent(const EventClass&t, const TCP::PPeer&p, const String&msg)	{if (p) LogEvent(t,*p,msg); else LogEvent(t,msg);}
inline void		LogEvent(const EventClass&t, const TCP::PPeer&p, const Objects::PWorldEntity&e, const String&msg)	{if (p) LogEvent(t,*p,e,msg); else LogEvent(t,e.get(),msg);}



#endif

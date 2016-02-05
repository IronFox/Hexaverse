#include <global_root.h>
#include "serverLogEvent.h"

#include "networkChannels.h"
#include "remoteControl.h"

namespace Logging
{
	void SendEntityEvent( TCP::Peer*toPeer,const EventClass&type, const Objects::WorldEntity*e, const String&msg)
	{
		if (type.flavor == EventFlavor::ServerPrivate || type.significance <= EventSignificance::Common)
			return;


		if (!Network::server.IsOnline())
			return;
		if (!toPeer && e)
		{
			Network::PClient*cl = Network::playerMap.QueryPointer(e->owner);
			if (cl)
				toPeer = (*cl)->peer.lock().get();
			else
				return;	//still private
		}


		Protocol::ControlMessage out;

		if (e)
			out.message = e->GetNiceName()+": "+msg;
		else
			out.message = msg;
		out.significance = (BYTE)type.significance;
		out.flavor = (BYTE)type.flavor;

		if (!toPeer)
			RemoteControl::RelayEvent(out.message);

		if (toPeer)
		{
			out.message = "[private] "+out.message;
			Channels::controlMessage.SendTo(*toPeer,out);
		}
		else
			Channels::controlMessage.SendTo(Network::server,out,2);
	}
}

void LogEvent(const EventClass&t, TCP::Peer&peer, const String&msg)
{
	Logging::SendEntityEvent(&peer,t,nullptr,msg);

	Logging::WriteEvent(t,msg,&peer,nullptr);
}

void LogEvent(const EventClass&t, TCP::Peer&peer, const Objects::PWorldEntity&e, const String&msg)
{
	Logging::SendEntityEvent(&peer,t,e.get(),msg);
	Logging::WriteEvent(t,msg,&peer,e.get());
}



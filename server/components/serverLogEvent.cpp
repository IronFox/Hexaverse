#include <global_root.h>
#include "serverLogEvent.h"

#include "networkChannels.h"
#include "remoteControl.h"

namespace Logging
{
	void SendEntityEvent( TCP::Peer*toPeer,const EventClass&type, const Objects::WorldEntity*e, const Objects::OwnerParameter&owner, const String&msg)
	{
		if (!toPeer && !e && !owner && type.significance >= EventSignificance::Uncommon)
			RemoteControl::RelayEvent(msg);

		if (type.flavor == EventFlavor::ServerPrivate || type.significance < EventSignificance::Uncommon)
			return;


		if (!Network::server.IsOnline())
			return;
		if (!toPeer && (e || owner))
		{
			Network::PClient*cl = e ? Network::playerMap.QueryPointer(e->owner) : Network::playerMap.QueryPointer(owner);
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

		if (toPeer)
		{
			out.message = "[private] "+out.message;
			out.message.Truncate(out.message.MaxSize);
			Channels::controlMessage.SendTo(*toPeer,out);
		}
		else
		{
			out.message.Truncate(out.message.MaxSize);
			Channels::controlMessage.SendTo(Network::server,out,2);
		}
	}
}

void LogEvent(const EventClass&t, TCP::Peer&peer, const String&msg)
{
	Logging::SendEntityEvent(&peer,t,nullptr, Objects::OwnerParameter(),msg);

	Logging::WriteEvent(t,msg,&peer,nullptr,Objects::OwnerParameter());
}

void LogEvent(const EventClass&t, TCP::Peer&peer, const Objects::PWorldEntity&e, const String&msg)
{
	Logging::SendEntityEvent(&peer,t,e.get(),Objects::OwnerParameter(),msg);
	Logging::WriteEvent(t,msg,&peer,e.get(),Objects::OwnerParameter());
}



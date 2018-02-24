#include <global_root.h>
#include "serverLogEvent.h"

#include "networkChannels.h"
#include "remoteControl.h"

namespace Logging
{
	void SendEntityEvent( TCP::Peer*toPeer,const EventClass&type, const Objects::WorldEntity*e, const Objects::PFaction&owner, const String&msg, Logging::Privacy privacy/*=Logging::Privacy::Auto*/)
	{

		if (type.flavor == EventFlavor::ServerPrivate && privacy == Logging::Privacy::Auto)
			privacy = Logging::Privacy::ServerPrivate;

		if (privacy == Logging::Privacy::Auto || privacy == Logging::Privacy::ClientPrivate)
		{
			if (!toPeer && (e || owner))
			{
				Network::PClient*cl = e ? Network::playerMap.QueryPointer(e->owner) : Network::playerMap.QueryPointer(owner);
				if (cl)
					toPeer = (*cl)->peer.lock().get();
				else
					return;	//no point: entity or owner is set. either is auto and implicitly private, or explicitly. either way: stop
			}
			elif (!toPeer && privacy == Logging::Privacy::ClientPrivate)
				return;	//should not happen, but just in case
		}
		else
			toPeer = nullptr;

		//from here on: toPeer != null => private

		if (!toPeer && type.significance >= EventSignificance::Uncommon)
			RemoteControl::RelayEvent(msg);
		
		if (privacy == Logging::Privacy::ServerPrivate || !Network::server.IsOnline())
			return;


		Protocol::ControlMessage out;
		if (msg.Length() <= out.message.MaxLength)
			out.message = msg;
		else
			out.message = msg.SubString(0,out.message.MaxLength-3)+"...";
		out.significance = (BYTE)type.significance;
		out.flavor = (BYTE)type.flavor;
		out.focus = e ? e->id : (owner ? owner->id : TUniqueID::Empty());
		if (toPeer)
		{
			out.isPrivate = true;
			out.message.Truncate(out.message.MaxSize);
			Channels::controlMessage.SendTo(*toPeer,out);
		}
		else
		{
			out.isPrivate = false;
			out.message.Truncate(out.message.MaxSize);
			Channels::controlMessage.SendTo(Network::server,out,Channels::Level::Faction);
		}
	}
}

void LogEvent(const EventClass&t, TCP::Peer&peer, const String&msg)
{
	Logging::SendEntityEvent(&peer,t,nullptr, nullptr,msg);

	Logging::WriteEvent(t,msg,&peer,nullptr,nullptr,Logging::Origin::Local);
}

void LogEvent(const EventClass&t, TCP::Peer&peer, const Objects::PWorldEntity&e, const String&msg)
{
	Logging::SendEntityEvent(&peer,t,e.get(),nullptr,msg);
	Logging::WriteEvent(t,msg,&peer,e.get(),nullptr,Logging::Origin::Local);
}



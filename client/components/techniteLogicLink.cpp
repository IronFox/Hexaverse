#include <global_root.h>
#include "techniteLogicLink.h"
#include "network.h"


namespace TechniteLogicLink
{
	namespace Receiver
	{
		void TechniteInstructionChunk(Protocol::TechniteInstructions&chunk)
		{
			//re-route to server as is
			Network::Channels::techniteInstructions.SendTo(Network::client,chunk);
		}
	}


	namespace Channels
	{
		TCP::Channel<Protocol::TechniteInstructions,0,
					TechniteProtocol::ChannelID::TechniteInstructionChunk>			techniteInstructionChunk(Receiver::TechniteInstructionChunk);
		TCP::ObjectSender<TechniteProtocol::TechniteStateChunk,
					TechniteProtocol::ChannelID::TechniteStateChunk>				techniteStateChunk;
		TCP::ObjectSender<TechniteProtocol::SessionBegin,
					TechniteProtocol::ChannelID::SessionBegin>						sessionBegin;
		TCP::ObjectSender<TechniteProtocol::NodeChunk,
					TechniteProtocol::ChannelID::NodeChunk>							nodeChunk;

	}


	static TCP::Server		logicServer;


	static void Init(TCP::Peer&peer)
	{


	}



	void		OnEvent(TCP::event_t ev, TCP::Peer&peer)
	{
		switch (ev)
		{
			case TCP::Event::ConnectionEstablished:
			{
				LogEvent(EventType::SessionRelated, nullptr, "TechniteLogic: Peer connected: "+peer.ToString());
				Init(peer);

			}
			break;
			case TCP::Event::ConnectionFailed:
			{
			}
			break;
			case TCP::Event::ConnectionLost:
			case TCP::Event::ConnectionClosed:
			{
				LogEvent(EventType::SessionRelated, nullptr, "TechniteLogic: Peer connection lost: "+peer.ToString());
			}
			break;
		}
	}

	static void		OnSignal(UINT32 signal, TCP::Peer&origin)
	{
		//none supported for now
	}

	UINT16 myPort;

	void		Create()
	{
		logicServer.InstallChannel(Channels::techniteInstructionChunk);

		logicServer.SetAsync(false);	//can't be async because of world grid. damn
		logicServer.onEvent = OnEvent;
		logicServer.onSignal = OnSignal;

		if (!logicServer.StartService(0,&myPort))
		{
			FATAL__(logicServer.GetError(false));
		}



	}

	void		ProcessEvents()
	{
		logicServer.Resolve();
	}


	void		Destroy()
	{
		logicServer.EndService();
	}


}

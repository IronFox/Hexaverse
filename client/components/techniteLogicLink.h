#ifndef techniteLogicLinkH
#define techniteLogicLinkH

#include "../TechniteLogic/techniteProtocol.h"

namespace TechniteLogicLink
{
	namespace Channels
	{
		extern TCP::Channel<Protocol::TechniteInstructions,0,
					TechniteProtocol::ChannelID::TechniteInstructionChunk>			techniteInstructionChunk;
		extern TCP::ObjectSender<TechniteProtocol::TechniteStateChunk,
					TechniteProtocol::ChannelID::TechniteStateChunk>				techniteStateChunk;
		extern TCP::ObjectSender<TechniteProtocol::SessionBegin,
					TechniteProtocol::ChannelID::SessionBegin>						sessionBegin;
		extern TCP::ObjectSender<TechniteProtocol::NodeChunk,
					TechniteProtocol::ChannelID::NodeChunk>							nodeChunk;

	}



	void		Create();
	void		Destroy();

	void		ProcessEvents();

}





#endif

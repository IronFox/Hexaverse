#ifndef techniteLogicLinkH
#define techniteLogicLinkH

#include "../TechniteLogic/techniteProtocol.h"

namespace TechniteLogicLink
{
	namespace Channels
	{
		extern TCP::Channel<Package::NetString<64>,0,
					TechniteProtocol::ChannelID::Authenticate>						authenticate;
		extern TCP::Channel<Protocol::TechniteInstructions,1,
					TechniteProtocol::ChannelID::TechniteInstructionChunk>			techniteInstructionChunk;
		extern TCP::ObjectSender<Protocol::TechniteChunk,
					TechniteProtocol::ChannelID::TechniteStateChunk>				techniteStateChunk;
		extern TCP::ObjectSender<TechniteProtocol::GridConfig,
					TechniteProtocol::ChannelID::GridConfig>						gridConfig;
		extern TCP::ObjectSender<TechniteProtocol::NodeChunk,
					TechniteProtocol::ChannelID::NodeChunk>							nodeChunk;
		extern TCP::ObjectSender<TechniteProtocol::GridDelta,
					TechniteProtocol::ChannelID::GridDelta>							gridDelta;

	}



	void		SendDelta(Protocol::WorldDelta&delta);
	void		Forward(Protocol::TechniteChunk&chunk);
	void		SignalTerrainReset();
	void		Create();
	void		Destroy();

	void		ProcessEvents();

}





#endif

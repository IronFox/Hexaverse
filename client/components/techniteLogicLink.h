#ifndef techniteLogicLinkH
#define techniteLogicLinkH

#include "../TechniteLogic/techniteProtocol.h"

namespace TechniteLogicLink
{
	namespace Channels
	{
		//extern TCP::Channel<Package::NetString<64>,0,
		//			TechniteProtocol::ChannelID::Authenticate>						authenticate;
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
		extern TCP::ObjectSender<TechniteProtocol::WorldInfo,
					TechniteProtocol::ChannelID::WorldInfo>							worldInfo;
	}



	/**
	Sends a world delta chunk to all connected technite logic clients
	@param delta World delta chunk. Contained data will be modified briefly, but reverted before the function returns.
	*/
	void		SendDelta(Protocol::WorldDelta&delta);
	/**
	Forwards a technite chunk to all connected technite logic clients.
	Contained technites will be buffered for future logic connection attempts
	*/
	void		Forward(Protocol::TechniteChunk&chunk);
	/**
	Signals that the world has been reset, and any leftover technite states should be flushed.
	*/
	void		SignalTerrainReset();

	/**
	Sends a logic execution command to all connected technite logic clients
	*/
	void		ExecuteLogic();

	/**
	Starts the technite logic link. Nothing happens if the system is already active.
	*/
	void		Create();

	/**
	Stops technite logic link. Nothing happens if the system is already down.
	*/
	void		Destroy();

	/**
	Processes link network events, and checks the logic process.
	Must be called frequently, ideally once per frame.
	*/
	void		ProcessEvents();


	void		SetWorldInfo(Grid::Content coreContent);


	/**
	Checks whether or not the local link expects a client to connect
	*/
	bool		AwaitsClient();


	extern String autoStartClientPath; //="TechniteLogic/Client.exe"
	extern bool autoStartClient; //=true
	extern UINT16 initialServerPortNumber, //=0
					actualServerPortNumber;

}





#endif

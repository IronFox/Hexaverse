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
		extern TCP::Channel<TechniteProtocol::TechniteColorChunk,1,
					TechniteProtocol::ChannelID::TechniteColorChunk>				techniteColorChunk;

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
		extern TCP::ObjectSender<Package::NetString<256>,
					TechniteProtocol::ChannelID::Error>								error;
		extern TCP::ObjectSender<TechniteProtocol::InstructTechnites,
					TechniteProtocol::ChannelID::InstructTechnites>					instructTechnites;
		extern TCP::ObjectSender<TechniteProtocol::GameObject,
					TechniteProtocol::ChannelID::CreateGameObject>					createGameObject;
		extern TCP::ObjectSender<TechniteProtocol::GameObjectID,
					TechniteProtocol::ChannelID::RemoveGameObject>					removeGameObject;
		extern TCP::ObjectSender<TechniteProtocol::ControlMarker,
					TechniteProtocol::ChannelID::CreateControlMarker>				createControlMarker;
		extern TCP::ObjectSender<NET(UINT32),
					TechniteProtocol::ChannelID::RemoveControlMarker>				removeControlMarker;
		extern TCP::ObjectSender<TechniteProtocol::BeginSession,
					TechniteProtocol::ChannelID::BeginSession>						beginSession;
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
	void		ExecuteLogicWhenAllTechnitesAreThere(UINT32 roundNumber, UINT32 techniteSubRoundNumber, bool reExecution=false);

	/**
	Queries whether or not technite logic processing has completed this round
	*/
	bool		LogicIsDone();

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
	void		StartSession(const TUniqueID&factionID, BYTE techniteGridID);

	/**
	Checks whether or not the local link expects a client to connect
	*/
	bool		AwaitsClient();


	/**
	Handler function for various entity-related messages received from the server.
	Since technites do not have an animation phase, these are forwarded/stored as they come.
	Nothing happens, if the local link is inactive
	*/
	void		Handle(const Protocol::DestroyEntity&);
	void		Handle(const Protocol::NewMobileEntity&);	//!< @overload
	void		Handle(const Protocol::MobileState&);	//!< @overload
	void		Handle(const Protocol::StructureConstruction&);	//!< @overload
	void		Handle(const Protocol::StructureState&);	//!< @overload
	void		Handle(const Protocol::MobileEntityMotion&);	//!< @overload

	void		CreateMarker(const Objects::Marker&);
	void		DestroyMarker(const TUniqueID&);

	struct DebugMessage
	{
		UINT32	roundNumber,
				subRoundNumber;
		Timer::Time	received;
		String	message;
	};

	bool		HasDebugMessage(const Grid::HCellIdU&);
	void		ForeachDebugMessage(const Grid::HCellIdU&,const std::function<void(const DebugMessage&)>&f);


	extern PathString autoStartClientPath; //="TechniteLogic/Client.exe"
	extern bool autoStartClient; //=true
	extern UINT16 initialServerPortNumber, //=0
					actualServerPortNumber;

}





#endif

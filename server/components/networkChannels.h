#ifndef serverNetworkChannelsH
#define serverNetworkChannelsH

#include "../../shared/protocol.h"
#include "state.h"

namespace Receiver
{
	void	Hello(Protocol::Hello&state, TCP::Peer&peer);
	void	EntityDirectCommand(Protocol::EntityDirectCommand&cmd, TCP::Peer&peer);
	void	ChoiceIntersection(Protocol::ChoiceIntersection&ch, TCP::Peer&peer);
	void	DropLastTask(NET(TUniqueID)&uuid, TCP::Peer&peer);
	void	DropAllTasks(NET(TUniqueID)&uuid, TCP::Peer&peer);
	void	SignalNewJob(Protocol::StandardJob&job, TCP::Peer&peer);
	void	SignalTerrainMod(Protocol::TerrainMod&mod, TCP::Peer&peer);
	void	ConstructionAbort(Protocol::ConstructionAbort&ca, TCP::Peer&peer);
	void	ConstructionBegin(Protocol::ConstructionBegin&cb, TCP::Peer&peer);
	void	EnableStructure(Protocol::EnableStructure&es, TCP::Peer&peer);
	void	ClientFaction(Protocol::ClientFactionRequest&, TCP::Peer&peer);
	void	RestoreCoreLink(NET(TUniqueID)&uuid, TCP::Peer&peer);
	void	RemoveEntity(NET(TUniqueID)&uuid, TCP::Peer&peer);
	void	Automate(Protocol::Automate&, TCP::Peer&peer);
	void	CreateMarker(Protocol::Marker&, TCP::Peer&peer);
	void	DestroyMarker(NET(TUniqueID)&, TCP::Peer&peer);
	void	TouchMarker(Protocol::TouchedMarker&, TCP::Peer&peer);
	void	ChangeMyName(Protocol::ClientName&, TCP::Peer&peer);
	void	IdentifyUser(NET(TUniqueID)&, TCP::Peer&peer);
	void	ChatMessage(Package::NetString<256>&, TCP::Peer&peer);



	/**
	Handler function to process network signals (packages without payloads)
	*/
	void		OnSignal(UINT32 signal, TCP::Peer&peer);
	/**
	Handler function to process network events (disconnects, etc)
	*/
	void		OnEvent(TCP::event_t ev, TCP::Peer&peer);

	/**
	Handler function to process events where packages have been ignored by the network system
	*/
	void		OnIgnorePackage(UINT32 channelId, UINT32 packageSize, TCP::Peer&peer);



}



namespace Channels
{



	
	//TCP::ObjectSender<Protocol::ServerState,Protocol::ChannelID::GetInfo>		infoSender;
	//TCP::ObjectSender<Protocol::Result,Protocol::ChannelID::
	static const UINT32 FactionLevel = (UINT32)Network::UserLevel::CommonFaction;
	static const UINT32 PreFaction = (UINT32)Network::UserLevel::PreFaction;

	extern TCP::Channel<Protocol::Hello, 0,Protocol::ChannelID::Hello>											hello;
	extern TCP::Channel<Protocol::EntityDirectCommand, FactionLevel, Protocol::ChannelID::EntityDirectCommand>	entityDirectCommand;
	extern TCP::Channel<Protocol::ChoiceIntersection, FactionLevel, Protocol::ChannelID::ChoiceIntersection>	choiceIntersection;
	extern TCP::Channel<NET(TUniqueID), FactionLevel, Protocol::ChannelID::DropLastTask>						dropLastTask;
	extern TCP::Channel<NET(TUniqueID), FactionLevel, Protocol::ChannelID::DropAllTasks>						dropAllTasks;
	extern TCP::Channel<Protocol::StandardJob, FactionLevel, Protocol::ChannelID::SignalNewJob>					signalNewJob;
	extern TCP::Channel<Protocol::TerrainMod, FactionLevel, Protocol::ChannelID::SignalTerrainMod>				signalTerrainMod;
	extern TCP::Channel<Protocol::ConstructionAbort, FactionLevel, Protocol::ChannelID::ConstructionAbort>		constructionAbort;
	extern TCP::Channel<Protocol::ConstructionBegin, FactionLevel, Protocol::ChannelID::ConstructionBegin>		constructionBegin;
	extern TCP::Channel<Protocol::EnableStructure, FactionLevel, Protocol::ChannelID::EnableStructure>			enableStructure;
	extern TCP::Channel<Protocol::ClientFactionRequest, PreFaction, Protocol::ChannelID::ClientFaction>			clientFactionRequest;
	
	extern TCP::Channel<NET(TUniqueID), FactionLevel, Protocol::ChannelID::RestoreCoreLink>						restoreCoreLink;
	extern TCP::Channel<NET(TUniqueID), FactionLevel, Protocol::ChannelID::RemoveEntity>						removeEntity;
	extern TCP::Channel<Protocol::Automate, FactionLevel, Protocol::ChannelID::Automate>						automate;
	extern TCP::Channel<Protocol::Marker, FactionLevel, Protocol::ChannelID::CreateMarker>						createMarker;
	extern TCP::Channel<NET(TUniqueID), FactionLevel, Protocol::ChannelID::DestroyMarker>						destroyMarker;
	extern TCP::Channel<Protocol::TouchedMarker, FactionLevel, Protocol::ChannelID::TouchMarker>				touchMarker;
	extern TCP::Channel<Protocol::ClientName,FactionLevel,Protocol::ChannelID::ChangeMyName>					changeMyName;
	extern TCP::Channel<NET(TUniqueID), FactionLevel, Protocol::ChannelID::IdentifyUser>						identifyUserRecv;
	extern TCP::Channel<Package::NetString<256>, FactionLevel, Protocol::ChannelID::ChatMessage>				chatMessage;

	extern TCP::ObjectSender<Protocol::ClientFactionResponse, Protocol::ChannelID::ClientFaction>		clientFactionResponse;
	extern TCP::ObjectSender<Protocol::Marker, Protocol::ChannelID::MarkerCreationFailedLimitReached>	markerCreationFailedLimitReached;
	extern TCP::ObjectSender<Protocol::WorldDelta,Protocol::ChannelID::WorldDelta>						updateWorld;
	extern TCP::ObjectSender<Protocol::NewMobileEntity,Protocol::ChannelID::NewMobileEntity>			newMobileEntity;
	extern TCP::ObjectSender<Protocol::StructureConstruction,Protocol::ChannelID::InstantiateStructure>	createStructure;
	extern TCP::ObjectSender<Protocol::MobileState,Protocol::ChannelID::MobileState>					setMobileState;
	extern TCP::ObjectSender<Protocol::MobileEntityMotion,Protocol::ChannelID::MobileEntityMotion>		mobileEntityMotion;
	extern TCP::ObjectSender<Protocol::AttackTrajectory,Protocol::ChannelID::AttackTrajectory>			attackTrajectory;
	extern TCP::ObjectSender<Protocol::EntityTurretRotation,Protocol::ChannelID::EntityTurretRotation>	entityTurretRotation;
	extern TCP::ObjectSender<Protocol::Explosion,Protocol::ChannelID::Explosion>						explosion;
	extern TCP::ObjectSender<Protocol::StructureState,Protocol::ChannelID::UpdateStructure>				setStructureState;
	extern TCP::ObjectSender<Protocol::DestroyEntity,Protocol::ChannelID::DestroyEntity>				destroyEntity;
	extern TCP::ObjectSender<Protocol::ControlMessage,Protocol::ChannelID::ControlMessage>				controlMessage;
	extern TCP::ObjectSender<NET(TUniqueID),Protocol::ChannelID::SetFactionID>							setFactionID;
	extern TCP::ObjectSender<Protocol::PhaseChange,Protocol::ChannelID::PhaseChange>					phaseChange;
	extern TCP::ObjectSender<Protocol::ResourceUpdate,Protocol::ChannelID::ResourceUpdate>				resourceUpdate;
	extern TCP::ObjectSender<Protocol::GameProgress,Protocol::ChannelID::GameProgress>					gameProgress;
	extern TCP::ObjectSender<Protocol::ChangeEntityOwner,Protocol::ChannelID::ChangeEntityOwner>		changeEntityOwner;
	extern TCP::ObjectSender<Protocol::IdentifyUser, Protocol::ChannelID::IdentifyUser>					identifyUser;
	extern TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::FactionOnline>						factionOnline;
	extern TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::FactionOffline>						factionOffline;
	extern TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::FactionRequestedNewRound>				factionRequestedNewRound;
	extern TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::FactionContinuedRound>				factionContinuedRound;
	
	extern TCP::ObjectSender<NET(UINT16), Protocol::ChannelID::LockEMPZones>							lockEmpZones;
	#ifndef NO_HIBERNATION
		extern TCP::ObjectSender<NET(UINT32), Protocol::ChannelID::UplinkBandwidth>						uplinkBandwidth;
	#endif
	extern TCP::ObjectSender<Protocol::UnitLimits,Protocol::ChannelID::UnitLimits>						unitLimits;

//	Channel<class Object, unsigned MinUserLevel, UINT32 ChannelID>

}




#endif

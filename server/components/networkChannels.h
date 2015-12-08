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
	void	TechniteInstructions(Protocol::TechniteInstructions&instructions, TCP::Peer&peer);


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



	bool HasAccess(const State::Client&c, const Objects::WorldEntity&e);
	bool HasAccess(const State::PClient&c, const Objects::PWorldEntity&e);
	bool HasAccess(const State::PClient&c, const Objects::WorldEntity&e);

	bool PeerHasAccess(TCP::Peer&peer, const Objects::WorldEntity&e);
	bool PeerHasAccess(TCP::Peer&peer, const Objects::PWorldEntity&e);

	bool CanSee(const State::Client&c, const Objects::PWorldEntity&e, float t);
	bool CanSee(const State::PClient&c, const Objects::PWorldEntity&e, float t);

}



namespace Channels
{



	
	//TCP::ObjectSender<Protocol::ServerState,Protocol::ChannelID::GetInfo>		infoSender;
	//TCP::ObjectSender<Protocol::Result,Protocol::ChannelID::


	extern TCP::Channel<Protocol::Hello, 0,Protocol::ChannelID::Hello>									hello;
	extern TCP::Channel<Protocol::EntityDirectCommand, 1, Protocol::ChannelID::EntityDirectCommand>		entityDirectCommand;
	extern TCP::Channel<Protocol::ChoiceIntersection, 1, Protocol::ChannelID::ChoiceIntersection>		choiceIntersection;
	extern TCP::Channel<NET(TUniqueID), 1, Protocol::ChannelID::DropLastTask>							dropLastTask;
	extern TCP::Channel<NET(TUniqueID), 1, Protocol::ChannelID::DropAllTasks>							dropAllTasks;
	extern TCP::Channel<Protocol::StandardJob, 1, Protocol::ChannelID::SignalNewJob>					signalNewJob;
	extern TCP::Channel<Protocol::TerrainMod, 1, Protocol::ChannelID::SignalTerrainMod>					signalTerrainMod;
	extern TCP::Channel<Protocol::ConstructionAbort, 1, Protocol::ChannelID::ConstructionAbort>			constructionAbort;
	extern TCP::Channel<Protocol::ConstructionBegin, 1, Protocol::ChannelID::ConstructionBegin>			constructionBegin;
	extern TCP::Channel<Protocol::EnableStructure, 1, Protocol::ChannelID::EnableStructure>				enableStructure;
	extern TCP::Channel<Protocol::ClientFactionRequest, 1, Protocol::ChannelID::ClientFaction>			clientFactionRequest;
	
	extern TCP::Channel<NET(TUniqueID), 1, Protocol::ChannelID::RestoreCoreLink>						restoreCoreLink;
	extern TCP::Channel<NET(TUniqueID), 1, Protocol::ChannelID::RemoveEntity>							removeEntity;
	extern TCP::Channel<Protocol::Automate, 1, Protocol::ChannelID::Automate>							automate;
	extern TCP::Channel<Protocol::Marker, 1, Protocol::ChannelID::CreateMarker>							createMarker;
	extern TCP::Channel<NET(TUniqueID), 1, Protocol::ChannelID::DestroyMarker>							destroyMarker;
	extern TCP::Channel<Protocol::TouchedMarker, 1, Protocol::ChannelID::TouchMarker>					touchMarker;
	extern TCP::Channel<Protocol::ClientName,1,Protocol::ChannelID::ChangeMyName>						changeMyName;
	extern TCP::Channel<NET(TUniqueID), 1, Protocol::ChannelID::IdentifyUser>							identifyUserRecv;
	extern TCP::Channel<Package::NetString<256>, 1, Protocol::ChannelID::ChatMessage>					chatMessage;

	extern TCP::Channel<Protocol::TechniteInstructions, 1, Protocol::ChannelID::TechniteInstructions>	techniteInstructions;

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
	extern TCP::ObjectSender<Protocol::FactionID,Protocol::ChannelID::SetFactionID>						setFactionID;
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
	extern TCP::ObjectSender<Protocol::TechniteChunk, Protocol::ChannelID::TechniteChunk>				techniteChunk;
	extern TCP::ObjectSender<Protocol::TechniteColors, Protocol::ChannelID::TechniteColors>				techniteColors;

//	Channel<class Object, unsigned MinUserLevel, UINT32 ChannelID>

}

void SendTechniteStateTo(const Technites::PFaction&tf, TCP::Peer&peer);



#endif

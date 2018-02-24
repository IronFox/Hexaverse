#ifndef serverNetworkChannelsH
#define serverNetworkChannelsH

#include <protocol.h>
#include "state.h"
#include <objects/statistics.h>

namespace Receiver
{
	void	Hello(Protocol::Hello&state, TCP::Peer&peer);
	void	EntityDirectCommand(Protocol::EntityDirectCommand&cmd, TCP::Peer&peer);
	void	ChoiceIntersection(Protocol::ChoiceIntersection&ch, TCP::Peer&peer);
	void	DropTasks(Protocol::DropTasks&uuid, TCP::Peer&peer);
	void	SignalNewJob(Protocol::StandardJob&job, TCP::Peer&peer);
	void	SignalTerrainMod(Protocol::TerrainMod&mod, TCP::Peer&peer);
	void	ConstructionAbort(Protocol::ConstructionAbort&ca, TCP::Peer&peer);
	void	ConstructionBegin(Protocol::ConstructionBegin&cb, TCP::Peer&peer);
	void	EnableStructure(Protocol::EnableStructure&es, TCP::Peer&peer);
	void	ClientFaction(Protocol::ClientFactionRequest&, TCP::Peer&peer);
	void	RestoreCoreLink(NET(TUniqueID)&uuid, TCP::Peer&peer);
	void	RemoveEntity(NET(TUniqueID)&uuid, TCP::Peer&peer);
	void	Automate(Protocol::EntityStateUpdate&, TCP::Peer&peer);
	void	Sleep(Protocol::EntityStateUpdate&, TCP::Peer&peer);
	void	CreateMarker(Protocol::Marker&, TCP::Peer&peer);
	void	DestroyMarker(NET(TUniqueID)&, TCP::Peer&peer);
	void	TouchMarker(Protocol::TouchedMarker&, TCP::Peer&peer);
	void	ChangeMyName(Protocol::ClientName&, TCP::Peer&peer);
	void	IdentifyFaction(NET(TUniqueID)&, TCP::Peer&peer);
	void	QueueResearch(Protocol::QueueResearch&, TCP::Peer&peer);
	void	ChatMessage(Protocol::ChatMessage&, TCP::Peer&peer);
	void	SaveAs(Protocol::SaveAs&, TCP::Peer&peer);
	void	RequestResource(Protocol::ResourceID&, TCP::Peer&peer);
	void	TutorialAt(NET(INT32)&at);

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
	void		OnDeserializationFailed(UINT32 channel, TCP::Peer&origin, const String&exception);


}



namespace Channels
{

	#undef DECLARE_FACTION_CHANNEL
	#define	DECLARE_FACTION_CHANNEL(NAME)	extern TCP::Channel<Protocol::NAME,Level::Faction,Protocol::ChannelID::NAME>	NAME;

	#undef DECLARE_OBJECT_SENDER
	#define	DECLARE_OBJECT_SENDER(NAME)	extern TCP::ObjectSender<Protocol::NAME,Protocol::ChannelID::NAME>	NAME;

	
	//TCP::ObjectSender<Protocol::ServerState,Protocol::ChannelID::GetInfo>		infoSender;
	//TCP::ObjectSender<Protocol::Result,Protocol::ChannelID::
	namespace Level
	{
		static const UINT32 Faction = (UINT32)Network::UserLevel::CommonFaction;
		static const UINT32 PreFaction = (UINT32)Network::UserLevel::PreFaction;
		static const UINT32 Spectator = (UINT32)Network::UserLevel::Spectator;
	}


	extern TCP::Channel<Protocol::Hello, 0,Protocol::ChannelID::Hello>												hello;
	extern TCP::Channel<Protocol::EntityDirectCommand, Level::Faction, Protocol::ChannelID::EntityDirectCommand>	entityDirectCommand;
	extern TCP::Channel<Protocol::ChoiceIntersection, Level::Faction, Protocol::ChannelID::ChoiceIntersection>		choiceIntersection;
	extern TCP::Channel<Protocol::DropTasks, Level::Faction, Protocol::ChannelID::DropTasks>						dropTasks;
	extern TCP::Channel<Protocol::StandardJob, Level::Faction, Protocol::ChannelID::SignalNewJob>					signalNewJob;
	extern TCP::Channel<Protocol::TerrainMod, Level::Faction, Protocol::ChannelID::SignalTerrainMod>				signalTerrainMod;
	extern TCP::Channel<Protocol::ConstructionAbort, Level::Faction, Protocol::ChannelID::ConstructionAbort>		constructionAbort;
	extern TCP::Channel<Protocol::ConstructionBegin, Level::Faction, Protocol::ChannelID::ConstructionBegin>		constructionBegin;
	extern TCP::Channel<Protocol::EnableStructure, Level::Faction, Protocol::ChannelID::EnableStructure>			enableStructure;
	extern TCP::Channel<Protocol::ClientFactionRequest, Level::PreFaction, Protocol::ChannelID::ClientFaction>		clientFactionRequest;
	
	extern TCP::Channel<NET(TUniqueID), Level::Faction, Protocol::ChannelID::RestoreCoreLink>					restoreCoreLink;
	extern TCP::Channel<NET(TUniqueID), Level::Faction, Protocol::ChannelID::RemoveEntity>						removeEntity;
	extern TCP::Channel<Protocol::EntityStateUpdate, Level::Faction, Protocol::ChannelID::Automate>				automate;
	extern TCP::Channel<Protocol::EntityStateUpdate, Level::Faction, Protocol::ChannelID::Sleep>				sleep;
	extern TCP::Channel<Protocol::Marker, Level::Faction, Protocol::ChannelID::CreateMarker>					createMarker;
	extern TCP::Channel<NET(TUniqueID), Level::Faction, Protocol::ChannelID::DestroyMarker>						destroyMarker;
	extern TCP::Channel<Protocol::TouchedMarker, Level::Faction, Protocol::ChannelID::TouchMarker>				touchMarker;
	extern TCP::Channel<Protocol::ClientName,Level::Faction,Protocol::ChannelID::ChangeMyName>					changeMyName;
	extern TCP::Channel<NET(TUniqueID), Level::Faction, Protocol::ChannelID::IdentifyFaction>					identifyFactionRecv;
	extern TCP::Channel<Protocol::QueueResearch, Level::Faction, Protocol::ChannelID::QueueResearch>			queueResearch;
	extern TCP::Channel<Protocol::ResourceID, Level::PreFaction, Protocol::ChannelID::RequestResource>			requestResource;
	extern TCP::Channel<NET(INT32),Level::Faction,Protocol::ChannelID::TutorialAt>								tutorialAt;


	extern TCP::ObjectSender<Protocol::ClientFactionResponse, Protocol::ChannelID::ClientFaction>				clientFactionResponse;
	extern TCP::ObjectSender<Protocol::Marker, Protocol::ChannelID::MarkerCreationFailedLimitReached>			markerCreationFailedLimitReached;
	extern TCP::ObjectSender<Protocol::WorldDelta,Protocol::ChannelID::WorldDelta>								updateWorld;
	extern TCP::ObjectSender<Protocol::NewMobileEntity,Protocol::ChannelID::NewMobileEntity>					newMobileEntity;
	extern TCP::ObjectSender<Protocol::StructureConstruction,Protocol::ChannelID::InstantiateStructure>			createStructure;
	extern TCP::ObjectSender<Protocol::MobileState,Protocol::ChannelID::MobileState>							setMobileState;
	extern TCP::ObjectSender<Protocol::MobileEntityMotion,Protocol::ChannelID::MobileEntityMotion>				mobileEntityMotion;
	extern TCP::ObjectSender<Protocol::VagueAttackTrajectory,Protocol::ChannelID::VagueAttackTrajectory>		vagueAttackTrajectory;
	extern TCP::ObjectSender<Protocol::PreciseAttackTrajectory,Protocol::ChannelID::PreciseAttackTrajectory>	preciseAttackTrajectory;
	extern TCP::ObjectSender<Protocol::EntityTurretRotation,Protocol::ChannelID::EntityTurretRotation>			entityTurretRotation;
	extern TCP::ObjectSender<Protocol::Explosion,Protocol::ChannelID::Explosion>							explosion;
	extern TCP::ObjectSender<Protocol::StructureState,Protocol::ChannelID::UpdateStructure>				setStructureState;
	extern TCP::ObjectSender<Protocol::DestroyEntity,Protocol::ChannelID::DestroyEntity>				destroyEntity;
	extern TCP::ObjectSender<Protocol::ControlMessage,Protocol::ChannelID::ControlMessage>				controlMessage;
	extern TCP::ObjectSender<Protocol::PhaseChange,Protocol::ChannelID::PhaseChange>					phaseChange;
	extern TCP::ObjectSender<Protocol::ResourceUpdate,Protocol::ChannelID::ResourceUpdate>				resourceUpdate;
	extern TCP::ObjectSender<Protocol::GameProgress,Protocol::ChannelID::GameProgress>					gameProgress;
	extern TCP::ObjectSender<Protocol::ChangeEntityOwner,Protocol::ChannelID::ChangeEntityOwner>		changeEntityOwner;
	extern TCP::ObjectSender<Protocol::IdentifyFaction, Protocol::ChannelID::IdentifyFaction>			identifyFaction;
	extern TCP::ObjectSender<Protocol::ResearchState, Protocol::ChannelID::ResearchState>				researchState;
	extern TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::FactionOnline>						factionOnline;
	extern TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::FactionOffline>						factionOffline;
	extern TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::FactionRequestedNewRound>				factionRequestedNewRound;
	extern TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::FactionContinuedRound>				factionContinuedRound;
	extern TCP::ObjectSender<Protocol::ResourceData,Protocol::ChannelID::ResourceData>					resourceData;
	
	extern TCP::ObjectSender<NET(UINT16), Protocol::ChannelID::LockEMPZones>							lockEmpZones;
	#ifndef NO_HIBERNATION
		extern TCP::ObjectSender<NET(UINT32), Protocol::ChannelID::UplinkBandwidth>						uplinkBandwidth;
	#endif
	extern TCP::ObjectSender<Protocol::UnitLimits,Protocol::ChannelID::UnitLimits>						unitLimits;
	extern TCP::ObjectSender<Protocol::OperationLacksResources,Protocol::ChannelID::OperationLacksResources>	operationLacksResources;
	extern TCP::ObjectSender<Protocol::ResearchQueue, Protocol::ChannelID::ResearchQueue>				researchQueue;

	extern TCP::ObjectSender<TUniqueID, Protocol::ChannelID::WinningFaction>							winningFaction;

	DECLARE_OBJECT_SENDER(VoteCreation);
	DECLARE_OBJECT_SENDER(VoteResult);
	DECLARE_FACTION_CHANNEL(ClientVote);
	DECLARE_FACTION_CHANNEL(AllianceInvite);
	DECLARE_FACTION_CHANNEL(AllianceConfigChange);
	DECLARE_FACTION_CHANNEL(ChatMessage);
	DECLARE_FACTION_CHANNEL(SaveAs);

	DECLARE_OBJECT_SENDER(MyFaction);
	DECLARE_OBJECT_SENDER(GameConfig);
	DECLARE_OBJECT_SENDER(KillFaction);

	namespace Statistics
	{
		extern TCP::ObjectSender<TUniqueID, Protocol::ChannelID::BeginStatisticsSequence>				beginSequence;
		extern TCP::ObjectSender<Objects::Statistics::Round, Protocol::ChannelID::RoundStatistics>		round;
		extern TCP::ObjectSender<Objects::Statistics::EntityType, Protocol::ChannelID::EntityTypeStatistics>	entityType;
		extern TCP::ObjectSender<Objects::Statistics::FactionInteraction, Protocol::ChannelID::FactionInteractionStatistics>	factionInteraction;
	}

//	Channel<class Object, unsigned MinUserLevel, UINT32 ChannelID>

}




#endif

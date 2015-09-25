#ifndef techniteServerH
#define techniteServerH

namespace TechniteServer
{
	extern TCP::Channel<Protocol::ClientState, 0,Protocol::ChannelID::SetClientState>					setClientState;
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
	extern TCP::Channel<Package::NetString<32>,1,Protocol::ChannelID::ChangeMyName>						changeMyName;
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
	extern TCP::ObjectSender<NET(UINT16), Protocol::ChannelID::LockEMPZones>							lockEmpZones;
	#ifndef NO_HIBERNATION
		extern TCP::ObjectSender<NET(UINT32), Protocol::ChannelID::UplinkBandwidth>						uplinkBandwidth;
	#endif
	extern TCP::ObjectSender<Protocol::UnitLimits,Protocol::ChannelID::UnitLimits>						unitLimits;
	extern TCP::ObjectSender<Protocol::TechniteChunk, Protocol::ChannelID::TechniteChunk>				techniteChunk;
	



}





#endif

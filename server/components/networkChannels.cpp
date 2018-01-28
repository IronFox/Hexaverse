#include <global_root.h>
#include "networkChannels.h"
#include "state.h"
#include "serverLogEvent.h"
#include "journalSaveGame.h"
#include "configurationMode.h"
#include <objects/structure/mine.h>
#include "state/gameState.h"

extern TwisterRandom		random;


namespace Receiver
{
	void	ClientVote(Protocol::ClientVote&vote, TCP::Peer&peer)
	{
		Network::PClient client = To<Network::Client>::Cast(peer.attachment);
		if (!client)
		{
			LogEvent(EventClass::Preset::internalError,peer,"ClientVote: client is nullptr");
			return;
		}
		if (!client->faction)
		{
			LogEvent(EventClass::Preset::internalError,peer,"ClientVote: client->faction is nullptr");
			return;
		}
		Game::Vote::CastVote(vote.voteID,client->faction,vote.inFavor);
	}

	static Game::Vote::FScope AllianceScope(const Objects::PAlliance&a)
	{
		TUniqueID tID = a->id;
		return [tID](const Objects::PFaction&f)
		{
			return f->alliance->id == tID && !f->isDead;
		};
	}


	static Game::Vote::FScope FactionScope(const Objects::PFaction&f)
	{
		TUniqueID tID = f->id;
		return [tID](const Objects::PFaction&f)
		{
			return f->id == tID;
		};
	}

	void	AllianceConfigChange(Protocol::AllianceConfigChange&ch, TCP::Peer&peer)
	{
		Network::PClient client = To<Network::Client>::Cast(peer.attachment);
		if (!client)
		{
			LogEvent(EventClass::Preset::internalError,peer,"AllianceConfigChange: client is nullptr");
			return;
		}
		if (!client->faction)
		{
			LogEvent(EventClass::Preset::internalError,peer,"AllianceConfigChange: client->faction is nullptr");
			return;
		}

		Objects::PAlliance target = Objects::Alliance::FindByID(ch.allianceID);
		if (target != client->faction->alliance)
		{
			LogEvent(EventClass::Preset::commandRejected,peer,"Access denied. You are not a member of alliance "+ch.allianceID.ToNiceString());
			return;
		}
		if (ch.flags == target->config.ToFlags())
			return;	//nothing to change

		auto cfg = Objects::Alliance::TConfig(ch.flags);
		Game::PVote v = Game::Vote::CreateNew(target->id,
			"Change Alliance configuration to "+ToString(cfg)+"?",
			120,
			AllianceScope(target),
			[cfg](const Game::PVote&v, const Protocol::VoteResult&rs)
			{
				if (rs.resultType == Protocol::VoteResultType::Passed)
				{
					auto a = Objects::Alliance::FindByID(v->concerning[0]);
					a->UpdateConfiguration(cfg);
				}
			}
		);
		if (v)
			v->clearedAfterSeconds = v->closingAfterSeconds+10;	//make sure the vote does not remain more than 10 seconds

	}

	void	AllianceJoinRequest(Protocol::AllianceJoinRequest&req, TCP::Peer&peer)
	{
		Network::PClient client = To<Network::Client>::Cast(peer.attachment);
		if (!client)
		{
			LogEvent(EventClass::Preset::internalError,peer,"AllianceJoinRequest: client is nullptr");
			return;
		}
		if (!client->faction)
		{
			LogEvent(EventClass::Preset::internalError,peer,"AllianceJoinRequest: client->faction is nullptr");
			return;
		}
		if (client->faction->isDead)
		{
			LogEvent(EventClass::Preset::internalError,peer,"AllianceJoinRequest: client->faction is dead");
			return;
		}


		if (req.allianceID.IsEmpty())
		{
			if (RuntimeConfig::IsInAnyCommandPhase())
			{
				LogEvent(EventClass::Preset::uncommon,peer,"Left alliance");
				if (client->faction->alliance->CountLivingMembers() == 1)
					return;	//nothing to do

				Objects::PAlliance a {new Objects::Alliance(TUniqueID::New())};
			
				client->faction->ChangeAlliance(a);
			}
			else
			{
				LogEvent(EventClass::Preset::commandRejected,peer,"Cannot change alliances right now. Must be in a command phase");
			}
			return;
		}

		Objects::PAlliance target = Objects::Alliance::FindByID(req.allianceID);
		if (target == nullptr)
		{
			LogEvent(EventClass::Preset::commandRejected,peer,"Requested Join request rejected, alliance does not exist");
			return;
		}
		if (client->faction->alliance == target)
		{
			LogEvent(EventClass::Preset::commandRejected,peer,"Already joined that alliance");
			return;
		}

		TUniqueID ids[] = {client->faction->id, target->id };

		if (req.abortJoining)
		{
			Game::PVote v = Game::Vote::FindConcerning(ArrayRef<TUniqueID>( ids,2));
			if (v != nullptr)
			{
				v->Cancel();
//				LogEvent(EventClass( EventSignificance::Important, EventFlavor::Neutral),client->faction,"Canceled vote about joining alliance "+target->id.ToNiceString());
			}
		}
		else
		{
			const String niceTask = target->CountLivingMembers() > 1 ? "joining alliance "+target->id.ToNiceString() : "creating an alliance with "+target->GetMember(0)->GetName();

			Game::PVote v = Game::Vote::CreateNew(ArrayRef<TUniqueID>( ids,2),
				"Allow "+client->faction->name+ " to join your alliance?", 
				120,
				AllianceScope(target),
				[niceTask](const Game::PVote&v, const Protocol::VoteResult&rs)
				{
					auto a = Objects::Alliance::FindByID(v->concerning[1]);
					auto f = Objects::Faction::FindByID(v->concerning[0]);
					if (f)
					{
						if (!a)
						{
							LogEvent(EventClass(EventSignificance::Important,EventFlavor::Loss), f,"Vote for "+niceTask+" failed since targeted alliance no longer exists");
						}
						else
							switch (rs.resultType)
							{
								case Protocol::VoteResultType::Passed:
									LogEvent(EventClass(EventSignificance::Important,EventFlavor::Gain), f,"Vote passed. Admission to alliance "+a->id.ToNiceString()+" granted");
									f->ChangeAlliance(a);
								break;
								case Protocol::VoteResultType::Rejected:
									LogEvent(EventClass(EventSignificance::Important,EventFlavor::Loss), f,"Admission to alliance "+a->id.ToNiceString()+" has been rejected");
								break;
								case Protocol::VoteResultType::TimedOut:
									LogEvent(EventClass(EventSignificance::Important,EventFlavor::Loss), f,"Vote for "+niceTask+" has timed out");
								break;
								case Protocol::VoteResultType::Canceled:
									LogEvent(EventClass(EventSignificance::Important,EventFlavor::Neutral), f,"Vote for "+niceTask+" was canceled");
								break;
							}
					}
				}
			);


			if (v)
			{
				LogEvent(EventSignificance::Debug,client->faction,"Started vote on joining alliance "+ToString(target->id));
				LogEvent(EventClass( EventSignificance::Important, EventFlavor::Gain),client->faction,"Started vote on "+niceTask);
				v->quorum = Game::Vote::Quorum::TwoThirds;
				v->votesToPass = Game::Vote::VotesToPass::SimpleMajority;
			}
			else
				LogEvent(EventClass( EventSignificance::Important, EventFlavor::Loss),client->faction,"There already exists a vote on you "+niceTask+". Either it is still active or has been rejected");
		}
	}
}

namespace Channels
{

	#undef DECLARE_FACTION_CHANNEL
	#define	DECLARE_FACTION_CHANNEL(NAME)	TCP::Channel<Protocol::NAME,Level::Faction,Protocol::ChannelID::NAME>	NAME(Receiver::NAME);
	#undef DECLARE_OBJECT_SENDER
	#define	DECLARE_OBJECT_SENDER(NAME)	TCP::ObjectSender<Protocol::NAME,Protocol::ChannelID::NAME>	NAME;


//	DECLARE_FACTION_CHANNEL(FactionAllianceChange);



	TCP::Channel<Protocol::Hello, 0,Protocol::ChannelID::Hello>												hello(Receiver::Hello);
	TCP::Channel<Protocol::EntityDirectCommand, Level::Faction, Protocol::ChannelID::EntityDirectCommand>	entityDirectCommand(Receiver::EntityDirectCommand);
	TCP::Channel<Protocol::ChoiceIntersection, Level::Faction, Protocol::ChannelID::ChoiceIntersection>		choiceIntersection(Receiver::ChoiceIntersection);
	TCP::Channel<Protocol::DropTasks, Level::Faction, Protocol::ChannelID::DropTasks>						dropTasks(Receiver::DropTasks);

	TCP::Channel<Protocol::StandardJob, Level::Faction, Protocol::ChannelID::SignalNewJob>					signalNewJob(Receiver::SignalNewJob);
	TCP::Channel<Protocol::TerrainMod, Level::Faction, Protocol::ChannelID::SignalTerrainMod>				signalTerrainMod(Receiver::SignalTerrainMod);
	TCP::Channel<Protocol::ConstructionAbort, Level::Faction, Protocol::ChannelID::ConstructionAbort>		constructionAbort(Receiver::ConstructionAbort);
	TCP::Channel<Protocol::ConstructionBegin, Level::Faction, Protocol::ChannelID::ConstructionBegin>		constructionBegin(Receiver::ConstructionBegin);
	TCP::Channel<Protocol::EnableStructure, Level::Faction, Protocol::ChannelID::EnableStructure>			enableStructure(Receiver::EnableStructure);
	TCP::Channel<Protocol::ClientFactionRequest, Level::PreFaction, Protocol::ChannelID::ClientFaction>		clientFactionRequest(Receiver::ClientFaction);
	
	TCP::Channel<NET(TUniqueID), Level::Faction, Protocol::ChannelID::RestoreCoreLink>						restoreCoreLink(Receiver::RestoreCoreLink);
	TCP::Channel<NET(TUniqueID), Level::Faction, Protocol::ChannelID::RemoveEntity>							removeEntity(Receiver::RemoveEntity);
	TCP::Channel<Protocol::EntityStateUpdate, Level::Faction, Protocol::ChannelID::Automate>				automate(Receiver::Automate);
	TCP::Channel<Protocol::EntityStateUpdate, Level::Faction, Protocol::ChannelID::Sleep>					sleep(Receiver::Sleep);
	TCP::Channel<Protocol::Marker, Level::Faction, Protocol::ChannelID::CreateMarker>						createMarker(Receiver::CreateMarker);
	TCP::Channel<NET(TUniqueID), Level::Faction, Protocol::ChannelID::DestroyMarker>						destroyMarker(Receiver::DestroyMarker);
	TCP::Channel<Protocol::TouchedMarker, Level::Faction, Protocol::ChannelID::TouchMarker>					touchMarker(Receiver::TouchMarker);
	TCP::Channel<Protocol::ClientName,Level::Faction,Protocol::ChannelID::ChangeMyName>						changeMyName(Receiver::ChangeMyName);
	TCP::Channel<NET(TUniqueID), Level::Faction, Protocol::ChannelID::IdentifyFaction>						identifyFactionRecv(Receiver::IdentifyFaction);
	TCP::Channel<Protocol::QueueResearch, Level::Faction, Protocol::ChannelID::QueueResearch>				queueResearch(Receiver::QueueResearch);
	TCP::Channel<Grid::ExtendedPlanetConfig, Level::Configurator, Protocol::ChannelID::PlanetConfig>		planetConfig(Receiver::PlanetConfig);
	TCP::Channel<Protocol::GameConfig, Level::Configurator, Protocol::ChannelID::GameConfig>				gameConfig(Receiver::GameConfig);
	TCP::Channel<Protocol::ResourceID, Level::PreFaction, Protocol::ChannelID::RequestResource>				requestResource(Receiver::RequestResource);
	TCP::Channel<Protocol::LoadPlanetByName, Level::Configurator, Protocol::ChannelID::LoadPlanetByName>	loadPlanetByName(Receiver::LoadPlanetByName);
	
	TCP::Channel<NET(INT32),Level::Faction,Protocol::ChannelID::TutorialAt>							tutorialAt(Receiver::TutorialAt);


	TCP::ObjectSender<Protocol::ClientFactionResponse, Protocol::ChannelID::ClientFaction>			clientFactionResponse;
	TCP::ObjectSender<Protocol::Marker, Protocol::ChannelID::MarkerCreationFailedLimitReached>		markerCreationFailedLimitReached;
	TCP::ObjectSender<Protocol::WorldDelta,Protocol::ChannelID::WorldDelta>							updateWorld;
	TCP::ObjectSender<Protocol::NewMobileEntity,Protocol::ChannelID::NewMobileEntity>				newMobileEntity;
	TCP::ObjectSender<Protocol::StructureConstruction,Protocol::ChannelID::InstantiateStructure>	createStructure;
	TCP::ObjectSender<Protocol::MobileState,Protocol::ChannelID::MobileState>						setMobileState;
	TCP::ObjectSender<Protocol::MobileEntityMotion,Protocol::ChannelID::MobileEntityMotion>			mobileEntityMotion;
	TCP::ObjectSender<Protocol::VagueAttackTrajectory,Protocol::ChannelID::VagueAttackTrajectory>		vagueAttackTrajectory;
	TCP::ObjectSender<Protocol::PreciseAttackTrajectory,Protocol::ChannelID::PreciseAttackTrajectory>	preciseAttackTrajectory;
	TCP::ObjectSender<Protocol::EntityTurretRotation,Protocol::ChannelID::EntityTurretRotation>			entityTurretRotation;
	TCP::ObjectSender<Protocol::Explosion,Protocol::ChannelID::Explosion>							explosion;
	TCP::ObjectSender<Protocol::StructureState,Protocol::ChannelID::UpdateStructure>				setStructureState;
	TCP::ObjectSender<Protocol::DestroyEntity,Protocol::ChannelID::DestroyEntity>					destroyEntity;
	TCP::ObjectSender<Protocol::ControlMessage,Protocol::ChannelID::ControlMessage>					controlMessage;
	TCP::ObjectSender<Protocol::PhaseChange,Protocol::ChannelID::PhaseChange>						phaseChange;
	TCP::ObjectSender<Protocol::ResourceUpdate,Protocol::ChannelID::ResourceUpdate>					resourceUpdate;
	TCP::ObjectSender<Protocol::GameProgress,Protocol::ChannelID::GameProgress>						gameProgress;
	TCP::ObjectSender<Protocol::ChangeEntityOwner,Protocol::ChannelID::ChangeEntityOwner>			changeEntityOwner;
	TCP::ObjectSender<Protocol::IdentifyFaction, Protocol::ChannelID::IdentifyFaction>				identifyFaction;
	TCP::ObjectSender<Protocol::ResearchState, Protocol::ChannelID::ResearchState>					researchState;
	TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::FactionOnline>							factionOnline;
	TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::FactionOffline>							factionOffline;
	TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::FactionRequestedNewRound>				factionRequestedNewRound;
	TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::FactionContinuedRound>					factionContinuedRound;
	TCP::ObjectSender<Protocol::ResourceData,Protocol::ChannelID::ResourceData>						resourceData;
	TCP::ObjectSender<NETA(Package::NetString<256>),Protocol::ChannelID::PlanetNames>				planetNames;

	TCP::ObjectSender<NET(UINT16), Protocol::ChannelID::LockEMPZones>								lockEmpZones;
	#ifndef NO_HIBERNATION
		TCP::ObjectSender<NET(UINT32), Protocol::ChannelID::UplinkBandwidth>						uplinkBandwidth;
	#endif

	TCP::ObjectSender<Protocol::UnitLimits,Protocol::ChannelID::UnitLimits>							unitLimits;
	TCP::ObjectSender<Protocol::OperationLacksResources,Protocol::ChannelID::OperationLacksResources>	operationLacksResources;
	TCP::ObjectSender<Protocol::ResearchQueue, Protocol::ChannelID::ResearchQueue>					researchQueue;

	TCP::ObjectSender<TUniqueID, Protocol::ChannelID::WinningFaction>								winningFaction;

	DECLARE_OBJECT_SENDER(VoteCreation);
	DECLARE_OBJECT_SENDER(VoteResult);
	DECLARE_FACTION_CHANNEL(ClientVote);
	DECLARE_FACTION_CHANNEL(AllianceJoinRequest);
	DECLARE_FACTION_CHANNEL(AllianceConfigChange);
	DECLARE_FACTION_CHANNEL(ChatMessage);
	
	DECLARE_OBJECT_SENDER(MyFaction);

	namespace Statistics
	{
		TCP::ObjectSender<TUniqueID, Protocol::ChannelID::BeginStatisticsSequence>					beginSequence;
		TCP::ObjectSender<Objects::Statistics::Round, Protocol::ChannelID::RoundStatistics>			round;
		TCP::ObjectSender<Objects::Statistics::EntityType, Protocol::ChannelID::EntityTypeStatistics>	entityType;
		TCP::ObjectSender<Objects::Statistics::FactionInteraction, Protocol::ChannelID::FactionInteractionStatistics>	factionInteraction;
	}


}






namespace Receiver
{

	static void ConvertReferenceEntity(const Objects::Marker&source, Protocol::MarkerReferenceEntity&e)
	{
		e.id = source.referenceEntity;
		e.owner = source.referenceEntityOwner;
		e.type = source.referencedType;
	}


	static void ConvertMarker(const Objects::Marker&source, Protocol::Marker&dest)
	{
		switch (source.type)
		{
			case 0:
				ConvertReferenceEntity(source, dest.data.Set0());
			break;
			case 1:
				ConvertReferenceEntity(source, dest.data.Set1());
			break;
			case 2:
				dest.data.Set2();
			break;
		}
		dest.createdInRound = source.createdInRound;
		dest.id = source.id;
		dest.location = source.location;
	}



	static index_t GetStartLocation()
	{
		return World::GetFactionStartLocation(random);
	}




	void	SendSessionConfigTo(TCP::Peer&peer, const Objects::PFaction&exclude)
	{
		Objects::ForeachFaction([&peer,exclude](const Objects::PFaction&f)
		{
			if (f != exclude)
			{

				Channels::identifyFaction.SendTo(peer,Network::Identify(f));
				if (f->online)
				{
					Channels::factionOnline.SendTo(peer,f->id);
					if (Network::playerMap.Require(f)->wantNewRound)
						Channels::factionRequestedNewRound.SendTo(peer,f->id);
				}
			}
//			Channels::researchState.SendTo(peer,f->researchState);
		});

		#ifndef NO_HIBERNATION
			Channels::uplinkBandwidth.SendTo(peer,State::Config::uplinkBandwidth);
		#endif
		Protocol::UnitLimits	limits;
		RuntimeConfig::UnitLimits::Export(limits);
		Channels::unitLimits.SendTo(peer,limits);
		
		Channels::lockEmpZones.SendTo(peer,Grid::empZonesLocked);


		Protocol::PhaseChange	ph;
		ph.newPhase = RuntimeConfig::phase;
		ph.roundNumber = RuntimeConfig::roundNumber;
		ph.timeFactor = -1;


		Channels::phaseChange.SendTo(peer,ph);

	}


	void	SendAllToSpectator(TCP::Peer&peer, bool firstSpectator)
	{
		Protocol::Timestamp	timestamp;
		timestamp.rightNow = true;

		peer.SendSignal(Protocol::ChannelID::BeginEverything);

		Objects::structures.Foreach([timestamp,&peer](const Objects::PStructure&s)
		{
			Protocol::StructureConstruction st;
			Game::Network::Describe(st,*s,true,timestamp);
			Channels::createStructure.SendTo(peer,st);
		});

		Objects::ForeachMobileWorldEntity([&peer, timestamp](const Objects::PMobileEntity&v)
		{
			Protocol::NewMobileEntity st;
			Game::Network::Describe(st,*v,true,timestamp,0xFFFFFFFF);
			Channels::newMobileEntity.SendTo(peer,st);
		},false);


		{
			Protocol::ResourceUpdate	package;
			package.timestamp = timestamp;
			package.cells.SetSize(Grid::LowSetup::GridType::CellCount);
			for (index_t i = 0; i < Grid::LowSetup::GridType::CellCount; i++)
				package.cells[i].cellIndex = (UINT32)i;
			foreach (Grid::Resource::all,res)
			{
				package.resourceName = res->GetCName();
				for (index_t i = 0; i < Grid::LowSetup::GridType::CellCount; i++)
				{
					BYTE sourceCov = res->coverage[i];
					package.cells[i].coverage = sourceCov;
				}

				Channels::resourceUpdate.SendTo(peer,package);
			}
		}
		static World::DeltaContainer	delta;
		if (firstSpectator)
		{
			World::spectatorWorldState.FlushTerrain();
			World::spectatorWorldState.FlushAndValidateSensory(1);
			delta.Process(World::spectatorWorldState);
		}
		else
			delta.CompressState(World::spectatorWorldState,Protocol::Timestamp(true,0));
		for (index_t i = 0; i < delta.numDeltas; i++)
		{
			delta.delta[i].timestamp.rightNow = true;
			Channels::updateWorld.SendTo(peer,delta.delta[i]);
		}

		SendSessionConfigTo(peer, Objects::PFaction());

		Objects::ForeachFaction([&peer](const Objects::PFaction&f)
		{
			Protocol::ResearchState state;
			state.ownerID = f->id;
			f->researchState.Export(state.serializedData);
			ASSERT_LESS__(state.serializedData.Count(),state.serializedData.MaxElementCount);
			Channels::researchState.SendTo(peer,state);
		});

		//if (RuntimeConfig::phase == RuntimeConfig::Phase::EndOfGame)
		Objects::ForeachFaction([&peer](const Objects::PFaction&f)
		{
			Game::Network::SendStatisticsTo(peer,f);
		},true);

		if (!Game::winner.expired())
			Channels::winningFaction.SendTo(peer,Game::winner.lock()->id);

		peer.SendSignal(Protocol::ChannelID::EndEverything);
	}

	void	SendEntitiesTo(const Objects::PFaction&f, TCP::Peer&peer, bool detailed, const Protocol::Timestamp&timestamp)
	{
		foreach (f->structures,_s)
		{
			Objects::PStructure s= _s->lock();
			ASSERT__(s);
			if (s)
			{
				Protocol::StructureConstruction st;
				Game::Network::Describe(st,*s,detailed,timestamp);
				Channels::createStructure.SendTo(peer,st);
			}
		}
		foreach (f->allMobiles,_v)
		{
			Objects::PMobileEntity v= _v->lock();
			ASSERT__(v);
			if (v)
			{
				Protocol::NewMobileEntity st;
				Game::Network::Describe(st,*v,detailed,timestamp,0xFFFFFFFF);
				Channels::newMobileEntity.SendTo(peer,st);
			}
		}
		foreach (f->storedMobiles,_v)
		{
			Objects::PMobileEntity v= _v->lock();
			ASSERT__(v);
			if (v)
			{
				Protocol::NewMobileEntity st;
				Game::Network::Describe(st,*v,detailed,timestamp,0xFFFFFFFF);
				Channels::newMobileEntity.SendTo(peer,st);
			}
		}
	}

	void	ClientFaction(Protocol::ClientFactionRequest&_p, TCP::Peer&peer)
	{
		if (!peer.attachment)
			return;

		if (RuntimeConfig::tutorialMode && !peer.IsLocalhost())
		{
			LogEvent(EventClass( EventSignificance::ClientFatal, EventFlavor::Loss ), peer,"Server is in tutorial mode. Rejecting connection from non-localhost.");
			peer.DisconnectPeer();
		}

		Network::PClient client = To<Network::Client>::Cast(peer.attachment);
		Protocol::ClientFactionResponse response;
		response.startFactionTypes = _p.startFactionTypes;
		response.factionToken = _p.factionToken;
		response.planetConfig = Grid::currentPlanetConfig;
		Game::Network::AssembleGameInfo(response.info, peer.IsLocalhost());
		//response.info = CompileWorldInfo();


		if (RuntimeConfig::phase == RuntimeConfig::Phase::EndOfGame)
			response.startFactionTypes = Protocol::FactionType::SpectatorFaction;	//automatic spectator status


		if (!response.startFactionTypes || response.startFactionTypes == Protocol::FactionType::SpectatorFaction)
		{
			if (Network::activePlayersAndSpectators.FindAndErase(client))
			{
				if (client->faction)
				{
					Journal::currentRound.LogFactionOffline(client->faction);
					LogEvent( EventClass( EventSignificance::Uncommon, EventFlavor::Loss ),peer,"Abandoning faction");
					Network::playerMap.Unset(client->faction);
					Channels::factionOffline.SendTo(Network::server,client->faction->id,2);
					client->faction->online = false;
					client->faction.reset();
				}
				Network::ChangeUserLevel(peer,Network::UserLevel::PreFaction);
			}
			if (!response.startFactionTypes)
			{
				Channels::clientFactionResponse.SendTo(client->peer.lock(), response);
				return;
			}
		}
		
		if (response.startFactionTypes != Protocol::FactionType::SpectatorFaction && client->isSpectator)
		{
			client->isSpectator = false;	
			Network::spectators.FindAndErase(client);
		}

		if (!RuntimeConfig::IsInAnyCommandPhase()
			&& RuntimeConfig::phase != RuntimeConfig::Phase::EndOfGame
			&& RuntimeConfig::phase != RuntimeConfig::Phase::AnimationPhase
			)
		{
			//disconnecting client. no new faction
			LogEvent(EventClass::Preset::commandRejected, peer, "Cannot connect ("+String(RuntimeConfig::phase.ToString())+") right now. Disconnecting");
			response.startFactionTypes = 0;
			Channels::clientFactionResponse.SendTo(client->peer.lock(), response);
			client->peer.lock()->DisconnectPeer();
			return;
		}


		if (Network::activePlayersAndSpectators.Contains(client))
			return;

		bool factionTypeNotSupported = true;

		switch (response.startFactionTypes)
		{
			case Protocol::FactionType::ContinueOnly:
				factionTypeNotSupported = false;
			break;
			case Protocol::FactionType::SpectatorFaction:
			{
				factionTypeNotSupported =	RuntimeConfig::phase != RuntimeConfig::Phase::EndOfGame &&
											(
												!peer.IsLocalhost()
												|| !(Game::Config::factionSupportFlags & Protocol::GameFlag::SupportSpectatorFactions)
											)
											;
				if (factionTypeNotSupported && !peer.IsLocalhost())
					LogEvent(EventClass::Preset::commandRejected,peer,"Spectator type is allowed only from localhost, but peer is "+peer.ToString());
			}
			break;
			case Protocol::FactionType::RegularFaction:
				factionTypeNotSupported = !(Game::Config::factionSupportFlags & Protocol::GameFlag::SupportRegularFactions);
			break;
		}


		if (factionTypeNotSupported)
		{
			LogEvent(EventClass::Preset::commandRejected, peer, "Requested faction type is not supported");
			response.startFactionTypes = 0;
			Channels::clientFactionResponse.SendTo(client->peer.lock(), response);
			client->peer.lock()->DisconnectPeer();
			return;
		}

		if (response.startFactionTypes == Protocol::FactionType::SpectatorFaction)
		{
			//authenticated and purged any previous faction. let's go
			Network::ChangeUserLevel(peer,ConfigurationMode::IsActive()?Network::UserLevel::PlanetConfigurator :  Network::UserLevel::Spectator);


			bool firstSpectator = Network::spectators.IsEmpty();
			if (!client->isSpectator)
			{
				Network::spectators << client;
				Network::activePlayersAndSpectators << client;
			}
			client->isSpectator = true;

			response.factionToken = CryptographicID();
			Channels::clientFactionResponse.SendTo(client->peer.lock(), response);

			//send world state:

			SendAllToSpectator(peer, firstSpectator);

			Channels::planetConfig.SendTo(peer,Grid::currentPlanetConfig);

			if (ConfigurationMode::IsActive())
			{
				Ctr::Array<String>	planetNames;
				World::GetAvailablePlanetNames(planetNames);
				NETA(Package::NetString<256>) names;
				names.SetSize(planetNames.Count());
				for (index_t i = 0; i < planetNames.Count(); i++)
					names[i] = planetNames[i];
				Channels::planetNames.SendTo(peer,names);
			}


			peer.SendSignal(Protocol::ChannelID::BeginGame);
			return;
		}

		Objects::PFaction	faction;

		bool newFaction = false;

		if (Game::Config::Nexus::enableAI && Objects::Faction::nexus->accessToken == response.factionToken && peer.IsLocalhost())
		{
			LogEvent(EventClass( EventSignificance::Important, EventFlavor::Gain ), peer, "Access to nexus faction permitted.");
			faction = Objects::Faction::nexus;
		}
		if (faction
			|| Network::tokenMap.Query(response.factionToken,faction)
			|| (	response.startFactionTypes == Protocol::FactionType::ContinueOnly 
					#ifndef _DEBUG
						&& RuntimeConfig::tutorialMode
					#endif
					&& Objects::Faction::all.IsNotEmpty()
					&& (faction = Objects::Faction::all.First())	//in tutorial mode (or debug mode) take first known faction
				)
			)
		{
			#ifdef _DEBUG
				response.factionToken = faction->accessToken;
			#endif
			Network::PClient other;
			if (Network::playerMap.Query(faction,other))
			{
				LogEvent(EventClass( EventSignificance::Important, EventFlavor::Loss ),peer,"Faction access token recognized, but faction is already being controlled. Disconnecting "+other->ConvertToString());

				if (faction == Objects::Faction::nexus)
				{
					//disconnecting client. no new faction
					LogEvent(EventClass( EventSignificance::Uncommon, EventFlavor::Loss ), peer, "Disconnecting AI client.");
					response.startFactionTypes = 0;
					Channels::clientFactionResponse.SendTo(client->peer.lock(), response);
					client->peer.lock()->DisconnectPeer();
					return;
				}
				other->peer.lock()->DisconnectPeer();
				Network::playerMap.Unset(faction);
				faction->online = false;
				other->faction.reset();
			}

			client->faction = faction;
			LogEvent(EventClass( EventSignificance::Common, EventFlavor::Gain ),peer,"Faction access token recognized.");
		}
		else
		{
			if (response.factionToken.IsSet())
			{
				LogEvent(EventClass( EventSignificance::Important, EventFlavor::UserError ),peer,"Provided token not recognized: "+response.factionToken.ToString());

				if (Game::Config::closedGame && RuntimeConfig::roundNumber > 0)
				{
					foreach (Objects::Faction::all,f)
					{
						ASSERT__((*f)->accessToken != response.factionToken);
						LogEvent(EventSignificance::Debug,peer,"Known token: "+(*f)->accessToken.ToString());
					}
				}

			}
			else
			{
				LogEvent(EventSignificance::Common,peer,"No faction token provided");
				response.factionToken.Generate();
			}
			newFaction = true;
		}

		if (RuntimeConfig::phase == RuntimeConfig::Phase::EndOfGame)
		{
			FATAL__("Should not be here");
			return;
		}


		index_t startLocation = InvalidIndex;
		if (newFaction)
		{
			if (response.startFactionTypes == Protocol::FactionType::ContinueOnly)
			{
				response.startFactionTypes = 0;
				Channels::clientFactionResponse.SendTo(peer,response);
				LogEvent(EventClass::Preset::commandRejected,peer,"Client chose to continue faction. No faction found that can be continued. Join-request rejected.");
				return;
			}

			if (Game::Config::closedGame && RuntimeConfig::roundNumber > 0)
			{
				response.startFactionTypes = 0;
				Channels::clientFactionResponse.SendTo(peer,response);
				LogEvent(EventClass::Preset::commandRejected,peer,"Game is closed. User does not have any valid access token. Join-request rejected.");
				return;
			}

			World::UpdateEMPZones(1);
			startLocation = GetStartLocation();
			if (startLocation == InvalidIndex)
			{
				World::UpdateEMPZones(0);
				response.startFactionTypes = 0;
				Channels::clientFactionResponse.SendTo(peer,response);
				LogEvent(EventClass::Preset::operationAborted,peer,"Unable to find valid starting location for new faction.");
				return;
			}
			//p.factionToken.value.Generate();	let's keep it even if it's unknown
			faction = client->faction = Objects::Faction::Create(TUniqueID::New(),TUniqueID::New());
			ApplicationInterface::SignalFactionsChanged();
			if (client->faction != Objects::Faction::nexus)
				client->faction->name = client->name;
			else
				client->faction->name = Objects::Faction::nexusName;

			LogEvent(EventClass( EventSignificance::Important, EventFlavor::Gain ),peer,"Created new faction.");

			client->faction->accessToken = response.factionToken;
			Network::tokenMap.Set(response.factionToken,client->faction);
		}
		else
		{
			if (faction != Objects::Faction::nexus)
				faction->name = client->name;
			else
				faction->name = Objects::Faction::nexusName;
		}
		Network::ChangeUserLevel(peer,Network::UserLevel::CommonFaction);

		Network::activePlayersAndSpectators << client;
		Network::playerMap.Set(client->faction,client);
		client->faction->online = true;
		client->wantNewRound = false;


		response.startFactionTypes = Protocol::FactionType::RegularFaction;
		if (client->faction == Objects::Faction::nexus)
			response.startFactionTypes |= Protocol::FactionType::NexusFaction;

		Channels::clientFactionResponse.SendTo(client->peer.lock(),response);
		{
			Protocol::MyFaction faction;
			faction.alliance.allianceID = client->faction->alliance->id;
			faction.alliance.flags = client->faction->alliance->config.ToFlags();
			faction.factionID = client->faction->id;
			Channels::MyFaction.SendTo(client->peer.lock(),faction);
		}
		if (RuntimeConfig::tutorialMode)
			Channels::tutorialAt.SendTo(client->peer.lock(),Game::tutorialAt);
		//CHEAT_SHOW_ALL_RESOURCES

		#ifdef CHEAT_SHOW_ALL_RESOURCES
			faction->visibility.resourcesDiscovered.Fill(true);

		#endif
	


		foreach(client->faction->markers, m)
		{
			Protocol::Marker	outMarker;
			ConvertMarker(*m, outMarker);
			Channels::createMarker.SendTo(peer, outMarker);
		}


		if (newFaction)
		{
			Game::SpawnFactionStartUnits(client->faction,startLocation);

		}

		Objects::ForeachFaction([&peer](const Objects::PFaction&f)
		{
			Protocol::ResearchState state;
			state.ownerID = f->id;
			f->researchState.Export(state.serializedData);
			ASSERT_LESS__(state.serializedData.Count(),state.serializedData.MaxElementCount);
			Channels::researchState.SendTo(peer,state);
		});
		{
			Protocol::ResearchQueue queue;
			client->faction->researchState.ExportResearchQueue(queue);
			Channels::researchQueue.SendTo(peer,queue);
		}

		#ifndef CHEAT_SHOW_ALL_RESOURCES
		if (!newFaction)
		#endif
		{
			Protocol::Timestamp	timestamp;
			timestamp.rightNow = true;

			static Ctr::Buffer0<UINT32>	final;
			final.Clear();
			auto&discovered = client->faction->knownResources.resourcesDiscovered;
			for (index_t i = 0; i < discovered.Count(); i++)
			{
				if (discovered[i])
					final << (UINT32)i;
			}

			if (final.IsNotEmpty())
			{
				Protocol::ResourceUpdate	package;
				package.timestamp = timestamp;
				package.cells.SetSize(final.Count());
				for (index_t i = 0; i < final.Count(); i++)
					package.cells[i].cellIndex = final[i];
				foreach (Grid::Resource::all,res)
				{
					package.resourceName = res->GetCName();
					for (index_t i = 0; i < final.Count(); i++)
					{
						BYTE sourceCov = res->coverage[final[i]];
						package.cells[i].coverage = sourceCov;
					}

					Channels::resourceUpdate.SendTo(peer,package);
				}
			}

		}
		static World::DeltaContainer staticDeltaContainer;
		//if (newFaction)
		//	staticDeltaContainer.Process(client->faction->knownWorld);
		//else
			staticDeltaContainer.CompressState(client->faction->knownWorld,Protocol::Timestamp(true,0));
		for (index_t i = 0; i< staticDeltaContainer.numDeltas; i++)
		{
			staticDeltaContainer.delta[i].timestamp.rightNow = true;
			Channels::updateWorld.SendTo(peer,staticDeltaContainer.delta[i]);
		}

		String why;
		if (Game::Config::basesRequired && faction->deathInRounds > 0)
			faction->CanLive(why);
		client->SendProgressUpdate(why,faction->alliance->GetNexusStats());


		Objects::ForeachFaction([&peer](const Objects::PFaction&f)
		{
			Channels::identifyFaction.SendTo(peer,Network::Identify(f));

		},false);


		if (!newFaction)
		{
			Protocol::Timestamp	timestamp;
			timestamp.rightNow = true;

			LogEvent(EventSignificance::Debug,"Sending known foreign contacts to "+ peer.ToString());
			client->faction->seeing.VisitAllKeys([&peer,client,timestamp](const Objects::PWorldEntity&e)
			{
				LogEvent(EventSignificance::Debug,e.get(),"Sending contact (@"+String(e.get())+")");
				if (e->IsDeadOrGhost())
				{
					//weird...? weirder...
					LogEvent(EventSignificance::Debug,e.get(),"Unit has bad health");
					return;
				}

				Objects::PStructure s = To<Objects::Structure>::DynamicCast(e);
				if (s)
				{
					Protocol::StructureConstruction st;
					Game::Network::Describe(st,*s,(s)->owner->ExposesEntityDetailsTo(client->faction),timestamp);
					Channels::createStructure.SendTo(peer,st);
				}
				else
				{
					Objects::PMobileEntity v = To<Objects::MobileEntity>::DynamicCast(e);
					if (v)
					{
						Protocol::NewMobileEntity st;
						Game::Network::Describe(st,*v,(v)->owner->ExposesEntityDetailsTo(client->faction),timestamp,0xFFFFFFFF);
						Channels::newMobileEntity.SendTo(peer,st);
					}
					else
						FATAL__("WTF");

				}
			});

			//friendly units will be registered as contacts, therefor they should not be send otherwise
		/*	client->faction->alliance->ForeachMember([client,&peer,timestamp](const Objects::PFaction&other)
			{
				SendEntitiesTo(other, peer,client->faction->alliance->config.exposeEntityDetails,timestamp);
			});*/
			SendEntitiesTo(client->faction,peer,true,timestamp);
		}


		



		//faction->online = true;
		//probably optional later on
		{
			//broadcast name and state

			Channels::identifyFaction.SendTo(Network::server,Network::Identify(client->faction),Channels::Level::Faction);
			Channels::factionOnline.SendTo(Network::server,client->faction->id,2);

		}

		if (newFaction)
			Journal::currentRound.LogFactionCreation(client->faction,Journal::GetType(client->faction));
		Journal::currentRound.LogFactionOnline(client->faction);
		LogEvent(EventClass( EventSignificance::Uncommon, EventFlavor::Gain ),faction->name+" connected");

		SendSessionConfigTo(peer,client->faction);

		Game::Vote::AdvertiseAllToNewlyConnectedPeer(client->faction,peer);
		
		peer.SendSignal(Protocol::ChannelID::BeginGame);
	}

	//bool CanSee(const TCP::PPeer&p, const Objects::WorldEntity&e)
	//{
	//	if (!p->attachment)
	//		return false;
	//	return CanSee(To<State::Client>::Cast(p->attachment),e);
	//}
	//bool CanSee(const TCP::PPeer&p, const Objects::PWorldEntity&e)
	//{
	//	return CanSee(p,*e);
	//}

	bool	CheckCommandMode(TCP::Peer&peer)
	{
		if (RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase)
		{
			return true;
		}
		LogEvent(EventClass::Preset::commandRejected ,peer,"Cannot issue commands in current phase ("+String(RuntimeConfig::phase.ToString())+")");
		return false;
	}

	Objects::PWorldEntity	CheckCommandModeGetAccessibleEntity(const TUniqueID&eid, TCP::Peer&peer)
	{
		if (!CheckCommandMode(peer))
			return nullptr;

		Objects::PWorldEntity e = Objects::GetEntity(eid);
		if (!e)
		{
			//LogEvent(EventClass::Preset::commandRejected ,peer,"Tried to access non-existent entity");
			for (auto f : Objects::Faction::all)
			{
				if (f->FindMarker(eid))
				{
					LogEvent(EventClass::Preset::commandRejected, peer, "Access denied (this is a marker/ghost/contact)");
					//LogEvent(EventSignificance::Debug, peer, "(because it doesn't exist)");
					return nullptr;
				}
			}


			LogEvent(EventClass::Preset::commandRejected, peer, "Access denied");
			LogEvent(EventSignificance::Debug, peer, "(because it doesn't exist)");
			return nullptr;
		}

		if (!Network::PeerCanControl(peer,e))
		{
			LogEvent(EventClass::Preset::commandRejected,peer,e,"Access denied");
			return nullptr;
		}

		{
			if (e->IsHibernating())
			{
				LogEvent(EventClass::Preset::commandRejected,peer,e,"No reaction. Entity is hibernating.");
				return nullptr;
			}
		}


		return e;
	}

	void	CreateMarker(Protocol::Marker&m, TCP::Peer&peer)
	{
		LogEvent(EventSignificance::Common, peer, nullptr, "Begin: " __func__);


		if (!peer.attachment)
		{
			LogEvent(EventClass::Preset::commandRejected, peer, "Marker creation denied. Player not ready");
			return;
		}
		const Objects::PFaction&f = To<Network::Client>::Cast(peer.attachment)->faction;
		if (!f)
		{
			LogEvent(EventClass::Preset::commandRejected, peer, "Marker creation denied. Player has no faction");
			return;
		}
		if (!f->CanCreateMarker(f == Objects::Faction::nexus))
		{
			LogEvent(EventSignificance::Debug, peer, "Marker creation denied. Limit reached");
			Channels::markerCreationFailedLimitReached.SendTo(peer, m);
			return;
		}
		bool failed = false;
		static_assert(Objects::Marker::Type::NumberOfPossibilities == 3, "Please adjust marker logic");
		const Protocol::MarkerReferenceEntity*e = m.data.Get0();
		Protocol::MarkerReferenceEntity ref;
		ref.type = Objects::EntityType();
		Objects::Marker::Type type;
		TUniqueID	refID = TUniqueID::Empty();
		if (e)
		{
			type = Objects::Marker::Type::Reinterpret(0);
			refID = e->id;
		}
		else
		{
			e = m.data.Get1();
			if (e)
			{
				type = Objects::Marker::Type::Reinterpret(1);
				refID = e->id;
			}
			else
			{
				if (m.data.Get2())
					type = Objects::Marker::Type::Reinterpret(2);
				else
				{
					failed = true;
				}
			}

		}
		if (e)
			ref = *e;
		ref.id = refID;

		if (failed)
		{
			LogEvent(EventClass::Preset::commandRejected, peer, "Marker creation rejected. Invalid marker configuration");
			return;
		}

		Journal::currentRound.Register(true,f,new Journal::Round::CreateMarker(m.id,type,m.createdInRound,m.location,ref));
		//Channels::createdMarker.SendTo(peer, outMarker);
	}


	void	DestroyMarker(NET(TUniqueID)&id, TCP::Peer&peer)
	{
		LogEvent(EventSignificance::Common, peer, nullptr, "Begin: " __func__);

		if (!peer.attachment)
		{
			LogEvent(EventClass::Preset::commandRejected, peer, "Marker destruction denied. Player not ready");
			return;
		}
		const Objects::PFaction&f = To<Network::Client>::Cast(peer.attachment)->faction;
		if (!f)
		{
			LogEvent(EventClass::Preset::commandRejected, peer, "Marker destruction denied. Player has no faction");
			return;
		}

		Journal::currentRound.Register(true,f,new Journal::Round::DestroyMarker(id));
	}

	void	ChatMessage(Protocol::ChatMessage&message, TCP::Peer&peer)
	{
		if (!peer.attachment)
			return;
		Network::PClient client = To<Network::Client>::Cast(peer.attachment);
		if (!client->faction)
			return;
		if (message.recipient.IsEmpty())
			LogEvent(EventClass( EventSignificance::Important, EventFlavor::ChatMessage ),client->faction,message.message,Logging::Origin::Local, Logging::Privacy::Public);
		else
		{
			LogEvent(EventClass( EventSignificance::Common, EventFlavor::ChatMessage ),client->faction,message.message,Logging::Origin::Local, Logging::Privacy::ServerPrivate);

			auto a = Objects::Alliance::FindByID(message.recipient);
			if (a)
			{
				a->ForeachMember([&message,client](const Objects::PFaction&f)
				{
					Network::PClient other;
					if (Network::playerMap.Query(f,other))
					{
						auto p = other->peer.lock();
						if (p)
							Logging::SendEntityEvent( p.get(),
								EventClass( EventSignificance::Important, EventFlavor::ChatMessage ), 
								nullptr, 
								client->faction, 
								message.message, 
								Logging::Privacy::ClientPrivate);
					}
				});
				return;
			}

			auto f = Objects::Faction::FindByID(message.recipient);
			if (f)
			{
				Network::PClient other;
				if (Network::playerMap.Query(f,other))
				{
					auto p = other->peer.lock();
					if (p)
						Logging::SendEntityEvent( p.get(),
							EventClass( EventSignificance::Important, EventFlavor::ChatMessage ), 
							nullptr, 
							client->faction, 
							message.message, 
							Logging::Privacy::ClientPrivate);
				}
			}

		}

	}

	void	QueueResearch(Protocol::QueueResearch&research, TCP::Peer&peer)
	{
		if (!peer.attachment)
			return;
		Network::PClient client = To<Network::Client>::Cast(peer.attachment);
		if (!client->faction)
			return;
		Journal::currentRound.Register(true,client->faction,new Journal::Round::QueueResearch(Objects::ResearchState::GetTopic( research.research),research.drop));
	}

	void	IdentifyFaction(NET(TUniqueID)&id, TCP::Peer&peer)
	{
		//LATER
	}

	void	PlanetConfig(Grid::ExtendedPlanetConfig&cfg, TCP::Peer&peer)
	{
		if (ConfigurationMode::IsActive())
		{
			LogEvent(EventClass::Preset::configurationPrint,peer,"Received planet configuration (seed="+String(cfg.lastSeed)+"). Rebuilding...");
			ConfigurationMode::SetPlanetSeed(cfg.lastSeed);
			Grid::currentPlanetConfig = cfg;
			Network::BuildConfiguredWorld();
		}
		else
			LogEvent(EventClass::Preset::commandRejected,peer,"Configuration mode is not active. User should not have this level");

	}

	void	TutorialAt(NET(INT32)&at)
	{
		Game::tutorialAt = at;
	}

	void	LoadPlanetByName(Protocol::LoadPlanetByName&load, TCP::Peer&peer)
	{
		if (ConfigurationMode::IsActive())
		{
			LogEvent(EventClass::Preset::configurationPrint,peer,"Received request for planet preset '"+load.name+"'...");
			if (load.name.Contains('/') || load.name.Contains('\\'))
			{
				LogEvent(EventClass::Preset::commandRejected,peer,"Planet name contained path separators. Rejecting request.");
				return;
			}

			if (load.name.IsNotEmpty())
			{
				if (!World::IsPlanetIniFile(load.name+".ini"))
				{
					LogEvent(EventClass::Preset::remoteControlError,"'"+load.name+"' is not a valid planet type");
					return;
				}
			}

			if (load.name.IsNotEmpty())
			{
				Reflection::Variable::planetType->Set(load.name.Trim());
				Reflection::ReloadPlanet();
				Grid::currentPlanetConfig.versionCounter = load.configVersion;
			}
			Network::BuildConfiguredWorld();
		}
		else
			LogEvent(EventClass::Preset::commandRejected,peer,"Configuration mode is not active. User should not have this level");
	}


	void	GameConfig(Protocol::GameConfig&cfg, TCP::Peer&peer)
	{
		if (ConfigurationMode::IsActive())
		{
			UINT32 flags = cfg.gameFlags;
			LogEvent(EventClass::Preset::configurationPrint,peer,"Received game flags (="+String(flags)+"). Updating...");
			Game::Config::basesRequired = !!(flags & Protocol::GameFlag::BasesRequired);
			Game::Config::closedGame = !!(flags & Protocol::GameFlag::ClosedGame);
			Game::Config::startBases = !!(flags & Protocol::GameFlag::StartBases);
			World::Config::simulateLiquids = !!(flags & Protocol::GameFlag::SimulateLiquids);
			Game::Config::Nexus::enableAI = !!(flags & Protocol::GameFlag::NexusAI);
			bool eOld = RuntimeConfig::empZones;
			RuntimeConfig::empZones = !!(flags & Protocol::GameFlag::EMPZones);
			if (RuntimeConfig::empZones)
				World::SetUnlockedEMPZones(1);
			else
				World::SetUnlockedEMPZones(World::NumZones);

			if (RuntimeConfig::gameGoal != cfg.gameGoal || RuntimeConfig::empZones != eOld)
			{
				RuntimeConfig::gameGoal.Load(cfg.gameGoal);
				Network::BuildConfiguredWorld();
			}
			World::Config::worldDiscovery.Load(cfg.worldDiscovery);
		}
		else
			LogEvent(EventClass::Preset::commandRejected,peer,"Configuration mode is not active. User should not have this level");

	}

	void	RequestResource(Protocol::ResourceID&id, TCP::Peer&peer)
	{
		LogEvent(EventClass::Preset::common,peer,"Received resource request for resource "+Bin2Str::ConvertPOD(id));

		ScriptCache::PScript script = ScriptCache::Lookup(id);
		if (script)
		{
			LogEvent(EventClass::Preset::common,peer,"Resource "+Bin2Str::ConvertPOD(id)+" is script. Dispatching "+String(script->compressed.Count())+" bytes");
			Protocol::ResourceData data;
			data.data = script->compressed;	//stupid copy...
			data.id = id;
			Channels::resourceData.SendTo(peer,data);
			return;
		}
		LogEvent(EventClass::Preset::common,peer,"Resource "+Bin2Str::ConvertPOD(id)+" is unknown");
	}

	
	void	ChangeMyName(Protocol::ClientName&name, TCP::Peer&peer)
	{
		LogEvent(EventSignificance::Common, peer, nullptr, "Begin: " __func__);
		if (!peer.attachment)
		{
			LogEvent(EventClass::Preset::commandRejected, peer, "Name change denied. Player not ready");
			return;
		}
		Network::PClient cl = To<Network::Client>::Cast(peer.attachment);
		if (cl->faction != Objects::Faction::nexus)
			cl->name = name;
		else
			cl->name = Objects::Faction::nexusName;

		if (cl->faction)
		{
			cl->faction->name = cl->name;
			Channels::identifyFaction.SendTo(Network::server,Network::Identify(cl->faction),Channels::Level::Faction);
			LogEvent(EventClass( EventSignificance::Uncommon, EventFlavor::Gain ), peer, "User renamed to '"+cl->name+"'");
		}
	}

	void	TouchMarker(Protocol::TouchedMarker&m, TCP::Peer&peer)
	{
		LogEvent(EventSignificance::Common, peer, nullptr, "Begin: " __func__);

		if (!peer.attachment)
		{
			LogEvent(EventClass::Preset::commandRejected, peer, "Marker touch denied. Player not ready");
			return;
		}
		const Objects::PFaction&f = To<Network::Client>::Cast(peer.attachment)->faction;
		if (!f)
		{
			LogEvent(EventClass::Preset::commandRejected, peer, "Marker touch denied. Player has no faction");
			return;
		}
		Objects::Marker *marker = f->FindMarker(m.id);
		if (marker)
		{
			LogEvent(EventSignificance::Common, peer, "Touching " + marker->GetName()+" to round number "+m.newRoundNumber.ToString());
			marker->createdInRound = m.newRoundNumber;
			//Protocol::TouchedMarker m;
			//m.id = id;
			//m.newRoundNumber = marker->createdInRound;
			//Channels::touchedMarker.SendTo(peer,m);	//knows already
		}
		else
			LogEvent(EventClass::Preset::commandRejected, peer, "Marker " + ToString(m.id) + " not found for touch");
	}


	void	Automate(Protocol::EntityStateUpdate&a, TCP::Peer&peer)
	{
		LogEvent(EventSignificance::Common,peer,nullptr,"Begin: Automate ("+String(a.doSet)+") entity "+ToString(a.entityID));

		Objects::PWorldEntity e = CheckCommandModeGetAccessibleEntity(a.entityID,peer);
		if (!e)
			return;


		Objects::PMobileEntity v = To<Objects::MobileEntity>::DynamicCast(e);
		if (!v)
		{
			LogEvent(EventClass::Preset::commandRejected,peer,e,"Tried to automate immobile entity");
			return;
		}
		if (v->isAutomated == a.doSet)
			return;
		Journal::currentRound.RegisterAutomationChange(true,v,a.doSet);
	}

	void	Sleep(Protocol::EntityStateUpdate&a, TCP::Peer&peer)
	{
		LogEvent(EventSignificance::Common,peer,nullptr,"Begin: Sleep ("+String(a.doSet)+") entity "+ToString(a.entityID));

		Objects::PWorldEntity e = CheckCommandModeGetAccessibleEntity(a.entityID,peer);
		if (!e)
			return;

		if (e->IsSleeping() == a.doSet)
		{
			e->RegisterChange(Objects::WorldEntity::SleepChange,0.f);
			return;
		}
		Journal::currentRound.RegisterSleepChange(true,e,a.doSet);
	}

	void	EnableStructure(Protocol::EnableStructure&es, TCP::Peer&peer)
	{
		LogEvent(EventSignificance::Common,peer,nullptr,"Begin: "+String(es.enable?"Enable":"Disable")+" structure "+ToString(es.id));

		Objects::PWorldEntity e = CheckCommandModeGetAccessibleEntity(es.id,peer);
		if (!e)
			return;


		Objects::PWorkingStructure w = To<Objects::WorkingStructure>::DynamicCast(e);
		if (!w)
		{
			LogEvent(EventClass::Preset::commandRejected,peer,e,"Tried to enable/disable entity that is not a working structure");
			return;
		}
		Journal::currentRound.Register(true,w,new Journal::Round::EnableStructure(es.enable));
	}

	void	RestoreCoreLink(NET(TUniqueID)&uuid, TCP::Peer&peer)
	{
		LogEvent(EventSignificance::Common,peer,nullptr,"Begin: Restoring mine core link "+ToString(uuid));

		Objects::PWorldEntity e = CheckCommandModeGetAccessibleEntity(uuid, peer);
		if (!e)
			return;

		Objects::PMine m = To<Objects::Mine>::DynamicCast(e);
		if (!m)
		{
			LogEvent(EventClass::Preset::commandRejected,peer,e,"Invalid operation on entity");
			return;
		}	
		Journal::currentRound.Register(true,m, new Journal::Round::RestoreCoreLink());

	}
	void	RemoveEntity(NET(TUniqueID)&uuid, TCP::Peer&peer)
	{
		LogEvent(EventSignificance::Common,peer,nullptr,"Begin: Removing entity "+ToString(uuid));

		Objects::PWorldEntity e = CheckCommandModeGetAccessibleEntity(uuid,peer);
		if (!e)
			return;
		if (RuntimeConfig::tutorialMode)
		{
			LogEvent(EventClass::Preset::commandRejected,peer,e,"Unable to issue self destruction. Tutorial mode is active");
			return;
		}
		if (e->IsIndestructible())
		{
			LogEvent(EventClass::Preset::commandRejected,peer,e,"Unable to issue self destruction. Entity is indestructible");
			return;
		}
		Journal::currentRound.RegisterEntityDestruction(true,e,e->isInserted ? Objects::RemovalType::ControlledExplosion : Objects::RemovalType::DestroyedInStorage);
	}

	void	ConstructionAbort(Protocol::ConstructionAbort&ca, TCP::Peer&peer)
	{
		LogEvent(EventSignificance::Common,peer,nullptr,"Begin: Aborting vehicle plant construction for entity "+ToString(ca.plantID));

		Objects::PWorldEntity e = CheckCommandModeGetAccessibleEntity(ca.plantID,peer);
		if (!e)
			return;

		//Objects::PWorkingStructure ws = To<Objects::WorkingStructure>::DynamicCast(e);

		Objects::PMobileEntityProducer p = To<Objects::MobileEntityProducer>::DynamicCast(e);
		if (!p)
		{
			LogEvent(EventClass::Preset::commandRejected,peer,e,"Tried to abort construction-plant job on non-plant entity ");
			return;
		}
		Journal::currentRound.Register(true,e,new Journal::Round::ConstructionAbort(ca.taskId));
	}


	void	ConstructionBegin(Protocol::ConstructionBegin&cb, TCP::Peer&peer)
	{
		LogEvent(EventSignificance::Common,peer,nullptr,"Begin: Registering mobile producer construction, given type "+ToString(cb.type)+", for plant "
				+ToString(cb.plantID));

		Objects::PWorldEntity e = CheckCommandModeGetAccessibleEntity(cb.plantID,peer);
		if (!e)
			return;

		Objects::PMobileEntityProducer p = To<Objects::MobileEntityProducer>::DynamicCast(e);
		if (p)
		{
			Journal::currentRound.RegisterProductionJobStart(true,e,p,cb.type);
			return;
		}
		LogEvent(EventClass::Preset::commandRejected,peer,e,"Tried to issue construction job on incompatible entity");
		return;

	}

	Objects::Component::operation_t GetOperationAffinity(ConstName cname)
	{
		switch (cname)
		{
			case ConstName::Job_BuildRoad:
			case ConstName::Job_BuildWallUp:
			case ConstName::Job_BuildHorizontalWall:
				return Objects::Component::TerrainAdd;
			case ConstName::Job_DigDown:
				return Objects::Component::TerrainRemove;
			case ConstName::Job_Construct:
				return Objects::Component::Construct;
			case ConstName::Job_SelfDetonate:
				return Objects::Component::Nuclear;
		}
		return Objects::Component::TerrainRemove;	//shouldn't happen, but not our case to handle here
	}

	void	SignalNewJob(Protocol::StandardJob&sjob, TCP::Peer&peer)
	{
		LogEvent(EventSignificance::Common,peer,nullptr,"Begin: Registering new Job #"+String(int(sjob.jobName))+" for vehicle "+ToString(sjob.ownerID));

		Objects::PWorldEntity e = CheckCommandModeGetAccessibleEntity(sjob.ownerID,peer);
		if (!e)
			return;
		Objects::PMobileEntity v = To<Objects::MobileEntity>::DynamicCast(e);
		if (!v)
		{
			LogEvent(EventClass::Preset::commandRejected,peer,e,"Tried to issue mobile entity job on non-vehicle entity");
			return;
		}
		if (!v->GetOperationSpeed(GetOperationAffinity(sjob.jobName)))
		{
			LogEvent(EventClass::Preset::commandRejected,peer,e,"This mobile entity cannot perform the requested operation");
			return;
		}
		if (v->myTasks.Count() >= Objects::MaxProductionTasks)
		{
			LogEvent(EventClass::Preset::commandRejected,peer,e,"Max queue length ("+String(Objects::MaxProductionTasks)+") reached. Task rejected");
			return;
		}

		/*
			class BuildRoad : public Vehicle::BaseJob
		
			class AlterTerrain : public Vehicle::BaseJob
		
			class BuildFoundation : public Vehicle::BaseJob
		
			class TransferJob : public Vehicle::BaseJob
			
			class ClearAbove : public Vehicle::BaseJob
			
			class DigHorizontal : public Vehicle::BaseJob
			
			class DigDown : public Vehicle::BaseJob
				class Construct : public Vehicle::BaseJob
		
		
			class Sequence : public PathJob
		*/
		using namespace Objects;
		PVehicleJob job;
		Grid::LCellIdI target = Grid::LCellIdI::Invalid;
		const Grid::LCellIdI jobLocation = v->GetLastPointOfOrigin();
		switch (sjob.jobName)
		{
			case ConstName::Job_BuildRoad:
			{
				if (jobLocation.height <= -Grid::CellStack::HalfRange)
				{
					LogEvent(EventClass::Preset::commandRejected,peer,e,"Cannot build road on bottom layer");
					return;
				}

				job.reset(new VehicleJob::BuildRoad(*v,jobLocation));
				v->RegisterNewJob("", job,nullptr);
				LogEvent(EventSignificance::Common,peer,e,"Registered new job "+job->GetName());
			}
			break;
			case ConstName::Job_BuildWallUp:
			{
				target = v->GetLastPointOfOrigin()+1;	//has moveToFocusWhenDone, focus must be set to where vehicle will move
				if (target.height >= Grid::CellStack::HalfRange)
				{
					LogEvent(EventClass::Preset::commandRejected,peer,e,"Cannot build wall on top layer");
					return;
				}
				job.reset(new VehicleJob::BuildWallUp(*v,jobLocation));
				v->RegisterNewJob("", job,&target);
				LogEvent(EventSignificance::Common,peer,e,"Registered new job "+job->GetName());
			}
			break;
			case ConstName::Job_BuildHorizontalWall:
			{
				job.reset(new VehicleJob::BuildHorizontalWall(*v,jobLocation));
				v->RegisterNewJob("", job,nullptr);
				LogEvent(EventSignificance::Common,peer,e,"Registered new job "+job->GetName());
			}
			break;
			case ConstName::Job_DigDown:
			{
				target = v->GetLastPointOfOrigin()-1;
				if (target.height < -Grid::CellStack::HalfRange)
				{
					LogEvent(EventClass::Preset::commandRejected,peer,e,"Bottom layer reached. Cannot dig any further");
					return;
				}
				else
				{
					job.reset(new VehicleJob::DigDown(*v,jobLocation));
					v->RegisterNewJob("",job,&target);		
					LogEvent(EventSignificance::Common,peer,e,"Registered new job "+job->GetName());
				}
			}
			break;
			case ConstName::Job_Construct:
			{
				if (v->GetLastPointOfOrigin().height < Grid::CellStack::BottomHeight)
				{
					LogEvent(EventClass::Preset::commandRejected,peer,e,"Cannot build below bottom layer");	//would be weird, but okay
					return;
				}

				target = v->GetLastPointOfOrigin();

				Objects::PStructure p = Objects::Prototypes::structures.Find(sjob.parameterName);
				if (p)
				{	
					job.reset(new VehicleJob::Construct(*v,jobLocation,p));

					v->RegisterNewJob(p->GetTypeName(),job,&target);
					LogEvent(EventSignificance::Common,peer,e,"Registered new job "+job->GetName());
				}
				else
				{
					LogEvent(EventClass::Preset::commandRejected,peer,v,"Unable to construct structure type");
					Network::InvalidOperation(peer);
				}
			}
			break;
			case ConstName::Job_SelfDetonate:
			{
				job.reset(new VehicleJob::SelfDetonate(*v,jobLocation,sjob.parameterName.baseType));
				target = v->GetLastPointOfOrigin();
				v->RegisterNewJob("", job,&target);
				LogEvent(EventSignificance::Common,peer,e,"Registered new job "+job->GetName());
			}
			break;
			default:
			{
				LogEvent(EventClass::Preset::commandRejected,peer,v,"Illegal job name #"+String(int(sjob.jobName)));
				Network::InvalidOperation(peer);
			}
			return;
		}
		if (job)
			Journal::currentRound.LogJobCommandStart(e,*job,target);

	}


	void	SignalTerrainMod(Protocol::TerrainMod&mod, TCP::Peer&peer)
	{
		LogEvent(EventSignificance::Common,peer,nullptr,"Begin: Registering new AlterTerrain for vehicle "+ToString(mod.id)+", mode="+String(mod.mode)+", numCells="+String(mod.cells.Count()));

		Objects::PWorldEntity e = CheckCommandModeGetAccessibleEntity(mod.id,peer);
		if (!e)
			return;

		Objects::PVehicle v = To<Objects::Vehicle>::DynamicCast(e);
		if (!v)
		{
			LogEvent(EventClass::Preset::commandRejected,peer,e,"Tried to issue vehicle job on non-vehicle entity");
			return;
		}

		Objects::CursorMode mode = (Objects::CursorMode)mod.mode;

		if (!v->turret->CanDo(Objects::VehicleJob::AlterTerrain::GetOperationAffinity(mode)))
		{
			LogEvent(EventClass::Preset::commandRejected,peer,e,"This vehicle cannot shape terrain");
			return;
		}


		foreach (mod.cells,cell)
		{
			if (!Grid::ValidCell(*cell))
			{
				LogEvent(EventClass::Preset::commandRejected,peer,e,"Parameter fault");
				return;
			}
		}
	
		Objects::PVehicleJob job(new Objects::VehicleJob::AlterTerrain(*v,mode,mod.cells));

		v->RegisterNewJob("",job,nullptr);

		if (job)
			Journal::currentRound.LogJobCommandStart(e,*job,Grid::LCellIdI::Invalid);


		LogEvent(EventSignificance::Common,peer,e,"End: Registered new job "+job->GetName());
	}

	bool	ValidCell(const Grid::LCellIdI&loc)
	{
		if (loc.stackID >= Grid::LowSetup::GridType::CellCount)
			return false;
		if (loc.height >= Grid::CellStack::BottomHeight && loc.height <= Grid::CellStack::TopHeight)
			return true;
		#ifndef NO_SATELLITES
			if (Objects::Satellite::IsSatelliteHeight(loc.height))
				return true;
		#endif
		return false;
	}

	void	ChoiceIntersection(Protocol::ChoiceIntersection&ch, TCP::Peer&peer)
	{
		LogEvent(EventSignificance::Common,peer,nullptr,"Begin: Processing choice intersection in mode "+String(ch.mode)+" at cell "+ToString(ch.target)
			+" for entity "+ToString(ch.id));

		Objects::PWorldEntity e = CheckCommandModeGetAccessibleEntity(ch.id,peer);
		if (!e)
			return;


		if (!ValidCell(ch.target))
		{
			LogEvent(EventClass::Preset::commandRejected,peer,e,"Parameter fault");
			return;
		}



		const NET(TUniqueID)*deploying = ch.deployingWhat.Get0();
		Objects::PMobileEntity deployingWhat;
		
		if (deploying)
		{
			deployingWhat = To<Objects::MobileEntity>::DynamicCast(Objects::GetEntity(*deploying));
			ASSERT1__(deployingWhat,*deploying);
		}

		if (To<Objects::Hangar>::DynamicCast(e))
			ASSERT_NOT_NULL1__(deploying,e->GetName());

		Journal::currentRound.RegisterChoiceIntersection(true,e,ch.target,(Objects::Vehicle::mode_t)ch.mode,deployingWhat);

		LogEvent(EventSignificance::Common,peer,e,"End: Processed choice intersection in mode "+String(ch.mode)+" at cell "+ToString(ch.target));
	}

	
	void	DropTasks(Protocol::DropTasks&drop, TCP::Peer&peer)
	{
		Protocol::DropMode mode;
		
		if (!mode.Load(drop.dropMode))
		{
			LogEvent(EventClass::Preset::unexpectedBehavior,peer,"Invalid drop mode #"+String(drop.dropMode));
			return;
		}
		
		LogEvent(EventSignificance::Common,peer,nullptr,"Begin: Drop tasks ("+String(mode.ToString())+") of entity "+ToString(drop.targetID));

		Objects::PWorldEntity e = CheckCommandModeGetAccessibleEntity(drop.targetID,peer);
		if (!e)
			return;



		Objects::PMobileEntity v = To<Objects::MobileEntity>::DynamicCast(e);
		if (v)
		{
			if (v->myTasks.IsEmpty())
			{
				LogEvent(EventSignificance::Common,peer,e,"Nothing to undo");
				return;
			}
			if (v->HasUnsafeTask() && v->myTasks.Count() == 1 && mode == Protocol::DropMode::AllSafeTasks)
			{
				LogEvent(EventSignificance::Common,peer,e,"Nothing to safely undo");
				return;
			}
		}
		else
		{
			Objects::PTower t = To<Objects::Tower>::DynamicCast(e);
			if (t)
			{
				if (t->attackQueue.IsEmpty())
				{
					LogEvent(EventSignificance::Common,peer,e,"Nothing to undo");
					return;
				}
			}
			else
			{
				LogEvent(EventClass::Preset::commandRejected,peer,e,"Tried to drop command on incompatible entity");
				return;
			}
		}
		Journal::currentRound.RegisterDropEntityCommands(true,e, mode);
	}



	/**
	Handles client directing world entities in various modes.
	Most commonly this moves a vehicle, but may also schedule an attack or set a gathering point
	*/
	void	EntityDirectCommand(Protocol::EntityDirectCommand&cmd, TCP::Peer&peer)
	{
		static StringBuffer buffer;
		buffer.Clear();
		foreach (cmd.path,n)
			buffer << ' '<<ToString(*n);
		LogEvent(EventSignificance::Common,peer,nullptr,"Begin: Directing entity "+ToString(cmd.id)+" in mode "+String(cmd.mode)+" given flags "+String(cmd.directFlags)+" to"+buffer.CopyToString());

		Objects::PWorldEntity e = CheckCommandModeGetAccessibleEntity(cmd.id,peer);
		if (!e)
			return;
		foreach (cmd.path,n)
			if (!ValidCell(*n))
			{
				LogEvent(EventClass::Preset::commandRejected, peer, e, "Parameter fault ("+ToString(*n)+")");
				return;
			}
		const bool leftClick = (cmd.directFlags & Protocol::DirectFlags::LeftClick) != 0;
		const bool detailed = (cmd.directFlags & Protocol::DirectFlags::Detailed) != 0;
		const bool clearSafeTasks = (cmd.directFlags & Protocol::DirectFlags::ClearSafeTasks) != 0;
		//if (!e->DirectTo((Objects::Vehicle::mode_t)cmd.mode.value,cmd.path,detailed,leftClick))
		//{
		//	LogEvent(EventClass::Preset::commandRejected,peer,e,"Unable to comply");
		//	static StringBuffer buffer;
		//	buffer.Clear();
		//	foreach (cmd.path,p)
		//		buffer << ' '<<p->ToString();
		//	LogEvent(EventSignificance::Common,peer,e,"(Directed entity in mode "+String(cmd.mode.value)+" via click "+String(leftClick)+" to "+buffer.ToString()+" (detailed="+String(detailed)+"))");
		//	return;
		//}
		Journal::currentRound.RegisterDirectEntityTo(true,e,(Objects::Vehicle::mode_t)cmd.mode,cmd.path,detailed,leftClick,clearSafeTasks);
	}

	#if 0

		if (!e->IsSatellite()) //must find other logic for satellites...
		{
			Objects::PMobileEntityProducer p = To<Objects::MobileEntityProducer>::DynamicCast(e);
			if (p && cmd.path.Count() == 1)
			{
				if (p->gatheringPoint.current != cmd.path.First())
				{
					LogEvent(EventSignificance::Common, peer, e, "Set gathering point");
					p->gatheringPoint.current = cmd.path.First();
				}
				else
				{
					LogEvent(EventSignificance::Common, peer, e, "Unset gathering point");
					p->gatheringPoint.current = Grid::LCellIdI::Invalid;
				}
				e->RegisterChange(Objects::WorldEntity::AutomationChanged,0);

				Journal::currentRound.LogDirectEntityTo(*e,p->gatheringPoint.current,(Objects::Vehicle::mode_t)0,true);

				return;
			}
		}
		Objects::PMobileEntity v = To<Objects::MobileEntity>::DynamicCast(e);
		const bool leftClick = (cmd.directFlags&Protocol::DirectFlags::LeftClick)!=0;
		const bool detailedPath = (cmd.directFlags&Protocol::DirectFlags::Detailed)!=0;
		if (v)
		{
			if (!v->DirectTo((Objects::Vehicle::mode_t)cmd.mode.value,cmd.path,detailedPath,leftClick))
			{
				LogEvent(EventClass::Preset::commandRejected,peer,e,"Unable to comply");
				LogEvent(EventSignificance::Common,peer,e,"(Directed vehicle in mode "+String(cmd.mode.value)+" via click "+String(leftClick)+" to"+buffer.ToString()+" detailed="+String(detailedPath)+")");
			}
			else
				LogEvent(EventSignificance::Common,peer,e,"Directed vehicle in mode "+String(cmd.mode.value)+" via click "+String(leftClick)+" to"+buffer.ToString()+" detailed="+String(detailedPath));
			{
				Journal::currentRound.LogDirectEntityTo(*v,cmd.target,(Objects::Vehicle::mode_t)cmd.mode.value,cmd.leftClick);
				LogEvent(EventSignificance::Common,peer,e,"Directed vehicle in mode "+String(cmd.mode.value)+" via click "+String(cmd.leftClick)+" to "+cmd.target.ToString());
			}
			//Objects::WorldEntity::UnlockChanges();
			//Protocol::VehicleState	w;
			//State::Describe(w,*v, true, Protocol::Timestamp(true,0),false);
			//
			//Channels::setVehicleState.SendTo(peer,w);	
			return;
		}

		Objects::PTower t = To<Objects::Tower>::DynamicCast(e);
		if (t)
		{
			if (cmd.mode != Objects::CursorMode::AttackMode || !leftClick)
			{
				LogEvent(EventClass::Preset::commandRejected,peer,e,"Parameter fault");
				return;
			}

//			Objects::WorldEntity::LockChanges();
			foreach (cmd.path,n)
				if (!t->AttackNext(*n,false))
				{
					LogEvent(EventClass::Preset::commandRejected,peer,e,"Tower attack: Unable to comply");
				}
			else
			{
				Journal::currentRound.LogDirectEntityTo(*e,cmd.target,(Objects::Vehicle::mode_t)0,true);
				LogEvent(EventSignificance::Common,peer,e,"Issued tower attack to "+buffer.ToString());
			}
			return;		
		}


		LogEvent(EventClass::Preset::commandRejected,peer,e,"Tried to issue direct command on incompatible entity");
	}

	#endif /*0*/
	void	Hello(Protocol::Hello&hello, TCP::Peer&peer)
	{
		if (hello.Trim().IsEmpty())
		{
			Protocol::HelloResponse response;
			response.success = false;
			response.gameFlags = 0;
			response.errorMessage ="Invalid initial username";
			peer.SendPackage(TCP::Package(response,Protocol::ChannelID::Hello));
			return;
		}


		Network::PClient client = To<Network::Client>::Cast(peer.attachment);
		if (!client)
		{
			TCP::PPeer ppeer = Network::server.GetSharedPointerOfClient(&peer);
			if (!ppeer)	//disconnected in the meantime. rare, but it happens
			{
				LogEvent(EventClass( EventSignificance::ClientFatal, EventFlavor::ServerPrivate ),"Peer disconnected on its own before client state handling could commence. Ignoring event");
				return;
			}
			Network::ChangeUserLevel(peer,Network::UserLevel::PreFaction);


			client.reset(new Network::Client());
			client->peer = ppeer;
			ASSERT__(ppeer);
				//peer.shared_from_this();
			peer.attachment = client;
			//State::clients << client;
			client->name = hello;
			LogEvent(EventClass( EventSignificance::Common, EventFlavor::Gain ),peer,client->ConvertToString()+" joined the session");

			
			Protocol::PhaseChange	ph;
			ph.newPhase = RuntimeConfig::phase;
			ph.roundNumber = RuntimeConfig::roundNumber;
			ph.timeFactor = -1;


			Channels::phaseChange.SendTo(peer,ph);


			Protocol::HelloResponse response;
			response.success = true;
			response.gameFlags = Game::Network::AssembleGameFlags(peer.IsLocalhost());
			response.errorMessage ="Userlevel set to "+String(peer.userLevel);
			peer.SendPackage( TCP::Package(response, Protocol::ChannelID::Hello ));
			return;
		}


		ChangeMyName(hello,peer);
		Protocol::HelloResponse response;
		response.success = true;
		response.gameFlags = Game::Network::AssembleGameFlags(peer.IsLocalhost());
		response.errorMessage ="Duplicate Hello detected. Userlevel remains "+String(peer.userLevel);
		peer.SendPackage( TCP::Package(response,Protocol::ChannelID::Hello) );		
	}


	void OnEvent(TCP::event_t ev, TCP::Peer&peer)
	{
		switch (ev)
		{
			case TCP::Event::ConnectionEstablished:
				LogEvent(EventClass( EventSignificance::Uncommon, EventFlavor::Gain ), peer,"Client connected");
				if (RuntimeConfig::tutorialMode && !peer.IsLocalhost())
				{
					LogEvent(EventClass( EventSignificance::ClientFatal, EventFlavor::Loss ), peer,"Server is in tutorial mode. Rejecting connection from non-localhost.");
					peer.DisconnectPeer();
				}
			break;
			case TCP::Event::ConnectionLost:
			{
				Network::PClient client = To<Network::Client>::Cast(peer.attachment);
				if (client)
				{
					LogEvent(EventClass( EventSignificance::Uncommon, EventFlavor::Loss ),"Player disconnected: "+client->ConvertToString());
					Network::HandleConnectionLoss(client);

					peer.attachment.reset();
				}
				LogEvent(EventClass( EventSignificance::Uncommon, EventFlavor::ServerPrivate ),peer,"Client disconnected ("+peer.GetError(true)+")");


				if (!Network::AnyHumanPlayersActive() && !ConfigurationMode::IsActive())
				{
					LogEvent(EventClass::Preset::uncommon,"Last active player has disconnected. Pausing round timer");
					Game::roundTimerPaused = true;


					if (FileIO::Config::saveWhenAllQuit && Objects::Faction::all.IsNotEmpty())	//if factions are empty the system most likely just returned from interactive config mode and this is localhost disconnecting
					{
						if (RuntimeConfig::IsInAnyCommandPhase() || RuntimeConfig::phase == RuntimeConfig::Phase::AnimationPhase)
							FileIO::AutoSave();
						else
							FileIO::postEvolutionAutoSave = true;
					}
				}
			}
			break;
	
	
		}
	}

	void OnDeserializationFailed(UINT32 channel, TCP::Peer&origin, const String&exception)
	{
		LogEvent(EventClass::Preset::networkError,nullptr,"Network error: "+origin.ToString()+": Deserialization failed on channel "+String(channel)+" ("+exception+")");
		//DBG_FATAL__("Network error: "+origin.ToString()+": Deserialization failed on channel "+String(channel));
	}



	void OnIgnorePackage(UINT32 channel,UINT32 size, TCP::Peer&origin)
	{
		LogEvent(EventClass::Preset::networkError,nullptr,"Network error: "+origin.ToString()+": Package of size "+String(size)+" ignored on channel "+String(channel));
	}



	void OnSignal(UINT32 signal, TCP::Peer&peer)
	{
		switch (signal)
		{
			case Protocol::ChannelID::Unused:
				Network::InvalidOperation(peer);
			break;
			case Protocol::ChannelID::RequestContinueRound:
			{
				
				//Objects::Tick();
				Network::PClient client = To<Network::Client>::Cast(peer.attachment);
				if (client && client->faction && RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase && client->wantNewRound)
				{
					client->wantNewRound = false;
					LogEvent(EventSignificance::Common,peer,"Continued round");
					Channels::factionContinuedRound.SendTo(Network::server,client->faction->id,2);
				}
			}
			break;
			case Protocol::ChannelID::RequestRoundEnd:
			{
				
				//Objects::Tick();
				Network::PClient client = To<Network::Client>::Cast(peer.attachment);
				if (client && client->faction && RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase && !client->wantNewRound)
				{
					client->wantNewRound = true;
					LogEvent(EventSignificance::Common,peer,"Requested new round");
					Channels::factionRequestedNewRound.SendTo(Network::server,client->faction->id,2);
				}

			}
			break;
			case Protocol::ChannelID::Ping:
				//if (peer.attachment)
				//{
				//	State::PClient client = To<State::Client>::Cast(peer.attachment);
				//	client->lastPing = timer.Now();
				//}
				peer.SendSignal(Protocol::ChannelID::Pong);
				//#ifdef _DEBUG
				//	LogEvent(EventSignificance::Debug,"pong");
				//#endif
			break;
			case Protocol::ChannelID::RequestEverything:
			{
				Network::PClient client = To<Network::Client>::Cast(peer.attachment);
				if (client && client->isSpectator)
					SendAllToSpectator(peer,false);
				else
					LogEvent(EventClass::Preset::error,peer,"Should not have received RequestEverything from non-spectator");
			}
			break;
			case Protocol::ChannelID::EndConfigurationMode:
				ConfigurationMode::End();
			break;
		}
	}
	
}


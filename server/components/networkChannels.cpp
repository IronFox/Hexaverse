#include <global_root.h>
#include "networkChannels.h"
#include "state.h"
#include "serverLogEvent.h"
#include "journalSaveGame.h"

extern Random		random;



namespace Channels
{



	
	//TCP::ObjectSender<Protocol::ServerState,Protocol::ChannelID::GetInfo>		infoSender;
	//TCP::ObjectSender<Protocol::Result,Protocol::ChannelID::


	TCP::Channel<Protocol::Hello, 0,Protocol::ChannelID::Hello>										hello(Receiver::Hello);
	TCP::Channel<Protocol::EntityDirectCommand, 1, Protocol::ChannelID::EntityDirectCommand>		entityDirectCommand(Receiver::EntityDirectCommand);
	TCP::Channel<Protocol::ChoiceIntersection, 1, Protocol::ChannelID::ChoiceIntersection>			choiceIntersection(Receiver::ChoiceIntersection);
	TCP::Channel<NET(TUniqueID), 1, Protocol::ChannelID::DropLastTask>								dropLastTask(Receiver::DropLastTask);
	TCP::Channel<NET(TUniqueID), 1, Protocol::ChannelID::DropAllTasks>								dropAllTasks(Receiver::DropAllTasks);
	TCP::Channel<Protocol::StandardJob, 1, Protocol::ChannelID::SignalNewJob>						signalNewJob(Receiver::SignalNewJob);
	TCP::Channel<Protocol::TerrainMod, 1, Protocol::ChannelID::SignalTerrainMod>					signalTerrainMod(Receiver::SignalTerrainMod);
	TCP::Channel<Protocol::ConstructionAbort, 1, Protocol::ChannelID::ConstructionAbort>			constructionAbort(Receiver::ConstructionAbort);
	TCP::Channel<Protocol::ConstructionBegin, 1, Protocol::ChannelID::ConstructionBegin>			constructionBegin(Receiver::ConstructionBegin);
	TCP::Channel<Protocol::EnableStructure, 1, Protocol::ChannelID::EnableStructure>				enableStructure(Receiver::EnableStructure);
	TCP::Channel<Protocol::ClientFactionRequest, 1, Protocol::ChannelID::ClientFaction>				clientFactionRequest(Receiver::ClientFaction);
	
	TCP::Channel<NET(TUniqueID), 1, Protocol::ChannelID::RestoreCoreLink>							restoreCoreLink(Receiver::RestoreCoreLink);
	TCP::Channel<NET(TUniqueID), 1, Protocol::ChannelID::RemoveEntity>								removeEntity(Receiver::RemoveEntity);
	TCP::Channel<Protocol::Automate, 1, Protocol::ChannelID::Automate>								automate(Receiver::Automate);
	TCP::Channel<Protocol::Marker, 1, Protocol::ChannelID::CreateMarker>							createMarker(Receiver::CreateMarker);
	TCP::Channel<NET(TUniqueID), 1, Protocol::ChannelID::DestroyMarker>								destroyMarker(Receiver::DestroyMarker);
	TCP::Channel<Protocol::TouchedMarker, 1, Protocol::ChannelID::TouchMarker>						touchMarker(Receiver::TouchMarker);
	TCP::Channel<Protocol::ClientName,1,Protocol::ChannelID::ChangeMyName>							changeMyName(Receiver::ChangeMyName);
	TCP::Channel<NET(TUniqueID), 1, Protocol::ChannelID::IdentifyUser>								identifyUserRecv(Receiver::IdentifyUser);
	TCP::Channel<Package::NetString<256>, 1, Protocol::ChannelID::ChatMessage>						chatMessage(Receiver::ChatMessage);

	TCP::ObjectSender<Protocol::ClientFactionResponse, Protocol::ChannelID::ClientFaction>			clientFactionResponse;
	TCP::ObjectSender<Protocol::Marker, Protocol::ChannelID::MarkerCreationFailedLimitReached>		markerCreationFailedLimitReached;
	TCP::ObjectSender<Protocol::WorldDelta,Protocol::ChannelID::WorldDelta>							updateWorld;
	TCP::ObjectSender<Protocol::NewMobileEntity,Protocol::ChannelID::NewMobileEntity>				newMobileEntity;
	TCP::ObjectSender<Protocol::StructureConstruction,Protocol::ChannelID::InstantiateStructure>	createStructure;
	TCP::ObjectSender<Protocol::MobileState,Protocol::ChannelID::MobileState>						setMobileState;
	TCP::ObjectSender<Protocol::MobileEntityMotion,Protocol::ChannelID::MobileEntityMotion>			mobileEntityMotion;
	TCP::ObjectSender<Protocol::AttackTrajectory,Protocol::ChannelID::AttackTrajectory>				attackTrajectory;
	TCP::ObjectSender<Protocol::EntityTurretRotation,Protocol::ChannelID::EntityTurretRotation>		entityTurretRotation;
	TCP::ObjectSender<Protocol::Explosion,Protocol::ChannelID::Explosion>							explosion;
	TCP::ObjectSender<Protocol::StructureState,Protocol::ChannelID::UpdateStructure>				setStructureState;
	TCP::ObjectSender<Protocol::DestroyEntity,Protocol::ChannelID::DestroyEntity>					destroyEntity;
	TCP::ObjectSender<Protocol::ControlMessage,Protocol::ChannelID::ControlMessage>					controlMessage;
	TCP::ObjectSender<NET(TUniqueID),Protocol::ChannelID::SetFactionID>								setFactionID;
	TCP::ObjectSender<Protocol::PhaseChange,Protocol::ChannelID::PhaseChange>						phaseChange;
	TCP::ObjectSender<Protocol::ResourceUpdate,Protocol::ChannelID::ResourceUpdate>					resourceUpdate;
	TCP::ObjectSender<Protocol::GameProgress,Protocol::ChannelID::GameProgress>						gameProgress;
	TCP::ObjectSender<Protocol::ChangeEntityOwner,Protocol::ChannelID::ChangeEntityOwner>			changeEntityOwner;
	TCP::ObjectSender<Protocol::IdentifyUser, Protocol::ChannelID::IdentifyUser>					identifyUser;
	TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::FactionOnline>							factionOnline;
	TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::FactionOffline>							factionOffline;
	TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::FactionRequestedNewRound>				factionRequestedNewRound;
	TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::FactionContinuedRound>					factionContinuedRound;

	TCP::ObjectSender<NET(UINT16), Protocol::ChannelID::LockEMPZones>								lockEmpZones;
	#ifndef NO_HIBERNATION
		TCP::ObjectSender<NET(UINT32), Protocol::ChannelID::UplinkBandwidth>						uplinkBandwidth;
	#endif

	TCP::ObjectSender<Protocol::UnitLimits,Protocol::ChannelID::UnitLimits>							unitLimits;

//	Channel<class Object, unsigned MinUserLevel, UINT32 ChannelID>

}






namespace Receiver
{

	static void ConvertReferenceEntity(const Objects::Marker&source, Protocol::MarkerReferenceEntity&e)
	{
		e.id = source.referenceEntity;
		e.type0 = source.referencedType[0];
		e.type1 = source.referencedType[1];
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
		return State::GetFactionStartLocation(random);
	}



	void	SendSessionConfigTo(TCP::Peer&peer, const Objects::PFaction&exclude)
	{
		Objects::ForeachFaction([&peer,exclude](const Objects::PFaction&f)
		{
			if (f != exclude)
			{
				Channels::identifyUser.SendTo(peer,Protocol::IdentifyUser(f->id,f->name));
				if (f->online)
				{
					Channels::factionOnline.SendTo(peer,f->id);
					if (State::playerMap.Require(f)->wantNewRound)
						Channels::factionRequestedNewRound.SendTo(peer,f->id);
				}
			}
		});

		#ifndef NO_HIBERNATION
			Channels::uplinkBandwidth.SendTo(peer,State::Config::uplinkBandwidth);
		#endif
		Protocol::UnitLimits	limits;
		RuntimeConfig::UnitLimits::Export(limits);
		Channels::unitLimits.SendTo(peer,limits);
		
		Channels::lockEmpZones.SendTo(peer,State::empZonesLocked);


		Protocol::PhaseChange	ph;
		ph.newPhase = RuntimeConfig::phase;
		ph.roundNumber = (UINT32)RuntimeConfig::roundNumber;
		ph.timeFactor = -1;


		Channels::phaseChange.SendTo(peer,ph);

	}

	void	ClientFaction(Protocol::ClientFactionRequest&_p, TCP::Peer&peer)
	{
		if (!peer.attachment)
			return;
		State::PClient client = To<State::Client>::Cast(peer.attachment);
		Protocol::ClientFactionResponse response;
		response.startFactionTypes = _p.startFactionTypes;
		response.factionToken = _p.factionToken;
		response.planetConfig = Grid::currentPlanetConfig;
		State::AssembleGameInfo(response.info);
		//response.info = CompileWorldInfo();

		if (!response.startFactionTypes || response.startFactionTypes == Protocol::FactionType::SpectatorFaction)
		{
			if (State::activePlayers.FindAndErase(client))
			{
				if (client->faction)
				{
					Journal::currentRound.LogFactionOffline(client->faction);
					LogEvent(EventType::SessionRelated,peer,"Abandoning faction");
					State::playerMap.Unset(client->faction);
					client->faction->online = false;
					client->faction.reset();
				}
				peer.userLevel = 1;
				LogEvent(EventType::Debug,peer,"User level changed to "+String(peer.userLevel));
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
			State::spectators.FindAndErase(client);
		}

		if (!RuntimeConfig::IsInAnyCommandPhase()
			&& RuntimeConfig::phase != RuntimeConfig::Phase::EndOfGame
			&& RuntimeConfig::phase != RuntimeConfig::Phase::AnimationPhase
			)
		{
			//disconnecting client. no new faction
			LogEvent(EventType::CommandRejected, peer, "Cannot connect ("+String(RuntimeConfig::phase.ToString())+") right now. Disconnecting");
			response.startFactionTypes = 0;
			Channels::clientFactionResponse.SendTo(client->peer.lock(), response);
			client->peer.lock()->DisconnectPeer();
			return;
		}


		if (State::activePlayers.Contains(client))
			return;

		bool factionTypeNotSupported = true;

		switch (response.startFactionTypes)
		{
			case Protocol::FactionType::ContinueOnly:
				factionTypeNotSupported = false;
			break;
			case Protocol::FactionType::SpectatorFaction:
			{
				factionTypeNotSupported = !peer.IsLocalhost();
				if (factionTypeNotSupported)
					LogEvent(EventType::CommandRejected,peer,"Spectator type is allowed only from localhost, but peer is "+peer.ToString());
			}
			break;
			case Protocol::FactionType::RegularFaction:
				factionTypeNotSupported = !(State::Config::factionSupportFlags & Protocol::GameFlag::SupportRegularFactions);
			break;
		}


		if (factionTypeNotSupported)
		{
			LogEvent(EventType::CommandRejected, peer, "Requested faction type is not supported");
			response.startFactionTypes = 0;
			Channels::clientFactionResponse.SendTo(client->peer.lock(), response);
			client->peer.lock()->DisconnectPeer();
			return;
		}

		if (response.startFactionTypes == Protocol::FactionType::SpectatorFaction)
		{
			//authenticated and purged any previous faction. let's go
			peer.userLevel = 2;
			LogEvent(EventType::Debug,peer,"User level changed to "+String(peer.userLevel));

			bool firstSpectator = State::spectators.IsEmpty();
			if (!client->isSpectator)
			{
				State::spectators << client;
				State::activePlayers << client;
			}
			client->isSpectator = true;

			response.factionToken = CryptographicID();
			Channels::clientFactionResponse.SendTo(client->peer.lock(), response);

			//send world state:

			Protocol::Timestamp	timestamp;
			timestamp.rightNow = true;

			for (const auto&s : Objects::structures)
			{
				Protocol::StructureConstruction st;
				State::Describe(st,*s,true,timestamp);
				Channels::createStructure.SendTo(peer,st);
			}

			for (const auto&v : Objects::vehicles)
			{
				Protocol::NewMobileEntity st;
				State::Describe(st,*v,true,timestamp,0xFFFFFFFF);
				Channels::newMobileEntity.SendTo(peer,st);
			}

			for (const auto&v : Objects::satellites)
			{
				Protocol::NewMobileEntity st;
				State::Describe(st,*v,true,timestamp,0xFFFFFFFF);
				Channels::newMobileEntity.SendTo(peer,st);
			}

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
			if (firstSpectator)
				State::spectatorWorldState.FlushAllData();
			Protocol::WorldDelta	delta;
			delta.timestamp.rightNow = true;
			for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
			{
				if (firstSpectator)
				{
					Protocol::GenerateWorldDelta(nullptr,&State::spectatorWorldState,
											delta,k,true,k+1 == BaseGrid::NumFamilies,Protocol::WorldDeltaType::ProgressiveInit);
				}
				else
					Protocol::GenerateWorldDelta(nullptr,&State::spectatorWorldState,
											delta,k,true,k+1 == BaseGrid::NumFamilies,Protocol::WorldDeltaType::LocalCopy);

				Channels::updateWorld.SendTo(peer,delta);
			}

			SendSessionConfigTo(peer, Objects::PFaction());

			return;
		}

		Objects::PFaction	faction;

		bool newFaction = false;

		if (State::Config::Nexus::enableAI && Objects::Faction::nexus->accessToken == response.factionToken && (peer.ToString(false) == "localhost"))
		{
			LogEvent(EventType::SessionRelated, peer, "Access to nexus faction permitted.");
			faction = Objects::Faction::nexus;
		}
		#ifdef _DEBUG
			if (faction
				|| State::tokenMap.Query(response.factionToken,faction)
				|| (
						response.startFactionTypes == Protocol::FactionType::ContinueOnly 
						&& Objects::Faction::all.IsNotEmpty()
						&& (faction = Objects::Faction::all.first().lock())
					)
				)
		#else
			if (faction || State::tokenMap.Query(response.factionToken,faction))
		#endif
		{
			#ifdef _DEBUG
				response.factionToken = faction->accessToken;
			#endif
			State::PClient other;
			if (State::playerMap.Query(faction,other))
			{
				LogEvent(EventType::SessionRelated,peer,"Faction access token recognized, but faction is already being controlled. Disconnecting "+other->ToString());

				if (faction == Objects::Faction::nexus)
				{
					//disconnecting client. no new faction
					LogEvent(EventType::SessionRelated, peer, "Disconnecting AI client.");
					response.startFactionTypes = 0;
					Channels::clientFactionResponse.SendTo(client->peer.lock(), response);
					client->peer.lock()->DisconnectPeer();
					return;
				}
				other->peer.lock()->DisconnectPeer();
				State::playerMap.Unset(faction);
				faction->online = false;
				other->faction.reset();
				client->faction = faction;
				LogEvent(EventType::SessionRelated,peer,"Faction access token recognized.");


//				p.factionToken.value.Generate();
	//			newFaction = true;
			}
			else
			{
				client->faction = faction;
				LogEvent(EventType::SessionRelated,peer,"Faction access token recognized.");
			}
		}
		else
		{
			if (response.factionToken.value.IsSet())
			{
				LogEvent(EventType::SessionRelated,peer,"Provided token not recognized: "+response.factionToken.value.ToString());

				if (State::Config::closedGame && RuntimeConfig::roundNumber > 0)
				{
					foreach (Objects::Faction::all,f)
					{
						ASSERT__(f->lock()->accessToken != response.factionToken);
						LogEvent(EventType::Debug,peer,"Known token: "+f->lock()->accessToken.ToString());
					}
				}

			}
			else
			{
				LogEvent(EventType::SessionRelated,peer,"No faction token provided");
				response.factionToken.value.Generate();
			}
			newFaction = true;
		}

		if (RuntimeConfig::phase == RuntimeConfig::Phase::EndOfGame)
		{
			{
				client->SendProgressUpdate(true,"<null>");
			}
		}


		index_t startLocation = InvalidIndex;
		if (newFaction)
		{
			if (response.startFactionTypes == Protocol::FactionType::ContinueOnly)
			{
				response.startFactionTypes = 0;
				Channels::clientFactionResponse.SendTo(peer,response);
				LogEvent(EventType::CommandRejected,peer,"Client chose to continue faction. No faction found that can be continued. Join-request rejected.");
				return;
			}

			if (State::Config::closedGame && RuntimeConfig::roundNumber > 0)
			{
				response.startFactionTypes = 0;
				Channels::clientFactionResponse.SendTo(peer,response);
				LogEvent(EventType::CommandRejected,peer,"Game is closed. User does not have any valid access token. Join-request rejected.");
				return;
			}

			State::UpdateEMPZones(1);
			startLocation = GetStartLocation();
			if (startLocation == InvalidIndex)
			{
				State::UpdateEMPZones(0);
				response.startFactionTypes = 0;
				Channels::clientFactionResponse.SendTo(peer,response);
				LogEvent(EventType::OperationAborted,peer,"Unable to find valid starting location for new faction.");
				return;
			}
			//p.factionToken.value.Generate();	let's keep it even if it's unknown
			client->faction.reset(new Objects::Faction());
			faction = client->faction;
			Objects::Faction::all << client->faction;
			client->faction->name = client->name;


			LogEvent(EventType::SessionRelated,peer,"Created new faction.");

			client->faction->accessToken = response.factionToken;
			State::tokenMap.Set(response.factionToken,client->faction);
		}
		else
		{
			faction->name = client->name;
		}
		peer.userLevel = 2;
		LogEvent(EventType::Debug,peer,"User level changed to "+String(peer.userLevel));

		State::activePlayers << client;
		State::playerMap.Set(client->faction,client);
		client->faction->online = true;
		client->wantNewRound = false;


		response.startFactionTypes = Protocol::FactionType::RegularFaction;
		if (client->faction == Objects::Faction::nexus)
			response.startFactionTypes |= Protocol::FactionType::NexusFaction;

		Channels::clientFactionResponse.SendTo(client->peer.lock(),response);
		Channels::setFactionID.SendTo(client->peer.lock(),client->faction->id);

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
			State::SpawnFactionStartUnits(client->faction,startLocation);

		}


		#ifndef CHEAT_SHOW_ALL_RESOURCES
		if (!newFaction)
		#endif
		{
			Protocol::Timestamp	timestamp;
			timestamp.rightNow = true;

			static Buffer0<UINT32>	final;
			final.Clear();
			auto&discovered = client->faction->visibility.resourcesDiscovered;
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
		Protocol::WorldDelta	delta;
		delta.timestamp.rightNow = true;
		for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
		{
			Protocol::GenerateWorldDelta(client->faction ? &client->faction->visibility : nullptr,  client->faction ? &client->faction->currentState : nullptr,
									delta,k,true,k+1 == BaseGrid::NumFamilies,newFaction ? Protocol::WorldDeltaType::ProgressiveInit : Protocol::WorldDeltaType::LocalCopy);

			Channels::updateWorld.SendTo(peer,delta);
		}

		String why;
		if (State::Config::basesRequired && faction->deathInRounds > 0)
			faction->CanLive(why);
		client->SendProgressUpdate(false,why);


		if (!newFaction)
		{
			Protocol::Timestamp	timestamp;
			timestamp.rightNow = true;


			client->faction->seeing.VisitAllKeys([&peer,client,timestamp](const Objects::PWorldEntity&e)
			{
				if (e->health <= 0)
				{
					//weird...? weirder...
					return;
				}

				Objects::PStructure s = To<Objects::Structure>::DynamicCast(e);
				if (s)
				{
					Protocol::StructureConstruction st;
					State::Describe(st,*s,(s)->owner == client->faction,timestamp);
					Channels::createStructure.SendTo(peer,st);
				}
				else
				{
					Objects::PMobileEntity v = To<Objects::MobileEntity>::DynamicCast(e);
					if (v)
					{
						Protocol::NewMobileEntity st;
						State::Describe(st,*v,(v)->owner == client->faction,timestamp,0xFFFFFFFF);
						Channels::newMobileEntity.SendTo(peer,st);
					}
					else
						FATAL__("WTF");

				}
			});

			foreach (client->faction->structures,_s)
			{
				Objects::PStructure s= _s->lock();
				ASSERT__(s);
				if (s)
				{
					Protocol::StructureConstruction st;
					State::Describe(st,*s,(s)->owner == client->faction,timestamp);
					Channels::createStructure.SendTo(peer,st);
				}
			}
			foreach (client->faction->allMobiles,_v)
			{
				Objects::PMobileEntity v= _v->lock();
				ASSERT__(v);
				if (v)
				{
					Protocol::NewMobileEntity st;
					State::Describe(st,*v,true,timestamp,0xFFFFFFFF);
					Channels::newMobileEntity.SendTo(peer,st);
				}
			}
			foreach (client->faction->storedMobiles,_v)
			{
				Objects::PMobileEntity v= _v->lock();
				ASSERT__(v);
				if (v)
				{
					Protocol::NewMobileEntity st;
					State::Describe(st,*v,true,timestamp,0xFFFFFFFF);
					Channels::newMobileEntity.SendTo(peer,st);
				}
			}

		}



		



		//faction->online = true;
		//probably optional later on
		{
			//broadcast name and state
			Channels::identifyUser.SendTo(State::server,Protocol::IdentifyUser(client->faction->id,client->name),2);
			Channels::factionOnline.SendTo(State::server,client->faction->id,2);

		}

		if (newFaction)
			Journal::currentRound.LogFactionCreation(client->faction,Journal::GetType(client->faction));
		Journal::currentRound.LogFactionOnline(client->faction);
		LogEvent(EventType::SessionRelated,faction->name+" connected");

		SendSessionConfigTo(peer,client->faction);
		
		peer.SendSignal(Protocol::ChannelID::BeginGame);
	}

	bool CanControl(const State::Client&c, const Objects::WorldEntity&e)
	{
		if (c.faction != e.owner)
			return false;
		if (c.wantNewRound)
		{
			ASSERT__(c.faction == e.owner);
			LogEvent(EventType::CommandRejected,&e,"Unable to comply: You have ended your round");
			return false;
		}
		return true;
	}
	bool CanControl(const State::PClient&c, const Objects::PWorldEntity&e)
	{
		return CanControl(*c,*e);
	}
	bool CanControl(const State::PClient&c, const Objects::WorldEntity&e)
	{
		return CanControl(*c,e);
	}
	bool PeerCanControl(TCP::Peer&peer, const Objects::WorldEntity&e)
	{
		if (!peer.attachment)
			return false;
		return CanControl(To<State::Client>::Cast(peer.attachment),e);
	}
	bool PeerCanControl(TCP::Peer&peer, const Objects::PWorldEntity&e)
	{
		return PeerCanControl(peer,*e);
	}


	bool CanSeePrivateDetails(const State::Client&c, const Objects::WorldEntity&e)
	{
		return c.faction == e.owner || c.isSpectator;
	}
	bool CanSeePrivateDetails(const State::PClient&c, const Objects::PWorldEntity&e)
	{
		return CanSeePrivateDetails(*c,*e);
	}
	bool CanSeePrivateDetails(const State::PClient&c, const Objects::WorldEntity&e)
	{
		return CanSeePrivateDetails(*c,e);
	}


	bool CanSee(const State::Client&c, const Objects::PWorldEntity&e, float t)
	{
		return c.CanSee(e,t);
		//return /*c.id == e.owner*/ true;	//placeholder
	}
	bool CanSee(const State::PClient&c, const Objects::PWorldEntity&e, float t)
	{
		return CanSee(*c,e,t);
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
		LogEvent(EventType::CommandRejected ,peer,"Cannot issue commands in current phase ("+String(RuntimeConfig::phase.ToString())+")");
		return false;
	}

	Objects::PWorldEntity	CheckCommandModeGetAccessibleEntity(const TUniqueID&eid, TCP::Peer&peer)
	{
		if (!CheckCommandMode(peer))
			return Objects::PWorldEntity();

		Objects::PWorldEntity e = Objects::GetEntity(eid);
		if (!e)
		{
			//LogEvent(EventType::CommandRejected ,peer,"Tried to access non-existent entity");
			for (auto f : Objects::Faction::all)
			{
				Objects::PFaction faction = f.lock();
				if (faction && faction->FindMarker(eid))
				{
					LogEvent(EventType::CommandRejected, peer, "Access denied (this is a marker/ghost/contact)");
					//LogEvent(EventType::Debug, peer, "(because it doesn't exist)");
					return Objects::PWorldEntity();
				}
			}


			LogEvent(EventType::CommandRejected, peer, "Access denied");
			LogEvent(EventType::Debug, peer, "(because it doesn't exist)");
			return Objects::PWorldEntity();
		}

		if (!PeerCanControl(peer,e))
		{
			LogEvent(EventType::CommandRejected,peer,e,"Access denied");
			return Objects::PWorldEntity();
		}

		{
			if (e->IsVehicle())
			{
				if (To<Objects::Vehicle>::Cast(e)->IsHibernating())
				{
					LogEvent(EventType::CommandRejected,peer,e,"No reaction. Vehicle is hibernating.");
					return Objects::PWorldEntity();
				}
			}
			else
				if (e->IsWorkingStructure())
				{
					if (To<Objects::WorkingStructure>::Cast(e)->IsHibernating())
					{
						LogEvent(EventType::CommandRejected,peer,e,"No reaction. Structure is hibernating.");
						return Objects::PWorldEntity();
					}
				}
		}


		return e;
	}

	void	CreateMarker(Protocol::Marker&m, TCP::Peer&peer)
	{
		LogEvent(EventType::MinorEntityProcess, peer, nullptr, "Begin: " __func__);


		if (!peer.attachment)
		{
			LogEvent(EventType::CommandRejected, peer, "Marker creation denied. Player not ready");
			return;
		}
		const Objects::PFaction&f = To<State::Client>::Cast(peer.attachment)->faction;
		if (!f)
		{
			LogEvent(EventType::CommandRejected, peer, "Marker creation denied. Player has no faction");
			return;
		}
		if (!f->CanCreateMarker(f == Objects::Faction::nexus))
		{
			LogEvent(EventType::Debug, peer, "Marker creation denied. Limit reached");
			Channels::markerCreationFailedLimitReached.SendTo(peer, m);
			return;
		}
		bool failed = false;
		static_assert(Objects::Marker::Type::NumberOfPossibilities == 3, "Please adjust marker logic");
		const Protocol::MarkerReferenceEntity*e = m.data.Get0();
		Protocol::MarkerReferenceEntity ref;
		ref.type0 = ref.type1 = ConstName::Empty;
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
			LogEvent(EventType::CommandRejected, peer, "Marker creation rejected. Invalid marker configuration");
			return;
		}

		Journal::currentRound.Register(true,f,new Journal::Round::CreateMarker(m.id,type,m.createdInRound,m.location,ref));
		//Channels::createdMarker.SendTo(peer, outMarker);
	}


	void	DestroyMarker(NET(TUniqueID)&id, TCP::Peer&peer)
	{
		LogEvent(EventType::MinorEntityProcess, peer, nullptr, "Begin: " __func__);

		if (!peer.attachment)
		{
			LogEvent(EventType::CommandRejected, peer, "Marker destruction denied. Player not ready");
			return;
		}
		const Objects::PFaction&f = To<State::Client>::Cast(peer.attachment)->faction;
		if (!f)
		{
			LogEvent(EventType::CommandRejected, peer, "Marker destruction denied. Player has no faction");
			return;
		}

		Journal::currentRound.Register(true,f,new Journal::Round::DestroyMarker(id));
	}

	void	ChatMessage(Package::NetString<256>&message, TCP::Peer&peer)
	{
		if (!peer.attachment)
			return;
		State::PClient client = To<State::Client>::Cast(peer.attachment);
		if (!client->faction)
			return;
		LogEvent(EventType::ChatMessage,client->faction->GetName()+": "+message);

	}


	void	IdentifyUser(NET(TUniqueID)&id, TCP::Peer&peer)
	{
		//LATER
	/*	for (const auto&weak : Objects::Faction::all)
		{
			Objects::PFaction fac = weak.lock();
			if (fac && fac->

		}
		State::playerMap.QueryPointer(id);
*/

	}

	
	void	ChangeMyName(Protocol::ClientName&name, TCP::Peer&peer)
	{
		LogEvent(EventType::MinorEntityProcess, peer, nullptr, "Begin: " __func__);
		if (!peer.attachment)
		{
			LogEvent(EventType::CommandRejected, peer, "Name change denied. Player not ready");
			return;
		}
		State::PClient cl = To<State::Client>::Cast(peer.attachment);
		cl->name = name;
		if (cl->faction)
		{
			Protocol::IdentifyUser userID;
			userID.id = cl->faction->id;
			userID.name = name;
			cl->faction->name = name;
			Channels::identifyUser.SendTo(State::server,userID,2);
			LogEvent(EventType::SessionRelated, peer, "User renamed to '"+name+"'");
		}
	}

	void	TouchMarker(Protocol::TouchedMarker&m, TCP::Peer&peer)
	{
		LogEvent(EventType::MinorEntityProcess, peer, nullptr, "Begin: " __func__);

		if (!peer.attachment)
		{
			LogEvent(EventType::CommandRejected, peer, "Marker touch denied. Player not ready");
			return;
		}
		const Objects::PFaction&f = To<State::Client>::Cast(peer.attachment)->faction;
		if (!f)
		{
			LogEvent(EventType::CommandRejected, peer, "Marker touch denied. Player has no faction");
			return;
		}
		Objects::Marker *marker = f->FindMarker(m.id);
		if (marker)
		{
			LogEvent(EventType::Notification, peer, "Touching " + marker->GetName()+" to round number "+String(m.newRoundNumber));
			marker->createdInRound = m.newRoundNumber;
			//Protocol::TouchedMarker m;
			//m.id = id;
			//m.newRoundNumber = marker->createdInRound;
			//Channels::touchedMarker.SendTo(peer,m);	//knows already
		}
		else
			LogEvent(EventType::CommandRejected, peer, "Marker " + m.id.value.ToString() + " not found for touch");
	}


	void	Automate(Protocol::Automate&a, TCP::Peer&peer)
	{
		LogEvent(EventType::MinorEntityProcess,peer,nullptr,"Begin: Automate ("+String(a.doAutomate)+") entity "+a.entityID.value.ToString());

		Objects::PWorldEntity e = CheckCommandModeGetAccessibleEntity(a.entityID,peer);
		if (!e)
			return;


		Objects::PMobileEntity v = To<Objects::MobileEntity>::DynamicCast(e);
		if (!v)
		{
			LogEvent(EventType::CommandRejected,peer,e,"Tried to automate immobile entity");
			return;
		}
		if (v->isAutomated == a.doAutomate)
			return;
		Journal::currentRound.RegisterAutomationChange(true,v,a.doAutomate);
	}

	void	EnableStructure(Protocol::EnableStructure&es, TCP::Peer&peer)
	{
		LogEvent(EventType::MinorEntityProcess,peer,nullptr,"Begin: "+String(es.enable?"Enable":"Disable")+" structure "+es.id.value.ToString());

		Objects::PWorldEntity e = CheckCommandModeGetAccessibleEntity(es.id,peer);
		if (!e)
			return;


		Objects::PWorkingStructure w = To<Objects::WorkingStructure>::DynamicCast(e);
		if (!w)
		{
			LogEvent(EventType::CommandRejected,peer,e,"Tried to enable/disable entity that is not a working structure");
			return;
		}
		Journal::currentRound.Register(true,w,new Journal::Round::EnableStructure(es.enable));
	}

	void	RestoreCoreLink(NET(TUniqueID)&uuid, TCP::Peer&peer)
	{
		LogEvent(EventType::MinorEntityProcess,peer,nullptr,"Begin: Restoring mine core link "+uuid.value.ToString());

		Objects::PWorldEntity e = CheckCommandModeGetAccessibleEntity(uuid, peer);
		if (!e)
			return;

		Objects::PMine m = To<Objects::Mine>::DynamicCast(e);
		if (!m)
		{
			LogEvent(EventType::CommandRejected,peer,e,"Invalid operation on entity");
			return;
		}	
		Journal::currentRound.Register(true,m, new Journal::Round::RestoreCoreLink());

	}
	void	RemoveEntity(NET(TUniqueID)&uuid, TCP::Peer&peer)
	{
		LogEvent(EventType::MinorEntityProcess,peer,nullptr,"Begin: Removing entity "+uuid.value.ToString());

		Objects::PWorldEntity e = CheckCommandModeGetAccessibleEntity(uuid,peer);
		if (!e)
			return;

		Journal::currentRound.RegisterEntityDestruction(true,e,e->isInserted ? Objects::RemovalType::ControlledExplosion : Objects::RemovalType::DestroyedInStorage);
	}

	void	ConstructionAbort(Protocol::ConstructionAbort&ca, TCP::Peer&peer)
	{
		LogEvent(EventType::MinorEntityProcess,peer,nullptr,"Begin: Aborting vehicle plant construction for entity "+ca.plantID.value.ToString());

		Objects::PWorldEntity e = CheckCommandModeGetAccessibleEntity(ca.plantID,peer);
		if (!e)
			return;

		//Objects::PWorkingStructure ws = To<Objects::WorkingStructure>::DynamicCast(e);

		Objects::PMobileEntityProducer p = To<Objects::MobileEntityProducer>::DynamicCast(e);
		if (!p)
		{
			LogEvent(EventType::CommandRejected,peer,e,"Tried to abort construction-plant job on non-plant entity ");
			return;
		}
		Journal::currentRound.Register(true,e,new Journal::Round::ConstructionAbort(ca.taskId));
	}


	void	ConstructionBegin(Protocol::ConstructionBegin&cb, TCP::Peer&peer)
	{
		LogEvent(EventType::MinorEntityProcess,peer,nullptr,"Begin: Registering mobile producer construction, given "+String(cb.parameters.Count())+" parameter(s), for entity "+cb.plantID.value.ToString());

		Objects::PWorldEntity e = CheckCommandModeGetAccessibleEntity(cb.plantID,peer);
		if (!e)
			return;

		Objects::PMobileEntityProducer p = To<Objects::MobileEntityProducer>::DynamicCast(e);
		if (p)
		{
			Journal::currentRound.RegisterProductionJobStart(true,e,p,cb.parameters.pointer(),cb.parameters.Count());
			return;
		}
		LogEvent(EventType::CommandRejected,peer,e,"Tried to issue construction job on incompatible entity");
		return;

	}


	void	SignalNewJob(Protocol::StandardJob&sjob, TCP::Peer&peer)
	{
		LogEvent(EventType::MinorEntityProcess,peer,nullptr,"Begin: Registering new Job #"+String(int(sjob.jobName.value))+" for vehicle "+sjob.id.value.ToString());

		Objects::PWorldEntity e = CheckCommandModeGetAccessibleEntity(sjob.id,peer);
		if (!e)
			return;
		Objects::PMobileEntity v = To<Objects::MobileEntity>::DynamicCast(e);
		if (!v)
		{
			LogEvent(EventType::CommandRejected,peer,e,"Tried to issue mobile entity job on non-vehicle entity");
			return;
		}
		if (!v->GetConstructionSpeed())
		{
			LogEvent(EventType::CommandRejected,peer,e,"This mobile entity cannot build");
			return;
		}
		if (v->myTasks.Count() >= Objects::MaxProductionTasks)
		{
			LogEvent(EventType::CommandRejected,peer,e,"Max queue length ("+String(Objects::MaxProductionTasks)+") reached. Task rejected");
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
		switch (sjob.jobName)
		{
			case ConstName::Job_BuildRoad:
			{
				if (v->GetLastPointOfOrigin().height <= -Grid::CellStack::HalfRange)
				{
					LogEvent(EventType::CommandRejected,peer,e,"Cannot build road on bottom layer");
					return;
				}

				job.reset(new VehicleJob::BuildRoad(*v));
				v->RegisterNewJob("", job,nullptr);
				LogEvent(EventType::MinorEntityProcess,peer,e,"Registered new job "+job->GetName());
			}
			break;
			case ConstName::Job_BuildWallUp:
			{
				target = v->GetLastPointOfOrigin()+1;	//has moveToFocusWhenDone, focus must be set to where vehicle will move
				if (target.height >= Grid::CellStack::HalfRange)
				{
					LogEvent(EventType::CommandRejected,peer,e,"Cannot build wall on top layer");
					return;
				}
				job.reset(new VehicleJob::BuildWallUp(*v));
				v->RegisterNewJob("", job,&target);
				LogEvent(EventType::MinorEntityProcess,peer,e,"Registered new job "+job->GetName());
			}
			break;
			case ConstName::Job_BuildHorizontalWall:
			{
				//Grid::LCellIdI target = v->GetLastPointOfOrigin()+1;
				//if (target.height+1 >= Grid::CellStack::HalfRange)
				//{
				//	LogEvent(EventType::CommandRejected,peer,e,"Cannot build wall on top layer");
				//	return;
				//}
				job.reset(new VehicleJob::BuildHorizontalWall(*v));
				v->RegisterNewJob("", job,nullptr);
				LogEvent(EventType::MinorEntityProcess,peer,e,"Registered new job "+job->GetName());
			}
			break;
			case ConstName::Job_DigDown:
			{
				target = v->GetLastPointOfOrigin()-1;
				if (target.height < -Grid::CellStack::HalfRange)
				{
					LogEvent(EventType::CommandRejected,peer,e,"Bottom layer reached. Cannot dig any further");
					return;
				}
				else
				{
					job.reset(new VehicleJob::DigDown(*v));
					v->RegisterNewJob("",job,&target);		
					LogEvent(EventType::MinorEntityProcess,peer,e,"Registered new job "+job->GetName());
				}
			}
			break;
			case ConstName::Job_Construct:
			{
				if (v->GetLastPointOfOrigin().height <= -Grid::CellStack::HalfRange)
				{
					LogEvent(EventType::CommandRejected,peer,e,"Cannot build on bottom layer");
					return;
				}

				target = v->GetLastPointOfOrigin();

				Objects::PStructure p;
				foreach (Objects::structurePrototypes,proto)
					if ((*proto)->GetCTypeName() == sjob.parameterName)
					{
						p = *proto;
						break;
					}
				if (p)
				{	
					job.reset(new VehicleJob::Construct(*v,p));

					v->RegisterNewJob(p->GetTypeName(),job,&target);
					LogEvent(EventType::MinorEntityProcess,peer,e,"Registered new job "+job->GetName());
				}
				else
				{
					LogEvent(EventType::CommandRejected,peer,v,"Unable to construct structure type");
					State::InvalidOperation(peer);
				}
			}
			break;
			default:
			{
				LogEvent(EventType::CommandRejected,peer,v,"Illegal job name #"+String(int(sjob.jobName.value)));
				State::InvalidOperation(peer);
			}
			return;
		}
		if (job)
			Journal::currentRound.LogJobCommandStart(e,*job,target);

	}


	void	SignalTerrainMod(Protocol::TerrainMod&mod, TCP::Peer&peer)
	{
		LogEvent(EventType::MinorEntityProcess,peer,nullptr,"Begin: Registering new AlterTerrain for vehicle "+mod.id.value.ToString()+", mode="+String(mod.mode.value)+", numCells="+String(mod.cells.Count()));

		Objects::PWorldEntity e = CheckCommandModeGetAccessibleEntity(mod.id,peer);
		if (!e)
			return;

		Objects::PVehicle v = To<Objects::Vehicle>::DynamicCast(e);
		if (!v)
		{
			LogEvent(EventType::CommandRejected,peer,e,"Tried to issue vehicle job on non-vehicle entity");
			return;
		}
		if (!v->turret->constructionSpeed)
		{
			LogEvent(EventType::CommandRejected,peer,e,"This vehicle cannot shape terrain");
			return;
		}


		foreach (mod.cells,cell)
		{
			if (!Grid::ValidCell(*cell))
			{
				LogEvent(EventType::CommandRejected,peer,e,"Parameter fault");
				return;
			}
		}
	
		Objects::PVehicleJob job(new Objects::VehicleJob::AlterTerrain(*v,(Objects::Vehicle::mode_t)mod.mode.value,mod.cells));

		v->RegisterNewJob("",job,nullptr);

		if (job)
			Journal::currentRound.LogJobCommandStart(e,*job,Grid::LCellIdI::Invalid);


		LogEvent(EventType::MinorEntityProcess,peer,e,"End: Registered new job "+job->GetName());
	}

	void	ChoiceIntersection(Protocol::ChoiceIntersection&ch, TCP::Peer&peer)
	{
		LogEvent(EventType::MinorEntityProcess,peer,nullptr,"Begin: Processing choice intersection in mode "+String(ch.mode)+" at cell "+ch.target.ToString()+" for entity "+ch.id.value.ToString());

		Objects::PWorldEntity e = CheckCommandModeGetAccessibleEntity(ch.id,peer);
		if (!e)
			return;


		if (!Grid::ValidCell(ch.target, Objects::Satellite::BaseResidenceHeight, Objects::Satellite::DeclinedResidenceHeight))
		{
			LogEvent(EventType::CommandRejected,peer,e,"Parameter fault");
			return;
		}



		const NET(TUniqueID)*deploying = ch.deployingWhat.Get0();
		Objects::PMobileEntity deployingWhat;
		
		if (deploying)
		{
			deployingWhat = To<Objects::MobileEntity>::DynamicCast(Objects::GetEntity(deploying->value));
			ASSERT1__(deployingWhat,deploying->value.ToString());
		}

		if (To<Objects::VehicleStore>::DynamicCast(e))
			ASSERT_NOT_NULL__(deploying);

		Journal::currentRound.RegisterChoiceIntersection(true,e,ch.target,(Objects::Vehicle::mode_t)ch.mode.value,deployingWhat);

		LogEvent(EventType::MinorEntityProcess,peer,e,"End: Processed choice intersection in mode "+String(ch.mode)+" at cell "+ch.target.ToString());
	}

	void	DropLastTask(NET(TUniqueID)&uuid, TCP::Peer&peer)
	{
		LogEvent(EventType::MinorEntityProcess,peer,nullptr,"Begin: Drop last task of entity "+uuid.value.ToString());

		Objects::PWorldEntity e = CheckCommandModeGetAccessibleEntity(uuid,peer);
		if (!e)
			return;


		Objects::PMobileEntity v = To<Objects::MobileEntity>::DynamicCast(e);
		if (v)
		{
			if (v->myTasks.IsEmpty())
			{
				LogEvent(EventType::MinorEntityProcess,peer,e,"Nothing to undo");
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
					LogEvent(EventType::MinorEntityProcess,peer,e,"Nothing to undo");
					return;
				}
			}
			else
			{
				LogEvent(EventType::CommandRejected,peer,e,"Tried to drop command on incompatible entity");
				return;
			}
		}
		Journal::currentRound.RegisterDropLastEntityCommand(true,e);

		
	}

	void	DropAllTasks(NET(TUniqueID)&uuid, TCP::Peer&peer)
	{
		LogEvent(EventType::MinorEntityProcess,peer,nullptr,"Begin: Drop all tasks of entity "+uuid.value.ToString());

		Objects::PWorldEntity e = CheckCommandModeGetAccessibleEntity(uuid,peer);
		if (!e)
			return;

		Objects::PMobileEntity v = To<Objects::MobileEntity>::DynamicCast(e);
		if (v)
		{
			if (v->myTasks.IsEmpty())
			{
				LogEvent(EventType::MinorEntityProcess,peer,e,"No tasks to clear");
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
					LogEvent(EventType::MinorEntityProcess,peer,e,"No tasks to clear");
					return;
				}
			}
			else
			{
				LogEvent(EventType::CommandRejected,peer,e,"Tried to drop commands of incompatible entity");
				return;
			}
		}
		Journal::currentRound.RegisterClearEntityCommands(true,e);

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
			buffer << ' '<<n->ToString();
		LogEvent(EventType::MinorEntityProcess,peer,nullptr,"Begin: Directing entity "+cmd.id.value.ToString()+" in mode "+String(cmd.mode.value)+" given flags "+String(cmd.directFlags)+" to"+buffer.ToString());

		Objects::PWorldEntity e = CheckCommandModeGetAccessibleEntity(cmd.id,peer);
		if (!e)
			return;
		foreach (cmd.path,n)
			if (!Grid::ValidCell(*n, Objects::Satellite::BaseResidenceHeight,Objects::Satellite::DeclinedResidenceHeight))
			{
				LogEvent(EventType::CommandRejected, peer, e, "Parameter fault ("+n->ToString()+")");
				return;
			}
		const bool leftClick = (cmd.directFlags & Protocol::DirectFlag::LeftClick) != 0;
		const bool detailed = (cmd.directFlags & Protocol::DirectFlag::Detailed) != 0;
		//if (!e->DirectTo((Objects::Vehicle::mode_t)cmd.mode.value,cmd.path,detailed,leftClick))
		//{
		//	LogEvent(EventType::CommandRejected,peer,e,"Unable to comply");
		//	static StringBuffer buffer;
		//	buffer.Clear();
		//	foreach (cmd.path,p)
		//		buffer << ' '<<p->ToString();
		//	LogEvent(EventType::MinorEntityProcess,peer,e,"(Directed entity in mode "+String(cmd.mode.value)+" via click "+String(leftClick)+" to "+buffer.ToString()+" (detailed="+String(detailed)+"))");
		//	return;
		//}
		Journal::currentRound.RegisterDirectEntityTo(true,e,(Objects::Vehicle::mode_t)cmd.mode.value,cmd.path,detailed,leftClick);
	}

	#if 0

		if (!e->IsSatellite()) //must find other logic for satellites...
		{
			Objects::PMobileEntityProducer p = To<Objects::MobileEntityProducer>::DynamicCast(e);
			if (p && cmd.path.Count() == 1)
			{
				if (p->gatheringPoint.current != cmd.path.first())
				{
					LogEvent(EventType::EntityProcess, peer, e, "Set gathering point");
					p->gatheringPoint.current = cmd.path.first();
				}
				else
				{
					LogEvent(EventType::EntityProcess, peer, e, "Unset gathering point");
					p->gatheringPoint.current = Grid::LCellIdI::Invalid;
				}
				e->RegisterChange(Objects::WorldEntity::AutomationChanged,0);

				Journal::currentRound.LogDirectEntityTo(*e,p->gatheringPoint.current,(Objects::Vehicle::mode_t)0,true);

				return;
			}
		}
		Objects::PMobileEntity v = To<Objects::MobileEntity>::DynamicCast(e);
		const bool leftClick = (cmd.directFlags&Protocol::DirectFlag::LeftClick)!=0;
		const bool detailedPath = (cmd.directFlags&Protocol::DirectFlag::Detailed)!=0;
		if (v)
		{
			if (!v->DirectTo((Objects::Vehicle::mode_t)cmd.mode.value,cmd.path,detailedPath,leftClick))
			{
				LogEvent(EventType::CommandRejected,peer,e,"Unable to comply");
				LogEvent(EventType::MinorEntityProcess,peer,e,"(Directed vehicle in mode "+String(cmd.mode.value)+" via click "+String(leftClick)+" to"+buffer.ToString()+" detailed="+String(detailedPath)+")");
			}
			else
				LogEvent(EventType::MinorEntityProcess,peer,e,"Directed vehicle in mode "+String(cmd.mode.value)+" via click "+String(leftClick)+" to"+buffer.ToString()+" detailed="+String(detailedPath));
			{
				Journal::currentRound.LogDirectEntityTo(*v,cmd.target,(Objects::Vehicle::mode_t)cmd.mode.value,cmd.leftClick);
				LogEvent(EventType::MinorEntityProcess,peer,e,"Directed vehicle in mode "+String(cmd.mode.value)+" via click "+String(cmd.leftClick)+" to "+cmd.target.ToString());
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
				LogEvent(EventType::CommandRejected,peer,e,"Parameter fault");
				return;
			}

//			Objects::WorldEntity::LockChanges();
			foreach (cmd.path,n)
				if (!t->AttackNext(*n,false))
				{
					LogEvent(EventType::CommandRejected,peer,e,"Tower attack: Unable to comply");
				}
			else
			{
				Journal::currentRound.LogDirectEntityTo(*e,cmd.target,(Objects::Vehicle::mode_t)0,true);
				LogEvent(EventType::MinorEntityProcess,peer,e,"Issued tower attack to "+buffer.ToString());
			}
			return;		
		}


		LogEvent(EventType::CommandRejected,peer,e,"Tried to issue direct command on incompatible entity");
	}

	#endif /*0*/
	void	Hello(Protocol::Hello&hello, TCP::Peer&peer)
	{
		if (hello.trim().IsEmpty())
		{
			peer.SendObject(Protocol::ChannelID::Hello,Protocol::Result(false,"Invalid username"));
			return;
		}
		//foreach (State::clients,c)
		//{
		//	if ((*c)->state.playerName == state.playerName)
		//	{
		//		peer.SendObject(Protocol::ChannelID::SetClientState,Protocol::Result(false,"Name already in use"));
		//		return;
		//	}
		//
		//}

		State::PClient client = To<State::Client>::Cast(peer.attachment);
		if (!client)
		{
			TCP::PPeer ppeer = State::server.GetSharedPointerOfClient(&peer);
			if (!ppeer)	//disconnected in the meantime. rare, but it happens
			{
				LogEvent(EventType::SessionRelated,"Peer disconnected on its own before client state handling could commence. Ignoring event");
				return;
			}
			peer.userLevel = 1;
			LogEvent(EventType::Debug,peer,"User level changed to "+String(peer.userLevel));

			client.reset(new State::Client());
			client->peer = ppeer;
			ASSERT__(ppeer);
				//peer.shared_from_this();
			peer.attachment = client;
			//State::clients << client;
			client->name = hello;
			LogEvent(EventType::SessionRelated,peer,client->ToString()+" joined the session");

			
			Protocol::PhaseChange	ph;
			ph.newPhase = RuntimeConfig::phase;
			ph.roundNumber = (UINT32)RuntimeConfig::roundNumber;
			ph.timeFactor = -1;


			Channels::phaseChange.SendTo(peer,ph);



			/*
			foreach (Objects::vehicles,_v)
			{
				const Objects::Vehicle&v = **_v;
				//if (CanSee(client,v))	//doesnt have vehicles, always false, hence no vehicles at all
				{
					Protocol::VehicleConstruction	w;
					State::Describe(w,v,false, Protocol::Timestamp(true,0),0xFFFFFFFF);
					Channels::createVehicle.SendTo(peer,w);
				}
			}

			foreach (Objects::structures,_s)
			{
				const Objects::Structure&v = **_s;
				//if (CanSee(client,v))
				{
					Protocol::StructureConstruction	w;
					State::Describe(w,*_s,false, nullptr);
					Channels::createStructure.SendTo(peer,w);
				}
			}
			*/
			peer.SendObject(Protocol::ChannelID::Hello,Protocol::Result(true,"Userlevel set to "+String(peer.userLevel)));
			return;
		}
		//else
			//LogEvent(EventType::SessionRelated,client->state.playerName+" -> "+ state.playerName);


		ChangeMyName(hello,peer);
		peer.SendObject(Protocol::ChannelID::Hello,Protocol::Result(true,"User already known. Userlevel is "+String(peer.userLevel)+". Username updated"));
	}



	void OnEvent(TCP::event_t ev, TCP::Peer&peer)
	{
		switch (ev)
		{
			case TCP::Event::ConnectionEstablished:
				LogEvent(EventType::SessionRelated, peer,"Client connected");

			break;
			case TCP::Event::ConnectionClosed:
			case TCP::Event::ConnectionLost:
			{
				State::PClient client = To<State::Client>::Cast(peer.attachment);
				if (client)
				{
					LogEvent(EventType::SessionRelated,"Player disconnected: "+client->ToString());
					State::HandleConnectionLoss(client);

					peer.attachment.reset();
				}
				LogEvent(EventType::SessionRelated,peer,"Client disconnected");

				if (State::activePlayers.IsEmpty() || (State::activePlayers.Count() == 1 && State::activePlayers.first()->faction == Objects::Faction::nexus))
				{
					State::roundTimerPaused = true;
					if (State::Config::saveWhenAllQuit)
						State::AutoSave();
				}
			}
			break;
	
	
		}
	}

	void						OnIgnorePackage(UINT32 channelId, UINT32 packageSize, TCP::Peer&peer)
	{
		Logging::WriteEvent(EventType::UnexpectedBehavior, "Warning: network package (size "+String(packageSize)+", channel "+String(channelId)+") ignored",&peer,nullptr);
	}



	void OnSignal(UINT32 signal, TCP::Peer&peer)
	{
		switch (signal)
		{
			case Protocol::ChannelID::Unused:
				State::InvalidOperation(peer);
			break;
			case Protocol::ChannelID::RequestContinueRound:
			{
				
				//Objects::Tick();
				State::PClient client = To<State::Client>::Cast(peer.attachment);
				if (client && client->faction && RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase && client->wantNewRound)
				{
					client->wantNewRound = false;
					LogEvent(EventType::SessionRelated,peer,"Continued round");
					Channels::factionContinuedRound.SendTo(State::server,client->faction->id,2);
				}
			}
			break;
			case Protocol::ChannelID::RequestRoundEnd:
			{
				
				//Objects::Tick();
				State::PClient client = To<State::Client>::Cast(peer.attachment);
				if (client && client->faction && RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase && !client->wantNewRound)
				{
					client->wantNewRound = true;
					LogEvent(EventType::SessionRelated,peer,"Requested new round");
					Channels::factionRequestedNewRound.SendTo(State::server,client->faction->id,2);
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
				#ifdef _DEBUG
					LogEvent(EventType::Debug,"pong");
				#endif
			break;
		}
	}
	
}


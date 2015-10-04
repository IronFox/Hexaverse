#include <global_root.h>
#include "networkChannels.h"
#include "state.h"
#include "serverLogEvent.h"

extern Random		random;



namespace Channels
{



	
	//TCP::ObjectSender<Protocol::ServerState,Protocol::ChannelID::GetInfo>		infoSender;
	//TCP::ObjectSender<Protocol::Result,Protocol::ChannelID::


	TCP::Channel<Protocol::ClientState, 0,Protocol::ChannelID::SetClientState>						setClientState(Receiver::ClientState);
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
	TCP::Channel<Package::NetString<32>,1,Protocol::ChannelID::ChangeMyName>						changeMyName(Receiver::ChangeMyName);
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

	static bool ConvertReferenceEntity(const Protocol::MarkerReferenceEntity&source, Objects::Marker&dest )
	{
		dest.referenceEntity = source.id;
		dest.referencedType[0] = source.type0;
		dest.referencedType[1] = source.type1;
		if (dest.referencedType[0] == ConstName::Empty && dest.referencedType[1] == ConstName::Empty)
			return true;
		if (dest.referencedType[1] == ConstName::Empty)
		{
			if (!Objects::GetPrototypeByName(dest.referencedType[0]))
			{
				LogEvent(EventType::CommandRejected, "Unknown structure type #" + String(int(dest.referencedType[0])));
				return false;
			}
			return true;
		}
		if (dest.referencedType[0] == ConstName::Empty)
		{
			LogEvent(EventType::CommandRejected, "Unknown marker format");
			return false;
		}

		//String turretName = source.type.subStringRef((int)separatorAt);
		if (!Objects::Component::GetTurretByName(dest.referencedType[1]))
		{
			LogEvent(EventType::CommandRejected, "Unknown turret type #" + String(int(dest.referencedType[1])));
			return false;
		}
		if (!Objects::Component::GetVehicleBaseByName(dest.referencedType[0]))
		{
			LogEvent(EventType::CommandRejected, "Unknown base type #" + String(int(dest.referencedType[0])));
			return false;
		}
		return true;
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



	void	ClientFaction(Protocol::ClientFactionRequest&_p, TCP::Peer&peer)
	{
		if (!peer.attachment)
			return;
		State::PClient client = To<State::Client>::Cast(peer.attachment);
		Protocol::ClientFactionResponse response;
		response.startFactionTypes = _p.startFactionTypes;
		response.factionToken = _p.factionToken;
		response.planetConfig = State::persistentPlanetInfo;

		if (!response.startFactionTypes)
		{
			if (State::activePlayers.FindAndErase(client))
			{
				State::playerMap.Unset(client->faction);
				client->faction.reset();

				Channels::clientFactionResponse.SendTo(client->peer.lock(), response);
			}
			return;
		}

		if (State::phase != Protocol::Phase::CommandPhase && State::phase != Protocol::Phase::EndOfGame)
		{
			//disconnecting client. no new faction
			LogEvent(EventType::SessionRelated, peer, "Cannot connect ("+String(State::phase.ToString())+") right now. Disconnecting");
			response.startFactionTypes = 0;
			Channels::clientFactionResponse.SendTo(client->peer.lock(), response);
			client->peer.lock()->Disconnect();
			return;
		}


		if (State::activePlayers.Contains(client))
			return;


		Objects::PFaction	faction;

		bool newFaction = false;

		if (State::Config::enableNexusAI && Objects::Faction::nexus->accessToken == response.factionToken && (peer.ToString() == "localhost" || peer.ToString() == "127.0.0.1"))
		{
			LogEvent(EventType::SessionRelated, peer, "Access to nexus faction permitted.");
			faction = Objects::Faction::nexus;
		}
		#ifdef _DEBUG
			if (faction
				|| (Objects::Faction::all.IsNotEmpty() && (faction = Objects::Faction::all.first().lock()))
				|| State::tokenMap.Query(response.factionToken,faction))
		#else
			if (faction || State::tokenMap.Query(p.factionToken,faction))
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
					client->peer.lock()->Disconnect();
					return;
				}
				other->peer.lock()->Disconnect();
				State::playerMap.Unset(faction);
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

				if (State::Config::closedGame && State::roundNumber > 0)
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

		if (State::phase == Protocol::Phase::EndOfGame)
		{
			{
				client->SendProgressUpdate(true);
			}
		}

		if (newFaction)
		{
			if (State::Config::closedGame && State::roundNumber > 0)
			{
				response.startFactionTypes = 0;
				Channels::clientFactionResponse.SendTo(peer,response);
				LogEvent(EventType::SessionRelated,peer,"Game is closed. User does not have any valid access token. Join-request rejected.");
				return;
			}
			//p.factionToken.value.Generate();	let's keep it even if it's unknown
			client->faction.reset(new Objects::Faction());
			faction = client->faction;
			Objects::Faction::all << client->faction;


			
			LogEvent(EventType::SessionRelated,peer,"Created new faction.");

			client->faction->accessToken = response.factionToken;
			State::tokenMap.Set(response.factionToken,client->faction);
		}
		else
		{

		}
		State::activePlayers << client;
		State::playerMap.Set(client->faction,client);
		client->wantNewRound = false;


		Channels::clientFactionResponse.SendTo(client->peer.lock(),response);
		Channels::setFactionID.SendTo(client->peer.lock(),client->faction->id);

		//CHEAT_SHOW_ALL_RESOURCES

		#ifdef CHEAT_SHOW_ALL_RESOURCES
			faction->resourcesDiscovered.Fill(true);

		#endif
	

		Protocol::WorldDelta	delta;
		delta.timestamp.rightNow = true;
		for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
		{
			Protocol::GenerateWorldDelta(client->faction ? &client->faction->visibility : nullptr,  client->faction ? &client->faction->currentState : nullptr,
									delta,k,true,k+1 == BaseGrid::NumFamilies,newFaction ? Protocol::WorldDeltaType::ProgressiveInit : Protocol::WorldDeltaType::FactionCopy);

			Channels::updateWorld.SendTo(peer,delta);
		}

		foreach(client->faction->markers, m)
		{
			Protocol::Marker	outMarker;
			ConvertMarker(*m, outMarker);
			Channels::createMarker.SendTo(peer, outMarker);
		}


		if (newFaction)
		{
			index_t best = InvalidIndex;
			float bestDist = 0;

			Objects::Component::VehicleBase *testBase = &Objects::Component::hoverbase;

			if (State::Config::goal == State::Config::Goal::EMPConquerNexus)
			{
				count_t numZones = Objects::Faction::all.Count();	//not counting nexus faction
				numZones = (numZones/3) + !!(numZones % 3);

				int unlocked = 0;
				for (index_t i = 0; i < numZones; i++)
				{
					if (State::Config::NexusPorts::haveBases && BaseGrid::IsIsolatedCorner(unlocked))
					{
						unlocked++;
					}
					unlocked++;
				}
				

				State::unlockedEMPZones = ((1<<unlocked) - 1);
				State::empZonesLocked = ((1 << 12)-1)  & ~State::unlockedEMPZones;

				Channels::lockEmpZones.SendTo(State::server,State::empZonesLocked);

			}



			const float r = Grid::ToRadius(0);
			for (index_t i = 0; i < Grid::LowSetup::GridType::CellCount; i++)
			{
				if (State::Config::goal == State::Config::Goal::EMPConquerNexus)
				{
					if (!(State::unlockedEMPZones & Grid::low.grid.GetCell(i).cornerFlags))
						continue;

					const auto&n = Grid::low.grid.GetCell(i).neighborhood;
					bool broken = false;
					for (int k = 0; k < n.numNeighbors ; k++)
					{
						if (!(State::unlockedEMPZones & Grid::low.grid.GetCell(n.neighborID[k]).cornerFlags))
						{
							broken = true;
							break;
						}
					}
					if (broken)
						continue;
				}


				const int h0 = Objects::FindApplicableGround(Grid::LCellIdI((UINT32)i,0),true);
				if (h0 == Objects::badHeight || h0 == -Grid::CellStack::HalfRange || !testBase->CanStandOn(Grid::actualWorld,Grid::LCellIdI((UINT32)i,h0-1),false,false))
					continue;
				{
					const auto&n = Grid::low.grid.GetCell(i).neighborhood;
					bool broken = false;
					for (int k = 0; k < n.numNeighbors && !broken; k++)
					{
						const int h1 = Objects::FindApplicableGround(Grid::LCellIdI(n.neighborID[k],0),true);
						if (h1 == Objects::badHeight || h1 == -Grid::CellStack::HalfRange || !testBase->CanStandOn(Grid::actualWorld,Grid::LCellIdI(n.neighborID[k],h1-1),false,false))
							broken = true;
					}
					if (broken)
						continue;

				}


				float3 ref = Grid::low.cells[i].centerVertex.xyz;// * r;
				float dist2 = std::numeric_limits<float>::max();
				float ndist2 = std::numeric_limits<float>::max();
				foreach (Objects::structures, s)
				{
					float d2 = Vec::quadraticDistance(Grid::low.cells[(*s)->location.stackID].centerVertex.xyz,ref);
					if (!To<Objects::NexusPort>::DynamicCast(*s))
					{
						Objects::PTurretBase base = (*s)->GetTurretBase();
						if (base)
							d2 = sqr(sqrt(d2) - base->GetFlatAttackRange());	//fair enough here
						dist2 = std::min(dist2, d2);
					}
					else
					{
						ndist2 = std::min(ndist2, d2);
					}
				}
				foreach (Objects::vehicles, v)
				{
					float d2 = Vec::quadraticDistance(Grid::low.cells[(*v)->location.stackID].centerVertex.xyz,ref);
					dist2 = std::min(dist2, d2);
				}
				if (/*Objects::structures.IsEmpty() &&*/ Objects::vehicles.IsEmpty())
					dist2 = sqr(random.getFloat(1,2));
				else
					dist2 *= random.getFloat(0.9f,1.f / 0.9f);

				if (dist2 > bestDist && ndist2 > 0.25f)	//actual
				{
					bestDist = dist2;
					//nbestDist = ndist2;
					best = i;

					//if (Objects::structures.IsEmpty() && Objects::vehicles.IsEmpty())
						//break;
				}

			}


			if (State::Config::startBases)
			{
				Grid::LCellIdI location(best,0);	//first 12 are corner cells
				int h = Objects::FindApplicableGround(location,true);
				location.height = h;
				static const int want = 20;
				static const int stackHeight = want / Objects::NaturalResourceVector::TranslationFactor + (want % Objects::NaturalResourceVector::TranslationFactor ? 1 : 0);
				Grid::Resource::Require("Fuel").coverage[location.stackID] = stackHeight;
				Grid::Resource::Require("Minerals").coverage[location.stackID] = stackHeight;
				foreach (Objects::Faction::all, _f)
				{
					_f->lock()->visibility.resourcesDiscovered[location.stackID] = false;
				}

				Grid::LCellIdI at0 = Grid::LCellIdI(Grid::low.grid.GetCell(best).neighborhood.neighborID[0],h);
				Grid::LCellIdI at1 = Grid::LCellIdI(Grid::low.grid.GetCell(best).neighborhood.neighborID[1],h);
				Grid::LCellIdI at2 = Grid::LCellIdI(Grid::low.grid.GetCell(best).neighborhood.neighborID[2],h);
				Grid::LCellIdI at3 = Grid::LCellIdI(Grid::low.grid.GetCell(best).neighborhood.neighborID[3],h);
				Grid::LCellIdI at4 = Grid::LCellIdI(Grid::low.grid.GetCell(best).neighborhood.neighborID[4],h);

				Objects::PMine mine = To<Objects::Mine>::Cast(
					Objects::Mine::globalPrototype->CreateNewInstance(client->faction,location,0));
				mine->CorrectTerrain(false,0,true);
				Objects::InsertConstructed(mine,0);
				mine->SetEnabledOverride(true);
				
				Objects::PGenerator generator = To<Objects::Generator>::Cast(
					Objects::Generator::globalPrototype->CreateNewInstance(client->faction,at1,0));
				generator->CorrectTerrain(false,0,true);
				Objects::InsertConstructed(generator,0);
				generator->SetEnabledOverride(true);
				
				std::shared_ptr<Objects::MineralContainer> container = To<Objects::MineralContainer>::Cast(
					Objects::MineralContainer::globalPrototype->CreateNewInstance(client->faction,at2,0));
				container->CorrectTerrain(false,0,true);
				Objects::InsertConstructed(container,0);
				Objects::RefreshCompoundCoverage(0);


				Objects::SpawnVehicle(client->faction,at0,Objects::Component::hoverbase, Objects::Component::terraformer,true,0);
				Objects::SpawnVehicle(client->faction,at3,Objects::Component::hoverbase, Objects::Component::terraformer,true,0);
				Objects::SpawnVehicle(client->faction,at4,Objects::Component::hoverbase, Objects::Component::resourceScanner,true,0);

			}
			else
			{
				Objects::SpawnVehicle(client->faction,Grid::LCellIdI((UINT32)best,0),Objects::Component::hoverbase, Objects::Component::terraformer,true,0);
				Grid::LCellIdI at0 = Grid::LCellIdI(Grid::low.grid.GetCell(best).neighborhood.neighborID[0],0);
				Grid::LCellIdI at1 = Grid::LCellIdI(Grid::low.grid.GetCell(best).neighborhood.neighborID[1],0);
				Objects::SpawnVehicle(client->faction,at1,Objects::Component::hoverbase, Objects::Component::radar,true,0);
				Grid::LCellIdI at2 = Grid::LCellIdI(Grid::low.grid.GetCell(best).neighborhood.neighborID[2],0);
				Grid::LCellIdI at3 = Grid::LCellIdI(Grid::low.grid.GetCell(best).neighborhood.neighborID[3],0);
				Objects::SpawnVehicle(client->faction,at3,Objects::Component::hoverbase, Objects::Component::resourceScanner,true,0);
				Grid::LCellIdI at4 = Grid::LCellIdI(Grid::low.grid.GetCell(best).neighborhood.neighborID[4],0);
				Objects::SpawnVehicle(client->faction,at2,Objects::Component::hoverbase, Objects::Component::terraformer,true,0);
				Objects::SpawnVehicle(client->faction,at4,Objects::Component::hoverbase, Objects::Component::terraformer,true,0);
				Objects::SpawnVehicle(client->faction,at0,Objects::Component::hoverbase, Objects::Component::artillery,true,0);
			}
		}
		else
		{
			Protocol::Timestamp	timestamp;
			timestamp.rightNow = true;


			client->faction->canSee.VisitAllKeys([&peer,client,timestamp](const Objects::PWorldEntity&e)
			{
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

		client->SendProgressUpdate(false);
		

		Protocol::PhaseChange	ph;
		ph.newPhase = State::phase;
		ph.roundNumber = (UINT32)State::roundNumber;
		ph.timeFactor = -1;


		Channels::phaseChange.SendTo(peer,ph);


		faction->name = client->state.playerName;
		faction->online = true;
		//probably optional later on
		{
			//broadcast name and state
			Channels::identifyUser.SendTo(State::server,Protocol::IdentifyUser(client->faction->id,client->state.playerName));
			Channels::factionOnline.SendTo(State::server,client->faction->id);

		}

		LogEvent(EventType::SessionRelated,faction->name+" connected");

		for (const auto&weak : Objects::Faction::all)
		{
			Objects::PFaction f = weak.lock();
			if (f && f != client->faction)
			{
				Channels::identifyUser.SendTo(peer,Protocol::IdentifyUser(f->id,f->name));
				if (f->online)
				{
					Channels::factionOnline.SendTo(peer,f->id);
					if (State::playerMap.Require(f)->wantNewRound)
						Channels::factionRequestedNewRound.SendTo(peer,f->id);
				}
			}
		}

		{
			Objects::PFaction f = Objects::Faction::nexus;
			if (f && f != client->faction)
			{
				Channels::identifyUser.SendTo(peer,Protocol::IdentifyUser(f->id,f->name));
				if (f->online)
				{
					Channels::factionOnline.SendTo(peer,f->id);

					if (State::playerMap.Require(f)->wantNewRound)
						Channels::factionRequestedNewRound.SendTo(peer,f->id);

				}
				
			}
		}
		#ifndef NO_HIBERNATION
			Channels::uplinkBandwidth.SendTo(peer,State::Config::uplinkBandwidth);
		#endif
		Protocol::UnitLimits	limits;
		Objects::RuntimeConfig::UnitLimits::Export(limits);
		Channels::unitLimits.SendTo(peer,limits);
		
		Channels::lockEmpZones.SendTo(peer,State::empZonesLocked);
		
	}

	bool HasAccess(const State::Client&c, const Objects::WorldEntity&e)
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
	bool HasAccess(const State::PClient&c, const Objects::PWorldEntity&e)
	{
		return HasAccess(*c,*e);
	}
	bool HasAccess(const State::PClient&c, const Objects::WorldEntity&e)
	{
		return HasAccess(*c,e);
	}
	bool PeerHasAccess(TCP::Peer&peer, const Objects::WorldEntity&e)
	{
		if (!peer.attachment)
			return false;
		return HasAccess(To<State::Client>::Cast(peer.attachment),e);
	}
	bool PeerHasAccess(TCP::Peer&peer, const Objects::PWorldEntity&e)
	{
		return PeerHasAccess(peer,*e);
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
		if (State::phase == Protocol::Phase::CommandPhase)
		{
			return true;
		}
		LogEvent(EventType::CommandRejected ,peer,"Cannot issue commands in current phase ("+String(State::phase.ToString())+")");
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

		if (!PeerHasAccess(peer,e))
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

		if (failed)
		{
			LogEvent(EventType::CommandRejected, peer, "Marker creation rejected. Invalid marker configuration");
			return;
		}

		Objects::Marker&marker = f->CreateMarker(m.id,f == Objects::Faction::nexus,type,refID);

		marker.createdInRound = m.createdInRound;
		marker.location = m.location;
		if (marker.UtilizesReferenceEntity())
			failed = !ConvertReferenceEntity(*e, marker);

		if (failed)
		{
			f->DestroyMarker(marker.id);
			LogEvent(EventType::CommandRejected, peer, "Marker creation rejected. Invalid marker configuration");
			return;
		}
		//Protocol::Marker outMarker;
		//ConvertMarker(marker, outMarker);
		String ref;
		if (marker.UtilizesReferenceEntity())
			ref = ", referencing #"+String(int(marker.referencedType[0]))+"-#"+String(int(marker.referencedType[1]));
		LogEvent(EventType::Notification, peer, marker.GetName()+" created"+ref);
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
		if (f->DestroyMarker(id))
			LogEvent(EventType::Notification, peer, "Marker " + id.value.ToString() + " destroyed");
		else
			LogEvent(EventType::CommandRejected, peer, "Marker " + id.value.ToString() + " not found for destruction");

		//Channels::destroyMarker.SendTo(peer, id);	//knows already
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

	
	void	ChangeMyName(Package::NetString<32>&name, TCP::Peer&peer)
	{
		LogEvent(EventType::MinorEntityProcess, peer, nullptr, "Begin: " __func__);
		if (!peer.attachment)
		{
			LogEvent(EventType::CommandRejected, peer, "Name change denied. Player not ready");
			return;
		}
		State::PClient cl = To<State::Client>::Cast(peer.attachment);
		cl->state.playerName = name;
		Protocol::IdentifyUser userID;
		userID.id = cl->faction->id;
		userID.name = name;
		Channels::identifyUser.SendTo(State::server,userID);
		LogEvent(EventType::SessionRelated, peer, "User renamed to '"+name+"'");
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
		v->isAutomated = a.doAutomate;
		v->RegisterChange(Objects::WorldEntity::AutomationChanged,0);
		LogEvent(EventType::MinorEntityProcess,peer,e,"Modified automation-state "+String(v->isAutomated));
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
		Objects::RefreshCompoundCoverageIfNeeded(0);
		#ifndef CONNECTOR_LESS
			if (w->compoundID == Objects::Compound::InvalidNetwork)
			{
				Objects::RefreshCompoundCoverage(0);
				ASSERT__(w->compoundID != Objects::Compound::InvalidNetwork);
			}
		#endif
		LogEvent(EventType::MinorEntityProcess,peer,e,"Modified enabled-state "+String(es.enable));
		w->SetEnabled(es.enable,0);
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
		if (m->LinkToCore(0))
			LogEvent(EventType::EntityProcess,peer,e,"Core-link restored");
		else
			LogEvent(EventType::EntityProcessFailed,peer,e,"Unable to restore Core-link");
		//w->SetEnabled(es.enable);
	}
	void	RemoveEntity(NET(TUniqueID)&uuid, TCP::Peer&peer)
	{
		LogEvent(EventType::MinorEntityProcess,peer,nullptr,"Begin: Removing entity "+uuid.value.ToString());

		Objects::PWorldEntity e = CheckCommandModeGetAccessibleEntity(uuid,peer);
		if (!e)
			return;

		Objects::Remove(e,Objects::RemovalType::ControlledExplosion,0,false);	//user-requested. let's explode it anyways
		if (To<Objects::Structure>::DynamicCast(e))
			Objects::RefreshCompoundCoverageIfNeeded(0);

	}

	void	ConstructionAbort(Protocol::ConstructionAbort&ca, TCP::Peer&peer)
	{
		LogEvent(EventType::MinorEntityProcess,peer,nullptr,"Begin: Aborting vehicle plant construction for entity "+ca.plantID.value.ToString());

		Objects::PWorldEntity e = CheckCommandModeGetAccessibleEntity(ca.plantID,peer);
		if (!e)
			return;

		//Objects::PWorkingStructure ws = To<Objects::WorkingStructure>::DynamicCast(e);

		Objects::PMobileEntityProducer p = To<Objects::MobileEntityProducer>::DynamicCast(e);
		//ws?ws->ToProducer() : nullptr;
		if (!p)
		{
			LogEvent(EventType::CommandRejected,peer,e,"Tried to abort construction-plant job on non-plant entity ");
			return;
		}
		if (!p->AbortConstruction(ca.taskId))
		{
			LogEvent(EventType::CommandRejected,peer,e, "Tried to abort non-existend construction-plant job on plant entity");
		}
		else
			LogEvent(EventType::MinorEntityProcess,peer,e,"Aborted construction job "+ca.taskId.value.ToString());

	}


	void	ConstructionBegin(Protocol::ConstructionBegin&cb, TCP::Peer&peer)
	{
		LogEvent(EventType::MinorEntityProcess,peer,nullptr,"Begin: Registering vehicle plant construction, given "+String(cb.parameters.Count())+" parameter(s), for entity "+cb.plantID.value.ToString());

		Objects::PWorldEntity e = CheckCommandModeGetAccessibleEntity(cb.plantID,peer);
		if (!e)
			return;

		Objects::PMobileEntityProducer p = To<Objects::MobileEntityProducer>::DynamicCast(e);
		//Objects::PWorkingStructure ws = To<Objects::WorkingStructure>::DynamicCast(e);
		//Objects::MobileEntityProducer*p = ws?ws->ToProducer() : nullptr;
		if (p)
		{
			switch (cb.parameters.Count())
			{
				case 2:
					p->EnqueueProductionOfVehicle(cb.parameters.first(), cb.parameters.last());
				break;
				case 1:
					p->EnqueueProductionOfSatellite(cb.parameters.first());
				break;
				default:
					LogEvent(EventType::CommandRejected,peer,e,"Received invalid construction-plant job parameter configuration");
				break;
			}
			return;
		}
		//Objects::PTower t = To<Objects::Tower>::DynamicCast(e);	//deprecated concept
		//if (t)
		//{
		//	if (cb.parameters.Count() != 1)
		//	{
		//		LogEvent(EventType::CommandRejected,peer,e,"Received invalid tower job parameter configuration");
		//		return;
		//	}
		//	t->Enqueue(cb.parameters.first());
		//	return;
		//}

		LogEvent(EventType::CommandRejected,peer,e,"Tried to issue construction job on incompatible entity");
		return;

	}


	void	SignalNewJob(Protocol::StandardJob&job, TCP::Peer&peer)
	{
		LogEvent(EventType::MinorEntityProcess,peer,nullptr,"Begin: Registering new Job #"+String(int(job.jobName.value))+" for vehicle "+job.id.value.ToString());

		Objects::PWorldEntity e = CheckCommandModeGetAccessibleEntity(job.id,peer);
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
		switch (job.jobName)
		{
			case ConstName::Job_BuildRoad:
			{
				if (v->GetLastPointOfOrigin().height <= -Grid::CellStack::HalfRange)
				{
					LogEvent(EventType::CommandRejected,peer,e,"Cannot build road on bottom layer");
					return;
				}

				PVehicleJob job(new VehicleJob::BuildRoad(*v));
				v->RegisterNewJob("", job,nullptr);
				LogEvent(EventType::MinorEntityProcess,peer,e,"Registered new job "+job->GetName());
			}
			break;
			case ConstName::Job_BuildWallUp:
			{
				Grid::LCellIdI target = v->GetLastPointOfOrigin()+1;
				if (target.height+1 >= Grid::CellStack::HalfRange)
				{
					LogEvent(EventType::CommandRejected,peer,e,"Cannot build wall on top layer");
					return;
				}
				PVehicleJob job(new VehicleJob::BuildWallUp(*v));
				v->RegisterNewJob("", job,&target);
				LogEvent(EventType::MinorEntityProcess,peer,e,"Registered new job "+job->GetName());
			}
			break;
			case ConstName::Job_BuildHorizontalWall:
			{
				Grid::LCellIdI target = v->GetLastPointOfOrigin()+1;
				//if (target.height+1 >= Grid::CellStack::HalfRange)
				//{
				//	LogEvent(EventType::CommandRejected,peer,e,"Cannot build wall on top layer");
				//	return;
				//}
				PVehicleJob job(new VehicleJob::BuildHorizontalWall(*v));
				v->RegisterNewJob("", job,nullptr);
				LogEvent(EventType::MinorEntityProcess,peer,e,"Registered new job "+job->GetName());
			}
			break;
			case ConstName::Job_DigDown:
			{
				Grid::LCellIdI target = v->GetLastPointOfOrigin()-1;
				if (target.height < -Grid::CellStack::HalfRange)
				{
					LogEvent(EventType::CommandRejected,peer,e,"Bottom layer reached. Cannot dig any further");
					return;
				}
				else
				{
					PVehicleJob job(new VehicleJob::DigDown(*v));
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

				auto loc = v->GetLastPointOfOrigin();
				/*if (!p->CanPlace(loc))
				{
					LogEvent("Unable to place " + p->GetTypeName() + " here");
					return;
				}*/

				Objects::PStructure p;
				foreach (Objects::structurePrototypes,proto)
					if ((*proto)->GetCTypeName() == job.parameterName)
					{
						p = *proto;
						break;
					}
				if (p)
				{	
					//if (To<Objects::Mine>::DynamicCast(p))
					//{
					//	count_t numMines = 0;
					//	for (const auto&w : v->owner->structures)
					//	{
					//		if (To<Objects::Mine>::DynamicCast(w.lock()))
					//			numMines ++;
					//	}
					//	for (const auto&w : v->owner->vehicles)
					//	{
					//		PVehicle o = w.lock();
					//		if (o)
					//			for (const auto&job : o->myTasks)
					//				if (job.IsJob())
					//				{
					//					Objects::VehicleJob::PConstruct cnst = To<Objects::VehicleJob::Construct>::DynamicCast(job.job);
					//					if (cnst && To<Objects::Mine>::DynamicCast(cnst->constructWhat))
					//						numMines++;
					//			
					//				}
					//	}
					//	if (numMines >= State::Config::limitMinesTo)
					//	{
					//		LogEvent(EventType::CommandRejected,peer,e,"Maximum mine count ("+String(State::Config::limitMinesTo)+") reached");
					//		return;
					//	}
					//}


					PVehicleJob job(new VehicleJob::Construct(*v,p));

					v->RegisterNewJob(p->GetTypeName(),job,&loc);
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
				LogEvent(EventType::CommandRejected,peer,v,"Illegal job name #"+String(int(job.jobName.value)));
				State::InvalidOperation(peer);
			}
		}
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

		if (!Objects::ProcessChoiceIntersection(e,ch.target,(Objects::Vehicle::mode_t)ch.mode.value,deployingWhat))
		{
			LogEvent(EventType::CommandRejected,peer,e,"Invalid choice intersection");
			return;
		}
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
			v->DropLastTask();
			LogEvent(EventType::MinorEntityProcess,peer,e,"Cleared last entity task");
			return;
		}

		Objects::PTower t = To<Objects::Tower>::DynamicCast(e);
		if (t)
		{
			t->DropLastTask();
			LogEvent(EventType::MinorEntityProcess,peer,e,"Cleared last tower attack target");
			return;
		}

		LogEvent(EventType::CommandRejected,peer,e,"Tried to drop command on incompatible entity");
		return;
		
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
			v->ClearAllTasks(false,CLOCATION);
			LogEvent(EventType::MinorEntityProcess,peer,e,"Cleared all mobile entity tasks");
			return;
		}

		Objects::PTower t = To<Objects::Tower>::DynamicCast(e);
		if (t)
		{
			t->ClearTasks();
			LogEvent(EventType::MinorEntityProcess,peer,e,"Cleared all tower attack targets");
			return;
		}

		LogEvent(EventType::CommandRejected,peer,e,"Tried to drop command on incompatible entity");
		return;
	}


	/**
	Handles client directing world entities in various modes.
	Most commonly this moves a vehicle, but may also schedule an attack or set a gathering point
	*/
	void	EntityDirectCommand(Protocol::EntityDirectCommand&cmd, TCP::Peer&peer)
	{
		LogEvent(EventType::MinorEntityProcess,peer,nullptr,"Begin: Directing entity "+cmd.id.value.ToString()+" in mode "+String(cmd.mode.value)+" via click "+String(cmd.leftClick)+" to "+cmd.target.ToString());

		Objects::PWorldEntity e = CheckCommandModeGetAccessibleEntity(cmd.id,peer);
		if (!e)
			return;

		if (!Grid::ValidCell(cmd.target, Objects::Satellite::BaseResidenceHeight,Objects::Satellite::DeclinedResidenceHeight))
		{
			LogEvent(EventType::CommandRejected, peer, e, "Parameter fault");
			return;
		}

		if (!e->IsSatellite()) //must find other logic for satellites...
		{
			Objects::PMobileEntityProducer p = To<Objects::MobileEntityProducer>::DynamicCast(e);
			if (p)
			{
				if (p->gatheringPoint.current != cmd.target)
				{
					LogEvent(EventType::EntityProcess, peer, e, "Set gathering point");
					p->gatheringPoint.current = cmd.target;
				}
				else
				{
					LogEvent(EventType::EntityProcess, peer, e, "Unset gathering point");
					p->gatheringPoint.current = Grid::LCellIdI::Invalid;
				}
				e->RegisterChange(Objects::WorldEntity::AutomationChanged,0);

				return;
			}
		}
		Objects::PMobileEntity v = To<Objects::MobileEntity>::DynamicCast(e);
		if (v)
		{
			if (!v->DirectTo((Objects::Vehicle::mode_t)cmd.mode.value,cmd.target,cmd.leftClick))
			{
				LogEvent(EventType::CommandRejected,peer,e,"Unable to comply");
				LogEvent(EventType::MinorEntityProcess,peer,e,"(Directed vehicle in mode "+String(cmd.mode.value)+" via click "+String(cmd.leftClick)+" to "+cmd.target.ToString()+")");
			}
			else
				LogEvent(EventType::MinorEntityProcess,peer,e,"Directed vehicle in mode "+String(cmd.mode.value)+" via click "+String(cmd.leftClick)+" to "+cmd.target.ToString());
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
			if (cmd.mode != Objects::CursorMode::AttackMode || !cmd.leftClick)
			{
				LogEvent(EventType::CommandRejected,peer,e,"Parameter fault");
				return;
			}

//			Objects::WorldEntity::LockChanges();
			if (!t->AttackNext(cmd.target,false))
			{
				LogEvent(EventType::CommandRejected,peer,e,"Unable to comply");
			}
	//		Objects::WorldEntity::UnlockChanges();

			//Protocol::StructureState	w;
			//State::Describe(w,t,true,Protocol::Timestamp(true,0),Objects::WorldEntity::TasksChanged);

			//Channels::setStructureState.SendTo(peer,w);	
			LogEvent(EventType::MinorEntityProcess,peer,e,"Issued tower attack to "+cmd.target.ToString());
			return;		
		}


		LogEvent(EventType::CommandRejected,peer,e,"Tried to issue direct command on incompatible entity");
	}

	void	ClientState(Protocol::ClientState&state, TCP::Peer&peer)
	{
		if (state.playerName.trim().IsEmpty())
		{
			peer.SendObject(Protocol::ChannelID::SetClientState,Protocol::Result(false,"Invalid username"));
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
			client.reset(new State::Client());
			client->peer = ppeer;
			ASSERT__(ppeer);
				//peer.shared_from_this();
			peer.attachment = client;
			State::clients << client;
			client->state = state;
			LogEvent(EventType::SessionRelated,peer,client->ToString()+" joined the session");

			



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

		}
		//else
			//LogEvent(EventType::SessionRelated,client->state.playerName+" -> "+ state.playerName);

		client->state = state;
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
					State::clients.FindAndErase(client);
					State::activePlayers.FindAndErase(client);
					if (client->faction)
					{
						State::playerMap.Unset(client->faction);
						Channels::factionOffline.SendTo(State::server,client->faction->id);
						client->faction->online = false;
					}

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
			case Protocol::ChannelID::RequestRoundEnd:
			{
				
				//Objects::Tick();
				State::PClient client = To<State::Client>::Cast(peer.attachment);
				if (client && client->faction && State::phase == Protocol::Phase::CommandPhase)
				{
					client->wantNewRound = true;
					LogEvent(EventType::SessionRelated,peer,"Requested new round");
					Channels::factionRequestedNewRound.SendTo(State::server,client->faction->id);
				}

			}
			break;
			case Protocol::ChannelID::Ping:
				if (peer.attachment)
				{
					State::PClient client = To<State::Client>::Cast(peer.attachment);
					client->lastPing = timer.Now();
				}
				peer.SendSignal(Protocol::ChannelID::Pong);
				LogEvent(EventType::Debug,"pong");
			break;
		}
	}
	
}


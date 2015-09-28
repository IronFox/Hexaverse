#include <global_root.h>
#include "network.h"
#include "../version.h"
#include "animation.h"
#include "runtimeConfig.h"
#include "world.h"
#include "cameraControl.h"
#include "ui.h"
#include "tutorial.h"
#include "dialog.h"
#include "ai.h"




void			SignalMarkerDeletion(TUniqueID markerID);	//must be copy to prevent weird shit

#ifdef HAS_WINDOW
	void		QueryNicknameIfEmpty();

#endif


namespace Network
{
	bool			amReady = false,haveId = false,amOnline=false;
	#ifdef HAS_WINDOW
		bool		amLocated=false;
	#endif

	TUniqueID		myID,serverID,worldID;
	CryptographicID	activeToken;
	String			myIDString,serverIDString,worldIDString,activeTokenString;
	Objects::OwnerParameter myFaction;
	INT32			uplinkBandwidth = -1;
	
	GenericHashTable<TUniqueID,CryptographicID,TUniqueID>	accessMap;
	GenericHashTable<TUniqueID,Objects::OwnerParameter,TUniqueID>	factionMap;


	UINT32			roundNumber;
					//dbgRoundNumber = 0xFFFFFFFF;
	Protocol::GameProgress	gameProgress;

	Protocol::Phase	serverPhase;
	float			animationTimeFactor = 1,
					roundTimer = 0;
	Timer::Time		phaseStarted;
	bool			newRoundRequested = false;



	Objects::OwnerParameter ResolveFaction(const TUniqueID&id)
	{
		{
			Objects::OwnerParameter*rs = factionMap.QueryPointer(id);
			if (rs)
				return *rs;
		}
		Objects::OwnerParameter rs(new Objects::Faction());
		rs->id = id;
		factionMap.Set(id,rs);
		Objects::Faction::allKnown << rs;
		return rs;
	}

	void SaveWorldAccessTokens()
	{
		FileSystem::CreateDirectory("config");
		FILE*f = fopen(accessFilename, "wb+");
		if (!f)
		{
			LogEvent(EventType::InternalFault,nullptr,"Unable to write to world access map");
			return;
		}

		accessMap.visitAllEntries([f](const TUniqueID&id, const CryptographicID&access)
		{
			fwrite(id.bytes,sizeof(id.bytes),1,f);
			access.Write(f);
		});

		fclose(f);
		LogEvent(EventType::SessionRelated,nullptr,"Updated world access map");
	}

	void LoadWorldAccessTokens()
	{
		FILE*f = fopen(accessFilename, "rb");
		if (!f)
			return;

		accessMap.Clear();
		TUniqueID id;
		CryptographicID access;
		while (!feof(f))
		{
			if (fread(id.bytes,sizeof(id.bytes),1,f))
			{
				access.Read(f);
				accessMap.Set(id,access);
			}
		}

		LogEvent(EventType::SessionRelated,nullptr,"World access map imported. "+String(accessMap.count())+" entry/ies loaded.");
		fclose(f);
	}

	
	namespace ServerDiscovery
	{


		void OnServerInfo(Protocol::ServerInfo&info, TCP::Peer&peer);
		void OnPing(Protocol::Ping&ping, TCP::Peer&peer);

		TCP::Channel<Protocol::ServerInfo,0,Protocol::InfoChannel::ServerInfo>	serverInfoChannel(OnServerInfo);
		TCP::Channel<Protocol::Ping,0,Protocol::InfoChannel::Ping>	pingChannel(OnPing);

		class HostContactAttempt : public ThreadObject, public TCP::Peer::Attachment, public std::enable_shared_from_this<HostContactAttempt>
		{
		public:
			TCP::Client		client;
			String			url;
			bool			hasContact,hasInfo,rightVersion = false;
			Protocol::ServerInfo	lastInfo;
			float			lastPing;
			//Semaphore		sem;

			/**/			HostContactAttempt():/*sem(0),*/hasContact(false),hasInfo(false),lastPing(0)	{}


			static void		OnEvent(TCP::event_t ev, TCP::Peer&peer);

			void			ThreadMain() override
			{
				client.SetAsync(true);
				client.onEvent = OnEvent;
				client.attachment = shared_from_this();
				client.InstallChannel(serverInfoChannel);
				client.InstallChannel(pingChannel);

				while (!application_shutting_down)
				{
					hasContact = false;
					lastPing = 0;
					hasInfo = false;
					while (!application_shutting_down)
					{
						if (client.Connect(url + ":"+String(INFO_PORT)))
						{
							break;
						}
						Sleep(1000);
					}
					hasContact = true;
					while (!application_shutting_down && client.IsConnected())
					{
						pingChannel.SendTo(client,timer.Now());
						Sleep(200);
					}
					//if (!application_shutting_down)
						//sem.enter();
				}
			}

			//void			Release()
			//{
			//	sem.release();
			//}


		};
		typedef std::shared_ptr<HostContactAttempt>	PHostContactAttempt;

		Buffer0<PHostContactAttempt>	attempts;

		void OnServerInfo(Protocol::ServerInfo&info, TCP::Peer&peer)
		{
			PHostContactAttempt me = To<HostContactAttempt>::Cast(peer.attachment);
			me->lastInfo = info;
			info.hostInfo.value.version[ARRAYSIZE(info.hostInfo.value.version)-1] = 0;
			me->hasInfo = true;
			me->rightVersion = !strcmp(info.hostInfo.value.version, SERVER_NAME);
		}

		void OnPing(Protocol::Ping&ping, TCP::Peer&peer)
		{
			PHostContactAttempt me = To<HostContactAttempt>::Cast(peer.attachment);
			me->lastPing = timer.GetSecondsSince(ping);
		}


		/*static*/ void		HostContactAttempt::OnEvent(TCP::event_t ev, TCP::Peer&peer)
		{
			PHostContactAttempt me = To<HostContactAttempt>::Cast(peer.attachment);
			switch (ev)
			{
				case TCP::Event::ConnectionClosed:
				case TCP::Event::ConnectionLost:
					//me->Release();
				break;
			}

		}


		void	RegisterNewHost(const String&host, bool probablyRightVersion)
		{
			PHostContactAttempt&attempt = attempts.Append();
			attempt.reset(new HostContactAttempt());
			attempt->url = host;
			attempt->rightVersion = probablyRightVersion;
			
			attempt->start();
		}


	}




	namespace Receiver
	{
		bool			eventsLocked = true;


		void	OnClientStateResponse(Protocol::Result&response)
		{
			ASSERT__(response.success);
			if (response.success)
				LogEvent(EventType::SessionRelated, nullptr, "Status confirmed");
		}

		void	OnWorldDelta(Protocol::WorldDelta&delta)
		{
			ASSERT__(!eventsLocked);
			if (!delta.timestamp.rightNow)
			{
				foreach (Animation::primitives,p)
					if (p->type == Animation::Primitive::TerrainChange )
					{
						if (p->time.min == delta.timestamp.atTime)
						{
							p->worldDelta->Append().swap(delta);
							return;
						}

					}

				Animation::primitives.Append().Init(delta);
			}
			else
			{
				BYTE family = delta.familyAndLastFlag & 0x7F;
				bool last = (delta.familyAndLastFlag & 0x80) != 0;
				Grid::actualWorld.ApplyFamilyDelta(family,delta.data0,delta.data1,false,true);

				#ifdef HAS_WINDOW
					World::allowRecompilation = last;
				#endif
			}

		}




		void OnMobileState(Protocol::MobileState&vs)
		{
			ASSERT__(!eventsLocked);

			if (vs.entityState.timestamp.rightNow && Animation::AllDone())
			{
				Objects::PWorldEntity e =Objects::GetEntity(vs.entityState.id);
				ASSERT__(e);
				Objects::PMobileEntity v = To<Objects::MobileEntity>::DynamicCast(e);
				ASSERT__(v);
				Animation::ApplyState(v,vs,false,false);
			}
			else
			{
				if (vs.entityState.timestamp.rightNow)
					Animation::whenDone.Append().Init(vs);
				else
					Animation::primitives.Append().Init(vs);
			}
		}

		void	OnNewMobileEntity(Protocol::NewMobileEntity&vc)
		{
			ASSERT__(!eventsLocked);
			ASSERT__(Network::haveId);

			Objects::PMobileEntity v = Objects::SpawnMobileEntity(Network::ResolveFaction(vc.owner),  vc.state.entityState.id,vc.state.entityState.location,vc.baseName,vc.turretName,vc.state.entityState.timestamp.rightNow,Network::haveId && Network::myID == vc.owner);
			if (vc.state.entityState.timestamp.rightNow)
				LogEvent(EventType::MinorEntityProcess,v.get(),  "Placed vehicle entity outside animation");
			bool isMine = v->isMine;

			//v->id = vc.state.entityState.id;
			Animation::ApplyState(v,vc.state,true, !vc.state.entityState.timestamp.rightNow);
			#ifdef HAS_WINDOW
				if (v->isMine && !amLocated)
				{
					amLocated = true;
					Camera::TranslateTo(v->system.w.xyz);
				}
			#endif
			if (vc.state.entityState.timestamp.rightNow)
			{
				if (!isMine)
					UI::HexGrid::RefreshContacts();
			}
			else
			{
				Animation::primitives.Append().Init(vc,v);
			}
		}

		

		void	OnNewStructure(Protocol::StructureConstruction&sc)
		{
			ASSERT__(!eventsLocked);

			#ifdef HAS_WINDOW
				if (Grid::actualWorld.lightingOutdated && World::allowRecompilation)
					Grid::actualWorld.UpdateLighting();
			#endif

			Objects::PStructure s = Objects::SpawnStructure(Network::ResolveFaction(sc.owner),  sc.state.entityState.id,sc.state.entityState.location,sc.typeName, sc.state.entityState.timestamp.rightNow, Network::haveId && Network::myID == sc.owner);
			ASSERT__(s);
			if (sc.state.entityState.timestamp.rightNow)
				LogEvent(EventType::MinorEntityProcess,s.get(),  "Placed structure entity outside animation");

			Animation::UpdateState(s,sc.state);
			if (!sc.state.entityState.timestamp.rightNow)
			{
				Animation::primitives.Append().Init(sc,s);
			}
			else
			{
				if (!s->isMine)
					UI::HexGrid::RefreshContacts();
			}
		}

		void OnStructureState(Protocol::StructureState&vs)
		{
			ASSERT__(!eventsLocked);

			if (vs.entityState.timestamp.rightNow && Animation::AllDone())
			{
				Objects::PWorldEntity e =Objects::GetEntity(vs.entityState.id);
				ASSERT1__(e,vs.entityState.id.value.ToString());
				Objects::PStructure s = To<Objects::Structure>::DynamicCast(e);
				ASSERT1__(s,vs.entityState.id.value.ToString());
				Animation::UpdateState(s,vs);

			}
			else
			{
				if (vs.entityState.timestamp.rightNow)
					Animation::whenDone.Append().Init(vs);
				else
					Animation::primitives.Append().Init(vs);
			}

		}

		void DestroyEntity(Protocol::DestroyEntity&de)
		{
			ASSERT__(!eventsLocked);

			if (de.timestamp.rightNow && Animation::AllDone())
			{
				const Objects::PWorldEntity&e = Objects::GetEntity(de.id);
				ASSERT__(e);
				LogEvent(EventType::MinorEntityProcess,e.get(),  "Removed entity outside animation");

				const Objects::RemovalType type = Objects::RemovalType::Reinterpret(de.type);
				bool explode = type == Objects::RemovalType::ControlledExplosion 
							|| type == Objects::RemovalType::UncontrolledExplosion;

				if (explode)
				{
					Animation::ExplodeEntity(e,[e,type]()
					{
						if (Objects::GetEntity(e->id))
							Objects::Remove(e,type, 0,Network::ControlEnabled());
						#ifdef HAS_WINDOW
							Dialog::SignalSituationChange();
						#endif
					});
				}
				else
				{
					Objects::Remove(e,type, 0,Network::ControlEnabled());
					#ifdef HAS_WINDOW
						Dialog::SignalSituationChange();
					#endif
				}

				if (!e->isMine)
					UI::HexGrid::RefreshContacts();
			}
			else
			{
				LogEvent(EventType::Debug,nullptr,"Reception of entity destruction message for "+de.id.value.ToString()+" ("+Objects::RemovalType::Reinterpret(de.type.value).ToString()+")");
				if (de.timestamp.rightNow)
					Animation::whenDone.Append().Init(de);
				else
					Animation::primitives.Append().Init(de);
			}
		}

		void ControlMessage(Protocol::ControlMessage&msg)
		{
			LogEvent(EventType::Reinterpret(msg.type),nullptr,">"+msg.message);
		}



		void	ClientReady(Protocol::ClientReady&cr)
		{
			ASSERT__(!eventsLocked);

			Grid::CellStack::bottom.content = Grid::Content::Reinterpret(cr.planetConfig.value.innerCoreType);
			#ifdef HAS_WINDOW
				Grid::CellStack::bottom.visible = 1;
				Grid::UpdateAtmosphere(cr.planetConfig.value.atmosphereColor);
				Grid::UpdateSun(cr.planetConfig.value.sunPlaneNormal_Speed,cr.planetConfig.value.sunColor);
			#endif


			amReady = cr.isReady;
			if (amReady)
			{
				activeToken = cr.factionToken;
				activeTokenString = activeToken.ToString();
				if (!RuntimeConfig::aiMode)
				{
					LogEvent(EventType::SessionRelated,nullptr,"Updating access token of world "+worldID.ToString()+" to "+activeToken.ToString());
					accessMap.Set(worldID,activeToken);
					SaveWorldAccessTokens();
				}
			}
		}

		void	SetFactionID(NET(TUniqueID)&id)
		{
			ASSERT__(!eventsLocked);

			haveId = true;
			myID = id;
			myFaction = ResolveFaction(myID);
			myIDString = myID.ToString();
			myFaction->isMine = true;
		}

		void	PhaseChange(Protocol::PhaseChange&c);
		void	MobileEntityMotion(Protocol::MobileEntityMotion&m)
		{
			ASSERT__(!eventsLocked);

//			Objects::PWorldEntity e = Objects::GetEntity(m.id,true);
	//		Objects::PVehicle v = To<Objects::Vehicle>::DynamicCast(e);
		//	ASSERT__(v);
			if (m.timestamp.rightNow)
			{
				Objects::PWorldEntity e = Objects::GetEntity(m.id);
				Objects::PMobileEntity v = To<Objects::MobileEntity>::DynamicCast(e);
				ASSERT__(v);
				v->Relocate(m.toLocation,true);
				//v->location = m.toLocation;
				//v->UpdateSystem();
			}
			else
			{
				bool found = false;
				foreach (Animation::primitives,p)
					if (p->targetEntityID == m.id && p->type == Animation::Primitive::MovementStream && p->path->rawPath.last().first == m.fromLocation)
					{
						p->Append(m);
						found = true;
						break;
					}
				if (!found)
				{
					Animation::primitives.Append().Init(m);
				}

			}
		}

		void	AttackTrajectory(Protocol::AttackTrajectory&a)
		{
			ASSERT__(!eventsLocked);

			if (a.timestamp.rightNow)
			{
				FATAL__("weird");
			}
			else
			{
				Animation::primitives.Append().Init(a);
			}
			
		}

		void	Explosion(Protocol::Explosion&ex)
		{
			ASSERT__(!eventsLocked);

			if (ex.timestamp.rightNow)
			{
				FATAL__("weird");
			}
			else
			{
				Animation::primitives.Append().Init(ex);
			}
			
		}

		void	EntityTurretRotation(Protocol::EntityTurretRotation&rot)
		{
			ASSERT__(!eventsLocked);

			if (rot.timestamp.rightNow)
			{
				Objects::PWorldEntity e = Objects::GetEntity(rot.id);
				Objects::PVehicle v = To<Objects::Vehicle>::DynamicCast(e);
				ASSERT__(v);
				float3 dir = rot.toWorld;
				float3 innerDir;
				Mat::rotate(v->inverse,dir,innerDir);
				v->turretAngleX = Vec::angle2PI(innerDir.xy) + M_PIF/2.f;
				v->turretAngleY = -(90.f - Vec::angle(rot.toWorld.value,Grid::low.cells[v->location.stackID].centerVertex.xyz)) * M_PIF / 180.f;
			}
			else
				Animation::primitives.Append().Init(rot);

		}


		void	ResourceUpdate(Protocol::ResourceUpdate&u)
		{
			ASSERT__(!eventsLocked);

			if (u.timestamp.rightNow)
			{
				#ifdef HAS_WINDOW
					Grid::UpdateResourceMap(u.resourceName,u.cells);
				#endif
			}
			else
				Animation::primitives.Append().Init(u);

		}

		void	ChangeEntityOwner(Protocol::ChangeEntityOwner&c)
		{
			Objects::PWorldEntity e = Objects::GetEntity(c.entityID);
			ASSERT__(e);
			ASSERT__(e->owner->id != c.ownerID);
			Objects::ChangeOwnership(e,Network::ResolveFaction(c.ownerID),Network::myID == c.ownerID);
		}

		void	GameProgress(Protocol::GameProgress&progress)
		{
			Network::gameProgress.swap(progress);
		}

		void	CreateMarker(Protocol::Marker&marker)
		{
			TUniqueID	entityID = TUniqueID::Empty();
			ConstName	entityType[2];
			Objects::Marker::Type type;

			const Protocol::MarkerReferenceEntity*entity = marker.data.Get0();
			if (entity)
			{
				type = Objects::Marker::Type::Reinterpret(0);
				entityID = entity->id;
				entityType[0] = entity->type0;
				entityType[1] = entity->type1;
			}
			else
			{
				entity = marker.data.Get1();
				if (entity)
				{
					type = Objects::Marker::Type::Reinterpret(1);
					entityID = entity->id;
					entityType[0] = entity->type0;
					entityType[1] = entity->type1;
				}
				else
				{
					ASSERT_NOT_NULL__(marker.data.Get2());
					type = Objects::Marker::Type::Reinterpret(2);
				}
			}
			Objects::Marker::Create(marker.id, type, marker.location, marker.createdInRound, entityID, entityType, std::max<int>(Network::roundNumber,marker.createdInRound));
			#ifdef HAS_WINDOW
				OnSelectionChange(true);
			#endif
		}

		void	MarkerCreationFailedLimitReached(Protocol::Marker&marker);	//must re-send, must know channels, cannot be implemented here

		void	DestroyMarker(NET(TUniqueID)&mid)
		{
			Objects::Marker::Destroy(mid, ControlEnabled());
			#ifdef HAS_WINDOW
				OnSelectionChange(true);
			#endif
		}

		void	IdentifyUser(Protocol::IdentifyUser&id);
		void	FactionOnline(NET(TUniqueID)&);
		void	FactionOffline(NET(TUniqueID)&);
		void	FactionRequestedNewRound(NET(TUniqueID)&);
		void	LockEmpZones(NET(UINT16)&);
		void	UplinkBandwidth(NET(INT32)&v)
		{
			uplinkBandwidth = v;
			Objects::RuntimeConfig::hibernationSystemEnabled = v > 0;
			#ifdef HAS_WINDOW
				OnSelectionChange(true);
			#endif
		}

		void	UnitLimits(Protocol::UnitLimits&limits)
		{
			Objects::RuntimeConfig::UnitLimits::Import(limits);
			#ifdef HAS_WINDOW
				OnSelectionChange(true);
			#endif

		}
	}


	namespace Channels
	{
		TCP::Channel<Protocol::ControlMessage,0,Protocol::ChannelID::ControlMessage>				controlMessage(Receiver::ControlMessage);

		TCP::Channel<Protocol::Result,0,Protocol::ChannelID::SetClientState>						clientStateUpdateResponse(Receiver::OnClientStateResponse);
		TCP::Channel<Protocol::WorldDelta,0,Protocol::ChannelID::WorldDelta>						worldData(Receiver::OnWorldDelta);
		TCP::Channel<Protocol::NewMobileEntity,0,Protocol::ChannelID::NewMobileEntity>				newMobileEntity(Receiver::OnNewMobileEntity);
		TCP::Channel<Protocol::MobileState,0,Protocol::ChannelID::MobileState>						mobileStateUpdate(Receiver::OnMobileState);
		TCP::Channel<Protocol::StructureConstruction,0,Protocol::ChannelID::InstantiateStructure>	newStructure(Receiver::OnNewStructure);
		TCP::Channel<Protocol::StructureState,0,Protocol::ChannelID::UpdateStructure>				structureStateUpdate(Receiver::OnStructureState);
		TCP::Channel<Protocol::DestroyEntity,0,Protocol::ChannelID::DestroyEntity>					destroyEntity(Receiver::DestroyEntity);
		TCP::Channel<Protocol::ClientReady,0,Protocol::ChannelID::ClientReady>						clientReady(Receiver::ClientReady);
		TCP::Channel<NET(TUniqueID),0,Protocol::ChannelID::SetFactionID>							setFactionID(Receiver::SetFactionID);

		TCP::Channel<Protocol::PhaseChange,0,Protocol::ChannelID::PhaseChange>						phaseChange(Receiver::PhaseChange);
		TCP::Channel<Protocol::MobileEntityMotion,0,Protocol::ChannelID::MobileEntityMotion>		mobileEntityMotion(Receiver::MobileEntityMotion);
		TCP::Channel<Protocol::AttackTrajectory,0,Protocol::ChannelID::AttackTrajectory>			attackTrajectory(Receiver::AttackTrajectory);
		TCP::Channel<Protocol::Explosion,0,Protocol::ChannelID::Explosion>							explosion(Receiver::Explosion);
		TCP::Channel<Protocol::EntityTurretRotation,0,Protocol::ChannelID::EntityTurretRotation>	entityTurretRotation(Receiver::EntityTurretRotation);
		TCP::Channel<Protocol::ResourceUpdate,0,Protocol::ChannelID::ResourceUpdate>				resourceUpdate(Receiver::ResourceUpdate);
		TCP::Channel<Protocol::ChangeEntityOwner,0,Protocol::ChannelID::ChangeEntityOwner>			changeEntityOwner(Receiver::ChangeEntityOwner);
		TCP::Channel<Protocol::GameProgress,0,Protocol::ChannelID::GameProgress>					gameProgress(Receiver::GameProgress);

		TCP::Channel<Protocol::Marker, 0, Protocol::ChannelID::CreateMarker>						createMarker(Receiver::CreateMarker);
		TCP::Channel<Protocol::Marker, 0, Protocol::ChannelID::MarkerCreationFailedLimitReached>	markerCreationFailedLimitReached(Receiver::MarkerCreationFailedLimitReached);
		TCP::Channel<NET(TUniqueID), 0, Protocol::ChannelID::DestroyMarker>							destroyMarker(Receiver::DestroyMarker);

		TCP::Channel<Protocol::IdentifyUser, 0, Protocol::ChannelID::IdentifyUser>					identifyUser(Receiver::IdentifyUser);
		TCP::Channel<NET(TUniqueID), 0, Protocol::ChannelID::FactionOnline>							factionOnline(Receiver::FactionOnline);
		TCP::Channel<NET(TUniqueID), 0, Protocol::ChannelID::FactionOffline>						factionOffline(Receiver::FactionOffline);
		TCP::Channel<NET(TUniqueID), 0, Protocol::ChannelID::FactionRequestedNewRound>				factionRequestedNewRound(Receiver::FactionRequestedNewRound);
		TCP::Channel<NET(UINT16), 0, Protocol::ChannelID::LockEMPZones>								lockEmpZones(Receiver::LockEmpZones);
		TCP::Channel<NET(INT32), 0, Protocol::ChannelID::UplinkBandwidth>							uplinkBandwidth(Receiver::UplinkBandwidth);
		TCP::Channel<Protocol::UnitLimits, 0, Protocol::ChannelID::UnitLimits>						unitLimits(Receiver::UnitLimits);


		TCP::ObjectSender<Protocol::TouchedMarker,Protocol::ChannelID::TouchMarker>					touchMarker;
		TCP::ObjectSender<Protocol::ClientState,Protocol::ChannelID::SetClientState>				clientStateSender;
		TCP::ObjectSender<Protocol::EntityDirectCommand, Protocol::ChannelID::EntityDirectCommand>	entityDirectCommand;
		TCP::ObjectSender<Protocol::ChoiceIntersection, Protocol::ChannelID::ChoiceIntersection>	choiceIntersection;
		TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::DropLastTask>						dropLastTask;
		TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::DropAllTasks>						dropAllTasks;
		TCP::ObjectSender<Protocol::StandardJob, Protocol::ChannelID::SignalNewJob>					signalNewJob;
		TCP::ObjectSender<Protocol::TerrainMod, Protocol::ChannelID::SignalTerrainMod>				signalTerrainMod;
		TCP::ObjectSender<Protocol::ConstructionAbort, Protocol::ChannelID::ConstructionAbort>		constructionAbort;
		TCP::ObjectSender<Protocol::ConstructionBegin, Protocol::ChannelID::ConstructionBegin>		constructionBegin;
		TCP::ObjectSender<Protocol::EnableStructure, Protocol::ChannelID::EnableStructure>			enableStructure;
		TCP::ObjectSender<Protocol::Automate,Protocol::ChannelID::Automate>							automate;
		TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::RestoreCoreLink>						restoreCoreLink;
		TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::RemoveEntity>						removeEntity;
		TCP::ObjectSender<Package::NetString<32>, Protocol::ChannelID::ChangeMyName>				changeMyName;
		TCP::ObjectSender<Package::NetString<256>,Protocol::ChannelID::ChatMessage>					chatMessage;

			//	,			//c2s: NET(bool) -> NET(bool)
			//SetFactionId,		//s2c: NET(TUniqueID)

		
	}


	TCP::Client		client;
	SOCKET			broadcastListenSocket = INVALID_SOCKET;

	String			chosenNickname;
	Timer::Time		lastPong = timer.Now();


	void		OnEvent(TCP::event_t ev, TCP::Peer&)
	{
		switch (ev)
		{
			case TCP::Event::ConnectionEstablished:
			{
				Receiver::eventsLocked = false;

				#ifdef HAS_WINDOW
					Tutorial::HideMessage("Servers");
					Tutorial::lastDelayingEvent = timer.Now();
				#endif

				Protocol::ClientState state;
				state.playerName = RuntimeConfig::aiMode ? "<NexusAI>" : chosenNickname;
				state.state = 0U;
				amOnline = true;

				LogEvent(EventType::SessionRelated, nullptr, "Connection established to "+client.ToString());
				Channels::clientStateSender.SendTo(client,state);

				Protocol::ClientReady ready;
				ready.isReady = true;
				if (RuntimeConfig::aiMode)
				{
					ready.factionToken = RuntimeConfig::aiAccessToken;
				}
				if (RuntimeConfig::aiMode)
				{
					LogEvent(EventType::SessionRelated, nullptr, "Attempting to log in to " + worldID.ToString() + " as AI using token: " + ready.factionToken.value.ToString());
				}
				elif (accessMap.Query(worldID, ready.factionToken.value))
				{
					LogEvent(EventType::SessionRelated,nullptr,"Attempting to use existing access token of world "+worldID.ToString()+": "+ready.factionToken.value.ToString());
				}
				else
				{
						
					LogEvent(EventType::SessionRelated, nullptr, "No access token found for world " + worldID.ToString());
					ready.factionToken.value = CryptographicID();
						//Generate();
				}


				Channels::clientReady.SendTo(client,ready);
				lastPong = timer.Now();
			}
			break;
			case TCP::Event::ConnectionFailed:
			{
				LogEvent(EventType::SessionRelated, nullptr, "Connection attempt failed");
			}
			break;
			case TCP::Event::ConnectionLost:
			case TCP::Event::ConnectionClosed:
			{
				LogEvent(EventType::SessionRelated, nullptr, "Connection lost");
				amOnline = false;
				amReady = false;
				//dbgRoundNumber = 0xFFFFFFFF;
				Objects::FlushAllData();
				Network::gameProgress.gameOver = false;
				Network::gameProgress.gameWon = false;
				Network::gameProgress.status = "";
				Network::gameProgress.warning = "";
				Network::newRoundRequested = false;
				Network::factionMap.Clear();
				Objects::Faction::allKnown.Clear();

				Network::myFaction.reset();
				Animation::primitives.Clear();
				Animation::actives.Clear();
				Animation::activeTrajectory.Clear();
				Animation::atTime = 0;
				Grid::FlushAllData();
				Grid::empZonesLocked = 0;
				Grid::empZonesUnlocked = ~Grid::empZonesLocked;

				#ifdef HAS_WINDOW
					Dialog::CloseAllModal();
				#endif

				Receiver::eventsLocked = true;

				#ifdef HAS_WINDOW
					Camera::Reset();
					amLocated = false;
				#endif


				if (RuntimeConfig::aiMode && RuntimeConfig::serverURL.IsNotEmpty())
					application_shutting_down = true;
				#ifdef HAS_WINDOW
					QueryNicknameIfEmpty();
				#endif
			}
			break;
		}
	}

	void		OnSignal(UINT32 signal, TCP::Peer&origin)
	{
		switch (signal)
		{
			case Protocol::ChannelID::Pong:
				lastPong = timer.Now();
			break;
		}
	}

	void		Setup()
	{
		client.InstallChannel(Channels::clientStateUpdateResponse);
		client.InstallChannel(Channels::worldData);
		client.InstallChannel(Channels::newMobileEntity);
		client.InstallChannel(Channels::mobileStateUpdate);
		client.InstallChannel(Channels::newStructure);
		client.InstallChannel(Channels::structureStateUpdate);
		client.InstallChannel(Channels::destroyEntity);
		client.InstallChannel(Channels::controlMessage);
		client.InstallChannel(Channels::clientReady);
		client.InstallChannel(Channels::setFactionID);

		client.InstallChannel(Channels::phaseChange);
		client.InstallChannel(Channels::mobileEntityMotion);
		client.InstallChannel(Channels::attackTrajectory);
		client.InstallChannel(Channels::explosion);
		client.InstallChannel(Channels::entityTurretRotation);
		client.InstallChannel(Channels::resourceUpdate);
		client.InstallChannel(Channels::changeEntityOwner);
		client.InstallChannel(Channels::gameProgress);

		client.InstallChannel(Channels::createMarker);
		client.InstallChannel(Channels::markerCreationFailedLimitReached);
		client.InstallChannel(Channels::destroyMarker);
		client.InstallChannel(Channels::identifyUser);
		client.InstallChannel(Channels::factionOnline);
		client.InstallChannel(Channels::factionOffline);
		client.InstallChannel(Channels::factionRequestedNewRound);

		client.InstallChannel(Channels::lockEmpZones);
		client.InstallChannel(Channels::uplinkBandwidth);
		client.InstallChannel(Channels::unitLimits);

		client.OpenSignalChannel(Protocol::ChannelID::Pong,0);


		client.SetAsync(false);
		client.onEvent = OnEvent;
		client.onSignal = OnSignal;

		Net::initNet();
		if (RuntimeConfig::udpListen)
		{
			broadcastListenSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			ASSERT_NOT_EQUAL__(broadcastListenSocket, INVALID_SOCKET);
			{
				sockaddr_in me;
				memset(&me, 0, sizeof(me));

				me.sin_family = AF_INET;
				me.sin_port = htons(BROADCAST_PORT);
				me.sin_addr.s_addr = ADDR_ANY;

				int rc = ::bind(broadcastListenSocket, (const sockaddr*)&me, (int)sizeof(me));
				if (rc != 0)
				{
					LogEvent(EventType::OperationAborted,nullptr,"Unable to initiate UDP listening for servers on the local area network (maybe another instance of " PRODUCT_NAME " is running already?). Listening will be disabled." );
					RuntimeConfig::udpListen = false;
					closesocket(broadcastListenSocket);
					return;
				}
				u_long dontBlock = 1;
				rc = ioctlsocket(broadcastListenSocket, FIONBIO, &dontBlock);	//set socket to non-blocking
				ASSERT_EQUAL__(rc, 0);
			}
		}


	}


	//class ServerAddress
	//{
	//public:
	//	sockaddr_storage	addr;
	//	int					addrLen;
	//	Timer::Time			lastSignal;
	//	TUniqueID			id;

	//};


	//Buffer0<ServerAddress>	servers;



	Buffer0<ServerInfo,Swap>	servers;
	GenericHashTable<TUniqueID,index_t,TUniqueID>	serverMap;

	Timer::Time		lastPing = timer.Now(),lastServerTableUpdate=timer.Now();

	void		Update()
	{
		if (!client.IsConnected() && RuntimeConfig::serverURL.IsNotEmpty())
			ConnectToConfiguredServerIfAvailable();


		client.Resolve();



		if (broadcastListenSocket != INVALID_SOCKET)
		{
			Protocol::THostBroadcastPacket	packet;
			for (;;)
			{
				sockaddr_storage from;
				memset(&from, 0, sizeof(from));
				int len = sizeof(from);
				int bytes = recvfrom(broadcastListenSocket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&from, &len);
				if (bytes == sizeof(packet))
				{
					packet.version[ARRAYSIZE(packet.version) - 1] = 0;
					//if (!strcmp(packet.version, SERVER_NAME))	//connect anyways
					{
						bool found = false;
						String url = TCP::ToString(from);
						if (url == "127.0.0.1" || url == "::1")
							url = "localhost";
						foreach(ServerDiscovery::attempts, sv)
						{
							if ((*sv)->url == url)
							{
								found = true;
								break;
							}
						}
						if (!found)
						{

							ServerDiscovery::RegisterNewHost(url, !strcmp(packet.version, SERVER_NAME));

							//if (!client.isActive())
							//{
							//	Network::client.ConnectAsync(TCP::ToString(from) + ":"+String(SERVICE_PORT));
							//}
						}
					}
				}
				else
					if (bytes <= 0)
						break;
					else
						FATAL__("weird");
			}
		}

		Timer::Time t = timer.Now();
		if (timer.ToSeconds(t -  lastPing) > 2)
		{
			lastPing = t;
			if (client.IsConnected())
			{
				static const float maxPong =
					#ifdef _DEBUG
						15.f;
					#else
						10.f;
					#endif
				float age = timer.ToSeconds(t - lastPong);
				if (age > maxPong)
				{
					LogEvent(EventType::SessionRelated,nullptr,"Server has not responded for "+String(age)+"/"+String(maxPong)+" second(s). Disconnecting");
					client.Disconnect();
				}
				else
					client.SendSignal(Protocol::ChannelID::Ping);
			}

		}

		if (timer.ToSeconds(t-lastServerTableUpdate) > 0.5f && !client.IsConnected() && !client.IsAttemptingToConnect())
		{
			lastServerTableUpdate = t;

			servers.Clear();
			serverMap.Clear();

			foreach (ServerDiscovery::attempts,_a)
			{
				const auto&a = **_a;
				if (a.hasInfo && a.hasContact)
				{
					index_t at;
					if (!serverMap.query(a.lastInfo.hostInfo.value.hostID,at))
					{
						at = servers.Count();
						servers.Append().info = a.lastInfo;
						servers.last().haveToken = 	accessMap.IsSet(a.lastInfo.worldID);

						servers.last().id = servers.last().info.hostInfo.value.hostID;
						servers.last().rightVersion =  a.rightVersion;
						serverMap.Set(a.lastInfo.hostInfo.value.hostID,at);
						
					}

					ServerInfo&info = servers[at];
					info.urls.Append(std::make_pair(a.url,a.lastPing));
				}
			}
		}
	
	}

	void		Shutdown()
	{
		client.Disconnect();
		closesocket(broadcastListenSocket);
		broadcastListenSocket = INVALID_SOCKET;

	}


	bool		ControlEnabled( )
	{
		return client.IsConnected() && serverPhase == Protocol::Phase::CommandPhase && Animation::AllDone() && !newRoundRequested;
	}


}




void	Network::Receiver::PhaseChange(Protocol::PhaseChange&c)
{
	ASSERT__(!eventsLocked);

	switch (c.newPhase.value)
	{
		case Protocol::Phase::AnimationPhase:
			if (RuntimeConfig::aiMode)
				AI::vehicles.Clear();	//whichever is left, cannot be computed
			Animation::atTime = 0.f;
			foreach (Objects::vehicles, v)
				(*v)->preAnimationLocation = (*v)->location;
			if (c.timeFactor > 0)
				animationTimeFactor = c.timeFactor;
		break;
		case Protocol::Phase::CommandPhase:
			newRoundRequested = false;
			roundTimer = c.timeFactor;
			roundNumber = c.roundNumber;
			serverPhase = c.newPhase;



			#ifdef HAS_WINDOW
				for (const auto&s : Objects::structures)
					if (s->isMine && s->IsWorkingStructure() && (To<Objects::WorkingStructure>::Cast(s))->AwaitsUserInteraction())
						Tutorial::ShowMessage("Interaction");

			#endif

		break;
	}
	#ifdef HAS_WINDOW
		Tutorial::lastDelayingEvent = timer.Now();
	#endif
	serverPhase = c.newPhase;
	roundNumber = c.roundNumber;
	phaseStarted = timer.Now();
	#ifdef HAS_WINDOW
		Objects::UpdateChoices(ControlEnabled());
	#endif

	UI::HexGrid::RefreshContacts();
	//while (Objects::Network::records.IsNotEmpty() && timer.GetSecondsSince(Objects::Network::records.Peek().recorded) > 10)
	//	Objects::Network::records.EraseFront();
	LogEvent(EventType::MinorEntityProcess,nullptr,"Server phase changed to "+String(serverPhase.ToString())+" ("+String(c.timeFactor.value)+") @"+String(Animation::atTime));
	if (serverPhase == Protocol::Phase::CommandPhase)
	{
		Objects::CheckMobileEntitySanity();


		static Buffer0<TUniqueID> outdated;
		Objects::Marker::GetOutdated(outdated,roundNumber,RuntimeConfig::aiMode ? 100 : 10);
		foreach(outdated, o)
		{
			LogEvent(EventType::Debug,nullptr,"Deleting marker "+Objects::Marker::Find(*o)->GetName()+" since it's outdated");
			SignalMarkerDeletion(*o);
			//Network::Channels::destroyMarker.SendTo(Network::client, *o);
		}

		LogEvent(EventType::SessionRelated, nullptr, "Begin round " + String(roundNumber));

		if (Animation::AllDone())
		{
			Objects::RefreshSkyHubConnections();
			Animation::UpdateAutomation(CLOCATION);
		}
	}
	#ifdef HAS_WINDOW
		for (const auto&f : Objects::Faction::allKnown)
		//factionMap.VisitAllValues([](const Objects::PFaction&f)
		{
			f->requestedNewRound = false;
		};//);
		Dialog::SignalSituationChange();
	#endif
}


void	Network::Receiver::LockEmpZones(NET(UINT16)&zones)
{
	Grid::empZonesLocked = zones;
	Grid::empZonesUnlocked = ~zones;

	#ifdef HAS_WINDOW
		for (int i = 0; i < BaseGrid::NumFamilies; i++)
		{
			Grid::high.family[i].SetOutdated(0);
			Grid::low.family[i].SetOutdated(0);
		}

		Grid::visibleOrbitGrid.Invalidate();
	#endif
	//SpaceGrid::Invalidate();
}


void	Network::Receiver::FactionOnline(NET(TUniqueID)&id)
{
	Objects::OwnerParameter faction = Network::ResolveFaction(id);
	faction->online = true;
	#ifdef HAS_WINDOW
		UI::RefreshSessionView();
	#endif
}
void	Network::Receiver::FactionOffline(NET(TUniqueID)&id)
{
	Objects::OwnerParameter faction = Network::ResolveFaction(id);
	faction->online = false;
	#ifdef HAS_WINDOW
		UI::RefreshSessionView();
	#endif
}
void	Network::Receiver::FactionRequestedNewRound(NET(TUniqueID)&id)
{
	Objects::OwnerParameter faction = Network::ResolveFaction(id);
	faction->requestedNewRound = true;
	#ifdef HAS_WINDOW
		LogEvent(EventType::SessionRelated,nullptr,faction->GetName()+" is done thinking");
		UI::RefreshSessionView();
	#endif
}



void	Network::Receiver::IdentifyUser(Protocol::IdentifyUser&id)
{
	Objects::OwnerParameter faction = Network::ResolveFaction(id.id);
	faction->name = id.name;
	#ifdef HAS_WINDOW
		UI::RefreshSessionView();
	#endif
}


void	Network::Receiver::MarkerCreationFailedLimitReached(Protocol::Marker&marker)
{
	bool anyDeleted = false;
	Buffer0<Objects::Marker::TCandidate>	candidates;
	Objects::Marker::GetDeletionCandidates(candidates);
	ASSERT__(candidates.IsNotEmpty());
	foreach(candidates,m)
	{
		Network::Channels::destroyMarker.SendTo(Network::client,m->markerID);
		Objects::Marker::Destroy(m->markerID,Network::ControlEnabled());
	}
	Network::Channels::createMarker.SendTo(Network::client,marker);
	//SignalMarkerCreation(marker);	//can't - would create marker once more
}


bool		Network::ConnectToConfiguredServerIfAvailable()
{
	if (Network::client.IsConnected() || Network::client.IsAttemptingToConnect())
		return false;
	static Timer::Time lastAttempt = timer.Now()-10;
	static count_t delayCounter = 10;
	if (++delayCounter < 3 || timer.GetSecondsSince(lastAttempt) < 5)
		return false;	//make sure there is enough time to rebuild the server list in-between connection attempts to a ghost-server that is configured via runtime parameter
	foreach(Network::servers, srv)
	{
		if (srv->rightVersion && srv->HasURL(RuntimeConfig::serverURL))
		{
			index_t best = 0;
			float bestPing = srv->urls.first().second;
			for (index_t i = 1; i < srv->urls.Count(); i++)
				if (srv->urls[i].second < bestPing)
				{
					bestPing = srv->urls[i].second;
					best = i;
				}
			srv->selected = true;

			Network::serverID = srv->id;
			Network::serverIDString = Network::serverID.ToNiceString();
			Network::worldID = srv->info.worldID;
			Network::worldIDString = Network::worldID.ToNiceString();
			Network::client.ConnectAsync(srv->urls[best].first + ":" + String(SERVICE_PORT));

			lastAttempt = timer.Now();
			delayCounter = 0;
			return true;
		}

	}
	return false;
}



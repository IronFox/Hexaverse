#include <global_root.h>
#include "network.h"
#include "../version.h"
#include "animation.h"
#include "clientRuntimeConfig.h"
#include "world.h"
#include "cameraControl.h"
#include "ui.h"
#include "tutorial.h"
#include "dialog.h"
#include "ai.h"
#include "clientRuntimeConfig.h"
#include "timelapse.h"


void			SignalMarkerDeletion(TUniqueID markerID);	//must be copy to prevent weird shit
void			OnPlayerControlBegin();

#ifdef HAS_WINDOW
	void		QueryNicknameIfEmpty();



#endif



namespace Network
{
	bool			haveFaction = false,
					haveID = false,
					amSpectator = false,
					amOnline=false,
					gameHasBegun=false;
	#ifdef HAS_WINDOW
		bool		amLocated=false;
	#endif

	TUniqueID		myID,serverID,worldID;
	UINT32				gameFlags = 0;

	CryptographicID	activeToken;
	String			myIDString,serverIDString,worldIDString,activeTokenString;
	INT32			uplinkBandwidth = -1;
	
	GenericHashTable<TUniqueID,CryptographicID,TUniqueID>	accessMap;


	Protocol::GameProgress	gameProgress;

	float			animationTimeFactor = 1,
					roundTimer = 0;
	Timer::Time		phaseStarted;
	bool			newRoundRequested = false;


	void			ConnectTo(const ServerAddress&addr)
	{
		RuntimeConfig::phase = RuntimeConfig::Phase::Uninitialized;
		gameHasBegun = false;

		client.ConnectAsync(addr.host + ":"+String(addr.basePort + PortOffset::Session));
	}

	Objects::OwnerParameter ResolveFaction(const TUniqueID&id)
	{
		return Objects::Faction::GetOrCreate(id);
	}


	static PathString AccessFilename()
	{
		return RuntimeConfig::ConfigFile("worldAccess.m32");
	}

	void SaveWorldAccessTokens()
	{
		FileSystem::CreateDirectory("config");
		FILE*f = FOPEN(AccessFilename().c_str(), "wb+");
		if (!f)
		{
			LogEvent(EventClass::Preset::error,nullptr,"Unable to write to world access map");
			return;
		}

		accessMap.visitAllEntries([f](const TUniqueID&id, const CryptographicID&access)
		{
			fwrite(id.bytes,sizeof(id.bytes),1,f);
			access.Write(f);
		});

		fclose(f);
		LogEvent(EventClass::Preset::common,nullptr,"Updated world access map");
	}

	void LoadWorldAccessTokens()
	{
		FILE*f = FOPEN(AccessFilename().c_str(), "rb");
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

		LogEvent(EventClass::Preset::common,nullptr,"World access map imported. "+String(accessMap.count())+" entry/ies loaded.");
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
			ServerAddress	address;
			bool			hasContact = false,
							hasInfo = false,
							rightVersion = false,
							autoAttempt = false,
							protocolIncompatible = false;
			Protocol::ServerInfo	lastInfo;
			float			lastPing;
			volatile bool	doQuit = false;
			//Semaphore		sem;

			/**/			HostContactAttempt():/*sem(0),*/lastPing(0)	{}


			static void		OnEvent(TCP::event_t ev, TCP::Peer&peer);

			static void		OnDeserializationFailed(UINT32, TCP::Peer&peer);

			void			ThreadMain() override
			{
				client.SetAsync(true);
				client.onEvent = OnEvent;
				client.attachment = shared_from_this();
				client.InstallChannel(serverInfoChannel);
				client.InstallChannel(pingChannel);
				client.onDeserializationFailed = OnDeserializationFailed;

				while (!application_shutting_down && !doQuit)
				{
					hasContact = false;
					lastPing = 0;
					hasInfo = false;
					while (!application_shutting_down && !doQuit)
					{
						if (client.Connect(address.host + ":"+String(address.basePort + PortOffset::Info)))
						{
							break;
						}
						Sleep(1000);
					}
					if (doQuit)
						return;
					hasContact = true;
					while (!doQuit && !application_shutting_down && client.IsConnected())
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

		Buffer0<PHostContactAttempt>	attempts,waste;

		void	ListFailedAttempts(Buffer0<ServerAddress,Adopt>&outList)
		{
			outList.Clear();
			foreach (attempts,a)
			{
				if (!(*a)->autoAttempt && !(*a)->hasContact)
					outList << (*a)->address;
			}

		}

		void	ListNonAutoAttempts(Buffer0<ServerAddress>&outList)
		{
			outList.Clear();
			foreach (attempts,a)
			{
				if (!(*a)->autoAttempt)
					outList << (*a)->address;
			}
		}


		bool	TryRemoveHost(const ServerAddress&addr)
		{
			if (client.IsAttemptingToConnect() || client.IsConnected())
				return false;
			foreach (attempts,a)
			{
				if ((*a)->address == addr)
				{
					(*a)->doQuit = true;
					waste << *a;
					attempts.erase(a);
					#ifdef HAS_WINDOW
						Dialog::SignalSituationChange();
					#endif
					return true;
				}
			}
			return false;
		}


		bool	ListeningButCantConnectTo(const ServerAddress&addr)
		{
			if (client.IsAttemptingToConnect() || client.IsConnected())
				return false;
			foreach (attempts,a)
			{
				if ((*a)->address == addr)
				{
					return !(*a)->hasContact || !(*a)->rightVersion || (*a)->protocolIncompatible;
				}

			}
			return false;

		}

		void OnServerInfo(Protocol::ServerInfo&info, TCP::Peer&peer)
		{
			PHostContactAttempt me = To<HostContactAttempt>::Cast(peer.attachment);
			me->lastInfo = info;
			info.hostInfo.value.version[ARRAYSIZE(info.hostInfo.value.version)-1] = 0;
			me->hasInfo = true;
			me->rightVersion = !strcmp(info.hostInfo.value.version, SERVER_NAME);
		}


		/*static*/ void		HostContactAttempt::OnDeserializationFailed(UINT32, TCP::Peer&peer)
		{
			PHostContactAttempt me = To<HostContactAttempt>::Cast(peer.attachment);
			if (me)
				me->protocolIncompatible = true;
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


		bool	RegisterNewHost(const ServerAddress&host, bool probablyRightVersion, bool automaticallyDiscovered)
		{
			foreach (attempts,a)
				if ((*a)->address == host)
					return false;	//already there
			PHostContactAttempt&attempt = attempts.Append();
			attempt.reset(new HostContactAttempt());
			attempt->address = host;
			attempt->rightVersion = probablyRightVersion;
			attempt->autoAttempt = automaticallyDiscovered;
			
			attempt->start();
			return true;
		}


	}



	static void ApplyPlanetConfig(const Grid::TSharedPlanetConfig&config)
	{
		Grid::Content coreContent = Grid::Content::Reinterpret(config.innerCoreType);
		Grid::CellStack::bottom.content = coreContent;
		#ifdef HAS_WINDOW
			Grid::CellStack::bottom.visible = 1;
			Grid::UpdateAtmosphere(config.atmosphereColor);
			Grid::UpdateSun(config.sunPlaneNormal_Speed,config.sunColor);
		#endif
	}


	static void		FlushFaction(bool alsoFlushWorld)
	{
		#ifdef HAS_WINDOW
			TimeLapse::enabled = false;
		#endif

		haveFaction = false;
		//dbgRoundNumber = 0xFFFFFFFF;

		roundTimer = -1;

		Objects::FlushAllData();
		Network::gameProgress.gameOver = false;
		Network::gameProgress.gameWon = false;
		Network::gameProgress.status = "";
		Network::gameProgress.warning = "";
		Network::newRoundRequested = false;
		Objects::Faction::idMap.Clear();
		Objects::Faction::allKnown.Clear();
		Objects::Faction::mine.reset();

		Animation::primitives.Clear();
		Animation::actives.Clear();
		Animation::activeTrajectory.Clear();
		Animation::atTime = 0;

		if (alsoFlushWorld)
		{
			Grid::FlushAllData();
			Grid::empZonesLocked = 0;
			Grid::empZonesUnlocked = ~Grid::empZonesLocked;
			RuntimeConfig::gameGoal = RuntimeConfig::GameGoal();
			ApplyPlanetConfig(Grid::TSharedPlanetConfig());
		}

		#ifdef HAS_WINDOW
			Objects::Renderer::ClearSmokeTrails();
			UI::EventLog::Clear();

			amLocated = false;
			if (alsoFlushWorld)
				Camera::Reset();
		#endif
	}


	namespace Receiver
	{
		bool			eventsLocked = true;


		void	OnHelloResponse(Protocol::Result&response)
		{
			ASSERT__(response.success);
			if (response.success)
				LogEvent(EventClass::Preset::common, nullptr, "Status confirmed");
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
				LogEvent(EventSignificance::Debug, "Appended world delta primitive to animation sequence at t="+String(delta.timestamp.atTime.value));
			}
			else
			{
				BYTE family = delta.familyAndLastFlag & 0x7F;
				bool last = (delta.familyAndLastFlag & 0x80) != 0;
				Grid::actualWorld.ApplyFamilyDelta(family,delta.data0,false,true);

				#ifdef HAS_WINDOW
					World::allowRecompilation = last && gameHasBegun;
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
				{
					foreach (Animation::primitives,p)
					{
						if (p->type == Animation::Primitive::MobileState
							&&
							p->targetEntityID == vs.entityState.id
							&&
							p->time.min == vs.entityState.timestamp.atTime)
						{
							p->Init(vs);
							p->overwritten++;
							if (p->overwritten==1 || !(p->overwritten %100000))
								LogEvent(EventSignificance::Debug, "Rewrote 'mobile state("+vs.entityState.id.value.ToString()+")' primitive to animation sequence at t="+String(vs.entityState.timestamp.atTime.value)+" n="+String(p->overwritten));
							return;
						}
					}
					Animation::primitives.Append().Init(vs);
					LogEvent(EventSignificance::Debug, "Appended 'mobile state("+vs.entityState.id.value.ToString()+")' primitive to animation sequence at t="+String(vs.entityState.timestamp.atTime.value));
				}
			}
		}

		void	OnNewMobileEntity(Protocol::NewMobileEntity&vc)
		{
			ASSERT__(!eventsLocked);
			ASSERT__(Network::haveID || Network::amSpectator);

			Objects::PMobileEntity v = Objects::SpawnMobileEntity(Network::ResolveFaction(vc.owner),  vc.state.entityState.id,vc.state.entityState.location,vc.baseName,vc.turretName,vc.state.entityState.timestamp.rightNow,Network::haveID && Network::myID == vc.owner);
			if (vc.state.entityState.timestamp.rightNow)
				LogEvent(EventSignificance::Common,v.get(),  "Placed vehicle entity outside animation");
			bool isOwnedByMe = v->isOwnedByMe;

			//v->id = vc.state.entityState.id;
			Animation::ApplyState(v,vc.state,true, !vc.state.entityState.timestamp.rightNow);
			#ifdef HAS_WINDOW
				if (v->isOwnedByMe && !amLocated)
				{
					amLocated = true;
					Camera::TranslateTo(v->system.w.xyz);
				}
			#endif
			if (vc.state.entityState.timestamp.rightNow)
			{
				if (!isOwnedByMe)
					UI::HexGrid::RefreshContacts();
			}
			else
			{
				Animation::primitives.Append().Init(vc,v);
				LogEvent(EventSignificance::Debug, "Appended 'new mobile entity("+vc.state.entityState.id.value.ToString()+")' primitive to animation sequence at t="+String(vc.state.entityState.timestamp.atTime.value));

			}
		}

		

		void	OnNewStructure(Protocol::StructureConstruction&sc)
		{
			ASSERT__(!eventsLocked);

			#ifdef HAS_WINDOW
				if (Grid::actualWorld.lightingOutdated && World::allowRecompilation)
					Grid::actualWorld.UpdateLighting();
			#endif

			Objects::PStructure s = Objects::SpawnStructure(Network::ResolveFaction(sc.owner),  sc.state.entityState.id,sc.state.entityState.location,sc.typeName, sc.state.entityState.timestamp.rightNow, Network::haveID && Network::myID == sc.owner);
			ASSERT__(s);
			if (sc.state.entityState.timestamp.rightNow)
				LogEvent(EventSignificance::Common,s.get(),  "Placed structure entity outside animation");

			Animation::UpdateState(s,sc.state);
			if (!sc.state.entityState.timestamp.rightNow)
			{
				Animation::primitives.Append().Init(sc,s);
				LogEvent(EventSignificance::Debug, "Appended 'new structure("+sc.state.entityState.id.value.ToString()+")' primitive to animation sequence at t="+String(sc.state.entityState.timestamp.atTime.value));
			}
			else
			{
				if (!s->isOwnedByMe)
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
				{
					Animation::primitives.Append().Init(vs);
					LogEvent(EventSignificance::Debug, "Appended 'structure state("+vs.entityState.id.value.ToString()+")' primitive to animation sequence at t="+String(vs.entityState.timestamp.atTime.value));
				}
			}

		}

		void DestroyEntity(Protocol::DestroyEntity&de)
		{
			ASSERT__(!eventsLocked);

			if (de.timestamp.rightNow && Animation::AllDone())
			{
				const Objects::PWorldEntity&e = Objects::GetEntity(de.id);
				ASSERT__(e);
				LogEvent(EventSignificance::Common,e.get(),  "Removed entity outside animation");

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

				if (!e->isOwnedByMe)
					UI::HexGrid::RefreshContacts();
			}
			else
			{
				LogEvent(EventSignificance::Debug,nullptr,"Reception of entity destruction message for "+de.id.value.ToString()+" ("+Objects::RemovalType::Reinterpret(de.type.value).ToString()+")");
				if (de.timestamp.rightNow)
					Animation::whenDone.Append().Init(de);
				else
				{
					Animation::primitives.Append().Init(de);
					LogEvent(EventSignificance::Debug, "Appended 'destruction' primitive to animation sequence at t="+String(de.timestamp.atTime.value));
				}
			}
		}

		void ControlMessage(Protocol::ControlMessage&msg)
		{
			LogEvent(EventClass( EventSignificance::Reinterpret(msg.significance),  EventFlavor::Reinterpret(msg.flavor)) ,nullptr,">"+msg.message);
		}




		void	ClientFactionResponse(Protocol::ClientFactionResponse&cr)
		{
			ASSERT__(!eventsLocked);

			ApplyPlanetConfig(cr.planetConfig);

			myID = TUniqueID::Empty();
			Objects::Faction::mine.reset();
			myIDString = myID.ToString();

			Network::gameFlags = cr.info.gameFlags;
			RuntimeConfig::empZones = (Network::gameFlags & Protocol::GameFlag::EMPZones) != 0;
			RuntimeConfig::gameGoal = RuntimeConfig::GameGoal::Reinterpret(cr.info.gameGoal);
			RuntimeConfig::phase = RuntimeConfig::Phase::Reinterpret(cr.info.activePhase);

			#ifdef HAS_WINDOW
				World::allowRecompilation = false;
			#endif
			gameHasBegun = false;


			haveFaction = cr.startFactionTypes != 0;
			if (haveFaction)
			{
				amSpectator = cr.startFactionTypes == Protocol::FactionType::SpectatorFaction;
				activeToken = cr.factionToken;
				activeTokenString = activeToken.ToString();
				if (!RuntimeConfig::aiMode && !amSpectator)
				{
					LogEvent(EventClass::Preset::common,nullptr,"Updating access token of world "+worldID.ToString()+" to "+activeToken.ToString());
					accessMap.Set(worldID,activeToken);
					SaveWorldAccessTokens();
				}
				Network::Channels::changeMyName.SendTo(Network::client,Network::chosenNickname);

				#ifdef HAS_WINDOW
					Dialog::Hide(Dialog::loginMenu);
				#endif
			}
			else
			{
				amSpectator = false;
				LogEvent(EventClass::Preset::uncommon, nullptr, "Server response: Faction void.");
				FlushFaction(true);
				#ifdef HAS_WINDOW
					Dialog::Show(Dialog::loginMenu);
				#endif
			}

			if (amSpectator)
			{
				foreach (Grid::actualWorld.cellStack,stack)
					for (UINT32 l = 0; l < Grid::CellStack::Range; l++)
						stack->volumeCell[l].visible = 1;	//entire world is visible

			}
		}

		void	SetFactionID(NET(TUniqueID)&id)
		{
			ASSERT__(!eventsLocked);

			haveID = true;
			amSpectator = false;
			myID = id;
			Objects::Faction::mine = ResolveFaction(myID);
			myIDString = myID.ToString();
			Objects::Faction::mine->isOwnedByMe = true;
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
					LogEvent(EventSignificance::Debug, "Appended 'mobile entity motion' primitive to animation sequence at t="+String(m.timestamp.atTime.value));
				}

			}
		}

		void	VagueAttackTrajectory(Protocol::VagueAttackTrajectory&a)
		{
			ASSERT__(!eventsLocked);

			if (a.timestamp.rightNow)
			{
				FATAL__("weird");
			}
			else
			{
				Animation::primitives.Append().Init(a);
				LogEvent(EventSignificance::Debug, "Appended 'vague trajectory' primitive to animation sequence at t="+String(a.timestamp.atTime.value));
			}
			
		}

		void	PreciseAttackTrajectory(Protocol::PreciseAttackTrajectory&a)
		{
			ASSERT__(!eventsLocked);

			if (a.timestamp.rightNow)
			{
				FATAL__("weird");
			}
			else
			{
				Animation::primitives.Append().Init(a);
				LogEvent(EventSignificance::Debug, "Appended 'precise trajectory' primitive to animation sequence at t="+String(a.timestamp.atTime.value));
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
				LogEvent(EventSignificance::Debug, "Appended 'explosion' primitive to animation sequence at t="+String(ex.timestamp.atTime.value));
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
			{
				Animation::primitives.Append().Init(rot);
				LogEvent(EventSignificance::Debug, "Appended 'entity turret rotation' primitive to animation sequence at t="+String(rot.timestamp.atTime.value));
			}
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
			{
				Animation::primitives.Append().Init(u);
				LogEvent(EventSignificance::Debug, "Appended 'resource update' primitive to animation sequence at t="+String(u.timestamp.atTime.value));
			}

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
			TUniqueID	entityID = TUniqueID::Empty(), ownerID = TUniqueID::Empty();
			ConstName	entityType[2];
			Objects::Marker::Type type;

			const Protocol::MarkerReferenceEntity*entity = marker.data.Get0();
			if (entity)
			{
				type = Objects::Marker::Type::Reinterpret(0);
				entityID = entity->id;
				ownerID = entity->owner;
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
					ownerID = entity->owner;
					entityType[0] = entity->type0;
					entityType[1] = entity->type1;
				}
				else
				{
					ASSERT_NOT_NULL__(marker.data.Get2());
					type = Objects::Marker::Type::Reinterpret(2);
				}
			}
			Objects::Marker::Create(marker.id, type, marker.location, marker.createdInRound, entityID, ownerID, entityType, std::max<int>(RuntimeConfig::roundNumber,marker.createdInRound));
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
		void	FactionContinuedNewRound(NET(TUniqueID)&);
		void	LockEmpZones(NET(UINT16)&);
		void	UplinkBandwidth(NET(INT32)&v)
		{
			uplinkBandwidth = v;
			RuntimeConfig::hibernationSystemEnabled = v > 0;
			#ifdef HAS_WINDOW
				OnSelectionChange(true);
			#endif
		}

		void	UnitLimits(Protocol::UnitLimits&limits)
		{
			RuntimeConfig::UnitLimits::Import(limits);
			#ifdef HAS_WINDOW
				OnSelectionChange(true);
			#endif

		}

		void	ResearchState(Objects::ResearchState&state)
		{
			ResolveFaction(state.ownerID)->researchState = state;
		}

		void	SelectResearch(Protocol::SelectResearch&research)
		{
			if (Objects::Faction::mine)
			{
				Objects::Faction::mine->researchState.currentResearch = research.target;
				Objects::Faction::mine->researchState.currentResearchAttribute.Load(research.attribute);
				//Dialog::SignalSituationChange();
				UI::RefreshButtonState();
			}

		}
	}


	namespace Channels
	{
		TCP::Channel<Protocol::ControlMessage,0,Protocol::ChannelID::ControlMessage>				controlMessage(Receiver::ControlMessage);

		TCP::Channel<Protocol::ClientFactionResponse,0,Protocol::ChannelID::ClientFaction>			clientFactionResponse(Receiver::ClientFactionResponse);

		TCP::Channel<Protocol::Result,0,Protocol::ChannelID::Hello>									helloResponse(Receiver::OnHelloResponse);
		TCP::Channel<Protocol::WorldDelta,0,Protocol::ChannelID::WorldDelta>						worldData(Receiver::OnWorldDelta);
		TCP::Channel<Protocol::NewMobileEntity,0,Protocol::ChannelID::NewMobileEntity>				newMobileEntity(Receiver::OnNewMobileEntity);
		TCP::Channel<Protocol::MobileState,0,Protocol::ChannelID::MobileState>						mobileStateUpdate(Receiver::OnMobileState);
		TCP::Channel<Protocol::StructureConstruction,0,Protocol::ChannelID::InstantiateStructure>	newStructure(Receiver::OnNewStructure);
		TCP::Channel<Protocol::StructureState,0,Protocol::ChannelID::UpdateStructure>				structureStateUpdate(Receiver::OnStructureState);
		TCP::Channel<Protocol::DestroyEntity,0,Protocol::ChannelID::DestroyEntity>					destroyEntity(Receiver::DestroyEntity);
		TCP::Channel<NET(TUniqueID),0,Protocol::ChannelID::SetFactionID>							setFactionID(Receiver::SetFactionID);

		TCP::Channel<Protocol::PhaseChange,0,Protocol::ChannelID::PhaseChange>						phaseChange(Receiver::PhaseChange);
		TCP::Channel<Protocol::MobileEntityMotion,0,Protocol::ChannelID::MobileEntityMotion>		mobileEntityMotion(Receiver::MobileEntityMotion);
		TCP::Channel<Protocol::VagueAttackTrajectory,0,Protocol::ChannelID::VagueAttackTrajectory>			vagueAttackTrajectory(Receiver::VagueAttackTrajectory);
		TCP::Channel<Protocol::PreciseAttackTrajectory,0,Protocol::ChannelID::PreciseAttackTrajectory>		preciseAttackTrajectory(Receiver::PreciseAttackTrajectory);
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
		TCP::Channel<NET(TUniqueID), 0, Protocol::ChannelID::FactionContinuedRound>					factionContinuedRound(Receiver::FactionContinuedRound);
		TCP::Channel<NET(UINT16), 0, Protocol::ChannelID::LockEMPZones>								lockEmpZones(Receiver::LockEmpZones);
		TCP::Channel<NET(INT32), 0, Protocol::ChannelID::UplinkBandwidth>							uplinkBandwidth(Receiver::UplinkBandwidth);
		TCP::Channel<Protocol::UnitLimits, 0, Protocol::ChannelID::UnitLimits>						unitLimits(Receiver::UnitLimits);
		TCP::Channel<Objects::ResearchState, 0, Protocol::ChannelID::ResearchState>					researchState(Receiver::ResearchState);
		TCP::Channel<Protocol::SelectResearch, 0, Protocol::ChannelID::SelectResearch>				selectResearch(Receiver::SelectResearch);



		TCP::ObjectSender<Protocol::ClientFactionRequest,Protocol::ChannelID::ClientFaction>		clientFactionRequest;
		TCP::ObjectSender<Protocol::TouchedMarker,Protocol::ChannelID::TouchMarker>					touchMarker;
		TCP::ObjectSender<Protocol::Hello,Protocol::ChannelID::Hello>								hello;
		TCP::ObjectSender<Protocol::EntityDirectCommand, Protocol::ChannelID::EntityDirectCommand>	entityDirectCommand;
		TCP::ObjectSender<Protocol::ChoiceIntersection, Protocol::ChannelID::ChoiceIntersection>	choiceIntersection;
		TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::DropLastTask>						dropLastTask;
		TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::DropAllTasks>						dropAllTasks;
		TCP::ObjectSender<Protocol::StandardJob, Protocol::ChannelID::SignalNewJob>					signalNewJob;
		TCP::ObjectSender<Protocol::TerrainMod, Protocol::ChannelID::SignalTerrainMod>				signalTerrainMod;
		TCP::ObjectSender<Protocol::ConstructionAbort, Protocol::ChannelID::ConstructionAbort>		constructionAbort;
		TCP::ObjectSender<Protocol::ConstructionBegin, Protocol::ChannelID::ConstructionBegin>		constructionBegin;
		TCP::ObjectSender<Protocol::EnableStructure, Protocol::ChannelID::EnableStructure>			enableStructure;
		TCP::ObjectSender<Protocol::EntityStateUpdate,Protocol::ChannelID::Automate>				automate;
		TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::RestoreCoreLink>						restoreCoreLink;
		TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::RemoveEntity>						removeEntity;
		TCP::ObjectSender<Protocol::EntityStateUpdate, Protocol::ChannelID::Sleep>					sleep;
		TCP::ObjectSender<Protocol::ClientName, Protocol::ChannelID::ChangeMyName>					changeMyName;
		TCP::ObjectSender<Package::NetString<256>,Protocol::ChannelID::ChatMessage>					chatMessage;

			//	,			//c2s: NET(bool) -> NET(bool)
			//SetFactionId,		//s2c: NET(TUniqueID)

		
	}


	TCP::Client		client;
	SOCKET			broadcastListenSocket = INVALID_SOCKET;

	String			chosenNickname;
	//Timer::Time		lastPong = timer.Now();
	


	void			AbandonCurrentFaction()
	{
		if (haveFaction && client.IsConnected())
		{
			Network::Channels::changeMyName.SendTo(Network::client,Network::chosenNickname+" (abandoned)");

			Protocol::ClientFactionRequest ready;
			ready.startFactionTypes = 0;
			ready.factionToken.value = CryptographicID();
			LogEvent(EventClass::Preset::uncommon, nullptr, "Abandoning current faction");
			Channels::clientFactionRequest.SendTo(client,ready);

			#ifdef HAS_WINDOW
				Dialog::Show(Dialog::loginMenu);
			#endif
		}
	}

	bool		HaveFactionKeyForCurrentWorld()
	{
		return client.IsConnected() && accessMap.IsSet(worldID);
	}

	void		AttemptFactionLogin(bool forceNewFaction, UINT32 factionTypes)
	{
		Protocol::ClientFactionRequest ready;
		ready.startFactionTypes = factionTypes;
		if (RuntimeConfig::aiMode)
		{
			ready.factionToken = RuntimeConfig::aiAccessToken;
			LogEvent(EventClass::Preset::common, nullptr, "Attempting to log in to " + worldID.ToString() + " as AI using token: " + ready.factionToken.value.ToString());
		}
		elif (!forceNewFaction && accessMap.Query(worldID, ready.factionToken.value))
		{
			LogEvent(EventClass::Preset::common,nullptr,"Attempting to use existing access token of world "+worldID.ToString()+": "+ready.factionToken.value.ToString());
		}
		else
		{
						
			if (forceNewFaction)
				LogEvent(EventClass::Preset::common, nullptr, "Forcing new faction in world " + worldID.ToString());
			else
				LogEvent(EventClass::Preset::common, nullptr, "No access token found for world " + worldID.ToString());

			ready.factionToken.value = CryptographicID();
		}


		Channels::clientFactionRequest.SendTo(client,ready);
	}



	void		OnEvent(TCP::event_t ev, TCP::Peer&)
	{
		switch (ev)
		{
			case TCP::Event::ConnectionEstablished:
			{
				Receiver::eventsLocked = false;


				#ifdef HAS_WINDOW
					World::allowRecompilation = false;	//until game begins
					Tutorial::HideMessage("Servers");
					Tutorial::lastDelayingEvent = timer.Now();
				#endif

				amOnline = true;

				LogEvent(EventClass::Preset::uncommon, nullptr, "Connection established to "+client.ToString());
				
				Protocol::Hello hello;
				hello = RuntimeConfig::aiMode ? "<NexusAI>" : chosenNickname;
				if (RuntimeConfig::aiMode)
					chosenNickname = hello;
				//state.state = 0U;
				Channels::hello.SendTo(client,hello);	//elevate client state

				if (RuntimeConfig::aiMode)
					AttemptFactionLogin(false,Protocol::FactionType::ContinueOnly);
				//lastPong = timer.Now();


				#ifdef HAS_WINDOW
					Dialog::Show(Dialog::loginMenu);
					Dialog::CheckServersAreShown(false);
				#endif

			}
			break;
			case TCP::Event::ConnectionFailed:
			{
				LogEvent(EventClass::Preset::networkError, nullptr, "Connection attempt failed");
				#ifdef HAS_WINDOW
					World::allowRecompilation = true;
					Dialog::SignalSituationChange();
				#endif
			}
			break;
			case TCP::Event::ConnectionLost:
			case TCP::Event::ConnectionClosed:
			{
				LogEvent(EventClass( EventSignificance::Uncommon, EventFlavor::Loss ), nullptr, "Connection lost");
				amOnline = false;

				FlushFaction(true);
				#ifdef HAS_WINDOW
					World::allowRecompilation = true;
					Dialog::CloseAllModal();
				#endif



				if (RuntimeConfig::aiMode && RuntimeConfig::serverAddress.host.IsNotEmpty())
					application_shutting_down = true;
				#ifdef HAS_WINDOW
					QueryNicknameIfEmpty();
				#endif

				Receiver::eventsLocked = true;

				#ifdef HAS_WINDOW
					Dialog::Hide(Dialog::loginMenu);
					Dialog::CheckServersAreShown(true);
					Dialog::SignalSituationChange();
				#endif

			}
			break;
		}
	}

	void		OnSignal(UINT32 signal, TCP::Peer&origin)
	{
		switch (signal)
		{
			case Protocol::ChannelID::BeginGame:
				#ifdef HAS_WINDOW
					World::allowRecompilation = true;
				#endif
				gameHasBegun = true;
				LogEvent(EventClass::Preset::uncommon,nullptr,"Game has begun");
				if (RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase && Animation::AllDone())
				{
					Objects::RefreshSkyHubConnections();
					Animation::UpdateAutomation(CLOCATION);
				}
			break;
			case Protocol::ChannelID::Pong:
				//lastPong = timer.Now();
				#ifdef HAS_WINDOW
					Dialog::SignalSituationChange();
				#endif
				#ifdef _DEBUG
					//LogEvent(EventSignificance::Debug,nullptr,"Got pong from server");
				#endif
			break;
		}
	}

	void		Setup()
	{
		client.InstallChannel(Channels::helloResponse);
		client.InstallChannel(Channels::worldData);
		client.InstallChannel(Channels::newMobileEntity);
		client.InstallChannel(Channels::mobileStateUpdate);
		client.InstallChannel(Channels::newStructure);
		client.InstallChannel(Channels::structureStateUpdate);
		client.InstallChannel(Channels::destroyEntity);
		client.InstallChannel(Channels::controlMessage);
		client.InstallChannel(Channels::clientFactionResponse);
		client.InstallChannel(Channels::setFactionID);

		client.InstallChannel(Channels::phaseChange);
		client.InstallChannel(Channels::mobileEntityMotion);
		client.InstallChannel(Channels::vagueAttackTrajectory);
		client.InstallChannel(Channels::preciseAttackTrajectory);
		client.InstallChannel(Channels::explosion);
		client.InstallChannel(Channels::entityTurretRotation);
		client.InstallChannel(Channels::resourceUpdate);
		client.InstallChannel(Channels::changeEntityOwner);
		client.InstallChannel(Channels::gameProgress);

		client.InstallChannel(Channels::createMarker);
		client.InstallChannel(Channels::markerCreationFailedLimitReached);
		client.InstallChannel(Channels::destroyMarker);
		client.InstallChannel(Channels::identifyUser);
		client.InstallChannel(Channels::researchState);
		client.InstallChannel(Channels::selectResearch);
		client.InstallChannel(Channels::factionOnline);
		client.InstallChannel(Channels::factionOffline);
		client.InstallChannel(Channels::factionRequestedNewRound);
		client.InstallChannel(Channels::factionContinuedRound);

		client.InstallChannel(Channels::lockEmpZones);
		client.InstallChannel(Channels::uplinkBandwidth);
		client.InstallChannel(Channels::unitLimits);

		client.OpenSignalChannel(Protocol::ChannelID::Pong,0);
		client.OpenSignalChannel(Protocol::ChannelID::BeginGame,0);
		client.OpenSignalChannel(Protocol::ChannelID::BeginEverything,0);
		client.OpenSignalChannel(Protocol::ChannelID::EndEverything,0);


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
					LogEvent(EventClass::Preset::operationAborted,nullptr,"Unable to initiate UDP listening for servers on the local area network (maybe another instance of " PRODUCT_NAME " is running already?). Listening will be disabled." );
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


	/*explicit*/			ServerAddress::ServerAddress(const String&url)
	{
		index_t portAt = url.find(':');
		if (portAt)
		{
			host = url.subStringRef(0,portAt-1);
			StringRef conv = url.subStringRef((int)portAt);
			if (!convert(conv.pointer(),conv.length(),basePort))
			{
				throw Program::ParameterFault("Unable to parse port component '" + String(conv) + "' of '"+url+"'");
			}
		}
		else
		{
			host = url;
			basePort = DEFAULT_SERVER_BASE_PORT;
		}
	}


	Buffer0<ServerInfo,Swap>	servers;
	GenericHashTable<TUniqueID,index_t,TUniqueID>	serverMap;

	Timer::Time		lastPing = timer.Now(),lastServerTableUpdate=timer.Now();

	void		Update()
	{
		if (!client.IsConnected() && RuntimeConfig::serverAddress.host.IsNotEmpty())
			ConnectToConfiguredServerIfAvailable();


		client.Resolve();



		if (broadcastListenSocket != INVALID_SOCKET)
		{
			Protocol::THostBroadcastPacket	packet;
			for (;;)
			{
				sockaddr_storage from;
				memset(&from, 0, sizeof(from));
				socklen_t len = (socklen_t)sizeof(from);
				int bytes = recvfrom(broadcastListenSocket, (char*)&packet, sizeof(packet), 0, (sockaddr*)&from, &len);
				if (bytes == sizeof(packet))
				{
					packet.version[ARRAYSIZE(packet.version) - 1] = 0;
					//if (!strcmp(packet.version, SERVER_NAME))	//connect anyways
					{
						bool found = false;
						ServerAddress addr;
						addr.host = TCP::ToString(from,len,false);
						if (addr.host == "127.0.0.1" || addr.host == "::1")
							addr.host = "localhost";
						addr.basePort = packet.serverBasePort;
						foreach(ServerDiscovery::attempts, sv)
						{
							if ((*sv)->address == addr)
							{
								found = true;
								break;
							}
						}
						if (!found)
						{
							ServerDiscovery::RegisterNewHost(addr, !strcmp(packet.version, SERVER_NAME),true);
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
				float age = timer.ToSeconds(t - client.GetLastPackageTime());
				if (age > maxPong)
				{
					LogEvent(EventClass::Preset::uncommon,nullptr,"Server has not responded for "+String(age)+"/"+String(maxPong)+" second(s). Disconnecting");
					client.Disconnect();
				}
				else
				{
					client.SendSignal(Protocol::ChannelID::Ping);
					//#ifdef _DEBUG
					//	LogEvent(EventSignificance::Debug,nullptr,"ping");
					//#endif
				}
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
				if (a.hasContact && (a.hasInfo || a.protocolIncompatible))
				{
					index_t at;
					if (!serverMap.query(a.lastInfo.hostInfo.value.hostID,at))
					{
						at = servers.Count();
						servers.Append().info = a.lastInfo;
						servers.last().haveToken = 	a.protocolIncompatible ? false : accessMap.IsSet(a.lastInfo.gameInfo.worldID);
						servers.last().protocolIncompatible = a.protocolIncompatible;
						servers.last().id = a.protocolIncompatible ? TUniqueID::Empty() : servers.last().info.hostInfo.value.hostID;
						servers.last().rightVersion = a.rightVersion && !a.protocolIncompatible;
						//if (!a.protocolIncompatible)
						serverMap.Set(servers.last().id,at);
						
					}

					ServerInfo&info = servers[at];
					info.addresses.Append(std::make_pair(a.address,a.lastPing));
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
		return client.IsConnected() && RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase && Animation::AllDone() && !newRoundRequested;
	}


}




void	Network::Receiver::PhaseChange(Protocol::PhaseChange&c)
{
	ASSERT__(!eventsLocked);

	switch (c.newPhase.value)
	{
		case RuntimeConfig::Phase::AnimationPhase:
			#ifdef HAS_WINDOW
				Objects::Renderer::ClearSmokeTrails();
				UI::EventLog::Clear();
				if (TimeLapse::enabled)
					saveScreenshot = true;
			#endif
			if (RuntimeConfig::aiMode)
				AI::vehicles.Clear();	//whichever is left, cannot be computed
			Animation::atTime = 0.f;
			foreach (Objects::vehicles, v)
				(*v)->preAnimationLocation = (*v)->location;
			if (c.timeFactor > 0)
				animationTimeFactor = c.timeFactor;
		break;
		case RuntimeConfig::Phase::CommandPhase:
			newRoundRequested = false;
			roundTimer = c.timeFactor;
			RuntimeConfig::roundNumber = c.roundNumber;
			RuntimeConfig::phase.Load(c.newPhase);

			Objects::ForeachMobileWorldEntity([](const Objects::PWorldEntity&e)
			{
				if (e->health <= 0 || !e->isOwnedByMe)
					return;
				Objects::Marker *m = Objects::Marker::Find(e->location);
				if (m)
					SignalMarkerDeletion(m->id);
			});

			#ifdef HAS_WINDOW
				for (const auto&s : Objects::structures)
				{
					if (s->isOwnedByMe && s->IsWorkingStructure() )
					{
						Objects::PWorkingStructure ws = To<Objects::WorkingStructure>::Cast(s);
						if (ws->AwaitsUserInteraction())
							Tutorial::ShowMessage("Interaction");
					}
				}

				if (TimeLapse::enabled)
					saveScreenshot = true;
			#endif
		break;
	}
	#ifdef HAS_WINDOW
		Tutorial::lastDelayingEvent = timer.Now();
	#endif
	RuntimeConfig::phase.Load(c.newPhase);
	RuntimeConfig::roundNumber = c.roundNumber;
	phaseStarted = timer.Now();
	#ifdef HAS_WINDOW
		Objects::UpdateChoices(ControlEnabled());
	#endif

	UI::HexGrid::RefreshContacts();
	//while (Objects::Network::records.IsNotEmpty() && timer.GetSecondsSince(Objects::Network::records.Peek().recorded) > 10)
	//	Objects::Network::records.EraseFront();
	LogEvent(EventSignificance::Common,nullptr,"Server phase changed to "+String(RuntimeConfig::phase.ToString())+" ("+String(c.timeFactor.value)+") @"+String(Animation::atTime));
	if (RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase)
	{
		Objects::CheckMobileEntitySanity();

		static Buffer0<Objects::Marker> outdated;
		Objects::Marker::GetOutdated(outdated,RuntimeConfig::roundNumber,RuntimeConfig::aiMode ? 100 : 10);
		foreach(outdated, o)
		{
			
			LogEvent(EventSignificance::Debug,nullptr,"Deleting marker "+o->GetName()+" since it's outdated (at age "+String(o->age)+")");
			SignalMarkerDeletion(o->id);
			//Network::Channels::destroyMarker.SendTo(Network::client, *o);
		}

		LogEvent(EventClass::Preset::important, nullptr, "Begin round " + String(RuntimeConfig::roundNumber));


	}
	if (gameHasBegun)
		for (const auto&f : Objects::Faction::allKnown)
		{
			f->requestedNewRound = false;
			f->roundChangeNotifications = 0;
		};
	if (RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase && gameHasBegun && Animation::AllDone())
		OnPlayerControlBegin();
	#ifdef HAS_WINDOW
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
	#endif
	Grid::visibleOrbitGrid.Invalidate();
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
	faction->roundChangeNotifications++;
	LogEvent(faction->roundChangeNotifications < 3 ? EventSignificance::Uncommon : EventSignificance::Common,nullptr,faction->GetName()+" is done thinking");
	#ifdef HAS_WINDOW
		UI::RefreshSessionView();
		UI::RefreshButtonState();
	#endif
}

void	Network::Receiver::FactionContinuedRound(NET(TUniqueID)&id)
{
	Objects::OwnerParameter faction = Network::ResolveFaction(id);
	faction->requestedNewRound = false;
	faction->roundChangeNotifications++;
	LogEvent(faction->roundChangeNotifications < 3 ? EventSignificance::Uncommon : EventSignificance::Common,nullptr,faction->GetName()+" is thinking some more");
	#ifdef HAS_WINDOW
		UI::RefreshSessionView();
		UI::RefreshButtonState();
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
		if (srv->rightVersion && srv->HasAddress(RuntimeConfig::serverAddress))
		{
			index_t best = 0;
			float bestPing = srv->addresses.first().second;
			for (index_t i = 1; i < srv->addresses.Count(); i++)
				if (srv->addresses[i].second < bestPing)
				{
					bestPing = srv->addresses[i].second;
					best = i;
				}
			srv->selected = true;

			Network::serverID = srv->id;
			Network::serverIDString = Network::serverID.ToNiceString();
			Network::worldID = srv->info.gameInfo.worldID;
			Network::worldIDString = Network::worldID.ToNiceString();
			Network::ConnectTo(srv->addresses[best].first);

			lastAttempt = timer.Now();
			delayCounter = 0;
			return true;
		}

	}
	return false;
}



#include <scriptCache.h>
#include <grid/resources.h>
#include <io/string_file.h>

#include "network.h"
#include "../version.h"
#include "animation.h"
#include "clientRuntimeConfig.h"
#include "visual/scene/world.h"
#include "cameraControl.h"
#include "visual/hud/hexGrid.h"
#include "visual/tutorial.h"
#include "visual/hud/dialog.h"
#include "visual/hud/menu.h"
#include "visual/hud/guiStatistics.h"
#include "ai.h"
#include "clientRuntimeConfig.h"
#include "timelapse.h"
#include "configurationMode.h"
#include "gridControl.h"
#include "visual/objectChoices.h"
#include "visual/scene/terrain.h"
#include "visual/hud/guiFactionTable.h"
#include "clientEvents.h"
#include "visual/hud/guiHostList.h"
#include "visual/hud/eventLog.h"
#include "voting.h"
#include "objectNetworkHelper.h"
#include "playerControlBegin.h"
#include <expectedBehavior.h>


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
	UINT16			connectionBasePort = DEFAULT_SERVER_BASE_PORT;

	CryptographicID	activeToken;
	String			myIDString,serverIDString,worldIDString,activeTokenString;
	INT32			uplinkBandwidth = -1;
	
	GenericHashTable<TUniqueID,CryptographicID>	accessMap;


	Protocol::GameProgress	gameProgress;
	TUniqueID				winningAllianceID = TUniqueID::Empty();


	float			animationTimeFactor = 1,
					roundTimer = 0;
	Timer::Time		phaseStarted;
	bool			newRoundRequested = false;

	GenericHashSet<TUniqueID>	visitedWorldMap;


	bool	IsValidNickname(const String&candidate)
	{
		if (candidate.GetLength() < 4)
			return false;
		bool rs = Package::IsValid(candidate,32);
		return rs;
	}


	void RegisterVisited(const TUniqueID&worldID)
	{
		if (visitedWorldMap.SetNew(worldID))
			SaveHosts();
	}

	bool DidVisit(const TUniqueID&worldID)
	{
		return visitedWorldMap.IsSet(worldID);
	}
	
	void LoadHosts()
	{
		try
		{
			StringFile visitedWorlds;
			visitedWorlds.Open(RuntimeConfig::myConfig.Lookup("visitedWorlds.txt"));
			String visited;
			while (visitedWorlds >> visited)
			{
				TUniqueID id;
				if (id.Decode(visited))
					visitedWorldMap.Set(id);
			}
		}
		catch (...)
		{}

		try
		{
			StringFile hosts;
			hosts.Open(RuntimeConfig::myConfig.Lookup("hosts.txt"));
			String host;
			while (hosts >> host)
				ServerDiscovery::RegisterNewHost(Network::ServerAddress(host),false, false);
		}
		catch (...)
		{}
	}

	void	SaveHosts()
	{
		try
		{
			StringFile visited;
			visited.Create(RuntimeConfig::myConfig.Lookup("visitedWorlds.txt"));
			static Array<TUniqueID>	 worlds;
			visitedWorldMap.ExportKeys(worlds);
			foreach (worlds,world)
				visited << world->ToFullString() << nl;
			visited.Close();
		}
		catch (const std::exception&ex)
		{
			LogException(nullptr,ex);
		}
		try
		{
			StringFile hosts;
			hosts.Create(RuntimeConfig::myConfig.Lookup("hosts.txt"));
		
			static Buffer0<Network::ServerAddress>	 urls;
			ServerDiscovery::ListNonAutoAttempts(urls);
			foreach (urls,url)
				hosts << url->host << ':' << url->basePort << nl;
			hosts.Close();
		}
		catch (const std::exception&ex)
		{
			LogException(nullptr,ex);
		}
	}

	void		SendChatMessage(const String&msg, const Objects::PFaction&receiver)
	{
		if (!receiver)
			return;
		if (msg.TrimRef().GetLength() > 0)
		{
			if (client.IsConnected())
				Channels::ChatMessage.SendTo(client,Protocol::ChatMessage(msg,receiver->id));
		}
	}

	void		SendChatMessage(const String&msg, const Objects::PAlliance&receiver)
	{
		if (!receiver)
			return;
		if (msg.TrimRef().GetLength() > 0)
		{
			if (client.IsConnected())
				Channels::ChatMessage.SendTo(client,Protocol::ChatMessage(msg,receiver->id));
		}
	}

	void		BroadcastChatMessage(const String&msg)
	{
		if (msg.TrimRef().GetLength() > 0)
		{
			if (client.IsConnected())
				Channels::ChatMessage.SendTo(client,Protocol::ChatMessage(msg,TUniqueID::Empty()));
		}
	}



	void			LoadGameConfig(const Protocol::GameConfig&cfg)
	{
		gameFlags = cfg.gameFlags;
		RuntimeConfig::gameGoal = RuntimeConfig::GameGoal::Reinterpret(cfg.gameGoal);
	}

	void			ConnectTo(const Host&host)
	{
		client.Disconnect();

		#ifdef HAS_WINDOW
			if (HUD::Dialog::QueryNicknameIfEmpty())
				return;
		#endif

		RuntimeConfig::tutorialMode = (host.serverInfo.gameInfo.gameConfig.gameFlags & Protocol::GameFlag::TutorialMode) != 0;

		RuntimeConfig::phase = RuntimeConfig::Phase::Uninitialized;
		gameHasBegun = false;

		LoadGameConfig(host.serverInfo.gameInfo.gameConfig);

		client.ConnectAsync(host.address.host + ":"+String(host.address.basePort + PortOffset::Session));
		#ifdef HAS_WINDOW
			HUD::WindowStack::SignalSituationChange();
		#endif

		serverID = host.serverInfo.hostInfo.hostID;
		serverIDString = Network::serverID.ToNiceString();

		worldID = host.serverInfo.gameInfo.worldID;
		worldIDString = worldID.ToNiceString();

		connectionBasePort = host.address.basePort;
	}




	Objects::OwnerParameter ResolveFaction(const TUniqueID&id, const TUniqueID&allianceID)
	{
		bool allianceChanged = false;
		auto rs = Objects::Faction::GetOrCreate(id,allianceID, allianceChanged);
		if (allianceChanged)
			GlobalEvents::onAllianceChange(rs);
		return rs;
	}

	Objects::OwnerParameter ResolveFaction(const TUniqueID&id)
	{
		return Objects::Faction::GetOrCreate(id);
	}


	static PathString AccessFilename()
	{
		return RuntimeConfig::myConfig.Lookup(RuntimeConfig::profilePrefix+ "worldAccess.m32");
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

		LogEvent(EventClass::Preset::common,nullptr,"World access map imported. "+String(accessMap.Count())+" entry/ies loaded.");
		fclose(f);
	}

	
	namespace ServerDiscovery
	{


		void OnServerInfo(Protocol::ServerInfo&info, TCP::Peer&peer);
		void OnPing(Protocol::Ping&ping, TCP::Peer&peer);

		TCP::Channel<Protocol::ServerInfo,0,Protocol::InfoChannel::ServerInfo>	serverInfoChannel(OnServerInfo);
		TCP::Channel<Protocol::Ping,0,Protocol::InfoChannel::Ping>	pingChannel(OnPing);

		class HostContactAttempt : public Sys::ThreadObject, public TCP::Peer::Attachment, public std::enable_shared_from_this<HostContactAttempt>
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

			static void		OnDeserializationFailed(UINT32, TCP::Peer&peer, const String&exception);

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
						HUD::WindowStack::SignalSituationChange();
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
			info.hostInfo.version[ARRAYSIZE(info.hostInfo.version)-1] = 0;
			me->hasInfo = true;
			me->rightVersion = !strcmp(info.hostInfo.version, SERVER_NAME);
		}


		/*static*/ void		HostContactAttempt::OnDeserializationFailed(UINT32, TCP::Peer&peer, const String&exception)
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
				case TCP::Event::ConnectionLost:
					//me->Release();
					if (me)
						me->protocolIncompatible = false;	//can't know for next connection
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
		ClientGrid::Apply(config);
	}


	static void		FlushFaction(bool alsoFlushWorld)
	{
		#ifdef HAS_WINDOW
			TimeLapse::enabled = false;
			Camera::locked = false;
		#endif

		haveFaction = false;
		//dbgRoundNumber = 0xFFFFFFFF;

		roundTimer = -1;

		GlobalEvents::FlushObjects();
		Network::winningAllianceID = TUniqueID::Empty();
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
		Animation::atTime = 0;

		if (alsoFlushWorld)
		{
			GlobalEvents::terrainFlush();
			RuntimeConfig::gameGoal = RuntimeConfig::GameGoal();
			ApplyPlanetConfig(Grid::TSharedPlanetConfig());
		}

		#ifdef HAS_WINDOW
			HUD::HexGrid::EventLog::Clear();
			HUD::HexGrid::RefreshContacts();

			amLocated = false;
			if (alsoFlushWorld)
				Camera::Reset();
		#endif
	}


	namespace Receiver
	{
		bool			eventsLocked = true;

		void	GameConfig(Protocol::GameConfig&cfg)
		{
			Network::LoadGameConfig(cfg);
			LogEvent(EventClass::Preset::important,"Game config updated");
		}


		void	OnHelloResponse(Protocol::HelloResponse&response)
		{
			ASSERT__(response.success);
			if (response.success)
			{
				LogEvent(EventClass::Preset::common, nullptr, "Status confirmed ('"+response.errorMessage+"')");
				const bool gameEndSpectatorMode = (response.gameFlags & Protocol::GameFlag::GameHasEnded) != 0;
				if (gameEndSpectatorMode)
					AttemptFactionLogin(false,Protocol::FactionType::SpectatorFaction);
				elif (RuntimeConfig::tutorialMode)
					AttemptFactionLogin(false,Protocol::FactionType::ContinueOnly);
				else
				{
					#ifdef HAS_WINDOW
						HUD::WindowStack::Show(HUD::Menu::login);
					#endif
				}
				#ifdef HAS_WINDOW
					HUD::HostList::CheckVisibility(false);
				#endif
			}
			else
			{
				LogEvent(EventClass::Preset::networkError,"Server rejected hello ('"+response.errorMessage+"')");
				client.Disconnect();
			}
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
				LogEvent(EventSignificance::Debug, "Appended world delta primitive to animation sequence at t="+String(delta.timestamp.atTime));
			}
			else
			{
				#ifdef HAS_WINDOW
					DBG_ASSERT__(!ConfigurationMode::IsActive());
				#endif
				BYTE family = delta.familyAndLastFlag & 0x7F;
				bool last = (delta.familyAndLastFlag & 0x80) != 0;
				World::actual.BeginChangeSequence(0).ApplyFamilyDelta(family,delta.data0,false,true);

				#ifdef HAS_WINDOW
					World::allowRecompilation = last && gameHasBegun;
				#endif
			}

		}




		void OnMobileState(Protocol::MobileState&vs)
		{
			ASSERT__(!eventsLocked);
			#ifdef HAS_WINDOW
				DBG_ASSERT__(!ConfigurationMode::IsActive());
			#endif

			if (vs.entityState.timestamp.rightNow && Animation::AllDone())
			{
				Objects::PWorldEntity e =Objects::GetEntity(vs.entityState.id);
				ASSERT1__(e, vs.entityState.id);
				Objects::PMobileEntity v = To<Objects::MobileEntity>::DynamicCast(e);
				ASSERT1__(v,e->GetName());
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
								LogEvent(EventSignificance::Debug, "Rewrote 'mobile state("+ToString(vs.entityState.id)+")' primitive to animation sequence at t="+String(vs.entityState.timestamp.atTime)+" n="+String(p->overwritten));
							return;
						}
					}
					Animation::primitives.Append().Init(vs);
					LogEvent(EventSignificance::Debug, "Appended 'mobile state("+ToString(vs.entityState.id)+")' primitive to animation sequence at t="+String(vs.entityState.timestamp.atTime));
				}
			}
		}


		void	OnNewMobileEntity(Protocol::NewMobileEntity&vc)
		{
			ASSERT__(!eventsLocked);
			ASSERT__(Network::haveID || Network::amSpectator);
			#ifdef HAS_WINDOW
				DBG_ASSERT__(!ConfigurationMode::IsActive());
			#endif

			try
			{

				Objects::PMobileEntity v;
				
				EXPECT2(vc.state.entityState.health>0,vc.state.entityState.health,vc.state.entityState.id);

				auto m = vc.state.myMobileEntity.Get0();
				const bool place = vc.state.entityState.timestamp.rightNow && (!m || m->storedIn.Get0() == 0);
				if (place)
				{
					Objects::PWorldEntity existing = Objects::GetEntity(vc.state.entityState.id);
					if (existing)
					{
						//could throw an exception here, but observed that if this happens, it happens A LOT in quick succession
						LogEvent(EventClass::Preset::protocolError,existing.get(),"Trying to double-spawn as "+ToString(vc.type)+" (mobile)");
						return;
					}
				}
				v = Objects::SpawnMobileEntity(Network::ResolveFaction(vc.owner),  vc.state.entityState.id,vc.state.entityState.location,vc.type,place,Network::haveID && Network::myID == vc.owner);
				if (vc.state.entityState.timestamp.rightNow)
					LogEvent(EventSignificance::Common,v.get(),  "Placed vehicle entity outside animation");
				bool isOwnedByMe = v->isOwnedByMe;

				//v->id = vc.state.entityState.id;
				Animation::ApplyState(v,vc.state,true, !vc.state.entityState.timestamp.rightNow);
				ASSERT_EQUAL1__(v->health,vc.state.entityState.health,v->id);
				#ifdef HAS_WINDOW
					if (v->isOwnedByMe && !amLocated)
					{
						amLocated = true;
						Camera::TranslateTo(v->system.w.xyz);
					}
				#endif
				if (vc.state.entityState.timestamp.rightNow)
				{
					#ifdef HAS_WINDOW
						if (!isOwnedByMe)
							HUD::HexGrid::RefreshContacts();
					#endif
				}
				else
				{
					Animation::primitives.Append().Init(vc,v);
					LogEvent(EventSignificance::Debug, "Appended 'new mobile entity("+ToString(vc.state.entityState.id)+")' primitive to animation sequence at t="+String(vc.state.entityState.timestamp.atTime));

				}
				ASSERT_EQUAL1__(v->health,vc.state.entityState.health,v->id);

			}
			catch (const std::exception & ex)
			{
				LogEvent(EventClass::Preset::protocolError,ex.what());
				DBG_FATAL__(ex.what());
			}
		}

		

		void	OnNewStructure(Protocol::StructureConstruction&sc)
		{
			ASSERT__(!eventsLocked);
			#ifdef HAS_WINDOW
				DBG_ASSERT__(!ConfigurationMode::IsActive());
			#endif

			//#ifdef HAS_WINDOW
			//	if (World::actual.lightingOutdated && World::allowRecompilation)
			//		World::actual.UpdateLighting();
			//#endif
			try
			{
				const bool place = sc.state.entityState.timestamp.rightNow;
				if (place)
				{
					Objects::PWorldEntity existing = Objects::GetEntity(sc.state.entityState.id);
					if (existing)
					{
						//could throw an exception here, but observed that if this happens, it happens A LOT in quick succession
						LogEvent(EventClass::Preset::protocolError,existing.get(),"Trying to double-spawn as "+ToString(sc.type)+" (structure)");
						return;
					}
				}
				Objects::PStructure s = Objects::SpawnStructure(Network::ResolveFaction(sc.owner),  sc.state.entityState.id,sc.state.entityState.location,sc.type, place, Network::haveID && Network::myID == sc.owner);
				ASSERT__(s);
				if (sc.state.entityState.timestamp.rightNow)
					LogEvent(EventSignificance::Common,s.get(),  "Placed structure entity outside animation");

				Animation::UpdateState(s,sc.state);
				if (!sc.state.entityState.timestamp.rightNow)
				{
					Animation::primitives.Append().Init(sc,s);
					LogEvent(EventSignificance::Debug, "Appended 'new structure("+ToString(sc.state.entityState.id)+")' primitive to animation sequence at t="+String(sc.state.entityState.timestamp.atTime));
				}
				else
				{
					#ifdef HAS_WINDOW
						if (!s->isOwnedByMe)
							HUD::HexGrid::RefreshContacts();
					#endif
				}
			}
			catch (const std::exception & ex)
			{
				LogEvent(EventClass::Preset::protocolError,ex.what());
				DBG_FATAL__(ex.what());
			}
		}

		void OnStructureState(Protocol::StructureState&vs)
		{
			ASSERT__(!eventsLocked);
			#ifdef HAS_WINDOW
				DBG_ASSERT__(!ConfigurationMode::IsActive());
			#endif

			if (vs.entityState.timestamp.rightNow && Animation::AllDone())
			{
				Objects::PWorldEntity e =Objects::GetEntity(vs.entityState.id);
				if (!e)
				{
					LogEvent(EventClass::Preset::networkError,"Trying to update structure state of entity "+ToString(vs.entityState.id)+", which is currently not registered");
					DBG_FATAL__("Invalid state detected");
				}
				else
				{
					Objects::PStructure s = To<Objects::Structure>::DynamicCast(e);
					if (s)
						Animation::UpdateState(s,vs);
					else
					{
						LogEvent(EventClass::Preset::networkError,"Trying to update structure state of entity "+e->GetName()+", which is not a structure");
						DBG_FATAL__("Invalid state detected");
					}
				}
			}
			else
			{
				if (vs.entityState.timestamp.rightNow)
					Animation::whenDone.Append().Init(vs);
				else
				{
					Animation::primitives.Append().Init(vs);
					LogEvent(EventSignificance::Debug, "Appended 'structure state("+ToString(vs.entityState.id)+")' primitive to animation sequence at t="+String(vs.entityState.timestamp.atTime));
				}
			}

		}

		void DestroyEntity(Protocol::DestroyEntity&de)
		{
			ASSERT__(!eventsLocked);
			#ifdef HAS_WINDOW
				DBG_ASSERT__(!ConfigurationMode::IsActive());
			#endif

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
						float t = 0;
						if (Objects::GetEntity(e->id))
							Objects::Remove(e,type, t);
						#ifdef HAS_WINDOW
							HUD::WindowStack::SignalSituationChange();
						#endif
					});
				}
				else
				{
					float t = 0;
					Objects::Remove(e,type, t);
					#ifdef HAS_WINDOW
						HUD::WindowStack::SignalSituationChange();
					#endif
				}

				#ifdef HAS_WINDOW
					if (!e->isOwnedByMe)
						HUD::HexGrid::RefreshContacts();
				#endif
			}
			else
			{
				LogEvent(EventSignificance::Debug,nullptr,"Reception of entity destruction message for "+ToString(de.id)+" ("+Objects::RemovalType::Reinterpret(de.type).ToString()+")");
				if (de.timestamp.rightNow)
					Animation::whenDone.Append().Init(de);
				else
				{
					Animation::primitives.Append().Init(de);
					LogEvent(EventSignificance::Debug, "Appended 'destruction' primitive to animation sequence at t="+String(de.timestamp.atTime));
				}
			}
		}

		void ControlMessage(Protocol::ControlMessage&msg)
		{

			Objects::OwnerParameter f;
			Objects::PWorldEntity	e;
			if (msg.focus.IsNotEmpty())
			{
				f = Objects::Faction::FindByID(msg.focus);
				if (!f)
				{
					e = Objects::GetEntity(msg.focus);
				}
			}

			Logging::Origin origin = msg.isPrivate ? Logging::Origin::ServerPrivate : Logging::Origin::ServerPublic;

			auto cls = EventClass( EventSignificance::Reinterpret(msg.significance),  EventFlavor::Reinterpret(msg.flavor));
			if (f)
				LogEvent(cls ,f,msg.message,origin);
			else
				LogEvent(cls ,e.get(),msg.message,origin);

			#ifdef HAS_WINDOW
				if (cls.flavor == EventFlavor::ChatMessage)
					HUD::FactionTable::SignalChatMessage(f,msg.message);
			#endif

		}




		void	ClientFactionResponse(Protocol::ClientFactionResponse&cr)
		{
			ASSERT__(!eventsLocked);

			ApplyPlanetConfig(cr.planetConfig);

			myID = TUniqueID::Empty();
			Objects::Faction::mine.reset();
			myIDString = ToString(myID);

			Network::gameFlags = cr.info.gameConfig.gameFlags;

			RuntimeConfig::nexusBases = (Network::gameFlags & Protocol::GameFlag::NexusHasBases) != 0;
			RuntimeConfig::nexusPortsOnAllPoles = (Network::gameFlags & Protocol::GameFlag::NexusPortsOnAllPoles) != 0;
			RuntimeConfig::empZones = (Network::gameFlags & Protocol::GameFlag::EMPZones) != 0;
			RuntimeConfig::gameGoal = RuntimeConfig::GameGoal::Reinterpret(cr.info.gameConfig.gameGoal);
			RuntimeConfig::phase = RuntimeConfig::Phase::Reinterpret(cr.info.activePhase);
			RuntimeConfig::tutorialMode = (Network::gameFlags & Protocol::GameFlag::TutorialMode) != 0;
			#ifdef HAS_WINDOW
				ConfigurationMode::SetGameConfig(Network::gameFlags, RuntimeConfig::gameGoal, Grid::WorldDiscovery::Reinterpret( cr.info.gameConfig.worldDiscovery ));
				World::allowRecompilation = false;
				ConfigurationMode::SetActive(false);
			#endif
			gameHasBegun = false;

			haveFaction = cr.startFactionTypes != 0;
			if (haveFaction)
			{
				RegisterVisited(worldID);
				amSpectator = cr.startFactionTypes == Protocol::FactionType::SpectatorFaction;
				#ifdef HAS_WINDOW
					Objects::resourceView = !amSpectator;
					HUD::HexGrid::RefreshButtonState();
				#endif

				activeToken = cr.factionToken;
				activeTokenString = activeToken.ToString();
				if (!RuntimeConfig::aiMode && !amSpectator)
				{
					LogEvent(EventClass::Preset::common,nullptr,"Updating access token of world "+ToString(worldID)+" to "+activeToken.ToString());
					accessMap.Set(worldID,activeToken);
					SaveWorldAccessTokens();
					Network::Channels::changeMyName.SendTo(Network::client,Network::chosenNickname);
				}

				#ifdef HAS_WINDOW
					HUD::WindowStack::Hide(HUD::Menu::login);
					//if (ConfigurationMode::IsActive())
					//	HUD::HexGrid::Configure
					//	HUD::WindowStack::Show(HUD::Dialog::configMenu);
				#endif
			}
			else
			{
				amSpectator = false;
				LogEvent(EventClass::Preset::uncommon, nullptr, "Server response: Faction void.");
				FlushFaction(true);
				#ifdef HAS_WINDOW
					HUD::WindowStack::Show(HUD::Menu::login);
				#endif
			}

			if (!amSpectator)
				Network::Channels::changeMyName.SendTo(Network::client,chosenNickname);

			#ifdef HAS_WINDOW
				if (amSpectator)
				{
					World::actual.FlushAndValidateSensory(1);

				}
			#endif
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
					if (p->targetEntityID == m.id && p->type == Animation::Primitive::MovementStream && p->path->rawPath.Last().first == m.fromLocation)
					{
						p->Append(m);
						found = true;
						break;
					}
				if (!found)
				{
					Animation::primitives.Append().Init(m);
					LogEvent(EventSignificance::Debug, "Appended 'mobile entity motion' primitive to animation sequence at t="+String(m.timestamp.atTime));
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
				LogEvent(EventSignificance::Debug, "Appended 'vague trajectory' primitive to animation sequence at t="+String(a.timestamp.atTime));
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
				LogEvent(EventSignificance::Debug, "Appended 'precise trajectory' primitive to animation sequence at t="+String(a.timestamp.atTime));
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
				LogEvent(EventSignificance::Debug, "Appended 'explosion' primitive to animation sequence at t="+String(ex.timestamp.atTime));
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
				v->turretAngleY = -(90.f - Vec::angle(rot.toWorld,Grid::low.cells[v->location.stackID].centerVertex.xyz)) * M_PIF / 180.f;
			}
			else
			{
				Animation::primitives.Append().Init(rot);
				LogEvent(EventSignificance::Debug, "Appended 'entity turret rotation' primitive to animation sequence at t="+String(rot.timestamp.atTime));
			}
		}


		void	ResourceUpdate(Protocol::ResourceUpdate&u)
		{
			ASSERT__(!eventsLocked);

			if (u.timestamp.rightNow)
			{
				#ifdef HAS_WINDOW
					Grid::UpdateResourceMap(u.resourceName,u.cells,Grid::ResourceDiscovery::client);
				#endif
			}
			else
			{
				Animation::primitives.Append().Init(u);
				LogEvent(EventSignificance::Debug, "Appended 'resource update' primitive to animation sequence at t="+String(u.timestamp.atTime));
			}

		}

		void	GameProgress(Protocol::GameProgress&progress)
		{
			Network::gameProgress.swap(progress);
		}

		void	CreateMarker(Protocol::Marker&marker)
		{
			TUniqueID	entityID = TUniqueID::Empty(), ownerID = TUniqueID::Empty();
			Objects::EntityType	entityType;
			Objects::Marker::Type type;

			const Protocol::MarkerReferenceEntity*entity = marker.data.Get0();
			if (entity)
			{
				type = Objects::Marker::Type::Reinterpret(0);
				entityID = entity->id;
				ownerID = entity->owner;
				entityType = entity->type;
			}
			else
			{
				entity = marker.data.Get1();
				if (entity)
				{
					type = Objects::Marker::Type::Reinterpret(1);
					entityID = entity->id;
					ownerID = entity->owner;
					entityType = entity->type;
				}
				else
				{
					ASSERT_NOT_NULL__(marker.data.Get2());
					type = Objects::Marker::Type::Reinterpret(2);
				}
			}
			Objects::Marker::Create(marker.id, type, marker.location, marker.createdInRound, entityID, ownerID, entityType, std::max(RuntimeConfig::roundNumber,marker.createdInRound));
			GlobalEvents::invalidateObjectCache();
		}

		void	MarkerCreationFailedLimitReached(Protocol::Marker&marker);	//must re-send, must know channels, cannot be implemented here

		void	DestroyMarker(NET(TUniqueID)&mid)
		{
			Objects::Marker::Destroy(mid, ControlEnabled());
			GlobalEvents::invalidateObjectCache();
		}

		void	IdentifyFaction(Protocol::IdentifyFaction&id);
		void	FactionOnline(NET(TUniqueID)&);
		void	FactionOffline(NET(TUniqueID)&);
		void	FactionRequestedNewRound(NET(TUniqueID)&);
		void	FactionContinuedNewRound(NET(TUniqueID)&);
		void	LockEmpZones(NET(UINT16)&);
		void	UplinkBandwidth(NET(INT32)&v)
		{
			uplinkBandwidth = v;
			RuntimeConfig::hibernationSystemEnabled = v > 0;
			GlobalEvents::invalidateObjectCache();
		}

		void	TutorialAt(NET(INT32)&at)
		{
			#ifdef HAS_WINDOW
				Tutorial::SetProgress(at);
			#endif
		}


		void	UnitLimits(Protocol::UnitLimits&limits)
		{
			RuntimeConfig::UnitLimits::Import(limits);
			GlobalEvents::invalidateObjectCache();
		}

		void	PlanetConfig(Grid::ExtendedPlanetConfig&cfg)
		{
			#ifdef HAS_WINDOW
				ConfigurationMode::Update(cfg);
			#endif
		}

		void	OperationLacksResources(Protocol::OperationLacksResources&res)
		{
			bool brk = true;
		}

		enum class ScriptChangeFlags
		{
			Empty = 0x0,
			HaveNewTopics = 0x1,
			HaveMyAttributes = 0x2,
			HaveZeroAttributes = 0x4,
			HaveMyResearchFactionState = 0x8,

			ShouldLoad = HaveNewTopics | HaveMyAttributes | HaveZeroAttributes | HaveMyResearchFactionState

		};

		inline ScriptChangeFlags	operator|(ScriptChangeFlags a,ScriptChangeFlags b)
		{
			typedef std::underlying_type<ScriptChangeFlags>::type	T;
			return static_cast<ScriptChangeFlags>( static_cast<T>(a) | static_cast<T>(b) );
		}

		inline void	operator|=(ScriptChangeFlags&a,ScriptChangeFlags b)
		{
			a = a | b;
		}

		inline bool	operator&(ScriptChangeFlags a,ScriptChangeFlags b)
		{
			typedef std::underlying_type<ScriptChangeFlags>::type	T;
			return (static_cast<T>(a) & static_cast<T>(b) ) != 0;
		}
		
		static ScriptChangeFlags scriptChangeFlags = ScriptChangeFlags::Empty;


		void	CheckScriptChangeFlags()
		{
			if (scriptChangeFlags == ScriptChangeFlags::ShouldLoad)
			{
				scriptChangeFlags = ScriptChangeFlags::Empty;
				Objects::Research::globalState.onScriptChange();
			}
		}


		void	ResearchFactionState(Protocol::ResearchFactionState&state)
		{
			Objects::PFaction f = ResolveFaction(state.ownerID);
			EXPECT_EQUAL(Objects::Research::globalState.allTopics.Count(),state.progressVector.Length());
			f->researchState.topicProgressVector.adoptData(state.progressVector);
			f->researchState.topicProgressChanged = true;
			#ifdef HAS_WINDOW
				if (RuntimeConfig::tutorialMode && f == Objects::Faction::mine)
					Tutorial::SignalResearchChange();
			#endif

			if (f->isOwnedByMe && f->researchState.topicProgressVector.Count() == Objects::Research::globalState.allTopics.Count())
			{
				scriptChangeFlags |= ScriptChangeFlags::HaveMyResearchFactionState;

				CheckScriptChangeFlags();
			}

		}

		//research program updates must always preceed research states
		void	ResearchProgram(Protocol::ResearchProgram&program)
		{
			Objects::Research::FactionState::BeginServerTopicUpdate();
			foreach (program,prog)
				Objects::Research::FactionState::DeclareServerTopic(prog->parentTopic -1, prog->name,prog->iconName, prog->maxProgress,prog->dependentTopic-1);


			scriptChangeFlags = ScriptChangeFlags::HaveNewTopics; //must wait for state. Flush all other updates
		}


		void	ResearchQueue(Protocol::ResearchQueue&research)
		{
			if (Objects::Faction::mine)
			{
				Objects::Faction::mine->researchState.RestoreResearchQueue(research);
				//HUD::Dialog::SignalSituationChange();
				#ifdef HAS_WINDOW
					if (Objects::selectedEntity && Objects::selectedEntity->IsWorkingStructure() && To<Objects::WorkingStructure>::Cast(Objects::selectedEntity)->IsLaboratory())
						HUD::HexGrid::Configure(Objects::selectedEntity);
					HUD::HexGrid::RefreshButtonState();
					if (RuntimeConfig::tutorialMode)
						Tutorial::SignalResearchChange();
				#endif
			}
		}


		template <typename T>
			static void Import(Protocol::ResearchAttributeArray<T>&source, Objects::Research::Attributes::TypeMap<T>&dest)
			{
				dest.attributes.SetSize(source.Count());	//don't assert, just resize. might change
				for (index_t i = 0; i < dest.attributes.Count(); i++)
				{
					auto&src = source[i];
					auto&dst = dest.attributes[i];
					dst.value = src.value;
					dst.description.adoptData(src.description);
					dst.flags = src.flags;
				}
			}

		template <typename T>
			static void Import(Protocol::ResearchAttributeDeltaArray<T>&source, Objects::Research::Attributes::TypeMap<T>&dest)
			{
				foreach (source,src)
				{
					auto&dst = Expect::SafeGet(dest.attributes,src->index,CLOCATION);
					dst.value = src->value;
					dst.description.adoptData(src->description);
					dst.flags = src->flags;
				}
			}

		void	ResearchAttributeVector( Protocol::ResearchAttributeVector&attributes )
		{
			Objects::Research::Attributes::Table*table0 = nullptr;
			bool isMine = false;
			const bool zeroTable = attributes.faction.IsEmpty();
			if (zeroTable)
				table0 = &Objects::Research::globalState.zeroTable;
			else
			{
				auto f = ResolveFaction(attributes.faction);
				if (f->isOwnedByMe)
					isMine = true;
				table0 = &f->researchState.attributeTable;
			}
			auto&table = *table0;

			Import(attributes.boolAttributes,table.bools);
			Import(attributes.intAttributes,table.ints);
			Import(attributes.floatAttributes,table.floats);


			if (isMine)
				scriptChangeFlags |= ScriptChangeFlags::HaveMyAttributes;
			elif (zeroTable)
				scriptChangeFlags |= ScriptChangeFlags::HaveZeroAttributes;

			CheckScriptChangeFlags();
		}

		void	ResearchAttributeDeltaVector(Protocol::ResearchAttributeDeltaVector&attributes)
		{
			Objects::Research::Attributes::Table*table0 = nullptr;
			if (attributes.faction.IsEmpty())
				table0 = &Objects::Research::globalState.zeroTable;
			else
				table0 = &ResolveFaction(attributes.faction)->researchState.attributeTable;
			auto&table = *table0;

			Import(attributes.boolAttributes,table.bools);
			Import(attributes.intAttributes,table.ints);
			Import(attributes.floatAttributes,table.floats);
		}



		void	WinningAlliance(TUniqueID&id)
		{
			winningAllianceID = id;
			#ifdef HAS_WINDOW
				HUD::Statistics::Update();

				const auto winningAlliance = Objects::Alliance::FindByID(winningAllianceID);
				if (winningAlliance)
				{
					if (Objects::Faction::mine && Objects::Faction::mine->alliance == winningAlliance)
					{
						if (winningAlliance->CountLivingMembers() > 1)
							HUD::Dialog::ShowMessage("Congratulations!\nYour alliance has won this game!");
						else
							HUD::Dialog::ShowMessage("Congratulations!\nYou have won this game!");
					}
					else
					{
						Buffer0<String> names;
						winningAlliance->VisitMembers([&names](const Objects::PFaction&f)
						{
							names << f->GetName();
						});
						if (names.Count() != 1)
							HUD::Dialog::ShowMessage(implode(", ",names.ToRef())+" have won this game!");
						else
							HUD::Dialog::ShowMessage(names.First()+" has won this game!");
					}
				}

			#endif
		}



		namespace Statistics
		{
			static Objects::WFaction target;

			void	BeginSequence(Protocol::BeginStatisticsSequence&begin)
			{
				if (!target.expired())
				{
					LogEvent(EventClass::Preset::networkError,"Sequence violation. Statistics sequence on "+target.lock()->GetLogName()+" should have been ended");
					DBG_FATAL__("Sequence violation");
					EndSequence();
				}

				Objects::PFaction f = ResolveFaction(begin.factionID,begin.allianceID);
				f->name = begin.factionName;
				f->isDead = begin.factionIsDead;
				f->color = begin.color;

				LogEvent(EventClass::Preset::common,"Starting statistics sequence on "+f->GetLogName());
				target = f;
				
				f->statistics.BeginLoading();
			}

			void	EndSequence()
			{
				auto f = target.lock();
				target.reset();
				if (f)
				{
					f->statistics.EndLoading(false);
					#ifdef HAS_WINDOW
						HUD::Statistics::Update();
					#endif
				}
			}

			void	Round(Objects::Statistics::Round&r)
			{
				auto f = target.lock();
				if (!f)
					throw Except::Program::DataConsistencyFault(CLOCATION,"Unable to load round statistics: Not in sequence or sequence faction has meanwhile expired");
				f->statistics.Load(std::move(r));
			}
			void	FactionInteraction(Objects::Statistics::FactionInteraction&i)
			{
				auto f = target.lock();
				if (!f)
					throw Except::Program::DataConsistencyFault(CLOCATION,"Unable to load faction interaction statistics: Not in sequence or sequence faction has meanwhile expired");
				f->statistics.Load(std::move(i));
			}
			void	EntityType(Objects::Statistics::EntityType&t)
			{
				auto f = target.lock();
				if (!f)
					throw Except::Program::DataConsistencyFault(CLOCATION,"Unable to load entity type statistics: Not in sequence or sequence faction has meanwhile expired");
				f->statistics.Load(std::move(t));
			}
		}

		void	BeginStatisticsSequence(Protocol::BeginStatisticsSequence&begin)
		{
			Statistics::BeginSequence(begin);
		}

		void MyFaction(Protocol::MyFaction&faction)
		{
			ASSERT__(!eventsLocked);

			haveID = true;
			amSpectator = false;
			myID = faction.factionID;
			Objects::Faction::mine = ResolveFaction(myID,faction.alliance.allianceID);
			Objects::Faction::mine->alliance->UpdateConfiguration(faction.alliance.flags);
			myIDString = ToString(myID);
			Objects::Faction::mine->isOwnedByMe = true;
			#ifdef HAS_WINDOW
				HUD::FactionTable::Update();
			#endif
		}

		void AllianceConfigChange(Protocol::AllianceConfigChange&ch)
		{
			ASSERT__(!eventsLocked);
			Objects::PAlliance alliance = Objects::Alliance::FindByID(ch.allianceID);
			if (alliance)
			{
				alliance->config = ch.flags;
				#ifdef HAS_WINDOW
					HUD::FactionTable::Update();
				#endif
			}
			else
				LogEvent(EventClass::Preset::internalError,"Can't find alliance "+ToString(ch.allianceID)+" for config update");

		}

		void	VoteCreation(Protocol::VoteCreation&vote)
		{
			Vote::Create(vote.voteID,vote.remainingSeconds,vote.phrase);
		}

		void	AllianceInvite(Protocol::AllianceInvite&invite)
		{
			auto alliance = Objects::Alliance::FindByID(invite.allianceID);
			auto faction = Objects::Faction::FindByID(invite.factionID);
			if (!alliance)
			{
				LogEvent(EventClass::Preset::internalError,"Can't find alliance "+ToString(invite.allianceID)+" for invite");
				return;
			}
			if (!faction)
			{
				LogEvent(EventClass::Preset::internalError,"Can't find faction "+ToString(invite.factionID)+" for alliance invite");
				return;
			}
			switch (invite.type)
			{
				case Protocol::AllianceInviteType::Invite:
					alliance->Invite(faction);
				break;
				case Protocol::AllianceInviteType::Cancel:
				case Protocol::AllianceInviteType::Accept:
				case Protocol::AllianceInviteType::Reject:
					alliance->CancelInvite(faction);
				break;
				default:
					LogEvent(EventClass::Preset::internalError,"Unexpected invite type for alliance "+ToString(alliance->id)+" and faction "+faction->GetLogName());
			}
			#ifdef HAS_WINDOW
				HUD::FactionTable::Update();
			#endif
		}

		void	VoteResult(Protocol::VoteResult&result)
		{
			auto v = Vote::FindByID(result.voteID);
			if (!v)
			{
				LogEvent(EventSignificance::NonFatalError,"Unable to find vote "+ToString(result.voteID));
				return;
			}
			Vote::TOutcome outcome;
			outcome.eligibleToVote = result.eligibleToVote;
			outcome.quorum = result.quorum;
			outcome.result = (Protocol::VoteResultType::T)result.resultType;
			outcome.votedAgainst = result.votedAgainst;
			outcome.votedInFavor = result.votedInFavor;

			v->Close(outcome);
		}

		void	KillFaction(Protocol::KillFaction&kill)
		{
			Objects::PFaction f = Objects::Faction::FindByID(kill);
			if (f)
			{
				const bool isMine = f == Objects::Faction::mine;
				bool shouldBeGone = f->alliance->CountLivingMembers() == 1;
				const TUniqueID aid = f->alliance->id;
				const Objects::WAlliance wa = f->alliance;
				Objects::Faction::Kill(f);
				ASSERT__(!f->alliance);
				if (shouldBeGone)
				{
					ASSERT1__(wa.expired(),wa.lock()->CountLivingMembers());
					ASSERT__(!Objects::Alliance::FindByID(aid));
				}
				#ifdef HAS_WINDOW
					HUD::FactionTable::Update();
				#endif
				if (isMine)
				{
					haveFaction = false;
					haveID = false;
					accessMap.Unset(worldID);
					SaveWorldAccessTokens();
					LogEvent(EventClass(EventSignificance::ClientFatal,EventFlavor::Loss),"Your faction has been annihilated");
				}
			}
		}

	}


	namespace Channels
	{
		#undef DECLARE_CHANNEL
		#define	DECLARE_CHANNEL(NAME)	TCP::Channel<Protocol::NAME,0,Protocol::ChannelID::NAME>	NAME(Receiver::NAME);
		#undef DECLARE_OBJECT_SENDER
		#define	DECLARE_OBJECT_SENDER(NAME)	TCP::ObjectSender<Protocol::NAME,Protocol::ChannelID::NAME>	NAME;

		TCP::Channel<Protocol::ControlMessage,0,Protocol::ChannelID::ControlMessage>				controlMessage(Receiver::ControlMessage);

		TCP::Channel<Protocol::ClientFactionResponse,0,Protocol::ChannelID::ClientFaction>			clientFactionResponse(Receiver::ClientFactionResponse);

		TCP::Channel<Protocol::HelloResponse,0,Protocol::ChannelID::Hello>							helloResponse(Receiver::OnHelloResponse);
		TCP::Channel<Protocol::WorldDelta,0,Protocol::ChannelID::WorldDelta>						worldData(Receiver::OnWorldDelta);
		TCP::Channel<Protocol::NewMobileEntity,0,Protocol::ChannelID::NewMobileEntity>				newMobileEntity(Receiver::OnNewMobileEntity);
		TCP::Channel<Protocol::MobileState,0,Protocol::ChannelID::MobileState>						mobileStateUpdate(Receiver::OnMobileState);
		TCP::Channel<Protocol::StructureConstruction,0,Protocol::ChannelID::InstantiateStructure>	newStructure(Receiver::OnNewStructure);
		TCP::Channel<Protocol::StructureState,0,Protocol::ChannelID::UpdateStructure>				structureStateUpdate(Receiver::OnStructureState);
		TCP::Channel<Protocol::DestroyEntity,0,Protocol::ChannelID::DestroyEntity>					destroyEntity(Receiver::DestroyEntity);

		TCP::Channel<Protocol::PhaseChange,0,Protocol::ChannelID::PhaseChange>						phaseChange(Receiver::PhaseChange);
		TCP::Channel<Protocol::MobileEntityMotion,0,Protocol::ChannelID::MobileEntityMotion>		mobileEntityMotion(Receiver::MobileEntityMotion);
		TCP::Channel<Protocol::VagueAttackTrajectory,0,Protocol::ChannelID::VagueAttackTrajectory>			vagueAttackTrajectory(Receiver::VagueAttackTrajectory);
		TCP::Channel<Protocol::PreciseAttackTrajectory,0,Protocol::ChannelID::PreciseAttackTrajectory>		preciseAttackTrajectory(Receiver::PreciseAttackTrajectory);
		TCP::Channel<Protocol::Explosion,0,Protocol::ChannelID::Explosion>							explosion(Receiver::Explosion);
		TCP::Channel<Protocol::EntityTurretRotation,0,Protocol::ChannelID::EntityTurretRotation>	entityTurretRotation(Receiver::EntityTurretRotation);
		TCP::Channel<Protocol::ResourceUpdate,0,Protocol::ChannelID::ResourceUpdate>				resourceUpdate(Receiver::ResourceUpdate);
		TCP::Channel<Protocol::GameProgress,0,Protocol::ChannelID::GameProgress>					gameProgress(Receiver::GameProgress);

		TCP::Channel<Protocol::Marker, 0, Protocol::ChannelID::CreateMarker>						createMarker(Receiver::CreateMarker);
		TCP::Channel<Protocol::Marker, 0, Protocol::ChannelID::MarkerCreationFailedLimitReached>	markerCreationFailedLimitReached(Receiver::MarkerCreationFailedLimitReached);
		TCP::Channel<NET(TUniqueID), 0, Protocol::ChannelID::DestroyMarker>							destroyMarker(Receiver::DestroyMarker);

		TCP::Channel<Protocol::IdentifyFaction, 0, Protocol::ChannelID::IdentifyFaction>			identifyFaction(Receiver::IdentifyFaction);
		TCP::Channel<NET(TUniqueID), 0, Protocol::ChannelID::FactionOnline>							factionOnline(Receiver::FactionOnline);
		TCP::Channel<NET(TUniqueID), 0, Protocol::ChannelID::FactionOffline>						factionOffline(Receiver::FactionOffline);
		TCP::Channel<NET(TUniqueID), 0, Protocol::ChannelID::FactionRequestedNewRound>				factionRequestedNewRound(Receiver::FactionRequestedNewRound);
		TCP::Channel<NET(TUniqueID), 0, Protocol::ChannelID::FactionContinuedRound>					factionContinuedRound(Receiver::FactionContinuedRound);
		TCP::Channel<NET(UINT16), 0, Protocol::ChannelID::LockEMPZones>								lockEmpZones(Receiver::LockEmpZones);
		TCP::Channel<NET(INT32), 0, Protocol::ChannelID::UplinkBandwidth>							uplinkBandwidth(Receiver::UplinkBandwidth);
		TCP::Channel<Protocol::UnitLimits, 0, Protocol::ChannelID::UnitLimits>						unitLimits(Receiver::UnitLimits);
		TCP::Channel<Protocol::OperationLacksResources, 0, Protocol::ChannelID::OperationLacksResources>			operationLacksResources(Receiver::OperationLacksResources);
		TCP::Channel<Protocol::GameConfig,0,Protocol::ChannelID::GameConfig>						gameConfig(Receiver::GameConfig);

		TCP::Channel<NET(INT32),0,Protocol::ChannelID::TutorialAt>									tutorialAt(Receiver::TutorialAt);
		TCP::Channel<TUniqueID,0,Protocol::ChannelID::WinningAlliance>								winningAlliance(Receiver::WinningAlliance);

		namespace Statistics
		{
			TCP::Channel<Objects::Statistics::Round, 0, Protocol::ChannelID::RoundStatistics>		round(Receiver::Statistics::Round);
			TCP::Channel<Objects::Statistics::EntityType, 0, Protocol::ChannelID::EntityTypeStatistics>	entityType(Receiver::Statistics::EntityType);
			TCP::Channel<Objects::Statistics::FactionInteraction, 0, Protocol::ChannelID::FactionInteractionStatistics>	factionInteraction(Receiver::Statistics::FactionInteraction);

			DECLARE_CHANNEL(BeginStatisticsSequence);
		}


		TCP::ObjectSender<Protocol::ClientFactionRequest,Protocol::ChannelID::ClientFaction>		clientFactionRequest;
		TCP::ObjectSender<Protocol::TouchedMarker,Protocol::ChannelID::TouchMarker>					touchMarker;
		TCP::ObjectSender<Protocol::Hello,Protocol::ChannelID::Hello>								hello;
		TCP::ObjectSender<Protocol::EntityDirectCommand, Protocol::ChannelID::EntityDirectCommand>	entityDirectCommand;
		TCP::ObjectSender<Protocol::ChoiceIntersection, Protocol::ChannelID::ChoiceIntersection>	choiceIntersection;
		TCP::ObjectSender<Protocol::DropTasks, Protocol::ChannelID::DropTasks>						dropTasks;
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
		TCP::ObjectSender<Protocol::QueueResearch, Protocol::ChannelID::QueueResearch>				queueResearch;

		DECLARE_CHANNEL(MyFaction);
		DECLARE_CHANNEL(AllianceConfigChange);
		DECLARE_CHANNEL(VoteCreation);
		DECLARE_CHANNEL(VoteResult);
		DECLARE_OBJECT_SENDER(ClientVote);
		DECLARE_CHANNEL(AllianceInvite);
		DECLARE_OBJECT_SENDER(ChatMessage);
		DECLARE_OBJECT_SENDER(SaveAs);
		DECLARE_CHANNEL(KillFaction);

		DECLARE_CHANNEL(ResearchProgram);
		DECLARE_CHANNEL(ResearchFactionState);
		DECLARE_CHANNEL(ResearchQueue);
		DECLARE_OBJECT_SENDER(QueueResearch);
		DECLARE_CHANNEL(ResearchAttributeVector);
		DECLARE_CHANNEL(ResearchAttributeDeltaVector);

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
			ready.factionToken = CryptographicID();
			LogEvent(EventClass::Preset::uncommon, nullptr, "Abandoning current faction");
			Channels::clientFactionRequest.SendTo(client,ready);

			#ifdef HAS_WINDOW
				HUD::WindowStack::Show(HUD::Menu::login);
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
			LogEvent(EventClass::Preset::common, nullptr, "Attempting to log in to " + ToString(worldID) + " as AI using token: " + ready.factionToken.ToString());
		}
		elif (!forceNewFaction && accessMap.Query(worldID, ready.factionToken))
		{
			LogEvent(EventClass::Preset::common,nullptr,"Attempting to use existing access token of world "+ToString(worldID)+": "+ready.factionToken.ToString());
		}
		else
		{
						
			if (forceNewFaction)
				LogEvent(EventClass::Preset::common, nullptr, "Forcing new faction in world " + ToString(worldID));
			else
				LogEvent(EventClass::Preset::common, nullptr, "No access token found for world " + ToString(worldID));

			ready.factionToken = CryptographicID();
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
				hello = RuntimeConfig::aiMode ? Objects::Faction::nexusName : chosenNickname;
				if (RuntimeConfig::aiMode)
					chosenNickname = hello;
				//state.state = 0U;
				Channels::hello.SendTo(client,hello);	//elevate client state

				if (RuntimeConfig::aiMode)
					AttemptFactionLogin(false,Protocol::FactionType::ContinueOnly);
				//lastPong = timer.Now();




			}
			break;
			case TCP::Event::ConnectionFailed:
			{
				LogEvent(EventClass::Preset::networkError, nullptr, "Connection attempt failed");
				#ifdef HAS_WINDOW
					World::allowRecompilation = true;
					HUD::WindowStack::SignalSituationChange();
				#endif
			}
			break;
			case TCP::Event::ConnectionLost:
			{
				Receiver::scriptChangeFlags = Receiver::ScriptChangeFlags::Empty;
				RuntimeConfig::tutorialMode = false;
				if (!RuntimeConfig::aiMode)
					RuntimeConfig::serverAddress = ServerAddress();	//prevent any automatic reconnect
				String err = client.GetError(true);

				LogEvent(EventClass( EventSignificance::Uncommon, EventFlavor::Loss ), nullptr, err.IsNotEmpty() ? err : (ev == TCP::Event::ConnectionLost ? "Connection lost" : "Connection closed"));
				amOnline = false;

				FlushFaction(true);
				#ifdef HAS_WINDOW
					World::allowRecompilation = true;
					HUD::WindowStack::CloseAll();
				#endif



				if (RuntimeConfig::aiMode && RuntimeConfig::serverAddress.host.IsNotEmpty())
					application_shutting_down = true;
				#ifdef HAS_WINDOW
					HUD::Dialog::QueryNicknameIfEmpty();
				#endif

				Receiver::eventsLocked = true;


				#ifdef HAS_WINDOW
					ConfigurationMode::SetActive(false);
					HUD::WindowStack::Hide(HUD::Menu::login);
					HUD::HostList::CheckVisibility(true);
					HUD::WindowStack::SignalSituationChange();
					Tutorial::SignalEnd();
				#endif

			}
			break;
		}
	}

	void		OnSignal(UINT32 signal, TCP::Peer&origin)
	{
		switch (signal)
		{
			case Protocol::ChannelID::EndStatisticsSequence:
				Receiver::Statistics::EndSequence();
			break;
			case Protocol::ChannelID::BeginGame:
				#ifdef HAS_WINDOW
					World::allowRecompilation = true;
				#endif
				gameHasBegun = true;
				LogEvent(EventClass::Preset::uncommon,nullptr,"Game has begun");
				if (RuntimeConfig::IsInAnyCommandPhase())
					if (Animation::AllDone())
					{
						LogEvent(EventSignificance::Debug,"Begin Game. Animation done. OnPlayerControlBegin");
						PlayerControlBegin::OnPlayerControlBegin();
					}
					else
						LogEvent(EventSignificance::Debug,"Begin Game. Animation NOT done");
				#ifdef HAS_WINDOW
					if (RuntimeConfig::tutorialMode)
						Tutorial::SignalStart();
				#endif
			break;
			case Protocol::ChannelID::Pong:
				//lastPong = timer.Now();
				#ifdef HAS_WINDOW
					HUD::WindowStack::SignalSituationChange();
				#endif
				#ifdef _DEBUG
					//LogEvent(EventSignificance::Debug,nullptr,"Got pong from server");
				#endif
			break;
		}
	}

	void OnIgnorePackage(UINT32 channel,UINT32 size,TCP::Peer&origin)
	{
		LogEvent(EventClass::Preset::networkError,origin.ToString()+": Ignored package on channel #"+String(channel)+" of size "+String(size));
		//DBG_FATAL__("Ignored Package");

	}
	void OnIgnoredException(const TCodeLocation&loc, const std::exception&ex)
	{
		LogEvent(EventClass::Preset::networkError,String(loc)+": Ignored(TCP): "+ex.what());
		DBG_FATAL__("Ignored Exception");
	}

	void		Update();

	void		Setup()
	{
		LoadHosts();
		LoadWorldAccessTokens();

		client.InstallChannel(Channels::helloResponse);
		client.InstallChannel(Channels::worldData);
		client.InstallChannel(Channels::newMobileEntity);
		client.InstallChannel(Channels::mobileStateUpdate);
		client.InstallChannel(Channels::newStructure);
		client.InstallChannel(Channels::structureStateUpdate);
		client.InstallChannel(Channels::destroyEntity);
		client.InstallChannel(Channels::controlMessage);
		client.InstallChannel(Channels::clientFactionResponse);
		client.InstallChannel(Channels::MyFaction);
		client.InstallChannel(Channels::KillFaction);
		client.InstallChannel(Channels::AllianceConfigChange);
		client.InstallChannel(Channels::AllianceInvite);
		client.InstallChannel(Channels::VoteCreation);
		client.InstallChannel(Channels::VoteResult);

		client.InstallChannel(Channels::phaseChange);
		client.InstallChannel(Channels::mobileEntityMotion);
		client.InstallChannel(Channels::vagueAttackTrajectory);
		client.InstallChannel(Channels::preciseAttackTrajectory);
		client.InstallChannel(Channels::explosion);
		client.InstallChannel(Channels::entityTurretRotation);
		client.InstallChannel(Channels::resourceUpdate);
		client.InstallChannel(Channels::gameProgress);

		client.InstallChannel(Channels::createMarker);
		client.InstallChannel(Channels::markerCreationFailedLimitReached);
		client.InstallChannel(Channels::destroyMarker);
		client.InstallChannel(Channels::identifyFaction);
		client.InstallChannel(Channels::factionOnline);
		client.InstallChannel(Channels::factionOffline);
		client.InstallChannel(Channels::factionRequestedNewRound);
		client.InstallChannel(Channels::factionContinuedRound);

		client.InstallChannel(Channels::lockEmpZones);
		client.InstallChannel(Channels::uplinkBandwidth);
		client.InstallChannel(Channels::unitLimits);
		client.InstallChannel(Channels::operationLacksResources);
		client.InstallChannel(Channels::gameConfig);

		client.InstallChannel(Channels::tutorialAt);
		client.InstallChannel(Channels::winningAlliance);

		client.InstallChannel(Channels::Statistics::BeginStatisticsSequence);
		client.InstallChannel(Channels::Statistics::entityType);
		client.InstallChannel(Channels::Statistics::round);
		client.InstallChannel(Channels::Statistics::factionInteraction);

		client.InstallChannel(Channels::ResearchProgram);
		client.InstallChannel(Channels::ResearchFactionState);
		client.InstallChannel(Channels::ResearchQueue);
		client.InstallChannel(Channels::ResearchAttributeVector);
		client.InstallChannel(Channels::ResearchAttributeDeltaVector);

		

		client.OpenSignalChannel(Protocol::ChannelID::Pong,0);
		client.OpenSignalChannel(Protocol::ChannelID::BeginGame,0);
		client.OpenSignalChannel(Protocol::ChannelID::BeginEverything,0);
		client.OpenSignalChannel(Protocol::ChannelID::EndEverything,0);
		client.OpenSignalChannel(Protocol::ChannelID::EndStatisticsSequence,0);


		client.SetAsync(false);
		client.onEvent = OnEvent;
		client.onSignal = OnSignal;
		client.onIgnorePackage = OnIgnorePackage;
		TCP::onIgnoredException = OnIgnoredException;
		client.onDeserializationFailed = [](UINT32 channel, TCP::Peer&origin, const String&exceptionMessage)
		{
			LogEvent(EventSignificance::NonFatalError,"Failed to deserialize package on network channel "+String(channel)+": "+exceptionMessage);
		};

		ASSERT__(Net::initNet());
		if (RuntimeConfig::udpListen)
		{
			broadcastListenSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if (broadcastListenSocket == INVALID_SOCKET)
			{
				String error = String(Net::GetLastError());
				LogEvent(EventClass::Preset::operationAborted, nullptr, "Unable to initiate UDP listening for servers on the local area network: " + error + " (unable to create socket). Listening will be disabled.");
			}
			else
			{
				sockaddr_in me;
				memset(&me, 0, sizeof(me));

				me.sin_family = AF_INET;
				me.sin_port = htons(BROADCAST_PORT);
				me.sin_addr.s_addr = ADDR_ANY;

				int rc = ::bind(broadcastListenSocket, (const sockaddr*)&me, (int)sizeof(me));
				if (rc != 0)
				{
					String error = String(Net::GetLastError());
					LogEvent(EventClass::Preset::operationAborted,nullptr,"Unable to initiate UDP listening for servers on the local area network: "+error+" (maybe another instance of " PRODUCT_NAME " is running already?). Listening will be disabled." );
					RuntimeConfig::udpListen = false;
					closesocket(broadcastListenSocket);
				}
				else
				{
					u_long dontBlock = 1;
					rc = ioctlsocket(broadcastListenSocket, FIONBIO, &dontBlock);	//set socket to non-blocking
					ASSERT_EQUAL__(rc, 0);
				}
			}
		}


		GlobalEvents::frameBegin << Update;
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
		index_t portAt = url.Find(':');
		if (portAt != InvalidIndex)
		{
			host = url.subStringRef(0,portAt);
			StringRef conv = url.SubStringRef(portAt+1);
			if (!convert(conv.pointer(),conv.GetLength(),basePort))
			{
				throw Except::Program::ParameterFault(CLOCATION, "Unable to parse port component '" + String(conv) + "' of '"+url+"'");
			}
		}
		else
		{
			host = url;
			basePort = DEFAULT_SERVER_BASE_PORT;
		}
	}


	Buffer0<ServerInfo,Swap>	servers;
	GenericHashTable<TUniqueID,index_t>	serverMap;

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

		if (timer.CheckIfOutdatedAndUpdate(lastServerTableUpdate,0.5f) && !client.IsConnected() && !client.IsAttemptingToConnect())
		{
			#ifdef DEBUG_SERVER_LIST
				if (servers.IsEmpty())
				{
					Random rng;
					for (index_t i = 0; i < 100; i++)
					{
						auto&s = servers.Append();
						s.addresses << std::make_pair(ServerAddress("localhost"),rng.getFloat(1,200));
						s.id.Generate();
						s.info.gameInfo.activePhase = (BYTE)rng.get(RuntimeConfig::Phase::N-1);
						s.info.gameInfo.gameConfig.gameFlags = Protocol::GameFlag::AllFactionSupport;
						s.info.gameInfo.gameConfig.gameGoal = (BYTE)rng.get(RuntimeConfig::GameGoal::N -1);
						s.info.gameInfo.gameConfig.worldDiscovery = (BYTE)rng.get(Grid::WorldDiscovery::N -1);
						s.info.gameInfo.numFactionsCreated = M::Sqr(rng.get(0,16));
						s.info.gameInfo.numFactionsOnline = rng.get(s.info.gameInfo.numFactionsCreated);
						s.info.gameInfo.roundNumber = M::Sqr(M::Sqr(rng.get(8)));
						s.info.gameInfo.worldID.Generate();
						s.info.hostInfo.hostID = s.id;
						s.info.hostInfo.serverBasePort = DEFAULT_SERVER_BASE_PORT;
						s.rightVersion = true;
						s.selected = false;
					}
				}
			#else

				servers.Clear();
				serverMap.Clear();

				foreach (ServerDiscovery::attempts,_a)
				{
					const auto&a = **_a;
					if (a.hasContact && (a.hasInfo || a.protocolIncompatible))
					{
						index_t at;
						if (!serverMap.Query(a.lastInfo.hostInfo.hostID,at))
						{
							at = servers.Count();
							servers.Append();
							servers.Last().haveToken = 	a.protocolIncompatible ? false : accessMap.IsSet(a.lastInfo.gameInfo.worldID);
							servers.Last().protocolIncompatible = a.protocolIncompatible;
							servers.Last().id = a.protocolIncompatible ? TUniqueID::Empty() : a.lastInfo.hostInfo.hostID;
							servers.Last().rightVersion = a.rightVersion && !a.protocolIncompatible;
							//if (!a.protocolIncompatible)
							serverMap.Set(servers.Last().id,at);
						
						}
						ServerInfo&info = servers[at];

						if (info.addresses.IsNotEmpty())
						{
							if (info.addresses.Last().serverInfo.gameInfo.worldID != a.lastInfo.gameInfo.worldID)
							{
								LogEvent(EventClass::Preset::error,"Received mismatching world IDs from host "+info.id.ToNiceString()+". Ignoring address "+ToString(a.address));
								continue;
							}
						}

						Host&host = info.addresses.Append();
						host.address = a.address;
						host.serverInfo = a.lastInfo;
						host.ping = a.lastPing;
					}
				}
			#endif
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

	switch (c.newPhase)
	{
		case RuntimeConfig::Phase::AnimationPhase:
			#ifdef HAS_WINDOW
				Objects::Renderer::ClearSmokeTrails();
				HUD::HexGrid::EventLog::Clear();
				if (TimeLapse::enabled)
					InputControl::saveScreenshot = true;
			#endif
			if (RuntimeConfig::aiMode)
				AI::vehicles.Clear();	//whichever is left, cannot be computed
			Animation::atTime = 0.f;
			Objects::ForeachMobileWorldEntity([](const Objects::PMobileEntity&m)
			{
				m->isInMovementStream = false;	//just in case
			},false);
			if (c.timeFactor > 0)
				animationTimeFactor = c.timeFactor;
		break;
		case RuntimeConfig::Phase::CommandPhase:
			roundTimer = c.timeFactor;
			RuntimeConfig::roundNumber = c.roundNumber;
			RuntimeConfig::phase.Load(c.newPhase);

			Objects::ForeachMobileWorldEntity([](const Objects::PWorldEntity&e)
			{
				if (!e->isOwnedByMe)
					return;
				Objects::Marker *m = Objects::Marker::Find(e->location);
				if (m)
					ObjectHelper::SignalMarkerDeletion(m->id);
			},false);

			#ifdef HAS_WINDOW
				Objects::structures.Foreach([](const Objects::PStructure&s)
				{
					if (s->isOwnedByMe && s->IsWorkingStructure() )
					{
						Objects::PWorkingStructure ws = To<Objects::WorkingStructure>::Cast(s);
						if (ws->AwaitsUserInteraction())
							Tutorial::ShowMessage("Interaction");
					}
				});

				if (TimeLapse::enabled)
					InputControl::saveScreenshot = true;
			#endif
		break;
	}
	#ifdef HAS_WINDOW
		Tutorial::lastDelayingEvent = timer.Now();
	#endif
	RuntimeConfig::phase.Load(c.newPhase);
	if (RuntimeConfig::IsInAnyCommandPhase())
		newRoundRequested = false;
	RuntimeConfig::roundNumber = c.roundNumber;
	phaseStarted = timer.Now();
	#ifdef HAS_WINDOW
		Objects::UpdateChoices(ControlEnabled());
		HUD::HexGrid::RefreshContacts();
	#endif

	//while (Objects::Network::records.IsNotEmpty() && timer.GetSecondsSince(Objects::Network::records.Peek().recorded) > 10)
	//	Objects::Network::records.EraseFront();
	LogEvent(EventSignificance::Common,nullptr,"Server phase changed to "+String(RuntimeConfig::phase.ToString())+" ("+String(c.timeFactor)+") @"+String(Animation::atTime));
	if (RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase)
	{
		static Buffer0<Objects::Marker> outdated;
		ClientGrid::CheckSensory();
		Objects::Marker::GetOutdated(outdated,RuntimeConfig::roundNumber,RuntimeConfig::aiMode ? 100 : 10);
		foreach(outdated, o)
		{
			
			LogEvent(EventSignificance::Debug,nullptr,"Deleting marker "+o->GetName()+" since it's outdated (at age "+String(o->age)+")");
			ObjectHelper::SignalMarkerDeletion(o->id);
			//Network::Channels::destroyMarker.SendTo(Network::client, *o);
		}

		LogEvent(EventClass::Preset::important, nullptr, "Begin round " + RuntimeConfig::roundNumber.ToString());


	}
	if (gameHasBegun)
		for (const auto&f : Objects::Faction::allKnown)
		{
			f->requestedNewRound = false;
			f->roundChangeNotifications = 0;
		};
	if (RuntimeConfig::IsInAnyCommandPhase())
	{
		if (gameHasBegun)
		{
			if (Animation::AllDone())
			{
				LogEvent(EventSignificance::Debug,"Command phase. Game has begun. Animation done. OnPlayerControlBegin");
				PlayerControlBegin::OnPlayerControlBegin();
			}
			else
			{
				LogEvent(EventSignificance::Debug,"Command phase. Game has begun. Animation NOT done");
			}
		}
		else
			LogEvent(EventSignificance::Debug,"Command phase. Game has NOT begun");
	}
	else
		if (RuntimeConfig::phase == RuntimeConfig::Phase::EndOfGame)
		{
			#ifdef HAS_WINDOW
				HUD::Statistics::Update();
				HUD::Statistics::Show();
			#endif
		}

	#ifdef HAS_WINDOW
		HUD::WindowStack::SignalSituationChange();
	#endif
}


void	Network::Receiver::LockEmpZones(NET(UINT16)&zones)
{
	Grid::empZonesLocked = zones;
	Grid::empZonesUnlocked = ~zones;

	#ifdef HAS_WINDOW
		VisualTerrain::Invalidate();
	#endif
	#ifndef NO_SATELLITES
		Grid::visibleOrbitGrid.Invalidate();
	#endif
}


void	Network::Receiver::FactionOnline(NET(TUniqueID)&id)
{
	Objects::OwnerParameter faction = Network::ResolveFaction(id);
	faction->online = true;
	#ifdef HAS_WINDOW
		HUD::FactionTable::Update();
	#endif
}
void	Network::Receiver::FactionOffline(NET(TUniqueID)&id)
{
	Objects::OwnerParameter faction = Objects::Faction::FindByID(id);
	if (faction)
		faction->online = false;
	#ifdef HAS_WINDOW
		HUD::FactionTable::Update();
	#endif
}
void	Network::Receiver::FactionRequestedNewRound(NET(TUniqueID)&id)
{
	Objects::OwnerParameter faction = Network::ResolveFaction(id);
	faction->requestedNewRound = true;
	faction->roundChangeNotifications++;
	LogEvent(faction->roundChangeNotifications < 3 ? EventSignificance::Uncommon : EventSignificance::Common,nullptr,faction->GetName()+" is done thinking");
	#ifdef HAS_WINDOW
		HUD::FactionTable::Update();
		HUD::HexGrid::RefreshButtonState();
	#endif
}

void	Network::Receiver::FactionContinuedRound(NET(TUniqueID)&id)
{
	Objects::OwnerParameter faction = Network::ResolveFaction(id);
	faction->requestedNewRound = false;
	faction->roundChangeNotifications++;
	LogEvent(faction->roundChangeNotifications < 3 ? EventSignificance::Uncommon : EventSignificance::Common,nullptr,faction->GetName()+" is thinking some more");
	#ifdef HAS_WINDOW
		HUD::FactionTable::Update();
		HUD::HexGrid::RefreshButtonState();
	#endif
}



void	Network::Receiver::IdentifyFaction(Protocol::IdentifyFaction&id)
{
	Objects::OwnerParameter faction = Network::ResolveFaction(id.id, id.allianceID);
	faction->name = id.name;
	faction->color = id.color;
	#ifdef HAS_WINDOW
		HUD::FactionTable::Update();
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
			//index_t best = 0;
			//float bestPing = srv->addresses.First().ping;
			index_t found = InvalidIndex;
			for (index_t i = 0; i < srv->addresses.Count(); i++)
				if (srv->addresses[i].address == RuntimeConfig::serverAddress)
				{
					found = i;
					break;
				}
			ASSERT__(found != InvalidIndex);
			srv->selected = true;

			Network::ConnectTo(srv->addresses[found]);

			lastAttempt = timer.Now();
			delayCounter = 0;
			return true;
		}

	}
	return false;
}


const String Network::allowedChatCharacters = "abcdefghijklmnopqrstuvwxyzäöüABCDEFGHIJKLMNOPQRSTUVWXYZÄÖÜ0123456789_-:; <>!'\"'$%^&*()=+.,/\\#@?";

void	Network::SignalProtocolDisruption(const String&message)
{
	LogEvent(EventClass::Preset::error,message);
	if (client.IsConnected())
	{
		#ifdef HAS_WINDOW
			HUD::Dialog::ShowMessage("Protocol disruption detected. Disconnecting from server.");
		#endif
		client.Disconnect();
	}
}



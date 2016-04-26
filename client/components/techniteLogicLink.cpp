#include <global_root.h>
#include "techniteLogicLink.h"
#include "network.h"
#include <general/monitoredProcess.h>
#include <io/file_system.h>
#include "../../shared/runtimeConfig.h"
#include <objects/visualSelection.h>

#include "dialog.h"
#include "ui.h"

extern void	TransmitRoundEndRequest();

extern bool techniteSpawnMode;

namespace TechniteLogicLink
{
	static EventClass linkError = EventClass( EventSignificance::NonFatalError, EventFlavor::ServerPrivate );
	static EventClass event = EventClass( EventSignificance::Common, EventFlavor::ServerPrivate );
	static EventClass uncommonEvent = EventClass( EventSignificance::Uncommon, EventFlavor::ServerPrivate );

	PathString autoStartClientPath="TechniteLogic/Client.exe";
	bool autoStartClient=true;
	static bool isActive = false,sessionStarted=false;
	static TUniqueID	lastFactionID;
	static BYTE lastTechniteGridID = 0;
	
	UINT16 initialServerPortNumber = 0,
			actualServerPortNumber = 0;

	static const String DialogName = "TechniteFactionLink";

	#ifdef HAS_WINDOW
		Dialog::WDialog		connectionNotification;
	#endif
	count_t numClients = 0;

	static bool executed = false, haveAllTechnites = true, logicIsDone = false;


	GenericHashTable<Grid::HCellIdU,Queue<DebugMessage>,Grid::HCellIdU>	debugMessageTable;


	class PeerAttachment : public TCP::Client::Attachment
	{
	public:
	};

	typedef std::shared_ptr<PeerAttachment>	PPeerAttachment;
	static TCP::Server		logicServer;
	static Buffer<Technites::Technite::TCompactState>	recordedHistory;	//all technite states received since last flush

	TechniteProtocol::WorldInfo lastWorldInfo = TechniteProtocol::WorldInfo(0);

	void		StartSession(const TUniqueID&factionID, BYTE techniteGridID)
	{
		sessionStarted = true;
		lastFactionID = factionID;
		lastTechniteGridID = techniteGridID;
		TechniteProtocol::BeginSession session;
		session.factionID = factionID.ToString(TUniqueID::NumBytes);
		session.techniteGridID = lastTechniteGridID;
		session.roundNumber = RuntimeConfig::roundNumber;
		session.techniteSubRoundNumber = RuntimeConfig::techniteSubRoundNumber;
		Channels::beginSession.SendTo(logicServer,session,1);
	}

	void		SetWorldInfo(Grid::Content coreContent)
	{
		lastWorldInfo = TechniteProtocol::WorldInfo(  coreContent );
		Channels::worldInfo.SendTo(logicServer,lastWorldInfo,1);
	}


	static GenericHashTable<TUniqueID,TechniteProtocol::GameObject,TUniqueID,Copy,Swap>	gameObjects,storedGameObjects,ghostGameObjects;
	static GenericHashTable<TUniqueID,TechniteProtocol::ControlMarker,TUniqueID>		controlMarkers;

	static UINT32 EncodeLocation(const Grid::LCellIdI&location)
	{
		Technites::Technite::CompressedLocation loc;
		loc = Grid::HCellIdU(Grid::low.grid.GetCell(location.stackID).higherEquivalent, 
							Grid::CellStack::ToLayer(location.height));
		return loc.GetData();
	}



	static void FillIn(const Objects::PWorldEntity&e, TechniteProtocol::GameObject&obj)
	{
		obj.birthRound = RuntimeConfig::roundNumber;	//too lazy
		obj.height = (BYTE)e->GetHeight();
		obj.id.location = EncodeLocation(e->location);
		obj.isBroad = e->IsBroad();
		obj.isMine = e->isOwnedByMe;
		obj.className = e->GetTypeName();
		if (e->IsVehicle())
			obj.id.type = (BYTE)TechniteProtocol::GameObjectType::Vehicle;
		#ifndef NO_SATELLITES
			elif (e->IsSatellite())
				obj.id.type = (BYTE)TechniteProtocol::GameObjectType::Satellite;
		#endif
		elif (e->IsStructure())
			obj.id.type = (BYTE)TechniteProtocol::GameObjectType::Building;
		else
			FATAL__("Cannot transfer object data to technite logic client. Unsupported type "+e->GetTypeName());
	}


	bool		LogicIsDone()	{return logicIsDone || recordedHistory.IsEmpty();}


	void		Handle(const Protocol::DestroyEntity&entity)
	{
		if (!isActive)
			return;

		TechniteProtocol::GameObject obj;
		if (gameObjects.QueryAndUnset(entity.id,obj))
		{
			Channels::removeGameObject.SendTo(logicServer,obj.id,1);
		}
		else
		{
			if (!storedGameObjects.Unset(entity.id))
				LogEvent(linkError,nullptr,"TechniteLogicLink: Unable to destroy entity "+entity.id.value.ToString()+": Not known");
		}
	}


	void		Handle(const Protocol::NewMobileEntity&mobile)
	{
		if (!isActive)
			return;


		const Protocol::MyMobileEntity*m = 
			mobile.state.myMobileEntity.Get0();

		TechniteProtocol::GameObject obj;
		obj.birthRound = RuntimeConfig::roundNumber;
		obj.id.location	= EncodeLocation(mobile.state.entityState.location);
		obj.id.isGhost = false;
		obj.isMine = mobile.owner == Network::myID;

		
		if (mobile.turretName != ConstName::Empty)
		{
			obj.height = 1;
			obj.isBroad = true;
			const Objects::Component::VehicleBase*b = Objects::Component::GetVehicleBaseByName(mobile.baseName);
			const Objects::Component::Turret*t = Objects::Component::GetTurretByName(mobile.turretName);
			ASSERT_NOT_NULL1__(b,(int)mobile.baseName.value);
			ASSERT_NOT_NULL1__(t,(int)mobile.turretName.value);
			obj.className = Objects::Vehicle::GetTypeName(b,t);
			obj.id.type = (BYTE)TechniteProtocol::GameObjectType::Vehicle;
		}
		else
		{
			Objects::PWorldEntity e = Objects::GetNonVehiclePrototypeByName(mobile.baseName);
			ASSERT1__(e,(int)mobile.baseName.value);
			ASSERT1__(e->IsMobile(),e->GetTypeName());
			obj.className = e->GetTypeName();
			obj.id.type = (BYTE)TechniteProtocol::GameObjectType::Satellite;
			obj.height = e->GetHeight();
			obj.isBroad = e->IsBroad();
		}

		if (m)
		{
			if (m->storedIn.Get0())
			{
				LogEvent(event,"Registering new stored mobile entity '"+mobile.state.entityState.id.value.ToString()+"' at "+mobile.state.entityState.location.ToString());
				storedGameObjects.Set(mobile.state.entityState.id,obj);
				return;
			}
		}
		LogEvent(event,"Registering new mobile entity '"+mobile.state.entityState.id.value.ToString()+"' at "+mobile.state.entityState.location.ToString());
		gameObjects.Set(mobile.state.entityState.id,obj);
		Channels::createGameObject.SendTo(logicServer,obj,1);
	}

	void		Handle(const Protocol::EntityState&state)
	{
		if (state.location == Grid::LCellIdI::Invalid)
			return;

		TechniteProtocol::GameObject*obj = gameObjects.QueryPointer(state.id);
		if (!obj)
		{
			FATAL__("Unable to find game object by id (for motion): "+state.id.value.ToString());
			return;
		}
		LogEvent(event,"Registering motion of entity '"+state.id.value.ToString()+"' to "+state.location.ToString());

		Channels::removeGameObject.SendTo(logicServer,obj->id,1);
		obj->id.location = EncodeLocation(state.location);
		Channels::createGameObject.SendTo(logicServer,*obj,1);
	}

	void		Handle(const Protocol::MobileState&state)
	{
		if (!isActive)
			return;
		const Protocol::MyMobileEntity*m = state.myMobileEntity.Get0();

		bool isStored = storedGameObjects.IsSet(state.entityState.id);
		if (isStored && (m && m->storedIn.Get0()))
			return;
		if (isStored)
		{
			TechniteProtocol::GameObject obj;
			storedGameObjects.QueryAndUnset(state.entityState.id,obj);

			if (state.entityState.location != Grid::LCellIdI::Invalid)
			{
				obj.id.location = EncodeLocation(state.entityState.location);
			}
			Channels::createGameObject.SendTo(logicServer,obj,1);

			gameObjects.Set(state.entityState.id,obj);
			LogEvent(event,"Checked out entity '"+state.entityState.id.value.ToString()+"' to "+state.entityState.location.ToString());
			return;
		}
		if (m && m->storedIn.Get0())
		{
			TechniteProtocol::GameObject obj;
			ASSERT1__(gameObjects.QueryAndUnset(state.entityState.id,obj),state.entityState.id.value.ToString());
			Channels::removeGameObject.SendTo(logicServer,obj.id,1);
			storedGameObjects.Set(state.entityState.id,obj);
			LogEvent(event,"Checked in entity '"+state.entityState.id.value.ToString()+"' to "+state.entityState.location.ToString());
			return;
		}
		Handle(state.entityState);
	}

	void		Handle(const Protocol::StructureConstruction&struc)
	{
		if (!isActive)
			return;
		LogEvent(event,"Registering new structure entity '"+struc.state.entityState.id.value.ToString()+"' at "+struc.state.entityState.location.ToString());

		TechniteProtocol::GameObject obj;
		obj.birthRound = RuntimeConfig::roundNumber;
		obj.id.location	= EncodeLocation(struc.state.entityState.location);
		obj.isMine = struc.owner == Network::myID;

		
		Objects::PWorldEntity e = Objects::GetPrototypeByName(struc.typeName);
		ASSERT1__(e,(int)struc.typeName.value);
		obj.height = e->GetHeight();
		obj.isBroad = e->IsBroad();
		obj.className = e->GetTypeName();
		obj.id.type = (BYTE)TechniteProtocol::GameObjectType::Building;
		obj.id.isGhost = false;

		gameObjects.Set(struc.state.entityState.id,obj);
		Channels::createGameObject.SendTo(logicServer,obj,1);
	}

	void		Handle(const Protocol::StructureState&state)
	{
		if (!isActive)
			return;
		Handle(state.entityState);
	}

	void		Handle(const Protocol::MobileEntityMotion&motion)
	{
		if (!isActive)
			return;
		TechniteProtocol::GameObject*obj = gameObjects.QueryPointer(motion.id);
		if (!obj)
		{
			FATAL__("Unable to find game object by id (for animated motion): "+motion.id.value.ToString());
			return;
		}
		LogEvent(event,"Registering animated motion of entity '"+motion.id.value.ToString()+"' to "+motion.toLocation.ToString());

		Channels::removeGameObject.SendTo(logicServer,obj->id,1);
		obj->id.location = EncodeLocation(motion.toLocation);
		Channels::createGameObject.SendTo(logicServer,*obj,1);
	}




	static void SendWorldDataTo(TCP::Peer&peer)
	{
		LogEvent(event,peer.ToString()+ ": Technite logic: Sending world data to client");
		Protocol::WorldDelta delta;
		TechniteProtocol::GridDelta outDelta;
		for (int i = 0; i < BaseGrid::NumFamilies; i++)
		{
			count_t range = Grid::high.grid.CountFamilyMembers(i);
			index_t offset = Grid::high.grid.GetFamilyOffset(i);

			Protocol::GenerateWorldDelta(nullptr,&Grid::actualWorld,delta,(BYTE)i,true,false,Protocol::WorldDeltaType::GlobalCopy);

			outDelta.nodeCount = (UINT32)range;
			outDelta.nodeOffset = (UINT32)offset;
			outDelta.data0.adoptData(delta.data0);
			outDelta.data1.adoptData(delta.data1);
			Channels::gridDelta.SendTo(peer,outDelta);
		}
	}
	static void SendWorld(TCP::Peer&peer)
	{
		LogEvent(event,peer.ToString()+ ": Technite logic: Sending grid layout to client");
		TechniteProtocol::NodeChunk chunk;
		for (int i = 0; i < BaseGrid::NumFamilies; i++)
		{
			count_t range = Grid::high.grid.CountFamilyMembers(i);
			index_t offset = Grid::high.grid.GetFamilyOffset(i);
			chunk.isLast = i+1 == BaseGrid::NumFamilies;
			chunk.nodes.SetSize(range);
			for (index_t j = 0; j < range; j++)
			{
				auto&out = chunk.nodes[j];
				const auto&in = Grid::high.grid.GetCell(j+offset);

				out.neighbors.SetSize(in.neighborhood.numNeighbors);
				out.stackBase = in.center * Grid::ToRadius(Grid::CellStack::BottomHeight);
				out.stackDirection = in.center;
				for (int k = 0; k < in.neighborhood.numNeighbors; k++)
					out.neighbors[k] = in.neighborhood.neighborID[k];
			}
			Channels::nodeChunk.SendTo(peer,chunk);
		}
		TechniteProtocol::GridConfig cfg;
		cfg.heightPerLayer = Grid::SingleLayerHeight;
		cfg.numLayersPerStack = Grid::CellStack::Range;
		cfg.matterYieldByContentType.SetSize(Grid::Content::NumberOfPossibilities);
		cfg.matterDegradationTable.SetSize(Grid::Content::NumberOfPossibilities);
		cfg.energyYieldAtLayer.SetSize(Grid::CellStack::Range);
		cfg.initialTTLAtLayer.SetSize(Grid::CellStack::Range);
		static_assert(Grid::Content::NumberOfPossibilities == ARRAYSIZE(Technites::Technite::MatterYield),"Field size mismatch");
		for (index_t i = 0; i < Grid::Content::NumberOfPossibilities; i++)
		{
			cfg.matterYieldByContentType[i] = Technites::Technite::MatterYield[i];
			cfg.matterDegradationTable[i] = Technites::Technite::DegradeTo[i];
		}
		for (UINT32 layer = 0; layer < Grid::CellStack::Range; layer++)
		{
			int h = Grid::CellStack::ToHeight(layer);
			cfg.energyYieldAtLayer[layer] = Technites::Technite::GetLightEnergyYieldAtHeight(h);
			cfg.initialTTLAtLayer[layer] = Technites::Technite::GetInitialTTLAtLayer(layer);
		}


		Channels::gridConfig.SendTo(peer,cfg);

		if (lastWorldInfo != 0)
			Channels::worldInfo.SendTo(peer,lastWorldInfo);


		SendWorldDataTo(peer);


	}


	void		SignalTerrainReset()
	{
		executed = false;
		if (!logicServer.HasAnyClients())
			return;
		LogEvent(event,"Technite logic: Sending world data to all clients");
		Protocol::WorldDelta delta;
		TechniteProtocol::GridDelta outDelta;
		for (int i = 0; i < BaseGrid::NumFamilies; i++)
		{
			count_t range = Grid::high.grid.CountFamilyMembers(i);
			index_t offset = Grid::high.grid.GetFamilyOffset(i);

			Protocol::GenerateWorldDelta(nullptr,&Grid::actualWorld,delta,(BYTE)i,true,false,Protocol::WorldDeltaType::GlobalCopy);

			outDelta.nodeCount = (UINT32)range;
			outDelta.nodeOffset = (UINT32)offset;
			outDelta.data0.adoptData(delta.data0);
			outDelta.data1.adoptData(delta.data1);

			logicServer.VisitAllPeers([&outDelta](const TCP::PPeer&peer)
			{
				if (peer->userLevel > 0)
					Channels::gridDelta.SendTo(peer,outDelta);
			});
		}
		debugMessageTable.Clear();
		recordedHistory.Clear();
		Protocol::TechniteChunk chunk;
		chunk.flags = Protocol::TechniteChunkFlags::IsFirst | Protocol::TechniteChunkFlags::IsLast;
		logicServer.VisitAllPeers([&chunk](const TCP::PPeer&peer)
		{
			if (peer->userLevel > 0)
				Channels::techniteStateChunk.SendTo(peer,chunk);
		});
	}


	void		ExecuteLogicWhenAllTechnitesAreThere(UINT32 roundNumber,UINT32 techniteSubRoundNumber, bool reExecution/*=false*/)
	{
		ASSERT_EQUAL__(executed, reExecution);
		if (haveAllTechnites)
		{
			//DBG_ASSERT__(RuntimeConfig::IsInAnyCommandPhase());
			//static UINT32 lastRound = UNSIGNED_UNDEF;
			//static UINT32 lastSubRound = UNSIGNED_UNDEF;
			//DBG_ASSERT__(lastRound != roundNumber || lastSubRound != techniteSubRoundNumber);
			//lastRound = roundNumber;
			//lastSubRound = techniteSubRoundNumber;
			LogEvent(event,"ExecuteLogic("+String(roundNumber)+","+String(techniteSubRoundNumber)+")");
			executed = true;
			TechniteProtocol::InstructTechnites tech;
			tech.roundNumber = roundNumber;
			tech.techniteSubRoundNumber = techniteSubRoundNumber;
			Channels::instructTechnites.SendTo(logicServer,tech,1);
		}
		else
		{
			LogEvent(event,"Not executing logic("+String(roundNumber)+","+String(techniteSubRoundNumber)+"): don't have all technites");
			executed = true;
		}
	}


	namespace Receiver
	{
		void TechniteInstructionChunk(Protocol::TechniteInstructions&chunk)
		{
			//re-route to server as is
			LogEvent(event,"Forwarding technite instruction chunk to server, containing "+String(chunk.instructions.Count())+" technites, starting from "+String(chunk.offset));
			DBG_ASSERT__(RuntimeConfig::IsInAnyCommandPhase());
			Network::Channels::techniteInstructions.SendTo(Network::client,chunk);
			
			if (Objects::Faction::mine && Objects::Faction::mine->IsTechniteFaction())
			{
				Technites::PFaction pf = To<Technites::Faction>::Cast(Objects::Faction::mine);

				count_t end = chunk.instructions.Count() + chunk.offset;
				if (end <= pf->technites.Count())
					for (index_t i = 0; i < chunk.instructions.Count(); i++)
					{
						index_t at = i+chunk.offset;
						auto&tech = pf->technites[at];
						tech.Set(chunk.instructions[i]);
						Grid::high.family[Grid::high.grid.GetCell(tech.location.GetStackID()).family].SetOutdated(0.f);
						#ifdef HAS_WINDOW
						if (Objects::techniteIsSelected && tech.location == Objects::selectedTechnite.location)
						{
							Objects::Select(tech,Network::ControlEnabled());
						}
						#endif
						
					}
				else
					LogEvent(event,"Local technite field ("+String(pf->technites.Count())+") does not support that many technite instructions ("+String(end)+"). How weird");
			}

			if (chunk.offset + chunk.instructions.Count() == recordedHistory.Count())
			{
				LogEvent(event,"All technites instructed");
				
				logicIsDone = true;

				if (Network::amTechniteFaction && (!Network::amRegularFaction || Network::newRoundRequested || RuntimeConfig::phase == RuntimeConfig::Phase::TechniteCommandPhase))
				{
					//auto-new-round, if set. Not sure if this should be set
					if (!techniteSpawnMode)
						TransmitRoundEndRequest();
				}
				else
				{
					bool brk = true;

				}
				#ifdef HAS_WINDOW
				Dialog::SignalSituationChange();
				UI::HexGrid::RefreshButtonState();
				#endif
			}
		}


		void TechniteColorChunk(TechniteProtocol::TechniteColorChunk&colorChunk)
		{
			if (!Network::amTechniteFaction)
				return;

		//	colorChunk.Deserialize(test);


			Technites::PFaction tf = To<Technites::Faction>::Cast(Objects::Faction::mine);
			foreach (colorChunk.colors,c)
				if (c->index >= tf->technites.Count())
					return;
				else
					tf->SetTechniteColor(c->index,c->color);


		}

		static String CompileProtocolString()
		{
			return "Aquinas v1.7." + String(TechniteProtocol::ChannelID::Count);
		}


		void Ready(Package::NetString<256>&version, TCP::Peer&peer)
		{
			String expect = CompileProtocolString();
			if (version != expect)
			{
				String msg = "TechniteLogic: Logic client version mismatch (server uses '"+expect+"', client sent '"+version+"')";
				LogEvent(EventClass::Preset::operationAborted,nullptr,msg);
				msg = msg.subString(0,256);
				Channels::error.SendTo(peer,msg);
				return;
			}
			LogEvent(event,peer.ToString()+": Ready");

			SendWorld(peer);

			//if (Network::amTechniteFaction)
			{
				//Technites::PFaction tf = To<Technites::Faction>::Cast(Objects::Faction::mine);

				Protocol::TechniteChunk chunk;
				//chunk.first = true;

				count_t numChunks = recordedHistory.Count() / Protocol::MaxTechnitesPerChunk;
				if (recordedHistory.Count() % Protocol::MaxTechnitesPerChunk)
					numChunks++;
				LogEvent(event,"Sending "+String(numChunks)+" technite state chunks to logic client");
				index_t offset = 0;
				for (index_t i = 0; i < numChunks; i++)
				{
					chunk.flags = 0;
					if (i == 0)
						chunk.flags.value |= Protocol::TechniteChunkFlags::IsFirst;
					if (i+1 == numChunks && haveAllTechnites)
						chunk.flags.value |= Protocol::TechniteChunkFlags::IsLast;
					count_t numTechs = std::min(Protocol::MaxTechnitesPerChunk,recordedHistory.Count() - offset);
					chunk.technites.SetSize(numTechs);
					chunk.technites.copyFrom(recordedHistory+offset,numTechs);

					LogEvent(event," Sending chunk containing "+String(numTechs)+" technites, starting from "+String(offset)+", flagged as ="+String(chunk.flags));
					Channels::techniteStateChunk.SendTo(peer,chunk);

					offset += numTechs;
				}
			}

			{
				gameObjects.VisitAllValues([&peer](const TechniteProtocol::GameObject&obj)
				{
					Channels::createGameObject.SendTo(peer,obj);
					DBG_ASSERT__(!obj.id.isGhost);
				});
				ghostGameObjects.VisitAllValues([&peer](const TechniteProtocol::GameObject&obj)
				{
					Channels::createGameObject.SendTo(peer,obj);
					DBG_ASSERT__(obj.id.isGhost);
				});
				controlMarkers.VisitAllValues([&peer](const TechniteProtocol::ControlMarker&mrk)
				{
					Channels::createControlMarker.SendTo(peer,mrk);
				});

			}


			LogEvent(event," Elevating user level");
			peer.userLevel = 1;

			numClients ++;

			#ifdef HAS_WINDOW
			if (!connectionNotification.expired())
			{
				Dialog::RemoveDialog(connectionNotification.lock());
				ASSERT__(connectionNotification.expired());
				connectionNotification.reset();
			}
			#endif

			if (sessionStarted)
			{
				TechniteProtocol::BeginSession session;
				session.factionID = lastFactionID.ToString(TUniqueID::NumBytes);
				session.techniteGridID = lastTechniteGridID;
				session.roundNumber = RuntimeConfig::roundNumber;
				session.techniteSubRoundNumber = RuntimeConfig::techniteSubRoundNumber;
				Channels::beginSession.SendTo(logicServer,session,1);
			}


			if (haveAllTechnites && executed)
				ExecuteLogicWhenAllTechnitesAreThere(RuntimeConfig::roundNumber, RuntimeConfig::techniteSubRoundNumber,true);

		}

		void TechniteDebugMessage(TechniteProtocol::TechniteDebugMessage&message)
		{
			if (!Network::amTechniteFaction)
				return;

			Technites::PFaction tf = To<Technites::Faction>::Cast(Objects::Faction::mine);
			
			DebugMessage msg;
			msg.message = message.message;
			msg.roundNumber = RuntimeConfig::roundNumber;
			msg.subRoundNumber = RuntimeConfig::techniteSubRoundNumber;
			msg.received = timer.Now();
			

			auto&list = debugMessageTable.Set(Technites::Technite::CompressedLocation(message.location.value).GetU());
			
			while (list.GetLength() > 14)
				list.EraseFront();
			list.Push(msg);
		}

		
	}


	bool		HasDebugMessage(const Grid::HCellIdU&key)
	{
		return debugMessageTable.IsSet(key);
	}

	void		ForeachDebugMessage(const Grid::HCellIdU&key,const std::function<void(const DebugMessage&)>&f)
	{
		auto*list = debugMessageTable.QueryPointer(key);
		if (!list)
			return;
		foreach (*list,entry)
			f(*entry);
	}




	namespace Channels
	{
		//TCP::Channel<Package::NetString<64>,0,
		//			TechniteProtocol::ChannelID::Authenticate>						authenticate(Receiver::Authenticate);
		TCP::Channel<Protocol::TechniteInstructions,1,
					TechniteProtocol::ChannelID::TechniteInstructionChunk>			techniteInstructionChunk(Receiver::TechniteInstructionChunk);
		TCP::Channel<Package::NetString<256>,0,
					TechniteProtocol::ChannelID::Ready>								ready(Receiver::Ready);
		TCP::Channel<TechniteProtocol::TechniteColorChunk,1,
					TechniteProtocol::ChannelID::TechniteColorChunk>				techniteColorChunk(Receiver::TechniteColorChunk);
		TCP::Channel<TechniteProtocol::TechniteDebugMessage,1,
					TechniteProtocol::ChannelID::TechniteDebugMessage>				techniteDebugMessage(Receiver::TechniteDebugMessage);



		TCP::ObjectSender<Protocol::TechniteChunk,
					TechniteProtocol::ChannelID::TechniteStateChunk>				techniteStateChunk;
		TCP::ObjectSender<TechniteProtocol::GridConfig,
					TechniteProtocol::ChannelID::GridConfig>						gridConfig;
		TCP::ObjectSender<TechniteProtocol::NodeChunk,
					TechniteProtocol::ChannelID::NodeChunk>							nodeChunk;
		TCP::ObjectSender<TechniteProtocol::GridDelta,
					TechniteProtocol::ChannelID::GridDelta>							gridDelta;
		TCP::ObjectSender<TechniteProtocol::WorldInfo,
					TechniteProtocol::ChannelID::WorldInfo>							worldInfo;
		TCP::ObjectSender<Package::NetString<256>,
					TechniteProtocol::ChannelID::Error>								error;
		TCP::ObjectSender<TechniteProtocol::InstructTechnites,
					TechniteProtocol::ChannelID::InstructTechnites>					instructTechnites;


		TCP::ObjectSender<TechniteProtocol::GameObject,
					TechniteProtocol::ChannelID::CreateGameObject>					createGameObject;
		TCP::ObjectSender<TechniteProtocol::GameObjectID,
					TechniteProtocol::ChannelID::RemoveGameObject>					removeGameObject;
		TCP::ObjectSender<TechniteProtocol::ControlMarker,
					TechniteProtocol::ChannelID::CreateControlMarker>				createControlMarker;
		TCP::ObjectSender<NET(UINT32),
					TechniteProtocol::ChannelID::RemoveControlMarker>				removeControlMarker;
		TCP::ObjectSender<TechniteProtocol::BeginSession,
					TechniteProtocol::ChannelID::BeginSession>						beginSession;

	}


	static MonitoredProcess	logicProcess;

	//static index_t			techniteGenerationIndex = 0;


	static void Init(TCP::Peer&peer)
	{
		PPeerAttachment attachment(new PeerAttachment());

		peer.attachment = attachment;
	}

	#ifdef HAS_WINDOW
	void RefreshUserNotification();
	#endif

	void		OnEvent(TCP::event_t ev, TCP::Peer&peer)
	{
		switch (ev)
		{
			case TCP::Event::ConnectionEstablished:
			{
				LogEvent(uncommonEvent, "TechniteLogic: Peer connected: "+peer.ToString());
				Init(peer);

			}
			break;
			case TCP::Event::ConnectionFailed:
			{
			}
			break;
			case TCP::Event::ConnectionLost:
			case TCP::Event::ConnectionClosed:
			{
				if (peer.userLevel > 0)
				{
					numClients--;
					#ifdef HAS_WINDOW
						if (numClients == 0 && !autoStartClient)
						{
							RefreshUserNotification();
						}
					#endif
				}
				LogEvent(uncommonEvent, nullptr, "TechniteLogic: Peer connection lost");
			}
			break;
		}
	}

	static void		OnSignal(UINT32 signal, TCP::Peer&origin)
	{
		switch (signal)
		{
			case TechniteProtocol::ChannelID::RequestNextRound:
				if (isActive)
				{
					logicIsDone = true;
					if (Network::amTechniteFaction && (!Network::amRegularFaction || Network::newRoundRequested))
					{
						//auto-new-round, if set. Not sure if this should be set
					//	TransmitRoundEndRequest();
					}
				}
			break;
		}
		//none supported for now
	}


	void		Create()
	{
		isActive = true;

		static bool setup = false;
		if (!setup)
		{
			setup = true;

			logicServer.InstallChannel(Channels::techniteInstructionChunk);
			logicServer.InstallChannel(Channels::ready);
			logicServer.InstallChannel(Channels::techniteColorChunk);
			logicServer.InstallChannel(Channels::techniteDebugMessage);
			logicServer.OpenSignalChannel(TechniteProtocol::ChannelID::RequestNextRound,1);

			logicServer.SetAsync(false);	//can't be async because of world grid. damn
			logicServer.onEvent = OnEvent;
			logicServer.onSignal = OnSignal;
		}

		if (!logicServer.IsOnline())
		{
			logicProcess.Terminate();
			if (!logicServer.StartService(initialServerPortNumber,true,&actualServerPortNumber))
			{
				FATAL__(logicServer.GetError(false));
			}
		}

		if (autoStartClient)
		{
			if (!logicProcess.IsRunning() && !logicProcess.Start(FileSystem::GetWorkingDirectory(),autoStartClientPath,PathString(actualServerPortNumber)))
				FATAL__("Failed to launch technite logic");
		}

		#ifdef HAS_WINDOW
			RefreshUserNotification();
		#endif
	}

	#ifdef HAS_WINDOW
	void 	RefreshUserNotification()
	{
		if (AwaitsClient())
		{
			if (!connectionNotification.expired() && initialServerPortNumber == 0)
			{
				Dialog::RemoveDialog(connectionNotification.lock());
				ASSERT__(connectionNotification.expired());
				connectionNotification.reset();
			}

			if (connectionNotification.expired())
				connectionNotification = Dialog::PermanentMessage("Technite logic link active, no logic clients connected. Please connect logic client on port "+String(actualServerPortNumber)+" to continue",[]()->Dialog::CheckResult
				{
					return AwaitsClient() ? Dialog::CheckResult::OkayContinue : Dialog::CheckResult::Abort;
				});
			else
				Dialog::SignalSituationChange();
		}
		else
			if (!connectionNotification.expired())
			{
				Dialog::RemoveDialog(connectionNotification.lock());
				ASSERT__(connectionNotification.expired());
				connectionNotification.reset();
			}
	}
	#endif


	void		CreateMarker(const Objects::Marker&m)
	{
		if (!isActive)
			return;

		switch (m.type)
		{
			case Objects::Marker::Type::EntityDeath:
			case Objects::Marker::Type::HostileContact:
			{
				TechniteProtocol::GameObject obj;
				if (m.visualization)
					FillIn(m.visualization,obj);
				else
					return;
				LogEvent(event,"Creating new ghost-marker for "+m.id.ToString());
				obj.id.isGhost = true;
				obj.birthRound = m.createdInRound;
				ghostGameObjects.Set(m.id,obj);
				Channels::createGameObject.SendTo(logicServer,obj,1);
			}
			break;
			case Objects::Marker::Type::ControlMarker:
			{
				LogEvent(event,"Creating new control-marker for "+m.id.ToString());
				TechniteProtocol::ControlMarker mrk;
				mrk.birthRound = m.createdInRound;
				mrk.location = EncodeLocation(m.location);
				mrk.radius = m.radius;
				mrk.typeIndex = m.subType;
				controlMarkers.Set(m.id,mrk);
				Channels::createControlMarker.SendTo(logicServer,mrk,1);
			}
			break;
		}
	}

	void		DestroyMarker(const TUniqueID&id)
	{
		if (!isActive)
			return;

		TechniteProtocol::GameObject obj;
		if (ghostGameObjects.QueryAndUnset(id,obj))
		{
			LogEvent(event,"Removing ghost-marker for "+id.ToString());
			Channels::removeGameObject.SendTo(logicServer,obj.id);
			return;
		}
		TechniteProtocol::ControlMarker mrk;
		if (controlMarkers.QueryAndUnset(id,mrk))
		{
			LogEvent(event,"Removing control-marker for "+id.ToString());
			Channels::removeControlMarker.SendTo(logicServer,mrk.location);
			return;
		}
	}



	bool		AwaitsClient()
	{
		return !autoStartClient && logicServer.IsOnline() && !logicServer.HasAnyClients();
	}



	void		Forward(Protocol::TechniteChunk&chunk)
	{
		logicIsDone = false;
		executed = false;
		haveAllTechnites = false;
		if (chunk.flags & Protocol::TechniteChunkFlags::IsFirst)
			recordedHistory.Clear();
		recordedHistory.Append(chunk.technites);
		logicServer.VisitAllPeers([&chunk](const TCP::PPeer&peer)
		{
			if (peer->userLevel > 0)
			{
				Channels::techniteStateChunk.SendTo(peer,chunk);
			}
		});
		if ((chunk.flags & Protocol::TechniteChunkFlags::IsLast))
		{
			Array<Grid::HCellIdU>	debugTechnites;
			debugMessageTable.exportKeys(debugTechnites);
			foreach (debugTechnites,tech)
			{
				if (Grid::actualWorld.cellStack[tech->stackID].volumeCell[tech->layer].content != Grid::Content::Technite)
					debugMessageTable.Unset(*tech);
			}

			haveAllTechnites = true;
			if (executed)
				ExecuteLogicWhenAllTechnitesAreThere(RuntimeConfig::roundNumber, RuntimeConfig::techniteSubRoundNumber,true);
		}
	}


	void		SendDelta(Protocol::WorldDelta&delta)
	{
		if (!logicServer.HasAnyClients())
			return;
		TCP::Serial<TechniteProtocol::GridDelta>	serial;
		TechniteProtocol::GridDelta&outDelta = serial.Ref();
		BYTE family = delta.familyAndLastFlag & 0x7F;

		count_t range = Grid::high.grid.CountFamilyMembers(family);
		index_t offset = Grid::high.grid.GetFamilyOffset(family);

		outDelta.nodeCount = (UINT32)range;
		outDelta.nodeOffset = (UINT32)offset;
		outDelta.data0.swap(delta.data0);
		outDelta.data1.swap(delta.data1);

		serial.Seal();



		logicServer.VisitAllPeers([&serial](const TCP::PPeer&peer)
		{
			if (peer->userLevel > 0)
				Channels::gridDelta.SendTo(peer,serial);
		});

		//swap back:
		outDelta.data0.swap(delta.data0);
		outDelta.data1.swap(delta.data1);
	}


	void		ProcessEvents()
	{
		if (!logicServer.IsOnline())
			return;
		logicServer.Resolve();
		logicProcess.Check();
	}


	void		Destroy()
	{
		isActive = false;
		sessionStarted = false;
		executed = false;
		logicServer.EndService();
		logicProcess.Terminate();
		numClients = 0;
		recordedHistory.Clear();
		gameObjects.Clear();
		storedGameObjects.Clear();
		ghostGameObjects.Clear();
		controlMarkers.Clear();
	}


}

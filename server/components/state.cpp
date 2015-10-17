#include <global_root.h>
#include "state.h"
#include "networkChannels.h"
#include "serverLogEvent.h"
#include "aiProcess.h"
#include <io/file_system.h>
#include "../../client/version.h"
#include "../../shared/technite.h"

extern TUniqueID	serverID;


namespace State
{

	TCP::Server		server;

	bool						terrainChanged=false;

	String						Client::ToString() const
	{
		return (faction ? faction->id.ToString() : "[no faction]")+":'"+state.playerName+"'";
	}

	bool		Client::CanSee(const Objects::PWorldEntity&e, float t)	const
	{
		return faction->CanSee(e,t);
	}




	void		Client::UpdateResourceCoverage(const Grid::LCellIdI&center,float range, const Protocol::Timestamp&timestamp)
	{
		static IndexSet covered;
		static Buffer0<UINT32>	coverage, final;
		static Queue<UINT32>	stacks;
		covered.Clear();
		coverage.Clear();
		stacks.Clear();
		final.Clear();

		stacks << center.stackID;

		const TVec3<> worldCenter = Grid::ToWorld(center);

		UINT32 stackID;
		while (stacks >> stackID)
		{
			const TVec3<> world = Grid::ToWorld(Grid::LCellIdI(stackID,center.height));
			if (Vec::quadraticDistance(world,worldCenter) > sqr(range))
				continue;
			if (!covered.IsSet(stackID))
			{
				covered.Set(stackID);
				coverage << stackID;
			}
			else
				continue;

			const auto&n = Grid::low.grid.GetCell(stackID).neighborhood;
			for (BYTE k = 0; k < n.numNeighbors; k++)
				stacks << n.neighborID[k];
		}
			
		foreach (coverage,cov)
		{
			if (faction->visibility.resourcesDiscovered[*cov])
				continue;
			faction->visibility.resourcesDiscovered[*cov] = true;
			final << *cov;
		}

		if (final.IsEmpty())
			return;

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

			Channels::resourceUpdate.SendTo(peer.lock(),package);
		}
	}


	Buffer<PClient>	clients,activePlayers;
	GenericHashTable<Objects::OwnerParameter,PClient,Objects::OwnerHash>	playerMap;
	GenericHashTable<CryptographicID,Objects::PFaction,CryptographicID>		tokenMap;
	UINT32			roundNumber=0, lastSaveRound = 0, empZonesLocked = 0, unlockedEMPZones = 0xFFFF;
	Protocol::Phase	phase;
	Objects::PFaction	winner;	//if set indicates game is over
	TUniqueID		worldID;
	Timer::Time		lastPhaseChange = timer.Now();
	bool			roundTimerPaused = true;

	Grid::TSharedPlanetConfig	persistentPlanetInfo;


	namespace Config
	{
		float		factionStartMineralValue=1.f;	//determined during loading


		int			roundTimer = -1;
		UINT32		uplinkBandwidth = 500;

		bool		closedGame = false,
					startBases = false,
					basesRequired = false,
					simulateLiquids = true,
					empZones = false,
					exposeActionLeftToHostileFactions = true;
		UINT32		saveEveryNRounds = 10;
		bool		saveWhenAllQuit = true;
		//count_t		limitMinesTo = 100l;

		UINT32		factionSupportFlags = AllFactionSupportFlags;

		bool		enableNexusAI = false;
		float		nexusThreatEstimationFactor = 1.f;
		Array<Objects::Component::Turret*>	nexusVehicleTurrets;
		Array<String>			nexusVehicleTurretNames;	//used during config loading, identical to nexusVehicleTurrets later
		

		Goal		goal;
		

		namespace NexusPorts
		{
			bool	haveBases = false;
		}
	}
	


	Protocol::ServerMode		mode;
	float			effectiveAnimationLength = 1.f;


	void		CloseSession()
	{
		LogEvent(EventType::SessionRelated,"Closing session.");
		//Sleep(500);
		State::server.Shutdown();

		Objects::RuntimeConfig::doomsdayWeapons = Objects::RuntimeConfig::DoomsdayWeapons();	//load default value

		clients.Clear();
		activePlayers.Clear();
		playerMap.Clear();

		tokenMap.Clear();
		terrainChanged = false;
		roundNumber = 0;
		lastSaveRound = 0;
		empZonesLocked = 0;
		unlockedEMPZones = 0xFFFF;
		roundTimerPaused = true;
		phase = Protocol::Phase::CommandPhase;
		worldID.Generate();
		winner.reset();

		if (State::Config::enableNexusAI)
			AIProcess::End();

		Objects::FlushAllData();
		Grid::FlushAllData();
	}

	void	ResetFactionStartMineralValue()
	{
		if (State::Config::startBases)
		{
			Config::factionStartMineralValue = Objects::Mine::globalPrototype->GetConstructionCost()
			+ 
			Objects::Generator::globalPrototype->GetConstructionCost()
			+ 
			Objects::MineralContainer::globalPrototype->GetConstructionCost()
			+
			Objects::Vehicle::GetPrice(Objects::Component::hoverbase, Objects::Component::terraformer)
			+
			Objects::Vehicle::GetPrice(Objects::Component::hoverbase, Objects::Component::resourceScanner);
		}
		else
		{
			Config::factionStartMineralValue = 
				3 * Objects::Vehicle::GetPrice(Objects::Component::hoverbase, Objects::Component::terraformer)
				+
				Objects::Vehicle::GetPrice(Objects::Component::hoverbase, Objects::Component::radar)
				+
				Objects::Vehicle::GetPrice(Objects::Component::hoverbase, Objects::Component::resourceScanner)
				+
				Objects::Vehicle::GetPrice(Objects::Component::hoverbase, Objects::Component::artillery);
		}
		//if (State::Config::NexusPorts::haveBases)
		//{
		//	Config::nexusStartMineralValue = 
		//		Objects::Connector::globalPrototype->GetConstructionCost()*5
		//		+
		//		Objects::VehiclePlant::globalPrototype->GetConstructionCost()*4
		//		+
		//		Objects::MunitionFactory::globalPrototype->GetConstructionCost()
		//		+
		//		Objects::Tower::globalPrototypes[0]->GetConstructionCost()*5
		//		+
		//		Objects::Tower::globalPrototypes[1]->GetConstructionCost()*5
		//		;
		//}
		//else
		//	Config::nexusStartMineralValue = 0;
	}

	void	RestartSession()
	{
		LogEvent(EventType::SessionRelated,"Restarting session.");
		#ifndef NO_HIBERNATION
			UpdateHibernation();
		#endif


		State::server.FlushPendingEvents();
		if (!State::server.StartService(SERVICE_PORT))
			FATAL__("Unable to start service. Chances are, a server is already running.")

		if (State::Config::enableNexusAI)
			AIProcess::Begin(Objects::Faction::nexus->accessToken);

		ResetFactionStartMineralValue();

		Objects::VerifyVisibilityNetwork();
		Objects::CheckMobileEntitySanity();
		Objects::VerifyGrid();

		Technites::VerifyGrid();
	}





	void DebugCheckWorld();

	void	ChangePhase(Protocol::Phase to, float timeWindow/* = -1*/)
	{
		LogEvent(EventType::Debug,"Changing phase to "+String(to.ToString())+" with time window "+String(timeWindow));
		Protocol::PhaseChange	ph;
		ph.newPhase = to;
		ph.roundNumber = (UINT32)roundNumber;
		ph.timeFactor = timeWindow;
		Channels::phaseChange.SendTo(server,ph);
		phase = to;
		lastPhaseChange = timer.Now();

		#ifdef _DEBUG
			if (to == Protocol::Phase::AnimationPhase)	//should have enough time during animation phase
			{
				Objects::VerifyVisibilityNetwork();	 //seems stable
				Objects::CheckMobileEntitySanity();

				Objects::VerifyGrid();
			}
		#endif


		//if (to == Protocol::Phase::CommandPhase)
			//DebugCheckWorld();	//this assumes the world does not change. if it does, this will fail

	}


	void	Describe(Protocol::ProductionTask&task, const Objects::MobileEntityProducer::WorkItem&element)
	{
		if (element.producing->IsVehicle())
		{
			Objects::PVehicle v = To<Objects::Vehicle>::Cast(element.producing);
			task.parameters.SetSize(2);
			task.parameters.first() = v->base->constName;
			task.parameters.last() = v->turret->constName;
		}
		else
		{
			task.parameters.SetSize(1);
			task.parameters.first() = element.producing->GetCTypeName();
		}
		task.id = element.id;
	}

	void	Describe(Protocol::MobileTask&task, const Objects::MobileEntity&v, const Grid::LCellIdI&origin, const Objects::MobileEntity::Task&t,index_t taskIndex)
	{
		task.foci.SetSize(t.foci.Count());
		for (index_t i = 0; i < task.foci.Count(); i++)
			task.foci[i] = t.foci[i];
		task.mode = t.mode;
		task.name = t.name;
		task.type = t.type;
		if (t.job)
		{
			Objects::VehicleJob::PSequence seq = To<Objects::VehicleJob::Sequence>::DynamicCast(t.job);
			if (!seq)
			{
				task.mineralCostLeft = t.job->mineralCostLeft > 0 ? t.job->mineralCostLeft : 0;
				task.roundsToJobCompletion = t.job->roundsToJobCompletion;
			}
			else
			{
				
				seq->Guess(v, origin, taskIndex, task.roundsToJobCompletion.value,task.mineralCostLeft.value);
			}
			task.visualizePathToFocus = t.job->movesToFocusWhenDone;
			

			std::shared_ptr<Objects::VehicleJob::PathJob> pathJob = To<Objects::VehicleJob::PathJob>::DynamicCast(t.job);
			if (pathJob)
			{
				task.buildPath.SetSize(pathJob->path.GetLength());
				for (index_t i = 0; i < pathJob->path.GetLength(); i++)
					task.buildPath[i] = pathJob->path[i];
				//pathJob->path.CopyToArray(task.buildPath);
			}

			std::shared_ptr<Objects::VehicleJob::Construct> constJob = To<Objects::VehicleJob::Construct>::DynamicCast(t.job);
			if (constJob)
			{
				task.visualizationStructureName = constJob->constructWhat->GetCTypeName();
			}
		}
		else
		{
			task.visualizePathToFocus = false;
			task.roundsToJobCompletion = -1;
		}
	}

	void	Describe(Protocol::EntityState&state, const Objects::WorldEntity&e, bool owned, const Protocol::Timestamp&timestamp, UINT32 changeFlags)
	{
		state.health = e.health;
		state.id = e.id;
		state.empDamageLevel = e.empDamageLevel;
		state.location = (changeFlags & Objects::WorldEntity::PositionChanged) ? e.location : Grid::LCellIdI::Invalid;
		state.unstable = owned && e.unstable;
		state.timestamp = timestamp;
	}

	void	Describe(Protocol::MobileState&state, const Objects::MobileEntity&v, bool owned, const Protocol::Timestamp&timestamp, UINT32 changeFlags)
	{
		if (v.IsSatellite())
		{
			DBG_ASSERT_GREATER_OR_EQUAL1__(v.location.height, Grid::CellStack::HalfRange,v.GetName());
		}
	
		state.flags = 0;
		if (owned)
		{
			auto&info = state.myMobileEntity.Set0();
			Objects::PHangarStructure h = v.storedIn.lock();
			//Objects::PVehicleStore storedIn = To<Objects::VehicleStore>::DynamicCast(v.storedIn.lock());
			if (h)
				info.storedIn.Set0() = h->ToEntity().id;
			info.tasks.SetSize(v.myTasks.Count());
			Grid::LCellIdI loc = v.location;
			for	(index_t i = 0; i < info.tasks.Count(); i++)
			{
				const auto&task = v.myTasks[i];
				Describe(info.tasks[i],v,loc, task,i);
				if (task.ContainsMovement())
					loc = task.foci.GetNewest();
			}
			#ifndef CONNECTOR_LESS
				info.resourceVector = v.currentStorage;
			#endif
			if (v.isAutomated)
				state.flags.value |= Protocol::VehicleFlags::Automated;
			info.constructionRoundCounter = v.constructionRoundCounter;
			state.attackActionLeft = v.attackActionLeft;
			state.movementActionLeft = v.movementActionLeft;


			const Objects::MobileEntityProducer*p = dynamic_cast<const Objects::MobileEntityProducer*>(&v);
			if (p)
			{
				Protocol::ProducerState&subState = info.producerState.Set0();
				subState.productionTasks.SetSize(p->queue.Count());
				subState.productionDone = p->productionDone;
				subState.gatheringPoint = p->gatheringPoint.current;
				for (index_t i = 0; i < subState.productionTasks.Count(); i++)
				{
					Describe(subState.productionTasks[i],p->queue[i]);
				}
			}

		}
		else
		{
			if (Config::exposeActionLeftToHostileFactions)
			{
				state.attackActionLeft = v.attackActionLeft;
				state.movementActionLeft = v.movementActionLeft;
			}
			else
			{
				state.attackActionLeft = 0;
				state.movementActionLeft = 0;
			}
		}
		if (v.IsHibernating())
			state.flags.value |= Protocol::VehicleFlags::Hibernating;

		Describe(state.entityState,v,owned,timestamp,changeFlags);
		state.entityState.unstable = owned && ((v.myTasks.IsEmpty() && v.completeOnMove) || v.AwaitsUserInteraction());
	}

	void	Describe(Protocol::NewMobileEntity&cnst, const Objects::MobileEntity&v, bool owned, const Protocol::Timestamp&timestamp, UINT32 changeFlags)
	{
		if (v.IsVehicle())
		{
			const Objects::Vehicle&veh = (const Objects::Vehicle&)v;
			cnst.baseName = veh.base->constName;
			cnst.turretName = veh.turret->constName;
		}
		else
		{
			cnst.baseName = v.GetCTypeName();
			cnst.turretName = ConstName::Empty;
		}
		cnst.owner = v.owner->id;
		Describe(cnst.state,v,owned,timestamp,changeFlags);
	}



	void	Describe(Protocol::AttackTask&task, const Objects::Tower::AttackTask&element)
	{
		task.id = element.id;
		task.target = element.target;
	}

	void	Describe(Protocol::StructureState&state, const Objects::Structure&s, bool owned, const Protocol::Timestamp&timestamp, UINT32 changeFlags)
	{
		const Objects::WorkingStructure*w = dynamic_cast<const Objects::WorkingStructure*>(&s);
		//state.productionDone = 0;
		state.flags = 0;
		if (!w || w->IsEnabled())
			state.flags.value |= Protocol::StructureFlags::Enabled;
		if (w && w->IsHibernating())
			state.flags.value |= Protocol::StructureFlags::Hibernating;

		if (w && owned)
		{
			Objects::ResourceVector dontCare;
			w->GetResourceVector(dontCare, state.resourceVector, dontCare);

			//const Objects::WorkingStructure*p = dynamic_cast<const Objects::WorkingStructure*>(&s);
			const Objects::MobileEntityProducer*p = dynamic_cast<const Objects::MobileEntityProducer*>(&s);
			if (p)
			{
				Protocol::ProducerState&subState = state.selective.Set1();
				subState.productionTasks.SetSize(p->queue.Count());
				subState.productionDone = p->productionDone;
				subState.gatheringPoint = p->gatheringPoint.current;
				for (index_t i = 0; i < subState.productionTasks.Count(); i++)
				{
					Describe(subState.productionTasks[i],p->queue[i]);
				}
			}
			const Objects::Tower*t = dynamic_cast<const Objects::Tower*>(&s);
			if (t)
			{
				Protocol::TowerState&subState = state.selective.Set3();
				subState.attackTasks.SetSize(t->attackQueue.Count());
				for (index_t i = 0; i < subState.attackTasks.Count(); i++)
				{
					Describe(subState.attackTasks[i],t->attackQueue[i]);
				}
				
			}
			const Objects::NexusPort*n = dynamic_cast<const Objects::NexusPort*>(&s);
			if (n)
			{
				Protocol::NexusPortState&subState = state.selective.Set2();
				subState = n->resistence;
			}

			const Objects::Mine*m = dynamic_cast<const Objects::Mine*>(&s);
			if (m)
			{
				Protocol::MineState&subState = state.selective.Set0();
				subState.linkedToCore = m->IsLinkedToCore();
				subState.naturalResources = m->naturalResources;
			}
		}
		
		Describe(state.entityState,s,owned,timestamp,changeFlags);
	}

	void	Describe(Protocol::StructureConstruction&cnst, const Objects::Structure&s, bool owned, const Protocol::Timestamp&timestamp)
	{
		cnst.typeName = s.GetCTypeName();
		cnst.owner = s.owner->id;
		Describe(cnst.state,s,owned,timestamp,0xFFFFFFFF);

	}


	void InvalidOperation(TCP::Peer&peer)
	{
		PClient client = To<Client>::Cast(peer.attachment);
		Logging::WriteEvent(EventType::UnexpectedBehavior, "Invalid operation performed",&peer,nullptr);
		if (client)
		{
			clients.FindAndErase(client);
			activePlayers.FindAndErase(client);
			if (client->faction)
			{
				playerMap.Unset(client->faction);
				client->faction->online = false;
				Channels::factionOffline.SendTo(State::server,client->faction->id);
			}

			peer.attachment.reset();
		}
		peer.Disconnect();
	}



	void						Start()
	{
		worldID.Generate();

		server.InstallChannel(Channels::setClientState);
		server.InstallChannel(Channels::entityDirectCommand);
		server.InstallChannel(Channels::choiceIntersection);
		server.InstallChannel(Channels::dropLastTask);
		server.InstallChannel(Channels::dropAllTasks);
		server.InstallChannel(Channels::signalNewJob);
		server.InstallChannel(Channels::signalTerrainMod);
		server.InstallChannel(Channels::constructionAbort);
		server.InstallChannel(Channels::constructionBegin);
		server.InstallChannel(Channels::enableStructure);
		server.InstallChannel(Channels::clientFactionRequest);
		server.InstallChannel(Channels::removeEntity);
		server.InstallChannel(Channels::restoreCoreLink);
		server.InstallChannel(Channels::automate);
		server.InstallChannel(Channels::createMarker);
		server.InstallChannel(Channels::destroyMarker);
		server.InstallChannel(Channels::touchMarker);
		server.InstallChannel(Channels::changeMyName);
		server.InstallChannel(Channels::identifyUserRecv);
		server.InstallChannel(Channels::chatMessage);
		server.InstallChannel(Channels::techniteInstructions);

		server.OpenSignalChannel(Protocol::ChannelID::RequestRoundEnd,1);
		server.OpenSignalChannel(Protocol::ChannelID::Ping,1);
		
		server.onSignal = Receiver::OnSignal;
		server.SetAsync(false);
		server.onEvent = Receiver::OnEvent;
		server.onIgnorePackage = Receiver::OnIgnorePackage;
		if (!State::server.StartService(SERVICE_PORT))
			FATAL__("Unable to start service. Chances are, a server is already running.")

	}


	void						SignalFamilyChangeToFaction(const Objects::PFaction&faction,BYTE familyId, bool last, bool rightNow)
	{
		static Protocol::WorldDelta	delta;
		GenerateWorldDelta(&faction->visibility,&faction->currentState,delta,familyId,rightNow,last,Protocol::WorldDeltaType::ProgressiveUpdate);

		PClient client;
		if (playerMap.Query(faction,client))
		{
			Channels::updateWorld.SendTo(client->peer.lock(),delta);
		}
	}

	void						SignalFamilyChange(BYTE familyId, bool last, bool rightNow)
	{
		SignalFamilyChangeToFaction(Objects::Faction::nexus,familyId,last,rightNow);
		foreach (Objects::Faction::all,_f)
		{
			Objects::PFaction faction = _f->lock();
			if (faction)
				SignalFamilyChangeToFaction(faction,familyId,last,rightNow);
		}


		Grid::high.family[familyId].normal.outdated = false;
	}



	void DebugCheckWorld(bool restart)
	{
		FATAL__("when is this called?");
		static Array<Protocol::WorldDelta>	deltas;
		if (restart)
			deltas.Clear();
		if (deltas.IsEmpty())
		{
			deltas.SetSize(BaseGrid::NumFamilies);
			for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
			{
				GenerateWorldDelta(nullptr,nullptr, deltas[k], k, true, k+1==BaseGrid::NumFamilies, Protocol::WorldDeltaType::GlobalCopy, false);
			}
		}
		else
		{
			Protocol::WorldDelta delta;
			for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
			{
				bool isLast = k+1==BaseGrid::NumFamilies;
				GenerateWorldDelta(nullptr, nullptr, delta, k, true, isLast, Protocol::WorldDeltaType::GlobalCopy, false);
				ASSERT__(delta.data0 == deltas[k].data0);
				ASSERT__(delta.data1 == deltas[k].data1);
				ASSERT__(delta.data1.IsEmpty());
				ASSERT__(delta.familyAndLastFlag == deltas[k].familyAndLastFlag);
				ASSERT__(delta.timestamp.atTime == deltas[k].timestamp.atTime);

			}
			
		}
		deltas.SetSize(BaseGrid::NumFamilies);
		for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
		{
			std::cout<<(int)k<<": "<<deltas[k].GetSerialSize(false)<<std::endl;
		}
	}

	static String FactionStatus(const Objects::PFaction&f)
	{
		if (!f)
			return "Game Over";
		return "Nexus Ports enabled: "+String(f->acquiringNexusControl)+", conquered: "+String(f->nexusControl)+"/"
			+ String(State::Config::GetTargetNexusCount());
	}

	static String FactionWarning(const Objects::PFaction&f)
	{
		if (!f)
			return "Game Over";
		if (State::Config::basesRequired && f->deathInRounds > 0)
			return "Lack of base-functionality: Faction death in "+(f->deathInRounds > 1 ? String(f->deathInRounds)+" rounds" : "ONE round");
		return "";
	}


	void Client::SendProgressUpdate(bool dead)
	{
		Protocol::GameProgress progress;
		progress.gameOver = dead || State::phase == Protocol::Phase::EndOfGame;
		progress.gameWon = State::winner == faction && !!State::winner;
		progress.status = FactionStatus(faction);
		progress.warning = dead ? "Game Over" : FactionWarning(faction);
		Channels::gameProgress.SendTo(peer.lock(),progress);
	}





	enum struct ChunkName : UINT16
	{
		None = 0,
		WorldInfoGroup = 1,
			//WorldID,
			//RoundNumber,

		ObjectGroup = WorldInfoGroup + 1024,
			VehicleTaskSection,
			//Structure = Vehicle + 256,

		WorldGroup = ObjectGroup + 1024,

		InitialWorldGroup = WorldGroup + 1024,
			//WorldChunk,
		ResourceSection = InitialWorldGroup + 1024,
				//ResourceName,
				//ResourceCoverage,

		//FactionGroup = WorldGroup + 1024,
		FactionSection = ResourceSection + 1024,
				//FactionID,
				//FactionAccessToken,
				//ResourcesDiscovered_Compressed,
				//ResourcesDiscovered_Full,
	};


	static_assert(sizeof(ChunkName) == 2, "Alignment fault");
	static Riff::Chunk* CreateNamedList(Riff::Chunk&parent, ChunkName name)
	{
		return parent.AppendNamedList(name);
	}

	static Riff::Chunk* FindNamedList(Riff::Chunk&parent, ChunkName name)
	{
		Riff::Chunk*a,*b;
		if (parent.FindFirstNamedList(a,b))
			do
			{
				UINT16 key;
				if (a->size() == 2)
				{
					a->Get(&key);
					if (name == (ChunkName)key)
						return b;
				}

			}
			while (parent.FindNextNamedList(a,b));

		return nullptr;
	}


	namespace RID 	//RIFF id constants
	{
		static const Riff::TID	entity("ENTI"),
								location("LOCA"),
								vehicle("VEHC"),
								vehiclePlant("SVPL"),				//also used for MobileEntityProducers in general for backwards-compatibility reasons
								vehiclePlantGatheringPoint("SVPG"),	//also used for MobileEntityProducers in general for backwards-compatibility reasons
								vehiclePlantItem("SVPI"),
								towerItem("STOI"),
								structureEnabled("SENA"),
								structureType("STYP"),
								towerActionLeft("STAL"),
								resources("RSRC"),
								noMunitionResources("RSRM"),
								foci("FOCI"),
								taskName("TNME"),
								taskData("TDAT"),
								path("PATH"),
								vehicleBase("VEHB"),
								vehicleTurret("VEHT"),
								mobileEntityType("METY"),
								factionID("ID  "),
								factionName("NAME"),
								factionAccessToken("ACTK"),
								factionWorldTypeChunk("WTCK"),
								factionWorldCounterChunk("WCCK"),
								factionWorldTechniteFactionChunk("WTFC"),
								factionMarkers("MARK"),
								worldTypeChunk(" WTC"),
								worldCounterChunk(" WCC"),
								//worldChunk("WDCK"),
								baseJob("VHBJ"),
								resourceName("RSNM"),
								resourceCoverage("RSCV"),
								resourcesDiscoveredCompressed("RSDC"),
								resourcesDiscoveredFull("RSDF"),
								nexusTag("NEXU"),
								nexusData("NEXD"),
								nexusBases("NEXB"),
								closedGame("CLGM"),
								goal("GOAL"),
								nexusAI("NEXA"),	//currently only indicates that nexus ai is active
								startBases("STBS"),
								basesRequired("BSRQ"),
								newVehiclesHaveFullAmmo("NVHA"),
								doomsdayWeapons("DODW"),
								exposeActionLeftToHostileFactions("EAHF"),
								empDamageLevel("EMPD"),
								worldConfig("WCFG"),
								factionSupportFlags("FSFL"),
								techniteFactionGridID("TFGI"),
								technites("TECH")
								;


	}



	inline static void GetBinary(Riff::Chunk*parent, const Riff::TID&id, void*resultOut, size_t size)
	{
		Riff::Chunk*chk = parent->FindFirst(id);
		ASSERT_NOT_NULL1__(chk, id.ToString());
		ASSERT_EQUAL__(chk->size(),size);
		chk->Get(resultOut);
	}

	template <typename T>
		inline static void GetPOD(Riff::Chunk*parent, const Riff::TID&id, T&resultOut)
		{
			GetBinary(parent,id,&resultOut,sizeof(T));
		}
	
	template <typename T>
		inline static void GetPODs(Riff::Chunk*parent, const Riff::TID&id, ArrayData<T>&resultOut)
		{
			Riff::Chunk*chk = parent->FindFirst(id);
			ASSERT_NOT_NULL1__(chk, id.ToString());
			ASSERT__(chk->IsMultipleOf(sizeof(T)));
			count_t numElements = chk->size() / sizeof(T);
			resultOut.SetSize(numElements);
			chk->Get(resultOut.pointer());
		}

	template <typename T>
		inline static void GetPODs(Riff::Chunk*parent, const Riff::TID&id, BasicBuffer<T>&resultOut)
		{
			Riff::Chunk*chk = parent->FindFirst(id);
			ASSERT_NOT_NULL1__(chk, id.ToString());
			ASSERT__(chk->IsMultipleOf(sizeof(T)));
			count_t numElements = chk->size() / sizeof(T);
			resultOut.SetSize(numElements,true);
			chk->Get(resultOut.pointer());
		}

	inline static bool GetOptionalBinary(Riff::Chunk*parent, const Riff::TID&id, void*resultOut, size_t size)
	{
		Riff::Chunk*chk = parent->FindFirst(id);
		if (!chk)
			return false;
		ASSERT_EQUAL__(chk->size(),size);
		chk->Get(resultOut);
		return true;
	}

	template <typename T>
		inline static bool GetOptionalPOD(Riff::Chunk*parent, const Riff::TID&id, T&resultOut, const T&ifNonExistent)
		{
			bool there = GetOptionalBinary(parent,id,&resultOut,sizeof(T));
			if (there)
				return true;
			resultOut = ifNonExistent;
			return false;
		}

	static void Export(Riff::Chunk*parent, const Objects::WorldEntity&e, GenericHashSet<Objects::PFaction,SharedPtrHash>&factions)
	{
		factions.Set(e.owner);
		static ByteStream stream;
		stream.Clear();


		stream.Append(e.id);
		stream.Append(e.owner->id);
		stream.Append(e.health);
		//stream.Append(e.location);
		stream.Append(e.unstable);
		
		parent->AppendBlock(RID::entity,stream.pointer(),stream.GetFillLevel());
		parent->AppendBlock(RID::location)->SetPODData(e.location);
		if (e.empDamageLevel)
			parent->AppendBlock(RID::empDamageLevel)->SetPODData(e.empDamageLevel);
	}

	static void Import(Riff::Chunk*parent, Objects::WorldEntity&e, const GenericHashTable<TUniqueID,Objects::PFaction,TUniqueID>&ownerMap)
	{
		Riff::Chunk*source = parent->FindFirst(RID::entity);
		ASSERT_NOT_NULL__(source);

		source->OpenStream();
		ASSERT__(source->Stream(e.id));
		TUniqueID	ownerID;
		ASSERT__(source->Stream(ownerID));
		ASSERT__(source->Stream(e.health));
		//ASSERT__(source->Stream(e.location));
		ASSERT__(source->Stream(e.unstable));
		ASSERT__(source->CloseStream());
		ASSERT__(ownerMap.Query(ownerID,e.owner));

		GetOptionalPOD(parent,RID::empDamageLevel,e.empDamageLevel,0);


		//e.SetupSystem();
	}


	static void Export(Riff::Chunk*parent, const Objects::ResourceVector&v)
	{
		if (v.IsNotEmpty())
		{
			#ifdef NO_MUNITION
				parent->AppendBlock(RID::noMunitionResources,v.level,sizeof(v.level));
			#else
				parent->AppendBlock(RID::resources,v.level,sizeof(v.level));
			#endif
		}
	}

	static void Import(Riff::Chunk*parent, Objects::ResourceVector&v)
	{
		Riff::Chunk*source = parent->FindFirst(RID::resources);
		if (source)
		{
			Objects::ResourceVectorT<Objects::ResourceWithMunition> v2;
			ASSERT_EQUAL__(source->size(),sizeof(v2.level));
			memcpy(v2.level,source->pointer(),sizeof(v2.level));
			v.LoadCompatible(v2);
		}
		else
		{
			source = parent->FindFirst(RID::noMunitionResources);
			if (source)
			{
				Objects::ResourceVectorT<Objects::ResourceWithoutMunition> v2;
				ASSERT_EQUAL__(source->size(),sizeof(v2.level));
				memcpy(v2.level,source->pointer(),sizeof(v2.level));
				v.LoadCompatible(v2);
			}
			else
				v.Clear();
		}
	}


	static void Export(Riff::Chunk*taskChunk, const Queue<Grid::LCellIdI>&foci)
	{
		static Array<Grid::LCellIdI>	fociArray;
		if (foci.IsEmpty())
			return;
		foci.CopyToArray(fociArray);
		taskChunk->AppendBlock(RID::foci,fociArray.pointer(),fociArray.contentSize());
	}

	static void Import(Riff::Chunk*taskChunk, Queue<Grid::LCellIdI>&outFoci)
	{
		Riff::Chunk*source = taskChunk->FindFirst(RID::foci);
		outFoci.Clear();
		if (source)
		{
			const count_t numFoci = source->size() / sizeof(Grid::LCellIdI);
			const Grid::LCellIdI*foci = (const Grid::LCellIdI*)source->pointer();
			for (index_t i = 0; i < numFoci; i++)
				outFoci << foci[i];
		}
	}

	static void Export(Riff::Chunk*taskChunk, const BasicBuffer<Objects::PathNode>&path)
	{
		static_assert(sizeof(Objects::PathNode)==sizeof(Grid::LCellIdI)+sizeof(int), "Struct alignment fault" );
		if (path.IsEmpty())
			return;
		taskChunk->AppendBlock(RID::path,path.pointer(),path.GetContentSize());
	}

	static void Import(Riff::Chunk*taskChunk, BasicBuffer<Objects::PathNode>&outPath)
	{
		Riff::Chunk*source = taskChunk->FindFirst(RID::path);
		outPath.Clear();
		if (source)
		{
			const count_t numEntries = source->size() / sizeof(Objects::PathNode);
			const Objects::PathNode*entries = (const Objects::PathNode*)source->pointer();
			//outPath.SetSize(numEntries,false);
			outPath.Append(entries,numEntries);
		}
	}
	static void Export(Riff::Chunk*taskChunk, const Objects::PVehicleJob&job)
	{
		if (job)
		{
			Riff::Chunk*baseChunk = taskChunk->AppendBlock(RID::baseJob);
			static ByteStream	stream;
			stream.Clear();
			stream << job->roundsToJobCompletion << job->mineralCostLeft;// << job->jobAutoCompletion << job->movesToFocusWhenDone;
			baseChunk->SetData(stream);



			Riff::Chunk*jobChunk = taskChunk->AppendBlock(job->GetDataKey());
			size_t dataSize = job->GetSerialSize(false);
			if (dataSize)
				ASSERT2__(SerializeToMemory(*job,jobChunk->SetData(dataSize),(ISerializable::serial_size_t)dataSize,false), job->GetName(),dataSize);
			//this fails sometimes. gotta investigate
		}
	}
	static void Import(Riff::Chunk*taskChunk, Objects::PVehicleJob&job)
	{
		job.reset();
		Riff::Chunk*source0 = taskChunk->FindFirst(RID::baseJob);
		if (!source0)
			return;

		for (index_t i = 0; i < Objects::VehicleJob::BaseJob::allJobPrototypes.Count(); i++)
		{
			auto jobType = Objects::VehicleJob::BaseJob::allJobPrototypes[i];
			Riff::Chunk*source = taskChunk->FindFirst(jobType->GetDataKey());
			if (source)
			{
				job = jobType->Instantiate();
				ASSERT2__(DeserializeFromMemory(*job,source->pointer(),source->size(),false), job->GetName(),source->size());


				source0->OpenStream();
				ASSERT__(source0->Stream(job->roundsToJobCompletion));
				ASSERT__(source0->Stream(job->mineralCostLeft));
				//ASSERT__(source0->Stream(job->jobAutoCompletion));
				//ASSERT__(source0->Stream(job->movesToFocusWhenDone));
				ASSERT__(source0->CloseStream());

				return;
			}
		}
		FATAL__("Unable to find matching job implementation");
	}

	static void Export(Riff::Chunk*parent, const Riff::TID&id, const String&outString)
	{
		parent->AppendBlock(id,outString.c_str(),outString.length());
	}

	static void Import(Riff::Chunk*parent, const Riff::TID&id, String&outString)
	{
		Riff::Chunk*tname = parent->FindFirst(id);
		ASSERT_NOT_NULL__(tname);
		outString.setLength(tname->size());
		memcpy(outString.mutablePointer(),tname->pointer(),tname->size());
	}
	static bool ImportOptional(Riff::Chunk*parent, const Riff::TID&id, String&outString)
	{
		Riff::Chunk*tname = parent->FindFirst(id);
		if (!tname)
			return false;
		outString.setLength(tname->size());
		memcpy(outString.mutablePointer(),tname->pointer(),tname->size());
		return true;
	}


	static void Export(Riff::Chunk*taskChunk, const Objects::Vehicle::Task&t)
	{
		Export(taskChunk,t.foci);
		Export(taskChunk,t.path);
		Export(taskChunk,RID::taskName,t.name);

		static ByteStream stream;
		stream.Clear();
		stream << (UINT16)t.mode << (UINT16)t.type;
		taskChunk->AppendBlock(RID::taskData,stream.pointer(),stream.GetFillLevel());
		Export(taskChunk,t.job);
	}

	static void Import(Riff::Chunk*taskChunk, Objects::Vehicle::Task&t)
	{
		Import(taskChunk,t.foci);
		Import(taskChunk,t.path);
		Import(taskChunk,RID::taskName,t.name);

		Riff::Chunk*tdata = taskChunk->FindFirst(RID::taskData);
		ASSERT_NOT_NULL__(tdata);
		tdata->OpenStream();
		UINT16 i16;
		ASSERT__(tdata->Stream(i16));
		t.mode = (Objects::Vehicle::mode_t)i16;
		ASSERT__(tdata->Stream(i16));
		t.type = (Objects::Vehicle::Task::type_t)i16;
		ASSERT__(tdata->CloseStream());
		Import(taskChunk,t.job);
	}


	NETSTRUCT4(WorkItemSerializable, 
				Package::NetString<256>, baseName, 
				Package::NetString<256>, turretName, 
				NET(TUniqueID), productID, 
				NET(TUniqueID), id);

	static void Export(Riff::Chunk*parent, const Objects::MobileEntityProducer::WorkItem&item)
	{
		WorkItemSerializable helper;
		if (item.producing->IsVehicle())
		{
			Objects::PVehicle v = To<Objects::Vehicle>::Cast(item.producing);
			helper.baseName = v->base->typeName;
			helper.turretName = v->turret->typeName;
		}
		else
		{
			helper.baseName = item.producing->GetTypeName();
		}
		helper.id = item.id;
		helper.productID = item.producing->id;
		serial_size_t size = helper.GetSerialSize(false);
		ASSERT1__(SerializeToMemory(helper,parent->AppendBlock(RID::vehiclePlantItem)->SetData(size),size,false),size);
	}
	static void Import(Riff::Chunk*source, Objects::MobileEntityProducer::WorkItem&item, const Objects::PFaction&owner)
	{
		ASSERT1__(source->IsID(RID::vehiclePlantItem),source->GetID().ToString());

		WorkItemSerializable helper;

		ASSERT1__(DeserializeFromMemory(helper,source->pointer(),source->size(),false),source->size());
		if (helper.turretName.IsNotEmpty())
		{
			item.producing = Objects::AssembleVehicle(owner,helper.productID,Grid::LCellIdI(0,0),helper.baseName,helper.turretName);
		}
		else
		{
			Objects::PSatellite proto = Objects::GetSatellitePrototypeByName(helper.baseName);
			ASSERT1__(proto,helper.baseName);
			item.producing = To<Objects::MobileEntity>::Cast(proto->CreateNewInstance(owner,Grid::LCellIdI(0,0),0));
		}
		item.id = helper.id;
	}

	static void Export(Riff::Chunk*parent, const Objects::MobileEntityProducer&p)
	{
		parent->AppendBlock(RID::vehiclePlant)->SetPODData(p.productionDone);
		if (p.gatheringPoint.current != Grid::LCellIdI::Invalid)
			parent->AppendBlock(RID::vehiclePlantGatheringPoint)->SetPODData(p.gatheringPoint.current);
		if (p.queue.IsNotEmpty())
		{
			Riff::Chunk*jobList = parent->AppendBlock(RIFF_LIST);
			foreach (p.queue,job)
				Export(jobList,*job);
		}
	}

	static void Import(Riff::Chunk*parent, Objects::MobileEntityProducer&producer, const Objects::PWorldEntity&selfReference)
	//const Objects::OwnerParameter&owner)
	{
		Riff::Chunk*vehiclePlantChunk = parent->FindFirst(RID::vehiclePlant);
		if (!vehiclePlantChunk)
		{
			LogEvent(EventType::Notification,selfReference.get(),"Warning: Data chunk missing in file: MobileEntityProducer state. Skipping");
			return;
		}
//		ASSERT_NOT_NULL__(vehiclePlantChunk);
		ASSERT_EQUAL__(vehiclePlantChunk->size(),sizeof(producer.productionDone));
		vehiclePlantChunk->Get(&(producer.productionDone));

		GetOptionalPOD(parent,RID::vehiclePlantGatheringPoint,producer.gatheringPoint.current,Grid::LCellIdI::Invalid);


		Riff::Chunk*list = parent->FindFirst(RIFF_LIST);
		while (list)
		{
			//if (list->
			Riff::Chunk*entry = list->FindFirst(RID::vehiclePlantItem);
			if (entry)
			{
				do
				{
					Import(entry,producer.queue.Append(),selfReference->owner);
				}
				while (entry = list->FindNext(RID::vehiclePlantItem));

				break;
			}
			list = parent->FindNext(RIFF_LIST);
		}

	}


	enum class MobileEntityFlag : UINT16
	{
		None = 0x0,
		HasStartedJob = 0x1,
		IsAutomated = 0x2,
		IsStored = 0x4


	};

	void	operator|=(MobileEntityFlag&a, MobileEntityFlag b)
	{
		a = (MobileEntityFlag)((UINT16)a | (UINT16)b);
	}
	bool	operator&(MobileEntityFlag a, MobileEntityFlag b)
	{
		return ((UINT16)a & (UINT16)b) != 0;
	}

	Buffer0<Objects::PMobileEntity>	insertIntoHangar;

	static void Export(Riff::Chunk*parent, Objects::MobileEntity&v, GenericHashSet<Objects::PFaction,SharedPtrHash>&factions)
	{
		Objects::PHangarStructure store = v.storedIn.lock();
		if (store)
		{
			v.location = store->ToEntity().location;
		}
		v.TranslateLocationForExport();
		Export(parent,(Objects::WorldEntity&)v,factions);
		v.TranslateLocationFromExport();
		#ifndef CONNECTOR_LESS
			Export(parent,v.currentStorage);
		#endif
		Export(parent,v.completeOnMove);
		foreach (v.myTasks,task)
		{
			Riff::Chunk*t = parent->AppendNamedList(ChunkName::VehicleTaskSection);
			Export(t,*task);
		}

		const Objects::MobileEntityProducer*p = dynamic_cast<const Objects::MobileEntityProducer*>(&v);
		if (p)
		{
			Export(parent,*p);
		}

		static ByteStream stream;
		stream.Clear();
		stream.Append(v.attackActionLeft);
		stream.Append(v.movementActionLeft);
		stream.Append(v.constructionRoundCounter);

		MobileEntityFlag flags = MobileEntityFlag::None;
		if (v.isAutomated)
			flags |= MobileEntityFlag::IsAutomated;
		if (v.jobStarted)
			flags |= MobileEntityFlag::HasStartedJob;
		if (store)
			flags |= MobileEntityFlag::IsStored;

		stream.Append(flags);
		//stream.Append(v.hasConstructedThisRound);
		//stream.Append(v.hasMoved);
		
		parent->AppendBlock(RID::vehicle,stream.pointer(),stream.GetFillLevel());
		if (v.IsVehicle())
		{
			Objects::Vehicle&veh = (Objects::Vehicle&)v;
			Export(parent,RID::vehicleTurret,veh.turret->typeName);
			Export(parent,RID::vehicleBase,veh.base->typeName);
		}
		else
		{
			Export(parent,RID::mobileEntityType,v.GetTypeName());
		}

		Export(parent,v.completeOnMove);
	}

	static bool Import(Riff::Chunk*parent, Riff::Chunk*vehicleChunk,const Objects::PMobileEntity&v, GenericHashTable<TUniqueID,Objects::PFaction,TUniqueID>&factions)
	{
		//Grid::LCellIdI location;
		GetPOD(parent,RID::location,v->location);
		v->TranslateLocationFromExport();
		v->SetupSystem();
		 

		Import(parent,(Objects::WorldEntity&)*v,factions);
		#ifndef CONNECTOR_LESS
			Import(parent,v->currentStorage);
		#endif
		Import(parent,v->completeOnMove);
		Riff::Chunk *name,*data;
		if (parent->FindFirstNamedList(name,data))
			do
			{
				if (name->DataMatch(ChunkName::VehicleTaskSection))
				{
					Import(data,v->myTasks.Push());
				}
				else
				{
					FATAL__("key mismatch");
				}
			}
			while (parent->FindNextNamedList(name,data));

		Objects::MobileEntityProducer*p = dynamic_cast<Objects::MobileEntityProducer*>(v.get());
		if (p)
		{
			Import(parent,*p,v);
		}


		vehicleChunk->OpenStream();		
		ASSERT__(vehicleChunk->Stream(v->attackActionLeft));
		ASSERT__(vehicleChunk->Stream(v->movementActionLeft));
		//ASSERT__(vehicleChunk->Stream(v.hasConstructedThisRound));
		//ASSERT__(vehicleChunk->Stream(v.hasMoved));
		ASSERT__(vehicleChunk->Stream(v->constructionRoundCounter));

		MobileEntityFlag flags;
		ASSERT__(vehicleChunk->Stream(flags));
		v->jobStarted = flags & MobileEntityFlag::HasStartedJob;
		v->isAutomated = flags & MobileEntityFlag::IsAutomated;
		ASSERT__(vehicleChunk->CloseStream());
		if (flags & MobileEntityFlag::IsStored)
		{
			insertIntoHangar << v;
		}

		Import(parent,v->completeOnMove);

		return !(flags & MobileEntityFlag::IsStored);
	}


	NETSTRUCT3(AttackItemSerializable, NET(bool), autoTarget, NET(Grid::LCellIdI), destination, NET(TUniqueID), id);

	

	static void Export(Riff::Chunk*parent, const Objects::Tower::AttackTask&item)
	{
		AttackItemSerializable helper;
		helper.destination = item.target;
		helper.autoTarget = item.autoTarget;
		helper.id = item.id;
		serial_size_t size = helper.GetSerialSize(false);
		ASSERT1__(SerializeToMemory(helper,parent->AppendBlock(RID::towerItem)->SetData(size),size,false),size);
	}
	static void Import(Riff::Chunk*source, Objects::Tower::AttackTask&item)
	{
		ASSERT1__(source->IsID(RID::towerItem),source->GetID().ToString());
		AttackItemSerializable helper;
		ASSERT__(DeserializeFromMemory(helper,source->pointer(),source->size(),false));
		item.target = helper.destination;
		item.id = helper.id;
		item.autoTarget = helper.autoTarget;
	}



	static void Export(Riff::Chunk*parent, Objects::Structure&s, GenericHashSet<Objects::PFaction,SharedPtrHash>&factions)
	{
		Export(parent,(Objects::WorldEntity&)s,factions);
		Export(parent,RID::structureType,s.GetTypeName());
		const Objects::WorkingStructure*w = dynamic_cast<const Objects::WorkingStructure*>(&s);
//		state.enabled = !w || w->IsEnabled();
		if (w)
		{
			Objects::ResourceVector dontCare,resources;
			w->GetResourceVector(dontCare, resources, dontCare);
			Export(parent,resources);
			parent->AppendBlock(RID::structureEnabled)->SetPODData(w->IsEnabled());

			const Objects::MobileEntityProducer*p = dynamic_cast<const Objects::MobileEntityProducer*>(&s);
			if (p)
			{
				Export(parent,*p);
			}
			const Objects::Tower*t = dynamic_cast<const Objects::Tower*>(&s);
			if (t)
			{
				parent->AppendBlock(RID::towerActionLeft)->SetPODData(t->attackActionLeft);
				if (t->attackQueue.IsNotEmpty())
				{
					Riff::Chunk*jobList = parent->AppendBlock(RIFF_LIST);
					foreach (t->attackQueue,job)
						Export(jobList,*job);
				}
			}
			const Objects::NexusPort*n = dynamic_cast<const Objects::NexusPort*>(&s);
			if (n)
			{
				UINT32 data[2];
				data[0] = (UINT32)n->resistence;
				data[1] = n->lastSpawnRound;
				parent->AppendBlock(RID::nexusData)->SetData(data,sizeof(data));
			}

		}
		
	}

	
	static Objects::PStructure Import(Riff::Chunk*parent, GenericHashTable<TUniqueID,Objects::PFaction,TUniqueID>&factions)
	{
		String typeName;
		Import(parent,RID::structureType,typeName);
		Grid::LCellIdI location;
		GetPOD(parent,RID::location,location);
		if (globalVersionOfCurrentFile <= 1038)
		{
			if (typeName == "VehicleStore")
				typeName = "Vehicle Store";
			elif (typeName == "NexusPort")
				typeName = "Nexus Port";
		}
		Objects::PStructure proto = Objects::GetPrototypeByName(typeName);

		if (globalVersionOfCurrentFile <= 1038)
		{
			if (!proto)
			{
				if (typeName == "Artillery Tower" || typeName == "Cannon Tower")
				{
					proto = Objects::GetPrototypeByName(ConstName::Type_LauncherTower);
				}
			}
		}

		ASSERT1__(proto,typeName);


		Objects::PStructure rs = To<Objects::Structure>::Cast( proto->CreateNewInstance(Objects::PFaction(),location,0));



		Import(parent,(Objects::WorldEntity&)*rs,factions);

		Objects::WorkingStructure*w = dynamic_cast<Objects::WorkingStructure*>(rs.get());
//		state.enabled = !w || w->IsEnabled();
		if (w)
		{
			Objects::ResourceVector resources;
			Import(parent,resources);
			w->SetResourceStorage(resources);
			Riff::Chunk*enabled = parent->FindFirst(RID::structureEnabled);
			ASSERT_NOT_NULL__(enabled);
			bool isEnabled = *(const bool*)enabled->pointer();
			if (isEnabled)
				w->SetEnabled(isEnabled,0,false,Objects::WorkingStructure::ForceEnable,false);
			else
				w->SetEnabledOverride(false);
			Objects::MobileEntityProducer*p = dynamic_cast<Objects::MobileEntityProducer*>(w);
	//		Objects::MobileEntityProducer*p = w->ToProducer();
			//Objects::VehiclePlant*p = dynamic_cast<Objects::VehiclePlant*>(rs.get());
			if (p)
			{
				Import(parent,*p,rs);
			}
			Objects::Tower*t = dynamic_cast<Objects::Tower*>(rs.get());
			if (t)
			{
				Riff::Chunk*towerActionChunk = parent->FindFirst(RID::towerActionLeft);
				ASSERT_NOT_NULL__(towerActionChunk);
				ASSERT_EQUAL__(towerActionChunk->size(),sizeof(t->attackActionLeft));
				towerActionChunk->Get(&(t->attackActionLeft));
				Riff::Chunk*list = parent->FindFirst(RIFF_LIST);
				if (list)
				{
					Riff::Chunk*entry = list->First();
					if (entry)
						do
						{
							Import(entry,t->attackQueue.Push());
						}
						while (entry = list->Next());
				}
	
			}

			Objects::NexusPort*n = dynamic_cast<Objects::NexusPort*>(rs.get());
			if (n)
			{
				UINT32 data[2];

				GetBinary(parent,RID::nexusData,data,sizeof(data));

				n->resistence = (int)data[0];
				n->lastSpawnRound = data[1];

				n->OnLoad();
			}
		}


		return rs;
		
	}


	void SerializeToChunk(const ISerializable&data, Riff::Chunk*target)
	{
		static Array<BYTE> buffer;
		ISerializable::serial_size_t size = data.GetSerialSize(false);
		if (buffer.size() < size)
			buffer.SetSize(size);
		ASSERT1__(SerializeToMemory(data,buffer.pointer(),size,false),size);
		target->SetData(buffer.pointer(),size);
	}
}

void	State::Load(const String&filename)
{
	insertIntoHangar.Clear();

	Riff::Container file;
	bool failed = false;
	String filenameName = "'"+FileSystem::ExtractFileNameExt(filename)+ "'";
	if (!file.LoadFromFile(filename.c_str()))
	{
		LogEvent(EventType::InternalFault,"File not found");
		failed = true;
	}
	else
	{
		Riff::Chunk*vers = file.FindFirst("VERS");
		if (!vers)
		{
			LogEvent(EventType::InternalFault,"Unable to read version number");

			failed = true;
		}
		else
		{
			UINT32 vid;
			if (vers->size() != 4)
			{
				LogEvent(EventType::InternalFault,"Version size mismatch");

				failed = true;
			}
			else
			{
				vers->Get(&vid);
				if (vid < 1035)
				{
					LogEvent(EventType::InternalFault,"Version not supported anymore. Expected >= 1035, found "+String(vid));

					failed = true;
				}
				else
					if (vid > fileVersion)
					{
						LogEvent(EventType::InternalFault,"Unsupported version: "+String(vid)+". Versions are supported up to "+String(fileVersion));
						failed = true;
					}
					else
						globalVersionOfCurrentFile = vid;
			}

		}
	}


	if (failed)
	{
		LogEvent(EventType::InternalFault,"Unable to restore saved world state from '"+filename+"'.");
		return;
	}


	LogEvent(EventType::SessionRelated,"Restoring saved world state from '"+filename+"'.");
	State::CloseSession();

	bool nexusFound = false, worldInfoFound = false;

	GenericHashTable<TUniqueID,Objects::PFaction,TUniqueID>	factionMap;
	for (int round = 0; round < 2; round++)
	{
		Riff::Chunk*name,*list;
		if (file.FindFirstNamedList(name,list))
			do
			{
				ASSERT_EQUAL__(name->size(),2);
				UINT16 nameID;
				name->Get(&nameID);
				switch ((ChunkName)nameID)
				{
					case ChunkName::WorldInfoGroup:
						if (round == 0)
						{
							ASSERT__(!worldInfoFound);
							worldInfoFound = true;
							GetPOD(list,"WDID",worldID);
							GetPOD(list,"RNDN",roundNumber);


							State::Config::enableNexusAI = list->FindFirst(RID::nexusAI) != nullptr;

							GetPOD(list,RID::closedGame,State::Config::closedGame);
							GetOptionalPOD(list,RID::nexusBases,State::Config::NexusPorts::haveBases,false);
							UINT16 goal;
							GetPOD(list,RID::goal,goal);
							State::Config::goal = State::Config::Goal::Reinterpret(goal);
							GetPOD(list,RID::startBases,State::Config::startBases);
							GetPOD(list,RID::basesRequired,State::Config::basesRequired );
							GetOptionalPOD(list,RID::newVehiclesHaveFullAmmo,Objects::newVehiclesHaveFullAmmo,true);
							GetOptionalPOD(list,RID::exposeActionLeftToHostileFactions,State::Config::exposeActionLeftToHostileFactions,true);
							{
								UINT8 v;
								GetOptionalPOD(list,RID::doomsdayWeapons,v,(UINT8)0);
								Objects::RuntimeConfig::doomsdayWeapons.Load(v);
							}
							GetPOD(list,RID::worldConfig,State::persistentPlanetInfo);
							Grid::CellStack::bottom.content = (BYTE)State::persistentPlanetInfo.innerCoreType;

							GetOptionalPOD(list,RID::factionSupportFlags,State::Config::factionSupportFlags,State::Config::AllFactionSupportFlags);

						}
					break;
					case ChunkName::ObjectGroup:
						if (round == 1)
						{
						
							Riff::Chunk*child;
							if (child = list->First())
								do
								{
									if (child->IsID(RIFF_LIST))
									{
										Riff::Chunk*vehicleChunk = child->FindFirst(RID::vehicle);
										if (vehicleChunk)
										{
											String base,turret;
											if (!ImportOptional(child,RID::mobileEntityType,base))
											{
												Import(child,RID::vehicleBase,base);
												Import(child,RID::vehicleTurret,turret);
											}
											Objects::PMobileEntity v = Objects::AssembleMobileEntity(Objects::PFaction(),TUniqueID(),Grid::LCellIdI(0,0),base,turret);

											bool doInsert = Import(child,vehicleChunk,v,factionMap);
											#if defined CHEAT_FULL_STORAGE && !defined CONNECTOR_LESS
												 v->currentStorage = v->GetMaxStorageCapacity();
											#endif
											if (doInsert)
												Objects::InsertConstructed(v,0,true,false);
											else
											{
												//Objects::storedVehicles << v;
												//v->owner->storedVehicles << v;
												//Objects::idMap.Set(v->id,v);
												//v->isInserted = v->isInsertedIntoGrid = false;
											}
										}
										else
										{
											//child->FindFirst(RID::vehiclePlant),child->FindFirst(RID::towerActionLeft)
											Objects::PStructure s = Import(child,factionMap);
											if (s)
												Objects::InsertConstructed(s,0,false,false);
										}
									}
									else
										FATAL__("Encountered unexpected block type in object-group: "+String(child->GetID().ToString()));
								}
								while (child = list->Next());


							Objects::RefreshCompoundCoverage(0);

						}
					break;
					case ChunkName::WorldGroup:
						if (round == 0)
						{
							//Protocol::WorldDelta delta;
							Protocol::TypeDeltaArray	typeArray;
							Protocol::StructCountDeltaArray countArray;
							Protocol::TechniteFactionDeltaArray techniteArray;

							Riff::Chunk*child;
							BYTE atFamily = 0;
							if (child = list->First())
								do
								{

									if (child->IsID(RID::worldTypeChunk))
									{
										ASSERT__(DeserializeFromMemory(typeArray,child->pointer(),child->size(),false));
										ASSERT_LESS__(atFamily, BaseGrid::NumFamilies);
										Grid::actualWorld.ApplyFamilyDelta(atFamily, typeArray, countArray, techniteArray, false, true);

										atFamily++;
									}

									//if (child->IsID(RID::worldChunk))
									//{
									//	ASSERT__(DeserializeFromMemory(delta,child->pointer(),child->size(),false));
									//	BYTE family = delta.family & 0x7F;
									//	std::cout << "family: "<<(int)family<<std::endl;
									//	Grid::ApplyFamilyDelta(family,delta.data0,delta.data1,true);
									//}
									else
										FATAL__("Unexpected chunk id "+String(child->GetID().ToString()));
								}
								while (child = list->Next());
							ASSERT_EQUAL__(atFamily,BaseGrid::NumFamilies);
						}
					break;
					case ChunkName::InitialWorldGroup:
						if (round == 0)
						{
							Protocol::TypeDeltaArray	typeArray;
							Protocol::StructCountDeltaArray countArray;
							Protocol::TechniteFactionDeltaArray techniteArray;
							Riff::Chunk*child;
							BYTE atFamily = 0;
							if (child = list->First())
								do
								{
									if (child->IsID(RID::worldTypeChunk))
									{
										ASSERT__(DeserializeFromMemory(typeArray,child->pointer(),child->size(),false));
										Grid::initialWorld.ApplyFamilyDelta(atFamily, typeArray, countArray, techniteArray, true, false);
										atFamily++;
									}
									else
										FATAL__("Unexpected chunk id "+String(child->GetID().ToString()));
								}
								while (child = list->Next());
							ASSERT_EQUAL__(atFamily,BaseGrid::NumFamilies);

						}
					break;
					case ChunkName::ResourceSection:
						if (round == 0)
						{
							String resourceName;
							Import(list,RID::resourceName,resourceName);
							Grid::Resource&res = Grid::Resource::Require(resourceName);
							GetBinary(list,RID::resourceCoverage,res.coverage,sizeof(res.coverage));
						}
					break;
					case ChunkName::FactionSection:
						if (round == 0)
						{
							Objects::PFaction faction;
							
							TUniqueID factionID;
							GetPOD(list,RID::factionID,factionID);

							bool isNexus = false;
							if (list->FindFirst(RID::nexusTag))
							{
								isNexus = true;
								ASSERT__(!nexusFound);
								nexusFound = true;
								faction = Objects::Faction::nexus;
								faction->FlushData();
								faction->id = factionID;
								LogEvent(EventType::SessionRelated,"Loading nexus faction");
							}
							else
							{

								BYTE id;
								if (GetOptionalPOD(list, RID::techniteFactionGridID, id, (BYTE)0xFF))
								{
									Technites::PFaction tfaction = Technites::Faction::CreateFromFile(factionID, (BYTE)id);
									faction = tfaction;
									GetPODs(list, RID::technites, tfaction->technites);
									for (index_t i = 0; i < tfaction->technites.Count(); i++)
									{
										const Technites::Technite&t = tfaction->technites[i];
										Grid::HCellIdU loc = t.location.GetU();
										tfaction->techniteMap.Set(loc, i);
										Grid::actualWorld.cellStack[loc.stackID].SetLocalTechniteFaction(t.location.GetHeight(), tfaction->gridID);
										t.RegisterFactionSensory(tfaction->visibility,0.f);
									}
								}
								else
								{
									faction.reset(new Objects::Faction());
									Objects::Faction::all << faction;
									faction->currentState.FlushAllData();
									faction->id = factionID;
								}
								//foreach (faction->currentState,stat)
								//	stat->ZeroInit();
							}


							GetPOD(list,RID::factionAccessToken,faction->accessToken);

							Riff::Chunk*nameChunk = list->FindFirst(RID::factionName);
							if (nameChunk)
							{
								faction->name.setLength(nameChunk->size());
								nameChunk->Get(faction->name.mutablePointer());
							}
							else
								faction->name = isNexus ? "<Nexus>" : faction->id.ToString();
							
							if (globalVersionOfCurrentFile >= 1038)
							{
								Riff::Chunk*markerChunk = list->FindFirst(RID::factionMarkers);
								if (markerChunk)
								{
									Array<Objects::Marker>	markers;
									ASSERT__(DeserializeFromMemory(markers, markerChunk->pointer(), markerChunk->size(), false));
									faction->RestoreMarkers(markers);
								}
							}

							factionMap.Set(faction->id,faction);
							if (!isNexus)
								tokenMap.Set(faction->accessToken,faction);	//dont even allow access if nexus

							//BYTE k = 0;
							//Protocol::WorldDelta	delta;
							Riff::Chunk*child;

							Protocol::TypeDeltaArray	typeArray;
							Protocol::StructCountDeltaArray countArray;
							Protocol::TechniteFactionDeltaArray techniteFactionArray;
							//Riff::Chunk*child;
							BYTE atFamily = 0;

							if (child = list->First())
								do
								{
									
										//factionWorldTypeChunk("WTCK"),
										//factionWorldCounterChunk("WCCK"),

									if (child->IsID(RID::factionWorldTypeChunk))
									{
										ASSERT__(DeserializeFromMemory(typeArray,child->pointer(),child->size(),false));
									}
									elif(child->IsID(RID::factionWorldTechniteFactionChunk))
									{
										ASSERT__(DeserializeFromMemory(techniteFactionArray, child->pointer(), child->size(), false));
									}
									elif (child->IsID(RID::factionWorldCounterChunk))
									{
										ASSERT__(DeserializeFromMemory(countArray,child->pointer(),child->size(),false));
										ASSERT_LESS__(atFamily, BaseGrid::NumFamilies);
										faction->currentState.ApplyFamilyDelta(atFamily,typeArray,countArray,techniteFactionArray, false,false);

										atFamily++;
									}
								
								}
								while (child = list->Next());
							ASSERT_EQUAL__(atFamily,BaseGrid::NumFamilies);


							Riff::Chunk*compressed = list->FindFirst(RID::resourcesDiscoveredCompressed);
							if (compressed)
							{
								const UINT32*indices = (const UINT32*)compressed->pointer();
								const count_t numIndices = compressed->size() / sizeof(UINT32);
								for (UINT32 i = 0; i < numIndices; i++)
									faction->visibility.resourcesDiscovered[indices[i]] = true;
							}
							else
							{
								Riff::Chunk*raw = list->FindFirst(RID::resourcesDiscoveredFull);
								if (raw)
								{
									ASSERT_EQUAL__(raw->size(),faction->visibility.resourcesDiscovered.GetContentSize());
									memcpy(faction->visibility.resourcesDiscovered.pointer(),raw->pointer(),raw->size());
								}
							}
						}
					break;
					default:
						FATAL__("Unsupported object group encountered: "+String(nameID));

				}

			}
			while (file.FindNextNamedList(name,list));
	}





	for (const Objects::PMobileEntity&v : insertIntoHangar)
	{
		Objects::PHangarStructure s = To<Objects::HangarStructure>::DynamicCast(Objects::GetEntity(v->location));
		ASSERT2__(s,v->GetName(),v->location.ToString());
		s->StoreLocallyFromImport(v);
	}
	insertIntoHangar.Clear();

	Objects::ForeachWorldEntity([](const Objects::PWorldEntity&w)
	{
		w->ResetAllChange();
	});

	lastSaveRound = roundNumber;
	ASSERT__(worldInfoFound);
	//ASSERT__(nexusFound);	if it has no units, it is not stored



	if (State::Config::goal == State::Config::Goal::EMPConquerNexus)
	{
		count_t numZones = State::Config::CountAccessibleZones();

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
	}


	LogEvent(EventType::SessionRelated,"Restored world "+worldID.ToNiceString()+" in round "+String(roundNumber));
	State::RefreshWindowTitle();


	State::RestartSession();

}

void State::RefreshWindowTitle()
{
	String title = String(PRODUCT_NAME)+" server "+ serverID.ToNiceString()+" (world "+ worldID.ToNiceString()+")";
	SetConsoleTitleA(title.c_str());
}

void State::AutoSave()
{
	String base = "saves/auto/snapshot";
	FileSystem::CreateDirectory(FileSystem::ExtractFileDir(base));
	if (FileSystem::IsFile(base+"0."+savFileExtension))
	{
		FileSystem::UnlinkFile(base+"9."+savFileExtension);
		for (int i = 8; i >= 0; i--)
			FileSystem::MoveFile(base+String(i)+"."+savFileExtension,base+String(i+1)+"."+savFileExtension);
	}
	State::Save(base+"0."+savFileExtension);

	#ifdef SAVE_FILE_TEST
		State::Load(base+"0."+savFileExtension);
		State::Save("test1."+savFileExtension);

		Riff::File a,b;
		ASSERT__(a.Open((base+"0."+savFileExtension).c_str()));
		ASSERT__(b.Open("test1."+savFileExtension));
		Buffer<Riff::TID>	stack;
		Compare(a,b,stack);
	#endif

	State::lastSaveRound = State::roundNumber;
}

void	State::Save( const String&name)
{
	static_assert(sizeof(TUniqueID) == TUniqueID::NumBytes, "Alignment fault" );

	Timer::Time t0 = timer.Now();

	Riff::Container container;
	UINT32 vid = fileVersion;
	container.AppendBlock("VERS",&vid,sizeof(vid));
	Riff::Chunk*general = CreateNamedList(container,ChunkName::WorldInfoGroup);
	general->AppendBlock("WDID")->SetPODData(worldID);
	general->AppendBlock("RNDN")->SetPODData(roundNumber);
	general->AppendBlock(RID::worldConfig)->SetPODData(State::persistentPlanetInfo);
	general->AppendBlock(RID::factionSupportFlags)->SetPODData(State::Config::factionSupportFlags);
	general->AppendBlock(RID::closedGame)->SetPODData(State::Config::closedGame);
	general->AppendBlock(RID::nexusBases)->SetPODData(State::Config::NexusPorts::haveBases);
	general->AppendBlock(RID::goal)->SetPODData((UINT16)State::Config::goal);
	if (State::Config::enableNexusAI)
		general->AppendBlock(RID::nexusAI);
	general->AppendBlock(RID::startBases)->SetPODData(State::Config::startBases);
	general->AppendBlock(RID::basesRequired )->SetPODData(State::Config::basesRequired );
	general->AppendBlock(RID::newVehiclesHaveFullAmmo)->SetPODData(Objects::newVehiclesHaveFullAmmo);
	general->AppendBlock(RID::exposeActionLeftToHostileFactions )->SetPODData(State::Config::exposeActionLeftToHostileFactions );
	general->AppendBlock(RID::doomsdayWeapons)->SetPODData((UINT8)Objects::RuntimeConfig::doomsdayWeapons);

	GenericHashSet<Objects::PFaction,SharedPtrHash>	allFactions;

	Riff::Chunk*world = CreateNamedList(container,ChunkName::WorldGroup);
	for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
	{
		TCP::Serial<Protocol::WorldDelta>	delta;
		delta.Ref().timestamp.rightNow = true;
		Protocol::GenerateWorldDelta(nullptr, nullptr, delta.Ref(),k,true,k+1 == BaseGrid::NumFamilies,Protocol::WorldDeltaType::GlobalCopy,false);
		//delta.Ref().timestamp.atTime = 0;
		//delta.Ref().data1.Clear();
		//delta.Seal();
		SerializeToChunk(delta.Ref().data0,world->AppendBlock(RID::worldTypeChunk));
	}
	Riff::Chunk*initialWorld = CreateNamedList(container,ChunkName::InitialWorldGroup);
	for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
	{
		TCP::Serial<Protocol::WorldDelta>	delta,delta2;
		delta.Ref().timestamp.rightNow = true;
		Protocol::GenerateWorldDelta(nullptr, nullptr, delta.Ref(),k,true,k+1 == BaseGrid::NumFamilies,Protocol::WorldDeltaType::GlobalInitialCopy,false);

		SerializeToChunk(delta.Ref().data0,initialWorld->AppendBlock(RID::worldTypeChunk));
		//initialWorld->AppendBlock(RID::worldChunk)->SetData(delta.Get());
	}


	Riff::Chunk*objects = CreateNamedList(container,ChunkName::ObjectGroup);
	Objects::ForeachMobileEntityIncludingStored([objects,&allFactions](const Objects::PMobileEntity&_v)
	{
		Objects::MobileEntity&v = *_v;
		Export(objects->AppendBlock(RIFF_LIST),v,allFactions);
	});



	foreach (Objects::structures,_s)
	{
		Objects::Structure&s = **_s;
		Export(objects->AppendBlock(RIFF_LIST),s,allFactions);
	}


	foreach (Grid::Resource::all,res)
	{
		Riff::Chunk*resSec = CreateNamedList(container,ChunkName::ResourceSection);
		resSec->AppendBlock(RID::resourceName)->SetData(res->GetName().c_str(),res->GetName().length());
		resSec->AppendBlock(RID::resourceCoverage)->SetData(res->coverage,sizeof(res->coverage));
	}










	//Riff::Chunk*factions = CreateNamedList(container,ChunkName::FactionGroup);
	allFactions.VisitAllKeys([&container](const Objects::PFaction&faction)
	{
		Riff::Chunk*factionSection = CreateNamedList(container,ChunkName::FactionSection);
		
		if (faction == Objects::Faction::nexus)
		{
			factionSection->AppendBlock(RID::nexusTag);
		}

		if (faction->IsTechniteFaction())
		{
			Technites::PFaction tfaction = To<Technites::Faction>::Cast(faction);

			factionSection->AppendBlock(RID::techniteFactionGridID)->SetPODData(tfaction->gridID);
			factionSection->AppendBlock(RID::technites,tfaction->technites.pointer(),tfaction->technites.GetContentSize());
		}



		factionSection->AppendBlock(RID::factionID)->SetPODData(faction->id);	//faction id
		factionSection->AppendBlock(RID::factionName)->SetData(faction->name.c_str(),faction->name.length());

		factionSection->AppendBlock(RID::factionAccessToken)->SetPODData(faction->accessToken);	//faction access token

		if (faction->markers.IsNotEmpty())
		{
			Riff::Chunk*markerChunk = factionSection->AppendBlock(RID::factionMarkers);
			SerializeToChunk(faction->markers.CopyToArray(), markerChunk);
		}



		for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
		{
			TCP::Serial<Protocol::WorldDelta>	delta;
			delta.Ref().timestamp.rightNow = true;
			Protocol::GenerateWorldDelta(&faction->visibility,&faction->currentState, delta.Ref(),k,true,k+1 == BaseGrid::NumFamilies,Protocol::WorldDeltaType::FactionCopy);
			delta.Ref().timestamp.atTime = 0;

			//delta.Seal();
			SerializeToChunk(delta.Ref().data0, factionSection->AppendBlock(RID::factionWorldTypeChunk));
			SerializeToChunk(delta.Ref().data2, factionSection->AppendBlock(RID::factionWorldTechniteFactionChunk));
			SerializeToChunk(delta.Ref().data1, factionSection->AppendBlock(RID::factionWorldCounterChunk));
										//factionWorldTypeChunk("WTCK"),
										//factionWorldCounterChunk("WCCK"),


			//factionSection->AppendBlock(RID::worldChunk)->SetData(delta.Get());	//faction world chunk
		}

		static Buffer0<UINT32>	final;
		final.Clear();

		for (index_t i = 0; i < faction->visibility.resourcesDiscovered.Count(); i++)
		{
			if (faction->visibility.resourcesDiscovered[i])
				final << (UINT32)i;
		}

		if (final.IsNotEmpty())
		{
			if (final.Count()*4 < faction->visibility.resourcesDiscovered.Count())
				factionSection->AppendBlock(RID::resourcesDiscoveredCompressed)->SetData(final.pointer(),final.Count()*4);	//resource discovered - compressed
			else
				factionSection->AppendBlock(RID::resourcesDiscoveredFull)->SetData(faction->visibility.resourcesDiscovered);	//resource discovered - full
		}


	});
	//LogEvent(EventType::SessionRelated,"World snapshot created in "+String(timer.GetSecondsSince(t0))+" seconds");


	if (container.SaveToFile(name.c_str()))
	{
		LogEvent(EventType::SessionRelated,"World snapshot written to "+name+" in "+String(timer.GetSecondsSince(t0))+" seconds");
	}
	else
		LogEvent(EventType::InternalFault,"Unable to write world to "+name);
	//Grid::SaveToFiles(name+".grid");
	//Objects::SaveToFiles(name+".objects");




}



namespace State
{


	ChunkName Compare1(Riff::File&a, Riff::File&b, Buffer<Riff::TID>&idStack, const String&indent="")
	{
		ASSERT2__(a.GetID() == b.GetID(),a.GetID().ToString(),b.GetID().ToString());
		std::cout << indent << a.GetID().ToString() << std::endl;
		if (a.IsID(Riff::RIFF_NAME_CHUNK))
		{
			ASSERT__(a.GetSize() == 2);
			ASSERT__(b.GetSize() == 2);
			UINT16 na,nb;
			a.Get(&na);
			b.Get(&nb);
			ASSERT_EQUAL__(na,nb);
			std::cout << indent << "<name="<<na<<">" << std::endl;
			return (ChunkName)na;

		}
		if (a.IsID(RIFF_LIST) || a.IsID(RIFF_RIFF))
		{
			std::cout << indent << "<list>" << std::endl;
			idStack << a.GetID();
			a.Enter();
			b.Enter();
				bool f0 = a.First();
				bool f1 = b.First();
				ChunkName lastName;
				ASSERT_EQUAL__(f0,f1);
				if (f0 && f1)
					do
					{
						ChunkName cn = Compare1(a,b,idStack,indent+" ");
						if (cn != ChunkName::None)
							lastName = cn;

						f0 = a.Next();
						f1 = b.Next();
						ASSERT_EQUAL__(f0,f1);
					}
					while (f0&&f1);

			a.Exit();
			b.Exit();
			idStack.EraseLast();
		}
		else
		{
			static Buffer<BYTE>	data0,data1;
			data0.Clear(); data1.Clear();
			a.Get(data0.AppendRow(a.GetSize()));
			b.Get(data1.AppendRow(b.GetSize()));
			ASSERT_EQUAL__(a.GetSize(),b.GetSize());
			std::cout << indent << "<data>["<<a.GetSize()<<"]" << std::endl;
			ASSERT__(!memcmp(data0.pointer(),data1.pointer(),data0.GetContentSize()));
		}


		return ChunkName::None;
	}


	void Compare(Riff::File&a, Riff::File&b, Buffer<Riff::TID>&idStack, const String&indent/*=""*/)
	{
		bool f0 = a.First();
		bool f1 = b.First();
		ChunkName lastName;
		ASSERT_EQUAL__(f0,f1);
		if (f0 && f1)
			do
			{
				ChunkName cn = Compare1(a,b,idStack,indent+" ");
				if (cn != ChunkName::None)
					lastName = cn;

				f0 = a.Next();
				f1 = b.Next();
				ASSERT_EQUAL__(f0,f1);
			}
			while (f0&&f1);
	}




	void	SimulateLiquidsIfEnabled()
	{
	
		
		if (Config::simulateLiquids)
		{
			Timer::Time t0 = timer.Now();

			static bool changed[BaseGrid::NumFamilies];
			memset(changed,false,sizeof(changed));
			struct TStack
			{
				BYTE	volumeCell[Grid::CellStack::Range];
			};
			static Array<TStack>	tempStacks;
			tempStacks.SetSize(Grid::HighSetup::GridType::CellCount);
			for (index_t i = 0; i < Grid::HighSetup::GridType::CellCount; i++)
			{
				TStack&target = tempStacks[i];
				const Grid::CellStack&source = Grid::actualWorld.cellStack[i];
				for (index_t layer = 0; layer < Grid::CellStack::Range; layer++)
					target.volumeCell[layer] = source.volumeCell[layer].content;
			}

			Concurrency::parallel_for(index_t(0),Grid::HighSetup::GridType::CellCount,[](index_t stackIndex)
			{
				Grid::CellStack&stack = Grid::actualWorld.cellStack[stackIndex];
				Grid::CellStack*nStacks[6] = {nullptr};
				const auto&neighborhood = Grid::high.grid.GetCell(stackIndex).neighborhood;
				DBG_ASSERT_LESS_OR_EQUAL__(neighborhood.numNeighbors,ARRAYSIZE(nStacks));
				for (int i = 0; i < neighborhood.numNeighbors; i++)
					nStacks[i] = Grid::actualWorld.cellStack + neighborhood.neighborID[i];
				TStack&target = tempStacks[stackIndex];
				bool thisChanged = false;
				for (UINT32 layer = 0; layer < Grid::CellStack::Range; layer++)
				{
					if (stack.volumeCell[layer].content == Grid::Content::Clear)
					{
						BYTE fill = 
							layer+1 < Grid::CellStack::Range && Grid::CellStack::Cell::IsLiquid(stack.volumeCell[layer].content) ? stack.volumeCell[layer].content : 0;
						if (!fill)
							for (int i = 0; i < neighborhood.numNeighbors; i++)
								if (Grid::CellStack::Cell::IsLiquid(nStacks[i]->volumeCell[layer].content))
								{
									fill = nStacks[i]->volumeCell[layer].content;
									break;
								}

						if (fill)
						{
							target.volumeCell[layer] = fill;
							thisChanged = true;
						}
					}
				}
				if (thisChanged)
				{
					changed[Grid::high.grid.GetCell(stackIndex).family] = true;
					for (int i = 0; i < neighborhood.numNeighbors; i++)
						changed[Grid::high.grid.GetCell(neighborhood.neighborID[i]).family] = true;
				}
			});
			for (int i = 0; i < BaseGrid::NumFamilies; i++)
			{
				if (changed[i])
				{
					index_t offset = Grid::high.grid.GetFamilyOffset(i);
					count_t cnt = Grid::high.grid.CountFamilyMembers(i);
					for (index_t k = offset; k < offset+cnt; k++)
					{
						Grid::CellStack&target = Grid::actualWorld.cellStack[k];
						const TStack&source = tempStacks[k];
						for (index_t layer = 0; layer < Grid::CellStack::Range; layer++)
							target.volumeCell[layer].content = source.volumeCell[layer];
					}
					Grid::low.family[i].SetOutdated(0);
					Grid::high.family[i].SetOutdated(0);
				}
			}

			static Buffer0<Objects::PWorldEntity>	waste;

			waste.Clear();

			Objects::ForeachWorldEntity([](const Objects::PWorldEntity&entity)
			{
				if (!changed[Grid::low.grid.GetCell(entity->location.stackID).family])
					return;

				const int h = entity->GetHeight();
				const bool broad = entity->IsBroad();

				if (entity->IsIndestructible())
				{
					for (int y = 0; y < h; y++)
					{
						if (broad)
							Grid::actualWorld.Clear(entity->location + y,0);
						else
							Grid::actualWorld.cellStack[entity->location.stackID].MakeLocalHole(entity->location.height+y);
					}
				}
				else
				{
					for (int y = 0; y < h; y++)
					{
						if (broad)
						{
							if (!Grid::actualWorld.ClearNonLava(entity->location + y,0))
							{
								waste << entity;
								break;
							}
						}
						else
						{
							if (Grid::actualWorld.cellStack[entity->location.stackID].GetCellI(entity->location.height+y).content == Grid::Content::Lava)
							{
								waste << entity;
								break;
							}
							Grid::actualWorld.cellStack[entity->location.stackID].MakeLocalHole(entity->location.height+y);
						}
					}
				}
			});
			float t = timer.GetSecondsSince(t0);
			LogEvent(EventType::PerformanceMeasurement,"Time needed to process water: "+String(t));


			for (const auto&e:waste)
				Objects::Remove(e,Objects::RemovalType::UncontrolledExplosion,0,false);
			waste.Clear();
		}

	}

	struct TSortItem
	{
		Objects::PWorldEntity	target;
		int						priority;
		bool					wasHibernating,
								hibernating = false;

		bool					operator>(const TSortItem&other) const
		{
			return priority > other.priority;
		}
		bool					operator<(const TSortItem&other) const
		{
			return priority < other.priority;
		}

	};


	#ifndef NO_HIBERNATION
		void UpdateHibernation()
		{

			static Buffer0<TSortItem>	entityList;
			foreach (Objects::Faction::all,_f)
			{
				Objects::PFaction faction = _f->lock();
				if (faction)
				{
					UINT32 traffic = 0;
					entityList.Clear();
					for (const auto&_s : faction->structures)
					{
						Objects::PStructure s = _s.lock();
						DBG_ASSERT__(s);
						if (s)
						{
							traffic += s->GetUplinkTraffic();
							if (s->IsWorkingStructure())
							{
								Objects::PWorkingStructure ws = To<Objects::WorkingStructure>::Cast(s);
								entityList.Append().target = ws;
								entityList.last().priority = ws->GetUplinkPriority();
								entityList.last().wasHibernating = ws->IsHibernating();
							}
						}
					}
					for (const auto&_v : faction->allMobiles)
					{
						Objects::PMobileEntity v = _v.lock();
						DBG_ASSERT__(v);
						if (v)
						{
							traffic += v->GetUplinkTraffic();
							entityList.Append().target = v;
							entityList.last().priority = v->GetUplinkPriority();
							entityList.last().wasHibernating = v->IsHibernating();
						}
					}
					if (traffic > Config::uplinkBandwidth)
					{
						std::sort(entityList.begin(),entityList.end());
						for (auto&item : entityList)
						{
							traffic -= item.target->GetUplinkTraffic();
							item.hibernating = true;
							if (traffic <= Config::uplinkBandwidth)
								break;
						}
					}

					for (auto&item : entityList)
					{
						if (item.wasHibernating != item.hibernating)
						{
							if (item.target->IsVehicle())
							{
								To<Objects::Vehicle>::Cast(item.target)->Hibernate(item.hibernating,0);
							}
							else
								if (item.target->IsStructure())
								{
									To<Objects::WorkingStructure>::Cast(item.target)->Hibernate(item.hibernating,0);
								}
						}
					}
				}
			}

		}
	#endif

	void PerformNewRoundLogic(float&atTime)
	{

		



		if (Config::basesRequired)
		{
			foreach (Objects::Faction::all,_f)
			{
				Objects::PFaction faction = _f->lock();
				if (faction)
				{
					faction->hasMineralGeneratorCycle = false;
					faction->hasVehicleProductionCycle = false;
					faction->hasTerraformer = false;
				}
			}

			Objects::RefreshCompoundCoverageIfNeeded(atTime);

			Objects::ForeachCompound([](Objects::Compound&c)
			{
				bool hasFuelMine = false,
					hasMineralMine = false,
					hasMineralContainer = Objects::Mine::MineralStorage > 0,
					hasGenerator = false,
					hasVehiclePlant = false;


				#ifdef CONNECTOR_LESS
					hasMineralContainer = true;	//don't need
				#endif
				Objects::ResourceVector dummy,storage;
				foreach (c.containedStructures,_s)
				{
					Objects::PWorkingStructure s = _s->lock();
					if (!s)
						continue;
					Objects::PMine mine = To<Objects::Mine>::DynamicCast(s);
					if (mine && mine->IsLinkedToCore())
					{
						if (mine->naturalResources.fuel >= 10)
							hasFuelMine = true;
						if (mine->naturalResources.minerals >= 5)
							hasMineralMine = true;
					}
					if (To<Objects::Generator>::DynamicCast(s))
						hasGenerator = true;

					#ifndef CONNECTOR_LESS
						s->GetResourceVector(dummy,dummy,storage);
						if (storage.level[Objects::Resource::Minerals] > 0)
							hasMineralContainer = true;
					#endif
					if (To<Objects::VehiclePlant>::DynamicCast(s))
						hasVehiclePlant = true;
				}
				if (hasFuelMine && hasMineralMine && hasMineralContainer && hasGenerator)
				{
					c.containedStructures.first().lock()->owner->hasMineralGeneratorCycle = true;
				}
				if (hasFuelMine && hasMineralMine && hasVehiclePlant && hasGenerator)
				{
					c.containedStructures.first().lock()->owner->hasVehicleProductionCycle = true;
				}
			});

			Objects::ForeachMobileEntityIncludingStored([](const Objects::PMobileEntity&v)
			{
				if (v->GetConstructionSpeed())
				{
					v->owner->hasTerraformer = true;
				}
			});

			for (index_t i = 0; i < Objects::Faction::all.Count(); i++)
			{
				Objects::PFaction faction = Objects::Faction::all[i].lock();
				if (faction)
				{
					bool canLive = faction->CanLive();

					State::PClient client;


					if (!State::playerMap.Query(faction,client))
						client.reset();	//should be implied, but doesn't cost anything either

					if (canLive)
					{
						if (faction->deathInRounds > 0)
						{
							faction->deathInRounds = 0;
							if (client)
								client->SendProgressUpdate(false);
						}
					}
					else
					{
						bool dead = false;
						if (!faction->deathInRounds)
						{
							faction->deathInRounds = 5;
						}
						else
						{
							if (faction->deathInRounds > 1)
							{
								faction->deathInRounds--;
							}
							else
							{
								dead = true;
								String name = client ? client->ToString() : "Faction "+faction->id.ToString();
											
								LogEvent(EventType::GameProgressRelated,name+" died");

								if (client)
								{
									Protocol::ClientFactionResponse cr;
									cr.factionToken.value.Generate();
									cr.startFactionTypes = 0;
									cr.planetConfig = State::persistentPlanetInfo;
									Channels::clientFactionResponse.SendTo(client->peer.lock(),cr);
								}

								Objects::Faction::Kill(faction);
								if (client)
									State::activePlayers.FindAndErase(client);
								State::tokenMap.Unset(faction->accessToken);
								State::playerMap.Unset(faction);
								if (client)
									client->faction.reset();
								i--;

								if (State::Config::closedGame)
								{
									if (Objects::Faction::all.IsEmpty())
									{
										LogEvent(EventType::SessionRelated,"All factions destroyed. Application requires restart.");
										//application_shutting_down = true;
									}
									else
									if (Objects::Faction::all.Count() == 1)
									{
										if (State::phase != Protocol::Phase::EndOfGame)
										{
											LogEvent(EventType::SessionRelated,"Only one faction left. Declaring winner.");
											State::ChangePhase(Protocol::Phase::EndOfGame);
											State::winner = Objects::Faction::all.first().lock();
											State::PClient client;
											//debugCheckProgress = true;
											if (State::playerMap.Query(State::winner,client))
											{
												client->SendProgressUpdate(true);
											}
										}
									}
								}
							}
						}
						if (client)
							client->SendProgressUpdate(dead);
					}
				}
			}




		}


		Objects::ForeachWorldEntity([&atTime](const Objects::PWorldEntity&e)
		{
			if (!(Grid::low.cells[e->location.stackID].cornerFlags & State::unlockedEMPZones))
			{
				if (e->empDamageLevel < 10)
					LogEvent(EventType::EntityProcess,e.get(),"Entering EMP-Zone");
				e->HandleEMPDamage(10,atTime);
			}
					
		});

	
	}


	//void			SpawnNexusBaseAtPoleWithPort(int pole);
	void			SpawnNexusPortAtPole(int pole)
	{
		LogEvent(EventType::SessionRelated,"Spawning Nexus Port at pole "+String(pole));

		Grid::LCellIdI location((UINT32)pole,0);	//first 12 are corner cells
		int h = Objects::FindApplicableGround(location,true);
		if (h == Objects::badHeight)
			h = 0;	//just spawn it near median
		static const int R = 5;
		h = clamp(h,-Grid::CellStack::HalfRange + R + 1,Grid::CellStack::HalfRange - R - 2);

		location.height = h;
		std::shared_ptr<Objects::NexusPort> port = To<Objects::NexusPort>::Cast(
		Objects::NexusPort::globalPrototype->CreateNewInstance(Objects::Faction::nexus,location,0));
		port->CorrectTerrain(false,0,true);
		Objects::InsertConstructed(port,0);
//		if (enableNexus)
		{
			port->SetEnabled(true,0,false,Objects::WorkingStructure::ForceEnable);
			Objects::RefreshCompoundCoverageIfNeeded(0);
			//ASSERT__(port->IsEnabled());
		}
//			const auto&n = Grid::low.grid.GetCell(i).neighborhood;
		Grid::Resource::Require(ConstName::Resource_Fuel).coverage[pole] = 0;
		Grid::Resource::Require(ConstName::Resource_Minerals).coverage[pole] = 0;
		Grid::Resource::Require(ConstName::Resource_Uranium).coverage[pole] = 0;
	}


	void SpawnNexusBaseAtPoleWithPort(int pole)
	{
		LogEvent(EventType::SessionRelated,"Spawning Nexus base at pole "+String(pole));

		ASSERT__(Config::NexusPorts::haveBases);

		Objects::PNexusPort port;
		int h;
		for (h = - Grid::CellStack::HalfRange; h < Grid::CellStack::HalfRange; h++)
		{
			port = To<Objects::NexusPort>::DynamicCast(Objects::GetStructure(Grid::LCellIdI((UINT32)pole,h)));
			if (port)
				break;
		}
		if (!port)
		{
			FATAL__("Unable to find port at supposed nexus pole "+String(pole));
			return;
		}

		Grid::LCellIdI location = port->location;

		bool enableNexus = false;
		{
			//if (random.getFloat() < 0.8f)
			{

				//bool deadly = random.getFloat() > 0.4f;


			//	bool rebuild[BaseGrid::NumFamilies] = { false };

				Grid::LowSetup::TDistanceField field;
				static const int R = 5;

				Grid::low.UpdatePlanarDistanceField(pole,R+1,field);

				Buffer0<index_t>	distances;


				for (index_t i = 0; i < field.NumEntries; i++)
				{
					if (field.distance[i] == 0xFFFFFFFF)
						continue;
					if (field.distance[i] <= R)
						distances << i;
					else
					{
						Grid::actualWorld.Clear(Grid::LCellIdI(i,h),0);
						Grid::actualWorld.Clear(Grid::LCellIdI(i,h+1),0);
					}
				}

				for (index_t i : distances)
				{
					ASSERT_LESS_OR_EQUAL__(field.distance[i],R);
					UINT32 d1 = (R - field.distance[i])+1;

					int h2 = h + d1;
					for (int z = h; z <= h2 + 2 ; z++)
					{
						Grid::actualWorld.Clear(Grid::LCellIdI(i,z),0);
					//	Grid::TagLowCluster(rebuild,Grid::LCellIdI(i,z));
					}
				//	Grid::TagLowCluster(rebuild,Grid::LCellIdI(i,h2-1));
				}
				Buffer0<Objects::PWorkingStructure>	alwaysOn;
				for (index_t i : distances)
				{
					ASSERT_LESS_OR_EQUAL__(field.distance[i],R);

					UINT32 d1 = (R - field.distance[i])+1;

					bool notDirectEdge = Grid::low.grid.GetCell(i).family != 0;
					bool noEdge = notDirectEdge;
					if (noEdge)
					{
						const auto&n = Grid::low.grid.GetCell(i).neighborhood;
						for (int j = 0; j < n.numNeighbors; j++)
							if (Grid::low.grid.GetCell(n.neighborID[j]).family == 0)
								noEdge = false;

					}

					bool notCore = field.distance[i] >= 3;

					int h2 = h + d1;

					for (int z = h-2; z < h2; z++)
					{
						if (z < h || (z >=h2-1 && notCore/* && !deadly*/) || noEdge)
						{
							Grid::actualWorld.Overwrite(Grid::LCellIdI(i,z),Grid::Content::Rock,0);
						}
						else
							if (!notDirectEdge)
								break;

					}
				}
				for (index_t i : distances)
				{
					bool notDirectEdge = Grid::low.grid.GetCell(i).family != 0;
					if (notDirectEdge || field.distance[i] == 1)
						for (int z = h-1; z < h; z++)
						{
							Grid::actualWorld.Overwrite(Grid::LCellIdI(i,z),Grid::Content::Road,0);
						}
				}


				int factoryCount = 0;
				int generatorCount = 0,
					connectorCount = 0;
				for (index_t i : distances)
				{
					//ASSERT_LESS_OR_EQUAL__(field.distance[i],R);
					//UINT32 d1 = (R - field.distance[i])+1;

					#if 1
					if (Grid::low.grid.GetCell(i).family == 0)	//align of family members implies family zero members are along edges between nexus poles
					{
						if (field.distance[i] == 4)
						{
							#ifndef _DEBUG
								std::shared_ptr<Objects::Tower> tower = To<Objects::Tower>::Cast(
									Objects::GetPrototypeByName(ConstName::Type_LauncherTower)->CreateNewInstance(Objects::Faction::nexus,
									Grid::LCellIdI(i,h),
									0));
								tower->CorrectTerrain(false,0,true);
								Objects::InsertConstructed(tower,0);
							#endif
						}
						if (field.distance[i] == 5)
						{
							std::shared_ptr<Objects::Tower> tower = To<Objects::Tower>::Cast(
								Objects::GetPrototypeByName(ConstName::Type_RadarTower)->CreateNewInstance(Objects::Faction::nexus,
								Grid::LCellIdI(i,h),
								0));
							tower->CorrectTerrain(false,0,true);
							Objects::InsertConstructed(tower,0);
						}

						if (field.distance[i] == (2))
						{
							if (generatorCount == 4)
							{
								Objects::PMunitionStorage storage = To<Objects::MunitionStorage>::Cast(
									Objects::MunitionStorage::globalPrototype->CreateNewInstance(Objects::Faction::nexus,
									Grid::LCellIdI(i,h),
									0));
								storage->CorrectTerrain(false,0,true);
								storage->storage = storage->maxStorage;
								Objects::InsertConstructed(storage,0);
								alwaysOn << storage;
							}
							else
							{
								Objects::PGenerator generator = To<Objects::Generator>::Cast(
									Objects::Generator::globalPrototype->CreateNewInstance(Objects::Faction::nexus,
									Grid::LCellIdI(i,h),
									0));
								generator->CorrectTerrain(false,0,true);
								Objects::InsertConstructed(generator,0);
								alwaysOn << generator;
								generatorCount++;
							}
						}

						if (field.distance[i] == 1 /*|| field.distance[i] == 4*/)
						{
							{
								Objects::PConnector connector = To<Objects::Connector>::Cast(
									Objects::Connector::globalPrototype->CreateNewInstance(Objects::Faction::nexus,
									Grid::LCellIdI(i,h),
									0));
								connector->CorrectTerrain(false,0,true);
								Objects::InsertConstructed(connector,0);
								connectorCount++;
							}
						}
						if (field.distance[i] == (3))
						{
							if (factoryCount == 4)
							{
								Objects::PMunitionFactory factory = To<Objects::MunitionFactory>::Cast(
									Objects::MunitionFactory::globalPrototype->CreateNewInstance(Objects::Faction::nexus,
									Grid::LCellIdI(i,h),
									0));
								factory->CorrectTerrain(false,0,true);
								Objects::InsertConstructed(factory,0);
								alwaysOn << factory;
							}
							else
							{
								Objects::PVehiclePlant plant = To<Objects::VehiclePlant>::Cast(
									Objects::VehiclePlant::globalPrototype->CreateNewInstance(Objects::Faction::nexus,
									Grid::LCellIdI(i,h),
									0));
								plant->CorrectTerrain(false,0,true);
								Objects::InsertConstructed(plant,0);
								factoryCount++;
							}
						}
					}
					else
					{
						//if (/*field.distance[i] == 3 || */field.distance[i] == 5)
						//{
						//	Objects::PConnector connector = To<Objects::Connector>::Cast(
						//		Objects::Connector::globalPrototype->CreateNewInstance(Objects::Faction::nexus,
						//		Grid::LCellIdI(i,Objects::FindApplicableGround(Grid::LCellIdI(i,h),true)),
						//		0));
						//	connector->CorrectTerrain(false,0,true);
						//	Objects::InsertConstructed(connector,0);
						//}


					}
					#endif /*0*/
				}
				for (index_t i : distances)
				{
					ASSERT_LESS_OR_EQUAL__(field.distance[i],R);
					UINT32 d1 = (R - field.distance[i]);

					bool noEdge = Grid::low.grid.GetCell(i).family != 0;
					if (noEdge)
					{
						const auto&n = Grid::low.grid.GetCell(i).neighborhood;
						for (int j = 0; j < n.numNeighbors; j++)
							if (Grid::low.grid.GetCell(n.neighborID[j]).family == 0)
								noEdge = false;

					}

					int h2 = h + d1;
					//for (int z = h; z <= h2 + 2 ; z++)
					{
						if (!noEdge)
							Grid::actualWorld.Clear(Grid::LCellIdI(i,h),0);
					//	Grid::TagLowCluster(rebuild,Grid::LCellIdI(i,z));
					}
				//	Grid::TagLowCluster(rebuild,Grid::LCellIdI(i,h2-1));
				}

				for (auto ws : alwaysOn)
				{
					ws->SetEnabled(true,0,false,Objects::WorkingStructure::ForceEnable);
				}
				enableNexus = alwaysOn.IsNotEmpty();


				if (enableNexus)
				{
					port->SetEnabled(true,0,false,Objects::WorkingStructure::ForceEnable);

				}


				Objects::PSkyHub hub = To<Objects::SkyHub>::Cast( Objects::SkyHub::globalPrototype->CreateNewInstance(port->owner,Grid::LCellIdI(location.stackID,Objects::Satellite::BaseResidenceHeight),0) );
				Objects::InsertConstructed(hub,0);
				ASSERT__(hub->connectedTo.lock() == port);

				for (index_t i : distances)
				{
					ASSERT_LESS_OR_EQUAL__(field.distance[i],R);
					UINT32 d1 = (R - field.distance[i]);

					bool noEdge = Grid::low.grid.GetCell(i).family != 0;
					if (!noEdge && field.distance[i] == 2)
					{
						Objects::PSkyGuardian guard = To<Objects::SkyGuardian>::Cast( Objects::SkyGuardian::globalPrototype->CreateNewInstance(port->owner,Grid::LCellIdI(i,Objects::Satellite::BaseResidenceHeight),0));
						Objects::InsertConstructed(guard,0);
					}
				}


			}


		}

		location.height = h;
		//std::shared_ptr<Objects::NexusPort> port = To<Objects::NexusPort>::Cast(
		//Objects::NexusPort::globalPrototype->CreateNewInstance(Objects::Faction::nexus,location,0));
		//port->CorrectTerrain(false,0,true);
		//Objects::InsertConstructed(port,0);
		//if (enableNexus)
		//{
		//	port->SetEnabled(true,0,false,true);
		//	Objects::RefreshCompoundCoverageIfNeeded(0);
		//	ASSERT__(port->IsEnabled());
		//}
//			const auto&n = Grid::low.grid.GetCell(i).neighborhood;
		Grid::Resource::Require(ConstName::Resource_Fuel).coverage[pole] = 0;
		Grid::Resource::Require(ConstName::Resource_Minerals).coverage[pole] = 0;
		Grid::Resource::Require(ConstName::Resource_Uranium).coverage[pole] = 0;





	}

}



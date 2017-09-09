#include <global_root.h>
#include "gameState.h"
#include "networkState.h"
#include "../networkChannels.h"
#include "../aiProcess.h"
#include "../journalSaveGame.h"
#include "../configurationMode.h"
#include <objects/structure/mine.h>
#include <objects/structure/vehiclePlant.h>

void			ApplicationInterface::SignalFactionsChanged()
{
	Game::mostFactionsAlive = std::max(Game::mostFactionsAlive, Game::CountLivingPlayerFactions());
	Reflection::SignalFactionsChanged();
}



namespace Game
{
	UINT32				mostFactionsAlive=0;
	Evolver::Abstract	*evolver = nullptr;
	Objects::PFaction	winner;	//if set indicates game is over
	Objects::WFaction	postEvolutionWinner;	//if winner is declared while evolver is active, this variable is set instead for later
	Timer::Time			lastPhaseChange = timer.Now();
	bool				roundTimerPaused = true;
	float				factionStartMineralValue=1.f;	//determined during loading
	float				effectiveAnimationLength = 1.f;

	INT32				tutorialAt=1;



	void				SetDefaultEvolver()
	{
		evolver = Evolver::GetDefault();
	}

	Evolver::Abstract*	GetEvolver()
	{
		return evolver;
	}

	void			BeginRoundProcessing()
	{
		ASSERT__(!evolverActive);
		evolverActive = true;
		evolver->BeginRoundProcessing();
	}

	void			FinishRoundProcessing()
	{
		ASSERT__(evolverActive);
		evolverActive = false;
		evolver->FinishRoundProcessing();
	}

	bool			ContinueRoundProcessing()
	{
		ASSERT__(evolverActive);
		return evolver->ContinueRoundProcessing();
	}

	bool			IsProcessingRound()
	{
		return evolverActive;
	}


	namespace Config
	{
		int			roundTimer = -1;
		UINT32		uplinkBandwidth = 500;

		bool		closedGame = false,
					startBases = false,
					basesRequired = false,
					exposeActionLeftToHostileFactions = true;
		UINT32		factionsPerZone = 
					#ifdef LOWER_RESOLUTION_PLANET
						1
					#else
						3
					#endif
					;

		UINT32		factionSupportFlags = Protocol::GameFlag::AllFactionSupport;

		namespace Nexus
		{
			bool		haveBases = false
						IF_SATELLITES_P( ,haveSatellitesAtopBases = true )
						;

			bool		enableAI = false;
			UINT32		roundsPerResearchPoint=10;
			float		threatEstimationFactor = 1.f;
			Ctr::Array<Objects::Component::Turret*>	vehicleTurrets;
		}
	}
	


	namespace Network
	{
		void		UpdateResourceCoverage(const Objects::PFaction&f, const Grid::LCellIdI&center,float range, const Protocol::Timestamp&timestamp)
		{
			static Ctr::Buffer0<UINT32>	changed;
			if (!f)
				return;
			f->knownResources.Update(center,range,changed);
			if (changed.IsEmpty())
				return;
			::Network::PClient client;
			if (!::Network::playerMap.Query(f,client))
				return;
			TCP::PPeer peer = client->peer.lock();
			if (!peer)
				return;

			Protocol::ResourceUpdate	package;
			package.timestamp = timestamp;
			package.cells.SetSize(changed.Count());
			for (index_t i = 0; i < changed.Count(); i++)
				package.cells[i].cellIndex = changed[i];
			foreach (Grid::Resource::all,res)
			{
				package.resourceName = res->GetCName();
				for (index_t i = 0; i < changed.Count(); i++)
				{
					BYTE sourceCov = res->coverage[changed[i]];
					package.cells[i].coverage = sourceCov;
				}

				Channels::resourceUpdate.SendTo(peer,package);
			}
		}


		void SendStatisticsTo(TCP::Peer & peer, const Objects::PFaction & faction)
		{
			Objects::Statistics::EnterNetworkMode();
			Channels::Statistics::beginSequence.SendTo(peer,faction->id);

			faction->statistics.VisitRounds([&peer](const Objects::Statistics::Round&r)
			{
				Channels::Statistics::round.SendTo(peer,r);
			},true);
			faction->statistics.VisitEntityTypes([&peer](const Objects::Statistics::EntityType&t)
			{
				Channels::Statistics::entityType.SendTo(peer,t);
			});
			faction->statistics.VisitFactionInteractions([&peer](const Objects::Statistics::FactionInteraction&i)
			{
				Channels::Statistics::factionInteraction.SendTo(peer,i);
			});
			peer.SendSignal(Protocol::ChannelID::EndStatisticsSequence);
		}


		void		HandleSessionSuspension()
		{
			if (Config::Nexus::enableAI)
				AIProcess::End();
		}

		void		HandleSessionClosure()
		{
			roundTimerPaused = true;
			mostFactionsAlive = 0;
			tutorialAt=1;
			winner.reset();
			HandleSessionSuspension();
		}

		void		HandleSessionRestart()
		{
			if (Config::Nexus::enableAI)
				AIProcess::Begin(Objects::Faction::nexus->accessToken);
			ApplicationInterface::SignalFactionsChanged();
		}


		UINT32		AssembleGameFlags(bool allowSpectatorFactions)
		{
			UINT32 rs = 0;
			if (Config::closedGame)
				rs |= Protocol::GameFlag::ClosedGame;
			if (RuntimeConfig::empZones)
				rs |= Protocol::GameFlag::EMPZones;
			if (Config::basesRequired)
				rs |= Protocol::GameFlag::BasesRequired;
			if (Config::startBases)
				rs |= Protocol::GameFlag::StartBases;
			if (Config::Nexus::enableAI)
				rs |= Protocol::GameFlag::NexusAI;
			if (ConfigurationMode::IsActive() && allowSpectatorFactions)
				rs |= Protocol::GameFlag::ConfigurationMode;
			if (RuntimeConfig::tutorialMode)
				rs |= Protocol::GameFlag::TutorialMode;
			if (World::Config::simulateLiquids)
				rs |= Protocol::GameFlag::SimulateLiquids;

			rs |= Config::factionSupportFlags;
			if (!allowSpectatorFactions)
				rs &= ~Protocol::GameFlag::SupportSpectatorFactions;

			if (RuntimeConfig::phase == RuntimeConfig::Phase::EndOfGame)
			{
				rs |= Protocol::GameFlag::GameHasEnded | Protocol::GameFlag::SupportSpectatorFactions;	//always support spectator in this case
			}
			return rs;
		}


		void		AssembleGameConfig(Protocol::GameConfig&gameConfig, bool allowSpectatorFactions)
		{
			gameConfig.researchScript = Objects::ResearchState::GetScriptIdentifier();
			gameConfig.gameGoal = (UINT32)RuntimeConfig::gameGoal;
			gameConfig.gameFlags = AssembleGameFlags(allowSpectatorFactions);
			gameConfig.worldDiscovery = (BYTE)World::Config::worldDiscovery;

		}

		void	AssembleGameInfo(Protocol::GameInfo&info, bool allowSpectatorFactions)
		{
			AssembleGameConfig(info.gameConfig,allowSpectatorFactions);
			info.activePhase = RuntimeConfig::phase;
			info.numFactionsOnline = (UINT16)::Network::activePlayersAndSpectators.Count();
			info.numFactionsCreated = (UINT16)Objects::Faction::all.Count() + (Config::Nexus::enableAI ? 1 : 0);
			info.roundNumber = RuntimeConfig::roundNumber;
			info.worldID = World::worldID;
		}


		void	ChangePhase(RuntimeConfig::Phase to, float timeWindow/* = -1*/)
		{
			if (RuntimeConfig::phase == RuntimeConfig::Phase::EndOfGame)
			{
				DBG_FATAL__("Trying to change from end-of-game to "+String(to.ToString()));
				return;
			}
			if (RuntimeConfig::IsACommandPhase(to))
			{
				Objects::ForeachFaction([](const Objects::PFaction&f)
				{
					f->DropPerRoundData();	//make sure instantaneous destruction commands are processed with correct visibility information

					if (f->researchState.changed)
					{
						f->researchState.changed = false;
						Protocol::ResearchState state;
						state.ownerID = f->id;
						f->researchState.Export(state.serializedData);
						ASSERT_LESS__(state.serializedData.Count(),state.serializedData.MaxElementCount);
						Channels::researchState.SendTo(::Network::server,state,(UINT32)::Network::UserLevel::CommonFaction);
					}
					if (f->researchState.queueChanged)
					{
						f->researchState.queueChanged = false;
						::Network::PClient client;
						if (::Network::playerMap.Query(f,client))
						{
							Protocol::ResearchQueue queue;
							f->researchState.ExportResearchQueue(queue);
							::Channels::researchQueue.SendTo(client->peer.lock(),queue);
						}
					}
				});
			}

			LogEvent(EventSignificance::Debug,"Changing RuntimeConfig::phase to "+String(to.ToString())+" with time window "+String(timeWindow));
			Protocol::PhaseChange	ph;
			ph.newPhase = to;
			ph.roundNumber = RuntimeConfig::roundNumber;
			ph.timeFactor = timeWindow;
			Channels::phaseChange.SendTo(::Network::server,ph,(UINT32)::Network::UserLevel::PreFaction);
			RuntimeConfig::phase = to;
			lastPhaseChange = timer.Now();

			#ifdef _DEBUG
				if (to == RuntimeConfig::Phase::AnimationPhase)	//should have enough time during animation RuntimeConfig::phase
				{
					//Objects::VerifyVisibilityNetwork();	 //seems stable
					Objects::CheckMobileEntitySanity();

					Objects::VerifyGrid();
				}
			#endif


			if (to == RuntimeConfig::Phase::AnimationPhase)
			{
				FileIO::CheckAutoSave(true);	//round number is formally incremented at the beginning of the command phase, but animation phase produces a delay anyways, so better save here
			}


			if (to == RuntimeConfig::Phase::EndOfGame)
			{
				if (!winner)
				{
					UINT64 biggestScore = 0;
					Objects::PFaction newWinner;

					Objects::ForeachFaction([&](const Objects::PFaction&f)
					{
						if (f == Objects::Faction::nexus)
							return;
						const UINT64 score = f->GetWinningScore();
						if (score > biggestScore)
						{
							biggestScore = score;
							newWinner = f;
						}
					});
					if (newWinner)
						DeclareWinner(newWinner);
				}

				foreach (::Network::activePlayersAndSpectators,p)
				{
					(*p)->SendProgressUpdate("");
				}
			}

			//if (to == RuntimeConfig::Phase::CommandPhase)
				//DebugCheckWorld();	//this assumes the world does not change. if it does, this will fail

		}




		void	Describe(Protocol::ProductionTask&task, const Objects::MobileEntityProducer::WorkItem&element)
		{
			task.type = element.producing->GetType();
			task.id = element.id;
		}

		void	Describe(Protocol::MobileTask&task, const Objects::MobileEntity&v, const Grid::LCellIdI&origin, const Objects::MobileEntity::Task&t,index_t taskIndex)
		{
			task.foci.SetSize(t.foci.Count());
			for (index_t i = 0; i < task.foci.Count(); i++)
				task.foci[i] = t.foci[i].target;
			task.mode = (BYTE)t.mode;
			task.name = t.name;
			task.type = t.type;
			task.taskFlags = 0;
			if (t.job)
			{
				Objects::VehicleJob::PSequence seq = To<Objects::VehicleJob::Sequence>::DynamicCast(t.job);
				if (!seq)
				{
					task.resourceCostLeft = t.job->resourceCostLeft > 0 ? t.job->resourceCostLeft : 0;
					task.roundsToJobCompletion = t.job->roundsToJobCompletion;
				}
				else
				{
				
					seq->Guess(v, origin, taskIndex, task.roundsToJobCompletion,task.resourceCostLeft);
				}
				if (t.job->movesToFocusWhenDone)
					task.taskFlags |= Protocol::TaskFlags::VisualizePathToFocus;
				if (t.job->DelayedBySleep())
					task.taskFlags |= Protocol::TaskFlags::DelayedBySleep;
			

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
					task.visualizationName = constJob->constructWhat->GetType();
				}
				else
				{
					std::shared_ptr<Objects::VehicleJob::SelfDetonate> detonate = To<Objects::VehicleJob::SelfDetonate>::DynamicCast(t.job);

					if (detonate)
					{
						task.visualizationName = detonate->doomsday ? ConstName::DetonationSize_Doomsday : ConstName::DetonationSize_Regular;
					}
				}

			}
			else
			{
				task.roundsToJobCompletion = -1;
			}
		}

		void	Describe(Protocol::EntityState&state, const Objects::WorldEntity&e, bool owned, const Protocol::Timestamp&timestamp, UINT32 changeFlags)
		{
			state.health = e.health;
			state.id = e.id;
			state.empDamageLevel = e.empDamageLevel;
			state.location = (changeFlags & Objects::WorldEntity::PositionChanged) ? e.location : Grid::LCellIdI::Invalid;
			state.entityStateFlags = 0;
			if (owned)
			{
				if (e.unstable)
					state.entityStateFlags |= Protocol::EntityStateFlags::IsUnstable;
				if (e.IsSleeping())
					state.entityStateFlags |= Protocol::EntityStateFlags::IsSleeping;
			}
			state.timestamp = timestamp;
		}

		void	Describe(Protocol::MobileState&state, const Objects::MobileEntity&v, bool owned, const Protocol::Timestamp&timestamp, UINT32 changeFlags)
		{
			if (v.IsAboveGrid())
			{
				DBG_ASSERT_GREATER1__(v.location.height, Grid::CellStack::TopHeight,v.GetName());
			}
	
			state.flags = 0;
			if (owned)
			{
				auto&info = state.myMobileEntity.Set0();
				Objects::PHangar h = v.storedIn.lock();
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
						loc = task.foci.GetNewest().target;
				}
				#ifndef ONE_COMPOUND_PER_FACTION
					info.resourceVector = v.currentStorage;
				#endif
				if (v.isAutomated)
					state.flags |= Protocol::VehicleFlags::Automated;
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
				state.flags |= Protocol::VehicleFlags::Hibernating;

			Describe(state.entityState,v,owned,timestamp,changeFlags);
			if (owned)
			{
				if ((v.myTasks.IsEmpty() && v.completeOnMove) || v.AwaitsUserInteraction())
					state.entityState.entityStateFlags |= Protocol::EntityStateFlags::IsUnstable;
			}

		}

		void	Describe(Protocol::NewMobileEntity&cnst, const Objects::MobileEntity&v, bool owned, const Protocol::Timestamp&timestamp, UINT32 changeFlags)
		{
			cnst.type = v.GetType();
			cnst.owner = v.owner->id;
			Describe(cnst.state,v,owned,timestamp,changeFlags);
		}



		void	Describe(Protocol::AttackTask&task, const Objects::Tower::AttackTask&element)
		{
			//task.id = element.id;
			//task.target = element.target;
			task = element.target;
		}

		void	Describe(Protocol::StructureState&state, const Objects::Structure&s, bool owned, const Protocol::Timestamp&timestamp, UINT32 changeFlags)
		{
			const Objects::WorkingStructure*w = dynamic_cast<const Objects::WorkingStructure*>(&s);
			//state.productionDone = 0;
			state.flags = 0;
			if (!w || w->IsEnabled())
				state.flags |= Protocol::StructureFlags::Enabled;
			if (w && w->IsHibernating())
				state.flags |= Protocol::StructureFlags::Hibernating;

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
					subState.attackActionLeft = t->attackActionLeft;
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
			cnst.type = s.GetType();
			cnst.owner = s.owner->id;
			Describe(cnst.state,s,owned,timestamp,0xFFFFFFFF);

		}

	}


	void RegisterClassGain(const Objects::PWorldEntity&e, Objects::Faction::UnitClass cl, const String&debugName)
	{
		if (Config::basesRequired && !(e->owner->hasUnitClasses & cl))
		{
			const auto&f = e->owner;
			LogEvent(EventSignificance::Debug,e.get(),"Recognized as "+debugName+". Reevaluating survivial conditions");
			f->hasUnitClasses |= cl;
			String whyNot;
			bool canLive = f->CanLive(whyNot);
		
			if (canLive)	//conditions changed
			{
				if (f->deathInRounds > 0)
				{
					f->deathInRounds = 0;
					::Network::PClient client;
					if (::Network::playerMap.Query(f,client))
						client->SendProgressUpdate(whyNot);
				}
			}
			else
				LogEvent(EventSignificance::Debug,e.get(),"Nope: "+whyNot);
		}
	}

	void	ResetFactionStartMineralValue()
	{
		if (Config::startBases)
		{
			factionStartMineralValue = Objects::Mine::globalPrototype->GetConstructionCost()
			+ 
			Objects::Generator::globalPrototype->GetConstructionCost()
			+ 
			Objects::MineralContainer::globalPrototype->GetConstructionCost()
			+
			Objects::Vehicle::GetPrice(Objects::Component::hoverbase, Objects::Component::terraformer,Objects::OwnerParameter())
			+
			Objects::Vehicle::GetPrice(Objects::Component::hoverbase, Objects::Component::resourceScanner,Objects::OwnerParameter());
		}
		else
		{
			factionStartMineralValue = 
				3 * Objects::Vehicle::GetPrice(Objects::Component::hoverbase, Objects::Component::terraformer,Objects::OwnerParameter())
				+
				Objects::Vehicle::GetPrice(Objects::Component::hoverbase, Objects::Component::radar,Objects::OwnerParameter())
				+
				Objects::Vehicle::GetPrice(Objects::Component::hoverbase, Objects::Component::resourceScanner,Objects::OwnerParameter())
				+
				Objects::Vehicle::GetPrice(Objects::Component::hoverbase, Objects::Component::artillery,Objects::OwnerParameter());
		}
		//if (State::Config::Nexus::haveBases)
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




	void	ProcessChanges(bool thoroughly)
	{
		World::Network::CheckAllFactionCoverageAndSendAsNeeded();

		Objects::ForeachMobileEntityIncludingStored([/*&anythingChanged*/](const Objects::PMobileEntity&_v)
		{
			Objects::MobileEntity&v = *_v;
			if (v.changeFlags)
			{
				//anythingChanged = true;
				LogEvent(EventSignificance::Common,&v,"State change detected ("+String(v.changeFlags)+")");

				bool othersNeedToKnow = (v.changeFlags & Network::NotifyOthers) != 0;

				TCP::Serial<Protocol::MobileState>	w0,w1;
				Protocol::Timestamp timestamp(RuntimeConfig::IsInAnyCommandPhase(),v.changeTimer);
				Network::Describe(w0.Ref(),v,false,timestamp,v.changeFlags);
				Network::Describe(w1.Ref(),v,true,timestamp,v.changeFlags);
				w0.Seal();
				w1.Seal();
				if (!othersNeedToKnow)
				{
					::Network::SendToOwner(Channels::setMobileState,v,w1);
				}
				else
					foreach(::Network::activePlayersAndSpectators,cl)
					{
						if (::Network::CanSeeAsOwner(*cl,v))
							Channels::setMobileState.SendTo((*cl)->peer.lock(),w1);
						else
							if (::Network::CanSee(*cl,_v,0))
								Channels::setMobileState.SendTo((*cl)->peer.lock(),w0);
					}			
				v.ResetAllChange();
			}
		},false);

		Objects::structures.Foreach([](const Objects::PStructure&_s)
		{
			Objects::Structure&s = *_s;
			if (s.changeFlags)
			{
				//anythingChanged = true;
				LogEvent(EventSignificance::Common,&s,"State change detected");
				
				bool othersNeedToKnow = (s.changeFlags & Network::NotifyOthers) != 0;

				Protocol::Timestamp timestamp(RuntimeConfig::IsInAnyCommandPhase(),s.changeTimer);

				TCP::Serial<Protocol::StructureState>	w0,w1;
				Network::Describe(w0.Ref(),s,false,timestamp,s.changeFlags);
				Network::Describe(w1.Ref(),s,true,timestamp,s.changeFlags);
				w0.Seal();
				w1.Seal();
				if (!othersNeedToKnow)
				{
					::Network::SendToOwner(Channels::setStructureState,s,w1);
				}
				else
					foreach(::Network::activePlayersAndSpectators,cl)
					{
						if (::Network::CanSeeAsOwner(*cl,s))
							Channels::setStructureState.SendTo((*cl)->peer.lock(),w1);
						else
							if (::Network::CanSee(*cl,_s,0))
								Channels::setStructureState.SendTo((*cl)->peer.lock(),w0);
						//Channels::setStructureState.SendTo((*cl)->peer.lock(),Receiver::HasAccess(*cl,s) ? w1 : w0);
					}
				s.ResetAllChange();
			}
		});

		if (thoroughly)
		{
			Objects::ForeachFaction([](const Objects::PFaction&faction)
			{
				::Network::PClient client;
				if (::Network::playerMap.Query(faction,client))
					World::Network::CheckCoverageSendTerrain(faction,client->peer.lock());
				else
					World::Network::CheckCoverageSendTerrain(faction,TCP::PPeer());
			});
		}
		else
			foreach (::Network::activePlayersAndSpectators,cl)
			{
				//if ((*cl)->faction->global.coverageChanged)
				{
					const Objects::PFaction&faction = (*cl)->faction;
					if (faction)
						World::Network::CheckCoverageSendTerrain(faction,(*cl)->peer.lock());
						//anythingChanged = true;
				}
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
							if (item.target->IsGroundBoundVehicle())
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


	count_t CountLivingPlayerFactions()
	{

		count_t livingPlayerFactions = 0;

		for (index_t i = 0; i < Objects::Faction::all.Count(); i++)
		{
			const Objects::PFaction&faction = Objects::Faction::all[i];	//only player factions here: nexus is not included in ::all
			if (!faction->isDead)
				livingPlayerFactions ++;
		}

		return livingPlayerFactions;
	}


	void	DeclareWinner(const Objects::PFaction&w)
	{
		DBG_ASSERT__(w);
		DBG_ASSERT__(!winner);
		if (evolverActive)
		{
			postEvolutionWinner = w;
			return;
		}
		postEvolutionWinner.reset();
		LogEvent(EventClass{EventSignificance::Important,EventFlavor::ServerPrivate},"Declared winner: "+w->GetLogName());
		LogEvent(EventClass::Preset::winningFaction,"Congratulations. "+w->GetName()+" wins this game!");
		winner = w;
		Channels::winningFaction.SendTo(::Network::server,Game::winner->id,Channels::FactionLevel);
		if (RuntimeConfig::phase != RuntimeConfig::Phase::EndOfGame)
			Network::ChangePhase(RuntimeConfig::Phase::EndOfGame);
	}


	void PerformNewRoundLogic(float&atTime)
	{
		if (Config::basesRequired)
		{
			foreach (Objects::Faction::all,_f)
			{
				const Objects::PFaction&faction = *_f;
				faction->hasUnitClasses = Objects::Faction::UnitClass::None;
			}

			Objects::RefreshCompoundCoverageIfNeeded(atTime);

			Objects::ForeachCompound([](Objects::Compound&c)
			{
				if (c.workingStructures.IsEmpty())
					return;	//pure technite factions don't have units. Also, depending on server settings, some factions might not either
				typedef Objects::Faction::UnitClass	UC;

				UC	uc = UC::None;
				if (Objects::Mine::MineralStorage > 0)
					uc |= UC::MineralContainer;
					 


				#ifdef ONE_COMPOUND_PER_FACTION
					uc |= UC::MineralContainer;	//don't need
				#endif
				Objects::ResourceVector dummy,storage;
				Objects::OwnerParameter o;
				foreach (c.workingStructures,_s)
				{
					Objects::PWorkingStructure s = _s->lock();
					if (!s)
					{
						LogEvent(EventClass::Preset::internalError, "Unable to lock working structure in compound for living-check");
						continue;
					}
					o = s->owner;
					Objects::PMine mine = To<Objects::Mine>::DynamicCast(s);
					if (mine && mine->IsLinkedToCore())
					{
						if (mine->naturalResources.fuel >= 10)
							uc |= Objects::Faction::UnitClass::FuelMine;
						if (mine->naturalResources.minerals >= 5)
							uc |= Objects::Faction::UnitClass::MineralMine;
					}
					if (To<Objects::Generator>::DynamicCast(s))
						uc |= Objects::Faction::UnitClass::Generator;

					#ifndef ONE_COMPOUND_PER_FACTION
						s->GetResourceVector(dummy,dummy,storage);
						if (storage.level[Objects::Resource::Minerals] > 0)
							hasMineralContainer = true;
					#endif
					if (To<Objects::VehiclePlant>::DynamicCast(s))
						uc |= Objects::Faction::UnitClass::VehiclePlant;
				}
				Objects::PFaction f = o;//c.workingStructures.first().lock()->owner;
				if (f)
					f->hasUnitClasses |= uc;
			});

			Objects::ForeachMobileEntityIncludingStored([](const Objects::PMobileEntity&v)
			{
				if (v->GetOperationSpeed(Objects::Component::Construct))
					v->owner->hasUnitClasses |= Objects::Faction::UnitClass::Terraformer;
			},false);


			count_t livingPlayerFactions = CountLivingPlayerFactions();
			for (index_t i = 0; i < Objects::Faction::all.Count(); i++)
			{
				Objects::PFaction faction = Objects::Faction::all[i];	//only player factions here: nexus is not included in ::all
				if (faction)
				{
					String whyNot;
					bool canLive = faction->CanLive(whyNot);

					::Network::PClient client;


					if (!::Network::playerMap.Query(faction,client))
						client.reset();	//should be implied, but doesn't cost anything either

					if (canLive)
					{
						if (faction->deathInRounds > 0)
						{
							faction->deathInRounds = 0;
							if (client)
								client->SendProgressUpdate(whyNot);
						}
					}
					else
					{
						if (!faction->deathInRounds)
						{
							faction->deathInRounds = 5;
							LogEvent(EventSignificance::Common, "Faction death imminent. " + String(faction->deathInRounds) + " (initial) round(s) remaining");
						}
						else
						{
							if (faction->deathInRounds > 1)
							{
								faction->deathInRounds--;
								LogEvent(EventSignificance::Common, "Faction death imminent. " + String(faction->deathInRounds) + " round(s) remaining");
							}
							else
							{
								String name = client ? client->ConvertToString() : "Faction "+ToString(faction->id);
											
								LogEvent(EventClass::Preset::factionDeath,name+" died: "+whyNot);

								/*
								telling the client that the faction status has been revoked forces it to flush all entities which will cause issues with subsequently sent animation events.
								the two possible solutions are to either not send any animation events to this client, or to not tell it that its faction status has been revoked.
								the former will cause a very uncomfortable client transition back to faction choice screen and no visual representation of what happened.
								therefore it's probably better to just kill off the faction. but will have to check
								if (client)
								{
									Protocol::ClientFactionResponse cr;
									cr.factionToken.value.Generate();
									cr.startFactionTypes = 0;
									cr.planetConfig = Grid::currentPlanetConfig;
									Channels::clientFactionResponse.SendTo(client->peer.lock(),cr);
								}
								*/

								Objects::Faction::Kill(faction);
								ASSERT__(livingPlayerFactions > 0)
								livingPlayerFactions--;
								#if 0
								if (client)
									::Network::activePlayersAndSpectators.FindAndErase(client);
								::Network::tokenMap.Unset(faction->accessToken);
								::Network::playerMap.Unset(faction);
								faction->online = false;
								if (client)
									client->faction.reset();
								i--;
								#endif /*0*/
							}
						}
						if (client)
							client->SendProgressUpdate(whyNot);
					}
				}
			}

			if (Config::closedGame && RuntimeConfig::phase != RuntimeConfig::Phase::EndOfGame)
			{
				if (livingPlayerFactions == 0)
				{
					LogEvent(EventClass::Preset::factionDeath,"All factions destroyed. Application requires restart.");
					Network::ChangePhase(RuntimeConfig::Phase::EndOfGame);
				}
				else
				if (livingPlayerFactions == 1)
				{
					LogEvent(EventClass(EventSignificance::Important,EventFlavor::Gain),"Only one faction left. Declaring winner.");

					winner.reset();
					for (index_t i = 0; i < Objects::Faction::all.Count(); i++)
					{
						Objects::PFaction faction = Objects::Faction::all[i];
						if (faction && !faction->isDead)
						{
							DeclareWinner(faction);
						}
					}

					//::Network::PClient client;
					//if (::Network::playerMap.Query(winner,client))
					//{
					//	client->SendProgressUpdate("");
					//}
				}
			}


		}

		UpdateResidualEMPDamage(atTime);
	}


	void			UpdateResidualEMPDamage(float&atTime)
	{
		Objects::ForeachWorldEntity([&atTime](const Objects::PWorldEntity&e)
		{
			if (e->owner == Objects::Faction::nexus && e->IsAboveGrid())
				return;	//i know this is a hack, but i gotta do something to prevent nexus satellites from crashing.
			if (Grid::AffectedByEMP(e->location))
			{
				if (e->empDamageLevel < 10)
					LogEvent(EventSignificance::Common,e.get(),"Entering EMP-Zone");
				e->HandleEMPDamage(10,atTime);
			}
			elif (e->owner == Objects::Faction::nexus)
				e->HealAllEMPDamage(atTime);	//immediately heal when a new area gets unlocked
		},false);
	}


	


	//void			SpawnNexusBaseAtPoleWithPort(int pole);
	void			SpawnNexusPortAtPole(int pole)
	{
		LogEvent(EventSignificance::Common,"Spawning Nexus Port at pole "+String(pole));

		Grid::LCellIdI location((UINT32)pole,0);	//first 12 are corner cells
		int h = Objects::FindApplicableGround(location,true);
		if (h == Objects::badHeight)
			h = 0;	//just spawn it near median
		static const int R = 5;
		h = M::clamp(h,-Grid::CellStack::HalfRange + R + 1,Grid::CellStack::HalfRange - R - 2);

		location.height = h;
		std::shared_ptr<Objects::NexusPort> port = To<Objects::NexusPort>::Cast(
		Objects::NexusPort::globalPrototype->CreateNewInstance(Objects::Faction::nexus,location,0));
		port->CorrectTerrain(false,0);
		float t = 0;
		Objects::InsertConstructed(port,t);
//		if (enableNexus)
		{
			port->SetEnabled(true,t,false,Objects::WorkingStructure::ForceEnable);
			Objects::RefreshCompoundCoverageIfNeeded(t);
			//ASSERT__(port->IsEnabled());
		}
//			const auto&n = Grid::low.grid.GetCell(i).neighborhood;
		Grid::Resource::Require(ConstName::Resource_Fuel).coverage[pole] = 0;
		Grid::Resource::Require(ConstName::Resource_Minerals).coverage[pole] = 0;
		Grid::Resource::Require(ConstName::Resource_Uranium).coverage[pole] = 0;
	}





	void SpawnNexusBaseAtPoleWithPort(int pole)
	{
		LogEvent(EventSignificance::Common,"Spawning Nexus base at pole "+String(pole));

		auto sensorLock = Objects::Faction::nexus->LockSensorUpdates();

		ASSERT__(Config::Nexus::haveBases);

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
				#ifdef LOWER_RESOLUTION_PLANET
					static const int R = 4;
					static const int MissileDistance = 4;
					static const int RadarDistance = 3;
					static const int OuterFactoryDistance = 2;
					static const int MaxFactories = 3;
				#else
					static const int R = 5;
					static const int MissileDistance = 5;
					static const int RadarDistance = 4;
					static const int OuterFactoryDistance = 3;
					static const int MaxFactories = 4;
				#endif

				Grid::low.UpdatePlanarDistanceField(pole,R+1,field);

				Ctr::Buffer0<index_t>	distances;


				for (index_t i = 0; i < field.NumEntries; i++)
				{
					if (field.distance[i] == 0xFFFFFFFF)
						continue;
					if (field.distance[i] <= R)
						distances << i;
					else
					{
						World::actual.Clear(Grid::LCellIdI(i,h),0);
						World::actual.Clear(Grid::LCellIdI(i,h+1),0);
					}
				}

				for (index_t i : distances)
				{
					ASSERT_LESS_OR_EQUAL__(field.distance[i],R);
					UINT32 d1 = (R - field.distance[i])+1;

					int h2 = h + d1;
					for (int z = h; z <= h2 + 2 ; z++)
					{
						World::actual.Clear(Grid::LCellIdI(i,z),0);
					//	Grid::TagLowCluster(rebuild,Grid::LCellIdI(i,z));
					}
				//	Grid::TagLowCluster(rebuild,Grid::LCellIdI(i,h2-1));
				}
				Ctr::Buffer0<Objects::PWorkingStructure>	alwaysOn;
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
							World::actual.Overwrite(Grid::LCellIdI(i,z),Grid::Content::Rock,0);
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
							World::actual.Overwrite(Grid::LCellIdI(i,z),Grid::Content::Road,0);
						}
				}


				int factoryCount = 0;
				int generatorCount = 0,
					connectorCount = 0;
				float t = 0;
				const auto radarPrototype = Objects::Prototypes::structures.Find({ConstName::Type_Tower,ConstName::Turret_Radar});
				ASSERT__(radarPrototype);
				for (index_t i : distances)
				{
					//ASSERT_LESS_OR_EQUAL__(field.distance[i],R);
					//UINT32 d1 = (R - field.distance[i])+1;

					#if 1
					if (Grid::low.grid.GetCell(i).family == 0)	//align of family members implies family zero members are along edges between nexus poles
					{
						if (field.distance[i] == MissileDistance)
						{
							#ifndef _DEBUG
								std::shared_ptr<Objects::Tower> tower = To<Objects::Tower>::Cast(
									Objects::GetPrototypeByName(ConstName::Type_LauncherTower)->CreateNewInstance(Objects::Faction::nexus,
									Grid::LCellIdI(i,h),
									0));
								tower->CorrectTerrain(false,0);
								Objects::InsertConstructed(tower,t);
							#endif
						}
						if (field.distance[i] == RadarDistance)
						{
							std::shared_ptr<Objects::Tower> tower = To<Objects::Tower>::Cast(
								radarPrototype->CreateNewInstance(Objects::Faction::nexus,
								Grid::LCellIdI(i,h),
								0));
							tower->CorrectTerrain(false,0);
							Objects::InsertConstructed(tower,t);
						}

						if (field.distance[i] == OuterFactoryDistance)
						{
							if (factoryCount == MaxFactories)
							{
								#ifdef LOWER_RESOLUTION_PLANET
									Objects::PGenerator generator = To<Objects::Generator>::Cast(
										Objects::Generator::globalPrototype->CreateNewInstance(Objects::Faction::nexus,
										Grid::LCellIdI(i,h),
										0));
									generator->CorrectTerrain(false,0);
									Objects::InsertConstructed(generator,t);
									generatorCount++;
									alwaysOn << generator;	//only need three generators switched on
								#else
									Objects::PMunitionFactory factory = To<Objects::MunitionFactory>::Cast(
										Objects::MunitionFactory::globalPrototype->CreateNewInstance(Objects::Faction::nexus,
										Grid::LCellIdI(i,h),
										0));
									factory->CorrectTerrain(false,0);
									Objects::InsertConstructed(factory,t);
									alwaysOn << factory;
								#endif
							}
							else
							{
								Objects::PVehiclePlant plant = To<Objects::VehiclePlant>::Cast(
									Objects::VehiclePlant::globalPrototype->CreateNewInstance(Objects::Faction::nexus,
									Grid::LCellIdI(i,h),
									0));
								plant->CorrectTerrain(false,0);
								Objects::InsertConstructed(plant,t);
								factoryCount++;
							}
						}

						#ifndef LOWER_RESOLUTION_PLANET
							if (field.distance[i] == (2))
							{
								if (generatorCount == 4)
								{
									Objects::PMunitionStorage storage = To<Objects::MunitionStorage>::Cast(
										Objects::MunitionStorage::globalPrototype->CreateNewInstance(Objects::Faction::nexus,
										Grid::LCellIdI(i,h),
										0));
									storage->CorrectTerrain(false,0);
									storage->storage = storage->maxStorage;
									Objects::InsertConstructed(storage,t);
									alwaysOn << storage;
								}
								else
								{
									Objects::PGenerator generator = To<Objects::Generator>::Cast(
										Objects::Generator::globalPrototype->CreateNewInstance(Objects::Faction::nexus,
										Grid::LCellIdI(i,h),
										0));
									generator->CorrectTerrain(false,0);
									Objects::InsertConstructed(generator,t);
									generatorCount++;
									if (generatorCount < 4)
										alwaysOn << generator;	//only need three generators switched on
								}
							}
						#endif

						if (field.distance[i] == 1 /*|| field.distance[i] == 4*/)
						{
							{
								Objects::PConnector connector = To<Objects::Connector>::Cast(
									Objects::Connector::globalPrototype->CreateNewInstance(Objects::Faction::nexus,
									Grid::LCellIdI(i,h),
									0));
								connector->CorrectTerrain(false,0);
								Objects::InsertConstructed(connector,t);
								connectorCount++;
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
							World::actual.Clear(Grid::LCellIdI(i,h),0);
					//	Grid::TagLowCluster(rebuild,Grid::LCellIdI(i,z));
					}
				//	Grid::TagLowCluster(rebuild,Grid::LCellIdI(i,h2-1));
				}

				for (auto ws : alwaysOn)
				{
					ws->SetEnabled(true,t,false,Objects::WorkingStructure::ForceEnable);
				}
				enableNexus = alwaysOn.IsNotEmpty();


				if (enableNexus)
				{
					port->SetEnabled(true,t,false,Objects::WorkingStructure::ForceEnable);

				}


				#ifndef NO_SATELLITES
					if (Config::Nexus::haveSatellitesAtopBases)
					{
						Objects::PSkyHub hub = To<Objects::SkyHub>::Cast( Objects::SkyHub::globalPrototype->CreateNewInstance(port->owner,Grid::LCellIdI(location.stackID,Objects::Satellite::BaseResidenceHeight),0) );
						Objects::InsertConstructed(hub,t);
						ASSERT__(hub->connectedTo.lock() == port);

						for (index_t i : distances)
						{
							ASSERT_LESS_OR_EQUAL__(field.distance[i],R);
							UINT32 d1 = (R - field.distance[i]);

							bool noEdge = Grid::low.grid.GetCell(i).family != 0;
							if (!noEdge && field.distance[i] == 2)
							{
								Objects::PSkyGuardian guard = To<Objects::SkyGuardian>::Cast( Objects::SkyGuardian::globalPrototype->CreateNewInstance(port->owner,Grid::LCellIdI(i,Objects::Satellite::BaseResidenceHeight),0));
								Objects::InsertConstructed(guard,t);
							}
						}
					}
				#endif
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





	void			SpawnFactionStartUnits(const Objects::PFaction&f, index_t startLocation)
	{
		const auto&n = Grid::low.grid.GetCell(startLocation).neighborhood;
		if (Config::startBases)
		{
			Grid::LCellIdI location(startLocation,0);	//first 12 are corner cells
			int h = Objects::FindApplicableGround(location,true);
			location.height = h;
			static const int want = 10;
			static const int stackHeight = want / Objects::NaturalResourceVector::TranslationFactor + (want % Objects::NaturalResourceVector::TranslationFactor ? 1 : 0);
			Journal::currentRound.RegisterResourceCoverageChange(true,location.stackID,Objects::Resource::Fuel,stackHeight);
			Journal::currentRound.RegisterResourceCoverageChange(true,location.stackID,Objects::Resource::Minerals,stackHeight);
			Objects::ForeachFaction([location](const Objects::PFaction&f)
			{
				Journal::currentRound.RegisterResourceCoverageKnowledgeLoss(true,f,location.stackID);
			});

			Grid::LCellIdI at0 = Grid::LCellIdI(n.neighborID[0],h);
			Grid::LCellIdI at1 = Grid::LCellIdI(n.neighborID[1],h);
			Grid::LCellIdI at2 = Grid::LCellIdI(n.neighborID[2],h);
			Grid::LCellIdI at3 = Grid::LCellIdI(n.neighborID[3],h);
			Grid::LCellIdI at4 = Grid::LCellIdI(n.neighborID[4],h);

			float t = 0;
			Objects::PMine mine = To<Objects::Mine>::Cast(
				Objects::Mine::globalPrototype->CreateNewInstance(f,location,0));
			mine->CorrectTerrain(false,0);
			Objects::InsertConstructed(mine,t);
			mine->SetEnabledOverride(true);
				
			Objects::PGenerator generator = To<Objects::Generator>::Cast(
				Objects::Generator::globalPrototype->CreateNewInstance(f,at1,0));
			generator->CorrectTerrain(false,0);
			Objects::InsertConstructed(generator,t);
			generator->SetEnabledOverride(true);
				
			std::shared_ptr<Objects::MineralContainer> container = To<Objects::MineralContainer>::Cast(
				Objects::MineralContainer::globalPrototype->CreateNewInstance(f,at2,0));
			container->storage = container->maxStorage;
			container->CorrectTerrain(false,0);
			Objects::InsertConstructed(container,t);
			Objects::RefreshCompoundCoverage(t);


			Objects::InsertNewVehicle(f,at0,Objects::Component::hoverbase, Objects::Component::terraformer,true,t);
			Objects::InsertNewVehicle(f,at3,Objects::Component::hoverbase, Objects::Component::terraformer,true,t);
			Objects::InsertNewVehicle(f,at4,Objects::Component::hoverbase, Objects::Component::resourceScanner,true,t);

		}
		else
		{
			Grid::LCellIdI at0 = Grid::LCellIdI(n.neighborID[0],0);
			Grid::LCellIdI at1 = Grid::LCellIdI(n.neighborID[1],0);
			Grid::LCellIdI at2 = Grid::LCellIdI(n.neighborID[2],0);
			Grid::LCellIdI at3 = Grid::LCellIdI(n.neighborID[3],0);
			Grid::LCellIdI at4 = Grid::LCellIdI(n.neighborID[4],0);

			float t = 0;
			Objects::InsertNewVehicle(f,Grid::LCellIdI((UINT32)startLocation,0),Objects::Component::hoverbase, Objects::Component::terraformer,true,t);
			Objects::InsertNewVehicle(f,at1,Objects::Component::hoverbase, Objects::Component::radar,true,t);
			Objects::InsertNewVehicle(f,at3,Objects::Component::hoverbase, Objects::Component::resourceScanner,true,t);
			Objects::InsertNewVehicle(f,at2,Objects::Component::hoverbase, Objects::Component::terraformer,true,t);
			Objects::InsertNewVehicle(f,at4,Objects::Component::hoverbase, Objects::Component::terraformer,true,t);
			Objects::InsertNewVehicle(f,at0,Objects::Component::hoverbase, Objects::Component::artillery,true,t);
		}
	}


}


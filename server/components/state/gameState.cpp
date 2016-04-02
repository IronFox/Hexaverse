#include <global_root.h>
#include "gameState.h"
#include "networkState.h"
#include "../networkChannels.h"
#include "../aiProcess.h"
#include "../journalSaveGame.h"


namespace Game
{
	UINT32				mostFactionsAlive=0;
	Evolver::Abstract	*evolver = nullptr;
	Objects::PFaction	winner;	//if set indicates game is over
	Timer::Time			lastPhaseChange = timer.Now();
	bool				roundTimerPaused = true;
	float				factionStartMineralValue=1.f;	//determined during loading
	float				effectiveAnimationLength = 1.f;



	void			SignalFactionsChanged()
	{
		mostFactionsAlive = std::max(mostFactionsAlive,(UINT32)Objects::Faction::all.Count());
	}


	namespace Config
	{
		int			roundTimer = -1;
		UINT32		uplinkBandwidth = 500;

		bool		closedGame = false,
					startBases = false,
					basesRequired = false,
					exposeActionLeftToHostileFactions = true;
		UINT32		factionSupportFlags = AllFactionSupportFlags;

		namespace Nexus
		{
			bool		haveBases = false
						IF_SATELLITES_P( ,haveSatellitesAtopBases = true )
						;

			bool		enableAI = false;
			float		threatEstimationFactor = 1.f;
			Array<Objects::Component::Turret*>	vehicleTurrets;
		}
	}
	


	namespace Network
	{
		void		UpdateResourceCoverage(const Objects::PFaction&f, const Grid::LCellIdI&center,float range, const Protocol::Timestamp&timestamp)
		{
			static Buffer0<UINT32>	changed;
			if (!f)
				return;
			f->visibility.UpdateResourceCoverage(center,range,changed);
			if (changed.IsEmpty())
				return;
			::Network::PClient client;
			if (!::Network::playerMap.query(f,client))
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


		void		HandleSessionSuspension()
		{
			if (Config::Nexus::enableAI)
				AIProcess::End();
		}

		void		HandleSessionClosure()
		{
			roundTimerPaused = true;
			mostFactionsAlive = 0;
			winner.reset();
			HandleSessionSuspension();
		}

		void		HandleSessionRestart()
		{
			if (Config::Nexus::enableAI)
				AIProcess::Begin(Objects::Faction::nexus->accessToken);
			SignalFactionsChanged();
		}


		void	AssembleGameInfo(Protocol::GameInfo&info)
		{
			info.activePhase = RuntimeConfig::phase;
			info.numFactionsOnline = (UINT16)::Network::activePlayersAndSpectators.Count();
			info.numFactionsCreated = (UINT16)Objects::Faction::all.Count() + (Config::Nexus::enableAI ? 1 : 0);
			info.roundNumber = (UINT32)RuntimeConfig::roundNumber;
			info.worldID = World::worldID;
			info.gameGoal = (UINT32)RuntimeConfig::gameGoal;
			info.gameFlags = 0;
			if (Config::closedGame)
				info.gameFlags |= Protocol::GameFlag::ClosedGame;
			if (RuntimeConfig::empZones)
				info.gameFlags |= Protocol::GameFlag::EMPZones;
			if (Config::basesRequired)
				info.gameFlags |= Protocol::GameFlag::BasesRequired;
			if (Config::startBases)
				info.gameFlags |= Protocol::GameFlag::StartBases;
			if (Config::Nexus::enableAI)
				info.gameFlags |= Protocol::GameFlag::NexusAI;

			info.gameFlags |= Config::factionSupportFlags;
		}


		void	ChangePhase(RuntimeConfig::Phase to, float timeWindow/* = -1*/)
		{
			LogEvent(EventSignificance::Debug,"Changing RuntimeConfig::phase to "+String(to.ToString())+" with time window "+String(timeWindow));
			Protocol::PhaseChange	ph;
			ph.newPhase = to;
			ph.roundNumber = (UINT32)RuntimeConfig::roundNumber;
			ph.timeFactor = timeWindow;
			Channels::phaseChange.SendTo(::Network::server,ph,1);
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
			else if (RuntimeConfig::IsInAnyCommandPhase())
			{
				Objects::ForeachFaction([](const Objects::PFaction&f)
				{
					f->DropPerRoundData();	//make sure instantaneous destruction commands are processed with correct visibility information
				});
			}



			//if (to == RuntimeConfig::Phase::CommandPhase)
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
				task.foci[i] = t.foci[i].target;
			task.mode = (BYTE)t.mode;
			task.name = t.name;
			task.type = t.type;
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
				
					seq->Guess(v, origin, taskIndex, task.roundsToJobCompletion.value,task.resourceCostLeft.value);
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
					task.visualizationName = constJob->constructWhat->GetCTypeName();
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
			state.entityStateFlags = 0;
			if (owned)
			{
				if (e.unstable)
					state.entityStateFlags |= Protocol::EntityStateFlag::IsUnstable;
				if (e.IsSleeping())
					state.entityStateFlags |= Protocol::EntityStateFlag::IsSleeping;
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
			if (owned)
			{
				if ((v.myTasks.IsEmpty() && v.completeOnMove) || v.AwaitsUserInteraction())
					state.entityState.entityStateFlags |= Protocol::EntityStateFlag::IsUnstable;
			}

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
			cnst.typeName = s.GetCTypeName();
			cnst.owner = s.owner->id;
			Describe(cnst.state,s,owned,timestamp,0xFFFFFFFF);

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
			Objects::Vehicle::GetPrice(Objects::Component::hoverbase, Objects::Component::terraformer)
			+
			Objects::Vehicle::GetPrice(Objects::Component::hoverbase, Objects::Component::resourceScanner);
		}
		else
		{
			factionStartMineralValue = 
				3 * Objects::Vehicle::GetPrice(Objects::Component::hoverbase, Objects::Component::terraformer)
				+
				Objects::Vehicle::GetPrice(Objects::Component::hoverbase, Objects::Component::radar)
				+
				Objects::Vehicle::GetPrice(Objects::Component::hoverbase, Objects::Component::resourceScanner)
				+
				Objects::Vehicle::GetPrice(Objects::Component::hoverbase, Objects::Component::artillery);
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
		World::Network::SendChangedFamilies();

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
		});

		foreach (Objects::structures, _s)
		{
			Objects::Structure&s = **_s;
			if (s.changeFlags)
			{
				//anythingChanged = true;
				LogEvent(EventSignificance::Common,&s,"State change detected");
				
				bool othersNeedToKnow = (s.changeFlags & Network::NotifyOthers) != 0;

				Protocol::Timestamp timestamp(RuntimeConfig::IsInAnyCommandPhase(),s.changeTimer);

				TCP::Serial<Protocol::StructureState>	w0,w1;
				Network::Describe(w0.Ref(),**_s,false,timestamp,s.changeFlags);
				Network::Describe(w1.Ref(),**_s,true,timestamp,s.changeFlags);
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
							if (::Network::CanSee(*cl,*_s,0))
								Channels::setStructureState.SendTo((*cl)->peer.lock(),w0);
						//Channels::setStructureState.SendTo((*cl)->peer.lock(),Receiver::HasAccess(*cl,s) ? w1 : w0);
					}
				s.ResetAllChange();
			}
		}

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
					faction->hasFuelMine = false;
					faction->hasMineralMine = false;
					faction->hasMineralContainer = false;
					faction->hasGenerator = false;
					faction->hasVehiclePlant = false;
					faction->hasTerraformer = false;
				}
			}

			Objects::RefreshCompoundCoverageIfNeeded(atTime);

			Objects::ForeachCompound([](Objects::Compound&c)
			{
				if (c.workingStructures.IsEmpty())
					return;	//pure technite factions don't have units. Also, depending on server settings, some factions might not either
				bool hasFuelMine = false,
					hasMineralMine = false,
					hasMineralContainer = Objects::Mine::MineralStorage > 0,
					hasGenerator = false,
					hasVehiclePlant = false;


				#ifdef ONE_COMPOUND_PER_FACTION
					hasMineralContainer = true;	//don't need
				#endif
				Objects::ResourceVector dummy,storage;
				Objects::OwnerParameter o;
				foreach (c.workingStructures,_s)
				{
					Objects::PWorkingStructure s = _s->lock();
					if (!s)
						continue;
					o = s->owner;
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

					#ifndef ONE_COMPOUND_PER_FACTION
						s->GetResourceVector(dummy,dummy,storage);
						if (storage.level[Objects::Resource::Minerals] > 0)
							hasMineralContainer = true;
					#endif
					if (To<Objects::VehiclePlant>::DynamicCast(s))
						hasVehiclePlant = true;
				}
				Objects::PFaction f = o;//c.workingStructures.first().lock()->owner;
				if (f)
				{
					f->hasFuelMine = hasFuelMine;
					f->hasMineralMine = hasMineralMine;
					f->hasMineralContainer = hasMineralContainer;
					f->hasGenerator = hasGenerator;
					f->hasVehiclePlant = hasVehiclePlant;
				}
			});

			Objects::ForeachMobileEntityIncludingStored([](const Objects::PMobileEntity&v)
			{
				if (v->GetOperationSpeed(Objects::Component::Construct))
				{
					v->owner->hasTerraformer = true;
				}
			});

			for (index_t i = 0; i < Objects::Faction::all.Count(); i++)
			{
				Objects::PFaction faction = Objects::Faction::all[i].lock();
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
								client->SendProgressUpdate(false,whyNot);
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
											
								LogEvent(EventClass::Preset::factionDeath,name+" died: "+whyNot);

								if (client)
								{
									Protocol::ClientFactionResponse cr;
									cr.factionToken.value.Generate();
									cr.startFactionTypes = 0;
									cr.planetConfig = Grid::currentPlanetConfig;
									Channels::clientFactionResponse.SendTo(client->peer.lock(),cr);
								}

								Objects::Faction::Kill(faction);
								SignalFactionsChanged();
								if (client)
									::Network::activePlayersAndSpectators.FindAndErase(client);
								::Network::tokenMap.Unset(faction->accessToken);
								::Network::playerMap.Unset(faction);
								faction->online = false;
								if (client)
									client->faction.reset();
								i--;

								if (Config::closedGame)
								{
									if (Objects::Faction::all.IsEmpty())
									{
										LogEvent(EventClass::Preset::factionDeath,"All factions destroyed. Application requires restart.");
										//application_shutting_down = true;
									}
									else
									if (Objects::Faction::all.Count() == 1)
									{
										if (RuntimeConfig::phase != RuntimeConfig::Phase::EndOfGame)
										{
											LogEvent(EventClass(EventSignificance::Important,EventFlavor::Gain),"Only one faction left. Declaring winner.");
											Network::ChangePhase(RuntimeConfig::Phase::EndOfGame);
											winner = Objects::Faction::all.first().lock();
											::Network::PClient client;
											//debugCheckProgress = true;
											if (::Network::playerMap.Query(winner,client))
											{
												client->SendProgressUpdate(true,whyNot);
											}
										}
									}
								}
							}
						}
						if (client)
							client->SendProgressUpdate(dead,whyNot);
					}
				}
			}




		}


		Objects::ForeachWorldEntity([&atTime](const Objects::PWorldEntity&e)
		{
			if (e->owner == Objects::Faction::nexus)
				return;	//i know this is a hack, but i gotta do something to prevent nexus satellites from crashing.
			if (Grid::AffectedByEMP(e->location))
			{
				if (e->empDamageLevel < 10)
					LogEvent(EventSignificance::Common,e.get(),"Entering EMP-Zone");
				e->HandleEMPDamage(10,atTime);
			}
					
		});

	
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
		h = clamp(h,-Grid::CellStack::HalfRange + R + 1,Grid::CellStack::HalfRange - R - 2);

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
				float t = 0;
				for (index_t i : distances)
				{
					//ASSERT_LESS_OR_EQUAL__(field.distance[i],R);
					//UINT32 d1 = (R - field.distance[i])+1;

					#if 1
					if (Grid::low.grid.GetCell(i).family == 0)	//align of family members implies family zero members are along edges between nexus poles
					{
						if (field.distance[i] == 5)
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
						if (field.distance[i] == 4)
						{
							std::shared_ptr<Objects::Tower> tower = To<Objects::Tower>::Cast(
								Objects::GetPrototypeByName(ConstName::Type_RadarTower)->CreateNewInstance(Objects::Faction::nexus,
								Grid::LCellIdI(i,h),
								0));
							tower->CorrectTerrain(false,0);
							Objects::InsertConstructed(tower,t);
						}

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
						if (field.distance[i] == (3))
						{
							if (factoryCount == 4)
							{
								Objects::PMunitionFactory factory = To<Objects::MunitionFactory>::Cast(
									Objects::MunitionFactory::globalPrototype->CreateNewInstance(Objects::Faction::nexus,
									Grid::LCellIdI(i,h),
									0));
								factory->CorrectTerrain(false,0);
								Objects::InsertConstructed(factory,t);
								alwaysOn << factory;
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


			Objects::SpawnVehicle(f,at0,Objects::Component::hoverbase, Objects::Component::terraformer,true,t);
			Objects::SpawnVehicle(f,at3,Objects::Component::hoverbase, Objects::Component::terraformer,true,t);
			Objects::SpawnVehicle(f,at4,Objects::Component::hoverbase, Objects::Component::resourceScanner,true,t);

		}
		else
		{
			Grid::LCellIdI at0 = Grid::LCellIdI(n.neighborID[0],0);
			Grid::LCellIdI at1 = Grid::LCellIdI(n.neighborID[1],0);
			Grid::LCellIdI at2 = Grid::LCellIdI(n.neighborID[2],0);
			Grid::LCellIdI at3 = Grid::LCellIdI(n.neighborID[3],0);
			Grid::LCellIdI at4 = Grid::LCellIdI(n.neighborID[4],0);

			float t = 0;
			Objects::SpawnVehicle(f,Grid::LCellIdI((UINT32)startLocation,0),Objects::Component::hoverbase, Objects::Component::terraformer,true,t);
			Objects::SpawnVehicle(f,at1,Objects::Component::hoverbase, Objects::Component::radar,true,t);
			Objects::SpawnVehicle(f,at3,Objects::Component::hoverbase, Objects::Component::resourceScanner,true,t);
			Objects::SpawnVehicle(f,at2,Objects::Component::hoverbase, Objects::Component::terraformer,true,t);
			Objects::SpawnVehicle(f,at4,Objects::Component::hoverbase, Objects::Component::terraformer,true,t);
			Objects::SpawnVehicle(f,at0,Objects::Component::hoverbase, Objects::Component::artillery,true,t);
		}
	}


}


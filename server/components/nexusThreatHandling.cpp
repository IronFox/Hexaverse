#include <global_root.h>
#include "nexusThreatHandling.h"
#include <objects.h>
#include <runtimeConfig.h>
#include "state/gameState.h"
#include "journalSaveGame.h"
#include <objects/structure/vehiclePlant.h>


namespace Nexus
{
	
	namespace NexusStructures
	{
		#ifdef ONE_COMPOUND_PER_FACTION
			Ctr::Buffer0<Objects::WWorkingStructure>	portList[12];
		#endif



		void Reset(const Ctr::ArrayRef<Objects::PNexusPort>&ports)
		{
			#ifdef ONE_COMPOUND_PER_FACTION
				for (int i = 0; i < 12; i++)
					portList[i].Clear();

				if (!Objects::Faction::nexus || ports.IsEmpty())
					return;

				TVec3<> centers[12];
				DBG_ASSERT_LESS_OR_EQUAL__(ports.Count(),12);
				for (index_t i = 0; i < ports.Count(); i++)
					centers[i] = Grid::low.cells[ports[i]->location.stackID].centerVertex.xyz;
				foreach (Objects::Faction::nexus->structures,s0)
				{
					Objects::PStructure s = s0->lock();
					if (!s || !s->IsWorkingStructure())
						continue;
					float closestAt = std::numeric_limits<float>::lowest();
					index_t closest = InvalidIndex;
					for (index_t i = 0; i < ports.Count(); i++)
					{
						float d = Vec::dot(Grid::low.cells[s->location.stackID].centerVertex.xyz,centers[i]);
						if (d > closestAt)
						{
							closestAt = d;
							closest = i;
						}
					}
					portList[closest].Append(To<Objects::WorkingStructure>::Cast(s));
				}
			#endif
		}


		const Ctr::BasicBuffer<Objects::WWorkingStructure>&
			OfPort(const Objects::PNexusPort&p, index_t portIndex)
		{
			#ifdef ONE_COMPOUND_PER_FACTION
				return portList[portIndex];
			#else
				return p->GetCompound().workingStructures;
			#endif
		}


	}



	void EstimatePresence(Objects::WorldEntity&e, const Grid::LCellIdI&location, const Ctr::ArrayRef<index_t>&cornerField, float*hostileNearNexus, float*constructorNearNexus, float*friendlyNearNexus)
	{
		const TVec3 < >&loc = Grid::low.cells[location.stackID].centerVertex.xyz;
		float entityRange = e.GetScanningRange();
		float impact = e.GetConstructionCost();
		Objects::PTurretBase b = e.GetTurretBase();
		if (b && b->turret->CanDealDamage())
		{
			impact += b->GetDamagePotentialPerRound();
			entityRange = M::Max(entityRange,b->GetFlatAttackRange());
		}
		impact += entityRange;
		const TVec3<> worldLoc = Grid::ToWorld(location);
		const float avg = Grid::low.neighborCellDistances.GetAverage();
		if (impact > 0)
			foreach (cornerField, at)
			{
				float worldDistance = Vec::distance(Grid::low.cells[*at].centerVertex.xyz * Grid::MeanPlanetRadius,worldLoc);
				if (e.owner != Objects::Faction::nexus)
					worldDistance = M::Max(0,worldDistance-entityRange);
				//float angularDistance = Vec::angle(Grid::low.cells[*at].centerVertex.xyz,loc) * M_PIF/180.f;
				float stepDistance = worldDistance / Grid::MeanPlanetRadius / avg * 0.75f;

				bool isRed = stepDistance < 4;
				bool isYellow = stepDistance < 6;
				bool isNoticed = stepDistance < 9;

				float opacity = (float(isRed?3:0) + float(isYellow ? 2 : 0) + float(isNoticed))/3.f;	//0->1/3->1->2
				//opacity = Grid::powi(opacity,2);
				index_t index = at-cornerField.begin();
				if (opacity > 0)
				{
					if (e.owner != Objects::Faction::nexus)
					{
						hostileNearNexus[index] += opacity * impact;

						if (b && b->turret->CanDo(Objects::Component::Construct))
						{
							if (isYellow)
								constructorNearNexus[index] += opacity;
						}
					}
					else
					{
						if (e.IsMobile())
							friendlyNearNexus[index] += opacity * impact;
					}
				}
			}
	}



	static Objects::PNexusPort ClosestPort(const Objects::PWorldEntity&e, const Ctr::ArrayRef<Objects::PNexusPort>&cornerPorts)
	{
		float d2 = std::numeric_limits<float>::max();
		Objects::PNexusPort rs;
		foreach (cornerPorts,p)
		{
			float nd2 = Vec::quadraticDistance(e->GetWorldPosition(),(*p)->GetWorldPosition());
			if (nd2 < d2)
			{
				d2 = nd2;
				rs = *p;
			}
		}
		return rs;
	}




	

	void ProcessCoverage()
	{
		if (Game::Config::Nexus::enableAI
			//#ifdef _DEBUG
			//	|| State::Config::Nexus::haveBases
			//#endif

		)
		{
			//LogEvent(EventType::SessionRelated,"Processing AI coverage");
			float	hostileNearNexus[12] = { 0 }, // will assume nexus-ports are where they are supposed to be and use corner locations
				friendlyNearNexus[12] = { 0 },
				constructorNearNexus[12] = {0};

			if (Game::Config::startBases)
			{
			

			//			Objects::PMine mine = To<Objects::Mine>::Cast(
			//			Objects::Mine::globalPrototype->CreateNewInstance(client->faction,location,0));
			//		mine->CorrectTerrain(false,0,true);
			//		Objects::InsertConstructed(mine,0);
			//		
			//		Objects::PGenerator generator = To<Objects::Generator>::Cast(
			//			Objects::Generator::globalPrototype->CreateNewInstance(client->faction,at1,0));
			//		generator->CorrectTerrain(false,0,true);
			//		generator->SetEnabledOverride(true);
			//		Objects::InsertConstructed(generator,0);
			//		
			//		std::shared_ptr<Objects::MineralContainer> container = To<Objects::MineralContainer>::Cast(
			//			Objects::MineralContainer::globalPrototype->CreateNewInstance(client->faction,at2,0));
			//		container->CorrectTerrain(false,0,true);
			//		Objects::InsertConstructed(container,0);




			//		Objects::SpawnVehicle(client->faction,at3,Objects::Component::hoverbase, Objects::Component::terraformer,true,0);
			//		Objects::SpawnVehicle(client->faction,at4,Objects::Component::hoverbase, Objects::Component::resourceScanner,true,0);


			}


			Objects::PNexusPort compactPortList[12];
			index_t portAt = 0;
			Objects::structures.Foreach([&portAt,&compactPortList](const Objects::PStructure&s)
			{
				//bool nexus = (*s)->owner == Objects::Faction::nexus;
				Objects::PNexusPort p = To<Objects::NexusPort>::DynamicCast(s);
				if (p && BaseGrid::IsIsolatedCorner(p->location.stackID) && p->owner == Objects::Faction::nexus)
				{
					ASSERT_LESS__(portAt, BaseGrid::IsolatedCornerCount);
					compactPortList[portAt] = p;
					portAt++;
					//index_t portIn = (*s)->location.stackID;
					//ASSERT_LESS__(portIn, 12);
					//ASSERT__(!ports[portIn]);
					//ports[portIn] = p;
				}
			});
			ASSERT_LESS_OR_EQUAL__(portAt, BaseGrid::IsolatedCornerCount);	//some might have changed ownership
			LogEvent(EventSignificance::Debug,"Identified "+String(portAt)+" Nexus Ports under Nexus faction control");

			const Ctr::ArrayRef<Objects::PNexusPort>	ports (compactPortList,portAt);
			Ctr::Array<index_t> portLocations(portAt);
			for (index_t i = 0; i < ports.Count(); i++)
			{
				portLocations[i] = ports[i]->location.stackID;
				friendlyNearNexus[i] = Game::factionStartMineralValue /*- State::Config::nexusStartMineralValue*/;
			}

			Objects::ForeachWorldEntity([&](const Objects::PWorldEntity&e)
			{
				EstimatePresence(*e,e->location,portLocations, hostileNearNexus,constructorNearNexus,friendlyNearNexus);
			},false);

			static Ctr::Buffer0<UINT32> neighborField;
			NexusStructures::Reset(ports);
			for (index_t j = 0; j < portAt; j++)
			{
				Objects::PNexusPort p = compactPortList[j];
				if (p->empDamageLevel > 0)
					continue;
				if (RuntimeConfig::roundNumber - p->lastSpawnRound < 5 || p->owner != Objects::Faction::nexus)
					continue;	//not allowed to spawn anything for the moment

				float threatLevel = hostileNearNexus[j]*Game::Config::Nexus::threatEstimationFactor;
				LogEvent(EventSignificance::Debug, "Threat level for " + ports[j]->GetName() + " considered " + String(threatLevel)+" (friendlies here: "+String(friendlyNearNexus[j])+", hostile constructor(s) at: "+String(constructorNearNexus[j])+")");
				if (Game::Config::Nexus::haveBases)
				{
					Objects::RefreshCompoundCoverageIfNeeded(0);

					Objects::Compound&n = p->GetCompound();
					count_t producing = 0,
							existing = 0;
					for (Objects::WWorkingStructure _ws : NexusStructures::OfPort(p,j))
					{
						Objects::PWorkingStructure	ws = _ws.lock();
						if (!ws)
							continue;
						#ifndef NO_HIBERNATION
							if (ws->ToHibernating()->IsHibernating())
								continue;
						#endif
						if (Grid::AffectedByEMP(ws->location) || ClosestPort(ws,ports) != p)
							continue;
						if (ws->health < ws->GetMaxHealth())
							friendlyNearNexus[j] -= ws->GetConstructionCost();
						Objects::PMobileEntityProducer plant = To<Objects::MobileEntityProducer>::DynamicCast(ws);
						if (plant)
						{
							existing++;
							if (plant->AwaitsUserInteraction())
							{
								float t = 0;
								Objects::PMobileEntity v = plant->SpawnFinishedProductionAnywhere(t);
								if (!v)
								{
									producing++;	//dont count
									continue;
								}
								Journal::currentRound.RegisterChoiceIntersection(false,ws,v->location,Objects::CursorMode::Idle,v);
								EstimatePresence(*v,v->location,portLocations, hostileNearNexus,constructorNearNexus,friendlyNearNexus);
							}
							if (plant->queue.IsNotEmpty())
							{
								producing++;
								ASSERT__(plant->queue.first().producing->owner == Objects::Faction::nexus);
								EstimatePresence(*plant->queue.first().producing,ws->location,portLocations, hostileNearNexus,constructorNearNexus,friendlyNearNexus);
							}
						}
					}

	//				if (producing < existing)
					{
						while (producing < existing && (friendlyNearNexus[j] < threatLevel || (constructorNearNexus[j] > 0.5f && friendlyNearNexus[j] < Game::factionStartMineralValue + 20.f)))
						{
							LogEvent(EventSignificance::Debug, "Increasing production for " + p->GetName());
							bool anyFound = false;

							for (Objects::WWorkingStructure _ws : NexusStructures::OfPort(p,j))
							//for (Objects::WWorkingStructure _ws : n.workingStructures)
							{
								Objects::PWorkingStructure ws = _ws.lock();
								if (!ws)
									continue;
								Objects::PMobileEntityProducer plant = To<Objects::MobileEntityProducer>::DynamicCast(ws);
		//						Objects::MobileEntityProducer*plant = ws->ToProducer();
								Objects::PVehiclePlant vplant = To<Objects::VehiclePlant>::DynamicCast(ws);
								if (plant && vplant && plant->queue.IsEmpty())
								{
									index_t choice = vplant->entityRandom.NextIndex(Game::Config::Nexus::vehicleTurrets.Count());
									//random.getIndex(State::Config::Nexus::vehicleTurrets.Count() - 1);
									ASSERT_LESS__(choice,Game::Config::Nexus::vehicleTurrets.Count());
									//Objects::Component::Turret* turret = State::Config::Nexus::vehicleTurrets[choice];
									//Objects::MobileEntityProducer::WorkItem item = vplant->EnqueueProductionOfVehicle(&Objects::Component::hoverbase,turret);
									//Objects::PVehicle v = To<Objects::Vehicle>::Cast(item.producing);
									ConstName	parameters[2] = {ConstName::VehicleBase_HoverBase, Game::Config::Nexus::vehicleTurrets[choice]->constName};
									LogEvent(EventSignificance::Common,vplant.get(),"Enqueueing vehicle production job due to high threat level");
									Journal::currentRound.RegisterProductionJobStart(true,vplant,plant,parameters,2);
									
									if (vplant->queue.IsNotEmpty())	//shit happens
									{
										EstimatePresence(*vplant->queue.last().producing,vplant->location,portLocations, hostileNearNexus,constructorNearNexus,friendlyNearNexus);
										anyFound = true;
										producing++;
										break;
									}
								}
							}
							if (!anyFound)
							{
								LogEvent(EventSignificance::Debug,  p->GetName()+" no longer has idle plants capable of queueing jobs. Aborting");
								break;

							}
						}
					}

			
					//#ifdef ONE_COMPOUND_PER_FACTION
					//	break;	//only one compound for nexus faction. all done
					//#endif

				}
				else
				while (friendlyNearNexus[j] < threatLevel || (constructorNearNexus[j] > 0.2f && friendlyNearNexus[j] < Game::factionStartMineralValue + 20.f))
				{
					LogEvent(EventSignificance::Debug, "Spawning nexus unit near " + p->GetName());
					p->lastSpawnRound = RuntimeConfig::roundNumber;


					Grid::LCellIdI location = Grid::LCellIdI::Invalid;
					Grid::low.GetExtendedNeighborhood(p->location.stackID, 3, neighborField);
					foreach(neighborField, n)
					{
						Grid::LCellIdI candidate(*n, p->location.height);
						Grid::AssertIsValid(candidate);
						candidate.height = Objects::FindApplicableGround(candidate, true);

						if (candidate.height != Objects::badHeight && !Objects::GetVehicle(candidate))
						{
							location = candidate;
							break;
						}

					}

					if (location != Grid::LCellIdI::Invalid)
					{
						
						Objects::Component::Turret* turret = Game::Config::Nexus::vehicleTurrets[p->entityRandom.NextIndex(Game::Config::Nexus::vehicleTurrets.Count())];
						float t = 0;
						Objects::PVehicle v = Objects::InsertNewVehicle(Objects::Faction::nexus, location, &Objects::Component::hoverbase, turret, true, t);
						Journal::currentRound.LogEntityCreation(v);
						EstimatePresence(*v,v->location,  portLocations,hostileNearNexus,constructorNearNexus, friendlyNearNexus);
					//	Objects::VerifyVisibilityNetwork();
					}
					else
					{
						LogEvent(EventSignificance::Debug, "Unable to spawn nexus vehicle. Not enough space near " + p->GetName());
						break;
					}
				}

			}
		}
	}

}

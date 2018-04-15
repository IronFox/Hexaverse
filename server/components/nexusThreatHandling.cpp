#include <global_root.h>
#include "nexusThreatHandling.h"
#include <objects.h>
#include <runtimeConfig.h>
#include "state/gameState.h"
#include "journalSaveGame.h"
#include <objects/structure/vehiclePlant.h>


namespace Nexus
{
	struct BasePortEnvironment
	{
		UINT32		poleIndex = UNSIGNED_UNDEF;	//pole/stack index
		index_t		serialIndex = InvalidIndex;
		M::TVec3<>	center;	//!< Normalized environment center (corner center of the pole cell)
		Objects::PNexusPort	port;

		bool		hasBase = false,
					isInEMPZone = false;
		float		hostileNearNexus = 0,
					friendlyNearNexus = 0,
					constructorNearNexus = 0,
					highestConstructorNearNexus = 0;
	};


	struct NexusPortEnvironment : public BasePortEnvironment
	{
		typedef BasePortEnvironment Super;
		typedef NexusPortEnvironment Self;

		Buffer0<Objects::PWorkingStructure>	associatedWorkingStructures;
		Buffer0<std::pair<Objects::PWorldEntity, Objects::PMobileEntityProducer> > associatedEntityProducers;

		void	Clear()
		{
			(*(Super*)this) = Super();
			ClearAssociation();
		}

		void	ClearAssociation()
		{
			associatedWorkingStructures.Clear();
			associatedEntityProducers.Clear();
		}

		void	swap(Self&other)
		{
			using std::swap;
			swap((Super&)*this,(Super&)other);
			associatedWorkingStructures.swap(other.associatedWorkingStructures);
			associatedEntityProducers.swap(other.associatedEntityProducers);
		}

		friend void swap(Self&a, Self&b)
		{
			a.swap(b);
		}
	};
		

	static void UpdateAssociatedEntities(MutableArrayRef<NexusPortEnvironment>&env)
	{
		bool anyBases = false;
		foreach (env,e)
		{
			e->ClearAssociation();
			anyBases |= e->hasBase;
		}

		if (!Objects::Faction::nexus || !anyBases)
			return;

		Objects::Faction::nexus->VisitMyStructures([&env](const Objects::PStructure&s)
		{
			if (!s || !s->IsWorkingStructure())
				return;
			auto ws = To<Objects::WorkingStructure>::Cast(s);
			if (ws->IsNexusPort())
				return;
			float closestAt = std::numeric_limits<float>::lowest();
			index_t closest = InvalidIndex;
			foreach (env,e)
			{
				if (!e->hasBase)
					continue;
				float d = Vec::dot(Grid::low.cells[s->location.stackID].centerVertex.xyz,e->center);
				if (d > closestAt)
				{
					closestAt = d;
					closest = e->serialIndex;
				}
			}
			env[closest].associatedWorkingStructures << ws;
		});


		Objects::Faction::nexus->VisitMyEntities([&env](const Objects::PWorldEntity&w)
		{
			auto producer = w->CastToProducer();
			if (!producer)
				return;
			
			float closestAt = std::numeric_limits<float>::lowest();
			index_t closest = InvalidIndex;
			foreach (env,e)
			{
				if (!e->hasBase)
					continue;
				float d = Vec::dot(Grid::low.cells[w->location.stackID].centerVertex.xyz,e->center);
				if (d > closestAt)
				{
					closestAt = d;
					closest = e->serialIndex;
				}
			}
			env[closest].associatedEntityProducers << std::make_pair(w, To<Objects::MobileEntityProducer>::DynamicCast(w));
		},false);
	}



	/**
	Adds a friendly or hostile presence to the environments of nexus ports
	@param e Entity to add presence estimations of. Friendly structures are ignored
	@param location Location of the entity. Either its own or of the producer it is queued in
	@param[in,out] Port environments to add to. The specified entity may be added to multiple environments
	*/
	static void AddPresence( const Objects::WorldEntity&e, const Grid::LCellIdI&location, MutableArrayRef<NexusPortEnvironment>&env )
	{
		const M::TVec3 < >&loc = Grid::low.cells[location.stackID].centerVertex.xyz;
		float entityRange = e.GetScanningRange();
		float impact = e.GetConstructionCost();
		Objects::PConstTurretBase b = e.GetTurretBase();
		if (b && b->turret->CanDealDamage())
		{
			impact += b->GetDamagePotentialPerRound();
			entityRange = M::Max(entityRange,b->GetFlatAttackRange());
		}
		impact += entityRange;
		const M::TVec3<> worldLoc = Grid::ToWorld(location);
		const float avg = Grid::low.neighborCellDistances.GetAverage();
		if (impact > 0)
			foreach (env, at)
			{
				float worldDistance = Vec::distance(Grid::low.cells[at->poleIndex].centerVertex.xyz * Grid::MeanPlanetRadius,worldLoc);
				if (e.owner != Objects::Faction::nexus)
					worldDistance = M::Max(0,worldDistance-entityRange);


				const float stepDistance = worldDistance / Grid::MeanPlanetRadius / avg * 0.75f;

				const bool isHostile = e.owner != Objects::Faction::nexus;
				const bool isBlack = stepDistance < 2;
				const bool isRed = stepDistance < 4;
				const bool isYellow = stepDistance < 6;
				const bool isNoticed = stepDistance < 9;
				const float opacity = (float(isBlack?6:0) + float(isRed?3:0) + float(isYellow ? 2 : 0) + float(isNoticed))/3.f;	//0->1/3->1->2

				if (opacity > 0)
				{
					if (isHostile)
					{
						at->hostileNearNexus += opacity * impact;

						if (b && b->turret->CanDo(Objects::Component::Construct))
						{
							if (isRed)
							{
								at->constructorNearNexus += opacity;
								at->highestConstructorNearNexus = std::max(at->highestConstructorNearNexus,opacity);
							}
						}
					}
					else
					{
						if (e.IsMobile() && e.health == e.GetMaxHealth())
							at->friendlyNearNexus += opacity * impact;
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
		if (!Game::Config::Nexus::enableAI)
			return;
		

		Buffer<NexusPortEnvironment,12,Swap>	env;
		Objects::structures.Foreach([&env](const Objects::PStructure&s)
		{
			if (   s->location.stackID >= 12					//structure is not located on pole (there are 12 poles and they are the first 12 cell stacks)
				|| s->GetType() != ConstName::Type_NexusPort	//we're looking at something else
				|| s->owner != Objects::Faction::nexus			//the port no longer belongs to the nexus faction
				)
				return;
			auto&e = env.Append();
			e.poleIndex = s->location.stackID;
			e.port = To<Objects::NexusPort>::Cast(s);
			e.friendlyNearNexus = Game::factionStartMineralValue;
			e.hasBase = Game::Config::Nexus::bases.havePlaced && BaseGrid::IsIsolatedCorner(s->location.stackID);
			e.isInEMPZone = Grid::AffectedByEMP(s->location);
			e.serialIndex = env.Count()-1;
			e.center = Grid::low.cells[e.poleIndex].centerVertex.xyz;
		});
		LogEvent(EventSignificance::Debug,"Identified "+String(env.Count())+" Nexus Ports under Nexus faction control");

		UpdateAssociatedEntities(env.ToRef());

		Objects::ForeachWorldEntity([&](const Objects::PWorldEntity&e)
		{
			AddPresence(*e,e->location,env.ToRef());
		},false);


		float estimationFactor = Game::Config::Nexus::threatEstimationFactor;
		if (!Game::Config::Nexus::bases.havePlaced && Game::Config::Nexus::allPorts.havePlaced)
			estimationFactor *= 0.25f;


		for (auto&e : env)
		{
			if (e.isInEMPZone)
				continue;
			if (RuntimeConfig::roundNumber - e.port->lastSpawnRound < 5)
				continue;	//not allowed to spawn anything for the moment

			const float threatLevel = e.hostileNearNexus * estimationFactor;
			LogEvent(EventSignificance::Debug, "Threat level for " + e.port->GetName() + " considered " + String(threatLevel)+" (friendlies here: "+String(e.friendlyNearNexus)+", hostile constructor(s) at: "+String(e.highestConstructorNearNexus)+"/"+String(e.constructorNearNexus)+")");

			//let's take a look at producers...
			if (e.hasBase)
			{
				Objects::RefreshCompoundCoverageIfNeeded(0);

				count_t producing = 0,
						existing = 0;


				for (auto producer : e.associatedEntityProducers)
				{
						
					#ifndef NO_HIBERNATION
						if (producer.first->ToHibernating()->IsHibernating())
							continue;
					#endif
					if (Grid::AffectedByEMP(producer.first->location))
						continue;		
					Objects::PMobileEntityProducer plant = producer.second;
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
						Journal::currentRound.RegisterChoiceIntersection(false,producer.first,v->location,Objects::CursorMode::Idle,v);
						AddPresence(*v,v->location,env.ToRef());
					}
					if (plant->queue.IsNotEmpty())
					{
						producing++;
						ASSERT__(plant->queue.First().producing->owner == Objects::Faction::nexus);
						AddPresence(*plant->queue.First().producing,producer.first->location,env.ToRef());
					}
				}

				const bool terraformerViolation = e.constructorNearNexus > 0.5f;

				while (producing < existing && (e.friendlyNearNexus < threatLevel || (terraformerViolation && e.friendlyNearNexus < Game::factionStartMineralValue + 20.f)))
				{
					LogEvent(EventSignificance::Debug, "Increasing production for " + e.port->GetName());
					bool anyFound = false;

					for (auto producer : e.associatedEntityProducers)
					{
						Objects::PMobileEntityProducer plant = producer.second;
						auto entity = producer.first;
						if (plant->queue.IsEmpty())
						{
							index_t choice = entity->entityRandom.NextIndex(Game::Config::Nexus::vehicleTurrets.Count());
							ASSERT_LESS__(choice,Game::Config::Nexus::vehicleTurrets.Count());
							Objects::EntityType	t = {ConstName::VehicleBase_HoverBase, Game::Config::Nexus::vehicleTurrets[choice]->constName};
							LogEvent(EventSignificance::Common,entity.get(),"Enqueueing vehicle production job due to high threat level");
							Journal::currentRound.RegisterProductionJobStart(true,entity,plant,t);
									
							if (plant->queue.IsNotEmpty())	//shit happens
							{
								AddPresence(*plant->queue.Last().producing,entity->location,env.ToRef());
								anyFound = true;
								producing++;
								break;
							}
						}
					}
					if (!anyFound)
					{
						LogEvent(EventSignificance::Debug,  e.port->GetName()+" no longer has idle plants capable of queueing jobs. Aborting");
						break;
					}
				}
			}
			elif (!Game::Config::Nexus::bases.havePlaced)	//different logic
			{
				const auto ConstructorThreshold = 
							Game::Config::Nexus::allPorts.havePlaced 
							? 3.5f	//black
							: 0.5f;	//yellow

				const bool constructorViolation = (e.highestConstructorNearNexus > ConstructorThreshold && e.friendlyNearNexus < Game::factionStartMineralValue + 20.f);


				while (e.friendlyNearNexus < threatLevel || constructorViolation)
				{
					LogEvent(EventSignificance::Debug, "Spawning nexus unit near " + e.port->GetName());
					e.port->lastSpawnRound = RuntimeConfig::roundNumber;


					Grid::LCellIdI location = Grid::LCellIdI::Invalid;
					static Buffer0<UINT32> neighborField;
					Grid::low.GetExtendedNeighborhood(e.poleIndex, 3, neighborField);
					foreach(neighborField, n)
					{
						Grid::LCellIdI candidate(*n, e.port->location.height);
						Grid::AssertIsValid(candidate,CLOCATION);
						candidate.height = Objects::FindApplicableGround(candidate, true, false);

						if (candidate.height != Objects::badHeight && !Objects::GetVehicle(candidate))
						{
							location = candidate;
							break;
						}

					}

					if (location != Grid::LCellIdI::Invalid)
					{
						
						Objects::Component::Turret* turret = Game::Config::Nexus::vehicleTurrets[e.port->entityRandom.NextIndex(Game::Config::Nexus::vehicleTurrets.Count())];
						float t = 0;
						Objects::PVehicle v = Objects::InsertNewVehicle(Objects::Faction::nexus, location, &Objects::Component::hoverbase, turret, true, t);
						Journal::currentRound.LogEntityCreation(v);
						AddPresence(*v,v->location,  env.ToRef());

						if (e.friendlyNearNexus >= threatLevel)
							break;	//just spawn one if there's a terraformer violation
					}
					else
					{
						LogEvent(EventSignificance::Debug, "Unable to spawn nexus vehicle. Not enough space near " + e.port->GetName());
						break;
					}
				}
			}
		}
	}
}

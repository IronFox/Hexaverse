#include <global_root.h>
#include "nexusThreatHandling.h"
#include <objects.h>
#include <runtimeConfig.h>
#include "state/gameState.h"
#include "journalSaveGame.h"
#include <objects/structure/vehiclePlant.h>
#include <objects/structure/planeFactory.h>
#include <objects/nexus/nexusThreatEstimation.h>
#include <objects/mobileEntity.h>
#include <objects/vehicle.h>
#include <objects/subOrbital.h>
#include <math/vclasses.h>


namespace Nexus
{
	namespace Detail
	{
		RoundNumber lastSpawnRound[12];

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


	int2	AutoDeploy(const Objects::PWorldEntity&e, const Objects::PMobileEntityProducer&producer)
	{
		#ifndef NO_HIBERNATION
			if (e->ToHibernating()->IsHibernating())
				return std::make_pair(0,0);
		#endif
		if (Grid::AffectedByEMP(e->location))
			return int2(0,0);
		Objects::PMobileEntityProducer plant = producer;
		if (plant->AwaitsUserInteraction())
		{
			float t = 0;
			Objects::PMobileEntity v = plant->SpawnFinishedProductionAnywhere(t);
			if (!v)
			{
				return int2(1,1);	//dont count as finished, retry later
			}
			Journal::currentRound.RegisterChoiceIntersection(false,e,v->location,Objects::CursorMode::Idle,v, TUniqueID::Empty());
		}
		if (plant->queue.IsNotEmpty())
		{
			ASSERT__(plant->queue.First().producing->owner == Objects::Faction::nexus);
			return int2(1,1);
		}
		return int2(0,1);
	}


	bool SpawnSensorDrone(Random&rnd)
	{
		return rnd.NextFloat() < 0.25f;
	}


	bool SpawnBomberAgainstGroundThreat(Random&rnd)
	{
		return rnd.NextFloat() < 0.25f;
	}

	bool SpawnMobileAirDefense(Random&rnd)
	{
		return rnd.NextFloat() < 0.25f;
	}

	template <typename F>
	Objects::NexusThreat::TEstimation EnableFactory(UINT32 poleIndex, const Objects::PNexusPort&port, ArrayRef<std::shared_ptr<F> > factories, bool airborne, bool airThreat)
	{
		LogEvent(EventSignificance::Debug, "Increasing production for " + port->GetName());

		for (auto producer : factories)
		{
			Objects::PMobileEntityProducer plant = producer;
			Objects::PWorldEntity entity = producer;
			if (plant->queue.IsEmpty())
			{
				Objects::EntityType	t;
				if (!airborne)
				{
					index_t choice = entity->entityRandom.NextIndex(Game::Config::Nexus::vehicleTurrets.Count());
					t = {ConstName::VehicleBase_HoverBase, Game::Config::Nexus::vehicleTurrets[choice]->constName};

					if (airThreat)
						t.turretType = ConstName::Turret_AntiAircraft;
					if (Objects::NexusThreat::DeployHeavyVehicleBases(poleIndex) && port->entityRandom.NextBool())
						t.baseType = ConstName::VehicleBase_HeavyBase;
				}
				else
				{
					t = airThreat ? Objects::SubOrbital::Raptor::globalPrototype->GetType() :
						SpawnSensorDrone(entity->entityRandom) 
						? Objects::SubOrbital::SensorDrone::globalPrototype->GetType() 
						: Objects::SubOrbital::Bomber::globalPrototype->GetType();
				}

				LogEvent(EventSignificance::Common,entity.get(),"Enqueueing vehicle production job due to high threat level");

				Journal::currentRound.RegisterProductionJobStart(true,entity,plant,t);
									
				if (plant->queue.IsNotEmpty())	//shit happens
					return Objects::NexusThreat::Estimate(poleIndex,plant->queue.Last().producing);
			}
		}
		LogEvent(EventSignificance::Debug,  port->GetName()+" no longer has idle plants capable of queueing jobs. Aborting");
		return Objects::NexusThreat::TEstimation();

	}

	void SpawnEntities(UINT32 poleIndex, const Objects::PNexusPort&port, float threatLevel, float supportLevel, bool force, bool airThreat)
	{
		while (supportLevel < threatLevel || force)
		{
			force = false;

			LogEvent(EventSignificance::Debug, "Spawning nexus unit near " + port->GetName());
			port->lastSpawnRound = RuntimeConfig::roundNumber;

			bool airborne = airThreat;
			
			if (!airborne)
			{
				if (SpawnBomberAgainstGroundThreat(port->entityRandom))
					airborne = true;
			}
			else if (SpawnMobileAirDefense(port->entityRandom))
				airborne = false;

			Grid::LCellIdI location = Grid::LCellIdI::Invalid;
			static Buffer0<UINT32> neighborField;
			Grid::low.GetExtendedNeighborhood(poleIndex, 3, neighborField);
			foreach(neighborField, n)
			{
				Grid::LCellIdI candidate(*n, airborne ? Objects::SubOrbital::Height : port->location.height);
				Grid::AssertIsValid(candidate,CLOCATION);
				if (!airborne)
					candidate.height = Objects::FindApplicableGround(candidate, true, false);

				if (candidate.height != Objects::badHeight && !Objects::GetMobileEntity(candidate))
				{
					location = candidate;
					break;
				}
			}

			if (location != Grid::LCellIdI::Invalid)
			{
				Objects::PMobileEntity spawned;
				if (!airborne)
				{
					Objects::Component::Turret* turret = Game::Config::Nexus::vehicleTurrets[port->entityRandom.NextIndex(Game::Config::Nexus::vehicleTurrets.Count())];
					if (airThreat)
						turret = &Objects::Component::antiAircraft;
					float t = 0;
					spawned = Objects::InsertNewVehicle(Objects::Faction::nexus, location, &Objects::Component::hoverbase, turret, true, t);
				}
				else
				{
					Objects::PWorldEntity proto;
					if (airThreat)
						proto = Objects::SubOrbital::Raptor::globalPrototype;
					else
						proto =  SpawnSensorDrone(port->entityRandom)
							? To<Objects::WorldEntity>::Cast(Objects::SubOrbital::SensorDrone::globalPrototype) 
							: To<Objects::WorldEntity>::Cast(Objects::SubOrbital::Bomber::globalPrototype);
					float t = 0;
					spawned = To<Objects::MobileEntity>::Cast(proto->CreateNewInstance(Objects::Faction::nexus,location,t));
					Objects::InsertConstructed(spawned,t);
				}

				Journal::currentRound.LogEntityCreation(spawned);
				supportLevel += Objects::NexusThreat::Estimate(poleIndex,spawned).totalImpact;

				if (supportLevel >= threatLevel)
					break;	//just spawn one if there's a terraformer violation
			}
			else
			{
				LogEvent(EventSignificance::Debug, "Unable to spawn nexus vehicle. Not enough space near " + port->GetName()+" (airborne="+String(airborne)+")");
				break;
			}
		}
	}


	void ProcessCoverage()
	{
		if (!Game::Config::Nexus::enableAI || !Objects::Faction::nexus || 
			(
				Objects::Faction::nexus->aiScriptName.IsNotEmpty()
				&&
				Game::Config::Nexus::bases.havePlaced
			))
			return;
		
		Objects::Resource::Vector vehiclePlantCostPerRound, planeFactoryCostPerRound, dummy;
		Objects::VehiclePlant::globalPrototype->GetResourceVector(vehiclePlantCostPerRound,dummy,dummy);
		Objects::PlaneFactory::globalPrototype->GetResourceVector(planeFactoryCostPerRound,dummy,dummy);

		const auto ConstructorThreshold = 
			Game::Config::Nexus::allPorts.havePlaced 
			? 3.5f	//black
			: 0.5f;	//yellow


		for (int i = 0; i < 12; i++)
		{
			if (!Objects::NexusThreat::HasPort(i))
				continue;

			if (Objects::NexusThreat::IsInEmpZone(i))
				continue;
			if (RuntimeConfig::roundNumber - Detail::lastSpawnRound[i] < 5)
				continue;	//not allowed to spawn anything for the moment

			const auto threat = Objects::NexusThreat::Get(i);
			auto support = Objects::NexusThreat::GetSupport(i);

			const auto port = Objects::NexusThreat::GetPort(i);

			Objects::Resource::Vector portProductionPerRound;
			port->GetResourceVector(portProductionPerRound,dummy,dummy);




			LogEvent(EventSignificance::Debug, "Threat level for " + port->GetName() + " considered " + String(threat.GroundTotal())+"|"+String(threat.AirTotal())
				+" (support here: "+String(support.GroundTotal())+"|"+String(support.AirTotal())+", hostile constructor(s) at: "+String(threat.closestHostileConstructorOpacity)+"/"+String(threat.constructorOpacitySum)+")");

			bool constructorViolation = (threat.closestHostileConstructorOpacity > ConstructorThreshold && support.MobileTotal() < Game::factionStartMineralValue + 20.f);

			//let's take a look at producers...
			if (Objects::NexusThreat::IsBase(i))
			{
				Objects::RefreshCompoundCoverageIfNeeded(0);




				int2 groundState,airState;


				for (auto producer : Objects::NexusThreat::GetBaseVehiclePlants(i))
					groundState += AutoDeploy(producer,producer);
				for (auto producer : Objects::NexusThreat::GetBasePlaneFactories(i))
					airState += AutoDeploy(producer,producer);
				groundState.y = std::min(groundState.y, (portProductionPerRound - planeFactoryCostPerRound) / vehiclePlantCostPerRound);	//keep at least one plane factory free to enable


				if ((vehiclePlantCostPerRound * groundState.x + planeFactoryCostPerRound * airState.x) >= portProductionPerRound)
					continue;


				while (support.MobileTotal() < threat.Total() || constructorViolation)
				{
					auto airDemand = threat.AirTotal() - support.air.mobile;
					auto groundDemand = threat.GroundTotal() - support.ground.mobile;

					if (airDemand > groundDemand && !constructorViolation)
					{
						if (airState.x < airState.y && (vehiclePlantCostPerRound * groundState.x + planeFactoryCostPerRound * (airState.x+1)) <= portProductionPerRound)
						{
							support += EnableFactory(i,port,Objects::NexusThreat::GetBasePlaneFactories(i),true,true);
							airState.x++;	//always increment to prevent endless loop
							continue;
						}
						else if (groundState.x < groundState.y && (vehiclePlantCostPerRound * (groundState.x+1) + planeFactoryCostPerRound * airState.x) <= portProductionPerRound)
						{
							support += EnableFactory(i,port,Objects::NexusThreat::GetBaseVehiclePlants(i),false,true);
							groundState.x++;	//always increment to prevent endless loop
							continue;
						}
					}

					constructorViolation = false;

					if (airState.x < airState.y && (vehiclePlantCostPerRound * groundState.x + planeFactoryCostPerRound * (airState.x+1)) <= portProductionPerRound && SpawnBomberAgainstGroundThreat(port->entityRandom))
					{
						support += EnableFactory(i,port,Objects::NexusThreat::GetBasePlaneFactories(i),true,false);
						airState.x++;	//always increment to prevent endless loop
						continue;
					}
					else if (groundState.x < groundState.y && (vehiclePlantCostPerRound * (groundState.x+1) + planeFactoryCostPerRound * airState.x) <= portProductionPerRound)
					{
						support += EnableFactory(i,port,Objects::NexusThreat::GetBaseVehiclePlants(i),false,false);
						groundState.x++;	//always increment to prevent endless loop
						continue;
					}
					LogEvent(EventSignificance::Common, "Nexus threat level: All factories running. Can't enable more. Nothing to do");
					break;
				}


			}
			elif (!Game::Config::Nexus::bases.havePlaced)	//different logic
			{
				SpawnEntities(i, port, threat.GroundTotal(), support.ground.mobile, constructorViolation,false);
				SpawnEntities(i, port, threat.AirTotal(), support.air.mobile, false,true);
			}
		}
	}
}

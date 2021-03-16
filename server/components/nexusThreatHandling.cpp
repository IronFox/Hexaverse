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




	bool SpawnSensorDrone(Random&rnd)
	{
		return rnd.NextFloat() < 0.25f;
	}


	bool SpawnBomberAgainstGroundThreat(Random&rnd)
	{
		return RuntimeConfig::roundNumber >= Game::Config::Nexus::minRoundForPlanes
			&&
			rnd.NextFloat() < 0.25f;
	}

	bool SpawnMobileAirDefense(Random&rnd)
	{
		return rnd.NextFloat() < 0.25f;
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
					auto base = &Objects::Component::hoverbase;
					if (RuntimeConfig::roundNumber > Game::Config::Nexus::minRoundForHeavyVehicleBases &&  Objects::NexusThreat::DeployHeavyVehicleBases(poleIndex) && port->entityRandom.NextBool())
						base = &Objects::Component::heavybase;
					spawned = Objects::InsertNewVehicle(Objects::Faction::nexus, location, base, turret, true, t);

					
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
		if ( (!Game::Config::Nexus::enableAI && !Network::NexusClientIsConnected()) 
			|| !Objects::Faction::nexus 
			|| Game::Config::Nexus::bases.havePlaced)
			return;


		const auto ConstructorThreshold = 
			0.5f;


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


			LogEvent(EventSignificance::Debug, "Threat level for " + port->GetName() + " considered " + String(threat.GroundTotal())+"|"+String(threat.AirTotal())
				+" (support here: "+String(support.GroundTotal())+"|"+String(support.AirTotal())+", hostile constructor(s) at: "+String(threat.closestHostileConstructorOpacity)+"/"+String(threat.constructorOpacitySum)+")");

			bool constructorViolation = (threat.closestHostileConstructorOpacity > ConstructorThreshold && support.MobileTotal() < Game::factionStartMineralValue + 20.f);
			SpawnEntities(i, port, threat.GroundTotal(), support.ground.mobile, constructorViolation,false);
			if (RuntimeConfig::roundNumber >= Game::Config::Nexus::minRoundForPlanes)
				SpawnEntities(i, port, threat.AirTotal(), support.air.mobile, false,true);
		}
	}
}

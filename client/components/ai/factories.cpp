#include <global_root.h>
#include "factories.h"
#include <objects.h>
#include <runtimeConfig.h>
#include <objects/structure/vehiclePlant.h>
#include <objects/structure/planeFactory.h>
#include <objects/nexus/nexusThreatEstimation.h>
#include <objects/mobileEntity.h>
#include <objects/vehicle.h>
#include <objects/subOrbital.h>
#include <math/vclasses.h>
#include "../objectNetworkHelper.h"
#include "aiCommon.h"
#include "../network.h"


namespace AI
{
	namespace Detail
	{
		struct TChoice
		{
			Objects::PWorldEntity			fullPrototype;
			Objects::Component::VehicleBase	*base;
			Objects::Component::Turret		*turret;
			count_t							existing=0;
			float							demand = 0;
			float							offset = 0;

			bool							IsPlane() const {return fullPrototype && fullPrototype->IsPlane();}

			Objects::EntityType				GetType() const
			{
				if (fullPrototype)
					return fullPrototype->GetType();
				return Objects::EntityType(base->constName,turret->constName);
			}


		};

		struct TCounter
		{
			count_t			cnt;
			/**/			TCounter():cnt(0) {}

			void			operator++(int) {cnt ++;}

			operator count_t() const {return cnt;}
		};

		class ProductionTable
		{

		public:
			/**/				ProductionTable(int pole):pole(pole)
			{
				const auto mobiles = Objects::NexusThreat::GetOwnMobiles(pole);
				totalMobiles = mobiles.Count();
				for (const auto& m : mobiles)
				{
					typeCounter.Set(m->GetType())++;
				}
			}

			void				Add(const Objects::PWorldEntity&proto)
			{
				choices.Append().fullPrototype = proto;
				choices.Last().existing = typeCounter.Get(proto->GetType(),TCounter());
			}

			void				AddVehicle(Objects::Component::Turret&turret, Objects::Component::VehicleBase&base)
			{
				auto&choice = choices.Append();
				choice.base = &base;
				choice.turret = &turret;
				choice.existing = typeCounter.Get(choice.GetType(),TCounter());
			}

			void				AddVehicleTurret(Objects::Component::Turret&turret)
			{
				AddVehicle(turret,Objects::Component::hoverbase);
				if (Objects::NexusThreat::DeployHeavyVehicleBases(pole) 
					&& RuntimeConfig::roundNumber >= ApplicationInterface::GetMinAIRoundForHeavyVehicleBases())
					AddVehicle(turret,Objects::Component::heavybase);
			}

			bool				IsNotEmpty() const
			{
				return choices.IsNotEmpty();
			}

			const TChoice&		GetRandomChoice()
			{

				count_t sum = 0;
				foreach (choices,c)
					sum += c->existing;

				auto average = typeCounter.IsNotEmpty() ? M::Round(float(sum) / typeCounter.Count()) : 0.f;
				average = std::max(average,1.f);

				float totalDemand = 0;

				foreach (choices,c)
				{
					c->demand = 1.f / (1.f + c->existing);
						//std::max(average - c->existing,0.f);
					c->offset = totalDemand;
					totalDemand += c->demand;
				}

				float rand = AI::random.NextFloat() * totalDemand;


				foreach (choices,c)
				{
					if (c->demand > 0 && rand >= c->offset && rand <= c->offset + c->demand)
						return *c;
				}
				FATAL__("Should not be here");
				static TChoice whocares;
				return whocares;
			}

		private:
			Buffer0<TChoice>	choices;
			const int			pole;
			count_t				totalMobiles=0;
			HashTable<Objects::EntityType,TCounter>	typeCounter;

		};

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
			Grid::LCellIdI loc;
			if (plant->GetIdealSpawningPoint(*e,producer->queue.First().GetPrototype(),random,loc))
			{
				Protocol::ChoiceIntersection ch;
				ch.id = e->id;
				ch.deployingWhat.Set0() = producer->queue.First().id;
				ch.mode = (UINT16)Objects::CursorMode::Idle;
				ch.target = loc;
				Network::Channels::choiceIntersection.SendTo(Network::client,ch);
			}
			if (plant->queue.Count() > 1)
				return int2(1,1);
		}
		if (plant->queue.IsNotEmpty())
			return int2(1,1);
		return int2(0,1);
	}


	bool SpawnSensorDrone(Random&rnd)
	{
		return rnd.NextFloat() < 0.25f;
	}


	bool SpawnBomberAgainstGroundThreat(Random&rnd)
	{
		return RuntimeConfig::roundNumber >= ApplicationInterface::GetMinAIRoundForPlanes()
			&&
			rnd.NextFloat() < 0.25f;
	}

	bool SpawnMobileAirDefense(Random&rnd)
	{
		return rnd.NextFloat() < 0.25f;
	}

	template <typename F>
	Objects::NexusThreat::TEstimation EnableFactory(UINT32 poleIndex, const Objects::PNexusPort&port, ArrayRef<std::shared_ptr<F> > factories, const Detail::TChoice&choice)
	{
		LogEvent(EventSignificance::Debug, "Increasing production for " + port->GetName());

		for (auto producer : factories)
		{
			Objects::PMobileEntityProducer plant = producer;
			Objects::PWorldEntity entity = producer;
			if (plant->queue.IsEmpty())
			{
				Objects::EntityType	t = choice.GetType();

				LogEvent(EventSignificance::Common,entity.get(),"Enqueueing vehicle production job due to high threat level");


				Protocol::ConstructionBegin cst;
				cst.plantID = entity->id;
				cst.type = t;

				Network::Channels::constructionBegin.SendTo(Network::client,cst);
				if (choice.fullPrototype)
					Objects::NexusThreat::Estimate(poleIndex,choice.fullPrototype);

				return Objects::NexusThreat::Estimate(poleIndex,Objects::PWorldEntity(new Objects::Vehicle(Objects::Faction::mine,entity->location,choice.base,choice.turret)));
			}
		}
		LogEvent(EventSignificance::Debug,  port->GetName()+" no longer has idle plants capable of queueing jobs. Aborting");
		return Objects::NexusThreat::TEstimation();

	}

	void ProcessFactories()
	{
		Objects::Resource::Vector vehiclePlantCostPerRound, planeFactoryCostPerRound, dummy,v;
		Objects::VehiclePlant::globalPrototype->GetResourceVector(v,dummy,dummy);
		v.SeparateBySign(dummy,vehiclePlantCostPerRound);
		vehiclePlantCostPerRound.Negate();
		Objects::PlaneFactory::globalPrototype->GetResourceVector(v,dummy,dummy);
		v.SeparateBySign(dummy,planeFactoryCostPerRound);
		planeFactoryCostPerRound.Negate();

		const auto ConstructorThreshold =  3.5f;	//black


		for (int i = 0; i < 12; i++)
		{
			if (!Objects::NexusThreat::HasPort(i))
				continue;

			if (Objects::NexusThreat::IsInEmpZone(i))
				continue;

			const auto threat = Objects::NexusThreat::Get(i);
			auto support = Objects::NexusThreat::GetSupport(i);

			const auto port = Objects::NexusThreat::GetPort(i);

			Objects::Resource::Vector v,portProductionPerRound;
			port->GetResourceVector(v,dummy,dummy);
			v.SeparateBySign(portProductionPerRound,dummy);



			LogEvent(EventSignificance::Debug, "Threat level for " + port->GetName() + " considered " + String(threat.GroundTotal())+"|"+String(threat.AirTotal())
				+" (support here: "+String(support.GroundTotal())+"|"+String(support.AirTotal())+", hostile constructor(s) at: "+String(threat.closestHostileConstructorOpacity)+"/"+String(threat.constructorOpacitySum)+")");

			bool constructorViolation = (threat.closestHostileConstructorOpacity > ConstructorThreshold);

			//let's take a look at producers...
			if (Objects::NexusThreat::IsBase(i))
			{
				int2 groundState,airState;


				for (auto producer : Objects::NexusThreat::GetBaseVehiclePlants(i))
					groundState += AutoDeploy(producer,producer);
				for (auto producer : Objects::NexusThreat::GetBasePlaneFactories(i))
					airState += AutoDeploy(producer,producer);

				if (RuntimeConfig::roundNumber < ApplicationInterface::GetMinAIRoundForPlanes())
					airState.y = 0;
				else
				{
					groundState.y = std::min(groundState.y, (portProductionPerRound - planeFactoryCostPerRound) / vehiclePlantCostPerRound);	//keep at least one plane factory free to enable
					airState.y = std::min(airState.y,(portProductionPerRound - vehiclePlantCostPerRound * groundState.x) / planeFactoryCostPerRound);
				}


				if ((vehiclePlantCostPerRound * groundState.x + planeFactoryCostPerRound * airState.x) >= portProductionPerRound)
					continue;


				while (support.MobileTotal() < threat.Total() || constructorViolation)
				{
					auto airDemand = threat.AirTotal() - support.air.mobile;
					auto groundDemand = threat.GroundTotal() - support.ground.mobile;

					Detail::ProductionTable table(i);

					if (airDemand > groundDemand && !constructorViolation)
					{




						if (airState.x < airState.y && (vehiclePlantCostPerRound * groundState.x + planeFactoryCostPerRound * (airState.x+1)) <= portProductionPerRound)
						{
							table.Add(Objects::SubOrbital::Raptor::globalPrototype);
							table.Add(Objects::SubOrbital::SensorDrone::globalPrototype);
						}
						if (groundState.x < groundState.y && (vehiclePlantCostPerRound * (groundState.x+1) + planeFactoryCostPerRound * airState.x) <= portProductionPerRound)
							table.AddVehicleTurret(Objects::Component::antiAircraft);

						if (table.IsNotEmpty())
						{
							auto choice = table.GetRandomChoice();
							if (choice.IsPlane())
							{
								support += EnableFactory(i,port,Objects::NexusThreat::GetBasePlaneFactories(i),choice);
								airState.x++;
								continue;
							}

							support += EnableFactory(i,port,Objects::NexusThreat::GetBaseVehiclePlants(i),choice);
							groundState.x++;
							continue;
						}
					}

					constructorViolation = false;

					if (airState.x < airState.y && (vehiclePlantCostPerRound * groundState.x + planeFactoryCostPerRound * (airState.x+1)) <= portProductionPerRound && SpawnBomberAgainstGroundThreat(AI::random))
					{
						table.Add(Objects::SubOrbital::Bomber::globalPrototype);
						table.Add(Objects::SubOrbital::SensorDrone::globalPrototype);
					}



					if (groundState.x < groundState.y && (vehiclePlantCostPerRound * (groundState.x+1) + planeFactoryCostPerRound * airState.x) <= portProductionPerRound)
					{
						table.AddVehicleTurret(Objects::Component::armored);
						table.AddVehicleTurret(Objects::Component::artillery);
						table.AddVehicleTurret(Objects::Component::radar);
						table.AddVehicleTurret(Objects::Component::resourceScanner);
					}


					if (table.IsNotEmpty())
					{
						auto choice = table.GetRandomChoice();
						if (choice.IsPlane())
						{
							support += EnableFactory(i,port,Objects::NexusThreat::GetBasePlaneFactories(i),choice);
							airState.x++;
							continue;
						}

						support += EnableFactory(i,port,Objects::NexusThreat::GetBaseVehiclePlants(i),choice);
						groundState.x++;
						continue;
					}

					LogEvent(EventSignificance::Common, "Nexus threat level: All factories running. Can't enable more. Nothing to do");
					break;
				}


			}
		}
	}
}

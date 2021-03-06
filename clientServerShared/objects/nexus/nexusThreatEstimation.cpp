#include "nexusThreatEstimation.h"
#include "../../globalEvents.h"
#include "../../runtimeConfig.h"
#include "nexusPresence.h"
#include "../structure.h"
#include "../faction.h"
#include "../structure/vehiclePlant.h"
#include "../structure/planeFactory.h"
#include "../prototypes.h"
#include "../../grid/setup.h"

namespace Objects
{
	namespace NexusThreat
	{
		namespace Detail
		{
			struct Pole
			{
				float		hostileGroundSum = 0,
							hostileAirSum = 0,
							friendlyGroupSum[2] = {0,0}, 
							friendlyAirSum = 0,
							constructorNearNexus = 0,
							highestConstructorNearNexus = 0;
				bool		hasBase = false;
				PNexusPort	port;
				bool		isInEMPZone = false;
				M::TVec3<>	center;


				Buffer0<Objects::PWorkingStructure>	associatedWorkingStructures;
				Buffer0<Objects::PVehiclePlant > associatedVehiclePlants;
				Buffer0<Objects::PPlaneFactory > associatedPlaneFactories;

				void	Clear()
				{
					ClearAssociation();
				}

				void	ClearAssociation()
				{
					associatedWorkingStructures.Clear();
					associatedVehiclePlants.Clear();
					associatedPlaneFactories.Clear();
				}
			};


			FixedArray<Pole,12>	poles;

			void	CheckIndex(index_t poleIndex)
			{
				if (poleIndex >= 12)
					throw Except::Program::ParameterFault(CLOCATION,"Pole index "+String(poleIndex)+" is out of bounds (0..11)");
			}
			
			TEstimation	_Estimate(const Objects::PFaction&nexus, const Objects::WorldEntity&e, const Grid::LCellIdI&loc, const Objects::PFaction&owner, index_t poleIndex)
			{
				TEstimation rs;

				float entityRange = e.GetScanningRange();
				rs.impact = e.GetConstructionCost() + (e.t2 ? e.t2->level * Objects::Research::globalState.t2.upgradeCostQuantity : 0);
				auto turretBase = e.GetTurretBase();
				if (turretBase && turretBase->turret->CanDealDamage())
				{
					rs.impact += turretBase->GetDamagePotentialPerRound();
					entityRange = M::Max(entityRange,turretBase->GetFlatAttackRange());
				}
				rs.impact += entityRange;
				const M::TVec3<> worldLoc = Grid::ToWorld(loc);
				const float avg = Grid::low.neighborCellDistances.GetAverage();
				//const bool isHostile = owner != nexus;
				rs.isHostile = owner != nexus;
				rs.isConstructor = turretBase && turretBase->turret->CanDo(Objects::Component::Operation::Construct);
				rs.isPlane = e.IsPlane();
				rs.isStructure = e.IsStructure();

				if (rs.impact > 0)
				{

					float worldDistance = Vec::distance(Grid::low.cells[poleIndex].centerVertex.xyz * Grid::MeanPlanetRadius,worldLoc);
					if (rs.isHostile)
						worldDistance = M::Max(0,worldDistance-entityRange);


					const float stepDistance = worldDistance / Grid::MeanPlanetRadius / avg * 0.75f;

					const bool isBlack = stepDistance < 2;
					const bool isRed = stepDistance < 4;
					const bool isYellow = stepDistance < 6;
					const bool isNoticed = stepDistance < 9;
					rs.opacity = (float(isBlack?6:0) + float(isRed?3:0) + float(isYellow ? 2 : 0) + float(isNoticed))/3.f;	//0->1/3->1->2
					rs.isRedZone = isRed;
					rs.totalImpact = rs.opacity * rs.impact;
				}
				return rs;
			}

			/**
			Adds a friendly or hostile presence to the environments of nexus ports
			@param e Entity to add presence estimations of. Friendly structures are ignored
			@param location Location of the entity. Either its own or of the producer it is queued in
			@param[in,out] Port environments to add to. The specified entity may be added to multiple environments
			*/
			void AddPresence( const Objects::PFaction&nexus, const Objects::WorldEntity&e, const Grid::LCellIdI&location )
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
					foreach (poles, at)
					{
						TEstimation est = _Estimate(nexus, e, location, e.owner, at-poles.begin());
				
						if (est.isHostile)
						{
							#ifndef CLIENT
								if (est.isPlane)
									at->hostileAirSum += est.totalImpact;
								else
									at->hostileGroundSum += est.totalImpact;
								if (est.isConstructor && est.isRedZone)
								{
									at->constructorNearNexus += est.opacity;
									at->highestConstructorNearNexus = std::max(at->highestConstructorNearNexus,est.opacity);
								}
							#endif
						}
						else
						{
							if (est.isPlane)
								at->friendlyAirSum += est.totalImpact;
							else
							{
								at->friendlyGroupSum[0] += est.isStructure ? 0 : est.totalImpact;
								at->friendlyGroupSum[1] += est.totalImpact;
							}
						}
					}
			}

			Objects::PFaction GetNexus()
			{
				#ifdef CLIENT
					return Objects::Faction::mine;
				#else
					return Objects::Faction::nexus;
				#endif;
			}


			void UpdateAssociatedEntities()
			{
				bool anyBases = false;
				foreach (poles,e)
				{
					e->ClearAssociation();
					anyBases |= e->hasBase;
				}

				if (!anyBases)
					return;

				Objects::PFaction nexus = GetNexus();
			
				if (!nexus)
					return;

				ForeachWorldEntityIncludingStored([nexus](const Objects::PWorldEntity&e)
				{
					bool isMine = e->owner == nexus;

					AddPresence(nexus, *e,e->location);

					if (isMine)
					{
						if (e->IsWorkingStructure())
						{
							auto ws = To<Objects::WorkingStructure>::Cast(e);
							if (ws->IsNexusPort())
								return;
							float closestAt = std::numeric_limits<float>::lowest();
							index_t closest = InvalidIndex;
							foreach (poles,pole)
							{
								if (!pole->hasBase)
									continue;
								float d = Vec::dot(Grid::low.cells[e->location.stackID].centerVertex.xyz,pole->center);
								if (d > closestAt)
								{
									closestAt = d;
									closest = pole - poles.begin();
								}
							}
							poles[closest].associatedWorkingStructures << ws;


							Objects::PVehiclePlant plant = To<Objects::VehiclePlant>::DynamicCast(e);
							if (plant)
							{
								float closestAt = std::numeric_limits<float>::lowest();
								index_t closest = InvalidIndex;
								foreach (poles,pole)
								{
									if (!pole->hasBase)
										continue;
									float d = Vec::dot(Grid::low.cells[e->location.stackID].centerVertex.xyz,pole->center);
									if (d > closestAt)
									{
										closestAt = d;
										closest = pole - poles.begin();
									}
								}
								poles[closest].associatedVehiclePlants << plant;

								if (plant->queue.IsNotEmpty())
									AddPresence(nexus,*plant->queue.First().GetPrototype(),plant->location);
							}

							
							Objects::PPlaneFactory factory = To<Objects::PlaneFactory>::DynamicCast(e);
							if (factory)
							{
								float closestAt = std::numeric_limits<float>::lowest();
								index_t closest = InvalidIndex;
								foreach (poles,pole)
								{
									if (!pole->hasBase)
										continue;
									float d = Vec::dot(Grid::low.cells[e->location.stackID].centerVertex.xyz,pole->center);
									if (d > closestAt)
									{
										closestAt = d;
										closest = pole - poles.begin();
									}
								}
								poles[closest].associatedPlaneFactories << factory;
							}

						}
					}
				});
			}
		}


		void					Update(const std::function<bool(index_t)>&poleHasBase)
		{
			foreach (Detail::poles,pole)
				pole->port = nullptr;

			Objects::structures.Foreach([poleHasBase](const Objects::PStructure&s)
			{
				if (   s->location.stackID >= 12					//structure is not located on pole (there are 12 poles and they are the first 12 cell stacks)
					|| s->GetType() != ConstName::Type_NexusPort	//we're looking at something else
					|| !s->owner
					|| !s->owner->IsNexus()			//the port no longer belongs to the nexus faction
					)
					return;
				auto&pole = Detail::poles[s->location.stackID];
				pole.port = To<Objects::NexusPort>::Cast(s);
				pole.friendlyAirSum = 0;
				pole.friendlyGroupSum[0] = 0;
				pole.friendlyGroupSum[1] = 0;
				#ifndef CLIENT
					pole.hostileGroundSum = 0;
					pole.hostileAirSum = 0;
					
					pole.constructorNearNexus = 0;
					pole.highestConstructorNearNexus = 0;
				#endif
				if (poleHasBase)
					pole.hasBase = poleHasBase(s->location.stackID);
				pole.isInEMPZone = Grid::AffectedByEMP(s->location);
				pole.center = Grid::low.cells[s->location.stackID].centerVertex.xyz;
			});





		}


		void					Create()
		{


			GlobalEvents::phaseChange << []()
			{
				if (RuntimeConfig::IsInAnyCommandPhase())
					Update(nullptr);
			};
		}



		bool					IsBase(index_t poleIndex)
		{
			Detail::CheckIndex(poleIndex);
			return Detail::poles[poleIndex].hasBase;
		}

		float					SumGroundHostiles(index_t poleIndex)
		{
			Detail::CheckIndex(poleIndex);
			return Detail::poles[poleIndex].hostileGroundSum;
		}

		float					SumAirHostiles(index_t poleIndex)
		{
			Detail::CheckIndex(poleIndex);
			return Detail::poles[poleIndex].hostileAirSum;
		}

		float					SumGroundFriendlies(index_t poleIndex, bool includeStationaries)
		{
			Detail::CheckIndex(poleIndex);
			return Detail::poles[poleIndex].friendlyGroupSum[includeStationaries ? 1 : 0];
		}

		float					SumAirFriendlies(index_t poleIndex)
		{
			Detail::CheckIndex(poleIndex);
			return Detail::poles[poleIndex].friendlyAirSum;
		}

		float					SumHostileConstructorPresence(index_t poleIndex)
		{
			Detail::CheckIndex(poleIndex);
			return Detail::poles[poleIndex].constructorNearNexus;
		}

		float					GetClosestHostileConstructorPresence(index_t poleIndex)
		{
			Detail::CheckIndex(poleIndex);
			return Detail::poles[poleIndex].highestConstructorNearNexus;
		}

		ArrayRef<PVehiclePlant>	GetBaseVehiclePlants(index_t poleIndex)
		{
			Detail::CheckIndex(poleIndex);
			return Detail::poles[poleIndex].associatedVehiclePlants.ToRef();
		}

		ArrayRef<PPlaneFactory>	GetBasePlaneFactories(index_t poleIndex)
		{
			Detail::CheckIndex(poleIndex);
			return Detail::poles[poleIndex].associatedPlaneFactories.ToRef();
		}

		Objects::PNexusPort		GetPort(index_t poleIndex)
		{
			Detail::CheckIndex(poleIndex);
			return Detail::poles[poleIndex].port;
		}


		TEstimation				Estimate(index_t poleIndex, const Grid::LCellIdI&location, const Objects::EntityType&type, Objects::PFaction&owner)
		{
			Detail::CheckIndex(poleIndex);
			Objects::PWorldEntity proto = Objects::Prototypes::Find(type);
			if (!proto)
			{
				auto base = Objects::Component::baseComponents.Find(type.baseType);
				auto turret = Objects::Component::allTurretComponents.Find(type.turretType);
				if (!base)
					throw Except::Program::MemberNotFound(CLOCATION,"Base parameter not found");
				if (!turret)
					throw Except::Program::MemberNotFound(CLOCATION,"Turret parameter not found");
				proto.reset(new Objects::Vehicle(nullptr,Grid::LCellIdI(0,0),base,turret));
			}

			return Detail::_Estimate(Detail::GetNexus(),*proto,location,owner,poleIndex);
		}

		TEstimation				Estimate(index_t poleIndex, const PWorldEntity&e)
		{
			Detail::CheckIndex(poleIndex);
			return Detail::_Estimate(Detail::GetNexus(),*e,e->location,e->owner,poleIndex);
		}

		TEstimation				Estimate(index_t poleIndex, const PWorldEntity&e, const Grid::LCellIdI&location)
		{
			Detail::CheckIndex(poleIndex);
			return Detail::_Estimate(Detail::GetNexus(),*e,location,e->owner,poleIndex);
		}

		

		#ifdef CLIENT
			void				UpdatePole(index_t poleIdx, float groundSum, float airSum, bool hasBase, float constructorPresence, float highestConstructorPresence)
			{
				Detail::poles[poleIdx].hostileGroundSum = groundSum;
				Detail::poles[poleIdx].hostileAirSum = airSum;
				Detail::poles[poleIdx].hasBase = hasBase;
				Detail::poles[poleIdx].constructorNearNexus = constructorPresence;
				Detail::poles[poleIdx].highestConstructorNearNexus = highestConstructorPresence;
			}
		#endif

	}


}

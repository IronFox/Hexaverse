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
#include <container/bit_set.h>

namespace Objects
{
	namespace NexusThreat
	{
		namespace Detail
		{
			struct Pole
			{
				TLevel		threatLevel;
				TSupport	support;
				bool		hasBase = false, 
							deployHeavyVehicleBases = false;
				PNexusPort	port;
				bool		isInEMPZone = false;
				M::TVec3<>	center;
				Array<UINT32>distanceField;


				Buffer0<Objects::PWorkingStructure>	associatedWorkingStructures;
				Buffer0<Objects::PVehiclePlant> associatedVehiclePlants;
				Buffer0<Objects::PPlaneFactory> associatedPlaneFactories;
				Buffer0<Objects::PMobileEntity> associatedMobiles;

				void	Reset()
				{
					associatedWorkingStructures.Clear();
					associatedVehiclePlants.Clear();
					associatedPlaneFactories.Clear();
					associatedMobiles.Clear();
				}


				void	BuildDistanceField(UINT32 poleIdx)
				{
					if (distanceField.Count() == Grid::LowSetup::GridType::CellCount)
						return;
					Time::Stamp t0;

					Buffer0<Grid::TNeighborCellID>	neighborhood;
					Grid::low.GetExtendedNeighborhood(poleIdx,Grid::LowSetup::GridType::CellsPerEdge,neighborhood);
					distanceField.SetSize(Grid::LowSetup::GridType::CellCount);
					distanceField.Fill(std::numeric_limits<UINT32>::max());
					foreach (neighborhood,n)
						distanceField[n->stackID] = n->distanceFromOrigin;

					LogEvent(EventSignificance::PerformanceMeasurement,"Distance field computation around pole "+String(poleIdx)+" took seconds: "+String(t0.ElapsedSeconds()));
				}
			};


			FixedArray<Pole,12>	poles;
			float estimationFactor = 1;
			BitSet planeFactoryIsHere;


			bool	IsBlack(UINT32 stepDistance)
			{
				return stepDistance < 4;
			}

			bool	IsRed(UINT32 stepDistance)
			{
				return stepDistance < 8;
			}

			bool	IsYellow(UINT32 stepDistance)
			{
				return stepDistance < 11;
			}

			bool	IsNoticed(UINT32 stepDistance)
			{
				return stepDistance < 20;
			}


			void	CheckIndex(int poleIndex)
			{
				if (poleIndex >= 12 || poleIndex < 0)
					throw Except::Program::ParameterFault(CLOCATION,"Pole index "+String(poleIndex)+" is out of bounds (0..11)");
			}

			Pole& GetPole(int poleIndex)
			{
				CheckIndex(poleIndex);
				return poles[poleIndex];
			}
			
			TEstimation	_Estimate(const Objects::PFaction&nexus, const Objects::WorldEntity&e, const Grid::LCellIdI&loc, const Objects::PFaction&owner, int poleIndex)
			{
				TEstimation rs;

				const auto&pole = poles[poleIndex];

				rs.isPlane = e.IsPlane();
				rs.isHostile = owner != nexus;

				auto baseImpact = e.GetConstructionCost() + (e.t2 ? e.t2->level * Objects::Research::globalState.t2.upgradeCostQuantity : 0);
				auto turretBase = e.GetTurretBase();
				float entityRange;
				if (turretBase)
				{
					rs.sensor.impact = baseImpact + M::Max(turretBase->GetSubOrbitalScanningRange(),e.GetScanningRange());
					if (turretBase->turret->CanDealDamage())
						rs.attack.impact = baseImpact + turretBase->GetDamagePotentialPerRound() + turretBase->GetFlatAttackRange();
					else
						rs.attack.impact = baseImpact;

					entityRange = M::Max(turretBase->GetFlatAttackRange(),e.GetScanningRange());
				}
				else
				{
					rs.sensor.impact = baseImpact + e.GetScanningRange();
					rs.attack.impact = baseImpact;
					entityRange = e.GetScanningRange();
				}

				const M::TVec3<> worldLoc = Grid::ToWorld(loc);
				const float avg = Grid::low.neighborCellDistances.GetAverage();
				//const bool isHostile = owner != nexus;
				rs.isConstructor = turretBase && turretBase->turret->CanDo(Objects::Component::Operation::Construct);
				rs.isAirborne = e.IsAboveTerrain();
				if (turretBase)
				{
					if (e.IsPlane())
					{
						if (turretBase->CanAttackIgnoreAmmo())
						{
							rs.attack.groundActivity = 
							(
								turretBase->turret->trajectoryType == Objects::TrajectoryType::A2GMissile
								||
								turretBase->turret->trajectoryType == Objects::TrajectoryType::VerticalBeam
							) ? 1 : 0;

							rs.attack.airActivity = 1.f - rs.attack.groundActivity;
						}
						
						{
							rs.sensor.airActivity = 1;
							rs.sensor.groundActivity = std::min(1.f, float(turretBase->turret->groundScanningRange.GetValue(e.GetResearchState(owner))) / 
								e.GetScanningRange(owner));
						}
					}
					else
					{
						if (turretBase->CanAttackIgnoreAmmo())
						{
							rs.attack.airActivity = 
							(
								turretBase->turret->trajectoryType == Objects::TrajectoryType::AAMissile
							) ? 1 : 0;
							rs.attack.groundActivity = 1.f - rs.attack.airActivity;
						}


						rs.sensor.airActivity = turretBase->turret->subOrbitalScanningRange.IsFunctionOrConstNonZero() ? 1 : 0;
						if (rs.sensor.airActivity == 0)
							rs.sensor.groundActivity = 1;
						else
							rs.sensor.groundActivity = std::min(1.f, float(e.GetScanningRange(owner)) / turretBase->turret->subOrbitalScanningRange.GetValue(e.GetResearchState(owner))) * 0.25f;
					}
				}
				else
				{
					rs.sensor.airActivity = e.IsPlane() ? 1 : 0;
					rs.sensor.groundActivity = 1.f - rs.sensor.airActivity;
				}

				rs.isStationary = !e.IsMobile();

				//if (rs.TotalHostileImpact() > 0)
				{
					UINT32 locStack = loc.stackID;
					if (rs.isHostile)
					{
						float angleToPole = M::Vec::angle(worldLoc,pole.center);
						float angleToRange = acos(1.f - M::Sqr(entityRange) / (2.f * M::Sqr(Grid::ToRadius(loc.height)))) * 180 / M_PIF;
							//asin(entityRange / Grid::ToRadius(loc.height)) * 180 / M_PIF;
						if (angleToRange >= angleToPole)
						{
							locStack = poleIndex;
						}
						else
						{
							auto loc2 = worldLoc + (pole.center - worldLoc).Normalized() * entityRange;
							locStack = Grid::low.Locate(loc2,loc.stackID);
						}

					}
					UINT32 stepDistance = poles[poleIndex].distanceField[locStack];

					/*

					float worldDistance = Vec::distance(Grid::low.cells[poleIndex].centerVertex.xyz * Grid::MeanPlanetRadius,worldLoc);
					if (rs.isHostile)
						worldDistance = M::Max(0,worldDistance-entityRange);
					*/

					//const float stepDistance = worldDistance / Grid::MeanPlanetRadius / avg * 0.75f;

					const bool isBlack = IsBlack(stepDistance);
					const bool isRed = IsRed(stepDistance);
					const bool isYellow = IsYellow(stepDistance);
					const bool isNoticed = IsNoticed(stepDistance);
					rs.significance = (float(isBlack?6:0) + float(isRed?3:0) + float(isYellow ? 2 : 0) + float(isNoticed))/3.f;	//0->1/3->1->2->4(black)
					rs.distanceToPole = stepDistance;
					rs.attack.scaled = rs.significance * rs.attack.impact;
					rs.sensor.scaled = rs.significance * rs.sensor.impact;
				}
				return rs;
			}

			/**
			Adds a friendly or hostile presence to the environments of nexus ports
			@param e Entity to add presence estimations of. Friendly structures are ignored
			@param location Location of the entity. Either its own or of the producer it is queued in
			@param[in,out] Port environments to add to. The specified entity may be added to multiple environments
			*/
			void AddPresence( const Objects::PFaction&nexus, const Objects::WorldEntity&e, const Objects::PFaction&owner, const Grid::LCellIdI&location )
			{
				#ifdef CLIENT
					if (owner != nexus)
						return;
				#endif
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
						index_t poleIndex = at-poles.begin();
						TEstimation est = _Estimate(nexus, e, location, owner, (int)poleIndex);
				
						if (est.isHostile)
						{
							#ifndef CLIENT
								at->threatLevel += est;
							#endif
						}
						else
						{
							at->support += est;
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


			void UpdateAssociatedEntitiesAndLevels()
			{
				bool anyBases = false;
				foreach (poles,e)
				{
					e->Reset();
					anyBases |= e->hasBase;
				}

				planeFactoryIsHere.ResizeZero(Grid::LowSetup::GridType::CellCount);

				if (!anyBases)
					return;

				Objects::PFaction nexus = GetNexus();
			
				if (!nexus)
					return;

				ForeachWorldEntityIncludingStored([nexus](const Objects::PWorldEntity&e)
				{
					bool isMine = e->owner == nexus;

					AddPresence(nexus, *e, e->owner, e->location);




					if (isMine)
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

						if (e->IsWorkingStructure())
						{
							auto ws = To<Objects::WorkingStructure>::Cast(e);
							if (ws->IsNexusPort())
								return;

							poles[closest].associatedWorkingStructures << ws;


							Objects::PVehiclePlant plant = To<Objects::VehiclePlant>::DynamicCast(e);
							if (plant)
							{
								poles[closest].associatedVehiclePlants << plant;

								if (plant->queue.IsNotEmpty())
								{
									AddPresence(nexus,*plant->queue.First().GetPrototype(), nexus, plant->location);
								}
							}

							
							Objects::PPlaneFactory factory = To<Objects::PlaneFactory>::DynamicCast(e);
							if (factory)
							{
								poles[closest].associatedPlaneFactories << factory;
								planeFactoryIsHere.Set(factory->location.stackID);
							}

						}
						elif (e->IsMobile())
							poles[closest].associatedMobiles << To<Objects::MobileEntity>::Cast(e);

					}
				});
			}
		}


		Zone TEstimation::GetZone() const
		{
			if (Detail::IsBlack(distanceToPole))
				return Zone::Black;
			if (Detail::IsRed(distanceToPole))
				return Zone::Red;
			if (Detail::IsYellow(distanceToPole))
				return Zone::Yellow;
			if (Detail::IsNoticed(distanceToPole))
				return Zone::Noticed;
			return Zone::Irrelevant;
		}

		TLevel		TEstimation::ToThreat(bool includeThreatEstimationFactor) const
		{
			TLevel rs;
			rs += *this;
			if (includeThreatEstimationFactor)
				rs *= Detail::estimationFactor;
			return rs;
		}


		TSupport	TEstimation::ToSupport() const
		{
			TSupport rs;
			rs += *this;
			return rs;
		}


		void TSupport::operator+=(const TEstimation&est)
		{
			attack.Inc(est,est.attack);
			sensor.Inc(est,est.sensor);
		}


		void TVector::Inc(const TEstimation&est, const TAspect&aspect)
		{
			if (est.TotalHostileImpact() == 0)
				return;
			if (est.isHostile)
			{
				if (est.isPlane)
					air.Inc(est,aspect);
				else
					ground.Inc(est,aspect);
				return;
			}

			if (est.isStationary)
			{
				air.stationary += aspect.scaled * aspect.airActivity;
				ground.stationary += aspect.scaled * aspect.groundActivity;
			}
			else
			{
				air.mobile += aspect.scaled * aspect.airActivity;
				ground.mobile += aspect.scaled * aspect.groundActivity;
			}
		}

		void TLevel::operator+=(const TEstimation&est)
		{
			TAspect asp = TAspect::Max(est.attack,est.sensor);

			TVector::Inc(est,asp);

			if (est.isConstructor)
			{
				constructorSignificanceSum += est.significance;
				highestConstructorSignificance = std::max(highestConstructorSignificance,est.significance);
			}
		}



		void					SetEstimationFactor(float estimationFactor)
		{
			Detail::estimationFactor = estimationFactor;
		}


		void					Update(const std::function<bool(UINT32)>&poleHasBase, const std::function<bool(const Objects::PNexusPort&)>&deployHeavyVehicleBases)
		{
			LogEvent(EventSignificance::Common,"AI: Updating threat/support presence");
			FixedArray<Objects::PNexusPort,12> oldPorts;
			for (index_t i = 0; i < 12; i++)
			{
				oldPorts[i] = Detail::poles[i].port;
				Detail::poles[i].port = nullptr;
			}



			Objects::structures.Foreach([poleHasBase,deployHeavyVehicleBases,&oldPorts](const Objects::PStructure&s)
			{
				if (   s->location.stackID >= 12					//structure is not located on pole (there are 12 poles and they are the first 12 cell stacks)
					|| s->GetType() != ConstName::Type_NexusPort	//we're looking at something else
					|| !s->owner
					|| !s->owner->IsNexus()			//the port no longer belongs to the nexus faction
					)
					return;
				auto&pole = Detail::poles[s->location.stackID];
				pole.port = To<Objects::NexusPort>::Cast(s);
				pole.BuildDistanceField(s->location.stackID);

				pole.support = TSupport();
				#ifndef CLIENT
					pole.threatLevel = TLevel();
				#endif
				if (poleHasBase)
					pole.hasBase = poleHasBase(s->location.stackID);
				pole.isInEMPZone = Grid::AffectedByEMP(s->location);
				pole.center = Grid::low.cells[s->location.stackID].centerVertex.xyz;
				if (deployHeavyVehicleBases)
					pole.deployHeavyVehicleBases = deployHeavyVehicleBases(pole.port);
			});


			for (int i = 0; i < 12; i++)
				Detail::poles[i].BuildDistanceField(i);

			Detail::UpdateAssociatedEntitiesAndLevels();

			for (int i = 0; i < 12; i++)
				LogEvent(EventSignificance::Common,"AI: Nexus pole #"+String(i)+": threat={"+ToString(Detail::poles[i].threatLevel)+"}, supp={"+ToString(Detail::poles[i].support)+"}");


		}


		bool					DeployHeavyVehicleBases(int poleIndex)
		{
			return Detail::GetPole(poleIndex).deployHeavyVehicleBases;
		}



		bool					IsPlaneFactoryHere(UINT32 stackID)
		{
			return Detail::planeFactoryIsHere.IsSet(stackID);
		}



		bool					IsBase(int poleIndex)
		{
			return Detail::GetPole(poleIndex).hasBase;
		}

		bool					HasPort(int poleIndex)
		{
			return Detail::GetPole(poleIndex).port != nullptr;
		}

		bool					IsInEmpZone(int poleIndex)
		{
			return Detail::GetPole(poleIndex).isInEMPZone;
		}

		float					GetEstimationFactor()
		{
			return Detail::estimationFactor;
		}

		const TLevel&			Get(int poleIndex)
		{
			return Detail::GetPole(poleIndex).threatLevel;
		}

		const TSupport&			GetSupport(int poleIndex)
		{
			return Detail::GetPole(poleIndex).support;
		}


		ArrayRef<PVehiclePlant>	GetBaseVehiclePlants(int poleIndex)
		{
			return Detail::GetPole(poleIndex).associatedVehiclePlants.ToRef();
		}

		ArrayRef<PPlaneFactory>	GetBasePlaneFactories(int poleIndex)
		{
			return Detail::GetPole(poleIndex).associatedPlaneFactories.ToRef();
		}

		ArrayRef<PMobileEntity>	GetOwnMobiles(int poleIndex)
		{
			return Detail::GetPole(poleIndex).associatedMobiles.ToRef();
		}


		Objects::PNexusPort		GetPort(int poleIndex)
		{
			return Detail::GetPole(poleIndex).port;
		}


		TEstimation				Estimate(int poleIndex, const Grid::LCellIdI&location, const Objects::EntityType&type, Objects::PFaction&owner)
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

		TEstimation				Estimate(int poleIndex, const PWorldEntity&e)
		{
			Detail::CheckIndex(poleIndex);
			return Detail::_Estimate(Detail::GetNexus(),*e,e->location,e->owner,poleIndex);
		}

		TEstimation				Estimate(int poleIndex, const PWorldEntity&e, const Grid::LCellIdI&location)
		{
			Detail::CheckIndex(poleIndex);
			return Detail::_Estimate(Detail::GetNexus(),*e,location,e->owner,poleIndex);
		}

		

		#ifdef CLIENT
			void				UpdatePole(int poleIdx, const TLevel&lvl, bool deployHeavyVehicleBases, bool hasBase)
			{
				Detail::poles[poleIdx].threatLevel = lvl;
				Detail::poles[poleIdx].deployHeavyVehicleBases = deployHeavyVehicleBases;
				Detail::poles[poleIdx].hasBase = hasBase;
			}
		#endif

	}


}

#include <global_root.h>
#include "vehicles.h"
#include <objects/interface.h>
#include "../objectNetworkHelper.h"
#include "scriptedAI.h"
#include "../playerControlBegin.h"
#include <objects/nexus/nexusThreatEstimation.h>
#include "../gridControl.h"


namespace AI
{
	namespace Detail
	{
		Buffer0<Objects::PVehicle>	attackers,scanners,aa, processed, pending;
		HashSet<TUniqueID>		ofInterestToAttacker;
		Buffer0<M::TVec3<> >	tempMarkers;
		HashTable<TUniqueID,Grid::LCellIdI>	 destHistory;

		UINT32	minRoundForHeavyVehicleBases = 10,
				minRoundForPlanes = 10;
		bool	awaitSensory = false;

		Random random;

		bool done = true;

		struct TThreatEstimation;

		struct TThreatContext
		{
			bool	selfIsOffensiveVehicle = false,
					selfCanIgnoreThreat = false,
					selfIsOverconfident = false,
					selfIsScanner = false;
			Objects::PWorldEntity self;

			void	EstimateDeadThreatLevelFrom(const Objects::TrajectoryEndPoint&loc, Objects::WorldEntity&e, TThreatEstimation &current) const;
			void	EstimateLivingThreatLevelFrom(const Objects::TrajectoryEndPoint&loc, Objects::WorldEntity&e, TThreatEstimation &current) const;
			void	EstimateThreatFromAll(const Objects::TrajectoryEndPoint&loc, bool ignoreHostiles, TThreatEstimation&threat) const;
		};
		TThreatContext		currentContext;

		
		struct TThreatEstimation
		{
			bool	beingSeen = false;
			int		damageThreat = 0;
			float	unknownDistanceThreat = 0.f;

	

			bool	ShouldBlock(const TThreatEstimation&base, bool ignoreUnknownDistanceThreat) const
			{
				const TThreatContext&ctx = currentContext;


				if (!beingSeen && (ctx.selfIsOffensiveVehicle || ctx.selfIsScanner) && unknownDistanceThreat < 0.2f)
					return false;

				if (damageThreat > base.damageThreat)
					return true;
				if (ignoreUnknownDistanceThreat)
					return false;

				return unknownDistanceThreat * 0.8f > base.unknownDistanceThreat;
			}
		};

		HashTable<Grid::LCellIdI,TThreatEstimation> threatCache;
		TThreatEstimation	ambientThreat;

		
		struct TAttackTarget
		{
			Grid::LCellIdI target;
			M::TVec3<>		explosionAt;
			float		priority;
			bool		midflightExplosion = false;
			TUniqueID	targetID;

			bool		operator<(const TAttackTarget&other) const
			{
				return priority > other.priority;	//higher first
			}
		};

		Buffer0<TAttackTarget>	attackTarget;

		void ProcessAttackTarget(const Objects::Vehicle&source, const Objects::WorldEntity&candidate,  BasicBuffer<TAttackTarget>&attackTarget, bool attacksAirborne, bool attacksGroundEntities);
		float QuadraticInterestDistance(const Objects::TrajectoryEndPoint&self, const Objects::PWorldEntity&other, bool interestedInAirborne, bool interestedInGround);
		float EstimateInterest(const Objects::PTurretBase&base, int baseAtHeight, float constructionCost, float distance2);
		bool Seeing(const Objects::WorldEntity&e, const Objects::TrajectoryEndPoint&loc, float&effectiveSensoryRange);
		void AddSameTurretBadness(float&badWeight, const Grid::LCellIdI&myLoc, const Grid::LCellIdI&theirLoc, const Objects::PVehicle&v0, const Objects::PVehicle&v1, bool isAA);


		bool ProcessVehicle( const Objects::PVehicle&v )
		{
			Objects::Component::Turret*t = v->turret;
			if (t->detectsResources || t->CanDo(Objects::Component::Operation::Construct))
			{
				LogEvent(EventSignificance::Uncommon,v.get(),  "AI: Skipping unsupported vehicle type");
				return true;
			}

			int referenceSpeed = v->GetMovementStepsPerRound()*3;
			int minMotion = v->GetMovementStepsPerRound();

			//attacker logic
			Objects::Pathfinding::ReachabilityMapper::MapNode*best = nullptr, *current = nullptr;
			float bestScore = 0.f, currentScore = 0.f;
			const bool attacker = t->CanAttack();
			const bool attacksAirborne = attacker && (v->IsPlane() || t->trajectoryType == Objects::TrajectoryType::AAMissile);
			const bool detectsAirborne = v->IsPlane() || v->GetSubOrbitalScanningRange() > 0;
			const bool attacksGroundEntities = attacker 
				&& ( 
					(!v->IsPlane() && t->trajectoryType != Objects::TrajectoryType::AAMissile) 
					|| (v->IsPlane() 
						&& (t->trajectoryType == Objects::TrajectoryType::A2GMissile || t->trajectoryType == Objects::TrajectoryType::VerticalBeam)
						)
					);
			const bool isAA = !v->IsPlane() && attacksAirborne;

			const bool interestedInAirborne = attacksAirborne || v->GetSubOrbitalScanningRange() > 0 || (v->IsPlane() && !attacksGroundEntities);
			const bool interestedInGroundTargets = attacksGroundEntities || !v->IsPlane() || v->turret->groundScanningRange.GetValue(v->GetResearchState()) >= 0.5f * v->GetScanningRange();


	//		const float myRange = t->maxRange + t->GetExplosionRadius(e.owner) - 1;



			#ifndef ONE_COMPOUND_PER_FACTION
				if (attacker && !v->HasAmmoForShot())
				{
					LogEvent(EventSignificance::Debug,v.get(),"AI: Out of ammo: destroying vehicle");
					ApplicationInterface::SignalRemoval(v);
					return;
				}
			#endif
			//if (v->tasks.IsNotEmpty())
			{
				//LogEvent(EventSignificance::Debug,v.get(),"AI: dropping tasks");
				ApplicationInterface::SignalDropAllTasks(v);
			}


		
			bool haveMarker = false;
			if (!isAA)
			{
				Objects::MarkerStore::client.Foreach(Objects::MarkerType::Visited,
					[v,&haveMarker](const Objects::PMarker&m)
					{
						float distance = Vec::quadraticDistance(Grid::ToWorld(m->location),v->system.w.xyz);
						if (distance < 25.f)
						{
							ObjectHelper::SignalMarkerTouch(*m);
							haveMarker = true;
						}
					}
				);
				if (!haveMarker)
					foreach (tempMarkers,_m)
					{
						float distance = Vec::quadraticDistance(*_m,v->system.w.xyz);
						if (distance < 25.f)
						{
							haveMarker = true;
							break;
						}
					}

				if (!haveMarker)
				{
					Objects::VisitedMarker vm;
					vm.id.Generate();
					vm.location = v->location;
			
					Protocol::Marker marker;
					marker.markerType = vm.type;
					vm.PackToArray(marker.state, Objects::Packable::Target::Owner);

					tempMarkers << v->system.w.xyz;
					ObjectHelper::SignalMarkerCreation(marker);
				}
			}
			const float explosionRadius = v->GetAttackExplosionRadius();


			if (attacker && !isAA)
			{
				String id = ToString(v->id);
				attackTarget.Clear();

				Objects::ForeachWorldEntity([v,attacksAirborne, attacksGroundEntities](const Objects::PWorldEntity&e)
				{
					Detail::ProcessAttackTarget(*v,*e,attackTarget,attacksAirborne, attacksGroundEntities);
				},Objects::IncludeGhosts::HostileContacts);

				for(index_t at = 0; at < attackTarget.Count(); at++)
				{
					if (!attackTarget[at].midflightExplosion)
						continue;
					const M::TVec3<>&ref = attackTarget[at].explosionAt;
					bool conflict = false;
					Objects::ForeachWorldEntity([ref,explosionRadius,&conflict](const Objects::PWorldEntity&e)
					{
						if (!conflict && e->isOwnedByMe)
						{
							float distance = Vec::quadraticDistance(e->GetWorldPosition(),ref);
							if ( distance < M::Sqr(explosionRadius))
							{
								conflict = true;
							}
						}
					},Objects::IncludeGhosts::None);

					if (conflict)
					{
						attackTarget.Erase(at);
						at--;
					}
				}
				std::sort(attackTarget.begin(),attackTarget.end());
				attackTarget.Truncate(v->GetAttacksPerRound());
				static Array<Grid::LCellIdI>	targets;
				targets.SetSize(attackTarget.Count());
				foreach (attackTarget,t)
				{
					targets[t-attackTarget.pointer()] = t->target;
				}
				ObjectHelper::DirectTo(v,targets,ObjectHelper::AssembleMotionFlags( false,true, false),Objects::CursorMode::AttackMode);

				if (attackTarget.IsNotEmpty() && !v->base->ignoresVehicleAttacks)
				{
					foreach (attackTarget,t)
						ofInterestToAttacker.Set(t->targetID);
					return true;
				}
			}

			bool overconfident = false;
			{
				count_t proximityPresence = 0,
						radarProximity = 0,
						aaProximity = 0;
				bool primary = true;
				bool primaryAA = true;
				Objects::ForeachMobileWorldEntity([v,&proximityPresence,&radarProximity,&aaProximity,&primary,&primaryAA](const Objects::PMobileEntity&e)
				{
					if (e->isOwnedByMe && Vec::quadraticDistance(v->GetWorldPosition(),e->GetWorldPosition()) < 25.f)
					{
						if (e->turret->trajectoryType == Objects::TrajectoryType::AAMissile)
						{
							if (v->GetPrice() > e->GetConstructionCost() ||
								(v->GetPrice() == e->GetConstructionCost()
									&&
									v->id > e->id))
								primaryAA = false;
							aaProximity++;
						}
						elif (e->turret->CanDealDamage())
							proximityPresence++;
						else
						{
							if (v->GetPrice() > e->GetConstructionCost() ||
								(v->GetPrice() == e->GetConstructionCost()
									&&
									v->id > e->id))
								primary = false;
							radarProximity++;
						}
					}
				});
			
				overconfident = proximityPresence > 5 && !isAA;
				if (!attacker && radarProximity > 1 && primary && random.NextFloat() < 0.3f)
				{
					ApplicationInterface::SignalRemoval(v);
					return true;
				}

				if (isAA && aaProximity > 1 && primaryAA && random.NextFloat() < 0.3f)
				{
					ApplicationInterface::SignalRemoval(v);
					return true;
				}
			}



		
			static Objects::Pathfinding::ReachabilityMapper mapper(false);

			const bool canIgnoreThreat = ((attacker && !isAA) || random.NextBool()) && !t->dealsEMPDamage;
			BeginBlockTest(v,true,canIgnoreThreat,overconfident);	//force current to have no threat level, limiting motion to areas with zero threat

			const Objects::Component::GroundFlags groundFlags = Objects::Component::GroundFlags::DisallowUnknownGround;

			mapper.Map(*v, v->location,  Objects::Pathfinding::RoutingFlags::ForceRevertible | Objects::Pathfinding::RoutingFlags::IncludeOrigin, groundFlags,referenceSpeed, CostModifier);
			if (mapper.result.Count() <= 1)
			{
				//no chance to get out of danger, so try to reduce threat level
				BeginBlockTest(v,false,canIgnoreThreat,overconfident);
				mapper.Map(*v, v->location, Objects::Pathfinding::RoutingFlags::ForceRevertible, groundFlags,referenceSpeed, CostModifier);
				if (mapper.result.IsEmpty())
					mapper.Map(*v, v->location, Objects::Pathfinding::RoutingFlags::None, groundFlags,referenceSpeed, CostModifier);
			}


			{
				foreach (mapper.result,n)
				{
					if (v->IsPlane() && n->predecessor != InvalidIndex && n->cell.height != Objects::SubOrbital::Height)
						continue;//ignore all ground-based locations. they make no sense for a plane
					float goodWeight = 0;
					float badWeight = 0;
					bool abort = false;
					bool block = false;
					Objects::ForeachWorldEntity(
					[
						v,&block,&abort,attacker,explosionRadius,&goodWeight,
						&badWeight,isAA,interestedInAirborne,interestedInGroundTargets,n
					]
					(const Objects::PWorldEntity&e)
					{
						if (abort || block)
							return;
						if (e->isOwnedByMe IF_SATELLITES( || e->IsSatellite() ))
						{
							if (isAA)
							{
								//if (e->IsMobile())
								{
									float distance = QuadraticInterestDistance(Objects::TrajectoryEndPoint(n->cell),e,interestedInAirborne,interestedInGroundTargets);
									const auto base = e->GetTurretBase();
									if (base && base->turret->trajectoryType == Objects::TrajectoryType::AAMissile)
										badWeight += 20 / (1.f + distance);
									else
										goodWeight += 5 / (1.f + distance);
								}
								//if (distance < 16.f)
								//	badWeight += 100.f / (1.f + distance);	//repell close vehicles a bit
								return;
							}
							return;
						}
						if (isAA)
							return;
						if (e->IsAboveTerrain() && !interestedInAirborne)
							return;
						if (!e->IsAboveTerrain() && !interestedInGroundTargets)
							return;
						float distance = QuadraticInterestDistance(Objects::TrajectoryEndPoint(n->cell),e,interestedInAirborne,interestedInGroundTargets);

						float fc = EstimateInterest(e->GetTurretBase(),e->location.height,
									e->GetConstructionCost(),distance);
						if (distance < explosionRadius)
							badWeight += 20 / (1.f + distance);
						else
						{
							if (!attacker)
							{
								if (!e->IsAlive())
								{
									fc *= 2;
									if (ofInterestToAttacker.IsSet(e->id))
										fc *= 2;
								}
								elif (ofInterestToAttacker.IsSet(e->id))
								{
									if (World::actual.SensoryIsInvalid())
									{
										ClientGrid::CheckSensory();
										abort = true;
										return;
									}
									float effectiveSensoryRange;
									if (Seeing(*v,Objects::TrajectoryEndPoint(e->location),effectiveSensoryRange) && World::actual.GetLowestSensorStrength(e->location,e->GetHeight(),true) == 1)
									{
										if (distance <= M::Sqr(effectiveSensoryRange))
										{
											distance = 0;
											fc *= 10;	//only one seeing it. maintain visibility
										}
										else
										{
											block = true;
											fc = 0;
											badWeight += 1000;
											return;
										}
											//fc*100;
									}
								}
							}
							else
							{
								//if (fc > 0 && distance < M::Sqr(v->GetFlatAttackRange()*2.f))
									ofInterestToAttacker.Set(e->id);
							}
							goodWeight += fc / (1.f + distance);
						}
					},Objects::IncludeGhosts::HostileContacts);
					if (abort)
						return false;
					if (block)
						continue;

					//if (badWeight < 0.1f && goodWeight < 0.05f)

					if (!isAA)
					{
						const M::TVec3<> reference = Grid::ToWorld(n->cell);
						Objects::MarkerStore::client.Foreach(Objects::MarkerType::Visited,
							[reference, minMotion, n, &badWeight](const Objects::PMarker&m)
							{
								float distance = Vec::quadraticDistance(Grid::ToWorld(m->location),reference);
								//if (distance < 49.f || (n->movementCostsFromRoot >= minMotion && distance < 16.f))
								//if (distance < 16.f)
								badWeight += 10.f / (1.f + float(m->age) + distance * 0.2f);
							}
						);
						foreach (tempMarkers,_m)
						{
							float distance = Vec::quadraticDistance(*_m,reference);
							badWeight += 10.f / (1.f + distance * 0.2f);
							//if (distance < 49.f || (n->movementCostsFromRoot >= minMotion && distance < 16.f))
								//badWeight += 2.f;
						}

					}


					if (!attacker || isAA)
					{
						for (Objects::PVehicle v2 : processed)
							if (v2->turret == v->turret)
							{
								AddSameTurretBadness(badWeight, n->cell, v2->location,v,v2,isAA);
								Grid::LCellIdI directed;
								if (destHistory.Query(v2->id,directed))
									AddSameTurretBadness(badWeight, n->cell, directed,v,v2,isAA);
							}
						for (Objects::PVehicle v2 : pending)
							if (v2 != v && v2->turret == v->turret)
							{
								AddSameTurretBadness(badWeight, n->cell, v2->location,v,v2,isAA);
							}

					}


					float score = (1.f + goodWeight) / (1.f+badWeight);
					if (!isAA)
						score /= (1.f + float(abs(n->movementCostsFromRoot- minMotion * 3 / 2))*0.01f);

					if (!overconfident)
					{
						TThreatEstimation local;
						if (threatCache.Query(n->cell,local))
						{
							if (ambientThreat.ShouldBlock(local,true))
							{
								score += 3000.f / (1+n->movementCostsFromRoot);
							}
						}
					}

					{
						float extent = 1.f + M::Sqr(M::Sqr(M::Sqr(random.NextFloat())));
						if (random.NextBool())
							extent = 1.f / extent;
						score *= extent;				
					}
					//score *= random.NextFloat(0.8f,1.25f);
					if (score > bestScore)
					{
						best = n;
						bestScore = score;
					}

					if (n->predecessor == InvalidIndex)
					{
						current = n;
						currentScore = score;
					}
				}
				if (best)
				{
					if (!isAA || bestScore > currentScore * 1.2f)
					{
						IssuePath(v,mapper,best);
						destHistory.Set(v->id,best->cell);



						if (attacker)
						{
							Objects::ForeachWorldEntity(
							[interestedInAirborne,interestedInGroundTargets,best,v]
							(const Objects::PWorldEntity&e)
							{
								float distance = QuadraticInterestDistance(Objects::TrajectoryEndPoint(best->cell),e,interestedInAirborne,interestedInGroundTargets);
								if (distance < M::Sqr(2.f * v->GetFlatAttackRange()))
									ofInterestToAttacker.Set(e->id);
								distance = QuadraticInterestDistance(Objects::TrajectoryEndPoint(v->location),e,interestedInAirborne,interestedInGroundTargets);
								if (distance < M::Sqr(2.f * v->GetFlatAttackRange()))
									ofInterestToAttacker.Set(e->id);
							});
						}

					}
				}
			}
			return true;
		}





		float EstimateInterest(const Objects::PTurretBase&base, int baseAtHeight, float constructionCost, float distance2)
		{
			float rs = constructionCost;
		
			if (base && base->turret->CanDealDamage())
			{
				rs /= (1.f + float(base->GetDamagePotentialPerRound()) * base->GetFlatAttackRange() * 0.1f);
			}
			return rs * 3.f;
		}

		struct TExtendedNodeData
		{
			float	threadLevel = 0.f;
			bool	invalidDestination = false;
		};






		void TThreatContext::EstimateDeadThreatLevelFrom(const Objects::TrajectoryEndPoint&loc, Objects::WorldEntity&e, TThreatEstimation &current) const
		{
			if (e.IsAboveTerrain() != self->IsAboveTerrain())
				return;
			float distance = Vec::quadraticDistance(e.system.w.xyz,loc.worldLocation);

			current.unknownDistanceThreat += 1.f / (1.f + distance);
		}


		bool IsSubOrbital(const Grid::LCellIdI&loc)
		{
			return loc.height == Objects::SubOrbital::Height;
		}

		float3 ToSubOrbit(const Grid::LCellIdI&loc)
		{
			return Grid::ToWorld(Grid::LCellIdI(loc.stackID,Objects::SubOrbital::Height));
		}

		float3 ToGround(const Grid::LCellIdI&loc, const Objects::TrajectoryEndPoint&ref)
		{
			return Grid::ToWorld(Grid::LCellIdI(loc.stackID,ref.cellLocation.height));
		}

		float3 ToGround(const Grid::LCellIdI&loc, const Grid::LCellIdI&ref)
		{
			return Grid::ToWorld(Grid::LCellIdI(loc.stackID,ref.height));
		}


		bool Seeing(const Objects::WorldEntity&e, const Objects::TrajectoryEndPoint&loc, float&effectiveSensoryRange)
		{
		
			if (IsSubOrbital(loc.cellLocation))
			{
				if (e.IsAboveTerrain())
				{
					float distance = Vec::quadraticDistance(e.system.w.xyz,loc.worldLocation);
					effectiveSensoryRange = e.GetScanningRange();
					return distance < M::Sqr(effectiveSensoryRange);
				}
				else
				{
					float distance = Vec::quadraticDistance(ToSubOrbit(e.location),loc.worldLocation);
					auto turretBase = e.GetTurretBase();
					effectiveSensoryRange = turretBase ? turretBase->GetSubOrbitalScanningRange() : 0;
					return turretBase && distance < M::Sqr(effectiveSensoryRange);
				}
			}
			else
			{
				if (e.IsAboveTerrain())
				{
					float distance = Vec::quadraticDistance(e.system.w.xyz,ToSubOrbit(loc.cellLocation));

					auto turretBase = e.GetTurretBase();
					effectiveSensoryRange = turretBase ? turretBase->turret->groundScanningRange.GetValue(e.GetResearchState()) : 0;
					return turretBase && distance < M::Sqr(effectiveSensoryRange);
				}
				else
				{
					float distance = Vec::quadraticDistance(e.system.w.xyz,loc.worldLocation);

					effectiveSensoryRange = e.GetScanningRange();
					return distance < M::Sqr(effectiveSensoryRange);
				}
			}
		}


		void TThreatContext::EstimateLivingThreatLevelFrom(const Objects::TrajectoryEndPoint&loc, Objects::WorldEntity&e, TThreatEstimation &current) const
		{
			const TThreatContext&ctx = *this;
			if (ctx.selfCanIgnoreThreat && e.ignoreThreatRounds)
				return;
			if (e.isOwnedByMe)
				return;
			//Objects::PTurretBase b = e.GetTurretBase();

			float dummy;
			if (Seeing(e,loc,dummy))
				current.beingSeen = true;



		
			const auto&state = e.GetResearchState();

			if (e.CanHit(loc.worldLocation))
			{
				Objects::PTurretBase b = e.GetTurretBase();
				current.damageThreat += b->GetDamagePotentialPerRound();
			}
		}


		void TThreatContext::EstimateThreatFromAll(const Objects::TrajectoryEndPoint&loc, bool friendlyDeathsOnly, TThreatEstimation&threat) const
		{
		
			threat = TThreatEstimation();
			if (!friendlyDeathsOnly)
				Objects::ForeachWorldEntity([&threat,this,loc](const Objects::PWorldEntity&e)
				{
					EstimateLivingThreatLevelFrom(loc,*e,threat);
				},Objects::IncludeGhosts::HostileContacts);

			Objects::MarkerStore::client.GetArtifacts().ForeachGhost([&threat,this,loc](const Objects::PWorldEntity&e)
			{
				EstimateDeadThreatLevelFrom(loc,*e,threat);
			},Objects::IncludeGhosts::EntityDeaths);

			threatCache.Set(loc.cellLocation,threat);
		}




		


		float ProcessAttackPriority(Objects::WorldEntity&e,const M::TVec3<>&explosionCenter,float maxExplosionRadius)
		{
			if (e.isOwnedByMe || Vec::quadraticDistance(explosionCenter,e.system.w.xyz) > M::Sqr(maxExplosionRadius))
				return 0.f;
			Objects::PTurretBase b = e.GetTurretBase();
			float rs = e.GetScanningRange();
			if (b)
				rs = std::max(rs, b->GetFlatAttackRange() + b->GetAttackExplosionRadius());
			int health = e.health >0 ? e.health : e.GetMaxHealth();
			return rs / health;
		}

		void ProcessAttackTarget(const Objects::Vehicle&source, const Objects::WorldEntity&candidate,  BasicBuffer<TAttackTarget>&attackTarget, bool attacksAirborne, bool attacksGroundEntities)
		{
			if (candidate.isOwnedByMe || candidate.IsIndestructible())
				return;
			if (candidate.IsAboveTerrain() && !attacksAirborne)
				return;
			if (!candidate.IsAboveTerrain() && !attacksGroundEntities)
				return;

			//float distance = Vec::quadraticDistance(source.system.w.xyz,candidate.system.w.xyz);
			//const float range = source.GetAttackRange();
			Objects::TurretBase::TargetChoice choice;
			if (source.ExtCanHit(source.owner, Objects::TrajectoryEndPoint( source.location,source.base->height),Objects::TrajectoryEndPoint( candidate.location,0 ),choice))
			{
				const float explosionRadius = source.GetAttackExplosionRadius();

				const auto rs = source.TraceTrajectory(Objects::TrajectoryEndPoint( choice.cellTarget),true);
				LogEvent(EventSignificance::Debug,&source,"Checking possible attack trajectory to "+candidate.GetName()+", attacking "+ToString(choice.cellTarget)+", detonating at "+ToString(rs));

				if (rs == Objects::TraceResult::InvalidTrajectory)
				{
					LogEvent(EventSignificance::Debug,&source,"Cannot trace good trajectory to destination. Invalid trajectory");
					return;
				}
				if (rs != Objects::TraceResult::ReachesTarget)
				{
					//float distanceToTarget = Vec::distance(detonationAt,candidate.system.w.xyz);
					if (rs.distanceToTarget > explosionRadius)
					{
						LogEvent(EventSignificance::Debug,&source,"Cannot trace good trajectory to destination. Final distance is "+String(rs.distanceToTarget)+"/"+String(explosionRadius));
						return;
					}
				}
	
				const auto&state = source.GetResearchState();

				{
					LogEvent(EventSignificance::Debug,&source,"Trajectory checks out");
					count_t needAttacks = ceil(float(std::max(1,candidate.health)) / source.GetDamageLevel());
					if (rs != Objects::TraceResult::ReachesTarget)
						needAttacks ++;
					//if (rs == Objects::Turret::ReachesTarget)
					float priority = 0.f;
					Objects::ForeachWorldEntity([&priority,explosionRadius,rs](const Objects::PWorldEntity&e)
					{
						priority += ProcessAttackPriority(*e,rs.impactAt,explosionRadius);
					},Objects::IncludeGhosts::HostileContacts);
					LogEvent(EventSignificance::Debug,&source,"Base Priority considered "+String(priority));
					for (index_t i = 0; i < needAttacks; i++)
					{
						TAttackTarget&t = attackTarget.Append();
						t.targetID = candidate.id;
						t.explosionAt = rs.impactAt;
						t.priority = priority / (1.f + float(i)*0.1f);
						t.target = choice.cellTarget;
						t.midflightExplosion = (rs == Objects::TraceResult::MidFlightImpact);
					}
				}
			}	
	
		}

		float QuadraticInterestDistance(const Objects::TrajectoryEndPoint&self, const Objects::PWorldEntity&other, bool interestedInAirborne, bool interestedInGround)
		{
			if (IsSubOrbital(self.cellLocation))
			{
				if (other->IsAboveTerrain())
				{
					float distance = Vec::quadraticDistance(self.worldLocation,other->system.w.xyz);
					return distance;
				}

				return Vec::quadraticDistance(self.worldLocation,ToSubOrbit(other->location));
			}
			else
			{
				if (other->IsAboveTerrain())
					return Vec::quadraticDistance(ToSubOrbit(self.cellLocation),other->system.w.xyz);
				float distance = Vec::quadraticDistance(self.worldLocation,other->system.w.xyz);
				return distance;
			}





			/*

			if (other->IsAboveTerrain())
			{
				if (self.height == Objects::SubOrbital::Height)
					return Vec::quadraticDistance(Grid::ToWorld(self),other->system.w.xyz);
				if (interestedInAirborne)
				{
					auto airborne = Grid::ToWorld(Grid::LCellIdI(self.stackID,Objects::SubOrbital::Height));
					return Vec::quadraticDistance(airborne,other->system.w.xyz);
				}
				return std::numeric_limits<float>::max();	//shouldn't happen
			}

			if (self.height < Objects::SubOrbital::Height)
				return Vec::quadraticDistance(Grid::ToWorld(self),other->system.w.xyz);
			if (interestedInGround)
			{
				auto ground = Grid::ToWorld(Grid::LCellIdI(self.stackID,other->location.height));
				return Vec::quadraticDistance(ground,other->system.w.xyz);
			}
			return std::numeric_limits<float>::max();	//shouldn't happen
			*/
		}


		void AddSameTurretBadness( float&badWeight, const Grid::LCellIdI&myLoc, const Grid::LCellIdI&theirLoc, const Objects::PVehicle&v0, const Objects::PVehicle&v1, bool isAA )
		{
			if (isAA)
			{
				float distance = Vec::quadraticDistance(ToSubOrbit(myLoc),ToSubOrbit(theirLoc));
				float minDist = M::Sqr(std::max( v0->GetSubOrbitalScanningRange(), v1->GetSubOrbitalScanningRange()));
				if (distance > minDist)
					return;
				badWeight += 30.f / (1.f + distance * 0.2f);
				return;
			}

			float distance = Grid::GetQuadraticGeometricDistance(myLoc,theirLoc);
			float minDist = M::Sqr(std::max( v0->GetScanningRange(), v1->GetScanningRange()));
			if (distance > minDist)
				return;
			badWeight += 30.f / (1.f + distance * 0.2f);
		}


		void	ProcessIgnorance(Objects::WorldEntity&e)
		{
			if (e.isOwnedByMe)
				return;
			e.ignoreThreatRounds --;
			if (e.ignoreThreatRounds < 0)
				e.ignoreThreatRounds = 0;
			Objects::PTurretBase b = e.GetTurretBase();
			const M::TVec3<>&ref = e.GetWorldPosition();
			if (b && b->turret->CanDealDamage())
			{
				float r = b->GetFlatAttackRange() + b->GetAttackExplosionRadius();
				float damagePotentialPerRound = 0.f;
				count_t participants = 0;
				Objects::ForeachMobileWorldEntity([ref,r,&damagePotentialPerRound,&participants,&e](const Objects::PMobileEntity&e1)
				{
					bool canParticipate = (!e.IsAboveTerrain() && (!e1->IsPlane() || e1->GetType() == Objects::SubOrbital::Bomber::globalPrototype->GetType()))
						|| (e.IsAboveTerrain() && (e1->GetType() == Objects::SubOrbital::Raptor::globalPrototype->GetType() || e1->turret->trajectoryType == Objects::TrajectoryType::AAMissile));
					if (!canParticipate)
						return;

					if (e1->isOwnedByMe && e1->turret->CanAttack() && !e1->turret->dealsEMPDamage)
					{
						float d2 = Vec::quadraticDistance(e1->GetWorldPosition(),ref);
						if (d2 < M::Sqr(r*1.2f))	//might be okay anyway
						{
							float p = e1->GetDamagePotentialPerRound();
							if (!e1->turret->independentAttackAndMotion)
								p *= 0.5f;
							damagePotentialPerRound += p;
							participants++;
						}
					}
				});

				if (participants > 1)
				{
					float potentialLeft = damagePotentialPerRound * (participants -1) / float(participants);
					int health = e.health;
					if (e.IsDeadOrGhost())	//ghost
						health = e.GetMaxHealth();
					if (potentialLeft > health * 2)
					{
						e.ignoreThreatRounds = 3;
					}
				}
			}
		}

	}


	void ClearPending()
	{
		Detail::attackers.Clear();
		Detail::scanners.Clear();
		Detail::aa.Clear();
		Detail::processed.Clear();
		Detail::pending.Clear();
	}

	void	AddVehicle(const Objects::PVehicle&v)
	{
		if (v->CanAttack())
			Detail::attackers << v;
		elif (v->turret == &Objects::Component::antiAircraft)
			Detail::aa << v;
		elif (v->turret == &Objects::Component::radar 
			|| v->turret == &Objects::Component::SubOrbital::sensorDrone)
			Detail::scanners << v;
		else
			LogEvent(EventSignificance::Common,"AI: Skipping "+v->GetName());
	}




	void	SetMinRoundForHeavyVehicleBases(UINT32 v)
	{
		Detail::minRoundForHeavyVehicleBases = v;
	}

	void	SetMinRoundForPlanes(UINT32 v)
	{
		Detail::minRoundForPlanes = v;
	}



	void	BeginBlockTest( const Objects::PWorldEntity&v, bool initWithFriendlyDeathsOnly, bool canIgnoreThreat, bool overConfident )
	{
		auto base = v->GetTurretBase();
		Detail::currentContext.selfIsOffensiveVehicle = base && base->turret->CanAttack() && base->turret->independentAttackAndMotion;
		Detail::currentContext.selfIsScanner = base && (base->turret == &Objects::Component::radar || base->turret == &Objects::Component::SubOrbital::sensorDrone);
		Detail::currentContext.selfCanIgnoreThreat = canIgnoreThreat;
		Detail::currentContext.selfIsOverconfident = overConfident;
		Detail::currentContext.self = v;
		Detail::currentContext.EstimateThreatFromAll(Objects::TrajectoryEndPoint(v->location),initWithFriendlyDeathsOnly, Detail::ambientThreat);	//determine base threat based on ghosts
	}

	void CostModifier( const Grid::LCellIdI&location, int&cost )
	{
		if (Detail::currentContext.self->IsPlane())
		{
			if (Objects::NexusThreat::IsPlaneFactoryHere(location.stackID))
			{
				cost = -1;
				return;
			}
		}

		Detail::TThreatEstimation threat;
		Detail::currentContext.EstimateThreatFromAll(Objects::TrajectoryEndPoint(location),false,threat);

		if (threat.ShouldBlock(Detail::ambientThreat,false))
			cost = -1;
	}


	void AssemblePath(const Objects::Pathfinding::ReachabilityMapper&mapper, const Objects::Pathfinding::ReachabilityMapper::MapNode*current, BasicBuffer<Grid::LCellIdI>&outResult)
	{
		if (current->predecessor != InvalidIndex)	//if invalidindex, means this is a cell neighboring the point of origin
			AssemblePath(mapper, mapper.result + current->predecessor,outResult);


		//LogEvent(EventSignificance::Debug,v.get(),"AI: directing to next waypoint");

		outResult << current->cell;
		//DirectTo(v, current->cell, false, Objects::CursorMode::Idle);
	}


	void IssuePath(const Objects::PVehicle&v, const Objects::Pathfinding::ReachabilityMapper&mapper, const Objects::Pathfinding::ReachabilityMapper::MapNode*current)
	{
		static Buffer0<Grid::LCellIdI> path;
		path.Clear();
		AssemblePath(mapper, current,path);
		ObjectHelper::DirectTo(v,path.ToRef(),ObjectHelper::AssembleMotionFlags(true,false,false),Objects::CursorMode::Idle);
	}


	void	SignalStart( const String&because, const TCodeLocation&caller )
	{
		if (ScriptedAI::IsLoaded())
			return;
		Detail::ofInterestToAttacker.Clear();
		Detail::awaitSensory = false;
		Detail::processed.Clear();
		Detail::pending.Clear();
		Detail::destHistory.Clear();
		Detail::pending	.AppendAll(Detail::attackers)
						.AppendAll(Detail::aa)
						.AppendAll(Detail::scanners);
		if (Detail::done)
			LogEvent(EventSignificance::Debug,"AI: Starting ("+because+", by "+String(caller)+")");

		Detail::done = false;

		Objects::MarkerStore::client.Foreach(Objects::Marker::Type::HostileContact,
			[](const Objects::PMarker&m0)
			{
				Objects::PEntityMarker m = To<Objects::EntityMarker>::Cast(m0);
				if (m->age >= 10 && m->visualization && m->visualization->IsMobile())	//after 10 rounds, assume vehicle has moved
				{
					LogEvent(EventSignificance::Debug,nullptr,"AI: Deleting marker "+m->GetName()+" since hostile vehicle markers are considered outdated after 10 rounds (age="+String(m->age)+")");
					ObjectHelper::SignalMarkerDeletion(m->id);
				}
			});

		Objects::MarkerStore::client.Foreach(Objects::MarkerType::EntityDeath,
			[](const Objects::PMarker&m0)
			{
				Objects::PEntityDeathMarker m = To<Objects::EntityDeathMarker>::Cast(m0);
				BeginBlockTest(m->visualization,false,false,false);
				if (Detail::ambientThreat.damageThreat > 0)	//knowing cause, dropping ghost
				{
					LogEvent(EventSignificance::Debug,nullptr,"AI: Deleting marker "+m->GetName()+" because cause of destruction is considered known");
					ObjectHelper::SignalMarkerDeletion(m->id);
				}
			});

		Objects::ForeachWorldEntity([](const Objects::PWorldEntity&e)
		{
			Detail::ProcessIgnorance(*e);
		},Objects::IncludeGhosts::HostileContacts);


	}

	bool	IsDone()
	{
		return Detail::done;
	}


	void	Process( float timeQuantum )
	{
		if (Detail::done)
			return;

		if (ScriptedAI::IsLoaded())
		{
			Detail::done = true;
			return;
		}

		if (Detail::awaitSensory && World::actual.PaintingSensorsConcurrently())
			return;

		LogEvent(EventSignificance::Debug,nullptr,"AI: Resuming processing ("+String(timeQuantum)+")");
		Detail::tempMarkers.Clear();
		Timer::Time started = timer.Now();
		while (Detail::pending.IsNotEmpty())
		{

			if (!Detail::ProcessVehicle(Detail::pending.Last()))
			{
				Detail::awaitSensory = true;
				return;	//try again later
			}
			Detail::processed.Append(Detail::pending.Last());
			Detail::pending.EraseLast();
			if (timer.GetSecondsSince(started) > timeQuantum)
				return;
		}
		
		
		

		if (Detail::pending.IsEmpty())
		{
			LogEvent(EventSignificance::Debug,nullptr,"AI: Vehicle processing completed");
			
			if (!ObjectHelper::MarkerCheck::DoneThisRound())
			{
				//we could be more strict, enforcing that markers be checked before processing units, but in practice the difference should be minimal
				const bool painting = World::actual.PaintingSensorsConcurrently();
				LogEvent(EventSignificance::Debug,nullptr,"AI: Markers processing NOT completed (painting="+String(painting)+")");
				return;
			}

			Detail::done = true;
			Objects::MarkerStore::client.Foreach(Objects::MarkerType::Visited,
				[](const Objects::PMarker&m)
				{
					if (m->age > 100)
					{
						LogEvent(EventSignificance::Debug,nullptr,"AI: Deleting marker "+m->GetName()+". Reached end-of-life after 100 rounds");

						ObjectHelper::SignalMarkerDeletion(m->id);
					}
				}
			);


			PlayerControlBegin::CheckAutoNextRound();
		}

	}


}


namespace ApplicationInterface
{
	RoundNumber	GetMinAIRoundForHeavyVehicleBases()
	{
		return AI::Detail::minRoundForHeavyVehicleBases;
	}

	RoundNumber	GetMinAIRoundForPlanes()
	{
		return AI::Detail::minRoundForPlanes;
	}
}



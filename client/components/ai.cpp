#include <global_root.h>
#include "ai.h"
#include <objects/interface.h>
#include "objectNetworkHelper.h"
#include "scriptedAI.h"
#include "playerControlBegin.h"
#include <objects/nexus/nexusThreatEstimation.h>


namespace AI
{
	Buffer0<Objects::PVehicle>	vehicles;
	Buffer0<M::TVec3<> >	tempMarkers;

	UINT32	minRoundForHeavyVehicleBases = 10,
			minRoundForPlanes = 10;

	void	SetMinRoundForHeavyVehicleBases(UINT32 v)
	{
		minRoundForHeavyVehicleBases = v;
	}

	void	SetMinRoundForPlanes(UINT32 v)
	{
		minRoundForPlanes = v;
	}

	static Random random;


	static float EstimateInterest(const Objects::PTurretBase&base, int baseAtHeight, float constructionCost, float distance2)
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


	struct TThreatEstimation;

	struct TThreatContext
	{
		bool	selfIsOffensiveVehicle = false,
				selfCanIgnoreThreat = false,
				selfIsOverconfident = false;
		Objects::PWorldEntity self;

		void	EstimateDeadThreatLevelFrom(const Objects::TrajectoryEndPoint&loc, Objects::WorldEntity&e, TThreatEstimation &current) const;
		void	EstimateLivingThreatLevelFrom(const Objects::TrajectoryEndPoint&loc, Objects::WorldEntity&e, TThreatEstimation &current) const;
		void	EstimateThreatFromAll(const Objects::TrajectoryEndPoint&loc, bool ignoreHostiles, TThreatEstimation&threat) const;
	};
	static TThreatContext		currentContext;


	struct TThreatEstimation
	{
		bool	beingSeen = false;
		int		damageThreat = 0;
		float	unknownDistanceThreat = 0.f;

	

		bool	ShouldBlock(const TThreatEstimation&base) const
		{
			const TThreatContext&ctx = currentContext;


			if (!beingSeen && ctx.selfIsOffensiveVehicle && unknownDistanceThreat < 0.2f)
				return false;

			if (damageThreat > base.damageThreat)
				return true;
			if (unknownDistanceThreat == base.unknownDistanceThreat)
				return false;

			return unknownDistanceThreat * 0.8f > base.unknownDistanceThreat;
		}
	};


	void TThreatContext::EstimateDeadThreatLevelFrom(const Objects::TrajectoryEndPoint&loc, Objects::WorldEntity&e, TThreatEstimation &current) const
	{
		if (e.IsAboveTerrain() != self->IsAboveTerrain())
			return;
		float distance = Vec::quadraticDistance(e.system.w.xyz,loc.worldLocation);

		current.unknownDistanceThreat += 1.f / (1.f + distance);
	}


	static bool IsSubOrbital(const Grid::LCellIdI&loc)
	{
		return loc.height == Objects::SubOrbital::Height;
	}

	static float3 ToSubOrbit(const Grid::LCellIdI&loc)
	{
		return Grid::ToWorld(Grid::LCellIdI(loc.stackID,Objects::SubOrbital::Height));
	}

	static float3 ToGround(const Grid::LCellIdI&loc, const Objects::TrajectoryEndPoint&ref)
	{
		return Grid::ToWorld(Grid::LCellIdI(loc.stackID,ref.cellLocation.height));
	}


	void TThreatContext::EstimateLivingThreatLevelFrom(const Objects::TrajectoryEndPoint&loc, Objects::WorldEntity&e, TThreatEstimation &current) const
	{
		const TThreatContext&ctx = *this;
		if (ctx.selfCanIgnoreThreat && e.ignoreThreatRounds)
			return;
		if (e.isOwnedByMe)
			return;
		//Objects::PTurretBase b = e.GetTurretBase();
		if (IsSubOrbital(loc.cellLocation))
		{
			if (e.IsAboveTerrain())
			{
				float distance = Vec::quadraticDistance(e.system.w.xyz,loc.worldLocation);
				if (distance < M::Sqr(e.GetScanningRange()))
					current.beingSeen = true;
			}
			else
			{
				float distance = Vec::quadraticDistance(ToSubOrbit(e.location),loc.worldLocation);
				auto turretBase = e.GetTurretBase();
				if (turretBase && distance < M::Sqr(turretBase->GetSubOrbitalScanningRange()))
					current.beingSeen = true;
			}
		}
		else
		{
			if (e.IsAboveTerrain())
			{
				float distance = Vec::quadraticDistance(ToGround(e.location,loc),loc.worldLocation);

				auto turretBase = e.GetTurretBase();
				if (turretBase && distance < M::Sqr(turretBase->turret->groundScanningRange.GetValue(e.GetResearchState())))
				{
					//extended test here later. see if scanner can actually penetrate ground
					current.beingSeen = true;
				}
			}
			else
			{
				float distance = Vec::quadraticDistance(e.system.w.xyz,loc.worldLocation);

				if (distance < M::Sqr(e.GetScanningRange()))
					current.beingSeen = true;
			}
		}

		
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
	}


	static TThreatEstimation	ambientThreat;

	void	BeginBlockTest( const Objects::PWorldEntity&v, bool initWithFriendlyDeathsOnly, bool canIgnoreThreat, bool overConfident )
	{
		auto base = v->GetTurretBase();
		currentContext.selfIsOffensiveVehicle = base && base->turret->CanAttack() && base->turret->independentAttackAndMotion;
		currentContext.selfCanIgnoreThreat = canIgnoreThreat;
		currentContext.selfIsOverconfident = overConfident;
		currentContext.self = v;
		currentContext.EstimateThreatFromAll(Objects::TrajectoryEndPoint(v->location),initWithFriendlyDeathsOnly, ambientThreat);	//determine base threat based on ghosts
	}

	void CostModifier( const Grid::LCellIdI&location, int&cost )
	{
		if (currentContext.self->IsPlane())
		{
			if (Objects::NexusThreat::IsPlaneFactoryHere(location.stackID))
			{
				cost = -1;
				return;
			}
		}

		TThreatEstimation threat;
		currentContext.EstimateThreatFromAll(Objects::TrajectoryEndPoint(location),false,threat);

		if (threat.ShouldBlock(ambientThreat))
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


	struct TAttackTarget
	{
		Grid::LCellIdI target;
		M::TVec3<>		explosionAt;
		float		priority;
		bool		midflightExplosion = false;

		bool		operator<(const TAttackTarget&other) const
		{
			return priority > other.priority;	//higher first
		}
	};


	static float ProcessAttackPriority(Objects::WorldEntity&e,const M::TVec3<>&explosionCenter,float maxExplosionRadius)
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

	static void ProcessAttackTarget(const Objects::Vehicle&source, const Objects::WorldEntity&candidate,  BasicBuffer<TAttackTarget>&attackTarget, bool attacksAirborne, bool attacksGroundEntities)
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
					t.explosionAt = rs.impactAt;
					t.priority = priority / (1.f + float(i)*0.1f);
					t.target = choice.cellTarget;
					t.midflightExplosion = (rs == Objects::TraceResult::MidFlightImpact);
				}
			}
		}	
	
	}

	static float QuadraticInterestDistance(const Grid::LCellIdI&self, const Objects::PWorldEntity&other, bool interestedInAirborne, bool interestedInGround)
	{

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
	}

	static void ProcessVehicle( const Objects::PVehicle&v )
	{
		Objects::Component::Turret*t = v->turret;
		if (t->detectsResources || t->CanDo(Objects::Component::Operation::Construct))
		{
			LogEvent(EventSignificance::Uncommon,v.get(),  "AI: Skipping unsupported vehicle type");
			return;
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
			static Buffer0<TAttackTarget>	attackTarget;
			attackTarget.Clear();

			Objects::ForeachWorldEntity([v,attacksAirborne, attacksGroundEntities](const Objects::PWorldEntity&e)
			{
				ProcessAttackTarget(*v,*e,attackTarget,attacksAirborne, attacksGroundEntities);
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
				return;
		}

		bool overconfident = false;
		{
			count_t proximityPresence = 0,
					radarProximity = 0,
					aaProximity = 0;
			Objects::ForeachMobileWorldEntity([v,&proximityPresence,&radarProximity,&aaProximity](const Objects::PMobileEntity&e)
			{
				if (e->isOwnedByMe && Vec::quadraticDistance(v->GetWorldPosition(),e->GetWorldPosition()) < 25.f)
				{
					if (e->turret->trajectoryType == Objects::TrajectoryType::AAMissile)
						aaProximity++;
					elif (e->turret->CanDealDamage())
						proximityPresence++;
					else
						radarProximity++;
				}
			});
			
			overconfident = proximityPresence > 5 && !isAA;
			if (!attacker && radarProximity > 1 && random.NextFloat() < 0.3f)
			{
				ApplicationInterface::SignalRemoval(v);
				return;
			}

			if (isAA && aaProximity > 1 && random.NextFloat() < 0.3f)
			{
				ApplicationInterface::SignalRemoval(v);
				return;
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
				Objects::ForeachWorldEntity([explosionRadius,&goodWeight,&badWeight,isAA,interestedInAirborne,interestedInGroundTargets,n](const Objects::PWorldEntity&e)
				{
					if (e->isOwnedByMe IF_SATELLITES( || e->IsSatellite() ))
					{
						if (isAA)
						{
							float distance = QuadraticInterestDistance(n->cell,e,interestedInAirborne,interestedInGroundTargets);
							const auto base = e->GetTurretBase();
							if (base && base->turret->trajectoryType == Objects::TrajectoryType::AAMissile)
								badWeight += 20 / (1.f + distance);
							else
								goodWeight += 5 / (1.f + distance);

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

					float distance = QuadraticInterestDistance(n->cell,e,interestedInAirborne,interestedInGroundTargets);

					float fc = EstimateInterest(e->GetTurretBase(),e->location.height,
								e->GetConstructionCost(),distance);
					if (distance < explosionRadius)
						badWeight += 20 / (1.f + distance);
					else
						goodWeight += fc / (1.f + distance);
				},Objects::IncludeGhosts::HostileContacts);
				//if (badWeight < 0.1f && goodWeight < 0.05f)

				if (!isAA)
				{
					const M::TVec3<> reference = Grid::ToWorld(n->cell);
					Objects::MarkerStore::client.Foreach(Objects::MarkerType::Visited,
						[reference, minMotion, n, &badWeight](const Objects::PMarker&m)
						{
							float distance = Vec::quadraticDistance(Grid::ToWorld(m->location),reference);
							if (distance < 49.f || (n->movementCostsFromRoot >= minMotion && distance < 16.f))
								badWeight += 10.f / (1.f + float(m->age));
						}
					);
					foreach (tempMarkers,_m)
					{
						float distance = Vec::quadraticDistance(*_m,reference);
						if (distance < 49.f || (n->movementCostsFromRoot >= minMotion && distance < 16.f))
							badWeight += 10.f;
					}

					if (!haveMarker)
					{
						badWeight += 1.f;
					}
				}
			
				float score = (1.f + goodWeight) / (1.f+badWeight) / (1.f + float(abs(n->movementCostsFromRoot- minMotion * 3 / 2))*0.01f);
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
					IssuePath(v,mapper,best);
			}
		}
	}

	static bool done = true;


	static void	ProcessIgnorance(Objects::WorldEntity&e)
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

	void	SignalStart( const String&because, const TCodeLocation&caller )
	{
		if (ScriptedAI::IsLoaded())
			return;
		if (done)
			LogEvent(EventSignificance::Debug,"AI: Starting ("+because+", by "+String(caller)+")");

		done = false;

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
				if (ambientThreat.damageThreat > 0)	//knowing cause, dropping ghost
				{
					LogEvent(EventSignificance::Debug,nullptr,"AI: Deleting marker "+m->GetName()+" because cause of destruction is considered known");
					ObjectHelper::SignalMarkerDeletion(m->id);
				}
			});

		Objects::ForeachWorldEntity([](const Objects::PWorldEntity&e)
		{
			ProcessIgnorance(*e);
		},Objects::IncludeGhosts::HostileContacts);


	}

	bool	IsDone()
	{
		return done;
	}


	void	Process( float timeQuantum )
	{
		if (done)
			return;

		if (ScriptedAI::IsLoaded())
		{
			done = true;
			return;
		}

		LogEvent(EventSignificance::Debug,nullptr,"AI: Resuming processing ("+String(timeQuantum)+")");
		tempMarkers.Clear();
		Timer::Time started = timer.Now();
		for (index_t i = vehicles.Count() - 1; i < vehicles.Count(); i--)
		{
			ProcessVehicle(vehicles[i]);
			vehicles.EraseLast();
			if (timer.GetSecondsSince(started) > timeQuantum)
				return;
		}
		
		
		

		if (vehicles.IsEmpty())
		{
			LogEvent(EventSignificance::Debug,nullptr,"AI: Vehicle processing completed");
			
			if (!ObjectHelper::MarkerCheck::DoneThisRound())
			{
				//we could be more strict, enforcing that markers be checked before processing units, but in practice the difference should be minimal
				const bool painting = World::actual.PaintingSensorsConcurrently();
				LogEvent(EventSignificance::Debug,nullptr,"AI: Markers processing NOT completed (painting="+String(painting)+")");
				return;
			}

			done = true;
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
		return AI::minRoundForHeavyVehicleBases;
	}

	RoundNumber	GetMinAIRoundForPlanes()
	{
		return AI::minRoundForPlanes;
	}
}



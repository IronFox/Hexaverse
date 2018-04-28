#include <global_root.h>
#include "ai.h"
#include <objects/interface.h>
#include "objectNetworkHelper.h"


namespace AI
{
	Buffer0<Objects::PVehicle>	vehicles;
	Buffer0<M::TVec3<> >	tempMarkers;


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


	static bool	currentIsOffensiveVehicle = false;


	struct TThreatEstimation
	{
		bool	beingSeen = false;
		int		damageThreat = 0;
		float	unknownDistanceThreat = 0.f;
	

		bool	ShouldBlock(const TThreatEstimation&base) const
		{
			if (!beingSeen && currentIsOffensiveVehicle && unknownDistanceThreat < 0.2f)
				return false;

			if (damageThreat > base.damageThreat)
				return true;
			if (unknownDistanceThreat == base.unknownDistanceThreat)
				return false;

			return unknownDistanceThreat * 0.8f > base.unknownDistanceThreat;
		}
	};


	static void EstimateDeadThreatLevelFrom(Objects::WorldEntity&e, const M::TVec3<>&reference, TThreatEstimation &current)
	{
		float distance = Vec::quadraticDistance(e.system.w.xyz,reference);

		current.unknownDistanceThreat += 1.f / (1.f + distance);
	}


	static void EstimateLivingThreatLevelFrom(Objects::WorldEntity&e, const M::TVec3<>&reference, bool canIgnoreThreat, TThreatEstimation &current)
	{
		if (canIgnoreThreat && e.ignoreThreatRounds)
			return;
		//Objects::PTurretBase b = e.GetTurretBase();
		float distance = Vec::quadraticDistance(e.system.w.xyz,reference);
		if (distance < M::Sqr(e.GetScanningRange()))
			current.beingSeen = true;

		
		const auto&state = e.GetResearchState();

		if (e.CanHit(reference))
		{
			Objects::PTurretBase b = e.GetTurretBase();
			current.damageThreat += b->turret->GetDamageLevel(state) * b->GetAttacksPerRound();
		}
	}


	static void EstimateThreatFromAll(const M::TVec3<>&reference, bool ignoreHostiles, bool canIgnoreThreat, TThreatEstimation&threat)
	{
		threat = TThreatEstimation();
		if (!ignoreHostiles)
			Objects::ForeachWorldEntity([reference,canIgnoreThreat,&threat](const Objects::PWorldEntity&e)
			{
				EstimateLivingThreatLevelFrom(*e,reference,canIgnoreThreat,threat);
			},Objects::IncludeGhosts::Living);

		Objects::Marker::deadMobileGhosts.Foreach([reference,&threat](const Objects::PWorldEntity&e)
		{
			EstimateDeadThreatLevelFrom(*e,reference,threat);
		});
		Objects::Marker::deadStructureGhosts.Foreach([reference,&threat](const Objects::PWorldEntity&e)
		{
			EstimateDeadThreatLevelFrom(*e,reference,threat);
		});
	}


	static TThreatEstimation	currentThreat;
	static bool					currentCanIgnoreThreat,currentIsOverconfident=false;


	void	BeginBlockTest( const Objects::PVehicle&v, bool zeroThreatOnly, bool canIgnoreThreat, bool overConfident )
	{
		currentIsOffensiveVehicle = v->turret->CanAttack() && v->turret->independentAttackAndMotion;
		currentCanIgnoreThreat = canIgnoreThreat;
		currentIsOverconfident = overConfident;
		EstimateThreatFromAll(v->system.w.xyz,zeroThreatOnly, currentCanIgnoreThreat,currentThreat);	//determine base threat based on ghosts
	}

	bool BlockTest( const Grid::LCellIdI&location )
	{
		if (!(Grid::low.cells[location.stackID].cornerFlags & Grid::empZonesUnlocked))
			return true;

		const M::TVec3<> reference = Grid::ToWorld(location);
		TThreatEstimation threat;
		EstimateThreatFromAll(reference,currentIsOverconfident,  currentCanIgnoreThreat,threat);

		return threat.ShouldBlock(currentThreat);
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

	static void ProcessAttackTarget(const Objects::Vehicle&source, const Objects::WorldEntity&candidate,  BasicBuffer<TAttackTarget>&attackTarget)
	{
		if (candidate.isOwnedByMe || candidate.IsIndestructible())
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
				count_t needAttacks = ceil(float(std::max(1,candidate.health)) / source.turret->GetDamageLevel(state));
				if (rs != Objects::TraceResult::ReachesTarget)
					needAttacks ++;
				//if (rs == Objects::Turret::ReachesTarget)
				float priority = 0.f;
				Objects::ForeachWorldEntity([&priority,explosionRadius,rs](const Objects::PWorldEntity&e)
				{
					priority += ProcessAttackPriority(*e,rs.impactAt,explosionRadius);
				},Objects::IncludeGhosts::Living);
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

	static void ProcessVehicle(const Objects::PVehicle&v)
	{
		Objects::Component::Turret*t = v->turret;
		if (t->detectsResources || t->CanDo(Objects::Component::Construct))
		{
			LogEvent(EventSignificance::Uncommon,v.get(),  "AI: Skipping unsupported vehicle type");
			return;
		}

		int referenceSpeed = v->GetMovementStepsPerRound()*3;
		int minMotion = v->GetMovementStepsPerRound();

		//attacker logic
		Objects::Pathfinding::ReachabilityMapper::MapNode*best = nullptr;
		float bestScore = 0.f;
		const bool attacker = t->CanAttack();
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
		foreach (Objects::Marker::all,_m)
		{
			if (_m->type == Objects::Marker::Type::Visited)
			{
				float distance = Vec::quadraticDistance(Grid::ToWorld(_m->location),v->system.w.xyz);
				if (distance < 25.f)
				{
					ObjectHelper::SignalMarkerTouch(*_m);
					haveMarker = true;
//					break;
				}
			}
		}
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
			Protocol::Marker marker;
			marker.id.Generate();
			marker.location = v->location;
			tempMarkers << v->system.w.xyz;
			static_assert(Objects::Marker::Type::Visited == 2, "Please update marker logic");
			marker.data.Set2();
			ObjectHelper::SignalMarkerCreation(marker);
		}

		const float explosionRadius = v->GetAttackExplosionRadius();

		if (attacker)
		{
			String id = ToString(v->id);
			static Buffer0<TAttackTarget>	attackTarget;
			attackTarget.Clear();

			Objects::ForeachWorldEntity([v](const Objects::PWorldEntity&e)
			{
				ProcessAttackTarget(*v,*e,attackTarget);
			},Objects::IncludeGhosts::Living);

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
					radarProximity = 0;
			Objects::ForeachMobileWorldEntity([v,&proximityPresence,&radarProximity](const Objects::PMobileEntity&e)
			{
				if (e->isOwnedByMe && Vec::quadraticDistance(v->GetWorldPosition(),e->GetWorldPosition()) < 25.f)
				{
					if (e->turret->CanDealDamage())
						proximityPresence++;
					else
						radarProximity++;
				}
			}
			,Objects::IncludeGhosts::None);
			
			overconfident = proximityPresence > 5;
			if (!attacker && radarProximity > 1 && random.NextFloat() < 0.3f)
			{
				ApplicationInterface::SignalRemoval(v);
				return;
			}
		}



		
		static Objects::Pathfinding::ReachabilityMapper mapper(false);

		const bool canIgnoreThreat = (attacker || random.NextFloat() < 0.5f) && !t->dealsEMPDamage;
		BeginBlockTest(v,true,canIgnoreThreat,overconfident);	//force current to have no threat level, limiting motion to areas with zero threat

		Objects::Component::VehicleBase::canStandOnUnknown = false;

		mapper.Map(v.get(), v->location, BlockTest, referenceSpeed,true);
		if (mapper.result.IsEmpty())
		{
			//no chance to get out of danger, so try to reduce threat level
			BeginBlockTest(v,false,canIgnoreThreat,overconfident);
			mapper.Map(v.get(), v->location, BlockTest, referenceSpeed,true);
			if (mapper.result.IsEmpty())
				mapper.Map(v.get(), v->location, BlockTest, referenceSpeed,false);
		}

		Objects::Component::VehicleBase::canStandOnUnknown = true;

		{
			foreach (mapper.result,n)
			{
				const M::TVec3<> reference = Grid::ToWorld(n->cell);
				float goodWeight = 0;
				float badWeight = 0;
				Objects::ForeachWorldEntity([explosionRadius,&goodWeight,&badWeight,reference](const Objects::PWorldEntity&e)
				{
					if (e->isOwnedByMe IF_SATELLITES( || e->IsSatellite() ))
					{
						//if (distance < 16.f)
						//	badWeight += 100.f / (1.f + distance);	//repell close vehicles a bit
						return;
					}
					float distance = Vec::quadraticDistance(e->system.w.xyz,reference);
					float fc = EstimateInterest(e->GetTurretBase(),e->location.height,
								e->GetConstructionCost(),distance);
					if (distance < explosionRadius)
						badWeight += 20 / (1.f + distance);
					else
						goodWeight += fc / (1.f + distance);
				},Objects::IncludeGhosts::Living);
				//if (badWeight < 0.1f && goodWeight < 0.05f)
				{
					foreach (Objects::Marker::all,_m)
					{
						if (_m->type == Objects::Marker::Type::Visited)
						{
							float distance = Vec::quadraticDistance(Grid::ToWorld(_m->location),reference);
							if (distance < 49.f || (n->movementCostsFromRoot >= minMotion && distance < 16.f))
								badWeight += 10.f / (1.f + float(_m->age));
								//0.1f / (1.f + distance);
						}
					}
					foreach (tempMarkers,_m)
					{
						float distance = Vec::quadraticDistance(*_m,reference);
						if (distance < 49.f || (n->movementCostsFromRoot >= minMotion && distance < 16.f))
							badWeight += 10.f;
					}

				}
				if (!haveMarker)
				{
					badWeight += 1.f;
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
			}
			if (best)
			{
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
			Objects::ForeachMobileWorldEntity([ref,r,&damagePotentialPerRound,&participants](const Objects::PMobileEntity&e)
			{
				if (e->isOwnedByMe && e->turret->CanAttack() && !e->turret->dealsEMPDamage)
				{
					float d2 = Vec::quadraticDistance(e->GetWorldPosition(),ref);
					if (d2 < M::Sqr(r*1.2f))	//might be okay anyway
					{
						float p = e->GetDamagePotentialPerRound();
						if (!e->turret->independentAttackAndMotion)
							p *= 0.5f;
						damagePotentialPerRound += p;
						participants++;
					}
				}
			},Objects::IncludeGhosts::None);

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

	void	SignalStart(const String&because, const TCodeLocation&caller)
	{
		if (done)
			LogEvent(EventSignificance::Debug,"AI: Starting ("+because+", by "+String(caller)+")");

		done = false;

		for (index_t i = 0; i < Objects::Marker::all.Count(); i++)
		{
			const Objects::Marker&m = Objects::Marker::all[i];
			switch (m.type)
			{
				case Objects::Marker::Type::HostileContact:
					if (m.age >= 10 && m.visualization && m.visualization->IsMobile())	//after 10 rounds, assume vehicle has moved
					{
						LogEvent(EventSignificance::Debug,nullptr,"AI: Deleting marker "+m.GetName()+" since hostile vehicle markers are considered outdated after 10 rounds (age="+String(m.age)+")");
						ObjectHelper::SignalMarkerDeletion(m.id);
						i--;
					}
				break;
				case Objects::Marker::Type::EntityDeath:
				{
					TThreatEstimation estimation;
					EstimateThreatFromAll(Grid::ToWorld(m.location),false,false,estimation);
					if (estimation.damageThreat > 0)	//knowing cause, dropping ghost
					{
						LogEvent(EventSignificance::Debug,nullptr,"AI: Deleting marker "+m.GetName()+" because cause of destruction is considered known");
						ObjectHelper::SignalMarkerDeletion(m.id);
						i--;
					}
				}
				break;
			}
		}


		Objects::ForeachWorldEntity([](const Objects::PWorldEntity&e)
		{
			ProcessIgnorance(*e);
		},Objects::IncludeGhosts::Living);


	}

	void	Process(float timeQuantum)
	{
		if (done)
			return;
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
			for (index_t i = 0; i < Objects::Marker::all.Count(); i++)
			{
				const Objects::Marker&m = Objects::Marker::all[i];
				if (m.type == Objects::Marker::Type::Visited && m.age > 100)
				{
					LogEvent(EventSignificance::Debug,nullptr,"AI: Deleting marker "+m.GetName()+". Reached end-of-life after 100 rounds");

					ObjectHelper::SignalMarkerDeletion(m.id);
					i--;
				}
			}

			ASSERT__(done);
			LogEvent(EventSignificance::Debug,nullptr,"AI: all done. requesting new round");
			ApplicationInterface::SignalRoundEnd();
			
		}

	}


}

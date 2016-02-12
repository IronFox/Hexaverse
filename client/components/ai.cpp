#include <global_root.h>
#include "ai.h"

void	SignalDropAllTasks(const Objects::PWorldEntity&v);
void	DirectTo(const Objects::PWorldEntity&v, const ArrayRef<Grid::LCellIdI>&targets, bool detailedPath, bool leftClick, Objects::Vehicle::mode_t mode);
void	SignalRemoval(const Objects::PWorldEntity&entity);
void	SignalMarkerCreation(Protocol::Marker&marker);
void	SignalMarkerDeletion(TUniqueID markerID);
void	SignalMarkerTouch(Objects::Marker&marker);

void	SignalRoundEnd();

namespace AI
{
	Buffer0<Objects::PVehicle>	vehicles;
	Buffer0<TVec3<> >	tempMarkers;


	static Random random;


	static float EstimateInterest(const Objects::PTurretBase&base, int baseAtHeight, float constructionCost, float distance2)
	{
		float rs = constructionCost;
		
		if (base && base->turret->CanAttack())
		{
			rs /= (1.f + float(base->turret->damageLevel * base->GetAttacksPerRound() * base->GetFlatAttackRange()) * 0.1f);
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
	

		float	ActualDamageThreat() const
		{
			if (damageThreat == 0 && unknownDistanceThreat < 0.2f)
				return 0;
			if (!beingSeen && unknownDistanceThreat < 0.2f && currentIsOffensiveVehicle)
				return 0;
			return 20.f * unknownDistanceThreat +  damageThreat;
		}


		bool	operator>(const TThreatEstimation&base) const
		{
			return ActualDamageThreat() > base.ActualDamageThreat();	
		}
	};



	static void EstimateThreatLevelFrom(Objects::WorldEntity&e, const TVec3<>&reference, bool ignoreHostiles, bool canIgnoreThreat, TThreatEstimation &current)
	{
		if (e.isMine)
		{
			if (e.health <= 0)	//ghost
			{
				float distance = Vec::quadraticDistance(e.system.w.xyz,reference);

				current.unknownDistanceThreat += 1.f / (1.f + distance);
			}
			return;
		}
		if (ignoreHostiles || (canIgnoreThreat && e.ignoreThreatRounds))
			return;
		//Objects::PTurretBase b = e.GetTurretBase();
		float distance = Vec::quadraticDistance(e.system.w.xyz,reference);
		if (distance < sqr(e.GetScanningRange()))
			current.beingSeen = true;

		//if (!b)
			//return;

		//if (b->CanHit(e.owner,e.location,))
		if (e.CanHit(reference))
		//if (distance < sqr(b->GetAttackRange() + b->turret->explosionRadius - 1))
		{
			Objects::PTurretBase b = e.GetTurretBase();
			current.damageThreat += b->turret->damageLevel * b->GetAttacksPerRound();
		}
	}


	static void EstimateThreatFromAll(const TVec3<>&reference, bool ignoreHostiles, bool canIgnoreThreat, TThreatEstimation&threat)
	{
		threat = TThreatEstimation();
		foreach (Objects::vehicles,_v)
		{
			EstimateThreatLevelFrom(**_v,reference,ignoreHostiles,canIgnoreThreat,threat);
		}
		foreach (Objects::structures,_v)
		{
			EstimateThreatLevelFrom(**_v,reference,ignoreHostiles,canIgnoreThreat,threat);
		}

		foreach (Objects::Marker::mobileGhosts,_v)
		{
			EstimateThreatLevelFrom(**_v,reference,ignoreHostiles,canIgnoreThreat,threat);
		}
		foreach (Objects::Marker::structureGhosts,_v)
		{
			EstimateThreatLevelFrom(**_v,reference,ignoreHostiles,canIgnoreThreat,threat);
		}

	}


	static TThreatEstimation	currentThreat;
	static bool					currentCanIgnoreThreat,currentIsOverconfident=false;
	bool BlockTest(const Grid::LCellIdI&location)
	{
		if (!(Grid::low.cells[location.stackID].cornerFlags & Grid::empZonesUnlocked))
			return true;

		const TVec3<> reference = Grid::ToWorld(location);
		TThreatEstimation threat;
		EstimateThreatFromAll(reference,currentIsOverconfident,  currentCanIgnoreThreat,threat);

		return threat > currentThreat;
	}


	void AssemblePath(const Objects::ReachabilityMapper&mapper, const Objects::ReachabilityMapper::MapNode*current, BasicBuffer<Grid::LCellIdI>&outResult)
	{
		if (current->predecessor != InvalidIndex)	//if invalidindex, means this is a cell neighboring the point of origin
			AssemblePath(mapper, mapper.result + current->predecessor,outResult);


		//LogEvent(EventSignificance::Debug,v.get(),"AI: directing to next waypoint");

		outResult << current->cell;
		//DirectTo(v, current->cell, false, Objects::CursorMode::Idle);
	}


	void IssuePath(const Objects::PVehicle&v, const Objects::ReachabilityMapper&mapper, const Objects::ReachabilityMapper::MapNode*current)
	{
		static Buffer0<Grid::LCellIdI> path;
		path.Clear();
		AssemblePath(mapper, current,path);
		DirectTo(v,path.ToRef(),true,false,Objects::CursorMode::Idle);
	}


	struct TAttackTarget
	{
		Grid::LCellIdI target;
		TVec3<>		explosionAt;
		float		priority;
		bool		midflightExplosion = false;

		bool		operator<(const TAttackTarget&other) const
		{
			return priority > other.priority;	//higher first
		}
	};


	static float ProcessAttackPriority(Objects::WorldEntity&e,const TVec3<>&explosionCenter,float explosionRadius)
	{
		if (e.isMine || Vec::quadraticDistance(explosionCenter,e.system.w.xyz) > sqr(explosionRadius))
			return 0.f;
		Objects::PTurretBase b = e.GetTurretBase();
		float rs = e.GetMaxScanningRange();
		if (b)
			rs = std::max(rs, b->GetFlatAttackRange() + b->turret->explosionRadius);
		int health = e.health >0 ? e.health : e.GetMaxHealth();
		return rs / health;
	}

	static void ProcessAttackTarget(const Objects::Vehicle&source, const Objects::WorldEntity&candidate,  BasicBuffer<TAttackTarget>&attackTarget)
	{
		if (candidate.isMine || candidate.IsIndestructible())
			return;
		//float distance = Vec::quadraticDistance(source.system.w.xyz,candidate.system.w.xyz);
		//const float range = source.GetAttackRange();
		Grid::LCellIdI target;
		if (source.ExtCanHit(source.owner, source.location,source.base->height,candidate.location,target))
		{


			TVec3<> detonationAt;
			Objects::TurretTrajectory rs = source.TraceTrajectory(Grid::actualWorld,target,detonationAt);
			LogEvent(EventSignificance::Debug,&source,"Checking possible attack trajectory to "+candidate.GetName()+", attacking "+target.ToString()+", detonating at "+Vec::toString(detonationAt)+": "+rs.ToString());

			if (rs == Objects::TurretTrajectory::InvalidTrajectory)
			{
				LogEvent(EventSignificance::Debug,&source,"Cannot trace good trajectory to destination. Invalid trajectory");
				return;
			}
			if (rs != Objects::TurretTrajectory::ReachesTarget)
			{
				float distanceToTarget = Vec::distance(detonationAt,candidate.system.w.xyz);
				if (distanceToTarget > source.turret->explosionRadius)
				{
					LogEvent(EventSignificance::Debug,&source,"Cannot trace good trajectory to destination. Final distance is "+String(distanceToTarget)+"/"+String(source.turret->explosionRadius));
					return;
				}
			}
			//Objects::Turret::TraceTrajectory(
			//Objects::Turret::MaxRangeToVelocity(source.GetFlatAttackRange()),
				//									Grid::actualWorld, source.location,target,detonationAt);
			//if (rs == Objects::TurretTrajectory::ReachesTarget
			// || (rs == Objects::TurretTrajectory::MidFlightImpact && source.turret->explosionRadius > 1 && !source.turret->dealsEMPDamage))
			{
				LogEvent(EventSignificance::Debug,&source,"Trajectory checks out");
				count_t needAttacks = ceil(float(std::max(1,candidate.health)) / source.turret->damageLevel);
				if (rs != Objects::TurretTrajectory::ReachesTarget)
					needAttacks ++;
				//if (rs == Objects::Turret::ReachesTarget)
				float priority = 0.f;
				foreach (Objects::vehicles,_v)
					priority += ProcessAttackPriority(**_v,detonationAt,source.turret->explosionRadius);
				foreach (Objects::structures,_v)
					priority += ProcessAttackPriority(**_v,detonationAt,source.turret->explosionRadius);
				foreach (Objects::Marker::structureGhosts,_v)
					priority += ProcessAttackPriority(**_v,detonationAt,source.turret->explosionRadius);
				foreach (Objects::Marker::mobileGhosts,_v)
					priority += ProcessAttackPriority(**_v,detonationAt,source.turret->explosionRadius);
				LogEvent(EventSignificance::Debug,&source,"Base Priority considered "+String(priority));
				for (index_t i = 0; i < needAttacks; i++)
				{
					TAttackTarget&t = attackTarget.Append();
					t.explosionAt = detonationAt;
					t.priority = priority / (1.f + float(i)*0.1f);
					t.target = target;
					t.midflightExplosion = rs == Objects::TurretTrajectory::MidFlightImpact;
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
		Objects::ReachabilityMapper::MapNode*best = nullptr;
		float bestScore = 0.f;
		bool attacker = t->CanAttack();
//		const float myRange = t->maxRange + t->explosionRadius - 1;



		#ifndef CONNECTOR_LESS
			if (attacker && !v->HasAmmoForShot())
			{
				LogEvent(EventSignificance::Debug,v.get(),"AI: Out of ammo: destroying vehicle");
				SignalRemoval(v);
				return;
			}
		#endif
		//if (v->tasks.IsNotEmpty())
		{
			//LogEvent(EventSignificance::Debug,v.get(),"AI: dropping tasks");
			SignalDropAllTasks(v);
		}



		bool haveMarker = false;
		foreach (Objects::Marker::all,_m)
		{
			if (_m->type == Objects::Marker::Type::Visited)
			{
				float distance = Vec::quadraticDistance(Grid::ToWorld(_m->location),v->system.w.xyz);
				if (distance < 25.f)
				{
					SignalMarkerTouch(*_m);
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
			marker.id.value.Generate();
			marker.location = v->location;
			tempMarkers << v->system.w.xyz;
			static_assert(Objects::Marker::Type::Visited == 2, "Please update marker logic");
			marker.data.Set2();
			SignalMarkerCreation(marker);
		}


		if (attacker)
		{
			String id = v->id.ToString();
			static Buffer0<TAttackTarget>	attackTarget;
			attackTarget.Clear();

			foreach (Objects::vehicles,_v)
			{
				ProcessAttackTarget(*v,**_v,attackTarget);
			}
			foreach (Objects::structures,_v)
			{
				ProcessAttackTarget(*v,**_v,attackTarget);
			}
			foreach (Objects::Marker::structureGhosts,_v)
			{
				ProcessAttackTarget(*v,**_v,attackTarget);
			}
			for(index_t at = 0; at < attackTarget.Count(); at++)
			{
				if (!attackTarget[at].midflightExplosion)
					continue;
				const TVec3<>&ref = attackTarget[at].explosionAt;
				bool conflict = false;
				foreach (Objects::vehicles,_v)
				{
					if ((*_v)->isMine)
					{
						float distance = Vec::quadraticDistance((*_v)->system.w.xyz,ref);
						if ( distance < sqr(v->turret->explosionRadius))
						{
							conflict = true;
							break;
						}
					}
				}
				foreach (Objects::structures,_v)
				{
					if ((*_v)->isMine)
					{
						float distance = Vec::quadraticDistance((*_v)->system.w.xyz,ref);
						if ( distance < sqr(v->turret->explosionRadius))
						{
							conflict = true;
							break;
						}
					}
				}
				if (conflict)
				{
					attackTarget.Erase(at);
					at--;
				}
			}
			std::sort(attackTarget.begin(),attackTarget.end());
			attackTarget.Truncate(v->turret->attacksPerRound);
			static Array<Grid::LCellIdI>	targets;
			targets.SetSize(attackTarget.Count());
			foreach (attackTarget,t)
			{
				targets[t-attackTarget.pointer()] = t->target;
			}
			DirectTo(v,targets,false,true, Objects::CursorMode::AttackMode);

			if (attackTarget.IsNotEmpty() && !v->base->ignoresVehicleAttacks)
				return;
		}

		currentIsOverconfident = false;
		{
			count_t proximityPresence = 0,
					radarProximity = 0;
			foreach (Objects::vehicles,_v2)
				if ((*_v2)->isMine && Vec::quadraticDistance(v->system.w.xyz,(*_v2)->system.w.xyz) < 25.f)
				{
					if ((*_v2)->turret->CanAttack())
						proximityPresence++;
					else
						radarProximity++;
				}
			
			currentIsOverconfident = proximityPresence > 5;
			if (!v->turret->CanAttack() && radarProximity > 1 && random.getFloat() < 0.3f)
			{
				SignalRemoval(v);
				return;
			}
		}



		currentIsOffensiveVehicle = attacker && v->turret->ignoresVehicleMotion;
		static Objects::ReachabilityMapper mapper(false);
//		currentThreat = TThreatEstimation();
		currentCanIgnoreThreat = (v->turret->CanAttack() || random.getFloat() < 0.5f) && !v->turret->dealsEMPDamage;
		//currentIsOverconfident = v->overconfident;
		EstimateThreatFromAll(v->system.w.xyz,true, currentCanIgnoreThreat,currentThreat);	//determine base threat based on ghosts

		Objects::Component::VehicleBase::canStandOnUnknown = false;

		mapper.Map(v.get(), v->location, BlockTest, referenceSpeed);
		if (mapper.result.IsEmpty())
		{
			//no chance to get out of danger
			EstimateThreatFromAll(v->system.w.xyz, false,currentCanIgnoreThreat,currentThreat);
			mapper.Map(v.get(), v->location, BlockTest, referenceSpeed);
		}

		Objects::Component::VehicleBase::canStandOnUnknown = true;

		{
			foreach (mapper.result,n)
			{
				const TVec3<> reference = Grid::ToWorld(n->cell);
				float goodWeight = 0;
				float badWeight = 0;
				Objects::ForeachWorldEntity([&goodWeight,&badWeight,reference](const Objects::PWorldEntity&e)
				{
					if (e->isMine || e->IsSatellite())
					{
						//if (distance < 16.f)
						//	badWeight += 100.f / (1.f + distance);	//repell close vehicles a bit
						return;
					}
					float distance = Vec::quadraticDistance(e->system.w.xyz,reference);
					float fc = EstimateInterest(e->GetTurretBase(),e->location.height,
								e->GetConstructionCost(),distance);
					if (distance < Objects::Component::artillery.explosionRadius)
						badWeight += 20 / (1.f + distance);
					else
						goodWeight += fc / (1.f + distance);
				});
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
					float extend = 1.f + sqr(sqr(sqr(random.getFloat())));
					if (random.getIndex(1) == 0)
						extend = 1.f / extend;
					score *= extend;				
				}
				//score *= random.getFloat(0.8f,1.25f);
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
		if (e.isMine)
			return;
		e.ignoreThreatRounds --;
		if (e.ignoreThreatRounds < 0)
			e.ignoreThreatRounds = 0;
		Objects::PTurretBase b = e.GetTurretBase();
		const TVec3<>&ref = e.system.w.xyz;
		if (b && b->turret->CanAttack())
		{
			float r = b->GetFlatAttackRange();
			float damagePotentialPerRound = 0.f;
			count_t participants = 0;
			foreach (Objects::vehicles,_v)
			{
				Objects::PVehicle v = *_v;
				if (v->isMine && v->turret->CanAttack() && !v->turret->dealsEMPDamage)
				{
					float d2 = Vec::quadraticDistance(v->system.w.xyz,ref);
					if (d2 < sqr(r*1.2f))	//might be okay anyway
					{
						float p = v->GetDamagePotentialPerRound();
						if (!v->turret->ignoresVehicleMotion)
							p *= 0.5f;
						damagePotentialPerRound += p;
						participants++;
					}
				}
			}

			if (participants > 1)
			{
				float potentialLeft = damagePotentialPerRound * (participants -1) / float(participants);
				int health = e.health;
				if (health <= 0)
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
			LogEvent(EventSignificance::Debug,"AI: Starting ("+because+", by "+caller.ToString()+")");

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
						SignalMarkerDeletion(m.id);
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
						SignalMarkerDeletion(m.id);
						i--;
					}
				}
				break;
			}
		}


		foreach (Objects::structures,_s)
		{
			ProcessIgnorance(**_s);
		}
		foreach (Objects::vehicles,_v)
		{
			ProcessIgnorance(**_v);

		}
		foreach (Objects::Marker::structureGhosts,_s)
		{
			ProcessIgnorance(**_s);
		}
		foreach (Objects::Marker::mobileGhosts,_v)
		{
			ProcessIgnorance(**_v);
		}


	}

	void	Process(float timeQuantum)
	{
		if (done)
			return;
		LogEvent(EventSignificance::Debug,nullptr,"AI: Not done processing");
		tempMarkers.Clear();
		Timer::Time started = timer.Now();
		for (index_t i = vehicles.Count() - 1; i < vehicles.Count(); i--)
		{
			done = false;
			ProcessVehicle(vehicles[i]);
			vehicles.EraseLast();
			if (timer.GetSecondsSince(started) > timeQuantum)
				return;
		}
		if (!done && vehicles.IsEmpty())
		{
			done = true;
			for (index_t i = 0; i < Objects::Marker::all.Count(); i++)
			{
				const Objects::Marker&m = Objects::Marker::all[i];
				if (m.type == Objects::Marker::Type::Visited && m.age > 100)
				{
					LogEvent(EventSignificance::Debug,nullptr,"AI: Deleting marker "+m.GetName()+". Reached end-of-life after 100 rounds");

					SignalMarkerDeletion(m.id);
					i--;
				}
			}

			ASSERT__(done);
			LogEvent(EventSignificance::Debug,nullptr,"AI: all done. requesting new round");
			SignalRoundEnd();
			
		}

	}


}

#include <global_root.h>
#include "tower.h"
#include "../structureCompound.h"
#include "../gridAndChoices.h"
#include "../faction.h"

namespace Objects
{
	#ifdef HAS_WINDOW
		/*static*/ PClientAttachment	Tower::staticShape;
		/*static*/ Geometry				Tower::geometry;
	#endif
	/*static*/ float					Tower::foundationHeight;
	/**
	List of all known tower types (one for each tower turret type).
	Tower types that are not contained in this list can not be constructed cleanly
	*/
	Ctr::Buffer0<Tower::PProto>			Tower::globalPrototypes;

	#ifndef CLIENT
		Ctr::Buffer0<PTower>			towers;	//only used by server ... so far
	#endif



	#ifndef CLIENT
		

		/*virtual override*/ void			Tower::OnInsert( float&atTime, bool freshConstruction )
		{
			Super::OnInsert(atTime,freshConstruction);
			if (freshConstruction)
				ChangeStorage(*this,atTime,turret->ammoType,GetMaxStorageCapacity().level[turret->ammoType] - storage.level[turret->ammoType]);
		}

		


		/*virtual override*/ void		Tower::FlushAutoAttacks()
		{
			while (attackQueue.IsNotEmpty() && attackQueue.Peek().autoTarget)
				attackQueue.EraseFront();
		}




	
		/*virtual override*/ void Tower::Attack(float&attackTime)
		{
			ASSERT__(attackQueue.IsNotEmpty());

			const auto target = attackQueue.Pop();


			//const Grid::LCellIdI target = task.foci.Pop();
			float3 dir = Grid::low.cells[target.target.stackID].centerVertex.xyz * Grid::ToRadius(target.target.height) - this->GetWorldPosition();
			float3 innerDir;
			Mat::rotate(this->inverse,dir,innerDir);
			static Ctr::Buffer0<M::TVec4<> > trajectory;

			bool abort = false;
			auto rs = TraceTrajectory(TrajectoryEndPoint( target.target ),true,trajectory);
			if (rs != TraceResult::ReachesTarget && target.autoTarget)
			{
				rs = ImpactDetails{};
			}

			if (rs != TraceResult::InvalidTrajectory)
			{


				float len = 0;
				for (index_t i = 1; i < trajectory.GetLength(); i++)
					len += Vec::distance(ToWorld(trajectory[i-1]),ToWorld(trajectory[i]));

				const float flightTime =  Times::Attack * len;

				{
					float beginT = attackTime - flightTime - Times::TurretRotation;
					SyncTime(beginT);
				}


				
				float change = 1.f / float(GetAttacksPerRound());
				attackActionLeft -= change;
				RegisterChange(ActionChanged,entityTime);

				if (!IsBeam(turret->trajectoryType))
				{
					//float delta = std::max(fabs(angles.x - turretAngleX), fabs(angles.y - turretAngleY));
					ApplicationInterface::SignalTurretRotation(shared_from_this(),(ToWorld(trajectory[1]) - ToWorld(trajectory.First())).Normalized0(),Times::TurretRotation);
					entityTime += Times::TurretRotation;
				}

				ApplicationInterface::TAttack at;
				at.attackAngle = rs.artilleryAngle;
				at.attackFrom = shared_from_this();
				at.flightTime = flightTime;
				at.isAutoAttack = target.autoTarget;
				at.traceTo = rs.impactAt;
				ApplicationInterface::SignalAttack(at);

				const int ammoNeed = turret->GetAmmoNeedPerShot(owner);
				if (storage.level[turret->ammoType] >= ammoNeed)
					ChangeStorage(*this,attackTime, turret->ammoType,-ammoNeed);
				else
				{
					RefreshCompoundCoverageIfNeeded(entityTime);
					ResourceVector change;
					change.level[turret->ammoType] = -ammoNeed;
					GetCompound().ChangeStorage(change,entityTime);
					//ASSERT__(change.IsEmpty());
				}
				

				
				entityTime += flightTime;
				const int damageLevel = (int)floor(turret->GetDamageLevel(owner));
				const float explosionRadius = GetAttackExplosionRadius();

				if (turret->dealsEMPDamage)
					ExplodeEMP(at,rs.impactCell,rs.impactAt, explosionRadius, damageLevel,entityTime);
				else
					Explode(at,
							rs.impactCell,rs.impactAt, explosionRadius, damageLevel,turret->isDoomsday, entityTime);

				RegisterChange((changeType_t)(TasksChanged),entityTime);
				//stateChangedAt = attackTime;	//problematic
				attackTime = entityTime;
			}
			else
			{
				LogEvent(EventClass::Preset::operationAborted,this,"Unable to plot trajectory to target");
			}



		}

		/*virtual override*/ bool Tower::FrontQueueAutoAttack(TurretBase::TargetChoice&lastChoice)
		{
			if (!CanAttack())
				return false;
			//index_t freeSlots = attackQueue.Count() < t->GetAttacksPerRound() <  t->GetAttacksPerRound() - t->attackQueue.Count() : 0;
			if ((int)attackQueue.Count() < CountPossibleAttacks())
			{
				return  GetSentryTarget(*this,this->storage.level[turret->ammoType], TrajectoryEndPoint( location,GetEffectiveFoundationHeight()),*this,lastChoice) 
						&& AttackNext(lastChoice.cellTarget,true);
			}
			return false;
		}

		/*virtual override*/ void			Tower::ProcessLeftovers(ResourceVector&v, index_t iteration, float atTime)
		{
			ResourceVector maxStorage = GetMaxStorageCapacity();
			index_t targetIteration = 1;
			#ifndef NO_MUNITION
				if (storage.level[Resource::Munitions] < maxStorage.level[Resource::Munitions] / 4)
					targetIteration = 0;
			#endif
			if (iteration != targetIteration && iteration != FinalLeftoverIteration)
				return;
			for (index_t i = 0; i < Resource::NumberOfPossibilities; i++)
			{
				if (v.level[i] == 0)
					continue;
				if (v.level[i] < 0 )
				{
					if (storage.level[i] > 0)
					{
						int delta = std::min(-v.level[i], storage.level[i]);

						ChangeStorage(*this,atTime,Resource::Reinterpret(i),-delta);

						v.level[i] += delta;
					}
				}
				else
				{
					int capacity = maxStorage.level[i] - storage.level[i];
					if (capacity > 0)
					{
						int delta = std::min(v.level[i], capacity);
						v.level[i] -= delta;

						ChangeStorage(*this,atTime,Resource::Reinterpret(i),delta);
					}	
				}
			}
		}

		/*virtual override*/ bool					Tower::AttackNext( const Grid::LCellIdI&target, bool autoTarget )
		{
			const count_t cnt = attackQueue.Count();
			if (cnt >= MaxFocusPoints)
			{
				LogEvent(EventClass::Preset::commandRejected,this,"Max queue length ("+String(MaxFocusPoints)+") reached");
				return false;
			}
			if (!turret->CanAttack())
			{
				LogEvent(EventClass::Preset::commandRejected,this,"This tower cannot attack");
				return false;
			}

			if (target != location+1)
			{
				//Grid::LCellIdI pointOfOrigin = location+1;


				//float targetDistance = Grid::GetGeometricDistance(pointOfOrigin,target);

				if (!CanHit(TrajectoryEndPoint( target)))
				{
					LogEvent(EventClass::Preset::commandRejected,this,"Destination out of range");
					return false;
				}
			}

			entityTime += timeEpsilon;
			AttackTask&cmd = attackQueue.Push();
//			cmd.id.Generate();
			cmd.target = target;
			cmd.autoTarget = autoTarget;
			SignalChange(TasksChanged,entityTime);
			return true;
		}


		/*virtual override*/ bool			Tower::DirectTo(CursorMode mode, MutableArrayRef<Grid::LCellIdI> waypoints, bool detailedPath, bool leftClick /*= false*/)
		{
		
			if (mode != Objects::CursorMode::AttackMode || !leftClick)
				return false;
			for (const auto&destination : waypoints)
			{
				if (!AttackNext(destination,false))
					return false;
				using StringConversion::ToString;
				LogEvent(EventSignificance::Common,this,"Issued tower attack to "+ToString(destination));
			}
			return true;
		}

		bool					Tower::ClearTasks()
		{
			if (attackQueue.IsEmpty())
				return false;
			attackQueue.Clear();
			SignalChange(TasksChanged,0);
			return true;
		}

		bool					Tower::DropLastTask()
		{
			if (attackQueue.IsEmpty())
				return false;
			attackQueue.EraseBack();
			SignalChange(TasksChanged,0);
			return true;
		}
	#else


		Tower::AttackTask&			Tower::AppendTask(const Grid::LCellIdI&attackTarget)
		{
			AttackTask&task = attackQueue.Append();
			task.target = attackTarget;
			return task;
		}

		#ifdef HAS_WINDOW
			/*virtual override*/ bool			Tower::TestIntersection(const M::TVec3<>&_b, const M::TVec3<>&_d, float & distance) const
			{
				M::TVec3<> b,d;//,test;
				Mat::Transform(inverse,_b,b);
				Mat::rotate(inverse,_d,d);
				return geometry.TestIntersection(b,d,distance) | turret->TestIntersection(GetEffectiveFoundationHeight(), turretAngleX,turretAngleY,b,d,distance);	//binary or - must evaluate both
			}

			/*virtual override*/ void			Tower::DescribeType(Command::Base::Label&label, UINT32 descFlags) 
			{
				label.BeginBrief();
				Super::DescribeType(label,descFlags);
				label.MarkBriefEnd();
			}

		#endif

	#endif


	/*virtual override*/ void				Tower::VisitSensors( const Grid::LCellIdI&loc, const std::function<void(const World::Sensor&)>&visitor, bool appearance /*= false*/ ) const
	{
		if (turret->detectsResources)
		{
			const auto center = GetScanningCenter(loc);
			float range = GetScanningRange();

			visitor(World::Sensor(center, range, World::Sensor::UpperHalfSolidSphere));
			visitor(World::Sensor(center, range, World::Sensor::ShadedCylinder,20));
		}
		else
			Super::VisitSensors(loc, visitor, appearance);
	}


	/*virtual override*/ float3				Tower::GetScanningCenter(const Grid::LCellIdI&loc) const
	{
		return Grid::low.cells[loc.stackID].centerVertex.xyz * Grid::ToRadiusf(1.5f + loc.height);
	}


	bool	Tower::setup = false;
	/*virtual override*/	void			Tower::PrototypeSetup()
	{
		Super::PrototypeSetup();
		if (setup)
			return;
		setup = true;

		#ifdef HAS_WINDOW
			geometry.Clear();
		#endif

		static const float staticHeight = 1.005f;
		foundationHeight = staticHeight; //checked during visual client Objects::StructureShape::LoadAll(). Should match loaded geometry
	}


	/*virtual override*/ float			Tower::GetAttackExplosionRadius() const
	{
		if (!turret->CanAttack())
			return 0.f;
		return turret->explosionRadius.GetValue(GetResearchState());
	}

	/*virtual override*/ int			Tower::GetOperationSpeed(Component::operation_t op) const
	{
		return turret->operationSpeed[op].GetValue(GetResearchState());
	}

	/*virtual override*/ String			Tower::DescribeOperationSpeed(Component::operation_t op) const
	{
		return turret->operationSpeed[op].Describe(GetResearchState());
	}
	

	/*virtual override*/ float			Tower::GetFlatAttackRange() const
	{
		if (!turret->CanAttack())
			return 0.f;
		return EMPModify(turret->horizontalAttackRange.GetValue(GetResearchState())+2.f * Grid::UnitScale,2.f);
	}

	/*virtual override*/ String			Tower::DescribeFlatAttackRange() const
	{
		if (!turret->CanAttack())
			return 0.f;
		String rs = turret->horizontalAttackRange.Describe(GetResearchState());
		const auto final = turret->horizontalAttackRange.GetValue(GetResearchState())+2.f * Grid::UnitScale;
		return String(M::Round(final*10.f)/10.f)+"=["+rs+"]+"+String(2.f * Grid::UnitScale)+" (Tower Bonus)";
	}


	bool	Tower::HasAmmoForShot()
	{
		return GetCompound().total.storage.level[turret->ammoType] >= turret->GetAmmoNeedPerShot(owner);
		//compoundID < compounds.Count() && compounds[compoundID].total.storage.level[turret->ammoType] >= turret->ammoNeedPerShot;
	}

}

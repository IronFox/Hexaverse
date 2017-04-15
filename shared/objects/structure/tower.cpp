#include <global_root.h>
#include "tower.h"
#include "../structureCompound.h"

namespace Objects
{
	/*static*/ CommandContainer			Tower::commands;
	/*static*/ PClientAttachment		Tower::staticShape;
	/*static*/ Geometry					Tower::geometry;
	/*static*/ float					Tower::foundationHeight;
	/**
	List of all known tower types (one for each tower turret type).
	Tower types that are not contained in this list can not be constructed cleanly
	*/
	Buffer0<PTower>				Tower::globalPrototypes;

	#ifndef CLIENT
		Buffer0<PTower>			towers;	//only used by server ... so far
	#endif



	#ifndef CLIENT
		

		/*virtual override*/ void			Tower::OnConstructionFinished(float&atTime)
		{
			Super::OnConstructionFinished(atTime);
			ChangeStorage(*this,atTime,turret->ammoType,GetMaxStorageCapacity().level[turret->ammoType] - storage.level[turret->ammoType]);
			//storage.level[Resource::Munitions] = GetMaxStorageCapacity().level[Resource::Munitions];
			//LogEvent(EventSignificance::Common,this,"Munition storage changed to "+String(storage.level[Resource::Munitions]));

		}

		

		bool Tower::WouldAttack(const PWorldEntity&target, Grid::LCellIdI&outTarget, float entityTime)
		{
			if (!HasAmmoForShot() || empDamageLevel != 0 || IsHibernating())
			{
				//LogEvent(EventClass::Preset::commandRejected,this,"Insufficient ammunition");
				return false;
			}

			static Buffer0<TVec4<> > trajectory;

			TVec3<> iat;
			Grid::HCellIdI ic;
			float angle;

			//float d2 = Grid::GetQuadraticGeometricDistance(location+1,target->location);
			TrajectoryEndPoint endPoint(target->location); 
			return		//owner->canSee.IsSet(target)
					/*&&	*/turret->CanAttack() 
					&& entityTime <= entityTime 
					&& CountPossibleAttacks() > 0 
					&& ExtCanHit(owner,TrajectoryEndPoint( location,GetEffectiveFoundationHeight() ), endPoint,outTarget)
					//&& d2 > sqr(turret->explosionRadius)
					&& TraceTrajectory(endPoint,trajectory,iat,ic,angle)==TurretTrajectory::ReachesTarget;
		}



	
		/*virtual override*/ void Tower::Attack(float&attackTime)
		{
			ASSERT__(attackQueue.IsNotEmpty());

			const auto target = attackQueue.Pop();


			//const Grid::LCellIdI target = task.foci.Pop();
			float3 dir = Grid::low.cells[target.target.stackID].centerVertex.xyz * Grid::ToRadius(target.target.height) - this->GetWorldPosition();
			float3 innerDir;
			Mat::rotate(this->inverse,dir,innerDir);
			TVec3<> t0;
			Grid::HCellIdI t1;
			static Buffer0<TVec4<> > trajectory;

			bool abort = false;
			float angle;
			TurretTrajectory t = TraceTrajectory(TrajectoryEndPoint( target.target ),trajectory,t0,t1,angle);
			if (t != TurretTrajectory::ReachesTarget && target.autoTarget)
			{
				t = TurretTrajectory::InvalidTrajectory;
			}

			if (t != TurretTrajectory::InvalidTrajectory)
			{


				float len = 0;
				for (index_t i = 1; i < trajectory.length(); i++)
					len += Vec::distance(ToWorld(trajectory[i-1]),ToWorld(trajectory[i]));

				const float flightTime =  Times::Attack * len;

				{
					float beginT = attackTime - flightTime - Times::TurretRotation;
					SyncTime(beginT);
				}


				
				float change = 1.f / float(GetAttacksPerRound());
				attackActionLeft -= change;
				RegisterChange(ActionChanged,entityTime);

				{
					//float delta = std::max(fabs(angles.x - turretAngleX), fabs(angles.y - turretAngleY));
					ApplicationInterface::SignalTurretRotation(shared_from_this(),(ToWorld(trajectory[1]) - ToWorld(trajectory.First())).Normalized0(),Times::TurretRotation);
					entityTime += Times::TurretRotation;
				}

				ApplicationInterface::TAttack at;
				at.attackAngle = angle;
				at.attackFrom = shared_from_this();
				at.flightTime = flightTime;
				at.isAutoAttack = target.autoTarget;
				at.traceTo = t0;
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
				const float explosionRadius = turret->GetExplosionRadius(owner);

				if (turret->dealsEMPDamage)
					ExplodeEMP(at,
								t1, t0, explosionRadius, damageLevel,entityTime);
				else
					Explode(at,
							t1, t0, explosionRadius, damageLevel,turret->isDoomsday, entityTime);

				RegisterChange((changeType_t)(TasksChanged),entityTime);
				//stateChangedAt = attackTime;	//problematic
				attackTime = entityTime;
			}
			else
			{
				LogEvent(EventClass::Preset::operationAborted,this,"Unable to plot trajectory to target");
			}



		}

		/*virtual override*/ bool Tower::FrontQueuePossibleAttacks()
		{
			if (!HasAmmoForShot())
				return false;
			//index_t freeSlots = attackQueue.Count() < t->GetAttacksPerRound() <  t->GetAttacksPerRound() - t->attackQueue.Count() : 0;
			bool rs = false;
			while ((int)attackQueue.Count() < CountPossibleAttacks())
			{
				Grid::LCellIdI target;
				if (!GetSentryTarget(*this,this->storage.level[turret->ammoType], TrajectoryEndPoint( location,GetEffectiveFoundationHeight()),*this,target) || !AttackNext(target,true))
					return rs;
				rs = true;
			}
			return rs;

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

		bool					Tower::AttackNext(const Grid::LCellIdI&target, bool autoTarget)
		{
			if (attackQueue.Count() >= MaxStructureTasks)
			{
				LogEvent(EventClass::Preset::commandRejected,this,"Max queue length ("+String(MaxStructureTasks)+") reached");
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


		/*virtual override*/ bool			Tower::DirectTo(CursorMode mode, ArrayRef<Grid::LCellIdI> waypoints, bool detailedPath, bool leftClick /*= false*/)
		{
		
			if (mode != Objects::CursorMode::AttackMode || !leftClick)
				return false;
			for (const auto&destination : waypoints)
			{
				if (!AttackNext(destination,false))
					return false;
				LogEvent(EventSignificance::Common,this,"Issued tower attack to "+destination.ToString());
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
		/*virtual override*/ void				Tower::VisitAllCommands(FCommandVisitor visitor, bool force)
		{
			commands.VisitMembers([visitor,this](const int2&coords, const Command::PBase&cmd)
			{
				if (turret->CanAttack() || cmd->name!="Attack")
					visitor(coords,cmd);
			},force);
		}


		Tower::AttackTask&			Tower::AppendTask(const Grid::LCellIdI&attackTarget)
		{
			AttackTask&task = attackQueue.Append();
			task.target = attackTarget;
			return task;
		}

		#ifdef HAS_WINDOW
			/*virtual override*/ bool			Tower::TestIntersection(const TVec3<>&_b, const TVec3<>&_d, float & distance) const
			{
				TVec3<> b,d;//,test;
				Mat::transform(inverse,_b,b);
				Mat::rotate(inverse,_d,d);
				return geometry.TestIntersection(b,d,distance) | turret->TestIntersection(GetEffectiveFoundationHeight(), turretAngleX,turretAngleY,b,d,distance);	//binary or - must evaluate both
			}
			/*virtual override*/ void			Tower::DescribeType(Command::Base::Label&label, UINT32 descFlags) 
			{
				label.BeginBrief();
				Super::DescribeType(label,descFlags);
				label.MarkBriefEnd();
				if (turret->CanAttack())
					label << "The unit automatically attacks hostile units in range (possibly extended by its impact explosion radius)."
							" Towers are the only units that can intercept hostile units during their movement phase."
							" A hostile mobile unit that enters their attack range is immediately attacked as long as sufficient attack action is left."
							" This tower will never automatically fire at locations that endanger friendly units."
							;
			}

		#endif

	#endif




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


	
	/*virtual override*/ float			Tower::GetFlatAttackRange() const
	{
		if (!turret->CanAttack())
			return 0.f;
		return cache.horizontalAttackRange.GetEMPModified(*this,turret->horizontalAttackRange,Objects::LuaAttribute<float,UnitScaleFilter>(2.f));
		//EMPModify(turret->GetHorizontalAttackRange(owner,2.f),2.f);
	}

	/*virtual override*/ String			Tower::DescribeFlatAttackRange() const
	{
		if (!turret->CanAttack())
			return 0.f;
		return cache.horizontalAttackRange.DescribeEMPModified(*this,turret->horizontalAttackRange,Objects::LuaAttribute<float,UnitScaleFilter>(2.f));
		//EMPModify(turret->GetHorizontalAttackRange(owner,2.f),2.f);
	}


	bool	Tower::HasAmmoForShot()
	{
		return GetCompound().total.storage.level[turret->ammoType] >= turret->GetAmmoNeedPerShot(owner);
		//compoundID < compounds.Count() && compounds[compoundID].total.storage.level[turret->ammoType] >= turret->ammoNeedPerShot;
	}

}

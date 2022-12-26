#include <global_root.h>
#include "tower.h"
#include "../structureCompound.h"
#include "../gridAndChoices.h"
#include "../subOrbital.h"
#include "../faction.h"
#include "../t2.h"

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
	Ctr::Vector<Tower::PProto>			Tower::globalPrototypes;

	#ifndef CLIENT
		Ctr::Vector<PTower>			towers;	//only used by server ... so far
	#endif



	#ifndef CLIENT
		

		/*virtual override*/ void			Tower::OnInsert( float&atTime, bool freshConstruction )
		{
			Super::OnInsert(atTime,freshConstruction);
			if (freshConstruction)
				ChangeStorage(*this,atTime,turret->ammoType,GetMaxStorageCapacity()[turret->ammoType] - storage[turret->ammoType]);
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
			static Ctr::Vector<M::TVec4<> > trajectory;

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
				RegisterChange(ActionChanged);

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

				const auto&state = GetResearchState();
				const int ammoNeed = turret->GetAmmoNeedPerShot(state);
				if (storage[turret->ammoType] >= ammoNeed)
					ChangeStorage(*this,attackTime, turret->ammoType,-ammoNeed);
				else
				{
					RefreshCompoundCoverageIfNeeded(entityTime);
					Resource::Vector change;
					change.Set(turret->ammoType, -ammoNeed);
					GetCompound().ChangeStorage(change,entityTime);
					//ASSERT__(change.IsEmpty());
				}
				

				
				entityTime += flightTime;
				const int damageLevel = (int)floor(GetDamageLevel());
				const float explosionRadius = GetAttackExplosionRadius();

				if (turret->dealsEMPDamage)
					ExplodeEMP(at,rs.impactCell,rs.impactAt, explosionRadius, damageLevel,entityTime);
				else
					Explode(at,
							rs.impactCell,rs.impactAt, explosionRadius, damageLevel,turret->mainAttackIsDoomsday, entityTime);

				RegisterChange(TasksChanged);
				//stateChangedAt = attackTime;	//problematic
				attackTime = entityTime;
			}
			else
			{
				LogEvent(EventClass::Preset::operationAborted,this,"Unable to plot trajectory to target");
			}



		}

		/*virtual override*/ bool Tower::QueueAutoAttack(TurretBase::TargetChoice&lastChoice)
		{
			if (!CanAttack())
				return false;
			//index_t freeSlots = attackQueue.Count() < t->GetAttacksPerRound() <  t->GetAttacksPerRound() - t->attackQueue.Count() : 0;
			if ((int)attackQueue.Count() < CountPossibleAttacks())
			{
				return  GetSentryTarget(*this,this->storage[turret->ammoType], TrajectoryEndPoint( location,GetEffectiveFoundationHeight()),*this,lastChoice) 
						&& AttackNext(lastChoice.cellTarget,true);
			}
			return false;
		}

		/*virtual override*/ void			Tower::ProcessLeftovers(Resource::Vector&v, index_t iteration, float&atTime)
		{
			Resource::Vector maxStorage = GetMaxStorageCapacity();
			index_t targetIteration = 1;
			#ifndef NO_MUNITION
				if (storage[Resource::Munitions] < maxStorage[Resource::Munitions] / 4)
					targetIteration = 0;
			#endif
			if (iteration != targetIteration && iteration != FinalLeftoverIteration)
				return;
			for (index_t i = 0; i < Resource::table.GetSize(); i++)
			{
				if (v[i] == 0)
					continue;
				if (v[i] < 0 )
				{
					if (storage[i] > 0)
					{
						int delta = std::min(-v[i], storage[i]);

						ChangeStorage(*this,atTime,Resource::ID(i),-delta);

						v.Add(i, delta);
					}
				}
				else
				{
					int capacity = maxStorage[i] - storage[i];
					if (capacity > 0)
					{
						int delta = std::min(v[i], capacity);
						v.Add(i, -delta);

						ChangeStorage(*this,atTime,Resource::ID(i),delta);
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

			AttackTask&cmd = attackQueue.Push();
//			cmd.id.Generate();
			cmd.target = target;
			cmd.autoTarget = autoTarget;
			RegisterChange(TasksChanged);
			return true;
		}


		/*virtual override*/ bool			Tower::DirectTo( CursorMode mode, MutableArrayRef<Grid::LCellIdI> waypoints, bool detailedPath, bool leftClick /*= false*/ )
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
			RegisterChange(TasksChanged);
			return true;
		}

		bool					Tower::DropLastTask()
		{
			if (attackQueue.IsEmpty())
				return false;
			attackQueue.EraseBack();
			RegisterChange(TasksChanged);
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

			/*virtual override*/ void			Tower::DescribeType(Command::Base::Label&label, UINT32 descFlags, const OwnerParameter&owner, const T2::PConstProperties&t2)
			{
				label.BeginBrief();
				Super::DescribeType(label,descFlags, owner, t2);
				label.MarkBriefEnd();
			}

		#endif

	#endif


	/*virtual override*/ void				Tower::VisitSensors( const Grid::LCellIdI&loc, const std::function<void(const World::Sensor&)>&visitor, bool appearance /*= false*/ ) const
	{
		const float sub = GetSubOrbitalScanningRange();
		if (sub > 0)
		{
			visitor(World::Sensor(Grid::ToWorld(Grid::LCellIdI(loc.stackID,SubOrbital::Height)),sub,World::Sensor::UpperHalfSolidSphere));
		}

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






	/*virtual override*/ int			Tower::GetOperationSpeed(Component::Operation op, const OwnerParameter&owner) const
	{
		return turret->operationSpeed[op].GetValue(GetResearchState(owner));
	}

	/*virtual override*/ String			Tower::DescribeOperationSpeed(Component::Operation op, const OwnerParameter&owner) const
	{
		return turret->operationSpeed[op].Describe(GetResearchState(owner));
	}
	/*virtual override*/ float				Tower::GetSubOrbitalScanningRange(const OwnerParameter&owner) const
	{
		if (empDamageLevel)
			return 0;
		return EMPModify(turret->subOrbitalScanningRange.GetValue(GetResearchState(owner)),1);
	}

	/*virtual override*/ String				Tower::DescribeSubOrbitalScanningRange(const OwnerParameter&) const
	{
		if (empDamageLevel)
			return 0;
		return turret->subOrbitalScanningRange.Describe(GetResearchState());
	}
	

	/*virtual override*/ float			Tower::GetFlatAttackRangeExt(const OwnerParameter&owner, const T2::TModifier&t2) const
	{
		if (!turret->CanAttack())
			return 0.f;
		return EMPModify(t2.Apply(turret->horizontalAttackRange.GetValue(GetResearchState(owner))+2.f),2.f);
	}

	/*virtual override*/ String			Tower::DescribeFlatAttackRangeExt(const OwnerParameter&owner, const T2::TModifier&t2) const
	{
		if (!turret->CanAttack())
			return "";
		const auto&state = GetResearchState(owner);
		const String rs = turret->horizontalAttackRange.Describe(state);
		const auto preMod = turret->horizontalAttackRange.GetValue(state)+2.f;
		const auto final = t2.Apply(preMod);
		if (t2.HasEffect())
			return DescNumeric(final) + " [="+ DescNumeric(preMod)+" [="+ rs+"+2 (tower modifier)]" + t2.Describe()+"]";
		return DescNumeric(final)+" [="+rs+"+2 (tower modifier)]";
	}


	bool	Tower::HasAmmoForShot() const
	{
		return GetCompound().total.storage[turret->ammoType] >= turret->GetAmmoNeedPerShot(GetResearchState());
		//compoundID < compounds.Count() && compounds[compoundID].total.storage[turret->ammoType] >= turret->ammoNeedPerShot;
	}

}

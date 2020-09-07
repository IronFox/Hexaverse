#include <global_root.h>
#include "turretBase.h"

#include "gridAndChoices.h"
#include "faction.h"
#include "t2.h"


#ifdef HAS_WINDOW
	#include <engine/display.h>
	extern Engine::Display<Engine::DirectX11>	display;
#endif

namespace Objects
{



	
	#ifndef CLIENT
		bool TurretBase::WouldAutoAttack(const PWorldEntity&target, TurretBase::TargetChoice&outTarget, float entityTime)
		{
			auto&e = ToEntity();
			if (!CanAttack() || e.empDamageLevel != 0 || e.IsHibernating() || e.owner->IsAlliedWith(target->owner))
			{
				//LogEvent(EventClass::Preset::commandRejected,this,"Insufficient ammunition");
				return false;
			}



			//float d2 = Grid::GetQuadraticGeometricDistance(location+1,target->location);
			TrajectoryEndPoint endPoint(target->location); 
			 

			return		//owner->canSee.IsSet(target)
					/*&&	*/turret->CanAttack() 
					&& entityTime <= entityTime 
					&& CountPossibleAttacks() > 0 
					&& ExtCanHit(e.owner,TrajectoryEndPoint( e.location,GetZOffset() ), endPoint,outTarget);
		}
	#endif

	/*static*/ bool				TurretBase::CheckArea(const PFaction&myFaction, const Grid::LCellIdI&origin,const M::TVec3<>&source3, float explosionRadius, TargetChoice&outExtResult)
	{
		AreaHelper	helper;
		using M::Sqr;
		
		helper << origin;
		Grid::LCellIdI current;
		//const M::TVec3<> source3 = Grid::ToWorld(origin);
		const float range2 = M::Sqr(explosionRadius);

		#ifdef _DEBUG
			outExtResult.fieldsChecked.Clear();
			outExtResult.explosionRadius = explosionRadius;
			outExtResult.explosionCenter = source3;
		#endif

		while (helper >> current)
		{
			#ifdef _DEBUG
				auto&f = outExtResult.fieldsChecked.Append();
				f.cell = current;
			#endif

			M::TVec3<> world;
			Vec::mult(Grid::low.cells[current.stackID].cornerCenter,Grid::ToRadius(current.height),world);
				//Grid::ToWorld(current);
			float fromSource2 = Vec::quadraticDistance(source3,world);
			if (fromSource2 > range2)
			{
				#ifdef _DEBUG
					f.outOfRange = true;
				#endif
				continue;
			}


			PWorldEntity e = GetEntity(current);
			if (e && myFaction->IsFriendly(e->owner) && !e->IsIndestructible())
			{
				#ifdef _DEBUG
					f.friendlyFound = true;
				#endif
				return false;
			}


			{
				const auto&n = Grid::low.grid.GetCell(current.stackID).neighborhood;
				for (int i = 0; i < n.numNeighbors; i++)
					helper << Grid::LCellIdI(n.neighborID[i],current.height);
				if (current.height+1 < Grid::CellStack::HalfRange)
					helper << current + 1;
				if (current.height-1 >= -Grid::CellStack::HalfRange)
					helper << current - 1;
			}

		}
		return true;
	
	}
	
	int						TurretBase::GetOperationSpeed(Component::Operation op) const
	{
		return GetOperationSpeed(op,ToEntity().owner);
	}


	float					TurretBase::GetAttackInitiative() const
	{
		float ini = turret->initiative.GetValue(ToEntity().GetResearchState()) * GetAttackActionLeft();
		const auto&e = ToEntity();
		if (e.t2)
			ini = e.t2->initiative.Apply(ini);
		return ini;
	}

	int						TurretBase::GetAttacksPerRound() const
	{
		return turret->GetAttacksPerRound(ToEntity().GetResearchState());
	}
	int						TurretBase::GetAttacksPerRound(const OwnerParameter&owner, const T2::TModifier&p) const
	{
		return p.Apply(turret->GetAttacksPerRound(WorldEntity::GetResearchState(owner)));
	}


	int						TurretBase::GetDamagePotentialPerRound() const
	{
		const auto&e = ToEntity();
		return GetDamagePotentialPerRound(e.owner,e.t2);
	}

	int						TurretBase::GetDamagePotentialPerRound(const OwnerParameter&owner, const T2::PConstProperties&p) const
	{
		const auto&state = WorldEntity::GetResearchState(owner); 
		if (p)
			return std::max(1,p->attacksPerRound.Apply(turret->GetAttacksPerRound(state))) * p->damageLevel.Apply(turret->GetDamageLevel(state));
		return std::max(1,turret->GetAttacksPerRound(state)) * turret->GetDamageLevel(state);
	}

	float					TurretBase::GetFlatAttackRange() const
	{
		const auto&e = ToEntity();
		return GetFlatAttackRangeExt(e.owner,e.t2 ? e.t2->horizontalAttackRange : T2::TModifier());
	}

	float					TurretBase::GetAttackExplosionRadius() const
	{
		const auto&e = ToEntity();
		return GetAttackExplosionRadiusExt(e.owner, e.t2 ? e.t2->explosionRadius : T2::TModifier());
	}

	/*virtual*/ float		TurretBase::GetAttackExplosionRadiusExt(const OwnerParameter&owner, const T2::TModifier&t2) const
	{
		if (!turret->CanAttack())
			return 0.f;
		return t2.Apply(turret->explosionRadius.GetValue(ToEntity().GetResearchState(owner)));
	}

	/*virtual*/ String		TurretBase::DescribeAttackExplosionRadiusExt(const OwnerParameter&owner, const T2::TModifier&m) const
	{
		if (!turret->CanAttack())
			return "";
		const auto&state = ToEntity().GetResearchState(owner);
		if (m.HasEffect())
			return DescNumeric(m.Apply(turret->explosionRadius.GetValue(state)))+" [="+turret->explosionRadius.Describe(state) + m.Describe()+"]";
		return turret->explosionRadius.Describe(state) + m.Describe();
	}


	String					TurretBase::DescribeAttackExplosionRadius() const
	{
		const auto&e = ToEntity();
		return DescribeAttackExplosionRadiusExt(e.owner, e.t2 ? e.t2->explosionRadius : T2::TModifier());
	}


	String					TurretBase::DescribeFlatAttackRange() const
	{
		const auto&e = ToEntity();
		return DescribeFlatAttackRangeExt(e.owner, e.t2 ? e.t2->horizontalAttackRange : T2::TModifier());
	}


	int						TurretBase::GetDamageLevel() const
	{
		const auto&e = ToEntity();
		return GetDamageLevel(e.owner,e.t2);
	}

	int						TurretBase::GetDamageLevel(const OwnerParameter&owner, const T2::PConstProperties&p) const
	{
		const auto&state = WorldEntity::GetResearchState(owner); 
		if (p)
			return p->damageLevel.Apply(turret->GetDamageLevel(state));
		return turret->GetDamageLevel(state);
	}

	String					TurretBase::DescribeDamageLevel() const
	{
		const auto&e = ToEntity();
		return DescribeDamageLevel(e.owner,e.t2);
	}


	String					TurretBase::DescribeDamageLevel(const OwnerParameter&owner, const T2::PConstProperties&p) const
	{
		
		const auto&state = WorldEntity::GetResearchState(owner); 

		String rs = turret->damageLevel.Describe(state);
		if (p)
			rs = DescNumeric(GetDamageLevel(owner,p))+" [="+rs+   p->damageLevel.Describe()+"]";
		return rs;
	}


	/*static*/ bool TurretBase::IsSafeHit( const PFaction&myFaction, TrajectoryType t, float range,const World::Data&visibleWorld, const World::Data&actualWorld,const TrajectoryEndPoint&source, const TrajectoryEndPoint&dest, float explosionRadius, TargetChoice&outExtResult )
	{
		outExtResult.impact = Component::Turret::TraceTrajectory(t,range,visibleWorld, actualWorld,true,source,dest,outExtResult.trajectory);

		if (outExtResult.impact == TraceResult::InvalidTrajectory)
			return false;
		if (outExtResult.impact == TraceResult::MidFlightImpact)
		{
			float3 last = outExtResult.trajectory.Last().xyz * outExtResult.trajectory.Last().w;
			float dist = Vec::quadraticDistance(last,dest.worldLocation);
			if (dist > M::Sqr(explosionRadius))
				return false;
		}

		//Component::Turret::CanHit(turret->trajectoryType, range,source,destIn)

		if (outExtResult.impact.distanceToTarget == 0)
		{
			if (!CheckArea(myFaction,dest.cellLocation,dest.worldLocation,  explosionRadius, outExtResult))
				return false;
		}
		else
		{
			const Grid::LCellIdI impact = {Grid::low.Locate(outExtResult.impact.impactAt,dest.cellLocation.stackID),outExtResult.impact.impactCell.height };
			if (!CheckArea(myFaction,impact,outExtResult.impact.impactAt, explosionRadius, outExtResult))
				return false;
		}
		outExtResult.cellTarget = dest.cellLocation;
		return true;
	}

	bool					TurretBase::CanAttack()
	{
		return CanAttackIgnoreAmmo()
			&& HasAmmoForShot();
	}

	bool					TurretBase::CanAttackIgnoreAmmo() const
	{
		return turret->CanAttack() 
			;
	}


	/*static*/ bool					TurretBase::FindTarget(const PFaction&myFaction, TrajectoryType trajectoryType, float flatAttackRange, float explosionRadius, const TrajectoryEndPoint&source, const TrajectoryEndPoint&destIn, TargetChoice&outExtResult)
	{
		using M::Sqr;
		const float range = flatAttackRange;
		const float explosionRadius2 = M::Sqr(explosionRadius);

		if (!Component::Turret::CanHit(trajectoryType, range + explosionRadius * 2, source, destIn))
			return false;

		const auto&visibleWorld =
			#ifdef CLIENT
						World::actual;
			#else
						myFaction->knownWorld;
			#endif
		if (IsSafeHit(myFaction, trajectoryType, range, visibleWorld, World::actual, source, destIn, explosionRadius, outExtResult))
			return true;


		AreaHelper	helper;
		//queue << destIn;
		{
			const auto&n = Grid::low.grid.GetCell(destIn.cellLocation.stackID).neighborhood;
			for (int i = 0; i < n.numNeighbors; i++)
				helper << Grid::LCellIdI(n.neighborID[i], destIn.cellLocation.height);
			if (destIn.cellLocation.height < Grid::CellStack::TopHeight)
				helper << destIn.cellLocation + 1;
			if (destIn.cellLocation.height <= Grid::CellStack::TopHeight)
				helper << destIn.cellLocation - 1;
		}
		Grid::LCellIdI current;
		while (helper >> current)
		{
			M::TVec3<> w3 = Grid::ToWorld(current);
			float fromDest2 = Vec::quadraticDistance(destIn.worldLocation, w3);
			if (fromDest2 > explosionRadius2)
				continue;
			const auto target2 = TrajectoryEndPoint(current, 0);
			if (!World::actual.LowCellIsClear(current) ||
				!Component::Turret::CanHit(trajectoryType, range, source, target2)
				)
				continue;

			if (IsSafeHit(myFaction, trajectoryType, range, visibleWorld, World::actual, source, target2, explosionRadius, outExtResult))
				return true;

			{
				const auto&n = Grid::low.grid.GetCell(current.stackID).neighborhood;
				for (int i = 0; i < n.numNeighbors; i++)
					helper << Grid::LCellIdI(n.neighborID[i], current.height);
				if (current.height < Grid::CellStack::TopHeight)
					helper << current + 1;
				if (current.height <= Grid::CellStack::TopHeight)
					helper << current - 1;
			}
		}
		return false;

	}

	bool					TurretBase::ExtCanHit(const PFaction&myFaction, const TrajectoryEndPoint&source, const TrajectoryEndPoint&destIn, TargetChoice&outExtResult)	const
	{
		return FindTarget(myFaction, turret->trajectoryType, GetFlatAttackRange(), GetAttackExplosionRadius(), source, destIn, outExtResult);
	}



}

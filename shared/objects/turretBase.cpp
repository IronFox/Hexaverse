#include <global_root.h>
#include "turretBase.h"

#include "gridAndChoices.h"
#include "faction.h"


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
			&& (turret->trajectoryType != TrajectoryType::AAMissile || !ToEntity().empDamageLevel)
			;
	}


	bool					TurretBase::ExtCanHit(const PFaction&myFaction, const TrajectoryEndPoint&source, const TrajectoryEndPoint&destIn, TargetChoice&outExtResult)	const
	{
		using M::Sqr;
		//const float velocity = Turret::MaxRangeToVelocity(GetFlatAttackRange());
		const float range = GetFlatAttackRange();
		//const float extRange = range + (turret->explosionRadius);
		//const float range2 = M::Sqr(range);
		//const float extRange2 = M::Sqr(extRange);
		//const M::TVec3<> source3 = Grid::ToWorld(source);
		//M::TVec3<> dest3 = Grid::ToWorld(destIn);
		//float distance2 = Vec::quadraticDistance(source3,dest3);
		//if (Turret::GetAngleForDistance(velocity,source,destIn))
		//if (distance2 > extRange2)
		//	return false;
		const float explosionRadius = GetAttackExplosionRadius();
		const float explosionRadius2 = M::Sqr(explosionRadius);

		if (!Component::Turret::CanHit(turret->trajectoryType, range+explosionRadius*2,source,destIn))
			return false;
	
		const auto&visibleWorld = 
			#ifdef CLIENT
				World::actual;
			#else
				myFaction->knownWorld;
			#endif
		if (IsSafeHit(myFaction, turret->trajectoryType,range,visibleWorld,World::actual,source,destIn,explosionRadius,outExtResult))
			return true;

	
		AreaHelper	helper;
		//queue << destIn;
		{
			const auto&n = Grid::low.grid.GetCell(destIn.cellLocation.stackID).neighborhood;
			for (int i = 0; i < n.numNeighbors; i++)
				helper << Grid::LCellIdI(n.neighborID[i],destIn.cellLocation.height);
			if (destIn.cellLocation.height < Grid::CellStack::TopHeight)
				helper << destIn.cellLocation + 1;
			if (destIn.cellLocation.height <= Grid::CellStack::TopHeight)
				helper << destIn.cellLocation - 1;
		}
		Grid::LCellIdI current;
		while (helper >> current)
		{
			M::TVec3<> w3 = Grid::ToWorld(current);
			float fromDest2 = Vec::quadraticDistance(destIn.worldLocation,w3);
			if (fromDest2 > explosionRadius2)
				continue;
			const auto target2 = TrajectoryEndPoint( current,0);
			if (!World::actual.LowCellIsClear(current) || 
				!Component::Turret::CanHit(turret->trajectoryType, range,source,target2)
				)
				continue;
			//float fromSource2 = Vec::quadraticDistance(source3,world);
			//if (fromSource2 > extRange2)
				//continue;

			if (IsSafeHit(myFaction, turret->trajectoryType,range,visibleWorld,World::actual,source,target2,explosionRadius,outExtResult))
				return true;

			{
				const auto&n = Grid::low.grid.GetCell(current.stackID).neighborhood;
				for (int i = 0; i < n.numNeighbors; i++)
					helper << Grid::LCellIdI(n.neighborID[i],current.height);
				if (current.height < Grid::CellStack::TopHeight)
					helper << current + 1;
				if (current.height <= Grid::CellStack::TopHeight)
					helper << current - 1;
			}
		}
		return false;
	}



}

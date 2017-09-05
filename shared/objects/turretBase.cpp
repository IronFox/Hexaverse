#include <global_root.h>
#include "turretBase.h"

#include "gridAndChoices.h"


#ifdef HAS_WINDOW
	#include <engine/display.h>
	extern Engine::Display<Engine::DirectX11>	display;
#endif

namespace Objects
{

	


	


	/*static*/ bool				TurretBase::CheckArea(const OwnerParameter&myFaction, const Grid::LCellIdI&origin, float explosionRadius)
	{
		AreaHelper	helper;
		using M::Sqr;
		
		{
			const auto&n = Grid::low.grid.GetCell(origin.stackID).neighborhood;
			for (int i = 0; i < n.numNeighbors; i++)
				helper << Grid::LCellIdI(n.neighborID[i],origin.height);
			if (origin.height+1 < Grid::CellStack::HalfRange)
				helper << origin + 1;
			if (origin.height-1 >= -Grid::CellStack::HalfRange)
				helper << origin - 1;
		}
		Grid::LCellIdI current;
		const M::TVec3<> source3 = Grid::ToWorld(origin);
		float range2 = M::Sqr(explosionRadius);

		while (helper >> current)
		{

			M::TVec3<> world = Grid::ToWorld(current);
			float fromSource2 = Vec::quadraticDistance(source3,world);
			if (fromSource2 > range2)
				continue;


			PWorldEntity e = GetEntity(current);
			if (e && e->owner == myFaction && !e->IsIndestructible())
				return false;


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



	bool					TurretBase::ExtCanHit(const OwnerParameter&myFaction, const TrajectoryEndPoint&source, const TrajectoryEndPoint&destIn, Grid::LCellIdI&destOut)	const
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
		const float explosionRadius = turret->GetExplosionRadius(myFaction);
		const float explosionRadius2 = M::Sqr(explosionRadius);
		if (Component::Turret::CanHit(turret->trajectoryType, range,source,destIn)
			&& CheckArea(myFaction,destIn.cellLocation,explosionRadius))
		{
			destOut = destIn.cellLocation;
			return true;
		}

		if (!Component::Turret::CanHit(turret->trajectoryType, range+explosionRadius*2,source,destIn))
			return false;

		AreaHelper	helper;
		//queue << destIn;
		{
			const auto&n = Grid::low.grid.GetCell(destIn.cellLocation.stackID).neighborhood;
			for (int i = 0; i < n.numNeighbors; i++)
				helper << Grid::LCellIdI(n.neighborID[i],destIn.cellLocation.height);
			helper << destIn.cellLocation + 1;
			helper << destIn.cellLocation - 1;
		}
		Grid::LCellIdI current;
		while (helper >> current)
		{
			M::TVec3<> world = Grid::ToWorld(current);
			float fromDest2 = Vec::quadraticDistance(destIn.worldLocation,world);
			if (fromDest2 > explosionRadius2)
				continue;
			if (!World::actual.LowCellIsClear(current) || 
				!Component::Turret::CanHit(turret->trajectoryType, range,source,TrajectoryEndPoint( current,0)))
				continue;
			//float fromSource2 = Vec::quadraticDistance(source3,world);
			//if (fromSource2 > extRange2)
				//continue;



			if (CheckArea(myFaction,current,explosionRadius))
			{
				destOut = current;
				return true;
			}
			{
				const auto&n = Grid::low.grid.GetCell(current.stackID).neighborhood;
				for (int i = 0; i < n.numNeighbors; i++)
					helper << Grid::LCellIdI(n.neighborID[i],current.height);
				helper << current + 1;
				helper << current - 1;
			}
		}
		return false;
	}



}

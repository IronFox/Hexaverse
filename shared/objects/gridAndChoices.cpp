#include <global_root.h>
#include "gridAndChoices.h"
#include "rendering.h"
#include "interface.h"
#include "visualSelection.h"
#include "prototypes.h"
#include "vehicle.h"
#include "satellite.h"
#include "faction.h"
#ifndef CLIENT
	#include "../runtimeConfig.h"
#endif

namespace Objects
{
	Array<PVehicle>			vehicleGrid(Grid::LowSetup::GridType::CellCount * Grid::CellStack::Range);
	Array<PStructure>		structureGrid(Grid::LowSetup::GridType::CellCount * Grid::CellStack::Range);


	




	#ifdef CLIENT

		UINT32		uplinkTraffic = 0,
					onlineUplinkTraffic = 0;


		bool NoHostileVehicleHere(const Grid::LCellIdI&cellID, const OwnerParameter&v)
		{
			PVehicle vh = GetVehicle(cellID);
			return !vh || vh->owner == v;
		}
		#ifndef NO_SATELLITES
			bool NoHostileSatelliteHere(UINT32 cellID, const OwnerParameter&v)
			{
				PSatellite vh = GetSatellite(cellID);
				return !vh || vh->owner == v;
			}
		#endif
	#else
		bool NoHostileVehicleHere(const Grid::LCellIdI&cellID, const OwnerParameter&v, float t)
		{
			PVehicle vh = GetVehicle(cellID);
			return !vh || vh->owner == v 
					|| !v->CanSee(vh,t)
			;
		}

		#ifndef NO_SATELLITES
			bool NoHostileSatelliteHere(UINT32 cellID, const OwnerParameter&v, float t)
			{
				PSatellite vh = GetSatellite(cellID);
				return !vh || vh->owner == v 
						|| !v->CanSee(vh,t)
				;
			}
		#endif
		

		void SignalClusterChange(const Grid::LCellIdI&location, int verticalTolerance,float atTime,const WorldEntity*except)
		{

			for (int h = location.height-verticalTolerance; h <= location.height+verticalTolerance; h++)
			{
				if (h >= -Grid::CellStack::HalfRange && h < Grid::CellStack::HalfRange)
				{
					PStructure s = GetStructure(Grid::LCellIdI(location.stackID,h));
					if (s && s.get() != except)
					{
						TStructureNeighborhood env;
						Objects::GetNeighborhood(s->owner,s->ConnectsToDifferentOwnership(),s->location, env);
						s->SignalEnvironmentChange(env,atTime);
					}
				}
			}


			//foreach(structures, s)
			//	if ((*s)->location.stackID == location.stackID && abs((*s)->location.height - location.height) <= verticalTolerance)
			//	{
			//		Objects::GetNeighborhood((*s)->owner,(*s)->ConnectsToDifferentOwnership(),(*s)->location, env);
			//		(*s)->SignalEnvironmentChange(env,atTime);
			//	}



			#ifdef CLIENT
				Grid::SignalLightingChange(location+1);
			#endif
		}

		void		VerifyGrid()
		{
			foreach (vehicles,v)
				ASSERT1__(GetVehicle((*v)->location) == *v, (*v)->location.ToString());
			foreach (structures,s)
				ASSERT1__(GetStructure((*s)->location) == *s, (*s)->location.ToString());

		}






		void SignalVehicleClusterChange(const Grid::LCellIdI&location, int verticalTolerance, float atTime, const WorldEntity*except)
		{
			for (int h = location.height-verticalTolerance; h <= location.height+verticalTolerance; h++)
			{
				if (h >= -Grid::CellStack::HalfRange && h < Grid::CellStack::HalfRange)
				{
					PVehicle v = GetVehicle(Grid::LCellIdI(location.stackID,h));
					if (v && v.get() != except)
					{
					
						if (Grid::actualWorld.Clear(v->location,atTime))
						{
							LogEvent(EventSignificance::Common,v.get(),"Cleared terrain to make space for vehicle");
						}

						if (v->base->requirements == Component::VehicleBase::Requirement::Water)
						{
							if (Grid::actualWorld.Overwrite(v->location-1,Grid::Content::Water, atTime))
							{
								LogEvent(EventSignificance::Common,v.get(),"Cleared terrain to make water for naval vehicle");
							}
						}

						v->PlaceExactlyAt(v->location,0,atTime);	//should do the trick
					}
				}
			}
			//foreach(vehicles, s)
			//	if ((*s)->location.stackID == location.stackID && abs((*s)->location.height - location.height) <= verticalTolerance)
			//	{
			//		Grid::Clear((*s)->location,atTime);
			//		(*s)->PlaceExactlyAt((*s)->location,0,atTime);	//should do the trick
			//	}

		}

		void SignalClusterAndNeighborsChange(const Grid::LCellIdI&location, float atTime, const WorldEntity*except/* = nullptr*/)
		{
			const BaseGrid::TCellNeighborhood&n = Grid::low.grid.GetCell(location.stackID).neighborhood;
			SignalClusterChange(location, 1,atTime,except);
			for (int j = 0; j < n.numNeighbors; j++)
				SignalClusterChange(Grid::LCellIdI(n.neighborID[j], location.height), 1,atTime,except);

			SignalVehicleClusterChange(location, 1,atTime,except);
			for (int j = 0; j < n.numNeighbors; j++)
				SignalVehicleClusterChange(Grid::LCellIdI(n.neighborID[j], location.height), 1,atTime,except);


			#ifdef CLIENT
				Grid::SignalLightingChange(location);
			#endif

		}


		bool		GetSentryTarget(const WorldEntity&source, int munitionStorage, const TrajectoryEndPoint&sourceLocation, const TurretBase&base, Grid::LCellIdI&outTarget)
		{
			float c2 = std::numeric_limits<float>::max();
			Grid::LCellIdI closest = Grid::LCellIdI::Invalid;
			PWorldEntity closestEntity;
			if (base.turret->GetAmmoNeedPerShot(source.owner) > munitionStorage)
				return false;
			const Grid::WorldData&world = source.owner->currentState;
			source.owner->seeing.VisitAllKeys([&world, &source,sourceLocation,&base,&c2,&closest,&closestEntity](const PWorldEntity&e)
			{
//					&& turret->TraceTrajectory(location+1,target.location,trajectory,iat,ic)==Turret::ReachesTarget;		
				
				Grid::LCellIdI loc;
				if (!base.ExtCanHit(source.owner,sourceLocation,TrajectoryEndPoint( e->location ),loc))
					return;

				
				//if (d2 > sqr(maxRange))
				//	return;
				//if (d2 < sqr(turret->explosionRadius))
				//	return;
				static Buffer0<TVec4<> > trajectory;
				Grid::HCellIdI ic;
				TVec3<> iat;
				float angle;
				if (Component::Turret::TraceTrajectory(base.turret->trajectoryType,base.GetFlatAttackRange(),   
							world, Grid::actualWorld,sourceLocation,TrajectoryEndPoint( loc ),trajectory,
							iat,ic,angle) != TurretTrajectory::ReachesTarget)
					return;
				float d2 = Grid::GetQuadraticGeometricDistance(sourceLocation.cellLocation,loc);
				if (d2 < c2)
				{
					c2 = d2;
					closest = loc;
					closestEntity = e;
				}
			});
			if (closest != Grid::LCellIdI::Invalid)
			{
				ASSERT__(closestEntity->isInserted);
				LogEvent(EventSignificance::Debug,&source,"Auto-targeting "+closestEntity->GetName()+" (located at "+closestEntity->location.ToString()+") via closest viable trajectory target "+closest.ToString());
				LogEvent(EventClass( EventSignificance::Important, EventFlavor::Neutral),&source,"Auto-targeting "+closestEntity->GetNiceName());

				outTarget = closest;
				return true;
			}
			return false;
		}


		
		void Explode(const ApplicationInterface::TAttack& attack, 
					 const Grid::HCellIdI&targetCell, const TVec3<>&center, int explosionRadius, int damageLevel, bool doomsday,
					 float&atTime)
		{
			if (doomsday && RuntimeConfig::doomsdayWeapons == RuntimeConfig::DoomsdayWeapons::Disallow)
			{
				LogEvent(EventClass::Preset::commandRejected,nullptr,"Unable to process doomsday explosion: disallowed");
				return;
			}

			//if (RuntimeConfig::empZones && Grid::AffectedByEMP(targetCell))
			//{
			//	LogEvent(EventClass::Preset::commandRejected,nullptr,"Explosion dissipated due to EMP field in target area");
			//	return;
			//}

			ApplicationInterface::SignalExplosion(
							attack, 
							center,explosionRadius,false,
							doomsday,
							atTime);

			atTime += ExplosionTime(explosionRadius) * 0.25f;

			int core = std::max(0,explosionRadius-3);
			DBG_ASSERT__(explosionRadius >= 1);
			float terrainExplosionRadius = float(explosionRadius)-0.5f;
			//float coreRadius = core*core;
			float outerRadius = explosionRadius*explosionRadius;
			static Buffer0<PWorldEntity>	toDestroy,allLivingAffected;
			static Buffer0<PStructure>		affectedStructures;
			static Buffer0<PVehicle>		affectedVehicles;
			ForeachWorldEntity([center,outerRadius,damageLevel,&atTime](const PWorldEntity e)
			{
				if (e->IsIndestructible())
					return;
				float d2 = std::numeric_limits<float>::max();
				const int h = e->GetHeight();
				const TVec3<>&c = Grid::low.cells[e->location.stackID].cornerCenter;
				TVec3<> p;
				for (int k = 0; k <= h; k++)
				{
					Vec::mult(c,Grid::ToRadius(e->location.height + k),p);
					float d3 = Vec::quadraticDistance(p,center);
					d2 = std::min(d2,d3);
				}
				//LogEvent(EventSignificance::Debug,e->get(),Vec::toString((*e)->system.w.xyz)+" <> "+Vec::toString(center)+ " = "+ String(sqrt(d2)));
				//if (d2 < coreRadius)
				//{
				//	toDestroy << *e;
				//}
				//else
				{
					if (d2 <= outerRadius)
					{
						atTime = std::max(atTime,e->motionTime);
						e->TakeDamage(damageLevel);
						if (e->health <= 0)
						{
							toDestroy << e;
						}
						else
						{
							e->RegisterChange(WorldEntity::HealthChanged,atTime);
						}
					}
				}
				if (e->health <= 0)
					return;	//prevent being added to affect*
				allLivingAffected << e;
				{
					Vec::mult(c,Grid::ToRadius(e->location.height -1),p);	//include own lower level
					float d3 = Vec::quadraticDistance(p,center);
					d2 = std::min(d2,d3);
				}

				if (e->IsVehicle())
				{
					if (d2 <= outerRadius)
					{
						affectedVehicles << To<Vehicle>::Cast(e);
					}
				}
				else
				if (e->IsStructure())
				{
					const auto&n = Grid::low.grid.GetCell(e->location.stackID).neighborhood;
					for (int k = 0; k < n.numNeighbors; k++)
					{
						p = Grid::ToWorld(Grid::LCellIdI(n.neighborID[k],e->location.height-1));	//include lower level of neighbors
						float d3 = Vec::quadraticDistance(p,center);
						d2 = std::min(d2,d3);
					}

					if (d2 <= outerRadius)
					{
						affectedStructures << To<Structure>::Cast(e);
					}
				}
			},false);
			atTime += timeEpsilon;
			foreach (toDestroy,d)
			{
				(*d)->SyncTime(atTime);
				Remove(*d,Objects::RemovalType::UncontrolledExplosion,atTime,false);
			}
			toDestroy.Clear();



			if (terrainExplosionRadius > 0)
			{
				Grid::actualWorld.Explode(targetCell,terrainExplosionRadius, atTime);
				if (doomsday && RuntimeConfig::doomsdayWeapons == RuntimeConfig::DoomsdayWeapons::Expose)
					foreach (Faction::all,f)
					{
						bool changed[BaseGrid::NumFamilies] = {false};
						PFaction faction = f->lock();
						if (faction)
						{
							faction->RegisterExplosionCrater(targetCell,terrainExplosionRadius,atTime);
							//faction->currentState.Explode(targetCell,terrainExplosionRadius, changed);
							//faction->SignalCoverageChange(changed,atTime);
						}

					}

				foreach (affectedStructures, s)
				{
					TStructureNeighborhood env;
					Objects::GetNeighborhood((*s)->owner,(*s)->ConnectsToDifferentOwnership(),(*s)->location, env);
					(*s)->SignalEnvironmentChange(env,atTime);
				}
				//static Buffer0<PVehicle>	copy;
				//copy = vehicles;	//in case they get whooped by falling
				for (const auto&v : affectedVehicles)
				{
					v->PlaceExactlyAt(v->location,0, atTime);	//in case they fall due to explosions
					if (v->health <= 0)
					{
						DBG_ASSERT__(!vehicles.Contains(v));
					}
				}
			}
			foreach (allLivingAffected,d)
			{
				(*d)->SyncTime(atTime);
			}
			allLivingAffected.Clear();
			affectedStructures.Clear();
			affectedVehicles.Clear();
		}
	
		void ExplodeEMP(const ApplicationInterface::TAttack& attack, 
						const Grid::HCellIdI&targetCell, const TVec3<>&center, int explosionRadius, int damageLevel, float&atTime)
		{
			ApplicationInterface::SignalExplosion(
							attack, 
							center,explosionRadius,true,false,atTime);

			float outerRadius = explosionRadius*explosionRadius;
			static Buffer0<PWorldEntity>	toDisable;


			ForeachWorldEntity([center,outerRadius,damageLevel,&atTime](const PWorldEntity e)
			{
				float d2 = std::numeric_limits<float>::max();
				const int h = e->GetHeight();
				const TVec3<>&c = Grid::low.cells[e->location.stackID].cornerCenter;
				TVec3<> p;
				for (int k = 0; k <= h; k++)
				{
					Vec::mult(c,Grid::ToRadius(e->location.height + k),p);
					float d3 = Vec::quadraticDistance(p,center);
					d2 = std::min(d2,d3);
				}
				if (d2 <= outerRadius)
				{
					atTime = std::max(atTime,e->motionTime);
					toDisable << e;
				}
			},false);
			atTime += timeEpsilon;
			foreach (toDisable,_d)
			{
				PWorldEntity d = *_d;
				d->SyncTime(atTime);
				d->HandleEMPDamage(damageLevel, atTime);
			}
			toDisable.Clear();
		}
	


	#endif


	bool NoStructureHere(const Grid::LCellIdI&stackID)
	{
		return !GetStructure(stackID);
	}


	bool _IsNeighbor(const Grid::LCellIdI&cellID, const BaseGrid::TCellNeighborhood&n, const Grid::LCellIdI&testThis)
	{
		if (abs(testThis.height - cellID.height) > 1)
			return false;
		for (int j = 0; j < n.numNeighbors; j++)
			if (n.neighborID[j] == testThis.stackID)
				return true;
		return false;
	}




	void GetNeighborhood( const OwnerParameter&referenceOwner, bool ignoreOwnership, const Grid::LCellIdI&cellID, TStructureNeighborhood&outNeighborhood)
	{
		DBG_ASSERT__(outNeighborhood.numEntries == 0);	//must be empty
		const auto& n = Grid::low.grid.GetCell(cellID.stackID).neighborhood;

		#ifndef CLIENT
			int lower = clamp(cellID.height - 1, Grid::CellStack::BottomHeight, Grid::CellStack::TopHeight),
				upper = clamp(cellID.height + 1, Grid::CellStack::BottomHeight, Grid::CellStack::TopHeight);
			for (int h = lower; h <= upper; h++)
			{
				UINT32 layer = Grid::CellStack::ToLayer(h);
				for (int j = 0; j < n.numNeighbors; j++)
				{
					PStructure s = GetStructure(Grid::LCellIdI(n.neighborID[j],h));
					if (!s)
						continue;
					if (
							#ifndef CLIENT
								!ignoreOwnership && 
								!s->ConnectsToDifferentOwnership() &&
							#endif
							s->owner != referenceOwner 
						)
					{
						//LogEvent(EventSignificance::Debug,s->get(),"Rejected due to ownership: "+referenceOwner.ToString()+" != "+(*s)->owner.ToString());
						continue;
					}

					//if (_IsNeighbor(cellID, n, (*s)->location))
					outNeighborhood.Append(s);
				}
			}
		#else
			foreach(structures, s)
			{
				//LogEvent(EventSignificance::Debug,s->get(),"Testing as neighbor of cell "+cellID.ToString()+" local position is "+(*s)->location.ToString());
				if (
						#ifndef CLIENT
							!ignoreOwnership && 
							!(*s)->ConnectsToDifferentOwnership() &&
						#endif
						(*s)->owner != referenceOwner 
					)
				{
					//LogEvent(EventSignificance::Debug,s->get(),"Rejected due to ownership: "+referenceOwner.ToString()+" != "+(*s)->owner.ToString());
					continue;
				}

				if (_IsNeighbor(cellID, n, (*s)->location))
					outNeighborhood.Append(*s);
				//else
					//LogEvent(EventSignificance::Debug,s->get(),"Rejected due to location");
			}
		#endif
	}


	


	index_t					CellToGridIndex(const Grid::LCellIdU&location)
	{
		index_t rs = location.layer * Grid::LowSetup::GridType::CellCount + location.stackID;
		ASSERT_LESS__(rs,vehicleGrid.GetLength());
		return rs;
	}

	index_t					CellToGridIndex(const Grid::LCellIdI&location)
	{
		index_t rs = Grid::CellStack::ToLayer(location.height) * Grid::LowSetup::GridType::CellCount + location.stackID;
		ASSERT_LESS__(rs,vehicleGrid.GetLength());
		return rs;
	}

	bool NothingHere(const Grid::LCellIdI&stackID)
	{
		return !GetStructure(stackID) && !GetVehicle(stackID);
	}
	
	bool NoVehicleHere(const Grid::LCellIdI&stackID)
	{
		return !GetVehicle(stackID);
	}
	bool NoVehicleHereExcept(const Grid::LCellIdI&cellID, const Vehicle*v)
	{
		PVehicle vh = GetVehicle(cellID);
		return !vh || vh.get() == v 
			#ifndef CLIENT
				|| !v->owner->CanSee(vh,v->motionTime)
			#endif
		;
	}

	#ifndef NO_SATELLITES
		bool NoSatelliteHere(const UINT32 stackID)
		{
			return !GetSatellite(stackID);
		}
		bool NoSatelliteHereExcept(UINT32 stackID, const Satellite*v)
		{
			PSatellite vh = GetSatellite(stackID);
			return !vh || vh.get() == v
				#ifndef CLIENT
					|| !v->owner->CanSee(vh,v->motionTime)
				#endif
			;
		}
	#endif

		
	template <typename T>
		void				RemoveFromGrid(Array<T>&grid, const Grid::LCellIdI&location, const T&object)
		{
			ASSERT__(object->isInsertedIntoGrid);
			index_t at = CellToGridIndex(location);
			if (grid[at] != object)
			{
				index_t foundAt = InvalidIndex;
				index_t cloneAt = InvalidIndex;
				T	cloneIs;
				for (index_t i = 0; i < grid.length(); i++)
				{
					if (grid[i] == object)
					{
						ASSERT__(foundAt == InvalidIndex);
						foundAt = i;
					}
					else if (grid[i] && grid[i]->id == object->id)
					{
						cloneAt = i;
						cloneIs = grid[i];
					}
				}
				if (grid[at])
					LogEvent(EventClass::Preset::error,grid[at].get(),"Found instead of "+object->GetName());
				String clone = cloneAt != InvalidIndex ? " {found clone ("+cloneIs->GetName()+") at "+Grid::LCellIdI(cloneAt%Grid::LowSetup::GridType::CellCount,Grid::CellStack::ToHeight((UINT32)( cloneAt/Grid::LowSetup::GridType::CellCount))).ToString()+"}" : "";
				if (foundAt != InvalidIndex)
				{
					Grid::LCellIdI actualLocation(foundAt%Grid::LowSetup::GridType::CellCount,Grid::CellStack::ToHeight((UINT32)( foundAt/Grid::LowSetup::GridType::CellCount)));
					FATAL__("Trying to remove "+object->GetName()+" from location "+String(at)+"/"+location.ToString()+", but object resides in "+String(foundAt)+"/"+actualLocation.ToString()+clone);
				}
				else
					FATAL__("Trying to remove "+object->GetName()+" from location "+String(at)+"/"+location.ToString()+", but object is not mapped in the grid"+clone);
			}
			LogEvent(EventSignificance::Common,object.get(),"Removed from grid location "+String(at)+"/"+location.ToString());
			grid[at].reset();
			object->isInsertedIntoGrid = false;
		}
	template <typename T>
		void				InsertIntoGrid(Array<T>&grid, const Grid::LCellIdI&location, const T&object)
		{
			ASSERT__(!object->isInsertedIntoGrid);
			Grid::AssertIsValid(location);
			index_t at = CellToGridIndex(location);
			if (grid[at])
			{
				FATAL__("Trying to register "+object->GetName()+"(@"+object->location.ToString()+") at "+location.ToString()+": Cell occupied by "+grid[at]->GetName()+"(@"+grid[at]->location.ToString()+")");
			}
			ASSERT3__(!grid[at],grid[at]->GetName(),location.ToString(),object->GetName());
			LogEvent(EventSignificance::Common,object.get(),"Inserted into grid location "+String(at)+"/"+location.ToString());
			grid[at] = object;
			object->isInsertedIntoGrid = true;
		}
	template <typename T>
		T					QueryGrid(const Array<T>&grid, const Grid::LCellIdI&location)
		{
			return grid[CellToGridIndex(location)];
		}
	template <typename T>
		T					QueryGrid(const Array<T>&grid, const Grid::LCellIdU&location)
		{
			return grid[CellToGridIndex(location)];
		}


	PStructure GetStructure(const Grid::LCellIdI&stackID)
	{
		if (stackID.height > Grid::CellStack::TopHeight)
			return PStructure();
		return QueryGrid(structureGrid,stackID);
	}

	PStructure GetStructure(const Grid::LCellIdU&stackID)
	{
		if (stackID.layer >= Grid::CellStack::Range)
			return PStructure();
		return QueryGrid(structureGrid,stackID);
	}

	PVehicle GetVehicle(const Grid::LCellIdI&stackID)
	{
		return QueryGrid(vehicleGrid,stackID);
	}

	PMobileEntity GetMobileEntity(const Grid::LCellIdI&cellID)
	{
		if (cellID.height <= Grid::CellStack::TopHeight)
			return QueryGrid(vehicleGrid,cellID);
		#ifndef NO_SATELLITES
			return GetSatellite(cellID.stackID);
		#else
			return PMobileEntity();
		#endif
	}
	PMobileEntity GetMobileEntity(const Grid::LCellIdU&cellID)
	{
		if (cellID.layer < Grid::CellStack::Range)
			return QueryGrid(vehicleGrid,cellID);
		#ifndef NO_SATELLITES
			return GetSatellite(cellID.stackID);
		#else
			return PMobileEntity();
		#endif
	}


	#ifndef NO_SATELLITES
		PSatellite		GetSatellite(UINT32 flatCellID)
		{
			for (const auto&s : satellites)
				if (s->location.stackID == flatCellID)
					return s;
			return PSatellite();
		}
	#endif

	PWorldEntity					GetEntity(const Grid::LCellIdI&cellID)
	{
		if (cellID.height > Grid::CellStack::TopHeight)
		{
			#ifndef NO_SATELLITES
				if (Satellite::IsSatelliteHeight( cellID.height ))
					return GetSatellite(cellID.stackID);	//if there is no satellite here, there is nothing else here either
			#endif
			return PWorldEntity();
		}
		PWorldEntity e = GetVehicle(cellID);
		if (e)
			return e;
		return GetStructure(cellID);
	}



	bool NothingBlockingHere(const Grid::LCellIdI&cellID, bool checkVehiclesOnClient)
	{
		if (cellID.height > Grid::CellStack::TopHeight)
		{
			#ifndef NO_SATELLITES
				if (Satellite::IsSatelliteHeight( cellID.height ))
				{
					#ifdef CLIENT
						return !checkVehiclesOnClient || !GetSatellite(cellID.stackID);	//timing issues may cause problems with getVehicle, but sometimes it wont matter
					#else
						return !GetSatellite(cellID.stackID);
					#endif
				}
			#endif
			return false;
		}

		PStructure s = GetStructure(cellID);
		if (s && !To<Connector>::DynamicCast(s))
			return false;
		#ifdef CLIENT
			return !checkVehiclesOnClient || !GetVehicle(cellID);	//timing issues may cause problems with getVehicle, but sometimes it wont matter
		#else
			PVehicle v = GetVehicle(cellID);
			return !v;
		#endif
	}


	void	GetProductionChoices(const WorldEntity*producer,Component::VehicleBase*production,
									BasicBuffer<Choice>&choices)
	{
		choices.Clear();	
		const auto&n = Grid::low.grid.GetCell(producer->location.stackID).neighborhood;

		for (int j =  -1; j <= 1 + producer->GetHeight(); j++)
		{
			int h = producer->location.height + j;
			if (h < Grid::CellStack::BottomHeight || h > Grid::CellStack::TopHeight)
				continue;
			if (!production->CanClimb(j))
				continue;
			for (int i = 0; i < n.numNeighbors; i++)
			{
				const Grid::LCellIdI	cell(n.neighborID[i], producer->location.height + j);


				bool okay = cell.height < Grid::CellStack::HalfRange;
				if (okay)
				{
					if (j < 0)
					{
						for (int j = cell.height + 1; j <= producer->location.height; j++)
							if (!Grid::actualWorld.LowCellIsClear(Grid::LCellIdI(n.neighborID[i], j)))
							{
								okay = false;
								break;
							}
					}
					else
						for (int j = cell.height; j > producer->location.height; j--)
							if (j >= producer->location.height + producer->GetHeight() &&  !Grid::actualWorld.LowCellIsClear(Grid::LCellIdI(producer->location.stackID, j)))
							{
								okay = false;
								break;
							}
				}
				if (!okay)
					continue;

				if (Grid::actualWorld.LowCellIsClear(cell) && NothingBlockingHere(cell,true)
						&& production->CanStandOn(Grid::actualWorld,cell-1,false,false)
						&& !Grid::AffectedByEMP(cell)
				)
				{
					choices << Choice(cell,0);
				}
			}
		}

	}


	
	void		GetChoices(WorldEntity&entity, const PMobileEntity&deploying, CursorMode activeCursorMode, BasicBuffer<Choice>&movementChoices, bool&showArrow, bool&hasArrowCellTarget, TVec3<>&arrowFrom, Grid::LCellIdI&arrowFromCell)
	{
		const Grid::WorldData&world = 
		#ifdef CLIENT
			Grid::actualWorld;
		#else
			entity.owner->currentState;
		#endif

		movementChoices.Clear();
		
		const MobileEntityProducer*p = dynamic_cast<const MobileEntityProducer*>(&entity);// entity.ToProducer();
		//const WorkingStructure*ws = dynamic_cast<WorkingStructure*>(&entity);
		if (p && p->AwaitsUserInteraction())
		{
			{
				ASSERT1__(p->queue.IsNotEmpty(),entity.GetName());
				Component::VehicleBase* base = nullptr;
				const auto&building = p->queue.First();

				if (building.CanSpawnInGrid())
					GetProductionChoices(&entity,building.GetVehicleBase(),movementChoices);

				int h;
				if (building.CanSpawnAboveGrid(h))
				{
					if (entity.IsAboveGrid())
					{
						//off-grid producing off-grid

						const auto&n = Grid::low.grid.GetCell(entity.location.stackID).neighborhood;

						for (int i = 0; i < n.numNeighbors; i++)
						{
							UINT32 stackID = n.neighborID[i];
							Grid::LCellIdI	at(stackID,h);
							if (!GetEntity(at) && !Grid::AffectedByEMP(at))
								movementChoices << Choice(at,0);
						}
					}
					else
					{
						Grid::LCellIdI	at(entity.location.stackID,h);
						if (!GetEntity(at))
							movementChoices << Choice(at,0);
					}
				}
			}
			return;
		}

		if (deploying)
		{
			const Hangar*s = dynamic_cast<Hangar*>(&entity);
			if (s)
			{
				const Grid::LCellIdI location = s->ToEntity().location;
				const auto&n = Grid::low.grid.GetCell(location.stackID).neighborhood;

				//const auto&c = Grid::low.grid.GetCell(location.stackID);
				if (location.height <= Grid::CellStack::TopHeight && deploying->CanSpawnInGrid())
				{
					for (int h = location.height -1; h <= location.height+1; h++)
					{
						if (h >= Grid::CellStack::BottomHeight && h <= Grid::CellStack::TopHeight)
						{
							for (int j = 0; j < n.numNeighbors; j++)
							{
								const Grid::LCellIdI candidate(n.neighborID[j],h);
								if (Grid::actualWorld.LowCellIsClear(candidate) && NothingBlockingHere(candidate,true) && !Grid::AffectedByEMP(candidate))
								{
									if (!deploying->IsVehicle() || To<Vehicle>::Cast(deploying)->CanStandOn(Grid::actualWorld,candidate-1,false,false))
										movementChoices << Choice(candidate,0);
								}
							}
						}
					}
				}
				
				int h;
				if (deploying->CanSpawnAboveGrid(h))
				{
					if (location.height > Grid::CellStack::TopHeight)
						for (int j = 0; j < n.numNeighbors; j++)
						{
							const Grid::LCellIdI candidate(n.neighborID[j],h);
							if (NothingBlockingHere(candidate,true) && !Grid::AffectedByEMP(candidate))
							{
								movementChoices << Choice(candidate,0);
							}
						}
					else
					{
						const Grid::LCellIdI candidate(location.stackID,h);
						if (NothingBlockingHere(candidate,true))
						{
							movementChoices << Choice(candidate,0);
						}
					}
				}
			}
		}
		

//		const PVehicle v = To<Vehicle>::DynamicCast(entity);
		 MobileEntity*v = dynamic_cast<MobileEntity*>(&entity);
		float h;
		if (PTurretBase b = entity.GetTurretBase(h))
		{
			showArrow = CursorModeExpectsLeftClick(activeCursorMode);
			if (showArrow)
			{
				arrowFromCell = v ? v->GetLastPointOfOrigin() : entity.location;
				arrowFrom = Grid::low.cells[arrowFromCell.stackID].centerVertex.xyz * ( h + Grid::ToRadius(arrowFromCell.height));
			}

			#ifdef HAS_WINDOW
				if (showArrow && activeCursorMode == CursorMode::AttackMode)
				{
					switch (b->turret->trajectoryType)
					{
						case TrajectoryType::Artillery:
							Grid::SetParaboloidDisplay(
								Grid::low.cells[arrowFromCell.stackID].centerVertex.xyz, 
								h + Grid::ToRadius(arrowFromCell.height),
								b->GetFlatAttackRange(),
								float4(1,0,0,0.5f));
						break;
						case TrajectoryType::Missile:
							Grid::SetSphereDisplay(
								entity.GetWorldPosition(),
								b->GetFlatAttackRange(),
								float4(1,0,0,0.5f));
						break;
					}
				}
				else
					Grid::UnsetParaboloidAndSphereDisplay();
			#endif
		}
		if (v)
		{
			Grid::LCellIdI location = v->GetLastPointOfOrigin();
			const auto&n = Grid::low.grid.GetCell(location.stackID).neighborhood;
			//movementChoices.SetSize(n.numNeighbors);

			switch (activeCursorMode)
			{
				case CursorMode::BuildBlockMode:
				case CursorMode::RemoveBlockMode:
					break;
				#ifndef ONE_COMPOUND_PER_FACTION
					case CursorMode::RestockMode:
					case CursorMode::DepositMode:
					//case CursorMode::TransferMode:
					{

						v->resourceEnvironment.CheckRefresh(*v, location);
						const TVec3<> world = WorldEntity::GetWorldPosition(location);
						foreach (v->resourceEnvironment.sourceCompounds,cmp)
						{
							const Compound&compound = compounds[cmp->first];
							foreach (compound.allStructures,_s)
							{
								PStructure s = _s->lock();
								if (s && Vec::quadraticDistance(world,s->GetWorldPosition()) <= sqr(s->GetWirelessTransferRange()))
									movementChoices << Choice(s->location,s->GetHeight());
							}
						}
						#ifdef CLIENT
							v->resourceEnvironment.CheckRefresh(*v);
							ForeachMobileWorldEntity([v,&movementChoices,world](const PMobileEntity&m)
							{
								if (m->owner == v->owner && v != m.get() && Vec::quadraticDistance(world,m->GetWorldPosition()) <= sqr(MobileWirelessTransferRange))
									movementChoices << Choice(m->location,m->GetHeight());
							});
						#else
							foreach (v->owner->allMobiles,_m)
							{
								PMobileEntity m = _m->lock();
								if (m && v != m.get() && Vec::quadraticDistance(world,m->GetWorldPosition()) <= sqr(MobileWirelessTransferRange))
									movementChoices << Choice(m->location,m->GetHeight());
							}
						#endif
						//const auto&c = Grid::low.grid.GetCell(location.stackID);
						//for (int h = location.height -1; h <= location.height+1; h++)
						//{
						//	if (h >= -Grid::CellStack::HalfRange && h < Grid::CellStack::HalfRange-1)
						//	{
						//		for (int j = 0; j < n.numNeighbors; j++)
						//		{
						//			const Grid::LCellIdI candidate(n.neighborID[j],h);
						//			PVehicle other = GetVehicle(candidate);
						//			PStructure otherStructure = GetStructure(candidate);
						//			if ((otherStructure && otherStructure->owner==v->owner) || (other && other.get() != v && other->owner == v->owner))
						//				movementChoices << (candidate+1);
						//		}
						//	}
						//}
					}
					break;
				#endif
				case CursorMode::RepairMode:
				//case CursorMode::TransferMode:
				{
					for (int h = location.height -1; h <= location.height+1; h++)
					{
						if (h >= -Grid::CellStack::HalfRange && h < Grid::CellStack::HalfRange-1)
						{
							for (int j = 0; j < n.numNeighbors; j++)
							{
								const Grid::LCellIdI candidate(n.neighborID[j],h);
								PWorldEntity other = GetEntity(candidate);
								if (!other || other.get() == v 
									#ifndef _DEBUG
									|| other->health == other->GetMaxHealth()
									#endif
									)	//no owner-check (for now)
									continue;
								movementChoices << Choice(candidate,1);
							}
						}
					}
				}
				break;
				case CursorMode::ClearAboveMode:
					if (v->IsVehicle())
					{
						const auto&c = Grid::low.grid.GetCell(location.stackID);
						if (!world.LowCellIsClear(location + 1))
							movementChoices << Choice(location + 1,0);
						else
							for (int j = 0; j < c.neighborhood.numNeighbors; j++)
							{
								Grid::LCellIdI c2(c.neighborhood.neighborID[j], location.height + 1);
								if (!world.LowCellIsClear(c2))
									movementChoices << Choice(c2,0);
							}
					}
				break;
				case CursorMode::DigMode:
					if (v->IsVehicle())
						for (int i = 0; i < n.numNeighbors; i++)
						{
							const Grid::LCellIdI	cell(n.neighborID[i], location.height);
							if (
								(!world.LowCellIsClear(cell) 
								||
								(((Objects::Vehicle*)v)->base->requirements == Component::VehicleBase::Requirement::Water && !world.LowCellIsClearOrWater(cell-1)
								&& NothingHere(cell-1))
								)
								&& NothingHere(cell))
								movementChoices << Choice(cell,1);
						}
				break;
				case CursorMode::Idle:
				break;
			}
		}
	}

	
	int FindApplicableGround(const Grid::LCellIdI&cell, bool canStandOnWater)
	{
		#ifdef HYBRID_GRID
			index_t finer = Grid::low.grid.GetCell(cell.stackID).higherEquivalent;
			const auto&c = Grid::high.grid.GetCell(finer);
		#else
			index_t finer = cell.cellID;
		#endif
		bool incremented = true;
		int h = cell.height;
		bool set = false;
		while (incremented)
		{
			int _h = h;
			//incremented = false;
			if (set)
				_h = std::max(_h, Grid::actualWorld.cellStack[finer].FindMostApplicableGround(std::max(_h, cell.height),canStandOnWater));
			else
			{
				_h = Grid::actualWorld.cellStack[finer].FindMostApplicableGround(std::max(_h, cell.height),canStandOnWater);
				set = true;
			}
			#ifdef HYBRID_GRID
				for (int j = 0; j < c.neighborhood.numNeighbors; j++)
					_h = std::max(_h, Grid::actualWorld.cellStack[c.neighborhood.neighborID[j]].FindMostApplicableGround(std::max(_h, cell.height),canStandOnWater));
			#endif
			incremented = _h != h;
			h = _h;
		}

		if (Grid::actualWorld.cellStack[finer].GetCellI(h).IsOccupied())
			return badHeight;
		#ifdef HYBRID_GRID
			for (int j = 0; j < c.neighborhood.numNeighbors; j++)
				if (Grid::actualWorld.cellStack[c.neighborhood.neighborID[j]].GetCellI(h).IsOccupied())
					return badHeight;
		#endif
		return h;
	}



	void					GetChoices(const PWorldEntity&entity, const PMobileEntity&deploying, CursorMode activeCursorMode, BasicBuffer<Choice>&movementChoices, bool&showArrow, bool&hasArrowCellTarget, TVec3<>&arrowFrom, Grid::LCellIdI&arrowFromCell)
	{
		//showArrow = false;		//why were these here...?
		//hasArrowCellTarget = false;

		movementChoices.Clear();
		if (entity)
		{
			GetChoices(*entity,deploying,activeCursorMode, movementChoices, showArrow, hasArrowCellTarget, arrowFrom, arrowFromCell);

		}
	
	
	}

	
	static void	InsertConstructedEntity(const PWorldEntity&e, float&atTime, bool setHealthToFull)
	{
		ASSERT1__(!e->isInserted, e->GetName());
		#ifndef CLIENT
			e->SyncTime(atTime);

			ASSERT2__(To<Connector>::DynamicCast(e) || NothingBlockingHere(e->location,false),e->GetName(),e->location.ToString());
			e->ResetAllChange();
		#else
			ASSERT__(To<Connector>::DynamicCast(e) || e->IsMobile() || NothingBlockingHere(e->location,false));
			#ifndef NO_HIBERNATION
				{
					UINT32 t = e->GetUplinkTraffic();
					uplinkTraffic += t;
					if (!e->ToHibernating()->IsHibernating())
						onlineUplinkTraffic += t;
				}
			#endif
		#endif
		#ifdef HAS_WINDOW
			if (!e->ambientSound.IsCreated())
			{
				OpenAL::WaveBuffer wbo = e->GetAmbientSound();
				//Renderer::GetAmbientSoundOf(e->GetTypeName(),e->GetCTypeName());
				if (wbo.IsNotEmpty())
				{
					e->ambientSound.Create();
					e->ambientSound.EnqueueWaveBuffer(wbo);
					e->ambientSound.SetReferenceDistance(3.f,1.5f - (e->IsMobile()? 0.5f : 0.f));
					static Random random;
					e->ambientSound.SetPitch(1.f + random.NextFloat(-0.02f,0.02f));
					#ifdef _DEBUG
						e->ambientSound.name = e->GetName()+".ambient";
						e->ambientSound.reference = e.get();
					#endif
				}
			}

			if (e->ambientSound.IsCreated())
			{
				e->ambientSound.SetPosition(e->GetWorldPosition());
				e->ambientSound.SetVelocity(float3());
				e->ambientSound.SetGain(e->GetSoundLevel());
				e->ambientSound.SetLooping(true);
				e->ambientSound.Play();
			}
		#endif

		bool restored = idMap.IsSet(e->id);
		if (restored)
			ASSERT1__(storedMobileEntities.FindAndErase(e),e->GetName());
		if (!restored)
			e->OnConstructionFinished(atTime);
		LogEvent(EventSignificance::Common, e.get(),String(restored ? "Redeployed" : "Deployed")+" to "+e->location.ToString()+" at t="+String(atTime));
		if (!restored)
		{
			if (setHealthToFull)
				e->health = e->GetMaxHealth();
			ASSERT1__(idMap.SetNew(e->id,e),e->GetName());
		}

		e->RegisterSensory(atTime);

		#ifdef CLIENT
			#ifdef HAS_WINDOW
				PTurretBase b = e->GetTurretBase();
				if (b && b->turret->continuousRotation 
					#ifndef NO_HIBERNATION
						&& !e->ToHibernating()->IsHibernating()
					#endif
				)
					Renderer::animatedTurrets << b;
				if (b && b->turret->CanAttack())
					Renderer::attackers << e;
			#endif
			ApplicationInterface::OnAppear(e);
		#else
			ForeachFaction([e,&atTime](const PFaction&f)
			{
				f->CheckGridVisibility(e,atTime);
			});

		#endif

		e->isInserted = true;
	}



	void	InsertConstructed( const PMobileEntity&v, float&atTime, bool insertIntoGrid/*= true*/, bool setHealthToFull/*=true*/)
	{
		//if (insertIntoGrid)
		v->UpdateSystem();
		ASSERT1__(!v->isInsertedIntoGrid,v->GetName());
		bool restored = idMap.IsSet(v->id);
		if (restored)
		{
			ASSERT1__(!v->storedIn.expired(),v->GetName());
			v->storedIn.reset();
		}
		#ifndef CLIENT
			if (restored)
			{
				v->owner->RegisterStoredMobileLoss(v);
			}
			v->owner->RegisterActiveMobileGain(v);
			float signalTime = atTime;
		#endif

		atTime += timeEpsilon;
		InsertConstructedEntity(v,atTime,setHealthToFull);
		if (v->IsVehicle())
		{
			PVehicle veh = To<Vehicle>::Cast(v);
			vehicles << veh;
			if (insertIntoGrid)
			{
				InsertIntoGrid(vehicleGrid,v->location,veh);
			}
		}
		else
		#ifndef NO_SATELLITES
			if (v->IsSatellite())
			{
				//ASSERT__(!satellitesAreLocked);
				satellites << To<Satellite>::Cast(v);
			}
			else
		#endif
				FATAL__("Cannot insert. Unexpected mobile: "+v->GetName());

		#ifndef CLIENT
			ApplicationInterface::SignalNewMobileEntity(v,restored,signalTime);
		#endif
	}

	void			RegisterNewOffGrid(const PMobileEntity&v, float&atTime, bool setHealthToFull/*=true*/)
	{
		ASSERT1__(!v->isInsertedIntoGrid,v->GetName());
		ASSERT1__(!v->isInserted,v->GetName());
		DBG_ASSERT__(!storedMobileEntities.Contains(v));
		storedMobileEntities << v;
		v->OnConstructionFinished(atTime);
		LogEvent(EventSignificance::Common, v.get(),"Deployed into storage");
		if (setHealthToFull)
			v->health = v->GetMaxHealth();
		ASSERT1__(idMap.SetNew(v->id,v),v->GetName());


		#ifndef CLIENT
			v->SyncTime(atTime);
			v->owner->storedMobiles.Append(v);
		#endif
	}


	void InsertConstructed( const PStructure&s, float&atTime, bool refreshNetworkNow/*=false*/, bool setHealthToFull/*=true*/)
	{
		ASSERT__(s);

		PStructure existing;
		
		for (int h = 0; h < s->GetHeight(); h++)
		{
			int actualLayer = s->location.height+h;
			if (actualLayer > Grid::CellStack::TopHeight)
			{
				ASSERT__(h > 0);
				break;
			}
			existing = QueryGrid(structureGrid,s->location+h);
			ASSERT4__(!existing,s->GetName(),s->location.ToString(),h,existing->GetName());
		}
		if (s->HasFoundation())
		{
			existing = QueryGrid(structureGrid,s->location-1);
			ASSERT3__(!existing,s->GetName(),s->location.ToString(),existing->GetName());
		}

		InsertConstructedEntity(s,atTime,setHealthToFull);


		structures << s;

		#ifdef CLIENT
			if (s->IsSkyLinkStructure())
			{
				PSkyLinkStructure link = To<SkyLinkStructure>::DynamicCast(s);
				skyLinks << std::make_pair(link,To<WorkingStructure>::Cast(link->CastToStructure()->shared_from_this()));
			}
			if (s->IsPylon())
				pylons << To<Pylon>::Cast(s);
		#endif


		InsertIntoGrid(structureGrid,s->location,s);

//		InjectIntoNeighborhood(s,atTime);

		#ifdef CLIENT
			RefreshCompoundCoverage(atTime);
		#else
			s->owner->structures << s;
			PTower t = To<Tower>::DynamicCast(s);
			if (t)
				s->owner->towers << t;
			if (s->IsPylon())
				s->owner->pylons << To<Pylon>::Cast(s);
			if (!refreshNetworkNow)
				compoundCoverageChanged = true;
			else
				RefreshCompoundCoverage(atTime);
			ApplicationInterface::SignalNewStructure(s,atTime);
			if (s->IsWorkingStructure())
			{
				PWorkingStructure ws = To<WorkingStructure>::Cast(s);
				if (ws->IsEnabled())
				{
					if (refreshNetworkNow)
						FATAL__("this should not happen anymore")
					else
						ws->SetEnabled(true,atTime,false,WorkingStructure::ForceEnable);
					//if (!RefreshCompoundCoverageIfNeeded(atTime))
					//	networks[ws->compoundID].RefreshState(atTime);
					//if (!ws->CanEnable())
					//	ws->SetEnabled(false,atTime);
				}
				if (ws->WouldLikeToEnable())
				{
					if (refreshNetworkNow)
						ws->SetEnabled(true,atTime);
					else
						ws->SetEnabled(true,atTime,false,WorkingStructure::ForceEnable);
				}
				//ws->RegisterChange(WorldEntity::EnableChanged,atTime);
			}

		#endif


	}





	void					Remove( const PWorldEntity&e, RemovalType t, float&timestamp, bool controlEnabled, bool isRemovedFromGrid/*=false*/, bool ignoreIndestructible/*=false*/)
	{
		ASSERT__(e);
		e->SyncTime(timestamp,timeEpsilon);
		ASSERT1__(e->isInserted != ( t == RemovalType::DestroyedInStorage ),e->GetName());
		#ifdef CLIENT
			if (t != RemovalType::DestroyedInStorage)
			{
				#ifdef HAS_WINDOW
					if (e->ambientSound.IsCreated())
						e->ambientSound.Stop();
				#endif
				#ifndef NO_HIBERNATION
					UINT32 t = e->GetUplinkTraffic();
					uplinkTraffic -= t;
					if (!e->ToHibernating()->IsHibernating())
						onlineUplinkTraffic -= t;
				#endif
			}
			else
				isRemovedFromGrid = true;	//more permissive here
		#else
			ASSERT1__(ignoreIndestructible || !e->IsIndestructible(), e->GetName());
		#endif
		LogEvent(EventSignificance::Common, e.get(),"Removing entity ("+String(t.ToString())+") at time="+String(timestamp));
		const bool move = t==RemovalType::EnteredStorage;
		if (!move)
		{
			idMap.Unset(e->id);
			if (t != RemovalType::ChangingOwnership)	//preserve health if changing ownership
				e->health = -1;
			//e->OnDestroy(timestamp);	//moved destroy further down to prevent some cascading edge cases, which loop back into the destroyed entity
		}
		#ifdef HAS_WINDOW
			PTurretBase b = e->GetTurretBase();
			if (b)
			{
				if (b->turret->continuousRotation)
					Renderer::animatedTurrets.FindAndErase(b);
				if (b->turret->CanAttack())
					Renderer::attackers.FindAndErase(e);
			}
		#endif


		bool isStructure = e->IsStructure();
		if (isStructure)
		{
			ASSERT1__(!move,e->GetName());	//make sure we don't store structures for now, they don't like that
			#ifndef CLIENT
				e->owner->RegisterStructureLoss(e);

			#endif

			if (!isRemovedFromGrid)
				RemoveFromGrid(structureGrid,e->location,To<Structure>::Cast(e));

			ASSERT1__(structures.FindAndErase(e),e->GetName());
			#ifdef CLIENT
				PStructure s = To<Structure>::Cast(e);
				if (s->IsSkyLinkStructure())
				{
					foreach (skyLinks,link)
						if (link->second.lock() == e)
						{
							skyLinks.erase(link);
							break;
						}
				}
				if (s->IsPylon())
					pylons.FindAndErase(s);

			#endif


		}
		else
			if (e->IsMobile())
			{
				PMobileEntity v = To<MobileEntity>::Cast(e);
				#ifdef CLIENT
					ASSERT1__(v->location==v->preAnimationLocation,v->GetName());
					//inequality means that we are in an animation stream right now.
					//bad stuff happens when we delete now.
					//must fix timing issues. some deletion event did not have enough temporal
					//distance to the last motion of this vehicle
				#else
					if (t != RemovalType::DestroyedInStorage)
					{
						e->owner->RegisterActiveMobileLoss(e);
						if (move)
							e->owner->storedMobiles << v;
					}
					else
						e->owner->RegisterStoredMobileLoss(e);

				#endif
				if (v->IsVehicle() && !isRemovedFromGrid)
				{
					RemoveFromGrid(vehicleGrid,v->location,To<Objects::Vehicle>::Cast(v));
				}

				if (t == RemovalType::DestroyedInStorage)
					storedMobileEntities.FindAndErase(e);
				else
				{
					if (e->IsVehicle())
						vehicles.FindAndErase(e);
					else
						#ifndef NO_SATELLITES
							if (e->IsSatellite())
							{
								//ASSERT__(!satellitesAreLocked);
								satellites.FindAndErase(e);
							}
							else
						#endif
							FATAL__("Type not supported at this point: "+e->GetName());

					if (move)
						storedMobileEntities << v;
				}
			}
			else
			{
				FATAL__("Trying to remove unsupported entity "+e->GetName());
			}

		#ifdef HAS_WINDOW
			if (selectedEntity == e)
			{
				Deselect(controlEnabled);
			}
		#endif

		if (!move)
		{
			LogEvent(EventSignificance::Debug,e.get(),"OnDestroy()");
			e->OnDestroy(t,timestamp);
		}
		#ifdef CLIENT
			if (isStructure)
				RefreshCompoundCoverage(timestamp);
			if (!move)
				waste << e;
			ApplicationInterface::OnVanish(e,t);
		#else
			ApplicationInterface::SignalRemoval(e,t,timestamp);	//must be sent before clearing seeing-list below (UnregisterSensory) or nobody will be told except owner

			if (isStructure)
				compoundCoverageChanged = true;
		#endif
		if (t != RemovalType::DestroyedInStorage)
		{
			e->UnregisterSensory(timestamp);
			#ifndef CLIENT
			ForeachFaction([e,&timestamp](const PFaction&f)
			{
				f->SignalDisappearance(e,timestamp,true,false);
			});
			#endif
		}

		e->isInserted = false;
	}



	
	static count_t GetAllWays( const Grid::LCellIdI&location, Grid::LCellIdI*outChoices)
	{
		count_t rs = 0;
		const auto&n = Grid::low.grid.GetCell(location.stackID).neighborhood;

		for (int j = -1; j <= 1; j++)
		{
			if (location.height + j < -Grid::CellStack::HalfRange)
				continue;
			if (location.height + j >= Grid::CellStack::HalfRange)
				continue;
			if (j != 0)
			{
				(*outChoices++) = location + j;
				rs++;			
			}
			else
				for (int i = 0; i < n.numNeighbors; i++)
				{
					const Grid::LCellIdI	cell(n.neighborID[i], location.height + j);

					{
						(*outChoices++) = cell;
						rs++;
					}
				}	
		}
		return rs;
	
	}

	static count_t GetAllSatelliteWays( const UINT32 location, UINT32*outChoices)
	{
		count_t rs = 0;
		const auto&n = Grid::low.grid.GetCell(location).neighborhood;

		{
			for (int i = 0; i < n.numNeighbors; i++)
			{
				{
					(*outChoices++) = n.neighborID[i];
					rs++;
				}
			}	
		}
		return rs;
	
	}

	template <bool IgnoreStructuresAtDestination>
	bool ValidDestinationOf(const Grid::WorldData&world,const Vehicle&v, const Grid::LCellIdI&location, bool isDestination, bool ignoreWater, bool doFloat)
	{
		return 
			(world.LowCellIsClear(location) || (isDestination && (IgnoreStructuresAtDestination || MobileDestinationStructureHere(location,v))))
			&&
			(v.CanStandOn(world,location-1,ignoreWater,doFloat))
			&&
			!Grid::AffectedByEMP(location);

	}



	void GetWays(	const Grid::WorldData&world,
							const Vehicle*v,  const Grid::LCellIdI&location, 
							TWays&outWays, const Grid::LCellIdI&destination, 
							bool ignoreVehiclesOnDestination, bool ignoreFriendlyVehiclesOnRoute, bool forceRevertible)
	{
		outWays.Clear();
		const auto&n = Grid::low.grid.GetCell(location.stackID).neighborhood;

		int maxWayDown = std::min(location.height + Grid::CellStack::HalfRange, v->GetMaxFallingHeight());
		if (forceRevertible)
			maxWayDown = std::min(maxWayDown,v->GetMaxClimbingHeight());
		bool canWalk[6] = {true,true,true,true,true,true};

		for (int j = v->GetMaxClimbingHeight(); j >= -maxWayDown; j--)
		{
			if (j > 0 && !world.LowCellIsClear(Grid::LCellIdI(location.stackID, location.height+j)))
			{
				outWays.Clear();	//discard any previously found (upper) ways, which are now considered inaccessible. this is only relevant with vehicles that can climb more than 1 height level up
				continue;
			}

			bool anyCanWalk = false;
			for (int i = 0; i < n.numNeighbors; i++)
			{
				if (!canWalk[i])
					continue;
				anyCanWalk = true;
				const Grid::LCellIdI	cell(n.neighborID[i], location.height + j);

				{
					bool okay = cell.height < Grid::CellStack::HalfRange && cell.height >= -Grid::CellStack::HalfRange;
					if (!okay)
						continue;
				}
				bool valid = ValidDestinationOf<false>(world,*v,cell, cell == destination, false, false);
				if (!valid)
				{
					if (j <= 0 && !world.LowCellIsClear(cell))
						canWalk[i] = false;
					continue;
				}


				if (cell==destination)
				{
					if (!ignoreVehiclesOnDestination && !NoVehicleHereExcept(cell,v))
						continue;
				}
				else
				{
					if (!ignoreFriendlyVehiclesOnRoute)
					{
						if (!NoVehicleHereExcept(cell,v))
							continue;
					}
					else
						#ifdef CLIENT
							if (!NoHostileVehicleHere(cell,v->owner))
								continue;
						#else
							if (!NoHostileVehicleHere(cell,v->owner,v->motionTime))
								continue;
						#endif
				}

				//DBG_ASSERT__(!GetStructure(cell));//should be implied by LowCellIsClear (except if it's a connector, then not, so don't check this)
				outWays.Add(cell);
			}
			if (!anyCanWalk)
				break;
		}
	}


	#ifndef NO_SATELLITES
		void GetWays(		const Satellite*v,  UINT32 location, 
								TOrbitalWays&outWays, UINT32 destination, 
								bool ignoreVehiclesOnDestination, bool ignoreFriendlyVehiclesOnRoute)
		{
			outWays.Clear();
			const auto&n = Grid::low.grid.GetCell(location).neighborhood;

			{
				//if (!v->CanClimb(j))	//now redundant
					//continue;
				for (int i = 0; i < n.numNeighbors; i++)
				{
					UINT32	cell = n.neighborID[i];


					if (cell==destination)
					{
						if (!ignoreVehiclesOnDestination && !NoSatelliteHereExcept(cell,v))
							continue;
					}
					else
					{
						if (!ignoreFriendlyVehiclesOnRoute)
						{
							if (!NoSatelliteHereExcept(cell,v))
								continue;
						}
						else
							#ifdef CLIENT
								if (!NoHostileSatelliteHere(cell,v->owner))
									continue;
							#else
								if (!NoHostileSatelliteHere(cell,v->owner,v->motionTime))
									continue;
							#endif
					}

					//DBG_ASSERT__(!GetStructure(cell));//should be implied by LowCellIsClear (except if it's a connector, then not, so don't check this)
					outWays.Add(cell);
				}	
			}
		}
	#endif

	
	void			GetMobileWays(	const Grid::WorldData&world,
							const MobileEntity*v,  const Grid::LCellIdI&location, 
							TWays&outWays, const Grid::LCellIdI&destination, 
							bool ignoreVehiclesOnDestination, bool ignoreFriendlyVehiclesOnRoute)
	{
		if (v->IsVehicle())
		{
			GetWays(world,(const Vehicle*)v, location, outWays, destination,ignoreVehiclesOnDestination,ignoreFriendlyVehiclesOnRoute,false);
			return;
		}
		outWays.Clear();
		#ifndef NO_SATELLITES
			if (v->IsSatellite())
			{
				TOrbitalWays ways;
				GetWays((const Satellite*)v,location.stackID,ways,destination.stackID,ignoreVehiclesOnDestination,ignoreFriendlyVehiclesOnRoute);
				for (index_t i = 0; i < ways.numWays; i++)
				{
					outWays.Add(Grid::LCellIdI(ways.way[i],location.height));
				}
				return;
			}
		#endif

		DBG_FATAL__("Unexpected mobile type used to determine ways");
	}



	static count_t GetConstructionWays(const Grid::WorldData&world, bool ignoreWater, bool flat, bool canGoDownVertically,
										const Vehicle*v,  const Grid::LCellIdI&location, 
										Grid::LCellIdI*outChoices, const Grid::LCellIdI&destination)
	{
		index_t rs = 0;
		const auto&n = Grid::low.grid.GetCell(location.stackID).neighborhood;


		if (flat)
		{
			for (int i = 0; i < n.numNeighbors; i++)
			{
				const Grid::LCellIdI	cell(n.neighborID[i], location.height);

				bool okay = cell.height < Grid::CellStack::HalfRange && cell.height >= -Grid::CellStack::HalfRange;
				if (okay)
				{
					for (int j = cell.height; j > location.height; j--)
						if (!world.LowCellIsClear(Grid::LCellIdI(location.stackID, j)))
						{
							okay = false;
							break;
						}
				}
				if (!okay)
					continue;

				if ( ValidDestinationOf<true>(world,*v,cell, cell == destination, ignoreWater, true) )
				{
					(*outChoices++) = cell;
					rs++;
				}
			}

			return rs;
		}

		for (int j = -1; j <= 1; j++)
		{
			if (!v->CanClimb(j))
				continue;
			for (int i = 0; i < n.numNeighbors; i++)
			{
				const Grid::LCellIdI	cell(n.neighborID[i], location.height + j);

				bool okay = cell.height < Grid::CellStack::HalfRange && cell.height >= -Grid::CellStack::HalfRange;
				if (okay)
				{
					if (j < 0)
					{
						for (int j = cell.height + 1; j <= location.height; j++)
							if (!world.LowCellIsClear(Grid::LCellIdI(n.neighborID[i], j)))
							{
								okay = false;
								break;
							}
					}
					else
						for (int j = cell.height; j > location.height; j--)
							if (!world.LowCellIsClear(Grid::LCellIdI(location.stackID, j)))
							{
								okay = false;
								break;
							}
				}
				if (!okay)
					continue;

				if ( ValidDestinationOf<true>(world,*v,cell, cell == destination, ignoreWater, false) )
//				if ((cell == destination || world.LowCellIsClear(cell)) && (v->CanStandOn(world,cell-1,ignoreWater)))
				{
					(*outChoices++) = cell;
					rs++;
				}
			}
		}

		if (canGoDownVertically)
		{
			static const int j = -1;
			const Grid::LCellIdI	cell(location.stackID, location.height + j);

			bool okay = cell.height < Grid::CellStack::HalfRange && cell.height >= -Grid::CellStack::HalfRange;
			if (okay)
			{
				if (j < 0)
				{
					for (int j = cell.height + 1; j <= location.height; j++)
						if (!world.LowCellIsClear(Grid::LCellIdI(location.stackID, j)))
						{
							okay = false;
							break;
						}
				}
				else
					for (int j = cell.height; j > location.height; j--)
						if (!world.LowCellIsClear(Grid::LCellIdI(location.stackID, j)))
						{
							okay = false;
							break;
						}
			}
			if ( okay && ValidDestinationOf<true>(world,*v,cell, cell == destination, ignoreWater, false) )
//				if ((cell == destination || world.LowCellIsClear(cell)) && (v->CanStandOn(world,cell-1,ignoreWater)))
			{
				(*outChoices++) = cell;
				rs++;
			}
		}
		return rs;

	}




	

	void AccumulateCellMovementCosts(const Grid::WorldData&world, count_t&airCount,count_t&waterCount,count_t&roadCount,const Grid::HCellIdI&cell,count_t multiplier=1)
	{
		const Grid::CellStack::Cell&c = world.cellStack[cell.stackID].GetCellI(cell.height);
		switch (c.content)
		{
			case Grid::Content::Clear:
				airCount += multiplier;
			break;
			case Grid::Content::Water:
				waterCount += multiplier;
			break;
			case Grid::Content::Road:
			case Grid::Content::Foundation:
				roadCount += multiplier;
			break;
		}
	}


	int GetCellMovementCosts(const Grid::WorldData&world, const Grid::LCellIdI&cell, const THindranceVector&hindrance, int vehicleSpeed)
	{
		#ifdef HYBRID_GRID
			const UINT32 high = Grid::low.grid.GetCell(cell.stackID).higherEquivalent;
			const auto&n = Grid::high.grid.GetCell(high).neighborhood;
		#else
			const UINT32 high = cell.cellID;
		#endif

		count_t airCount = 0;
		count_t waterCount = 0;
		count_t roadCount = 0;
		

		AccumulateCellMovementCosts(world, airCount,waterCount,roadCount,Grid::HCellIdI(high,cell.height-1),2);
		#ifdef HYBRID_GRID
			for (int i = 0; i < n.numNeighbors; i++)
				AccumulateCellMovementCosts(world, airCount,waterCount,roadCount,Grid::HCellIdI(n.neighborID[i],cell.height-1));
			count_t total = 2 + n.numNeighbors;
		#else
			count_t total = 2;
		#endif

		count_t rest = total - airCount - waterCount - roadCount;

		return Round(	float(hindrance.road) * float(roadCount) / float(total) + 
						float(hindrance.air) * float(airCount) / float(total) + 
						float(hindrance.water) * float(waterCount) / float(total) + 
						float(hindrance.other) * float(rest) / float(total));


		//float speed = 1.f - hindrance.road * float(roadCount) / float(total)
		///*speed *= 1.f */- hindrance.air * float(airCount) / float(total)
		///*speed *= 1.f */- hindrance.water * float(waterCount) / float(total)
		///*speed *= 1.f */- hindrance.other * float(rest) / float(total);

		//return std::max(1,(int)ceil(  (1.f - speed) * vehicleSpeed));
	}


	bool AppendShortestPath(const Vehicle*vehicle, const Grid::LCellIdI&from,
							const Grid::LCellIdI&to, BasicBuffer<PathNode>&oPath,
							bool ignoreFriendlyVehiclesOnRoute, bool failIfLimitReached/*=true*/, bool*outWouldFailDueToLimitReached/*=nullptr*/);

	bool AppendShortestPathI(const Vehicle*vehicle, const Grid::LCellIdI&from,
							const Grid::LCellIdI&to, BasicBuffer<PathNode>&oPath, bool failIfLimitReached/*=true*/, bool*outWouldFailDueToLimitReached/*=nullptr*/)
	{
		static Buffer0<PathNode>	routeI, route;
		routeI.Clear();
		route.Clear();
		bool hasI = AppendShortestPath(vehicle,from,to,routeI,true,failIfLimitReached,outWouldFailDueToLimitReached);
		if (!hasI)
			return false;
		bool has = AppendShortestPath(vehicle,from,to,route,false,failIfLimitReached,outWouldFailDueToLimitReached);
		//foreach (routeI,r)
		//	ASSERT__(*r != Grid::LCellIdI::Invalid);
		//if (has)
		//	foreach (route,r)
		//		ASSERT__(*r != Grid::LCellIdI::Invalid);
		if (!has)
		{
			oPath.Append(routeI);
			return true;
		}

		count_t lenI = 0, len = 0;
		foreach (routeI,n)
			lenI += n->movementCostsFromPrevNode;
		foreach (route,n)
			len += n->movementCostsFromPrevNode;
		if (len < lenI * 4)	//less than 400% more
		{
			oPath.Append(route);
			return true;
		}

		oPath.Append(routeI);		//else: use the one that ignores friendly vehicles
		return true;

	}

	#ifndef NO_SATELLITES
		bool AppendShortestPath(const Satellite*sat, const Grid::LCellIdI&from,
								const UINT32 to, BasicBuffer<PathNode>&oPath,
								bool ignoreFriendlyVehiclesOnRoute, bool failIfLimitReached/*=true*/, bool*outWouldFailDueToLimitReached/*=nullptr*/);

		bool AppendShortestPathI(const Satellite*sat, const Grid::LCellIdI&from,
								const UINT32 to, BasicBuffer<PathNode>&oPath, bool failIfLimitReached/*=true*/, bool*outWouldFailDueToLimitReached/*=nullptr*/)
		{
			static Buffer0<PathNode>	routeI, route;
			routeI.Clear();
			route.Clear();
			bool hasI = AppendShortestPath(sat,from,to,routeI,true,failIfLimitReached,outWouldFailDueToLimitReached);
			if (!hasI)
				return false;
			bool has = AppendShortestPath(sat,from,to,route,false,failIfLimitReached,outWouldFailDueToLimitReached);
			//foreach (routeI,r)
			//	ASSERT__(*r != Grid::LCellIdI::Invalid);
			//if (has)
			//	foreach (route,r)
			//		ASSERT__(*r != Grid::LCellIdI::Invalid);
			if (!has)
			{
				oPath.Append(routeI);
				return true;
			}

			count_t lenI = 0, len = 0;
			foreach (routeI,n)
				lenI += n->movementCostsFromPrevNode;
			foreach (route,n)
				len += n->movementCostsFromPrevNode;
			if (len < lenI * 4)	//less than 400% more
			{
				oPath.Append(route);
				return true;
			}

			oPath.Append(routeI);		//else: use the one that ignores friendly vehicles
			return true;
		}
	#endif

	PHangar		MobileDestinationStructureHere(const Grid::LCellIdI&location, const MobileEntity&m)
	{
		PStructure s = GetStructure(location);
		return s && s->owner == m.owner ? s->CastToDestinationForType(m) : PHangar();
	}


	bool AppendShortestPath(const Vehicle*vehicle, const Grid::LCellIdI&from,
							const Grid::LCellIdI&to, BasicBuffer<PathNode>&oPath,
							bool ignoreFriendlyVehiclesOnRoute, bool failIfLimitReached/*=true*/, bool*outWouldFailDueToLimitReached/*=nullptr*/)
	{
		bool inRange = true;
		if (outWouldFailDueToLimitReached)
			(*outWouldFailDueToLimitReached) = false;

		const Grid::WorldData&world = 
		#ifdef CLIENT
			Grid::actualWorld;
		#else
			vehicle->owner->currentState;
		#endif

		if (!ValidDestinationOf<false>(world,*vehicle,to,true,false, false))
			return false;

		//if (!vehicle->CanStandOn(world,to -1,false) || 
		//	(!world.LowCellIsClear(to) && !VehicleDestinationStructureHere(to) ))// || !NothingHere(destination))
		//		return false;
	//	Timer::Time t0 = timer.Now();
		const THindranceVector&hindrance = vehicle->base->hindrance;
		const int speed = vehicle->GetMovementStepsPerRound();
		TWays	possibilities;

		static const count_t cellCount = Grid::LowSetup::GridType::CellCount * Grid::CellStack::Range;

		struct TNode
		{
			UINT32	backLink, minDistance;
			int		nodeCosts;
		};

		static Array<TNode>	buffer(cellCount);
		TNode empty = {0xFFFFFFFF,0xFFFFFFFF,0};
		buffer.Fill(empty);

		PriorityQueue<index_t,int>	queue;
		#undef IDOF
		#define IDOF(CELL)	(Grid::CellStack::ToLayer( (CELL).height ) * Grid::LowSetup::GridType::CellCount + (CELL).stackID)
		#undef DESCOF
		#define DESCOF(CELLID)	Grid::LCellIdI((CELLID)%Grid::LowSetup::GridType::CellCount, Grid::CellStack::ToHeight( (UINT32)((CELLID) / Grid::LowSetup::GridType::CellCount)))

		buffer[IDOF(from)].minDistance = 0;

		queue.Push( IDOF(from),0);

		count_t iterations = 0;
		index_t current;
		while (queue.PopLeast(current))
		{
			iterations ++;
			if (iterations > 2000)
			{
				if (failIfLimitReached)	//client sided operation may choose to not implement maximum ranges
				{
					//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
					return false;//let's ignore long ranges
				}
				else
				{
					if (outWouldFailDueToLimitReached)
						(*outWouldFailDueToLimitReached) = true;
					inRange = false;
				}
			}

			Grid::LCellIdI cellID(DESCOF(current));
			TNode&tn = buffer[current];
			if (!tn.nodeCosts)
			{
				tn.nodeCosts = GetCellMovementCosts(world,cellID,hindrance,speed);
			}


			if (cellID == to)
			{
				static Buffer<PathNode>	outPath;
				outPath.Clear();

				while (current < cellCount)
				{
					UINT32 next = buffer[current].backLink;
					int nextCosts = next < buffer.Count() ? buffer[next].nodeCosts : 0;
					//GetCellMovementCosts(next, vehicle);
					int hCosts = next < buffer.Count() ?  std::max(0, DESCOF(current).height - DESCOF(next).height ) : 0;
					outPath << PathNode(DESCOF(current), std::max(nextCosts, buffer[current].nodeCosts) + hCosts);
//					outPath.last().reachedInProcessingLimit = inRange;
					current = next;
				}
				outPath.EraseLast();	//dont need entry point
				outPath.Revert();
				oPath.Append(outPath);
				//ShowMessage("found route with "+String(outPath.Count())+" steps, requiring "+String(iterations)+" iterations" );
				//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
				return true;
			}
			GetWays(world,vehicle,  cellID, possibilities, to, true, ignoreFriendlyVehiclesOnRoute,false);


			for (index_t i = 0; i < possibilities.numWays; i++)
			{
				UINT32 option = IDOF(possibilities.way[i]);
				TNode&n = buffer[option];
				if (!n.nodeCosts)
					n.nodeCosts = GetCellMovementCosts(world,possibilities.way[i],hindrance,speed);
				int cost = std::max(tn.nodeCosts,n.nodeCosts);
				int hCosts = std::max(0, possibilities.way[i].height - cellID.height );
				cost += hCosts;
				if (tn.minDistance + cost < n.minDistance)
				{
					n.minDistance = tn.minDistance + cost;
					n.backLink = (UINT32)current;
					queue.Push(option,n.minDistance);
				}
			}
		}
		//ShowMessage("no route found after "+String(iterations)+" iterations");
		//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
		return false;
	}

	#ifndef NO_SATELLITES
		bool AppendShortestPath(const Satellite*sat, const Grid::LCellIdI&from,
								const UINT32 to, BasicBuffer<PathNode>&oPath,
								bool ignoreFriendlyVehiclesOnRoute
								, bool failIfLimitReached/*=true*/, bool*outWouldFailDueToLimitReached/*=nullptr*/)
		{
			if (outWouldFailDueToLimitReached)
				(*outWouldFailDueToLimitReached) = false;
			const int speed = sat->GetMovementStepsPerRound();
			TOrbitalWays	possibilities;

			static const count_t cellCount = Grid::LowSetup::GridType::CellCount;

			struct TNode
			{
				UINT32	backLink, minDistance;
			};

			static Array<TNode>	buffer(cellCount);
			TNode empty = {0xFFFFFFFF,0xFFFFFFFF};
			buffer.Fill(empty);

			PriorityQueue<UINT32,int>	queue;
			//#undef IDOF
			//#define IDOF(CELL)	((CELL).stackID)
			//#undef DESCOF
			//#define DESCOF(CELLID)	Grid::LCellIdI((CELLID), from.height)

			buffer[from.stackID].minDistance = 0;

			queue.Push( from.stackID,0);

			count_t iterations = 0;
			UINT32 current;
			while (queue.PopLeast(current))
			{
				iterations ++;
				if (iterations > 2000)
				{
					if (failIfLimitReached)
					{
						//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
						return false;//let's ignore long ranges
					}
					else
						if (outWouldFailDueToLimitReached)
							(*outWouldFailDueToLimitReached) = true;
				}

				//Grid::LCellIdI stackID(DESCOF(current));
				TNode&tn = buffer[current];


				if (current == to)
				{
					static Buffer<PathNode>	outPath;
					outPath.Clear();

					while (current < cellCount)
					{
						UINT32 next = buffer[current].backLink;
						int nextCosts = next < buffer.Count() ? 1 : 0;
						//GetCellMovementCosts(next, vehicle);
						outPath << PathNode(Grid::LCellIdI(current,sat->location.height), 1);
						current = next;
					}
					outPath.EraseLast();	//dont need entry point
					outPath.Revert();
					oPath.Append(outPath);
					//ShowMessage("found route with "+String(outPath.Count())+" steps, requiring "+String(iterations)+" iterations" );
					//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
					return true;
				}
				GetWays(sat,  current, possibilities, to, true, ignoreFriendlyVehiclesOnRoute);

				for (index_t i = 0; i < possibilities.numWays; i++)
				{
					UINT32 option = possibilities.way[i];
					TNode&n = buffer[option];
					if (tn.minDistance + 1 < n.minDistance)
					{
						n.minDistance = tn.minDistance + 1;
						n.backLink = current;
						queue.Push(option,n.minDistance);
					}
				}
			}
			//ShowMessage("no route found after "+String(iterations)+" iterations");
			//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
			return false;
		}
	#endif

	void ReachabilityMapper::Map(const Vehicle*vehicle, const Grid::LCellIdI&from, FBlockTest isBlocked, UINT32 maxRange/*=0xFFFFFFFF*/, bool forceRevertible/*=false*/)
	{
		const Grid::WorldData&world = 
		#ifdef CLIENT
			Grid::actualWorld;
		#else
			vehicle->owner->currentState;
		#endif
		//Timer::Time t0 = timer.Now();
		const THindranceVector&hindrance = vehicle->base->hindrance;
		const int speed = vehicle->GetMovementStepsPerRound();
		TWays	possibilities;

		static const count_t cellCount = Grid::LowSetup::GridType::CellCount * Grid::CellStack::Range;


		buffer.SetSize(cellCount);
		TReachableNode empty = {0xFFFFFFFF,0xFFFFFFFF,0,false,InvalidIndex};
		buffer.Fill(empty);

		PriorityQueue<index_t,int>	queue;
		#undef IDOF
		#define IDOF(CELL)	(Grid::CellStack::ToLayer( (CELL).height ) * Grid::LowSetup::GridType::CellCount + (CELL).stackID)
		#undef DESCOF
		#define DESCOF(CELLID)	Grid::LCellIdI((CELLID)%Grid::LowSetup::GridType::CellCount, Grid::CellStack::ToHeight( (UINT32)((CELLID) / Grid::LowSetup::GridType::CellCount)))

		buffer[IDOF(from)].minDistance = 0;

		queue.Push( IDOF(from),0);

		touched.Clear();

		count_t iterations = 0;
		index_t current;
		while (queue.PopLeast(current))
		{
			iterations ++;
			static bool ignoreIterations = false;	//runtime switch
			if (!ignoreIterations && iterations > 1998)
			{
				//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
				break;
			}

			Grid::LCellIdI cellID(DESCOF(current));
			TReachableNode&tn = buffer[current];
			if (!tn.nodeCosts)
			{
				tn.isBlocked = isBlocked && isBlocked(cellID);
				tn.nodeCosts = tn.isBlocked ? 1000000 :
								GetCellMovementCosts(world,cellID,hindrance,speed);
			}
			if (tn.isBlocked)
				continue;

			touched << current;

			GetWays(world,vehicle,  cellID, possibilities, Grid::LCellIdI::Invalid, true, false,forceRevertible);





			for (index_t i = 0; i < possibilities.numWays; i++)
			{
				UINT32 option = IDOF(possibilities.way[i]);
				TReachableNode&n = buffer[option];
				if (!n.nodeCosts)
				{
					n.isBlocked = isBlocked && isBlocked(possibilities.way[i]);
					n.nodeCosts = n.isBlocked ? 1000000 :
									GetCellMovementCosts(world,possibilities.way[i],hindrance,speed);
				}
				if (n.isBlocked)
					continue;

				int cost = std::max(tn.nodeCosts,n.nodeCosts);
				int hCosts = std::max(0, possibilities.way[i].height - cellID.height );
				cost += hCosts;
				if (tn.minDistance + cost < n.minDistance)
				{
					n.minDistance = tn.minDistance + cost;
					n.backLink = (UINT32)current;
					if (n.minDistance < maxRange)
						queue.Push(option,n.minDistance);
				}
			}
		}



		result.clear();
		foreach (touched,t)
		{
			TReachableNode&n = buffer[*t];
			if (n.mapped!=InvalidIndex || !n.minDistance)	//ignore first, and duplicates
				continue;
			n.mapped = result.Count();
			result << MapNode(DESCOF(*t),n.minDistance);
		}

		foreach (touched,t)
		{
			TReachableNode&n = buffer[*t];
			if (n.mapped != InvalidIndex)
			{
				if (n.backLink != 0xFFFFFFFF)
				{
					TReachableNode&pre = buffer[n.backLink];
					result[n.mapped].predecessor = pre.mapped;
					if (pre.mapped == InvalidIndex)
						ASSERT__(DESCOF(n.backLink) == from);
				}
				else
					ASSERT__(DESCOF(*t) == from);
			}
		}


		//ShowMessage("found route with "+String(outPath.Count())+" steps, requiring "+String(iterations)+" iterations" );
		//if (logPerformance)
			//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
	
	}


	static float maxAngleBetweenNeighboringCells = 0.f;


	void InitGrid()
	{
		float max = 0.f;
		for (index_t i = 0; i < Grid::LowSetup::GridType::CellCount; i++)
		{
			const TVec3<>&d0 = Grid::low.cells[i].centerVertex.xyz;

			const auto&n = Grid::low.grid.GetCell(i).neighborhood;
			for (int c = 0; c < n.numNeighbors; c++)
				if (n.neighborID[c] > i)
				{
					const TVec3<>&d1 = Grid::low.cells[n.neighborID[c]].centerVertex.xyz;

					float a = Vec::angle(d0,d1);
					max = std::max(max,a);
				}

		}
		maxAngleBetweenNeighboringCells = max;
	}

	int EstimateMinimumRemainingCost(const Grid::LCellIdI&from, const Grid::LCellIdI&to, int minHorizontalCost, int minVerticalCost)
	{
		const TVec3<>&d0 = Grid::low.cells[from.stackID].centerVertex.xyz;
		const TVec3<>&d1 = Grid::low.cells[to.stackID].centerVertex.xyz;
		float a = Vec::angle(d0,d1);
		int hSteps = (int)floor(a / maxAngleBetweenNeighboringCells);
		int vSteps = abs(from.height - to.height);
		return hSteps * minHorizontalCost + vSteps * minVerticalCost;
	}

	
	bool AppendShortestConstructionPath(const Vehicle*vehicle, bool ignoreWater, bool flat, const Grid::LCellIdI&from, Grid::LCellIdI to, BasicBuffer<Grid::LCellIdI>&oPath)
	{
		if (flat)
			to.height = from.height;
		if (from == to)
			return true;
		const Grid::WorldData&world = 
		#ifdef CLIENT
			Grid::actualWorld;
		#else
			vehicle->owner->currentState;
		#endif
		if (!flat)
			if (!vehicle->CanStandOn(world,to -1,ignoreWater,false) /*|| !Grid::LowCellIsClear(to)*/)// || !NothingHere(destination))
			{
				if (flat)
					to++;
				if (!vehicle->CanStandOn(world,to -1,ignoreWater,false) /*|| !Grid::LowCellIsClear(to)*/)// || !NothingHere(destination))
					return false;
			}

		//Timer::Time t0 = timer.Now();
		Grid::LCellIdI	possibilities[6 * 3 + 2];

		static const count_t cellCount = Grid::LowSetup::GridType::CellCount * Grid::CellStack::Range;

		struct TNode
		{
			UINT32	backLink, minDistance;
		};
		

		static Array<TNode>	buffer(cellCount);
		static Array<bool> flatBuffer(Grid::LowSetup::GridType::CellCount);
		TNode empty = {0xFFFFFFFF,0xFFFFFFFF};
		buffer.Fill(empty);
		flatBuffer.Fill(false);

		PriorityQueue<index_t,int>	queue;
		#undef IDOF
		#define IDOF(CELL)	(Grid::CellStack::ToLayer( (CELL).height ) * Grid::LowSetup::GridType::CellCount + (CELL).stackID)
		#undef DESCOF
		#define DESCOF(CELLID)	Grid::LCellIdI((CELLID)%Grid::LowSetup::GridType::CellCount, Grid::CellStack::ToHeight( (UINT32)((CELLID) / Grid::LowSetup::GridType::CellCount)))

		buffer[IDOF(from)].minDistance = 0;

		queue.Push(IDOF(from),0);

		count_t iterations = 0;
		index_t current;
		while (queue.PopLeast(current))
		{
			iterations ++;
			if (iterations > 2000)
			{
				//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
				return false;//let's ignore long ranges
			}

			Grid::LCellIdI cellID(DESCOF(current));
			TNode&tn = buffer[current];

			flatBuffer[cellID.stackID] = true;
			if (cellID == to)
			{
				static Buffer<Grid::LCellIdI>	outPath;
				outPath.Clear();
				if (!world.LowCellIsClear(to))
				{
					current = buffer[current].backLink;	
				}
				while (current < cellCount)
				{
					UINT32 next = buffer[current].backLink;
					//GetCellMovementCosts(next, vehicle);
					outPath << DESCOF(current);
					current = next;
				}
				outPath.EraseLast();	//dont need entry point
				outPath.Revert();
				oPath.Append(outPath);
				//ShowMessage("found route with "+String(outPath.Count())+" steps, requiring "+String(iterations)+" iterations" );
				//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
				return true;
			}
			count_t ways = GetConstructionWays(world, ignoreWater, flat, cellID == from, vehicle, cellID, possibilities, to);
			ASSERT_LESS_OR_EQUAL__(ways,ARRAYSIZE(possibilities));





			for (index_t i = 0; i < ways; i++)
			{
				if (flatBuffer[possibilities[i].stackID] && possibilities[i].stackID != cellID.stackID)
					continue;
				UINT32 option = IDOF(possibilities[i]);
				TNode&n = buffer[option];
				int cost = possibilities[i].stackID != cellID.stackID ? 1 : 0;
				int hCosts = abs(possibilities[i].height - cellID.height );
				cost += hCosts;

				//if (doFloat && possibilities[i].height != cellID.height)
				//{
				//	//cost ++;
				//	if (
				//		(possibilities[i].height > cellID.height && tn.backLink < buffer.Count() && DESCOF(tn.backLink).height < cellID.height)	//avoid vertical shafts up
				//		||
				//		(possibilities[i].height < cellID.height && tn.backLink < buffer.Count() && DESCOF(tn.backLink).height > cellID.height)	//avoid vertical shafts down
				//		)
				//		cost += 100000;
				//	if (possibilities[i].height > cellID.height && possibilities[i] == to)	//avoid going up to destination
				//		cost += 10000;
				//}

				if (tn.minDistance + cost < n.minDistance)
				{
					n.minDistance = tn.minDistance + cost + EstimateMinimumRemainingCost(possibilities[i],to, 1, 1);
					n.backLink = (UINT32)current;
					queue.Push(option,n.minDistance);
				}
			}
		}
		//ShowMessage("no route found after "+String(iterations)+" iterations");
		//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
		return false;
	}

	
	bool AppendShortestDirectPath(const Grid::LCellIdI&from, const Grid::LCellIdI&to, BasicBuffer<PathNode>&oPath, bool penaltizeVerticalMotion/*=true*/, bool constrainVerticalMotion/*=true*/)
	{
		//Timer::Time t0 = timer.Now();

		Grid::LCellIdI	possibilities[6 * 3 + 2];

		const TIntRange<int> heightRange = IntRange(std::min(from.height,to.height),std::max(from.height,to.height)+1);

		static const count_t cellCount = Grid::LowSetup::GridType::CellCount * Grid::CellStack::Range;

		struct TNode
		{
			UINT32	backLink, minDistance;
		};

		static Array<TNode>	buffer(cellCount);
		TNode empty = {0xFFFFFFFF,0xFFFFFFFF};
		buffer.Fill(empty);

		PriorityQueue<index_t,int>	queue;
		//Queue<index_t>	queue;
		#undef IDOF
		#define IDOF(CELL)	(Grid::CellStack::ToLayer( (CELL).height ) * Grid::LowSetup::GridType::CellCount + (CELL).stackID)
		#undef DESCOF
		#define DESCOF(CELLID)	Grid::LCellIdI((CELLID)%Grid::LowSetup::GridType::CellCount, Grid::CellStack::ToHeight( (UINT32)((CELLID) / Grid::LowSetup::GridType::CellCount)))

		buffer[IDOF(from)].minDistance = 0;

		queue.Push(IDOF(from),0);

		count_t iterations = 0;
		index_t current;
		while (queue.PopLeast(current))
		{
			iterations ++;
			if (iterations > 2000)
			{
				//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
				return false;//let's ignore long ranges
			}

			Grid::LCellIdI cellID(DESCOF(current));
			TNode&tn = buffer[current];


			if (cellID == to)
			{
				static Buffer<PathNode>	outPath;
				outPath.Clear();

				while (current < cellCount)
				{
					UINT32 next = buffer[current].backLink;
					//GetCellMovementCosts(next, vehicle);
					outPath << PathNode(DESCOF(current),1);
					current = next;
				}
				outPath.EraseLast();	//dont need entry point
				outPath.Revert();
				oPath.Append(outPath);
				//ShowMessage("found route with "+String(outPath.Count())+" steps, requiring "+String(iterations)+" iterations" );
				//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
				return true;
			}
			count_t ways = GetAllWays(cellID, possibilities);
			DBG_ASSERT_LESS_OR_EQUAL__(ways,ARRAYSIZE(possibilities));





			for (index_t i = 0; i < ways; i++)
			{
				if (constrainVerticalMotion && !heightRange.Contains(possibilities[i].height))
					continue;
				UINT32 option = IDOF(possibilities[i]);
				TNode&n = buffer[option];
				int cost = 1;
				if (penaltizeVerticalMotion && possibilities[i].height != cellID.height)
				{
					//cost ++;
					if (
						(possibilities[i].height > cellID.height && tn.backLink < buffer.Count() && DESCOF(tn.backLink).height < cellID.height)	//avoid vertical shafts up
						||
						(possibilities[i].height < cellID.height && tn.backLink < buffer.Count() && DESCOF(tn.backLink).height > cellID.height)	//avoid vertical shafts down
						)
						cost += 100000;
					if (possibilities[i].height > cellID.height && possibilities[i] == to)	//avoid going up to destination
						cost += 10000;
				}
				if (tn.minDistance + cost < n.minDistance)
				{
					n.minDistance = tn.minDistance + cost;
					n.backLink = (UINT32)current;
					queue.Push(option,n.minDistance);
				}
			}
		}
		//ShowMessage("no route found after "+String(iterations)+" iterations");
		//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
		return false;
	}

	
	bool AppendShortestDirectSatellitePath(const Grid::LCellIdI&from, const UINT32 to, BasicBuffer<PathNode>&oPath)
	{
		//Timer::Time t0 = timer.Now();

		UINT32	possibilities[6];

		static const count_t cellCount = Grid::LowSetup::GridType::CellCount;

		struct TNode
		{
			UINT32	backLink, minDistance;
		};

		static Array<TNode>	buffer(cellCount);
		TNode empty = {0xFFFFFFFF,0xFFFFFFFF};
		buffer.Fill(empty);

		PriorityQueue<UINT32,int>	queue;
		//Queue<index_t>	queue;

		buffer[from.stackID].minDistance = 0;

		queue.Push(from.stackID,0);

		count_t iterations = 0;
		UINT32 current;
		while (queue.PopLeast(current))
		{
			iterations ++;
			if (iterations > 2000)
			{
				//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
				return false;//let's ignore long ranges
			}

			TNode&tn = buffer[current];


			if (current == to)
			{
				static Buffer<PathNode>	outPath;
				outPath.Clear();

				while (current < cellCount)
				{
					UINT32 next = buffer[current].backLink;
					//GetCellMovementCosts(next, vehicle);
					outPath << PathNode(Grid::LCellIdI(current,from.height),1);
					current = next;
				}
				outPath.EraseLast();	//dont need entry point
				outPath.Revert();
				oPath.Append(outPath);
				//ShowMessage("found route with "+String(outPath.Count())+" steps, requiring "+String(iterations)+" iterations" );
				//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
				return true;
			}
			count_t ways = GetAllSatelliteWays(current, possibilities);
			DBG_ASSERT_LESS_OR_EQUAL__(ways,ARRAYSIZE(possibilities));





			for (index_t i = 0; i < ways; i++)
			{
				UINT32 option = possibilities[i];
				TNode&n = buffer[option];
				int cost = 1;
				if (tn.minDistance + cost < n.minDistance)
				{
					n.minDistance = tn.minDistance + cost;
					n.backLink = (UINT32)current;
					queue.Push(option,n.minDistance);
				}
			}
		}
		//ShowMessage("no route found after "+String(iterations)+" iterations");
		//LogEvent(EventSignificance::PerformanceMeasurement,nullptr,__func__": Time needed for "+String(iterations)+" iterations: "+String(timer.GetSecondsSince(t0)));
		return false;
	}


	


	PVehicle		SpawnVehicle(const OwnerParameter&owner, const Grid::LCellIdI&baseLocation,Component::VehicleBase&base, Component::Turret&turret, bool fillStorage, float&atTime)
	{
		return SpawnVehicle(owner,baseLocation,&base,&turret,fillStorage,atTime);
	}

	PVehicle SpawnVehicle(const OwnerParameter&owner, const Grid::LCellIdI&baseLocation,Component::VehicleBase*base, Component::Turret*turret, bool fillStorage, float&atTime)
	{
		PVehicle vehicle(new Vehicle(owner, baseLocation,base,turret));
		#ifdef CLIENT
			vehicle->location = baseLocation;
			ASSERT3__(vehicle->preAnimationLocation == vehicle->location,vehicle->preAnimationLocation.ToString(),vehicle->location.ToString(),vehicle->GetName());
			//vehicle->preAnimationLocation = vehicle->location;
		#else
			vehicle->health = vehicle->GetMaxHealth();	//or else PositionOnTop will fail
			vehicle->PositionOnTop(baseLocation,false);
		//	vehicle->UpdateSystem();
		#endif
		#ifndef ONE_COMPOUND_PER_FACTION
			if (fillStorage)
				vehicle->currentStorage = vehicle->GetMaxStorageCapacity();
		#endif
		InsertConstructed(vehicle,atTime);
		return vehicle;
	}


	#ifdef CLIENT
		PVehicle		SpawnVehicle(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&baseLocation,ConstName base, ConstName turret, bool placeNow, bool isOwnedByMe)
	#else
		PVehicle		AssembleVehicle(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&baseLocation, const String&base, const String&turret)
	#endif
	{
		Component::Turret* t = Component::GetTurretByName(turret);
		Component::VehicleBase* b = Component::GetVehicleBaseByName(base);
		ASSERT__(b);
		ASSERT__(t);
		PVehicle vehicle(new Vehicle(owner, baseLocation,b,t));
		vehicle->location = baseLocation;
		vehicle->id = id;
		vehicle->UpdateSystem();
		#ifdef CLIENT
			vehicle->UpdateNiceName();
			vehicle->isOwnedByMe = isOwnedByMe;
			if (placeNow)
				Place(vehicle);
			//else
			//	unplacedIDMap.Set(id,vehicle);
		#endif
		return vehicle;
	}



	#ifndef CLIENT
		PMobileEntity	AssembleMobileEntity(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&baseLocation, const String&base, const String&turret)
		{
			if (turret.IsNotEmpty())
				return AssembleVehicle(owner,id,baseLocation,base,turret);
			
			PWorldEntity	proto = GetNonVehiclePrototypeByName(base);
			if (!proto)
				return PMobileEntity();
			ASSERT1__(proto,base);
			ASSERT1__(proto->IsMobile(),proto->GetTypeName());
			PMobileEntity s = To<MobileEntity>::Cast( proto->CreateNewInstance(owner,baseLocation,0) );
			s->location = baseLocation;
			s->id = id;

			if (baseLocation.height < Grid::CellStack::HalfRange)
				s->TranslateLocationFromExport();
			s->UpdateSystem();
			return s;
		}

	#else

		PMobileEntity		SpawnMobileEntity(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&baseLocation,ConstName base, ConstName turret, bool placeNow, bool isOwnedByMe)
		{
			if (turret != ConstName::Empty)
				return Objects::SpawnVehicle(owner,  id,baseLocation,base,turret,placeNow,isOwnedByMe);
			return Objects::SpawnNonVehicleMobile(owner,  id,baseLocation,base,placeNow,isOwnedByMe);
		}

		PMobileEntity			SpawnNonVehicleMobile(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&baseLocation,ConstName type, bool placeNow, bool isOwnedByMe)
		{
			PWorldEntity	proto = GetNonVehiclePrototypeByName(type);
			ASSERT1__(proto,(int)type);
			ASSERT1__(proto->IsMobile(),proto->GetTypeName());
			PMobileEntity s = To<MobileEntity>::Cast( proto->CreateNewInstance(owner,baseLocation,0) );
			s->location = baseLocation;
			s->id = id;
			s->UpdateSystem();
			//ASSERT_GREATER_OR_EQUAL1__(baseLocation.height, Grid::CellStack::HalfRange,s->GetName());
			s->UpdateNiceName();
			s->isOwnedByMe = isOwnedByMe;
			if (placeNow)
				PlaceMobileEntity(s);
			return s;
		}

		PStructure		SpawnStructure(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&location,ConstName type, bool placeNow, bool isOwnedByMe)
		{
			foreach (structurePrototypes,p)
				if ((*p)->GetCTypeName() == type)
				{
					//PStructure s = (*p)->CreateNewInstance(location);
					PStructure s = To<Structure>::Cast((*p)->CreateNewInstance(owner,location,0));
					s->health = s->GetMaxHealth();
					s->id = id;
					s->UpdateNiceName();
					s->isOwnedByMe = isOwnedByMe;
					//LogEvent(EventSignificance::Common, s.get(),"Constructed " + s->GetName());
					if (placeNow)
					{
						Place(s);
					}
					//else
					//	unplacedIDMap.Set(id,s);


					return s;
				}
			FATAL__("Type not found for construction #" + String((int)type));
			return PStructure();
		}
		void			Place( const PStructure&s)
		{
			float t = 0;
			InsertConstructed(s,t,false,false);
		}
		void			Place( const PVehicle&vehicle, bool insertIntoGrid/*=true*/)
		{
			ASSERT__(vehicle);
			ASSERT3__(vehicle->preAnimationLocation == vehicle->location,vehicle->preAnimationLocation.ToString(),vehicle->location.ToString(),vehicle->GetName());
			float t = 0;
			//vehicle->preAnimationLocation = vehicle->location;
			InsertConstructed(vehicle,t,insertIntoGrid,false);
			ASSERT1__(vehicles.Contains(vehicle),vehicle->GetName());
		}
		#ifndef NO_SATELLITES
			void			Place( const PSatellite&s)
			{
				ASSERT__(s);
				ASSERT3__(s->preAnimationLocation == s->location,s->preAnimationLocation.ToString(),s->location.ToString(),s->GetName());
				float t = 0;
				InsertConstructed(s,t);
				ASSERT1__(satellites.Contains(s),s->GetName());

				if (s->IsSkyHub())
					To<SkyHub>::Cast(s)->UpdateConnections();
			}
		#endif

		void			PlaceMobileEntity( const PMobileEntity&m, bool insertIntoGridIfVehicle/*=true*/)
		{
			ASSERT__(m);
			ASSERT3__(m->preAnimationLocation == m->location,m->preAnimationLocation.ToString(),m->location.ToString(),m->GetName());
			float t = 0;
			InsertConstructed(m,t,insertIntoGridIfVehicle);
			#ifndef NO_SATELLITES
				if (m->IsSatellite())
				{
					ASSERT1__(satellites.Contains(m),m->GetName());
				}
				else
			#endif
				if (m->IsVehicle())
				{
					ASSERT1__(vehicles.Contains(m),m->GetName());
				}
				else
					FATAL__("Unsupported mobile entity type at this point: "+m->GetName());
		}


	#endif




	void					RemoveVehicleFromGrid(const PVehicle&v,const Grid::LCellIdI&location)
	{
		RemoveFromGrid(vehicleGrid,location,v);
	}
	void					InsertVehicleIntoGrid(const PVehicle&v,const Grid::LCellIdI&location)
	{
		InsertIntoGrid(vehicleGrid,location,v);
	}


	#ifdef CLIENT
		void					MoveGridVehicle(const PVehicle&v, const Grid::LCellIdI&from, const Grid::LCellIdI&to)
		{
			RemoveFromGrid(vehicleGrid,from,v);
			InsertIntoGrid(vehicleGrid,to,v);
		}
	#else
		bool					ProcessChoiceIntersection(const PWorldEntity&e, const Grid::LCellIdI&choice,  Vehicle::mode_t mode, const PMobileEntity&deployWhat)
		{
			Buffer0<Choice>	movementChoices;
			bool showArrow, hasArrowCellTarget;
			TVec3<>arrowFrom;
			Grid::LCellIdI arrowFromCell;
			GetChoices(e,deployWhat,mode,movementChoices,showArrow,hasArrowCellTarget,arrowFrom,arrowFromCell);

			if (!movementChoices.Contains(choice))
				return false;


			PMobileEntityProducer p = To<MobileEntityProducer>::DynamicCast(e);
			//e->ToProducer();
			if (e->AwaitsUserInteraction() && p)
			{
				auto c = choice;
				float t= 0;
				PMobileEntity v = p->CompleteProduction(t);
				if (!v)
				{
					return false;
				}
				MobilePlacementResult result = v->PlaceExactlyAt(c,0,0,false,false);
				if (Failed(result))
				{
					FATAL__(v->GetName()+": Failed to position non-constructed at "+c.ToString());
				}
				if (result == MobilePlacementResult::PlacementOkay || result == MobilePlacementResult::PlacementOkayFellDown)
				{
					float t = 0;
					InsertConstructed(v,t);
				}

				if (e->IsStructure())
				{
					PStructure s = To<Structure>::Cast(e);
					s->GetCompound().RefreshState(0);
				}
			}
			else
			{
				PMobileEntity v = To<MobileEntity>::DynamicCast(e);
				if (v && !deployWhat)
				{
					auto c = choice;
					switch (mode)
					{
						#ifndef ONE_COMPOUND_PER_FACTION
						case CursorMode::DepositMode:
						case CursorMode::RestockMode:
						{
							PStructure target = GetStructure(c);
							PMobileEntity vTarget = GetMobileEntity(c);
							if ((target && target->owner == e->owner) || (vTarget && vTarget->owner == e->owner))
							{
								PVehicleJob job(new VehicleJob::TransferJob(*v,mode));
								v->RegisterNewJob("", job,&c);
							}
						}
						break;
						#endif
						case CursorMode::RepairMode:
						{
							//PWorldEntity target = GetEntity(c);
							//if (target && target->health < target->GetMaxHealth() && target != v && v->CanRepair())
							//for consistency reasons with journal-saves no check here. just put it in there, and have the job figure out if it's good to execute
							{
								PVehicleJob job(new VehicleJob::RepairJob(*v));
								v->RegisterNewJob("", job,&c);
							}
						}
						break;
						case CursorMode::ClearAboveMode:
						{
							if (!v->GetOperationSpeed(Component::TerrainRemove))
								return false;
							PVehicleJob job(new VehicleJob::ClearAbove(*v));
							v->RegisterNewJob("", job,&c);
						}
						break;
						case CursorMode::DigMode:
						{
							if (!v->GetOperationSpeed(Component::TerrainRemove))
								return false;

							auto copy = c;
							PVehicleJob job(new VehicleJob::DigHorizontal(*v));
							//std::weak_ptr<Vehicle> v2 = v;
							v->RegisterNewJob("", job, &c);
							return true;
						}
						break;
						case CursorMode::Idle:
						{
						}
						break;
					}
				}
				else
				{
				
					//PWorkingStructure ws = To<WorkingStructure>::DynamicCast(e);

					{
						if (deployWhat)
						{
							PHangar s = To<Hangar>::DynamicCast(e);
							PHangar actual = deployWhat->storedIn.lock();
							//PVehicleStore actualStore = To<VehicleStore>::DynamicCast(actual);
							if (s && s->SharesStorageWith(actual))
							{
								float t = 0.f;
								actual->LocalCheckout(deployWhat,choice,t);
							}
							else
							{
								if (s)
								{
									if (deployWhat)
									{
										if (!actual)
											LogEvent(EventClass::Preset::fatal,&s->ToEntity(),"Attempting to re-checkout "+deployWhat->GetName());
										else
											if (!s->SharesStorageWith(actual))
											{
												LogEvent(EventClass::Preset::fatal,&s->ToEntity(),"Unable to checkout "+deployWhat->GetName()+" from this store: stored in a different park");
											}
											else
												FATAL__("Equation evaluation error. Did the compiler fuck up?");
									}
									else
									{
										LogEvent(EventClass::Preset::fatal,&s->ToEntity(),"Nothing to deploy (reference is null)");
									}
								}

								#ifdef _DEBUG
									FATAL__("unexpected choice operation");
								#endif
								return false;
							}
						}
						else
						{
							FATAL__("deployWhat must be set when checking out from "+e->GetName());
						}
					}
				}
			}
			return true;
				//FATAL__("unexpected");
		}
	#endif
}

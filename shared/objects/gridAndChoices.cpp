#include <global_root.h>
#include "gridAndChoices.h"
#include "interface.h"
#include "prototypes.h"
#include "vehicle.h"
#include "satellite.h"
#include "faction.h"
#include "structure/tower.h"
#ifndef CLIENT
	#include "../runtimeConfig.h"
#endif

namespace Objects
{
	Ctr::Array<PVehicle>			vehicleGrid(Grid::LowSetup::GridType::CellCount * Grid::CellStack::Range);
	Ctr::Array<PStructure>		structureGrid(Grid::LowSetup::GridType::CellCount * Grid::CellStack::Range);


	




	#ifdef CLIENT

		UINT32		uplinkTraffic = 0,
					onlineUplinkTraffic = 0;


	#else



		void		VerifyGrid()
		{
			ForeachWorldEntity([](const Objects::PWorldEntity&e)
			{
				if (e->IsAboveGrid())
					return;
				if (e->IsMobile())
				{
					const PMobileEntity found = GetMobileEntity(e->location);
					if (found != e)
					{
						const String foundName = found ? (found->GetTypeName() + ' '+ found->GetName()) : "null";
						FATAL__("Expected to find "+e->GetTypeName()+" "+e->GetName()+" in location "+ToString(e->location)+", but found "+foundName);
					}
				}
				elif (e->IsStructure())
				{
					const PStructure found = GetStructure(e->location);
					if (found != e)
					{
						const String foundName = found ? (found->GetTypeName() + ' '+ found->GetName()) : "null";
						FATAL__("Expected to find "+e->GetTypeName()+" "+e->GetName()+" in location "+ToString(e->location)+", but found "+foundName);
					}
				}
			});
		}








		bool		GetSentryTarget(WorldEntity&source, int munitionStorage, const TrajectoryEndPoint&sourceLocation, const TurretBase&base, TurretBase::TargetChoice& outTarget)
		{
			float c2 = std::numeric_limits<float>::max();
			Grid::LCellIdI closest = Grid::LCellIdI::Invalid;
			PWorldEntity closestEntity;
			if (base.turret->GetAmmoNeedPerShot(source.owner) > munitionStorage || base.turret->dealsEMPDamage || base.turret->isDoomsday)
				return false;
			const World::Data&world = source.owner->knownWorld;

			Buffer0<PWorldEntity>	candidates;

			source.owner->VisitVisibleForeignEntities([&source,&candidates](const PWorldEntity&e)
			{
				if (e->IsIndestructible() || source.owner->IsAlliedWith(e->owner) || Grid::AffectedByEMP(e->location))
					return;
				candidates << e;
			});

			std::random_shuffle(candidates.begin(),candidates.end(),[&source](count_t range)->index_t
			{
				return source.entityRandom.NextIndex(range);
			});

			foreach (candidates,c)
			{
				auto e = *c;
				ASSERT__(e->isInserted);
				Grid::LCellIdI loc;
				if (base.ExtCanHit(source.owner,sourceLocation,TrajectoryEndPoint( e->location ),outTarget))
				{
					LogEvent(EventSignificance::Debug,&source,"Auto-targeting "+e->GetName()+" (located at "+ToString(e->location)+") via closest viable trajectory target "+ToString(outTarget.cellTarget));
					LogEvent(EventClass( EventSignificance::Common, EventFlavor::Neutral),&source,"Auto-targeting "+e->GetNiceName());
					return true;
				}
			};
			return false;
		}

		const ApplicationInterface::TAttack*	currentExplosionAttack = nullptr;

		
		void Explode( const ApplicationInterface::TAttack& attack, 
					  const Grid::HCellIdI&targetCell, const M::TVec3<>&center, int explosionRadius, int damageLevel, bool doomsday,
					  float&atTime)
		{
			LogEvent(EventSignificance::Common, attack.attackFrom.get(),"Handling regular explosion at "+ToString(center)+", radius="+String(explosionRadius));

			if (doomsday && RuntimeConfig::doomsdayWeapons == RuntimeConfig::DoomsdayWeapons::Disallow)
			{
				LogEvent(EventClass::Preset::commandRejected,nullptr,"Unable to process doomsday explosion: disallowed");
				return;
			}

			DBG_ASSERT__(attack.attackFrom);
			currentExplosionAttack = &attack;

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
			static Ctr::Buffer0<PWorldEntity>	allLivingAffected;
			static Ctr::Buffer0<PStructure>		affectedStructures;
			static Ctr::Buffer0<PVehicle>		affectedVehicles;
			ForeachWorldEntity([center,outerRadius,damageLevel,&atTime,attack](const PWorldEntity e)
			{
				if (e->IsIndestructible())
					return;
				float dist2 = std::numeric_limits<float>::max(),
						dist2IncludingBase = dist2;
				const int h = e->GetHeight();
				const M::TVec3<>&c = Grid::low.cells[e->location.stackID].cornerCenter;
				M::TVec3<> p;
				for (int k = 0; k <= h; k++)
				{
					Vec::mult(c,Grid::ToRadius(e->location.height + k),p);
					const float d2 = Vec::quadraticDistance(p,center);
					dist2 = std::min(dist2,d2);
				}
				dist2IncludingBase = dist2;
				{
					Vec::mult(c,Grid::ToRadius(e->location.height - 1),p);
					const float d2 = Vec::quadraticDistance(p,center);
					dist2IncludingBase = std::min(dist2IncludingBase,d2);
				}
				if (e->IsStructure())
				{
					const auto&n = Grid::low.grid.GetCell(e->location.stackID).neighborhood;
					for (int k = 0; k < n.numNeighbors; k++)
					{
						p = Grid::ToWorld(Grid::LCellIdI(n.neighborID[k],e->location.height-1));	//include lower level of neighbors
						float d3 = Vec::quadraticDistance(p,center);
						dist2IncludingBase = std::min(dist2IncludingBase,d3);
					}
				}


				if (dist2IncludingBase > outerRadius)
					return;
				//if (d2 < coreRadius)
				//{
				//	toDestroy << *e;
				//}
				//else
				if (dist2 <= outerRadius)
				{
					LogEvent(EventSignificance::Debug,e.get(),"Caught in explosion radius with dist="+String(sqrt(dist2))+", health change "+String(e->health)+"->"+String(e->health-damageLevel));

					if (attack.attackFrom)
					{
						int d = M::Min(damageLevel,e->health);
						attack.attackFrom->owner->statistics.SignalDamageDealtTo(attack.attackFrom,e->owner,d);
						e->owner->statistics.SignalDamageReceivedFrom(attack.attackFrom->owner,d);
					}


					e->health -= damageLevel;
					if (e->IsDeadOrGhost())
					{
						if (attack.attackFrom)
						{
							attack.attackFrom->owner->statistics.SignalDestructionOfHostile(attack.attackFrom,e);
							e->owner->statistics.SignalLossOf(e,attack.attackFrom->owner,false);
						}

						e->SyncTime(atTime);
						Remove(e,Objects::RemovalType::UncontrolledExplosion,atTime);
						return;	//prevent being added to affected*
					}
					else
					{
						e->SignalChangeByExternalCause(WorldEntity::HealthChanged,atTime);
					}
				}
				allLivingAffected << e;


				if (e->IsVehicle())
				{
					affectedVehicles << To<Vehicle>::Cast(e);
				}
				else
				if (e->IsStructure())
				{
					affectedStructures << To<Structure>::Cast(e);
				}
			});
			atTime += timeEpsilon;




			if (terrainExplosionRadius > 0)
			{
				World::actual.Explode(targetCell,terrainExplosionRadius, atTime);
				if (doomsday && RuntimeConfig::doomsdayWeapons == RuntimeConfig::DoomsdayWeapons::Expose)
					foreach (Faction::all,f)
					{
						bool changed[BaseGrid::NumFamilies] = {false};
						(*f)->CheckSensory();
						(*f)->RegisterExplosionCrater(targetCell,terrainExplosionRadius,atTime);
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
					v->CheckGround(atTime);	//in case they fall due to explosions
					if (v->IsDeadOrGhost())
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

			currentExplosionAttack = nullptr;
		}
	
		void ExplodeEMP(const ApplicationInterface::TAttack& attack, 
						const Grid::HCellIdI&targetCell, const M::TVec3<>&center, int explosionRadius, int damageLevel, float&atTime)
		{
			LogEvent(EventSignificance::Common, attack.attackFrom.get(),"Handling EMP explosion at "+ToString(center)+", radius="+String(explosionRadius));
			ApplicationInterface::SignalExplosion(
							attack, 
							center,explosionRadius,true,false,atTime);

			float outerRadius = explosionRadius*explosionRadius;
			static Ctr::Buffer0<PWorldEntity>	toDisable;

			currentExplosionAttack = &attack;


			ForeachWorldEntity([center,outerRadius,damageLevel,&atTime,attack](const PWorldEntity e)
			{
				float d2 = std::numeric_limits<float>::max();
				const int h = e->GetHeight();
				const M::TVec3<>&c = Grid::low.cells[e->location.stackID].cornerCenter;
				M::TVec3<> p;
				for (int k = 0; k <= h; k++)
				{
					Vec::mult(c,Grid::ToRadius(e->location.height + k),p);
					float d3 = Vec::quadraticDistance(p,center);
					d2 = std::min(d2,d3);
				}
				if (d2 <= outerRadius)
				{
					if (attack.attackFrom)
					{
						int newDamage = std::max(e->empDamageLevel,damageLevel);
						int dealt = newDamage - e->empDamageLevel;
						attack.attackFrom->owner->statistics.SignalEMPDamageDealtTo(attack.attackFrom,e->owner,dealt);
						e->owner->statistics.SignalEMPDamageReceivedFrom(attack.attackFrom->owner,dealt);
					}


					atTime = std::max(atTime,e->entityTime);
					toDisable << e;
				}
			});
			atTime += timeEpsilon;
			foreach (toDisable,_d)
			{
				PWorldEntity d = *_d;
				d->SyncTime(atTime);
				d->HandleEMPDamage(damageLevel, atTime);
			}
			toDisable.Clear();

			currentExplosionAttack = nullptr;
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
			int lower = M::Clamp(cellID.height - 1, Grid::CellStack::BottomHeight, Grid::CellStack::TopHeight),
				upper = M::Clamp(cellID.height + 1, Grid::CellStack::BottomHeight, Grid::CellStack::TopHeight);
			for (int h = lower; h <= upper; h++)
			{
				UINT32 layer = Grid::CellStack::ToLayer(h);
				for (int j = 0; j < n.numNeighbors; j++)
				{
					PStructure s = GetStructure(Grid::LCellIdI(n.neighborID[j],h));
					if (!s || s->location.height != h)//since buildings can be any height, and are now registered multiple times, they can be encountered several times here 
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
			structures.Foreach([referenceOwner,cellID,n,&outNeighborhood](const PStructure&s)
			{
				//LogEvent(EventSignificance::Debug,s->get(),"Testing as neighbor of cell "+cellID.ToString()+" local position is "+(*s)->location.ToString());
				if (
						#ifndef CLIENT
							!ignoreOwnership && 
							!(*s)->ConnectsToDifferentOwnership() &&
						#endif
						s->owner != referenceOwner 
					)
				{
					//LogEvent(EventSignificance::Debug,s->get(),"Rejected due to ownership: "+referenceOwner.ToString()+" != "+(*s)->owner.ToString());
					return;
				}

				if (_IsNeighbor(cellID, n, s->location))
					outNeighborhood.Append(s);
				//else
					//LogEvent(EventSignificance::Debug,s->get(),"Rejected due to location");
			});
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
				|| !v->owner->CanSee(vh,v->entityTime)
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
		void				RemoveFromGrid(Ctr::Array<T>&grid, const Grid::LCellIdI&location, const T&object)
		{
			ASSERT__(object->isInsertedIntoGrid);

			const int h = object->GetHeight();

			for (int z = 0; z < h; z++)
			{
				index_t at = CellToGridIndex(location+z);
				if (grid[at] != object)
				{
					index_t foundAt = InvalidIndex;
					index_t cloneAt = InvalidIndex;
					T	cloneIs;
					for (index_t i = 0; i < grid.GetLength(); i++)
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
					String clone = cloneAt != InvalidIndex ? " {found clone ("+cloneIs->GetName()+") at "+ToString(Grid::LCellIdI(cloneAt%Grid::LowSetup::GridType::CellCount,Grid::CellStack::ToHeight((UINT32)( cloneAt/Grid::LowSetup::GridType::CellCount))))+"}" : "";
					if (foundAt != InvalidIndex)
					{
						Grid::LCellIdI actualLocation(foundAt%Grid::LowSetup::GridType::CellCount,Grid::CellStack::ToHeight((UINT32)( foundAt/Grid::LowSetup::GridType::CellCount)));
						FATAL__("Trying to remove "+object->GetName()+" from location "+String(at)+"/"+ToString(location)+", but object resides in "+String(foundAt)+"/"+ToString(actualLocation)+clone);
					}
					else
						FATAL__("Trying to remove "+object->GetName()+" from location "+String(at)+"/"+ToString(location)+", but object is not mapped in the grid"+clone);
				}
				LogEvent(EventSignificance::Common,object.get(),"Removed from grid location "+String(at)+"/"+ToString(location));
				grid[at].reset();
			}
			object->isInsertedIntoGrid = false;
		}
	template <typename T>
		void				InsertIntoGrid(Ctr::Array<T>&grid, const Grid::LCellIdI&location, const T&object)
		{
			ASSERT__(!object->isInsertedIntoGrid);
			Grid::AssertIsValid(location,CLOCATION);

			const int h = object->GetHeight();
			if (h > 1)
				Grid::AssertIsValid(location+h-1,CLOCATION);
			for (int z = 0; z < h; z++)
			{
				index_t at = CellToGridIndex(location+z);
				if (grid[at])
				{
					FATAL__("Trying to register "+object->GetName()+"(@"+ToString(object->location)+",z="+String(z)+") at "+ToString(location)+": Cell occupied by "+grid[at]->GetName()+"(@"+ToString(grid[at]->location)+")");
				}
				ASSERT3__(!grid[at],grid[at]->GetName(),location,object->GetName());
				LogEvent(EventSignificance::Debug,object.get(),"Inserted into grid location "+String(at)+"/"+ToString(location));
				grid[at] = object;
			}
			object->isInsertedIntoGrid = true;
		}
	template <typename T>
		T					QueryGrid(const Ctr::Array<T>&grid, const Grid::LCellIdI&location)
		{
			return grid[CellToGridIndex(location)];
		}
	template <typename T>
		T					QueryGrid(const Ctr::Array<T>&grid, const Grid::LCellIdU&location)
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


	bool	CanDeployHere( const Grid::LCellIdI&cell, const PMobileEntity&entity, const OwnerParameter&owner )
	{
		if (Grid::AffectedByEMP(cell))
			return false;
		auto h = AccessibleHangarHere(cell,*entity, owner);
		if (h && h->ToEntity().location == cell)
			return h->GetLoad() < h->GetMaximumLoad();
		if (!World::actual.LowCellIsClear(cell) || !NothingBlockingHere(cell,true))
			return false;
		if (cell.height > Grid::CellStack::TopHeight)
		{
			int rs = 0;
			return entity->CanSpawnAboveGrid(rs) && rs == cell.height;	//must NOT go into CanStandOn() when above grid. That makes no sense. So return here, whatever the result
		}
		else
			if (!entity->CanSpawnInGrid())
				return false;
		return !entity->IsVehicle() || To<Vehicle>::Cast(entity)->CanStandOn(World::actual,cell-1,false,false);
	}

	void	GetProductionChoices( const WorldEntity*producer,const PVehicle&production,
									Ctr::BasicBuffer<Choice>&choices)
	{
		choices.Clear();	
		if (!production)
			return;
		const auto&n = Grid::low.grid.GetCell(producer->location.stackID).neighborhood;

		for (int j =  -1; j <= 1 + producer->GetHeight(); j++)
		{
			const int h = producer->location.height + j;
			if (h < Grid::CellStack::BottomHeight || h > Grid::CellStack::TopHeight)
				continue;
			if (!production->CanClimb(j))
				continue;
			for (int i = 0; i < n.numNeighbors; i++)
			{
				const Grid::LCellIdI	cell(n.neighborID[i], h);


				bool okay = cell.height < Grid::CellStack::HalfRange;
				if (okay)
				{
					if (j < 0)
					{
						for (int j = cell.height + 1; j <= producer->location.height; j++)
						{
							const auto candidate = Grid::LCellIdI(n.neighborID[i], j);
							if (World::actual.LowCellIsClear(candidate))
								continue;
							auto hangar = AccessibleHangarHere(candidate,*production, producer->owner);
							if (!hangar)
							{
								okay = false;
								break;
							}
						}
					}
					else
						for (int j = cell.height; j > producer->location.height; j--)
						{
							if (j < producer->location.height + producer->GetHeight())
								continue;
							const auto candidate = Grid::LCellIdI(producer->location.stackID, j);
							if (World::actual.LowCellIsClear(candidate))
								continue;
							auto hangar = AccessibleHangarHere(candidate,*production, producer->owner);
							if (!hangar)
							{
								okay = false;
								break;
							}
						}
				}
				if (!okay)
					continue;

				if (CanDeployHere(cell,production,producer->owner))
				{
					choices << Choice(cell,0);
				}
			}
		}

	}


	
	void		GetChoices(WorldEntity&entity, const PMobileEntity&deploying, CursorMode activeCursorMode, Ctr::BasicBuffer<Choice>&movementChoices, TChoiceVisualization&vis)
	{
		const World::Data&world = 
		#ifdef CLIENT
			World::actual;
		#else
			entity.owner->knownWorld;
		#endif
		vis = TChoiceVisualization();
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
					GetProductionChoices(&entity,To<Vehicle>::DynamicCast( building.GetPrototype() ),movementChoices);

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
								if (CanDeployHere(candidate,deploying,entity.owner))
									movementChoices << Choice(candidate,0);
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
							if (CanDeployHere(candidate,deploying,entity.owner))
								movementChoices << Choice(candidate,0);
						}
					else
					{
						const Grid::LCellIdI candidate(location.stackID,h);
						if (CanDeployHere(candidate,deploying,entity.owner))
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
			vis.showArrow = CursorModeExpectsLeftClick(activeCursorMode);
			if (vis.showArrow)
			{
				vis.arrowFromCell = v ? v->GetLastPointOfOrigin() : entity.location;
				vis.arrowFrom = TrajectoryEndPoint(vis.arrowFromCell,h);
				if (activeCursorMode == CursorMode::AttackMode)
				{
					vis.showTrajectory = true;
					vis.paraboloidTrajectory = b->turret->trajectoryType == TrajectoryType::Artillery;
					vis.flatTrajectoryRange = b->GetFlatAttackRange();
				}

			}	
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
						//const auto&c = Grid::low.grid.GetCell(location.stackID);
						for (int h = location.height -1; h <= location.height+1; h++)
						{
							if (h >= -Grid::CellStack::HalfRange && h < Grid::CellStack::HalfRange-1)
							{
								for (int j = 0; j < n.numNeighbors; j++)
								{
									const Grid::LCellIdI candidate(n.neighborID[j],h);
									PVehicle other = GetVehicle(candidate);
									PStructure otherStructure = GetStructure(candidate);
									if ((otherStructure && otherStructure->owner==v->owner) || (other && other.get() != v && other->owner == v->owner))
										movementChoices << (candidate+1);
								}
							}
						}
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

	
	int		FindApplicableGround( const World::Data&world, const Grid::LCellIdI&cell, bool canStandOnWater, bool canStandOnLava)
	{
		#ifdef HYBRID_GRID
			UINT32 finer = Grid::low.grid.GetCell(cell.stackID).higherEquivalent;
			const auto&c = Grid::high.grid.GetCell(finer);
		#else
			UINT32 finer = cell.cellID;
		#endif
		bool incremented = true;
		int h = cell.height;
		bool set = false;
		while (incremented)
		{
			int _h = h;
			//incremented = false;
			if (set)
				_h = std::max(_h, world.GetStack(finer).FindMostApplicableGround(std::max(_h, cell.height),canStandOnWater,canStandOnLava));
			else
			{
				_h = world.GetStack(finer).FindMostApplicableGround(std::max(_h, cell.height),canStandOnWater,canStandOnLava);
				set = true;
			}
			#ifdef HYBRID_GRID
				for (int j = 0; j < c.neighborhood.numNeighbors; j++)
					_h = std::max(_h, world.GetStack(c.neighborhood.neighborID[j]).FindMostApplicableGround(std::max(_h, cell.height),canStandOnWater,canStandOnLava));
			#endif
			incremented = _h != h;
			h = _h;
		}

		if (world.GetStack(finer).GetCellI(h).IsOccupied())
			return badHeight;
		#ifdef HYBRID_GRID
			for (int j = 0; j < c.neighborhood.numNeighbors; j++)
				if (world.GetStack(c.neighborhood.neighborID[j]).GetCellI(h).IsOccupied())
					return badHeight;
		#endif
		return h;
	}

	int FindApplicableGround(const Grid::LCellIdI&cell, bool canStandOnWater, bool canStandOnLava)
	{
		return FindApplicableGround(World::actual,cell,canStandOnWater,canStandOnLava);
	}



	void					GetChoices(const PWorldEntity&entity, const PMobileEntity&deploying, CursorMode activeCursorMode, Ctr::BasicBuffer<Choice>&movementChoices, TChoiceVisualization&vis)
	{
		//showArrow = false;		//why were these here...?
		//hasArrowCellTarget = false;

		if (entity)
			GetChoices(*entity,deploying,activeCursorMode, movementChoices, vis);
		else
		{
			movementChoices.Clear();
			vis = TChoiceVisualization();
		}
	
	
	}

	
	static void	InsertConstructedEntity(const PWorldEntity&e, float&atTime, bool setHealthToFull)
	{
		#ifndef CLIENT
			if (setHealthToFull && e->owner)
				e->owner->statistics.SignalUnitAcquired(e,PFaction());
		#endif

		ASSERT1__(!e->isInserted, e->GetName());
		#ifndef CLIENT
			e->SyncTime(atTime);

			ASSERT2__(To<Connector>::DynamicCast(e) || NothingBlockingHere(e->location,false),e->GetName(),e->location);
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
		

		bool restored = idMap.IsSet(e->id);
		if (restored)
			ASSERT1__(storedMobileEntities.FindAndErase(e),e->GetName());
		e->OnInsert(atTime,setHealthToFull);
		LogEvent(EventSignificance::Debug, e.get(),String(restored ? "Redeployed" : "Deployed")+" to "+ToString(e->location)+" at t="+String(atTime));
		if (!restored)
		{
			if (setHealthToFull)
				e->health = e->GetMaxHealth();
			ASSERT1__(idMap.SetNew(e->id,e),e->GetName());
		}

		e->RegisterSensory(atTime);

		#ifdef CLIENT
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
			float signalTime = atTime;
			if (restored)
			{
				v->SignalChangeByExternalCause(WorldEntity::StoredInChanged,signalTime);
				v->owner->RegisterStoredMobileLoss(v);
			}
			v->owner->RegisterActiveMobileGain(v);
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

		v->OnInsert(atTime,setHealthToFull);
		LogEvent(EventSignificance::Common, v.get(),"Deployed into storage");
		if (setHealthToFull)
			v->health = v->GetMaxHealth();
		ASSERT1__(idMap.SetNew(v->id,v),v->GetName());


		#ifndef CLIENT
			float signalTime = atTime;
			v->SyncTime(atTime);
			v->owner->RegisterStoredMobileGain(v);
			ApplicationInterface::SignalNewMobileEntity(v,false,signalTime);
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
			if (existing)
				throw Except::Program::DataConsistencyFault(CLOCATION,
					"Trying to insert "+s->GetNameAndOwner()+" into grid, but "+existing->GetNameAndOwner()
					+" was found at height delta "+String(h)+", from location "+ToString(s->location));
		}
		if (s->HasFoundation())
		{
			existing = QueryGrid(structureGrid,s->location-1);
			if (existing)
				throw Except::Program::DataConsistencyFault(CLOCATION,
					"Trying to insert foundation of "+s->GetNameAndOwner()+" into grid, but "+existing->GetNameAndOwner()
					+" was found at foundation height, from location "+ToString(s->location));
		}

		InsertConstructedEntity(s,atTime,setHealthToFull);


		structures << s;

		#ifdef CLIENT
			if (s->IsSkyLinkStructure())
			{
				PSkyLinkStructure link = To<SkyLinkStructure>::DynamicCast(s);
				skyLinks << std::make_pair(link,To<WorkingStructure>::Cast(link->CastToStructure()->shared_from_this()));
			}
		#endif


		InsertIntoGrid(structureGrid,s->location,s);

//		InjectIntoNeighborhood(s,atTime);

		#ifdef CLIENT
			RefreshCompoundCoverage(atTime);
		#else
			s->owner->RegisterStructureGain(s);
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
					{
						LogEvent(EventClass::Preset::debug,s.get(),"InsertConstructed(): refreshNetworkNow is true (this is unexpected)");	//i suspect ownership change of some enabled nexus port might cause this
						//DBG_FATAL__("this should not happen anymore")
					}
					else
						ws->SetEnabled(true,atTime,false,WorkingStructure::ForceEnable);
					//if (!RefreshCompoundCoverageIfNeeded(atTime))
					//	networks[ws->compoundID].RefreshState(atTime);
					//if (!ws->CanEnable())
					//	ws->SetEnabled(false,atTime);
				}
				if (setHealthToFull)
				{
					if (ws->WouldLikeToEnable())
					{
						if (refreshNetworkNow)
							ws->SetEnabled(true,atTime);
						else
							ws->SetEnabled(true,atTime,false,WorkingStructure::ForceEnable);
					}
				}
				//ws->RegisterChange(WorldEntity::EnableChanged,atTime);
			}

		#endif


	}





	static void					InternalRemove( const PWorldEntity&e, RemovalType t, float&timestamp, bool isRemovedFromGrid/*=false*/, bool ignoreIndestructible/*=false*/)
	{
		#ifndef CLIENT
			if (e->IsIndestructible() && !ignoreIndestructible)
				return;

			/*
			This lock assures that we don't get issues with nested updates.
			The call to OnDestroy() can do all sorts of nasty stuff, including remove other entities, or perform operations resulting in invalidated sensory.
			If sensory is allowed to be rebuild (also during OnDestroy()), then UnregisterSensory() will cause havoc.
			This has caused sensor loss after Nexus Port ownership changes.
			The lock assures that any invalidation remains at least until the end of this function call, requiring UnregisterSensory() to be skipped.
			*/
			auto lock = e-> owner->LockSensorUpdates();
		#endif

		e->SyncTime(timestamp);
		ASSERT_NOT_EQUAL1__(e->isInserted, ( t == RemovalType::DestroyedInStorage ),e->GetName());
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


		bool isStructure = e->IsStructure();
		if (isStructure)
		{
			ASSERT1__(!move,e->GetName());	//make sure we don't store structures for now, they don't like that
			auto s = To<Structure>::Cast(e);

			#ifndef CLIENT
				e->owner->RegisterStructureLoss(s);

			#endif

			if (!isRemovedFromGrid)
				RemoveFromGrid(structureGrid,e->location,s);

			ASSERT1__(structures.FindAndErase(e),e->GetName());
			#ifdef CLIENT
				if (s->IsSkyLinkStructure())
				{
					foreach (skyLinks,link)
						if (link->second.lock() == e)
						{
							skyLinks.erase(link);
							break;
						}
				}
			#endif


		}
		else
			if (e->IsMobile())
			{
				PMobileEntity v = To<MobileEntity>::Cast(e);
				#ifdef CLIENT
					ASSERT1__(!v->isInMovementStream,v->GetName());
					//bad stuff happens when we delete now.
					//must fix timing issues. some deletion event did not have enough temporal
					//distance to the last motion of this vehicle
				#else
					if (t != RemovalType::DestroyedInStorage)
					{
						e->owner->RegisterActiveMobileLoss(v);
						if (move)
							e->owner->RegisterStoredMobileGain(v);
					}
					else
						e->owner->RegisterStoredMobileLoss(v);

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



		if (!move)
		{
			LogEvent(EventSignificance::Debug,e.get(),"OnDestroy()");
			e->OnDestroy(t,timestamp);
		}
		#ifdef CLIENT
			ApplicationInterface::OnVanish(e, t, false);
			if (isStructure)
				RefreshCompoundCoverage(timestamp);
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

	void			Remove( const PWorldEntity&e, RemovalType t, float&atTime)
	{
		if (!e)
		{
			LogEvent(EventClass::Preset::internalError,"Trying to remove null-entity, t="+String(t.ToString())+", timestamp="+String(atTime));
			DBG_FATAL__("Trying to remove null-entity");
			return;
		}
		InternalRemove(e,t,atTime,!e->isInsertedIntoGrid,false);
	}

	void			ForceRemove(const PWorldEntity&e, RemovalType t, float&atTime)
	{
		if (!e)
		{
			LogEvent(EventClass::Preset::internalError,"Trying to remove null-entity, t="+String(t.ToString())+", timestamp="+String(atTime));
			DBG_FATAL__("Trying to remove null-entity");
			return;
		}
		InternalRemove(e,t,atTime,!e->isInsertedIntoGrid,true);
	}




	




	PHangar		AccessibleHangarHere( const Grid::LCellIdI&location, const MobileEntity&m, const OwnerParameter&mOwner )
	{
		PWorldEntity e = GetEntity(location);
		if (!e)
			return nullptr;
		if (e->owner != mOwner)
		{
			//may have to check alliances here eventually, but for now this messes things up
			return nullptr;
		}
		return e->CastToDestinationForType(m);
	}





	


	PVehicle		InsertNewVehicle(const OwnerParameter&owner, const Grid::LCellIdI&baseLocation,Component::VehicleBase&base, Component::Turret&turret, bool fillStorage, float&atTime)
	{
		return InsertNewVehicle(owner,baseLocation,&base,&turret,fillStorage,atTime);
	}

	PVehicle InsertNewVehicle(const OwnerParameter&owner, const Grid::LCellIdI&baseLocation,Component::VehicleBase*base, Component::Turret*turret, bool fillStorage, float&atTime)
	{
		PVehicle vehicle(new Vehicle(owner, baseLocation,base,turret));
		#ifdef CLIENT
			vehicle->location = baseLocation;
			ASSERT2__(!vehicle->isInMovementStream,vehicle->location,vehicle->GetName());
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


	


	#ifndef CLIENT
		PMobileEntity	AssembleMobileEntity(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&baseLocation, const String&base, const String&turret)
		{
			if (turret.IsNotEmpty())
				return AssembleVehicle(owner,id,baseLocation,base,turret);
			
			PWorldEntity	proto = Prototypes::all.FindByName(base);
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


		PVehicle		AssembleVehicle(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&baseLocation, const EntityType&type)
		{
			Component::Turret* t = Component::vehicleTurretComponents.Find(type.turretType);
			Component::VehicleBase* b = Component::baseComponents.Find(type.baseType);
			ASSERT__(b);
			ASSERT__(t);
			PVehicle vehicle(new Vehicle(owner, baseLocation,b,t));
			vehicle->location = baseLocation;
			vehicle->id = id;
			vehicle->UpdateSystem();
			return vehicle;
		}

		PVehicle		AssembleVehicle(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&baseLocation, const String&base, const String&turret)
		{
			Component::Turret* t = Component::vehicleTurretComponents.FindByName(turret);
			Component::VehicleBase* b = Component::baseComponents.FindByName(base);
			if (!t)
			{
				LogEvent(EventClass::Preset::fileError,"Turret type is not supported: '"+turret+"'");
				return nullptr;
			}
			if (!b)
			{
				LogEvent(EventClass::Preset::fileError,"Base type is not supported: '"+base+"'");
				return nullptr;
			}
			PVehicle vehicle(new Vehicle(owner, baseLocation,b,t));
			vehicle->location = baseLocation;
			vehicle->id = id;
			vehicle->UpdateSystem();
			return vehicle;
		}

	#else

		PVehicle		SpawnVehicle(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&baseLocation,EntityType type, bool placeNow, bool isOwnedByMe)
		{
			Component::Turret* t = Component::vehicleTurretComponents.Find(type.turretType);
			Component::VehicleBase* b = Component::baseComponents.Find(type.baseType);
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


		PMobileEntity			SpawnNonVehicleMobile(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&baseLocation,const PMobileEntity&proto, bool placeNow, bool isOwnedByMe)
		{
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

		PMobileEntity		SpawnMobileEntity(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&baseLocation,EntityType type, bool placeNow, bool isOwnedByMe)
		{

			PWorldEntity proto = Prototypes::Find(type);
			if (proto)
			{
				ASSERT1__(proto->IsMobile(),proto->GetTypeName());
				return SpawnNonVehicleMobile(owner,id,baseLocation,To<MobileEntity>::Cast( proto ),placeNow,isOwnedByMe);
			}
			return SpawnVehicle(owner,  id,baseLocation,type,placeNow,isOwnedByMe);
		}

		PMobileEntity			SpawnNonVehicleMobile(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&baseLocation,EntityType type, bool placeNow, bool isOwnedByMe)
		{
			PWorldEntity proto = Prototypes::Find(type);
			ASSERT1__(proto,ToString(type));
			ASSERT1__(proto->IsMobile(),proto->GetTypeName());
			return SpawnNonVehicleMobile(owner,id,baseLocation,To<MobileEntity>::Cast( proto ),placeNow,isOwnedByMe);
		}

		PStructure		SpawnStructure(const OwnerParameter&owner, const TUniqueID&id, const Grid::LCellIdI&location,EntityType type, bool placeNow, bool isOwnedByMe)
		{
			auto proto = Prototypes::structures.Find(type);
			if (proto)
			{
				//PStructure s = (*p)->CreateNewInstance(location);
				PStructure s = To<Structure>::Cast(proto->CreateNewInstance(owner,location,0));
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
			FATAL__("Type not found for construction: " + type.ToNumericString());
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
			ASSERT2__(!vehicle->isInMovementStream,vehicle->location,vehicle->GetName());
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
			ASSERT2__(!m->isInMovementStream,m->location,m->GetName());
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
			if (deployWhat && deployWhat->lastDeployment == RuntimeConfig::roundNumber)
			{
				LogEvent(EventClass::Preset::commandRejected,e.get(),"Cannot redeploy "+deployWhat->GetNiceName()+" twice during one round. Try again next round");
				return false;
			}

			Ctr::Buffer0<Choice>	movementChoices;
			TChoiceVisualization vis;
			GetChoices(e,deployWhat,mode,movementChoices,vis);

			if (!movementChoices.Contains(choice))
			{
				LogEvent(EventClass::Preset::commandRejected,e.get(),"Choice "+ToString(choice)+" doesn't appear to be an option.");
				return false;
			}



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
					LogEvent(EventClass::Preset::error,e.get(),v->GetName()+": Failed to position constructed at "+ToString(c)+" ("+String(result.ToString())+"). Production lost.");
					return false;
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
								PVehicleJob job(new VehicleJob::RepairJob(*v,c));
								v->RegisterNewJob("", job,&c);
							}
						}
						break;
						case CursorMode::ClearAboveMode:
						{
							if (!v->GetOperationSpeed(Component::TerrainRemove))
								return false;
							PVehicleJob job(new VehicleJob::ClearAbove(*v,c));
							v->RegisterNewJob("", job,&c);
						}
						break;
						case CursorMode::DigMode:
						{
							if (!v->GetOperationSpeed(Component::TerrainRemove))
								return false;

							auto copy = c;
							PVehicleJob job(new VehicleJob::DigHorizontal(*v,c));
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

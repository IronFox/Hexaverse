#include <global_root.h>
#include "vehicle.h"
#include "satellite.h"
#include "prototypes.h"
#include "systems.h"
#include "gridAndChoices.h"
#include "faction.h"
#include "pathfinding.h"
#include "t2.h"

namespace Objects
{
	SharedContainer<Vehicle>		vehicles;

	#ifndef CLIENT
		bool						newVehiclesHaveFullAmmo = true;

	#endif

	Grid::THighDistanceField		vehicleDistanceField;

	/*void						ForeachVehicle(const std::function<void(const PVehicle&)>&f)
	{
		for (const auto&v : vehicles)
			f(v);
		for (const auto&v : storedVehicles)
			f(v);
	}
*/


	/*static*/ count_t			Vehicle::CountTurretOccurrences(const PFaction&o,Component::Turret*t, bool includeStored)
	{
		count_t rs = 0;
		vehicles.Foreach([&rs,o,t](const PVehicle&v)
		{
			if (v->owner == o && v->turret == t)
				rs++;
		});
		if (includeStored)
			storedMobileEntities.Foreach([&rs,o,t](const PMobileEntity&m)
			{
				if (m->owner == o && m->turret == t)
					rs++;
			});
		return rs;
	}





	

	#ifndef NO_HIBERNATION
		/*virtual override*/ int				Vehicle::GetUplinkTraffic() const
		{
			return turret->uplinkTraffic + 5;

		}
	#endif

	/*static*/ int					Vehicle::GetPrice(Component::VehicleBase*base, Component::Turret*turret, const Research::Attributes::Table&table)
	{
		return (turret ? turret->productionCosts.GetValue(table) : 0) + (base ? base->productionCosts.GetValue(table) : 0);
	}



	/*virtual override*/ float		Vehicle::GetScanningRangeExt(const OwnerParameter&owner, const T2::TModifier&m) const
	{
		const auto& state = GetResearchState(owner);
		return EMPModify(m.Apply(turret->scanningRange.GetValue(state) + (turret->CanAttack() ? 0.f : base->turretRangeModifier.GetValue(state))),2.f);
	}

	/*virtual override*/ String		Vehicle::DescribeScanningRangeExt(const OwnerParameter&owner, const T2::TModifier&m) const
	{
		const auto& state = GetResearchState(owner);
		String rs = turret->scanningRange.Describe(state);
		if (!turret->CanAttack() && base->turretRangeModifier.GetValue(state) != 0)
		{
			rs += "+"+base->turretRangeModifier.Describe(state);
		}
		return rs + m.Describe();
	}

	/*virtual override*/ int				Vehicle::GetMaxHealthExt(const OwnerParameter&owner, const T2::TModifier&m) const
	{
		const auto& state = GetResearchState(owner);
		return m.Apply(base->health.GetValue(state) * turret->healthMultiplier.GetValue(state));
	}

	/*virtual override*/ String				Vehicle::DescribeMaxHealthExt(const OwnerParameter&owner, const T2::TModifier&m) const
	{
		const auto& state = GetResearchState(owner);
		String multiplier = turret->healthMultiplier.Describe(state);
		auto h0 = base->health.GetValue(state)*turret->healthMultiplier.GetValue(state);
		auto h1 = m.Apply(h0);
		if (multiplier == "1" || multiplier == "1.0")
			if (!m.HasEffect())
				return String(h1);
			else
				return String(h1)+"[="+ String(h0) + base->health.Describe(state) + m.Describe()+"]";
		return String(h1)+"[="+ String(base->health.GetValue(state)) + base->health.Describe(state)+"*" + turret->healthMultiplier.Describe(state) + m.Describe()+"]";
	}

	
	#ifndef CLIENT


		/*virtual override*/ void		Vehicle::OnInsert( float&atTime, bool freshConstruction )
		{
			Super::OnInsert(atTime,freshConstruction);
			entityTime = atTime;
			if (freshConstruction && turret->CanAttack())
			{
				//leave munitions here. it seems fair that a new tank starts with full ammo, but an emp-tank should not start with full uranium...
				#if !defined ONE_COMPOUND_PER_FACTION && !defined NO_MUNITION
					if (newVehiclesHaveFullAmmo)
						ChangeStorage(*this,atTime,Resource::Munitions,GetMaxStorageCapacity().level[Resource::Munitions] - currentStorage.level[Resource::Munitions]);
				#endif
			}
		}





	#endif

	
	#ifndef ONE_COMPOUND_PER_FACTION

	ResourceVector			Vehicle::GetMaxStorageCapacity()	const
	{
		ResourceVector rs = turret->baseStorageCapacity;
		rs *= base->storageMultiplier;
		return rs;
	}

	#endif

	int						Vehicle::GetTurretOverweight() const
	{
		const auto& state = GetResearchState();
		return std::max(0, (int)floor((turret->weight.GetValue(state) - base->GetCarryingCapacity(state))));
	}

	float					Vehicle::GetTurretOverweightFactor() const
	{
		return Grid::powi(0.75f,GetTurretOverweight());
	}




	/*virtual override*/int			Vehicle::GetMovementStepsPerRoundExt(const OwnerParameter&owner, const T2::TModifier&m)	const
	{
		return (int)ceil(EMPModify(
				m.Apply(
					base->GetBaseMovementSpeed(GetResearchState(owner)) 
					* GetTurretOverweightFactor()
				),
				1.f));
	}

	/*virtual override*/ float			Vehicle::GetBaseMovementInitiative() const
	{
		return base->initiative.GetValue(GetResearchState(owner));
	}

	/*virtual override*/String			Vehicle::DescribeMovementStepsPerRoundExt(const OwnerParameter&owner, const T2::TModifier&m)	const
	{
		return base->movementSpeed.Describe(GetResearchState(owner))+m.Describe();
	}

	/*virtual override*/ String			Vehicle::DescribeFlatAttackRangeExt(const OwnerParameter&owner, const T2::TModifier&m) const
	{
		const auto& state = GetResearchState(owner);
		String rs = turret->horizontalAttackRange.Describe(state);
		{
			String mod = base->turretRangeModifier.Describe(state);
			if (mod != '0')
				rs += "+"+mod;
		}
		return rs + m.Describe();
	}

	
	/*virtual override*/ float			Vehicle::GetFlatAttackRangeExt(const OwnerParameter&owner, const T2::TModifier&m) const
	{
		const auto& state = GetResearchState(owner);
		return EMPModify(m.Apply(turret->horizontalAttackRange.GetValue(state) + base->turretRangeModifier.GetValue(state)),2.f);
	}

	/*virtual override*/ int			Vehicle::GetOperationAccelerationExponent(Component::Operation op) const
	{
		return base->acceleration[op];
	}


	/*virtual override*/ int			Vehicle::GetOperationSpeed(Component::Operation op, const OwnerParameter&owner) const
	{
		const auto&state = GetResearchState(owner);
		return (int)floor(turret->GetOperationSpeed(op,state) * base->operationSpeedMultiplier.GetValue(state));
	}

	/*virtual override*/ String			Vehicle::DescribeOperationSpeed(Component::Operation op, const OwnerParameter&owner) const
	{
		const auto&state = GetResearchState(owner);
		String rs = turret->operationSpeed[op].Describe(state);
		const auto multiplier = base->operationSpeedMultiplier.GetValue(state);
		if (multiplier == 1)
			return rs;
		const auto final = (int)floor(turret->operationSpeed[op].GetValue(state) * multiplier);
		return String(final)+"=["+rs+"*"+base->operationSpeedMultiplier.Describe(state)+"(base)]";
	}


	#ifdef CLIENT
	#else
		void Vehicle::PositionOnTop(const Grid::LCellIdI&cell, bool signalEvent)
		{
			int h = FindApplicableGround(cell,base->moveOnWater,false);
			ASSERT__(h != badHeight);
			MobilePlacementResult rs = PlaceExactlyAt(Grid::LCellIdI(cell.stackID, h),0,0,signalEvent,false);
			ASSERT_EQUAL__(rs, MobilePlacementResult::PlacementOkay);
			ClearAllTasks(true,CLOCATION);
		}



		/*virtual override*/ MobilePlacementResult Vehicle::PlaceExactlyAt(const Grid::LCellIdI&cell, float timeCost, float minTimeOffset, bool signalEvent/*=true*/, bool isCreated/*=true*/, bool failIsFatal/*=false*/)
		{
			if (IsDeadOrGhost())
			{
				if (failIsFatal)
				{
					LogEvent(EventClass::Preset::fatal,this,"Dead");
					FATAL__("Critical abort");
				}
				return MobilePlacementResult::PlacementFailedHarmfulOrBlocked;
			}
			if (Grid::AffectedByEMP(cell) && !failIsFatal)
				return MobilePlacementResult::PlacementFailedHarmfulOrBlocked;
			
			//VerifyVisibilityNetwork();
			SyncTime(minTimeOffset);

			if (!World::actual.LowCellIsClear(cell))
			{
				PHangar s = AccessibleHangarHere(cell,*this);
				if (s && s->Store(To<Vehicle>::Cast(shared_from_this()),entityTime))
					return MobilePlacementResult::EnteredHangar;

				if (failIsFatal)
				{
					PStructure st = GetStructure(cell);
					LogEvent(EventClass::Preset::fatal,this,"Target cell ("+ToString(cell)+") is obstructed by structure "+(st ? st->GetName(): "<>")+": "+World::actual.LowCellReport(cell));
					FATAL__("Critical abort");
				}

				return MobilePlacementResult::PlacementFailedHarmfulOrBlocked;
			}
			if (location != cell) 
			{
				auto v = GetMobileEntity(cell);
				if (v)
				{
					if (v->owner == owner)
					{
						PHangar s = v->CastToDestinationForType(*this);
						if (s && s->Store(To<Vehicle>::Cast(shared_from_this()),entityTime))
							return MobilePlacementResult::EnteredHangar;
					}

					if (failIsFatal)
					{
						LogEvent(EventClass::Preset::fatal,this,"Target cell ("+ToString(cell)+") is obstructed by vehicle");
						FATAL__("Critical abort");
					}
					if (!v->HasMotionTasks() || v->ConstructingHere(v->location))
						return MobilePlacementResult::PlacementFailedPermanentlyObstructed;
					else
					{
						LogEvent(EventClass::Preset::debug,this,"Obstruction detected: "+v->GetName()+" at "+ToString(v->location));
						return MobilePlacementResult::PlacementFailedTemporarilyObstructed;
					}
				}
			}
			const Grid::LCellIdI old = location;

			#ifdef _DEBUG
				bool hadTasks = HasTasks();
			#endif
			
			location = cell;
			int fallingHeight = 0;
			while (location.height-fallingHeight > -Grid::CellStack::HalfRange 
					&& World::actual.LowCellIsClear(Grid::LCellIdI(location.stackID, location.height - fallingHeight - 1))
					&& NoMobileEntityHereExcept(Grid::LCellIdI(location.stackID, location.height - fallingHeight - 1),this))
				fallingHeight ++;
			//if (fallingHeight > 0)
			{
				timeCost += Times::Fall * fallingHeight;
				location = location - fallingHeight;
				if (!CanClimb(-fallingHeight) || !CanStandOn(World::actual,location-1))
				{
					if (cell.stackID != old.stackID)	//at least some horizontal motion
					{
						location = old;
						PlaceExactlyAt(location,0,minTimeOffset,signalEvent,isCreated);
						if (failIsFatal)
						{
							LogEvent(EventClass::Preset::fatal,this,"Target cell ("+ToString(cell)+") is unsuitable for placement");
							FATAL__("Critical abort");
						}

						return MobilePlacementResult::PlacementFailedHarmfulOrBlocked;
					}
					if (isCreated)
					{
						RemoveFromGrid(shared_from_this(),old);
						//RemoveFromGrid(vehicleGrid,old,To<Vehicle>::Cast(shared_from_this()));
						//InsertIntoGrid(vehicleGrid,location,To<Vehicle>::Cast(shared_from_this()));

						if (timeCost > 0)
							ApplicationInterface::SignalMotion(To<Vehicle>::Cast(shared_from_this()),old,location,entityTime,timeCost);

						entityTime += timeCost;
						InsertIntoGrid(shared_from_this(),location);
						//RemoveFromGrid(vehicleGrid,location,To<Vehicle>::Cast(shared_from_this()));
					}
					if (!IsIndestructible())
						LogEvent(EventClass::Preset::commandRejected,this, "Unable to relocate here. Vehicle destroyed.");
					else
						LogEvent(EventClass::Preset::debug,this, "Extended fall. Entity is indestructible.");
//					Remove(shared_from_this(),RemovalType::UncontrolledExplosion,timeAt,false,true);

					if (failIsFatal)
					{
						LogEvent(EventClass::Preset::fatal,this,"Target cell ("+ToString(cell)+") is unsuitable for placement. Destruction event");
						FATAL__("Critical abort");
					}

					return MobilePlacementResult::PlacementFailedMobileDestroyed;
				}

			}
			MobilePlacementResult result = MobilePlacementResult::PlacementOkay;
			if (old != location)
			{
				if (fallingHeight > 0)
				{
					result = MobilePlacementResult::PlacementOkayFellDown;
				}
				if (isCreated)
				{
					RemoveFromGrid(shared_from_this(),old);
					InsertIntoGrid(shared_from_this(),location);

					if (constructionActionCounter)
					{
						constructionActionCounter = 0;
						RegisterChange(WorldEntity::TasksChanged);
					}
				}

				Super::HandleMotion(result, signalEvent,old, timeCost);
			}
			#ifdef _DEBUG
				ASSERT__(Failed(result) || hadTasks == HasTasks());
			#endif
			return result;
		}


	#endif


	
	#ifdef HAS_WINDOW

		/*virtual override*/ const CC*	Vehicle::GetCommands(CC&dynamicStorage) const
		{
			return &dynamicStorage
					.Append(Super::GetCommands(dynamicStorage))
					.Append(base->commands)
					.Append(turret->commands)
					;
		}


		void Vehicle::DescribeType(Command::Base::Label&outResult, UINT32 descFlags, const OwnerParameter&owner)
		{
			outResult.BeginBrief();
			Super::DescribeType(outResult,descFlags, owner);
			base->DescribeType(outResult,true,true,owner);
			outResult.BeginBrief();

			const auto&state = GetResearchState(owner);

			const float capacity = floor(base->GetCarryingCapacity(state));
			const float weight = turret->weight.GetValue(state);
			if (weight > capacity)
				outResult << "Overweight: "+String(weight - capacity);
			const int baseMovement = (int)floor(base->GetBaseMovementSpeed(state));
			if (baseMovement > 0)
			{
				const auto move = this->GetMovementStepsPerRoundExt(owner,t2 ? t2->speed : T2::TModifier());
				String speed = "Effective speed: "+String(move);
				if (move != baseMovement)
					speed += " /"+String(baseMovement);
				outResult << speed;
			}
			else
				outResult << "Stationary on ground";


			if (base->requirements == Component::VehicleBase::Requirement::Water)
				outResult.GetLastLine()+= " (naval)";
			else
				if (base->moveOnWater)
					outResult.GetLastLine() += " (moves over water)";

			int opSpeed = std::max(GetOperationSpeed(Component::Operation::Construct, owner),GetOperationSpeed(Component::Operation::AddTerrain,owner));
			if (opSpeed)
				outResult << "Effective construction speed: "+String(ResourceTag(Resource::Minerals))+String(opSpeed)+" per round";

			#ifndef ONE_COMPOUND_PER_FACTION
				if (turret->baseStorageCapacity.IsNotEmpty())
				{
					outResult << "Effective storage capacity:";
					ResourceVector vec = this->GetMaxStorageCapacity(owner);
					for (index_t i = 0; i < Resource::NumberOfPossibilities; i++)
					{
						if (vec.level[i])
						{
							outResult << " "+String(ResourceTag(i))+String(vec.level[i]);
						}
					}
				}
			#endif

		}


	#endif




	#ifndef CLIENT




		bool	Vehicle::DirectTo( mode_t mode, MutableArrayRef<Grid::LCellIdI> waypoints, bool detailedPath, bool leftClick /*= false*/ )
		{
			DBG_ASSERT1__(storedIn.expired(),GetName());
			switch (mode)
			{
				case CursorMode::Idle:
					return PlotCourse(waypoints,detailedPath);
				break;
				case CursorMode::DigTunnelMode:
					if (!turret->CanDo(Component::Operation::RemoveTerrain))
						return false;
					if (leftClick)
					{
						const Grid::LCellIdI pointOfOrigin = GetLastPointOfOrigin();

						Buffer0<PathNode> path;
						Pathfinding::TWays	possibilities;
						if (detailedPath)
						{
							Grid::LCellIdI prev = pointOfOrigin;
							for (const auto&dest : waypoints)
							{
								Pathfinding::GetWays(owner->knownWorld,this,prev,possibilities,dest,nullptr, Pathfinding::RoutingFlags::IgnoreMobilesOnDestination | Pathfinding::RoutingFlags::IgnoreOwnMobilesOnRoute);
								bool found = false;
								for (index_t j = 0; j < possibilities.numWays; j++)
								{
									if (possibilities.way[j] == dest)
									{
										found = true;
										break;
									}
								}
								if (!found)
								{
									LogEvent(EventClass::Preset::commandRejected,this,"Failed to trace explicit path");
								}
								path.Append().cell = dest;
								prev = dest;
							}
						}
						else
						{

							for (Grid::LCellIdI destination : waypoints)
							{
								if (destination.height > pointOfOrigin.height)
									destination--;
								if (destination != pointOfOrigin)
								{
									if (!Pathfinding::AppendShortestDirectPath(pointOfOrigin,destination,path))
									{
										LogEvent(EventClass::Preset::commandRejected,this,"Unable to plot path to destination");
										return false;
									}
								}
							}


						}

						Grid::LCellIdI c = pointOfOrigin;

						foreach (path,node)
						{
							if (GetStructure(node->cell))
								break;

							if (node->cell.height == c.height)
								RegisterNewJob("", PVehicleJob(new VehicleJob::DigHorizontal(*this,node->cell)),&node->cell);
							else
							{
								if (node->cell.height < c.height)
								{
									RegisterNewJob("", PVehicleJob(new VehicleJob::DigDown(*this,node->cell)),&node->cell);
								}
								else
								{
									RegisterNewJob("", PVehicleJob(new VehicleJob::ClearAbove(*this,node->cell)),&node->cell);
									++node;
									if (node != path.end())
										RegisterNewJob("", PVehicleJob(new VehicleJob::ClearAbove(*this,node->cell)),&node->cell);
									else
										break;
									ASSERT__(!TasksAreLocked());

									myTasks << Task("Move To",node->cell,true,Task::MoveTo);
									RegisterChange(TasksChanged);
									myTasks.GetNewest().Replot(pointOfOrigin, this,FocusChangeType::FullChange,true);
								}

							}

							c = node->cell;
						}


					}
					else
						return PlotCourse(waypoints,detailedPath);

				break;

				case CursorMode::BuildConnectorChainMode:
				case CursorMode::BuildRoadChainMode:
				case CursorMode::BuildHighWallChainMode:
				case CursorMode::BuildLowWallChainMode:
				case CursorMode::BuildBridgeMode:
					if (!turret->CanDo(mode == CursorMode::BuildConnectorChainMode ? Component::Operation::Construct : Component::Operation::AddTerrain  ) )
						return false;

					if (leftClick)
					{
						/*if (!Grid::LowCellIsClear(destination))
							destination ++;*/
						for (const auto&way : waypoints)
						{
							if (myTasks.Count() >= MaxTasks)
							{
								LogEvent(EventClass::Preset::commandRejected,this,"Job denied: Max production jobs reached");
								break;
							}
							RegisterNewJob("", PVehicleJob(new VehicleJob::Sequence(*this,mode)),&way);
						}
					}
					else
						return PlotCourse(waypoints,detailedPath);
				break;
				case CursorMode::AttackMode:
					if (!turret->CanAttack())
						return false;

					if (leftClick)
					{
						for (Grid::LCellIdI destination : waypoints)
						{
							if (destination != location)
							{
								Grid::LCellIdI pointOfOrigin = GetLastPointOfOrigin();

								if (!CanHitFrom(pointOfOrigin,TrajectoryEndPoint(destination)))
								{
									LogEvent(EventClass::Preset::commandRejected,this,"Destination out of range");
									return false;
								}

								if (!EnqueueAttack(destination))
									return false;
							}
						}
					}
					else
						return PlotCourse(waypoints,detailedPath);
				break;
			}
			return true;

		}

		//void	Vehicle::DirectTo(Grid::LCellIdI destination, bool leftClick /*= false*/)
		//{
		//	DirectTo(activeCursorMode, destination, leftClick);
		//	if (activeCursorMode != Idle)
		//	{
		//		activeCursorMode = Idle;
		//		UpdateChoices();
		//	}
		//}




	#endif




	
	#ifdef HAS_WINDOW



		/*virtual overrider*/ bool			Vehicle::TestIntersection(const M::TVec3<>&_b, const M::TVec3<>&_d, float & distance) const
		{
			M::TVec3<> b,d;//,test;
			Mat::Transform(inverse,_b,b);
			Mat::rotate(inverse,_d,d);
			
			return base->TestIntersection(0,0,0,b,d,distance) | turret->TestIntersection(this->base->height, turretAngleX,turretAngleY,b,d,distance);	//binary or - must evaluate both
		}

	#endif


	int						Vehicle::GetPrice() const
	{
		return GetPrice(base,turret, owner->GetAttributeTable());
	}

	int						Vehicle::GetPrice(const PFaction&owner) const
	{
		return GetPrice(base,turret, owner->GetAttributeTable());
	}


}


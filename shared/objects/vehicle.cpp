#include <global_root.h>
#include "vehicle.h"
#include "satellite.h"
#include "rendering.h"
#include "prototypes.h"
#include "systems.h"
#include "gridAndChoices.h"
#if !defined CLIENT || defined ONE_COMPOUND_PER_FACTION
	#include "faction.h"
#endif

namespace Objects
{
	Buffer0<PVehicle>				vehicles;

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


	/*static*/ count_t			Vehicle::CountTurretOccurrences(const OwnerParameter&o,Component::Turret*t)
	{
		count_t rs = 0;
		for (const auto&m : vehicles)
			if (m->owner == o && m->turret == t)
				rs++;
		return rs;
	}



	
	int						Vehicle::GetOperationSpeed(Component::operation_t op)  const
	{
		return (int)floor(turret->GetOperationSpeed(op,owner) * base->operationSpeedMultiplier);
	}


	

	#ifndef NO_HIBERNATION
		/*virtual override*/ int				Vehicle::GetUplinkTraffic() const
		{
			return turret->uplinkTraffic + 5;

		}
	#endif

	/*static*/ int					Vehicle::GetPrice(Component::VehicleBase*base, Component::Turret*turret)
	{
		return (turret ? turret->productionCosts : 0) + (base ? base->productionCosts : 0);
	}



	/*virtual override*/ float		Vehicle::GetScanningRange() const
	{
		return EMPModify(turret->GetScanningRange(owner,turret->CanAttack() ? 0 : base->rangeModifier), 2.f);
	}

	/*virtual override*/ String		Vehicle::DescribeScanningRange() const
	{
		return owner
				?
				owner->researchState.Describe(*turret,turret->baseScanningRange+(turret->CanAttack() ? 0 : base->rangeModifier),Component::Attribute::ScanningRange, ResearchState::ModulationType::DefaultFloat,BindEMPModifyFunction())
				:
				String(GetScanningRange())
				;
	}

	/*virtual override*/ int				Vehicle::GetMaxHealth() const
	{
		return std::max(1,(int)floor(base->GetBaseHealth(owner) * turret->healthMultiplier));
	}

	/*virtual override*/ String				Vehicle::DescribeMaxHealth() const
	{
		return owner ? owner->researchState.Describe(*base,base->unmodulatedHealth * turret->healthMultiplier,Component::Attribute::Health,ResearchSateModulation::FloorInt) : String(GetMaxHealth());
	}

	
	#ifndef CLIENT


		/*virtual override*/ void		Vehicle::OnConstructionFinished(float atTime)
		{
			if (turret->CanAttack())
			{
				//leave munitions here. it seems fair that a new tank starts with full ammo, but an emp-tank should not start with full uranium...
				#if !defined ONE_COMPOUND_PER_FACTION && !defined NO_MUNITION
					if (newVehiclesHaveFullAmmo)
						ChangeStorage(*this,atTime,Resource::Munitions,GetMaxStorageCapacity().level[Resource::Munitions] - currentStorage.level[Resource::Munitions]);
				#endif

				//
				//currentStorage.level[Resource::Munitions] = GetMaxStorageCapacity().level[Resource::Munitions];
				//LogEvent(EventSignificance::Common,this,"Munition storage changed to "+String(currentStorage.level[Resource::Munitions]));
			}
		}

		/*virtual override*/ bool Vehicle::FillAttackQueue()
		{
			if (!turret->CanAttack() || !HasAmmoForShot())
			{
				//LogEvent(EventClass::Preset::commandRejected,this,"Insufficient ammunition");
				return false;
			}
			//index_t freeSlots = attackQueue.Count() < t->GetAttacksPerRound() <  t->GetAttacksPerRound() - t->attackQueue.Count() : 0;
			//if (attackQueue.Count() < CountPossibleAttacks())
			{
				Grid::LCellIdI target;

				const ResourceVector&storage = 
					#ifndef ONE_COMPOUND_PER_FACTION
						this->currentStorage
					#else
						this->owner->compound.total.storage
					#endif
				;

				if (GetSentryTarget(*this, storage.level[turret->ammoType], location,base->height,*this, target))
				{
					EnqueueAttack(target);
					return true;
				}
			}
			//else
				//return false;
			return false;

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
		return std::max(0, turret->weight - (int)floor(base->GetCarryingCapacity(owner)));
	}

	float					Vehicle::GetTurretOverweightFactor() const
	{
		return Grid::powi(0.75f,GetTurretOverweight());
	}




	/*virtual override*/int			Vehicle::GetMovementStepsPerRound()	const
	{
		return (int)ceil(EMPModify(
				base->GetBaseMovementSpeed(owner) 
				* GetTurretOverweightFactor(),
				1.f));
	}

	/*virtual override*/String			Vehicle::DescribeMovementStepsPerRound()	const
	{
		return owner ?
				owner->researchState.Describe(*base,GetTurretOverweightFactor() * base->unmodulatedMovementSpeed,Component::Attribute::MovementSpeed,ResearchSateModulation::CeilInt,BindEMPModifyFunction())
				:
				String(GetMovementStepsPerRound());
	}

	
	/*virtual override*/ float			Vehicle::GetFlatAttackRange() const 
	{
		return EMPModify(turret->GetHorizontalAttackRange(owner,base->rangeModifier),1.f);
	}


	#ifdef CLIENT
	#else
		void Vehicle::PositionOnTop(const Grid::LCellIdI&cell, bool signalEvent)
		{
			int h = FindApplicableGround(cell,base->moveOnWater);
			ASSERT__(h != badHeight);
			MobilePlacementResult rs = PlaceExactlyAt(Grid::LCellIdI(cell.stackID, h),0,0,signalEvent,false);
			ASSERT_EQUAL__(rs, MobilePlacementResult::PlacementOkay);
			ClearAllTasks(true,CLOCATION);
		}



		/*virtual override*/ MobilePlacementResult Vehicle::PlaceExactlyAt(const Grid::LCellIdI&cell, float timeCost, float minTimeOffset, bool signalEvent/*=true*/, bool isCreated/*=true*/, bool failIsFatal/*=false*/)
		{
			if (health <= 0)
			{
				if (failIsFatal)
				{
					LogEvent(EventClass::Preset::fatal,this,"Dead");
					FATAL__("Critical abort");
				}
				return MobilePlacementResult::PlacementFailedHarmfulOrBlocked;
			}
			
			//VerifyVisibilityNetwork();
			SyncTime(minTimeOffset);

			if (!Grid::actualWorld.LowCellIsClear(cell))
			{
				PHangarStructure s = MobileDestinationStructureHere(cell,*this);
				if (s)
				{
					PVehicleStore st = To<VehicleStore>::DynamicCast(s);
					ASSERT1__(st,s->ToEntity().GetName());
					if (st && st->StoreInPark(To<Vehicle>::Cast(shared_from_this()),motionTime))
					{
						return MobilePlacementResult::EnteredStructure;
					}
				}
				if (failIsFatal)
				{
					LogEvent(EventClass::Preset::fatal,this,"Target cell ("+cell.ToString()+") is obstructed by structure");
					FATAL__("Critical abort");
				}

				return MobilePlacementResult::PlacementFailedHarmfulOrBlocked;
			}
			if (location != cell) 
			{
				PVehicle v = GetVehicle(cell);
				if (v)
				{
					if (failIsFatal)
					{
						LogEvent(EventClass::Preset::fatal,this,"Target cell ("+cell.ToString()+") is obstructed by vehicle");
						FATAL__("Critical abort");
					}
					if (!v->HasMotionTasks() || v->ConstructingHere(v->location))
						return MobilePlacementResult::PlacementFailedPermanentlyObstructed;
					else
						return MobilePlacementResult::PlacementFailedTemporarilyObstructed;
				}
			}
			const Grid::LCellIdI old = location;

			#ifdef _DEBUG
				bool hadTasks = HasTasks();
			#endif
			
			location = cell;
			int fallingHeight = 0;
			while (location.height-fallingHeight > -Grid::CellStack::HalfRange 
					&& Grid::actualWorld.LowCellIsClear(Grid::LCellIdI(location.stackID, location.height - fallingHeight - 1))
					&& NoVehicleHereExcept(Grid::LCellIdI(location.stackID, location.height - fallingHeight - 1),this))
				fallingHeight ++;
			//if (fallingHeight > 0)
			{
				timeCost += Times::Fall * fallingHeight;
				location = location - fallingHeight;
				if (!CanClimb(-fallingHeight) || !CanStandOn(Grid::actualWorld,location-1,false,false))
				{
					if (cell.stackID != old.stackID)	//at least some horizontal motion
					{
						location = old;
						PlaceExactlyAt(location,0,minTimeOffset,signalEvent,isCreated);
						if (failIsFatal)
						{
							LogEvent(EventClass::Preset::fatal,this,"Target cell ("+cell.ToString()+") is unsuitable for placement");
							FATAL__("Critical abort");
						}

						return MobilePlacementResult::PlacementFailedHarmfulOrBlocked;
					}
					if (isCreated)
					{
						RemoveVehicleFromGrid(To<Vehicle>::Cast(shared_from_this()),old);
						//RemoveFromGrid(vehicleGrid,old,To<Vehicle>::Cast(shared_from_this()));
						//InsertIntoGrid(vehicleGrid,location,To<Vehicle>::Cast(shared_from_this()));

						if (timeCost > 0)
							SignalMotion(To<Vehicle>::Cast(shared_from_this()),old,location,motionTime,timeCost);

						motionTime += timeCost;
						InsertVehicleIntoGrid(To<Vehicle>::Cast(shared_from_this()),location);
						//RemoveFromGrid(vehicleGrid,location,To<Vehicle>::Cast(shared_from_this()));
					}
					LogEvent(EventClass::Preset::commandRejected,this, "Unable to relocate here. Vehicle destroyed.");
//					Remove(shared_from_this(),RemovalType::UncontrolledExplosion,timeAt,false,true);

					if (failIsFatal)
					{
						LogEvent(EventClass::Preset::fatal,this,"Target cell ("+cell.ToString()+") is unsuitable for placement. Destruction event");
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
					RemoveVehicleFromGrid(To<Vehicle>::Cast(shared_from_this()),old);
					InsertVehicleIntoGrid(To<Vehicle>::Cast(shared_from_this()),location);

					if (constructionRoundCounter)
					{
						constructionRoundCounter = 0;
						RegisterChange(WorldEntity::TasksChanged,motionTime);
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



	/*virtual override*/bool				Vehicle::HasAmmoForShot() const 
	{
	#ifdef ONE_COMPOUND_PER_FACTION
		const Compound&compound = owner->compound;
		return compound.total.storage.level[turret->ammoType] >= turret->GetAmmoNeedPerShot(owner);
	#else
		return currentStorage.level[turret->ammoType] >= turret->GetAmmoNeedPerShot(owner);
	#endif
	}
	
	#ifdef HAS_WINDOW

		void Vehicle::DescribeType(BaseCommand::Label&outResult, UINT32 descFlags)
		{
			Super::DescribeType(outResult,descFlags);
			outResult << "";
			base->DescribeType(outResult,true,owner);
			outResult << "";

			const int capacity = (int)floor(base->GetCarryingCapacity(owner));
			if (turret->weight > capacity)
				outResult << "Overweight: "+String(turret->weight - capacity);
			String speed = "Effective speed: "+String(this->GetMovementStepsPerRound());
			const int baseMovement = (int)floor(base->GetBaseMovementSpeed(owner));
			if (GetMovementStepsPerRound() != baseMovement)
				speed += " /"+String(baseMovement);

			outResult << speed;
			if (base->requirements == Component::VehicleBase::Requirement::Water)
				outResult.lines.last()+= " (naval)";
			else
				if (base->moveOnWater)
					outResult.lines.last() += " (moves over water)";

			int opSpeed = std::max(GetOperationSpeed(Component::Construct),GetOperationSpeed(Component::TerrainAdd));
			if (opSpeed)
				outResult << "Effective construction speed: "+String(ResourceTag(Resource::Minerals))+String(opSpeed)+" per round";

			#ifndef ONE_COMPOUND_PER_FACTION
				if (turret->baseStorageCapacity.IsNotEmpty())
				{
					outResult << "Effective storage capacity:";
					ResourceVector vec = this->GetMaxStorageCapacity();
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



		void	Vehicle::EnqueueAttack(Grid::LCellIdI destination)
		{
			Grid::LCellIdI pointOfOrigin = GetLastPointOfOrigin();
			ASSERT__(!TasksAreLocked());
			if (!HasTasks() || !myTasks.GetNewest().IsMode(CursorMode::AttackMode))
			{
				myTasks << Task("Attack Position",destination,true,Task::Mode,CursorMode::AttackMode);
				myTasks.GetNewest().Replot(pointOfOrigin,this,FocusChangeType::FullChange,true);
				RegisterChange(TasksChanged,-1);
			}
			else
			{
				//if (!tasks.GetNewest().foci.Contains(destination))	//irrelevant. attack same location as often as you want
				{
					myTasks.GetNewest().foci.Push().target = destination;
					myTasks.GetNewest().Replot(pointOfOrigin,this,FocusChangeType::FocusAppended,true);
					RegisterChange(TasksChanged,-1);
				}
			}
		}


		bool	Vehicle::DirectTo(mode_t mode, ArrayRef<Grid::LCellIdI> waypoints, bool detailedPath, bool leftClick /*= false*/)
		{
			switch (mode)
			{
				case CursorMode::Idle:
					return PlotCourse(waypoints,detailedPath);
				break;
				case CursorMode::DigTunnelMode:
					if (!turret->CanDo(Component::TerrainRemove))
						return false;
					if (leftClick)
					{
						const Grid::LCellIdI pointOfOrigin = GetLastPointOfOrigin();

						Buffer0<PathNode> path;
						TWays	possibilities;
						if (detailedPath)
						{
							Grid::LCellIdI prev = pointOfOrigin;
							for (const auto&dest : waypoints)
							{
								Objects::GetWays(owner->currentState,this,prev,possibilities,dest,true,true,false);
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
									if (!AppendShortestDirectPath(pointOfOrigin,destination,path))
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
								RegisterNewJob("", PVehicleJob(new VehicleJob::DigHorizontal(*this)),&node->cell);
							else
							{
								if (node->cell.height < c.height)
								{
									RegisterNewJob("", PVehicleJob(new VehicleJob::DigDown(*this)),&node->cell);
								}
								else
								{
									RegisterNewJob("", PVehicleJob(new VehicleJob::ClearAbove(*this)),&node->cell);
									++node;
									if (node != path.end())
										RegisterNewJob("", PVehicleJob(new VehicleJob::ClearAbove(*this)),&node->cell);
									else
										break;
									ASSERT__(!TasksAreLocked());

									myTasks << Task("Move To",node->cell,true,Task::MoveTo);
									RegisterChange(TasksChanged,-1);
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
				case CursorMode::BuildWallChainMode:
				case CursorMode::BuildBridgeMode:
					if (!turret->CanDo(mode == CursorMode::BuildConnectorChainMode ? Component::Construct : Component::TerrainAdd  ) )
						return false;

					if (leftClick)
					{
						/*if (!Grid::LowCellIsClear(destination))
							destination ++;*/
						for (const auto&way : waypoints)
							RegisterNewJob("", PVehicleJob(new VehicleJob::Sequence(*this,mode)),&way);
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

								if (!CanHitFrom(pointOfOrigin,destination))
								{
									LogEvent(EventClass::Preset::commandRejected,this,"Destination out of range");
									return false;
								}

								EnqueueAttack(destination);
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



		/*virtual overrider*/ bool			Vehicle::TestIntersection(const TVec3<>&_b, const TVec3<>&_d, float & distance) const
		{
			TVec3<> b,d;//,test;
			Mat::transform(inverse,_b,b);
			Mat::rotate(inverse,_d,d);
			
			return base->TestIntersection(0,0,0,b,d,distance) | turret->TestIntersection(this->base->height, turretAngleX,turretAngleY,b,d,distance);	//binary or - must evaluate both
		}

	#endif

}


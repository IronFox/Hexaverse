#include <global_root.h>
#include "jobs.h"

#ifndef CLIENT
	#include "structure.h"
	#include "faction.h"
	#include "gridAndChoices.h"
	#include "prototypes.h"
#endif
#include "vehicle.h"
#include "gridAndChoices.h"
#include "../runtimeConfig.h"


namespace Objects
{


	namespace VehicleJob
	{

		/**/					BaseJob::BaseJob(int mineralCosts, const MobileEntity&v, Component::operation_t op, Autocomplete jobAutoCompletion, bool movesToFocusWhenDone):
										roundsToJobCompletion(-1),resourceCostLeft(0),
										jobAutoCompletion(jobAutoCompletion),movesToFocusWhenDone(movesToFocusWhenDone)
		{
			ResetResourceAndRoundsLeft(mineralCosts,v,op);
		}

		void BaseJob::ResetResourceAndRoundsLeft(int cost, const MobileEntity&v, Component::operation_t op)
		{
			roundsToJobCompletion = v.GetOperationRounds(abs(cost),op);
			resourceCostLeft = std::max(cost,0);
		}

		BYTE					BaseJob::GetConstructionActionCostPerRound(const MobileEntity&v) const
		{
			return MobileEntity::MaxConstructionActionLeft >> v.GetConstructionAcceleration(GetOperationAffinity());
		}


		#ifndef CLIENT

		/*static*/ Ctr::Buffer0<PVehicleJob>			BaseJob::allJobPrototypes;
		
		BaseJob::continue_t BaseJob::Continue(MobileEntity&v, const Grid::LCellIdI&focus, bool isNewRound)
		{
			String error;
			if (!CanContinue(v,focus,error))
			{
				ASSERT3__(error.IsNotEmpty(), GetName(), v.GetName(), focus);
				LogEvent(EventClass::Preset::operationAborted,&v,"Unable to continue '"+GetName()+"' ("+error+").");	//cant use GetName(), once we call ClearTasks()
				return TerminalFailure;
			}

			int withdrawNow = std::min(resourceCostLeft,v.GetOperationSpeed(GetOperationAffinity()));


			#ifdef ONE_COMPOUND_PER_FACTION
				int storage = GetStorage(v,this->GetConsumedResourceType());
				if (storage < withdrawNow)
				{
					LogEvent(EventClass::Preset::operationAborted,&v,"Insufficient "+String(this->GetConsumedResourceType().ToString())+" available ("+String(storage)+"/"+String(withdrawNow)+"). Delaying operation.");

					v.entityTime += timeEpsilon;
					ApplicationInterface::SignalOperationLacksResources(v,this->GetConsumedResourceType(),v.entityTime);
					v.entityTime += timeEpsilon;

					return JobDelayed;
				}

			#endif

			if (roundsToJobCompletion > 0 && isNewRound)
			{
				roundsToJobCompletion --;

				#ifndef ONE_COMPOUND_PER_FACTION
					if (!CanWithdrawFromStorage(v,this->GetConsumedResourceType(),resourceCostLeft))
					{
						LogEvent(EventClass::Preset::operationAborted,&v,"Insufficient "+String(this->GetConsumedResourceType().ToString())+" available (need "+String(withdrawNow)+"). Aborting operation.");

						v.motionTime += timeEpsilon;
						ApplicationInterface::SignalOperationLacksResources(v,this->GetConsumedResourceType(),v.motionTime);
						v.motionTime += timeEpsilon;

						return TerminalFailure;
					}
				#endif
			}
		
			bool rs = true;
			if (roundsToJobCompletion<=0)
			{

				LogEvent(EventSignificance::Common,&v,"'"+GetName()+"' completed");
				ChangeStorage(v,v.entityTime, this->GetConsumedResourceType(),-resourceCostLeft);

				resourceCostLeft = 0;

				if (jobAutoCompletion == BaseJob::Autocomplete::Always 
					#ifndef NO_SATELLITES
						|| 
						(jobAutoCompletion == BaseJob::Autocomplete::IfSatellite && v.IsSatellite())
					#endif
					)
				{
					auto lock = World::actual.LockOnChange();
					continue_t rs = Finish(v,focus);
					if (rs == TerminalFailure || rs == TerminalFailureVehicleDestroyed)
						return rs;
					ASSERT__(rs != ContinueJob);
					if (movesToFocusWhenDone)
					{
						v.entityTime += timeEpsilon;
						if (v.PlaceExactlyAt(focus,0.1f,0) == MobilePlacementResult::PlacementFailedMobileDestroyed)
							return BaseJob::TerminalFailureVehicleDestroyed;
						v.entityTime += timeEpsilon;
					}
				}
				else
				{
					//v.unstable = true;
					v.completeOnMove = shared_from_this();
				}
				return JobDone;
			}
			else
			{
				if (resourceCostLeft > 0 && isNewRound)
				{
					int capacity = v.GetOperationSpeed(GetOperationAffinity());
					ChangeStorage(v,v.entityTime,this->GetConsumedResourceType(),-capacity);
					resourceCostLeft -= capacity;
				}
			}
			#ifdef CLIENT
				if (selectedEntity.get() == &v)
					UpdateChoices();
			#endif
			return ContinueJob;
		}

	
		/*virtual override*/ bool	DigHorizontal::CanContinue(MobileEntity&v, const Grid::LCellIdI&focus, String&outError)
		{
			if (focus.height != v.location.height)
			{
				outError =  "Focus height ("+String(focus.height)+") does not match vehicle height ("+String(v.location.height)+")";
				return false;
			}
			if (focus.stackID == v.location.stackID)
			{
				outError =  "Focus location ("+String(focus.stackID)+") matches vehicle location ("+String(v.location.stackID)+"). Should diverge";
				return false;
			}

			const auto&n = Grid::low.grid.GetCell(v.location.stackID).neighborhood;
			for (int i = 0; i < n.numNeighbors; i++)
				if (n.neighborID[i] == focus.stackID)
					return true;
			
			outError =  "Focus location ("+String(focus.stackID)+") is not in neighborhood of vehicle location ("+String(v.location.stackID)+")";
			return false;
		}

		/*virtual override*/ BaseJob::continue_t	DigHorizontal::Finish(MobileEntity&v, const Grid::LCellIdI&focus)
		{			
			World::actual.Clear(focus,v.entityTime);
			if (v.IsGroundBoundVehicle() && ((Vehicle&)v).base->requirements == Component::VehicleBase::Requirement::Water)
					World::actual.Overwrite(focus-1,Grid::Content::Water,v.entityTime);
			if (World::actual.LowCellIsClear(focus))
			{
				if (v.PlaceExactlyAt(focus,Times::Dig,v.entityTime) == MobilePlacementResult::PlacementFailedMobileDestroyed)
					return BaseJob::TerminalFailureVehicleDestroyed;
			}
			return JobDone;
			//v.motionTime += Times::Dig;
		}

		/*virtual override*/ BaseJob::continue_t	BuildRoad::Finish(MobileEntity&v, const Grid::LCellIdI&focus)
		{
			//can't use this because it doesn't understand directions:
			//	Structure::CommonFixTerrain(&v,focus,Grid::CellStack::Cell::Content::Road,0,true,v.motionTime,true,true,true);

			const Grid::LCellIdI target = focus;
			//ASSERT__(prevCell != target.stackID);

			const UINT32 finer = Grid::low.grid.GetCell(target.stackID).higherEquivalent;
			const auto&c = Grid::high.grid.GetCell(finer);
			int bottom = std::max(Grid::CellStack::BottomHeight, target.height-2);

			auto seq = World::actual.BeginChangeSequence(v.entityTime);

			{
				seq.SelectStack(finer);
				if (seq.Change(target.height - 1, Grid::Content::Road))
				{
					int h = target.height-2;
					if (!Grid::CellStack::bottom.IsClearOrWaterIgnoreStructure())
						while (h >= bottom && seq.ChangeConditionally(h,Grid::Content::Earth,World::Condition::IsClearOrWater))
							h--;
				}
			}
			for (int j = 0; j < c.neighborhood.numNeighbors; j++)
			{
				UINT32 n = c.neighborhood.neighborID[j];
				seq.SelectStack(n);
				if (prevCell != target.stackID &&
					((Grid::IsHighPartOfLow(n, prevCell) || Grid::IsHighPartOfLow(n, nextCell)) && seq.GetCellI(target.height-1).content == Grid::Content::Clear))
						continue;
				if (seq.Change(target.height - 1, Grid::Content::Road))
				{
					int h = target.height-2;
					if (!Grid::CellStack::bottom.IsClearOrWaterIgnoreStructure())
						while (h >= bottom && seq.ChangeConditionally(h,Grid::Content::Earth,World::Condition::IsClearOrWater))
							h--;
				}


			}
			return JobDone;
		}


		/*virtual override*/ BaseJob::begin_t			BuildWallUp::BeginHere(MobileEntity&v, const Grid::LCellIdI&focus, bool hasToMoveFirst)
		{
			//focus is one ABOVE vehicle location!

			if (hasToMoveFirst)
			{
				LogEvent(EventClass::Preset::commandRejected,&v, GetName()+": Would have to move first. But cannot move");
				return CannotStartAbort;
			}
			const Grid::LCellIdI target = FocusToBuildLocation(focus);
			PStructure existing = GetStructure(target);	//this isn't caught by LowCellIsClear() since we don't want connectors to be submerged either
			if (existing)
			{
				LogEvent(EventClass::Preset::commandRejected,&v,GetName()+": Destination occupied by "+existing->GetName());
				return CannotStartAbort;
			}


			const auto&stack = World::actual.GetStack(Grid::low.grid.GetCell(focus.stackID).higherEquivalent);
			if (stack.GetCellI(v.location.height-1).IsClearOrWater() && focus == v.location+1)
			{
				//focus --;
				Grid::LCellIdI f2 = focus;
				f2--;
				v.RegisterNewFrontJob("", PVehicleJob(new BuildWallUp(v)),shared_from_this(), &f2);
				return Delayed;
			}

			if (movesToFocusWhenDone && !World::actual.LowCellIsClear(focus))
			{
				if (!World::actual.LowCellHasStructure(focus))
				{
					LogEvent(EventSignificance::Common,&v,GetName()+": Obstruction detected. Resolution: clearing upper terrain.");
					PVehicleJob job;
					if (focus == v.location + 1)
					{
						job.reset(new ClearAbove(v));
					}
					//else
					//{
					//	if (focus.height == v.location.height)
					//		job.reset(new DigHorizontal(v));
					//}
					Grid::LCellIdI up = focus;
					if (job)
					{
						v.RegisterNewFrontJob("", job,shared_from_this(), &up);
						return Delayed;
					}
					else
					{
						LogEvent(EventSignificance::Common,&v,GetName()+": Obstruction detected. Focus cell is not above or horizontal to vehicle. Cannot remove. Aborting.");
						return CannotStartAbort;
					}
				}
				else
				{
					LogEvent(EventSignificance::Common,&v,GetName()+": Obstruction detected. Cannot remove. Aborting.");
					return CannotStartAbort;
				}
			}
			return Started;
		}

		
		/*virtual override*/ bool			BuildWallUp::CanContinue(MobileEntity&v, const Grid::LCellIdI&focus, String&outError)
		{
			if (this->movesToFocusWhenDone && !World::actual.LowCellIsClear(focus))
			{
				outError = "Cannot build wall here: "+ToString(focus)+" is not empty";
				return false;
			}

			Grid::LCellIdI target = FocusToBuildLocation(focus);
			PStructure existing = GetStructure(target);	//this isn't caught by LowCellIsClear() since we don't want connectors to be submerged either
			if (existing)
			{
				outError = "Cannot build wall here: "+ToString(target)+" is occupied by "+existing->GetName();
				return false;
			}
			return true;
		}


		/*virtual	override*/ BaseJob::continue_t	BuildWallUp::Finish(MobileEntity&v, const Grid::LCellIdI&focus)
		{
			Grid::LCellIdI target = FocusToBuildLocation(focus);


			auto seq = World::actual.BeginChangeSequence(v.entityTime);

			const UINT32 higher = Grid::low.grid.GetCell(target.stackID).higherEquivalent;
			seq.SelectStack(higher);
			seq.Change(target.height,Grid::Content::Foundation);


			const auto&n = Grid::low.grid.GetCell(target.stackID).neighborhood;
			for (int j = 0; j < n.numNeighbors; j++)
			{
				const UINT32 higher2 = Grid::low.grid.GetCell(n.neighborID[j]).higherEquivalent;
				seq.SelectStack(higher2);
				if (ConnectTo(seq.GetCellI(target.height).content))
				{
					UINT32 n = BaseGrid::RequireIntersection(Grid::high.grid.GetCell(higher).neighborhood, Grid::high.grid.GetCell(higher2).neighborhood);
					seq.SelectStack(n);
					seq.Change(target.height,Grid::Content::Foundation);
				}
			}





			//SignalClusterAndNeighborsChange(target,v.motionTime);
			return JobDone;
		}

		/*virtual	override*/ BaseJob::continue_t	AlterTerrain::Finish(MobileEntity&v, const Grid::LCellIdI&focus)
		{
			const Grid::LCellIdI target = focus;

			auto seq = World::actual.BeginChangeSequence(v.entityTime);

			if (mode == CursorMode::BuildBlockMode)
			{
				#ifdef HYBRID_GRID
					Grid::high.UpdatePlanarDistanceField(Grid::low.grid.GetCell(target.stackID).higherEquivalent, 8, Objects::vehicleDistanceField);
				#else
					Grid::high.UpdatePlanarDistanceField(target.stackID, 4, Objects::vehicleDistanceField);
				#endif

				foreach(selection, cell)
				{
					if (cell->stackID >= Grid::HighSetup::GridType::CellCount )
					{
						LogEvent(EventClass::Preset::unexpectedBehavior,&v,GetName()+": Invalid block index "+String(cell->stackID));
						return JobDone;
					}
					if (Objects::vehicleDistanceField.distance[cell->stackID] > 3)
					{
						continue;
					}
					if (abs(cell->height - target.height) > maxHeightDifference)
					{
						continue;
					}
					seq.Change(*cell,Grid::Content::Foundation);
				}
			}
			if (mode == CursorMode::RemoveBlockMode)
			{
				#ifdef HYBRID_GRID
					Grid::high.UpdatePlanarDistanceField(Grid::low.grid.GetCell(target.stackID).higherEquivalent, 8, Objects::vehicleDistanceField);
				#else
					Grid::high.UpdatePlanarDistanceField(target.stackID, 4, Objects::vehicleDistanceField);
				#endif

				foreach(selection, cell)
				{
					if (cell->stackID >= Grid::HighSetup::GridType::CellCount )
					{
						LogEvent(EventClass::Preset::unexpectedBehavior,&v,GetName()+": Invalid block index "+String(cell->stackID));
						return JobDone;
					}
					if (Objects::vehicleDistanceField.distance[cell->stackID] > 3)
					{
						continue;
					}
					if (abs(cell->height - target.height) > 1 )
					{
						continue;
					}
					seq.Clear(*cell);
				}
			}
			return JobDone;
		}

		/*virtual override*/ bool	ClearAbove::CanContinue(MobileEntity&v, const Grid::LCellIdI&focus, String&outError)
		{
			if (focus.height != v.location.height+1)
			{
				outError = "Focus height ("+String(focus.height)+") expected one above vehicle height ("+String(v.location.height)+")";
				return false;
			}
			if (focus.stackID == v.location.stackID)
				return true;

			const auto&n = Grid::low.grid.GetCell(v.location.stackID).neighborhood;
			for (int i = 0; i < n.numNeighbors; i++)
				if (n.neighborID[i] == focus.stackID)
					return true;
					
			outError = "Focus cell ("+String(focus.stackID)+") expected around or on top of vehicle ("+String(v.location.height)+")";
			return false;
		}

		/*virtual override*/ BaseJob::continue_t	ClearAbove::Finish(MobileEntity&v, const Grid::LCellIdI&focus)
		{
			World::actual.Clear(focus,v.entityTime);
			return JobDone;
		}



		#ifndef ONE_COMPOUND_PER_FACTION

			/*virtual override*/ bool	TransferJob::CanContinue(MobileEntity&v, const Grid::LCellIdI&focus, String&outError)
			{
				PWorldEntity e = GetEntity(focus);
			
				if (!e || !v.owner->CanSee(e,v.motionTime) || e.get() == &v)
				{
					outError = "Nothing in focus cell "+focus.ToString();
					return false;
				}
				if (e->owner != v.owner)
				{
					outError = "Access to '"+e->GetNiceName()+"' denied";
					return false;
				}
				return true;
			}


			/*virtual override*/ BaseJob::continue_t	TransferJob::Finish(MobileEntity&v, const Grid::LCellIdI&focus)
			{
				PStructure target = GetStructure(focus);
				if (target && target->owner == v.owner)
				{
					Compound &n = compounds[target->compoundID];
					if (mode == CursorMode::RestockMode)
					{
						ResourceVector demand = v.GetMaxStorageCapacity();
						demand -= v.currentStorage;
						if (demand.IsEmpty())
						{
							LogEvent(EventSignificance::Common,&v,"Nothing to restock. Storage is full.");
						}
						else
						{
							demand.Cap(n.total.storage);
							if (demand.IsNotEmpty())
							{
								LogEvent(EventSignificance::Common,&v,"Restocking vehicle: transferred "+demand.ToString());
								n.Collect(demand,v.motionTime);
								v.currentStorage += demand;
								v.RegisterChange(WorldEntity::StorageChanged,v.motionTime);
							}
							else
								LogEvent(EventClass::Preset::commandRejected,&v, "Compound: Nothing to withdraw");
						}
					}
					else
					{
						ResourceVector supply = v.currentStorage;
						ResourceVector networkCapacity = n.total.storageMax;
						networkCapacity -= n.total.storage;
						if (supply.IsEmpty())
						{
							LogEvent(EventClass::Preset::commandRejected,&v,"Insufficient resources to complete operation");
						}
						else
						{
							supply.Cap(networkCapacity);

							if (supply.IsNotEmpty())
							{
								LogEvent(EventSignificance::Common,&v,"Deposition: transferred "+supply.ToString());

								n.Distribute(supply,v.motionTime);
								v.currentStorage -= supply;
								v.RegisterChange(WorldEntity::StorageChanged,v.motionTime);
							}
							else
								LogEvent(EventClass::Preset::commandRejected,&v,"Compound: Insufficient resource storage capacity at destination to complete operation");
						}
					}
				}
				else
				{
					PMobileEntity vTarget = GetMobileEntity(focus);
					if (!vTarget || !v.owner->CanSee(vTarget,v.motionTime))
					{
						LogEvent(EventClass::Preset::commandRejected,&v,"Nothing here");
						return JobDone;
					}
					if (vTarget->owner != v.owner)
					{
						LogEvent(EventClass::Preset::commandRejected,&v,"Access denied");
						return JobDone;
					}
					if (mode == CursorMode::RestockMode)
					{
						ResourceVector demand = v.GetMaxStorageCapacity();
						demand -= v.currentStorage;
						if (demand.IsEmpty())
						{
							LogEvent(EventSignificance::Common,&v,"Nothing to restock. Storage is full.");
						}
						else
						{
							demand.Cap(vTarget->currentStorage);
							if (demand.IsNotEmpty())
							{
								LogEvent(EventSignificance::Common,&v,"Restocking vehicle: transferred "+demand.ToString());
								vTarget->currentStorage -= demand;
								v.currentStorage += demand;
								vTarget->RegisterChange(WorldEntity::StorageChanged,v.motionTime);
								v.RegisterChange(WorldEntity::StorageChanged,v.motionTime);
							}
							else
								LogEvent(EventClass::Preset::commandRejected,&v, vTarget->GetNiceName()+": Insufficient resource availability to complete operation");
						}
					}
					else
					{
						ResourceVector supply = v.currentStorage;
						ResourceVector remoteCapacity = vTarget->GetMaxStorageCapacity();
						remoteCapacity -= vTarget->currentStorage;
						if (supply.IsEmpty())
						{
							LogEvent(EventClass::Preset::commandRejected,&v,"Insufficient resources to complete operation");
						}
						else
						{
							supply.Cap(remoteCapacity);

							if (supply.IsNotEmpty())
							{
								LogEvent(EventSignificance::Common,&v,"Deposition: transferred "+supply.ToString());

								vTarget->currentStorage += supply;
								v.currentStorage -= supply;
								vTarget->RegisterChange(WorldEntity::StorageChanged,v.motionTime);
								v.RegisterChange(WorldEntity::StorageChanged,v.motionTime);
							}
							else
								LogEvent(EventClass::Preset::commandRejected,&v,vTarget->GetNiceName()+ ": Insufficient resource storage capacity at destination to complete operation");
						}
					}

				}
				return JobDone;
			}

		#endif



		/*virtual override*/ bool	SelfDetonate::CanContinue(MobileEntity&v, const Grid::LCellIdI&focus, String&outError)
		{
			PWorldEntity e = GetEntity(focus);
			
			if (!e)
			{
				outError = "Self is empty";
				return false;
			}
			return true;
		}


		/*virtual override*/ BaseJob::continue_t	SelfDetonate::Finish(MobileEntity&v, const Grid::LCellIdI&focus)
		{
			float radius = Component::Bomb::GetRadius(doomsday,v.owner);
			int damageLevel = Component::Bomb::GetDamageLevel(doomsday,v.owner);

			Remove(v.shared_from_this(),Objects::RemovalType::ControlledExplosion,v.entityTime);
			v.entityTime += timeEpsilon;

	
			ApplicationInterface::TAttack at;
			at.attackFrom = v.shared_from_this();
			Explode(at,Grid::HCellIdI(Grid::low.grid.GetCell(v.location.stackID).higherEquivalent,v.location.height),v.GetWorldPosition(),radius,damageLevel,doomsday,v.entityTime);

			return TerminalFailureVehicleDestroyed;
		}



		
		/*virtual override*/	void			SelfDetonate::Serialize(IWriteStream&outStream) const 
		{
			outStream.WritePrimitive(doomsday);
		}

		/*virtual override*/	void			SelfDetonate::Deserialize(IReadStream&inStream)
		{
			inStream.ReadPrimitive(doomsday);
		}		
		
		/**/					SelfDetonate::SelfDetonate(MobileEntity&v, ConstName p)	
			: Super(
				p == ConstName::DetonationSize_Doomsday && RuntimeConfig::doomsdayWeapons != RuntimeConfig::DoomsdayWeapons::Disallow 
					? DoomsdaySelfDetonationCosts 
					: RegularSelfDetonationCosts,
				v,GetOperationAffinity(),Autocomplete::Always,false
			),
			doomsday(p == ConstName::DetonationSize_Doomsday && RuntimeConfig::doomsdayWeapons != RuntimeConfig::DoomsdayWeapons::Disallow)
		{}

		/*virtual override*/ String			SelfDetonate::GetName() const
		{
			return doomsday?"Obliterate (DOOMSDAY)" : "Self-Denotate";
		}




		/*virtual override*/ bool	RepairJob::CanContinue(MobileEntity&v, const Grid::LCellIdI&focus, String&outError)
		{
			if (!v.CanRepair())
			{
				outError = v.GetNiceName()+" cannot repair";
				return false;
			}
			PWorldEntity e = GetEntity(focus);
			
			if (!e || !v.owner->CanSee(e,v.entityTime) || e.get() == &v)
			{
				outError = "Nothing in focus cell "+ToString(focus);
				return false;
			}
			if (e->health == e->GetMaxHealth())
			{
				outError = "'"+e->GetNiceName()+"' is at full health";
				return false;
			}
			//if (e->owner != v.owner)
			//{
			//	LogEvent(EventClass::Preset::commandRejected,&v,this->GetNiceName()+ ": Access to '"+e->GetNiceName()+"' denied");
			//	return false;
			//}
			return true;
		}


		/*virtual override*/ BaseJob::continue_t	RepairJob::Finish(MobileEntity&v, const Grid::LCellIdI&focus)
		{
			if (!v.CanRepair())
				return JobDone;
			PWorldEntity target = GetEntity(focus);
			if (!target || !v.owner->CanSee(target,v.entityTime) || target.get() == &v)
			{
				LogEvent(EventClass::Preset::commandRejected,&v,this->GetName()+ ": Nothing in focus cell "+ToString(focus));
				return JobDone;
			}
			if (target->health == target->GetMaxHealth())
			{
				LogEvent(EventClass::Preset::commandRejected,&v,this->GetName()+ ": '"+target->GetNiceName()+"' is at full health");
				return JobDone;
			}
			const int maxHealth = target->GetMaxHealth();
			const int missing = maxHealth - target->health;
			float relative = float(missing)/float(maxHealth);
			if (relative <= 0)
			{
				LogEvent(EventClass::Preset::error,target.get(),this->GetName()+ ": Invalid health state ("+String(target->health)+")");
				return JobDone;
			}

			int cost = (int)ceil(float(target->GetConstructionCost()) * relative);

			if (CanWithdrawFromStorage(v,this->GetConsumedResourceType(),cost))
			{
				target->health = maxHealth;
				target->RegisterChange(WorldEntity::HealthChanged,v.entityTime);
				ChangeStorage(v,v.entityTime, this->GetConsumedResourceType(),-cost);
				LogEvent(EventClass( EventSignificance::Uncommon, EventFlavor::Gain ) ,&v,this->GetName()+ ": '"+target->GetNiceName()+"' repaired");
				return JobDone;
			}

			int canRepair = (int)floor(float(maxHealth) * float(GetStorage(v,this->GetConsumedResourceType())) / float(target->GetConstructionCost()));
			if (!canRepair)
			{
				LogEvent(EventClass::Preset::commandRejected,&v,this->GetName()+ ": Insufficient "+String(this->GetConsumedResourceType().ToString())+" in storage to repair '"+target->GetNiceName()+"'");

				v.entityTime += timeEpsilon;
				ApplicationInterface::SignalOperationLacksResources(v,this->GetConsumedResourceType(),v.entityTime);
				v.entityTime += timeEpsilon;

				return JobDelayed;
			}
			LogEvent(EventClass::Preset::commandRejected,&v,this->GetName()+ ": Insufficient "+String(this->GetConsumedResourceType().ToString())+" in storage to fully repair '"+target->GetNiceName()+"'. Repairing "+String(canRepair)+" health...");

			v.entityTime += timeEpsilon;
			ApplicationInterface::SignalOperationLacksResources(v,this->GetConsumedResourceType(),v.entityTime);
			v.entityTime += timeEpsilon;

			target->health += canRepair;
			target->RegisterChange(WorldEntity::HealthChanged,v.entityTime);

			ChangeStorage(v,v.entityTime, this->GetConsumedResourceType(),-(int)ceil(float(canRepair) * float(target->GetConstructionCost()) / float(maxHealth)));
			return JobDone;

		}

		/*virtual override*/ bool	DigDown::CanContinue(MobileEntity&v, const Grid::LCellIdI&focus, String&outError)
		{
			if (focus.height +1 != v.location.height)
			{
				outError = "MobileEntity expected at height "+String(focus.height +1)+" but found at "+String(v.location.height);
				return false;
			}
			if (focus.stackID != v.location.stackID)
			{
				outError = "MobileEntity expected at cell "+String(focus.stackID)+" but found at "+String(v.location.stackID);
				return false;
			}
			return true;
		}

		/*static*/ bool	DigDown::Succeeded(MobilePlacementResult rs)
		{
			return rs == MobilePlacementResult::PlacementOkay || rs == MobilePlacementResult::PlacementOkayFellDown;
		}


		/*virtual override*/ BaseJob::continue_t	DigDown::Finish(MobileEntity&v, const Grid::LCellIdI&focus)
		{
			const Grid::LCellIdI target = focus;
			#ifdef HYBRID_GRID
				const UINT32 finer = Grid::low.grid.GetCell(target.stackID).higherEquivalent;
				const auto&c = Grid::high.grid.GetCell(finer);
			#else
				const UINT32 finer = target.stackID;
			#endif

			auto seq = World::actual.BeginChangeSequence(v.entityTime);

			seq.Clear(Grid::HCellIdI(finer,target.height));

			#ifdef HYBRID_GRID
				for (int j = 0; j < c.neighborhood.numNeighbors; j++)
				{
					UINT32 n = c.neighborhood.neighborID[j];
					seq.Clear(Grid::HCellIdI(n,target.height));
				}
			#endif

			MobilePlacementResult rc = v.PlaceExactlyAt(target, Times::Dig,v.entityTime);
			if (!Succeeded(rc))
			{
				if (rc == MobilePlacementResult::PlacementFailedMobileDestroyed)
					return TerminalFailureVehicleDestroyed;
				if (!Succeeded(v.PlaceExactlyAt(v.location,0,v.entityTime)))
					return TerminalFailureVehicleDestroyed;
			}
					//or else it falls, breaking the task list. HOWEVER: must be done AFTER signalling neighbors because they must have a chance to Fill the hole again.
			return JobDone;
		}
			


		/*virtual override*/	void			Construct::Serialize(IWriteStream&outStream) const 
		{
			outStream.Write(useID.bytes,(serial_size_t)useID.NumBytes);
			SerialSync(outStream,constructWhat->GetTypeName());
		}



		/*virtual override*/	void			Construct::Deserialize(IReadStream&inStream)
		{
			String whatName;
			if (ApplicationInterface::globalVersionOfCurrentFile >= ApplicationInterface::FileVersions::HasUUIDInConstructionJobs)
			{
				inStream.Read(useID.bytes,(serial_size_t)useID.NumBytes);
			}
			else
				useID.Generate();
			SerialSync(inStream,whatName);

			constructWhat = Prototypes::structures.FindByName(whatName);
			if (!constructWhat)
			{
				if (whatName == "Artillery Tower" || whatName=="Cannon Tower")
					constructWhat = Prototypes::structures.Find(EntityType(ConstName::Type_Tower,ConstName::Turret_Launcher));
				if (!constructWhat)
					throw Except::Memory::SerializationFault(CLOCATION,"Unable to determine construction type of '"+whatName+"'");
			}
			jobAutoCompletion = !!To<Connector>::DynamicCast(constructWhat) ? Autocomplete::Always : Autocomplete::IfSatellite;
		};

		/*virtual override*/ void			Construct::Restart(const MobileEntity&v)
		{
			Super::ResetResourceAndRoundsLeft(constructWhat?constructWhat->GetConstructionCost():0,v,GetOperationAffinity());
		}



		/**/					Construct::Construct(MobileEntity&v, const PStructure&c) : 
			BaseJob(c->GetConstructionCost(),v,GetOperationAffinity(),!!To<Connector>::DynamicCast(c) ? Autocomplete::Always : Autocomplete::IfSatellite,false),constructWhat(c)
		{
			useID.Generate();
		}

		/*virtual override*/ String			Construct::GetName() const
		{
			return "construct "+(constructWhat ? constructWhat->GetTypeName() : "???");
		}

		/*virtual override*/ bool			Construct::CanContinue(MobileEntity&v, const Grid::LCellIdI&, String&outError)
		{
			Grid::LCellIdI location = v.GetConstructionLocation();
			Structure::placement_t placement = constructWhat->CheckPlacement(location);
			if (placement != Structure::CanPlace)
			{
				outError = "Placement not possible: "+Structure::ToString(placement);
				return false;
			}
			PWorldEntity other = GetEntity(location);
			if (other && other.get() != &v)
			{
				outError = "Production location occupied by "+other->GetNiceName();
				return false;
			}

			return v.owner->CanSpawnType(constructWhat,outError);
		}



		/*virtual override*/ Construct::begin_t	Construct::BeginHere(MobileEntity&v, const Grid::LCellIdI&, bool hasToMoveFirst)
		{
			//Grid::LCellIdI target = focus;
			if (hasToMoveFirst)
			{
				LogEvent(EventClass::Preset::commandRejected,&v, GetName()+": Would have to move first. But cannot move");
				return CannotStartAbort;
			}
			{
				String error;
				if (!v.owner->CanSpawnType(constructWhat,error))
				{
					LogEvent(EventClass::Preset::commandRejected,&v, GetName()+": "+error);
					return CannotStartAbort;
				}
			}
			
			const Grid::LCellIdI constLoc = v.GetConstructionLocation();
			switch (constructWhat->CheckPlacement(constLoc))
			{
//				LogEvent(EventSignificance::Common,&v,GetName()+": Unable to place without modifications.");

				//case Structure::LacksCentralEarthFoundation:
				//{
				//	LogEvent(EventSignificance::Common,&v,GetName()+": Central earth foundation missing. Resolution: placing earth block underneath.");

				//	#ifdef HYBRID_GRID
				//		const UINT32 finer = Grid::low.grid.GetCell(v.location.stackID).higherEquivalent;
				//	#else
				//		const UINT32 finer = v.location.stackID;
				//	#endif

				//	const auto&stack = World::actual.cellStack[finer];
				//	Grid::HCellIdI ground(finer,v.location.height-1);

				//	int h = v.location.height - 2;
				//	int cnt = 0;
				//	while (h > -Grid::CellStack::HalfRange )
				//	{
				//		if (!stack.GetCellI(h).IsClearOrWater())
				//			break;

				//		h--;
				//		cnt++;
				//	}
				//	h++;

				//	Ctr::Array<Grid::HCellIdI> selection(1 + cnt);
				//	selection.First() = ground;
				//	for (int i = 1; i <= cnt; i++)
				//		selection[i] = ground - i;
				//	auto job = new Objects::Job::AlterTerrain(v,MobileEntity::BuildBlockMode,selection);
				//	job->maxHeightDifference = 1 + cnt;
				//	PVehicleJob foundationJob(job);
				//	//maxHeightDifference

				//	v.RegisterNewFrontJob("", foundationJob,nullptr);
				//}
				//return Delayed;
				//case Structure::LacksFullFoundation:
				//{
				//	const auto & n = Grid::low.grid.GetCell(v.location.stackID).neighborhood;
				//	bool buildFoundation = true;
				//	for (int i = 0; i < n.numNeighbors; i++)
				//	{
				//		PWorldEntity entity = GetEntity(Grid::LCellIdI(n.neighborID[i],v.location.height-1));
				//		if (entity)
				//		{
				//			LogEvent(EventClass::Preset::operationAborted,&v,GetName()+": Unable to fuild foundation. "+entity->GetNiceName()+" in the way.");

				//			buildFoundation = false;
				//			break;
				//		}
				//	}
				//	if (buildFoundation)
				//	{
				//		PVehicleJob foundationJob(new BuildFoundation(v));
				//		v.RegisterNewFrontJob("", foundationJob,nullptr);
				//	}
				//	else
				//	{
				//		LogEvent(EventSignificance::Common,&v,GetName()+": Unable to fuild foundation. Attempting to dig down.");
				//		bool digDown = true;
				//		for (int i = 0; i < n.numNeighbors; i++)
				//			if (To<WorkingStructure>::DynamicCast( GetStructure(Grid::LCellIdI(n.neighborID[i],v.location.height))))
				//			{
				//				LogEvent(EventClass::Preset::operationAborted,&v,GetName()+": Cannot dig down: Neighboring working structure found.");
				//				digDown = false;
				//				break;
				//			}
				//		if (digDown && GetStructure(Grid::LCellIdI(v.location)))
				//		{
				//			LogEvent(EventClass::Preset::operationAborted,&v,GetName()+": Cannot dig down: Connector in place.");
				//			digDown = false;
				//		}
				//		if (!digDown)
				//		{
				//			buildFoundation = true;
				//			for (int i = 0; i < n.numNeighbors; i++)
				//			{
				//				PWorldEntity entity = GetEntity(Grid::LCellIdI(n.neighborID[i],v.location.height-1));
				//				if (entity && !To<Connector>::DynamicCast(entity))
				//				{
				//					buildFoundation = false;
				//					break;
				//				}
				//			}
				//			if (!buildFoundation)
				//				return CannotStartAbort;
				//			else
				//			{
				//				LogEvent(EventClass::Preset::operationAborted,&v,GetName()+": Blocking entity is connector. Proceeding anyway.");
				//				PVehicleJob foundationJob(new BuildFoundation(v));
				//				v.RegisterNewFrontJob("", foundationJob,nullptr);
				//			}
				//		}
				//		if (!buildFoundation)
				//		{
				//			LogEvent(EventSignificance::Common,&v,GetName()+": Resolution: digging down.");

				//			PVehicleJob job(new DigDown(v));
				//			Grid::LCellIdI down = v.location-1;
				//			v.RegisterNewFrontJob("", job,&down);
				//		}
				//	}
				//}
				//return Delayed;
				case Structure::GroundLevelObstruction:
					LogEvent(EventClass::Preset::operationAborted,&v,GetName()+": Obstruction detected. Aborting.");
				return CannotStartAbort;
				case Structure::UpperLevelObstruction:
				{
					if (GetStructure(constLoc+1) || GetStructure(constLoc+2))
					{
						LogEvent(EventClass::Preset::operationAborted,&v,GetName()+": Structure obstruction detected. Cannot remove. Aborting.");
						return CannotStartAbort;
					}
					LogEvent(EventSignificance::Common,&v,GetName()+": Obstruction detected. Resolution: clearing upper terrain.");
					PVehicleJob job(new ClearAbove(v));
					Grid::LCellIdI up = constLoc+1;
					v.RegisterNewFrontJob("", job,shared_from_this(), &up);
				}
				return Delayed;
			}			
			LogEvent(EventSignificance::Common,&v,GetName()+": All fine. Commencing.");
			return Started;
			
		}

		/*virtual override*/ BaseJob::continue_t	Construct::Finish(MobileEntity&v, const Grid::LCellIdI&focus)
		{
			Grid::LCellIdI target = v.GetConstructionLocation(focus);
			auto check = constructWhat->CheckPlacement(target);
			if (check != Structure::CanPlace)
			{
				LogEvent(EventClass::Preset::commandRejected,&v,GetName()+" (finish): Unable to finish construction. Placement-check of "+constructWhat->GetTypeName()+" returned "+Structure::ToString(check));
				return TerminalFailure;
			}
			
			PStructure existing = GetStructure(target);
			if (existing)
			{
				if (dynamic_cast<Connector*>(existing.get()))
				{
					Remove(existing,Objects::RemovalType::BeingReplaced,v.entityTime);
					v.entityTime += 0.025f;
					existing.reset();
					DBG_ASSERT__(!GetStructure(target));
				}
				else
				{
					LogEvent(EventClass::Preset::commandRejected,&v, GetName()+" (finish): Unable to place " +constructWhat->GetTypeName() + " here. "+existing->GetNiceName()+" is in the way");
					return TerminalFailure;
				}
			}

			PStructure s = To<Structure>::Cast(constructWhat->CreateNewInstance(v.owner, target, v.entityTime));
			s->id = useID;
			LogEvent(EventSignificance::Common,&v,"Constructed " + s->GetName()+" at "+ToString(s->location));
			InsertConstructed(s,v.entityTime,true);
			return JobDone;
		}

		/*virtual override*/ String			Sequence::GetName() const
		{
			switch (mode)
			{
				case CursorMode::BuildRoadChainMode:
					return "Build Road to...";
				case CursorMode::BuildConnectorChainMode:
					return "Build Connectors to...";
				case CursorMode::BuildHighWallChainMode:
					return "Build High Wall to...";
				case CursorMode::BuildLowWallChainMode:
					return "Build Low Wall to...";
				case CursorMode::BuildBridgeMode:
					return "Build Bridge to...";
			}
			return "???...";
		}

		/*virtual override*/	void			Sequence::Deserialize(IReadStream&inStream)
		{
			UINT16 i16;
			inStream.ReadPrimitive(i16);
			mode = (CursorMode)i16;

			if (ApplicationInterface::globalVersionOfCurrentFile == 1035)
			{
				bool dead = false;
				inStream.ReadPrimitive(lastLocation);
				inStream.ReadPrimitive(first);
				inStream.ReadPrimitive(dead);
				Super::Deserialize(inStream);
				return;
			}
			if (ApplicationInterface::globalVersionOfCurrentFile ==1036)
			{
				String dummy;
				inStream.ReadPrimitive(lastLocation);
				inStream.ReadPrimitive(first);
				SerialSync(inStream,dummy);
				Super::Deserialize(inStream);
				return;
			}

			inStream.ReadPrimitive(lastLocation);
			inStream.ReadPrimitive(first);
			Super::Deserialize(inStream);
		}

		bool					Sequence::DoneHere(const World::Data&world,const Grid::LCellIdI&cell, const Grid::LCellIdI&nextCell)	const
		{
			switch (mode)
			{
				case CursorMode::BuildRoadChainMode:
					return world.IsRoad(cell-1,true);
				case CursorMode::BuildBridgeMode:
				{
					if (!world.IsRoad(cell-1,true))
						return false;
					if (nextCell == cell)
						return true;
					if (nextCell.height != cell.height)
						return false;

					UINT32 inter = BaseGrid::RequireIntersection(
									Grid::high.grid.GetCell(Grid::low.grid.GetCell(cell.stackID).higherEquivalent).neighborhood,
									Grid::high.grid.GetCell(Grid::low.grid.GetCell(nextCell.stackID).higherEquivalent).neighborhood);
					return world.GetStack(inter).GetCellI(cell.height-1).content == Grid::Content::Road;
				}
				case CursorMode::BuildConnectorChainMode:
					return !!GetStructure(cell);
				case CursorMode::BuildHighWallChainMode:
				{
					const auto&stack = world.GetStack(Grid::low.grid.GetCell(cell.stackID).higherEquivalent);
					Grid::Content c0 = Grid::Content::Reinterpret(stack.GetCellI(cell.height-1).content),
									c1 = Grid::Content::Reinterpret(stack.GetCellI(cell.height-2).content);
					return c0 == Grid::Content::Foundation && c1 == Grid::Content::Foundation;
				}
				case CursorMode::BuildLowWallChainMode:
				{
					const auto&stack = world.GetStack(Grid::low.grid.GetCell(cell.stackID).higherEquivalent);
					Grid::Content c0 = Grid::Content::Reinterpret(stack.GetCellI(cell.height-1).content);
					return c0 == Grid::Content::Foundation;
				}
			}
			return true;
		}

		void			Sequence::Guess(const MobileEntity&v, const Grid::LCellIdI&origin, index_t myJobIndex, UINT16&roundsToJobCompletion, UINT16&resourceCostLeft)	const
		{
			roundsToJobCompletion = 0;
			resourceCostLeft = 0;

			const World::Data&world = v.owner->knownWorld;
			int speed = v.GetOperationSpeed(GetOperationAffinity());
			//if (destination != v.location)
			{
				auto next = path.IsNotEmpty() ? *path.begin() : origin;

				if (!DoneHere(world,origin,next))
				{
					UINT32 cost = 0;
					bool actual = false;
					Grid::LCellIdI at = v.location;
					switch (mode)
					{
						case CursorMode::BuildRoadChainMode:
						case CursorMode::BuildBridgeMode:
						{
							bool found = false;
							for (index_t i = 0; i < myJobIndex; i++)
							{
								const auto&task = v.myTasks[i];
								if (To<BuildRoad>::DynamicCast(task.job) && 
									(
										(task.foci.Count() == 1 && task.foci.Peek().target == origin)
										||
										(task.foci.Count() == 0 && at == origin)
									)
									)
								{
									found = true;
									break;
								}
								if (task.ContainsMovement())
									at = task.foci.GetNewest().target;
							}
							if (!found)
								cost = BuildRoadMineralCosts;
							resourceCostLeft += cost;
							roundsToJobCompletion += cost / speed + (cost % speed ? 1 : 0);
						}
						break;
						case CursorMode::BuildConnectorChainMode:
						{
							bool found = false;
							for (index_t i = 0; i < myJobIndex; i++)
							{
								const auto&task = v.myTasks[i];
								PConstruct cnst  = To<Construct>::DynamicCast(task.job);
								if (cnst && cnst->constructWhat == Connector::globalPrototype && task.foci.Count() == 1 && task.foci.Peek().target == origin)
								{
									found = true;
									break;
								}
							}
							if (!found)
								cost = Connector::globalPrototype->GetConstructionCost();
							resourceCostLeft += cost;
							roundsToJobCompletion += cost / speed + (cost % speed ? 1 : 0);
						}
						break;
						case CursorMode::BuildHighWallChainMode:
						{
							int found = 0;
							for (index_t i = 0; i < myJobIndex; i++)
							{
								const auto&task = v.myTasks[i];
								if (To<BuildHorizontalWall>::DynamicCast(task.job) && task.foci.Count() == 1 && task.foci.Peek().target.stackID == origin.stackID)
								{
									found ++;
								}
								else
									if (
										To<BuildWallUp>::DynamicCast(task.job) 
										&& 
										(
											(task.foci.Count() == 1 && task.foci.Peek().target.stackID == origin.stackID)
											||
											(task.foci.Count() == 0 && at.stackID == origin.stackID)
										)
									)
									{
										found ++;
									}
							}
							cost = BuildWallMineralCosts;
							resourceCostLeft += cost * (2 - found);
							roundsToJobCompletion += (cost / speed + (cost % speed ? 1 : 0))  * (2 - found);
						}
						break;
						case CursorMode::BuildLowWallChainMode:
						{
							int found = 0;
							for (index_t i = 0; i < myJobIndex; i++)
							{
								const auto&task = v.myTasks[i];
								if (To<BuildHorizontalWall>::DynamicCast(task.job) && task.foci.Count() == 1 && task.foci.Peek().target.stackID == origin.stackID)
								{
									found ++;
								}
							}
							cost = BuildWallMineralCosts;
							resourceCostLeft += cost * (1 - found);
							roundsToJobCompletion += ((cost + speed-1) / speed)  * (1 - found);
						}
						break;
					}
					
					
				}

				for (const auto&node : path)
				{
					if (node == origin)
						continue;


					auto next = (&node)+1;	//evil hack to get the next element


					if (!DoneHere(world,node, next == path.end() ? node : *next))
					{
						UINT32 cost = 0;
						bool actual = false;
						switch (mode)
						{
							case CursorMode::BuildRoadChainMode:
							case CursorMode::BuildBridgeMode:
								cost = BuildRoadMineralCosts;
								resourceCostLeft += cost;
								roundsToJobCompletion += cost / speed + (cost % speed ? 1 : 0);
							break;
							case CursorMode::BuildConnectorChainMode:
							{
								cost = Connector::globalPrototype->GetConstructionCost();
								resourceCostLeft += cost;
								roundsToJobCompletion += cost / speed + (cost % speed ? 1 : 0);
							}
							break;
							case CursorMode::BuildHighWallChainMode:
								cost = BuildWallMineralCosts;
								resourceCostLeft += 2 * cost;
								roundsToJobCompletion += 2*(cost / speed + (cost % speed ? 1 : 0));
							break;
							case CursorMode::BuildLowWallChainMode:
								cost = BuildWallMineralCosts;
								resourceCostLeft += cost;
								roundsToJobCompletion += (cost+speed-1) / speed ;
							break;
						}
					}
				}
			}

		}


		/*virtual override*/ bool				Sequence::UpdatePath(const MobileEntity&v, const Grid::LCellIdI&_origin, Grid::LCellIdI&destination)
		{
			const World::Data&world = v.owner->knownWorld;
//			Grid::LCellIdI destination = _destination;
			if (v.IsGroundBoundVehicle())
			{
				while (!world.LowCellIsClearIgnoreStructure(destination))
					destination++;
				//if (mode == CursorMode::BuildBridgeMode
				//	&&
				//	v.IsGroundBoundVehicle()
				//	&& !((Vehicle&)v).CanStandOn(world,destination-1,false,true))
				//		destination++;
				if (mode != CursorMode::BuildBridgeMode)
					while (world.LowCellIsClear(destination-1))
						destination--;
			}

			path.Clear();
			pathChanged = true;
			//path << pointOfOrigin;
			if (v.location != destination)
			{
				Grid::LCellIdI origin = _origin;
				//if (mode == MobileEntity::BuildWallChainMode && DoneHere(origin))
				//{
				//	origin--;	//prevent two-block fall
				//	origin--;	//prevent two-block fall
				//}
				if (mode == CursorMode::BuildBridgeMode)
					destination.height = origin.height;


				if (v.IsGroundBoundVehicle())
				{
					if (!AppendShortestConstructionPath((const Vehicle*)&v,
														this->mode == CursorMode::BuildRoadChainMode || this->mode == CursorMode::BuildBridgeMode,
														this->mode == CursorMode::BuildBridgeMode,
														 origin,destination,path))
					{

						return false;
					}
				}
				else
				{
					FATAL__("routing not implemented for non-vehicle mobile entities");
				}

			}
			return true;	
		}

		/*virtual override*/ bool		Sequence::IsImplicitlyDone(MobileEntity&v, const Grid::LCellIdI&_destination) const
		{
			const World::Data&world = v.owner->knownWorld;
			Grid::LCellIdI destination = _destination;
			if (v.IsGroundBoundVehicle())
			{
				while (!world.LowCellIsClear(destination))
					destination++;
				while (world.LowCellIsClear(destination-1))
					destination--;
			}

			auto next = path.IsNotEmpty() ? *path.begin() : v.location;
			if (destination == v.location && DoneHere(World::actual,v.location,next))
			{
				return true;
			}

			return false;
		}


		/*virtual override*/ Sequence::begin_t	Sequence::BeginHere(MobileEntity&v, const Grid::LCellIdI&_destination,bool hasToMoveFirst)
		{
			const World::Data&world = v.owner->knownWorld;
			Grid::LCellIdI destination = _destination;
			if (mode != CursorMode::BuildBridgeMode)
				while (!world.LowCellIsClearIgnoreStructure(destination))
					destination++;
			else
				if (v.IsGroundBoundVehicle() && !((Vehicle&)v).CanStandOn(world,destination,false,true))
					destination++;
			while (world.LowCellIsClear(destination-1))
				destination--;

			auto next = path.IsNotEmpty() ? *path.begin() : v.location;
			if (destination == v.location && DoneHere(World::actual,v.location,next))
			{
				roundsToJobCompletion = 0;
				return Started;
			}
			roundsToJobCompletion = 1;
			
			//if (destination != v.location)
			{
				if (!UpdatePath(v,v.location,destination))
				{
					LogEvent(EventClass::Preset::commandRejected,&v, GetName()+": Unable to plot path to destination");
					return CannotStartAbort;
				}

				auto next = path.IsNotEmpty() ? *path.begin() : v.location;
				bool nextIsLast = path.Count() <= 1;
				bool hasNext = path.Count() > 0;

				if (!DoneHere(World::actual,v.location,next) && !hasToMoveFirst)
				{
					switch (mode)
					{

						case CursorMode::BuildRoadChainMode:
						{
							//auto next = path.begin();
							//++next;
							if (
								(lastLocation != Grid::LCellIdI::Invalid &&lastLocation.height < v.location.height)
								 ||
								 (!nextIsLast && next.height < v.location.height))
							{
								//RegisterNewJob(nullptr, PJob(new Objects::Job::DigDown(*this)),nullptr);
								v.RegisterNewFrontJob("", PVehicleJob(
								new BuildRoad(v,
									lastLocation != Grid::LCellIdI::Invalid && lastLocation.height < v.location.height ? lastLocation.stackID : 0xFFFFFFFF,
									!nextIsLast && next.height < v.location.height ?next.stackID : 0xFFFFFFFF)),
									shared_from_this(), 
									nullptr);
							}
							else
								v.RegisterNewFrontJob("", PVehicleJob(new BuildRoad(v)),shared_from_this(), nullptr);
						}
						break;
						case CursorMode::BuildConnectorChainMode:
							v.RegisterNewFrontJob(Connector::globalPrototype->GetTypeName(), 
								PVehicleJob(new Construct(v,Connector::globalPrototype)),
								shared_from_this(),
								&v.location);
						break;
						case CursorMode::BuildHighWallChainMode:
						{
							Grid::LCellIdI focus = v.location;
							focus++;
							v.myTasks.PushFront(MobileEntity::Task("Move To",next,true,MobileEntity::Task::MoveTo));
							v.RegisterNewFrontJob("", PVehicleJob(new BuildHorizontalWall(v)),shared_from_this(), nullptr);
							v.RegisterNewFrontJob("", PVehicleJob(new BuildWallUp(v)),shared_from_this(), &focus);
						}
						break;
						case CursorMode::BuildLowWallChainMode:
						{
							Grid::LCellIdI focus = v.location;
							focus++;
							v.myTasks.PushFront(MobileEntity::Task("Move To",next,true,MobileEntity::Task::MoveTo));
							v.RegisterNewFrontJob("", PVehicleJob(new BuildHorizontalWall(v)),shared_from_this(), nullptr);
						}
						break;
						case CursorMode::BuildBridgeMode:
						{
							
							//auto next = path.begin();
							Grid::LCellIdI focus = Grid::LCellIdI(v.location.stackID,next.height);
							if (next.height > v.location.height)
							{
								v.RegisterNewFrontJob("", PVehicleJob(new BuildWallUp(v)),shared_from_this(), &focus);
							}


							if (
								(lastLocation != Grid::LCellIdI::Invalid &&lastLocation.height < v.location.height)
								 ||
								 (!nextIsLast && next.height < v.location.height))
							{
								//RegisterNewJob(nullptr, PJob(new Objects::Job::DigDown(*this)),nullptr);
								v.RegisterNewFrontJob("", PVehicleJob(
									new BuildRoad(v,
										lastLocation != Grid::LCellIdI::Invalid && lastLocation.height < v.location.height ? lastLocation.stackID : 0xFFFFFFFF,
										/*next != path.end() && next->height < v.location.height ?next->stackID :*/ 
										0xFFFFFFFF)),
									shared_from_this(), 
									nullptr);
							}
							else
								v.RegisterNewFrontJob("", PVehicleJob(new BuildRoad(v)),shared_from_this(), nullptr);

							if (
								(hasNext && next.height < v.location.height && next.stackID == v.location.stackID)
								//||
								//(!hasNext && destination.height < v.location.height)
								)
							{
								focus--;
								v.RegisterNewFrontJob("", PVehicleJob(new DigDown(v)),shared_from_this(), &focus);
							}

//							v.RegisterNewFrontJob("", PVehicleJob(new BuildRoad(v)),nullptr);
							//v.myTasks.PushFront(MobileEntity::Task("Move To",Grid::LCellIdI(next->stackID,v.location.height),
							//					MobileEntity::Task::MoveTo));
						}
						break;
					}
				}
				else
				{
					Grid::LCellIdI c = v.location;
						
						
					foreach (path,node)
					{
						c = *node;
						auto next = node+1;
						if (c != v.location && !DoneHere(World::actual,c, next == path.end() ? c : *next))
							break;
					}
					if (c == v.location)
					{
						return CannotStartAbort;
					}
					if (!v.PlotFrontCourse(c,false))
					{
						//bool abort = true;
						if (GetStructure(c))
						{
							roundsToJobCompletion = 0;
							LogEvent(EventClass::Preset::commandRejected,&v, GetName()+": Destination is occupied. Job done");
						}
						else
						{
							//if (mode == CursorMode::BuildBridgeMode
							//	&& c.height - v.location.height > 0)
							//	{
							//		Grid::LCellIdI focus = v.location+1;
							//		v.RegisterNewFrontJob("", PVehicleJob(new BuildWallUp(v)),&focus);
							//		abort = false;
							//	}
							//	else
									LogEvent(EventClass::Preset::commandRejected,&v, GetName()+": Unable to plot path to destination. Motion delayed");
						}
//						if (abort)
							return CannotStartAbort;
					}
					//if (mode != CursorMode::BuildBridgeMode || next.stackID != v.location.stackID)
						lastLocation = v.location;


				}
			}
			first = false;
			return Delayed;
		}
		#endif
	}

}

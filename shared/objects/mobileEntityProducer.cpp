#include <global_root.h>
#include "mobileEntityProducer.h"
#include "prototypes.h"
#include "vehicle.h"
#include "satellite.h"
#include "gridAndChoices.h"
#include "structureCompound.h"
#include "faction.h"


#ifdef HAS_WINDOW
	#include "visualChoices.h"
	#include "rendering.h"
	#include "visualCommands.h"
	#include "visualSelection.h"
	#include "../../client/components/dialog.h"
#endif


namespace Objects
{
	UINT32				MobileEntityProducer::ProductionRounds(int costs, bool isActive) const
	{
		if (isActive)
			costs -= productionDone;
		costs = std::max(0,costs);
		UINT32 rs = costs / productionPerRound;
		if (costs % productionPerRound)
			rs ++;
		return rs;
	}

	UINT32				MobileEntityProducer::ProductionRounds(const PMobileEntity&v, bool isActive) const
	{
		return ProductionRounds(v->GetConstructionCost(),isActive);
	}



	bool					MobileEntityProducer::WorkItem::IsSatellite() const
	{
		#ifdef CLIENT
			return !base;
		#else
			return producing->IsSatellite();
		#endif

	}

	bool				MobileEntityProducer::AwaitsUserInteraction() const 
	{
		#ifdef CLIENT
			return queue.IsNotEmpty() && ProductionRounds(queue.first().cost,true) == 0;
		#else
			return queue.IsNotEmpty() && ProductionRounds(queue.first().producing,true) == 0;
		#endif
	}



	bool						GatheringPoint::Set(const Grid::LCellIdI&point, const PWorldEntity&parent)
	{
		if (current == point)
			return false;
		current = point;
		#ifdef HAS_WINDOW
			if (selectedEntity == parent)
			{
				UpdateVisuals();
			}
		#endif
		return true;
	}

	#ifdef HAS_WINDOW

		void					GatheringPoint::UpdateVisuals()
		{
			Grid::LCellIdI c = selected = current;
				
			if (c != Grid::LCellIdI::Invalid)
			{
				const Grid::CellStack&stack = Grid::actualWorld.cellStack[Grid::low.grid.GetCell(c.stackID).higherEquivalent];
				while (stack.GetCellI(c.height).structureCounter > 1)
					c++;
				TMatrix4<> sys;
				const TVec3<>&dir = Grid::low.cells[c.stackID].centerVertex.xyz;
				float h = Grid::ToRadiusf(float(c.height));
				Mat::makeAxisSystem(dir*h,dir,2,sys);
				sys.x.xyz *= 0.75f;
				sys.y.xyz *= 0.75f;
				selectedVisual = sys;
			}
			else
				selected = Grid::LCellIdI::Invalid;
		}

		void				MobileEntityProducer::VisitAllCommands(WorldEntity::FCommandVisitor visitor,bool force)
		{
			for (index_t i = 0; i < queue.Count(); i++)
				queue[i].commandToTerminate.VisitMembers(visitor,force);
		}

		

	#endif




	#ifndef CLIENT
		PMobileEntity				MobileEntityProducer::SpawnFinishedProductionAnywhere(float&t)
		{
			if (!AwaitsUserInteraction())
				return PMobileEntity();
			Buffer0<Grid::LCellIdI>	movementChoices;
			bool showArrow, hasArrowCellTarget;
			TVec3<>arrowFrom;
			Grid::LCellIdI arrowFromCell;
			PMobileEntity production = GetActiveProduction();



			WorldEntity*parent = dynamic_cast<WorldEntity*>(this);
			LogEvent(EventSignificance::Debug,parent,"Trying to spawn finished production "+production->GetNiceName()+" anywhere...");
			String error;
			if (!parent->owner->CanSpawnType(production,error))
			{
				LogEvent(EventClass::Preset::operationAborted,parent,"Unable to deploy "+production->GetNiceName()+": "+error);
				return PMobileEntity();
			}
			ASSERT_NOT_NULL__(parent);

			GetChoices(*parent,production,CursorMode::Idle,movementChoices,showArrow,hasArrowCellTarget,arrowFrom,arrowFromCell);

			if (movementChoices.IsEmpty())
			{
				LogEvent(EventClass::Preset::operationAborted,parent,"Failed to automatically spawn finished unit. Is something blocking deployment?");
				return PMobileEntity();
			}

			PMobileEntity v = CompleteProduction(t);
			//static Random random;
			Grid::LCellIdI c = movementChoices[parent->entityRandom.getIndex(movementChoices.Count()-1)];

			MobilePlacementResult rs = v->PlaceExactlyAt(c,0,0,false,false);
			if (Failed(rs))
			{
				LogEvent(EventClass::Preset::fatal,v.get(),"Failed to position non-constructed at "+c.ToString());
				FATAL__("Failed to position entity as designated position");
			}
			LogEvent(EventSignificance::Debug,parent,"Successfully spawned finished production  "+production->GetNiceName()+" at "+c.ToString()+" with placement result "+String(rs.ToString()));

			if (rs != MobilePlacementResult::EnteredStructure)
				InsertConstructed(v,t);
			Structure*s = dynamic_cast<Structure*>(this);
			if (s)
				s->GetCompound().RefreshState(0);
			return v;
		}


		void MobileEntityProducer::Tick(ResourceVector&v, float&t)
		{
			WorldEntity*parent = dynamic_cast<WorldEntity*>(this);
			ASSERT_NOT_NULL__(parent);

			if (queue.IsEmpty())
			{
				FATAL__("should be disabled");
			}

			productionRoundsLeft = ProductionRounds(queue.first().producing,true);

			if (productionRoundsLeft == 0)
			{
				if (queue.first().IsSatellite())
				{
					LogEvent(EventSignificance::Common,parent,"Auto-spawning satellite");
					Objects::PMobileEntity e = SpawnFinishedProductionAnywhere(t);


					return;
				}
				FATAL__("This should not happen");
			}

			if (productionRoundsLeft == 1)
			{
				int delta = (queue.first().producing->GetConstructionCost() - productionDone);
				ASSERT_IS_CONSTRAINED__(delta, 0, (int)productionPerRound);
				v.level[Resource::Minerals] -= delta;
			}
			else
				v.level[Resource::Minerals] -= productionPerRound;

			productionDone += productionPerRound;
			productionRoundsLeft = ProductionRounds(queue.first().producing,true);
			if (productionDone >= (UINT32)(queue.first().producing->GetConstructionCost()))
			{
				LogEvent(EventSignificance::Debug,parent,"Construction of "+queue.first().producing->GetName()+" completed");
				LogEvent(EventClass( EventSignificance::Uncommon, EventFlavor::Gain ) ,parent,"Construction of "+queue.first().producing->GetNiceName()+" completed");

				if (gatheringPoint.current != Grid::LCellIdI::Invalid || queue.first().IsSatellite())
				{
					LogEvent(EventSignificance::Common,parent,"Auto-spawning mobile entity...");
					Objects::PMobileEntity v = SpawnFinishedProductionAnywhere(t);
					if (v)
					{
						LogEvent(EventSignificance::Common,parent,"Auto-spawned mobile entity "+v->GetName());
						if (!v->IsSatellite())
						{
							LogEvent(EventSignificance::Common,v.get(),"Directing vehicle to gathering point");
							v->PlotCourse(gatheringPoint.current,false);
						}
					}
					else
						LogEvent(EventClass( EventSignificance::Common, EventFlavor::Loss ),parent,"Failed to automatically spawn finished entity");
				}
			}

			parent->RegisterChange(WorldEntity::TasksChanged,0);

		}


		PMobileEntity				MobileEntityProducer::GetActiveProduction() const
		{
			return queue.IsNotEmpty() ? queue.first().producing : PMobileEntity();

		}



		PMobileEntity					MobileEntityProducer::CompleteProduction(float &t)
		{
			PMobileEntity v = GetActiveProduction();
			WorldEntity*parent = dynamic_cast<WorldEntity*>(this);
			String error;
			if (!parent->owner->CanSpawnType(v,error))
			{
				LogEvent(EventClass::Preset::operationAborted,parent,"Unable to deploy "+v->GetNiceName()+": "+error);
				return PMobileEntity();
			}

			ASSERT__(v);
			ASSERT_EQUAL__(0,ProductionRounds(v,true));
			v->health = v->GetMaxHealth();	//must set to max here, or positioning will fail
			t+= timeEpsilon;
			v->motionTime = t;
			t+= timeEpsilon;

			queue.Erase(0);
			productionDone = 0;	//must be before ProductionRounds() or calculation will produce invalid results
			if (queue.IsEmpty())
			{
				productionRoundsLeft = 0;
				
				SignalLastProductionDone(t);
			}
			else
				productionRoundsLeft = ProductionRounds(queue.first().producing,true);

//			WorldEntity*parent = dynamic_cast<WorldEntity*>(this);
			ASSERT_NOT_NULL__(parent);
			parent->SignalChange(WorldEntity::TasksChanged, -1);
			return v;
		}

		MobileEntityProducer::WorkItem*				MobileEntityProducer::EnqueueProductionOfVehicle(ConstName baseName, ConstName turretName)
		{
			WorldEntity*parent = dynamic_cast<WorldEntity*>(this);
			ASSERT_NOT_NULL__(parent);
			Component::VehicleBase* base = Component::GetVehicleBaseByName(baseName);
			Component::Turret* turret = Component::GetTurretByName(turretName);
			if (!turret)
			{
				LogEvent(EventClass::Preset::commandRejected,parent,"Invalid turret component");
				return nullptr;
			}
			if (!base)
			{
				LogEvent(EventClass::Preset::commandRejected,parent,"Invalid base component");
				return nullptr;
			}
			return EnqueueProductionOfVehicle(base,turret);
		}

		MobileEntityProducer::WorkItem*				MobileEntityProducer::EnqueueProductionOfSatellite(ConstName typeName)
		{
			WorldEntity*parent = dynamic_cast<WorldEntity*>(this);
			ASSERT_NOT_NULL__(parent);

			PSatellite proto = GetSatellitePrototypeByName(typeName);
			String error;
			if (!parent->owner->CanSpawnType(proto,error))
			{
				LogEvent(EventClass::Preset::operationAborted,parent,"Unable to enqueue production of "+proto->GetTypeName()+": "+error);
				return nullptr;
			}

			if (!proto)
			{
				LogEvent(EventClass::Preset::commandRejected,parent,"Invalid satellite type");
				return nullptr;
			}

			PSatellite sat = To<Satellite>::Cast(proto->CreateNewInstance(parent->owner,parent->location,0.f));
			WorkItem*rs = EnqueueProductionOf(sat);
			parent->RegisterChange(WorldEntity::TasksChanged,-1);
			SignalNewProductionEnqueued();
			return rs;
		}



		MobileEntityProducer::WorkItem*				MobileEntityProducer::EnqueueProductionOf(const PMobileEntity&e, index_t at /*= InvalidIndex*/)
		{
			index_t index = std::min(at,queue.Count());
			//productionRoundsLeft
			UINT32 rounds = ProductionRounds(e,!index);
			if (!index)
				productionRoundsLeft = rounds;
			WorkItem&item = queue.Insert(at);
			item.id.Generate();
			item.producing = e;
			WorldEntity*parent = dynamic_cast<WorldEntity*>(this);
			ASSERT_NOT_NULL__(parent);
			parent->RegisterChange(WorldEntity::TasksChanged,-1);
			return &item;
		}

		MobileEntityProducer::WorkItem*				MobileEntityProducer::EnqueueProductionOfVehicle(Component::VehicleBase*selectedBase, Component::Turret*selectedTurret)
		{
			WorldEntity*parent = dynamic_cast<WorldEntity*>(this);
			ASSERT_NOT_NULL__(parent);
			if (queue.Count() >= MaxStructureTasks)
			{
				LogEvent(EventClass::Preset::commandRejected,parent,"Max queue length ("+String(MaxStructureTasks)+") reached");
				return nullptr;
			}
			if (selectedBase && selectedTurret)
			{
				PVehicle vehicle(new Vehicle(parent->owner,parent->location,selectedBase,selectedTurret));
				String error;
				if (!parent->owner->CanSpawnType(vehicle,error))
				{
					LogEvent(EventClass::Preset::operationAborted,parent,"Unable to enqueue production of "+vehicle->GetTypeName()+": "+error);
					return nullptr;
				}
				WorkItem*rs = EnqueueProductionOf(vehicle);
				parent->RegisterChange(WorldEntity::TasksChanged,-1);
				SignalNewProductionEnqueued();
				return rs;
			}
			return nullptr;
		}

		bool				MobileEntityProducer::AbortConstruction(const TUniqueID&id)
		{
			WorldEntity*parent = dynamic_cast<WorldEntity*>(this);
			ASSERT_NOT_NULL__(parent);

			foreach (queue,e)
			{
				if (e->id == id)
				{
					if (e == queue.begin())
					{
						productionDone = 0;
						parent->unstable = false;
					}
					queue.erase(e);
					if (queue.IsNotEmpty())
						productionRoundsLeft = ProductionRounds(queue.first(),true);
					else
					{
						productionRoundsLeft = 0;
						float t = 0;
						SignalLastProductionDone(t);
					}
					parent->RegisterChange(WorldEntity::TasksChanged,-1);
					return true;
				}
			}
			return false;
		}

		void			MobileEntityProducer::UpdateGatheringPoint(const Grid::LCellIdI&loc)
		{
			WorldEntity*parent = dynamic_cast<WorldEntity*>(this);
			if (gatheringPoint.current != loc)
			{
				LogEvent(EventSignificance::Common, parent, "Set gathering point");
				gatheringPoint.current = loc;
			}
			else
			{
				LogEvent(EventSignificance::Common, parent, "Unset gathering point");
				gatheringPoint.current = Grid::LCellIdI::Invalid;
			}
			parent->RegisterChange(Objects::WorldEntity::AutomationChanged,0);
		}


	#else
		Component::VehicleBase*				MobileEntityProducer::GetActiveProductionBase() const
		{
			return queue.IsNotEmpty() ? queue.first().base : nullptr;
		}
		
				String				MobileEntityProducer::JobName(index_t index, const String&itemName, UINT32 roundsLeft)	const
		{
			return "#"+String(index)+" "+itemName+ " ("+String(roundsLeft)+")";
		}

		#ifdef HAS_WINDOW
		void				MobileEntityProducer::SetCommandOffset(const int2&c)
		{
			if (commandOffset == c)
				return;
			commandOffset = c;
			int index = 0;
			foreach (queue,item)
				item->commandToTerminate.SetCoords(commandOffset + int2(index++,0));
		}
		#endif

		void				MobileEntityProducer::AppendTask(const TUniqueID&id, ConstName baseName, ConstName turretName)
		{
			//PSatellite	satellite;
			//VehicleBase* base;
			//Turret* turret;
			int price = 0;
			String name;
			Component::VehicleBase* base = nullptr;
			Component::Turret* turret = nullptr;
			PSatellite sat;
			if (turretName != ConstName::Empty)
			{
				base = Component::GetVehicleBaseByName(baseName);
				turret = Component::GetTurretByName(turretName);
				ASSERT_NOT_NULL1__(turret, (int)turretName);
				ASSERT_NOT_NULL1__(base, (int)baseName);

				price = Vehicle::GetPrice(base,turret);
				name = Vehicle::GetTypeName(base,turret);
			}
			else
			{
				sat = GetSatellitePrototypeByName(baseName);
				ASSERT1__(sat, (int)baseName);
				price = sat->GetConstructionCost();
				name = sat->GetTypeName();
			}

			{
				//PVehicle vehicle(new Vehicle(location,base,turret));
				index_t index = queue.Count();
				//productionRoundsLeft
				UINT32 rounds = ProductionRounds(price,!index);
				if (index)
					rounds += queue.last().roundsToCompletion;
				else
					productionRoundsLeft = rounds;
				WorkItem&item = queue.Append();
				item.id = id;
				item.name = name;
				item.cost = price;
				item.base = base;
				item.roundsToCompletion = rounds;

				#ifdef HAS_WINDOW
					item.commandToTerminate = EntityConstructionCommand(commandOffset + int2(index,0), JobName(index,item.name,rounds),base,turret,sat,[id,index]()
					{
//						PWorkingStructure s = To<WorkingStructure>::DynamicCast(e);
	//					ASSERT__(s);
						PMobileEntityProducer p = To<MobileEntityProducer>::DynamicCast(selectedEntity);
						ASSERT__(p);
						if (!index && p->productionDone)
						{
							WWorldEntity e = selectedEntity;
							Dialog::Confirm("Confirm cancellation of production job "+p->queue.first().name+". Material has been invested into this production, which will be lost.",
							[e,id]()
							{
								PWorldEntity e1 = e.lock();
								if (e1)
									SignalConstructionAbort(e1->id,id);
							}
							,
							[e,p,id]()->Dialog::CheckResult
							{
								PWorldEntity e1 = e.lock();
								if (!e1 || e1->health <= 0)
									return Dialog::CheckResult::Abort;
								if (p->queue.IsEmpty() || p->queue.first().id != id)
									return Dialog::CheckResult::Abort;
								if (!controlIsEnabled)
									return Dialog::CheckResult::OkayNotRightNow;
								return Dialog::CheckResult::OkayContinue;
							}
							);

						}
						else
							SignalConstructionAbort(selectedEntity->id,id);	//lossless

					},CommonCmd::CommonButtonCheck);
					if (!index)
					{
						item.commandToTerminate.roundsLeft = rounds;
						item.commandToTerminate.mineralsLeft = price - productionDone;
					}
					else
					{
						item.commandToTerminate.roundsLeft = 0xFFFFFFFF;
						item.commandToTerminate.mineralsLeft = 0xFFFFFFFF;
					}

					item.commandToTerminate.RasterizeImage();
				#endif

			}
		}

	#endif


}

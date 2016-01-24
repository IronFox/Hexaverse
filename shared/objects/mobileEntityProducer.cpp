#include <global_root.h>
#include "mobileEntityProducer.h"
#include "prototypes.h"
#include "vehicle.h"
#include "orbitalVehicle.h"
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
	UINT32				EntityFactory::ProductionRounds(int costs, bool isActive) const
	{
		if (isActive)
			costs -= productionDone;
		costs = std::max(0,costs);
		UINT32 rs = costs / productionPerRound;
		if (costs % productionPerRound)
			rs ++;
		return rs;
	}

	UINT32				EntityFactory::ProductionRounds(const PWorldEntity&v, bool isActive) const
	{
		return ProductionRounds(v->GetConstructionCost(),isActive);
	}



	bool					EntityFactory::WorkItem::IsOrbitalShip() const
	{
		#ifdef CLIENT
			return !base;
		#else
			return producing->IsOrbital();
		#endif

	}

	bool				EntityFactory::AwaitsUserInteraction() const 
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

		void				EntityFactory::VisitAllCommands(WorldEntity::FCommandVisitor visitor)
		{
			for (index_t i = 0; i < queue.Count(); i++)
				visitor(queue[i].commandToTerminate);
		}

		

	#endif




	#ifndef CLIENT
		PWorldEntity				EntityFactory::SpawnFinishedProductionAnywhere()
		{
			if (!AwaitsUserInteraction())
				return PMobileEntity();
			Buffer0<Grid::LCellIdI>	movementChoices;
			bool showArrow, hasArrowCellTarget;
			TVec3<>arrowFrom;
			Grid::LCellIdI arrowFromCell;
			PWorldEntity production = GetActiveProduction();



			WorldEntity*parent = dynamic_cast<WorldEntity*>(this);
			String error;
			if (!parent->owner->CanSpawnType(production,error))
			{
				LogEvent(EventType::OperationAborted,parent,"Unable to deploy "+production->GetNiceName()+": "+error);
				return PMobileEntity();
			}
			ASSERT_NOT_NULL__(parent);

			if (production->IsMobile())
			{
				GetChoices(*parent,To<MobileEntity>::Cast(production),CursorMode::Idle,movementChoices,showArrow,hasArrowCellTarget,arrowFrom,arrowFromCell);
			}
			else
				if (production->IsStructure())
				{
					if (production->IsOrbital() == parent->IsOrbital())
					{
						LogEvent(EventType::InternalFault,parent,"Structures cannot build other structures in the same domain.");
						return PMobileEntity();
					}
					if (production->IsOrbital())
					{
						if (Orbital::NothingHere(parent->location.stackID))
							movementChoices << Grid::LCellIdI(parent->location.stackID,Orbital::BaseHeight);
					}
					else
					{
						int h = Orbital::Station::GetStructureBuildHeight(parent->location);
						if (h != badHeight)
							movementChoices << Grid::LCellIdI(parent->location.stackID,h);
					}
				}
				else
					FATAL__("Unsupported entity type for deployment: "+production->GetName());


			if (movementChoices.IsEmpty())
			{
				LogEvent(EventType::EntityProcessFailed,parent,"Failed to automatically spawn finished unit. Is something blocking deployment?");
				return PMobileEntity();
			}

			PWorldEntity v = CompleteProduction();
			//static Random random;
			Grid::LCellIdI c = movementChoices[parent->localRandom.getIndex(movementChoices.Count()-1)];

			if (v->IsMobile())
			{
				auto m = To<MobileEntity>::Cast(v);
				MobilePlacementResult rs = m->PlaceExactlyAt(c,0,0,false,false);
				if (Failed(rs))
				{
					LogEvent(EventType::Fatal,v.get(),"Failed to position non-constructed at "+c.ToString());
					FATAL__("Failed to position entity as designated position");
				}
				if (rs == MobilePlacementResult::PlacementOkay || rs == MobilePlacementResult::PlacementOkayFellDown)
					InsertConstructed(m,0);
			}
			else
				if (v->IsStructure())
				{
					v->location = c;
					v->SetupSystem();
					InsertConstructed(To<Structure>::Cast(v),0,true);
				}
				else
					FATAL__("Unsupported entity type for deployment: "+v->GetName());
			Structure*s = dynamic_cast<Structure*>(this);
			if (s)
				s->GetCompound().RefreshState(0);
			return v;
		}


		void EntityFactory::Tick(ResourceVector&v)
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
				if (queue.first().IsOrbitalShip())
				{
					LogEvent(EventType::MinorEntityProcess,parent,"Auto-spawning satellite");
					Objects::PWorldEntity e = SpawnFinishedProductionAnywhere();


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
				LogEvent(EventType::EntityProcess,parent,"Construction of "+queue.first().producing->GetNiceName()+" completed");

				if (gatheringPoint.current != Grid::LCellIdI::Invalid || queue.first().IsOrbitalShip())
				{
					LogEvent(EventType::MinorEntityProcess,parent,"Auto-spawning mobile entity");
					Objects::PWorldEntity v = SpawnFinishedProductionAnywhere();
					if (v && v->IsMobile())
					{
//						if (!v->IsOrbital())
						{
							LogEvent(EventType::EntityProcess,v.get(),"Directing vehicle to gathering point");
							To<MobileEntity>::Cast(v)->PlotCourse(gatheringPoint.current,false);
						}
					}
//					else
	//					LogEvent(EventType::EntityProcessFailed,parent,"Failed to automatically spawn finished entity. Is something blocking deployment?");
				}
			}

			parent->RegisterChange(WorldEntity::TasksChanged,0);

		}


		PWorldEntity				EntityFactory::GetActiveProduction() const
		{
			return queue.IsNotEmpty() ? queue.first().producing : PWorldEntity();

		}



		PWorldEntity					EntityFactory::CompleteProduction()
		{
			PWorldEntity v = GetActiveProduction();
			WorldEntity*parent = dynamic_cast<WorldEntity*>(this);
			String error;
			if (!parent->owner->CanSpawnType(v,error))
			{
				LogEvent(EventType::OperationAborted,parent,"Unable to deploy "+v->GetNiceName()+": "+error);
				return PMobileEntity();
			}

			ASSERT__(v);
			ASSERT_EQUAL__(0,ProductionRounds(v,true));
			v->health = v->GetMaxHealth();	//must set to max here, or positioning will fail
			queue.Erase(0);
			productionDone = 0;	//must be before ProductionRounds() or calculation will produce invalid results
			if (queue.IsEmpty())
			{
				productionRoundsLeft = 0;
				
				SignalLastProductionDone();
			}
			else
				productionRoundsLeft = ProductionRounds(queue.first().producing,true);

//			WorldEntity*parent = dynamic_cast<WorldEntity*>(this);
			ASSERT_NOT_NULL__(parent);
			parent->SignalChange(WorldEntity::TasksChanged, -1);
			return v;
		}

		EntityFactory::WorkItem*				EntityFactory::EnqueueProductionOfVehicle(ConstName baseName, ConstName turretName)
		{
			WorldEntity*parent = dynamic_cast<WorldEntity*>(this);
			ASSERT_NOT_NULL__(parent);
			Component::VehicleBase* base = Component::GetVehicleBaseByName(baseName);
			Component::Turret* turret = Component::GetTurretByName(turretName);
			if (!turret)
			{
				LogEvent(EventType::CommandRejected,parent,"Invalid turret component");
				return nullptr;
			}
			if (!base)
			{
				LogEvent(EventType::CommandRejected,parent,"Invalid base component");
				return nullptr;
			}
			return EnqueueProductionOfVehicle(base,turret);
		}

		EntityFactory::WorkItem*				EntityFactory::EnqueueProductionOf(ConstName typeName)
		{
			WorldEntity*parent = dynamic_cast<WorldEntity*>(this);
			ASSERT_NOT_NULL__(parent);

			PWorldEntity proto = Orbital::GetShipPrototypeByName(typeName);
			if (!proto)
				proto = Orbital::GetStationPrototypeByName(typeName);
			if (!proto)
				proto = GetPrototypeByName(typeName);
			if (!proto)
			{
				LogEvent(EventType::CommandRejected,parent,"Invalid entity type: #"+String((unsigned)typeName));
				return nullptr;
			}

			String error;
			if (!parent->owner->CanSpawnType(proto,error))
			{
				LogEvent(EventType::OperationAborted,parent,"Unable to enqueue production of "+proto->GetTypeName()+": "+error);
				return nullptr;
			}



			PWorldEntity instance = proto->CreateNewInstance(parent->owner,parent->location,0.f);
			WorkItem*rs = EnqueueProductionOf(instance);
			parent->RegisterChange(WorldEntity::TasksChanged,-1);
			SignalNewProductionEnqueued();
			return rs;
		}



		EntityFactory::WorkItem*				EntityFactory::EnqueueProductionOf(const PWorldEntity&e, index_t at /*= InvalidIndex*/)
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

		EntityFactory::WorkItem*				EntityFactory::EnqueueProductionOfVehicle(Component::VehicleBase*selectedBase, Component::Turret*selectedTurret)
		{
			WorldEntity*parent = dynamic_cast<WorldEntity*>(this);
			ASSERT_NOT_NULL__(parent);
			if (queue.Count() >= MaxStructureTasks)
			{
				LogEvent(EventType::CommandRejected,parent,"Max queue length ("+String(MaxStructureTasks)+") reached");
				return nullptr;
			}
			if (selectedBase && selectedTurret)
			{
				PVehicle vehicle(new Vehicle(parent->owner,parent->location,selectedBase,selectedTurret));
				String error;
				if (!parent->owner->CanSpawnType(vehicle,error))
				{
					LogEvent(EventType::OperationAborted,parent,"Unable to enqueue production of "+vehicle->GetTypeName()+": "+error);
					return nullptr;
				}
				WorkItem*rs = EnqueueProductionOf(vehicle);
				parent->RegisterChange(WorldEntity::TasksChanged,-1);
				SignalNewProductionEnqueued();
				return rs;
			}
			return nullptr;
		}

		bool				EntityFactory::AbortConstruction(const TUniqueID&id)
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

					parent->RegisterChange(WorldEntity::TasksChanged,-1);
					return true;
				}
			}
			return false;
		}

		void			EntityFactory::UpdateGatheringPoint(const Grid::LCellIdI&loc)
		{
			WorldEntity*parent = dynamic_cast<WorldEntity*>(this);
			if (gatheringPoint.current != loc)
			{
				LogEvent(EventType::EntityProcess, parent, "Set gathering point");
				gatheringPoint.current = loc;
			}
			else
			{
				LogEvent(EventType::EntityProcess, parent, "Unset gathering point");
				gatheringPoint.current = Grid::LCellIdI::Invalid;
			}
			parent->RegisterChange(Objects::WorldEntity::AutomationChanged,0);
		}


	#else
		Component::VehicleBase*				EntityFactory::GetActiveProductionBase() const
		{
			return queue.IsNotEmpty() ? queue.first().base : nullptr;
		}
		
				String				EntityFactory::JobName(index_t index, const String&itemName, UINT32 roundsLeft)	const
		{
			return "#"+String(index)+" "+itemName+ " ("+String(roundsLeft)+")";
		}

		void				EntityFactory::AppendTask(const TUniqueID&id, ConstName baseName, ConstName turretName)
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
					item.commandToTerminate = FactoryConstructionAbortCommand(int2(5+index,2), JobName(index,item.name,rounds),base,turret,sat,[id,index](const PWorldEntity&e)
					{
//						PWorkingStructure s = To<WorkingStructure>::DynamicCast(e);
	//					ASSERT__(s);
						PEntityFactory p = To<EntityFactory>::DynamicCast(e);
						ASSERT__(p);
						if (!index && p->productionDone)
						{
							Dialog::Confirm("Confirm cancellation of production job "+p->queue.first().name+". Material has been invested into this production, which will be lost.",
							[e,id]()
							{
								SignalConstructionAbort(e->id,id);
							}
							,
							[e,p,id]()->Dialog::CheckResult
							{
								if (e->health <= 0)
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
							SignalConstructionAbort(e->id,id);	//lossless

					},CommonCmd::MostlyEnabled<PWorldEntity const&>,CommonCmd::AlwaysFalse<PWorldEntity const&>);
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

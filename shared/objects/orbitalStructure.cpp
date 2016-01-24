#include <global_root.h>
#include "orbitalStructure.h"
#include "gridAndChoices.h"
#include "orbitalStructure.h"
#include "orbitalVehicle.h"
#include "structureCompound.h"
#include "faction.h"


namespace Objects
{
	namespace Orbital
	{
		Buffer0<PStation>	stations;


		/*virtual override*/ void		Hub::VisitAllConnected(const std::function<void(const PStructure&)>&visitor) const
		{
			Super::VisitAllConnected(visitor);
			PSkyLinkStructure s =  this->connectedToLink.lock();
			if (s)
				visitor(To<Structure>::Cast(s->CastToStructure()->shared_from_this()));
		}


		void			Hub::OnConstructionFinished(float atTime)
		{
			UpdateConnections();
		}


		bool			Hub::UpdateConnections()
		{
			PSkyLinkStructure port = connectedToLink.lock();
			WorkingStructure*ws = port ? port->CastToStructure() : nullptr;

			bool rs= false;
			if (port && ws->health <= 0)
			{
				connectedTo.reset();
				port.reset();
				rs = true;
			}

			if (ws)
			{
				if (ws->location.stackID != location.stackID || !ws->IsEnabled() || ws->IsHibernating())
				{
					LoseGroundConnection();
					port.reset();
					rs = true;
				}
			}

			if (!port)
			{
				for (int h = Grid::CellStack::HalfRange-1; h >= -Grid::CellStack::HalfRange; h--)
				{
					PStructure s = GetStructure(Grid::LCellIdI(location.stackID,h));
					if (s)
					{
						port = To<SkyLinkStructure>::DynamicCast(s);
						if (port)
						{
							WorkingStructure*ws = port->CastToStructure();
							if (s->owner != owner || !ws->IsEnabled() || ws->IsHibernating())
								port.reset();
						}
						break;
					}
				}

				if (port)
				{
					GainGroundConnection(port);
					rs = true;
				}
			}
			if (rs)
				compoundCoverageChanged = true;
			return rs;
		}



		
		/*virtual override*/ void		Station::RegisterSensory(const Grid::LCellIdI&loc, float&atTime, bool checkChangesNow/*=true*/, Buffer0<PWorldEntity>*newContacts /*= nullptr*/)
		{
			Super::RegisterSensory(loc,atTime,checkChangesNow && turret->groundScanningRange <= 0, newContacts);
			#ifdef CLIENT
				if (isMine)
					Grid::visibleOrbitGrid.ChangeSensory(loc,GetScanningRange(),+1);
			#endif
			if (turret->groundScanningRange > 0.f)
			{
				float range = EMPModify(turret->groundScanningRange,2);
				Grid::actualWorld.MapSensorState(loc.stackID,range,loc.height,turret->groundScannerPenetration,skySensorState);
				Grid::actualWorld.UpdateSensorState(skySensorState);
				#ifdef CLIENT
					if (isMine)
					{
						Grid::actualWorld.InsertSensorCoverage(skySensorState);
					}
				#else
					owner->RegisterSensorCreation(skySensorState,atTime,checkChangesNow,newContacts);
				#endif
			}
		}

		/*virtual override*/ void		Station::UnregisterSensory(const Grid::LCellIdI&loc, float&atTime, bool checkChangesNow/*=true*/)
		{
			Super::UnregisterSensory(loc,atTime,checkChangesNow && turret->groundScanningRange <= 0);
			#ifdef CLIENT
				if (isMine)
					Grid::visibleOrbitGrid.ChangeSensory(loc,GetScanningRange(),-1);
			#endif
			if (turret->groundScanningRange > 0.f)
			{
				#ifdef CLIENT
					if (isMine)
					{
						Grid::actualWorld.RemoveSensorCoverage(skySensorState);
					}
				#else
					owner->RegisterSensorRemoval(skySensorState,atTime,checkChangesNow);
				#endif
			}
		}

		#ifndef CLIENT


			bool					Station::RefreshGroundSensor(float&atTime)
			{
				if (turret->groundScanningRange > 0.f)
				{
					Grid::SkySensorState state = skySensorState;
					if (Grid::actualWorld.UpdateSensorState(skySensorState))
					{
						LogEvent(EventType::MinorEntityProcess,this,"Ground-sensor change detected");

						owner->RegisterSensorCreation(skySensorState,atTime,false);
						atTime += timeEpsilon;
						owner->RegisterSensorRemoval(state,atTime,true);
						atTime += timeEpsilon;
						return true;
					}

				}
				return false;
			}

			


		#endif


		
		int				Station::GetStructureBuildHeight() const
		{
			return GetStructureBuildHeight(location);
		}

		/*static*/ int				Station::GetStructureBuildHeight(const Grid::LCellIdI&satelliteLocation)
		{
			#ifdef HYBRID_GRID
				index_t finer = Grid::low.grid.GetCell(satelliteLocation.stackID).higherEquivalent;
				const auto&c = Grid::high.grid.GetCell(finer);
			#else
				index_t finer = who->location.stackID;
			#endif
			int h = Grid::actualWorld.cellStack[finer].FindTopNonClear(true);
			if (h == badHeight)
				return badHeight;
			#ifdef HYBRID_GRID
				for (int j = 0; j < c.neighborhood.numNeighbors; j++)
				{
					int h2 = Grid::actualWorld.cellStack[c.neighborhood.neighborID[j]].FindTopNonClear(true);
					if (h2 == badHeight)
						return badHeight;
					h = std::max(h2,h);
				}
			#endif
			return h <= Grid::CellStack::TopHeight ? h : badHeight;
		}

		/*static*/ void Hub::RegisterProduction(const PStructure&s)
		{
			#ifdef HAS_WINDOW
				int2 at = AllocateProductionSlot();
				structureConstructionCommands.Append(
					StructureConstructionCommand(at, /*"Build " + */s->GetTypeName(), s,
					std::bind(&Component::Terraformer::Cmd::Construct, s, std::placeholders::_1),
					std::bind(&MyCmd::CanBuild,s,std::placeholders::_1),
					&CommonCmd::AlwaysFalse<PWorldEntity const&>));
				structureConstructionCommands.last().RasterizeImage();
			#endif
		}

		/*static*/ void Hub::RegisterProduction(const PShip&s)
		{
			#ifdef HAS_WINDOW
				int2 at = AllocateProductionSlot();
				satelliteConstructionCommands.Append(
					SatelliteConstructionCommand(at, /*"Build " + */s->GetTypeName(), s,
					std::bind(&MyCmd::Build, s, std::placeholders::_1),
					&CommonCmd::IsModeActiveC<CursorMode::Idle>,
					&CommonCmd::AlwaysFalse<PWorldEntity const&>));
			#endif
		}

		

		/*virtual override*/ void		Hub::Tick(ResourceVector&v)
		{
			Super::Tick(v);
			PSkyLinkStructure port = connectedToLink.lock();

			if (MobileEntityProducer::queue.IsNotEmpty())
			{
				Compound&cmp = port->CastToStructure()->GetCompound();
				if (cmp.total.balance.level[Resource::Minerals] >= (int)MobileEntityProducer::productionPerRound)
				{
					MobileEntityProducer::Tick(cmp.total.balance);
				}
				else
					LogEvent(EventType::EntityProcessFailed,this,"Insufficient resources available in "+port->CastToStructure()->GetNiceName()+". Delaying production");
			}
		}




		/*virtual override*/ void			Hub::PrototypeSetup()
		{
			Super::PrototypeSetup();
			RegisterProduction(SkyGate::globalPrototype);
			RegisterProduction(Guardian::globalPrototype);
			RegisterProduction(Hub::globalPrototype);
			RegisterProduction(Ragnarok::globalPrototype);
			RegisterProduction(StarPort::globalPrototype);
		}

		/*static*/ int2	Hub::AllocateProductionSlot()
		{
			static int current = 0;
			int at = current++;
			return int2(5 + (at%9),1+at/9);
		}

		
		#ifdef HAS_WINDOW

			/*virtual override*/ void			SkyHub::OnSelect()
			{
				gatheringPoint.UpdateVisuals();
			}
			/*virtual override*/ void			SkyHub::OnDeselect()
			{
				gatheringPoint.selected = Grid::LCellIdI::Invalid;
			}


			/*virtual override*/ void		SkyHub::VisitAllCommands(FCommandVisitor visitor)
			{
				Super::VisitAllCommands(visitor);
				foreach (structureConstructionCommands,cmd)
					visitor(*cmd);
				foreach (satelliteConstructionCommands,cmd)
					visitor(*cmd);
				MobileEntityProducer::VisitAllCommands(visitor);
			}

			
			/*static*/ bool			Hub::MyCmd::CanBuild(const PStructure&what, const PWorldEntity&who)
			{
				PSkyHub hub = To<Hub>::DynamicCast(who);
				ASSERT1__(hub,who->GetName());
		
				if (!CommonCmd::IsModeActiveC<CursorMode::Idle>(who,true)
					||
					!hub->connectedTo.expired())
					return false;

				Grid::LCellIdI loc = hub->GetLastPointOfOrigin();
				int h = GetStructureBuildHeight(loc);
				return h != badHeight && !GetVehicle(Grid::LCellIdI(loc.stackID,h));
			}



			/*static*/ void Hub::MyCmd::Build(const PSatellite&p, const PWorldEntity&v)
			{
				PMobileEntity mob = To<MobileEntity>::DynamicCast(v);
				if (!mob)
					return;
				SignalConstructionBegin(mob->id, p->GetCTypeName(), ConstName::Empty);
			}

		#endif


		
		bool				Hub::GainGroundConnection(const PSkyLinkStructure&to)
		{
			{
				PSkyLinkStructure port = connectedToLink.lock();
				if (port && port->CastToStructure()->location.stackID == location.stackID)
				{
					#ifndef CLIENT
						LogEvent(EventType::EntityProcessFailed,this,"Cannot connect to "+port->CastToStructure()->GetNiceName()+": Already connected to "+port->connectedToHub->GetNiceName());
					#endif
					return false;
				}
			}

			connectedToLink = to;
			#ifndef CLIENT
				timeAt+=timeEpsilon;
				SignalChange(OrbitalConnectionChanged,timeAt);
				WorkingStructure*ws = to->CastToStructure();
				ws->SignalChange(OrbitalConnectionChanged,timeAt);
				LogEvent(EventType::EntityProcess,this,"Connection to "+ws->GetNiceName()+" established");
			#endif

			to->connectedToHub = To<Hub>::Cast(shared_from_this());

			return true;
		}

		void				Hub::LoseGroundConnection()
		{
			PSkyLinkStructure port = connectedToLink.lock();
			if (port)
			{
				WorkingStructure*ws = port->CastToStructure();
				#ifndef CLIENT
					LogEvent(EventType::EntityProcessFailed,this,"Connection to "+ws->GetNiceName()+" lost");
				#endif


				connectedTo.reset();
				#ifndef CLIENT
					timeAt+=timeEpsilon;
					SignalChange(OrbitalConnectionChanged,timeAt);
					ws->SignalChange(OrbitalConnectionChanged,timeAt);
				#endif

				port->connectedToHub.reset();
			}

		}
	}
}

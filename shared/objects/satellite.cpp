#include <global_root.h>
#include "satellite.h"
#include "gridAndChoices.h"
#include "faction.h"

namespace Objects
{

	Buffer0<PSatellite>	satellites;
	//bool					satellitesAreLocked = false;

	#ifndef ONE_COMPOUND_PER_FACTION
		/*static*/ Buffer0<Satellite::THub>	Satellite::hubs;
	#endif

	#ifdef HAS_WINDOW
		/*static*/ Buffer<StructureConstructionCommand>	SkyHub::structureConstructionCommands;
		/*static*/ Buffer<SatelliteConstructionCommand>	SkyHub::satelliteConstructionCommands;


	#endif

	/*static*/ count_t			Satellite::CountTurretOccurrences(const OwnerParameter&o,Component::Turret*t)
	{
		count_t rs = 0;
		for (const auto&m : satellites)
			if (m->owner == o && m->turret == t)
				rs++;
		return rs;
	}



	///*virtual override*/ bool				Satellite::CanSee(const PWorldEntity&other)	const
	//{
	//	if (Super::CanSee(other))
	//		return true;
	//	if (!turret->groundScanningRange)
	//		return false;
	//	UINT32 higher = Grid::low.grid.GetCell(other->location.stackID).higherEquivalent;
	//	int h;
	//	if (skySensorState.coverageMap.Query(higher,h))
	//	{
	//		return h <= other->location.height;
	//	}
	//	return false;
	//}


	///*virtual override*/ bool				Satellite::CanSee(const TVec3<>&center, float radius)	const
	//{
	//	if (Super::CanSee(center,radius))
	//		return true;

	//	if (!turret->groundScanningRange)
	//		return false;
	//	
	//	UINT32 higher = Grid::low.grid.GetCell(location.stackID).higherEquivalent;
	//	int h;
	//	if (!skySensorState.coverageMap.Query(higher,h))
	//		return false;	//this would be a bug... ah, whatever

	//	float r0 = Grid::ToRadius(h);
	//	const TVec3<>&n = Grid::low.cells[location.stackID].cornerCenter;
	//	float vertical = Vec::dot(n,center) - r0;
	//	if (vertical + radius < 0.f)
	//		return false;
	//	TVec3<> base = n*vertical;
	//	float d2 = Vec::quadraticDistance(base,center);
	//	float range = EMPModify(turret->groundScannerPenetration,2);
	//	return d2 <= sqr(range+radius);
	//}


	/*virtual override*/ void		Satellite::RegisterSensory(const Grid::LCellIdI&loc, float&atTime, bool checkChangesNow/*=true*/, Buffer0<PWorldEntity>*newContacts /*= nullptr*/)
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

	/*virtual override*/ void		Satellite::UnregisterSensory(const Grid::LCellIdI&loc, float&atTime, bool checkChangesNow/*=true*/)
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


		bool					Satellite::RefreshGroundSensor(float&atTime)
		{
			if (turret->groundScanningRange > 0.f)
			{
				Grid::SkySensorState state = skySensorState;
				if (Grid::actualWorld.UpdateSensorState(skySensorState))
				{
					LogEvent(EventSignificance::Common,this,"Ground-sensor change detected");

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


		///*virtual override*/ void Satellite::RegisterWorldSensory()
		//{
		//	Grid::visibleOrbitGrid.ChangeSensory(location,GetScanningRange(),+1);

		//	if (turret->groundScanningRange > 0.f)
		//	{
		//		float range = EMPModify(turret->groundScanningRange,2);
		//		Grid::actualWorld.MapSensorState(location.stackID,range,location.height,turret->groundScannerPenetration,skySensorState);
		//		Grid::actualWorld.UpdateSensorState(skySensorState);
		//		Grid::actualWorld.InsertSensorCoverage(skySensorState);
		//	}
		//}

		///*virtual override*/ void Satellite::UnregisterWorldSensory()
		//{
		//	Grid::visibleOrbitGrid.ChangeSensory(location,GetScanningRange(),-1);


		//	if (turret->groundScanningRange > 0.f)
		//	{
		//		Grid::actualWorld.RemoveSensorCoverage(skySensorState);
		//	}
		//}

		///*virtual override*/ void	Satellite::MoveWorldSensory(const Grid::LCellIdI&from, const Grid::LCellIdI&to)
		//{
		//	if (from != to)
		//	{
		//		Grid::visibleOrbitGrid.ChangeSensory(from,GetScanningRange(),-1);
		//		Grid::visibleOrbitGrid.ChangeSensory(to,GetScanningRange(),+1);


		//		if (turret->groundScanningRange > 0.f)
		//		{
		//			Grid::actualWorld.RemoveSensorCoverage(skySensorState);

		//			float range = EMPModify(turret->groundScanningRange,2);
		//			Grid::actualWorld.MapSensorState(to.stackID,range,to.height,turret->groundScannerPenetration,skySensorState);
		//			Grid::actualWorld.UpdateSensorState(skySensorState);
		//			Grid::actualWorld.InsertSensorCoverage(skySensorState);
		//		}
		//	}


		//}

		#if 0

			/*virtual*/ void WorldEntity::RegisterWorldSensory()
			{
				Grid::actualWorld.InsertSensor(location,GetScanningRange());

			}

			/*virtual*/ void WorldEntity::UnregisterWorldSensory()
			{
				Grid::actualWorld.RemoveSensor(location,GetScanningRange());
			}

			/*virtual*/ void	WorldEntity::MoveWorldSensory(const Grid::LCellIdI&from, const Grid::LCellIdI&to)
			{
				Grid::actualWorld.MoveSensor(from,to,GetScanningRange());
			}

		#endif /*0*/



		/*virtual override*/ void Satellite::DescribeType(BaseCommand::Label&outResult, UINT32 descFlags)
		{
			Super::DescribeType(outResult,descFlags);
			outResult << "";
			String speed = "Speed: "+String(this->GetMovementStepsPerRound()) + " (orbital)";

			outResult << speed;
			int opSpeed = GetOperationSpeed(Component::Construct);
			if (opSpeed)
				outResult << "Effective construction speed: "+String(ResourceTag(Resource::Minerals))+String(opSpeed)+" per round";

			const String fuel (ResourceTag(Resource::Fuel));

			int demand = this->GetFuelConsumptionPerRound();
			if ((descFlags & DescIncludeFuelDemandModifier) && !this->RequiresFuel())
				demand = 0;
			if ((descFlags & DescIncludeFuelDemandModifier) && demand)
			{
				outResult << "Consumes"+fuel+String(demand)+'+'+String(GetExtraFuelDemand())+" per round";
			}
			else
			{
				outResult << "Consumes"+fuel+String(demand)+" per round";
			}



			outResult.MarkBriefEnd();
			outResult<<"";
			outResult<< "Satellites hover high above the surface on a singular residual height level. They constantly consume "+fuel+"fuel to preserve their height."
				"Short term failure to supply the required amount of "+fuel+" will result in height decline. Extended periods of starvation will result in satellite loss.";
		}
		


	#endif



	void			SkyHub::OnConstructionFinished(float&atTime)
	{
		Super::OnConstructionFinished(atTime);
		OffsetTime(atTime);
		UpdateConnections();
	}


	#ifdef HAS_WINDOW
		int			Satellite::GetExtraFuelDemand()
		{
			if (lastExtraFuelDemand >= 0)
				return lastExtraFuelDemand;
			if (!RequiresFuel() || GetFuelConsumptionPerRound() == 0)
			{
				lastExtraFuelDemand = 0;
				return lastExtraFuelDemand;
			}
			#ifndef ONE_COMPOUND_PER_FACTION
				UpdateHubMap();
				bool hasAny = hubs.IsNotEmpty();
				float distance = hasAny ? hubs.first().distance : 0;
			#else
				float distance;
				bool hasAny = !!GetClosestExternalSupplier(distance);
			#endif
			if (!hasAny)
			{
				lastExtraFuelDemand = 1000;
			}
			else
				lastExtraFuelDemand = GetFuelConsumptionPerRound() * (FuelFactorAtDistance(distance) - 1.f);
			return lastExtraFuelDemand;
		}
	#endif


	/*static*/ int2	SkyHub::AllocateProductionSlot()
	{
		static int current = 2;
		int at = current++;
		return int2(5 + (at%9),1+at/9);
	}

	/*static*/ int				Satellite::GetGroundHeight(const Grid::LCellIdI&location)
	{
		UINT32 fine = Grid::low.grid.GetCell(location.stackID).higherEquivalent;
		return Detail::WalkStack(Grid::actualWorld.cellStack[fine],false);
	}


	int						Satellite::GetGroundHeight() const
	{
		return GetGroundHeight(location);
	}


	/*static*/ int			Satellite::Detail::WalkStack(const Grid::CellStack&stack, bool failOnStructureCount)
	{
		UINT32 h;
		for (h = Grid::CellStack::Range-1; h < Grid::CellStack::Range; h--)
		{
			const Grid::CellStack::Cell&c = stack.volumeCell[h];
			if (c.content != Grid::Content::Clear)
				break;
		}
		h++;
		if (h >= Grid::CellStack::Range-1)
			return badHeight;
		if (failOnStructureCount)
		{
			const Grid::CellStack::Cell&c = stack.volumeCell[h];
			if (c.structureCounter > 1)
				return badHeight;
		}

		return Grid::CellStack::ToHeight(h);
		
	}


	#ifdef HAS_WINDOW

		/*static*/ bool			SkyHub::MyCmd::CanBuild(const PStructure&what, const PWorldEntity&who)
		{
			PSkyHub hub = To<SkyHub>::DynamicCast(who);
			ASSERT1__(hub,who->GetName());
		
			if (!CommonCmd::IsModeActiveC<CursorMode::Idle>(who,true)
				||
				!hub->connectedTo.expired())
				return false;

			Grid::LCellIdI loc = hub->GetLastPointOfOrigin();
			int h = GetStructureBuildHeight(loc);
			return h != badHeight && !GetVehicle(Grid::LCellIdI(loc.stackID,h));
		}



		/*static*/ void SkyHub::MyCmd::Build(const PSatellite&p, const PWorldEntity&v)
		{
			PMobileEntity mob = To<MobileEntity>::DynamicCast(v);
			if (!mob)
				return;
			SignalConstructionBegin(mob->id, p->GetCTypeName(), ConstName::Empty);
		}

	#endif

	int				SkyHub::GetStructureBuildHeight() const
	{
		return GetStructureBuildHeight(location);
	}

	/*static*/ int				SkyHub::GetStructureBuildHeight(const Grid::LCellIdI&satelliteLocation)
	{
		#ifdef HYBRID_GRID
			index_t finer = Grid::low.grid.GetCell(satelliteLocation.stackID).higherEquivalent;
			const auto&c = Grid::high.grid.GetCell(finer);
		#else
			index_t finer = who->location.stackID;
		#endif
		int h = Detail::WalkStack(Grid::actualWorld.cellStack[finer],true);
		if (h == badHeight)
			return badHeight;
		#ifdef HYBRID_GRID
			for (int j = 0; j < c.neighborhood.numNeighbors; j++)
			{
				int h2 = Detail::WalkStack(Grid::actualWorld.cellStack[c.neighborhood.neighborID[j]],true);
				if (h2 == badHeight)
					return badHeight;
				h = std::max(h2,h);
			}
		#endif
		return h <= Grid::CellStack::TopHeight ? h : badHeight;
	}

	/*static*/ void SkyHub::RegisterProduction(const PStructure&s)
	{
		#ifdef HAS_WINDOW
			int2 at = AllocateProductionSlot();
			structureConstructionCommands.Append(
				StructureConstructionCommand(at, /*"Build " + */s->GetTypeName(), s,
				std::bind(&Component::Terraformer::Cmd::Construct, s, std::placeholders::_1),
				[s](const PWorldEntity&e, bool controlEnabled)->BaseCommand::Flags
				{
					return MyCmd::CanBuild(s,e) ? BaseCommand::Flags::IsEnabled : BaseCommand::Flags::None;
				}
				));
			structureConstructionCommands.last().RasterizeImage();
		#endif
	}

	/*static*/ void SkyHub::RegisterProduction(const PSatellite&s)
	{
		#ifdef HAS_WINDOW
			int2 at = AllocateProductionSlot();
			satelliteConstructionCommands.Append(
				SatelliteConstructionCommand(at, /*"Build " + */s->GetTypeName(), s,
				std::bind(&MyCmd::Build, s, std::placeholders::_1),
				&CommonCmd::IdleCheck));
		#endif
	}


	/*virtual override*/ void			SkyHub::PrototypeSetup()
	{
		Super::PrototypeSetup();
		RegisterProduction(SkyPort::globalPrototype);
		RegisterProduction(SkyGuardian::globalPrototype);
		RegisterProduction(SkyHub::globalPrototype);
		RegisterProduction(Ragnarok::globalPrototype);
		//RegisterProduction(StarPort::globalPrototype);
	}


	void			SkyHub::UpdateConnections()
	{
		#ifdef HAS_WINDOW
			FlushFuelDemandCache();
		#endif
		PSkyLinkStructure port = connectedTo.lock();
		WorkingStructure*ws = port ? port->CastToStructure() : nullptr;

		if (port && ws->health <= 0)
		{
			connectedTo.reset();
			port.reset();
		}

		if (ws)
		{
			if (ws->location.stackID != location.stackID || !ws->IsEnabled() || ws->IsHibernating())
			{
				LoseGroundConnection();
				port.reset();
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
			}
		}
	}

	/*static*/ float				Satellite::FuelFactorAtDistance(float quadraticDistanceFromSource)
	{
		return 1.f + (quadraticDistanceFromSource*0.0005f);

	}


	#ifdef ONE_COMPOUND_PER_FACTION
		#if defined HAS_WINDOW || !defined CLIENT
			PSkyHub	Satellite::GetClosestExternalSupplier(float&quadraticDistance)
			{
				float distance = std::numeric_limits<float>::max();
				PSkyHub found;
				#ifdef CLIENT
					for (const auto&port : skyLinks)
						if (port.second->owner == owner && !port.first->connectedToHub.expired())
						{
							PSkyHub hub = port.first->connectedToHub.lock();
							if (hub.get() != this)
							{
								float d2 = Vec::quadraticDistance(hub->GetWorldPosition(),GetWorldPosition());
								if (d2 < distance)
								{
									distance = d2;
									found = hub;
								}
							}
						}
				#else
					for (const auto&sat : owner->skyHubs)
					{
						PSkyHub hub = sat.lock();
						if (hub && hub.get() != this && !hub->connectedTo.expired())
						{
							float d2 = Vec::quadraticDistance(hub->GetWorldPosition(),GetWorldPosition());
							if (d2 < distance)
							{
								distance = d2;
								found = hub;
							}
						}
					}
				#endif
				quadraticDistance = distance;
				return found;
			}
		#endif
	#else

		void	Satellite::UpdateHubMap()
		{
			hubs.Clear();
			#ifdef CLIENT
				for (const auto&port : skyLinks)
					if (port.second->owner == owner && port.first->connectedToHub)
					{
						PSkyHub hub = port.first->connectedToHub;
						if (hub.get() != this)
						{
							hubs.Append().hub = hub;
							hubs.last().distance = Vec::quadraticDistance(hub->GetWorldPosition(),GetWorldPosition());
						}
					}
			#else
				for (const auto&sat : owner->skyHubs)
				{
					PSkyHub hub = sat.lock();
					if (hub && hub.get() != this && !hub->connectedTo.expired())
					{
						hubs.Append().hub = hub;
						hubs.last().distance = Vec::quadraticDistance(hub->GetWorldPosition(),GetWorldPosition());
					}
				}
			#endif

			std::sort(hubs.begin(),hubs.end());
		}
	#endif


	#ifndef CLIENT



		PSkyLinkStructure				Satellite::RemoteSupplyFuel(float&atTime)
		{
			#ifndef ONE_COMPOUND_PER_FACTION
				UpdateHubMap();
			#endif
			const int baseDemand = GetFuelConsumptionPerRound();
			ASSERT2__(baseDemand > 0,baseDemand,GetName());

			#ifndef ONE_COMPOUND_PER_FACTION
				for (const auto&item : hubs)
			#endif
			{
				float distance;
				#ifndef ONE_COMPOUND_PER_FACTION
					PSkyHub hub = item.hub.lock();
					distance = item.distance;
				#else
					PSkyHub hub = GetClosestExternalSupplier(distance);
				#endif
				if (hub)
				{
					PSkyLinkStructure port = hub->connectedTo.lock();
					ASSERT1__(port,hub->GetName());
					//lastExtraFuelDemand = baseDemand * FuelFactorAtDistance(item.distance);
					int demand = (int)(baseDemand * FuelFactorAtDistance(distance));
					if (location.height < BaseResidenceHeight)
						demand *= 2;

					Compound&compound = port->CastToStructure()->GetCompound();
					int available = compound.total.storage.level[Resource::Fuel];
					//#ifdef ONE_COMPOUND_PER_FACTION
					available += compound.GetOverproduction(Resource::Fuel);
					//#endif

					if (available >= demand)
					{
						MobileResourceEnvironment::ForceChangeCompoundStorage(compound, this, atTime, Resource::Fuel, -demand);
						return port;
					}
				}
			}
			return PSkyLinkStructure();
		}


		/*virtual override*/ bool		Satellite::ProcessRound(float&atTime)
		{
			if (!this->RequiresFuel())
				return true;
			return !!RemoteSupplyFuel(atTime);
		}

		/*virtual override*/ bool		SkyHub::ProcessRound(float&atTime)
		{
			PSkyLinkStructure port = connectedTo.lock();
			if (!port)
			{
				port = RemoteSupplyFuel(atTime);
				if (!port)
					return false;
			}

			if (MobileEntityProducer::queue.IsNotEmpty())
			{
				Compound&cmp = port->CastToStructure()->GetCompound();
				if (cmp.total.balance.level[Resource::Minerals] >= (int)MobileEntityProducer::productionPerRound)
				{
					MobileEntityProducer::Tick(cmp.total.balance,atTime);
				}
				else
					LogEvent(EventClass::Preset::operationAborted,this,"Insufficient resources available in "+port->CastToStructure()->GetNiceName()+". Delaying production");
			}
			return true;
		}


		/*virtual override*/ MobilePlacementResult	Satellite::PlaceExactlyAt(const Grid::LCellIdI&_cell, float timeCost, float minTimeOffset, bool signalEvent/*=true*/, bool isCreated/*=true*/, bool failIsFatal/*=false*/)
		{
			UINT32 cell = _cell.stackID;
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
			timeAt = std::max(minTimeOffset,timeAt);

			if (location.stackID != cell) 
			{
				PSatellite v = GetSatellite(cell);
				if (v)
				{
					if (failIsFatal)
					{
						LogEvent(EventClass::Preset::fatal,this,"Target cell ("+String(cell)+") is obstructed by satellite");
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
			
			location = _cell;
	
			MobilePlacementResult result = MobilePlacementResult::PlacementOkay;
			if (old != location)
			{
				if (isCreated)
				{
					if (constructionRoundCounter)
					{
						constructionRoundCounter = 0;
						RegisterChange(WorldEntity::TasksChanged,timeAt);
					}
				}
				Super::HandleMotion(result,signalEvent,old,timeCost);
			}
			#ifdef _DEBUG
				ASSERT__(Failed(result) || hadTasks == HasTasks());
			#endif
			return result;

		}

	
		bool	Satellite::DirectTo(mode_t mode, ArrayRef<Grid::LCellIdI> waypoints, bool detailedPath, bool leftClick /*= false*/)
		{
			switch (mode)
			{
				case CursorMode::Idle:
					return PlotCourse(waypoints,detailedPath);
				break;
				case CursorMode::DigTunnelMode:
					return false;

				case CursorMode::BuildConnectorChainMode:
					if (!GetOperationSpeed(Component::Construct))
						return false;

					if (leftClick)
					{
						/*if (!Grid::LowCellIsClear(destination))
							destination ++;*/
						foreach (waypoints,way)
							if (myTasks.Count() < MaxProductionTasks)
								RegisterNewJob("", PVehicleJob(new VehicleJob::Sequence(*this,mode)),way);
							else
								break;
					}
					else
						PlotCourse(waypoints,detailedPath);
				break;
				case CursorMode::AttackMode:
					if (!CanAttack())
						return false;

					if (leftClick)
					{
						for (const auto&destination : waypoints)
						{
							if (destination.stackID != location.stackID || turret->trajectoryType == TrajectoryType::VerticalBeam)
							{
								if (turret->trajectoryType != TrajectoryType::VerticalBeam)
								{
									Grid::LCellIdI pointOfOrigin = GetLastPointOfOrigin();
									//attack logic here
						
									if (!CanHitFrom(pointOfOrigin,destination))
									{
										LogEvent(EventClass::Preset::commandRejected,this,"Destination out of range");
										return false;
									}
								}
								if (!AttackNext(destination,false))
									return false;
							}
							else
								LogEvent(EventClass::Preset::commandRejected,this,"Attempting to attack current location");
						}
					}
					else
						PlotCourse(waypoints,detailedPath);
				break;
			}
			return true;

		}
	#endif


	#if 0

	/*virtual override*/ bool					SkyGuardian::CanHitFrom(const Grid::LCellIdI&origin, const TVec3<>&dest) const
	{
		return Turret::CanHit(TrajectoryType::Missile,FlatRange,location,0.f,dest);

	}
	/*virtual override*/ bool					SkyGuardian::CanHitFrom(const Grid::LCellIdI&origin, const Grid::LCellIdI&dest) const
	{
		return Turret::CanHit(TrajectoryType::Missile,FlatRange,location,0.f,dest);

	}

	/*virtual override*/ bool					SkyGuardian::ExtCanHit(const Grid::LCellIdI&attackFrom,const Grid::LCellIdI&attackTarget,Grid::LCellIdI&actual) const
	{
		//const float velocity = Turret::MaxRangeToVelocity(GetFlatAttackRange());
		const float range = GetFlatAttackRange();
		//const float extRange = range + (turret->explosionRadius);
		//const float range2 = sqr(range);
		//const float extRange2 = sqr(extRange);
		//const TVec3<> source3 = Grid::ToWorld(source);
		TVec3<> dest3 = Grid::ToWorld(destIn);
		//float distance2 = Vec::quadraticDistance(source3,dest3);
		//if (Turret::GetAngleForDistance(velocity,source,destIn))
		//if (distance2 > extRange2)
		//	return false;
		const float explosionRadius = turret->explosionRadius;
		const float explosionRadius2 = sqr(explosionRadius);
		if (Turret::CanHit(turret->trajectoryType, range,source,zOffset,destIn)
			&& CheckArea(myFaction,destIn,explosionRadius))
		{
			destOut = destIn;
			return true;
		}

		if (!Turret::CanHit(turret->trajectoryType, range+explosionRadius*2,source,zOffset,destIn))
			return false;

		AreaHelper	helper;
		//queue << destIn;
		{
			const auto&n = Grid::low.grid.GetCell(destIn.stackID).neighborhood;
			for (int i = 0; i < n.numNeighbors; i++)
				helper << Grid::LCellIdI(n.neighborID[i],destIn.height);
			helper << destIn + 1;
			helper << destIn - 1;
		}
		Grid::LCellIdI current;
		while (helper >> current)
		{
			TVec3<> world = Grid::ToWorld(current);
			float fromDest2 = Vec::quadraticDistance(dest3,world);
			if (fromDest2 > sqr(turret->explosionRadius))
				continue;
			if (!Grid::actualWorld.LowCellIsClear(current) || 
				!Turret::CanHit(turret->trajectoryType, range,source,zOffset,current))
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

	#endif /*0*/


	/*virtual override*/ bool					Satellite::HasAmmoForShot()
	{
	#ifdef ONE_COMPOUND_PER_FACTION
		const Compound&compound = owner->compound;
		return compound.total.storage.level[turret->ammoType] >= turret->ammoNeedPerShot;
	#else
		return currentStorage.level[turret->ammoType] >= turret->ammoNeedPerShot;
	#endif
	}

	
	#ifndef CLIENT
		/*virtual override*/ bool					Satellite::FillAttackQueue()
		{
			if (!turret->CanAttack() || !HasAmmoForShot())
			{
				//LogEvent(EventClass::Preset::commandRejected,this,"Insufficient ammunition");
				return false;
			}

			DBG_ASSERT__(IsSentry());
			{
				Grid::LCellIdI target;

				const ResourceVector&storage = 
					#ifndef ONE_COMPOUND_PER_FACTION
						this->currentStorage
					#else
						this->owner->compound.total.storage
					#endif
				;

				if (GetSentryTarget(*this, storage.level[turret->ammoType], location,0.f,*this, target))
				{
					AttackNext(target,true);
					return true;
				}
			}
			//else
				//return false;
			return false;

		}



		/*virtual override*/ bool			SkyGuardian::ExtCanHit(const Grid::LCellIdI&attackFrom,const Grid::LCellIdI&attackTarget,Grid::LCellIdI&actual) const
		{
			return TurretBase::ExtCanHit(owner,attackFrom,0.f,attackTarget,actual);
		}



		/*virtual override*/ bool	Satellite::AttackNext(Grid::LCellIdI target, bool autoTarget)
		{
			ASSERT__(!TasksAreLocked());
			if (!CanAttack())
				return false;
			if (myTasks.Count() >= MaxStructureTasks)
			{
				LogEvent(EventClass::Preset::commandRejected,this,"Max queue length ("+String(MaxStructureTasks)+") reached");
				return false;
			}
			Grid::LCellIdI pointOfOrigin = GetLastPointOfOrigin();
			if (target.stackID == pointOfOrigin.stackID && turret->trajectoryType != TrajectoryType::VerticalBeam)
			{
				LogEvent(EventClass::Preset::commandRejected,this,"Destination too close");
				return false;
			}
			if (turret->trajectoryType != TrajectoryType::VerticalBeam)
			{
				//Grid::LCellIdI pointOfOrigin = location+1;


				//float targetDistance = Grid::GetGeometricDistance(pointOfOrigin,target);

				if (!CanHitFrom(pointOfOrigin,target))
				{
					LogEvent(EventClass::Preset::commandRejected,this,"Destination out of range");
					return false;
				}
			}

			if (!HasTasks() || !myTasks.GetNewest().IsMode(CursorMode::AttackMode))
			{
				myTasks << Task("Attack Position",target,false,Task::Mode,CursorMode::AttackMode);
				myTasks.GetNewest().Replot(pointOfOrigin,this,FocusChangeType::FullChange,true);
				RegisterChange(TasksChanged,-1);
			}
			else
			{
				//if (!tasks.GetNewest().foci.Contains(destination))	//irrelevant. attack same location as often as you want
				{
					myTasks.GetNewest().foci.Push().target = target;
					myTasks.GetNewest().Replot(pointOfOrigin,this,FocusChangeType::FocusAppended,true);
					RegisterChange(TasksChanged,-1);
				}
			}
			return true;

		}


		/*virtual override*/ bool SkyGuardian::WouldAttack(const PWorldEntity&target, Grid::LCellIdI&outTarget, float entityTime)
		{
			if (!turret->CanAttack() || !HasAmmoForShot())
			{
				//LogEvent(EventClass::Preset::commandRejected,this,"Insufficient ammunition");
				return false;
			}

			static Buffer0<TVec4<> > trajectory;

			TVec3<> iat;
			Grid::HCellIdI ic;


			//float d2 = Grid::GetQuadraticGeometricDistance(location+1,target->location);

			return		//owner->ExtCanSee(target,entityTime)
					/*&&	*/
						turret->CanAttack() 
					&& timeAt <= entityTime 
					&& CountPossibleAttacks() > 0 
					&& ExtCanHit(location,target->location,outTarget)
					//&& d2 > sqr(turret->explosionRadius)
					&& TraceTrajectory(owner->currentState, target->location,trajectory,iat,ic)==TurretTrajectory::ReachesTarget;
		}


		


	#endif

	bool				SkyHub::GainGroundConnection(const PSkyLinkStructure&to)
	{
		{
			PSkyLinkStructure port = connectedTo.lock();
			if (port && port->CastToStructure()->location.stackID == location.stackID)
			{
				#ifndef CLIENT
					PSkyHub other = port->connectedToHub.lock();
					ASSERT2__(other,GetName(),port->CastToStructure()->GetName());
					LogEvent(EventClass::Preset::operationAborted,this,"Cannot connect to "+port->CastToStructure()->GetNiceName()+": Already connected to "+other->GetNiceName());
				#endif
				return false;
			}
		}

		connectedTo = to;
		#ifndef CLIENT
			timeAt+=timeEpsilon;
			SignalChange(OrbitalConnectionChanged,timeAt);
			WorkingStructure*ws = to->CastToStructure();
			ws->SignalChange(OrbitalConnectionChanged,timeAt);
			LogEvent(EventSignificance::Common,this,"Connection to "+ws->GetNiceName()+" established");
		#endif

		to->connectedToHub = To<SkyHub>::Cast(shared_from_this());

		return true;
	}

	void				SkyHub::LoseGroundConnection()
	{
		PSkyLinkStructure port = connectedTo.lock();
		if (port)
		{
			WorkingStructure*ws = port->CastToStructure();
			#ifndef CLIENT
				LogEvent(EventClass::Preset::operationAborted,this,"Connection to "+ws->GetNiceName()+" lost");
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

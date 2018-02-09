#include "nexusPresence.h"
#include "../logEvent.h"
#include "../grid/base.h"
#include "../grid/worldData.h"
#include "gridAndChoices.h"
#include "faction.h"
#include "prototypes.h"
#include "structure/tower.h"
#include "structure/vehiclePlant.h"

namespace NexusPresence
{



	


	//void			SpawnNexusBaseAtPoleWithPort(int pole);
	void			SpawnNexusPortAtPole(int pole)
	{
		LogEvent(EventSignificance::Common,"Spawning Nexus Port at pole "+String(pole));

		Grid::LCellIdI location((UINT32)pole,0);	//first 12 are corner cells
		int h = Objects::FindApplicableGround(location,true,true); //do not submerge into lava
		if (h == Objects::badHeight)
			h = 0;	//just spawn it near median
		static const int R = 5;
		h = M::clamp(h,-Grid::CellStack::HalfRange + R + 1,Grid::CellStack::HalfRange - R - 2);

		location.height = h;
		std::shared_ptr<Objects::NexusPort> port = To<Objects::NexusPort>::Cast(
		Objects::NexusPort::globalPrototype->CreateNewInstance(Objects::Faction::nexus,location,0));
		port->ClearOccupiedTerrain(0);
		float t = 0;
		Objects::InsertConstructed(port,t);
//		if (enableNexus)
		{
			port->SetEnabled(true,t,false,Objects::WorkingStructure::ForceEnable);
			Objects::RefreshCompoundCoverageIfNeeded(t);
			//ASSERT__(port->IsEnabled());
		}
//			const auto&n = Grid::low.grid.GetCell(i).neighborhood;
		Grid::Resource::Require(ConstName::Resource_Fuel).coverage[pole] = 0;
		Grid::Resource::Require(ConstName::Resource_Minerals).coverage[pole] = 0;
		Grid::Resource::Require(ConstName::Resource_Uranium).coverage[pole] = 0;
	}





	void SpawnNexusBaseAtPoleWithPort(int pole)
	{
		LogEvent(EventSignificance::Common,"Spawning Nexus base at pole "+String(pole));

		auto sensorLock = Objects::Faction::nexus->LockSensorUpdates();

		Objects::PNexusPort port;
		int h;
		for (h = - Grid::CellStack::HalfRange; h < Grid::CellStack::HalfRange; h++)
		{
			port = To<Objects::NexusPort>::DynamicCast(Objects::GetStructure(Grid::LCellIdI((UINT32)pole,h)));
			if (port)
				break;
		}
		if (!port)
		{
			FATAL__("Unable to find port at supposed nexus pole "+String(pole));
			return;
		}

		Grid::LCellIdI location = port->location;

		bool enableNexus = false;
		{
			//if (random.getFloat() < 0.8f)
			{

				//bool deadly = random.getFloat() > 0.4f;


			//	bool rebuild[BaseGrid::NumFamilies] = { false };

				Grid::LowSetup::TDistanceField field;
				#ifdef LOWER_RESOLUTION_PLANET
					static const int R = 4;
					static const int MissileDistance = 4;
					static const int RadarDistance = 3;
					static const int OuterFactoryDistance = 2;
					static const int MaxFactories = 3;
				#else
					static const int R = 5;
					static const int MissileDistance = 5;
					static const int RadarDistance = 4;
					static const int OuterFactoryDistance = 3;
					static const int MaxFactories = 4;
				#endif

				Grid::low.UpdatePlanarDistanceField(pole,R+1,field);

				Ctr::Buffer0<index_t>	distances;


				for (index_t i = 0; i < field.NumEntries; i++)
				{
					if (field.distance[i] == 0xFFFFFFFF)
						continue;
					if (field.distance[i] <= R)
						distances << i;
					else
					{
						World::actual.Clear(Grid::LCellIdI(i,h),0);
						World::actual.Clear(Grid::LCellIdI(i,h+1),0);
					}
				}

				for (index_t i : distances)
				{
					ASSERT_LESS_OR_EQUAL__(field.distance[i],R);
					UINT32 d1 = (R - field.distance[i])+1;

					int h2 = h + d1;
					for (int z = h; z <= h2 + 2 ; z++)
					{
						World::actual.Clear(Grid::LCellIdI(i,z),0);
					//	Grid::TagLowCluster(rebuild,Grid::LCellIdI(i,z));
					}
				//	Grid::TagLowCluster(rebuild,Grid::LCellIdI(i,h2-1));
				}
				Ctr::Buffer0<Objects::PWorkingStructure>	alwaysOn;
				for (index_t i : distances)
				{
					ASSERT_LESS_OR_EQUAL__(field.distance[i],R);

					UINT32 d1 = (R - field.distance[i])+1;

					bool notDirectEdge = Grid::low.grid.GetCell(i).family != 0;
					bool noEdge = notDirectEdge;
					if (noEdge)
					{
						const auto&n = Grid::low.grid.GetCell(i).neighborhood;
						for (int j = 0; j < n.numNeighbors; j++)
							if (Grid::low.grid.GetCell(n.neighborID[j]).family == 0)
								noEdge = false;

					}

					bool notCore = field.distance[i] >= 3;

					int h2 = h + d1;

					for (int z = h-2; z < h2; z++)
					{
						if (z < h || (z >=h2-1 && notCore/* && !deadly*/) || noEdge)
						{
							World::actual.Overwrite(Grid::LCellIdI(i,z),Grid::Content::Rock,0);
						}
						else
							if (!notDirectEdge)
								break;

					}
				}
				for (index_t i : distances)
				{
					bool notDirectEdge = Grid::low.grid.GetCell(i).family != 0;
					if (notDirectEdge || field.distance[i] == 1)
						for (int z = h-1; z < h; z++)
						{
							World::actual.Overwrite(Grid::LCellIdI(i,z),Grid::Content::Road,0);
						}
				}


				int factoryCount = 0;
				int generatorCount = 0,
					connectorCount = 0;
				float t = 0;
				const auto radarPrototype = Objects::Prototypes::structures.Find({ConstName::Type_Tower,ConstName::Turret_Radar});
				ASSERT__(radarPrototype);
				const auto launcherPrototype = Objects::Prototypes::structures.Find({ConstName::Type_Tower,ConstName::Turret_Launcher});
				ASSERT__(launcherPrototype);
				for (index_t i : distances)
				{
					//ASSERT_LESS_OR_EQUAL__(field.distance[i],R);
					//UINT32 d1 = (R - field.distance[i])+1;

					#if 1
					if (Grid::low.grid.GetCell(i).family == 0)	//align of family members implies family zero members are along edges between nexus poles
					{
						if (field.distance[i] == MissileDistance)
						{
							#ifndef _DEBUG
								std::shared_ptr<Objects::Tower> tower = To<Objects::Tower>::Cast(
									launcherPrototype->CreateNewInstance(Objects::Faction::nexus,
									Grid::LCellIdI(i,h),
									0));
								tower->ClearOccupiedTerrain(0);
								Objects::InsertConstructed(tower,t);
							#endif
						}
						if (field.distance[i] == RadarDistance)
						{
							std::shared_ptr<Objects::Tower> tower = To<Objects::Tower>::Cast(
								radarPrototype->CreateNewInstance(Objects::Faction::nexus,
								Grid::LCellIdI(i,h),
								0));
							tower->ClearOccupiedTerrain(0);
							Objects::InsertConstructed(tower,t);
						}

						if (field.distance[i] == OuterFactoryDistance)
						{
							if (factoryCount == MaxFactories)
							{
								#ifdef LOWER_RESOLUTION_PLANET
									Objects::PGenerator generator = To<Objects::Generator>::Cast(
										Objects::Generator::globalPrototype->CreateNewInstance(Objects::Faction::nexus,
										Grid::LCellIdI(i,h),
										0));
									generator->ClearOccupiedTerrain(0);
									Objects::InsertConstructed(generator,t);
									generatorCount++;
									alwaysOn << generator;	//only need three generators switched on
								#else
									Objects::PMunitionFactory factory = To<Objects::MunitionFactory>::Cast(
										Objects::MunitionFactory::globalPrototype->CreateNewInstance(Objects::Faction::nexus,
										Grid::LCellIdI(i,h),
										0));
									factory->ClearOccupiedTerrain(false,0);
									Objects::InsertConstructed(factory,t);
									alwaysOn << factory;
								#endif
							}
							else
							{
								Objects::PVehiclePlant plant = To<Objects::VehiclePlant>::Cast(
									Objects::VehiclePlant::globalPrototype->CreateNewInstance(Objects::Faction::nexus,
									Grid::LCellIdI(i,h),
									0));
								plant->ClearOccupiedTerrain(0);
								Objects::InsertConstructed(plant,t);
								factoryCount++;
							}
						}

						#ifndef LOWER_RESOLUTION_PLANET
							if (field.distance[i] == (2))
							{
								if (generatorCount == 4)
								{
									Objects::PMunitionStorage storage = To<Objects::MunitionStorage>::Cast(
										Objects::MunitionStorage::globalPrototype->CreateNewInstance(Objects::Faction::nexus,
										Grid::LCellIdI(i,h),
										0));
									storage->ClearOccupiedTerrain(false,0);
									storage->storage = storage->maxStorage;
									Objects::InsertConstructed(storage,t);
									alwaysOn << storage;
								}
								else
								{
									Objects::PGenerator generator = To<Objects::Generator>::Cast(
										Objects::Generator::globalPrototype->CreateNewInstance(Objects::Faction::nexus,
										Grid::LCellIdI(i,h),
										0));
									generator->ClearOccupiedTerrain(false,0);
									Objects::InsertConstructed(generator,t);
									generatorCount++;
									if (generatorCount < 4)
										alwaysOn << generator;	//only need three generators switched on
								}
							}
						#endif

						if (field.distance[i] == 1 /*|| field.distance[i] == 4*/)
						{
							{
								Objects::PConnector connector = To<Objects::Connector>::Cast(
									Objects::Connector::globalPrototype->CreateNewInstance(Objects::Faction::nexus,
									Grid::LCellIdI(i,h),
									0));
								connector->ClearOccupiedTerrain(0);
								Objects::InsertConstructed(connector,t);
								connectorCount++;
							}
						}
					}
					else
					{
						//if (/*field.distance[i] == 3 || */field.distance[i] == 5)
						//{
						//	Objects::PConnector connector = To<Objects::Connector>::Cast(
						//		Objects::Connector::globalPrototype->CreateNewInstance(Objects::Faction::nexus,
						//		Grid::LCellIdI(i,Objects::FindApplicableGround(Grid::LCellIdI(i,h),true)),
						//		0));
						//	connector->ClearOccupiedTerrain(false,0,true);
						//	Objects::InsertConstructed(connector,0);
						//}


					}
					#endif /*0*/
				}
				for (index_t i : distances)
				{
					ASSERT_LESS_OR_EQUAL__(field.distance[i],R);
					UINT32 d1 = (R - field.distance[i]);

					bool noEdge = Grid::low.grid.GetCell(i).family != 0;
					if (noEdge)
					{
						const auto&n = Grid::low.grid.GetCell(i).neighborhood;
						for (int j = 0; j < n.numNeighbors; j++)
							if (Grid::low.grid.GetCell(n.neighborID[j]).family == 0)
								noEdge = false;

					}

					int h2 = h + d1;
					//for (int z = h; z <= h2 + 2 ; z++)
					{
						if (!noEdge)
							World::actual.Clear(Grid::LCellIdI(i,h),0);
					//	Grid::TagLowCluster(rebuild,Grid::LCellIdI(i,z));
					}
				//	Grid::TagLowCluster(rebuild,Grid::LCellIdI(i,h2-1));
				}

				for (auto ws : alwaysOn)
				{
					ws->SetEnabled(true,t,false,Objects::WorkingStructure::ForceEnable);
				}
				enableNexus = alwaysOn.IsNotEmpty();


				if (enableNexus)
				{
					port->SetEnabled(true,t,false,Objects::WorkingStructure::ForceEnable);

				}


				#ifndef NO_SATELLITES
					if (Config::Nexus::spawnSatellitesAtopBases)
					{
						Objects::PSkyHub hub = To<Objects::SkyHub>::Cast( Objects::SkyHub::globalPrototype->CreateNewInstance(port->owner,Grid::LCellIdI(location.stackID,Objects::Satellite::BaseResidenceHeight),0) );
						Objects::InsertConstructed(hub,t);
						ASSERT__(hub->connectedTo.lock() == port);

						for (index_t i : distances)
						{
							ASSERT_LESS_OR_EQUAL__(field.distance[i],R);
							UINT32 d1 = (R - field.distance[i]);

							bool noEdge = Grid::low.grid.GetCell(i).family != 0;
							if (!noEdge && field.distance[i] == 2)
							{
								Objects::PSkyGuardian guard = To<Objects::SkyGuardian>::Cast( Objects::SkyGuardian::globalPrototype->CreateNewInstance(port->owner,Grid::LCellIdI(i,Objects::Satellite::BaseResidenceHeight),0));
								Objects::InsertConstructed(guard,t);
							}
						}
					}
				#endif
			}


		}

		location.height = h;
		//std::shared_ptr<Objects::NexusPort> port = To<Objects::NexusPort>::Cast(
		//Objects::NexusPort::globalPrototype->CreateNewInstance(Objects::Faction::nexus,location,0));
		//port->ClearOccupiedTerrain(false,0,true);
		//Objects::InsertConstructed(port,0);
		//if (enableNexus)
		//{
		//	port->SetEnabled(true,0,false,true);
		//	Objects::RefreshCompoundCoverageIfNeeded(0);
		//	ASSERT__(port->IsEnabled());
		//}
//			const auto&n = Grid::low.grid.GetCell(i).neighborhood;
		Grid::Resource::Require(ConstName::Resource_Fuel).coverage[pole] = 0;
		Grid::Resource::Require(ConstName::Resource_Minerals).coverage[pole] = 0;
		Grid::Resource::Require(ConstName::Resource_Uranium).coverage[pole] = 0;





	}





	void Create(RuntimeConfig::GameGoal goal, bool & placeBases, bool & placeAllPorts, bool &placeSatellites, bool aiEnabled, bool empZones)
	{
		placeBases = placeBases && aiEnabled;
		placeAllPorts = placeAllPorts ||		//all ports by config
			(
				goal == RuntimeConfig::GameGoal::ConquerNexus && //game is conquer nexus, so we need some
				(
					empZones					//emp zones are on, so we need all ports
					|| 
					!placeBases	//nexus bases don't exist, so it makes sense to place all
				)
			);

		if (placeBases || placeAllPorts)
		{
			LogEvent(EventSignificance::Common, "Spawning Nexus Ports (EMP fields="+String(empZones)+")...");
			for (int pole = 0; pole < 12; pole++)
				if (placeAllPorts || BaseGrid::IsIsolatedCorner(pole))
					SpawnNexusPortAtPole(pole);

			LogEvent(EventSignificance::Debug, "Spawning Nexus bases...");
			if (placeBases)
			{
				for (int pole = 0; pole < 12; pole++)
					if (BaseGrid::IsIsolatedCorner(pole))
					{
						SpawnNexusBaseAtPoleWithPort(pole);
					}
			}
		}
	}
	
}


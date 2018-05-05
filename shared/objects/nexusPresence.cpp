#include "nexusPresence.h"
#include "../logEvent.h"
#include "../grid/base.h"
#include "../grid/worldData.h"
#include "gridAndChoices.h"
#include "faction.h"
#include "prototypes.h"
#include "structure/tower.h"
#include "structure/vehiclePlant.h"
#include "../grid/resources.h"

namespace NexusPresence
{
	template <typename T>
		static std::shared_ptr<T> Create(const Objects::PFaction&nexusFaction, World::Data&world, const Grid::LCellIdI&location, const Objects::PStructure&prototype)
		{
			std::shared_ptr<T> structure = To<T>::Cast(
				prototype->CreateNewInstance(nexusFaction,location,0));
			#ifdef CLIENT
				structure->id = TUniqueID::New();
			#endif
			structure->ClearOccupiedTerrain(world, 0);
			float t = 0;
			Objects::InsertConstructed(structure,t);

			return structure;
		}
	

	template <typename T>
		static std::shared_ptr<T> Create(const Objects::PFaction&nexusFaction, World::Data&world, const Grid::LCellIdI&location)
		{
			return Create<T>(nexusFaction,world,location,T::globalPrototype);
		}
	


	void			SpawnNexusPortAtPole(World::Data&world, const Objects::PFaction&nexusFaction, int pole)
	{
		LogEvent(EventSignificance::Common,"Spawning Nexus Port at pole "+String(pole));

		Grid::LCellIdI location((UINT32)pole,0);	//first 12 are corner cells
		int h = Objects::FindApplicableGround(world, location,true,true); //do not submerge into lava
		if (h == Objects::badHeight)
			h = 0;	//just spawn it near median
		static const int R = 5;
		h = M::clamp(h,Grid::CellStack::BottomHeight+1,Grid::CellStack::TopHeight -2);

		location.height = h;
		auto port = Create<Objects::NexusPort>(nexusFaction,world, location);
		#ifndef CLIENT
		{
			float t = 0;
			port->SetEnabled(true,t,false,Objects::WorkingStructure::ForceEnable);
			Objects::RefreshCompoundCoverageIfNeeded(t);
		}
		#endif

		Grid::Resource::Require(ConstName::Resource_Fuel).coverage[pole] = 0;
		Grid::Resource::Require(ConstName::Resource_Minerals).coverage[pole] = 0;
		Grid::Resource::Require(ConstName::Resource_Uranium).coverage[pole] = 0;
	}





	void SpawnNexusBaseAtPoleWithPort( World::Data&world, const Objects::PFaction&nexusFaction, int pole, bool useNexusTowers )
	{
		LogEvent(EventSignificance::Common,"Spawning Nexus base at pole "+String(pole));

		#ifndef CLIENT
			auto sensorLock = nexusFaction->LockSensorUpdates();
		#endif

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
					static const int OffensiveDistance = 4;
					static const int RadarDistance = 3;
					static const int OuterFactoryDistance = 2;
					static const int MaxFactories = 3;
				#else
					static const int R = 5;
					static const int OffensiveDistance = 5;
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
						world.Clear(Grid::LCellIdI(i,h),0);
						world.Clear(Grid::LCellIdI(i,h+1),0);
					}
				}

				for (index_t i : distances)
				{
					ASSERT_LESS_OR_EQUAL__(field.distance[i],R);
					UINT32 d1 = (R - field.distance[i])+1;

					int h2 = h + d1;
					for (int z = h; z <= h2 + 2 ; z++)
					{
						world.Clear(Grid::LCellIdI(i,z),0);
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
							world.Overwrite(Grid::LCellIdI(i,z),Grid::Content::Rock,0);
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
							world.Overwrite(Grid::LCellIdI(i,z),Grid::Content::Road,0);
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
				const auto nexusPrototype = Objects::Prototypes::structures.Find({ConstName::Type_Tower,ConstName::Turret_Nexus});
				ASSERT__(nexusPrototype);

				const auto offensivePrototype = useNexusTowers ? nexusPrototype : radarPrototype;


				#ifndef _DEBUG
					static const bool placeOffensiveTowers = true;
				#else
					static const bool placeOffensiveTowers = true;	//change if this behavior is desirable
				#endif

				for (index_t i : distances)
				{
					//ASSERT_LESS_OR_EQUAL__(field.distance[i],R);
					//UINT32 d1 = (R - field.distance[i])+1;

					#if 1
					if (Grid::low.grid.GetCell(i).family == 0)	//align of family members implies family zero members are along edges between nexus poles
					{
						if (field.distance[i] == OffensiveDistance && placeOffensiveTowers)
						{
							Create<Objects::Tower>(nexusFaction,world,Grid::LCellIdI(i,h),offensivePrototype);
						}
						if (field.distance[i] == RadarDistance && !useNexusTowers)
						{
							Create<Objects::Tower>(nexusFaction,world,Grid::LCellIdI(i,h),radarPrototype);
						}

						if (field.distance[i] == OuterFactoryDistance)
						{
							if (factoryCount == MaxFactories)
							{
								#ifdef LOWER_RESOLUTION_PLANET
									auto generator = Create<Objects::Generator>(nexusFaction,world,Grid::LCellIdI(i,h));
									generatorCount++;
									alwaysOn << generator;	//only need three generators switched on
								#else
									auto factory = Create<Objects::MunitionFactory>(nexusFaction,world,Grid::LCellIdI(i,h));
									alwaysOn << factory;
								#endif
							}
							else
							{
								Create<Objects::VehiclePlant>(nexusFaction,world,Grid::LCellIdI(i,h));
								factoryCount++;
							}
						}

						#ifndef LOWER_RESOLUTION_PLANET
							if (field.distance[i] == (2))
							{
								if (generatorCount == 4)
								{
									auto storage = Create<Objects::MunitionStorage>(nexusFaction,world,Grid::LCellIdI(i,h));
									storage->storage = storage->maxStorage;
									alwaysOn << storage;
								}
								else
								{
									auto generator = Create<Objects::Generator>(nexusFaction,world,Grid::LCellIdI(i,h));
									generatorCount++;
									if (generatorCount < 4)
										alwaysOn << generator;	//only need three generators switched on
								}
							}
						#endif

						if (field.distance[i] == 1 /*|| field.distance[i] == 4*/)
						{
							{
								Create<Objects::Connector>(nexusFaction,world,Grid::LCellIdI(i,h));
								connectorCount++;
							}
						}
					}
					else
					{
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
							world.Clear(Grid::LCellIdI(i,h),0);
					//	Grid::TagLowCluster(rebuild,Grid::LCellIdI(i,z));
					}
				//	Grid::TagLowCluster(rebuild,Grid::LCellIdI(i,h2-1));
				}

				#ifndef CLIENT
					for (auto ws : alwaysOn)
					{
						ws->SetEnabled(true,t,false,Objects::WorkingStructure::ForceEnable);
					}
					enableNexus = alwaysOn.IsNotEmpty();


					if (enableNexus)
					{
						port->SetEnabled(true,t,false,Objects::WorkingStructure::ForceEnable);

					}
				#endif


				#ifndef NO_SATELLITES
					if (Config::Nexus::spawnSatellitesAtopBases)
					{
						Objects::PSkyHub hub = To<Objects::SkyHub>::Cast( Objects::SkyHub::globalPrototype->CreateNewInstance(port->owner,Grid::LCellIdI(location.stackID,Objects::Satellite::BaseResidenceHeight),0) );
						#ifdef CLIENT
							hub->id = TUniqueID::New();
						#endif
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
								#ifdef CLIENT
									guard->id = TUniqueID::New();
								#endif
								Objects::InsertConstructed(guard,t);
							}
						}
					}
				#endif
			}


		}

		location.height = h;

		Grid::Resource::Require(ConstName::Resource_Fuel).coverage[pole] = 0;
		Grid::Resource::Require(ConstName::Resource_Minerals).coverage[pole] = 0;
		Grid::Resource::Require(ConstName::Resource_Uranium).coverage[pole] = 0;





	}





	void Create( World::Data&world, const Objects::PFaction&nexusFaction, bool placeBases, bool & placeAllPorts, bool placeSatellites, bool placeNexusTowers )
	{
		placeAllPorts = placeAllPorts ||		//all ports by config
			(
				RuntimeConfig::gameGoal == RuntimeConfig::GameGoal::ConquerNexus && //game is conquer nexus, so we need some
				(
					RuntimeConfig::empZones					//emp zones are on, so we need all ports
					|| 
					!placeBases	//nexus bases don't exist, so it makes sense to place all
				)
			);

		if (placeBases || placeAllPorts)
		{
			LogEvent(EventSignificance::Common, "Spawning Nexus Ports (EMP fields="+String(RuntimeConfig::empZones)+")...");
			for (int pole = 0; pole < 12; pole++)
				if (placeAllPorts || BaseGrid::IsIsolatedCorner(pole))
					SpawnNexusPortAtPole(world,nexusFaction, pole);

			LogEvent(EventSignificance::Debug, "Spawning Nexus bases...");
			if (placeBases)
			{
				for (int pole = 0; pole < 12; pole++)
					if (BaseGrid::IsIsolatedCorner(pole))
					{
						SpawnNexusBaseAtPoleWithPort(world,nexusFaction, pole, placeNexusTowers);
					}
			}
		}
	}

	#ifndef CLIENT
		void Create( bool placeBases, bool & placeAllPorts, bool placeSatellites, bool placeNexusTowers )
		{
			Create(World::actual,Objects::Faction::nexus,placeBases,placeAllPorts,placeSatellites,placeNexusTowers);
		}
	#endif
	
}


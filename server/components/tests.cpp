#include <global_root.h>
#include "tests.h"




namespace Objects
{
	/**
	Extracts all possible user choices for the current state of a given entity, and an active cursor mode.
	Usually this function is not required to be called manually.
	*/
//	void			GetChoices(const PWorldEntity&entity, Vehicle::mode_t activeCursorMode, BasicBuffer<Grid::LCellIdI>&movementChoices, bool&showArrow, bool&hasArrowCellTarget, TVec3<>&arrowFrom, Grid::LCellIdI&arrowFromCell);


}

namespace Test
{

	void Run(Test&test)
	{
		std::cout << "Running test '" << test.name << "'..." << std::endl;
		//std::cout << "Running test '" << typeid(test).name() << "'..." << std::endl;
		Random random;
		Timer::Time t0 = timer.Now();
		test.Run(random);
		std::cout << "    ...completed. No errors detected after " << timer.GetSecondsSince(t0) << " seconds" << std::endl;
	}




	/*static*/ ServerClient	*ServerClient::executingInstance = nullptr;

	/*static*/ void		ServerClient::OnMessage(String&msg, TCP::Peer&peer)
	{
		const bool isClient = &peer == &executingInstance->client;
		if (isClient)
			executingInstance->messageChannel.SendTo(peer,msg);
		else
			executingInstance->quit = true;
	}


	/*virtual override*/ void	ServerClient::Run(Random&random)
	{

		executingInstance = this;

		client.InstallChannel(messageChannel);
		server.InstallChannel(messageChannel);

		server.SetAsync(false);
		client.SetAsync(false);

		const count_t NumIterations = 10;
		for (index_t i = 0; i < NumIterations; i++)
		{
			std::cout << " starting test iteration "<<i<<"/"<<NumIterations<<std::endl;
			quit = false;
			USHORT port;
			ASSERT__(server.StartService(0,true,&port));

			ASSERT__(client.Connect("localhost:"+String(port)));

			ASSERT__(messageChannel.SendTo(client,"hello"));

			Timer::Time t0 = timer.Now();
			for (;;)
			{
				server.Resolve();
				client.Resolve();


				if (!client.IsAttemptingToConnect() && !client.IsConnected())
				{
					FATAL__("connection lost");
				}

				if (quit)
					break;

				float sec = timer.GetSecondsSince(t0);
				if (sec > 10)
				{
					FATAL__("Transmission failed for "+String(sec)+" seconds");
				}
				Sleep(100);
			}

			client.Disconnect();
			server.EndService();

		}



	}



	/*virtual override*/ void	Journal::Run(Random&random)
	{
		Grid::TPlanetConfig	config;
		Objects::FlushAllData();
		Grid::ResetStructureCounters();
		config.extendedCore.heightScale = 0.f;
		config.extendedCore.baseHeight = -Grid::CellStack::HalfRange;
		config.liquidHeight = -Grid::CellStack::HalfRange;
		config.mountains.baseHeight = 1;
		config.mountains.heightScale = 1.5f;	//cause turbulence
		config.RandomizeSeeds(random);
		Grid::actualWorld.BuildWorld(config);
		Grid::actualWorld.ExportDiscovered(Grid::initialWorld,Grid::WorldDiscovery::FullTerrain);







		Objects::FlushAllData();
		Grid::ResetStructureCounters();
	}




	/*virtual override*/ void	Serialization::Run(Random&random)
	{
		ApplicationInterface::globalVersionOfCurrentFile = ApplicationInterface::SupportedFileVersion;
		for (index_t i = 0; i < 3000; i++)
		{
			Objects::Marker m;
			m.id.Generate();
			m.createdInRound = random.Next(MaxSerialSize);
			if (random.NextFloat() > 0.1f)
			{
				m.location.stackID = random.Next(Grid::LowSetup::GridType::CellCount - 1);
				m.location.height = random.Next(-Grid::CellStack::HalfRange, Grid::CellStack::HalfRange - 1);
			}
			m.type = Objects::Marker::Type::Reinterpret(random.NextIndex(Objects::Marker::Type::NumberOfPossibilities));
			if (m.UtilizesReferenceEntity())
			{
				m.referenceEntity.Generate();
				if (random.NextBool())
				{
					m.referencedType[0] = Objects::structurePrototypes[random.NextIndex(Objects::structurePrototypes.Count())]->GetCTypeName();
					m.referencedType[1] = ConstName::Empty;
				}
				else
				{
					m.referencedType[0] = Objects::Component::baseComponents[random.NextIndex(Objects::Component::baseComponents.Count())]->constName;
					m.referencedType[1] = Objects::Component::allTurretComponents[random.NextIndex(Objects::Component::allTurretComponents.Count())]->constName;
				}
			}
			Protocol::VerifySerialization(m);
		}

		{
			Protocol::VerifySerialization(Protocol::LCellIdI(Grid::LCellIdI::Invalid));
			for (int i = 0; i < 10000; i++)
			{
				Protocol::LCellIdI coords;
				coords.stackID = random.Next(Grid::LowSetup::GridType::CellCount-1);
				coords.height = random.Next(-Grid::CellStack::HalfRange, Grid::CellStack::HalfRange);
				Protocol::VerifySerialization(coords);


				Protocol::PositiveResourceVector vec;
				for (index_t i = 0; i < Protocol::PositiveResourceVector::NumEntries; i++)
				{
					if (random.NextBool())
						vec.level[i] = 0;
					else
						vec.level[i] = cubic(random.Next(100));
				}

				Protocol::VerifySerialization(vec);

			}


		}

	}

	Objects::PWorldEntity CreateStructure(const Objects::OwnerParameter&owner, const Objects::PStructure&prototype, const Grid::LCellIdI&location)
	{
		Objects::PStructure result(To<Objects::Structure>::Cast( prototype->CreateNewInstance(owner, location, 0)));
		result->CorrectTerrain(true, 0);
		float t = 0;
		Objects::InsertConstructed(result, t);
		return result;
	}

	template <typename T>
	std::shared_ptr<T> CreateStructure(const Objects::OwnerParameter&owner, const Grid::LCellIdI&location)
	{
		return To<T>::Cast(CreateStructure(owner,T::globalPrototype,location));
		//T::globalPrototype->CreateNewInstance(owner, location, 0)));
		//result->CorrectTerrain(true, 0, true);
		//Objects::InsertConstructed(result, 0);
		//return result;
	}


	/*virtual override*/ void	AutoTargeting::Run(Random&random)
	{
		Grid::TPlanetConfig	config;
		Objects::FlushAllData();
		Grid::ResetStructureCounters();
		config.extendedCore.heightScale = 0.f;
		config.extendedCore.baseHeight = -Grid::CellStack::HalfRange;
		config.liquidHeight = -Grid::CellStack::HalfRange;
		config.mountains.baseHeight = 1;
		config.mountains.heightScale = 1.5f;	//cause turbulence
		config.RandomizeSeeds(random);
		Grid::actualWorld.BuildWorld(config);
		Grid::actualWorld.ExportDiscovered(Grid::initialWorld,Grid::WorldDiscovery::FullTerrain);



		Objects::PFaction faction0(new Objects::Faction(TUniqueID::Empty())),
							faction1(new Objects::Faction(TUniqueID::Empty()));

		Objects::Faction::all << faction0 << faction1;

		//Objects::PTower prototype = To<Objects::Tower>::DynamicCast( Objects::GetPrototypeByName(ConstName::Type_ArtilleryTower));

		Buffer0<TVec4<> > trajectory;
		TVec3<> impactAt;
		Grid::HCellIdI impactCell;
		auto turret = &Objects::Component::artillery;
		int outOfScanning = 0;
		auto original = turret->explosionRadius.constOverride;
		for (int i = 0; i < 100 || outOfScanning < 10; i++)
		{
			turret->explosionRadius.SetConst(random.NextFloat(1,4));

			Grid::LCellIdI	generatorAt = RandomCell(random);
			generatorAt.height = Objects::FindApplicableGround(generatorAt,true);
			while (generatorAt.height == Objects::badHeight)
			{
				generatorAt = RandomCell(random);
				generatorAt.height = Objects::FindApplicableGround(generatorAt,true);
			}
			Objects::PGenerator generator(CreateStructure<Objects::Generator>(faction0, generatorAt));

			#ifdef ONE_COMPOUND_PER_FACTION
			{
				Grid::LCellIdI	munitionStoreAt = RandomCell(random);
				munitionStoreAt.height = Objects::FindApplicableGround(munitionStoreAt,true);
				while (munitionStoreAt.height == Objects::badHeight)
				{
					munitionStoreAt = RandomCell(random);
					munitionStoreAt.height = Objects::FindApplicableGround(munitionStoreAt,true);
				}
				Objects::PMunitionStorage storage(CreateStructure<Objects::MunitionStorage>(faction1, munitionStoreAt));
				storage->storage = storage->maxStorage;
			}
			#endif
			Objects::RefreshCompoundCoverage(0);

			Grid::LCellIdI	vehicleAt = RandomCellAtRange(random, generatorAt, FloatRange(0.f, turret->horizontalAttackRange(faction0) + turret->explosionRadius(faction0) * 2));	//allow out-of-range
			vehicleAt.height = Objects::FindApplicableGround(vehicleAt,true);
			while (vehicleAt.height == Objects::badHeight)
			{
				vehicleAt = RandomCellAtRange(random, generatorAt, FloatRange(0.f, turret->horizontalAttackRange(faction0) + turret->explosionRadius(faction0) * 2));	//allow out-of-range
				vehicleAt.height = Objects::FindApplicableGround(vehicleAt,true);
			}


			Objects::PVehicle v(new Objects::Vehicle(faction1,vehicleAt,&Objects::Component::hoverbase,turret));
			float t = 0;
			Objects::InsertConstructed(v,t);
			#ifndef ONE_COMPOUND_PER_FACTION
				v->currentStorage.level[v->turret->ammoType] = v->GetMaxStorageCapacity().level[v->turret->ammoType];
			#endif
			faction1->SignalAppearance(generator,t);	//make sure tower can see generator
			//faction1->currentState = Grid::actualWorld;
			bool wouldFire = v->FillAttackQueue();
			if (wouldFire)
			{
				//check target
				for (const auto&task : v->myTasks)
				{
					ASSERT__(task.IsMode(Objects::CursorMode::AttackMode));
					for (const auto&target : task.foci)
					{
						Objects::TurretTrajectory	tr;
						float angle;
						if (tr = v->TraceTrajectory(Objects::TrajectoryEndPoint( target.target ),trajectory,impactAt,impactCell,angle))
						{
							ASSERT__(tr == Objects::TurretTrajectory::ReachesTarget);
							if (Grid::GetGeometricDistance(vehicleAt,generatorAt) > v->GetScanningRange())
								outOfScanning++;

							float distance = Vec::distance(impactAt,Grid::ToWorld(v->location));
							if (distance <= v->GetAttackExplosionRadius())
							{
								//baaaaad
								bool chk = Objects::TurretBase::CheckArea(faction1, target.target, turret->GetExplosionRadius(faction1));
								ASSERT__(!chk);
							}
							ASSERT_GREATER__(distance,v->turret->GetExplosionRadius(v->owner));
						}
					}
				}
			
			}
			else
			{
				//check if there would be a target. less important, if imprecise. might remove it
			
			
			}



			t++;
			faction1->SignalDisappearance(generator,t);
			Objects::Remove(v,Objects::RemovalType::Vanished,t,true);
			Objects::Remove(generator,Objects::RemovalType::Vanished,t,true);


		}
		turret->explosionRadius.constOverride = original;
		
		Objects::FlushAllData();
		Grid::ResetStructureCounters();
	}


	/*static*/ Grid::LCellIdI	AutoTargeting::RandomCell(Random&random)
	{
		return Grid::LCellIdI(
			random.Next(Grid::LowSetup::GridType::CellCount-1),
			random.Next(-Grid::CellStack::HalfRange+1,Grid::CellStack::HalfRange-2)
		);
	}
	/*static*/ Grid::LCellIdI	AutoTargeting::RandomCellAtRange(Random&random,const Grid::LCellIdI&center, const TFloatRange<>&range)
	{
		Grid::LCellIdI	candidate = RandomCell(random);
		while (candidate == center || !range.Contains( Grid::GetGeometricDistance(center,candidate)))
			candidate = RandomCell(random);
		return candidate;
	}


	/*virtual override*/ void	StorageUnits::Run(Random&random)
	{
		Grid::TPlanetConfig	config;
		Objects::FlushAllData();
		Grid::ResetStructureCounters();

		Objects::PFaction faction(new Objects::Faction(TUniqueID::Empty()));
		Objects::Faction::all << faction;

		Grid::LCellIdI	mineAt(0, 1);
		const auto&n = Grid::low.grid.GetCell(mineAt.stackID).neighborhood;
		Grid::LCellIdI	generatorAt(n.neighborID[0], 1),	//up to 5 neighbors are guaranteed. In fact cell 0 will have exactly 5 neighbors
			fuelContainer0At(n.neighborID[1], 1),
			fuelContainer1At(n.neighborID[2], 1),
			mineralContainerAt(n.neighborID[3], 1);



		Objects::PMine mine(CreateStructure<Objects::Mine>(faction, mineAt));
		Objects::PGenerator generator(CreateStructure<Objects::Generator>(faction, generatorAt));
		std::shared_ptr<Objects::MineralContainer> mineralContainer(CreateStructure<Objects::MineralContainer>(faction, mineralContainerAt));
		std::shared_ptr<Objects::FuelContainer> fuelContainer0(CreateStructure<Objects::FuelContainer>(faction, fuelContainer0At));
		std::shared_ptr<Objects::FuelContainer> fuelContainer1(CreateStructure<Objects::FuelContainer>(faction, fuelContainer1At));

		int fuelPerRound = 10,
			mineralsPerRound = 10;

		ASSERT__(mine->IsLinkedToCore());
		mine->naturalResources.fuel = fuelPerRound + generator->FuelCostPerRound;
		mine->naturalResources.minerals = mineralsPerRound;

		Objects::RefreshCompoundCoverage(0);

		ASSERT_EQUAL__(&mine->GetCompound(), &generator->GetCompound());
		ASSERT_EQUAL__(&mine->GetCompound(), &mineralContainer->GetCompound());
		ASSERT_EQUAL__(&mine->GetCompound(), &fuelContainer0->GetCompound());
		ASSERT_EQUAL__(&mine->GetCompound(), &fuelContainer1->GetCompound());
		//#ifndef ONE_COMPOUND_PER_FACTION
		//	ASSERT_EQUAL__(Objects::compounds.Count(), 1);
		//#else
		//	ASSERT_EQUAL__(Objects::compounds.Count(), 2);	//+nexus
		//#endif

		Objects::Compound&net = mine->GetCompound();
		//Objects::compounds[mine->compoundID];

		float t = 0;
		generator->SetEnabled(false, t, false);
		mine->SetEnabled(false, t, false);

		mineralContainer->SetEnabled(true, t, false);
		fuelContainer0->SetEnabled(true, t, false);
		fuelContainer1->SetEnabled(false, t, false);

		net.TickBegin();
		net.TickEnd(0);

		generator->SetEnabled(true, t);
		ASSERT__(generator->IsEnabled());
		ASSERT__(mine->IsEnabled());

		int expectedFuel = 0,
			expectedMinerals = 0;
		for (int i = 0; i < 100; i++)
		{
			net.TickBegin();
			net.TickEnd(0);
			ASSERT__(generator->IsEnabled());
			ASSERT__(mine->IsEnabled());
			expectedMinerals += mineralsPerRound;
			expectedFuel += fuelPerRound;
			ASSERT_EQUAL__(std::min(mineralContainer->maxStorage.level[Objects::Resource::Minerals], expectedMinerals), mineralContainer->storage.level[Objects::Resource::Minerals]);
			if (expectedFuel <= fuelContainer0->maxStorage.level[Objects::Resource::Fuel])
			{
				ASSERT_EQUAL__(expectedFuel, fuelContainer0->storage.level[Objects::Resource::Fuel]);
				ASSERT_EQUAL__(0, fuelContainer1->storage.level[Objects::Resource::Fuel]);
			}
			else
			{
				ASSERT_EQUAL__(fuelContainer0->maxStorage.level[Objects::Resource::Fuel], fuelContainer0->storage.level[Objects::Resource::Fuel]);
				int expected1 = std::min(fuelContainer1->maxStorage.level[Objects::Resource::Fuel], expectedFuel - fuelContainer0->storage.level[Objects::Resource::Fuel]);
				ASSERT_EQUAL__(expected1, fuelContainer1->storage.level[Objects::Resource::Fuel]);
			}
		}
		Objects::FlushAllData();
		Grid::ResetStructureCounters();
	}

	/*static*/ void VehiclePlant::EnqueueRandom(Objects::PVehiclePlant&plant, Random&random)
	{
		Objects::Component::VehicleBase *base = plant->baseComponents[random.NextIndex(plant->baseComponents.Count())];
		while (base->requirements == Objects::Component::VehicleBase::Requirement::Water)
			base = plant->baseComponents[random.NextIndex(plant->baseComponents.Count())];
		Objects::Component::Turret* turret = plant->turretComponents[random.NextIndex(plant->turretComponents.Count())];
		bool rs = !!plant->Enqueue(base, turret);
		ASSERT__(rs);
	}


	/*virtual override*/ void	VehiclePlant::Run(Random&random)
	{
		Grid::TPlanetConfig	config;
		Objects::FlushAllData();
		Grid::ResetStructureCounters();

		Objects::PFaction faction(new Objects::Faction(TUniqueID::Empty()));
		Objects::Faction::all << faction;

		Grid::LCellIdI	mineAt(0, 1);
		const auto&n = Grid::low.grid.GetCell(mineAt.stackID).neighborhood;
		Grid::LCellIdI	generatorAt(n.neighborID[0], 1),	//up to 5 neighbors are guaranteed. In fact cell 0 will have exactly 5 neighbors
			plantAt(n.neighborID[1], 1);



		Objects::PMine mine(CreateStructure<Objects::Mine>(faction, mineAt));
		Objects::PGenerator generator(CreateStructure<Objects::Generator>(faction, generatorAt));
		Objects::PVehiclePlant plant(CreateStructure<Objects::VehiclePlant>(faction, plantAt));

		const auto&n2 = Grid::low.grid.GetCell(plantAt.stackID).neighborhood;
		for (int i = 0; i < n2.numNeighbors; i++)
			if (!Objects::GetStructure(Grid::LCellIdI(n2.neighborID[i], plantAt.height)))
				Grid::actualWorld.Clear(Grid::LCellIdI(n2.neighborID[i], plantAt.height), 0);	//make space for vehicles


		ASSERT__(mine->IsLinkedToCore());
		mine->naturalResources.fuel = generator->FuelCostPerRound;
		mine->naturalResources.minerals = plant->productionPerRound;

		Objects::RefreshCompoundCoverage(0);

		ASSERT_EQUAL__(&mine->GetCompound(), &generator->GetCompound());
		ASSERT_EQUAL__(&mine->GetCompound(), &plant->GetCompound());
		//#ifndef ONE_COMPOUND_PER_FACTION
		//	ASSERT_EQUAL__(Objects::compounds.Count(), 1);
		//#else
		//	ASSERT_EQUAL__(Objects::compounds.Count(), 2);	//+nexus
		//#endif
		Objects::Compound&net = mine->GetCompound();

		float t = 0;
		generator->SetEnabled(false, t, false);
		mine->SetEnabled(false, t, false);
		plant->SetEnabled(false, t, false);



		net.TickBegin();
		net.TickEnd(0);

		generator->SetEnabled(true, t);
		ASSERT__(generator->IsEnabled());
		ASSERT__(mine->IsEnabled());
		ASSERT__(!plant->IsEnabled());

		Buffer0<Objects::Choice> choices;
		bool showArrow, hasArrowCellTarget;
		TVec3<> arrowFrom;
		Grid::LCellIdI arrowFromCell;

		for (int i = 0; i < 100; i++)
		{
			EnqueueRandom(plant,random);
			EnqueueRandom(plant, random);

			Objects::PVehicle v0, v1;

			ASSERT__(plant->IsEnabled());
			std::pair<int,int> progress;
			while (plant->IsBuilding(progress))
			{
				net.TickBegin();
				net.TickEnd(0);
				if (plant->AwaitsUserInteraction())
				{
					//void			GetChoices(const PWorldEntity&entity, Vehicle::mode_t activeCursorMode, BasicBuffer<Grid::LCellIdI>&movementChoices, bool&showArrow, bool&hasArrowCellTarget, TVec3<>&arrowFrom, Grid::LCellIdI&arrowFromCell);

					Objects::GetChoices(plant, Objects::PVehicle(),Objects::CursorMode::Idle, choices, showArrow, hasArrowCellTarget, arrowFrom, arrowFromCell);
					ASSERT__(choices.IsNotEmpty());
					Objects::PVehicle v = plant->GetProduction();
					Grid::LCellIdI choice = choices[random.NextIndex(choices.Count())].location;
					Objects::ProcessChoiceIntersection(plant, choice, Objects::CursorMode::Idle, Objects::PVehicle());
					ASSERT_EQUAL__(v->health, v->GetMaxHealth());
					ASSERT2__(v->location == choice, v->location.ToString(), choice.ToString());
					if (!v0)
						v0 = v;
					else
						v1 = v;
				}
			}
			ASSERT__(!plant->IsEnabled());
			ASSERT__(v0);
			ASSERT__(v1);
			ASSERT__(Objects::GetVehicle(v0->location) == v0);
			ASSERT__(Objects::GetVehicle(v1->location) == v1);
			Objects::Remove(v0,	Objects::RemovalType::ControlledExplosion,t,false);
			Objects::Remove(v1, Objects::RemovalType::ControlledExplosion,t, false);
			ASSERT__(Objects::vehicles.IsEmpty());
			ASSERT__(generator->IsEnabled());
			ASSERT__(mine->IsEnabled());
		}
		Objects::FlushAllData();
		Grid::ResetStructureCounters();
	}

	
	#ifndef NO_SATELLITES
		/*static*/ void SkyPort::EnqueueRandom(Objects::PSkyPort&plant, Random&random)
		{
			bool rs ;
			if (random.getFloat() < 0.8f)
			{
				rs = plant->EnqueueProductionOfVehicle(&Objects::Component::hoverbase,&Objects::Component::terraformer) != nullptr;
			}
			else
				rs = plant->EnqueueProductionOfSatellite(Objects::SkyHub::globalPrototype->GetCTypeName()) != nullptr;

			ASSERT__(rs);
		}


		/*virtual override*/ void	SkyPort::Run(Random&random)
		{
			Grid::TPlanetConfig	config;
			Objects::FlushAllData();
			Grid::ResetStructureCounters();

			Objects::PFaction faction(new Objects::Faction(TUniqueID::Empty()));
			Objects::Faction::all << faction;

			Grid::LCellIdI	mineAt(0, 1);
			const auto&n = Grid::low.grid.GetCell(mineAt.stackID).neighborhood;
			Grid::LCellIdI	generatorAt(n.neighborID[0], 1),	//up to 5 neighbors are guaranteed. In fact cell 0 will have exactly 5 neighbors
				plantAt(n.neighborID[1], 1);



			Objects::PMine mine(CreateStructure<Objects::Mine>(faction, mineAt));
			Objects::PGenerator generator(CreateStructure<Objects::Generator>(faction, generatorAt));
			Objects::PSkyPort plant(CreateStructure<Objects::SkyPort>(faction, plantAt));

			const auto&n2 = Grid::low.grid.GetCell(plantAt.stackID).neighborhood;
			for (int i = 0; i < n2.numNeighbors; i++)
				if (!Objects::GetStructure(Grid::LCellIdI(n2.neighborID[i], plantAt.height)))
					Grid::actualWorld.Clear(Grid::LCellIdI(n2.neighborID[i], plantAt.height), 0);	//make space for vehicles


			ASSERT__(mine->IsLinkedToCore());
			mine->naturalResources.fuel = generator->FuelCostPerRound;
			mine->naturalResources.minerals = plant->productionPerRound;

			Objects::RefreshCompoundCoverage(0);

			ASSERT_EQUAL__(&mine->GetCompound(), &generator->GetCompound());
			ASSERT_EQUAL__(&mine->GetCompound(), &plant->GetCompound());
			//#ifndef ONE_COMPOUND_PER_FACTION
			//	ASSERT_EQUAL__(Objects::compounds.Count(), 1);
			//#else
			//	ASSERT_EQUAL__(Objects::compounds.Count(), 2);	//+nexus
			//#endif
			Objects::Compound&net = mine->GetCompound();

			float t = 0;
			generator->SetEnabled(false, t, false);
			mine->SetEnabled(false, t, false);
			plant->SetEnabled(false, t, false);



			net.TickBegin();
			net.TickEnd(0);
			generator->SetEnabled(true, t);
			ASSERT__(generator->IsEnabled());
			ASSERT__(mine->IsEnabled());
			ASSERT__(!plant->IsEnabled());

			Buffer0<Objects::Choice> choices;
			bool showArrow, hasArrowCellTarget;
			TVec3<> arrowFrom;
			Grid::LCellIdI arrowFromCell;

			count_t twoCount = 0;
			for (int i = 0; i < 100 || twoCount < 20; i++)
			{
				ASSERT__(Objects::satellites.IsEmpty());

				EnqueueRandom(plant,random);
				EnqueueRandom(plant, random);

				Objects::PMobileEntity v0, v1;

				ASSERT__(Objects::satellites.IsEmpty());

				ASSERT__(plant->IsEnabled());
				std::pair<int,int> progress;
				count_t satelliteOffset = 0;
				while (plant->IsBuilding(progress))
				{
					net.TickBegin();
					net.TickEnd(0);

					if (Objects::satellites.Count() > satelliteOffset)
					{
						satelliteOffset = Objects::satellites.Count();
						//satellites are spawned automatically during Tick...()
						ASSERT_LESS_OR_EQUAL__(satelliteOffset,1);

						Objects::PSatellite v = Objects::satellites.last();
						ASSERT_EQUAL__(v->health, v->GetMaxHealth());
						ASSERT2__(v->location.stackID == plant->location.stackID, v->location.ToString(), plant->location.ToString());
						if (!v0)
							v0 = v;
						else
						{
							ASSERT__(!v1);
							v1 = v;
						}
					}


					if (plant->AwaitsUserInteraction())
					{
						//void			GetChoices(const PWorldEntity&entity, Vehicle::mode_t activeCursorMode, BasicBuffer<Grid::LCellIdI>&movementChoices, bool&showArrow, bool&hasArrowCellTarget, TVec3<>&arrowFrom, Grid::LCellIdI&arrowFromCell);

						Objects::GetChoices(plant, Objects::PVehicle(),Objects::CursorMode::Idle, choices, showArrow, hasArrowCellTarget, arrowFrom, arrowFromCell);
						if (choices.IsEmpty())
						{
							Objects::PMobileEntity producing = plant->GetActiveProduction();
							ASSERT1__(producing->IsSatellite(),producing->GetName());	//only satellites should be able to block each other
							plant->AbortConstruction(plant->queue.first().id);
							ASSERT__(v0);	//this should not happen with the first one
							continue;
						}
						//ASSERT__(choices.IsNotEmpty());
						Objects::PMobileEntity v = plant->GetActiveProduction();
						Grid::LCellIdI choice = choices[random.getIndex(choices.Count() - 1)].location;
						Objects::ProcessChoiceIntersection(plant, choice, Objects::CursorMode::Idle, Objects::PVehicle());
						ASSERT_EQUAL__(v->health, v->GetMaxHealth());
						ASSERT2__(v->location == choice, v->location.ToString(), choice.ToString());
						if (!v0)
							v0 = v;
						else
						{
							ASSERT__(!v1);
							v1 = v;
						}
					}
				}
				//ASSERT__(!plant->IsEnabled());	//SkyPort does not switch off
				ASSERT__(v0);
				//ASSERT__(v1);
				ASSERT__(Objects::GetMobileEntity(v0->location) == v0);
				Objects::Remove(v0,	Objects::RemovalType::ControlledExplosion,t,false);
				if (v1)
				{
					twoCount++;
					ASSERT__(Objects::GetMobileEntity(v1->location) == v1);
					Objects::Remove(v1, Objects::RemovalType::ControlledExplosion,t, false);
				}
				ASSERT__(Objects::vehicles.IsEmpty());
				ASSERT__(Objects::satellites.IsEmpty());
				ASSERT__(generator->IsEnabled());
				ASSERT__(mine->IsEnabled());
			}
			Objects::FlushAllData();
			Grid::ResetStructureCounters();
		}
	#endif

	/*virtual override*/ void	HighGridFamilies::Run(Random&random)
	{
		for (int family = 0; family < BaseGrid::NumFamilies; family++)
		{
			index_t offset = Grid::high.grid.GetFamilyOffset(family);
			count_t range = Grid::high.grid.CountFamilyMembers(family);
			for (index_t j = offset; j < offset + range; j++)
			{
				int stored = Grid::high.grid.GetCell(j).family;
				ASSERT_EQUAL2__(family, stored, j, offset);
			}
		}

		for (index_t i = 0; i < Grid::HighSetup::GridType::CellCount; i++)
		{
			int stored = Grid::high.grid.GetCell(i).family;
			index_t offset = Grid::high.grid.GetFamilyOffset(stored);
			count_t range = Grid::high.grid.CountFamilyMembers(stored);
			ASSERT_GREATER_OR_EQUAL__(i, offset);
			ASSERT_LESS2__(i, offset + range, offset, range);
		}

	}


	/*static*/ void		Bin2Str::Check(const CryptographicID&original)
	{
		String asString = original.ToFullString();
		static CryptographicID decoded;
		decoded.Decode(asString);
		ASSERT2__(original == decoded, original.ToFullString(), decoded.ToFullString());
	}

	/*virtual override*/ void	Bin2Str::Run(Random&random)
	{
		try
		{
			CryptographicID id;
			Check(id);
			for (int i = 0; i < 10000; i++)
			{
				id.Generate();
				Check(id);
			}
		}
		catch (const std::exception&e)
		{
			FATAL__(e.what());
		}
	}

}
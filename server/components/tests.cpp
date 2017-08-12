#include <global_root.h>
#include "tests.h"
#include <sharedContainer.h>
#include <globalEvents.h>
#include <objects/structure/storageStructures.h>
#include <objects/structure/mine.h>
#include <objects/structure/vehiclePlant.h>
#include <string/encoding.h>
#include <io/byte_stream.h>

#include "state/worldState.h"


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
		try
		{
			test.Run(random);
			std::cout << "  ...completed. No errors detected after " << timer.GetSecondsSince(t0) << " seconds" << std::endl;
		}
		catch (const Exception&ex)
		{
			std::cout << "  ...FAILED. Failure detected after " << timer.GetSecondsSince(t0) << " seconds:" << std::endl << ex.what()<<std::endl;
		}
		catch (const std::exception&ex)
		{
			std::cout << "  ...FAILED. Unhandled exception caught after " << timer.GetSecondsSince(t0) << " seconds:" << std::endl << ex.what()<<std::endl;
		}
		catch (...)
		{
			std::cout << "  ...FAILED. Unknown exception caught after " << timer.GetSecondsSince(t0) << " seconds:" << std::endl;
		}

	}

	void Test::BeginSection(const String&name)	const
	{
		std::cout << "    ->"<<name<<std::endl;
	}



	
	template <typename Serializable>
	inline void						VerifySerialization(ByteStreamBuffer&testContainer, const Serializable&serializable)
	{
		testContainer.Clear();
		
		using DeltaWorks::Serialization::SerialSync;
		SerialSync(testContainer,serializable);
		

		

		Serializable copy;
		SerialSync(ByteReadStream(testContainer.GetWrittenData()),copy);
		if (serializable != copy)
			throw Except::Memory::SerializationFault(CLOCATION,"Serialization/deserialization of type "+String(typeid(serializable).name())+" failed. Deserialized data does not match original data");
	}

	static String RandomString(Random&random, size_t minLength, size_t maxLength)
	{
		const count_t length = random.Next(minLength,maxLength);
		String rs = TStringLength(length);
		for (index_t i = 0; i < length; i++)
			rs.set(i,(char)(BYTE)random.Next(1,255));	//full spectrum, no zeroes
		return rs;
	}



	static char32_t		RandomUTF32NoZero(Random&random)
	{
		for (;;)
		{
			char32_t rs = (char32_t)(UINT32)random.Next(1,0x10FFFF);	//full spectrum, no zeroes
			if (StringEncoding::IsValidUnicode(rs))
				return rs;
		}
	}

	/*virtual override*/ void CharacterEncoding::Run(Random&random)
	{
		for (index_t i = 0; i <= 0x10FFFF; i++)
		{
			StringEncoding::UTF8::TChar ch8;
			StringEncoding::UTF16::TChar ch16;
			const char32_t testChar = (char32_t)i;

			StringEncoding::UTF32::ToUTF8(testChar,ch8);
			bool haveUTF16 = false;
			if (StringEncoding::IsValidUnicode(testChar))
			{
				StringEncoding::UTF32::ToUTF16(testChar,ch16);
				haveUTF16 = true;
			}
			StringEncoding::AssertValidity(ch8);
			ASSERT__(StringEncoding::IsValid(ch8));		//if these trigger then AssertValidity() implements a different check than IsValid()
			if (haveUTF16)
			{
				StringEncoding::AssertValidity(ch16);
				ASSERT__(StringEncoding::IsValid(ch16));	//if these trigger then AssertValidity() implements a different check than IsValid()
			}

			ASSERT_EQUAL__(StringEncoding::UTF32::ToUTF8Length(testChar),ch8.numCharsUsed);

			char32_t reverse = StringEncoding::UTF8::ToUTF32(ch8);
			ASSERT_EQUAL__(reverse,testChar);
			if (haveUTF16)
			{
				reverse = StringEncoding::UTF16::ToUTF32(ch16);
				ASSERT_EQUAL__(reverse,testChar);
			}

			if (i < 0x100)
			{
				const char cp1252C = (char)(BYTE)i;
				char32_t utf322 = StringEncoding::CP1252::ToUTF32(cp1252C);
				char cp1252C2 = StringEncoding::UTF32::ToCP1252(utf322);
				ASSERT_EQUAL__(cp1252C,cp1252C2);
			}
		}
		//okay, so the whole range works. let's test some strings

		for (index_t i = 0; i < 1000; i++)
		{
			String windowsString = RandomString(random,1,1000);

			String backConverted;
			String utf8String;
			UTF32String utf32String;
			StringEncoding::CP1252::ToUTF8(windowsString,utf8String);
			StringEncoding::UTF8::ToCP1252(utf8String,backConverted);
			ASSERT_EQUAL__(backConverted,windowsString);

			StringEncoding::CP1252::ToUTF32(windowsString,utf32String);
			StringEncoding::UTF32::ToCP1252(utf32String,backConverted);
			ASSERT_EQUAL__(backConverted,windowsString);
		}


		for (index_t i = 0; i < 1000; i++)
		{
			const count_t length = random.Next(1,1000);
			UTF32String utf32String = TStringLength(length);
			for (index_t i = 0; i < length; i++)
				utf32String.Set(i,RandomUTF32NoZero(random));

			UTF32String backConverted;
			String utf8String;
			UTF16String utf16String;

			StringEncoding::UTF32::ToUTF8(utf32String,utf8String);
			StringEncoding::UTF32::ToUTF16(utf32String,utf16String);
			StringEncoding::UTF8::ToUTF32(utf8String,backConverted);
			ASSERT__(backConverted==utf32String);
			StringEncoding::UTF16::ToUTF32(utf16String,backConverted);
			ASSERT__(backConverted==utf32String);
		}

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
			BeginSection("Iteration "+String(i)+"/"+String(NumIterations));
			quit = false;
			USHORT port;
			ASSERT__(server.StartService(0,true,&port));

			ASSERT__(client.Connect("localhost:"+String(port)));

			messageChannel.SendTo(client,"hello");

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


	/*virtual override*/ void	StringMethods::Run(Random&random)
	{
		String test0 = "hello WORLD helloWORLD HELLO world";

		//contains:
		if (!test0.Contains("hello"))
			Fail(CLOCATION,"String lookup failure");
		if (!test0.Contains("world"))
			Fail(CLOCATION,"String lookup failure");
		if (test0.Contains("World"))
			Fail(CLOCATION,"String lookup failure");
		if (!test0.ContainsIgnoreCase("World"))
			Fail(CLOCATION,"String lookup failure");
		

		ASSERT_EQUAL__(test0.Find("WORLD"),6);
		ASSERT_EQUAL__(test0.Find("WORLD",6),6);
		ASSERT_EQUAL__(test0.Find("WORLD",7),17);
		ASSERT_EQUAL__(test0.Find("WORLD",17),17);
		ASSERT_EQUAL__(test0.Find("WORLD",18),InvalidIndex);
		ASSERT_EQUAL__(test0.FindIgnoreCase("WORLD",18),29);

		ASSERT_EQUAL__(test0.FindLast("WORLD"),17);
		ASSERT_EQUAL__(test0.FindLast("WORLD",17),17);
		ASSERT_EQUAL__(test0.FindLast("WORLD",16),6);

		ASSERT_EQUAL__(test0.FindWord("hello"),0);
		ASSERT_EQUAL__(test0.FindWord("hello",1),InvalidIndex);
		ASSERT_EQUAL__(test0.FindWordIgnoreCase("hello",1),23);

		ASSERT_EQUAL__(test0.FindLastWord("HELLO"),23);
		ASSERT_EQUAL__(test0.FindLastWord("HELLO",23),23);
		ASSERT_EQUAL__(test0.FindLastWord("HELLO",24),23);
		ASSERT_EQUAL__(test0.FindLastWord("HELLO",22),InvalidIndex);
		ASSERT_EQUAL__(test0.FindLastWordIgnoreCase("world",22),6);
		ASSERT_EQUAL__(test0.FindLastWordIgnoreCase("world",6),6);
		ASSERT_EQUAL__(test0.FindLastWordIgnoreCase("world",5),InvalidIndex);



		{
			count_t cnt = 0;
			index_t at = 0;
			while (at < test0.GetLength())
			{
				at = test0.Find("hello",at);
				if (at == InvalidIndex)
					break;
				at++;
				cnt++;
			}
			if (cnt != 2)
				Fail(CLOCATION,"Expected 2 occurrences. Found "+String(cnt));
		}
		{
			count_t cnt = 0;
			index_t at = 0;
			while (at < test0.GetLength())
			{
				at = test0.FindIgnoreCase("hello",at);
				if (at == InvalidIndex)
					break;
				at++;
				cnt++;
			}
			if (cnt != 3)
				Fail(CLOCATION,"Expected 3 occurrences. Found "+String(cnt));
		}
		{
			count_t cnt = 0;
			index_t at = 0;
			while (at < test0.GetLength())
			{
				at = test0.FindWord("hello",at);
				if (at == InvalidIndex)
					break;
				at++;
				cnt++;
			}
			if (cnt != 1)
				Fail(CLOCATION,"Expected 1 occurrences. Found "+String(cnt));
		}
		{
			count_t cnt = 0;
			index_t at = 0;
			while (at < test0.GetLength())
			{
				at = test0.FindWordIgnoreCase("hello",at);
				if (at == InvalidIndex)
					break;
				at++;
				cnt++;
			}
			if (cnt != 2)
				Fail(CLOCATION,"Expected 2 occurrences. Found "+String(cnt));
		}



		{
			count_t cnt = 0;
			index_t at = InvalidIndex;
			for (;;)
			{
				at = test0.FindLastWordIgnoreCase("world",at);
				if (at == InvalidIndex)
					break;
				at--;
				cnt++;
			}
			if (cnt != 2)
				Fail(CLOCATION,"Expected 2 occurrences. Found "+String(cnt));
		}





	}





	/*virtual override*/ void	Journal::Run(Random&random)
	{
		Grid::TPlanetConfig	config;

		GlobalEvents::FlushObjects();
		config.extendedCore.heightScale = 0.f;
		config.extendedCore.baseHeight = -Grid::CellStack::HalfRange;
		config.liquidHeight = -Grid::CellStack::HalfRange;
		config.mountains.baseHeight = 1;
		config.mountains.heightScale = 1.5f;	//cause turbulence
		config.RandomizeSeeds(random);
		config.BuildWorld(World::actual);
		World::actual.ExportDiscovered(World::initial,Grid::WorldDiscovery::FullTerrain);
	}


	void			Serialization::TestRange(serial_size64_t begin, serial_size64_t end, bool expectException)
	{
		
		for (serial_size64_t t = begin; t < end; t++)
		{
			try
			{
				BYTE storage[8];

				MemWriteStream write(storage,sizeof(storage));
				MemReadStream read(storage,sizeof(storage));

				write.WriteSize(t);
				const size_t writtenBytes = write.GetCurrent() - storage;
				const serial_size_t shouldHaveWritten = SerialSizeScanner::GetSerialSizeOfSize(t);
				ASSERT_EQUAL__(writtenBytes,shouldHaveWritten);

				serial_size64_t t2;
				read.ReadSize(t2);
				const size_t readBytes = read.GetCurrent() - storage;
				ASSERT_EQUAL__(readBytes,writtenBytes);
				ASSERT_EQUAL__(t,t2);
			}
			catch (Except::Memory::SerializationFault&ex)
			{
				if (expectException)
					return;	//all good, but no point continuing
				FATAL__("Unexpected serialization fault while handling serialSize="+String(t)+": "+ex.what());
			}
			catch (const std::exception&ex)
			{
				FATAL__("Unexpected other exception while handling serialSize="+String(t)+": "+ex.what());
			}
		}
		if (expectException)
			FATAL__("Expected exception in range ["+String(begin)+","+String(end)+")");
	}


	/*static*/ void		Serialization::RandomFill(Objects::Marker&m, Random&random)
	{
		m.id.Generate();
		m.createdInRound = random();
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
	}


	/*static*/ void		Serialization::RandomFill(Objects::Statistics::MetricTable&table, Random&random)
	{
		count_t numMetrics = random.Next(0,16);
		table.Clear();
		for (index_t i = 0; i < numMetrics; i++)
			table.Set((Objects::Statistics::Metric)i,random.Next(0,1<<31));
	}

	/*static*/ void		Serialization::RandomFill(Objects::Statistics::EntityType&stat,Random&random)
	{
		stat.typeName = RandomString(random,5,16);
		RandomFill(stat.metrics,random);
	}

	/*static*/ void		Serialization::RandomFill(Objects::PositiveResourceVector&vec, Random&random)
	{
		for (index_t i = 0; i < Objects::ResourceVector::NumEntries; i++)
			if (random.NextBool())
				vec.level[i] = random.Next(1,1024);
	}


	/*static*/ void		Serialization::RandomFill(Objects::Statistics::Round&stat,Random&random)
	{
		RandomFill(stat.consumption,random);
		RandomFill(stat.production,random);
		RandomFill(stat.metrics,random);
		stat.mainRound = random.Next(0,1000);
	}




	/*virtual override*/ void	Serialization::Run(Random&random)
	{
		ApplicationInterface::globalVersionOfCurrentFile = ApplicationInterface::SupportedFileVersion;


		BeginSection("Serial Size");
		/*
		if (size <= 0x3F)
			return 1;
		if (size <= 0x3FFF)
			return 2;
		if (size <= 0x3FFFFF)
			return 3;
		if (size < (1U<<29))
			return 4;
		return 8;
		*/

		TestRange(0,0x5000,false);
		TestRange(0x3FFFF0,0x40000F,false);
		TestRange((1U<<29) - 16,(1U<<29) + 16,false);
		TestRange((1ULL<<61) - 16,(1ULL<<61) + 16,true);	//this should throw an exception

		for (index_t i = 0; i < 1000; i++)
		{
			const serial_size64_t begin = random.NextT<serial_size64_t>(0,(1ULL<<61)-16);
			TestRange(begin,begin+16,false);
		}
		ByteStreamBuffer stream;
		BeginSection("Statistics::EntityType");

		for (index_t i = 0; i < 3000; i++)
		{
			Objects::Statistics::EntityType stat;
			RandomFill(stat,random);

			VerifySerialization(stream,stat);
		}


		BeginSection("Markers");
		for (index_t i = 0; i < 3000; i++)
		{
			Objects::Marker m;
			RandomFill(m,random);

			VerifySerialization(stream,m);
		}

		BeginSection("Grid Cells");
		{
			VerifySerialization(stream,Grid::LCellIdI(Grid::LCellIdI::Invalid));
			for (int i = 0; i < 10000; i++)
			{
				Grid::LCellIdI coords;
				coords.stackID = random.Next(Grid::LowSetup::GridType::CellCount-1);
				coords.height = random.Next(-Grid::CellStack::HalfRange, Grid::CellStack::HalfRange);
				VerifySerialization(stream,coords);


				Objects::PositiveResourceVector vec;
				for (index_t i = 0; i < Objects::PositiveResourceVector::NumEntries; i++)
				{
					if (random.NextBool())
						vec.level[i] = 0;
					else
						vec.level[i] = Math::cubic(random.Next(100));
				}

				VerifySerialization(stream,vec);

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
		GlobalEvents::FlushObjects();
		config.extendedCore.heightScale = 0.f;
		config.extendedCore.baseHeight = -Grid::CellStack::HalfRange;
		config.liquidHeight = -Grid::CellStack::HalfRange;
		config.mountains.baseHeight = 1;
		config.mountains.heightScale = 1.5f;	//cause turbulence
		config.RandomizeSeeds(random);
		config.BuildWorld(World::actual);
		World::actual.ExportDiscovered(World::initial,Grid::WorldDiscovery::FullTerrain);



		Objects::PFaction faction0(new Objects::Faction(TUniqueID::Empty())),
							faction1(new Objects::Faction(TUniqueID::Empty()));

		Objects::Faction::all << faction0 << faction1;

		//Objects::PTower prototype = To<Objects::Tower>::DynamicCast( Objects::GetPrototypeByName(ConstName::Type_ArtilleryTower));

		Ctr::Buffer0<TVec4<> > trajectory;
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

			Grid::LCellIdI	vehicleAt = RandomCellAtRange(random, generatorAt, FloatRange(0.f, turret->GetHorizontalAttackRange(faction0) + turret->GetExplosionRadius(faction0) * 2));	//allow out-of-range
			vehicleAt.height = Objects::FindApplicableGround(vehicleAt,true);
			while (vehicleAt.height == Objects::badHeight)
			{
				vehicleAt = RandomCellAtRange(random, generatorAt, FloatRange(0.f, turret->GetHorizontalAttackRange(faction0) + turret->GetExplosionRadius(faction0) * 2));	//allow out-of-range
				vehicleAt.height = Objects::FindApplicableGround(vehicleAt,true);
			}


			Objects::PVehicle v(new Objects::Vehicle(faction1,vehicleAt,&Objects::Component::hoverbase,turret));
			float t = 0;
			Objects::InsertConstructed(v,t);
			#ifndef ONE_COMPOUND_PER_FACTION
				v->currentStorage.level[v->turret->ammoType] = v->GetMaxStorageCapacity().level[v->turret->ammoType];
			#endif
			faction1->SignalAppearance(generator,t);	//make sure tower can see generator
			//faction1->knownWorld = World::actual;
			bool wouldFire = v->FrontQueuePossibleAttacks();
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
		
		GlobalEvents::FlushObjects();
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
		GlobalEvents::FlushObjects();

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
		GlobalEvents::FlushObjects();
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
		GlobalEvents::FlushObjects();

		Objects::PFaction faction  = Objects::Faction::Create(TUniqueID::Empty(),"Test:VehiclePlant");

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
				World::actual.Clear(Grid::LCellIdI(n2.neighborID[i], plantAt.height), 0);	//make space for vehicles


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

		Ctr::Buffer0<Objects::Choice> choices;
		Objects::TChoiceVisualization vis;

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

					Objects::GetChoices(plant, Objects::PVehicle(),Objects::CursorMode::Idle, choices, vis);
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
		GlobalEvents::FlushObjects();
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
					World::actual.Clear(Grid::LCellIdI(n2.neighborID[i], plantAt.height), 0);	//make space for vehicles


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


	struct TTestElement
	{
		count_t touched = 0;
	};

	typedef std::shared_ptr<TTestElement>	PTestElement;


	/*virtual override*/ void	SharedContainer::Run(Random&random)
	{
		::SharedContainer<TTestElement>	container;

		for (index_t run = 0; run < 1000; run++)
		{
			for (index_t i = 0; i < 100; i++)
				container << PTestElement(new TTestElement());

		
			container.Foreach([&container,&random](const PTestElement&e)
			{
				e->touched++;

				while (random.NextFloat()>0.2f)
				{
					container.Erase(random.NextIndex(container.Count()));
				}
			});
			container.Foreach([run](const PTestElement&e)
			{
				ASSERT_EQUAL1__(e->touched,1,run);
			});

			container.Clear();
		}
	}


	void	VisitHighCells(const Grid::LCellIdI&cell, const std::function<void(const Grid::HCellIdI&, bool)>&f)
	{
		#ifdef HYBRID_GRID
			const UINT32 fine = Grid::low.grid.GetCell(cell.stackID).higherEquivalent;
			const auto&n = Grid::high.grid.GetCell(fine).neighborhood;
			f(Grid::HCellIdI(fine, cell.height), true);
			for (BYTE k = 0; k < n.numNeighbors; k++)
			{
				f(Grid::HCellIdI(n.neighborID[k], cell.height), false);
			}
		#else
			f(cell,true);
		#endif

	}

	/*virtual override*/ void	TerrainUpdates::Run(Random&random)
	{
		GlobalEvents::FlushAll();



		for (index_t run = 0; run < 10; run++)
		{
			World::actual.FlushAllChanges();
			{
				World::Data::ChangeSequence seq = World::actual.BeginChangeSequence(0);
				for (UINT32 i = 0; i < Grid::HighSetup::GridType::CellCount; i++)
				{
					auto acc = seq.AccessStack(i);
					Grid::CellStack&stack = acc.GetStack();
					for (unsigned k = 0; k < Grid::CellStack::Range; k++)
						stack.volumeCell[k].content = Grid::Content::Granite;	//fill up with granite
				}
			}

			World::actual.FlushAllChanges();
			World::initial = World::actual;

			Objects::PFaction faction = Objects::Faction::Create(TUniqueID::Empty(), "Test:Terrain");	//makes World::initial copy
			ASSERT__(!faction->knownWorld.SensoryIsInvalid());

			const auto strucLoc = Grid::LCellIdI(random.Next(Grid::LowSetup::GridType::CellCount - 1), 0);

			VisitHighCells(strucLoc, [faction](const Grid::HCellIdI&loc, bool isCenter)
			{
				const auto&cell = World::actual.GetCell(loc);
				const auto&myCell = faction->knownWorld.GetCell(loc);
				ASSERT__(!myCell.IsCurrentlyVisible());	//no structure => no visibility
				ASSERT_EQUAL__(World::actual.GetCell(loc + 1).structureCounter, 0);
				ASSERT_EQUAL__(cell.structureCounter, 0);
				ASSERT_EQUAL__(World::actual.GetCell(loc - 1).structureCounter, 0);
				ASSERT_EQUAL__(World::actual.GetCell(loc - 2).structureCounter, 0);
				ASSERT_EQUAL__(cell .content, Grid::Content::Granite);
			});
			Objects::PVehiclePlant s(CreateStructure<Objects::VehiclePlant>(faction, strucLoc));
			float sensorRange = s->GetScanningRange();
			ASSERT__(!faction->knownWorld.SensoryIsInvalid());

			VisitHighCells(s->location,[faction](const Grid::HCellIdI&loc, bool isCenter)
			{
				const auto&cell = World::actual.GetCell(loc);
				const auto&myCell = faction->knownWorld.GetCell(loc);
				ASSERT__(myCell.IsCurrentlyVisible());
				ASSERT__(faction->knownWorld.GetCell(loc + 1).IsCurrentlyVisible());
				ASSERT__(faction->knownWorld.GetCell(loc-1).IsCurrentlyVisible());

				ASSERT_EQUAL__(World::actual.GetCell(loc + 1).structureCounter, 0);
				ASSERT_EQUAL__(cell.structureCounter, isCenter ? 2 : 1);
				ASSERT_EQUAL__(World::actual.GetCell(loc - 1).structureCounter, isCenter ? 2 : 1);
				ASSERT_EQUAL__(World::actual.GetCell(loc - 2).structureCounter, 0);
				ASSERT_EQUAL__(cell.content, Grid::Content::Clear);
				ASSERT_EQUAL__(faction->knownWorld.GetCell(loc).content, Grid::Content::Granite);	//not updated yet
				const BaseGrid::family_t fam = Grid::high.grid.GetCell(loc.stackID).family;
				ASSERT__(World::actual.FamilyIsOutdated(fam));
				ASSERT__(faction->knownWorld.FamilyIsOutdated(fam));
			});

			World::terrainChanged = false;
		
			World::Network::CheckAllFactionCoverageAndSendAsNeeded();

			ASSERT__(World::terrainChanged);

			VisitHighCells(s->location, [faction](const Grid::HCellIdI&loc, bool isCenter)
			{
				ASSERT_EQUAL__(faction->knownWorld.GetCell(loc).content, Grid::Content::Clear);	//updated
				const BaseGrid::family_t fam = Grid::high.grid.GetCell(loc.stackID).family;
				ASSERT__(!faction->knownWorld.FamilyIsOutdated(fam));	//should have been handled by CheckAllFactionCoverageAndSendAsNeeded()
			});
			GlobalEvents::FlushObjects();
			DBG_ASSERT__(Objects::structures.IsEmpty());
			DBG_ASSERT__(Objects::Faction::all.IsEmpty());
		}


		GlobalEvents::FlushAll();
	}

	

}
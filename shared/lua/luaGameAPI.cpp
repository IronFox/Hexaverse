#include <global_root.h>
#include "luaGameAPI.h"
#include "logEvent.h"
#include "grid/stack.h"
#include "grid/resources.h"
#include "objects.h"
#include "../objects/faction.h"
#include "../runtimeConfig.h"

namespace Lua
{
	namespace Detail
	{
		void LogLua(const String&msg, const String&programName)
		{
			LogEvent(EventClass::Preset::important,programName+"(Lua): "+msg);
		}
		

		index_t	LuaCellToStack( const Lua::Table&cellTable, const String&parameterName, const int cellCount )
		{
			if (cellTable.IsEmpty())
				throw Lua::Exception(parameterName + " is nil.");
			auto luaIndex = cellTable.GetField("index");
			if (luaIndex.IsEmpty())
				throw Lua::Exception(parameterName + " has no entry 'index'.");

			const int stackID = luaIndex.GetAs<int>();

			if (stackID < 1 || stackID > cellCount)
				throw Lua::Exception(parameterName + " is invalid cell location (#"+String(stackID)+" not in [1,"+String(cellCount)+"]).");

			return index_t(stackID-1);
		}

		Grid::LCellIdI CellHToLGridCell( const Lua::Table&luaCell, int h, const String&parameterName)
		{
			return Grid::LCellIdI(LuaCellToStack(luaCell,parameterName,Grid::LowSetup::GridType::CellCount),h);

		}

		Grid::LCellIdI ToLGridCell( const Lua::Table&loc, const String&parameterName )
		{
			if (loc.IsEmpty())
				throw Lua::Exception(parameterName+" is nil");
			Lua::Table luaHeight = loc.GetField("height");
			if (!luaHeight)
				throw Lua::Exception(parameterName+".height is nil");
			const int h = luaHeight.GetAs<int>();
			if (h < Grid::CellStack::BottomHeight)
				throw Lua::Exception(parameterName+".height ("+String(h)+") falls below minimum height ("+String(Grid::CellStack::BottomHeight)+")");
			//_might_ test top limit here, but not for now

			return CellHToLGridCell(loc.GetField("cell"),h,parameterName+".cell");
		}

		void UpdateResourceTable(Lua::Table&t,const Objects::ResourceVector&v)
		{
			for (index_t i = 0; i < Objects::Resource::N; i++)
			{
				String name = Objects::Resource::Reinterpret(i).ToString();
				name.Set(0,CharFunctions::tolower(name.GetFirstChar()));
				t.SetField(name,v.level[i]);
			}
		}

		bool NaturalResourcesSurveyedAt(Lua::Table cellTable)
		{
			const auto stackID = LuaCellToStack(cellTable,"NaturalResourcesSurveyedAt(): cell (parameter #1)",Grid::LowSetup::GridType::CellCount);
			#ifdef CLIENT
				return Grid::ResourceDiscovery::client.resourcesDiscovered[stackID];
			#else
				return true;
			#endif
		}
		bool NoNaturalResourcesHere(Lua::Table cellTable)
		{
			const auto stackID = LuaCellToStack(cellTable,"GetResourceCoverage(): cell (parameter #2)",Grid::LowSetup::GridType::CellCount);
			#ifdef CLIENT
				if (!Grid::ResourceDiscovery::client.resourcesDiscovered[stackID])
					return false;
			#endif
			foreach (Grid::Resource::all,r)
			{
				if (r->coverage[stackID] != 0)
					return false;
			}
			return true;
		}


		int GetNaturalResourceAt(const String&resourceName, Lua::Table cellTable)
		{
			const auto stackID = LuaCellToStack(cellTable,"GetNaturalResourceAt(): cell (parameter #2)",Grid::LowSetup::GridType::CellCount);
			foreach (Grid::Resource::all,r)
				if (r->GetName().EqualsIgnoreCase(resourceName))
				{
					#ifdef CLIENT
						if (!Grid::ResourceDiscovery::client.resourcesDiscovered[stackID])
							return -1;
					#endif
					return r->coverage[stackID] * Objects::NaturalResourceVector::TranslationFactor;
				}
			throw Lua::Exception("GetNaturalResourceAt(): Resource '"+resourceName+"' does not exist.");
		}

		Lua::Table GetNaturalResourcesAt(Lua::Table cellTable)
		{
			const auto stackID = LuaCellToStack(cellTable,"GetNaturalResourcesAt(): cell (parameter #1)",Grid::LowSetup::GridType::CellCount);
			Objects::NaturalResourceVector vec;
			if (!vec.SampleWorld(stackID) || vec.IsEmpty())
				return Lua::nil;
			Lua::Table rs = cellTable.GetProgram()->CreateTable();
			UpdateResourceTable(rs,vec);
			return rs;
		}




		float GetDistance(const Lua::Table&locA, const Lua::Table&locB)
		{
			if (locA == locB)
				return 0;
			auto loc0 = ToLGridCell(locA,"GetDistance(): parameter #1");
			auto loc1 = ToLGridCell(locB,"GetDistance(): parameter #2");
			return Grid::GetGeometricDistance(loc0,loc1);
		}


		Lua::Reference ToTable(const Lua::PProgram&p, const M::TVec3<>&v)
		{
			Lua::Table c = p->CreateTable();

			c.SetField("x" ,v.x);
			c.SetField("y" ,v.y);
			c.SetField("z" ,v.z);

			return c;
		}

	
	}


	Lua::Table API::ToTable(const Grid::LCellIdI&cell)
	{
		Lua::Table*t = locationMap.QueryPointer(cell);
		if (t)
			return *t;
		Lua::Table c = program->CreateTable();

		c.SetField("height" ,cell.height);
		c.SetField("cell",lowCellGrid[cell.stackID]);
		c.SetField("world",Detail::ToTable(program,Grid::ToWorld(cell)));

		locationMap.Set(cell,c);

		return c;
	}

	Lua::Table API::ToTable(const Objects::MobileEntity::Task&task, const Objects::PMobileEntity&entity, bool isFirst)
	{
		Lua::Table taskOut = program->CreateTable();
		taskOut.SetField("name",task.name);
		taskOut.SetField("isJob",task.IsJob());
		if (task.IsJob())
		{
			Lua::Table job = taskOut.SetSubTable("job");
			
			if (isFirst && entity->constructionActionCounter > 0 
						&& task.job->resourceCostLeft > 0)
				job.SetField("hasBegun",true);
			else
				job.SetField("hasBegun",false);

			job.SetField("roundsToCompletion", entity->ContinuationsToRounds(task.job->continuationsToJobCompletion,task.job->GetOperationAffinity()));
			job.SetField("continuationsToCompletion",task.job->continuationsToJobCompletion);
			if (task.name.BeginsWithIgnoreCase("construct "))	//kind of a hack
			{
				job.SetField("buildingType", task.name.SubStringRef(10));
			};
		}

		Lua::Table foci = program->CreateTable();
		foreach (task.foci,f)
			foci.AppendElement(ToTable(f->target));
		taskOut.SetField("foci",foci);

		return taskOut;
	}

	Lua::Table API::TurretToTable( const Objects::TurretBase&b, float zOffset, const Objects::Research::Attributes::Table&researchState )
	{

		Lua::Table t = program->CreateTable();
		//b.
		t.SetField("typeName",b.turret->typeName);
		t.SetField("weight",b.turret->weight.GetValue(researchState));
		t.SetField("detectsResources",b.turret->detectsResources);
		t.SetField("canAttack",b.turret->CanAttack());
		if (b.turret->CanAttack())
		{
			t.SetField("trajectoryType",b.turret->trajectoryType.ToString());
			t.SetField("independentAttackAndMotion",b.turret->independentAttackAndMotion);
			t.SetField("ammoType",b.turret->ammoType.ToString());
			t.SetField("ammoDemandPerShot",b.turret->ammoNeedPerShot.GetValue(researchState));
			#ifdef ONE_COMPOUND_PER_FACTION
				const auto&compound = b.ToEntity().owner->compound;
				const int ammo = compound.total.storage.level[b.turret->ammoType];
			#else
				const int ammo = b.ToEntity().currentStorage.level[b.turret->ammoType];
			#endif
			t.SetField("ammoAvailability",ammo);
			t.SetField("possibleAttacks",b.CountPossibleAttacks());
			t.SetField("attackActionLeft",b.GetAttackActionLeft());
			t.SetField("attackInitiative",b.GetAttackInitiative());
			t.SetField("attacksPerRound",b.GetAttacksPerRound());
			t.SetField("flatAttackRange",b.GetFlatAttackRange());
			t.SetField("explosionRadius",b.GetAttackExplosionRadius());
			t.SetField("damageLevel",b.turret->GetDamageLevel(researchState));
			t.SetField("dealsEMPDamage",b.turret->dealsEMPDamage);
			
			const float flatAttackRange = b.GetFlatAttackRange();
			const auto origin = Objects::TrajectoryEndPoint(b.ToEntity().location,zOffset);
			const auto trajectory = b.turret->trajectoryType;
			const auto explosionRadius = b.GetAttackExplosionRadius();
			t.SetFunction("IsInRange",[trajectory,flatAttackRange,origin](const Lua::Table&vector)
			{
				if (vector.IsEmpty())
					return false;
				M::TVec3<>	dest;
				vector.GetField("x",dest.x);
				vector.GetField("y",dest.y);
				vector.GetField("z",dest.z);
				return Objects::Component::Turret::CanHit(trajectory,flatAttackRange,origin,dest);
			});
			t.SetFunction("IsInRangeFrom",[trajectory,flatAttackRange,zOffset](const Lua::Table&loc, const Lua::Table&vector)
			{
				if (vector.IsEmpty())
					return false;
				M::TVec3<>	dest;
				vector.GetField("x",dest.x);
				vector.GetField("y",dest.y);
				vector.GetField("z",dest.z);
				const auto origin = Objects::TrajectoryEndPoint(Detail::ToLGridCell(loc,"IsInRangeFrom(): Location parameter #1"),zOffset);
				return Objects::Component::Turret::CanHit(trajectory,flatAttackRange,origin,dest);
			});

			const Objects::WFaction owner = b.ToEntity().owner;
			t.SetFunction("SearchAttackTarget", [this,trajectory, flatAttackRange, origin, explosionRadius,owner](const Lua::Table&location)->Lua::Table
			{
				Objects::TurretBase::TargetChoice choice;
				const auto dest = Objects::TrajectoryEndPoint(Detail::ToLGridCell(location,"FindTarget(): Target parameter"),0);
				Objects::PFaction myFaction = owner.lock();
				if (!myFaction)
					throw Lua::Exception(CLOCATION,"FindTarget(): Owning faction unknown or expired");
				bool rs = Objects::TurretBase::FindTarget(myFaction, trajectory, flatAttackRange, explosionRadius, origin, dest, choice);
				if (!rs)
					return Lua::Reference();
				return ToTable(choice.cellTarget);
			});
			t.SetFunction("SearchAttackTargetFrom", [this,trajectory, flatAttackRange, explosionRadius,owner, zOffset](const Lua::Table&org,const Lua::Table&location)->Lua::Table
			{
				Objects::TurretBase::TargetChoice choice;
				const auto dest = Objects::TrajectoryEndPoint(Detail::ToLGridCell(location,"FindTargetFrom(): Target parameter #2"),0);
				const auto origin = Objects::TrajectoryEndPoint(Detail::ToLGridCell(org,"FindTargetFrom(): Location parameter #1"),zOffset);
				Objects::PFaction myFaction = owner.lock();
				if (!myFaction)
					throw Lua::Exception(CLOCATION,"FindTarget(): Owning faction unknown or expired");
				bool rs = Objects::TurretBase::FindTarget(myFaction, trajectory, flatAttackRange, explosionRadius, origin, dest, choice);
				if (!rs)
					return Lua::Reference();
				return ToTable(choice.cellTarget);
			});


			t.SetFunction("TraceTrajectory", [this, trajectory, flatAttackRange, origin, owner](const Lua::Table&location)->Lua::Table
			{
				const auto dest = Objects::TrajectoryEndPoint(Detail::ToLGridCell(location, "TraceTrajectory(): Target parameter"), 0);
				return LuaTraceTrajectoryFrom(origin,dest,trajectory,flatAttackRange,owner);
			});
			t.SetFunction("TraceTrajectoryFrom", [this, trajectory, flatAttackRange, zOffset, owner](const Lua::Table&from, const Lua::Table&location)->Lua::Table
			{
				const auto dest = Objects::TrajectoryEndPoint(Detail::ToLGridCell(location, "TraceTrajectoryFrom(): Target parameter #2"), 0);
				const auto src = Objects::TrajectoryEndPoint(Detail::ToLGridCell(from,"TraceTrajectoryFrom(): Origin parameter #1"),zOffset);
				return LuaTraceTrajectoryFrom(src,dest,trajectory,flatAttackRange,owner);
			});
		}

		for (index_t op = 0; op < Objects::Component::OperationCount; op++)
		{
			const auto o = (Objects::Component::operation_t)op;
			const String os = String(Objects::Component::OperationToString(o));
			t.SetField("can"+os,b.turret->CanDo(o));
					
			t.SetField("speedOf"+os,b.GetOperationSpeed(o));
			t.SetField("perRound"+os, 1 << b.GetOperationAccelerationExponent(o));
		}

		return t;
	}


	Lua::Table API::LuaFindHighestAvailableResourceLocation(const String&resourceName)
	{
		return LuaFindHighestAvailableResourceLocationExt(resourceName,false,false);
	}
	Lua::Table API::LuaFindHighestAvailableResourceLocationExt(const String&resourceName, bool checkLinkPath, bool checkMobiles)
	{
		foreach (Grid::Resource::all,r)
			if (r->GetName().EqualsIgnoreCase(resourceName))
			{
				int highestAt = -1;
				Grid::Resource::coverage_t highest = 0;
				for (index_t i = 0; i < Grid::Resource::NumEntries; i++)
					#ifdef CLIENT
					if (Grid::ResourceDiscovery::client.resourcesDiscovered[i])
					#endif
					{
						if (r->coverage[i] > highest)
						{
							auto h = Objects::FindApplicableGround(Grid::LCellIdI(i,0),true,true);
							if (h != Objects::badHeight)
							{
								if (checkLinkPath)
								{
									for (int h2 = Grid::CellStack::BottomHeight; h2 < h; h2++)
									{
										const auto loc = Grid::LCellIdI(i,h2);
										if (Objects::GetStructure(loc))
										{
											h2 = h = Objects::badHeight;
											break;
										}
										if (checkMobiles && Objects::GetMobileEntity(loc))
										{
											h2 = h = Objects::badHeight;
											break;
										}
									}
								}
								if (checkMobiles && Objects::GetMobileEntity(Grid::LCellIdI(i,h)))
									continue;
								if (h != Objects::badHeight)
								{
									highest = r->coverage[i];
									highestAt = (int)i;
								}
							}
						}
					}
				return highestAt >= 0 ? lowCellGrid[highestAt] : Lua::Table{};
			}
		throw Lua::Exception(CLOCATION,"Resource '"+resourceName+"' does not exist.");
	}

	Lua::Table API::LuaTraceTrajectoryFrom( const Objects::TrajectoryEndPoint&origin, const Objects::TrajectoryEndPoint&dest, Objects::TrajectoryType trajectory, float flatAttackRange, const Objects::WFaction&owner )
	{
		#ifndef CLIENT
			auto lockedOwner = owner.lock();
			if (!lockedOwner)
				throw Lua::Exception(CLOCATION, "TraceTrajectory(): Owner expired");
		#endif
		const World::Data&visibleWorld =
			#ifndef CLIENT
				lockedOwner->knownWorld
			#else
				World::actual
			#endif
		;

		auto rs = Objects::Component::Turret::TraceTrajectory(trajectory, flatAttackRange, visibleWorld, World::actual, false, origin, dest);
		if (rs.result == Objects::TraceResult::InvalidTrajectory)
			return Lua::Reference{};
		Lua::Table t = program->CreateTable();
		t.SetField("midFlightImpact", rs.result == Objects::TraceResult::MidFlightImpact);
		t.SetField("distanceToTarget", rs.distanceToTarget);
		t.SetField("impactAt", Detail::ToTable(program, rs.impactAt));
		return t;
	}


	#ifdef CLIENT

		void		API::AddMyMarkers()
		{
			foreach(Objects::Marker::all, m)
				AddMyMarker(*m);
		}

		void		API::AddMyMarker(const Objects::Marker&m)
		{
			Lua::Table t = program->CreateTable();
			myMarkerIDMap.SetField(m.id.ToFullString(), t);
			t.SetField("location",ToTable(m.location));
			t.SetField("type",m.type.ToString());
			t.SetField("confirmed", true);
			t.SetField("age",m.age);
			t.SetField("id",m.id.ToFullString());
			if (m.visualization)
				t.SetField("unitReference",MapEntity(m.visualization,false,false));
			auto id = m.id;
			index_t at = t.GetLength();
			t.SetFunction("Touch",[id,this,at]()
			{
				if (onMarkerTouch)
					onMarkerTouch(id);
				else
					throw Lua::Exception("AddMyMarker(): onMarkerTouch not set");
				Lua::Table t = myMarkerIDMap.GetField(id.ToFullString());
				ASSERT__(t);
				ASSERT_EQUAL__(t.GetField("id").GetAs<String>(),id.ToFullString());
				t.SetField("age",0);
			});
			t.SetFunction("Destroy",[id,this]()
			{
				if (onMarkerDestroy)
					onMarkerDestroy(id);
				else
					throw Lua::Exception("AddMyMarker(): onMarkerDestroy not set");
				myMarkerIDMap.UnsetField(id.ToFullString());
			});
		}	
		void	API::SetSelectedUnit( const Objects::PWorldEntity&e, bool updateIfFound )
		{
			if (e)
				gameTable.SetField("selectedUnit",MapEntity(e,true,updateIfFound));
			else
				gameTable.UnsetField("selectedUnit");
		}

	#endif


	void	API::UpdateResearch()
	{
		researchTopicList.ClearTable();
		#ifdef CLIENT		
			if (myResearchQueue.IsEmpty())
			{
				myResearchQueue = program->CreateTable();
				researchTable.SetField("myQueue",myResearchQueue);
			}
			else
			{
				myResearchQueue.ClearTable();
			}

			const auto&st = Objects::Faction::mine->researchState.topicProgressVector;
			Array<UINT32> q;
			Objects::Faction::mine->researchState.ExportResearchQueue(q);
		#endif

		Array<Lua::Table> res (Objects::Research::globalState.allTopics.Count());
		for (index_t i = 0; i < Objects::Research::globalState.allTopics.Count(); i++)
		{
			auto t = Objects::Research::GetTopic(i);
			if (t->IsGroup())
				continue;
			String key = t->GetFullName();

			Lua::Table outT = program->CreateTable();
			res[i] = outT;
			researchTopicList.SetField(key,outT);

			outT.SetField("fullName",key);
			outT.SetField("name",t->name);
			outT.SetField("max",t->maxProgress);
			outT.SetField("isEndless",t->maxProgress <= 0);
				
			#ifdef CLIENT
				const int prog = st[t->serialIndex];
				outT.SetField("progress",prog);
				outT.SetField("isCompleted",t->ResearchCompleted(st));

				bool isActive = q.IsNotEmpty() && q.First() == i;
				outT.SetField("isActive",isActive);
				outT.SetField("isQueued",q.Contains((UINT32)i));

				
				outT.SetFunction("Dequeue",[this,i]()
				{
					if (onQueueResearch)
						onQueueResearch(i,true);
					else
						throw Lua::Exception(CLOCATION,"Dequeue(): onQueueResearch not set");
				});
				outT.SetFunction("Enqueue",[this,i]()
				{
					if (onQueueResearch)
						onQueueResearch(i,false);
					else
						throw Lua::Exception(CLOCATION,"Enqueue(): onQueueResearch not set");
				});
			#endif

		}


		#ifdef CLIENT
			foreach (q,queued)
				myResearchQueue.AppendElement(res[*queued]);
		#endif

		researchTable.SetField("maxQueueLength",Objects::Research::MaxResearchQueueLength);
	}



	void		API::FlushAllObjects(bool clearCaches/*=false*/)
	{
		onDeployedCallbacks.Clear();
		entityIDMap.ClearTable();
		factionIDMap.ClearTable();
		allianceIDMap.ClearTable();
		entityMap.Clear();
		researchTopicList.ClearTable();
		#ifdef CLIENT
			myEntityIDMap.ClearTable();
			alliedEntityIDMap.ClearTable();
			hostileEntityIDMap.ClearTable();

			myMarkerIDMap.ClearTable();
			myResearchQueue.ClearTable();
			gameTable.UnsetField("selectedUnit");
			gameTable.UnsetField("myFaction");
			myFactionIsSet= false;
		#endif

		entityMappedToTable.Clear();
		factionMappedToTable.Clear();
		allianceMappedToTable.Clear();

		if (clearCaches)
		{
			loadedAllianceTableMap.Clear();
			loadedEntityTableMap.Clear();
			loadedFactionTableMap.Clear();
			sharedHangars.Clear();
		}

		for (index_t i = 0; i < sharedHangars.Count(); i++)
			if (sharedHangars[i].hangar.expired())
				sharedHangars.Erase(i--);
			else
				sharedHangars[i].rebuild = true;

	}


	void API::Create(const String&programNameToCreate)
	{
		ASSERT__(!IsCreated());
		program = Lua::Program::Create(programNameToCreate);
		Setup();
	}

	void API::Create(const Lua::PProgram&p)
	{
		ASSERT__(!IsCreated());
		program = p;
		Setup();
	}

	void	API::Destroy()
	{
		FlushAllObjects(true);
		onDeployedCallbacks.Clear();
		gameTable.ClearReference();
		gridTable.ClearReference();
		terrainTable.ClearReference();
		terrainContentTable.ClearReference();
		researchTable.ClearReference();
		researchTopicList.ClearReference();
		#ifdef CLIENT
			myEntityIDMap.ClearReference();
			myMarkerIDMap.ClearReference();
			myResearchQueue.ClearReference();
		#endif
		lowCellGrid.Clear();
		highCellGrid.Clear();
		locationMap.Clear();
		program.reset();
	}

	void	API::AddObjectAndPathfindingFunctions()
	{
		#ifdef CLIENT
			gameTable.SetFunction("CreateMarker", [this](const Lua::Table&luaEntity, const String&markerType)
			{
				Objects::Marker::Type t;
				if (!t.Decode(markerType.c_str()))
					throw Lua::Exception("CreateMarker(): Type parameter #2 cannot be decoded to a marker type");

				auto e = LuaUnitToEntity(luaEntity,"CreateMarker(): Entity parameter #1");

				const Lua::Table loc = luaEntity.GetField("location");

				Lua::Reference unitReference;
				if (t != Objects::Marker::Type::Visited)
					unitReference = luaEntity;

				/*
				bool matchFound = false;

				myMarkerIDMap.Foreach([&matchFound, &loc, &unitReference, &markerType](const String&key, const Lua::Reference&member)
				{
					if (matchFound)
						return;
					if (member.IsTable())
					{
						Lua::Table t = member;
						if (t.GetField("location") != loc) //reference-equal should be 
							return;
						String type;
						if (!t.GetField("type", type) || type != markerType)
							return;
						Lua::Reference luaEntity;
						if (unitReference.IsSet())
							if (!t.GetField("unitReference", luaEntity) || luaEntity != unitReference)
								return;
						matchFound = true;
					}
					else
						FATAL__("Got non-table for key " + key);

				});
				if (matchFound)
					return;
					*/

				TUniqueID id;
				id.Generate();

				Lua::Table mt = program->CreateTable();
				myMarkerIDMap.SetField(id.ToFullString(), mt);
				mt.SetField("location", loc);
				mt.SetField("type", markerType);
				mt.SetField("confirmed", false);
				mt.SetField("age", 0);
				mt.SetField("id", id.ToFullString());
				if (unitReference.IsSet())
					mt.SetField("unitReference", unitReference);
				mt.SetFunction("Touch", []()
				{
					//do nothing
				});

				if (onMarkerCreate)
					onMarkerCreate(id, t, e, Detail::ToLGridCell(loc, "Entity parameter #1.location"));
				else
					throw Lua::Exception("CreateMarker(): onMarkerCreate() not set");
			});
		#endif

		gameTable.SetFunction("GetMobileAt",[this](const Lua::Table&location)
		{
			auto loc = Detail::ToLGridCell(location,"GetMobileAt(): Location parameter");
			auto entity = Objects::GetMobileEntity(loc);
			if (!entity)
				return Lua::Table{};
			auto ptr = loadedEntityTableMap.QueryPointer(entity->id);
			if (!ptr)
				return Lua::Table{};
			return ptr->first;
		});

		gameTable.SetFunction("GetStructureAt",[this](const Lua::Table&location)
		{
			auto loc = Detail::ToLGridCell(location,"GetStructureAt(): Location parameter");
			auto entity = Objects::GetStructure(loc);
			if (!entity)
				return Lua::Table{};
			auto ptr = loadedEntityTableMap.QueryPointer(entity->id);
			if (!ptr)
				return Lua::Table{};
			return ptr->first;
		});



	}


	void	API::Setup()
	{
		gameTable = program->SetGlobalTable("Game");
			gridTable = gameTable.SetSubTable("Grid");
			terrainTable = gameTable.SetSubTable("Terrain");
				terrainContentTable = terrainTable.SetSubTable("Content");

			researchTable = gameTable.SetSubTable("Research");
				researchTopicList = researchTable.SetSubTable("topics");
		
			entityIDMap = gameTable.SetSubTable("units");

		#ifdef CLIENT
			myEntityIDMap = gameTable.SetSubTable("myUnits");
			alliedEntityIDMap = gameTable.SetSubTable("alliedUnits");
			hostileEntityIDMap = gameTable.SetSubTable("hostileUnits");

			myMarkerIDMap = gameTable.SetSubTable("myMarkers");
		#endif

			factionIDMap = gameTable.SetSubTable("factions");
			allianceIDMap = gameTable.SetSubTable("alliances");
			
			gameTable.SetFunction("Log",[this](const String&msg)
			{
				LogEvent(EventClass::Preset::scriptMessage,program->GetName()+"(Lua): "+msg);
			});

			gameTable.SetFunction("GetRound",[]()
			{
				return RuntimeConfig::roundNumber.ToString();
			});
			gameTable.SetFunction("GetMainRoundNumber",[]()
			{
				return RuntimeConfig::roundNumber.main;
			});
			gameTable.SetFunction("GetPhase",[]()
			{
				return RuntimeConfig::phase.ToString();
			});
			gameTable.SetFunction("CanIssueUnitCommands",[]()
			{
				return RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase;	//not using IsInAnyCommandPhase() here, as unit instructions are valid only during the main command phase
			});


	}

	Lua::Table	API::LuaFindShortestPathFrom(const Objects::PMobileEntity&v, const Grid::LCellIdI&origin, const Grid::LCellIdI&dest, const Lua::Function<int(Lua::Table, int)>&costModifier, bool ignoreOwnedMobilesOnRoute)
	{
		Objects::Pathfinding::FCellCostModifier costMod;
		if (costModifier.IsSet())
			costMod = [&costModifier, this](const Grid::LCellIdI&loc, int&costs)
		{
			costs = costModifier(ToTable(loc), costs);
		};

		Objects::Pathfinding::RoutingFlags flags = Objects::Pathfinding::RoutingFlags::AllowAccessibleHangars | Objects::Pathfinding::RoutingFlags::IgnoreLimit;
		if (ignoreOwnedMobilesOnRoute)
			flags |= Objects::Pathfinding::RoutingFlags::IgnoreOwnMobilesOnRoute;

		auto success = shortestPath.Update(*v, origin, dest,flags,Objects::Component::GroundFlags::None, nullptr, costMod);
		if (success != Objects::Pathfinding::Result::DestinationReached)
			return Lua::nil;

		Lua::Table result = program->CreateTable();
		Lua::Table pred;
		int cost = 0;
		for (index_t i = 0; i < shortestPath.result.Count(); i++)
		{
			const auto&in = shortestPath.result[i];
			Lua::Table luaNode = program->CreateTable();
			luaNode.SetField("location", ToTable(in.cell));
			cost += in.movementCostsFromPrevNode;
			luaNode.SetField("movementCostsFromRoot", cost);
			if (pred.IsSet())
				luaNode.SetField("predecessor", pred);

			result.AppendElement(luaNode);
			pred = luaNode;
		}
		return result;
	};


	Objects::PVehicle	API::ParameterToVehicle(const Lua::Table&entityTable, Grid::LCellIdI&outLoc, const String&parameterName) const
	{
		if (!entityTable)
			throw Lua::Exception(parameterName+" is nil");
		const auto luaID = entityTable.GetField("id");
		if (!luaID)
			throw Lua::Exception(parameterName + ".id is nil");
		const String sid = luaID.GetAs<String>();
		TUniqueID id;
		if (!id.Decode(sid))
			throw Lua::Exception(parameterName + ".id is no valid unique id");
		auto ptr = this->entityMap.QueryPointer(id);
		if (ptr == nullptr)
			throw Lua::Exception(parameterName + ".id is unknown");
		Objects::PWorldEntity e = ptr->lock();
		if (!e)
			throw Lua::Exception(parameterName + " points to an expired entity");
		if (!e->IsVehicle())
			throw Lua::Exception(parameterName + " '" + e->GetName() + "' is not a vehicle");
		outLoc = Detail::ToLGridCell(entityTable.GetField("location"), parameterName+".location");

		return To<Objects::Vehicle>::Cast(e);
	}


	API::TAlliance*	API::AddAlliance( const Objects::PAlliance&a, bool updateIfFound, bool updateMembers )
	{
		if (!a)
			return nullptr;
		Lua::Table t;

		auto found = loadedAllianceTableMap.QueryPointer(a->id);
		if (found)
		{
			if (allianceMappedToTable.SetNew(a->id))
				allianceIDMap.SetField(a->id.ToFullString(),found->table);
			if (!updateIfFound)
				return found;
			t = found->table;
		}
		else
		{
			t = program->CreateTable();
			found = loadedAllianceTableMap.SetNew(a->id);
			found->table = t;
			found->factionTable = program->CreateTable();
			t.SetField("members",found->factionTable);

			allianceMappedToTable.Set(a->id);
			allianceIDMap.SetField(a->id.ToFullString(),t);
		}
		if (updateMembers)
		{
			found->factionTable.ClearTable();
			a->VisitMembers([found,this](const Objects::PFaction&f)
			{
				auto luaFaction = AddFaction(f,false);
				found->factionTable.AppendElement(luaFaction->table);
			},true);
		}
		t.SetField("id",a->id.ToFullString());
		return found;
	}


	void			API::TFactionResourceBalance::AllocateIn(Lua::Table&parent, const String&name)
	{
		auto t = table = parent.SetSubTable(name);
		balanceTable = t.SetSubTable("balance");
		storageTable = t.SetSubTable("storage");
		demandTable = t.SetSubTable("demand");
		supplyTable = t.SetSubTable("supply");
	}

	

	API::TFaction*	API::AddFaction( const Objects::PFaction&f, bool updateIfFound )
	{
		if (!f)
			return nullptr;
		Lua::Table t;

		auto found = loadedFactionTableMap.QueryPointer(f->id);
		if (found)
		{
			if (factionMappedToTable.SetNew(f->id))
			{
				factionIDMap.SetField(f->id.ToFullString(),found->table);
				#ifdef CLIENT
					if (f->isOwnedByMe)
					{
						gameTable.SetField("myFaction",found->table);
						myFactionIsSet= true;
					}
				#endif
			}

			if (!updateIfFound)
				return found;
			t = found->table;
		}
		else
		{
			t = program->CreateTable();
			found = loadedFactionTableMap.SetNew(f->id);
			found->table = t;
			found->entityTable = t.SetSubTable("units");
			found->current.AllocateIn(t,"currentResources");
			found->max.AllocateIn(t,"maxResources");

			factionMappedToTable.Set(f->id);
			factionIDMap.SetField(f->id.ToFullString(),t);

			#ifdef CLIENT
				if (f->isOwnedByMe)
				{
					gameTable.SetField("myFaction",t);
					myFactionIsSet= true;
				}
			#endif
		}


		t.SetField("name",f->name);
		t.SetField("id",f->id.ToFullString());
		const auto*const alliance = AddAlliance(f->alliance,false,true);
		t.SetField("alliance",alliance->table);

		auto vec = f->compound.GetVector();
		Detail::UpdateResourceTable(found->current.balanceTable,vec.balance);
		Detail::UpdateResourceTable(found->current.demandTable,vec.supply - vec.demand);
		Detail::UpdateResourceTable(found->current.storageTable,vec.storage);
		Detail::UpdateResourceTable(found->current.supplyTable,vec.supply);

		vec = f->compound.GetAllEnabledVector();
		Detail::UpdateResourceTable(found->max.balanceTable,vec.balance);
		Detail::UpdateResourceTable(found->max.demandTable,vec.demand);
		Detail::UpdateResourceTable(found->max.supplyTable,vec.supply - vec.demand);
		Detail::UpdateResourceTable(found->max.storageTable,vec.storageMax);

		return found;
	}


	void			API::SignalDeployment(const TUniqueID&deploymentID, const Objects::PWorldEntity&e)
	{
		Lua::Table m = MapEntity(e,true,true);
		LFOnDeploy dep;
		if (onDeployedCallbacks.QueryAndUnset(deploymentID,dep))
			dep(m);
	}


	void			API::TFaction::AddEntity(const Lua::Table&e)
	{
		entityTable.AppendElement(e);
	}

	Objects::PWorldEntity API::LuaUnitToEntity(const Lua::Table&luaEntity, const String&funcParameterName)	const
	{
		auto luaID = luaEntity.GetField("id");
		if (!luaID)
			throw Lua::Exception(funcParameterName+" does not contain an id field");
		TUniqueID entityID;
		if (!entityID.Decode(luaID.GetAs<String>()))
			throw Lua::Exception(funcParameterName+" does not contain a decodeable id field");
		auto ptr = this->entityMap.QueryPointer(entityID);
		if (ptr == nullptr)
			throw Lua::Exception(funcParameterName+".id is unknown");
		Objects::PWorldEntity e = ptr->lock();
		if (!e)
			throw Lua::Exception(funcParameterName+" points to an expired entity");
		return e;
	}


	Lua::Table	API::MapEntity( const Objects::PWorldEntity&e, bool actual, bool updateIfFound )
	{
		auto found = loadedEntityTableMap.QueryPointer(e->id);
		entityMap.Set(e->id,e);
		Lua::Table t;
		bool outIsNew = false;
		bool update = updateIfFound;
		bool setField = false;
		const auto location = e->location;
		if (found)
		{
			t = found->first;
			if (actual && entityMappedToTable.SetNew(e->id))
			{
				setField = true;
			}
			if (!updateIfFound && actual == found->second)
				return t;
		}
		else
		{
			update = true;
			t = program->CreateTable();
			outIsNew = true;
			
			if (actual)
			{
				entityMappedToTable.Set(e->id);
				setField = true;
			}
			loadedEntityTableMap.Set(e->id,std::make_pair(t,actual));
		}

		const String sid = e->id.ToFullString();
		if (setField)
		{
			entityIDMap.SetField(sid,t);
			#ifdef CLIENT
				if (e->isOwnedByMe)
					myEntityIDMap.SetField(sid,t);
				elif (e->owner && e->owner->IsAlliedWithMe())
					alliedEntityIDMap.SetField(sid,t);
				else
					hostileEntityIDMap.SetField(sid,t);
			#endif
		}

		if (!update)
			return t;

		t.SetField("name",e->GetName());
		t.SetField("niceName",e->GetNiceName());
		t.SetField("id",e->id.ToFullString());
		t.SetField("typeName",e->GetTypeName());
		t.SetField("height",e->GetHeight());
		t.SetField("isMobile", e->IsMobile());
		t.SetField("isHangar",e->IsHangar());
		t.SetField("constructionCost",e->GetConstructionCost());
		t.SetField("isStructure",e->IsStructure());
		#ifdef CLIENT
			t.SetField("isOwnedByMe",e->isOwnedByMe);
		#endif
		if (e->owner)
		{
			auto faction = AddFaction(e->owner,false);
			if (outIsNew)
				faction->AddEntity(t);
			t.SetField("owner",faction->table);
		}
		else
			t.UnsetField("owner");
		t.SetField("location",ToTable(e->location));
		t.SetField("isSleeping",e->IsSleeping());
		t.SetField("health",e->health);
		t.SetField("maxHealth",e->GetMaxHealth());
		t.SetField("isIndestructible", e->IsIndestructible());
		t.SetField("empDamageLevel",e->empDamageLevel);
		t.SetField("scanningRange",e->GetScanningRange());

		const TUniqueID id = e->id;

		if (actual)
		{
			Objects::WWorldEntity w= e;
			t.SetFunction("Destroy", [id,this,w]()
			{
				#ifdef CLIENT
					auto e = w.lock();
					if (!e)
						throw Lua::Exception(CLOCATION,"Destroy(): Entity expired");
					if (!e->isOwnedByMe)
						throw Lua::Exception(CLOCATION,"Destroy(): Entity "+e->GetName()+" belongs to some other faction");
				#endif

				if (onRemoveEntity)
				{
					onRemoveEntity(id);
				}
				else
					throw Lua::Exception("Destroy(): onRemoveEntity not set");


				//#ifdef CLIENT
				//	ApplicationInterface::SignalRemoval(e,false);
				//#else
				//	float t= 0;
				//	Objects::Remove(e,Objects::RemovalType::ControlledExplosion,t);
				//#endif

			});

			t.SetFunction("Sleep",[id,this](bool doSleep)
			{
				if (onUpdateSleep)
					onUpdateSleep(id, doSleep);
				else
					throw Lua::Exception(CLOCATION,"Sleep(): onUpdateSleep not set");
			});


			{
				auto h = e->CastToHangar();
				if (h && h->GetLoad() > 0)
				{
					if (h->SharesStorage())
					{
						TSharedHangar*sh = nullptr;
						foreach (sharedHangars,other)
						{
							auto h2 = other->hangar.lock();
							if (h2 && h->SharesStorageWith(h2))
							{
								sh = other;
								break;
							}
						}
						if (!sh)
						{
							sh = &sharedHangars.Append();
							sh->hangar = h;
							sh->rebuild = true;
							sh->sharedDeployables = program->CreateTable();
						}

						t.SetField("storedUnits",sh->sharedDeployables);

						if (sh->rebuild)
						{
							sh->rebuild = false;
							auto&table = sh->sharedDeployables;
							h->VisitStored([this,&table](const Objects::PMobileEntity&m)
							{
								table.AppendElement(MapEntity(m,true,false));
							});
						}
					}
					else
					{
						auto deployableUnits = t.SetSubTable("storedUnits");

						h->VisitStored([this,&deployableUnits](const Objects::PMobileEntity&m)
						{
							deployableUnits.AppendElement(MapEntity(m,true,false));
						});
					}


					Objects::WWorldEntity w = e;
					t.SetFunction("GetStoredDeploymentOptions",[this,w](const Lua::Table&luaEntity)
					{
						auto d = LuaUnitToEntity(luaEntity,"GetDeploymentOptionsFor(): Unit");
						if (!d->IsMobile())
							throw Lua::Exception(CLOCATION,"GetDeploymentOptionsFor(): Unit is not a mobile unit");
						auto m = To<Objects::MobileEntity>::Cast(d);
						auto e = w.lock();
						if (!e)
							throw Lua::Exception(CLOCATION,"GetDeploymentOptionsFor(): Local entity has expired");
						#ifdef CLIENT
							if (!m->isOwnedByMe)
								throw Lua::Exception(CLOCATION,"GetDeploymentOptionsFor(): Access denied to "+m->GetNameAndOwner());
							if (!e->isOwnedByMe)
								throw Lua::Exception(CLOCATION,"GetDeploymentOptionsFor(): Access denied to "+e->GetNameAndOwner());
						#endif
						auto h = e->CastToHangar();
						if (m->storedIn.lock() != h)
						{
							throw Lua::Exception(CLOCATION,"GetDeploymentOptionsFor(): Unit "+m->GetName()+" is not stored in "+e->GetName());
						}
						Buffer0<Objects::Choice> choices;
						Objects::TChoiceVisualization dummy;
						Objects::GetChoices(e,m,Objects::CursorMode::Idle,choices,dummy);
						Lua::Table rs = program->CreateTable();
						foreach (choices,ch)
							rs.AppendElement(ToTable(ch->location));
						return rs;
					});

					t.SetFunction("DeployStored",[this,w](const Lua::Table&luaEntity, const Lua::Table&luaLocation, const Lua::Function<void(const Lua::Table&)>&onDeploy)
					{
						if (!onDeployStored)
							throw Lua::Exception(CLOCATION,"DeployStoredAt(): onDeployStored not set");

						auto d = LuaUnitToEntity(luaEntity,"DeployStoredAt(): Unit parameter #1");
						if (!d->IsMobile())
							throw Lua::Exception(CLOCATION,"DeployStoredAt(): Unit is not a mobile unit");
						auto m = To<Objects::MobileEntity>::Cast(d);
						auto e = w.lock();
						if (!e)
							throw Lua::Exception(CLOCATION,"DeployStoredAt(): Local entity has expired");
						#ifdef CLIENT
							if (!m->isOwnedByMe)
								throw Lua::Exception(CLOCATION,"DeployStoredAt(): Access denied to "+m->GetNameAndOwner());
							if (!e->isOwnedByMe)
								throw Lua::Exception(CLOCATION,"DeployStoredAt(): Access denied to "+e->GetNameAndOwner());
						#endif
						auto h = e->CastToHangar();
						if (m->storedIn.lock() != h)
						{
							throw Lua::Exception(CLOCATION,"DeployStoredAt(): Unit "+m->GetName()+" is not stored in "+e->GetName());
						}

						const auto loc = Detail::ToLGridCell(luaLocation,"DeployStoredAt(): Location parameter #2");
						Buffer0<Objects::Choice> choices;
						Objects::TChoiceVisualization dummy;
						Objects::GetChoices(e,m,Objects::CursorMode::Idle,choices,dummy);
						bool found = false;
						foreach (choices,ch)
							if (ch->location == loc)
							{
								found = true;
								break;
							}

						if (!found)
							throw Lua::Exception(CLOCATION,"DeployStoredAt(): Chosen location "+ToString(loc)+" is not a valid deployment choice of "+m->GetName()+", being deployed from "+e->GetName());

						TUniqueID depID = TUniqueID::Empty();
						if (onDeploy)
						{
							depID.Generate();
							onDeployedCallbacks.Set(depID,onDeploy);
							lastDeploymentCallbackRegistered = timer.Now();
						}
						onDeployStored(e->id,m->id,loc,depID);
					});

				}
				else
				{
					t.UnsetField("storedUnits");
					t.UnsetField("GetStoredDeploymentOptions");
					t.UnsetField("DeployStored");
				}

				t.SetFunction("CanStore",[this,w](const Lua::Table&luaEntity)
				{
					auto d = LuaUnitToEntity(luaEntity,"CanStore(): Unit parameter");
					if (!d->IsMobile())
						return false;
					auto m = To<Objects::MobileEntity>::Cast(d);
					auto e = w.lock();
					if (!e)
						throw Lua::Exception(CLOCATION,"CanStore(): Local entity has expired");
					auto h = e->CastToHangar();
					if (!h)
						throw Lua::Exception(CLOCATION,"CanStore(): Local entity is no hangar");
					return h->CanStoreType(*m);
				});
				t.SetFunction("SharesStorageWith",[this,w](const Lua::Table&luaEntity)
				{
					auto d = LuaUnitToEntity(luaEntity,"CanStore(): Unit parameter");
					auto e = w.lock();
					if (!e)
						throw Lua::Exception(CLOCATION,"CanStore(): Local entity has expired");
					auto h0 = e->CastToHangar();
					if (!h0)
						throw Lua::Exception(CLOCATION,"CanStore(): Local entity is no hangar");
					auto h1 = d->CastToHangar();
					if (!h1)
						return false;
					return h0->SharesStorageWith(h1);
				});
			}
		}
		else
		{
			t.UnsetField("Destroy");
			t.UnsetField("storedUnits");
			t.UnsetField("GetStoredDeploymentOptions");
			t.UnsetField("DeployStored");
			t.UnsetField("CanStore");
			t.UnsetField("SharesStorageWith");
		}

		if (e->IsWorkingStructure())
		{
			const auto ws = To<Objects::WorkingStructure>::Cast(e);
			Objects::ResourceVector balance,storage,storageMax;
			ws->GetResourceVector(balance,storage,storageMax);

			Detail::UpdateResourceTable(t.SetSubTable("resourceBalance"),balance);
			Detail::UpdateResourceTable(t.SetSubTable("resourceStorage"),storage);
			Detail::UpdateResourceTable(t.SetSubTable("maxResourceStorage"),storageMax);

			t.SetField("isWorkingStructure",true);
			t.SetField("isEnabled",ws->IsEnabled());
			if (actual)
				t.SetFunction("Enable", [id,this](bool doEnable)
				{
					if (onEnableWorkingStructure)
						onEnableWorkingStructure(id,doEnable);
					else
						throw Lua::Exception("Enable(): onEnableWorkingStructure not set");
					//#ifdef CLIENT
					//	ApplicationInterface::SignalEnableStructure(*To<Objects::WorkingStructure>::Cast(e),doEnable);
					//#else
					//	float t= 0;
					//	To<Objects::WorkingStructure>::Cast(e)->SetEnabled(doEnable, t);
					//#endif
				});
			else
				t.UnsetField("Enable");
		}
		else
			t.SetField("isWorkingStructure",false);

		auto prod = e->CastToProducer();
		t.SetField("isMobileEntityProducer",prod != nullptr);
		if (prod)
		{
			if (prod->queue.IsNotEmpty())
			{
				Lua::Table queue = program->CreateTable();
				foreach (prod->queue,w)
				{
					Lua::Table production = program->CreateTable();
					production.SetField("name",w->GetName());
					production.SetField("id",w->id.ToFullString());

					TUniqueID tID = w->id;
					production.SetFunction("Abort",[this,id,tID]()
					{
						if (onAbortProduction)
							onAbortProduction(id,tID);
						else
							throw Lua::Exception(CLOCATION,"Abort(): onAbortProduction not set");

					});

					queue.AppendElement(production);
				}
				t.SetField("productionQueue",queue);
				t.SetField("roundsToCompletion",prod->productionRoundsLeft);

			}
			else
			{
				t.UnsetField("productionQueue");
				t.UnsetField("roundsToCompletion");
			}



			if (prod->queue.IsNotEmpty() && prod->productionRoundsLeft == 0)
			{
				ASSERT__(prod->AwaitsUserInteraction());	//let's check that the assumptions match
				static Buffer0<Objects::Choice>	choices;
				static Objects::TChoiceVisualization vis;
				Objects::GetChoices(e,nullptr,Objects::CursorMode::Idle,choices,vis);
				Lua::Table deploymentOptions = t.SetSubTable("deploymentOptions");
				foreach (choices,c)
					deploymentOptions.AppendElement(ToTable(c->location));
				t.SetField("productionCompleted",prod->queue.First().GetName());

				if (actual)
					t.SetFunction("DeployProductionAt",[this,id](const Lua::Table&luaLoc, const LFOnDeploy onDep)
					{
						auto loc = Detail::ToLGridCell(luaLoc,"DeployProductionAt(): Location parameter");

						if (onDeployCompletedProduction)
						{
							TUniqueID depID = TUniqueID::Empty();
							if (onDep)
							{
								depID.Generate();
								onDeployedCallbacks.Set(depID,onDep);
								lastDeploymentCallbackRegistered = timer.Now();
							}
							onDeployCompletedProduction(id,loc,depID);
						}
						else
							throw Lua::Exception(CLOCATION,"DeployProductionAt(): onDeployCompletedProduction not set");
					});
				else
					t.UnsetField("DeployProductionAt");
			}
			else
			{
				ASSERT__(!prod->AwaitsUserInteraction());	//let's check that the assumptions match
				t.UnsetField("productionCompleted");
				t.UnsetField("deploymentOptions");
				t.UnsetField("DeployProductionAt");
			}

			if (actual)
			{
				Objects::WFaction owner = e->owner;
				Objects::WWorldEntity w = e;
				t.SetFunction("BuildMobile",[this,w,owner](const String&type)
				{
					Objects::PWorldEntity e= w.lock();
					if (!e)
						throw Lua::Exception(CLOCATION,"BuildMobile(): Entity has expired");
					auto prod = e->CastToProducer();
					ASSERT_NOT_NULL1__(prod,e->GetName());
					Objects::EntityType t = Objects::DecodeType(type);
					if (t.IsEmpty())
						throw Lua::Exception(CLOCATION,"BuildMobile(): Given type is empty");
					if (onBeginProduction)
						onBeginProduction(e->id,t);
					else
						throw Lua::Exception(CLOCATION,"BuildMobile(): onBeginProduction not set");
				});

				t.SetFunction("PredictRoundsToCompletion",[this,w,owner](const String&type)
				{
					Objects::PWorldEntity e= w.lock();
					if (!e)
						throw Lua::Exception(CLOCATION,"PredictRoundsToCompletion(): Entity has expired");
					auto prod = e->CastToProducer();
					ASSERT_NOT_NULL1__(prod,e->GetName());
					Objects::EntityType t = Objects::DecodeType(type);
					if (t.IsEmpty())
						throw Lua::Exception(CLOCATION,"PredictRoundsToCompletion(): Given type is empty");

					int cost = prod->GetProductionCost(t,e->owner);
					return prod->ProductionRounds(cost,false);
				});
			}
			else
				t.UnsetField("BuildMobile");
		}


		float zOffset;
		const auto base = e->GetTurretBase(zOffset);
		if (base)
		{
			Lua::Table sub = TurretToTable(*base, zOffset, e->GetResearchState());
			sub.SetField("zOffset",zOffset);
			t.SetField("turret",sub);

			if (actual)
			{
				if (base->turret->CanAttack())
				{
					t.SetFunction("Attack",[id,this](const Lua::Table&t)
					{
						Array<Grid::LCellIdI> targets(t.GetLength());
						for (index_t i = 0; i < t.GetLength(); i++)
						{
							Lua::Table element = t.GetElement(i+1);
							if (element.IsEmpty())
								throw Lua::Exception("Attack(): Element #"+String(i+1)+" is nil.");
							Grid::LCellIdI loc = Detail::ToLGridCell(element,"table.#"+String(i+1));
							targets[i] = loc;
						}

						onAttackTargets(id,targets);
					});
				}
			}
			else
				t.UnsetField("Attack");
		}



		if (e->IsMobile())
		{
			Lua::Table tasks = program->CreateTable();
			const auto m = To<Objects::MobileEntity>::Cast(e);
			const Objects::WMobileEntity w = m;

			t.SetField("isAutomated",m->isAutomated);
			t.SetFunction("SetAutomation",[id,this](bool doAutomate)
			{
				if (onUpdateMobileAutomation)
					onUpdateMobileAutomation(id, doAutomate);
				else
					throw Lua::Exception(CLOCATION,"SetAutomation(): onUpdateAutomation not set");
			});


			if (!m->storedIn.expired())
			{
				t.SetField("storedIn",MapEntity(m->storedIn.lock()->ToEntity().shared_from_this(),actual,false));
			}
			foreach (m->myTasks,task)
			{
				tasks.AppendElement(ToTable(*task,m, task == m->myTasks.begin()));
			}
			t.SetField("tasks",tasks);



			t.SetField("movementSpeed",m->GetMovementStepsPerRound());

			t.SetField("motionIgnoresAttack",m->MotionIgnoresAttack());

			t.SetFunction("CanStandAt",[w](const Lua::Table&loc)
			{
				auto v = w.lock();
				if (!v)
					throw Lua::Exception(CLOCATION,"CanStandAt(): Local unit has expired");
				const World::Data&visibleWorld =
					#ifndef CLIENT
						v->owner->knownWorld
					#else
						World::actual
					#endif
				;
				const auto location = Detail::ToLGridCell(loc,"CanStandAt(): Location parameter");
				return visibleWorld.LowCellIsClear(location) && 
					(!v->IsAffectedByGravity(location) || v->CanStandOn(visibleWorld,location-1));
			});

			t.SetField("maxClimbUp", m->GetMaxClimbingHeight());
			t.SetField("minClimbDown", -m->GetMaxFallingHeight());


			t.SetFunction("IsAffectedByGravity",[this,w,location](const Lua::Table&luaLocation)
			{
				auto loc = luaLocation ? Detail::ToLGridCell(luaLocation,"IsAffectedByGravity(): Location parameter") : location;
				auto m = w.lock();
				return m->IsAffectedByGravity(loc);
			});

		//	Grid::LCellIdI loc;
			t.SetFunction("FindShortestPath", [this,w](const Lua::Table&targetLocation, const Lua::Function<int(Lua::Table, int)>&costModifier, bool ignoreFriendlyMobilesOnRoute)->Lua::Table
			{
				auto v = w.lock();
				if (!v)
					throw Lua::Exception("FindShortestPath(): Mobile entity has expired");

				const auto dest = Detail::ToLGridCell(targetLocation, "FindShortestPath(): Target parameter #1");
				return this->LuaFindShortestPathFrom(v,v->location,dest,costModifier,ignoreFriendlyMobilesOnRoute);
			});

			t.SetFunction("FindShortestPathFrom", [this,w](const Lua::Table&originLocation, const Lua::Table&targetLocation, const Lua::Function<int(Lua::Table, int)>&costModifier, bool ignoreFriendlyMobilesOnRoute)->Lua::Table
			{
				auto v = w.lock();
				if (!v)
					throw Lua::Exception("FindShortestPathFrom(): Vehicle has expired");
				const auto origin = Detail::ToLGridCell(originLocation, "FindShortestPathFrom(): Origin parmater #1");
				const auto dest = Detail::ToLGridCell(targetLocation, "FindShortestPathFrom(): Target parmater #2");

				return this->LuaFindShortestPathFrom(v,origin,dest,costModifier,ignoreFriendlyMobilesOnRoute);
			});


			t.SetFunction("GetReach", [this,w,location](const Lua::Function<bool(Lua::Table)>&isBlocked, int maxReach, bool forceReversible, bool allowAccessibleHangars, bool ignoreOwnedVehiclesOnRoute, bool cannotStandOnUnknown)->Lua::Table
			{
				auto v = w.lock();
				if (!v)
					throw Lua::Exception("GetReach(): Vehicle has expired");

				Objects::Component::GroundFlags groundFlags = Objects::Component::GroundFlags::None;

				if (cannotStandOnUnknown)
					groundFlags |= Objects::Component::GroundFlags::DisallowUnknownGround;

				Objects::Pathfinding::RoutingFlags routingFlags = Objects::Pathfinding::RoutingFlags::IgnoreLimit;
				if (forceReversible)
					routingFlags |= Objects::Pathfinding::RoutingFlags::ForceRevertible;
				if (allowAccessibleHangars)
					routingFlags |= Objects::Pathfinding::RoutingFlags::AllowAccessibleHangars;
				if (ignoreOwnedVehiclesOnRoute)
					routingFlags |= Objects::Pathfinding::RoutingFlags::IgnoreOwnMobilesOnRoute;

				if (!isBlocked)
					mapper.Map(*v, location, nullptr, routingFlags,groundFlags, maxReach);
				else
				{
					mapper.Map(*v, location, [&isBlocked, this](const Grid::LCellIdI&loc)->bool
					{
						return isBlocked(ToTable(loc));
					}, routingFlags,groundFlags);
				}

				Lua::Table result = program->CreateTable();
				Array<Lua::Table>	 tables;
				tables.SetSize(mapper.result.Count());
				for (index_t i = 0; i < mapper.result.Count(); i++)
				{
					const auto&in = mapper.result[i];
					Lua::Table luaReachable = program->CreateTable();
					tables[i] = luaReachable;
					luaReachable.SetField("location", ToTable(in.cell));
					luaReachable.SetField("movementCostsFromRoot", in.movementCostsFromRoot);
					if (allowAccessibleHangars)
					{
						auto hangar = Objects::AccessibleHangarHere(in.cell, *v);
						if (hangar)
						{
							auto e = hangar->ToEntity().shared_from_this();
							luaReachable.SetField("hangar", MapEntity(e,true,false));
						}
					}
					result.AppendElement(luaReachable);
				}

				for (index_t i = 0; i < mapper.result.Count(); i++)
				{
					const auto&in = mapper.result[i];
					if (in.predecessor != InvalidIndex)
						tables[i].SetField("predecessor", tables[in.predecessor]);
				}


				return result;
			});
			
			if (actual)
			{
				t.SetFunction("ClearTasks",[id,this]()
				{
					if (onDropAllTasks)
						onDropAllTasks(id);
					else
						throw Lua::Exception(CLOCATION,"ClearTasks(): onDropAllTasks not set");
				});
				t.SetFunction("ClearSafeTasks",[id,this]()
				{
					if (onDropSafeTasks)
						onDropSafeTasks(id);
					else
						throw Lua::Exception(CLOCATION,"ClearSafeTasks(): onDropSafeTasks not set");
				});
				t.SetFunction("DropLastTask",[id,this]()
				{
					if (onDropLastTask)
						onDropLastTask(id);
					else
						throw Lua::Exception(CLOCATION,"DropLastTask(): onDropLastTask not set");
				});

				auto loc = e->location;
				t.SetFunction("DirectTo",[id,this,loc](const Lua::Table&luaReachable)
				{
					Buffer0<Grid::LCellIdI>	path;
					Lua::Table at = luaReachable;
					while (at.IsSet())
					{
						path << Detail::ToLGridCell(at.GetField("location"),"DirectTo(): destination.location");
						at = at.GetField("predecessor");
					}
					if (path.Last() == loc)
						path.EraseLast();
					path.Revert();
					onDetailedDirectTo(id,path.ToRef());
				});


				if (m->turret->CanDo(Objects::Component::Construct))
					t.SetFunction("BuildStructure",[this,id](const String&type)
					{
						Objects::EntityType t = Objects::DecodeType(type);
						if (t.IsEmpty())
							throw Lua::Exception(CLOCATION,"BuildStructure(): Given type is empty: "+type);
						auto proto = Objects::Prototypes::Find(t);
						if (!proto)
							throw Lua::Exception(CLOCATION,"BuildStructure(): Unable to find prototype for "+ToString(t));
						if (!proto->IsStructure())
							throw Lua::Exception(CLOCATION,"BuildStructure(): Chosen type '"+ToString(t)+"' is no structure");
						if (onBeginStructureConstruction)
							onBeginStructureConstruction(id,t);
						else
							throw Lua::Exception(CLOCATION,"BuildStructure(): onBeginStructureConstruction not set");
					});
			}
			else
			{
				t.UnsetField("DirectTo");
				t.UnsetField("ClearTasks");
				t.UnsetField("Construct");
			}
		}
		return t;
	}


	void		API::AddEntity( const Objects::PWorldEntity&e )
	{
		if (!program)
			return;

		ASSERT__(lowCellGrid.IsNotEmpty());
		Lua::Table t = MapEntity(e,true,true);

	}

	void		API::AddFactionEntities( const Objects::PFaction&f )
	{
		Objects::ForeachWorldEntityIncludingStored([this,f](const Objects::PWorldEntity&e)
		{
			if (e->owner == f)
				AddEntity(e);
		});
	}

	void		API::AddAllAlliances()
	{
		Objects::Alliance::Foreach([this](const Objects::PAlliance&a)
		{
			AddAlliance(a, true,true);
		});
	}

	void		API::AddAllFactionsAndAlliances()
	{
		Objects::Alliance::Foreach([this](const Objects::PAlliance&a)
		{
			AddAlliance(a, false,false);
		});
		Objects::ForeachFaction([this](const Objects::PFaction&f)
		{
			AddFaction(f,true);
		});
		Objects::Alliance::Foreach([this](const Objects::PAlliance&a)
		{
			AddAlliance(a, true,true);
		});
	}



	void		API::AddAllEntities( )
	{
		Objects::ForeachWorldEntityIncludingStored([this](const Objects::PWorldEntity&e)
		{
			AddEntity(e);
		});
	}

	template <typename Setup>
		static void MapCells(const Lua::PProgram&program, Lua::Table&cellTable, const Setup&setup, Array<Lua::Table>&grid)
		{
			grid.SetSize(Setup::GridType::CellCount);
			for (UINT32 i = 0; i < Setup::GridType::CellCount; i++)
			{
				Lua::Table t = program->CreateTable();
				cellTable.SetElement(i+1,t);
				grid[i] = t;

				t.SetField("index",i+1);
				t.SetField("center",Detail::ToTable(program, setup.cells[i].cornerCenter));
			}

			for (UINT32 i = 0; i < Setup::GridType::CellCount; i++)
			{
				Lua::Table t = grid[i];
				Lua::Table nt = program->CreateTable();
				t.SetField("neighbors",nt);

				const auto&n = setup.grid.GetCell(i).neighborhood;
				for (BYTE k = 0; k < n.numNeighbors; k++)
					nt.AppendElement(grid[n.neighborID[k]]);
			}
		}

	void		API::AddTerrain()
	{
		Lua::Table cellTable = program->CreateTable();
		terrainTable.SetField("cells",cellTable);

		MapCells(program,cellTable,Grid::high,highCellGrid);

		terrainTable.SetFunction("TranslateUnitCell",[this](const Lua::Table&cell)
		{
			auto stack = Detail::LuaCellToStack(cell, "TranslateUnitCell(): Cell parameter",Grid::LowSetup::GridType::CellCount);
			return highCellGrid[Grid::low.grid.GetCell(stack).higherEquivalent];
		});

		terrainTable.SetFunction("GetContent",[](const Lua::Table&cell, int height)
		{
			auto stack = Detail::LuaCellToStack(cell, "GetContent(): Cell parameter #1",Grid::HighSetup::GridType::CellCount);
			return int(World::actual.GetCell(Grid::HCellIdI(stack,height)).content);
		});

		terrainContentTable.SetFunction("ToString",[](int content)
		{
			Grid::Content c;
			if (!c.Load(content))
				throw Lua::Exception(CLOCATION,"Game.Terrain.Content.ToString(): Parameter "+String(content)+" is outside supported content range");
			return String(c.ToString());
		});

		for (index_t i = 0; i < Grid::Content::N; i++)
			terrainContentTable.SetField(Grid::Content::Reinterpret(i).ToString(),i);
	}


	void		API::AddGrid()
	{
		Lua::Table cellTable = program->CreateTable();
		gridTable.SetField("unitCells",cellTable);

		MapCells(program,cellTable,Grid::low,lowCellGrid);

		gridTable.SetFunction("FindHighestAvailableResourceLocation",[this](const String&resource){return LuaFindHighestAvailableResourceLocation(resource);});
		gridTable.SetFunction("FindHighestAvailableResourceLocationExt",[this](const String&resource,bool checkLinkPath, bool checkMobiles){return LuaFindHighestAvailableResourceLocationExt(resource,checkLinkPath,checkMobiles);});
		gridTable.SetFunction("GetNaturalResourceAt",Detail::GetNaturalResourceAt);
		gridTable.SetFunction("GetNaturalResourcesAt",Detail::GetNaturalResourcesAt);
		gridTable.SetFunction("NoNaturalResourcesHere",Detail::NoNaturalResourcesHere);
		gridTable.SetFunction("NaturalResourcesSurveyedAt",Detail::NaturalResourcesSurveyedAt);


		gridTable.SetFunction("ToLocation",[this](const Lua::Table&cell, int height)
		{
			index_t stack = Detail::LuaCellToStack(cell,"ToLocation(): Cell parameter #1",Grid::LowSetup::GridType::CellCount);
			if (height < Grid::CellStack::BottomHeight)
				throw Lua::Exception("ToLocation(): Height parameter #1 ("+String(height)+") falls below minimum grid height ("+String(Grid::CellStack::BottomHeight)+")");
			if (height > Grid::CellStack::TopHeight)
				throw Lua::Exception("ToLocation(): Height parameter #1 ("+String(height)+") exceeds maximum grid height ("+String(Grid::CellStack::TopHeight)+")");
			return this->ToTable(Grid::LCellIdI(stack,height));
		});
		gridTable.SetFunction("AffectedByEMP",[](const Lua::Table&loc)
		{
			auto cell = Detail::ToLGridCell(loc,"AffectedByEMP(): Location parameter");
			return Grid::AffectedByEMP(cell);
		});
		gridTable.SetFunction("IsClear",[](const Lua::Table&loc)
		{
			auto location = Detail::ToLGridCell(loc,"IsClear(): Location parameter");
			return World::actual.LowCellIsClearIgnoreStructure(location);
		});
		gridTable.SetFunction("FindHighestVehicleGround",[this](const Lua::Table&unit, const Lua::Table&cell)->Lua::Table
		{
			const auto stack = Detail::LuaCellToStack(cell, "FindHighestVehicleGround(): Cell parameter #2" , Grid::LowSetup::GridType::CellCount);
			auto e = LuaUnitToEntity(unit, "FindHighestVehicleGround(): Unit parameter #1");
			if (!e->IsVehicle())
				throw Lua::Exception("FindHighestVehicleGround(): Unit parameter #1 does not point to a vehicle");
			auto v = To<Objects::Vehicle>::Cast(e);

			int h = Objects::FindApplicableGround(Grid::LCellIdI(stack,Grid::CellStack::TopHeight),v->base->moveOnWater,false);
			if (h == Grid::BadHeight)
				return Lua::nil;
			return this->ToTable(Grid::LCellIdI(stack,h));
		});



		gridTable.SetField("bottomHeight",Grid::CellStack::BottomHeight);
		gridTable.SetField("topHeight",Grid::CellStack::TopHeight);
		#ifdef LOWER_RESOLUTION_PLANET
			gridTable.SetField("lowerResolutionPlanet",true);
		#else
			gridTable.SetField("lowerResolutionPlanet",false);
		#endif

		gridTable.SetFunction("GetDistance",Detail::GetDistance);

	}



}


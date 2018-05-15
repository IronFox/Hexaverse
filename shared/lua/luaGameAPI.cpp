#include <global_root.h>
#include "luaGameAPI.h"
#include "logEvent.h"
#include "grid/stack.h"
#include "grid/resources.h"
#include "objects.h"
#include "../objects/faction.h"

namespace Lua
{
	namespace Detail
	{
		void LogLua(const String&msg, const String&programName)
		{
			LogEvent(EventClass::Preset::important,programName+"(Lua): "+msg);
		}
		

		index_t	LuaCellToStack( const Lua::Table&cellTable, const String&parameterName )
		{
			if (cellTable.IsEmpty())
				throw Lua::Exception(parameterName + " is nil.");
			auto luaIndex = cellTable.GetField("index");
			if (luaIndex.IsEmpty())
				throw Lua::Exception(parameterName + " has no entry 'index'.");

			const int stackID = luaIndex.GetAs<int>();

			if (stackID < 1 || stackID > Grid::LowSetup::GridType::CellCount)
				throw Lua::Exception(parameterName + " is invalid cell location (#"+String(stackID)+" not in [1,"+String(Grid::LowSetup::GridType::CellCount)+"]).");

			return index_t(stackID-1);
		}

		Grid::LCellIdI CellHToLGridCell( const Lua::Table&luaCell, int h, const String&parameterName)
		{
			return Grid::LCellIdI(LuaCellToStack(luaCell,parameterName),h);

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

		#ifdef CLIENT

			int	GetCompoundResourceChange(const String&resourceName)
			{
				Objects::Resource resource;
				if (!resource.Decode(resourceName.c_str()))
					throw Lua::Exception("Resource '"+resourceName+"' does not exist.");
				#ifndef ONE_COMPOUND_PER_FACTION
					return 0;
				#else
					return Objects::Faction::mine->compound.total.supply.level[resource] - Objects::Faction::mine->compound.total.demand.level[resource];
				#endif
			}

			int GetCompoundResourceStorage(const String&resourceName)
			{
				Objects::Resource resource;
				if (!resource.Decode(resourceName.c_str()))
					throw Lua::Exception("Resource '"+resourceName+"' does not exist.");
				#ifndef ONE_COMPOUND_PER_FACTION
					return 0;
				#else
					return Objects::Faction::mine->compound.total.storage.level[resource];
				#endif
			}

			int FindHighestAvailableResourceLocation(const String&resourceName)
			{
				foreach (Grid::Resource::all,r)
					if (r->GetName() == resourceName)
					{
						int highestAt = -1;
						Grid::Resource::coverage_t highest = 0;
						for (index_t i = 0; i < Grid::Resource::NumEntries; i++)
							if (Grid::ResourceDiscovery::client.resourcesDiscovered[i])
							{
								if (r->coverage[i] > highest)
								{
									if (Objects::FindApplicableGround(Grid::LCellIdI(i,0),true,true) != Objects::badHeight)
									{
										highest = r->coverage[i];
										highestAt = (int)i;
									}
								}
							}
						return highestAt;
					}
				throw Lua::Exception("Resource '"+resourceName+"' does not exist.");
			}

			int GetResourceCoverage(const String&resourceName, Lua::Table cellTable)
			{
				const auto stackID = LuaCellToStack(cellTable,"GetResourceCoverage(): cell (parameter #2)");
				foreach (Grid::Resource::all,r)
					if (r->GetName() == resourceName)
					{
						if (!Grid::ResourceDiscovery::client.resourcesDiscovered[stackID])
							return -1;
						return r->coverage[stackID];
					}
				throw Lua::Exception("GetResourceCoverage(): Resource '"+resourceName+"' does not exist.");
			}


			//Lua::Table	CreateMarker( const Lua::Table&entity, const String&type )
			//{
			//	Objects::Marker::Type t;
			//	if (!t.Decode(type.c_str()))
			//		throw Lua::Exception("CreateMarker(): Marker type '"+type+"' is not supported");
			//	Objects::Marker m = ApplicationInterface::CreateMarker(ToLGridCell(loc,"CreateMarker(): location parameter #1"),t);
			//	Protocol::Marker marker;
			//	marker.id.Generate();
			//	marker.location = v->location;
			//	tempMarkers << v->system.w.xyz;
			//	static_assert(Objects::Marker::Type::Visited == 2, "Please update marker logic");
			//	marker.data.Set2();
			//	ObjectHelper::SignalMarkerCreation(marker);

			//}


		#endif





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

	Lua::Table API::ToTable(const Objects::MobileEntity::Task&task)
	{
		Lua::Table taskOut = program->CreateTable();
		taskOut.SetField("name",task.name);
		taskOut.SetField("isJob",task.IsJob());

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
			t.SetFunction("CanHit",[trajectory,flatAttackRange,origin](const Lua::Table&vector)
			{
				if (vector.IsEmpty())
					return false;
				M::TVec3<>	dest;
				vector.GetField("x",dest.x);
				vector.GetField("y",dest.y);
				vector.GetField("z",dest.z);
				return Objects::Component::Turret::CanHit(trajectory,flatAttackRange,origin,dest);
			});
		}

		for (index_t op = 0; op < Objects::Component::OperationCount; op++)
		{
			const auto o = (Objects::Component::operation_t)op;
			const String os = String(Objects::Component::OperationToString(o));
			t.SetField("can"+os,b.turret->CanDo(o));
					
			t.SetField("speedOf"+os,b.GetOperationSpeed(o));
		}

		return t;
	}

	#ifdef CLIENT
		void		API::AddMyMarker(const Objects::Marker&m)
		{
			Lua::Table t = program->CreateTable();
			if (!myMarkerIDMap)
			{
				myMarkerIDMap = program->CreateTable();
				gameTable.SetField("myMarkers",myMarkerIDMap);
			}
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

		void	API::UpdateMyResearch()
		{
			
			if (myResearch.IsEmpty())
			{
				myResearch = program->CreateTable();
				gameTable.SetField("myResearch",myResearch);
			}
			else
				myResearch.ClearTable();

			Lua::Reference current;
			const auto&st = Objects::Faction::mine->researchState.topicProgressVector;
			//const index_t at = Objects::Faction::mine->researchState.GetActiveTopicIndex();
			Array<UINT32> q;
			Objects::Faction::mine->researchState.ExportResearchQueue(q);
			for (index_t i = 0; i < Objects::Research::globalState.allTopics.Count(); i++)
			{
				auto t = Objects::Research::GetTopic(i);
				if (t->IsGroup())
					continue;
				String key = t->GetFullName();

				Lua::Table outT = program->CreateTable();
				myResearch.SetField(key,outT);

				outT.SetField("fullName",key);
				outT.SetField("name",t->name);
				outT.SetField("max",t->maxProgress);
				outT.SetField("isEndless",t->maxProgress <= 0);
				
				const int prog = st[t->serialIndex];
				outT.SetField("progress",prog);
				outT.SetField("isCompleted",t->ResearchCompleted(st));

				bool isActive = q.IsNotEmpty() && q.First() == i;
				outT.SetField("isActive",isActive);
				outT.SetField("isQueued",q.Contains((UINT32)i));

				if (isActive)
					current = outT;
			}


			gameTable.SetField("myCurrentResearch",current);
		}

	#endif


	void		API::FlushAllObjects(bool clearCaches/*=false*/)
	{
		entityIDMap.ClearTable();
		factionIDMap.ClearTable();
		allianceIDMap.ClearTable();
		entityMap.Clear();
		#ifdef CLIENT
			myEntityIDMap.ClearTable();
			myMarkerIDMap.ClearTable();
			myResearch.ClearTable();
			gameTable.UnsetField("myCurrentResearch");
			gameTable.UnsetField("selectedUnit");
		#endif

		entityMappedToTable.Clear();
		factionMappedToTable.Clear();
		allianceMappedToTable.Clear();

		if (clearCaches)
		{
			loadedAllianceTableMap.Clear();
			loadedEntityTableMap.Clear();
			loadedFactionTableMap.Clear();
		}
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
		if (program)
			program->SetGlobal("Game",Lua::Reference{});
		program.reset();
		FlushAllObjects(true);
		gameTable.ClearReference();
		gridTable.ClearReference();
		pathfindingTable.ClearReference();
		lowCellGrid.Clear();
		locationMap.Clear();
	}


	void	API::Setup()
	{
		const auto p = program;
		ASSERT__(p);
		gameTable = p->CreateTable();
		gridTable = p->CreateTable();
		gameTable.SetField("Grid",gridTable);
		pathfindingTable = p->CreateTable();
		gameTable.SetField("Pathfinding", pathfindingTable);
		gameTable.SetGlobal("Game");

		entityIDMap = program->CreateTable();
		gameTable.SetField("units",entityIDMap);
		#ifdef CLIENT
			myEntityIDMap = program->CreateTable();
			gameTable.SetField("myUnits",myEntityIDMap);
		#endif

		factionIDMap = program->CreateTable();
		gameTable.SetField("factions",factionIDMap);

		allianceIDMap = program->CreateTable();
		gameTable.SetField("alliances",allianceIDMap);


		String name = program->GetName();
		gameTable.SetFunction("Log", [name](const String&msg){Detail::LogLua(msg,name);});
		#ifdef CLIENT
			gameTable.SetFunction("FindHighestAvailableResourceLocation",Detail::FindHighestAvailableResourceLocation);
			gameTable.SetFunction("GetResourceCoverage",Detail::GetResourceCoverage);
			gameTable.SetFunction("GetCompoundResourceChange",Detail::GetCompoundResourceChange);
			gameTable.SetFunction("GetCompoundResourceStorage",Detail::GetCompoundResourceStorage);
			gameTable.SetFunction("CreateMarker",[this](const Lua::Table&luaEntity, const String&markerType)
			{
				Objects::Marker::Type t;
				if (!t.Decode(markerType.c_str()))
					throw Lua::Exception("CreateMarker(): Type parameter #2 cannot be decoded to a marker type");

				auto luaID = luaEntity.GetField("id");
				if (!luaID)
					throw Lua::Exception("CreateMarker(): Entity parameter #1 does not contain an id field");
				TUniqueID entityID;
				if (!entityID.Decode(luaID.GetAs<String>()))
					throw Lua::Exception("CreateMarker(): Entity parameter #1 does not contain a decodeable id field");

				const Lua::Table loc = luaEntity.GetField("location");

				TUniqueID id;
				id.Generate();

				Lua::Table mt = program->CreateTable();
				if (!myMarkerIDMap)
				{
					myMarkerIDMap = program->CreateTable();
					gameTable.SetField("myMarkers",myMarkerIDMap);
				}
				myMarkerIDMap.SetField(id.ToFullString(), mt);
				mt.SetField("location",loc);
				mt.SetField("type",luaEntity.GetField("type"));
				mt.SetField("confirmed", false);
				mt.SetField("age",0);
				mt.SetField("id","");
				mt.SetField("unitReference",luaEntity);
				mt.SetFunction("Touch",[]()
				{
					//do nothing
				});

				if (onMarkerCreate)
					onMarkerCreate(id,t,entityID,Detail::ToLGridCell(loc,"Entity parameter #1.location"));
				else
					throw Lua::Exception("CreateMarker(): onMarkerCreate() not set");
				
			});
		#endif

		
		pathfindingTable.SetFunction("FindShortestPath", [this](const Lua::Table&entityTable, const Lua::Table&targetLocation, const Lua::Function<int(Lua::Table,int)>&costModifier, bool ignoreFriendlyMobilesOnRoute)->Lua::Table
		{
			Grid::LCellIdI loc;
			const auto v = ParameterToVehicle(entityTable, loc, "FindShortestPath(): Unit parameter #1");
			const auto dest = Detail::ToLGridCell(targetLocation, "FindShortestPath(): Target parmater #2");
			
			bool success = true;
			Objects::Pathfinding::FCellCostModifier costMod;
			if (costModifier.IsSet())
				costMod = [&costModifier, this](const Grid::LCellIdI&loc, int&costs)
				{
					costs = costModifier(ToTable(loc), costs);
				};
			success = shortestPath.Update(*v, loc, dest, ignoreFriendlyMobilesOnRoute, false, nullptr, costMod);
			if (!success)
				return Lua::Table{};
			
			Lua::Table result = program->CreateTable(),pred;
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
		});

		pathfindingTable.SetFunction("GetUnitReach",[this](const Lua::Table&entityTable,const Lua::Function<bool(Lua::Table)>&isBlocked,int maxReach,bool forceReversible,bool allowAccessibleHangars,bool ignoreFriendlyVehiclesOnRoute,bool cannotStandOnUnknown)->Lua::Table
		{
			Grid::LCellIdI loc;
			const auto v = ParameterToVehicle(entityTable, loc,"GetUnitReach(): Unit parameter #1");
			if (loc.height > Grid::CellStack::TopHeight)
				throw Lua::Exception("GetUnitReach(): Unit parameter #1 '" + v->GetName() + "' is located above maximum height");

			if (cannotStandOnUnknown)
				Objects::Component::VehicleBase::canStandOnUnknown = false;
				

			if (!isBlocked)
				mapper.Map(v.get(),loc,nullptr,maxReach,forceReversible,allowAccessibleHangars,ignoreFriendlyVehiclesOnRoute);
			else
			{
				mapper.Map(v.get(),loc,[&isBlocked,this](const Grid::LCellIdI&loc)->bool
				{
					return isBlocked(ToTable(loc));
				},maxReach,forceReversible,allowAccessibleHangars,ignoreFriendlyVehiclesOnRoute);
			}

			if (cannotStandOnUnknown)
				Objects::Component::VehicleBase::canStandOnUnknown = true;
			


			Lua::Table result = program->CreateTable();
			Array<Lua::Table>	 tables;
			tables.SetSize(mapper.result.Count());
			for (index_t i = 0; i < mapper.result.Count(); i++)
			{
				const auto&in = mapper.result[i];
				Lua::Table luaReachable  = program->CreateTable();
				tables[i] = luaReachable; 
				luaReachable.SetField("location",ToTable(in.cell));
				luaReachable.SetField("movementCostsFromRoot",in.movementCostsFromRoot);
				if (allowAccessibleHangars)
					luaReachable.SetField("isHangarHere",Objects::AccessibleHangarHere(in.cell,*v));
				result.AppendElement(luaReachable);
			}

			for (index_t i = 0; i < mapper.result.Count(); i++)
			{
				const auto&in = mapper.result[i];
				if (in.predecessor != InvalidIndex)
					tables[i].SetField("predecessor",tables[in.predecessor]);
			}


			return result;
		});
	//			Reachable[]	Game.MapUnitReach(Unit unit,func isBlocked, int maxReach, bool forceReversible, bool allowAccessibleHangars, bool ignoreFriendlyVehiclesOnRoute);
	//

	//Reachable	
	//{
	//	Loc		location;				Grid/world location
	//	int		movementCostsFromRoot;	Costs to reach this location from the current unit location
	//	Reachable	predecessor;		nil if this is root (the current unit location)
	//	bool	isHangarHere;			True if accessible hangars were included and there is a hangar here
	//}


		gameTable.SetFunction("GetDistance",Detail::GetDistance);

		#ifdef LOWER_RESOLUTION_PLANET
			gridTable.SetField("lowerResolutionPlanet",true);
		#else
			gridTable.SetField("lowerResolutionPlanet",false);
		#endif

		gridTable.SetFunction("ToLocation",[this](const Lua::Table&cell, int height)
		{
			index_t stack = Detail::LuaCellToStack(cell,"ToLocation(): Cell parameter #1");
			if (height < Grid::CellStack::BottomHeight)
				throw Lua::Exception("ToLocation(): Height parameter #1 ("+String(height)+") falls below minimum grid height ("+String(Grid::CellStack::BottomHeight)+")");
			return this->ToTable(Grid::LCellIdI(stack,height));
		});
		gridTable.SetFunction("AffectedByEMP",[this](const Lua::Table&loc)
		{
			auto cell = Detail::ToLGridCell(loc,"AffectedByEMP(): Parameter");
			return Grid::AffectedByEMP(cell);
		});

		gridTable.SetField("bottomHeight",Grid::CellStack::BottomHeight);
		gridTable.SetField("topHeight",Grid::CellStack::TopHeight);
	}


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


	API::TAlliance*	API::AddAlliance( const Objects::PAlliance&a, bool updateIfFound )
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
		found->factionTable.ClearTable();
		a->VisitMembers([found,this](const Objects::PFaction&f)
		{
			auto luaFaction = AddFaction(f,false);
			found->factionTable.AppendElement(luaFaction->table);
		},true);

		t.SetField("id",a->id.ToFullString());
		return found;
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
				factionIDMap.SetField(f->id.ToFullString(),found->table);

			if (!updateIfFound)
				return found;
			t = found->table;
		}
		else
		{
			t = program->CreateTable();
			found = loadedFactionTableMap.SetNew(f->id);
			found->table = t;
			found->entityTable = program->CreateTable();
			t.SetField("units",found->entityTable);

			factionMappedToTable.Set(f->id);
			factionIDMap.SetField(f->id.ToFullString(),t);
		}

		t.SetField("name",f->name);
		t.SetField("id",f->id.ToFullString());
		const auto*const alliance = AddAlliance(f->alliance,false);
		t.SetField("alliance",alliance->table);

		return found;
	}


	void			API::TFaction::AddEntity(const Lua::Table&e)
	{
		entityTable.AppendElement(e);
	}


	Lua::Table	API::MapEntity( const Objects::PWorldEntity&e, bool actual, bool updateIfFound )
	{
		auto found = loadedEntityTableMap.QueryPointer(e->id);
		Lua::Table t;
		bool outIsNew = false;
		if (found)
		{
			t = found->first;
			if (actual && entityMappedToTable.SetNew(e->id))
			{
				const String sid = e->id.ToFullString();
				entityIDMap.SetField(sid,t);
				#ifdef CLIENT
					if (e->isOwnedByMe)
						myEntityIDMap.SetField(sid,t);
				#endif
			}
			if (!updateIfFound && actual == found->second)
				return t;
		}
		else
		{
			entityMap.Set(e->id,e);
			t = program->CreateTable();
			outIsNew = true;
			
			if (actual)
			{
				const String sid = e->id.ToFullString();

				entityMappedToTable.Set(e->id);
				entityIDMap.SetField(sid,t);
				#ifdef CLIENT
					if (e->isOwnedByMe)
						myEntityIDMap.SetField(sid,t);
				#endif
			}
			loadedEntityTableMap.Set(e->id,std::make_pair(t,actual));
		}




		t.SetField("name",e->GetName());
		t.SetField("niceName",e->GetNiceName());
		t.SetField("id",e->id.ToFullString());
		t.SetField("typeName",e->GetTypeName());
		t.SetField("isMobile",e->IsMobile());
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
		t.SetField("empDamageLevel",e->empDamageLevel);
		t.SetField("scanningRange",e->GetScanningRange());

		const TUniqueID id = e->id;

		if (actual)
		{
			t.SetFunction("Destroy", [id,this]()
			{
				onRemove(id);

				//#ifdef CLIENT
				//	ApplicationInterface::SignalRemoval(e,false);
				//#else
				//	float t= 0;
				//	Objects::Remove(e,Objects::RemovalType::ControlledExplosion,t);
				//#endif

			});
		}
		else
			t.UnsetField("Destroy");

		if (e->IsWorkingStructure())
		{
			t.SetField("isWorkingStructure",true);
			t.SetField("isEnabled",To<Objects::WorkingStructure>::Cast(e)->IsEnabled());
			if (actual)
			t.SetFunction("Enable", [id,this](bool doEnable)
				{
					onEnableWorkingStructure(id,doEnable);
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
			Lua::Table queue = program->CreateTable();
			foreach (prod->queue,w)
			{
				queue.AppendElement(w->GetName());
			}
			t.SetField("productionQueue",queue);
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
				t.SetFunction("ScheduleAttacks",[id,this](const Lua::Table&t)
				{
					Array<Grid::LCellIdI> targets(t.GetLength());
					for (index_t i = 0; i < t.GetLength(); i++)
					{
						Lua::Table element = t.GetElement(i+1);
						if (element.IsEmpty())
							throw Lua::Exception("ScheduleAttacks(): Element #"+String(i+1)+" is nil.");
						Grid::LCellIdI loc = Detail::ToLGridCell(element,"table.#"+String(i+1));
						targets[i] = loc;
					}

					onAttackTargets(id,targets);
				});
			}
			else
				t.UnsetField("ScheduleAttacks");
		}



		if (e->IsMobile())
		{
			Lua::Table tasks = program->CreateTable();
			const auto&m = To<Objects::MobileEntity>::Cast(e);
			t.SetField("isAutomated",m->isAutomated);
			if (!m->storedIn.expired())
			{
				t.SetField("storedIn",MapEntity(m->storedIn.lock()->ToEntity().shared_from_this(),actual,false));
			}
			foreach (m->myTasks,task)
			{
				tasks.AppendElement(ToTable(*task));
			}
			t.SetField("tasks",tasks);



			t.SetField("movementSpeed",m->GetMovementStepsPerRound());

			t.SetField("motionIgnoresAttack",m->MotionIgnoresAttack());

			if (actual)
			{
				t.SetFunction("ClearTasks",[id,this]()
				{
					onDropAllTasks(id);
					//#ifdef CLIENT
					//	ApplicationInterface::SignalDropAllTasks(e,false);
					//#else
					//	ApplicationInterface::SignalDropAllTasks(e);
					//#endif
				});

				t.SetFunction("DirectTo",[id,this](const Lua::Table&luaReachable)
				{
					Buffer0<Grid::LCellIdI>	path;
					Lua::Table at = luaReachable;
					while (at.IsSet())
					{
						path << Detail::ToLGridCell(at.GetField("location"),"DirectTo(): destination.location");
						at = at.GetField("predecessor");
					}
					path.Revert();
					onDetailedDirectTo(id,path.ToRef());
				});
			}
			else
			{
				t.UnsetField("DirectTo");
				t.UnsetField("ClearTasks");
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

	void		API::AddAllEntities( )
	{
		Objects::ForeachWorldEntityIncludingStored([this](const Objects::PWorldEntity&e)
		{
			AddEntity(e);
		});
	}


	void		API::AddGrid()
	{
		if (!program)
		{
			lowCellGrid.Clear();
			return;
		}
		lowCellGrid.SetSize(Grid::LowSetup::GridType::CellCount);
		Lua::Table cellTable = program->CreateTable();
		gridTable.SetField("unitCells",cellTable);

		for (UINT32 i = 0; i < Grid::LowSetup::GridType::CellCount; i++)
		{
			Lua::Table t = program->CreateTable();
			cellTable.SetElement(i+1,t);
			lowCellGrid[i] = t;

			t.SetField("index",i+1);
			t.SetField("center",Detail::ToTable(program, Grid::low.cells[i].cornerCenter));
		}

		for (UINT32 i = 0; i < Grid::LowSetup::GridType::CellCount; i++)
		{
			Lua::Table t = lowCellGrid[i];
			Lua::Table nt = program->CreateTable();
			t.SetField("neighbors",nt);

			const auto&n = Grid::low.grid.GetCell(i).neighborhood;
			for (BYTE k = 0; k < n.numNeighbors; k++)
				nt.AppendElement(lowCellGrid[n.neighborID[k]]);
		}
	}



}


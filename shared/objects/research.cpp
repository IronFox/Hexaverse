#include <global_root.h>
#include "research.h"
#include "prototypes.h"
#include "structure.h"
#include "../logEvent.h"
#include "../scriptCache.h"
#include "faction.h"
#include "../runtimeConfig.h"
#include <io/file_system.h>

namespace Objects
{



	/*static*/ ResearchState::GlobalState		ResearchState::globalState;


	/*static*/ void ResearchState::LogLua(const String&msg)
	{
		LogEvent(EventClass::Preset::important,"Lua: "+msg);
	}

	/*static*/ PResearchTopic	ResearchState::CreateResearch(const PResearchTopic&parent, const String&name, const String&iconName, 
												const ResearchTopic::FSetProgress&setProgress, 
												const ResearchTopic::FGetProgress&getProgress, int maxProgress,
												const PResearchTopic&dependency
											)
	{
		LogEvent(EventClass::Preset::important,"Lua created new research '"+name+"'");

		const String&iconSource = iconName.IsEmpty() ? name : iconName;

		PWorldEntity proto = GetNonVehiclePrototypeByName(iconSource);
		Component::Base*component = proto ? nullptr : Component::GetByName(iconSource);
		bool hasIcon = !proto && !component;


		PResearchTopic rs(new ResearchTopic(parent, name,hasIcon ? iconSource : "",setProgress,getProgress,maxProgress,dependency));

		rs->componentReference = component;
		rs->prototypeReference = proto;
		rs->serialIndex = globalState.allTopics.Count();
		globalState.allTopics << rs;


		if (!parent)
		{
			if (!globalState.rootResearch.Insert(rs))
				throw Lua::Exception("Research '"+name+"' is not unique in global namespace");
		}
		else
		{
			if (!parent->Insert(rs))
				throw Lua::Exception("Research '"+name+"' is not unique in '"+parent->GetFullName()+"'");
			if (parent->maxProgress != 0)
			{
				throw Lua::Exception("Trying to convert regular research '"+parent->name+"' to group when adding '"+name+"' as child. '"+parent->name+"'.maxProgress must be 0 when used as a group");
			}
		}

		if (dependency)
		{
			if (dependency->getProgress.IsEmpty())
			{
				throw Lua::Exception("Trying to register research dependency of '"+name+"' to unresearchable topic '"+dependency->name+"'");
			}

		}
		return rs;
	}

	ResearchTopic::ResearchTopic(const PResearchTopic&parent, const String&name, const String&iconName, 
								const FSetProgress&setProgress, 
								const FGetProgress&getProgress, int maxProgress,
								const PResearchTopic&dependency
								)
								:Super(name), parent(parent),iconName(iconName),
								setProgress(setProgress),getProgress(getProgress),maxProgress(maxProgress),
								dependency(dependency)
	{}



	ResearchTopic::ResearchTopic::~ResearchTopic()
	{
		bool brk = true;

	}




	static Array<BaseAttributeTable>	structureTables;


	template <typename T, typename F>
		static void InitAttrib(const Lua::Table&source, const String&path, const String&name, LuaAttribute<T,F>&a, bool allowEmpty = false)
		{
			Lua::Reference ref = source.GetField(name);
			a.Clear();
			if (ref.IsEmpty())
			{
				if (!allowEmpty)
					throw Lua::Exception("Field '"+path+"."+name+"' is not defined");
				return;
			}

			if (ref.IsTable())
			{
				Lua::Table t = source.GetField(name);
			
				a.luaGetValue = t.GetField("Get");
				if (a.luaGetValue.IsEmpty())
					throw Lua::Exception("Function '"+path+"."+name+".Get' is not defined");

				a.luaDescribeValue = t.GetField("Describe");
			}
			elif (ref.IsFunction())
			{
				a.luaGetValue = ref;
			}
			else
			{
				a.SetConst(ref.GetAs<T>());
			}
		}

	static void InitTable(String baseName, BaseAttributeTable&t)
	{
		baseName.eraseCharacter(' ');
		baseName.eraseCharacter('-');
		
		static Lua::Table configTable;
		if (configTable.IsEmpty())
		{
			configTable = ResearchState::globalState.researchProgram->FindGlobal("StructureConfig");
			if (configTable.IsEmpty())
				throw Lua::Exception("Table 'StructureConfig' is not defined");
		}

		Lua::Table source = configTable.GetField(baseName);
		if (source.IsEmpty())
			throw Lua::Exception("Table 'StructureConfig."+baseName+"' is not defined");

		String path =  "StructureConfig."+baseName;
		InitAttrib(source,path,  "Cost",t.constructionCost);
		InitAttrib(source,path,  "Health",t.maxHealth);
		InitAttrib(source,path,  "ScanningRange",t.maxScanningRange);
		InitAttrib(source,path, "IsUnlocked", t.isUnlocked);
	}


	static Lua::Table InitComponent(const Lua::Table&parent, String&path, Component::Base&b)
	{
		String baseName = b.typeName;
		baseName.eraseCharacter(' ');
		baseName.eraseCharacter('-');

		path += '.'+baseName;

		Lua::Table baseTable = parent.GetField(baseName);
		if (baseTable.IsEmpty())
			throw Lua::Exception("Table '"+path+"' is not defined");
		InitAttrib(baseTable,path,"Cost",b.productionCosts);
		InitAttrib(baseTable,path,"IsUnlocked",b.isUnlocked);
		return baseTable;
	}

	static void InitVehicleBase(Component::VehicleBase&b)
	{
		static Lua::Table configTable;
		if (configTable.IsEmpty())
		{
			configTable = ResearchState::globalState.researchProgram->FindGlobal("VehicleBaseConfig");
			if (configTable.IsEmpty())
				throw Lua::Exception("Table 'VehicleBaseConfig' is not defined");
		}
		
		String path = "VehicleBaseConfig";
		Lua::Table myTable = InitComponent(configTable,path,b);

		InitAttrib(myTable,path,"TurretRangeModifier",b.turretRangeModifier);
		InitAttrib(myTable,path,"OperationSpeedMultiplier",b.operationSpeedMultiplier);
		InitAttrib(myTable,path,"MovementSpeed",b.movementSpeed);
		InitAttrib(myTable,path,"CarryingCapacity",b.carryingCapacity);
		InitAttrib(myTable,path,"Health",b.health);
	}

	static void InitTurret(Component::Turret&t)
	{
		static Lua::Table configTable;
		if (configTable.IsEmpty())
		{
			configTable = ResearchState::globalState.researchProgram->FindGlobal("TurretConfig");
			if (configTable.IsEmpty())
				throw Lua::Exception("Table 'TurretConfig' is not defined");
		}

		String path = "TurretConfig";
		Lua::Table myTable = InitComponent(configTable,path,t);

		InitAttrib(myTable,path,"HealthMultiplier",t.healthMultiplier);
		InitAttrib(myTable,path,"ScanningRange",t.scanningRange);
		InitAttrib(myTable,path,"Weight",t.weight);

		for (index_t i = 0; i < Component::OperationCount; i++)
		{
			InitAttrib(myTable,path,String("Op_")+Component::OperationToString((Component::operation_t)i),t.operationSpeed[i],true);
		}

		InitAttrib(myTable,path,"GroundScannerPenetration",t.groundScannerPenetration,true);
		InitAttrib(myTable,path,"GroundScanningRange",t.groundScanningRange,true);

		InitAttrib(myTable,path,"AmmoConsumption",t.ammoNeedPerShot,true);
		InitAttrib(myTable,path,"AttacksPerRound",t.attacksPerRound,true);
		if (t.attacksPerRound.IsSet())
		{
			InitAttrib(myTable,path,"HorizontalAttackRange",t.horizontalAttackRange);
		}
		InitAttrib(myTable,path,"ExplosionRadius",t.explosionRadius,true);	//can be independent
		InitAttrib(myTable,path,"DamageLevel",t.damageLevel,true);
	}


	/*static*/ void		ResearchState::StaticCreate()
	{
		structureTables.SetSize(structurePrototypes.Count());
		for (index_t i = 0; i < structurePrototypes.Count(); i++)
		{
			const PStructure&s = structurePrototypes[i];
			BaseAttributeTable&t = structureTables[i];
			s->DefineAttributeTable(&t);
		}

		globalState.program0 = ScriptCache::LoadContent(
			"ResearchState = {}\n"
			"ResearchState.__index = ResearchState\n"
			"function ResearchState.New()\n"
				"local rs = {}\n"
				"setmetatable(rs,ResearchState)\n"
				"return rs\n"
			"end\n"
			"function ResearchState:Export()\n"
				"return 0\n"
			"end\n"
			"function ResearchState:Import(exported)\n"
			"end\n"
					"\n"
		"function DefaultHealthAtCost(cost--[[float]]) --float\n"
		"	return cost /5\n"
		"end\n"
		"\n"
		"function ExplicitStructure(cost, health, scanningRange, isUnlocked)\n"
		"	return\n"
		"	{\n"
		"		Cost = cost,\n"
		"		Health = health,\n"
		"		ScanningRange = scanningRange,\n"
		"		IsUnlocked = isUnlocked\n"
		"	}\n"
		"end\n"
		"\n"
		"function Structure(cost, scanningRange)\n"
		"	return ExplicitStructure(cost, DefaultHealthAtCost(cost),scanningRange, true)\n"
		"end\n"
		"\n"
		"function DefaultStructure(cost)\n"
		"	return Structure(cost,2)\n"
		"end\n"
		"\n"
		"function Get(attribute, state)\n"
		"	if (attribute == nil) then\n"
		"		return 0\n"
		"	end\n"
		"	t = type(attribute)\n"
		"	if t == 'table' then\n"
		"		return attribute.Get(state)\n"
		"	end\n"
		"	if t == 'function' then\n"
		"		return attribute(state)\n"
		"	end\n"
		"	return attribute\n"
		"end\n"
		"\n"
		"function Describe(attribute, state)\n"
		"	if (attribute == nil) then\n"
		"		return '0'\n"
		"	end\n"
		"	t = type(attribute)\n"
		"	if t == 'table' then\n"
		"		return attribute.Describe(state)\n"
		"	end\n"
		"	if t == 'function' then\n"
		"		return attribute(state)\n"
		"	end\n"
		"	return attribute\n"
		"end"
			"	TurretConfig = "
			"	{\n"
			"	MissileLauncher = "
			"	{\n"
			"	Cost = 40,\n"
			"	Weight = 13,\n"
			"	HealthMultiplier = 1.5,\n"
			"	ScanningRange = 4,\n"
			"	AttacksPerRound = 1,\n"
			"	DamageLevel = 10,\n"
			"	ExplosionRadius = 1,\n"
			"	HorizontalAttackRange = 8,\n"
			"	IsUnlocked = true,\n"
			"	},\n"
			"	Cannon = "
			"	{\n"
			"	Cost = 15,\n"
			"	Weight = 3,\n"
			"	HealthMultiplier = 2,\n"
			"	ScanningRange = 5,\n"
			"	AttacksPerRound = 2,\n"
			"	DamageLevel = 3,\n"
			"	ExplosionRadius = 1,\n"
			"	HorizontalAttackRange = 5,\n"
			"	IsUnlocked = true,\n"
			"	},\n"
			"	Radar =\n"
			"	{\n"
			"	Cost = 10,\n"
			"	Weight = 2,\n"
			"	HealthMultiplier = 1,\n"
			"	ScanningRange = 12,\n"
			"	IsUnlocked = true,\n"
			"	},\n"
			"	Surveyor =\n"
			"	{\n"
			"	Cost = 10,\n"
			"	Weight = 1,\n"
			"	HealthMultiplier = 1,\n"
			"	ScanningRange = 4,\n"
			"	IsUnlocked = true,\n"
			"	},\n"
			"	Bomb =\n"
			"	{\n"
			"	Cost = 40,\n"
			"	Weight = 5,\n"
			"	HealthMultiplier = 1,\n"
			"	ScanningRange = 1,\n"
			"	ExplosionRadius = 7,\n"
			"	DamageLevel = 50,\n"
			"	HorizontalAttackRange = 0,\n"
			"	IsUnlocked = true,\n"
			"	},\n"
			"	EMP =\n"
			"	{\n"
			"	Cost = 100,\n"
			"	Weight = 5,\n"
			"	HealthMultiplier = 1,\n"
			"	ScanningRange = 3,\n"
			"	AttacksPerRound = 1,\n"
			"	DamageLevel = 5,\n"
			"	ExplosionRadius = 3,\n"
			"	HorizontalAttackRange = 17,\n"
			"	AmmoConsumption = 50,\n"
			"	IsUnlocked = false,\n"
			"	},\n"
			"	Artillery =\n"
			"	{\n"
			"	Cost = 25,\n"
			"	Weight = 3,\n"
			"	HealthMultiplier = 1.5,\n"
			"	ScanningRange = 4,\n"
			"	AttacksPerRound = 1,\n"
			"	DamageLevel = 7,\n"
			"	ExplosionRadius = 3.5,\n"
			"	HorizontalAttackRange = 7,\n"
			"	IsUnlocked = false,\n"
			"	},\n"
			"	Terraformer =\n"
			"	{\n"
			"	Cost = 40,\n"
			"	Weight = 2,\n"
			"	HealthMultiplier = 1,\n"
			"	ScanningRange = 4,\n"
			"	Op_Construct = 10,\n"
			"	Op_TerrainRemove = 10,\n"
			"	Op_TerrainAdd = 10,\n"
			"	IsUnlocked = true,\n"
			"	},\n"
			"	Mechanic =\n"
			"	{\n"
			"	Cost = 30,\n"
			"	Weight = 2,\n"
			"	HealthMultiplier = 1,\n"
			"	ScanningRange = 3,\n"
			"	Op_Repair = 10,\n"
			"	IsUnlocked = true,\n"
			"	},\n"
			"	Raptor = \n"
			"	{\n"
				"	Cost = 80,\n"
				"	Weight = 10,\n"
				"	HealthMultiplier = 5,\n"
				"	ScanningRange = 7,\n"
				"	HorizontalAttackRange = 7,\n"
				"	GroundScanningRange = 7,\n"
				"	GroundScannerPenetration = 0,\n"
				"	ExplosionRadius = 1,\n"
				"	DamageLevel = 5,\n"
				"	AttacksPerRound = 2,\n"
				"	IsUnlocked = true,\n"
			"	},\n"
			"	CargoPlane = \n"
			"	{\n"
				"	Cost = 40,\n"
				"	Weight = 12,\n"
				"	HealthMultiplier = 15,\n"
				"	ScanningRange = 5,\n"
				"	GroundScanningRange = 2,\n"
				"	GroundScannerPenetration = 0,\n"
				"	IsUnlocked = true,\n"
			"	},\n"
			"	SensorDrone = \n"
			"	{\n"
				"	Cost = 10,\n"
				"	Weight = 11,\n"
				"	HealthMultiplier = 4,\n"
				"	ScanningRange = 10,\n"
				"	GroundScanningRange = 10,\n"
				"	GroundScannerPenetration = 2,\n"
				"	IsUnlocked = true,\n"
			"	},\n"
			"	}\n"
			"	VehicleBaseConfig = "
			"	{\n"
			"	Hoverbase = "
			"	{\n"
			"	Cost = 40,\n"
			"	MovementSpeed = 20,\n"
			"	CarryingCapacity = 1,\n"
			"	Health = 4,\n"
			"	OperationSpeedMultiplier = 1,\n"
			"	TurretRangeModifier = 0,\n"
			"	IsUnlocked = true,\n"
			"	},\n"
			"	HeavyBase = "
			"	{\n"
			"	Cost = 80,\n"
			"	MovementSpeed = 15,\n"
			"	CarryingCapacity = 10,\n"
			"	Health = 12,\n"
			"	OperationSpeedMultiplier = 2,\n"
			"	TurretRangeModifier = 0,\n"
			"	IsUnlocked = true,\n"
			"	},\n"
			"	CatamaranBase = "
			"	{\n"
			"	Cost = 100,\n"
			"	MovementSpeed = 8,\n"
			"	CarryingCapacity = 15,\n"
			"	Health = 12,\n"
			"	OperationSpeedMultiplier = 2,\n"
			"	TurretRangeModifier = 1,\n"
			"	IsUnlocked = true,\n"
			"	},\n"
			"	}\n"


			"function CanAttack(turret)\n"
			"return Get(turret.AttacksPerRound,nil) > 0\n"
			"end\n"

			"function TowerScanningModifier(turret)\n"
			"if CanAttack(turret) then\n"
			"return -2.0	--penalty for attaching towers\n"
			"end\n"
			"return 2.0	--boost non-attacking towers\n"
			"end\n"

			"function Tower(turret--[[Table]]) --Table\n"
			"rs =\n"
			"{\n"
			"Cost =\n"
			"{\n"
			"Get = "
			"function(state) "
			"return 40 + Get(turret.Cost,state)\n"
			"end,\n"
			"Describe = "
			"function(state) "
			"return '40+'.. Describe(turret.Cost,state)\n"
			"end,\n"
			"},\n"
			"Health = "
			"{\n"
			"Get = "
			"function(state) "
			"return DefaultHealthAtCost(rs.Cost.Get(state)) * Get(turret.HealthMultiplier,state) "
			"end,\n"
			"Describe = "
			"function(state) "
			"return Round(DefaultHealthAtCost(rs.Cost.Get(state)) * Get(turret.HealthMultiplier,state),2)..'[='.. DefaultHealthAtCost(rs.Cost.Get(state))..'*'.. Describe(turret.HealthMultiplier,state)..']';\n"
			"end,\n"
			"},\n"
			"ScanningRange =\n"
			"{\n"
			"Get = "
			"function(state) "
			"return Get(turret.ScanningRange,state) + TowerScanningModifier(turret)\n"
			"end,\n"
			"Describe = "
			"function(state) "
			"return Describe(turret.ScanningRange,state) ..'+'.. TowerScanningModifier(turret) "
			"end,\n"
			"},\n"
			"IsUnlocked = "
			"function(state) "
			"return Get(turret.IsUnlocked,state) "
			"end,\n"
			"}\n"
			"return rs;\n"
			"end\n"


			"StructureConfig =\n"
			"{\n"
			"	Mine = DefaultStructure(40),\n"
			"	Generator = DefaultStructure(40),\n"
			"	ReactorCore = DefaultStructure(120),\n"
			"	VehicleStore = DefaultStructure(40),\n"
			"	VehiclePlant = DefaultStructure(80),\n"
			"	PlaneFactory = DefaultStructure(200),\n"
			"	Laboratory = DefaultStructure(80),\n"
			"	MunitionFactory = DefaultStructure(40),\n"
			"	Connector = DefaultStructure(5),\n"
			"	FuelContainer = DefaultStructure(10),\n"
			"	MineralContainer = DefaultStructure(10),\n"
			"	UraniumStorage = DefaultStructure(10),\n"
			"	MunitionStorage = DefaultStructure(10),\n"
			"	NexusPort = ExplicitStructure(0,1,2,false),\n"
			"	MissileLauncherTower = Tower(TurretConfig.MissileLauncher),\n"
			"	RadarTower = Tower(TurretConfig.Radar),\n"
			"	CannonTower = Tower(TurretConfig.Cannon),\n"
			"}\n"
		);

		#ifdef _DEBUG
			ActivateProgram(globalState.program0);	//test fallback program
		#endif

		//.reset(new ScriptCache::Script());
		

		#if defined _DEBUG || !defined CLIENT
			LoadDefaultProgram();
		#endif
	}


	/*static*/ void			ResearchState::LoadDefaultProgram()
	{
		PathString factoryDefault = "../lua/research.lua";
		#if 0 && defined _DEBUG
			PathString final = factoryDefault;
		#else
			FileSystem::CreateDirectory(RuntimeConfig::ConfigFile("config/lua"));
			PathString final = RuntimeConfig::ConfigFile("config/lua/research.lua");
			if (!FileSystem::IsFile(final))
			{
				if (FileSystem::CopyFile("../lua/research.lua",final,false))
					LogEvent(EventClass::Preset::configuration,"Copied ../lua/research.lua to "+String(final));
				else
					FATAL__("Unable to copy ../lua/research.lua to "+String(final));
			}
		#endif
		globalState.factoryDefault = ScriptCache::Load(factoryDefault); 
		//#ifdef _DEBUG
		//	ActivateProgram(globalState.factoryDefault);
		//#endif
		ActivateProgram(ScriptCache::Load(final));
	}



	/*static*/ void		ResearchState::ActivateProgram(const ScriptCache::PScript&script)
	{
		//#if defined _DEBUG && defined HAS_WINDOW
		//	if (globalState.script)
		//		return;		//prevent reloading
		//#endif
		if (!script || script == globalState.script)
			return;
		LogEvent(EventClass::Preset::common,"Activating research script "+Bin2Str::ConvertPOD(script->identifier));


		ForeachFaction([](const PFaction&f)
		{
			f->researchState.Export(f->researchState.serialData);
			if (f->researchState.GetActiveTopic())
				f->researchState.lastTopic = f->researchState.GetActiveTopic()->GetPath();
			else
				f->researchState.lastTopic.free();
		});

		

		ScriptCache::PScript current = globalState.script;
		try
		{
			InnerLoad(script);
			return;
		}
		catch (const std::exception&e)
		{
			if (script->filename.IsNotEmpty())
				LogEvent(EventClass(EventSignificance::NonFatalError,EventFlavor::ServerPrivate),String(script->filename)+": "+e.what());
			else
				LogEvent(EventClass(EventSignificance::NonFatalError,EventFlavor::ServerPrivate),e.what());
		};

		if (current && current != globalState.program0)
		{
			LogEvent(EventClass(EventSignificance::NonFatalError,EventFlavor::ServerPrivate),"Falling back to previous script");
			RequireLoad(current);
		}
		else
		{
			if (script == globalState.factoryDefault)
			{
				LogEvent(EventClass(EventSignificance::NonFatalError,EventFlavor::ServerPrivate),"Unable to load factory default. Falling back to emergency script");
				RequireLoad(globalState.program0);
			}
			else
			{
				ASSERT__(globalState.factoryDefault);
				LogEvent(EventClass(EventSignificance::NonFatalError,EventFlavor::ServerPrivate),"Falling back to factory default script");
				ActivateProgram(globalState.factoryDefault);
			}
		}
	}

	/*static*/ void ResearchState::RequireLoad(const ScriptCache::PScript&script)
	{
		try
		{
			InnerLoad(script);
			return;
		}
		catch (const std::exception&e)
		{
			FATAL__("Fallback research script failed to load: "+String( e.what() ));
		};
	}

	/*static*/ void	ResearchState::InnerLoad(const ScriptCache::PScript&script)
	{
		if (!script)
			throw Lua::Exception(CLOCATION, Except::globalString("Script is NULL"));
		LogEvent(EventClass(EventSignificance::Common,EventFlavor::ServerPrivate),"Attempting to activate research program "+Bin2Str::ConvertPOD(script->identifier));
		globalState.rootResearch.Clear();
		globalState.allTopics.Clear();

		globalState.researchProgram = Lua::Program::Create();
		ASSERT__(globalState.researchProgram);
		{
			Lua::Table api = globalState.researchProgram->CreateTable();

			api.SetFunction("CreateResearch",CreateResearch);
			api.SetFunction("Log",LogLua);

			#ifdef LOWER_RESOLUTION_PLANET
				api.SetField("LowerResolutionPlanet",true);
			#else
				api.SetField("LowerResolutionPlanet",false);
			#endif
			api.SetField("UnitScale",Grid::UnitScale);

			api.SetGlobal("Game");
		}



		globalState.researchProgram->Execute(script->content,String(script->filename));

		Lua::Table researchTable = globalState.researchProgram->FindGlobal("ResearchState");
		if (researchTable.IsEmpty())
		{
			throw Lua::Exception("Table 'ResearchState' is not defined");
		}
		globalState.stateConstructor = researchTable.GetField("New");
		if (globalState.stateConstructor.IsEmpty())
		{
			throw Lua::Exception("Function 'ResearchState.New' is not defined");
		}

		globalState.exportState = researchTable.GetField("Export");
		if (globalState.exportState.IsEmpty())
		{
			throw Lua::Exception("Method 'ResearchState:Export' is not defined");
		}
		globalState.importState = researchTable.GetField("Import");
		if (globalState.importState.IsEmpty())
		{
			throw Lua::Exception("Method 'ResearchState:Import' is not defined");
		}

		
		for (index_t i = 0; i < structurePrototypes.Count(); i++)
		{
			const PStructure&s = structurePrototypes[i];
			BaseAttributeTable&t = structureTables[i];
			InitTable(s->GetTypeName(),t);
		}

		foreach (Component::allTurretComponents,turret)
			InitTurret(**turret);
		foreach (Component::baseComponents,base)
			InitVehicleBase(**base);


		ForeachFaction([](const PFaction&f)
		{
			f->researchState.researchState = globalState.stateConstructor();
			if (f->researchState.serialData.IsNotEmpty())
				f->researchState.Import(f->researchState.serialData);

			if (f->researchState.lastTopic.IsNotEmpty())
			{
				f->researchState.SetActiveTopic(FindTopic(f->researchState.lastTopic));
			}
		});
		globalState.script = script;

		#ifdef HAS_WINDOW
			Laboratory::ConfigureResearchButtons();
		#endif
		//globalState.researchProgram.reset();
	}

	ResearchState::ResearchState(Faction*owner, bool isNexus):owner(owner)
	{
		factionIsNexus = isNexus;
		if (globalState.script)
			researchState = globalState.stateConstructor();

	}
	
	bool					ResearchGroup::Insert(const PResearchTopic&t)
	{
		if (Find(t->name))
			return false;
		map.Set(t->name,t);
		children << t;
		return true;
	}


	PResearchTopic			ResearchGroup::Find(const StringRef&name) const
	{
		//PResearchTopic t;
		//map.Query(name,t);
		//return t;
		foreach (children,c)
			if ((*c)->name == name)	//i suspect this is actually faster for slow numbers of topics per family, since it does not force a String copy
				return *c;
		return PResearchTopic();
	}


	/*static*/ PResearchTopic	ResearchState::FindTopic(const String&path)
	{
		Array<StringRef>	components;
		explode('/',path,components);
		PResearchTopic	current = globalState.rootResearch.Find(components.First());
		for (index_t i = 1; i < components.Count(); i++)
		{
			if (!current)
				return PResearchTopic();
			current = current->Find(components[i]);
		}
		return current;
	}

	/*static*/ PResearchTopic	ResearchState::GetTopic(index_t serialIndex)
	{
		return serialIndex < globalState.allTopics.Count() ? globalState.allTopics[serialIndex] : PResearchTopic();
	}

	bool	ResearchState::SetActiveTopic(const PResearchTopic&active)
	{
		if (!active || !active->CanResearch(researchState))
			return false;
		activeResearch = active;
		return true;
	}


	void					ResearchState::FinalizeRound(const PFaction&f)
	{
		if (activeResearch && totalContribution)
		{
			int left;
			int current = activeResearch->getProgress(researchState);
			if (activeResearch->maxProgress >= 0)
			{
				left = activeResearch->maxProgress - current;
				if (left <= 0)
					return;
				totalContribution = vmin(totalContribution,left);
			}
			int next = current + totalContribution;
			activeResearch->setProgress(researchState,next);
			ASSERT_EQUAL__(next,activeResearch->getProgress(researchState));
			changed = true;
			if (activeResearch->maxProgress >= 0 && next >= activeResearch->maxProgress)
			{
				activeResearch.reset();
			}
		}
		totalContribution = 0;
		BaseAttributeCache::versionCounter++;
	}

	void					ResearchState::Import(const ArrayRef<const BYTE>&serializedState)
	{
		serialData = serializedState;
		if (!globalState.researchProgram)
		{
			return;
		}
		try
		{
			#ifdef CLIENT
				if (owner == Faction::mine.get())
					ForeachWorldEntity([this](const PWorldEntity&e)
					{
						if (e->owner.get() == owner)
						{
							float t = 0;
							e->UnregisterSensory(t,false);
						}
					});
			#endif

			Lua::Reference deserialized = globalState.researchProgram->DeserializeToReference(ByteReadStream(serializedState));
			globalState.importState(researchState,deserialized);
			BaseAttributeCache::versionCounter++;
		}
		catch (const std::exception&e)
		{
			LogEvent(EventClass::Preset::error,e.what());
		}

		#ifdef CLIENT
			if (owner == Faction::mine.get())
				ForeachWorldEntity([this](const PWorldEntity&e)
				{
					if (e->owner.get() == owner)
					{
						float t = 0;
						e->RegisterSensory(t,false);
					}
				});
		#endif

	}
	
	
	void					ResearchState::Export(Array<BYTE>&outSerializedState) const
	{
		//Buffer0<BYTE> data;
		if (globalState.exportState.IsEmpty() || researchState.IsEmpty())
		{
			outSerializedState.Clear();
			return;
		}
		try
		{
			Lua::Reference exported = globalState.exportState(researchState);
			ASSERT__(exported.IsSet());
			ByteStream stream;
			ASSERT__(exported.Serialize(stream));
			stream.CopyToArray(outSerializedState);
		}
		catch (const std::exception&e)
		{
			LogEvent(EventClass::Preset::error,e.what());
			outSerializedState.Clear();
		}
	}



}


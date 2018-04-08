#include <global_root.h>
#include "research.h"
#include "prototypes.h"
#include "structure.h"
#include "../logEvent.h"
#include "../scriptCache.h"
#include "faction.h"
#include "../runtimeConfig.h"
#include "../luaGameAPI.h"
#include <io/file_system.h>
#include "structure/laboratory.h"
#include <io/byte_stream.h>
#include "../expectedBehavior.h"

namespace Objects
{



	/*static*/ ResearchState::GlobalState		ResearchState::globalState;


	int						ResearchTopic::GetProgress(const ResearchState&st) const
	{
		return st.topicProgressVector[serialIndex];
	}

	/*static*/ PResearchTopic	ResearchState::CreateResearch(const PResearchTopic&parent, const String&name, const String&iconName, 
												const ResearchTopic::FSetProgress&setProgress, 
												int maxProgress,
												const PResearchTopic&dependency
											)
	{
		LogEvent(EventClass::Preset::debug,"Lua created new research '"+name+"'");

		const String&iconSource = iconName.IsEmpty() ? name : iconName;

		PWorldEntity proto = Prototypes::all.FindByName(iconSource);
		if (proto)
			ASSERT__(!proto->owner);	//this should never be true, but i'm growing paranoid
		Component::Base*component = proto ? nullptr : Component::allComponents.FindByName(iconSource);
		bool hasIcon = !proto && !component;


		PResearchTopic rs(new ResearchTopic(parent, name,hasIcon ? iconSource : "",setProgress,maxProgress,dependency));

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
			if (dependency->maxProgress <= 0)
			{
				throw Lua::Exception("Trying to register research dependency of '"+name+"' to unresearchable topic '"+dependency->name+"'");
			}

		}
		return rs;
	}

	ResearchTopic::ResearchTopic(const PResearchTopic&parent, const String&name, const String&iconName, 
								const FSetProgress&setProgress, 
								int maxProgress,
								const PResearchTopic&dependency
								)
								:Super(name), parent(parent),iconName(iconName),
								setProgress(setProgress),maxProgress(maxProgress),
								dependency(dependency)
	{}



	ResearchTopic::ResearchTopic::~ResearchTopic()
	{
		bool brk = true;

	}

	bool			ResearchTopic::CanResearch(const PFaction&owner) const
	{
		if (IsGroup())
			return false;
		//auto st = owner ? owner->researchState.researchState : Lua::Reference();
		if (owner && ResearchCompleted(owner->researchState.topicProgressVector))
			return false;
		PResearchTopic p = dependency.lock();
		if (!p)
			return true;
		if (!owner)
			return false;
		return p->ResearchCompleted(owner->researchState.topicProgressVector) || owner->researchState.GetResearchQueue().Contains(p);
	}





	static Array<BaseAttributeTable>	structureTables;


	template <typename T>
		class LuaAttributes
		{
		private:
			struct ConstOverride
			{
				T			constValue;
				bool		isSet = false;
			};

			struct Attribute
			{
				Lua::Function<T(Lua::Reference)>		luaGetValue;
				Lua::Function<String(Lua::Reference)>	luaDescribeValue;
				ConstOverride							constOverride;

				void		ExportTo( const Lua::Reference&state, ResearchAttributeTable::Attrib<T>&outAttrib ) const
				{
					if (constOverride.isSet)
						outAttrib.value = constOverride.constValue;
					else
					{
						if (luaGetValue.IsEmpty())
							outAttrib.value = (T)0;
						outAttrib.value = luaGetValue(state);
					}

					if (luaDescribeValue.IsSet())
						outAttrib.description = luaDescribeValue(state);
					else
					{
						using StringConversion::ToString;
						outAttrib.description = ToString(outAttrib.value);
					}
				}

				void	SetConst( T value )
				{
					constOverride.constValue = value;
					constOverride.isSet = true;
				}

				bool	IsSet() const
				{
					return luaGetValue.IsSet() || constOverride.isSet;
				}

				bool	IsFunctionOrConstNonZero() const
				{
					return constOverride.isSet ? constOverride.constValue != 0 : luaGetValue.IsSet();
				}

				void	swap(Attribute&other)
				{
					using std::swap;
					swap(constOverride,other.constOverride);
					luaGetValue.swap(other.luaGetValue);
					luaDescribeValue.swap(other.luaDescribeValue);
				}

				friend void swap(Attribute&a, Attribute&b)
				{
					a.swap(b);
				}


			};

		public:

			template <typename F>
				static void Set(const Lua::Table&source, const String&path, const String&name, LuaAttribute<T,F>&a, bool allowEmpty = false)
				{
					Lua::NamedReference ref = source.GetField(name);

					a.Clear(list.Count());
					auto&attrib = list.Append();
					if (ref.IsEmpty())
					{
						if (!allowEmpty)
							throw Lua::Exception("Field '"+path+"."+name+"' is not defined");
						return;
					}

					if (ref.IsTable())
					{
						Lua::Table t = ref;
			
						attrib.luaGetValue = t.GetField("Get");
						if (attrib.luaGetValue.IsEmpty())
							throw Lua::Exception("Function '"+path+"."+name+".Get' is not defined");

						attrib.luaDescribeValue = t.GetField("Describe");
					}
					elif (ref.IsFunction())
					{
						attrib.luaGetValue = ref;
					}
					else
					{
						attrib.SetConst(ref.GetAs<T>());
					}
				}

			static void Clear()
			{
				list.Clear();
			}

			static bool IsSet( index_t idx )
			{
				return list[idx].IsSet();
			}

			static bool IsFunctionOrConstNonZero( index_t idx )
			{
				return list[idx].IsFunctionOrConstNonZero();
			}

			static void ExportTo( const Lua::Reference&state, Array<ResearchAttributeTable::Attrib<T> >&outValues )
			{
				outValues.SetSize(list.Count());
				for (index_t i = 0; i < list.Count(); i++)
					list[i].ExportTo(state,outValues[i]);
			}

		private:





			static Buffer0<Attribute,Swap>	list;

		};

	template <typename T>
		/*static*/ Buffer0<typename LuaAttributes<T>::Attribute,Swap>	LuaAttributes<T>::list;

	template <typename T, typename F>
		static void InitAttrib(const Lua::Table&source, const String&path, const String&name, LuaAttribute<T,F>&a, bool allowEmpty = false)
		{
			LuaAttributes<T>::Set(source,path,name,a,allowEmpty);
		}



	/*static*/ bool			ResearchState::LuaIntAttributeIsSet( index_t idx )
	{
		return LuaAttributes<int>::IsSet(idx);
	}

	/*static*/ bool			ResearchState::LuaBoolAttributeIsSet( index_t idx )
	{
		return LuaAttributes<bool>::IsSet(idx);
	}

	/*static*/ bool			ResearchState::LuaFloatAttributeIsSet( index_t idx )
	{
		return LuaAttributes<float>::IsSet(idx);
	}

	/*static*/ bool			ResearchState::LuaIntAttributeIsFunctionOrConstNonZero( index_t idx )
	{
		return LuaAttributes<int>::IsFunctionOrConstNonZero(idx);
	}

	/*static*/ bool			ResearchState::LuaBoolIsFunctionOrConstNonZero( index_t idx )
	{
		return LuaAttributes<bool>::IsFunctionOrConstNonZero(idx);
	}

	/*static*/ bool			ResearchState::LuaFloatAttributeIsFunctionOrConstNonZero( index_t idx )
	{
		return LuaAttributes<float>::IsFunctionOrConstNonZero(idx);
	}

	/*static*/ void ResearchState::UpdateAttributeTable( const Lua::Reference&luaResearchState, ResearchAttributeTable&table )
	{
		LuaAttributes<int>::ExportTo(luaResearchState,table.intAttributes);
		LuaAttributes<float>::ExportTo(luaResearchState,table.floatAttributes);
		LuaAttributes<bool>::ExportTo(luaResearchState,table.boolAttributes);
	}

	void ResearchState::UpdateAttributeTable()
	{
		UpdateAttributeTable(luaResearchState,attributeTable);
		changed = true;
	}


	static void InitTable(String baseName, BaseAttributeTable&t)
	{
		baseName.EraseCharacter(' ');
		baseName.EraseCharacter('-');
		
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
		baseName.EraseCharacter(' ');
		baseName.EraseCharacter('-');

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
		structureTables.SetSize(Prototypes::structures.Count());
		for (index_t i = 0; i < Prototypes::structures.Count(); i++)
		{
			const PStructure&s = Prototypes::structures[i];
			BaseAttributeTable&t = structureTables[i];
			s->DefineAttributeTable(&t);
		}

		globalState.program0 = ScriptCache::LoadContent(
			"ResearchState = {}\n"
			"function ResearchState.New()\n"
				"local rs = {}\n"
				"setmetatable(rs,ResearchState)\n"
				"return rs\n"
			"end\n"
			"function ResearchState.Export(self)\n"
				"return 0\n"
			"end\n"
			"function ResearchState.Import(self, exported)\n"
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
			"	RocketLauncher = "
			"	{\n"
			"	Cost = 15,\n"
			"	Weight = 3,\n"
			"	HealthMultiplier = 2,\n"
			"	ScanningRange = 5,\n"
			"	AttacksPerRound = 2,\n"
			"	DamageLevel = 7,\n"
			"	ExplosionRadius = 1,\n"
			"	HorizontalAttackRange = 5,\n"
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
			"	Laboratory = DefaultStructure(80),\n"
			"	MunitionFactory = DefaultStructure(40),\n"
			"	Connector = DefaultStructure(5),\n"
			"	FuelContainer = DefaultStructure(10),\n"
			"	MineralContainer = DefaultStructure(10),\n"
			"	UraniumStorage = DefaultStructure(10),\n"
			"	MunitionStorage = DefaultStructure(10),\n"
			"	NexusPort = ExplicitStructure(0,1,2,false),\n"
			"	MissileLauncherTower = Tower(TurretConfig.MissileLauncher),\n"
			"	RocketLauncherTower = Tower(TurretConfig.RocketLauncher),\n"
			"	RadarTower = Tower(TurretConfig.Radar),\n"
			"	CannonTower = Tower(TurretConfig.Cannon),\n"
			"	SurveyorTower = Tower(TurretConfig.Surveyor),\n"
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
		PathString factoryDefault = "../lua/research.v2.lua";
		#if 0 && defined _DEBUG
			PathString final = factoryDefault;
		#else
			FileSystem::CreateDirectory(RuntimeConfig::myConfig.Lookup("config/lua"));
			FileSystem::CreateDirectory(RuntimeConfig::sharedConfig.Lookup("config/lua"));
			PathString final = RuntimeConfig::myConfig.Lookup("config/lua/research.v2.lua");
			if (!FileSystem::IsFile(final))
				final = RuntimeConfig::sharedConfig.Lookup("config/lua/research.v2.lua");
			if (!FileSystem::IsFile(final))
			{
				final = factoryDefault;
				#if 0	//leave this disabled. do not copy automatically
					if (FileSystem::CopyFile("../lua/research.lua",final,false))
						LogEvent(EventClass::Preset::configuration,"Copied ../lua/research.lua to "+String(final));
					else
						FATAL__("Unable to copy ../lua/research.lua to "+String(final));
				#endif /*0*/
			}
		#endif
		globalState.factoryDefault = ScriptCache::Load(factoryDefault); 
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

	/*static*/ void				ResearchState::BeginServerTopicUpdate()
	{
		globalState.rootResearch.Clear();
		globalState.allTopics.Clear();
		globalState.researchProgram.reset();
	}

	/*static*/ void				ResearchState::DeclareServerTopic(index_t parentTopicIndex, const String&topicName, const String&iconName, int maxProgress, index_t dependentTopicIndex)
	{
		CreateResearch(parentTopicIndex != InvalidIndex ? Expect::SafeGet(globalState.allTopics,parentTopicIndex,CLOCATION) : nullptr,topicName,iconName,ResearchTopic::FSetProgress(),maxProgress,dependentTopicIndex != InvalidIndex ? Expect::SafeGet(globalState.allTopics,dependentTopicIndex,CLOCATION):nullptr);
	}


	/*static*/ void	ResearchState::InnerLoad( const ScriptCache::PScript&script )
	{
		if (!script)
			throw Lua::Exception(CLOCATION, Except::globalString("Script is NULL"));
		LogEvent(EventClass(EventSignificance::Common,EventFlavor::ServerPrivate),"Attempting to activate research program "+Bin2Str::ConvertPOD(script->identifier));


		Ctr::Buffer0<TSerialState,Swap> taskLists;
		ForeachFaction([&taskLists](const PFaction&f)
		{
			auto&entry = taskLists.Append();
			f->researchState.ExportLuaState(entry.serialState);
			entry.researchQueue = f->researchState.ExportResearchQueue();
		});




		globalState.rootResearch.Clear();
		globalState.allTopics.Clear();

		globalState.researchProgram = Lua::Program::Create("Research");
		ASSERT__(globalState.researchProgram);
		{
			Lua::Table api = Lua::CreateGeneralAPI(globalState.researchProgram);
			api.SetFunction("CreateResearch",CreateResearch);
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

		LuaAttributes<int>::Clear();
		LuaAttributes<bool>::Clear();
		LuaAttributes<float>::Clear();

		for (index_t i = 0; i < Prototypes::structures.Count(); i++)
		{
			const PStructure&s = Prototypes::structures[i];
			BaseAttributeTable&t = structureTables[i];
			InitTable(s->GetTypeName(),t);
		}

		foreach (Component::allTurretComponents,turret)
			InitTurret(**turret);
		foreach (Component::baseComponents,base)
			InitVehicleBase(**base);


		index_t at = 0;
		ForeachFaction([&at,&taskLists](const PFaction&f)
		{
			const auto&entry = taskLists[at];
			f->researchState.luaResearchState = globalState.stateConstructor();
			if (entry.serialState.IsNotEmpty())
				f->researchState.ImportLuaState(entry.serialState);
			else
				f->researchState.UpdateAttributeTable();
			f->researchState.RestoreResearchQueue(entry.researchQueue);
			at++;
		});
		{
			globalState.zeroState = globalState.stateConstructor();
			UpdateAttributeTable(globalState.zeroState,globalState.zeroTable);
			globalState.zeroTableChanged = true;
		}
		globalState.script = script;

		#ifndef CLIENT
			if (Faction::nexus)
				Faction::nexus->researchState.LoadNexusResearchState();
		#endif

		globalState.onScriptChange();
		//globalState.researchProgram.reset();
	}

	void ResearchState::LoadNexusResearchState()
	{
		foreach (globalState.allTopics,_t)
		{
			auto&t = **_t;
			if (t.setProgress.IsSet())
			{
				if (t.maxProgress > 0)
					t.setProgress(luaResearchState,t.maxProgress);
			}
		}
		changed = true;
	}

	ResearchState::ResearchState(Faction*owner, bool isNexus):owner(owner)
	{
		factionIsNexus = isNexus;
		if (globalState.script)
		{
			luaResearchState = globalState.stateConstructor();
			if (isNexus)
				LoadNexusResearchState();
		}

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


	static PResearchTopic FindTopicOfPrototypeIn(const PResearchTopic&topic, const PWorldEntity&proto)
	{
		foreach (topic->children,_c)
		{
			const auto&t = *_c;
			if (t->IsGroup())
			{
				const auto&rs = FindTopicOfPrototypeIn(t,proto);
				if (rs)
					return rs;
			}
			if (t->prototypeReference == proto)
				return t;
		}
		return nullptr;
	}

	/*static*/ PResearchTopic	ResearchState::FindTopicOfPrototype(const PWorldEntity&proto)
	{
		foreach (globalState.allTopics,_t)
		{
			const auto&t = *_t;
			if (t->IsGroup())
			{
				const auto&rs = FindTopicOfPrototypeIn(t,proto);
				if (rs)
					return rs;
			}
			if (t->prototypeReference == proto)
				return t;
		}
		return nullptr;
	}


	/*static*/ PResearchTopic	ResearchState::FindTopic(const StringRef&path)
	{
		Ctr::Array<StringRef>	components;
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

	bool	ResearchState::ResearchIsPlanned(const PResearchTopic&topic) const
	{
		return researchQueue.Contains(topic);
	}

	bool	ResearchState::ResearchQueueIsFull() const
	{
		return researchQueue.Count() >= MaxResearchQueueLength;
	}

	bool	ResearchState::AppendResearchTask(const PResearchTopic&active)
	{
		if (!active || ResearchQueueIsFull() || !active->CanResearch(topicProgressVector,researchQueue.ToRef()) || researchQueue.Contains(active))
			return false;
		researchQueue << active;
		return true;
	}

	bool					ResearchState::DropResearchTask(const PResearchTopic&active)
	{
		if (!active)
			return false;
		index_t at = researchQueue.GetIndexOf(active);
		if (at == InvalidIndex)
			return false;
		researchQueue.Erase(at);
		for (index_t i = at; i < researchQueue.Count();)
		{
			if (!researchQueue[i]->CanResearch(topicProgressVector,researchQueue.SubRef(0,i)))
			{
				LogEvent(EventClass::Preset::operationAborted,owner->shared_from_this(),"Unable to continue research "+researchQueue[i]->GetFullName()+". Preconditions lost. Aborting operation");
				researchQueue.Erase(i);
			}
			else
				i++;
		}
		return true;
	}


	void		ResearchState::RestoreResearchQueue(const String&source)
	{
		researchQueue.Clear();
		Ctr::Array<StringRef> paths;
		explode(';',source,paths);
		foreach (paths,p)
			AppendResearchTask(FindTopic(*p));
		globalState.onQueueChange(owner);
	}

	String		ResearchState::ExportResearchQueue()
	{
		if (researchQueue.IsEmpty())
			return "";
		StringBuffer buffer;
		foreach (researchQueue,q)
		{
			if (buffer.IsNotEmpty())
				buffer << ';';
			buffer << (*q)->GetPath();
		}
		return buffer.CopyToString();
	}

	void		ResearchState::ExportResearchQueue(Ctr::Array<UINT32>&outRS)
	{
		outRS.SetSize(researchQueue.Count());
		for (index_t i = 0; i < researchQueue.Count(); i++)
			outRS[i] = ((UINT32)(researchQueue[i]->serialIndex));
	}
	void					ResearchState::RestoreResearchQueue(const Ctr::Array<UINT32>&q)
	{
		researchQueue.Clear();
		foreach (q,e)
			AppendResearchTask(GetTopic(*e));
		globalState.onQueueChange(owner);
	}




	void					ResearchState::AdvanceNexusResearch()
	{
		DBG_ASSERT__(factionIsNexus);

		foreach (globalState.allTopics,_t)
		{
			auto&t = **_t;
			if (t.setProgress.IsSet())
			{
				if (t.maxProgress < 0)
					topicProgressVector[ t.serialIndex ] ++;
			}
		}
		UpdateAttributeTable();
	}

	void					ResearchState::FinalizeRound(const PFaction&f)
	{
		while (researchQueue.IsNotEmpty() && totalContribution)
		{
			int left;
			const auto&res = researchQueue.First();
			int&progress = topicProgressVector[ res->serialIndex ];
			if (res->maxProgress >= 0)
			{
				left = res->maxProgress - progress;
				if (left <= 0)	//already done...?
				{
					LogEvent(EventClass::Preset::important,owner->shared_from_this(),"Research "+res->name+" completed");
					res->setProgress(luaResearchState,res->maxProgress);
					researchQueue.Erase(0);
					queueChanged = true;
					globalState.onQueueChange(owner);
					continue;
				}
				changed = true;
				if (totalContribution < left)	//no chance to get done
				{
					progress += totalContribution;
					res->setProgress(luaResearchState,progress);
					break;
				}
				totalContribution -= left;
				res->setProgress(luaResearchState,res->maxProgress);
				LogEvent(EventClass::Preset::important,owner->shared_from_this(),"Research "+res->name+" completed");
				researchQueue.Erase(0);
				queueChanged = true;
				globalState.onQueueChange(owner);
				continue;
			}
			progress += totalContribution;
			res->setProgress(luaResearchState,progress);
			changed = true;

			if (researchQueue.Count() > 1)
			{
				LogEvent(EventClass::Preset::common,owner->shared_from_this(),"Multiple research topics defined after "+res->name+". Cycling");
				auto topic = res;
				researchQueue.Erase(0);
				researchQueue.Append(topic);
				queueChanged = true;
				globalState.onQueueChange(owner);
			}

			break;
		}
		lastSpeed = totalContribution;
		totalContribution = 0;
		UpdateAttributeTable();
	}

	void					ResearchState::ImportLuaState(const ArrayRef<const BYTE>&serializedState)
	{
		if (!globalState.researchProgram)
		{
			return;
		}
		try
		{
			#ifdef CLIENT
				World::actual.InvalidateSensory();
			#else
				owner->knownWorld.InvalidateSensory();
			#endif

			Lua::Reference deserialized = globalState.researchProgram->DeserializeToReference(ByteReadStream(serializedState));
			globalState.importState(luaResearchState,deserialized);
		}
		catch (const std::exception&e)
		{
			LogEvent(EventClass::Preset::error,e.what());
		}
		UpdateAttributeTable();
	}
	
	
	void					ResearchState::ExportLuaState(Array<BYTE>&outSerializedState) const
	{
		//Buffer0<BYTE> data;
		if (globalState.exportState.IsEmpty() || luaResearchState.IsEmpty())
		{
			outSerializedState.Clear();
			return;
		}
		try
		{
			Lua::Reference exported = globalState.exportState(luaResearchState);
			ASSERT__(exported.IsSet());
			ByteStreamBuffer stream;
			exported.Serialize(stream);
			outSerializedState = stream.GetWrittenData();
		}
		catch (const std::exception&e)
		{
			LogEvent(EventClass::Preset::error,e.what());
			outSerializedState.Clear();
		}
	}



}


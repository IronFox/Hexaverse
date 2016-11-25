#include <global_root.h>
#include "research.h"
#include "prototypes.h"
#include "structure.h"
#include "../logEvent.h"

namespace Objects
{



	/*static*/ ResearchState::GlobalState		ResearchState::globalState;



	ResearchState::Research::Research(const PResearch&parent, const String&name, const String&iconName, const FOnActivate&onActivate):
		name(name),iconName(iconName.IsNotEmpty() ? iconName : name),parent(parent),onActivate(onActivate)
	{

	}

	ResearchState::Research::~Research()
	{
		bool brk = true;

	}

	ResearchState::PResearch ResearchState::CreateResearch(const PResearch & parent, const String & name, const String & icon, const FOnActivate&onActivate)
	{
		LogEvent(EventClass::Preset::important,"Lua created new research '"+name+"'");
		PResearch rs(new Research(parent, name,icon,onActivate));
		if (!parent)
			globalState.rootResearch << rs;
		return rs;
	}



	static Array<BaseAttributeTable>	structureTables;


	template <typename T>
		static void InitAttrib(const Lua::Table&source, const String&path, const String&name, LuaAttribute<T>&a, bool allowEmpty = false)
		{
			Lua::Reference ref = source.GetField(name);
			if (ref.IsEmpty())
			{
				if (!allowEmpty)
					FATAL__("research.lua does not define "+path+"."+name);
				return;
			}

			if (ref.IsTable())
			{
				Lua::Table t = source.GetField(name);
			
				a.luaGetValue = t.GetField("Get");
				if (a.luaGetValue.IsEmpty())
					FATAL__("research.lua does not define "+path+"."+name+".Get");

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
				FATAL__("research.lua does not define StructureConfig table");
		}

		Lua::Table source = configTable.GetField(baseName);
		if (source.IsEmpty())
			FATAL__("research.lua does not define StructureConfig."+baseName+" table");

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
			FATAL__("research.lua does not define "+path+" table");
		InitAttrib(baseTable,path,"Cost",b.productionCosts);
		return baseTable;
	}

	static void InitTurret(Component::Turret&t)
	{
		static Lua::Table configTable;
		if (configTable.IsEmpty())
		{
			configTable = ResearchState::globalState.researchProgram->FindGlobal("TurretConfig");
			if (configTable.IsEmpty())
				FATAL__("research.lua does not define TurretConfig table");
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
			InitAttrib(myTable,path,"DamageLevel",t.damageLevel);
			InitAttrib(myTable,path,"ExplosionRadius",t.explosionRadius);
			InitAttrib(myTable,path,"HorizontalAttackRange",t.horizontalAttackRange);
		}
	}


	/*static*/ void		ResearchState::StaticCreate()
	{
		globalState.researchProgram = Lua::Program::Create();
		ASSERT__(globalState.researchProgram);
		{
			Lua::Table api = globalState.researchProgram->CreateTable();

			api.SetFunction("CreateResearch",CreateResearch);
			api.SetGlobal("API");
		}



		try
		{
			globalState.researchProgram->ExecuteFile("../lua/research.lua");
		}
		catch (const std::exception&e)
		{
			FATAL__(e.what());
		}
		Lua::Table researchTable = globalState.researchProgram->FindGlobal("ResearchState");
		if (researchTable.IsEmpty())
		{
			FATAL__("Research Lua script does not define ResearchState class");
		}
		globalState.stateConstructor = researchTable.GetField("New");
		if (globalState.stateConstructor.IsEmpty())
		{
			FATAL__("Research Lua script does not define ResearchState.New method");
		}

		
		structureTables.SetSize(structurePrototypes.Count());
		for (index_t i = 0; i < structurePrototypes.Count(); i++)
		{
			const PStructure&s = structurePrototypes[i];
			BaseAttributeTable&t = structureTables[i];
			InitTable(s->GetTypeName(),t);
			s->DefineAttributeTable(&t);
		}

		foreach (Component::allTurretComponents,turret)
		{
			InitTurret(**turret);
		}


		//globalState.researchProgram.reset();
	}


	ResearchState::ResearchState(const TUniqueID&id, bool isNexus)
	{
		factionID = id;
		factionIsNexus = isNexus;

		researchState = globalState.stateConstructor();

	}







}


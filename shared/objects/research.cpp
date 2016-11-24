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
		static void InitAttrib(const Lua::Table&source, const String&path, const String&name, LuaAttribute<T>&a)
		{
			Lua::Reference ref = source.GetField(name);
			if (ref.IsEmpty())
				FATAL__("research.lua does not define "+path+"."+name);

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


		//globalState.researchProgram.reset();
	}


	ResearchState::ResearchState(const TUniqueID&id, bool isNexus)
	{
		factionID = id;
		factionIsNexus = isNexus;

		researchState = globalState.stateConstructor();

	}







}


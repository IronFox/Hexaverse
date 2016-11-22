#include <global_root.h>
#include "research.h"
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
		globalState.researchProgram.reset();
	}


	ResearchState::ResearchState(const TUniqueID&id, bool isNexus)
	{
		factionID = id;
		factionIsNexus = isNexus;

		researchState = globalState.stateConstructor();

	}







}


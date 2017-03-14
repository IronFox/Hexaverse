#include <global_root.h>
#include "luaGameAPI.h"
#include "logEvent.h"
#include "sphereGrid.h"

namespace Lua
{
	namespace Detail
	{
		void LogLua(const String&msg)
		{
			LogEvent(EventClass::Preset::important,"Lua: "+msg);
		}
	}


	void DefineGeneralAPI(Lua::Table&api)
	{
		api.SetFunction("Log",Detail::LogLua);

		#ifdef LOWER_RESOLUTION_PLANET
			api.SetField("LowerResolutionPlanet",true);
		#else
			api.SetField("LowerResolutionPlanet",false);
		#endif
		api.SetField("UnitScale",Grid::UnitScale);
	}

	Lua::Table CreateGeneralAPI(const Lua::PProgram&p)
	{
		Lua::Table api = p->CreateTable();
		DefineGeneralAPI(api);
		api.SetGlobal("Game");
		return api;
	}

}


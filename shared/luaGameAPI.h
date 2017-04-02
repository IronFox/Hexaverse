#pragma once

#include "luaInterface.h"
#include "objects/types.h"

namespace Lua
{
	/**
	Creates and registers the global 'Game' table which contains default functions for lua to use
	*/
	Lua::Table CreateGeneralAPI(const Lua::PProgram&);


	void		EntityToTable(const Objects::PWorldEntity&, Lua::Table&);
}


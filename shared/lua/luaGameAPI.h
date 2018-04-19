#pragma once

#include "luaInterface.h"
#include "objects/types.h"

namespace Lua
{
	/**
	Creates and registers the global 'Game' table which contains default functions for lua to use
	*/
	Lua::Table	CreateGeneralAPI(const Lua::PProgram&);

	/**
	Constructs the low-resolution planar grid across the planet in Lua:
		Cell[]		*.lowCells		Ordered table of all planar cells in the world, with:
		Cell
		{
			Vector	center;			Normalized center vector of the cell
			Cell[]	neighbors;		Ordered table of all planar neighbor cells
			int		index;			Linear index of this cell in Game.Cells in [1,#*.lowCells]
		};
		Vector
		{
			float	x,y,z;
		};
	*/
	void		DefineLowGridCells(Lua::Table&targetTable);

	void		EntityToTable(const Objects::PWorldEntity&, Lua::Table&);
}


#pragma once

#include <grid/types.h>

namespace ClientGrid
{
	void	Create();
	void	Destroy();
	void	Apply(const Grid::TSharedPlanetConfig&cfg);
}


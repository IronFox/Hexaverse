#pragma once

#include <grid/types.h>

namespace ClientGrid
{
	void	Create();
	void	Destroy();
	void	Apply(const Grid::TSharedPlanetConfig&cfg);

	/**
	Checks whether the current planet sensory is invalid and rebuilds it if necessary, using all entities owned by the local player
	*/
	void	CheckSensory();
}


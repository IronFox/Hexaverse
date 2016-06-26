#pragma once
#include <sphereGrid.h>


namespace ConfigurationMode
{
	void	Update(const Grid::ExtendedPlanetConfig&);
	void	SetActive(bool);
	bool	IsActive();
}

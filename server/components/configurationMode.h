#pragma once

#include <general/random.h>

namespace ConfigurationMode
{
	void	Begin();
	void	End();
	bool	IsActive();
	UINT	GetPlanetSeed();
	void	SetPlanetSeed(UINT32 seed);
}

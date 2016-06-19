#pragma once

#include <general/random.h>

namespace ConfigurationMode
{
	void	Begin();
	void	End();
	bool	IsActive();
	Random&	GetPlanetRandomVariable();
}

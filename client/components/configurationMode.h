#pragma once
#include <sphereGrid.h>


namespace ConfigurationMode
{
	void	Update(const Grid::ExtendedPlanetConfig&);
	void	SetActive(bool);
	bool	IsActive();
	void	RandomizeSeed();

	/**
	Clears all visual commands and rebuilds their configuration, depending on command group expansion,
	and only if configuration mode is active.
	Automatically called by UI::Configure().
	@return True if configuration mode is active, and visual commands were rebuilt, false otherwise.
	*/
	bool	ReconfigureVisualCommands();
}

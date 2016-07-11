#pragma once
#include <sphereGrid.h>
#include <runtimeConfig.h>

namespace ConfigurationMode
{
	void	Update(const Grid::ExtendedPlanetConfig&);
	void	SetActive(bool);
	void	SetGameFlags(UINT32, RuntimeConfig::GameGoal);
	bool	IsActive();
	void	RandomizeSeed();

	/**
	Clears all visual commands and rebuilds their configuration, depending on command group expansion,
	and only if configuration mode is active.
	Automatically called by UI::Configure().
	@return True if configuration mode is active, and visual commands were rebuilt, false otherwise.
	*/
	bool	ReconfigureVisualCommands();

	/**
	Assures that the server-received world is currently loaded and not any locally generated planet.
	Does nothing if this is already the case.
	Must be called before applying world deltas
	*/
	void	RestoreServerWorld();
	void	RestoreServerObjects();
}

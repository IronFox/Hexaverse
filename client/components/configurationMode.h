#pragma once

#ifdef HAS_WINDOW

#include <grid/planet.h>
#include <runtimeConfig.h>

namespace ConfigurationMode
{
	void	Create();
	void	Update(const Grid::ExtendedPlanetConfig&);
	void	SetActive(bool);
	void	SetGameConfig(UINT32, RuntimeConfig::GameGoal, Grid::WorldDiscovery);
	bool	IsActive();
	void	RandomizeSeed();

	UINT32	GetServerSeed();

	/**
	Clears all visual commands and rebuilds their configuration, depending on command group expansion,
	and only if configuration mode is active.
	Automatically called by UI::Configure().
	@return True if configuration mode is active, and visual commands were rebuilt, false otherwise.
	*/
	bool	ReconfigureVisualCommands();

	/**
	Executed if the user enters the Ctrl+C key combination
	*/
	void	OnCopy();
	
	/**
	Executed if the user enters the Ctrl+V key combination
	*/
	void	OnPaste();
}

#endif

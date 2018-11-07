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
	Rebuilds the local command configuration, depending on command group expansion,
	and only if configuration mode is active.
	Expects all commands to have been removed.
	Automatically called by UI::Configure(). Do not call manually, unless you know what you are doing.
	@return True if configuration mode is active, and visual commands were rebuilt, false otherwise.
	*/
	bool	DefineHexCommands();

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

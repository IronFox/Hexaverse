#pragma once

#ifdef HAS_WINDOW

#include <grid/planet.h>
#include <runtimeConfig.h>

namespace ConfigurationMode
{
	void	Update(const Grid::ExtendedPlanetConfig&);
	void	SetActive(bool);
	void	SetGameConfig(UINT32, RuntimeConfig::GameGoal, Grid::WorldDiscovery);
	bool	IsActive();
	bool	ViewingServerVersion();
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
	@return true if the server world is loaded, and updates may be applied. Returns true if configuration mode is currently inactive
	*/
	bool	RestoreServerWorld();
	void	RestoreServerObjects();

	/**
	Executed if the user enters the Ctrl+C key combination
	*/
	void	OnCopy();
	
	/**
	Executed if the user enters the Ctrl+V key combination
	*/
	void	OnPaste();

	/**
	Notifies the configuration mode of a new list of available planet presets
	The specified list @a fleetingData will be adopted and left empty
	*/
	void	UpdatePlanetList(Array<String>&fleetingData);

	void	Tick();
	void	SignalServerWorldUpdateCompleted();
}

#endif

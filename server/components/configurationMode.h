#pragma once

#include <general/random.h>

namespace ConfigurationMode
{
	/**
	If found inactive,
	closes the session (spectators remain),
	and generates and builds the configured world.
	Configuration mode becomes active.
	*/
	void	Begin();
	/**
	If found active, 
	flushes global objects, 
	shuts down the server, 
	flushes known clients, 
	builds the configured world, 
	and restarts the session.
	Configuration mode becomes inactive.
	*/
	void	End();
	/**
	If found active, restores the original faction flags, window title, and shuts off
	Configuration mode becomes inactive.
	This function is called when a new savegame is loaded.
	*/
	bool	Abort();
	bool	IsActive();
	UINT	GetPlanetSeed();
	void	SetPlanetSeed(UINT32 seed);
}

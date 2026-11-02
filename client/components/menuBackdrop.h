#pragma once

#ifdef HAS_WINDOW

/**
Renders a randomly chosen predefined planet behind the server list while the client is not connected.
The planet is generated locally, without any Nexus presence (no Nexus faction, buildings, or roads) and without EMP zones.
*/
namespace MenuBackdrop
{
	/**
	Registers the per-frame check that generates a new backdrop planet whenever the client is idle
	(not connected, not connecting, not in configuration mode) and none is currently shown.
	*/
	void	Create();

	/**
	Queries whether the local terrain currently holds a backdrop planet
	*/
	bool	IsShowing();

	/**
	Erases the backdrop planet, if any. Must be called before the client starts receiving terrain from a server,
	since the server assumes the client starts out with an empty terrain
	*/
	void	ClearIfShowing();
}

#endif

#pragma once
#include <globalEvents.h>
#include <objects/types.h>

DECLARE_POINTER_CLASS(Vote);


namespace GlobalEvents
{
	extern Observable<>	frameBegin;
	#ifdef HAS_WINDOW
		extern Observable<>	frameAfterCamera,
							shadowQualityChanged,
							uiScaleChanged;
	#endif

	/**
	Triggered when the client successfully connected to a client (before receiving any game data or making faction choices)
	*/
	extern Observable<>	serverConnectionEstablished;

	/**
	A previous network connection was closed or lost
	*/
	extern Observable<>	serverConnectionLost;
	/**
	The current network connection attempt failed
	*/
	extern Observable<>	serverConnectionFailed;

	/**
	The player is given control in a new round. Triggered exactly once per round, after all hardcoded effects have been executed
	*/
	extern Observable<>	playerControlBegin;

	/**
	Triggered when a previous initialized concurrent sensor painting operation has finished, and sensory is now valid
	*/
	extern Observable<>	gridSensoryValid;

	/**
	All server data has been received and the game may begin.
	Triggered when the respective server signal has been received
	*/
	extern Observable<>	beginGame;

	#ifdef HAS_WINDOW
		extern Observable<bool>	onFullscreenChange;
	#endif

	extern Observable<const Objects::PFaction&>	onAllianceChange;
	extern Observable<const PVote&> voteCreated, voteClosed, voteCast;
}


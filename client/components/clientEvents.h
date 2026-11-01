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
							windowSizeChanged,
							uiScaleChanged,
							buttonScaleChanged;
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
	The local client is given control in a new round. Triggered exactly once per round, 
	after all hardcoded effects have been executed, but before round is possibly ended by automatic processes
	*/
	extern Observable<>	clientControlBegin;

	/**
	The local player is given control in a new round. Triggered after clientControlBegin,
	after a possible automatic round progression has not happened early on.
	Late automatic round progression is not covered
	*/
	extern Observable<> playerControlBegin;

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


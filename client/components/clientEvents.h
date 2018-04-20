#pragma once
#include <globalEvents.h>
#include <objects/types.h>

DECLARE_POINTER_CLASS(Vote);


namespace GlobalEvents
{
	extern Observable<>	frameBegin,
						frameAfterCamera,
						shadowQualityChanged,
						uiScaleChanged;

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
	All server data has been received and the game may begin.
	Triggered when the respective server signal has been received
	*/
	extern Observable<>	beginGame;

	extern Observable<bool>	onFullscreenChange;

	extern Observable<const Objects::PFaction&>	onAllianceChange;
	extern Observable<const PVote&> voteCreated, voteClosed, voteCast;
}


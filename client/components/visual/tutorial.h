#pragma once

#ifdef HAS_WINDOW

#include <global_string.h>
#include <general/timer.h>

namespace Tutorial
{
	using namespace DeltaWorks;

	void	ShowMessage(const String&name, bool forceOpenNow=false, bool overrideMemory=false);
	void	HideMessage(const String&name);

	/**
	Permanently flushes the memory of any visited tutorial messages.
	Subsequently shown messages will be memorized again, however.
	*/
	void	FlushMemory();

	void	Create();

	extern Timer::Time	lastDelayingEvent;
	extern bool			movementTutorialIssued,
						endRoundTutorialIssued;


	/**
	Signals that the tutorial should (re)start.
	Call this when all player units have been loaded (e.g. the game has begun on a tutorial session)
	*/
	void	SignalStart();
	void	SignalRoundStart();
	void	SignalEnd();
	void	SignalResearchChange();
	void	SetProgress(int);

	void	StartTutorialServer();
}

#endif


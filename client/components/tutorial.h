#ifndef tutorialH
#define tutorialH

#ifdef HAS_WINDOW

#include <global_string.h>
#include <general/timer.h>

namespace Tutorial
{
	void	ShowMessage(const String&name, bool forceOpenNow=false);
	void	HideMessage(const String&name);

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
	void	SignalEntitySelectionChange(bool stateChangeOnly);
	void	SignalEnd();
	void	ProcessTriggers(float timeDelta);
}

#endif

#endif

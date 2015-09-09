#ifndef tutorialH
#define tutorialH

#ifdef HAS_WINDOW

#include <global_string.h>
#include <general/timer.h>

namespace Tutorial
{
	void	ShowMessage(const String&name);
	void	HideMessage(const String&name);

	void	Create();

	extern Timer::Time	lastDelayingEvent;
	extern bool			movementTutorialIssued,
						endRoundTutorialIssued;



}

#endif

#endif

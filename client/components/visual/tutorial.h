#pragma once

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



}

#endif


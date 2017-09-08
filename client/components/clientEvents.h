#pragma once
#include <globalEvents.h>


namespace GlobalEvents
{
	extern Observable<>	frameBegin,
						frameAfterCamera,
						shadowQualityChanged,
						uiScaleChanged;

	extern Observable<bool>	onFullscreenChange;


}


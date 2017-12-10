#pragma once
#include <globalEvents.h>
#include <objects/types.h>

namespace GlobalEvents
{
	extern Observable<>	frameBegin,
						frameAfterCamera,
						shadowQualityChanged,
						uiScaleChanged;

	extern Observable<bool>	onFullscreenChange;

	extern Observable<Objects::PFaction&>	onAllianceChange;
}


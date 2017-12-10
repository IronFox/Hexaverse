#include <global_root.h>

#include "clientEvents.h"


namespace GlobalEvents
{
	Observable<>	frameBegin,
					frameAfterCamera,
					shadowQualityChanged,
					uiScaleChanged;

	Observable<bool>	onFullscreenChange;

	Observable<Objects::PFaction&>	onAllianceChange;
}


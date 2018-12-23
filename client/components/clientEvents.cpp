#include <global_root.h>

#include "clientEvents.h"


namespace GlobalEvents
{
	Observable<>	frameBegin,
					serverConnectionEstablished,
					serverConnectionLost,
		serverConnectionFailed,
		clientControlBegin,
		playerControlBegin,
		beginGame,
		gridSensoryValid
		;
	#ifdef HAS_WINDOW
		Observable<>frameAfterCamera,
					shadowQualityChanged,
					windowSizeChanged,
					uiScaleChanged;

		Observable<bool>	onFullscreenChange;
	#endif

	Observable<const Objects::PFaction&>	onAllianceChange;
	Observable<const PVote&> voteCreated, voteClosed,voteCast;
}


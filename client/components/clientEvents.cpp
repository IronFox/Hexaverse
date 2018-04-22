#include <global_root.h>

#include "clientEvents.h"


namespace GlobalEvents
{
	Observable<>	frameBegin,
					serverConnectionEstablished,
					serverConnectionLost,
		serverConnectionFailed,
		playerControlBegin,
		beginGame,
		gridSensoryValid
		;
	#ifdef HAS_WINDOW
		Observable<>frameAfterCamera,
					shadowQualityChanged,
					uiScaleChanged;

		Observable<bool>	onFullscreenChange;
	#endif

	Observable<const Objects::PFaction&>	onAllianceChange;
	Observable<const PVote&> voteCreated, voteClosed,voteCast;
}


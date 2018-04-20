#include <global_root.h>

#include "clientEvents.h"


namespace GlobalEvents
{
	Observable<>	frameBegin,
					frameAfterCamera,
					shadowQualityChanged,
					uiScaleChanged,
					serverConnectionEstablished,
					serverConnectionLost,
		serverConnectionFailed,
		playerControlBegin,
		beginGame,
		gridSensoryValid
		;

	Observable<bool>	onFullscreenChange;

	Observable<const Objects::PFaction&>	onAllianceChange;
	Observable<const PVote&> voteCreated, voteClosed,voteCast;
}


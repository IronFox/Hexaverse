#pragma once
#include <globalEvents.h>
#include <objects/types.h>

DECLARE_POINTER_CLASS(Vote);


namespace GlobalEvents
{
	extern Observable<>	frameBegin,
						frameAfterCamera,
						shadowQualityChanged,
						uiScaleChanged;

	extern Observable<bool>	onFullscreenChange;

	extern Observable<const Objects::PFaction&>	onAllianceChange;
	extern Observable<const PVote&> voteCreated, voteClosed, voteCast;
}


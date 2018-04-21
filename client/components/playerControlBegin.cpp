#include "playerControlBegin.h"
#include "visual/hud/hexGrid.h"
#include "visual/tutorial.h"
#include "animation.h"
#include "clientEvents.h"
#include "objectNetworkHelper.h"

namespace PlayerControlBegin
{
	bool pauseAutomaticRoundProgression=false;
	bool checked=false;


	bool			SeeingHostileActivity()
	{
		#ifdef HAS_WINDOW
			return HUD::HexGrid::EventLog::HasRegisteredHostileActivity();
		#else
			return false;
		#endif
	}
		

	void CheckAutoNextRound()
	{
		if (!ObjectHelper::MarkerCheck::DoneThisRound())
			return;

		if (checked)
			return;

		checked = true;

		if (RuntimeConfig::autoNewRound && ApplicationInterface::ShouldSignalRoundEnd() && !pauseAutomaticRoundProgression && !SeeingHostileActivity())
		{
			ApplicationInterface::SignalRoundEnd();
		}

		pauseAutomaticRoundProgression = false;
	}


																
	/**
	Triggered when entering command mode and animations have finished (whichever of those events happens later)
	*/
	void	OnPlayerControlBegin()
	{
	//	LogEvent(EventSignificance::Debug,"OnPlayerControlBegin("+String(RuntimeConfig::roundNumber)+","+String(RuntimeConfig::techniteSubRoundNumber)+")");
		LogEvent(EventSignificance::Debug,"OnPlayerControlBegin("+RuntimeConfig::roundNumber.ToString()+")");

		#ifdef HAS_WINDOW
			GlobalEvents::invalidateObjectCache();
			HUD::HexGrid::RefreshButtonState();
		#endif
		#ifndef NO_SATELLITES
			Objects::RefreshSkyHubConnections();
		#endif
		Animation::UpdateAutomation(CLOCATION);
		Objects::CheckMobileEntitySanity();

		checked = false;

		CheckAutoNextRound();

		GlobalEvents::playerControlBegin();

		CheckAutoNextRound();
	}


}

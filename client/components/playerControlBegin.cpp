#include "playerControlBegin.h"
#include "visual/hud/hexGrid.h"
#include "visual/tutorial.h"
#include "animation.h"
#include "clientEvents.h"
#include "objectNetworkHelper.h"
#include "clientRuntimeConfig.h"
#include "scriptedAI.h"
#include "network.h"

namespace PlayerControlBegin
{
	bool pauseAutomaticRoundProgression=false;
	bool allowChecks = true;


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
		if (!allowChecks)
			return;

		if (!ApplicationInterface::AutomaticProcessesAreDoneThisRound())
			return;

		if (RuntimeConfig::autoNewRound && ApplicationInterface::ShouldSignalRoundEnd() && !pauseAutomaticRoundProgression && (RuntimeConfig::aiMode || !SeeingHostileActivity()))
			ApplicationInterface::SignalRoundEnd();
		elif (!RuntimeConfig::aiMode && ScriptedAI::DoneThisRound())
			allowChecks = false;	//don't auto-complete if player has assumed control

		#ifdef HAS_WINDOW
			HUD::HexGrid::RefreshButtonState();
			HUD::WindowStack::SignalSituationChange();
		#endif
	}


																
	/**
	Triggered when entering command mode and animations have finished (whichever of those events happens later)
	*/
	void	OnPlayerControlBegin()
	{
	//	LogEvent(EventSignificance::Debug,"OnPlayerControlBegin("+String(RuntimeConfig::roundNumber)+","+String(RuntimeConfig::techniteSubRoundNumber)+")");
		LogEvent(EventSignificance::Debug,"OnPlayerControlBegin("+RuntimeConfig::roundNumber.ToString()+")");

		allowChecks = false;

		#ifdef HAS_WINDOW
			GlobalEvents::invalidateObjectCache();
			HUD::HexGrid::RefreshButtonState();
		#endif
		#ifndef NO_SATELLITES
			Objects::RefreshSkyHubConnections();
		#endif
		Animation::UpdateAutomation(CLOCATION);
		Objects::CheckMobileEntitySanity();

		GlobalEvents::clientControlBegin();

		allowChecks = true;
		CheckAutoNextRound();

		if (!Network::newRoundRequested)
			GlobalEvents::playerControlBegin();
	}


}

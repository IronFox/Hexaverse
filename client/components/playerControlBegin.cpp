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
		
		/*
		problem:
			if not in AI mode (non-dedicated AI), there is a temporal gap between script completion and detection of automatic end round conditions (server-round-trip).
			if checking during this gap, further checks are prevented but new rounds are not requested
			if not preventing further checks here, the round will be instantly ended the moment the player has issued a command to the last idle entity
			only way around is to detect all user interactions (everything that is not purely display) and signal that automatic round end should end
			or add 1 second timeout. that seems to also work
		*/
		if (!RuntimeConfig::aiMode && ScriptedAI::SuccessfullyExecutedThisRound() && timer.GetSecondsSince(ScriptedAI::GetCompletionTime()) > 1.f)
		{
			allowChecks = false;	//don't auto-complete if player has assumed control
			return;
		}



		if (RuntimeConfig::autoNewRound && ApplicationInterface::ShouldSignalRoundEnd() && !pauseAutomaticRoundProgression && (RuntimeConfig::aiMode || !SeeingHostileActivity()))
		{
			allowChecks = false;	//don't redo this during this round
			ApplicationInterface::SignalRoundEnd();
		}
		elif (!RuntimeConfig::aiMode && !ScriptedAI::WillExecuteOnNewRound())
			allowChecks = false;	//don't auto-complete if player has assumed control and we failed here once already

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

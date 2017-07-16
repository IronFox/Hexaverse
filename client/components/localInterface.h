#pragma once

namespace LocalApplicationInterface
{
	extern bool 	pauseAutomaticRoundProgression;

	#ifdef HAS_WINDOW
		void		QueryNickname();
		bool		QueryNicknameIfEmpty();
		void		RefreshSessionView();
	#else

	#endif

	void			SignalMarkerDeletion(TUniqueID markerID);	//must be copy to prevent weird shit


	bool			SeeingHostileActivity();
																				
	/**
	Triggered when entering command mode and animations have finished (whichever of those events happens later)
	*/
	void			OnPlayerControlBegin();
}

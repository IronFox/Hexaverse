#ifndef componentsAudioIntefaceH
#define componentsAudioIntefaceH

#ifdef HAS_WINDOW

#include <engine/aspect.h>

namespace AudioInterface
{
	void	Create();
	void	Destroy();
	//void	UpdateListener(const Engine::Aspect<>&);
	void	SetMasterVolume(float volume);
	float	GetMasterVolume();

	bool	IsActive();

	void	SetMuteOnWindowFocusLoss(bool doMute);
	bool	MutingOnWindowFocusLoss();
}

#endif


#endif

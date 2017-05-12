#ifndef componentsAudioIntefaceH
#define componentsAudioIntefaceH

#ifdef HAS_WINDOW

#include <engine/aspect.h>

namespace AudioInterface
{
	extern bool	isActive;
	
	void	Create();
	void	Destroy();
	//void	UpdateListener(const Engine::Aspect<>&);
	void	SetMasterVolume(float volume);
}

#endif


#endif

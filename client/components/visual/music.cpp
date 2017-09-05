#include "music.h"
#include <al/musicPlayer.h>


namespace Music
{

	static bool	lastShown = false;

	Engine::OpenAL::MusicPlayer	player;


	void	SignalServerListShown(bool doShow)
	{
		if (doShow == lastShown)
			return;

		lastShown = doShow;

		if (doShow)
		{
			player.Begin("resources/audio/Hexaverse Title M4.mp3",true);
		}
		else
			player.FadeOut(2.f);

	}

}


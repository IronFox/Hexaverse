#include "music.h"
#include <al/musicPlayer.h>


namespace Music
{

	static bool	lastShown = false;

	Engine::OpenAL::MusicPlayer	player;

	float	musicVolume = 0.5f;

	float	GetVolume()
	{
		return musicVolume;
	}

	void	SetVolume(float v)
	{
		musicVolume = v;
		player.SetVolume(v);
	}


	void	SignalServerListShown(bool doShow)
	{
		if (doShow == lastShown)
			return;

		lastShown = doShow;

		if (doShow)
		{
			player.SetVolume(musicVolume);
			player.Begin("resources/audio/Hexaverse Title M4.mp3",true);
		}
		else
			player.FadeOut(5.f);

	}

}


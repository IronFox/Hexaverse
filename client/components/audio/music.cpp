#include "music.h"
#include <al/musicPlayer.h>


namespace Music
{

	static bool	lastShown = false;
	static bool musicDuringGameplay = false;
	static const DeltaWorks::PathString file = "resources/audio/Hexaverse Title M4.mp3";

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

	bool	PlayMusicDuringGameplay()
	{
		return musicDuringGameplay;
	}

	static void UpdatePlayback()
	{
		bool doPlay = lastShown || musicDuringGameplay;

		if (doPlay == player.IsPlaying())
			return;

		if (doPlay)
		{
			player.SetVolume(musicVolume);
			player.Begin(file,true);
		}
		else
			player.FadeOut(5.f);
	}


	void	SetPlayMusicDuringGameplay(bool play)
	{
		if (musicDuringGameplay == play)
			return;
		musicDuringGameplay = play;

		if (lastShown)
			return;	//nothing necessary

		UpdatePlayback();

	}



	void	SignalServerListShown(bool doShow)
	{
		if (doShow == lastShown)
			return;

		lastShown = doShow;

		UpdatePlayback();

	}

}


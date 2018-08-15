#include "newRoundSound.h"
#include <al/openal.h>
#include <logEvent.h>
#include <globalEvents.h>
#include <runtimeConfig.h>
#include "../clientEvents.h"
#include "../cameraControl.h"
#include "../network.h"

namespace NewRoundSound
{
	static bool isEnabled = false;

	static Engine::OpenAL::Player	player;
	static Engine::OpenAL::Wave	sound;
	static index_t registeredCallback = InvalidIndex;
	static index_t updatePositionCallback = InvalidIndex;

	bool		IsEnabled()
	{
		return isEnabled;

	}

	static void DoDisable()
	{
		player.Destroy();
		isEnabled = false;
		GlobalEvents::phaseChange.UnregisterAndSetInvalid(registeredCallback);
		GlobalEvents::frameAfterCamera.UnregisterAndSetInvalid(updatePositionCallback);
	}

	static void OnPhaseChange()
	{
		if (RuntimeConfig::phase == RuntimeConfig::Phase::CommandPhase && Network::haveFaction)
			if (!player.IsPlaying())
			{
				player.Rewind();
				player.Play();
			}
	}

	static void OnFrameBegin()
	{
		player.SetPosition(Camera::scene.GetAbsoluteLocation());
	}

	static void DoEnable()
	{
		if (sound.IsEmpty())
		{
			DeltaWorks::String error;
			sound = Engine::OpenAL::Load("resources/audio/newRound.wav",&error);
			if (sound.IsEmpty())
			{
				LogEvent(EventClass::Preset::fileError,"Unable to load new round sound: "+error);
				return;
			}
		}
		isEnabled = true;
		player.Create();
		player.SetVolume(0.5f);
		player.ReplaceWaveBuffer(sound);
		registeredCallback = GlobalEvents::phaseChange.Register(OnPhaseChange);
		updatePositionCallback = GlobalEvents::frameAfterCamera.Register(OnFrameBegin);

	}

	void		SetEnabled(bool doEnable)
	{
		if (doEnable == isEnabled)
			return;

		if (doEnable)
			DoEnable();
		else
			DoDisable();
	}

}

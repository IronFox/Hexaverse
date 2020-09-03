#include "hudAudio.h"

#include <al/openal.h>
#include <math/vclasses.h>

using namespace DeltaWorks;


namespace HUD
{
	namespace Audio
	{
		namespace AL = Engine::OpenAL;

		namespace Detail
		{
			AL::Wave buttonDown;
			//AL::Wave buttonUp;

		}

		
		void				PlayButtonDownSound(const M::TVec2<>&center)
		{
			AL::PlayOnce(float3(center.x*2-1,center.y*2-1,1),Detail::buttonDown,0.5,1,true);
		}

		void				PlayButtonUpSound(const M::TVec2<>&center)
		{
			//AL::PlayOnce(float3(center.x*2-1,center.y*2-1,1),Detail::buttonUp,0.5,1,true);
		}


		void	Create()
		{
			Detail::buttonDown = AL::Load("resources/audio/buttonDown.wav");
			//Detail::buttonUp = AL::Load("resources/audio/buttonUp.wav");

		}

	}

}

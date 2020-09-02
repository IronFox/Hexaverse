#pragma once

#include <math/vector.h>

namespace HUD
{
	namespace Audio
	{
		namespace M = DeltaWorks::Math;


		void				PlayButtonDownSound(const M::TVec2<>&center);
		void				PlayButtonUpSound(const M::TVec2<>&center);

		void				Create();


	}

}
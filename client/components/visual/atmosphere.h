#pragma once

#ifdef HAS_WINDOW

#include <engine/aspect.h>


namespace Atmosphere
{
	void		Render(const Engine::Aspect<>&mainCamera);

	void		Create();
	void		Destroy();


}
#endif



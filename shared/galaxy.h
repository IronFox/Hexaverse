#ifndef sharedGalaxyH
#define sharedGalaxyH

#include <engine/aspect.h>

namespace Galaxy
{
	void		Build(UINT32 seed);

	#ifdef HAS_WINDOW
		void	Render(const Engine::Aspect<>&, const Engine::Frustum<>&frustum);
	#endif


}



#endif

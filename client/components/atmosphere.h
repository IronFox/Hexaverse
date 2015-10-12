#ifndef atmosphereH
#define atmosphereH


#ifdef HAS_WINDOW

#include <math/vclasses.h>
#include <engine/aspect.h>


namespace Atmosphere
{
	void		Render(const Engine::Aspect<>&mainCamera);

	void		Create();
	void		Destroy();


}
#endif


#endif


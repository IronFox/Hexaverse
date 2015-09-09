#ifndef atmosphereH
#define atmosphereH

#include <math/vclasses.h>

#ifdef HAS_WINDOW

namespace Atmosphere
{
	void		Render(float lowerPlanetRadius,float upperPlanetRadius, const TVec3<>&cameraPosition);

	void		Create();
	void		Destroy();


}
#endif


#endif


#ifdef HAS_WINDOW
#ifndef worldComponentH
#define worldComponentH

#include "../../shared/sphereGrid.h"


namespace World
{
	extern bool allowRecompilation;

	void		RecompileIfNecessary();
	void		Render(const Engine::Frustum<>&);
	void		RenderWater(const Engine::Frustum<>&);
	void		RenderOverlay(const Engine::Frustum<>&);
	void		RenderResources(const Engine::Frustum<>&);

	void		RenderBuildBlocks();

}



#endif
#endif

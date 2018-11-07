#pragma once
#ifdef HAS_WINDOW

#include <engine/aspect.h>

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

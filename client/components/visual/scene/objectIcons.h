#pragma once
#ifdef HAS_WINDOW
#include <objects/types.h>
#include <container/array.h>

namespace Objects
{
	namespace Icons
	{
		/**
		Passes visible entity status icons to the sprite renderer.
		Depth-tested, cutout, limited transparency
		*/
		void	Render();
		void	Create();
		void	Refresh();


		void	AddAnimatedFloater(const Objects::PWorldEntity&, const Resource::FlagSet&resourceIndexes);
	}
}


#endif

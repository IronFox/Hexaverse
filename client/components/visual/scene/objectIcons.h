#pragma once
#ifdef HAS_WINDOW

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

	}
}


#endif

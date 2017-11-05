#pragma once
#ifdef HAS_WINDOW

namespace HUD
{

	namespace HostList
	{

		/**
		Completely rebuilds the entire GUI related to server connection choices.

		*/
		void	Update();

		void	Create();
		void	CheckVisibility(bool);
		bool	IsVisible();
	}
}
	
#endif

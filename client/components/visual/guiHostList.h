#pragma once
#ifdef HAS_WINDOW

namespace GUI
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

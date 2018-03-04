#pragma once
#ifdef HAS_WINDOW

namespace HUD
{

	/**
	Visual list of available hosts.
	Also currently responsible for displaying the changelog beneath
	*/
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

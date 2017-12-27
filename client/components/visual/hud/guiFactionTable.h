#pragma once
#ifdef HAS_WINDOW

#include <objects/types.h>
#include <global_string.h>

namespace HUD
{
	namespace FactionTable
	{

		void	Update();

		void	Show();
		bool	IsVisible();
		void	Hide();

		void	SignalChatMessage(const Objects::OwnerParameter&, const String&msg);
		void	ClearChatHistory();
	}
}


#endif

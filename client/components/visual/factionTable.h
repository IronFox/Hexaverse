#pragma once
#ifdef HAS_WINDOW

#include "guiElements.h"
#include <objects/types.h>

namespace GUI
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

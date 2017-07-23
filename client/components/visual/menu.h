#pragma once

#ifdef HAS_WINDOW

#include "dialog.h"
#include <io/config.h>
#include <global_string.h>

namespace Menu
{
	extern Dialog::PMenu	main,help,login;

	namespace Configuration
	{
		void	Assign(::DeltaWorks::Configuration&, const ::DeltaWorks::PathString&userConfigPath);
	}


	void					Create();
};


#endif

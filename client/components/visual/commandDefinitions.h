#pragma once
#ifdef HAS_WINDOW
#include <objects/entityCommand.h>

namespace Objects
{
	namespace Commands
	{
		void	DefineWorkingStructure(CommandContainer&container);
		void	DefineMine(CommandContainer&commands);
		
		
		void	DefineAll();


		/**
		Resets all commands to the currently loaded research script. Need only be called when the research script has changed
		*/
		void	ReconfigureResearchButtons();


	}
}


#endif

#pragma once
#ifdef HAS_WINDOW
#include <objects/entityCommand.h>
#include <objects/mobileEntityProducer.h>

namespace Objects
{
	namespace Commands
	{
		void	DefineWorkingStructure(CommandContainer&container);
		void	DefineMine(CommandContainer&commands);
		
		
		/**
		Creates and defines all commands for all entity types, as well as all global commands.
		Also rasterizes component images
		*/
		void	DefineAll();


		/**
		Resets all commands to the currently loaded research script. Need only be called when the research script has changed
		*/
		void	ReconfigureResearchButtons();


		
	}
}


#endif

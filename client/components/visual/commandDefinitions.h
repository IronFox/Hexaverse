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
		Enters structure placement mode on the selected terraformer.
		Fails silently if the operation is not possible for any reason.
		Otherwise activeCursorMode, choiceVis, and activeStructureToPlace are updated accordingly

		@param prototype Structure prototype to queue construction of
		*/
		void	TerraformerConstruct(const PStructure&prototype);

		
		/**
		Creates and defines all commands for all entity types, as well as all global commands.
		Also rasterizes component images
		*/
		void	DefineAll();


		/**
		Resets all commands to the currently loaded research script. Need only be called when the research script has changed
		*/
		void	ReconfigureResearchButtons();

		
		Grid::LCellIdI	GetPointOfOrigin(const Objects::MobileEntity&e);
		
	}
}


#endif

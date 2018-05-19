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
		Commits block building or removal operations, if found to be in that mode
		*/
		bool	TerraformerCommitOperation();

		/**
		Visits all currently defined terraformer shortcuts
		*/
		void		VisitTerraformerShortcuts(const std::function<void(Key::Name,const Objects::PStructure&)>&visitor);
		Key::Name	FindTerraformerShortcutKey(const Objects::PStructure&proto);
		
		/**
		Creates and defines all commands for all entity types, as well as all global commands.
		Also rasterizes component images
		*/
		void	DefineAll();
		void	RedefineGlobalCommands();


		/**
		Resets all commands to the currently loaded research script. Need only be called when the research script has changed
		*/
		void	ReconfigureResearchButtons();

		
		Grid::LCellIdI	GetPointOfOrigin(const Objects::MobileEntity&e);


		/**
		Executes whatever action is appropritate on the selected entity if the attack button is pressed.
		If the selected entity uses beam weapons, an attack command is send to the server.
		Otherwise the attack mode is activated (or deactivated if active).
		*/
		void	OnAttackButtonPress();
		
	}
}


#endif

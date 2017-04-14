#include <global_root.h>
#include "commandDefinitions.h"
#include "objectCommands.h"
#include <objects/structure.h>
#include "objectSelection.h"


namespace Objects
{
	namespace Commands
	{
		static void				EnableWorkingStructure()
		{
			Objects::WorkingStructure*s2 = dynamic_cast<Objects::WorkingStructure*>(selectedEntity.get());
			if (s2)
			{
				ApplicationInterface::SignalEnableStructure(*s2, !s2->IsEnabled());
				//s2->SetEnabled(!s2->IsEnabled());
			}
		}

		static bool				WorkingStructureIsEnabled()
		{
			Objects::WorkingStructure*s2 = dynamic_cast<Objects::WorkingStructure*>(selectedEntity.get());
			return s2 && s2->IsEnabled();
		};



		void DefineMine(CommandContainer&commands)
		{
			DefineWorkingStructure(commands);
			commands.Add( new IconCommand("Restore Core Link",[]()
			{
				PMine mine = ToMine(selectedEntity);
				if (mine)
				{
					ApplicationInterface::SignalRestoreCoreLink(mine->id);
				}
			},
			[](bool controlEnabled)
			{
				if (!controlEnabled)
					return BaseCommand::Flags::None;
				PMine mine = ToMine(selectedEntity);
				return mine && !mine->IsLinkedToCore() ? BaseCommand::Flags::IsEnabled : BaseCommand::Flags::None;
			},
			NoCosts
			));
		}

		void DefineWorkingStructure(CommandContainer & commands)
		{
			commands.baseCoords = int2(3,0);

			commands.Add(new IconCommand("Enable",&EnableWorkingStructure,
			[](bool controlEnabled)->BaseCommand::Flags
			{
				BaseCommand::Flags rs = BaseCommand::Flags::None;
				Objects::WorkingStructure*div = dynamic_cast<Objects::WorkingStructure*>(selectedEntity.get());
				if (controlEnabled)
				{
					if (div && div->CanEnable())
					{
						rs |= BaseCommand::Flags::IsEnabled;
					}
				}
				if (div && div->IsEnabled())
					rs |= BaseCommand::Flags::IsPermaActive;
				return rs;
			}
			,NoCosts
			));
		}
	}
}

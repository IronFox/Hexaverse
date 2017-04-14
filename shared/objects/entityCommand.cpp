#include <global_root.h>
#include "entityCommand.h"

namespace Objects
{
	/*static*/ const int2 BaseCommand::coordOffset(1,0);





	index_t					CommandContainer::Find(const PBaseCommand&cmd) const
	{
		foreach (members,m)
			if (m->second == cmd)
				return m - members.pointer();
		return InvalidIndex;
	}


	PBaseCommand			CommandContainer::Add(const int2&coords, bool relativeCoords, const PBaseCommand&newCommand)
	{
		DBG_ASSERT__(Find(newCommand) == InvalidIndex);
		Member&m = members.Append();
		m.second = newCommand;
		m.first = relativeCoords ? coords : coords - baseCoords;
		if (newCommand->IsGroup())
		{
			newCommand->SignalBaseChange(this,baseCoords + int2(0, m.first.y+1));
		}
		return newCommand;
	}

	PBaseCommand			CommandContainer::Add(const PBaseCommand&newCommand)
	{
		DBG_ASSERT__(Find(newCommand) == InvalidIndex);
		Member&m = members.Append();
		m.second = newCommand;
		m.first = int2((int)members.Count()-1,0);
		if (newCommand->IsGroup())
		{
			newCommand->SignalBaseChange(this,baseCoords + int2(0, m.first.y+1));
		}
		return newCommand;
	}


}

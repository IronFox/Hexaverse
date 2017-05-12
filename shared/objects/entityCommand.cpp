#include <global_root.h>
#include "entityCommand.h"

namespace Objects
{
	namespace Command
	{
		/*static*/ const int2 Base::coordOffset(1,0);





		index_t					Container::Find(const PBase&cmd) const
		{
			foreach (members,m)
				if (m->second == cmd)
					return m - members.pointer();
			return InvalidIndex;
		}


		PBase			Container::Add(const int2&coords, bool relativeCoords, const PBase&newCommand)
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

		PBase			Container::Add(const PBase&newCommand)
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

		void			Container::AddSpace()
		{
			members.Append();
			members.Last().first = int2((int)members.Count()-1,0);
		}

	}
}

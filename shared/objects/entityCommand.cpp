#include <global_root.h>
#include "entityCommand.h"

namespace Objects
{
	namespace Command
	{
		/*static*/ const int2 Base::coordOffset(1,0);

		Container&			Container::Append(const Container*other)
		{
			if (!other || other == this || !this)
				return *this;
			rows.Append(other->rows);
			return *this;
		}


		void			Container::VisitLocalMembers(const FVisitor&visitor)	const
		{
			foreach (rows,r)
				foreach (*r,cmd)
					if (*cmd)
						visitor(*cmd);
		}

		bool			Container::Contains(const PBase&cmd) const
		{
			foreach (rows,r)
				if (r->Contains(cmd))
					return true;
			return false;
		}
		PBase			Container::Add(const PBase&newCommand)
		{
			return Add(rows.IsEmpty() ? 0 : rows.Count()-1,newCommand);
		}
		PBase			Container::Add(index_t row, const PBase&newCommand)
		{
			DBG_ASSERT__(!Contains(newCommand));
			while (rows.Count() <= row)
				rows.Append();

			if (newCommand)
			{
				auto child = newCommand->ToContainer();
				if (child)
					child->parent = this;
			}

			rows[row] << newCommand;
			return newCommand;
		}

		count_t			Container::CountCommands() const
		{
			count_t rs = 0;
			foreach (rows,r)
				rs += r->Count();
			return rs;
		}

		//void			Container::Truncate(count_t numMembers)
		//{
		//	count_t current = CountCommands();
		//	if (current <= numMembers)
		//		return;
		//	count_t del = current - numMembers;
		//	while (rows.IsNotEmpty() && rows.Last().Count() <= del)
		//	{
		//		del -= rows.Last().Count();
		//		rows.EraseLast();
		//	}

		//	if (rows.IsNotEmpty() && del > 0)
		//	{
		//		rows.Last().Truncate(rows.Last().Count() - del);
		//	}
		//}


		void			Container::AddSpace()
		{
			if (rows.IsEmpty())
				rows.Append();
			rows.Last().Append();
			//members.Last().first = int2((int)members.Count()-1,0);
		}

	}
}

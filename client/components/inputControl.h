#pragma once

#include <objects/types.h>
#include <grid/stack.h>
#include <sharedContainer.h>

namespace InputControl
{

	

	bool	CanHit(const Objects::PMobileEntity&m, const Grid::LCellIdI&loc);
	bool	IsIdle(const Objects::PMobileEntity&v);
	bool	IsIdle(const Objects::PStructure&s);
	bool	IsIdle(const Objects::PWorldEntity&e);
	#ifdef HAS_WINDOW
		template <typename T>
		inline void GetIterateableEntity(Objects::PWorldEntity&rs, index_t&i, const SharedContainer<T>&list)
		{
			if (rs)
				return;
			if (i < list.Count())
			{
				rs = list[i];
				return;
			}
			i -= list.Count();
		}

	

		template <typename T>
		inline void FindIterateableEntity(index_t&offset,index_t&rs, const Objects::PWorldEntity&e, const SharedContainer<T>&list)
		{
			if (rs != InvalidIndex)
				return;
			rs = list.GetIndexOf(e);
			if (rs == InvalidIndex)
				offset += list.Count();
			else
				rs += offset;
		}


		count_t		CountIterateableEntities();

		Objects::PWorldEntity	GetIterateableEntity(index_t i);
		index_t	FindIterateableEntity(const Objects::PWorldEntity&e);

		void	SelectAndFocusNextIdle();
		void	SelectAndFocusPrev();
		void	SelectAndFocusNext();
		void	ResumeBatchConstruction();
		void	AbortBatchConstruction();
		void	OnWheel(short delta);


	#endif

}

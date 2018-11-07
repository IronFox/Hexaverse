#pragma once

#include <container/buffer.h>
#include <functional>
#include <memory>
#include <limits>

/**
Container for shared objects (effectively stores std::shared_ptr<T>).
The container is currently not thread-safe but allows insert and removal operations while iteration is in progress.
When appending while iteration is in progress, the iteration will also iterate over the new elements
*/
template <typename T>
	class SharedContainer : private Ctr::Buffer0<std::shared_ptr<T> >
	{
		typedef Ctr::Buffer0<std::shared_ptr<T> >	Super;
		typedef std::shared_ptr<T>				PT;

		static_assert(sizeof(sindex_t) == sizeof(index_t),"Type size error");

		count_t		recursionDepth=0;
		sindex_t	currentIterationAt[10]={0};

	public:
		Super::IsEmpty;
		Super::IsNotEmpty;
		Super::Count;
		Super::First;
		Super::Last;
		Super::operator[];
		Super::GetIndexOf;

		void		Foreach(const std::function<void(const PT&)>&f)
		{
			DBG_ASSERT_LESS__(recursionDepth,ARRAYSIZE(currentIterationAt));
			sindex_t&c = currentIterationAt[recursionDepth];
			recursionDepth++;
			for (c = 0; c < (sindex_t)Super::Count(); c++)
			{
				PT copy = Super::at(c);	//make sure we can manipulate the base as we see fit
				f(copy);
			}
			recursionDepth--;
		}

		SharedContainer<T>&	operator<<(const PT&element)
		{
			Super::operator<<(element);	//should be fine, even when iterating. new element will also be iterated over
			return *this;
		}

		void		Clear()
		{
			Super::Clear();	//if iterating, successive iteration check (currentIterationAt < Super::Count()) will fail
		}

		void		Erase(index_t at)
		{
			Super::Erase(at);
			for (index_t d = 0; d < recursionDepth; d++)
				if ((sindex_t)at <= currentIterationAt[d])
					currentIterationAt[d]--;
		}

		template <typename T2>	//allow base types here
			bool	FindAndErase(const std::shared_ptr<T2>&e)
			{
				index_t at = Super::GetIndexOf(e);
				if (at == InvalidIndex)
					return false;
				Erase(at);
				return true;
			}

		template <typename T2>	//allow base types here
			bool	Contains(const std::shared_ptr<T2>&element) const
			{
				return Super::Contains(element);
			}

		template <typename T2>
			void	CastAppendAllTo(BasicBuffer<std::shared_ptr<T2> >&outTarget) const
			{
				outTarget.AppendAll((const Super&)*this);
			}
		
	};
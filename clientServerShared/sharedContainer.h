#pragma once

#include <container/vectorList.h>
#include <functional>
#include <memory>
#include <limits>
#include <global_string.h>

/**
Container for shared objects (effectively stores std::shared_ptr<T>).
The container is currently not thread-safe but allows insert and removal operations while iteration is in progress.
When appending while iteration is in progress, the iteration will also iterate over the new elements
*/
template <typename T>
	class SharedContainer : private Ctr::Vector<std::shared_ptr<T> >
	{
		typedef Ctr::Vector<std::shared_ptr<T> >	Super;
		typedef std::shared_ptr<T>				PT;

		static_assert(sizeof(sindex_t) == sizeof(index_t),"Type size error");

		mutable count_t		recursionDepth=0;
		mutable sindex_t	currentIterationAt[10]={0};

	public:
		typedef SharedContainer<T>	Self;

		using Super::IsEmpty;
		using Super::IsNotEmpty;
		using Super::Count;
		using Super::First;
		using Super::Last;
		using Super::operator[];
		using Super::GetIndexOf;

		void		Foreach(const std::function<void(const PT&)>&f) const
		{
			DBG_ASSERT_LESS__(recursionDepth,ARRAYSIZE(currentIterationAt));
			sindex_t&c = currentIterationAt[recursionDepth];
			recursionDepth++;
			try
			{
				for (c = 0; c < (sindex_t)Super::Count(); c++)
				{
					PT copy = Super::at(c);	//make sure we can manipulate the base as we see fit
					f(copy);
				}
			}
			catch (...)
			{
				recursionDepth--;
				throw;
			}
			recursionDepth--;
		}

		PT			Scan(const std::function<bool(const PT&)>&f) const
		{
			DBG_ASSERT_LESS__(recursionDepth,ARRAYSIZE(currentIterationAt));
			sindex_t&c = currentIterationAt[recursionDepth];
			recursionDepth++;
			try
			{
				for (c = 0; c < (sindex_t)Super::Count(); c++)
				{
					PT copy = Super::at(c);	//make sure we can manipulate the base as we see fit
					if (f(copy))
					{
						recursionDepth--;
						return copy;
					}
				}
			}
			catch (...)
			{
				recursionDepth--;
				throw;
			}
			recursionDepth--;
			return nullptr;
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
			void	CastAppendAllTo(BasicVector<std::shared_ptr<T2> >&outTarget) const
			{
				outTarget.AppendAll((const Super&)*this);
			}
		

		void	swap(Self&other)
		{
			//leave recursion depth etc as they are
			Super::swap(other);
		}

		friend void swap(Self&a, Self&b)
		{
			a.swap(b);
		}
	};
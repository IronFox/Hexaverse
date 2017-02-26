#pragma once

#include <container/buffer.h>
#include <functional>
#include <memory>

/**
Container for shared objects (effectively stores std::shared_ptr<T>).
The container is currently not thread-safe but allows insert and removal operations while iteration is in progress.
When appending while iteration is in progress, the iteration will also iterate over the new elements
*/
template <typename T>
	class SharedContainer : private Buffer0<std::shared_ptr<T> >
	{
		typedef Buffer0<std::shared_ptr<T> >	Super;
		typedef std::shared_ptr<T>				PT;

		bool		iterating = false;
		index_t		current;

	public:
		Super::IsEmpty;
		Super::IsNotEmpty;
		Super::Count;
		Super::operator[];
		Super::GetIndexOf;

		void		Foreach(const std::function<void(const PT&)>&f)
		{
			iterating = true;
			for (current = 0; current < Super::Count(); current++)
			{
				PT copy = Super::at(current);	//make sure we can manipulate the base as we see fit
				f(copy);
			}
			iterating = false;
		}

		SharedContainer<T>&	operator<<(const PT&element)
		{
			Super::operator<<(element);	//should be fine, even when iterating. new element will also be iterated over
			return *this;
		}

		void		Clear()
		{
			Super::Clear();	//if iterating, successive iteration check (current < Super::Count()) will fail
		}

		template <typename T2>	//allow base types here
		bool		FindAndErase(const std::shared_ptr<T2>&e)
		{
			index_t at = Super::GetIndexOf(e);
			if (at == InvalidIndex)
				return false;
			Super::Erase(at);
			if (iterating && at <= current)
				current--;
			return true;
		}

		template <typename T2>	//allow base types here
		bool		Contains(const std::shared_ptr<T2>&element) const
		{
			return Super::Contains(element);
		}
		
	};
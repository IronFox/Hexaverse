#pragma once

#include <container/buffer.h>
#include <container/hashtable.h>
#include <functional>

template <typename...T>
	class Observer
	{
	public:
		typedef std::function<void(T...)>	F;

		void	Clear()	{functions.Clear();}
		void	operator()(T...p)const
		{
			foreach (functions,f)
				(*f)(p...);
		}
		void	operator+=(const F&f)
		{
			functions << f;
		}
		Observer&	operator<<(const F&f)
		{
			functions << f;
			return *this;
		}

		index_t		Register(const F&f)
		{
			index_t at = functions.Count();
			functions << f;
			keyCounter++;
			keyMap.Set(keyCounter,at);
			return keyCounter;
		}

		void		Unregister(index_t key)
		{
			index_t at;
			if (!keyMap.QueryAndUnset(key,at))
				return;
			keyMap.VisitAllValues([at](index_t&v)
			{
				if (v > at)
					v--;
				ASSERT__(v != at);
			});
			functions.Erase(at);
		}

	private:
		Buffer0<F,Swap>		functions;
		IndexTable<index_t>	keyMap;
		static index_t		keyCounter;
	};

template <typename...T>
	index_t Observer<T...>::keyCounter = 0;

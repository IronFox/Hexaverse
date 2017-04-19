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
		void	operator()(T...p)
		{
			iterating = true;
			for (index_t i = 0; i < functions.Count(); i++)
				functions[i](p...);
			iterating = false;
			foreach (dropBuffer,key)
				Unregister(*key);
			dropBuffer.Clear();
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
			//index_t key;
			//DBG_ASSERT1__(!keyMap.findKeyOf(at,key),key);
			keyMap.Set(keyCounter,at);
			return keyCounter;
		}

		void		Unregister(index_t key)
		{
			if (iterating)
			{
				dropBuffer << key;
				return;
			}
			index_t at;
			if (!keyMap.QueryAndUnset(key,at))
				return;
			//index_t k;
			//DBG_ASSERT1__(!keyMap.findKeyOf(at,k),k);
			keyMap.VisitAllValues([at](index_t&v)
			{
				DBG_ASSERT__(v != at);
				if (v > at)
					v--;
			});
			functions.Erase(at);
		}

	private:
		Buffer0<index_t>	dropBuffer;
		bool				iterating = false;
		Buffer0<F,Swap>		functions;
		IndexTable<index_t>	keyMap;
		static index_t		keyCounter;
	};

template <typename...T>
	index_t Observer<T...>::keyCounter = 0;

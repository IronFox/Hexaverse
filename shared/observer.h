#pragma once

#include <container/buffer.h>
#include <container/hashtable.h>
#include <functional>

using namespace DeltaWorks;


template <typename...T>
	class Observable
	{
	public:
		typedef std::function<void(T...)>	F;
		typedef Observable<T...>	Self;


		/**/	Observable() {}
		/**/	Observable(const Self&other) = delete;//these aren't inherently bad or impossible but typically imply that something isn't as it should be
		void	operator=(const Self &other) = delete;

		void	Clear()	{functions.Clear();}
		void	operator()(T...p)
		{
			iterating ++;
			for (index_t i = 0; i < functions.Count(); i++)
				functions[i].function(p...);
			iterating --;
			if (iterating == 0)
			{
				foreach(dropBuffer, key)
					Unregister(*key);
				dropBuffer.Clear();
			}
		}
		void	operator+=(const F&f)
		{
			functions.Append().function =f;
		}
		Observable&	operator<<(const F&f)
		{
			functions.Append().function =f;
			return *this;
		}

		bool		IsRegistered(index_t key) const
		{
			return keyMap.IsSet(key);
		}

		index_t		Register(const F&f)
		{
			index_t at = functions.Count();
			TFunction&func = functions.Append();
			func.function = f;
			keyCounter++;
			func.key = keyCounter;
			keyMap.Set(keyCounter,at);
			return keyCounter;
		}

		void		Unregister(index_t key)
		{
			if (key == InvalidIndex)
				return;
			if (iterating)
			{
				dropBuffer << key;
				return;
			}
			index_t at;
			if (!keyMap.QueryAndUnset(key,at))
				return;
			//index_t k;
			//DBG_ASSERT1__(!keyMap.FindKeyOf(at,k),k);
			keyMap.VisitAllValues([at](index_t&v)
			{
				DBG_ASSERT__(v != at);
				if (v > at)
					v--;
			});
			DBG_ASSERT_EQUAL__(functions[at].key,key);
			functions.Erase(at);
		}

		bool		IsEmpty() const { return functions.IsEmpty(); }
		bool		IsNotEmpty() const { return functions.IsNotEmpty(); }

		void		swap(Self&other)
		{
			dropBuffer.swap(other.dropBuffer);
			swp(iterating, other.iterating);
			functions.swap(other.functions);
			keyMap.swap(other.keyMap);
		}

	private:
		struct TFunction
		{
			F		function;
			index_t	key = InvalidIndex;

			void	swap(TFunction&other)
			{
				function.swap(other.function);
				swp(key,other.key);
			}
		};

		Ctr::Buffer0<index_t>		dropBuffer;
		index_t						iterating = 0;
		Ctr::Buffer0<TFunction,Swap>functions;
		Ctr::IndexTable<index_t>	keyMap;
		static index_t				keyCounter;
	};

template <typename...T>
	index_t Observable<T...>::keyCounter = 0;

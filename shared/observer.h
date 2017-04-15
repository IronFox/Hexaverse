#pragma once

#include <container/buffer.h>
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
		void	operator<<(const F&f)
		{
			functions << f;
		}
	private:
		Buffer0<F,Swap>	functions;

	};

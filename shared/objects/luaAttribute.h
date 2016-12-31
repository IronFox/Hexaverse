#pragma once

#include "../luaInterface.h"

namespace Objects
{
	template <typename T>
		struct ConstOverride
		{
			T			constValue;
			bool		isSet = false;
		};

	struct NoFilter
	{
		template <typename T>
			static void	Filter(T&){}
	};

	struct UnitScaleFilter
	{
		template <typename T>
			static void	Filter(T&v)
			{
				v *= Grid::UnitScale;
			}
	};

	template <typename T, typename Filter>
		struct LuaAttribute
		{
			Lua::Function<T(Lua::Reference)>		luaGetValue;
			Lua::Function<String(Lua::Reference)>	luaDescribeValue;

			ConstOverride<T>	constOverride;

			/**/		LuaAttribute()	{}
			/**/		LuaAttribute(const T&constOverride)
			{
				SetConst(constOverride);
			}

			bool		IsSet() const {return constOverride.isSet || luaGetValue.IsSet();}
			bool		IsEmpty() const {return !constOverride.isSet && luaGetValue.IsEmpty();}

			void		SetConst(const T&value)
			{
				constOverride.constValue = value;
				constOverride.isSet = true;
			}
			void		Clear()
			{
				constOverride.isSet = false;
				luaGetValue.Clear();
				luaDescribeValue.Clear();
			}

			T			GetValue(const Lua::Reference&researchState)	const
			{
				T rs = _GetValue(researchState);
				Filter::Filter(rs);
				return rs;
			}

			String		Describe(const Lua::Reference&researchState)	const
			{
				if (luaDescribeValue.IsSet())
					return luaDescribeValue(researchState);
				return _GetValue(researchState);
			}

		private:
			T			_GetValue(const Lua::Reference&researchState)	const
			{
				if (constOverride.isSet)
					return constOverride.constValue;
				if (luaGetValue.IsEmpty())
					return (T)0;
				return luaGetValue(researchState);
			}
		};	
	
}

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
	template <typename T>
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

			T			GetValue(const Lua::Reference&researchState)	const
			{
				if (constOverride.isSet)
					return constOverride.constValue;
				return luaGetValue(researchState);
			}

			String		Describe(const Lua::Reference&researchState)	const
			{
				if (luaDescribeValue.IsSet())
					return luaDescribeValue(researchState);
				return GetValue(researchState);
			}
		};	
	
}

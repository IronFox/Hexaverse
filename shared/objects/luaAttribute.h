#pragma once

#include "../luaInterface.h"

namespace Objects
{
	#ifdef CLIENT
		struct TStateEntry
		{
			float	value;
			String	description;
		};
		typedef Array<TStateEntry>	RawResearchState;

	#else
		typedef Lua::Reference	RawResearchState;

	#endif



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

	template <typename T, typename Filter= NoFilter>
		struct LuaAttribute
		{
			#ifndef CLIENT
				Lua::Function<T(Lua::Reference)>		luaGetValue;
				Lua::Function<String(Lua::Reference)>	luaDescribeValue;
				ConstOverride<T>	constOverride;
			#endif

			index_t		linearIndex = InvalidIndex;

			/**/		LuaAttribute()	{}
			/**/		LuaAttribute(nullptr_t)	{}
			/**/		LuaAttribute(const T&constOverride)
			{
				SetConst(constOverride);
			}

			bool		IsFunctionOrConstNonZero() const
			{
				#ifdef CLIENT
					return serverValue > 0;
				#else
					return constOverride.isSet ? constOverride.constValue != 0 : luaGetValue.IsSet();
				#endif
			}
			bool		IsSet() const
			{
				#ifdef CLIENT
					return serverValueDescription.IsNotEmpty();
				#else
					return constOverride.isSet || luaGetValue.IsSet();
				#endif
			}
			bool		IsEmpty() const {return !IsSet();}

			#ifndef CLIENT
				void		SetConst(const T&value)
				{
					constOverride.constValue = value;
					constOverride.isSet = true;
				}
			#endif

			void		Clear(index_t linearIndex)
			{
				#ifndef CLIENT
					constOverride.isSet = false;
					luaGetValue.ClearReference();
					luaDescribeValue.ClearReference();
				#endif

				this->linearIndex = linearIndex;
			}

			T			GetUnfilteredValue(const RawResearchState&researchState)	const
			{
				#ifdef CLIENT
					return (T)researchState[linearIndex].value;
				#else
					if (constOverride.isSet)
						return constOverride.constValue;
					if (luaGetValue.IsEmpty())
						return (T)0;
					return luaGetValue(researchState);
				#endif
			}

			T			GetValue(const RawResearchState&researchState)	const
			{
				T rs = GetUnfilteredValue(researchState);
				Filter::Filter(rs);
				return rs;
			}

			String		Describe(const RawResearchState&researchState)	const
			{
				#ifdef CLIENT
					return (T)researchState[linearIndex].description;
				#else
					if (luaDescribeValue.IsSet())
						return luaDescribeValue(researchState);
					return GetUnfilteredValue(researchState);
				#endif
			}

		private:

		};	
	
}

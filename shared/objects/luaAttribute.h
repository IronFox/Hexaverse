#pragma once

#include "research.h"

namespace Objects
{


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
			index_t		linearIndex = InvalidIndex;

			/**/		LuaAttribute()	{}
			/**/		LuaAttribute(nullptr_t)	{}
			/**/		LuaAttribute(const T&constOverride)
			{
				SetConst(constOverride);
			}

			bool		IsFunctionOrConstNonZero() const
			{
				return Research::LuaAttributeIsFunctionOrConstNonZero<T>(linearIndex);
			}
			bool		IsSet() const
			{
				return Research::LuaAttributeIsSet<T>(linearIndex);
			}
			bool		IsEmpty() const {return !IsSet();}


			void		Clear(index_t linearIndex)
			{
				this->linearIndex = linearIndex;
			}

			T			GetUnfilteredValue(const Research::Attributes::Table&researchState)	const
			{
				return researchState.Get<T>(linearIndex);
			}

			T			GetValue(const Research::Attributes::Table&researchState)	const
			{
				T rs = GetUnfilteredValue(researchState);
				Filter::Filter(rs);
				return rs;
			}

			String		Describe(const Research::Attributes::Table&researchState)	const
			{
				return researchState.Describe<T>(linearIndex);
			}

		private:

		};	
	
}

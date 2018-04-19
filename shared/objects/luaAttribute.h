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
			bool		isInvariant = false;
			T			invariantValue;

			index_t		linearIndex = InvalidIndex;

			/**/		LuaAttribute()	{}
			/**/		LuaAttribute(nullptr_t)	{}

			/**
			Declares the local attribute invariant, ignoring all and any settings found in Lua files.
			Under normal circumstances invariants and Lua loading are mutually exclusive.
			An invariant attribute should not be part of the regular Lua attribute mapping.
			Invariants must be set by server and client on exactly the same attributes with the same value, or stuff will break
			*/
			void		SetInvariant(const T&invariantValue)
			{
				DBG_ASSERT_EQUAL__(linearIndex,InvalidIndex);
				isInvariant = true;
				this->invariantValue = invariantValue;
			}

			bool		IsFunctionOrConstNonZero() const
			{
				if (isInvariant)
					return invariantValue != 0;
				return Research::LuaAttributeIsFunctionOrConstNonZero<T>(linearIndex);
			}
			bool		IsSet() const
			{
				return isInvariant || Research::LuaAttributeIsSet<T>(linearIndex);
			}
			bool		IsEmpty() const {return !IsSet();}


			void		Clear(index_t linearIndex)
			{
				DBG_ASSERT__(!isInvariant);
				this->linearIndex = linearIndex;
			}

			T			GetUnfilteredValue(const Research::Attributes::Table&researchState)	const
			{
				if (isInvariant)
					return invariantValue;
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
				if (isInvariant)
				{
					using StringConversion::ToString;
					return ToString(invariantValue)+" (const)";
				}
				return researchState.Describe<T>(linearIndex);
			}

		private:

		};	
	
}

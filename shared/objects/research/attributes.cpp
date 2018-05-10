#include "attributes.h"
#include <global_string.h>
#include <general/enumeration.h>
#include "../../lua/luaInterface.h"
#include "../../expectedBehavior.h"
#include "attributeDetails.h"
#ifdef CLIENT
	#include "globalState.h"
#endif

using namespace DeltaWorks;

namespace Objects
{
	namespace Research
	{
		String						attributeContext;


	#ifndef CLIENT
		void Attributes::Table::Update( const Lua::Reference&luaResearchState )
		{
			ClearChanges();
			LuaAttributes<int>::ExportTo(luaResearchState,ints);
			LuaAttributes<float>::ExportTo(luaResearchState,floats);
			LuaAttributes<bool>::ExportTo(luaResearchState,bools);
		}

		/*static*/ bool			LuaIntAttributeIsSet( index_t idx )
		{
			return LuaAttributes<int>::IsSet(idx);
		}

		/*static*/ bool			LuaBoolAttributeIsSet( index_t idx )
		{
			return LuaAttributes<bool>::IsSet(idx);
		}

		/*static*/ bool			LuaFloatAttributeIsSet( index_t idx )
		{
			return LuaAttributes<float>::IsSet(idx);
		}

		/*static*/ bool			LuaIntAttributeIsFunctionOrConstNonZero( index_t idx )
		{
			return LuaAttributes<int>::IsFunctionOrConstNonZero(idx);
		}

		/*static*/ bool			LuaBoolIsFunctionOrConstNonZero( index_t idx )
		{
			return LuaAttributes<bool>::IsFunctionOrConstNonZero(idx);
		}

		/*static*/ bool			LuaFloatAttributeIsFunctionOrConstNonZero( index_t idx )
		{
			return LuaAttributes<float>::IsFunctionOrConstNonZero(idx);
		}
	#else
		/*static*/ bool			LuaIntAttributeIsSet( index_t idx )
		{
			return globalState.zeroTable.ints.IsSet(idx);
		}

		/*static*/ bool			LuaBoolAttributeIsSet( index_t idx )
		{
			return globalState.zeroTable.bools.IsSet(idx);
		}

		/*static*/ bool			LuaFloatAttributeIsSet( index_t idx )
		{
			return globalState.zeroTable.floats.IsSet(idx);
		}

		/*static*/ bool			LuaIntAttributeIsFunctionOrConstNonZero( index_t idx )
		{
			return globalState.zeroTable.ints.IsFunctionOrConstNonZero(idx);
		}

		/*static*/ bool			LuaBoolIsFunctionOrConstNonZero( index_t idx )
		{
			return globalState.zeroTable.bools.IsFunctionOrConstNonZero(idx);
		}

		/*static*/ bool			LuaFloatAttributeIsFunctionOrConstNonZero( index_t idx )
		{
			return globalState.zeroTable.floats.IsFunctionOrConstNonZero(idx);
		}
	#endif
	}
}

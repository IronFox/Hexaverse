#pragma once


#include <global_root.h>
#include <container/array.h>
#include <container/buffer.h>
#include <global_string.h>

/**
Handling of various events resulting from networked communication.
On failure throws a Program::UnsupportedRequest exception, which may be handled differently from bugs.
*/
namespace Expect
{
	using namespace DeltaWorks;

	template <typename T>
		const T& SafeGet(const ArrayRef<T>&container, index_t index, const TCodeLocation&loc)
		{
			if (index < container.Count())
				return container[index];
			throw Except::Program::UnsupportedRequest(loc,"Index out of bounds for array access: "+String(index)+"/"+String(container.Count()));
		}

	template <typename T>
		T& SafeGet(MutableArrayRef<T>&container, index_t index, const TCodeLocation&loc)
		{
			if (index < container.Count())
				return container[index];
			throw Except::Program::UnsupportedRequest(loc,"Index out of bounds for array access: "+String(index)+"/"+String(container.Count()));
		}

	template <typename T, typename S>
		const T& SafeGet(const BasicBuffer<T,S>&container, index_t index, const TCodeLocation&loc)
		{
			if (index < container.Count())
				return container[index];
			throw Except::Program::UnsupportedRequest(loc,"Index out of bounds for buffer access: "+String(index)+"/"+String(container.Count()));
		}


	#define FAIL_EXPECT(MSG)	throw Except::Program::UnsupportedRequest(CLOCATION,MSG);
	#define EXPECT(X)			{if (!(X)) FAIL_EXPECT(#X " evaluates to false");}
	#define EXPECT1(X,V0)		{using ::DeltaWorks::StringConversion::ToString; if (!(X)) FAIL_EXPECT(String(#X " evaluates to false, with " #V0 "=")+ToString(V0));}
	#define EXPECT2(X,V0,V1)	{using ::DeltaWorks::StringConversion::ToString; if (!(X)) FAIL_EXPECT(String(#X " evaluates to false, with " #V0 "=")+ToString(V0)+" and " #V1 "="+ToString(V1));}
	#define EXPECT_EQUAL(A,B)	EXPECT2((A)==(B),A,B)
}

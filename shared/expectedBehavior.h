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
		const T& SafeGet(const BasicBuffer<T>&container, index_t index, const TCodeLocation&loc)
		{
			if (index < container.Count())
				return container[index];
			throw Except::Program::UnsupportedRequest(loc,"Index out of bounds for buffer access: "+String(index)+"/"+String(container.Count()));
		}

}

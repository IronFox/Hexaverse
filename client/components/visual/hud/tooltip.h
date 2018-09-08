#pragma once

#ifdef HAS_WINDOW

#include <global_string.h>
#include <functional>
#include <container/buffer.h>

namespace HUD
{
	namespace Tooltip
	{
		namespace D = DeltaWorks;

		interface IProvider
		{
			/**
			Retrieves a pair of tooltip texts.
			The first returned is the short, the second the long text
			*/
			virtual std::pair<D::String,D::String>	GetTooltip() const=0;
		};
		typedef std::function<IProvider*()>	FGet;

		/**
		Registry function to retrieve the tooltip provider based on the current mouse position.
		The first non-nullptr result is used (if any).
		Returned pointers are kept for pointer-comparison. Methods are queried right after invokation only if at all.
		Unless as an immediate response to tooltipQuery querying, returned pointers may be discarded independently without risk of access violations
		*/
		extern D::Buffer0<FGet,D::Swap>	tooltipQuery;

		void	Create();
		void	Render();
		void	Hide();

	}
}



#endif

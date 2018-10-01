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

		typedef std::pair<D::String,D::String>	Info;

		interface IProvider
		{
			/**
			Retrieves a pair of tooltip texts.
			The first returned is the short, the second the long text
			*/
			virtual Info	ResolveTooltip() const=0;
		};
		typedef std::function<const IProvider*()>	FGet;
		typedef std::function<Info()>	FInfo;

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

		Info	Build();
		Info	Build(const D::String&info);
		Info	Build(const D::String&shortInfo, const D::String&longInfo);

	}
}



#endif

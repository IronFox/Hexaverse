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

		typedef const char*	Info;

		interface IProvider
		{
			/**
			Retrieves the tooltip text at the specified number of hovering seconds (>= 0.25).
			The returned pointer will be used for comparison and instantly consumed.
			Must be provided by the IProvider instance but need not be maintained beyond the lifetime of the object itself.
			Called by the main thread only. Synchronization is necessary only in case of separately updating thread(s).
			Result may be null.
			Once any non-null return value was detected, subsequent null values merely preserve the current value.
			@param hoverSeconds Seconds since cursor remained stationary. Always >= 0.25
			@param lastCall Last time this provider was called since the cursor became stationary. 0 if this is the first call
			@return Object-managed string pointer, containing the tooltip text
			*/
			virtual Info	ResolveTooltip(float hoverSeconds, float lastCall) const=0;
		};
		typedef std::function<const IProvider*()>	FGet;
		typedef std::function<Info(float t, float lastT)>	FInfo;

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

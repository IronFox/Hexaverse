#pragma once


#ifdef HAS_WINDOW

#include <objects/resource.h>

namespace HUD
{

	/**
	Right-hand status display of the selected entity
	*/
	namespace StatePanel
	{
		static const float tagIconSize = 50.f;
		//0.0135f;

		float		OnTag(const float3&at, char32_t tagChar, float scaleX, float scaleY);

		inline float		TagWidth(char32_t tagChar)
		{
			return tagIconSize;
		}

		void		Render(const Objects::WorldEntity&e);

		/**
		Signifies that the state of the selected entity has changed and the panel should be recompiled
		*/
		void		SignalChange();

		float		GetPanelWidth();
		void		Create();

	}

}

#endif

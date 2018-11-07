#pragma once


#ifdef HAS_WINDOW

#include <objects/resource.h>

namespace HUD
{

namespace StatePanel
{
	static const char tagOffset = Objects::TextoutResourceTagOffset;
	static const float tagIconSize = 50.f;
	//0.0135f;

	float		OnTag(const float3&at, char tagChar, float scaleX, float scaleY);

	inline float		TagWidth(char tagChar)
	{
		return tagIconSize;
	}

	void		Render( const Objects::WorldEntity&e);

	/**
	Signifies that the state of the selected unit has changed and the panel should be recompiled
	*/
	void		SignalChange();

	float		GetPanelWidth();

}

}

#endif

#pragma once

#ifdef HAS_WINDOW

namespace UI
{
	/**
	Fetches the current UI scale, applied to all pixel-sized elements on screen (including font)
	*/
	float				GetScale();
	/**
	Updates the UI scale, applied to all pixel-sized elements on screen (including font)
	*/
	void				SetScale(float scale);
}

#endif
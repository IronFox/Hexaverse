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

	/**
	Fetches the current button scale, applied on top of the UI scale to hexagonal buttons
	(including unit navigation) and notifications only
	*/
	float				GetButtonScale();
	/**
	Updates the button scale, applied on top of the UI scale to hexagonal buttons
	(including unit navigation) and notifications only
	*/
	void				SetButtonScale(float scale);
}

#endif
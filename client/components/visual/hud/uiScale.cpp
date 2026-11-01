#include "uiScale.h"
#include <math/basic.h>
#include "../../clientEvents.h"

namespace UI
{
	static float scale = 1.f;

	float				GetScale()
	{
		return scale;
	}

	void				SetScale(float newScale)
	{
		if (DeltaWorks::M::Similar(scale,newScale))
			return;
		scale = newScale;
		GlobalEvents::uiScaleChanged();
	}

	static float buttonScale = 1.f;

	float				GetButtonScale()
	{
		return buttonScale;
	}

	void				SetButtonScale(float newScale)
	{
		if (DeltaWorks::M::Similar(buttonScale,newScale))
			return;
		buttonScale = newScale;
		GlobalEvents::buttonScaleChanged();
	}


}


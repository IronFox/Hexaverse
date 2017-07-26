#include "uiScale.h"
#include <math/basic.h>
#include "../clientEvents.h"

namespace UI
{
	static float scale = 1.f;

	float				GetScale()
	{
		return scale;
	}

	void				SetScale(float newScale)
	{
		if (DeltaWorks::Math::similar(scale,newScale))
			return;
		scale = newScale;
		GlobalEvents::uiScaleChanged();
	}


}


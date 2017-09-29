#pragma once

#ifdef HAS_WINDOW

#include <logEvent.h>

/**
Controls the visual representation of important events on screen
*/
namespace VisualEventLog
{
	void	SignalWindowResize();
	void	LogEvent( EventClass type,  const Objects::WorldEntity*e, const String&_event);
	void	Render();
}




#endif

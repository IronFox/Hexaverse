#pragma once

#ifdef HAS_WINDOW

#include <logEvent.h>

namespace HUD
{

/**
Controls the visual representation of important events on screen
*/
namespace VisualEventLog
{


	void	SignalWindowResize();
	/**
	Adds a new event to the visual event queue.
	It will be listed beneath any existing log messages, pushing them up.
	The message automatically fades after a while, depending on significance.
	*/
	void	DisplayEvent( EventClass type,  const Objects::WorldEntity*e, const Objects::OwnerParameter&owner, const String&_event, Logging::Origin);
	/**
	Renders all visible events.
	Expects Camera::hud to be bound
	*/
	void	Render();
}

}


#endif

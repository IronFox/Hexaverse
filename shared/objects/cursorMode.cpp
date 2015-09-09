#include <global_root.h>
#include "cursorMode.h"


namespace Objects
{
	#ifdef HAS_WINDOW
		CursorMode::mode_t			activeCursorMode(CursorMode::Idle);
	#endif
}
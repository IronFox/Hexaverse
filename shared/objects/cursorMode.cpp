#include <global_root.h>
#include "cursorMode.h"


namespace Objects
{
	#ifdef HAS_WINDOW
		CursorMode			activeCursorMode(CursorMode::Idle);
	#endif
}
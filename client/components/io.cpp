#include "io.h"

#ifdef HAS_WINDOW
	void	SetFullScreen(bool isFull);
#endif

void OpenDocumentExternally(const PathString&document)
{
	#ifdef HAS_WINDOW
		SetFullScreen(false);
	#endif
	_wsystem(("start "+document).c_str());
}

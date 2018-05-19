#include "io.h"

#ifdef HAS_WINDOW
	void	SetFullScreen(bool isFull);
#endif

void OpenDocumentExternally(const PathString&document)
{
	#ifdef HAS_WINDOW
		SetFullScreen(false);
	#endif
	ShellExecuteW(NULL, NULL, document.c_str(), NULL, NULL, SW_SHOWNORMAL);
	//_wsystem(("start \""+document+"\"").c_str());
}

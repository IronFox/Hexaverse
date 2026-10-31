#include <global_string.h>
using namespace DeltaWorks;

/**
Exits fullscreen mode, and attempts to open the specified document (or URL) with the appropriate application.

Documents are opened using the windows task system in order to prevent Steam from picking up on the operation.
Otherwise Steam would consider any externally opened document viewer (browser, editor, w/e) an extension of the local game,
and maintain that the game was still running when in fact all game executables have ended.
*/
void OpenDocumentExternally(const PathString&document);

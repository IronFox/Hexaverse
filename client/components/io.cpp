#include "io.h"
#include <general/system.h>
#include <uniqueID.h>
#include <io/string_file.h>
#include <string/encoding.h>
#include <io/file_system.h>
#include <io/monitoredProcess.h>
#include <logEvent.h>
#include <io/sharedLibrary.h>

#ifdef HAS_WINDOW
	void	SetFullScreen(bool isFull);
#endif

	void OpenDocumentExternally(const PathString&document)
{
	#ifdef HAS_WINDOW
		SetFullScreen(false);
	#endif

	#if 0
	MonitoredProcess proc(false);

	static bool wineChecked = false, haveWine = false;
	if (!wineChecked)
	{
		wineChecked = true;
		SharedLibrary lib;
		if (lib.Open("ntdll.dll"))
		{
			typedef const char * (CDECL *FGetWineVersion)(void);

			FGetWineVersion getVersion;
			if (lib.FindFunction("wine_get_version",getVersion))
			{
				const char*version = getVersion();
				if (!version)
					version = "unknown version";
				LogEvent(EventSignificance::Uncommon,"Wine ("+String(version)+") detected. Opening external documents directly...");
				haveWine = true;
			}
		}
	}

	if (!haveWine)
		proc.Start(FileSystem::WorkingDirectory(),"../TaskRun/TaskRun" PLATFORM_TARGET_NAME_EXTENSION_STR ".exe","cmd.exe /C start "+document,false);
	else
	#endif	/*0*/
		ShellExecute(0, 0, document.c_str(), 0, 0 , SW_SHOW );
	
}

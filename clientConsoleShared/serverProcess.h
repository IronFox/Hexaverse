#pragma once

#include <global_string.h>
#include <functional>
#include "../shared/protocol.h"

#include <structure/observable.h>

using namespace DeltaWorks;

namespace ServerProcess
{
	typedef std::function<void(const String&)> FLogger;

	void			Setup(Observable<>&onProgramTerminate, const FLogger&minor, const FLogger&common, const FLogger&important, const FLogger&error);
	void			SetOnEndAsync(const std::function<void()>&);
	/**
	Starts server process.
	If a process is already running, the function returns.
	*/
	void			Begin(bool remoteControl, bool tutorialMode,const PathString&customFlags="", USHORT basePort = DEFAULT_SERVER_BASE_PORT);
	void			End();
	bool			IsRunning();
	void			Destroy();
	/**
	Executes the server using system(), and does not return before the server has quit.
	Use this to query data from the server. Do not use to just run the server, as the method will never return in that case.
	*/
	bool			RunNow(const PathString&parameters);

	void			GetWindows(Array<HWND>&outWindows);
}

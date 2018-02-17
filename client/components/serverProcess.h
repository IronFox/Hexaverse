#pragma once

#include <global_string.h>
using namespace DeltaWorks;

namespace ServerProcess
{
	/**
	Starts server process.
	If a process is already running, the function returns.
	*/
	void			Begin(bool remoteControl, bool tutorialMode,const PathString&customFlags="");
	void			End();
	bool			IsRunning();
	void			Destroy();
	void			StartTutorialServer();
	/**
	Executes the server using system(), and does not return before the server has quit.
	Use this to query data from the server. Do not use to just run the server, as the method will never return in that case.
	*/
	bool			RunNow(const PathString&parameters);
}

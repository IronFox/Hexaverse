#pragma once

namespace ServerProcess
{
	/**
	Starts server process.
	If a process is already running, the function returns.
	*/
	void			Begin(const String&planetName, bool remoteControl, bool tutorialMode);
	void			End();
	bool			IsRunning();
	void			Destroy();
	void			StartTutorialServer();

}

#pragma once

namespace ServerProcess
{
	void			Begin(bool remoteControl=true, bool configurationMode=true, bool tutorialMode=false);
	void			End();
	bool			IsRunning();
	void			Destroy();
	void			StartTutorialServer();

}

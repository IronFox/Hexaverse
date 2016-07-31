#ifndef serverProcessH
#define serverProcessH


namespace ServerProcess
{
	void			Begin(bool remoteControl=true, bool configurationMode=true);
	void			End();
	bool			IsRunning();
	void			Destroy();
}





#endif

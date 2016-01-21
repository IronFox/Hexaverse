#include <global_root.h>
#include "serverProcess.h"
#include <global_string.h>
//#include "../../shared/uniqueID.h"
#include <general/timer.h>
#include <general/monitoredProcess.h>

namespace ServerProcess
{
	namespace Detail
	{
		//CryptographicID	accessToken;

		MonitoredProcess	process;
		bool				remoteControl = false;

		void	Start()
		{
			char parameterBuffer[32768];
			PathString	path,
						workingDirectory = "..\\server\\";
			#ifdef _DEBUG
				path = "..\\server\\.projector\\Debug\\server.exe";
			#else
				path = "..\\server\\server" PLATFORM_TARGET_NAME_EXTENSION_STR ".exe";
			#endif

			PathString parameters;
			if (remoteControl)
				parameters = "--remote";

			ASSERT3__(process.ResumeOrStart(workingDirectory,path,parameters), workingDirectory,path,parameterBuffer);
//			ASSERT3__(process.ResumeOrStart(workingDirectory,path,parameters,false), workingDirectory,path,parameterBuffer);
		}
	
	}


	void Begin(bool remoteControl/*=true*/)
	{
		if (Detail::process.IsStarted())
		{
			ASSERT__(remoteControl == Detail::remoteControl);
			return;
		}
		Detail::remoteControl = remoteControl;
		Detail::Start();
	}

	void End()
	{
		Detail::process.Terminate();
	}

	bool			IsRunning()
	{
		return Detail::process.IsRunning();
	}



	//void Check()
	//{
	//	detail::process.Check();
	//}


}





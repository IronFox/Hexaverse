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


		PathString	GetWorkingDirectory()
		{
			return "..\\server\\";
		}

		PathString GetExecutablePath()
		{
			#ifdef _DEBUG
				return "..\\server\\.projector\\Debug\\server.exe";
			#else
				return "..\\server\\server" PLATFORM_TARGET_NAME_EXTENSION_STR ".exe";
			#endif
		}

		void	Start()
		{
			char parameterBuffer[32768];
			PathString	path = GetExecutablePath(),
						workingDirectory = GetWorkingDirectory();

			PathString parameters;
			if (remoteControl)
				parameters = "--remote";

			bool showWindow = false;
			#ifdef _DEBUG
				showWindow = true;
			#endif
			ASSERT3__(process.ResumeOrStart(workingDirectory,path,parameters,showWindow), workingDirectory,path,parameterBuffer);
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
		if (!Detail::process.IsRunning())
			Detail::process.TryResume(Detail::GetWorkingDirectory(),Detail::GetExecutablePath());
		return Detail::process.IsRunning();
	}



	//void Check()
	//{
	//	detail::process.Check();
	//}


}





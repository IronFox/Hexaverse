#include <global_root.h>
#include "serverProcess.h"
#include <global_string.h>
//#include "../../shared/uniqueID.h"
#include <general/timer.h>
#include <general/monitoredProcess.h>
#include <logEvent.h>

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

		bool TryResume()
		{
			if (Detail::process.TryResume(Detail::GetWorkingDirectory(),Detail::GetExecutablePath()))
			{
				LogEvent(EventType::Debug,"Resumed running server process at '"+String(Detail::GetExecutablePath())+"'");
				return true;
			}
			return false;


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
			if (TryResume())
				return;
			LogEvent(EventType::Debug,"Starting server process (workingDirectory='"+String(workingDirectory)+"', path='"+String(path)+"', parameters='"+String(parameters)+"', showWindow="+String(showWindow)+")");
			ASSERT3__(process.Start(workingDirectory,path,parameters,showWindow), workingDirectory,path,parameterBuffer);
		}

			
	}


	void Begin(bool remoteControl/*=true*/)
	{
		if (Detail::process.IsRunning())
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
			Detail::TryResume();
		return Detail::process.IsRunning();
	}



	//void Check()
	//{
	//	detail::process.Check();
	//}


}





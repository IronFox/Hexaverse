#include <global_root.h>
#include "serverProcess.h"
#include <global_string.h>
//#include "../../shared/uniqueID.h"
#include <general/timer.h>
#include <general/monitoredProcess.h>
#include <logEvent.h>
#include "visual/dialog.h"

#include "clientRuntimeConfig.h"
#include "network.h"

namespace ServerProcess
{
	namespace Detail
	{
		//CryptographicID	accessToken;

		MonitoredProcess	process;
		bool				remoteControl = false, configurationMode=false,tutorialMode=false;


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
				LogEvent(EventSignificance::Debug,"Resumed running server process at '"+String(Detail::GetExecutablePath())+"'");
				return true;
			}
			return false;


		}

		void	Start()
		{
			PathString	path = GetExecutablePath(),
						workingDirectory = GetWorkingDirectory();

			PathString parameters;
			if (!remoteControl)
				parameters += " -r";
			if (configurationMode)
				parameters += " -c";
			if (tutorialMode)
				parameters += " -t";

			bool showWindow = false;
			#ifdef _DEBUG
				showWindow = true;
			#endif
			if (TryResume())
				return;
			LogEvent(EventSignificance::Debug,"Starting server process (workingDirectory='"+String(workingDirectory)+"', path='"+String(path)+"', parameters='"+String(parameters)+"', showWindow="+String(showWindow)+")");
			try
			{
				process.Start(workingDirectory,path,parameters,showWindow);
			}
			catch (const std::exception&ex)
			{
				DBG_FATAL__(ex.what());
				LogEvent(EventClass::Preset::error, ex.what());
			}
		}

		
		void	StartTutorial()
		{
			RuntimeConfig::serverAddress = Network::ServerAddress("localhost");
			#ifdef HAS_WINDOW
				Dialog::PermanentMessage("Hold on. Getting tutorial server ready.",[]()->Dialog::CheckResult
				{
					if (Network::haveFaction)
					{
						//Dialog::ShowMessage("Welcome to the Hexaverse Tutorial");
						return Dialog::CheckResult::Abort;
					}
					return Dialog::CheckResult::OkayContinue;
				});
			#endif
			ServerProcess::Begin(true,false,true);
		}

	}


	void Begin(bool remoteControl/*=true*/, bool configurationMode/*=true*/, bool tutorialMode/*=false*/)
	{
		if (Detail::process.IsRunning())
		{
			ASSERT__(remoteControl == Detail::remoteControl);
			return;
		}
		Detail::remoteControl = remoteControl;
		Detail::configurationMode = configurationMode;
		Detail::tutorialMode = tutorialMode;
		Detail::Start();
	}

	void End()
	{
		Detail::process.Terminate();
	}

	void Destroy()
	{
		if (Detail::process.IsRunning() && !Detail::process.HasAnyVisibleWindows())
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



	void StartTutorialServer()
	{
		#ifdef HAS_WINDOW
		if (ServerProcess::IsRunning())
		{
			Dialog::Confirm("In order to start a tutorial server, the local server must be shut down. Continue?",
			[]()
			{
				ServerProcess::End();
				Detail::StartTutorial();
			},
			[]()->Dialog::CheckResult
			{
				if (!ServerProcess::IsRunning())
					return Dialog::CheckResult::Confirm;
				return Dialog::CheckResult::OkayContinue;
			});
		}
		else
		#endif
			Detail::StartTutorial();

	}
}





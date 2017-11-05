#include <global_root.h>
#include "serverProcess.h"
#include <global_string.h>
//#include "../../shared/uniqueID.h"
#include <general/timer.h>
#include <io/monitoredProcess.h>
#include <logEvent.h>
#include "visual/hud/dialog.h"
#include "clientEvents.h"

#include "clientRuntimeConfig.h"
#include "network.h"

namespace ServerProcess
{
	namespace Detail
	{
		//CryptographicID	accessToken;

		MonitoredProcess	process(false);
		bool				remoteControl = false, configurationMode=false,tutorialMode=false,resumed=false;
		index_t				shutdownRegistration=InvalidIndex;
		#ifdef HAS_WINDOW
			HUD::Dialog::WDialog	holdOn;
			index_t			holdOnID;
			Timer::Time		holdOnCreated;
		#endif

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
				resumed = true;
				LogEvent(EventSignificance::Debug,"Resumed running server process at '"+String(Detail::GetExecutablePath())+"'");
				return true;
			}
			return false;
		}

		void	TerminateIfNotResumed()
		{
			if (!resumed)
				process.Terminate();
		}

		void	Start()
		{
			if (shutdownRegistration == InvalidIndex)
				shutdownRegistration = GlobalEvents::applicationTerminate.Register(TerminateIfNotResumed);


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
			resumed = false;
			LogEvent(EventSignificance::Debug,"Starting server process (workingDirectory='"+String(workingDirectory)+"', path='"+String(path)+"', parameters='"+String(parameters)+"', showWindow="+String(showWindow)+")");
			try
			{
				process.Start(workingDirectory,path,parameters,showWindow);
			}
			catch (const std::exception&ex)
			{
				LogEvent(EventClass::Preset::error, ex.what());
				if (IsDebuggerPresent())
					DebugBreak();
			}
		}

		
		void	StartTutorial()
		{
			LogEvent(EventClass::Preset::uncommon,"Starting tutorial server...");
			RuntimeConfig::serverAddress = Network::ServerAddress("localhost");
			#ifdef HAS_WINDOW
				holdOn = HUD::Dialog::PermanentMessage("Hold on. Getting tutorial server ready.",[]()->HUD::Dialog::CheckResult
				{
					if (Network::haveFaction)
					{
						//HUD::Dialog::ShowMessage("Welcome to the Hexaverse Tutorial");
						return HUD::Dialog::CheckResult::Abort;
					}
					return HUD::Dialog::CheckResult::OkayContinue;
				});
				holdOnCreated = timer.Now();

				holdOnID = GlobalEvents::frameBegin.Register([]()
				{
					HUD::Dialog::PDialog d = holdOn.lock();
					if (!d || !HUD::WindowStack::IsVisible(d))
					{
						GlobalEvents::frameBegin.Unregister(holdOnID);
						holdOnID = InvalidIndex;
						return;
					}
					if (timer.GetSecondsSince(holdOnCreated) > 30)
					{
						End();
						HUD::WindowStack::Hide(d);
						GlobalEvents::frameBegin.Unregister(holdOnID);
						holdOnID = InvalidIndex;
						RuntimeConfig::serverAddress = Network::ServerAddress();
						HUD::Dialog::ShowMessage("Tutorial server took too long to start. Something went wrong. Please check any available logs in\n"
								+String(FileSystem::Folder( RuntimeConfig::ConfigDir()).ParentLocation()->GetLocation())
							+"\nSorry about this :S");
					}

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
			HUD::Dialog::Confirm("In order to start a tutorial server, the local server must be shut down. Continue?",
			[]()
			{
				ServerProcess::End();
				Detail::StartTutorial();
			},
			[]()->HUD::Dialog::CheckResult
			{
				if (!ServerProcess::IsRunning())
					return HUD::Dialog::CheckResult::Confirm;
				return HUD::Dialog::CheckResult::OkayContinue;
			});
		}
		else
		#endif
			Detail::StartTutorial();

	}
}





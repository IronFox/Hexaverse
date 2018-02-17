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
		String				planetName;
		bool				remoteControl = false, tutorialMode=false,resumed=false;
		index_t				shutdownRegistration=InvalidIndex;
		#ifdef HAS_WINDOW
			HUD::Dialog::WDialog	holdOn;
			index_t			holdOnID;
			Timer::Time		holdOnCreated;
		#endif



		namespace Heartbeat
		{

			SOCKET s = INVALID_SOCKET;
			SOCKADDR_IN host;
			int hostLen;

			class Loop : public Sys::ThreadObject
			{
			public:
				virtual void ThreadMain()
				{
					char c = ' ';
					index_t at = 0;
					while (s != INVALID_SOCKET)
					{
						Sleep(50);	//higher frequency for quicker stops
						if (++at < (1000/50))
							continue;
						at = 0;
						int sent = sendto(s,&c,1,0,(const sockaddr*)&host,hostLen);
						if (sent <= 0)
						{
							String error = Net::GetLastError();
							if (s != INVALID_SOCKET)
								FATAL__("Heardbeat receive failure. Socket lost: "+error);
						}
					}
				}
			};
			Loop	loopThread;

			void	Stop()
			{
				if (s == INVALID_SOCKET)	//fast check
					return;

				static Sys::SpinLock lock;
				lock.lock();
				if (s != INVALID_SOCKET)	//check again after lock
				{
					SOCKET s1 = s;
					s = INVALID_SOCKET;
					closesocket(s1);		//swap close (s is INVALID_SOCKET before the socket is closed)
				}
				lock.unlock();
			}

			void	Start()
			{
				LogEvent(EventSignificance::Common,"Starting heartbeat sender");
				GlobalEvents::applicationTerminate << Stop;
				ASSERT__(Net::initNet());

				Stop();
				loopThread.CheckAwaitCompletion();


				s = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
				ASSERT_NOT_EQUAL__(s,INVALID_SOCKET);
			
				memset(&host,0,sizeof(host));
				host.sin_family=AF_INET;
				host.sin_port=htons(HEARTBEAT_PORT);
				host.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
				hostLen = (int)sizeof(host);

				SOCKADDR_IN me;
				memset(&me,0,sizeof(me));
				me.sin_family=AF_INET;
				me.sin_port=0;//htons(HEARTBEAT_PORT);	//just get any free
				me.sin_addr.s_addr = htonl(INADDR_ANY);
				int rc=::bind(s,(SOCKADDR*)&me,sizeof(SOCKADDR_IN));
				if (rc != 0)
				{
					closesocket(s);
					s = INVALID_SOCKET;
					FATAL__("Unable to bind UDP heartbeat socket. This should never happen");
					return;
				}
				loopThread.Start();
			}

		}


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


		void	Terminate()
		{
			process.Terminate();
			Heartbeat::Stop();
		}

		void	TerminateIfNotResumed()
		{
			if (!resumed)
				Terminate();
		}

		void	Start()
		{
			if (shutdownRegistration == InvalidIndex)
				shutdownRegistration = GlobalEvents::applicationTerminate.Register(TerminateIfNotResumed);


			PathString	path = GetExecutablePath(),
						workingDirectory = GetWorkingDirectory();

			PathString parameters = " -b";	//heartbeat
			if (!remoteControl)
				parameters += " -r";
			if (tutorialMode)
				parameters += " -t";
			if (planetName.IsNotEmpty())
				parameters += PathString(" -w "+planetName);
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
				Heartbeat::Start();
				process.onEndAsync = Heartbeat::Stop;
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
								+String(FileSystem::Folder( RuntimeConfig::myConfig.GetRoot()).ParentLocation()->GetLocation())
							+"\nSorry about this :S");
					}

				});
			#endif
			ServerProcess::Begin(true,false,true);
		}

	}


	void Begin(const String&planetName, bool remoteControl, bool tutorialMode)
	{
		if (Detail::process.IsRunning())
		{
			ASSERT__(remoteControl == Detail::remoteControl);
			return;
		}
		Detail::remoteControl = remoteControl;
		Detail::planetName = planetName;
		Detail::tutorialMode = tutorialMode;
		Detail::Start();
	}

	void End()
	{
		Detail::Terminate();
	}

	void Destroy()
	{
		if (Detail::process.IsRunning() && !Detail::process.HasAnyVisibleWindows())
			Detail::Terminate();
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





#include <global_root.h>
#include "serverProcess.h"
#include <global_string.h>
//#include "../../shared/uniqueID.h"
#include <general/timer.h>
#include <io/monitoredProcess.h>
#include <net/tcp.h>

//#include <logEvent.h>
//#include "visual/hud/dialog.h"
//#include "clientEvents.h"
//
//#include "clientRuntimeConfig.h"
//#include "network.h"

namespace ServerProcess
{
	namespace Detail
	{
		//CryptographicID	accessToken;

		MonitoredProcess	process(false);
		PathString			parameters = " -b";
		bool				resumed=false;
		index_t				shutdownRegistration=InvalidIndex;

		Observable<>		*onApplicationTerminate;

		FLogger				logNormal = [](const String&){},
							logMinor = [](const String&){},
							logError = [](const String&){},
							logImportant = [](const String&){};

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

			void	Start(UINT16 basePort)
			{
				logNormal("Starting heartbeat sender");
				*onApplicationTerminate << Stop;
				ASSERT__(Net::initNet());

				Stop();
				loopThread.CheckAwaitCompletion();


				s = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
				ASSERT_NOT_EQUAL__(s,INVALID_SOCKET);
			
				memset(&host,0,sizeof(host));
				host.sin_family=AF_INET;
				host.sin_port=htons(basePort + PortOffset::Heartbeat);
				host.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
				hostLen = (int)sizeof(host);

				SOCKADDR_IN me;
				memset(&me,0,sizeof(me));
				me.sin_family=AF_INET;
				me.sin_port=0; //just get any free
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
				logMinor("Resumed running server process at '"+String(Detail::GetExecutablePath())+"'");
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

		void	Start( UINT16 basePort)
		{
			if (shutdownRegistration == InvalidIndex)
				shutdownRegistration = onApplicationTerminate->Register(TerminateIfNotResumed);


			PathString	path = GetExecutablePath(),
						workingDirectory = GetWorkingDirectory();

			bool showWindow = false;
			#ifdef _DEBUG
				showWindow = true;
			#endif
			if (TryResume())
				return;
			resumed = false;
			logMinor("Starting server process (workingDirectory='"+String(workingDirectory)+"', path='"+String(path)+"', parameters='"+String(parameters)+"', showWindow="+String(showWindow)+")");
			try
			{
				process.Start(workingDirectory,path,parameters,showWindow);
				Heartbeat::Start(basePort);
				process.onEndAsync = Heartbeat::Stop;
			}
			catch (const std::exception&ex)
			{
				logError(ex.what());
				if (IsDebuggerPresent())
					DebugBreak();
			}
		}

	}


	void	Setup(Observable<>&onProgramTerminate, const FLogger&minor, const FLogger&common, const FLogger&important, const FLogger&error)
	{
		using namespace Detail;
		logMinor = minor ? minor : [](const String&){};
		logNormal = common ? common : [](const String&){};
		logImportant = important ? important : [](const String&){};
		onApplicationTerminate = &onProgramTerminate;
	}


	bool RunNow(const PathString&parameters)
	{
		Detail::process.Terminate();
		Detail::parameters = parameters;
		Detail::Start(DEFAULT_SERVER_BASE_PORT);
		Detail::process.AwaitCompletion();
		return Detail::process.GetLastExitCode() == 0;
	}


	void Begin(bool remoteControl, bool tutorialMode, const PathString&customFlags, USHORT basePort /*= DEFAULT_SERVER_BASE_PORT*/)
	{
		if (Detail::process.IsRunning())
			return;

		Detail::parameters = " -b";	//heartbeat
		if (!remoteControl)
			Detail::parameters += " -r";
		if (tutorialMode)
			Detail::parameters += " -t";
		if (basePort != DEFAULT_SERVER_BASE_PORT)
			Detail::parameters += " -p "+PathString(basePort);
		if (customFlags.IsNotEmpty())
			Detail::parameters += " "+customFlags;

		Detail::Start(basePort);
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





}





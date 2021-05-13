#include <global_root.h>
#include "aiProcess.h"
#include <global_string.h>
#include <uniqueID.h>
#include <general/timer.h>
#include <io/monitoredProcess.h>
#include <globalEvents.h>
#include <logEvent.h>
#include <startProcess.h>

namespace AIProcess
{
	namespace detail
	{
		CryptographicID	accessToken;
		std::function<String()> asyncGetServerPassword;

		MonitoredProcess	process(true);
		index_t	eventKey = InvalidIndex;
		USHORT serverBasePort=0;

		void	Start()
		{
			PathString	path,
						workingDirectory = L"..\\client\\";
				path = Process::LocateProjectExecutable(workingDirectory,
					#ifdef _DEBUG
						L"Visual Client"
					#else
						L"Console Client"
					#endif
				);


			try
			{
				bool window = false;
				#ifdef _DEBUG
					window = true;
				#endif

				process.Start(workingDirectory,path,[]()
				{
					char parameterBuffer[32768];
					sprintf_s(parameterBuffer,"aiAccessToken='%s' password='%s' udpListen=false serverURL=localhost:%u",accessToken.ToFullString().c_str(), asyncGetServerPassword().c_str(),serverBasePort);
					return PathString(parameterBuffer);
				},window);
			}
			catch (const std::exception&ex)
			{
				LogEvent(EventClass::Preset::error, ex.what());
				if (IsDebuggerPresent())
					DebugBreak();
			}
		}
	

		void EventEnd()
		{
			detail::process.Terminate();
		}
	}


	void Begin( const CryptographicID&accessToken, USHORT serverBasePort, const std::function<String()>&asyncGetServerPassword )
	{
		if (detail::process.IsRunning())
		{
			ASSERT__(accessToken == detail::accessToken);
			return;
		}
		detail::serverBasePort = serverBasePort;
		detail::asyncGetServerPassword = asyncGetServerPassword;
		detail::accessToken = accessToken;
		detail::Start();
		if (detail::eventKey == InvalidIndex)
			detail::eventKey = GlobalEvents::applicationTerminate.Register(detail::EventEnd);
	}

	void End()
	{
		detail::EventEnd();
	}


}





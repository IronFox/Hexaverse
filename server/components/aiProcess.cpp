#include <global_root.h>
#include "aiProcess.h"
#include <global_string.h>
#include "../../shared/uniqueID.h"
#include <general/timer.h>
#include <general/monitoredProcess.h>
#include <globalEvents.h>
#include <logEvent.h>

namespace AIProcess
{
	namespace detail
	{
		CryptographicID	accessToken;

		MonitoredProcess	process;
		index_t	eventKey = InvalidIndex;

		void	Start()
		{
			char parameterBuffer[32768];
			PathString	path,
						workingDirectory = "..\\client\\";
			#ifdef _DEBUG
				path = "..\\client\\.projector\\Debug\\Visual Client.exe";
			#else
				path = "..\\client\\Console Client" PLATFORM_TARGET_NAME_EXTENSION_STR ".exe";
			#endif
			//#ifdef _DEBUG
			//	path = "..\\client\\Debug\\client.exe";
			//#else
			//	path = "..\\client\\client.exe";
			//#endif


//			String executable = 

			sprintf_s(parameterBuffer,"aiAccessToken='%s' udpListen=false serverURL=localhost",accessToken.ToFullString().c_str());

			try
			{
				bool window = false;
				#ifdef _DEBUG
					window = true;
				#endif

				process.Start(workingDirectory,path,parameterBuffer,window);
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


	void Begin(const CryptographicID&accessToken)
	{
		if (detail::process.IsRunning())
		{
			ASSERT__(accessToken == detail::accessToken);
			return;
		}
		detail::accessToken = accessToken;
		detail::Start();
		if (detail::eventKey == InvalidIndex)
			detail::eventKey = GlobalEvents::applicationTerminate.Register(detail::EventEnd);
	}

	void End()
	{
		detail::EventEnd();
	}




	void Check()
	{
		detail::process.Check();
	}


}





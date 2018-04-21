#include <global_root.h>
#include "aiProcess.h"
#include <global_string.h>
#include <uniqueID.h>
#include <general/timer.h>
#include <io/monitoredProcess.h>
#include <globalEvents.h>
#include <logEvent.h>

namespace AIProcess
{
	namespace detail
	{
		CryptographicID	accessToken;

		MonitoredProcess	process(true);
		index_t	eventKey = InvalidIndex;

		void	Start()
		{
			char parameterBuffer[32768];
			PathString	path,
						workingDirectory = "..\\client\\";
			#ifdef _DEBUG
				path = "..\\client\\.projector\\Debug\\Visual Client" PLATFORM_TARGET_NAME_EXTENSION_STR ".exe";
			#elif defined _OPTIMIZED_DEBUG
				#ifdef PLATFORM_X64
					path = "..\\client\\.projector\\x64\\OptimizedDebug\\Console Client" PLATFORM_TARGET_NAME_EXTENSION_STR ".exe";
				#else
					path = "..\\client\\.projector\\OptimizedDebug\\Console Client" PLATFORM_TARGET_NAME_EXTENSION_STR ".exe";
				#endif
			#else
				path = "..\\client\\Console Client" PLATFORM_TARGET_NAME_EXTENSION_STR ".exe";
			#endif

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


}





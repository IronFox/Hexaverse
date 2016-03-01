#include <global_root.h>
#include "aiProcess.h"
#include <global_string.h>
#include "../../shared/uniqueID.h"
#include <general/timer.h>
#include <general/monitoredProcess.h>

namespace AIProcess
{
	namespace detail
	{
		CryptographicID	accessToken;

		MonitoredProcess	process;

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

			ASSERT3__(process.Start(workingDirectory,path,parameterBuffer,false), workingDirectory,path,parameterBuffer);
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
	}

	void End()
	{
		detail::process.Terminate();
	}




	void Check()
	{
		detail::process.Check();
	}


}





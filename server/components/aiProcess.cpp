#include <global_root.h>
#include "aiProcess.h"
#include <global_string.h>
#include "../../shared/uniqueID.h"
#include <general/timer.h>

namespace AIProcess
{
	namespace detail
	{
		CryptographicID	accessToken;
		PROCESS_INFORMATION	infoOut;
		Timer::Time		lastCheck = timer.Now();
		bool			isStarted = false;

		void Flush()
		{
			CloseHandle(infoOut.hProcess);
			CloseHandle(infoOut.hThread);
			infoOut.hProcess = INVALID_HANDLE_VALUE;
			infoOut.hThread = INVALID_HANDLE_VALUE;
			infoOut.dwThreadId = 0;
			infoOut.dwProcessId = 0;
			isStarted = false;
		}
	
		void	End()
		{
			ASSERT__(isStarted);
			isStarted = false;
			PostThreadMessageA(infoOut.dwThreadId,WM_DESTROY,0,0);
			if (WaitForSingleObject(infoOut.hProcess,2000) != 0)
				TerminateProcess(infoOut.hProcess,0);

			Flush();
		}

	
		void	Start()
		{
			ASSERT__(!isStarted);
			isStarted = true;
			char parameterBuffer[32768];
			String	path,
					workingDirectory = "..\\client\\";
			path = "..\\client\\client_console.exe";
			//#ifdef _DEBUG
			//	path = "..\\client\\Debug\\client.exe";
			//#else
			//	path = "..\\client\\client.exe";
			//#endif


			sprintf_s(parameterBuffer,"client.exe aiAccessToken='%s' udpListen=false serverURL=localhost",accessToken.ToFullString().c_str());


			STARTUPINFOA	infoIn;
			ZeroMemory(&infoIn,sizeof(infoIn));
			infoIn.cb = sizeof(infoIn);

			//infoIn.wShowWindow = SW_MINIMIZE;
			//infoIn.dwFlags |= STARTF_USESHOWWINDOW;	//dx11 doesn't start well minimized

			infoIn.dwXSize = 100;
			infoIn.dwYSize = 100;
			//infoIn.dwFlags |= STARTF_USESIZE;

			DWORD flags = 0;
			#ifdef _DEBUG
				//flags |= DEBUG_PROCESS;
			#endif
			flags |= CREATE_NEW_CONSOLE;
			
			BOOL rs = CreateProcessA(path.c_str(),parameterBuffer,NULL,NULL,FALSE,flags,NULL,workingDirectory.c_str(),&infoIn,&infoOut);
			ASSERT__(rs);

		}
	
	}


	void Begin(const CryptographicID&accessToken)
	{
		if (detail::isStarted)
		{
			ASSERT__(accessToken == detail::accessToken);
			return;
		}
		detail::accessToken = accessToken;
		detail::Start();
	}

	void End()
	{
		if (detail::isStarted)
			detail::End();
	}




	void Check()
	{
		if (timer.GetSecondsSince(detail::lastCheck) > 1)
		{
			DWORD code;
			if (!GetExitCodeProcess(detail::infoOut.hProcess,&code) || code != STILL_RUNNING)
			{
				detail::Flush();

				detail::Start();
			}
			detail::lastCheck = timer.Now();
		}	
	}


}





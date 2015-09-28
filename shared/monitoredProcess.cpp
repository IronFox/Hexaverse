#include <global_root.h>
#include "monitoredProcess.h"
#include <io/file_system.h>

bool		MonitoredProcess::Start(const String&workingDirectory, const String&executablePath, const String&parametersWithoutExecutableName)
{
	parameters = '"'+FileSystem::ExtractFileNameExt(executablePath)+"\" "+parametersWithoutExecutableName;
	this->executablePath = executablePath;
	this->workingDirectory = workingDirectory;
	return Restart();
}


bool	MonitoredProcess::Restart()
{
	ASSERT__(!isStarted);
	isStarted = true;
	
//	char parameterBuffer[32768];

	//sprintf_s(parameterBuffer,"\"Console Client" PLATFORM_TARGET_NAME_EXTENSION_STR ".exe\" aiAccessToken='%s' udpListen=false serverURL=localhost",accessToken.ToFullString().c_str());


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
			
	BOOL rs = CreateProcessA(executablePath.c_str(),parameters.mutablePointer(),NULL,NULL,FALSE,flags,NULL,workingDirectory.c_str(),&infoIn,&infoOut);
	return rs != 0;
}

void		MonitoredProcess::Check()
{
	if (timer.GetSecondsSince(lastCheck) > 1)
	{
		DWORD code;
		if (!GetExitCodeProcess(infoOut.hProcess,&code) || code != STILL_RUNNING)
		{
			Flush();
			Restart();
		}
		lastCheck = timer.Now();
	}	
}


void MonitoredProcess::Flush()
{
	CloseHandle(infoOut.hProcess);
	CloseHandle(infoOut.hThread);
	infoOut.hProcess = INVALID_HANDLE_VALUE;
	infoOut.hThread = INVALID_HANDLE_VALUE;
	infoOut.dwThreadId = 0;
	infoOut.dwProcessId = 0;
	isStarted = false;
}

void		MonitoredProcess::Terminate()
{
	if (isStarted)
	{
		isStarted = false;
		PostThreadMessageA(infoOut.dwThreadId,WM_DESTROY,0,0);
		if (WaitForSingleObject(infoOut.hProcess,2000) != 0)
			TerminateProcess(infoOut.hProcess,0);
		Flush();
	}
}


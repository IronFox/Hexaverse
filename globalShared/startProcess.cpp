#include "startProcess.h"
#include <io/monitoredProcess.h>
#include <io/file_system.h>

namespace Process
{

	void Start(const DeltaWorks::PathString&workingDirectory, const DeltaWorks::PathString&executable, const DeltaWorks::PathString&parameters/*=""*/)
	{
		using namespace DeltaWorks;

		FileSystem::Folder folder;
		MonitoredProcess::Paths paths;
		auto f = folder.FindDirectory(workingDirectory);
		if (!f)
		{
			FATAL__("Trying to execute in non-existing working directory "+FileSystem::PathToString(workingDirectory));
			return;
		}
		folder = FileSystem::Folder(f->GetLocation());
		f = folder.FindFile(executable);
		if (!f)
		{
			FATAL__("Trying to execute non-existing executable "+FileSystem::PathToString(executable));
			return;
		}
		paths.executablePath = f->GetLocation();

		paths.workingDirectory = folder.GetLocation();
		MonitoredProcess::Sanitize(paths);


		ShellExecuteW(NULL, NULL, paths.executablePath.c_str(), parameters.c_str(), paths.workingDirectory.c_str(), SW_SHOWNORMAL);
	}


	void					StartProject(const DeltaWorks::PathString&workingDirectory, const DeltaWorks::PathString&baseExecutableName, const DeltaWorks::PathString&parameters)
	{
		Start(workingDirectory,LocateProjectExecutable(workingDirectory,baseExecutableName),parameters);
	}

	DeltaWorks::PathString LocateProjectExecutable(const DeltaWorks::PathString&baseDirectory, const DeltaWorks::PathString&baseExecutableName)
	{
	
		#ifdef _DEBUG
			#ifdef PLATFORM_X64
				return baseDirectory+"\\.projector\\x64\\Debug\\"+baseExecutableName + PLATFORM_TARGET_NAME_EXTENSION_STR ".exe";
			#else
				return baseDirectory+"\\.projector\\Debug\\"+baseExecutableName + PLATFORM_TARGET_NAME_EXTENSION_STR ".exe";
			#endif
		#elif defined _OPTIMIZED_DEBUG
			#ifdef PLATFORM_X64
				return baseDirectory+"\\.projector\\x64\\OptimizedDebug\\"+baseExecutableName+ PLATFORM_TARGET_NAME_EXTENSION_STR ".exe";
			#else
				return baseDirectory+"\\.projector\\OptimizedDebug\\"+baseExecutableName+PLATFORM_TARGET_NAME_EXTENSION_STR ".exe";
			#endif
		#else
			return baseDirectory+"\\"+baseExecutableName+ PLATFORM_TARGET_NAME_EXTENSION_STR ".exe";
		#endif
	}

}

#include "startProcess.h"
#include <io/monitoredProcess.h>
#include <io/file_system.h>


void StartProcess(const DeltaWorks::PathString&workingDirectory, const DeltaWorks::PathString&executable, const DeltaWorks::PathString&parameters/*=""*/)
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

	paths.workingDirectory = f->GetLocation();
	MonitoredProcess::Sanitize(paths);


	ShellExecuteW(NULL, NULL, paths.executablePath.c_str(), parameters.c_str(), paths.workingDirectory.c_str(), SW_SHOWNORMAL);
}


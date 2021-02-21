#pragma once

#include <global_string.h>

namespace Process
{
	void					Start(const DeltaWorks::PathString&workingDirectory, const DeltaWorks::PathString&executable, const DeltaWorks::PathString&parameters="");
	void					StartProject(const DeltaWorks::PathString&workingDirectory, const DeltaWorks::PathString&baseExecutableName, const DeltaWorks::PathString&parameters="");

	/**
	Identifies the path to the executable as it would be compiled using the same configuration as the current selection.
	Essentially, it points towards executables in the given base directory when compiling release, /.projector/ subfolders
	otherwise.
	The function merely constructs the path. It does not check if the executable actually exists.

	@param baseDirectory Relative or absolute directory path to the project root without trailing slash
	@param baseExecutableName Name of the executable without extension
	@return Assembled path to the executable
	*/
	DeltaWorks::PathString	LocateProjectExecutable(const DeltaWorks::PathString&baseDirectory, const DeltaWorks::PathString&baseExecutableName);
}


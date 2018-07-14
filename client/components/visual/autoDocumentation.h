#pragma once


#ifdef HAS_WINDOW	//needs to render stuff

#include <io/file_system.h>

namespace AutoDocumentation
{
	/**
	Updates auto-documentation, using the currently loaded configuration.
	Connection to some server should exist.

	@param targetFolder Folder to write documentation into. Must exist. Sub folders will be created as necessary
	*/
	void Run(DeltaWorks::FileSystem::Folder targetFolder);
}


#endif

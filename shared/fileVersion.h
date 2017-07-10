#pragma once

#include <global_root.h>

namespace ApplicationInterface
{
	#ifndef CLIENT
		extern UINT32			globalVersionOfCurrentFile;

		static const UINT32	SupportedFileVersion = 1046;	//doubles as current file version to write to files

		struct FileVersions
		{
			enum version_t : UINT32
			{
				HasUUIDInConstructionJobs = 1041,
				FirstVersionWithVariables = 1042,
				MarkersHaveOwner = 1043,
				CurrentEntityHealthIsFloat = 1044,
				EntitiesHaveFlags = 1045,
				NewSerializationEngine = 1046,	//i suspect loading anything before this version could cause losses. so far it seems to work (most of the serialization is, in fact, identical, but not all)
			};
		};
	#endif
}

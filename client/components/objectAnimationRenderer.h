#pragma once
#include <io/file_system.h>


/**
Renders sequences of png files of select types to a given output folder.
Useful for very specific use cases only.
*/
namespace ObjectAnimationRenderer
{
	void	RenderTo(const FileSystem::Folder&);
}

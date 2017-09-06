#pragma once

#include <global_string.h>
#include <io/cryptographic_hash.h>

using namespace DeltaWorks;

/**
Management structure for scripts cached locally and transferred to clients on demand
*/
namespace ScriptCache
{
	class Script
	{
	public:
		SHA256::HashContainer	identifier;
		PathString				filename;
		String					content;
		Ctr::Array<BYTE>				compressed;


		bool					Recompress();
	};
	typedef std::shared_ptr<Script>	PScript;

	/**
	Loads the specified script file. Returns cached data if the specified file is requested multiple times
	*/
	PScript		Load(const PathString&);

	PScript		LoadContent(const String&content);

	PScript		Lookup(const SHA256::HashContainer&id);
	PScript		DefineFromFleetingCompressedData(const SHA256::HashContainer&id,Ctr::ArrayData<BYTE>&data, bool createFile=true);

	/**
	Cleans up outdated cache files on client.
	Does currently nothing on server, but should be called for future expansions
	*/
	void		Create();


}

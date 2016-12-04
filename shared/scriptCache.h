#pragma once

#include <global_string.h>
#include <io/cryptographic_hash.h>


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
		Array<BYTE>				compressed;
	};
	typedef std::shared_ptr<Script>	PScript;


	PScript		Fetch(const PathString&);
	PScript		Lookup(const SHA256::HashContainer&id);


}

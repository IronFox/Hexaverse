#include <global_root.h>
#include "scriptCache.h"
#include <container/hashtable.h>
#include <io/file_system.h>
#include <io/random_access_file.h>
#include <third_party/bz2/bz2.h>
#include "logEvent.h"

namespace ScriptCache
{
	GenericHashTable<PathString,PScript>	map;
	GenericHashTable<SHA256::HashContainer,PScript,PODHash>	hashMap;


	PScript		Fetch(const PathString&path)
	{
		FileSystem::File rs;
		ASSERT1__(FileSystem::Folder().Find(path,rs,true),path);
		PScript*script = map.QueryPointer(rs.GetLocation());
		if (script)
			return *script;

		LogEvent(EventClass::Preset::configurationPrint,"Loading script file '"+String(path)+"'...");

		PScript s(new Script());
		s->filename = rs.GetLocation();

		RandomAccessFile	file;
		if (!file.Open(s->filename.c_str()))
			FATAL__("Unable to load script file '"+String(s->filename)+"'");
		UINT len = file.GetSize();
		s->content.setLength(len);

		file.Extract(0,s->content.mutablePointer(),len);
		file.Close();

		s->compressed.SetSize(len+50);
		size_t compressed = BZ2::compress(s->content.c_str(),len,s->compressed.pointer(),len+50);
		if (!compressed)
			FATAL__("Script compression of file '"+String(s->filename)+"' failed");

		s->compressed.resizePreserveContent(compressed);
		SHA256::Hash(s->compressed.pointer(),s->compressed.size(),s->identifier);

		map.Set(s->filename,s);
		hashMap.Set(s->identifier,s);
		return s;
	}

	PScript		Lookup(const SHA256::HashContainer&id)
	{
		PScript*rs = hashMap.QueryPointer(id);
		return rs ? *rs : PScript();
	}


}

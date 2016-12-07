#include <global_root.h>
#include "scriptCache.h"
#include <container/hashtable.h>
#include <io/file_system.h>
#include <io/random_access_file.h>
#include <third_party/bz2/bz2.h>
#include "logEvent.h"
#include "uniqueID.h"

namespace ScriptCache
{
	GenericHashTable<PathString,PScript>	map;
	GenericHashTable<SHA256::HashContainer,PScript,PODHash>	hashMap;


	PScript		Load(const PathString&path)
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

		s->compressed.SetSize(4/*decompressed size*/  + len /*worst expected size=uncompressed*/ +50 /*overhead*/);
		size_t compressed = BZ2::compress(s->content.c_str(),len,s->compressed.pointer()+4,len+50);
		if (!compressed)
			FATAL__("Script compression of file '"+String(s->filename)+"' failed");
		(*(UINT32*)s->compressed.pointer()) = (UINT32)len;

		s->compressed.resizePreserveContent(compressed+4);
		SHA256::Hash(s->compressed.pointer(),s->compressed.size(),s->identifier);

		map.Set(s->filename,s);
		hashMap.Set(s->identifier,s);

		ASSERT_LESS__(compressed,32768);	//max transferrable byte count in current version

		LogEvent(EventClass::Preset::configurationPrint,"  compressed to "+String(compressed)+" bytes ("+String((float)compressed/(float)len * 100.f)+"%)");

		return s;
	}

	PScript		Lookup(const SHA256::HashContainer&id)
	{
		PScript*rs = hashMap.QueryPointer(id);
		return rs ? *rs : PScript();
	}

	PScript		DefineFromFleetingCompressedData(const SHA256::HashContainer&id, ArrayData<BYTE>&data)
	{
		if (data.GetLength() < 4)
		{
			LogEvent(EventClass::Preset::error,"Unexpected compressed data received for script "+Bin2Str::ConvertPOD(id)+".");
			return PScript();
		}

		PScript script;

		if (hashMap.Query(id,script))
		{
			LogEvent(EventClass::Preset::error,"Attempting to redefine script cache entry for "+Bin2Str::ConvertPOD(id)+". Ignoring...");
			return script;
		}

		UINT32 uncompressedSize = *(const UINT32*)data.pointer();
		String decompressed = TStringLength( uncompressedSize );
		size_t rsSize = BZ2::decompress(data.pointer()+4,data.GetLength()-4,decompressed.mutablePointer(),decompressed.length());
		if (uncompressedSize != rsSize)
		{
			LogEvent(EventClass::Preset::error,"Decompression of script '"+Bin2Str::ConvertPOD(id)+"' failed. Expected "+String(uncompressedSize)+" bytes in uncompressed data, but found "+ String(rsSize));
			return PScript();
		}

		LogEvent(EventClass::Preset::uncommon,"Defining new script cache entry for "+Bin2Str::ConvertPOD(id)+" using script of "+String(rsSize)+" bytes");
		script.reset(new Script());
		hashMap.Set(id,script);
		script->compressed.adoptData(data);
		script->content.adoptData(decompressed);
		script->identifier = id;
		return script;
	}



}

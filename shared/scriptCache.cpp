#include <global_root.h>
#include "scriptCache.h"
#include <container/hashtable.h>
#include <io/file_system.h>
#include <io/random_access_file.h>
#include <third_party/bz2/bz2.h>
#include "logEvent.h"
#include "uniqueID.h"
#include "runtimeConfig.h"

namespace ScriptCache
{
	GenericHashTable<PathString,std::pair<PScript,FileSystem::ftime_t> >	map;
	GenericHashTable<SHA256::HashContainer,PScript,PODHash>	hashMap;


	void	Script::Recompress()
	{
		size_t len = content.length();
		compressed.SetSize(4/*decompressed size*/  + len /*worst expected size=uncompressed*/ +50 /*overhead*/);
		size_t compressed = BZ2::compress(content.c_str(),len,this->compressed.pointer()+4,len+50);
		if (!compressed)
		{
			if (filename.IsNotEmpty())
				FATAL__("Script compression of file '"+String(filename)+"' failed")
			else
				FATAL__("Script compression failed");
		}
		(*(UINT32*)this->compressed.pointer()) = (UINT32)len;

		this->compressed.resizePreserveContent(compressed+4);
		SHA256::Hash(this->compressed.pointer(),this->compressed.size(),this->identifier);

		ASSERT_LESS__(compressed,32768);	//max transferrable byte count in current version
		LogEvent(EventClass::Preset::configurationPrint,"  compressed to "+String(compressed)+" bytes ("+String((float)compressed/(float)len * 100.f)+"%)");
		LogEvent(EventClass::Preset::configurationPrint,"  ID updated to "+Bin2Str::ConvertPOD(this->identifier));

	}

	PScript		LoadContent(const String&content)
	{
		LogEvent(EventClass::Preset::configurationPrint,"Loading raw script ...");

		PScript s(new Script());
		s->content = content;

		s->Recompress();

		PScript other = Lookup(s->identifier);
		if (other)
		{
			LogEvent(EventClass::Preset::configurationPrint,"  File mapped as '"+Bin2Str::ConvertPOD(s->identifier)+"' did not change. Returning previous instance.");
			return other;
		}

		hashMap.Set(s->identifier,s);

		return s;

	}

	PScript		Load(const PathString&path)
	{
		FileSystem::File rs;
		ASSERT1__(FileSystem::Folder().Find(path,rs,true),path);
		auto*cached = map.QueryPointer(rs.GetLocation());
		if (cached && FileSystem::GetModificationTime(path) == cached->second)
			return cached->first;

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

		s->Recompress();

		PScript other = Lookup(s->identifier);
		if (other)
		{
			LogEvent(EventClass::Preset::configurationPrint,"  File mapped as '"+Bin2Str::ConvertPOD(s->identifier)+"' did not change. Returning previous instance.");
			return other;
		}

		map.Set(s->filename,std::make_pair(s,FileSystem::GetModificationTime(path)) );
		hashMap.Set(s->identifier,s);

		return s;
	}


	static PathString	GetFilePath(const SHA256::HashContainer&id)
	{
		return RuntimeConfig::ConfigFile(PathString("cache/"+Bin2Str::ConvertPOD(id,InvalidIndex)));
	}

	PScript		Lookup(const SHA256::HashContainer&id)
	{
		PScript rs;
		if (hashMap.Query(id,rs))
			return rs;


		PathString path = GetFilePath(id);
		if (!FileSystem::IsFile(path))
			return rs;	//empty
		FILE*f = FOPEN(path.c_str(),"rb");
		if (!f)
			return rs;
		FileSystem::fsize_t size = FileSystem::GetFileSize(path);
		Array<BYTE> compressed(size);
		size_t read = fread(compressed.pointer(),1,size,f);
		fclose(f);
		if (read != size)
		{
			return rs;
		}
		SHA256::HashContainer check;
		SHA256::Hash(compressed.pointer(),size,check);
		if (check != id)
		{
			LogEvent(EventClass::Preset::error,"Requested script "+Bin2Str::ConvertPOD(id)+" was cached to local drive but hash code does not match. Removing file");
			FileSystem::UnlinkFile(path);
			return rs;
		}
		LogEvent(EventClass::Preset::configuration,"Requested script "+Bin2Str::ConvertPOD(id)+" was cached to local drive. Restoring");

		return DefineFromFleetingCompressedData(id,compressed,false);
	}

	PScript		DefineFromFleetingCompressedData(const SHA256::HashContainer&id, ArrayData<BYTE>&data, bool createFile/*=true*/)
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

		if (createFile)
		{
			PathString path = GetFilePath(id);
			FileSystem::CreateDirectory(FileSystem::ExtractFileDir(path));
			FILE*f = FOPEN(path.c_str(),"w+b");
			if (f)
			{
				size_t written = fwrite(data.pointer(),1,data.size(),f);
				fclose(f);
				if (written != data.size())
				{
					LogEvent(EventClass::Preset::error,"Warning: Unable to write entire received resource "+Bin2Str::ConvertPOD(id)+" to file: "+String(path)+". Erasing remains");
					FileSystem::UnlinkFile(path);
				}
				else
					LogEvent(EventClass::Preset::configuration,"Resource "+Bin2Str::ConvertPOD(id)+" cached to file: "+String(path));

			}
			else
			{
				LogEvent(EventClass::Preset::error,"Warning: Unable to write received resource "+Bin2Str::ConvertPOD(id)+" to file: "+String(path));
			}

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

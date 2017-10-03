#include <global_root.h>
#include "scriptCache.h"
#include <container/hashtable.h>
#include <io/file_system.h>
#include <io/random_access_file.h>
#include <third_party/bz2/bz2.h>
#include <container/sorter.h>
#include <container/buffer.h>
#include "logEvent.h"
#include "uniqueID.h"
#include "runtimeConfig.h"
#include <time.h>

namespace ScriptCache
{
	Ctr::GenericHashTable<PathString,std::pair<PScript,FileSystem::ftime_t> >	map;
	Ctr::GenericHashTable<SHA256::HashContainer,PScript>	hashMap;

	static const PathString cacheFileExtension = ".script";


	bool	Script::Recompress()
	{
		size_t len = content.length();
		compressed.SetSize(4/*decompressed size*/  + len /*worst expected size=uncompressed*/ +50 /*overhead*/);
		size_t compressed = BZ2::compress(content.c_str(),len,this->compressed.pointer()+4,len+50);
		if (!compressed)	//this should not happen
		{
			if (filename.IsNotEmpty())
				FATAL__("Script compression of file '"+String(filename)+"' failed")
			else
				FATAL__("Script compression failed");
		}
		if (compressed > 32768)	//max transferrable byte count in current protocol version. Might happen
		{
			LogEvent(EventClass::Preset::common,"  compressed size ("+String(compressed)+" bytes) exceeds maximum transferable size (32768 bytes)");
			return false;
		}
		(*(UINT32*)this->compressed.pointer()) = (UINT32)len;

		this->compressed.ResizePreserveContent(compressed+4);
		SHA256::Hash(this->compressed.pointer(),this->compressed.size(),this->identifier);

		LogEvent(EventClass::Preset::common,"  BZ2: "+String(len)+" bytes compressed to "+String(compressed)+" bytes (down to "+String(M::Round((float)compressed/(float)len * 100.f,1))+"%)");
		LogEvent(EventClass::Preset::common,"  ID updated to "+Bin2Str::ConvertPOD(this->identifier));
		return true;
	}

	PScript		LoadContent(const String&content)
	{
		LogEvent(EventClass::Preset::common,"Loading raw script ...");

		PScript s(new Script());
		s->content = content;

		if (!s->Recompress())
			return PScript();

		PScript other = Lookup(s->identifier);
		if (other)
		{
			LogEvent(EventClass::Preset::common,"  File mapped as '"+Bin2Str::ConvertPOD(s->identifier)+"' did not change. Returning previous instance.");
			return other;
		}

		hashMap.Set(s->identifier,s);

		return s;

	}

	PScript		Load(const PathString&path)
	{
		FileSystem::File rs;
		ASSERT1__(FileSystem::Folder().FindFile(path,rs),path);
		auto*cached = map.QueryPointer(rs.GetLocation());
		if (cached && FileSystem::GetModificationTime(path) == cached->second)
			return cached->first;

		LogEvent(EventClass::Preset::common,"Loading script file '"+String(path)+"'...");

		PScript s(new Script());
		s->filename = rs.GetLocation();

		RandomAccessFile	file;
		if (!file.Open(s->filename.c_str()))
			FATAL__("Unable to load script file '"+String(s->filename)+"'");
		UINT len = file.GetSize();
		s->content.setLength(len);

		file.Extract(0,s->content.mutablePointer(),len);
		file.Close();

		if (!s->Recompress())
			return PScript();

		PScript other = Lookup(s->identifier);
		if (other)
		{
			LogEvent(EventClass::Preset::common,"  File mapped as '"+Bin2Str::ConvertPOD(s->identifier)+"' did not change. Returning previous instance.");
			return other;
		}

		map.Set(s->filename,std::make_pair(s,FileSystem::GetModificationTime(path)) );
		hashMap.Set(s->identifier,s);

		return s;
	}


	static FileSystem::Folder	GetCacheDirectory()
	{
		FileSystem::File file;
		RuntimeConfig::LocateInConfigDir("cache/",file);
		FileSystem::ForceCreateDirectory(file);
		return file;
	}

	static PathString	GetFileCachePath(const SHA256::HashContainer&id)
	{
		FileSystem::File f;
		if (!GetCacheDirectory().FindFile(PathString(Bin2Str::ConvertPOD(id,InvalidIndex))+cacheFileExtension,f,false))
			return "";
		return f.GetLocation();
	}

	PScript		Lookup(const SHA256::HashContainer&id)
	{
		PScript rs;
		if (hashMap.Query(id,rs))
			return rs;


		PathString path = GetFileCachePath(id);
		if (!FileSystem::IsFile(path))
			return rs;	//empty
		FILE*f = FOPEN(path.c_str(),"rb");
		if (!f)
			return rs;
		FileSystem::fsize_t size = FileSystem::GetFileSize(path);
		Ctr::Array<BYTE> compressed(size);
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

	PScript		DefineFromFleetingCompressedData(const SHA256::HashContainer&id, Ctr::ArrayData<BYTE>&data, bool createFile/*=true*/)
	{
		if (data.GetLength() < 4)
		{
			LogEvent(EventClass::Preset::error,"Unexpected compressed data received for script "+Bin2Str::ConvertPOD(id)+".");
			return PScript();
		}

		PScript script;

		if (hashMap.Query(id,script))
		{
			LogEvent(EventClass::Preset::common,"Attempting to redefine script cache entry for "+Bin2Str::ConvertPOD(id)+". Ignoring...");
			return script;
		}

		if (createFile)
		{
			PathString path = GetFileCachePath(id);
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

		LogEvent(EventClass::Preset::common,"Defining new cache entry for script "+Bin2Str::ConvertPOD(id)+" ("+String(rsSize)+" uncompressed bytes)");
		script.reset(new Script());
		hashMap.Set(id,script);
		script->compressed.adoptData(data);
		script->content.adoptData(decompressed);
		script->identifier = id;
		return script;
	}

	struct TCacheSortEntry
	{
		FileSystem::ftime_t	stamp;
		FileSystem::File	file;


		bool				operator>(const TCacheSortEntry&other) const
		{
			return stamp > other.stamp;
		}
		bool				operator<(const TCacheSortEntry&other) const
		{
			return stamp < other.stamp;
		}

		void				swap(TCacheSortEntry&other)
		{
			file.swap(other.file);
			swp(stamp,other.stamp);
		}

		friend void			swap(TCacheSortEntry&a, TCacheSortEntry&b)
		{
			a.swap(b);
		}
	};



	void		Create()
	{
		LogEvent(EventClass::Preset::common,"Creating script cache");

		#ifdef CLIENT
			FileSystem::Folder f = GetCacheDirectory();
			FileSystem::File file;
			if (f.IsValidLocation())
			{
				const FileSystem::ftime_t baseTime = (FileSystem::ftime_t)time(NULL);

				Buffer0<TCacheSortEntry,Swap> entries;
				Buffer0<FileSystem::File,Swap> toDelete;
				f.Rewind();
				while (f.NextFile(cacheFileExtension, file))
				{
					FileSystem::ftime_t t = file.GetModificationTime();
					if (t > baseTime)
					{
						LogEvent(EventClass::Preset::common,"ScriptCache: "+String(file.GetName())+" has bad timestamp. Forcing deletion");
						toDelete << file;
					}
					else
					{
						entries.Append().stamp = t;
						entries.Last().file.swap(file);
					}
				}
				if (entries.Count() > 20)
				{
					count_t erase = entries.Count()-20;
					LogEvent(EventClass::Preset::common,"ScriptCache: Found "+String(entries.Count())+" cache entries, exceeding max cache size by "+String(erase));
					Sorting::ByOperator::QuickSort(entries);
					for (index_t i = 0; i < erase; i++)
					{
						toDelete.Append().swap( entries[i].file );
					}
				}
				if (toDelete.IsNotEmpty())
				{
					LogEvent(EventClass::Preset::common,"ScriptCache: Deleting "+String(toDelete.Count())+" file(s)");
					foreach (toDelete,f)
					{
						LogEvent(EventClass::Preset::common,"ScriptCache: Deleting "+String(f->GetLocation())+"...");
						if (!f->Unlink())
						{
							LogEvent(EventClass::Preset::error,"ScriptCache: Warning: Unable to delete "+String(f->GetLocation())+"...");
						}
					}
				}

			}

			LogEvent(EventClass::Preset::common,"ScriptCache: All done");
		#endif
	}


	void		Destroy()
	{
		LogEvent(EventClass::Preset::common,"Destroying script cache");
		map.Clear();
		hashMap.Clear();
	}
}

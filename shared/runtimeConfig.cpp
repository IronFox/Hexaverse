#include <global_root.h>
#include "runtimeConfig.h"

#include <algorithm>
//#include <general/undef.h>
#include <io/file_system.h>
#include <container/hashtable.h>

#undef NEAR
#define NEAR	//weeeird. but if i don't do this, the following include will explode
#include <ShlObj.h>



namespace RuntimeConfig
{
	#ifndef CLIENT
		bool		loadingPhase = false;
	#endif

	DoomsdayWeapons doomsdayWeapons;
	bool empZones = true, autoNewRound = false;
	GameGoal gameGoal;

	bool hibernationSystemEnabled = false;
	bool bombsExplodeWhenDestroyed = true;
	bool tutorialMode = false;
	RoundNumber roundNumber;
	Phase	phase;

	FileSystem::Folder base,actual;

	void		LocateInConfigDir(PathString base, FileSystem::File&outFile)
	{
		#if SYSTEM==WINDOWS
			base.FindAndReplace(L'/',L'\\');
		#else
			base.FindAndReplace(L'\\',L'/');
		#endif
		if (!actual.Find(base,outFile,FileSystem::FindFlags::None))
		{
			FATAL__("Unable to reference file '"+String(base)+"' relative to '"+String(actual.GetLocation())+"'");
		}
	}


	PathString	ConfigFile(const PathString&base)
	{
		FileSystem::File f;
		LocateInConfigDir(base,f);
		return f.GetLocation();
	}

	//PathString ConfigFile(const String&base)
	//{
	//	return actual.GetLocation() + FOLDER_SLASH + base;
	//}

	PathString		ConfigDir()	{return actual.GetLocation();}


	void	InitConfigDir(const String&subDir)
	{
		wchar_t folder[MAX_PATH];
		HRESULT hr = SHGetFolderPathW(0, CSIDL_MYDOCUMENTS, 0, 0, folder);
		base = folder;
		ASSERT1__(base.IsValidLocation(),base.GetLocation());

		FileSystem::File file;
		base.Find(L"./My Games/Hexaverse/"+PathString(subDir),file,FileSystem::FindFlags::None);
		FileSystem::ForceCreateDirectory(file,file);	//returns if directory exists
		actual = file;
	}


	namespace UnitLimits
	{
		static DeltaWorks::GenericHashTable<Objects::EntityType,UINT16>	map;

		static bool isSet = false;
		static bool isEnabled = true;

		bool		IsEnabled()
		{
			return isEnabled;
		}

		void		Enable(bool doEnable)
		{
			isEnabled = doEnable;
		}


		bool		AnySet()
		{
			return isSet;
		}
		void		Reset()
		{
			isSet = false;
			map.Clear();
		}
		bool		Set(Objects::EntityType entry, int value)
		{
			if (entry.IsEmpty() || entry.IsInvalid())
				return false;
			if (value < 0)
				return false;
			isSet = true;
			//ASSERT__(value >= 0);
			UINT16 val = (UINT16)std::min<UINT16>(0xFFFF,value);
			if (val != 0)
				map.Set(entry,val);
			else
				map.Unset(entry);
			return true;
		}
		count_t		Get(Objects::EntityType entry, bool ignoreEnabledState/*=false*/)
		{
			return (isEnabled || ignoreEnabledState) ? map.Get(entry,0) : 0xFFFF;
		}

		void		Export(Ctr::Array<std::pair<Objects::EntityType,UINT16> >&result)
		{
			if (isEnabled)
			{
				const count_t nonZero = map.Count();

				result.SetSize(nonZero);
				index_t at = 0;
				map.VisitAllEntries([&result,&at](const Objects::EntityType&key, UINT16 value)
				{
					result[at++] = std::make_pair(key,value);
				});
			}
			else
			{
				result.Clear();
			}
		}

		void		Import(const Ctr::Array<std::pair<Objects::EntityType,UINT16> >&input)
		{
			Reset();
			isSet = input.IsNotEmpty();
			foreach (input, entry)
				map.Set(entry->first,entry->second);
		}

	}

}

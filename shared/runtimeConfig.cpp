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


	ConfigurationScope	myConfig;
	ConfigurationScope	sharedConfig;
	/*static*/ bool ConfigurationScope::haveMyDocuments = false;
	/*static*/ FileSystem::Folder ConfigurationScope::myDocuments;



	void		ConfigurationScope::Lookup(PathString base, FileSystem::File&outFile)	const
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


	PathString	ConfigurationScope::Lookup(const PathString&base)	const
	{
		FileSystem::File f;
		Lookup(base,f);
		return f.GetLocation();
	}

	//PathString Lookup(const String&base)
	//{
	//	return actual.GetLocation() + FOLDER_SLASH + base;
	//}

	PathString		ConfigurationScope::GetRoot()	const {return actual.GetLocation();}

	/*static*/ void	ConfigurationScope::InitMyDocuments()
	{
		if (haveMyDocuments)
			return;
		haveMyDocuments = true;

		wchar_t folder[MAX_PATH];
		HRESULT hr = SHGetFolderPathW(0, CSIDL_MYDOCUMENTS, 0, 0, folder);
		myDocuments = folder;
		ASSERT1__(myDocuments.IsValidLocation(),myDocuments.GetLocation());
	}


	void	ConfigurationScope::InitConfigDir(const String&subDir)
	{
		InitMyDocuments();

		FileSystem::File file;
		myDocuments.Find(L"./My Games/Hexaverse/"+PathString(subDir),file,FileSystem::FindFlags::None);
		FileSystem::ForceCreateDirectory(file,file);	//returns if directory exists
		actual = file;
	}



	PathString				ConfigLookup(const PathString&base, bool favorMyConfig)
	{
		FileSystem::File f;
		ConfigLookup(base,f,favorMyConfig);
		return f.GetLocation();
	}

	void		ConfigLookup(PathString base, FileSystem::File&outFile, bool favorMyConfig)
	{
		if (favorMyConfig)
		{
			myConfig.Lookup(base,outFile);
			if (outFile.DoesExist())
				return;
			sharedConfig.Lookup(base,outFile);
			if (outFile.DoesExist())
				return;

			myConfig.Lookup(base,outFile);
		}
		else
		{
			sharedConfig.Lookup(base,outFile);
			if (outFile.DoesExist())
				return;
			myConfig.Lookup(base,outFile);
			if (outFile.DoesExist())
				return;

			sharedConfig.Lookup(base,outFile);
		}

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

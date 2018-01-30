#ifndef sharedRuntimeConfigH
#define sharedRuntimeConfigH

#include "stringTable.h"
#include <general/enumeration.h>
#include <global_string.h>
#include "roundNumber.h"
#include "objects/entityType.h"
#include <io/file_system.h>

namespace RuntimeConfig
{
	CONSTRUCT_ENUMERATION2(GameGoal, None, ConquerNexus);
	CONSTRUCT_ENUMERATION3(DoomsdayWeapons, Expose, Conceal, Disallow);
	CONSTRUCT_ENUMERATION7(Phase, 
							CommandPhase,
							StructurePhase,
							AttackPhase,
							MovementPhase,
							AnimationPhase, 
							EndOfGame,
							Uninitialized
							);

	extern DoomsdayWeapons doomsdayWeapons;
	extern GameGoal gameGoal;
	extern bool		empZones, autoNewRound;
	extern Phase	phase;

	extern bool		hibernationSystemEnabled;
	extern bool		bombsExplodeWhenDestroyed;
	extern bool		tutorialMode;

	typedef ::RoundNumber	RoundNumber;
	extern RoundNumber roundNumber;
	namespace UnitLimits
	{
		bool		IsEnabled();
		void		Enable(bool doEnable);
		void		Reset();
		bool		AnySet();
		bool		Set(Objects::EntityType, int);
		count_t		Get(Objects::EntityType, bool ignoreEnabledState=false);
		void		Export(Ctr::Array<std::pair<Objects::EntityType,UINT16> >&);
		void		Import(const Ctr::Array<std::pair<Objects::EntityType,UINT16> >&);
	}

	#ifndef CLIENT
		extern bool		loadingPhase;
	#endif


	/**
	Converts a relative path to an absolute one, in accordance with LocateInConfigDir()
	@param base Relative path to lookup. Existence of the specified file or folder is not required
	@return Absolute path
	*/
	PathString	ConfigFile(const PathString&base);
	inline PathString	ConfigFile(const char*base)	{return ConfigFile(PathString(base));}
	#ifdef _WIN32
		inline PathString	ConfigFile(const String&base)	{return ConfigFile(PathString(base));}
	#endif

	/**
	Determines the full path of a configuration (or cache) subfolder/file
	@param base Relative path to the file or folder in the configuration/cache directory. It need not exist
	@param[out] outFile File container for the resulting full path
	*/
	void		LocateInConfigDir(PathString base, FileSystem::File&outFile);

	/**
	Retrieves the absolute path to the configuration/cache root folder.
	See InitConfigDir() for details
	*/
	PathString	ConfigDir();

	/**
	Initializes the config user root directory. Must be called once at startup, before any calls to
	LocateInConfigDir(), ConfigFile(), or ConfigDir() are attempted.
	The final absolute root directory will be [My Documents]/My Games/Hexaverse/[subDir]/
	This directory is created if not found.
	The function triggers fatal exceptions if [My Documents] is not found, or the final directory could neither be found nor created.
	@param subDir Directory to lookup config/cache files and folders in
	*/
	void		InitConfigDir(const String&subDir);


	inline bool	IsACommandPhase(Phase p)	{return p == Phase::CommandPhase;}
	inline bool IsInAnyCommandPhase()	{return IsACommandPhase(phase);}
	inline bool	GameHasEnded() {return phase == Phase::EndOfGame;}
}


#endif


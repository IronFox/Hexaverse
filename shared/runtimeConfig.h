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
	static const PathString	savFileExtension = ".hsf";	//hexaverse save file

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
	Represents a single scope of configuration files.
	Typically initialized during program start.
	*/
	class ConfigurationScope
	{
	public:


		/**
		Determines the full path of a configuration (or cache) subfolder/file
		@param base Relative path to the file or folder in the configuration/cache directory. It need not exist
		@param[out] outFile File container for the resulting full path
		*/
		void		Lookup(PathString base, FileSystem::File&outFile) const;

		/**
		Converts a relative path to an absolute one, in accordance with Lookup()
		@param base Relative path to lookup. Existence of the specified file or folder is not required
		@return Absolute path
		*/
		PathString				Lookup(const PathString&base) const;
		inline PathString		Lookup(const char*base)	const {return Lookup(PathString(base));}
		#ifdef _WIN32
			inline PathString	Lookup(const String&base) const	{return Lookup(PathString(base));}
		#endif

		/**
		Retrieves the absolute path to the configuration/cache root folder.
		See InitConfigDir() for details
		*/
		PathString	GetRoot() const;

		/**
		Initializes the config user root directory. Must be called once at startup, before any calls to
		Lookup(), Lookup(), or GetRoot() are attempted.
		The final absolute root directory will be [My Documents]/My Games/Hexaverse/[subDir]/
		This directory is created if not found.
		The function triggers fatal exceptions if [My Documents] is not found, or the final directory could neither be found nor created.
		@param subDir Directory to lookup config/cache files and folders in
		*/
		void		InitConfigDir(const String&subDir);


	private:
		static void	InitMyDocuments();

		static bool haveMyDocuments;
		static FileSystem::Folder myDocuments;
		FileSystem::Folder actual;

	};

	/**
	Converts a relative path to the first found absolute one (favoring myConfig over sharedConfig)
	@param base Relative path to lookup. Existence of the specified file or folder is not required, but will determine order
	@return Absolute path
	*/
	PathString				ConfigLookup(const PathString&base, bool favorMyConfig);
	inline PathString		ConfigLookup(const char*base, bool favorMyConfig)	{return ConfigLookup(PathString(base),favorMyConfig);}
	#ifdef _WIN32
		inline PathString	ConfigLookup(const String&base, bool favorMyConfig) {return ConfigLookup(PathString(base),favorMyConfig);}
	#endif

	/**
	Determines the full path of a configuration (or cache) subfolder/file (favoring myConfig over sharedConfig)
	@param base Relative path to the file or folder in the configuration/cache directory. It need not exist, but existence will determine if a shared path is returned
	@param[out] outFile File container for the resulting full path
	*/
	void		ConfigLookup(PathString base, FileSystem::File&outFile, bool favorMyConfig);

	/**
	Configuration scope used only by the local application
	*/
	extern ConfigurationScope	myConfig;
	/**
	Configuration scope used by all game application
	*/
	extern ConfigurationScope	sharedConfig;


	inline bool	IsACommandPhase(Phase p)	{return p == Phase::CommandPhase;}
	inline bool IsInAnyCommandPhase()	{return IsACommandPhase(phase);}
	inline bool	GameHasEnded() {return phase == Phase::EndOfGame;}
}


#endif


#ifndef worldStateH
#define worldStateH

#include <objects.h>
#include <net/tcp.h>
#include <io/file_system.h>
#include <protocol.h>

namespace World
{



	/**
	Finds a randomized suitable starting location for a new faction.
	
	*/
	index_t			GetFactionStartLocation(TwisterRandom&random);


	/**
	Redefines the grid world based on the current reflection configuration, using the specified random variable to define seeds.
	Any existing session will be closed, all factions terminated before commencing.
	*/
	void			BuildConfiguredWorld(UINT32 seed);
	void			BuildConfiguredWorld(TwisterRandom&rnd);



	void			SimulateLiquidsIfEnabled();


	struct TPlanetIniFile
	{
		String				name;
		FileSystem::File	factoryDefault,userDefined;

		void				swap(TPlanetIniFile&other)
		{
			name.swap(other.name);
			factoryDefault.swap(other.factoryDefault);
			userDefined.swap(other.userDefined);
		}

		bool				operator<(const TPlanetIniFile&other) const
		{
			return name < other.name;
		}

		bool				operator>(const TPlanetIniFile&other) const
		{
			return name > other.name;
		}
	};

}

DECLARE_DEFAULT_STRATEGY(World::TPlanetIniFile,Swap);

namespace World
{
	/**
	Fetches the paths to all available planet ini files.
	Queries the file system to determine current state (all returned files existed at the moment of fetching).
	Any occurring error is logged, and an empty array is returned in this case.
	*/
	void			GetAvailablePlanetIniFiles(Ctr::Array<TPlanetIniFile>&rsOut);
	/**
	Fetches the unique names of all available planet types.
	Duplicates are filtered out. No distinction is made between user-defined and factory default types of identical name.
	Extensions are omitted.
	*/
	void			GetAvailablePlanetNames(Ctr::Array<String>&rsOut);
	/**
	Checks if the specified ini-filename is one of the available planet ini files
	@param filename Name of the file (name[.ini]), without folders
	@return true if it exists, false otherwise
	*/
	bool			IsPlanetIniFile(const String&filename);


	void			UpdateEMPZones(count_t temporaryFactionCountBonus=0);


	extern World::Data	spectatorWorldState;
	extern bool			terrainChanged;//!< Indicates that the terrain changed at any given time during the last processing round. Reset when entering command mode
	extern TUniqueID	worldID;


	static const constexpr int NumZones = 12;

	/**
	Bitset that contains all bits of all zones
	*/
	static const constexpr UINT16 AllZones = (UINT16)((1 << NumZones)-1);


	namespace Config
	{
		extern Grid::WorldDiscovery	worldDiscovery;

		extern UINT32	uplinkBandwidth/* = 500*/;
		extern bool		simulateLiquids/* = true*/;

	}



	void			SetUnlockedEMPZones(int unlocked);



	class DeltaContainer
	{
	public:
		Protocol::WorldDelta	delta[BaseGrid::NumFamilies];
		count_t					numDeltas = 0;

		bool					Process(World::Data&clientData,bool noDelta=false);
		void					CompressState(World::Data&sourceWorld,const Protocol::Timestamp&t);
	};


	namespace Network
	{
		bool		CheckCoverageSendTerrain(const Objects::PFaction&faction,const TCP::PPeer&peer);
		bool		CheckCoverageSendTerrain(const Objects::PFaction&faction);
		void		CheckAllFactionCoverageAndSendAsNeeded();

		/**
		Called automatically by Network::CloseSession()
		Flushes any residual network related data
		*/
		void		HandleSessionClosure();


	}

}


#endif

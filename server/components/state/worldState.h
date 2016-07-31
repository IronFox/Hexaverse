#ifndef worldStateH
#define worldStateH

#include <objects.h>
#include <net/tcp.h>


namespace World
{



	/**
	Finds a randomized suitable starting location for a new faction.
	
	*/
	index_t			GetFactionStartLocation(Random&random);


	/**
	Redefines the grid world based on the current reflection configuration, using the specified random variable to define seeds.
	Any existing session will be closed, all factions terminated before commencing.
	*/
	void			BuildConfiguredWorld(UINT32 seed);
	void			BuildConfiguredWorld(Random&rnd);



	void			SimulateLiquidsIfEnabled();



	void			UpdateEMPZones(count_t temporaryFactionCountBonus=0);


	extern Grid::WorldData	spectatorWorldState;
	extern bool			terrainChanged;//!< Indicates that the terrain changed at any given time during the last processing round. Reset when entering command mode
	extern TUniqueID	worldID;




	namespace Config
	{
		extern Grid::WorldDiscovery	worldDiscovery;

		extern UINT32	uplinkBandwidth/* = 500*/;
		extern bool		simulateLiquids/* = true*/;

	}


	/**
	Checks that the world state has not changed since the last call of this function
	@deprecated
	*/
	void			DebugCheckWorld(bool restart);


	void			SetUnlockedEMPZones(int unlocked);


	namespace Network
	{

		/**
		Signals that a terrain family has changed, and the specified faction should be updated and notified where necessary
		Calling this method explicitly is usually not necessary since it's called by SignalFamilyChange()
		@param faction Faction to create the delta for (and to send it to, assuming a client is online)
		@param familyID Index of the grid family that should be updated/sent
		@param last Indicates that this is the last family in the sequence
		@param rightNow Indicates that the sent terrain section should be applied as soon as possible as opposed to the timestamp stored in the respective family
		*/
		void		SignalFamilyChangeToFaction(const Objects::PFaction&faction,BYTE familyID, bool last, bool rightNow);

		/**
		Signals that a terrain family has changed, and all factions should be updated and notified where necessary
		@param familyID Index of the grid family that should be updated/sent
		@param last Indicates that this is the last family in the sequence
		@param rightNow Indicates that the sent terrain section should be applied as soon as possible as opposed to the timestamp stored in the respective family
		*/
		void		SignalFamilyChange(BYTE familyID, bool last, bool rightNow);

		bool		CheckCoverageSendTerrain(const Objects::PFaction&faction,const TCP::PPeer&peer);


		/**
		Scans through all global grid families and looks for outdated ones.
		Outdated families are sent to clients via SignalFamilyChange().
		Sets terrainChanged if at least one outdated family is encountered.
		terrainChanged is left unchanged otherwise.
		*/
		void		SendChangedFamilies();


		/**
		Called automatically by Network::CloseSession()
		Flushes any residual network related data
		*/
		void		HandleSessionClosure();


	}

}



#endif

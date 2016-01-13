#ifndef serverStateH
#define serverStateH

#include <global_string.h>
//#undef FAR
//#define FAR
//#include <net/tcp.h>
#include "../../shared/protocol.h"
#include "../../shared/objects.h"
#include "../../shared/runtimeConfig.h"

#include "evolver.h"




namespace State
{
	static const UINT32	SupportedFileVersion = 1042;

	namespace Milestones
	{
		static const UINT32	FirstVersionWithVariables = 1042;

	}


	static const UINT32 NotifyOthers = Objects::WorldEntity::PositionChanged 
										| Objects::WorldEntity::HealthChanged 
										| Objects::WorldEntity::EnableChanged 
										| Objects::WorldEntity::EMPDamageChanged
										| Objects::WorldEntity::HibernationChanged;


	void	WriteActualWorldToContainer(Riff::Chunk&container);
	/**
	Serializes @a inData to @a outChunk.
	@param inData Data to serialize. Is not altered. Reference only for compatibilty with existing functions.
	@param outChunk Riff-Chunk to write to
	*/
	void	WriteToListChunk(Grid::WorldData&inData, Riff::Chunk&outChunk);
	void	ReadFromListChunk(const Riff::Chunk&, Grid::WorldData&);
	void	Save(const PathString&name);
	void	Load(const PathString&name);
	void	AssembleGameInfo(Protocol::GameInfo&info);

	static const String	savFileExtension = "hsf";	//hexaverse save file
	void	AutoSave();

	extern TCP::Server		server;

	extern Evolver::Abstract		*evolver;

	class Client;
	typedef std::shared_ptr<Client>	PClient;

	class Client : public TCP::Peer::Attachment
	{
	public:
		Protocol::ClientName		name;
		std::weak_ptr<TCP::Peer>	peer;
		//Timer::Time					lastPing;
		Objects::PFaction			faction;
		bool						wantNewRound=false;


		/**
		Indicates that this client sees everything, but has no faction to control
		*/
		bool				isSpectator=false;

		

		String				ToString() const;
		bool				CanSee(const Objects::PWorldEntity&e, float t)	const;

		/**
		Refreshes personal game progress information
		*/
		void				SendProgressUpdate(bool dead, const String&warningReason);

	};
	/**
	Detects resource coverage changes within the specified sphere, and notifies the local faction client, if any
	Uses static fields. Do not call in parallel
	@param f Faction to update
	@param center Terrain center to detect resources from
	@param range Sphere radius to detect resources within
	@param timeStamp Timestamp to send to this client
	*/
	void				UpdateResourceCoverage(const Objects::PFaction&f, const Grid::LCellIdI&center,float range, const Protocol::Timestamp&timestamp);


	/**
	Processes connection loss of the specified client (or disconnects them).
	Parameter must be a copy.
	*/
	void	HandleConnectionLoss(PClient);


	extern Grid::WorldData	spectatorWorldState;


	extern Buffer<PClient>	//clients,
							activePlayers,spectators;
	extern GenericHashTable<Objects::OwnerParameter,PClient,Objects::OwnerHash>	playerMap;
	extern GenericHashTable<CryptographicID,Objects::PFaction,CryptographicID>		tokenMap;
	extern UINT32			lastSaveRound /*= 0*/,
							empZonesLocked/* = 0 */, unlockedEMPZones /*= 0xFFFF*/;
	extern Objects::PFaction	winner;	//if set indicates game is over
	extern TUniqueID		worldID;
	extern Timer::Time		lastPhaseChange /*= timer.Now()*/;
	extern bool				roundTimerPaused /*= true*/;

	//extern Grid::TSharedPlanetConfig	persistentPlanetInfo;


	namespace Config
	{
		extern float	factionStartMineralValue;	//determined during loading

		extern UINT16	sessionPort;

		extern Grid::WorldDiscovery	worldDiscovery;

		extern int		roundTimer/* = -1*/;
		extern UINT32	uplinkBandwidth/* = 500*/;
		extern bool		closedGame/* = false*/,
						startBases/* = false*/,
						basesRequired/* = false*/,
						simulateLiquids/* = true*/,
						exposeActionLeftToHostileFactions/* = true*/;
		extern int		saveEveryNRounds/* = 10*/;
		extern bool		saveWhenAllQuit/* = true*/;
		//extern count_t	limitMinesTo/* = 100*/;

		extern UINT32	factionSupportFlags;
		static const UINT32 AllFactionSupportFlags = 
								Protocol::GameFlag::SupportSpectatorFactions
								| Protocol::GameFlag::SupportRegularFactions
								;
		
		typedef RuntimeConfig::GameGoal	Goal;


		inline count_t	CountAccessibleZones()
		{
			count_t numZones = Objects::Faction::all.Count();	//not counting nexus faction
			numZones = (numZones/3) + !!(numZones % 3);
			return numZones;
		}

		inline void		CheckFactionSupportFlag(String&rs, UINT32 flag, const String&flagName)
		{
			if (factionSupportFlags & flag)
			{
				if (rs.IsEmpty())
					rs = flagName;
				else
					rs += ", "+flagName;
			}
		}

		inline String	FactionSupportFlagString()
		{
			String rs;
			CheckFactionSupportFlag(rs,Protocol::GameFlag::SupportSpectatorFactions,"Spectator");
			CheckFactionSupportFlag(rs,Protocol::GameFlag::SupportRegularFactions,"Regular");

			return rs;
		}

		inline bool		SetFactionSupportFlagString(const String&supp)
		{
			factionSupportFlags = 0;
			Array<String>	segs;
			explode(',',supp,segs);
			bool allGood = true;
			foreach (segs,seg)
			{
				seg->trimThis();
				if (*seg == "Spectator")
					factionSupportFlags |= Protocol::GameFlag::SupportSpectatorFactions;
				else
				if (*seg == "Regular")
					factionSupportFlags |= Protocol::GameFlag::SupportRegularFactions;
				else
				{
					LogEvent(EventType::ConfigRelated,nullptr,"Warning: Unsupported faction type for this build: '"+*seg+"'. Ignoring");
					allGood = false;
				}
			}
			return allGood;
		}



		inline count_t	GetTargetNexusCount()
		{
			return RuntimeConfig::gameGoal == Goal::EMPConquerNexus 
						? std::min<count_t>(std::max<count_t>(2,CountAccessibleZones()),3) 
						: 2;	//should be 2 here, because in normal ConquerNexus mode, only nexus bases are spawned, whereas in EMP ConsquerNexus mode ports are spawned on all of the 12 poles
		}

		namespace Nexus
		{
			extern bool		haveBases/* = false*/;
			extern bool		enableAI/* = false*/;
			extern float	threatEstimationFactor/* = 1.f*/;
			extern Array<Objects::Component::Turret*>	vehicleTurrets;
		}
	}
	

	extern Protocol::ServerMode		mode;
	extern float					effectiveAnimationLength/* = 1.f*/;
	extern bool						terrainChanged;//!< Indicates that the terrain changed at any given time during the last processing round. Reset when entering command mode

	/**
	Disconnects all clients, flushes the scene, and disables the TCP server systems
	Clients will not be able to connect unless RestartSession is called
	*/
	void		CloseSession();

	/**
	Redetermines the mineral value offset used to calculate when to spawn AI units
	Called automatically during RestartSession()
	*/
	void		ResetFactionStartMineralValue();

	/**
	Restarts a previously closed session (via CloseSession() ). A new world should be generated or restored from file
	prior to calling this function
	*/
	void		RestartSession();





	/**
	Updates the local phase and notifies all connected clients
	@param to Phase to change to
	@param timeWindow Change parameter. Usage depends on phase
	*/
	void		ChangePhase(RuntimeConfig::Phase to, float timeWindow = -1);
	void		Describe(Protocol::MobileTask&task, const Objects::MobileEntity&v, const Grid::LCellIdI&origin, const Objects::MobileEntity::Task&t,index_t taskIndex);
	void		Describe(Protocol::EntityState&state, const Objects::WorldEntity&e, bool owned, const Protocol::Timestamp&timestamp, UINT32 changeFlags);
	void		Describe(Protocol::MobileState&state, const Objects::MobileEntity&v, bool owned, const Protocol::Timestamp&timestamp, UINT32 changeFlags);
	void		Describe(Protocol::NewMobileEntity&cnst, const Objects::MobileEntity&v, bool owned, const Protocol::Timestamp&timestamp, UINT32 changeFlags);
	void		Describe(Protocol::ProductionTask&task, const Objects::MobileEntityProducer::WorkItem&element);
	void		Describe(Protocol::AttackTask&task, const Objects::Tower::AttackTask&element);
	void		Describe(Protocol::StructureState&state, const Objects::Structure&s, bool owned, const Protocol::Timestamp&timestamp, UINT32 changeFlags);
	void		Describe(Protocol::StructureConstruction&cnst, const Objects::Structure&s, bool owned, const Protocol::Timestamp&timestamp);

	/**
	Signals that the specified peer has issued an invalid operation (possibly a hacking attempt)
	and should be disconnected (and possibly banned)
	*/
	void		InvalidOperation(TCP::Peer&peer);

	/**
	Start function that must be called once during application start
	It installs all channels and configures the TCP server system
	*/
	void		GenerateWorldIdAndStartServer();




	/**
	Signals that a terrain family has changed, and the specified faction should be updated and notified where necessary
	Calling this method explicitly is usually not necessary since it's called by SignalFamilyChange()
	@param faction Faction to create the delta for (and to send it to, assuming a client is online)
	@param familyID Index of the grid family that should be updated/sent
	@param last Indicates that this is the last family in the sequence
	@param rightNow Indicates that the sent terrain section should be applied as soon as possible as opposed to the timestamp stored in the respective family
	*/
	void			SignalFamilyChangeToFaction(const Objects::PFaction&faction,BYTE familyID, bool last, bool rightNow);

	/**
	Signals that a terrain family has changed, and all factions should be updated and notified where necessary
	@param familyID Index of the grid family that should be updated/sent
	@param last Indicates that this is the last family in the sequence
	@param rightNow Indicates that the sent terrain section should be applied as soon as possible as opposed to the timestamp stored in the respective family
	*/
	void			SignalFamilyChange(BYTE familyID, bool last, bool rightNow);


	/**
	Checks that the world state has not changed since the last call of this function
	@deprecated
	*/
	void			DebugCheckWorld(bool restart);


	/**
	Detects all changes in entities and world states and sends updates to the respective clients
	@param thoroughly If set, also checks faction terrain changes that currently offline
	*/
	void			ProcessChanges(bool thoroughly);
	bool			CheckCoverageSendTerrain(const Objects::PFaction&faction,const TCP::PPeer&peer);


	void			PerformNewRoundLogic(float &atTime);	//includes UpdateHibernation()
	
	#ifndef NO_HIBERNATION
		void		UpdateHibernation();
	#endif

	void			SimulateLiquidsIfEnabled();


	void			Compare(Riff::File&a, Riff::File&b, Buffer<Riff::TID>&idStack, const String&indent="");

	void			SpawnNexusBaseAtPoleWithPort(int pole);
	void			SpawnNexusPortAtPole(int pole);
	void			RefreshWindowTitle();


	/**
	Spawns all units required by a new faction.
	The function does NOT check whether the location is any good.
	It assumes such checks have been performed. Both the specified stack and all immediately surrounding stacks must be suitable for unit spawning.
	*/
	void			SpawnFactionStartUnits(const Objects::PFaction&f, index_t startLocationCenterStackIndex);

	/**
	Finds a randomized suitable starting location for a new faction.
	
	*/
	index_t			GetFactionStartLocation(Random&random);


	void			UpdateEMPZones(count_t temporaryFactionCountBonus=0);

	
	template <index_t Channel, class Item>
		inline void	SendToOwner(TCP::ObjectSender<Item,Channel>&channel,const Objects::WorldEntity&e,const Item&item)
		{
			PClient*cl = playerMap.QueryPointer(e.owner);
			if (cl)
			{
				channel.SendTo((*cl)->peer.lock(),item);
			}
			foreach (spectators,s)
				channel.SendTo((*s)->peer.lock(),item);
		}

	template <index_t Channel, class Item>
		inline void	SendToOwner(TCP::ObjectSender<Item,Channel>&channel,const Objects::WorldEntity&e,const TCP::Serial<Item>&item)
		{
			PClient*cl = playerMap.QueryPointer(e.owner);
			if (cl)
			{
				channel.SendTo((*cl)->peer.lock(),item);
			}
			foreach (spectators,s)
				channel.SendTo((*s)->peer.lock(),item);
		}

}



#endif

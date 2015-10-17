#ifndef serverStateH
#define serverStateH

#include <global_string.h>
#include <net/tcp.h>
#include "../../shared/protocol.h"
#include "../../shared/objects.h"






namespace State
{
	static const UINT32	fileVersion = 1039;

	static const UINT32 NotifyOthers = Objects::WorldEntity::PositionChanged 
										| Objects::WorldEntity::HealthChanged 
										| Objects::WorldEntity::EnableChanged 
										| Objects::WorldEntity::EMPDamageChanged
										| Objects::WorldEntity::HibernationChanged;


	void	Save(const String&name);
	void	Load(const String&name);

	static const String	savFileExtension = "hsf";	//hexaverse save file
	void	AutoSave();

	extern TCP::Server		server;

	class Client;
	typedef std::shared_ptr<Client>	PClient;

	class Client : public TCP::Peer::Attachment
	{
	public:
		Protocol::ClientState		state;
		std::weak_ptr<TCP::Peer>	peer;
		Timer::Time					lastPing;
		Objects::PFaction			faction;
		bool						wantNewRound;

		
		/**/				Client():lastPing(timer.Now()),/*hasUnits(false),*/wantNewRound(false)
		{}

		String				ToString() const;
		bool				CanSee(const Objects::PWorldEntity&e, float t)	const;

		/**
		Refreshes personal game progress information
		*/
		void				SendProgressUpdate(bool dead);

		/**
		Detects resource coverage changes within the specified sphere, and notifies the local faction client
		@param center Terrain center to detect resources from
		@param range Sphere radius to detect resources within
		@param timeStamp Timestamp to send to this client
		*/
		void				UpdateResourceCoverage(const Grid::LCellIdI&center,float range, const Protocol::Timestamp&timestamp);
	};


	extern Buffer<PClient>	clients,activePlayers;
	extern GenericHashTable<Objects::OwnerParameter,PClient,Objects::OwnerHash>	playerMap;
	extern GenericHashTable<CryptographicID,Objects::PFaction,CryptographicID>		tokenMap;
	extern UINT32			roundNumber/*=0*/, lastSaveRound /*= 0*/,
							empZonesLocked/* = 0 */, unlockedEMPZones /*= 0xFFFF*/;
	extern Protocol::Phase	phase;
	extern Objects::PFaction	winner;	//if set indicates game is over
	extern TUniqueID		worldID;
	extern Timer::Time		lastPhaseChange /*= timer.Now()*/;
	extern bool				roundTimerPaused /*= true*/;

	extern Grid::TSharedPlanetConfig	persistentPlanetInfo;


	namespace Config
	{
		extern float	factionStartMineralValue;	//determined during loading


		extern int		roundTimer/* = -1*/;
		extern UINT32	uplinkBandwidth/* = 500*/;
		extern bool		closedGame/* = false*/,
						startBases/* = false*/,
						basesRequired/* = false*/,
						simulateLiquids/* = true*/,
						exposeActionLeftToHostileFactions/* = true*/;
		extern UINT32	saveEveryNRounds/* = 10*/;
		extern bool		saveWhenAllQuit/* = true*/;
		//extern count_t	limitMinesTo/* = 100*/;

		extern bool		enableNexusAI/* = false*/;
		extern float	nexusThreatEstimationFactor/* = 1.f*/;
		extern Array<Objects::Component::Turret*>	nexusVehicleTurrets;
		extern Array<String>			nexusVehicleTurretNames;	//used during config loading, identical to nexusVehicleTurrets later
		
		typedef Protocol::GameGoal	Goal;

		extern Goal		goal;

		inline count_t	CountAccessibleZones()
		{
			count_t numZones = Objects::Faction::all.Count();	//not counting nexus faction
			numZones = (numZones/3) + !!(numZones % 3);
			return numZones;
		}


		inline count_t	GetTargetNexusCount()
		{
			return goal == Goal::EMPConquerNexus ? std::min<count_t>(std::max<count_t>(2,CountAccessibleZones()),3) : 3;
			//goal == Goal::EMPConquerNexus ? 3 : 2;
		}

		namespace NexusPorts
		{
			extern bool	haveBases/* = false*/;
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
	void		ChangePhase(Protocol::Phase to, float timeWindow = -1);
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
	void		Start();




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



	void			PerformNewRoundLogic(float &atTime);	//includes UpdateHibernation()
	
	#ifndef NO_HIBERNATION
		void		UpdateHibernation();
	#endif

	void			SimulateLiquidsIfEnabled();


	void			Compare(Riff::File&a, Riff::File&b, Buffer<Riff::TID>&idStack, const String&indent="");

	void			SpawnNexusBaseAtPoleWithPort(int pole);
	void			SpawnNexusPortAtPole(int pole);
	void			RefreshWindowTitle();

	
	template <index_t Channel, class Item>
		inline void	SendToOwner(TCP::ObjectSender<Item,Channel>&channel,const Objects::WorldEntity&e,const Item&item)
		{
			PClient*cl = playerMap.QueryPointer(e.owner);
			if (cl)
			{
				channel.SendTo((*cl)->peer.lock(),item);
			}
		}

	template <index_t Channel, class Item>
		inline void	SendToOwner(TCP::ObjectSender<Item,Channel>&channel,const Objects::WorldEntity&e,const TCP::Serial<Item>&item)
		{
			PClient*cl = playerMap.QueryPointer(e.owner);
			if (cl)
			{
				channel.SendTo((*cl)->peer.lock(),item);
			}
		}

}



#endif

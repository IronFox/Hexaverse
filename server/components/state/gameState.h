#ifndef gameStateH
#define gameStateH

#include <protocol.h>
#include <runtimeConfig.h>
#include <objects/types.h>
#include "../evolver.h"

namespace Game
{

	extern float	factionStartMineralValue;	//determined during loading

	/**
	Most factions that have ever existed at the same time throughout the game. Used to determine number of accessible EMP areas.
	Not decremented when factions die. Restored from savegame.
	*/
	extern UINT32			mostFactionsAlive	/*=0*/;
	extern Objects::PFaction	winner;	//if set indicates game is over
	extern Timer::Time		lastPhaseChange /*= timer.Now()*/;
	extern bool				roundTimerPaused /*= true*/;
	extern Evolver::Abstract*evolver;

	//extern Grid::TSharedPlanetConfig	persistentPlanetInfo;


	/**
	Signals that a new faction has been created or an existing one has been removed.
	Alters @a mostFactionsAlive. Does not do anything else
	*/
	void				SignalFactionsChanged();

	namespace Config
	{
		extern int		roundTimer/* = -1*/,
						extraTechniteRoundTimer/* = -1*/;
		extern UINT32	extraTechniteRounds/* = 0*/;		
		extern UINT32	uplinkBandwidth/* = 500*/;
		extern bool		closedGame/* = false*/,
						startBases /*= false*/,
						basesRequired/* = false*/,
						exposeActionLeftToHostileFactions/* = true*/;

		extern UINT32	factionSupportFlags;
		static const UINT32 AllFactionSupportFlags = 
								Protocol::GameFlag::SupportSpectatorFactions
								| Protocol::GameFlag::SupportRegularFactions
								;
		
		typedef RuntimeConfig::GameGoal	Goal;


		inline count_t	CountAccessibleZones()
		{
			count_t numZones = mostFactionsAlive;	//not counting nexus faction
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
					LogEvent(EventClass::Preset::configurationError,nullptr,"Warning: Unsupported faction type for this build: '"+*seg+"'. Ignoring");
					allGood = false;
				}
			}
			return allGood;
		}



		inline count_t	GetTargetNexusCount()
		{
			return RuntimeConfig::empZones
						? std::min<count_t>(std::max<count_t>(2,CountAccessibleZones()),3) 
						: 2;	//should be 2 here, because in normal ConquerNexus mode, only nexus bases are spawned, whereas in EMP ConsquerNexus mode ports are spawned on all of the 12 poles
		}

		namespace Nexus
		{
			extern bool		haveBases/* = false*/,
							haveSatellitesAtopBases/* = true*/;
			extern bool		startBases/* = false*/,
							enableAI/* = false*/;
			extern float	threatEstimationFactor/* = 1.f*/;
			extern Array<Objects::Component::Turret*>	vehicleTurrets;
		}
	}
	

	extern float					effectiveAnimationLength/* = 1.f*/;



	namespace Network
	{
	
		static const UINT32 NotifyOthers = Objects::WorldEntity::PositionChanged 
											| Objects::WorldEntity::HealthChanged 
											| Objects::WorldEntity::EnableChanged 
											| Objects::WorldEntity::EMPDamageChanged
											| Objects::WorldEntity::HibernationChanged;


		void		AssembleGameInfo(Protocol::GameInfo&info);
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
		Detects resource coverage changes within the specified sphere, and notifies the local faction client, if any
		Uses static fields. Do not call in parallel
		@param f Faction to update
		@param center Terrain center to detect resources from
		@param range Sphere radius to detect resources within
		@param timeStamp Timestamp to send to this client
		*/
		void		UpdateResourceCoverage(const Objects::PFaction&f, const Grid::LCellIdI&center,float range, const Protocol::Timestamp&timestamp);
		/**
		Called automatically by Network::CloseSession()
		Flushes any residual data
		*/
		void		HandleSessionClosure();
		/**
		Called automatically by Network::SuspendSession()
		Stops AI process
		*/
		void		HandleSessionSuspension();
		/**
		Called automatically by Network::RestartSession()
		*/
		void		HandleSessionRestart();


	}


    inline bool		CurrentRoundIsExtraTechniteRound()    {return RuntimeConfig::techniteSubRoundNumber > 0 && RuntimeConfig::techniteSubRoundNumber <= Config::extraTechniteRounds;}


	void			PerformNewRoundLogic(float &atTime);	//includes UpdateHibernation()


	/**
	Redetermines the mineral value offset used to calculate when to spawn AI units
	Called automatically during RestartSession()
	*/
	void			ResetFactionStartMineralValue();


	#ifndef NO_HIBERNATION
		void		UpdateHibernation();
	#endif


	/**
	Detects all changes in entities and world states and sends updates to the respective clients
	@param thoroughly If set, also checks faction terrain changes that currently offline
	*/
	void			ProcessChanges(bool thoroughly);


	void			SpawnNexusBaseAtPoleWithPort(int pole);
	void			SpawnNexusPortAtPole(int pole);

	/**
	Spawns all units required by a new faction.
	The function does NOT check whether the location is any good.
	It assumes such checks have been performed. Both the specified stack and all immediately surrounding stacks must be suitable for unit spawning.
	*/
	void			SpawnFactionStartUnits(const Objects::PFaction&f, index_t startLocationCenterStackIndex);


}

#endif

#ifndef gameStateH
#define gameStateH

#include <protocol.h>
#include <runtimeConfig.h>
#include <objects/types.h>
#include <objects/structure/tower.h>
#include "../evolver.h"


namespace Game
{
	
	DECLARE_POINTER_CLASS(Vote);


	/**
	Non-persistant voting storage.
	Not written to or restored from savegames.
	Cleared out on session reset.
	*/
	class Vote : public std::enable_shared_from_this<Vote>
	{
	public:
		typedef Objects::PAlliance	PAlliance;
		typedef std::function<void(const PVote&)> FCallback;

		CONSTRUCT_ENUMERATION4(Quorum,All,TwoThirds, Majority, OneThird);
		CONSTRUCT_ENUMERATION3(VotesToPass, SimpleMajority, TwoThirds, Unanimous);

		//all const that is of relevance to clients, and may not be changed once sent
		const TUniqueID		id;
		const String		phrase;
		const Timer::Time	created = timer.Now();
		const float			closingAfterSeconds = 120;
		float				clearedAfterSeconds = 720;

		PAlliance		scope;	//if set: scope=alliance, else global
		FCallback		onPass;
		Quorum			quorum;
		VotesToPass		votesToPass;
		const Array<TUniqueID>	concerning;
		bool			closed = false;
		

		/**/			Vote(const ConstArrayRef<TUniqueID>&concerning, const String&phrase, float closingAfterSeconds)
						:id(TUniqueID::New()),phrase(phrase),closingAfterSeconds(closingAfterSeconds),concerning(concerning)	{Advertise();}
		void			CastVote(const Objects::PFaction&f, bool inFavor);
		bool			MayVote(const Objects::PFaction&f) const {return !scope || f->alliance == scope;}
		count_t			GetQuorum() const	{return GetQuorum(quorum,CountEligibleVoters());}
		count_t			CountEligibleVoters() const;
		static count_t	GetVotesToPass(VotesToPass, count_t voters);
		static count_t	GetQuorum(Quorum q, count_t eligibleVoters);


		static void		CheckAllTimeouts();
		static void		CastVote(const TUniqueID&voteID, const Objects::PFaction&f, bool inFavor);
		static PVote	CreateOrGet(const ConstArrayRef<TUniqueID>&concerning, const String&phrase, float closingAfterSeconds, const PAlliance&scope, const FCallback&onPass);
		static PVote	CreateNew(const ConstArrayRef<TUniqueID>&concerning, const String&phrase, float closingAfterSeconds, const PAlliance&scope, const FCallback&onPass);
		static void		Flush();

		static PVote	FindConcerning(const ConstArrayRef<TUniqueID>&concerning)	{PVote rs; map.Query(concerning,rs); return rs;}


	private:
		HashTable<TUniqueID,bool>	votes;
		static HashTable<TUniqueID,PVote>	all;
		static HashTable<Array<TUniqueID>,PVote>	map;

		bool			CheckTimeouts();
		void			Advertise()	const;
		void			CheckVotes();
		void			Close();
		void			Close(const Protocol::VoteResult&);
		void			CheckResults(Protocol::VoteResult&rs, bool timedOut)	const;

	};



	extern float	factionStartMineralValue;	//determined during loading

	/**
	Most factions that have ever existed at the same time throughout the game. Used to determine number of accessible EMP areas.
	Not decremented when factions die. Restored from savegame.
	*/
	extern UINT32			mostFactionsAlive	/*=0*/;

	extern Objects::PFaction	winner;	//if set indicates game is over. Do not set manually. Use DeclareWinner()
	extern Objects::WFaction	postEvolutionWinner;	//if winner is declared while evolver is active, this variable is set instead for later

	extern Timer::Time		lastPhaseChange /*= timer.Now()*/;
	extern bool				roundTimerPaused /*= true*/;
	//extern Evolver::Abstract*evolver;
	extern INT32			tutorialAt;
	extern bool				endRoundNow;
	//extern Grid::TSharedPlanetConfig	persistentPlanetInfo;



	namespace Config
	{
		extern int		roundTimer/* = -1*/;
		extern UINT32	uplinkBandwidth/* = 500*/;
		extern bool		closedGame/* = false*/,
						startBases /*= false*/,
						basesRequired/* = false*/,
						exposeActionLeftToHostileFactions/* = true*/;
		extern UINT32	factionsPerZone/* = 3/1*/;

		extern UINT32	factionSupportFlags;
		
		typedef RuntimeConfig::GameGoal	Goal;


		inline count_t	CountAccessibleZones(count_t temporaryFactionCountBonus=0)
		{
			count_t numZones = mostFactionsAlive+temporaryFactionCountBonus;	//not counting nexus faction
			numZones = (numZones/factionsPerZone) + !!(numZones % factionsPerZone);
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
			if (factionSupportFlags == Protocol::GameFlag::AllFactionSupport)
				return "All";

			String rs;
			CheckFactionSupportFlag(rs,Protocol::GameFlag::SupportSpectatorFactions,"Spectator");
			CheckFactionSupportFlag(rs,Protocol::GameFlag::SupportRegularFactions,"Regular");

			return rs;
		}

		inline void		SetFactionSupportFlagString(const String&supp)
		{
			factionSupportFlags = 0;
			Ctr::Array<String>	segs;
			explode(',',supp,segs);
			//bool allGood = true;
			foreach (segs,seg)
			{
				seg->TrimThis();
				if (*seg == "All")
					factionSupportFlags |= Protocol::GameFlag::AllFactionSupport;
				else
				if (*seg == "Spectator")
					factionSupportFlags |= Protocol::GameFlag::SupportSpectatorFactions;
				else
				if (*seg == "Regular")
					factionSupportFlags |= Protocol::GameFlag::SupportRegularFactions;
				else
				{
					LogEvent(EventClass::Preset::configurationError,nullptr,"Warning: Unsupported faction type for this build: '"+*seg+"'. Ignoring");
					//allGood = false;
				}
			}
			//no longer returning success, or failure, since different branches may allow different factions. So a warning is valid, but no handling of this case
			//return allGood;
		}



		inline count_t	GetTargetNexusCount()
		{
			return RuntimeConfig::empZones
						? std::min<count_t>(std::max<count_t>(2,CountAccessibleZones()),3) 
						: 2;	//should be 2 here, because in normal ConquerNexus mode, only nexus bases are spawned, whereas in EMP ConsquerNexus mode ports are spawned on all of the 12 poles
		}

		namespace Nexus
		{
			extern bool		haveBases/* = false*/
							IF_SATELLITES_P(,haveSatellitesAtopBases/* = true*/)
							;
			extern bool		startBases/* = false*/,
							enableAI/* = false*/;
			extern float	threatEstimationFactor/* = 1.f*/;
			extern UINT32	roundsPerResearchPoint;
			extern Ctr::Array<Objects::Component::Turret*>	vehicleTurrets;
		}
	}
	

	extern float					effectiveAnimationLength/* = 1.f*/;



	namespace Network
	{
	
		static const UINT32 NotifyOthers = Objects::WorldEntity::PositionChanged 
											| Objects::WorldEntity::StartedBuilding 
											| Objects::WorldEntity::StoppedBuilding 
											| Objects::WorldEntity::HealthChanged 
											| Objects::WorldEntity::EnableChanged 
											| Objects::WorldEntity::EMPDamageChanged
											| Objects::WorldEntity::HibernationChanged;


		UINT32		AssembleGameFlags(bool allowSpectatorFactions);
		void		AssembleGameConfig(Protocol::GameConfig&info, bool allowSpectatorFactions);
		void		AssembleGameInfo(Protocol::GameInfo&info, bool allowSpectatorFactions);
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

		void		SendStatisticsTo(TCP::Peer & peer, const Objects::PFaction & faction);


	}


	void			RegisterClassGain(const Objects::PWorldEntity&e, Objects::Faction::UnitClass, const String&debugName);
	count_t			CountLivingPlayerFactions();

	void			PerformNewRoundLogic(float &atTime);	//includes UpdateHibernation()


	/**
	Sets the game winner to the specified faction.
	Updates game phase to EndOfGame, and notifies all clients.
	Winner must have been empty before.
	*/
	void			DeclareWinner(const Objects::PFaction&);

	/**
	Redetermines the mineral value offset used to calculate when to spawn AI units
	Called automatically during RestartSession()
	*/
	void			ResetFactionStartMineralValue();


	#ifndef NO_HIBERNATION
		void		UpdateHibernation();
	#endif


	Evolver::Abstract*	GetEvolver();
	void			SetDefaultEvolver();

	/**
	Executes BeginRoundProcessing() of the active evolver
	*/
	void			BeginRoundProcessing();
	void			FinishRoundProcessing();
	bool			ContinueRoundProcessing();

	bool			IsProcessingRound();

	/**
	Detects all changes in entities and world states and sends updates to the respective clients
	@param thoroughly If set, also checks faction terrain changes that currently offline
	*/
	void			ProcessChanges(bool thoroughly);

	/**
	Updates EMP damage caused by residual EMP fields. This is called automatically at the end of round processing
	*/
	void			UpdateResidualEMPDamage(float&atTime);
	void			ApplyEMPZoneDamage(const Objects::PWorldEntity&e,float&atTime);



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

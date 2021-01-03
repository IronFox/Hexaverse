#ifndef protocolH
#define protocolH


#undef FAR
#define FAR
#include <iphlpapi.h>
#include <net/tcp.h>
#include <net/containers.h>
#include "objects/entityType.h"
#include "grid/stack.h"
#include "grid/planet.h"
#include "objects.h"
#include "stringTable.h"
#include "runtimeConfig.h"
#include "protocol/serialBaseTypes.h"

#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"Rpcrt4.lib")
#pragma comment(lib, "IPHLPAPI.lib")


#define MANAGER_UNICAST_PORT		10245	//must be constant, not reused by any heartbeat port
#define BROADCAST_PORT				10246	//must be constant, not reused by any heartbeat port

#define DEFAULT_SERVER_BASE_PORT	10247
#define TUTORIAL_BASE_PORTS_FROM	50000

namespace PortOffset
{
	/**
	@brief Service port delta to root port
	*/
	enum delta_t
	{
		Info,
		Session,
		Control,

		Heartbeat = Info	//heartbeat is UDP. Can coexist with others
	};
}

#ifdef ONE_COMPOUND_PER_FACTION
	#define NAME_APPEND0	"o"
#else
	#define NAME_APPEND0
#endif

#ifdef NO_MUNITION
	#define NAME_APPEND1	"m"
#else
	#define NAME_APPEND1
#endif

#ifdef NO_SATELLITES
	#define NAME_APPEND_S	"s"
#else
	#define NAME_APPEND_S
#endif

#ifdef LOWER_RESOLUTION_PLANET
	#define NAME_APPEND_L	"l"
#else
	#define NAME_APPEND_L
#endif



#define SERVER_NAME		PRODUCT_NAME " v" PRODUCT_VERSION\
							"["\
							NAME_APPEND0\
							NAME_APPEND1\
							NAME_APPEND_S\
							NAME_APPEND_L\
							"a"	/*aerial branch*/\
							"]"

#include "objects/resource.h"








namespace Protocol
{











	
	
	

	NETSTRUCT2(Timestamp
		, NET(bool), rightNow
		, NET(float), atTime
	)

	NETSTRUCT2(TimeWindow
		, Timestamp, begin
		, float, duration
	)

	String ToString(const Timestamp&t);
	String ToString(const TimeWindow&t);

	typedef Package::NetString<32>	ClientName,Hello;

	NETSTRUCT4(HelloResponse
		, NET(bool), success
		, UINT32, gameFlags
		, CryptographicID, passwordChallenge	//to be hashed into the password, and faction access token
		, Package::NetString<256>, errorMessage	//in case stateAccepted is false
	)





//	static_assert(sizeof(Grid::CellStack::TStructCountDeltaBlock) == sizeof(UINT32) + sizeof(Grid::CellStack::TStructCountDelta), "Struct alignment out of whack" );


	struct SerialUnsigned
	{
		typedef SerialUnsigned Self;
		serial_size_t	value = 0;
		
		void			swap(SerialUnsigned&other)
		{
			std::swap(value,other.value);
		}

		friend void Serialize(IWriteStream&s, const Self&v)
		{
			s.WriteSize(v.value);
		}

		friend	void	Deserialize(IReadStream&s, Self&v)
		{
			s.ReadSize(v.value);
		}

		void		operator=(serial_size_t v)
		{
			value = v;
		}

		void		operator=(int v)
		{
			ASSERT_GREATER_OR_EQUAL__(v,0);
			value = (serial_size_t)v;
		}

		operator serial_size_t&()
		{
			return value;
		}
	};


	//NETA(x)
	typedef NetArray<Grid::CellStack::TTypeDeltaBlock,4000000>	TypeDeltaArray;
	NETSTRUCT3(WorldDelta
		, Timestamp, timestamp
		, NET(BYTE), familyAndLastFlag	//flag stored in 0x80, family mask is 0x7f
		, TypeDeltaArray, data0
	)

	namespace EntityStateFlags
	{
		enum flag_t
		{
			IsUnstable = 0x1,
			IsSleeping = 0x2,
		};
	}




	NETSTRUCT6(EntityState
		, Timestamp, timestamp
		, TUniqueID, id
		, Grid::LCellIdI, location
		, SerialUnsigned, health
		, SerialUnsigned, empDamageLevel
		, BYTE, entityStateFlags
	)

	namespace TaskFlags
	{
		enum flag_t
		{
			VisualizePathToFocus = 0x1,
			DelayedBySleep = 0x2,
		};
	}

	NETSTRUCT10(MobileTask
		, NET(UINT8), type
		, NET(UINT8), mode
		, NET(UINT8), taskFlags
		, NETA(Grid::LCellIdI), foci
		, Package::NetString<256>, name
		, NET(USHORT), continuationsToJobCompletion
		, NET(USHORT), resourceCostLeft
		, BYTE, jobAffinity
		, NETA(Grid::LCellIdI), buildPath
		, Objects::EntityType, visualizationName	//can also be DetonationSize_Regular or DetonationSize_Doomsday
	)

	namespace VehicleFlags
	{
		enum flag_t
		{
			Automated = 0x1,
			Hibernating = 0x2,
			ConnectedToGround = 0x4,
			Building = 0x8,
		};
	}
	namespace StructureFlags
	{
		enum flag_t
		{
			Enabled = 0x1,
			Hibernating = 0x2,
			//ConnectedToSpace = 0x4,//never used
		};
	}

	
	NETSTRUCT2(ProductionTask
		, Objects::EntityType, type
		, TUniqueID, id
	)
	typedef NetArray<ProductionTask,Objects::MaxTasks> ProductionTasks;
	NETSTRUCT3(ProducerState
		, ProductionTasks, productionTasks
		, UINT32, productionDone
		, Grid::LCellIdI, gatheringPoint
	)


	#ifndef ONE_COMPOUND_PER_FACTION
		NETSTRUCT5(MyMobileEntity
			, NET(UINT16), constructionRoundCounter
			, Resource::PositiveVector, resourceVector
			, NETA(MobileTask), tasks
			, Optional<NET(TUniqueID)>, storedIn
			, Optional<ProducerState>, producerState
			)

	#else
		NETSTRUCT4(MyMobileEntity
			, NET(UINT16), constructionActionCounter
			, NETA(MobileTask), tasks
			, Optional<NET(TUniqueID)>, storedIn
			, Optional<ProducerState>, producerState
			)

	#endif

	struct NoData
	{
		friend void			Serialize(IWriteStream&s, const NoData&v){}
		friend void			Deserialize(IReadStream&s, NoData&v){}

		void swap(NoData&)	{}
		friend void swap(NoData&, NoData&)	{}
	};




	NETSTRUCT5(MobileState
		, EntityState, entityState
		, NET(UINT8), flags
		, Optional<MyMobileEntity>, myMobileEntity
		, NET(float), attackActionLeft
		, NET(float), movementActionLeft
	)

	NETSTRUCT4(MobileEntityMotion
		, TimeWindow, time
		, NET(TUniqueID), id
		, Grid::LCellIdI, fromLocation
		, Grid::LCellIdI, toLocation
	)

	NETSTRUCT7(VagueAttackTrajectory
		, TimeWindow, time
		, NET(M::TVec3<>), sourcePosition
		, NET(ConstName), turretType
		, NET(float), turretRange
		, NET(float), attackAngle
		, NET(float), explosionRadius
		, NET(M::TVec3<>), traceTo
	)

	NETSTRUCT5(PreciseAttackTrajectory
		, TimeWindow, time
		, NET(TUniqueID), attacker
		, NET(float), attackAngle
		, NET(M::TVec3<>), traceTo
		, NET(bool), isAutoAttack
	)

	NETSTRUCT3(EntityTurretRotation
		, TimeWindow, time
		, NET(TUniqueID), id
		, NET(float3), toWorld
	)

	NETSTRUCT4(Explosion
		, Timestamp, timestamp
		, NET(M::TVec3<>), center
		, NET(float), radius
		, NET(bool), isEMP
	)

	NETSTRUCT3(DestroyEntity
		, Timestamp, timestamp
		, NET(TUniqueID), id
		, NET(UINT8), type
	)


	typedef Grid::LCellIdI	AttackTask;

	NETSTRUCT2(MineState
		, Objects::Resource::PositiveVector, naturalResources
		, NET(bool), linkedToCore
	)
	typedef NET(int)	NexusPortState;
	NETSTRUCT2(TowerState
		, NETA(AttackTask), attackTasks
		, NET(float), attackActionLeft
	)

	typedef Selective4<MineState,ProducerState,NexusPortState,TowerState>
		SelectiveStructureState;


	namespace AllianceFlag
	{
		enum flag_t
		{
			ShareSensors = 0x1,
			ExposeEntityDetails = 0x2,
		};

	}


	NETSTRUCT2(AllianceConfigChange
		, TUniqueID, allianceID
		, UINT32, flags
	)
	
	NETSTRUCT2(AllianceMemberChange
		, TUniqueID, allianceID
		, NETA(TUniqueID), memberFactions
	)


	namespace AllianceInviteType
	{
		enum T
		{
			Invite,
			Cancel,
			Accept,
			Reject
		};
	}

	NETSTRUCT3(AllianceInvite
		, TUniqueID, allianceID
		, TUniqueID, factionID
		, BYTE, type
	)

	namespace VoteResultType
	{
		enum T
		{
			Passed,
			Rejected,
			TimedOut,
			Canceled,
		};
	}

	NETSTRUCT3(VoteCreation
		, TUniqueID, voteID
		, Package::NetString<512>, phrase
		, float, remainingSeconds
	)
	
	NETSTRUCT6(VoteResult
		, TUniqueID, voteID
		, BYTE, resultType
		, UINT32, votedInFavor
		, UINT32, votedAgainst
		, UINT32, eligibleToVote
		, UINT32, quorum
	)

	NETSTRUCT2(ClientVote
		, TUniqueID, voteID
		, bool, inFavor
	)


	NETSTRUCT5(StructureState
		, EntityState, entityState
		, Objects::Resource::PositiveVector, resourceStorage
		, NET(BYTE), flags
		, SelectiveStructureState, selective
		, NETA(BYTE), packed
	)



	NETSTRUCT3(NewMobileEntity
		, MobileState, state
		, NET(TUniqueID), owner
		, Objects::EntityType, type
	)

	NETSTRUCT3(StructureConstruction
		, StructureState, state
		, NET(TUniqueID), owner
		, Objects::EntityType, type
	)


	namespace DirectFlags
	{
		enum flag_t
		{
			LeftClick = 0x1,
			Detailed = 0x2,
			ClearSafeTasks = 0x4
		};

	}

	/**
	If directFlags contains DirectFlags::Explicit, path is considered explicit with one entry per neighboring node.
	In this case the command is rejected, if the path is not possible when applied to the known world of the respective client.
	If the path is found to be inapplicable during computation phases, an automatically determined path to the last given node is computed and applied, then dropped.
	Path must always be of at least length 1
	*/
	typedef NetArray<Grid::LCellIdI,Objects::MaxDirectPathLength>	DirectPath;
	NETSTRUCT4(EntityDirectCommand
		, NET(TUniqueID), id
		, NET(UINT16),	mode
		, DirectPath, path
		, NET(UINT8), directFlags
	)


	NETSTRUCT5(ChoiceIntersection
		, NET(TUniqueID), id
		, Grid::LCellIdI, target
		, NET(UINT16), mode
		, Optional<TUniqueID>, deployingWhat
		, Optional<TUniqueID>, requestID
	)

	NETSTRUCT2(ChoiceResponse
		, TUniqueID, requestID
		, TUniqueID, deployedID	//empty on failure
	)
	
	NETSTRUCT3(StandardJob
		, NET(TUniqueID), ownerID
		, NET(ConstName), jobName
		, Objects::EntityType, parameterName
		)

	typedef NetArray<Grid::HCellIdI,256>	CellArray;
	NETSTRUCT3(TerrainMod
		, NET(TUniqueID), id
		, NET(UINT16), mode
		, CellArray, cells
		)

	NETSTRUCT2(ConstructionAbort
		, NET(TUniqueID), plantID
		, NET(TUniqueID), taskId
	)

	NETSTRUCT2(ConstructionBegin
		, NET(TUniqueID), plantID
		, Objects::EntityType, type
	)

	NETSTRUCT2(EnableStructure
		, NET(TUniqueID), id
		, NET(bool), enable
		)


	NETSTRUCT5(ControlMessage
		, Package::NetString<256>, message
		, BYTE, significance
		, BYTE, flavor
		, bool, isPrivate
		, TUniqueID, focus	//may be faction or entity
	)




	NETSTRUCT3(PhaseChange
		, NET(UINT8), newPhase
		, NET(RuntimeConfig::RoundNumber), roundNumber
		, NET(float), timeFactor
	)


	typedef Grid::TCellResourceCoverage		TResourceCell;

	NETSTRUCT3(ResourceUpdate
		, Timestamp, timestamp
		, SerialUnsigned, resourceIndex
		, NETA(TResourceCell), cells
	)


	struct FactionType
	{
		enum flag_t
		{
			ContinueOnly = 0xFFFFFFFF,
			NexusFaction = 0x1,
			SpectatorFaction = 0x2,
			Reserved1 = 0x4,
			RegularFaction = 0x8,
		};
	};


	//typedef NET(SHA256::HashContainer) ResourceID;


	/**
	Configuration settings that should be forwarded to the client
	Keep in sync with [ServerManager:]ServerInfoReceiver.cs
	*/
	NETSTRUCT3(GameConfig
		, NET(UINT32), gameFlags
		, NET(BYTE), gameGoal
		, NET(BYTE), worldDiscovery
	)	

	/**
	Information about the running game
	Keep in sync with [ServerManager:]ServerInfoReceiver.cs
	*/
	NETSTRUCT6(GameInfo
		, NET(UINT16), numFactionsOnline
		, NET(UINT16), numFactionsCreated
		, NET(RuntimeConfig::RoundNumber), roundNumber
		, NET(TUniqueID), worldID	//used to manage factionTokens
		, NET(BYTE), activePhase
		, GameConfig, gameConfig
	)


	NETSTRUCT4(ClientFactionResponse
		, NET(UINT32), startFactionTypes
		, NET(CryptographicID), newFactionAccessToken
		, NET(Grid::TSharedPlanetConfig), planetConfig
		, GameInfo, info
	)

	NETSTRUCT3(ClientFactionRequest
		, NET(UINT32), startFactionTypes
		, SHA256::HashContainer, hashedServerPassword
		, SHA256::HashContainer, hashedFactionToken
	)

	NETSTRUCT4(GameProgress
		, Package::NetString<256>, status
		, Package::NetString<256>, warning
		, NET(bool), gameWon
		, NET(bool), gameOver
	)

	NETSTRUCT2(EntityStateUpdate
		, NET(TUniqueID), entityID
		, NET(bool), doSet
	)



	typedef NetArray<BYTE,100000>	SerialMarkerState;

	NETSTRUCT2(Marker
		, UINT32, markerType
		, SerialMarkerState, state
	)

	NETSTRUCT2(TouchedMarker
		, NET(TUniqueID), id
		, NET(RuntimeConfig::RoundNumber), newRoundNumber
	)

	static const size_t MaxFactionNameLength = 32;


	NETSTRUCT4(IdentifyFaction
		, NET(TUniqueID), id
		, TUniqueID, allianceID
		, Package::NetString<MaxFactionNameLength>, name
		, NET(M::TVec3<>), color
	)


	NETSTRUCT2(QueueResearch
		, UINT32,	research
		, bool,		drop
	);

	typedef NETA(UINT32)	ResearchQueue;

	typedef std::pair<Objects::EntityType,UINT16>	TUnitLimit;

	typedef NETA(TUnitLimit)	UnitLimits;


	NETSTRUCT3(OperationLacksResources
		, Timestamp, timestamp
		, NET(TUniqueID), entityID
		, UINT64,	resourceFlagSet
	)

	NETSTRUCT2(ResearchFactionState
		, NET(TUniqueID), ownerID
		, NETA(int), progressVector
	)

	NETSTRUCT5(ResearchTopic
		, SerialUnsigned, parentTopic		//+1. 0=empty
		, SerialUnsigned, dependentTopic	//+1. 0=empty
		, Package::NetString<128>, name
		, Package::NetString<128>, iconName
		, int, maxProgress
	)
	typedef ::Protocol::NetArray<ResearchTopic,256> ResearchProgram;


	template <typename T>
		struct ResearchAttribute
		{
			typedef ResearchAttribute<T> Self;

			T		value;
			Package::NetString<128>	description;
			UINT8	flags;

			friend	void		Deserialize(IReadStream&s, Self&v)
			{
				using Serialization::Deserialize;
				Deserialize(s,v.value);
				Deserialize(s,v.description);
				Deserialize(s,v.flags);
			}
			friend void			Serialize(IWriteStream&s, const Self&v)
			{
				using Serialization::Serialize;
				Serialize(s,v.value);
				Serialize(s,v.description);
				Serialize(s,v.flags);
			}

			void				swap(Self&other)
			{
				using std::swap;
				swap(value,other.value);
				swap(flags,other.flags);
				description.swap(other.description);
			}
		};

	template <typename T>
		struct ResearchAttributeDelta : public ResearchAttribute<T>
		{
			typedef ResearchAttributeDelta<T> Self;
			typedef ResearchAttribute<T> Super;

			index_t	index;

			friend	void		Deserialize(IReadStream&s, Self&v)
			{
				using Serialization::Deserialize;
				v.index = s.ReadSize();
				Deserialize(s,(Super&)v);
			}
			friend void			Serialize(IWriteStream&s, const Self&v)
			{
				using Serialization::Serialize;
				s.WriteSize(v.index);
				Serialize(s,(Super&)v);
			}

			void				swap(Self&other)
			{
				using std::swap;
				swap(index,other.index);
				Super::swap(other);
			}
		};

	template <typename T>
		class ResearchAttributeArray : public ::Protocol::NetArray<ResearchAttribute<T>,256> {};
	template <typename T>
		class ResearchAttributeDeltaArray : public ::Protocol::NetArray<ResearchAttributeDelta<T>,256> {};

	NETSTRUCT4( ResearchAttributeVector
		, TUniqueID, faction
		, ResearchAttributeArray<int>, intAttributes
		, ResearchAttributeArray<float>, floatAttributes
		, ResearchAttributeArray<bool>, boolAttributes
	);

	NETSTRUCT4( ResearchAttributeDeltaVector
		, TUniqueID, faction
		, ResearchAttributeDeltaArray<int>, intAttributes
		, ResearchAttributeDeltaArray<float>, floatAttributes
		, ResearchAttributeDeltaArray<bool>, boolAttributes
	);


	//NETSTRUCT2(ResourceData
	//	, ResourceID, id
	//	, NETA(BYTE), data
	//)

	CONSTRUCT_ENUMERATION3(DropMode, LastTask, AllTasks,AllSafeTasks);

	NETSTRUCT2(DropTasks
		, NET(TUniqueID), targetID
		, NET(BYTE), dropMode
	)

	NETSTRUCT2(LoadPlanetByName
		, Package::NetString<256>, name
		, UINT32, configVersion
	)

	NETSTRUCT2(MyFaction
		, TUniqueID, factionID
		, AllianceConfigChange, alliance
	)

	NETSTRUCT2(ChatMessage
		, Package::NetString<256>, message
		, TUniqueID, recipient	//may be faction, alliance, or everybody (if empty)
	)
		

	NETSTRUCT5(BeginStatisticsSequence
		, TUniqueID, factionID
		, TUniqueID, allianceID
		, Package::NetString<MaxFactionNameLength>, factionName
		, float3, color
		, bool, factionIsDead
	)

	NETSTRUCT2(EntityTrigger
		, TUniqueID, entityID
		, UINT16,	triggerIndex
	)


	typedef NetArray<BYTE,100000>	ScriptedAIStateVector;

	NETSTRUCT2(ScriptedAIState
		, Package::NetString<64>, scriptName
		, ScriptedAIStateVector, dataVector
	);

	NETSTRUCT2(AllianceMarkerText
		, TUniqueID, markerID
		, Package::NetString<64>, markerText
	);

	typedef Objects::Resource::Table ResourceTable;


	typedef Package::NetString<32>	SaveAs;
	typedef TUniqueID	KillFaction;

	namespace ChannelID
	{
		enum id_t
		{
			Unused,

			Ping,				//c2s: <signal> sent once every two seconds, connection terminated after 10 seconds of inactivity
			Pong,				//s2c: <signal> sent as a response to Ping

			ControlMessage,		//s2c: ControlMessage
			GameProgress,		//s2c: GameProgress

			PhaseChange,		//s2c: PhaseChange
			BeginGame,			//s2c: <signal> //indicates that the player faction is fully initialized and transferred, and the game should begin. Mostly important for AI, only

			Hello,				//c2s: Hello -> s2c: HelloResponse
			ClientFaction,		//c2s: ClientFactionRequest -> s2c: ClientFactionResponse
			MyFaction,			//s2c: MyFaction

			RequestEverything,	//c2s: <signal> -> [everything] //(only if spectator) 
			BeginEverything,	//s2c: <signal> //indicates that now everything will be (re)sent
			EndEverything,		//s2c: <signal> //indicates that the server is done sending everything, and normal delta updates will resume

			ResourceTable,		//s2c : ResourceTable
			WorldDelta,			//s2c : WorldDelta

			Explosion,			//s2c : Explosion

			NewMobileEntity,	//s2c: NewMobileEntity
			MobileState,		//s2c: MobileState
			MobileEntityMotion,		//s2c: MobileEntityMotion
			PreciseAttackTrajectory,	//s2c: PreciseAttackTrajectory
			VagueAttackTrajectory,		//s2c: VagueAttackTrajectory
			EntityTurretRotation,	//s2c: EntityTurretRotation

			RemoveEntity,		//c2s: TUniqueID
			RestoreCoreLink,	//c2s: TUniqueID

			InstantiateStructure,	//s2c: StructureConstruction
			UpdateStructure,		//s2c: StructureState

			DestroyEntity,		//s2c: DestroyEntity

			EntityTrigger,		//c2s: EntityTrigger

			ChoiceIntersection,	//c2s: ChoiceIntersection [->ChoiceResponse, if requestID is set]
			ChoiceResponse,		//s2c: ChoiceResponse

			RequestRoundEnd,	//c2s: <signal>
			RequestContinueRound,	//c2s: <signal>

			EntityDirectCommand,//c2s: EntityDirectCommand

			DropTasks,			//c2s: DropTasks

			SignalNewJob,		//c2s: StandardJob
			SignalTerrainMod,	//c2s: TerrainMod

			ConstructionBegin,	//c2s: ConstructionBegin
			ConstructionAbort,	//c2s: ConstructionAbort

			EnableStructure,	//c2s: EnableStructure

			Automate,			//c2s: EntityStateUpdate
			Sleep,				//c2s: EntityStateUpdate

			ResourceUpdate,		//s2c: ResourceUpdate

			CreateMarker,		//c2s/s2c: Marker (non-dialogue)
			MarkerCreationFailedLimitReached,	//s2c: ClientMarker
			AllianceMarkerText,		//c2s: AllianceMarkerText
			DestroyMarker,		//c2s/s2c: TUniqueID
			TouchMarker,		//c2s: TouchedMarker

			IdentifyFaction,	// [c2s: TUniqueID -> ] s2c: IdentifyFaction
			KillFaction,		// s2c: KillFaction
			ChangeMyName,		// c2s: ClientName
			FactionOnline,		// s2c: TUniqueID
			FactionOffline,		// s2c: TUniqueID
			FactionRequestedNewRound,	// s2c: TUniqueID
			FactionContinuedRound,		// s2c: TUniqueID
//			RequestResource,	// c2s : ResourceID -> s2c : ResourceData
	//		ResourceData,		// s2c : ResourceData

			ResearchProgram,	// s2c : ResearchProgram
			ResearchFactionState,	// s2c : ResearchFactionState
			QueueResearch,		// c2s : QueueResearch -> s2c : ResearchQueue
			ResearchQueue,		// s2c : ResearchQueue
			ResearchAttributeVector,	// s2c : ResearchAttributeVector
			ResearchAttributeDeltaVector,	//s2c : ResearchAttributeDeltaVector

			LockEMPZones,		// s2c : UINT16
			UplinkBandwidth,	// s2c : int, can be negative to indicate it's disabled
			UnitLimits,			// s2c : UnitLimits

			ChatMessage,		// c2s: ChatMessage

			AllianceConfigChange,	//both: AllianceConfigChange
			AllianceInvite,		//both: AllianceInvite
			LeaveAlliance,		//c2s: <signal>


			VoteCreation,		//s2c: VoteCreation
			VoteResult,			//s2c: VoteResult
			ClientVote,			//c2s: ClientVote

			GameConfig,			// both: GameConfig
			OperationLacksResources, //s2c: OperationLacksResources

			TutorialAt,			// both: int32

			WinningAlliance,		// s2c: TUniqueID (if game has winner)

			BeginStatisticsSequence,	//s2c: BeginStatisticsSequence
			EndStatisticsSequence,		//s2c: <signal>
			RoundStatistics,			//s2c: Statistics::Round
			EntityTypeStatistics,		//s2c: Statistics::EntityType
			FactionInteractionStatistics,	//s2c: Statistics::FactionInteraction

			ScriptedAIState,			//[both]: ScriptedAIState

			SaveAs,	//c2s: SaveAs
		};
	}




	/**
	Primary UDP broadcast package to notify potential clients of the server's existence.
	Keep in sync with [ServerManager:]ServerInfoReceiver.cs and [ServerManager:]UDP Receiver.cs
	*/
	struct THostBroadcastPacket
	{
		typedef THostBroadcastPacket Self;
		TUniqueID	hostID;
		char		version[64];
		UINT16		serverBasePort;


		friend void Serialize(IWriteStream&s, const Self&v)
		{
			using Serialization::Serialize;
			Serialize(s,v.hostID);
			s.Write(v.version,sizeof(v.version));
			Serialize(s,v.serverBasePort);
		}
		friend void Deserialize(IReadStream&s, Self&v)
		{
			using Serialization::Deserialize;
			Deserialize(s,v.hostID);
			s.Read(v.version,sizeof(v.version));
			Deserialize(s,v.serverBasePort);
		}
	};

	static const size_t THostBroadcastPacket_size = sizeof(THostBroadcastPacket);

	static_assert(sizeof(THostBroadcastPacket) == sizeof(TUniqueID) + 64 + 2, "Struct packing not appropriate for host broadcast packet");//make sure there are no gaps



	struct THostToManagerPacket
	{
		enum flag_t
		{
			SessionClosed = 0x1,
			RequireHeartbeats = 0x2,
		};

		TUniqueID	hostID;
		UINT16		flags;
		UINT16		serverBasePort;
	};
	static_assert(sizeof(THostToManagerPacket) == sizeof(TUniqueID) + 2 + 2, "Struct packing not appropriate for host manager packet");//make sure there are no gaps


	/**
	Flags that may or not be set for a game.
	Each signifies a particular switch being on, or in absence off.
	Keep in sync with [ServerManager:]ServerInfoReceiver.cs
	*/
	namespace GameFlag
	{
		enum flag_t
		{
			ClosedGame = 0x1,
			BasesRequired = 0x2,
			StartBases = 0x4,
			NexusAI = 0x8,
			EMPZones = 0x10,
			SimulateLiquids = 0x20,
			TutorialMode = 0x40,
			GameHasEnded = 0x80,
			NexusHasBases = 0x100,
			NexusPortsOnAllPoles = 0x200,
			RequireServerPassword = 0x400,

			SupportSpectatorFactions = 0x1000,
			SupportRegularFactions = 0x2000,


		};

		static const UINT32 AllFactionSupport = ~(SupportSpectatorFactions-1);

	}




	/**
	Info package regularly sent by the server to connected info listeners (TCP)
	Keep in sync with [ServerManager:]ServerInfoReceiver.cs
	*/
	NETSTRUCT2(ServerInfo
		, NET(THostBroadcastPacket), hostInfo
		, GameInfo, gameInfo
	)
	
	typedef NET(UINT64)		Ping;

	namespace InfoChannel
	{
		enum channel_t
		{
			ServerInfo,	//s2c: ServerInfo
			Ping,
		
		
		};
	
	}



}




#endif

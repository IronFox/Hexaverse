#ifndef networkComponentH
#define networkComponentH

#include <protocol.h>
#include <ports.h>
#include <network/steamInterface.h>
#include <network/net.h>


/**
All network-related aspects. Multiple additions to this namespace exist.
*/
namespace Network
{

	/**
	Checks whether or not user-input is currently allowed
	*/
	bool						ControlEnabled();


	extern bool			haveFactionLoaded,
						haveID,
						amSpectator,
						amOnline,
						gameHasBegun,
						haveFactionOnServer;
	#ifdef HAS_WINDOW
		extern bool		amLocated;
	#endif

	extern TUniqueID			myID,worldID;
	extern UINT32				gameFlags;
	extern String				myIDString,worldIDString;
	extern UINT16				connectionBasePort;
	


	extern Protocol::GameProgress	gameProgress;
	extern TUniqueID				winningAllianceID;

	extern float			animationTimeFactor,	//!< Speed factor applied to active animations. Updated when server issues next animation phase.
							roundTimer;
	extern Time::Stamp		phaseStarted;
	extern bool				newRoundRequested;
	extern bool				serverRequiresPassword;


	bool	IsValidNickname(const String&candidate);
	bool	IsValidNickname(const UTF32StringRef&candidate);
	void	SetSteamPersona();
	void	ResetNickname();
	void	ConnectToLobby(UINT64 lobbyID);
	void	SteamLobbyLost(UINT64 lobbyID);


	class ServerIPAddress
	{
	public:
		String				host;
		UINT16				basePort;


		/**/				ServerIPAddress():basePort(DEFAULT_SERVER_BASE_PORT){}
		explicit			ServerIPAddress(const String&url);

		bool				operator==(const ServerIPAddress&other) const
		{
			return basePort == other.basePort && host == other.host;
		}
		bool				operator!=(const ServerIPAddress&other) const
		{
			return !operator==(other);
		}

		bool				IsLocalhost() const {return host == "localhost" || host == "127.0.0.1" || host == "::1";}
	
		void				swap(ServerIPAddress&other)
		{
			host.swap(other.host);
			swp(basePort,other.basePort);
		}

		friend void			swap(ServerIPAddress&a,ServerIPAddress&b)
		{
			a.swap(b);
		}

		friend hash_t		Hash(const ServerIPAddress&addr)
		{
			return HashValue() << addr.host << addr.basePort;
		}

		friend String		ToString(const ServerIPAddress&addr) 
		{
			return addr.basePort != DEFAULT_SERVER_BASE_PORT ? addr.host + ':' + String(addr.basePort) : addr.host;
		}
	};



	class ServerAddress : public ServerIPAddress
	{
	public:
		typedef ServerIPAddress Super;
		UINT64				steamID=0;
		Protocol::ServerInfo	info;


		/**/				ServerAddress(){}
		explicit			ServerAddress(const String&url, UINT64 steamID, const Protocol::ServerInfo&info);
		explicit			ServerAddress(const ServerIPAddress&a, const Protocol::ServerInfo&info):Super(a),info(info)	{};

		bool				operator==(const ServerAddress&other) const
		{
			return Super::operator==(other) && steamID == other.steamID;
		}
		bool				operator!=(const ServerAddress&other) const
		{
			return !operator==(other);
		}
	
		void				swap(ServerAddress&other)
		{
			Super::swap(other);
			swp(steamID,other.steamID);
			info.swap(other.info);
		}

		friend hash_t		Hash(const ServerAddress&addr)
		{
			return HashValue() << ((const Super&)addr) << addr.steamID;
		}

		bool				IsLocalhost() const {return steamID ? false : Super::IsLocalhost();}

		friend String		ToString(const ServerAddress&addr)
		{
			if (addr.steamID)
				return SteamInterface::IDToString(addr.steamID);
			using StringConversion::ToString;
			return ToString((const Super&)addr);
		}
	};

	extern ServerAddress	lastServerAddress;

}

DECLARE_DEFAULT_STRATEGY(Network::ServerAddress,Swap);
DECLARE_DEFAULT_STRATEGY(Network::ServerIPAddress,Swap);

namespace Network
{


	Objects::OwnerParameter ResolveFaction(const TUniqueID&id, const TUniqueID&allianceID);
	Objects::OwnerParameter ResolveFaction(const TUniqueID&id);
	//void					ConnectTo(const ServerAddress&, UINT64 lobbyID);
	bool DidVisit(const TUniqueID&worldID);
	void AbandonCurrentFaction();
	bool HaveFactionKeyForCurrentWorld();

	void AttemptFactionLogin(Protocol::FactionOperation::op_t, UINT32 factionTypes, const String&serverPassword);
	#ifdef HAS_WINDOW
		void QueryPasswordAndLogin(Protocol::FactionOperation::op_t, UINT32 factionTypes);
		void AttemptFactionLogin(Protocol::FactionOperation::op_t, UINT32 factionTypes);
	#endif

	/**
	Signals that server and client states have deteriorated to a point where communication cannot continue.
	Causes imminent disconnection
	*/
	void	SignalProtocolDisruption(const String&message);
	
	bool CanSendFactionRequests();
	
	namespace ServerDiscovery
	{
		bool	ListeningButCantConnectTo(const ServerIPAddress&);
		bool	TryRemoveHost(const ServerIPAddress&);

		void OnServerInfo(Protocol::ServerInfo&info, TCP::Peer&peer);
		void OnPing(Protocol::Ping&ping, TCP::Peer&peer);

		extern TCP::Channel<Protocol::ServerInfo,0,Protocol::InfoChannel::ServerInfo>	serverInfoChannel;
		extern TCP::Channel<Protocol::Ping,0,Protocol::InfoChannel::Ping>	pingChannel;

		/**
		Queries all connection attempts that have so far not successfully connected.
		Only non-automatic (user-registered) attempts are listed.
		*/
		void	ListFailedAttempts(Buffer0<ServerIPAddress,Swap>&outList);
		/**
		Queries all connection attempts (successful or not).
		Only non-automatic (user-registered) attempts are listed.
		*/
		void	ListNonAutoAttempts(Buffer0<ServerIPAddress>&outList);

		bool	RegisterNewHost(UINT64 lobbyID, UINT64 ownerID,const ServerAddress&, bool probablyRightVersion, bool automaticallyDiscovered, bool makePersistent=false);



	}


	namespace Receiver
	{
		extern bool			eventsLocked/* = true*/;	//purely for debugging purposes. If set no events should be pouring in from the server. When the connection is lost, this is set to true, and no evenst should be triggered until a new connection is established


		void	OnHelloResponse(Protocol::HelloResponse&response);
		void	OnWorldDelta(Protocol::WorldDelta&delta);




		void	OnMobileState(Protocol::MobileState&vs);

		void	OnNewMobileEntity(Protocol::NewMobileEntity&vc);

		

		void	OnNewStructure(Protocol::StructureConstruction&sc);
		void	OnStructureState(Protocol::StructureState&vs);

		void	DestroyEntity(Protocol::DestroyEntity&de);
		void	ControlMessage(Protocol::ControlMessage&msg);

		void	ClientFactionRequest(Protocol::ClientFactionRequest&cr);

		void	SetFactionID(NET(TUniqueID)&id);
		void	PhaseChange(Protocol::PhaseChange&c);
		void	PreciseAttackTrajectory(Protocol::PreciseAttackTrajectory&a);
		void	VagueAttackTrajectory(Protocol::VagueAttackTrajectory&a);

		void	Explosion(Protocol::Explosion&ex);

		void	EntityTurretRotation(Protocol::EntityTurretRotation&rot);


		void	ResourceUpdate(Protocol::ResourceUpdate&u);

		void	GameProgress(Protocol::GameProgress&progress);

		void	CreateMarker(Protocol::Marker&marker);
		void	MarkerCreationFailedLimitReached(Protocol::Marker&marker);	//must re-send, must know channels, cannot be implemented here

		void	DestroyMarker(NET(TUniqueID)&mid);

		void	IdentifyFaction(Protocol::IdentifyFaction&id);
		void	FactionOnline(NET(TUniqueID)&);
		void	FactionOffline(NET(TUniqueID)&);
		void	FactionRequestedNewRound(NET(TUniqueID)&);
		void	FactionContinuedRound(NET(TUniqueID)&);
		void	LockEmpZones(NET(UINT16)&);
		void	PlanetConfig(Grid::ExtendedPlanetConfig&);
		void	GameConfig(Protocol::GameConfig&);
		void	TutorialAt(NET(INT32)&);

		namespace Statistics
		{
			void	BeginSequence(TUniqueID&id);
			void	EndSequence();
			void	Round(Objects::Statistics::Round&);
			void	FactionInteraction(Objects::Statistics::FactionInteraction&);
			void	EntityType(Objects::Statistics::EntityType&);
		}
	}


	
	#undef DECLARE_CHANNEL
	#define	DECLARE_CHANNEL(NAME)	extern TCP::Channel<Protocol::NAME,0,Protocol::ChannelID::NAME>	NAME;
	#undef DECLARE_OBJECT_SENDER
	#define	DECLARE_OBJECT_SENDER(NAME)	extern TCP::ObjectSender<Protocol::NAME,Protocol::ChannelID::NAME>	NAME;

	namespace Channels
	{
		extern TCP::Channel<Protocol::ControlMessage,0,Protocol::ChannelID::ControlMessage>				controlMessage;

		extern TCP::Channel<Protocol::ServerHello,0,Protocol::ChannelID::ServerHello>					serverHello;
		extern TCP::Channel<Protocol::HelloResponse,0,Protocol::ChannelID::Hello>						helloResponse;
		extern TCP::Channel<Protocol::WorldDelta,0,Protocol::ChannelID::WorldDelta>						worldData;
		extern TCP::Channel<Protocol::NewMobileEntity,0,Protocol::ChannelID::NewMobileEntity>			newMobileEntity;
		extern TCP::Channel<Protocol::MobileState,0,Protocol::ChannelID::MobileState>					mobileStateUpdate;
		extern TCP::Channel<Protocol::StructureConstruction,0,Protocol::ChannelID::InstantiateStructure>newStructure;
		extern TCP::Channel<Protocol::StructureState,0,Protocol::ChannelID::UpdateStructure>			structureStateUpdate;
		extern TCP::Channel<Protocol::DestroyEntity,0,Protocol::ChannelID::DestroyEntity>				destroyEntity;
		extern TCP::Channel<Protocol::ClientFactionResponse,0,Protocol::ChannelID::ClientFaction>		clientFactionResponse;

		extern TCP::Channel<Protocol::PhaseChange,0,Protocol::ChannelID::PhaseChange>					phaseChange;
		extern TCP::Channel<Protocol::MobileEntityMotion,0,Protocol::ChannelID::MobileEntityMotion>		mobileEntityMotion;
		extern TCP::Channel<Protocol::VagueAttackTrajectory,0,Protocol::ChannelID::VagueAttackTrajectory>			vagueAttackTrajectory;
		extern TCP::Channel<Protocol::PreciseAttackTrajectory,0,Protocol::ChannelID::PreciseAttackTrajectory>		preciseAttackTrajectory;
		extern TCP::Channel<Protocol::Explosion,0,Protocol::ChannelID::Explosion>						explosion;
		extern TCP::Channel<Protocol::EntityTurretRotation,0,Protocol::ChannelID::EntityTurretRotation>	entityTurretRotation;
		extern TCP::Channel<Protocol::ResourceUpdate,0,Protocol::ChannelID::ResourceUpdate>				resourceUpdate;
		extern TCP::Channel<Protocol::GameProgress,0,Protocol::ChannelID::GameProgress>					gameProgress;

		extern TCP::Channel<Protocol::Marker, 0, Protocol::ChannelID::CreateMarker>						createMarker;
		extern TCP::Channel<Protocol::Marker, 0, Protocol::ChannelID::MarkerCreationFailedLimitReached>	markerCreationFailedLimitReached;
		extern TCP::Channel<NET(TUniqueID), 0, Protocol::ChannelID::DestroyMarker>						destroyMarker;

		extern TCP::Channel<Protocol::IdentifyFaction, 0, Protocol::ChannelID::IdentifyFaction>			identifyFaction;
		extern TCP::Channel<NET(TUniqueID), 0, Protocol::ChannelID::FactionOnline>						factionOnline;
		extern TCP::Channel<NET(TUniqueID), 0, Protocol::ChannelID::FactionOffline>						factionOffline;
		extern TCP::Channel<NET(TUniqueID), 0, Protocol::ChannelID::FactionRequestedNewRound>			factionRequestedNewRound;
		extern TCP::Channel<NET(TUniqueID), 0, Protocol::ChannelID::FactionContinuedRound>				factionContinuedRound;
		extern TCP::Channel<NET(UINT16), 0, Protocol::ChannelID::LockEMPZones>							lockEmpZones;
		extern TCP::Channel<NET(int32_t), 0, Protocol::ChannelID::UplinkBandwidth>						uplinkBandwidth;
		extern TCP::Channel<Protocol::OperationLacksResources, 0, Protocol::ChannelID::OperationLacksResources>			operationLacksResources;




		extern TCP::Channel<Protocol::GameConfig,0,Protocol::ChannelID::GameConfig>						gameConfig;

		extern TCP::Channel<NET(INT32),0,Protocol::ChannelID::TutorialAt>								tutorialAt;

		namespace Statistics
		{
			extern TCP::Channel<Objects::Statistics::Round, 0, Protocol::ChannelID::RoundStatistics>		round;
			extern TCP::Channel<Objects::Statistics::EntityType, 0, Protocol::ChannelID::EntityTypeStatistics>	entityType;
			extern TCP::Channel<Objects::Statistics::FactionInteraction, 0, Protocol::ChannelID::FactionInteractionStatistics>	factionInteraction;

			DECLARE_CHANNEL(BeginStatisticsSequence);
		}

		extern TCP::ObjectSender<Protocol::ClientFactionRequest,Protocol::ChannelID::ClientFaction>		clientFactionRequest;
		extern TCP::ObjectSender<Protocol::TouchedMarker,Protocol::ChannelID::TouchMarker>					touchMarker;
		extern TCP::ObjectSender<Protocol::Hello,Protocol::ChannelID::Hello>								hello;
		extern TCP::ObjectSender<Protocol::EntityDirectCommand, Protocol::ChannelID::EntityDirectCommand>	entityDirectCommand;
		extern TCP::ObjectSender<Protocol::ChoiceIntersection, Protocol::ChannelID::ChoiceIntersection>		choiceIntersection;
		extern TCP::ObjectSender<Protocol::DropTasks, Protocol::ChannelID::DropTasks>					dropTasks;
		extern TCP::ObjectSender<Protocol::StandardJob, Protocol::ChannelID::SignalNewJob>				signalNewJob;
		extern TCP::ObjectSender<Protocol::TerrainMod, Protocol::ChannelID::SignalTerrainMod>			signalTerrainMod;
		extern TCP::ObjectSender<Protocol::ConstructionAbort, Protocol::ChannelID::ConstructionAbort>	constructionAbort;
		extern TCP::ObjectSender<Protocol::ConstructionBegin, Protocol::ChannelID::ConstructionBegin>	constructionBegin;
		extern TCP::ObjectSender<Protocol::EnableStructure, Protocol::ChannelID::EnableStructure>		enableStructure;
		extern TCP::ObjectSender<Protocol::EntityStateUpdate,Protocol::ChannelID::Automate>				automate;
		extern TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::RestoreCoreLink>					restoreCoreLink;
		extern TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::RemoveEntity>						removeEntity;
		extern TCP::ObjectSender<Protocol::EntityStateUpdate, Protocol::ChannelID::Sleep>				sleep;
		extern TCP::ObjectSender<Protocol::ClientName, Protocol::ChannelID::ChangeMyName>				changeMyName;
		
		DECLARE_CHANNEL(ResourceTable);

		DECLARE_CHANNEL(MyFaction);
		DECLARE_CHANNEL(AllianceConfigChange);
		DECLARE_CHANNEL(VoteCreation);
		DECLARE_CHANNEL(VoteResult);
		DECLARE_OBJECT_SENDER(ClientVote);
		DECLARE_CHANNEL(AllianceInvite);
		DECLARE_OBJECT_SENDER(ChatMessage);
		DECLARE_OBJECT_SENDER(SaveAs);
		DECLARE_CHANNEL(KillFaction);

		DECLARE_CHANNEL(ResearchProgram);
		DECLARE_CHANNEL(ResearchFactionState);
		DECLARE_CHANNEL(ResearchQueue);
		DECLARE_OBJECT_SENDER(QueueResearch);
		DECLARE_CHANNEL(T2Globals);
		DECLARE_CHANNEL(ResearchAttributeVector);
		DECLARE_CHANNEL(ResearchAttributeDeltaVector);

		DECLARE_CHANNEL(ChoiceResponse);

		DECLARE_CHANNEL(ScriptedAIState);
		DECLARE_CHANNEL(NexusAIHints);

		DECLARE_OBJECT_SENDER(EntityTrigger);
		DECLARE_CHANNEL(AllianceMarkerText);

		DECLARE_OBJECT_SENDER(TransferEntityOwnership);
		DECLARE_OBJECT_SENDER(MobilizeStructure);
		DECLARE_OBJECT_SENDER(SettleMobile);
	}


	extern SteamNetwork::HybridClient		client;
	//extern SOCKET			broadcastListenSocket;

	extern String			chosenNickname;
	extern bool				customNickname;
	//extern Timer::Time		lastPong;

	extern INT32			uplinkBandwidth;


	void		OnEvent(TCP::event_t ev, TCP::Peer&);

	void		OnSignal(UINT32 signal, TCP::Peer&origin);

	void		Setup();

	void		LoadHosts();
	void		SaveHosts();


	class Label
	{
	public:
		String					text;
		//float					visualLength;
		float4					color = M::Vector4<>::one;
		

		void					swap(Label&other)
		{
			text.swap(other.text);
			swp(color,other.color);
			//swp(visualLength,other.visualLength);
		}

	};



	class ServerInfo
	{
	public:
		class VisualAttachment
		{
		public:
			virtual ~VisualAttachment() {};
		};
		bool					rightVersion = false,
								protocolIncompatible = false;
		
		Buffer0<ServerAddress>	addresses;
		float					lastPing=0;
		UINT64					ownerID=0,lobbyID=0;
		#ifdef USE_STEAM_OWNER_AUTHENTICATION
			bool					authenticated=false;
		#endif
		String					ownerName;
		
		//float					top, bottom;
		bool					selected;

		std::shared_ptr<VisualAttachment>	visualAttachment;


		/**/					ServerInfo() :/*top(0), bottom(0),*/ selected(false)
		//, haveToken(false)	
		{}

		//bool					HasURL(const String&url) const
		//{
		//	foreach(urls, u)
		//		if (u->first.host == url)
		//			return true;
		//	return false;
		//}

		bool					HasLocalhost() const
		{
			foreach (addresses,u)
				if (u->IsLocalhost())
					return true;
			return false;
		}

		bool					HasAddress(const ServerAddress&addr) const
		{
			return addresses.Contains( addr );
		}


		bool					UpdateSteamAddress(const ServerAddress&addr)
		{
			foreach (addresses, a)
				if (a->steamID)
				{
					(*a) = addr;
					return true;	//should be only one
				}
			DBG_FATAL__("Trying to update non-existent steam address");
			return false;
		}


		bool					UpdateNonSteamAddress(const ServerAddress&addr)
		{
			foreach (addresses, a)
				if (!a->steamID)
				{
					(*a) = addr;
					return true;	//should be only one
				}
			DBG_FATAL__("Trying to update non-existent TCP address");
			return false;
		}


		UINT32					GetGeneralGameFlags() const
		{
			if (addresses.IsEmpty())
				return 0;
			return addresses.First().info.gameInfo.gameConfig.gameFlags;
		}

		bool					IsPrivate() const
		{
			return !!(GetGeneralGameFlags() & Protocol::GameFlag::PrivateServer);
		}

		const TUniqueID			GetWorldID() const
		{
			static const TUniqueID empty = TUniqueID::Empty();
			if (addresses.IsEmpty())
				return empty;
			return addresses.First().info.gameInfo.worldID;
		}

		bool					IsWorldID(const TUniqueID&id) const
		{
			return GetWorldID() == id;
		}


		const ServerAddress&	GetPrimaryAddress() const
		{
			bool forceNonSteam = false;
			foreach (addresses,a)
				if (!a->steamID)
					return *a;	//prioritize IP address, so spectators are possible
			static ServerAddress emptyAddress;
			if (forceNonSteam)
			{
				LogEvent(EventClass::Preset::networkError,"Operation requires non-steam address, but none known for this host");
				return emptyAddress;
			}
			return addresses.First();
		}

		/**
		Attempts connection to the primary server of the local info structure.
		Sets all local parameters live, if an address is available and an attempt is made
		*/
		void					Connect()	const;

		bool					HasNonSteamAddress() const
		{
			foreach (addresses,addr)
				if (addr->steamID == 0)
					return true;
			return false;
		}
		bool					HasSteamAddress() const
		{
			foreach (addresses,addr)
				if (addr->steamID != 0)
					return true;
			return false;
		}

		void					swap(ServerInfo&other)
		{
//			swp(id,other.id);
			swp(ownerID,other.ownerID);
			swp(lobbyID,other.lobbyID);
			ownerName.swap(other.ownerName); 
			swp(selected,other.selected);
			#ifdef USE_STEAM_OWNER_AUTHENTICATION
				swp(authenticated,other.authenticated);
			#endif
			addresses.swap(other.addresses);
			//swp(haveToken,other.haveToken);
			swp(rightVersion,other.rightVersion);
			visualAttachment.swap(other.visualAttachment);

		}
	};

	extern Buffer0<ServerInfo,Swap>	servers;

	extern Time::Stamp		lastPing,lastServerTableUpdate;

	/**
	Last attempted server host.
	Not cleared upon connection loss
	*/
	extern ServerAddress	lastServerAddress;
	extern UINT64			lastLobbyID;
	/**
	Set to non zero to automatically connect to the specified lobby if it comes up
	*/
	extern UINT64			autoConnectToLobby;
	

	void					SendChatMessage(const String&msg, const Objects::PFaction&receiver);
	void					SendChatMessage(const String&msg, const Objects::PAlliance&receiver);
	void					BroadcastChatMessage(const String&msg);

	/**
	Removes a host from all currently known tcp/steam servers
	*/
	void					RemoveServer(const ServerAddress&, UINT64 lobbyID);

	/**
	Triggers a server list refresh
	*/
	void					RefreshServerListNow();
	void					SetSteamDiscovery(bool enableDiscovery);
	bool					SteamDiscoveryIsEnabled();
	bool					ForcingTCPConnections();
	void					ForceTCPConnections(bool doForce);


	void		Update();

	void		Shutdown();


	bool		ControlEnabled( );


	bool		ConnectToConfiguredServerIfAvailable();

}



#endif

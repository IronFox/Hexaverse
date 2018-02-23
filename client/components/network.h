#ifndef networkComponentH
#define networkComponentH

#include <protocol.h>
//#include "visual/guiElements.h"

/**
All network-related aspects. Multiple additions to this namespace exist.
*/
namespace Network
{

	/**
	Checks whether or not user-input is currently allowed
	*/
	bool						ControlEnabled();


	extern bool			haveFaction,
						haveID,
						amSpectator,
						amOnline,
						gameHasBegun;
	#ifdef HAS_WINDOW
		extern bool		amLocated;
	#endif

	extern TUniqueID			myID,serverID,worldID;
	extern UINT32				gameFlags;
	extern CryptographicID		activeToken;
	extern String				myIDString,serverIDString,worldIDString,activeTokenString;
	extern UINT16				connectionBasePort;
	
	extern GenericHashTable<TUniqueID,CryptographicID>	accessMap;


	extern Protocol::GameProgress	gameProgress;
	extern TUniqueID				winningFactionID;

	extern float			animationTimeFactor,	//!< Speed factor applied to active animations. Updated when server issues next animation phase.
							roundTimer;
	extern Timer::Time		phaseStarted;
	extern bool				newRoundRequested;


	bool	IsValidNickname(const String&candidate);


	class ServerAddress
	{
	public:
		String				host;
		UINT16				basePort;


		/**/				ServerAddress():basePort(DEFAULT_SERVER_BASE_PORT){}
		explicit			ServerAddress(const String&url);

		bool				operator==(const ServerAddress&other) const
		{
			return basePort == other.basePort && host == other.host;
		}

		void				adoptData(ServerAddress&other)
		{
			host.adoptData(other.host);
			basePort = other.basePort;
		}

		bool				IsLocalhost() const {return host == "localhost" || host == "127.0.0.1" || host == "::1";}

		friend String		ToString(const ServerAddress&a) {return a.basePort != DEFAULT_SERVER_BASE_PORT ? a.host + ':' + String(a.basePort) : a.host;}
	};

	class Host
	{
	public:
		ServerAddress		address;
		Protocol::ServerInfo	serverInfo;
		float				ping;

		void				adoptData(Host&other)
		{
			address.adoptData(other.address);
			serverInfo = other.serverInfo;
			ping = other.ping;
		}
	};

}

DECLARE_DEFAULT_STRATEGY(Network::ServerAddress,Adopt);
DECLARE_DEFAULT_STRATEGY(Network::Host,Adopt);

namespace Network
{


	Objects::OwnerParameter ResolveFaction(const TUniqueID&id, const TUniqueID&allianceID);
	Objects::OwnerParameter ResolveFaction(const TUniqueID&id);

	void					ConnectTo(const Host&);
	bool DidVisit(const TUniqueID&worldID);
	void SaveWorldAccessTokens();

	void LoadWorldAccessTokens();

	void AbandonCurrentFaction();
	bool HaveFactionKeyForCurrentWorld();
	void AttemptFactionLogin(bool forceNewFaction, UINT32 factionTypes);

	/**
	Signals that server and client states have deteriorated to a point where communication cannot continue.
	Causes imminent disconnection
	*/
	void	SignalProtocolDisruption(const String&message);
	
	namespace ServerDiscovery
	{
		bool	ListeningButCantConnectTo(const ServerAddress&);
		bool	TryRemoveHost(const ServerAddress&);

		void OnServerInfo(Protocol::ServerInfo&info, TCP::Peer&peer);
		void OnPing(Protocol::Ping&ping, TCP::Peer&peer);

		extern TCP::Channel<Protocol::ServerInfo,0,Protocol::InfoChannel::ServerInfo>	serverInfoChannel;
		extern TCP::Channel<Protocol::Ping,0,Protocol::InfoChannel::Ping>	pingChannel;

		/**
		Queries all connection attempts that have so far not successfully connected.
		Only non-automatic (user-registered) attempts are listed.
		*/
		void	ListFailedAttempts(Buffer0<ServerAddress,Adopt>&outList);
		/**
		Queries all connection attempts (successful or not).
		Only non-automatic (user-registered) attempts are listed.
		*/
		void	ListNonAutoAttempts(Buffer0<ServerAddress>&outList);

		bool	RegisterNewHost(const ServerAddress&, bool probablyRightVersion, bool automaticallyDiscovered);



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
		void	VehicleMotion(Protocol::MobileEntityMotion&m);
		void	PreciseAttackTrajectory(Protocol::PreciseAttackTrajectory&a);
		void	VagueAttackTrajectory(Protocol::VagueAttackTrajectory&a);

		void	Explosion(Protocol::Explosion&ex);

		void	EntityTurretRotation(Protocol::EntityTurretRotation&rot);


		void	ResourceUpdate(Protocol::ResourceUpdate&u);

		void	ChangeEntityOwner(Protocol::ChangeEntityOwner&c);
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
		void	ResourceData(Protocol::ResourceData&);
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
		extern TCP::Channel<Protocol::ChangeEntityOwner,0,Protocol::ChannelID::ChangeEntityOwner>		changeEntityOwner;
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
		extern TCP::Channel<Protocol::ResearchState, 0, Protocol::ChannelID::ResearchState>				researchState;
		extern TCP::Channel<Protocol::ResearchQueue, 0, Protocol::ChannelID::ResearchQueue>				researchQueue;
		extern TCP::Channel<Protocol::ResourceData,0,Protocol::ChannelID::ResourceData>					resourceData;
		extern TCP::Channel<Protocol::GameConfig,0,Protocol::ChannelID::GameConfig>						gameConfig;

		extern TCP::Channel<NET(INT32),0,Protocol::ChannelID::TutorialAt>								tutorialAt;

		namespace Statistics
		{
			extern TCP::Channel<TUniqueID, 0, Protocol::ChannelID::BeginStatisticsSequence>				beginSequence;
			extern TCP::Channel<Objects::Statistics::Round, 0, Protocol::ChannelID::RoundStatistics>		round;
			extern TCP::Channel<Objects::Statistics::EntityType, 0, Protocol::ChannelID::EntityTypeStatistics>	entityType;
			extern TCP::Channel<Objects::Statistics::FactionInteraction, 0, Protocol::ChannelID::FactionInteractionStatistics>	factionInteraction;
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
		extern TCP::ObjectSender<Protocol::ResourceID, Protocol::ChannelID::RequestResource>			requestResource;
		extern TCP::ObjectSender<Protocol::QueueResearch, Protocol::ChannelID::QueueResearch>			queueResearch;
		

		DECLARE_CHANNEL(MyFaction);
		DECLARE_CHANNEL(AllianceConfigChange);
		DECLARE_CHANNEL(VoteCreation);
		DECLARE_CHANNEL(VoteResult);
		DECLARE_OBJECT_SENDER(ClientVote);
		DECLARE_CHANNEL(AllianceInvite);
		DECLARE_OBJECT_SENDER(ChatMessage);
		DECLARE_OBJECT_SENDER(SaveAs);
	}


	extern TCP::Client		client;
	extern SOCKET			broadcastListenSocket;

	extern String			chosenNickname;
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

		TUniqueID				id;
		bool					rightVersion = false,
								protocolIncompatible = false;
//		Protocol::ServerInfo	info;
		



		Buffer0<Host>			addresses;
		
		//float					top, bottom;
		bool					selected, haveToken;

		std::shared_ptr<VisualAttachment>	visualAttachment;



		/**/					ServerInfo() :/*top(0), bottom(0),*/ selected(false), haveToken(false)	{}

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
				if (u->address.IsLocalhost())
					return true;
			return false;
		}

		bool					HasAddress(const ServerAddress&addr) const
		{
			foreach(addresses, u)
				if (u->address == addr)
					return true;
			return false;
		}

		void					swap(ServerInfo&other)
		{
			swp(id,other.id);
			//swp(top,other.top);
			//swp(bottom,other.bottom);
			swp(selected,other.selected);
			addresses.swap(other.addresses);
			swp(haveToken,other.haveToken);
			swp(rightVersion,other.rightVersion);
			visualAttachment.swap(other.visualAttachment);

		}
	};

	extern Buffer0<ServerInfo,Swap>	servers;
	extern GenericHashTable<TUniqueID,index_t>	serverMap;

	extern Timer::Time		lastPing,lastServerTableUpdate;

	extern const String		allowedChatCharacters;


	void					SendChatMessage(const String&msg, const Objects::PFaction&receiver);
	void					SendChatMessage(const String&msg, const Objects::PAlliance&receiver);
	void					BroadcastChatMessage(const String&msg);

	void		Shutdown();


	bool		ControlEnabled( );


	bool		ConnectToConfiguredServerIfAvailable();

}



#endif

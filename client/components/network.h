#ifndef networkComponentH
#define networkComponentH

#include "../../shared/protocol.h"

/**
All network-related aspects. Multiple additions to this namespace exist.
*/
namespace Network
{
	extern Protocol::Phase		serverPhase;

	/**
	Checks whether or not user-input is currently allowed
	*/
	bool						ControlEnabled();


	extern bool			haveFaction,
						haveId,
						amOnline;
	#ifdef HAS_WINDOW
		extern bool		amLocated;
	#endif

	extern TUniqueID			myID,serverID,worldID;
	extern CryptographicID		activeToken;
	extern String				myIDString,serverIDString,worldIDString,activeTokenString;
	extern Objects::OwnerParameter myFaction;
	
	extern GenericHashTable<TUniqueID,CryptographicID,TUniqueID>	accessMap;
	extern GenericHashTable<TUniqueID,Objects::OwnerParameter,TUniqueID>	factionMap;


	extern UINT32			chosenFactionTypes;

	extern UINT32			roundNumber;
					//dbgRoundNumber = 0xFFFFFFFF;
	extern Protocol::GameProgress	gameProgress;

	extern Protocol::Phase	serverPhase;
	extern float			animationTimeFactor,	//!< Speed factor applied to active animations. Updated when server issues next animation phase.
							roundTimer;
	extern Timer::Time		phaseStarted;
	extern bool				newRoundRequested;

	static const char*accessFilename = "config/worldAccess.m32";


	Objects::OwnerParameter ResolveFaction(const TUniqueID&id);

	void SaveWorldAccessTokens();

	void LoadWorldAccessTokens();

	void AbandonCurrentFaction();
	bool HaveFactionKeyForCurrentWorld();
	void AttemptFactionLogin(bool forceNewFaction);

	
	namespace ServerDiscovery
	{


		void OnServerInfo(Protocol::ServerInfo&info, TCP::Peer&peer);
		void OnPing(Protocol::Ping&ping, TCP::Peer&peer);

		extern TCP::Channel<Protocol::ServerInfo,0,Protocol::InfoChannel::ServerInfo>	serverInfoChannel;
		extern TCP::Channel<Protocol::Ping,0,Protocol::InfoChannel::Ping>	pingChannel;



		void	RegisterNewHost(const String&host, bool probablyRightVersion);

	}


	namespace Receiver
	{
		extern bool			eventsLocked/* = true*/;	//purely for debugging purposes. If set no events should be pouring in from the server. When the connection is lost, this is set to true, and no evenst should be triggered until a new connection is established


		void	OnClientStateResponse(Protocol::Result&response);
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
		void	AttackTrajectory(Protocol::AttackTrajectory&a);

		void	Explosion(Protocol::Explosion&ex);

		void	EntityTurretRotation(Protocol::EntityTurretRotation&rot);


		void	ResourceUpdate(Protocol::ResourceUpdate&u);

		void	ChangeEntityOwner(Protocol::ChangeEntityOwner&c);
		void	GameProgress(Protocol::GameProgress&progress);

		void	CreateMarker(Protocol::Marker&marker);
		void	MarkerCreationFailedLimitReached(Protocol::Marker&marker);	//must re-send, must know channels, cannot be implemented here

		void	DestroyMarker(NET(TUniqueID)&mid);

		void	IdentifyUser(Protocol::IdentifyUser&id);
		void	FactionOnline(NET(TUniqueID)&);
		void	FactionOffline(NET(TUniqueID)&);
		void	FactionRequestedNewRound(NET(TUniqueID)&);
		void	LockEmpZones(NET(UINT16)&);
	}


	namespace Channels
	{
		extern TCP::Channel<Protocol::ControlMessage,0,Protocol::ChannelID::ControlMessage>				controlMessage;

		extern TCP::Channel<Protocol::Result,0,Protocol::ChannelID::SetClientState>						clientStateUpdateResponse;
		extern TCP::Channel<Protocol::WorldDelta,0,Protocol::ChannelID::WorldDelta>						worldData;
		extern TCP::Channel<Protocol::NewMobileEntity,0,Protocol::ChannelID::NewMobileEntity>			newMobileEntity;
		extern TCP::Channel<Protocol::MobileState,0,Protocol::ChannelID::MobileState>					mobileStateUpdate;
		extern TCP::Channel<Protocol::StructureConstruction,0,Protocol::ChannelID::InstantiateStructure>newStructure;
		extern TCP::Channel<Protocol::StructureState,0,Protocol::ChannelID::UpdateStructure>			structureStateUpdate;
		extern TCP::Channel<Protocol::DestroyEntity,0,Protocol::ChannelID::DestroyEntity>				destroyEntity;
		extern TCP::Channel<Protocol::ClientFactionResponse,0,Protocol::ChannelID::ClientFaction>		clientFactionResponse;
		extern TCP::Channel<NET(TUniqueID),0,Protocol::ChannelID::SetFactionID>							setFactionID;

		extern TCP::Channel<Protocol::PhaseChange,0,Protocol::ChannelID::PhaseChange>					phaseChange;
		extern TCP::Channel<Protocol::MobileEntityMotion,0,Protocol::ChannelID::MobileEntityMotion>		mobileEntityMotion;
		extern TCP::Channel<Protocol::AttackTrajectory,0,Protocol::ChannelID::AttackTrajectory>			attackTrajectory;
		extern TCP::Channel<Protocol::Explosion,0,Protocol::ChannelID::Explosion>						explosion;
		extern TCP::Channel<Protocol::EntityTurretRotation,0,Protocol::ChannelID::EntityTurretRotation>	entityTurretRotation;
		extern TCP::Channel<Protocol::ResourceUpdate,0,Protocol::ChannelID::ResourceUpdate>				resourceUpdate;
		extern TCP::Channel<Protocol::ChangeEntityOwner,0,Protocol::ChannelID::ChangeEntityOwner>		changeEntityOwner;
		extern TCP::Channel<Protocol::GameProgress,0,Protocol::ChannelID::GameProgress>					gameProgress;

		extern TCP::Channel<Protocol::Marker, 0, Protocol::ChannelID::CreateMarker>						createMarker;
		extern TCP::Channel<Protocol::Marker, 0, Protocol::ChannelID::MarkerCreationFailedLimitReached>	markerCreationFailedLimitReached;
		extern TCP::Channel<NET(TUniqueID), 0, Protocol::ChannelID::DestroyMarker>						destroyMarker;

		extern TCP::Channel<Protocol::IdentifyUser, 0, Protocol::ChannelID::IdentifyUser>				identifyUser;
		extern TCP::Channel<NET(TUniqueID), 0, Protocol::ChannelID::FactionOnline>						factionOnline;
		extern TCP::Channel<NET(TUniqueID), 0, Protocol::ChannelID::FactionOffline>						factionOffline;
		extern TCP::Channel<NET(TUniqueID), 0, Protocol::ChannelID::FactionRequestedNewRound>			factionRequestedNewRound;
		extern TCP::Channel<NET(UINT16), 0, Protocol::ChannelID::LockEMPZones>							lockEmpZones;
		extern TCP::Channel<NET(int32_t), 0, Protocol::ChannelID::UplinkBandwidth>						uplinkBandwidth;


		extern TCP::ObjectSender<Protocol::ClientFactionRequest,Protocol::ChannelID::ClientFaction>		clientFactionRequest;
		extern TCP::ObjectSender<Protocol::TouchedMarker,Protocol::ChannelID::TouchMarker>					touchMarker;
		extern TCP::ObjectSender<Protocol::ClientState,Protocol::ChannelID::SetClientState>					clientStateSender;
		extern TCP::ObjectSender<Protocol::EntityDirectCommand, Protocol::ChannelID::EntityDirectCommand>	entityDirectCommand;
		extern TCP::ObjectSender<Protocol::ChoiceIntersection, Protocol::ChannelID::ChoiceIntersection>		choiceIntersection;
		extern TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::DropLastTask>							dropLastTask;
		extern TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::DropAllTasks>						dropAllTasks;
		extern TCP::ObjectSender<Protocol::StandardJob, Protocol::ChannelID::SignalNewJob>				signalNewJob;
		extern TCP::ObjectSender<Protocol::TerrainMod, Protocol::ChannelID::SignalTerrainMod>			signalTerrainMod;
		extern TCP::ObjectSender<Protocol::ConstructionAbort, Protocol::ChannelID::ConstructionAbort>	constructionAbort;
		extern TCP::ObjectSender<Protocol::ConstructionBegin, Protocol::ChannelID::ConstructionBegin>	constructionBegin;
		extern TCP::ObjectSender<Protocol::EnableStructure, Protocol::ChannelID::EnableStructure>		enableStructure;
		extern TCP::ObjectSender<Protocol::Automate,Protocol::ChannelID::Automate>						automate;
		extern TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::RestoreCoreLink>					restoreCoreLink;
		extern TCP::ObjectSender<NET(TUniqueID), Protocol::ChannelID::RemoveEntity>						removeEntity;
		extern TCP::ObjectSender<Package::NetString<32>, Protocol::ChannelID::ChangeMyName>				changeMyName;
		extern TCP::ObjectSender<Package::NetString<256>,Protocol::ChannelID::ChatMessage>				chatMessage;

		
	}


	extern TCP::Client		client;
	extern SOCKET			broadcastListenSocket;

	extern String			chosenNickname;
	extern Timer::Time		lastPong;

	extern INT32			uplinkBandwidth;


	void		OnEvent(TCP::event_t ev, TCP::Peer&);

	void		OnSignal(UINT32 signal, TCP::Peer&origin);

	void		Setup();


	class Label
	{
	public:
		String					text;
		float					visualLength;
		float4					color = Vector4<>::one;
		

		void					swap(Label&other)
		{
			text.swap(other.text);
			swp(color,other.color);
		}

	};


	class ServerInfo
	{
	public:
		TUniqueID				id;
		bool					rightVersion = false;
		Protocol::ServerInfo	info;
		
		Buffer0<std::pair<String, float> >			urls;
		
		float					top, bottom;
		bool					selected, haveToken;

		Label					headLine;	//game/server ids, optionally (continue)


		/**/					ServerInfo() :top(0), bottom(0), selected(false), haveToken(false)	{}

		bool					HasURL(const String&url) const
		{
			foreach(urls, u)
				if (u->first == url)
					return true;
			return false;
		}

		void					swap(ServerInfo&other)
		{
			swp(id,other.id);
			swp(info,other.info);
			swp(top,other.top);
			swp(bottom,other.bottom);
			swp(selected,other.selected);
			urls.swap(other.urls);
			swp(haveToken,other.haveToken);
			swp(rightVersion,other.rightVersion);

			headLine.swap(other.headLine);
		}
	};

	extern Buffer0<ServerInfo,Swap>	servers;
	extern GenericHashTable<TUniqueID,index_t,TUniqueID>	serverMap;

	extern Timer::Time		lastPing,lastServerTableUpdate;

	void		Update();

	void		Shutdown();


	bool		ControlEnabled( );


	bool		ConnectToConfiguredServerIfAvailable();


}



#endif

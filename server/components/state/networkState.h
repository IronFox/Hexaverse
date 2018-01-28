#ifndef networkStateH
#define networkStateH

#include <container/hashtable.h>
#include <uniqueID.h>
#include <protocol.h>
#include <objects/types.h>


/**
Encloses all methods and variables describing the current, direct network state of the server
*/
namespace Network
{
	extern TUniqueID		serverID;
	extern TCP::Server		server;


	namespace Config
	{
		extern UINT16	sessionPort;
	}


	enum class UserLevel : UINT32
	{
		PreHello,
		PreFaction,
		CommonFaction,
		Spectator,
		PlanetConfigurator
	};


	class Client;
	typedef std::shared_ptr<Client>	PClient;

	class Client : public TCP::Peer::Attachment
	{
	public:
		Protocol::ClientName		name;
		std::weak_ptr<TCP::Peer>	peer;
		//Timer::Time					lastPing;
		Objects::PFaction	faction;
		bool				wantNewRound=false;


		/**
		Indicates that this client sees everything, but has no faction to control
		*/
		bool				isSpectator=false;

		

		String				ConvertToString() const;
		bool				CanSee(const Objects::PWorldEntity&e, float t)	const;

		/**
		Refreshes personal game progress information
		*/
		void				SendProgressUpdate(const String&warningReason, const Objects::Alliance::TNexusStats&stats);

	};
	

	extern Ctr::Buffer<PClient>	activePlayersAndSpectators,spectators;
	/**
	Direct mapping from factions to online players. If a lookup fails, the respective player is not online. Only one entry per faction is allowed
	*/
	extern Ctr::GenericHashTable<Objects::PFaction,PClient,Objects::OwnerHash>		playerMap;
	extern Ctr::GenericHashTable<CryptographicID,Objects::PFaction>		tokenMap;



	/**
	Processes connection loss of the specified client (or disconnects them).
	Parameter must be a copy.
	*/
	void		HandleConnectionLoss(PClient);

	bool		SessionIsStarted();
	/**
	Disconnects all clients, flushes the scene, and disables the TCP server systems.
	Clients will not be able to connect unless RestartSession is called.
	@return true, if the session was online and subsequently closed, false if it was not started
	*/
	bool		CloseSession(bool preserveSpectators=false, bool sendGridChangesToPreservedSpectators=true);

	/**
	Disconnects all clients and disables the TCP server systems. Does NOT flush the scene.
	Clients will not be able to connect unless RestartSession is called.
	@return true, if the server was running and subsequently closed, false if it was not started
	*/
	bool		SuspendSession();


	/**
	Restarts a previously closed session (via CloseSession() ). A new world should be generated or restored from file
	prior to calling this function
	@param logStateAfterRestart If set true, calls LogServerState() after the server has come back online
	*/
	void		RestartSession(bool logStateAfterRestart);

	/**
	Signals that the specified peer has issued an invalid operation (possibly a hacking attempt)
	and should be disconnected (and possibly banned)
	*/
	void		InvalidOperation(TCP::Peer&peer);


	/**
	Configures the local server. Does nothing if previously configured
	*/
	void		SetupServer();

	/**
	Checks if the specified faction is currently controlled by a connected client
	*/
	bool		FactionIsActive(const Objects::PFaction&);
	/**
	Checks if at least one faction is controlled by a human player. The nexus faction does not count, and neither do spectators
	*/
	bool		AnyHumanPlayersActive();

	/**
	Configures the local server and starts it. Also starts information provider on the given base port
	*/
	void		Start(UINT16 basePort);

	/**
	Creates a UDP socket that receives regular messages.
	If messages are not received in 10 seconds, the server shuts down automatically.
	Call only if the heartbeat system should be active. There is currently no setting to deactive it
	*/
	void		StartHeartbeat();

	/**
	Resolves network events
	*/
	void		Tick();

	/**
	Stops the local server and information provider
	*/
	void		Stop();

	/**
	Updates and logs the new user level (if it changed)
	*/
	void		ChangeUserLevel(TCP::Peer&peer, UserLevel newLevel);

	/**
	Closes the current session (if active), rebuilds the local world, and restarts the session (if it was active).
	If in configuration mode, the config mode random reference is used, otherwise a global one.
	Spectator clients are notified of changes if so.
	*/
	void		BuildConfiguredWorld();

	/**
	Prints the current server configuration and state to the console/log file.
	*/
	void		LogServerState();


	Protocol::IdentifyFaction	Identify(const Objects::PFaction&f);


	bool CanControl(const Client&c, const Objects::WorldEntity&e);
	bool CanControl(const PClient&c, const Objects::PWorldEntity&e);
	bool CanControl(const PClient&c, const Objects::WorldEntity&e);

	bool PeerCanControl(TCP::Peer&peer, const Objects::WorldEntity&e);
	bool PeerCanControl(TCP::Peer&peer, const Objects::PWorldEntity&e);


	bool CanSeeAsOwner(const Client&c, const Objects::WorldEntity&e);
	bool CanSeeAsOwner(const PClient&c, const Objects::PWorldEntity&e);
	bool CanSeeAsOwner(const PClient&c, const Objects::WorldEntity&e);

	bool CanSee(const Client&c, const Objects::PWorldEntity&e, float t);
	bool CanSee(const PClient&c, const Objects::PWorldEntity&e, float t);


	template <index_t Channel, class Item>
		inline void	SendToOwner(TCP::ObjectSender<Item,Channel>&channel,const Objects::WorldEntity&e,const Item&item)
		{
			if (e.owner->alliance->config.exposeEntityDetails)
			{
				e.owner->alliance->ForeachMember([&channel,&e,&item](const Objects::PFaction&f)
				{
					PClient*cl = playerMap.QueryPointer(f);
					if (cl)
						channel.SendTo((*cl)->peer.lock(),item);
				});
			}
			else
			{
				PClient*cl = playerMap.QueryPointer(e.owner);
				if (cl)
					channel.SendTo((*cl)->peer.lock(),item);
			}
			foreach (spectators,s)
				channel.SendTo((*s)->peer.lock(),item);
		}

	template <index_t Channel, class Item>
		inline void	SendToOwner(TCP::ObjectSender<Item,Channel>&channel,const Objects::WorldEntity&e,TCP::Serial<Item>&item)
		{
			if (e.owner->alliance->config.exposeEntityDetails)
			{
				e.owner->alliance->ForeachMember([&channel,&e,&item](const Objects::PFaction&f)
				{
					PClient*cl = playerMap.QueryPointer(f);
					if (cl)
						channel.SendTo((*cl)->peer.lock(),item);
				});
			}
			else
			{
				PClient*cl = playerMap.QueryPointer(e.owner);
				if (cl)
					channel.SendTo((*cl)->peer.lock(),item);
			}
			foreach (spectators,s)
				channel.SendTo((*s)->peer.lock(),item);
		}

	

	namespace InformationProvider
	{
		bool	IsOnline();
		void	Start(UINT16 basePort);
		void	Stop();
	}


}




	


#endif

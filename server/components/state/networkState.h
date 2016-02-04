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
		Spectator
	};


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
	

	extern Buffer<PClient>	activePlayers,spectators;
	/**
	Direct mapping from factions to online players. If a lookup fails, the respective player is not online. Only one entry per faction is allowed
	*/
	extern GenericHashTable<Objects::OwnerParameter,PClient,Objects::OwnerHash>		playerMap;
	extern GenericHashTable<CryptographicID,Objects::PFaction,CryptographicID>		tokenMap;



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
	bool		CloseSession();

	/**
	Disconnects all clients and disables the TCP server systems. Does NOT flush the scene.
	Clients will not be able to connect unless RestartSession is called.
	@return true, if the server was running and subsequently closed, false if it was not started
	*/
	bool		SuspendSession();


	/**
	Restarts a previously closed session (via CloseSession() ). A new world should be generated or restored from file
	prior to calling this function
	*/
	void		RestartSession();

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
	Configures the local server and starts it
	*/
	void		Start();

	void		ChangeUserLevel(TCP::Peer&peer, UserLevel newLevel);



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

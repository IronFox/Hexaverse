#ifndef networkNetH
#define networkNetH


#include <general/enumeration.h>
#include <net/tcp.h>
#include "steamInterface.h"

namespace SteamNetwork
{

	class HybridClient;
	
	CONSTRUCT_ENUMERATION3(AuthenticationState,
		Pending,
		AuthenticationFailed,
		Authenticated
	);
		
	class SteamPeer : public TCP::Peer
	{
		typedef TCP::Peer Super;
		UINT64				steamID;
		int					steamChannel=-1;

		friend class HybridClient;
	protected:
		virtual String		AddressToString(bool includePort) const override;
		virtual bool		AddressIsLocalhost() const
							{
								return steamID == 0 && Super::AddressIsLocalhost();
							}
	public:


		/**/				SteamPeer(TCP::Connection*connection, UINT64 steamID,int channel):Super(connection), steamID(steamID),steamChannel(channel){}
		virtual void		SendPackage(const TCP::Package&object, unsigned minUserLevel=0) override;
		virtual bool		SendSignal(UINT32 channel) override;
		virtual bool		IsConnected() const override {return steamID != 0 || Super::IsConnected();}
		UINT64				GetSteamID() const {return steamID;}
		virtual void		DisconnectPeer() override;
	};
	typedef std::shared_ptr<SteamPeer> PSteamPeer;
	

	class SteamEventHandler
	{
	protected:
		virtual void		HandleSteamPackages()=0;
		friend void			ResolveGlobalEvents();
	};


	/**
	@brief Hyrid Steam/TCP server
	*/
	class Server : public TCP::Server, public SteamEventHandler
	{
		typedef TCP::Server Super;

		Vector<PSteamPeer>	steamPeers;
		HashTable<UINT64,PSteamPeer> steamMap;
		int	steamChannel=-1;
		bool				ignoreDisconnection = false;


		Super::ExtStartService;
		Super::StartService;

		PSteamPeer			ResolvePeer(UINT64 steamID);
	protected:
		void				OnDisconnect(const TCP::Peer*, TCP::event_t) override;
		virtual void		SendPackage(const TCP::Package&object, unsigned minUserLevel) override;
		virtual void		SendPackage(const TCP::PPeer&exclude, const TCP::Package&object, unsigned minUserLevel) override;
		virtual void		HandleSignal(UINT32 signal, TCP::Peer&sender) override;
		virtual void		HandleObject(TCP::RootChannel*receiver, TCP::Peer&sender, const TCP::PDispatchable&object) override;
		virtual void		HandleEvent(TCP::event_t ev, TCP::Peer&sender) override;
		
	public:

		bool				HasAnyClients() const
		{
			return Super::HasAnyClients() 
			||
			steamPeers.IsNotEmpty()
			
			;
		}
		template <typename F>
			void			VisitAllPeerAttachments(const F&f)
							{
								Super::VisitAllPeerAttachments(f);
 
								foreach (steamPeers,p)
									f((*p)->attachment);
							}
		template <typename F>
			void			VisitAllPeers(const F&f)
							{
								Super::VisitAllPeers(f);

								foreach (steamPeers,p)
									f((*p));
							}

		/**
		Starts the local server on the specified channel and port.
		The server will be online on Steam and via TCP. However, if TCP socket binding fails,
		the Steam service will not be started either.

		A lobby will be created at this point, if possible and this is the first server to go online (not necessarily for the firsT time).
		The parameter of UpdateGameState() defines whether it will be published.
		If UpdateGameState() was/is never called, then the lobby will remain private.

		@param steamChannel Channel to communicate on. Use 0 if unsure. -1 is reserved and must not be used
		@param port TCP port to communicate on. May be 0 to dynamically determine a free port.
			The method will fail if the specified port is already in use
		@param rootPort TCP service root port used when advertising the server's TCP addresses.
			Effective only, if this is the first server to go online (not necessarily for the first time)
		@param limitToLocalHost Set true to disallow TCP-connections from non-localhost client addresses
		@param[out] outPort Set to a non-null pointer to fetch the actually used TCP port. Useful only if @a port was set to 0
		*/
		bool				StartService(int steamChannel, USHORT port, USHORT rootPort,bool limitToLocalhost=false, USHORT*outPort=nullptr);



		bool				ExtStartService(int steamChannel, USHORT port, USHORT rootPort,Protocol proto, bool limitToLocalhost=false, USHORT*outPort=nullptr);


		/**
		@brief Terminates the service.
				
		Any currently connected clients are automatically disconnected.
		This operation executes the ConnectionClosed event once for each remaining client and once for itself
		passing NULL as @b peer parameter.
		*/
		void				EndService();
		void				StopService()	{EndService();};	//!< @overload
		void				Shutdown()		{EndService();};		//!< @overload

		TCP::PPeer			GetSharedPointerOfClient(TCP::Peer*);


		void				Resolve();
	protected:
		/**
		Resolves and dispatches incoming steam packages.
		Automatically called by Resolve()
		*/
		virtual void		HandleSteamPackages() override;
	};


	/**
	TCP/Steam hybrid client.
	Can connect to either a Steam or a TCP server
	*/
	class HybridClient : public TCP::Connection, public SteamPeer, public SteamEventHandler
	{
	protected:
		TCP::ConnectionAttempt	attempt;

		enum connectionState_t
		{
			Idle,
			HelloSent,
			HelloAckSent,
			Connected
		};
		Time::Stamp			lastControlMessage;
		connectionState_t	state = Idle;
		count_t				numResents=0;
		friend class ConnectionAttempt;

		void				fail(const String&message);
		void				HandleSteamConnectionFailure(const String&reason);

	public:
					
		/**/				HybridClient():SteamPeer(this,0,-1),attempt(this,this)
							{}
		virtual				~HybridClient()
							{
								attempt.Join();
								DisconnectPeer();
								Join();
							}
		bool				Connect(const String&url);		//!< Attempts to connect to a remote server and waits until a connection was established (or not establishable). Depending on the local connection this may lag for a few seconds. @return true if a connection could be established, false otherwise.
		void				ConnectAsync(const String&url);	//!< Starts the process of connecting to a remote host. Due to the asynchronous nature of this method, No immediate result is available. Hook a function into the local object's inherited @a onEvent callback pointer to receive information about success or failure
		/**
		Asynchronously connects to the specified server id on the given channel.
		Any ongoing connection attempt is aborted, the connection closed
		@param id Server Steam ID to connect to
		@param channel Channel ID to communicate over. -1 is reserved and must not be used
		*/
		void				ConnectToSteam(UINT64 id, int channel=0);

		virtual bool		IsConnected() const override {return (steamID != 0 && state == Connected) || Super::IsConnected();}

		bool				IsAttemptingToConnect()	const	//!< Queries whether or not the client is currently attempting to connect
							{
								return state == HelloSent || state == HelloAckSent || ( attempt.IsRunning() && !attempt.IsDone() );
							}
		void				Disconnect();

		/**
		Resolves synchronous network events. Should be called at a high frequency.
		For a client set to asynchronous this is not necessary.
		*/
		void				Resolve();
		
	protected:
		virtual void		PostResolutionTermination() override {Disconnect();}
		virtual	void		OnDisconnect(const Peer*, TCP::event_t event)	override;
		virtual void		HandleSteamPackages() override;

	};


	/**
	Should be called once per (server/client) frame.
	Resolves global network related steam events.
	Independent of Server/HybridClient::Resolve()
	*/
	void					ResolveGlobalEvents();

	/**
	Updates the lobby variables of any locally running server.
	Should be called whenever the local server game state has changed.
	@param info Server game state to update to
	@param createLobbyIfNecessary Set true to force create a lobby if none was found
	*/
	void					UpdateGameState(const Protocol::GameInfo&info, bool createLobbyIfNecessary);
	
	/**
	Retrieves the authentication state for a given server steam ID.
	If the specified server is unknown, a new authentication request is dispatched to that server.
	In case of error or no response, the authentication state will eventually switch to AuthenticationFailed.
	Requests with an unexpeted user ID will yield AuthenticationFailed but not overwrite the stored status.
	Authentication requests will be dispatched only once per server
	*/
	AuthenticationState		GetAuthenticationState(UINT64 serverID, UINT64 expectUserID);




	void					AdvertiseServer(UINT64 serverID);

	void					HandleSessionClosure();
	/**
	Leaves the current lobby (if any), and creates (and joins) a new one.
	*/
	bool					CreateLobby();

	/**
	Invalidates any stored data, and leaves the local lobby (if any)
	*/
	void					LeaveLobby();
}




#endif

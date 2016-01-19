#ifndef netConsensusH
#define netConsensusH
#ifdef HAS_WINDOW


#include <global_string.h>

/**
Establishes and maintains a connection to the Consensus network.
The Consensus network is responsible for account and cluster management.
*/
namespace ConsensusLink
{

	void		Configure(const String&host, bool connectOnCreate, bool askToEncryptPrivateKey);


	void		Create();

	void		ConnectNow();
	void		Disconnect();

	bool		IsCreated();
	
	/**
	Sends queued network events to the respective handler functions,
	and tries to reconnect if no connection is established and the wait timer has reached zero.
	Should be called every frame.
	*/
	void		Resolve();
	
	/**
	Checks connection state
	@return true, if a connection has been established
	*/
	bool		IsConnected();

	/**
	Checks general connection state
	@return true, if currently connected or continuously attempting connection
	*/
	bool		IsConnectingOrConnected();
	/**
	Checks intermediate connection state
	@return true, if attempts are to be made at all, no connection has so far been established, and the respective server has not rejected the connection due to protocol version mismatches.
	*/
	bool		StillTryingToConnect();

	/**
	Disconnects from the consensus network (if any connection exists), and destroys all dynamically allocated resources
	*/
	void		Destroy();
}



#endif
#endif
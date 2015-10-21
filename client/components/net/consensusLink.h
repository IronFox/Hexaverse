#ifndef netConsensusH
#define netConsensusH


/**
Establishes and maintains a connection to the Consensus network.
The Consensus is necessary for account and cluster management.
*/
namespace ConsensusLink
{
	void		Create();
	
	void		Resolve();
	
	bool		IsConnected();

	void		Destroy();
}



#endif

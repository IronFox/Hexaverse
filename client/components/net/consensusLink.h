#ifndef netConsensusH
#define netConsensusH

#include <global_string.h>

/**
Establishes and maintains a connection to the Consensus network.
The Consensus is necessary for account and cluster management.
*/
namespace ConsensusLink
{
	void		Create(const String&host);
	
	void		Resolve();
	
	bool		IsConnected();

	void		Destroy();
}



#endif

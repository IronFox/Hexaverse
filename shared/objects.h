#ifndef objectsH
#define objectsH

#include <container/buffer.h>
#include <geometry/cgs.h>
#include <io/cryptographic_hash.h>
#include "grid/stack.h"
#include <image/converter/magic.h>

#include "uniqueID.h"
#include "stringTable.h"
#include "cast.h"


//parts:
#include "objects/interface.h"
#include "objects/structure.h"
#include "objects/vehicle.h"
#include "objects/satellite.h"
#include "objects/faction.h"
#include "objects/prototypes.h"
#include "objects/gridAndChoices.h"
//#include "objects/interface.h"



#undef LoadIcon




#ifdef CLIENT
/**
Used to disable commands that can otherwise never be disabled. Implemented in clientMain.cpp
*/
bool CanEnableAtAll();
#endif






namespace Objects
{


	/**
	Prevents links between entities, factions, and alliances from being severed upon object flush.
	Under ideal circumstances, the end result is identical, however entities, factions, and alliances may have been referenced elsewhere thus preventing their final deletion.
	The severing of links typically forces the deletion of at least factions and alliances
	*/
	extern bool				preserveReferencesOnFlush /*= false*/;





	/**
	Initial construction function.
	Creates/loads everything involving objects, their interaction, visualization, etc...
	*/
	void					Create();










	#ifndef CLIENT
		//void					Tick();
		///**
		//Processes resource generation and distribution.
		//Supposed to be called at the beginning of round processing (before attack or movement phase)
		//If not in connector-less mode, everything is done here.
		//*/
		//void					ProcessStructures0();
		///**
		//Distributes remaining overproduction to storage units.
		//Effective only if compiled in connector-less mode.
		//Supposed to be called at the end of the round processing (after movement phase, before animation phase)
		//*/
		//void					ProcessStructures1(float atTime);

	#endif




	#ifndef NO_SATELLITES
		/**
		Updates SkyHub connections to SkyPorts,	as well as dependent satellites.
		Not guaranteed to be accurate for non-player factions on client.
		Should be identical for player faction on client, since the client should know all of its own entities.
		*/
		void			RefreshSkyHubConnections();
	#endif



};


#endif

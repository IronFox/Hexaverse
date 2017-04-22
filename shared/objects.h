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
#include "objects/subOrbital.h"
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





	#ifdef HAS_WINDOW

		namespace DX11 = Engine::DX11;
	

		//extern bool					showArrow, hasArrowCellTarget;
		//extern float3				arrowFrom;	//arroa visualization point of origin. Might be the location of a vehicle, or any of the locations in its task queue. Only relevant if showArrow is true
		//extern TrajectoryEndPoint	arrowCellTarget;

		extern DX11::BaseInstructionBlock	stdVehicleShader;

	#endif




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

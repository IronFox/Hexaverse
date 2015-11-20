#ifndef objectsH
#define objectsH

#include <container/buffer.h>
#include <geometry/cgs.h>
#include <io/encryption.h>
#include "sphereGrid.h"
#include <image/converter/magic.h>

#include "uniqueID.h"
#include "stringTable.h"
#include "cast.h"


//parts:
#include "objects/interface.h"
#include "objects/rendering.h"
#include "objects/structure.h"
#include "objects/vehicle.h"
#include "objects/satellite.h"
#include "objects/faction.h"
#include "objects/prototypes.h"
#include "objects/gridAndChoices.h"
#include "objects/visualSelection.h"
#include "objects/visualChoices.h"
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
	

		extern bool					showArrow, hasArrowCellTarget;
		extern float3				arrowFrom;	//arroa visualization point of origin. Might be the location of a vehicle, or any of the locations in its task queue. Only relevant if showArrow is true
		extern Grid::LCellIdI		arrowCellTarget;

		extern DX11::BaseInstructionBlock	stdVehicleShader;

	#endif




	/**
	Initial construction function.
	Creates/loads everything involving objects, their interaction, visualization, etc...
	*/
	void					Create();

	/**
	Final destruction command. Tries to free as much as possible before returning.
	Although theoretically not necessary, it also frees all DirectX resources.
	*/
	void					Destroy();







	#ifdef HAS_WINDOW
		PWorldEntity	GetRayIntersection(const TVec3<>&b, const TVec3<>&d, float&distance);
		bool			Select(const TVec3<>&b, const TVec3<>&d, float&distance, bool controlEnabled);
	#endif


	#ifndef CLIENT
		//void					Tick();
		/**
		Processes resource generation and distribution.
		Supposed to be called at the beginning of round processing (before attack or movement phase)
		If not in connector-less mode, everything is done here.
		*/
		void					ProcessStructures0();
		/**
		Distributes remaining overproduction to storage units.
		Effective only if compiled in connector-less mode.
		Supposed to be called at the end of the round processing (after movement phase, before animation phase)
		*/
		void					ProcessStructures1(float atTime);
		bool					ProcessNextVehicleAttack();
		bool					ProcessVehicleMotion();
		void					ProcessEMPState(float atTime, const std::function<bool(const PWorldEntity&)>&doHeal);

	#endif





	/**
	Erases all objects and references to objects. Prototypes are preserved.
	Should be called whenever the scene is reset to prevent any lingering artifacts from the previous session.
	*/
	void				FlushAllData();

	/**
	Updates SkyHub connections to SkyPorts,	as well as dependent satellites.
	Not guaranteed to be accurate for non-player factions on client.
	Should be identical for player faction on client, since the client should know all of its own entities.
	*/
	void				RefreshSkyHubConnections();

	#ifdef HAS_WINDOW
		/**
		Resets the cursor mode to idle.
		Updates choices and commands if necessary.
		@param controlEnabled Pass true if player controls are currently enabled (not disabled due to wrong phase, or open modal dialogs)
		@return true, if any modes were disabled
		*/
		bool			DisableModes(bool controlEnabled);
	#endif


	#ifndef CLIENT
		/**
		Like vehicles, towers have an attack action floation point value. Unlike vehicles, they are not part of the
		movement phase. Therefore, once per round, this function must be called to reset their attack action.
		*/
		void			ResetTowerAttackAction(float atTime);
		/**
		Updates what satellites can see on the ground (if terrainChanged is set true), and their fuel supply.
		Unsupplied satellites may be destroyed.
		Must be called once per round.
		@param t Server time
		@param terrainChanged Set true if the terrain changed sind the last invocation of this function. If in doubt pass true. 
		*/
		void			CheckSatelliteSupplyAndSensory(float&t, bool terrainChanged);

	#endif



};


#endif

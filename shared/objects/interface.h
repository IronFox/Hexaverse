#ifndef objectInterfaceH
#define objectInterfaceH

#include "types.h"
#include "cursorMode.h"
#include "../grid/stack.h"
#include "../uniqueID.h"
#include "../fileVersion.h"


#ifdef HAS_WINDOW
	#include <engine/display.h>
	#include "../../client/ExtRenderer/DirectX11.h"
	extern Engine::Display<Engine::DirectX11>	display;

#endif

/**
Functions that send events to the non-shared components of the application.
The client application must implement these somewhere
*/
namespace ApplicationInterface
{
#ifdef CLIENT
	
	bool	DirectTo(const Objects::PWorldEntity&v, const ArrayRef<Grid::LCellIdI>&targets, bool detailedPath, bool leftClick);
	void	SignalChoiceIntersection(const Objects::PWorldEntity&e,const Grid::LCellIdI&c, Objects::CursorMode mode, const Objects::PMobileEntity&deployingWhat);
	void	SignalRoundEnd();
	void	SignalUndoRoundEnd();
	bool	HasSignalledRoundEnd();
	bool	ShouldSignalRoundEnd();
	bool	CanChangeRoundState();
	void	SignalNewJob(const Objects::PMobileEntity&v, ConstName jobName, Objects::EntityType parameterName);
	void	SignalTerrainModification(const Objects::PVehicle&v,Objects::CursorMode activeCursorMode,const Array<Grid::HCellIdI>&selection);
	void	SignalDropLastTask(const Objects::PWorldEntity&v);
	void	SignalDropAllTasks(const Objects::PWorldEntity&v);
	void	SignalDropSafeTasks(const Objects::PWorldEntity&v);
	void	SignalRestoreCoreLink(const TUniqueID&mineId);
	void	SignalConstructionAbort(const TUniqueID&plantId,const TUniqueID&taskId);
	void	SignalConstructionBegin(const TUniqueID&plantId,Objects::EntityType);
	void	SignalRemoval(const Objects::PWorldEntity&);
	void	SignalConstructTowerTurret(const TUniqueID&towerID,const Objects::Component::Turret*&newTurret);
	void	SignalAutomation(const Objects::PWorldEntity&e, bool doAutomate);
	void	SignalSleep(const Objects::PWorldEntity&v, bool doSleep);
	void	SignalEnableStructure(Objects::Structure&s, bool enabled);
	void	SignalTargetLessAttack(const Objects::PWorldEntity&e);
	
	#ifdef HAS_WINDOW
		void	SignalUserMessage(const String&message);
	#endif
	void	OnVanish(const Objects::PWorldEntity&e, Objects::RemovalType t, bool wasGhost);
	void	OnAppear(const Objects::PWorldEntity&e);
#else

	

	struct TAttack
	{
		Objects::PWorldEntity	attackFrom;
		float3	traceTo;
		float	flightTime=0,
				attackAngle=0;
		bool	isAutoAttack=false;
	};

	/*
	These functions signal certain events to the client application (the server in this case)
	*/

	void	SignalNewMobileEntity(const Objects::PMobileEntity&v, bool restoredFromStorage, float atTime);
	void	SignalRemoval(const Objects::PWorldEntity&e, Objects::RemovalType t, float atTime);
	void	SignalNewStructure(const Objects::PStructure&s, float atTime);
	void	SignalMotion(const Objects::PMobileEntity&v, const Grid::LCellIdI&from,const Grid::LCellIdI&to, float t0, float timeNeeded);
	void	SignalTurretRotation(const Objects::PWorldEntity&v, const M::TVec3<>&toDirection, float rotationTime);
	void	SignalAttack(const TAttack&);
	void	SignalExplosion(const TAttack&, const M::TVec3<>&center,float explosionRadius,bool isEMP, bool isDoomsday,float atTime);
	//void	SignalExplosion(const M::TVec3<>&center,float explosionRadius,bool isEMP, float atTime);
	void	SignalDisappearanceToHostileFaction(const Objects::OwnerParameter&target, const Objects::PWorldEntity&e, float atTime);
	void	SignalAppearanceToHostileFaction(const Objects::OwnerParameter&target, const Objects::PWorldEntity&e, float atTime);
	void	SignalOwnershipChange(const Objects::PWorldEntity&e,const Objects::OwnerParameter&target, float t);
	void	SignalNexusControlChange(const Objects::PFaction&f);
	void	SignalChainReactionExplosion(const Objects::PWorldEntity&causedBy, const Grid::LCellIdI&center, float radius, int damageLevel, float atTime);
	void	SignalOperationLacksResources(const Objects::WorldEntity&, const Ctr::ArrayRef<Objects::Resource>&resources, float atTime);
	/**
	Signals that a new faction has been created or an existing one has been removed.
	Also replacement as happens when the nexus faction is recreated.
	*/
	void	SignalFactionsChanged();
	/**
	Signals that an alliance state change has been detected from 
	*/
	void	SignalFactionAllianceChange(const Objects::OwnerParameter&origin,const Objects::OwnerParameter&dest, Objects::AllianceLevel newLevel);


#endif
}

#endif

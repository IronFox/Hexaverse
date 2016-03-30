#ifndef objectInterfaceH
#define objectInterfaceH

#include "types.h"
#include "cursorMode.h"


#ifdef CLIENT
	
	#ifdef HAS_WINDOW
		#include <engine/display.h>
		#include "../../client/ExtRenderer/DirectX11.h"
		extern Engine::Display<Engine::DirectX11>	display;

	#endif
	bool	DirectTo(const Objects::PWorldEntity&v, const ArrayRef<Grid::LCellIdI>&targets, bool detailedPath, bool leftClick);
	void	SignalChoiceIntersection(const Objects::PWorldEntity&e,const Grid::LCellIdI&c, Objects::CursorMode mode, const Objects::PMobileEntity&deployingWhat);
	void	SignalRoundEnd();
	void	SignalUndoRoundEnd();
	bool	HasSignalledRoundEnd();
	bool	ShouldSignalRoundEnd();
	bool	CanChangeRoundState();
	void	SignalNewJob(const Objects::PMobileEntity&v, ConstName jobName, ConstName parameterName);
	void	SignalTerrainModification(const Objects::PVehicle&v,Objects::CursorMode activeCursorMode,const Array<Grid::HCellIdI>&selection);
	void	SignalDropLastTask(const Objects::PWorldEntity&v);
	void	SignalDropAllTasks(const Objects::PWorldEntity&v);
	void	SignalRestoreCoreLink(const TUniqueID&mineId);
	void	SignalConstructionAbort(const TUniqueID&plantId,const TUniqueID&taskId);
	void	SignalConstructionBegin(const TUniqueID&plantId,ConstName baseName, ConstName turretName);
	void	SignalRemoval(const Objects::PWorldEntity&);
	void	SignalConstructTowerTurret(const TUniqueID&towerID,const Objects::Component::Turret*&newTurret);
	void	SignalAutomation(const Objects::PWorldEntity&e, bool doAutomate);
	void	SignalSleep(const Objects::PWorldEntity&v, bool doSleep);
	void	SignalEnableStructure(Objects::Structure&s, bool enabled);
	void	SignalTargetLessAttack(const Objects::PWorldEntity&e);

	#ifdef HAS_WINDOW
		void	OnSelectionChange(bool stateChangeOnly);
		void	SignalUserMessage(const String&message);
	#endif
	void	OnVanish(const Objects::PWorldEntity&e, Objects::RemovalType t);
	void	OnAppear(const Objects::PWorldEntity&e);
#else

	extern UINT32			globalVersionOfCurrentFile;

	static const UINT32	SupportedFileVersion = 1045;	//doubles as current file version to write to files

	struct FileVersions
	{
		enum version_t : UINT32
		{
			HasUUIDInConstructionJobs = 1041,
			FirstVersionWithVariables = 1042,
			MarkersHaveOwner = 1043,
			CurrentEntityHealthIsFloat = 1044,
			EntitiesHaveFlags = 1045,
		};
	};



	void	SignalNewMobileEntity(const Objects::PMobileEntity&v, bool restoredFromStorage, float atTime);
	void	SignalRemoval(const Objects::PWorldEntity&e, Objects::RemovalType t, float atTime);
	void	SignalNewStructure(const Objects::PStructure&s, float atTime);
	void	SignalMotion(const Objects::PMobileEntity&v, const Grid::LCellIdI&from,const Grid::LCellIdI&to, float t0, float timeNeeded);
	void	SignalTurretRotation(const Objects::PWorldEntity&v, const TVec3<>&toDirection, float rotationTime);
	void	SignalAttack(const Objects::PWorldEntity&v, const Grid::LCellIdI&traceTo, float flightTime, bool isAutoAttack);
	void	SignalExplosion(const Objects::PWorldEntity&attackFrom, const Grid::LCellIdI&traceTo, float flightTime, const TVec3<>&center,float explosionRadius,bool isEMP, bool isDoomsday,float atTime);
	//void	SignalExplosion(const TVec3<>&center,float explosionRadius,bool isEMP, float atTime);
	void	SignalDisappearanceToHostileFaction(const Objects::OwnerParameter&target, const Objects::PWorldEntity&e, float atTime);
	void	SignalAppearanceToHostileFaction(const Objects::OwnerParameter&target, const Objects::PWorldEntity&e, float atTime);
	void	SignalOwnershipChange(const Objects::PWorldEntity&e,const Objects::OwnerParameter&target, float t);
	void	SignalNexusControlChange(const Objects::PFaction&f);
	void	SignalChainReactionExplosion(const Grid::LCellIdI&center, float radius, int damageLevel, float atTime);

#endif


#endif

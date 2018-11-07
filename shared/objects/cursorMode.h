#ifndef objectCursorModeH
#define objectCursorModeH
#include "types.h"

namespace Objects
{
	enum class CursorMode	//progressive ONLY (add, do not replace, do not remove elements). Used for IO
	{
		Idle,
		DigMode,
		ClearAboveMode,
		BuildBlockMode,
		RemoveBlockMode,
		RestockMode,
		DepositMode,
		RepairMode,
		AttackMode,
		BuildConnectorChainMode,
		BuildRoadChainMode,
		BuildHighWallChainMode,
		DigTunnelMode,
		BuildBridgeMode,
		PlaceStructureMode,
		BuildLowWallChainMode,
	};

	#ifdef HAS_WINDOW
		extern CursorMode	activeCursorMode;
		extern PStructure	activeStructureToPlace;
	#endif

}




#endif

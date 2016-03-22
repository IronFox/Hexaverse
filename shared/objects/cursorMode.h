#ifndef objectCursorModeH
#define objectCursorModeH
#include "types.h"
#include <global_string.h>

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
		BuildWallChainMode,
		DigTunnelMode,
		BuildBridgeMode,
		PlaceStructureMode,
		PlaceTechniteMode,
	};

	#ifdef HAS_WINDOW
		extern CursorMode	activeCursorMode;
		extern PStructure	activeStructureToPlace;
		extern String		activeTechniteTypeToPlace;
	#endif

}




#endif

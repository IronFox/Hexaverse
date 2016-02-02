#ifndef objectCursorModeH
#define objectCursorModeH

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
	};

	#ifdef HAS_WINDOW
		extern CursorMode	activeCursorMode;
	#endif

}




#endif

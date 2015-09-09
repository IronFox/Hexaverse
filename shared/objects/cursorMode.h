#ifndef objectCursorModeH
#define objectCursorModeH

namespace Objects
{
	namespace CursorMode
	{
		enum mode_t	//progressive ONLY (add, do not replace, do not remove elements). Used for IO
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
	}

	#ifdef HAS_WINDOW
		extern CursorMode::mode_t	activeCursorMode;
	#endif

}




#endif

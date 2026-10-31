#pragma once

#define MANAGER_UNICAST_PORT		10245	//must be constant, not reused by any heartbeat port
#define BROADCAST_PORT				10246	//must be constant, not reused by any heartbeat port

#define DEFAULT_SERVER_BASE_PORT	10247
#define TUTORIAL_BASE_PORTS_FROM	50000




namespace PortOffset
{
	/**
	@brief Service port delta to root port
	*/
	enum delta_t
	{
		Info,
		Session,
		Control,
		Steam,
		SteamGame,
		SteamInfo,

		Heartbeat = Info	//heartbeat is UDP. Can coexist with others
	};
}
#pragma once

#include <lua/luaGameAPI.h>

namespace AttackLog
{
	/**
	Records attacks the client learns about while receiving round results (the same
	information players see as shot trails), so Lua AIs can react to attacks whose
	origin is not otherwise sensed (e.g. invisible bombers or artillery).
	Own attacks are not recorded. Thread-safe.
	*/
	void	RecordVague(UINT32 round, const M::TVec3<>&sourcePosition, ConstName turretType, const M::TVec3<>&impact, float explosionRadius);
	void	RecordPrecise(UINT32 round, const TUniqueID&attacker, const M::TVec3<>&impact);

	/**
	Copies all recorded attacks of the specified round or later. Older entries are discarded.
	*/
	void	Snapshot(UINT32 sinceRound, std::vector<Lua::API::RecentAttack>&out);
}

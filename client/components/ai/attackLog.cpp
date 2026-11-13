#include "attackLog.h"
#include <objects/entity.h>
#include <objects/faction.h>
#include <algorithm>
#include <mutex>

namespace AttackLog
{
	namespace
	{
		std::mutex								mutex;
		std::vector<Lua::API::RecentAttack>		entries;
		const UINT32							KeepRounds = 5;

		void	Prune(UINT32 round)
		{
			if (round <= KeepRounds)
				return;
			const UINT32 minRound = round - KeepRounds;
			entries.erase(std::remove_if(entries.begin(), entries.end(),
				[minRound](const Lua::API::RecentAttack&a){return a.round < minRound;}), entries.end());
		}
	}

	void	RecordVague(UINT32 round, const M::TVec3<>&sourcePosition, ConstName turretType, const M::TVec3<>&impact, float explosionRadius)
	{
		Lua::API::RecentAttack a;
		a.round = round;
		a.precise = false;
		a.isHostile = true;	//vague trajectories are sent for attackers we cannot see. Own units are always visible
		a.hasSource = true;
		a.source = sourcePosition;
		a.impact = impact;
		a.turretType = turretType;
		a.explosionRadius = explosionRadius;
		std::lock_guard<std::mutex> lock(mutex);
		Prune(round);
		entries.push_back(a);
	}

	void	RecordPrecise(UINT32 round, const TUniqueID&attacker, const M::TVec3<>&impact)
	{
		Objects::PWorldEntity e = Objects::GetEntity(attacker);
		if (e && e->isOwnedByMe)
			return;
		Lua::API::RecentAttack a;
		a.round = round;
		a.precise = true;
		a.attacker = attacker;
		a.impact = impact;
		if (e)
		{
			a.isHostile = !e->owner || !e->owner->IsAlliedWithMe();
			a.hasSource = true;
			a.source = e->GetWorldPosition();
			float zOffset;
			Objects::PTurretBase base = e->GetTurretBase(zOffset);
			if (base && base->turret)
			{
				a.turretType = base->turret->constName;
				a.explosionRadius = base->GetAttackExplosionRadius();
			}
		}
		std::lock_guard<std::mutex> lock(mutex);
		Prune(round);
		entries.push_back(a);
	}

	void	Snapshot(UINT32 sinceRound, std::vector<Lua::API::RecentAttack>&out)
	{
		std::lock_guard<std::mutex> lock(mutex);
		out.clear();
		for (const auto&a : entries)
			if (a.round >= sinceRound)
				out.push_back(a);
	}
}

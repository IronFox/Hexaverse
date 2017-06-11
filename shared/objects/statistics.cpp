#include "statistics.h"
#include "entity.h"
#include "../runtimeConfig.h"
#include "faction.h"

namespace Objects
{
	Statistics::Statistics(const Faction*owner):owner(owner)
	{
		history.Append();
	}

	Statistics::Interaction&	Statistics::Get(const PFaction&remote)
	{
		auto rs = interactionMap.QueryPointer(remote.get());
		if (rs)
			return *rs;
		rs = &interactionMap.Set(remote.get());
		rs->remote = remote;
		return *rs;
	}

	Statistics::UnitStatistics&	Statistics::GetTypeOf(const PWorldEntity&e)
	{
		String name = e->GetTypeName();
		auto rs = unitStatistics.QueryPointer(name);
		if (rs)
			return *rs;
		rs = &unitStatistics.Set(name);
		rs->typeName = name;
		return *rs;
	}


	void			Statistics::SignalDamageReceivedFrom(const PFaction&source, int level)
	{
		Get(source).Get(Interaction::DamageReceived) += level;
	}

	void			Statistics::SignalDamageDealtTo(const PWorldEntity&causedBy, const PFaction&receiver, int level)
	{
		Get(receiver).Get(Interaction::DamageDealt)+=level;
		GetTypeOf(causedBy).Get(UnitStatistics::DamageDealt)+=level;
	}

	void			Statistics::SignalEMPDamageReceivedFrom(const PFaction&source, int level)
	{
		Get(source).Get(Interaction::EMPDamageReceived)+=level;
	}

	void			Statistics::SignalEMPDamageDealtTo(const PWorldEntity&causedBy, const PFaction&receiver, int level)
	{
		Get(receiver).Get(Interaction::EMPDamageDealt)+=level;
		GetTypeOf(causedBy).Get(UnitStatistics::EMPDamageDealt)+=level;
	}
		
	void			Statistics::SignalDestructionOfHostile(const PWorldEntity&causedBy, const PWorldEntity&destroyed)
	{
		auto&in = Get(destroyed->owner);
		in.Get(Interaction::UnitsKilled)++;
		in.Get(Interaction::ResourcesDestroyed) += destroyed->GetConstructionCost();
		auto&un = GetTypeOf(causedBy);
		un.Get(UnitStatistics::UnitsKilled)++;
		un.Get(UnitStatistics::ResourcesDestroyed) += destroyed->GetConstructionCost();;
	}
		
	void			Statistics::SignalLossOf(const PWorldEntity&destroyed, const PFaction&causedBy, bool stolen)
	{
		auto&in = Get(causedBy);
		if (stolen)
		{
			in.Get(Interaction::UnitsStolenBy)++;
			in.Get(Interaction::ResourcesStolenBy) += destroyed->GetConstructionCost();
		}
		else
		{
			in.Get(Interaction::UnitsKilledBy)++;
			in.Get(Interaction::ResourcesDestroyedBy) += destroyed->GetConstructionCost();
		}
		auto&un = GetTypeOf(destroyed);
		un.Get(UnitStatistics::Lost) ++;
	}

	void			Statistics::SignalUnitAcquired(const PWorldEntity&e, const PFaction&previousOwner)
	{
		if (previousOwner)
		{
			auto&in = Get(previousOwner);
			in.Get(Interaction::UnitsStolen)++;
			in.Get(Interaction::ResourcesStolen) += e->GetConstructionCost();
		}
		auto&un = GetTypeOf(e);
		if (previousOwner)
			un.Get(UnitStatistics::Stolen) ++;
		else
			un.Get(UnitStatistics::Built) ++;
	}


	void			Statistics::SignalStructureResourceDelta(const PWorldEntity&, const ResourceVector&d)
	{
		ResourceVector pos,neg;
		d.SeparateBySign(pos,neg);
		roundProduction += pos;
		roundConsumption -= neg;
	}

	void Statistics::SignalCompoundProcessingEnd()
	{
		GetRoundStat().production = roundProduction;
		GetRoundStat().consumption = roundConsumption;

		roundConsumption.Clear();
		roundProduction.Clear();
	}

	/*static*/ void Statistics::SignalNewRoundBegin(bool mainRound)
	{
		if (mainRound)
		{
			Objects::ForeachWorldEntityIncludingStored([](const Objects::PWorldEntity&e)
			{
				e->owner->statistics.totalEntities ++;
				e->owner->statistics.totalEntityMineralValue += e->GetConstructionCost();
			},false);

			Objects::ForeachFaction([](const Objects::PFaction&f)
			{
				auto&stat = f->statistics;
				{
					auto&prog = stat.history.Last();
					prog.entityCount = stat.totalEntities;
					prog.entityMineralValue = stat.totalEntityMineralValue;
					f->CheckSensory();
					prog.sensorCoverage = f->knownWorld.CountVisibleCells();
					stat.totalEntities = 0;
					stat.totalEntityMineralValue = 0;
				}
				f->statistics.history.Append().mainRound = RuntimeConfig::roundNumber.main;
			});
		}
	}

	Statistics::RoundStatistics&	Statistics::GetRoundStat()
	{
		return history.Last();
	}




	/*virtual override*/ serial_size_t	Statistics::GetSerialSize(bool exportSize) const
	{
		return 0;
	}

	/*virtual override*/ void			Statistics::Serialize(IWriteStream&outStream, bool exportSize) const
	{

	}

	/*virtual override*/ void			Statistics::Deserialize(IReadStream&inStream, serial_size_t fixedSize)
	{

	}


}

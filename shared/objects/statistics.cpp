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
		rs->remoteID = remote->id;
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



	/*virtual override*/	serial_size_t			Statistics::Interaction::GetSerialSize(bool) const
	{
		serial_size_t rs = remoteID.NumBytes;
		foreach (metric,m)
			rs += GetSerialSizeOfSize(*m);
		return rs;
	}

	/*virtual override*/	void					Statistics::Interaction::Serialize(IWriteStream&outStream, bool) const
	{

		outStream.Write(remoteID.bytes,remoteID.NumBytes);
		foreach (metric,m)
			outStream.WriteSize(*m);
	}

	/*virtual override*/	void					Statistics::Interaction::Deserialize(IReadStream&inStream, serial_size_t)
	{
		inStream.Read(remoteID.bytes,remoteID.NumBytes);
		foreach (metric,m)
			inStream.ReadSize(*m);
	}


	/*virtual override*/	serial_size_t			Statistics::UnitStatistics::GetSerialSize(bool) const
	{
		serial_size_t rs = 0;
		rs += typeName.GetSerialSize(true);
		foreach (metric,m)
			rs += GetSerialSizeOfSize(*m);
		return rs;
	}

	/*virtual override*/	void					Statistics::UnitStatistics::Serialize(IWriteStream&outStream, bool) const
	{
		typeName.Serialize(outStream,true);
		foreach (metric,m)
			outStream.WriteSize(*m);
	}

	/*virtual override*/	void					Statistics::UnitStatistics::Deserialize(IReadStream&inStream, serial_size_t)
	{
		typeName.Deserialize(inStream,EmbeddedSize);
		foreach (metric,m)
			inStream.ReadSize(*m);
	}

	/*virtual override*/	serial_size_t			Statistics::RoundStatistics::GetSerialSize(bool exportSize) const
	{
		return	production.GetSerialSize(true)
			+	consumption.GetSerialSize(true)
			+	ISerializable::GetSerialSizeOfSize(entityCount)
			+	ISerializable::GetSerialSizeOfSize(entityMineralValue)
			+	ISerializable::GetSerialSizeOfSize(sensorCoverage)
			+	ISerializable::GetSerialSizeOfSize(mainRound);

	}

	/*virtual override*/	void					Statistics::RoundStatistics::Serialize(IWriteStream&outStream, bool exportSize) const
	{
		production.Serialize(outStream,true);
		consumption.Serialize(outStream,true);
		outStream.WriteSize(entityCount);
		outStream.WriteSize(entityMineralValue);
		outStream.WriteSize(sensorCoverage);
		outStream.WriteSize(mainRound);
	}

	/*virtual override*/	void					Statistics::RoundStatistics::Deserialize(IReadStream&inStream, serial_size_t fixedSize)
	{
		production.Deserialize(inStream,true);
		consumption.Deserialize(inStream,true);
		inStream.ReadSize(entityCount);
		inStream.ReadSize(entityMineralValue);
		inStream.ReadSize(sensorCoverage);
		inStream.ReadSize(mainRound);
	}




	#ifndef CLIENT
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



		namespace ChunkID
		{
			const Riff::TID
				round("RND "),

				unit("UNIT"),
				interaction("INTR"),



				unusedTerm("ZZZZ");	

		};


		void			Statistics::ExportInto(Riff::Chunk*chunk) const
		{
			{
				
				BYTE worstCaseData[Interaction::NumMetrics *8];

				foreach (this->history,r)
				{
					MemWriteStream stream(worstCaseData,sizeof(worstCaseData));
					r->Serialize(stream,false);
					chunk->AppendBlock(ChunkID::round,worstCaseData,stream.GetCurrent()-worstCaseData);
				}
			}
			{
				ByteStream stream;
				this->unitStatistics.VisitAllValues([chunk,&stream](const UnitStatistics&stat)
				{
					stream.Clear();
					stat.Serialize(stream,false);
					chunk->AppendBlock(ChunkID::unit,stream);
				});
			}
			{
				ByteStream stream;
				this->interactionMap.VisitAllValues([chunk,&stream](const Interaction&stat)
				{
					stream.Clear();
					stat.Serialize(stream,false);
					chunk->AppendBlock(ChunkID::interaction,stream);
				});
			}
		}

		void			Statistics::ImportFrom(const Riff::Chunk*chunk, const GenericHashTable<TUniqueID,Objects::PFaction>&factionMap)
		{
			history.Clear();
			unitStatistics.Clear();
			interactionMap.Clear();

			const Riff::Chunk*child = chunk->GetFirstChild();
			do
			{
				MemReadStream stream(child->pointer(),child->size());
				if (child->IsID(ChunkID::round))
				{
					RoundStatistics stat;
					stat.Deserialize(stream,child->size());
					ASSERT_EQUAL__(stream.GetRemainingBytes(),0);
					history << stat;
				}
				else
				if (child->IsID(ChunkID::unit))
				{
					UnitStatistics stat;
					stat.Deserialize(stream,child->size());
					ASSERT_EQUAL__(stream.GetRemainingBytes(),0);
					unitStatistics.Set(stat.typeName,stat);
				}
				else
				if (child->IsID(ChunkID::interaction))
				{
					Interaction stat;
					stat.Deserialize(stream,child->size());
					ASSERT_EQUAL__(stream.GetRemainingBytes(),0);

					PFaction f;
					factionMap.Query(stat.remoteID,f);
					if (f)
						interactionMap.Set(f.get(),stat);
				}
			}
			while (child = child->GetNextSibling());
		}

	#endif

	Statistics::RoundStatistics&	Statistics::GetRoundStat()
	{
		return history.Last();
	}






}

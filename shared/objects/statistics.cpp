#include "statistics.h"
#include "entity.h"
#include "../runtimeConfig.h"
#include "faction.h"

namespace Objects
{

	UINT64&					Statistics::MetricTable::Fetch(Metric metric)
	{
		auto ptr = map.QueryPointer((K)metric);
		if (ptr)
			return *ptr;
		ptr = map.SetNew((K)metric);
		(*ptr) = 0;
		return *ptr;
	}

	UINT64					Statistics::MetricTable::operator[](Metric metric) const
	{
		UINT64 rs = 0;
		map.Query((K)metric,rs);
		return rs;
	}

	void					Statistics::MetricTable::Inc(Metric m)
	{
		Fetch(m)++;
	}
	void					Statistics::MetricTable::Add(Metric m, int delta)
	{
		if (delta <= 0)
		{
			DBG_ASSERT_EQUAL__(delta,0);
			return;
		}
		Fetch(m) += delta;
	}
	void					Statistics::MetricTable::Set(Metric m, count_t value)
	{
		if (value == 0)
		{
			map.Unset((K)m);
			return;
		}
		map.Set((K)m,value);
	}


	/*virtual override*/	void			Statistics::MetricTable::Serialize(IWriteStream&outStream) const
	{
		outStream.WriteSize(map.Count());
		map.VisitAllEntries([&outStream](K key, UINT64 value)
		{
			outStream.WriteSize(key);
			outStream.WriteSize(value);
		});
	}
	/*virtual override*/	void			Statistics::MetricTable::Deserialize(IReadStream&inStream)
	{
		count_t entries = 0;
		inStream.ReadSize(entries);
		map.Clear();
		for (index_t i = 0; i < entries; i++)
		{
			K key;
			UINT64 val;
			inStream.ReadSize(key);
			inStream.ReadSize(val);
			if (val > 0)
				map.Set(key,val);
		};
	}

	bool					Statistics::MetricTable::operator==(const Self&other) const
	{
		bool equal = true;
		map.VisitAllEntries([&other,&equal](K key, UINT64 value)
		{
			if (!equal)
				return;
			const UINT64*v = other.map.QueryPointer(key);
			if (!v)
			{
				if (value != 0)
					equal = false;
				return;
			}
			if (value != *v)
				equal = false;
		});
		if (equal)
			other.map.VisitAllEntries([this,&equal](K key, UINT64 value)
			{
				if (!equal)
					return;
				const UINT64*v = map.QueryPointer(key);
				if (!v)
				{
					if (value != 0)
						equal = false;
					return;
				}
				if (value != *v)
					equal = false;
			});
		return equal;
	}



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




	/*virtual override*/	void					Statistics::Interaction::Serialize(IWriteStream&outStream) const
	{
		outStream.Write(remoteID.bytes,remoteID.NumBytes);
		metrics.Serialize(outStream);
	}

	/*virtual override*/	void					Statistics::Interaction::Deserialize(IReadStream&inStream)
	{
		inStream.Read(remoteID.bytes,remoteID.NumBytes);
		metrics.Deserialize(inStream);
	}


	/*virtual override*/	void					Statistics::UnitStatistics::Serialize(IWriteStream&outStream) const
	{
		SerialSync(outStream,typeName);
		metrics.Serialize(outStream);
	}

	/*virtual override*/	void					Statistics::UnitStatistics::Deserialize(IReadStream&inStream)
	{
		SerialSync(inStream,typeName);
		metrics.Deserialize(inStream);
	}

	/*virtual override*/	void					Statistics::RoundStatistics::Serialize(IWriteStream&s) const
	{
		SerialSync(s,production);
		SerialSync(s,consumption);
		metrics.Serialize(s);
		s.WriteSize(mainRound);
	}

	/*virtual override*/	void					Statistics::RoundStatistics::Deserialize(IReadStream&s)
	{
		SerialSync(s,production);
		SerialSync(s,consumption);
		metrics.Deserialize(s);
		s.ReadSize(mainRound);
	}




	#ifndef CLIENT
		void			Statistics::SignalDamageReceivedFrom(const PFaction&source, int level)
		{
			Get(source).metrics.Add(Metric::DamageReceived, level);
		}

		void			Statistics::SignalDamageDealtTo(const PWorldEntity&causedBy, const PFaction&receiver, int level)
		{
			Get(receiver).metrics.Add(Metric::DamageDealt,level);
			GetTypeOf(causedBy).metrics.Add(Metric::DamageDealt,level);
		}

		void			Statistics::SignalEMPDamageReceivedFrom(const PFaction&source, int level)
		{
			Get(source).metrics.Add(Metric::EMPDamageReceived,level);
		}

		void			Statistics::SignalEMPDamageDealtTo(const PWorldEntity&causedBy, const PFaction&receiver, int level)
		{
			Get(receiver).metrics.Add(Metric::EMPDamageDealt,level);
			GetTypeOf(causedBy).metrics.Add(Metric::EMPDamageDealt,level);
		}
		
		void			Statistics::SignalDestructionOfHostile(const PWorldEntity&causedBy, const PWorldEntity&destroyed)
		{
			auto&in = Get(destroyed->owner);
			in.metrics.Inc(Metric::UnitsKilled);
			in.metrics.Add(Metric::ResourcesDestroyed,destroyed->GetConstructionCost());
			auto&un = GetTypeOf(causedBy);
			un.metrics.Inc(Metric::UnitsKilled);
			un.metrics.Add(Metric::ResourcesDestroyed,destroyed->GetConstructionCost());
		}
		
		void			Statistics::SignalLossOf(const PWorldEntity&destroyed, const PFaction&causedBy, bool stolen)
		{
			auto&in = Get(causedBy);
			if (stolen)
			{
				in.metrics.Inc(Metric::UnitsStolenBy);
				in.metrics.Add(Metric::ResourcesStolenBy, destroyed->GetConstructionCost());
			}
			else
			{
				in.metrics.Inc(Metric::UnitsKilledBy);
				in.metrics.Add(Metric::ResourcesDestroyedBy,destroyed->GetConstructionCost());
			}
			auto&un = GetTypeOf(destroyed);
			un.metrics.Inc(Metric::Lost);
		}

		void			Statistics::SignalUnitAcquired(const PWorldEntity&e, const PFaction&previousOwner)
		{
			if (previousOwner)
			{
				auto&in = Get(previousOwner);
				in.metrics.Inc(Metric::UnitsStolen);
				in.metrics.Add(Metric::ResourcesStolen, e->GetConstructionCost());
			}
			auto&un = GetTypeOf(e);
			if (previousOwner)
				un.metrics.Inc(Metric::Stolen);
			else
				un.metrics.Inc(Metric::Built);
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
						prog.metrics.Set(Metric::CurrentEntityCount,stat.totalEntities);
						prog.metrics.Set(Metric::CurrentEntityMineralValue, stat.totalEntityMineralValue);
						f->CheckSensory();
						prog.metrics.Set(Metric::CurrentSensorCoverage, f->knownWorld.CountVisibleCells());
						stat.totalEntities = 0;
						stat.totalEntityMineralValue = 0;
					}
					f->statistics.history.Append().mainRound = RuntimeConfig::roundNumber.main;
				});
			}
		}





	#endif

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
		ByteStreamBuffer stream;
		{
				
			foreach (this->history,r)
			{
				stream.Clear();
				r->Serialize(stream);
				chunk->AppendBlock(ChunkID::round,stream);
			}
		}
		{
			this->unitStatistics.VisitAllValues([chunk,&stream](const UnitStatistics&stat)
			{
				stream.Clear();
				stat.Serialize(stream);
				chunk->AppendBlock(ChunkID::unit,stream);
			});
		}
		{
			this->interactionMap.VisitAllValues([chunk,&stream](const Interaction&stat)
			{
				stream.Clear();
				stat.Serialize(stream);
				chunk->AppendBlock(ChunkID::interaction,stream);
			});
		}
	}

	void			Statistics::Clear()
	{
		unitStatistics.Clear();
		interactionMap.Clear();
		history.Clear();
		history.Append().mainRound = RuntimeConfig::roundNumber.main;
	}

	void			Statistics::ImportFrom(const Riff::Chunk*chunk, const GenericHashTable<TUniqueID,Objects::PFaction>&factionMap)
	{
		history.Clear();
		unitStatistics.Clear();
		interactionMap.Clear();

		const Riff::Chunk*child = chunk->GetFirstChild();
		do
		{
			const char* chunkType = nullptr;
			try
			{
				if (child->IsID(ChunkID::round))
				{
					chunkType = "RoundStatistics";
					RoundStatistics stat;
					DeserializeFromCompactMemory(stat,child->pointer(),child->size());
					history << stat;
				}
				else
				if (child->IsID(ChunkID::unit))
				{
					chunkType = "UnitStatistics";
					UnitStatistics stat;
					DeserializeFromCompactMemory(stat,child->pointer(),child->size());
					unitStatistics.Set(stat.typeName,stat);
				}
				else
				if (child->IsID(ChunkID::interaction))
				{
					chunkType = "Interaction";
					Interaction stat;
					DeserializeFromCompactMemory(stat,child->pointer(),child->size());

					PFaction f;
					factionMap.Query(stat.remoteID,f);
					if (f)
						interactionMap.Set(f.get(),stat);
				}
			}
			catch (const std::exception&ex)
			{
				if (chunkType)
					LogEvent(EventClass::Preset::fileError,"Statistics::ImportFrom(): Exception caught in "+String(chunkType)+" chunk: "+ ex.what());
				else
					LogEvent(EventClass::Preset::fileError,"Statistics::ImportFrom(): Exception caught: "+ String(ex.what()));
			}
		}
		while (child = child->GetNextSibling());

		if (history.IsEmpty() || history.Last().mainRound != RuntimeConfig::roundNumber.main)
				history.Append().mainRound = RuntimeConfig::roundNumber.main;

	}


	Statistics::RoundStatistics&	Statistics::GetRoundStat()
	{
		return history.Last();
	}






}

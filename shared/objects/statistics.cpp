#include "statistics.h"
#include "entity.h"
#include "../runtimeConfig.h"
#include "faction.h"
#include "prototypes.h"
#include "../cast.h"
#include "mobileEntity.h"

namespace Objects
{

	static bool statisticsInNetworkMode = true;

	/*static*/ void		Statistics::EnterFileMode()
	{
		statisticsInNetworkMode = false;
	}

	/*static*/ void		Statistics::EnterNetworkMode()
	{
		statisticsInNetworkMode = true;
	}


	UINT64&					Statistics::MetricTable::FetchOptional(Metric metric)
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
		FetchOptional(m)++;
	}
	void					Statistics::MetricTable::Add(Metric m, int delta)
	{
		if (delta <= 0)
		{
			DBG_ASSERT_EQUAL__(delta,0);
			return;
		}
		FetchOptional(m) += delta;
	}
	void					Statistics::MetricTable::Set(Metric m, count_t value)
	{
		if (value == 0)
		{
			map.Unset((K)m);
			return;
		}
		#ifdef X64
			DBG_ASSERT_LESS__(value,(1ULL<<61));
		#endif
		map.Set((K)m,value);
	}


	void			Serialize(IWriteStream&s, const Statistics::MetricTable&m)
	{
		s.WriteSize(m.map.Count());
		m.map.VisitAllEntries([&s](Statistics::MetricTable::K key, UINT64 value)
		{
			if (value > 0)
			{
				s.WriteSize(key);
				s.WriteSize(value);
			}
		});
	}

	void			Deserialize(IReadStream&s, Statistics::MetricTable&m)
	{
		count_t entries = 0;
		s.ReadSize(entries);
		m.map.Clear();
		for (index_t i = 0; i < entries; i++)
		{
			Statistics::MetricTable::K key;
			UINT64 val;
			s.ReadSize(key);

			if (key >= (UINT64)Statistics::Metric::Count)
				throw Except::Memory::SerializationFault(CLOCATION,"Deserialized key #"+String(key)+" exceeds valid metric range");

			s.ReadSize(val);

			if (val > 0)
				m.map.Set(key,val);
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
		#ifndef CLIENT
			history.Append().mainRound = RuntimeConfig::roundNumber.main;
		#endif
	}

	Statistics::FactionInteraction&	Statistics::Get(const PFaction&remote)
	{
		auto rs = interactionMap.QueryPointer(remote.get());
		if (rs)
			return *rs;
		rs = &interactionMap.Set(remote.get());
		rs->remoteID = remote->id;
		return *rs;
	}

	Statistics::EntityType&	Statistics::GetTypeOf(const PWorldEntity&e)
	{
		const auto type = e->GetType();
		auto rs = entityTypes.QueryPointer(type);
		if (rs)
			return *rs;
		rs = &entityTypes.Set(type);
		rs->type = type;
		return *rs;
	}




	/*virtual override*/	void					Statistics::FactionInteraction::Serialize(IWriteStream&s) const
	{
		using Serialization::Serialize;
		s.Write(remoteID.bytes,remoteID.NumBytes);
		Serialize(s,metrics);
	}

	/*virtual override*/	void					Statistics::FactionInteraction::Deserialize(IReadStream&s)
	{
		using Serialization::Deserialize;
		s.Read(remoteID.bytes,remoteID.NumBytes);
		Deserialize(s,metrics);
	}


	/*virtual override*/	void					Statistics::EntityType::Serialize(IWriteStream&s) const
	{
		using Serialization::Serialize;
		if (statisticsInNetworkMode)
			Serialize(s,type);
		else
			Serialize(s,ToString(type));
		Serialize(s,metrics);
	}

	/*virtual override*/	void					Statistics::EntityType::Deserialize(IReadStream&s)
	{
		using Serialization::Deserialize;
		if (statisticsInNetworkMode)
			Deserialize(s,type);
		else
		{
			String str;
			Deserialize(s,str);
			type = DecodeType(str);
		}
		Deserialize(s,metrics);
	}

	/*virtual override*/	void					Statistics::Round::Serialize(IWriteStream&s) const
	{
		using Serialization::Serialize;
		Serialize(s,production);
		Serialize(s,consumption);
		Serialize(s,metrics);
		Serialize(s,mainRound);
	}

	/*virtual override*/	void					Statistics::Round::Deserialize(IReadStream&s)
	{
		using Serialization::Deserialize;
		Deserialize(s,production);
		Deserialize(s,consumption);
		Deserialize(s,metrics);
		Deserialize(s,mainRound);
	}




	#ifndef CLIENT
		void			Statistics::SignalDamageReceivedFrom(const PFaction&source, int level)
		{
			Get(source).metrics.Add(Metric::DamageReceived, level);
			history.Last().metrics.Add(Metric::DamageReceived,level);
		}

		void			Statistics::SignalDamageDealtTo(const PWorldEntity&causedBy, const PFaction&receiver, int level)
		{
			Get(receiver).metrics.Add(Metric::DamageDealt,level);
			GetTypeOf(causedBy).metrics.Add(Metric::DamageDealt,level);
			history.Last().metrics.Add(Metric::DamageDealt,level);
		}

		void			Statistics::SignalEMPDamageReceivedFrom(const PFaction&source, int level)
		{
			Get(source).metrics.Add(Metric::EMPDamageReceived,level);
			history.Last().metrics.Add(Metric::EMPDamageReceived,level);
		}

		void			Statistics::SignalEMPDamageDealtTo(const PWorldEntity&causedBy, const PFaction&receiver, int level)
		{
			Get(receiver).metrics.Add(Metric::EMPDamageDealt,level);
			GetTypeOf(causedBy).metrics.Add(Metric::EMPDamageDealt,level);
			history.Last().metrics.Add(Metric::EMPDamageDealt,level);
		}
		
		void			Statistics::SignalDestructionOfHostile(const PWorldEntity&causedBy, const PWorldEntity&destroyed)
		{
			int res = destroyed->GetConstructionCost();
			#ifndef ONE_COMPOUND_PER_FACTION
				if (destroyed->IsMobile())
				{
					res += To<MobileEntity>::Cast(destroyed)->currentStorage.Sum();
				}
			#endif

			auto&in = Get(destroyed->owner);
			in.metrics.Inc(Metric::UnitsKilled);
			in.metrics.Add(Metric::ResourcesDestroyed,res);
			auto&un = GetTypeOf(causedBy);
			un.metrics.Inc(Metric::UnitsKilled);
			un.metrics.Add(Metric::ResourcesDestroyed,res);

			history.Last().metrics.Inc(Metric::UnitsKilled);
			history.Last().metrics.Add(Metric::ResourcesDestroyed,res);
		}
		
		void			Statistics::SignalLossOf(const PWorldEntity&destroyed, const PFaction&causedBy, bool stolen)
		{
			int res = destroyed->GetConstructionCost();
			#ifndef ONE_COMPOUND_PER_FACTION
				if (destroyed->IsMobile())
				{
					res += To<MobileEntity>::Cast(destroyed)->currentStorage.Sum();
				}
			#endif

			auto&in = Get(causedBy);
			if (stolen)
			{
				in.metrics.Inc(Metric::UnitsStolenByOther);
				in.metrics.Add(Metric::ResourcesStolenByOther, res);

				history.Last().metrics.Inc(Metric::UnitsStolenByOther);
				history.Last().metrics.Add(Metric::ResourcesStolenByOther, res);
			}
			else
			{
				in.metrics.Inc(Metric::UnitsKilledByOther);
				in.metrics.Add(Metric::ResourcesDestroyedByOther,res);

				history.Last().metrics.Inc(Metric::UnitsKilledByOther);
				history.Last().metrics.Add(Metric::ResourcesDestroyedByOther, res);
			}
			auto&un = GetTypeOf(destroyed);
			un.metrics.Inc(Metric::Lost);
		}

		void			Statistics::SignalUnitAcquired(const PWorldEntity&e, const PFaction&previousOwner)
		{
			int res = e->GetConstructionCost();
			#ifndef ONE_COMPOUND_PER_FACTION
				if (e->IsMobile())
				{
					res += To<MobileEntity>::Cast(destroyed)->currentStorage.Sum();
				}
			#endif	
				
			if (previousOwner)
			{
				auto&in = Get(previousOwner);
				in.metrics.Inc(Metric::UnitsStolen);
				in.metrics.Add(Metric::ResourcesStolen, res);

				history.Last().metrics.Inc(Metric::UnitsStolen);
				history.Last().metrics.Add(Metric::ResourcesStolen, res);
			}
			else
			{
				history.Last().metrics.Inc(Metric::UnitsBuilt);
				history.Last().metrics.Add(Metric::ResourcesUsedForConstruction, res);
			}

			auto&un = GetTypeOf(e);
			if (previousOwner)
				un.metrics.Inc(Metric::UnitsStolen);
			else
				un.metrics.Inc(Metric::UnitsBuilt);
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
				});

				Objects::ForeachFaction([](const Objects::PFaction&f)
				{
					auto&stat = f->statistics;
					{
						auto&prog = stat.history.Last();
						prog.metrics.Set(Metric::CurrentEntityCount,stat.totalEntities);
						prog.metrics.Set(Metric::CurrentEntityMineralValue, stat.totalEntityMineralValue);
						f->CheckSensory();
						prog.metrics.Set(Metric::CurrentSensorCoverage, f->knownWorld.CountVisibleCells());
						prog.metrics.Set(Metric::OffensiveCapabilities, f->GetOffensiveCapabilities());
						prog.metrics.Set(Metric::NumTerraformers, f->CountTurretType(ConstName::Turret_Terraformer));
						prog.metrics.Set(Metric::NumFactories, f->CountStructureType(ConstName::Type_VehiclePlant));
						prog.metrics.Set(Metric::NumLaboratories, f->CountStructureType(ConstName::Type_Laboratory));
						prog.metrics.Set(Metric::ResearchSpeed, f->researchState.GetResearchSpeed());
						prog.metrics.Set(Metric::WinningScore,f->GetWinningScore());
						prog.metrics.Set(Metric::NexusPortsOwned,f->acquiringNexusControl + f->nexusControl);
						prog.metrics.Set(Metric::NexusPortsControlled,f->nexusControl);

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
			this->entityTypes.VisitAllValues([chunk,&stream](const EntityType&stat)
			{
				stream.Clear();
				stat.Serialize(stream);
				chunk->AppendBlock(ChunkID::unit,stream);
			});
		}
		{
			this->interactionMap.VisitAllValues([chunk,&stream](const FactionInteraction&stat)
			{
				stream.Clear();
				stat.Serialize(stream);
				chunk->AppendBlock(ChunkID::interaction,stream);
			});
		}
	}

	void			Statistics::Clear()
	{
		BeginLoading();
		EndLoading(false);
	}

	void			Statistics::BeginLoading()
	{
		if (isLoading)
			throw Except::Program::DataConsistencyFault(CLOCATION,"Trying to restart loading sequence without prior invocation of EndLoading()");
		isLoading = true;
		history.Clear();
		entityTypes.Clear();
		interactionMap.Clear();
	}

	void			Statistics::Load(Round&&stat)
	{
		if (history.Count() > 10000)
			history.Erase(0,1000);	//just erase some. ignore
		history.MoveAppend(stat);
	}
	void			Statistics::Load(EntityType&&stat)
	{
		if (entityTypes.Count() > 10000)
			throw Except::Memory::SerializationFault(CLOCATION,"Number of loaded unit type statistics ("+String(entityTypes.Count())+") number exceeds allowed range");
		entityTypes.Set(stat.type) = std::move(stat);
	}
	void			Statistics::Load(FactionInteraction&&inter)
	{
		PFaction f = Objects::Faction::FindByID(inter.remoteID);
		if (f)
			interactionMap.Set(f.get()) = std::move(inter);
	}


	void			Statistics::DeserializeRound(const void*data, serial_size_t size)
	{
		Round stat;
		DeserializeFromCompactMemory(stat,data,size);
		Load(std::move(stat));
	}
	void			Statistics::DeserializeEntityType(const void*data, serial_size_t size)
	{
		EntityType stat;
		DeserializeFromCompactMemory(stat,data,size);
		Load(std::move(stat));
	}
	void			Statistics::DeserializeFactionInteraction(const void*data, serial_size_t size)
	{
		FactionInteraction stat;
		DeserializeFromCompactMemory(stat,data,size);
		Load(std::move(stat));
	}

	void			Statistics::ImportFrom(const Riff::Chunk*chunk, bool factionIsDead)
	{
		BeginLoading();


		const Riff::Chunk*child = chunk->GetFirstChild();
		do
		{
			const char* chunkType = nullptr;
			try
			{
				if (child->IsID(ChunkID::round))
				{
					chunkType = "Round";
					DeserializeRound(child->pointer(),child->size());
				}
				else
				if (child->IsID(ChunkID::unit))
				{
					chunkType = "EntityType";
					DeserializeEntityType(child->pointer(),child->size());
				}
				else
				if (child->IsID(ChunkID::interaction))
				{
					chunkType = "FactionInteraction";
					DeserializeFactionInteraction(child->pointer(),child->size());
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

		EndLoading(factionIsDead);
	}

	void Statistics::EndLoading(bool factionIsDead)
	{
		if (!isLoading)
			throw Except::Program::DataConsistencyFault(CLOCATION,"Trying to end loading sequence without prior invocation of BeginLoading()");
		isLoading = false;

		for (index_t i = 0; i < history.Count(); i++)
		{
			bool good = true;
			if (history[i].mainRound > RuntimeConfig::roundNumber.main)
				good = false;
			elif (i && history[i].mainRound <= history[i-1].mainRound)
				good = false;

			if (!good)
			{
				history.Erase(i--);
			}
		}
		#ifndef CLIENT
			if (history.IsEmpty() || history.Last().mainRound != RuntimeConfig::roundNumber.main)
				if (!factionIsDead)
					history.Append().mainRound = RuntimeConfig::roundNumber.main;
		#endif
	}


	Statistics::Round&	Statistics::GetRoundStat()
	{
		return history.Last();
	}

}

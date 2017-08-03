#pragma once

#include "types.h"
#include <container/hashtable.h>
#include "resource.h"
#include <io/json.h>
#include <io/riff_handler.h>
#include "../uniqueID.h"


using namespace DeltaWorks;


namespace Objects
{

	class Statistics
	{
		const Faction*const	owner;
	public:

		enum class Metric
		{
			//VERY Important: if any no longer in use: DO NOT REMOVE. Prepend Deprecated_. These are used for IO, and may be imported from potentially very old savegames
			DamageReceived,
			DamageDealt,
			EMPDamageReceived,
			EMPDamageDealt,
			UnitsKilled,
			ResourcesDestroyed,
			UnitsStolen,
			ResourcesStolen,

			UnitsKilledBy,
			UnitsStolenBy,
			ResourcesDestroyedBy,
			ResourcesStolenBy,

			Lost,				//how many of this type we lost
			Built,				//how many of this type we built
			Stolen,				//how many of this type we stole

			CurrentEntityCount,
			CurrentEntityMineralValue,
			CurrentSensorCoverage,

		};


		class MetricTable : public ISerializable
		{
		public:
			typedef MetricTable		Self;
			typedef std::underlying_type<Metric>::type	K;


			UINT64					operator[](Metric metric) const;
			void					Inc(Metric);
			void					Add(Metric, int delta);
			void					Set(Metric, count_t value);

			bool					operator==(const Self&other) const;
			bool					operator!=(const Self&other) const {return !operator==(other);}

			void					Clear() {map.Clear();}

			virtual	void			Serialize(IWriteStream&outStream) const override;
			virtual	void			Deserialize(IReadStream&inStream) override;

		private:
			UINT64&					Fetch(Metric metric);

			GenericHashTable<K,UINT64>	map;
		};



		struct Interaction : public ISerializable
		{
			typedef Interaction Self;

			TUniqueID				remoteID;
			MetricTable				metrics;

			bool					operator==(const Self&other) const {return remoteID == other.remoteID && metrics == other.metrics;}
			bool					operator!=(const Self&other) const {return !operator==(other);}

			virtual	void			Serialize(IWriteStream&outStream) const override;
			virtual	void			Deserialize(IReadStream&inStream) override;
		};

		struct UnitStatistics : public ISerializable
		{
			typedef UnitStatistics	Self;

			String					typeName;
			MetricTable				metrics;
		
			bool					operator==(const Self&other) const {return typeName == other.typeName && metrics == other.metrics;}
			bool					operator!=(const Self&other) const {return !operator==(other);}

			virtual	void			Serialize(IWriteStream&outStream) const override;
			virtual	void			Deserialize(IReadStream&inStream) override;
		};

		struct RoundStatistics : public ISerializable
		{
			PositiveResourceVector	production,consumption;
			MetricTable				metrics;
			UINT32					mainRound;
			
			virtual	void			Serialize(IWriteStream&outStream) const override;
			virtual	void			Deserialize(IReadStream&inStream) override;
		};


	public:
		/**/			Statistics(const Faction*owner);

		#ifndef CLIENT
			void		SignalDamageReceivedFrom(const PFaction&source, int level);
			void		SignalDamageDealtTo(const PWorldEntity&causedBy, const PFaction&receiver, int level);
			void		SignalEMPDamageReceivedFrom(const PFaction&source, int level);
			void		SignalEMPDamageDealtTo(const PWorldEntity&causedBy, const PFaction&receiver, int level);
			void		SignalDestructionOfHostile(const PWorldEntity&causedBy, const PWorldEntity&);
			void		SignalLossOf(const PWorldEntity&, const PFaction&causedBy, bool stolen);
			void		SignalUnitAcquired(const PWorldEntity&, const PFaction&previousOwner);
			/**
			Invoked for each structure that generates or consumes resources during compound processing
			*/
			void		SignalStructureResourceDelta(const PWorldEntity&, const ResourceVector&);
			/**
			Invoked once at the end of compound processing
			*/
			void		SignalCompoundProcessingEnd();
			static void	SignalNewRoundBegin(bool mainRound);

		
		#endif

		void			ExportInto(Riff::Chunk*) const;
		void			ImportFrom(const Riff::Chunk*, const GenericHashTable<TUniqueID,Objects::PFaction>&factionMap);
		void			Clear();

	private:
		//temporary data:
		ResourceVector				roundProduction,
									roundConsumption;
		count_t						totalEntities=0,
									totalEntityMineralValue=0;

		//persistent data:
		PointerTable<Interaction>	interactionMap;
		StringTable<UnitStatistics>	unitStatistics;
		Buffer0<RoundStatistics>	history;


		//access helpers:
		Interaction&				Get(const PFaction&remote);
		UnitStatistics&				GetTypeOf(const PWorldEntity&);
		RoundStatistics&			GetRoundStat();
	};

}

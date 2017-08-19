#pragma once

#include "types.h"
#include <container/hashtable.h>
#include "resource.h"
#include <io/json.h>
#include <io/riff_handler.h>
#include "../uniqueID.h"
#include "entityType.h"


using namespace DeltaWorks;


namespace Objects
{

	class Statistics
	{
		const Faction*const	owner;
	public:

		/**
		Enumeration of all supported metrics.
		These are inserted into a hash-table and may therefor grow arbitrarily large.

		IMPORTANT: if any no longer in use: DO NOT REMOVE! Do not change order
		Prepend Deprecated_. These are used for IO, and may be imported from potentially very old savegames.
		*/
		enum class Metric
		{
			DamageReceived,
			DamageDealt,
			EMPDamageReceived,
			EMPDamageDealt,
			UnitsKilled,
			ResourcesDestroyed,
			UnitsStolen,				//how many of this type we stole
			ResourcesStolen,

			UnitsKilledByOther,
			UnitsStolenByOther,
			ResourcesDestroyedByOther,
			ResourcesStolenByOther,

			Lost,				//how many of this type we lost
			UnitsBuilt,				//how many of this type we built
			Deprecated0,

			CurrentEntityCount,
			CurrentEntityMineralValue,
			CurrentSensorCoverage,

			ResourcesUsedForConstruction,	//how many resources (minerals probably) were converted into other stuff (entities probably). Conversion into electricity does not count



			Count
		};


		class MetricTable
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

			friend void				SerialSync(IWriteStream&outStream, const MetricTable&);
			friend void				SerialSync(IReadStream&inStream, MetricTable&);

		private:
			UINT64&					Fetch(Metric metric);

			GenericHashTable<K,UINT64>	map;
		};



		struct FactionInteraction : public ISerializable
		{
			typedef FactionInteraction Self;

			TUniqueID				remoteID;
			MetricTable				metrics;

			bool					operator==(const Self&other) const {return remoteID == other.remoteID && metrics == other.metrics;}
			bool					operator!=(const Self&other) const {return !operator==(other);}

			virtual	void			Serialize(IWriteStream&outStream) const override;
			virtual	void			Deserialize(IReadStream&inStream) override;
		};

		struct EntityType : public ISerializable
		{
			typedef EntityType	Self;

			Objects::EntityType		type;
			MetricTable				metrics;
		
			bool					operator==(const Self&other) const {return type == other.type && metrics == other.metrics;}
			bool					operator!=(const Self&other) const {return !operator==(other);}

			virtual	void			Serialize(IWriteStream&outStream) const override;
			virtual	void			Deserialize(IReadStream&inStream) override;
		};

		struct Round : public ISerializable
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
		void			ImportFrom(const Riff::Chunk*);
		void			Clear();


		//Sequential loading functions

		/**
		Flushes all local data and awaits Load*() calls. Always finish with EndLoading()
		*/
		void			BeginLoading();
		/**
		Loads a new Round item from serialized data. May throw exceptions
		*/
		void			DeserializeRound(const void*, serial_size_t);
		/**
		Loads a new EntityType item from serialized data. May throw exceptions
		*/
		void			DeserializeEntityType(const void*,serial_size_t);
		/**
		Loads a new FactionInteraction item from serialized data. May throw exceptions
		*/
		void			DeserializeFactionInteraction(const void*,serial_size_t);
		void			Load(Round&&stat);
		void			Load(EntityType&&stat);
		void			Load(FactionInteraction&&inter);
		/**
		Concludes loading. Regardless of wether items were actually loaded, this method will assure the local Statistics instance is valid
		*/
		void			EndLoading();

		/**
		Changes serialization mode to use strings rather than EntityType
		*/
		static void		EnterFileMode();
		/**
		Changes serialization mode to use EntityType rather than strings
		*/
		static void		EnterNetworkMode();


		void			VisitRounds(const std::function<void(const Round&)>&f) const
		{
			foreach (history,h)
				f(*h);
		}
		void			VisitFactionInteractions(const std::function<void(const FactionInteraction&)>&f) const
		{
			interactionMap.VisitAllValues(f);
		}
		void			VisitEntityTypes(const std::function<void(const EntityType&)>&f) const
		{
			entityTypes.VisitAllValues(f);
		}

	private:
		//temporary data:
		ResourceVector				roundProduction,
									roundConsumption;
		count_t						totalEntities=0,
									totalEntityMineralValue=0;
		bool						isLoading = false;

		//persistent data:
		PointerTable<FactionInteraction>	interactionMap;
		HashTable<Objects::EntityType,EntityType>	entityTypes;
		Buffer0<Round>				history;


		//access helpers:
		FactionInteraction&		Get(const PFaction&remote);
		EntityType&				GetTypeOf(const PWorldEntity&);
		Round&					GetRoundStat();
	};

}

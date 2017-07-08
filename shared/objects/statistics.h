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


		struct Interaction : public ISerializable
		{
			typedef Interaction Self;
			TUniqueID	remoteID;

			enum metric_t
			{
				//VERY Important: if any no longer in use: DO NOT REMOVE. Append _Deprecated. These are used for IO, and may be imported from potentially very old savegames
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


				NumMetrics
			};


			FixedArray<UINT64,NumMetrics>		metric;

			UINT64&		Get(metric_t m) {return metric[m];}

			/**/		Interaction()
			{
				metric.Fill(0);
			}

			virtual	serial_size_t	GetSerialSize(bool exportSize) const override;
			virtual	void			Serialize(IWriteStream&outStream, bool exportSize) const override;
			virtual	void			Deserialize(IReadStream&inStream, serial_size_t fixedSize) override;
			bool					operator==(const Self&other) const {return remoteID == other.remoteID && metric == other.metric;}
			bool					operator!=(const Self&other) const {return !operator==(other);}
		};

		struct UnitStatistics : public ISerializable
		{
			typedef UnitStatistics	Self;
			String		typeName;

			enum metric_t
			{
				//VERY Important: if any no longer in use: DO NOT REMOVE. Append _Deprecated. These are used for IO, and may be imported from potentially very old savegames
				DamageDealt,	//how much damage this type caused
				EMPDamageDealt,	//how much EMP damage this type caused
				UnitsKilled,	//how many units this type killed
				ResourcesDestroyed,	//how many resources this type destroyed
				Lost,				//how many of this type we lost
				Built,				//how many of this type we built
				Stolen,				//how many of this type we stole
				NumMetrics
			};

			FixedArray<UINT64,NumMetrics>		metric;
		
			UINT64&		Get(metric_t m) {return metric[m];}

			/**/		UnitStatistics()
			{
				metric.Fill(0);
			}
			virtual	serial_size_t	GetSerialSize(bool exportSize) const override;
			virtual	void			Serialize(IWriteStream&outStream, bool exportSize) const override;
			virtual	void			Deserialize(IReadStream&inStream, serial_size_t fixedSize) override;
			bool					operator==(const Self&other) const {return typeName == other.typeName && metric == other.metric;}
			bool					operator!=(const Self&other) const {return !operator==(other);}
		};

		struct RoundStatistics : public ISerializable
		{
			PositiveResourceVector	production,consumption;
			UINT64			entityCount=0,
							entityMineralValue=0;
			UINT32			sensorCoverage=0;
			UINT32			mainRound=0;

			virtual	serial_size_t			GetSerialSize(bool exportSize) const override;
			virtual	void					Serialize(IWriteStream&outStream, bool exportSize) const override;
			virtual	void					Deserialize(IReadStream&inStream, serial_size_t fixedSize) override;
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

		
			void		ExportInto(Riff::Chunk*) const;
			void		ImportFrom(const Riff::Chunk*, const GenericHashTable<TUniqueID,Objects::PFaction>&factionMap);
		#endif


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

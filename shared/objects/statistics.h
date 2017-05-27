#pragma once

#include "types.h"
#include <container/hashtable.h>

namespace Objects
{

	class Statistics
	{
		const Faction*const	owner;
	public:
		struct Interaction
		{
			WFaction	remote;

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


			FixedArray<count_t,NumMetrics>		metric;

			count_t&	Get(metric_t m) {return metric[m];}

			/**/		Interaction()
			{
				metric.Fill(0);
			}
		};

		struct UnitStatistics
		{
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

			FixedArray<count_t,NumMetrics>		metric;
		
			count_t&	Get(metric_t m) {return metric[m];}

			/**/		UnitStatistics()
			{
				metric.Fill(0);
			}
		};


	public:
		/**/			Statistics(const Faction*owner);
		void			SignalDamageReceivedFrom(const PFaction&source, int level);
		void			SignalDamageDealtTo(const PWorldEntity&causedBy, const PFaction&receiver, int level);
		void			SignalEMPDamageReceivedFrom(const PFaction&source, int level);
		void			SignalEMPDamageDealtTo(const PWorldEntity&causedBy, const PFaction&receiver, int level);
		void			SignalDestructionOfHostile(const PWorldEntity&causedBy, const PWorldEntity&);
		void			SignalLossOf(const PWorldEntity&, const PFaction&causedBy, bool stolen);
		void			SignalUnitAcquired(const PWorldEntity&, const PFaction&previousOwner);
	private:
		PointerTable<Interaction>	interactionMap;
		StringTable<UnitStatistics>	unitStatistics;

		Interaction&	Get(const PFaction&remote);
		UnitStatistics&	GetTypeOf(const PWorldEntity&);

	};

}

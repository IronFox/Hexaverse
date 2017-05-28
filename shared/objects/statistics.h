#pragma once

#include "types.h"
#include <container/hashtable.h>

namespace Objects
{

	class Statistics : public SerializableObject
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

		
		virtual	serial_size_t	GetSerialSize(bool exportSize) const override;							//!< Queries the serial size of this object @param export_size True if the result should include additional space required for any size variable such as a string or array length @return Serial size (in bytes) of this object
		virtual	void			Serialize(IWriteStream&outStream, bool exportSize) const;		//!< Writes the serialized content of the local object to the specified stream. The size of the written data must match the result of GetSerialSize() as long as the object remains unchanged. @param outStream Stream to write to @param exportSize True if the object is expected to export any dynamic size such as string or array length @return true if the object could be fully written to the out stream, false otherwise
		virtual	void			Deserialize(IReadStream&inStream, serial_size_t fixedSize);			//!< Reads the content of the local object from the specified stream @param inStream Stream to read from @param fixedSize ISerializable::EmbeddedSize if any dynamic object size should be read from the stream or the size in bytes that the local object should adopt to otherwise. If this parameter is ISerializable::EmbeddedSize then the object data was serialized with @b export_size set to true @return true if the object data could be fully read from the out stream, false otherwise
		virtual	bool			HasFixedSize() const {return false;}									//!< Queries whether this object is of fixed size (always the same no matter what content)


	private:
		PointerTable<Interaction>	interactionMap;
		StringTable<UnitStatistics>	unitStatistics;

		Interaction&	Get(const PFaction&remote);
		UnitStatistics&	GetTypeOf(const PWorldEntity&);

	};

}

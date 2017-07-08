#include <global_root.h>
#include "resource.h"
#include "../grid/resources.h"


namespace Objects
{
	bool	NaturalResourceVector::SampleWorld(index_t lowCellID)
	{
		fuel = Grid::Resource::Sample(ConstName::Resource_Fuel,lowCellID)*TranslationFactor;
		minerals = Grid::Resource::Sample(ConstName::Resource_Minerals,lowCellID)*TranslationFactor;
		uranium = Grid::Resource::Sample(ConstName::Resource_Uranium,lowCellID)*TranslationFactor;
		#ifdef CLIENT
			return Grid::ResourceDiscovery::client.resourcesDiscovered[lowCellID];
		#else
			return true;
		#endif
	}


	void	ResourceFlagSet::SetAllNonZero(const ResourceVector&res)
	{
		for (index_t i = 0; i < Resource::N; i++)
			if (res.level[i])
				isSet |= ((flagT)1 << (flagT)i);
	}





	/**/	PositiveResourceVector::PositiveResourceVector(const Objects::ResourceVector&v)
	{
		((Objects::ResourceVector&)*this) = v;
		#ifdef _DEBUG
			for (index_t i = 0; i < Objects::ResourceVector::NumEntries; i++)
			{
				ASSERT_GREATER_OR_EQUAL__(level[i], 0);
			}
		#endif
	}

		
	/*static*/	serial_size_t			PositiveResourceVector::GetSerialSize(const Objects::ResourceVector&vec)
	{
		serial_size_t rs = 1;	//flag byte
		static_assert(Objects::ResourceVector::NumEntries <= 8, "Adjust protocol");
		for (index_t i = 0; i < ResourceVector::NumEntries; i++)
		{
			if (vec.level[i] > 0)
				rs += GetSerialSizeOfSize(abs(vec.level[i]));	//negative values are set to zero
		}
		return rs;
	}


	/*static*/	void					PositiveResourceVector::Serialize(IWriteStream&outStream, const Objects::ResourceVector&vec)
	{
		BYTE flag = 0;
		for (index_t i = 0; i < ResourceVector::NumEntries; i++)
		{
			if (vec.level[i] > 0)
			{
				flag |= (1 << i);
			}
			else
			{
				DBG_ASSERT_EQUAL1__(vec.level[i], 0,Objects::Resource::Reinterpret(i).ToString());
			}
		}
		outStream.Write(&flag,1);
		for (index_t i = 0; i < Objects::ResourceVector::NumEntries; i++)
			if (vec.level[i] > 0)
				outStream.WriteSize((serial_size_t)(vec.level[i]));
	}
		
	/*static*/	void					PositiveResourceVector::Deserialize(IReadStream&inStream, Objects::ResourceVector&vec)
	{
		BYTE flags;
		inStream.Read(&flags,1);

		for (index_t i = 0; i < ResourceVector::NumEntries; i++)
		{
			bool exists = ((flags >> i)&1) != 0;
			if (!exists)
			{
				vec.level[i] = 0;
				continue;
			}
			UINT32 val;
			inStream.ReadSize(val);
			vec.level[i] = val;
		}
	}




	String GetTagName(char c)
	{
		if (!IsTag(c))
			return c;
		index_t idx = c - TextoutResourceTagOffset;
		if (idx < Resource::N)
			return Resource::Reinterpret(idx).ToString();
		if (c == TimeResourceTag())
			return "time";
		if (c == TrafficResourceTag())
			return "traffic";
		return "";
	}

}

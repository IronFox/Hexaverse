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

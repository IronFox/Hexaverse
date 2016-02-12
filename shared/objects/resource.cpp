#include <global_root.h>
#include "resource.h"
#include "../sphereGrid.h"


namespace Objects
{
	bool	NaturalResourceVector::SampleWorld(index_t lowCellID)
	{
		fuel = Grid::Resource::Sample(ConstName::Resource_Fuel,lowCellID)*TranslationFactor;
		minerals = Grid::Resource::Sample(ConstName::Resource_Minerals,lowCellID)*TranslationFactor;
		uranium = Grid::Resource::Sample(ConstName::Resource_Uranium,lowCellID)*TranslationFactor;
		#ifdef CLIENT
			return Grid::Resource::Require(ConstName::Resource_Minerals).known[lowCellID];
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


}

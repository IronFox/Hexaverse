#include <global_root.h>
#include "runtimeConfig.h"

#include <algorithm>
//#include <general/undef.h>


namespace RuntimeConfig
{
	DoomsdayWeapons doomsdayWeapons;
	GameGoal gameGoal;

	bool hibernationSystemEnabled = false;
	UINT32 roundNumber =0;
	UINT32 techniteSubRoundNumber =0;
	Phase	phase;


	namespace UnitLimits
	{
		static UINT16	map[(unsigned)ConstName::NumTypes];

		static bool isSet = false;

		bool		AnySet()
		{
			return isSet;
		}
		void		Reset()
		{
			isSet = false;
			memset(map,0,sizeof(map));
		}
		void		Set(ConstName entry, int value)
		{
			isSet = true;
			ASSERT__(value >= 0);
			map[(unsigned)entry] = (UINT16)std::min<UINT16>(0xFFFF,value);
		}
		count_t		Get(ConstName entry)
		{
			return map[(unsigned)entry];
		}

		void		Export(Array<std::pair<UINT16,UINT16> >&result)
		{
			count_t nonZero = 0;
			for (index_t i = 0; i < ARRAYSIZE(map); i++)
				if (map[i])
					nonZero++;
			result.SetSize(nonZero);
			index_t at = 0;
			for (index_t i = 0; i < ARRAYSIZE(map); i++)
				if (map[i])
				{
					result[at++] = std::make_pair((UINT16)i,map[i]);
				}
		}

		void		Import(const Array<std::pair<UINT16,UINT16> >&input)
		{
			Reset();
			isSet = true;
			foreach (input, entry)
				map[entry->first] = entry->second;
		}

	}

}

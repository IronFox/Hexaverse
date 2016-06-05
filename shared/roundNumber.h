#pragma once

#include <global_string.h>
#include <algorithm>

struct RoundNumber
{
	UINT32		main=0,
				technite=0;

	static UINT32	extraTechniteRounds /*= 0*/;

	/**/		RoundNumber(
					UINT32 main = 0
					,UINT32 technite = 0
					):
					main(main)
					,technite(technite)
				{}
	bool		operator==(const RoundNumber&other) const
	{
		return 
			main == other.main
			&& technite == other.technite
		;
	}
	bool		operator>(const RoundNumber&other) const
	{
		return
			main > other.main
			|| (main == other.main && technite > other.technite)
		;
	}
	String		ToString() const
	{
		return String(main)
		+","+String(technite)
		;
	}
	int			operator-(const RoundNumber&other) const
	{
		return (main - other.main) * (1+extraTechniteRounds) + (technite - other.technite);
	}
	bool		operator!=(const RoundNumber&other) const {return !operator==(other);}
	bool		operator<(const RoundNumber&other) const {return !operator==(other) && !operator>(other);}
	bool		operator>=(const RoundNumber&other) const {return operator==(other) || operator>(other);}
	bool		operator<=(const RoundNumber&other) const {return !operator>(other);}
};

namespace std
{
	inline RoundNumber	max(const RoundNumber&a, const RoundNumber&b)
	{
		return a > b ? a : b;
	}
}

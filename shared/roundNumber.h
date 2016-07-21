#pragma once

#include <global_string.h>
#include <algorithm>

struct RoundNumber
{
	UINT32		main=0;

	/**/		RoundNumber(
					UINT32 main = 0
					):
					main(main)
				{}
	bool		operator==(const RoundNumber&other) const
	{
		return 
			main == other.main
		;
	}
	bool		operator>(const RoundNumber&other) const
	{
		return
			main > other.main
		;
	}
	String		ToString() const
	{
		return String(main)
		;
	}
	int			operator-(const RoundNumber&other) const
	{
		return main - other.main;
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

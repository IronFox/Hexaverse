#pragma once

#include <global_string.h>
#include <algorithm>

using namespace DeltaWorks;

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
	friend void	Serialize(IWriteStream&s, const RoundNumber&v)
	{
		using Serialization::Serialize;
		Serialize(s,v.main);
	}
	friend void	Deserialize(IReadStream&s, RoundNumber&v)
	{
		using Serialization::Deserialize;
		Deserialize(s,v.main);
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

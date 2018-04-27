#include <global_root.h>
#include "protocol.h"

namespace Protocol
{
	String ToString(const Timestamp&t)
	{
		if (t.rightNow)
			return "Now";
		return t.atTime;
	}


}	




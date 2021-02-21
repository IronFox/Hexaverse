#include <global_root.h>
#include "protocol.h"

namespace Protocol
{
	String ToString( const Timestamp&t )
	{
		if (t.rightNow)
			return "Now";
		return t.atTime;
	}
	String ToString( const TimeWindow&t )
	{
		if (t.begin.rightNow)
			return "Now";
		return "["+String(t.begin.atTime)+","+String(t.begin.atTime+t.duration)+"]";
	}


}	




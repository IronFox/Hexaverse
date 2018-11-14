#include "abstractEvent.h"

namespace Animation
{
	/*static*/ float			Event::currentTime = 0;

	Event::Event(const Protocol::Timestamp&t, float seconds, const TUniqueID&id):
		time(M::FloatRange(t.atTime,t.atTime+seconds)),targetEntityID(id),executeNow(t.rightNow)
	{}


}

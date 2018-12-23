#include "abstractEvent.h"

namespace Animation
{
	/*static*/ float			Event::currentTime = 0;

	Event::Event(const TUniqueID&id):
		targetEntityID(id)
	{}


}

#pragma once
#include "observer.h"

namespace GlobalEvents
{
	extern Observer<>	terrainFlush,	//erase terrain to factory default. Also includes other elements that are frequently flushed when resetting the scene. If this is called, objectFlush must also be called
						objectFlush;	//erases objects only. 


	inline void			FlushObjects()	{objectFlush();}
	inline void			FlushAll()		{objectFlush(); terrainFlush();}
}

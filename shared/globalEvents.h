#pragma once
#include <structure/observable.h>

namespace GlobalEvents
{
	extern DeltaWorks::Observable<>	
						terrainFlush,	//erase terrain to factory default. Also includes other elements that are frequently flushed when resetting the scene. If this is called, objectFlush must also be called
						objectFlush,	//erases objects only. 
						applicationTerminate,
						invalidateObjectCache;	//indicates that something changed in the status of units such that the client must invalidate the render cache of any selected units



	inline void			FlushObjects()	{objectFlush();}
	inline void			FlushAll()		{objectFlush(); terrainFlush();}
}

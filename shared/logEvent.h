#ifndef logEventH
#define logEventH

#include <general/enumeration.h>
#include <global_string.h>

CONSTRUCT_ENUMERATION17(EventType, 
						Fatal,
						UnexpectedBehavior,
						Debug,
						PerformanceMeasurement,
						ConfigRelated,
						InternalFault,
						Notification, //unspecified notification
						SessionRelated, //users joining or leaving
						GameProgressRelated,
						TerrainProcess, //terrain notifications
						MinorEntityProcess,
						EntityProcess,	//notifications about a specific entity
						EntityProcessFailed,
						CompoundProcess,	//notifications about a complex network
						OperationAborted,	//notifies that an operation was aborted (unplanned)
						CommandRejected,	//notifies that a specific command was rejected
						ChatMessage
						)


namespace Objects
{
	class WorldEntity;

}

void LogEvent(EventType, const Objects::WorldEntity*, const String&);




#endif

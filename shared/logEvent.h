#ifndef logEventH
#define logEventH

#include <general/enumeration.h>
#include <global_string.h>

CONSTRUCT_ENUMERATION18(EventType, 
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
						ChatMessage,
						TechniteLogic
						)


inline bool IsImportant(EventType t)
{
	switch (t)
	{
	case EventType::Fatal:
	case EventType::UnexpectedBehavior:
	case EventType::InternalFault:
	case EventType::CommandRejected:
		return true;
	default:
		return false;
	};
	
}

namespace Objects
{
	class WorldEntity;

}






extern bool		inTestMode /*= false*/;


namespace Logging
{
	void	End();
	void	Begin(const String&path);

	String	TimeStamp(bool filename=false);
}

void		LogEvent(EventType, const Objects::WorldEntity*, const String&);

inline void LogEvent(EventType type, const String&msg)
{
	::LogEvent(type,nullptr,msg);
}



#endif

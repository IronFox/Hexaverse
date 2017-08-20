#ifndef logEventH
#define logEventH

#include <general/enumeration.h>
#include <global_string.h>
#include "objects/types.h"



CONSTRUCT_ENUMERATION9(EventSignificance,
						Common,
						Debug,
						PerformanceMeasurement,
						Uncommon,		//this and everything downwards is logged on the console and sent to clients
						Important,
						NonFatalError,
						FactionFatal,
						ClientFatal,
						SystemFatal
						);

CONSTRUCT_ENUMERATION7(EventFlavor,
						Neutral,	//colored white by the client, unless important (then blue)
						Gain,		//colored green by the client
						Loss,		//colored red by the client
						ChatMessage,	//colored white by the client, but printed in large
						UserError,		//colored in purple red
						SystemError,		//colored in purple red
						ServerPrivate	//never sent the client
						);




//
//inline bool IsImportant(EventType t)
//{
//	switch (t)
//	{
//	case EventClass::Preset::fatal:
//	case EventClass::Preset::unexpectedBehavior:
//	case EventClass::Preset::error:
//	case EventClass::Preset::commandRejected:
//		return true;
//	default:
//		return false;
//	};
//	
//}

namespace Objects
{
	class WorldEntity;

}



struct EventClass
{
	struct Preset
	{
		static EventClass	error,	//!< Non-fatal error that should be logged and displayed on the console
							internalError,	//!< Non-fatal error that should be logged and displayed on the console. Not distributed to clients
							fatal,
							unexpectedBehavior,
							uncommon,
							important,
							debug,
							factionDeath,
							winningFaction,
							commandRejected,
							operationAborted,
							configuration,
							configurationPrint,
							configurationError,
							fileError,
							network,
							networkError,
							remoteControlResponse,
							remoteControlCommand,
							remoteControlError,
							common	//!< Internal normal process that should be logged but not displayed
							;
	};

	EventSignificance	significance;
	EventFlavor	flavor;

	/**/	EventClass()	{}
	/**/	EventClass(EventSignificance sig):significance(sig)	{}
	/**/	EventClass(EventSignificance::value_t sig):significance(sig)	{}
	/**/	EventClass(EventSignificance sig, EventFlavor fl):significance(sig),flavor(fl)	{}
};


extern bool		inTestMode /*= false*/;


namespace Logging
{
	void	End();
	void	Begin(const PathString&path);

	String	TimeStamp(bool filename=false);
}

void		LogEvent(const EventClass&evClass, const Objects::WorldEntity*, const String&);
void		LogEvent(const EventClass&evClass, const Objects::OwnerParameter&, const String&);
void		LogException(const Objects::WorldEntity*, const std::exception&ex);

inline void LogEvent(const EventClass&evClass, const String&msg)
{
	::LogEvent(evClass,nullptr,msg);
}



#endif

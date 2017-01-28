#include <global_root.h>
//#include "serverLogEvent.h"
#include <io/log.h>
#include <general/system.h>
//#include "state.h"
//#include "networkChannels.h"
#include <io/sharedLogFile.h>
#include <container/production_pipe.h>
#include "logEvent.h"
#include <net/tcp.h>

#include "runtimeConfig.h"
#include "./objects/faction.h"
#include <io/file_system.h>

#ifndef CLIENT
	#include "../server/components/state.h"
	#include "../server/components/serverLogEvent.h"
#endif

#ifdef HAS_WINDOW
	void VisualLogEvent( EventClass type,  const Objects::WorldEntity*e, const String&_event);
#endif

bool		inTestMode = false;


EventClass	EventClass::Preset::error = EventClass(EventSignificance::NonFatalError),
			EventClass::Preset::internalError = EventClass(EventSignificance::NonFatalError,EventFlavor::ServerPrivate),
			EventClass::Preset::fatal = EventClass(EventSignificance::SystemFatal, EventFlavor::SystemError),
			EventClass::Preset::unexpectedBehavior = EventClass(EventSignificance::ClientFatal, EventFlavor::SystemError),
			EventClass::Preset::common = EventClass(EventSignificance::Common),
			EventClass::Preset::debug = EventClass(EventSignificance::Debug),
			EventClass::Preset::factionDeath = EventClass(EventSignificance::FactionFatal, EventFlavor::Loss),
			EventClass::Preset::commandRejected = EventClass(EventSignificance::NonFatalError,EventFlavor::UserError),
			EventClass::Preset::operationAborted = EventClass(EventSignificance::NonFatalError,EventFlavor::Loss),
			EventClass::Preset::configuration = EventClass(EventSignificance::Common,EventFlavor::Neutral),
			EventClass::Preset::configurationPrint = EventClass(EventSignificance::Uncommon,EventFlavor::ServerPrivate),
			EventClass::Preset::configurationError = EventClass(EventSignificance::NonFatalError,EventFlavor::UserError),
			EventClass::Preset::fileError = EventClass(EventSignificance::NonFatalError,EventFlavor::SystemError),
			EventClass::Preset::network = EventClass(EventSignificance::Uncommon,EventFlavor::ServerPrivate),
			EventClass::Preset::networkError = EventClass(EventSignificance::NonFatalError,EventFlavor::ServerPrivate),
			EventClass::Preset::remoteControlResponse = EventClass(EventSignificance::Common,EventFlavor::ServerPrivate),
			EventClass::Preset::remoteControlCommand = EventClass(EventSignificance::Uncommon,EventFlavor::ServerPrivate),
			EventClass::Preset::remoteControlError = EventClass(EventSignificance::NonFatalError,EventFlavor::ServerPrivate),
			EventClass::Preset::important = EventClass(EventSignificance::Important),
			EventClass::Preset::uncommon = EventClass(EventSignificance::Uncommon);

namespace Logging
{

	String		TimeStamp(bool filename/*=false*/)
	{
		time_t tt = time(NULL);
		const tm*t = localtime(&tt);
		char output[256];
		size_t len = strftime(output,sizeof(output),filename ? "%Y %m %d %H%M%S" : "%Y %m %d. %H:%M:%S",t);
		return String(output,len);
	}


	SharedLogFile outFile;


	#ifndef HAS_WINDOW
	class ConsoleThread : public ThreadObject
	{
		volatile bool doEnd;
	public:
		WorkPipe<String> pipe;

		virtual void ThreadMain() override
		{
			while (!doEnd)
			{
				pipe.WaitForContent();
				if (doEnd)
					return;
				pipe.SignalRead();
					foreach (pipe,line)
					{
						std::cout << *line;
					}
				pipe.ExitRead();
			}
		}

		void	End()
		{
			doEnd = true;
			pipe.Clear();
			pipe.OverrideSignalNow();
			awaitCompletion();
		}

		void	Begin()
		{
			doEnd = false;
			start();
		}

	}	consoleThread;
	#endif


	//HANDLE	fileHandle = INVALID_HANDLE_VALUE;

	void	End()
	{
		#ifndef HAS_WINDOW
		consoleThread.End();
		#endif
		outFile.Close();
	}


	void	Begin(const PathString&path)
	{
		FileSystem::Folder folder(FileSystem::ExtractFileDir(path));
		folder.Rewind();
		FileSystem::File file;
		PathString ext = FileSystem::ExtractFileExt(path);
		typedef std::pair<FileSystem::ftime_t,PathString>  Entry;
		Buffer0<Entry>	entries;
		while (folder.NextFile(ext,file))
		{
			entries << std::make_pair(file.GetModificationTime(), file.GetLocation());
		}
		if (entries.Count() > 10)
		{
			ByComparator::QuickSort(entries,[](const Entry&a, const Entry&b){return a.first < b.first ? -1 : (a.first == b.first ? 0 : 1);});
			for (index_t i = 0; i+10 <= entries.Count(); i++)
			{
				FileSystem::UnlinkFile(entries[i].second);
			}
		}



		if (!outFile.Open(path))
		{
			LogEvent(EventClass::Preset::error , getLastError());
		}
		
		#ifndef HAS_WINDOW
		consoleThread.Begin();
		#endif
		LogEvent(EventClass::Preset::uncommon,"Logging to '"+String(path)+"'..." );
	}




	class LogBuffer
	{
	public:
		StringBuffer buffer;
		
		LogBuffer&	operator<<(const char*line)
		{
			buffer.Write(line,strlen(line));
			return *this;
		}
		LogBuffer&	operator<<(const String&line)
		{
			buffer.Write(line.c_str(),line.length());
			return *this;
		}
		LogBuffer&	operator<<(char c)
		{
			buffer.Write(&c,1);
			return *this;
		}
		LogBuffer&	operator<<(StrStream::TNewLine nl)
		{
			return operator<<('\n');
		}
	}	logFile,console;

	void	Flush(LogBuffer&buffer, bool toSyslog)
	{
		outFile.WriteAndClear(buffer.buffer,toSyslog);
	}



	void WriteEvent( const EventClass&evClass, const String&message, TCP::Peer*peer, const Objects::WorldEntity*e, const Objects::OwnerParameter&owner)
	{
		bool printToConsole = evClass.significance >= EventSignificance::Uncommon;
		//type != EventSignificance::Debug
		//					&& type != EventSignificance::Common
		//					&& type != EventSignificance::PerformanceMeasurement
		//					&& type != EventClass::Preset::remoteControlResponse;

		if (inTestMode)
		{
			printToConsole = evClass.significance >= EventSignificance::NonFatalError;
			if (!printToConsole)
				return;
		}

		static SpinLock lock;

		lock.lock();

		static bool headerWritten = false;
		if (!headerWritten)
		{
			headerWritten = true;
			if (!inTestMode)
				logFile << "Sig."<< tab << "Time" << tab << "Round"<<tab<< "Phase"<<tab<<  "ClientName"<<tab<<"IP"<<tab<<"EntityType"<<tab<<"EntityID"<<tab<<"OwnerID"<<tab<<"Location"<<tab<<"Message"<<nl;
		}
		if (printToConsole)
			console << '[' << RuntimeConfig::roundNumber.ToString()<<"] ";
		if (!inTestMode)
			logFile << evClass.significance.ToString()  << tab << TimeStamp() << tab << RuntimeConfig::roundNumber.ToString() << tab << RuntimeConfig::phase.ToString();

		if (peer)
		{
			#ifndef CLIENT
				Network::PClient c = To<Network::Client>::Cast(peer->attachment);
				if (c)
				{
					if (printToConsole)
						console << c->ToString()<<" ";
					if (!inTestMode)
						logFile << tab << c->ToString();
				}
				else
			#endif
				if (!inTestMode)
					logFile << tab;
			if (printToConsole)
				console << peer->ToString()<< ": ";
			if (!inTestMode)
				logFile << tab << peer->ToString();
		}
		else
			if (!inTestMode)
				logFile << tab << tab;
		if (e)
		{
			if (printToConsole)
				console << e->GetName()<<" ("<<e->location.ToString()<<"): ";
			if (!inTestMode)
				logFile << tab << e->GetTypeName() << tab << e->id.ToString() << tab << e->owner->GetLogName()<<tab<<e->location.ToString();
		}
		else
			if (!inTestMode)
				if (owner)
					logFile << tab << tab<<tab<<owner->GetLogName() <<tab;
				else
					logFile << tab << tab<<tab<<tab;

		if (message.contains('\n') || message.contains('\r'))
		{
			String copy = message;
			copy.replace('\n',' ');
			copy.replace('\r',"");
			if (!inTestMode)
				logFile << tab << copy << nl;
		}
		else
			if (!inTestMode)
				logFile << tab << message << nl;
		if (printToConsole)
			console << message << nl;
		if (!inTestMode)
			Flush(logFile,printToConsole);
		if (printToConsole)
		{
			#ifdef HAS_WINDOW
				VisualLogEvent(evClass,e,message);
			#else
				consoleThread.pipe << String(console.buffer.ToStringRef());
			#endif
			console.buffer.Clear();
		}
		lock.unlock();
	}


}



void LogEvent( const EventClass&type, const Objects::WorldEntity*e, const String&msg)
{
	#ifndef CLIENT
		Logging::SendEntityEvent(nullptr,type,e,Objects::OwnerParameter(),msg);
	#endif
	Logging::WriteEvent(type,msg,nullptr,e,Objects::OwnerParameter());
}

void		LogEvent(const EventClass&type, const Objects::OwnerParameter&owner, const String&msg)
{
	#ifndef CLIENT
		Logging::SendEntityEvent(nullptr,type,nullptr,owner,msg);
	#endif
	Logging::WriteEvent(type,msg,nullptr,nullptr,owner);
}




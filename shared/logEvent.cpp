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
#include <io/monitoredProcess.h>

#ifndef CLIENT
	#include "../server/components/state.h"
	#include "../server/components/serverLogEvent.h"
#else
	#include "../client/components/visual/hud/eventLog.h"
#endif

bool		inTestMode = false;


EventClass	EventClass::Preset::error = EventClass(EventSignificance::NonFatalError),
			EventClass::Preset::internalError = EventClass(EventSignificance::NonFatalError,EventFlavor::ServerPrivate),
			EventClass::Preset::fatal = EventClass(EventSignificance::SystemFatal, EventFlavor::SystemError),
			EventClass::Preset::unexpectedBehavior = EventClass(EventSignificance::ClientFatal, EventFlavor::SystemError),
			EventClass::Preset::common = EventClass(EventSignificance::Common),
			EventClass::Preset::debug = EventClass(EventSignificance::Debug),
			EventClass::Preset::factionDeath = EventClass(EventSignificance::FactionFatal, EventFlavor::Loss),
			EventClass::Preset::winningFaction = EventClass(EventSignificance::Important, EventFlavor::Gain),
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
			EventClass::Preset::gui = EventClass(EventSignificance::Common),
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


	SharedLogFile outFile;	//max 10MB


	#ifndef HAS_WINDOW
	class ConsoleThread : public Sys::ThreadObject
	{
		volatile bool doEnd;
	public:
		Ctr::WorkPipe<String> pipe;

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


	PathString activePath;

	void	CallBugReporter()
	{
		MonitoredProcess	process(false);
		process.Start("../BugReporter/","BugReporter.exe","\""+activePath+"\"");	//start and abandon
	}


	void	Begin(const PathString&path)
	{
		activePath = path;
		FileSystem::Folder folder(FileSystem::ExtractFileDir(path));
		folder.Rewind();
		FileSystem::File file;
		PathString ext = FileSystem::ExtractFileExt(path);
		typedef std::pair<FileSystem::ftime_t,PathString>  Entry;
		Ctr::Buffer0<Entry>	entries;
		while (folder.NextFile(ext,file))
		{
			entries << std::make_pair(file.GetModificationTime(), file.GetLocation());
		}
		if (entries.Count() > 10)
		{
			Sorting::ByComparator::QuickSort(entries,[](const Entry&a, const Entry&b){return a.first < b.first ? -1 : (a.first == b.first ? 0 : 1);});
			for (index_t i = 0; i+10 <= entries.Count(); i++)
			{
				FileSystem::UnlinkFile(entries[i].second);
			}
		}



		if (!outFile.Open(path,10*1024*1024))
		{
			LogEvent(EventClass::Preset::error , Sys::getLastError());
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
			buffer.Write(line.c_str(),line.GetLength());
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
		
		template <typename T>
			LogBuffer&	operator<<(const T&item)
			{
				buffer << ToString(item);
				return *this;
			}

	}	logFile,console;

	void	Flush(LogBuffer&buffer, bool toSyslog)
	{
		outFile.WriteAndClear(buffer.buffer,toSyslog);
	}



	void	WriteEvent(const EventClass&evClass, String message, TCP::Peer*peer, const Objects::WorldEntity*e, const Objects::OwnerParameter&owner, Origin origin /*= Logging::Origin::Local*/)
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

		static Sys::SpinLock lock;

		lock.lock();

		static bool headerWritten = false;
		if (!headerWritten)
		{
			headerWritten = true;
			if (!inTestMode)
			{
				//if you change this, make sure to update bug reporter
				logFile << "Sig."<< tab << "Time" << tab << "Round"<<tab<< "Phase"<<tab<<  "ClientName"<<tab<<"IP"<<tab<<"EntityType"<<tab<<"EntityID"<<tab<<"OwnerID"<<tab<<"Location"<<tab<<"Message"<<nl;
			}
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
						console << c->ConvertToString()<<" ";
					if (!inTestMode)
						logFile << tab << c->ConvertToString();
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
				console << e->GetName()<<" ("<<e->location<<"): ";
			if (!inTestMode)
				logFile << tab << e->GetTypeName() << tab << ToString(e->id) << tab << e->owner->GetLogName()<<tab<<e->location;
		}
		else
			if (!inTestMode)
				if (owner)
					logFile << tab << tab<<tab<<owner->GetLogName() <<tab;
				else
					logFile << tab << tab<<tab<<tab;



		if (message.Contains('\n') || message.Contains('\r'))
		{
			String copy = message;
			message.FindAndReplace('\n',' ');
			message.FindAndReplace('\r',"");
		}
		String logConsoleMsg = message;
		switch (origin)
		{
			case Logging::Origin::Local:
			break;
			case Logging::Origin::ServerPublic:
				logConsoleMsg = ">" +logConsoleMsg;
			break;
			case Logging::Origin::ServerPrivate:
				logConsoleMsg = ">[private]" +logConsoleMsg;
			break;
		}


		if (!inTestMode)
			logFile << tab << logConsoleMsg << nl;
		if (printToConsole)
			console << logConsoleMsg << nl;
		if (!inTestMode)
			Flush(logFile,printToConsole);
		if (printToConsole)
		{
			#ifdef HAS_WINDOW
				HUD::VisualEventLog::DisplayEvent(evClass,e,owner, message,origin);
			#else
				consoleThread.pipe << String(console.buffer.ToStringRef());
			#endif
			console.buffer.Clear();
		}
		lock.unlock();
	}


}

void		LogException(const Objects::WorldEntity*e, const std::exception&ex)
{
	LogEvent(EventClass::Preset::internalError,e,ex.what());
}



void LogEvent( const EventClass&type, const Objects::WorldEntity*e, const String&msg, Logging::Origin origin, Logging::Privacy privacy/*=Logging::Privacy::Auto*/)
{
	#ifndef CLIENT
		Logging::SendEntityEvent(nullptr,type,e,Objects::OwnerParameter(),msg,privacy);
	#endif
	Logging::WriteEvent(type,msg,nullptr,e,Objects::OwnerParameter(),origin);
}

void		LogEvent(const EventClass&type, const Objects::OwnerParameter&owner, const String&msg, Logging::Origin origin, Logging::Privacy privacy/*=Logging::Privacy::Auto*/)
{
	#ifndef CLIENT
		Logging::SendEntityEvent(nullptr,type,nullptr,owner,msg,privacy);
	#endif
	Logging::WriteEvent(type,msg,nullptr,nullptr,owner, origin);
}




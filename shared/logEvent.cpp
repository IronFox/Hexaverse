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

#ifndef CLIENT
	#include "../server/components/state.h"
	#include "../server/components/serverLogEvent.h"
#endif

#ifdef HAS_WINDOW
	void VisualLogEvent( EventType type,  const Objects::WorldEntity*e, const String&_event);
#endif

bool		inTestMode = false;

namespace Logging
{

	String		TimeStamp(bool filename/*=false*/)
	{
		time_t tt = time(NULL);
		const tm*t = localtime(&tt);
		char output[256];
		size_t len = strftime(output,sizeof(output),filename ? "%Y %B %d %H%M%S" : "%Y %B %d. %H:%M:%S",t);
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
		if (!outFile.Open(path))
		{
			LogEvent(EventType::InternalFault,getLastError());
		}
		
		#ifndef HAS_WINDOW
		consoleThread.Begin();
		#endif
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



	void WriteEvent( EventType type, const String&message, TCP::Peer*peer, const Objects::WorldEntity*e)
	{
		bool printToConsole = type != EventType::Debug && type != EventType::MinorEntityProcess
							&& type != EventType::PerformanceMeasurement;

		if (inTestMode)
		{
			printToConsole = type == EventType::InternalFault;
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
				logFile << "Type" << tab << "Time" << tab << "Round"<<tab<< "Phase"<<tab<<  "ClientName"<<tab<<"IP"<<tab<<"EntityType"<<tab<<"EntityID"<<tab<<"OwnerID"<<tab<<"Location"<<tab<<"Message"<<nl;
		}
		if (printToConsole)
			console << '[' << String(RuntimeConfig::roundNumber)<<"] ";
		if (!inTestMode)
			logFile << type.ToString() << tab << TimeStamp() << tab << String(RuntimeConfig::roundNumber) << tab << RuntimeConfig::phase.ToString();

		if (peer)
		{
			#ifndef CLIENT
				State::PClient c = To<State::Client>::Cast(peer->attachment);
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
			Flush(logFile,IsImportant(type));
		if (printToConsole)
		{
			#ifdef HAS_WINDOW
				VisualLogEvent(type,e,message);
			#else
				consoleThread.pipe << String(console.buffer.ToStringRef());
			#endif
			console.buffer.Clear();
		}
		lock.unlock();
	}


}



void LogEvent( EventType type, const Objects::WorldEntity*e, const String&msg)
{
	#ifndef CLIENT
		Logging::SendEntityEvent(nullptr,type,e,msg);
	#endif
	Logging::WriteEvent(type,msg,nullptr,e);
}





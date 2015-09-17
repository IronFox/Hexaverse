#include <global_root.h>
#include "serverLogEvent.h"
#include <io/log.h>
#include <general/system.h>
#include "state.h"
#include "networkChannels.h"

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


	StringBuffer buffer;


	HANDLE	fileHandle = INVALID_HANDLE_VALUE;

	void	End()
	{
		if (fileHandle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(fileHandle);
			fileHandle = INVALID_HANDLE_VALUE;
		}

	}


	void	Begin(const String&path)
	{
		End();
		fileHandle = CreateFileA(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
		if (fileHandle == INVALID_HANDLE_VALUE)
		{
			LogEvent(EventType::InternalFault,getLastError());
		}
	}

	void	Write(const char*ch, size_t length)
	{
		buffer.Write(ch,length);
		//DWORD at;
		//WriteFile(fileHandle,ch,(DWORD)length,&at,NULL);
	}

	void	Flush()
	{
		DWORD at;
		WriteFile(fileHandle,buffer.pointer(),(DWORD)buffer.GetLength(),&at,NULL);
		buffer.Clear();
	}


	class LogFile
	{
	public:
		const LogFile&	operator<<(const char*line) const
		{
			Write(line,strlen(line));
			return *this;
		}
		const LogFile&	operator<<(const String&line) const
		{
			Write(line.c_str(),line.length());
			return *this;
		}
		const LogFile&	operator<<(char c) const
		{
			Write(&c,1);
			return *this;
		}
		const LogFile&	operator<<(const StrStream::TNewLine&nl) const
		{
			return operator<<('\n');
		}
	}	logFile;




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

		static bool headerWritten = false;
		if (!headerWritten)
		{
			headerWritten = true;
			if (!inTestMode)
				logFile << "Type" << tab << "Time" << tab << "Round"<<tab<< "Phase"<<tab<<  "ClientName"<<tab<<"IP"<<tab<<"EntityType"<<tab<<"EntityID"<<tab<<"OwnerID"<<tab<<"Location"<<tab<<"Message"<<nl;
		}
		if (printToConsole)
			std::cout << '[' << State::roundNumber<<"] ";
		if (!inTestMode)
			logFile << type.ToString() << tab << TimeStamp() << tab << String(State::roundNumber) << tab << State::phase.ToString();

		if (peer)
		{
			State::PClient c = To<State::Client>::Cast(peer->attachment);
			if (c)
			{
				if (printToConsole)
					std::cout << c->ToString()<<" ";
				if (!inTestMode)
					logFile << tab << c->ToString();
			}
			else
				if (!inTestMode)
					logFile << tab;
			if (printToConsole)
				std::cout << peer->ToString()<< ": ";
			if (!inTestMode)
				logFile << tab << peer->ToString();
		}
		else
			if (!inTestMode)
				logFile << tab << tab;
		if (e)
		{
			if (printToConsole)
				std::cout << e->GetName()<<" ("<<e->location.ToString()<<"): ";
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
			std::cout << message << std::endl;
		if (!inTestMode)
		{
			Flush();
			//logFile.flush();
		}
	}


	void SendEntityEvent( TCP::Peer*toPeer,EventType type, const Objects::WorldEntity*e, const String&msg)
	{
		if (type == EventType::PerformanceMeasurement ||
			type == EventType::Debug || type == EventType::MinorEntityProcess || !State::server.IsOnline())
			return;
		if (!toPeer && e)
		{
			State::PClient*cl = State::playerMap.QueryPointer(e->owner);
			if (cl)
				toPeer = (*cl)->peer.lock().get();
			else
				return;	//still private
		}


		Protocol::ControlMessage out;

		if (e)
			out.message = e->GetNiceName()+": "+msg;
		else
			out.message = msg;
		out.type = (BYTE)type;

		if (toPeer)
		{
			out.message = "[private] "+out.message;
			Channels::controlMessage.SendTo(*toPeer,out);
		}
		else
			Channels::controlMessage.SendTo(State::server,out);
	}
}

void LogEvent(EventType t, TCP::Peer&peer, const String&msg)
{
	Logging::SendEntityEvent(&peer,t,nullptr,msg);

	Logging::WriteEvent(t,msg,&peer,nullptr);
}

void LogEvent(EventType t, TCP::Peer&peer, const Objects::PWorldEntity&e, const String&msg)
{
	Logging::SendEntityEvent(&peer,t,e.get(),msg);
	Logging::WriteEvent(t,msg,&peer,e.get());
}


void LogEvent( EventType type, const Objects::WorldEntity*e, const String&msg)
{
	Logging::SendEntityEvent(nullptr,type,e,msg);
	Logging::WriteEvent(type,msg,nullptr,e);
}





// RemoteControlConsole.cpp : Defines the entry point for the console application.
//

#include <global_root.h>
#include <net/tcp.h>
#include <net/containers.h>
#include <string/string_buffer.h>
#include <mutex>
#include <conio.h>
#include <general/timer.h>
#include "../shared/protocol.h"
#include <serverProcess.h>
#include <container/production_pipe.h>

using namespace DeltaWorks;

HANDLE mainThreadHandle = nullptr;
Observable<> onProgramEnd;

BOOL WINAPI MyHandlerRoutine(_In_  DWORD dwCtrlType)
{
	switch (dwCtrlType)
	{
		case CTRL_C_EVENT:
		case CTRL_CLOSE_EVENT:
		case CTRL_SHUTDOWN_EVENT:
			application_shutting_down = true;
			onProgramEnd();
			TerminateProcess(GetCurrentProcess(),0);
		return TRUE;
	
	}
	return FALSE;
}

void	Dispatch(const StringRef&);

String targetHost = "localhost";
Buffer0<String>	history;
index_t historyAt=InvalidIndex;

Timer::Time connectionAttemptStarted = timer.Now();
std::thread connectionRetryThread;
void ConnectionRetry();
void CompleteInput();


struct TUpdate
{
	enum type_t
	{
		LogLine,
		NewContext,
		SetInput,
		AppendToInput,
		MoveHistoryUp,
		MoveHistoryDown
	};

	type_t		type = LogLine;
	String		content;

	void swap(TUpdate&other)
	{
		std::swap(type,other.type);
		content.swap(other.content);
	}
};

WorkPipe<TUpdate,Swap>	updates;



namespace Console
{
	//std::recursive_mutex	mutex;
	//StringBuffer	input;
	String	context;
	StringBuffer	input;
	String	lastLine;
	HANDLE handle;
	COORD lastCursorPosition;

	void ScanCursorPosition(bool expectChange = false)
	{
		CONSOLE_SCREEN_BUFFER_INFO info;
		BOOL success = GetConsoleScreenBufferInfo(handle,&info);
		if (!success)
			ShowMessage(Sys::GetLastErrorString());

		if (info.dwCursorPosition.X > 0 && info.dwCursorPosition.Y >= 0)
		{
			if (expectChange && info.dwCursorPosition.X == lastCursorPosition.X)
			{
				ShowMessage("Expected change in cursor position");
			}
			lastCursorPosition = info.dwCursorPosition;
		}
		else
		{
			bool brk = true;
		}
	}

	void RestoreCursorPosition()
	{
		BOOL success = SetConsoleCursorPosition(handle,lastCursorPosition);
		if (!success)
			ShowMessage(Sys::GetLastErrorString());
		bool brk = true;
	}

	StringRef Truncate(const StringRef&ref, count_t toChars)
	{
		if (ref.GetLength() <= toChars)
			return ref;
		return StringRef(ref.pointer() + (ref.GetLength() - toChars),toChars);
	}

	static void ClearLine()
	{
		printf("\r");
		for (index_t i = 0; i < lastLine.GetLength(); i++)
			if (lastLine[i] != '\t')
				printf(" ");
			else
				printf("\t");
		printf("\r");
	}

	
	String AssembleLine()
	{
		String rs = Truncate(context.ref(),30);
		rs += '>';
		rs += Truncate(input.ToStringRef(),Sys::getConsoleWidth() - rs.Length());
		return rs;
	}

	void	_UpdateLastLine()
	{
		lastLine = AssembleLine();
	}

	
	void	AddChar(char c)
	{
		bool refresh = true;
		switch (c)
		{
			case '\b':
				if (input.IsNotEmpty())
				{
					input.EraseLast();
				}
			break;
			case '\t':
			{
				CompleteInput();
			}
			break;
			case '\n':
			case '\r':
				//std::cout << "\n" + lastLine;
				Dispatch(input.ToStringRef());
				input.Clear();
			break;
			default:
				if (Package::IsValidChar(c))
				{
					input << c;
				}
				else
					return;
		}
	}
}

namespace Handler
{
	bool	interactive = true;

	void	OnContext(Package::NetString<1024>&ctx)
	{

		if (interactive)
		{
			TUpdate update;
			update.content.adoptData(ctx);
			update.type = TUpdate::NewContext;
			updates << update;
		}
		else
			std::cout << '='<<ctx<<std::endl;
	}


	void	OnResponse(Package::NetString<1024>&output)
	{
		if (interactive)
		{
			TUpdate update;
			update.content.adoptData(output);
			update.type = TUpdate::LogLine;
			updates << update;
		}
		else
			std::cout << '+'<<output<<std::endl;
	}

	void	OnError(Package::NetString<1024>&error)
	{
		if (interactive)
		{
			TUpdate update;
			update.content = "ERROR: "+error;
			update.type = TUpdate::LogLine;
			updates << update;
		}
		else
			std::cout << '-'<<error<<std::endl;;
	}

	void	OnComplete(Package::NetString<1024>&input)
	{
		TUpdate update;
		update.content.adoptData(input);
		update.type = TUpdate::SetInput;
		updates << update;
	}

}

namespace Channels
{
	namespace ID
	{
		enum id_t
		{
			Command,	//c2s
			Complete,	//both : string
			Error,		//s2c : string
			Context,	//s2c : string
			Response,	//s2c : string
			ExecutionDone,	//s2c : <signal>
		};
	}


	TCP::ObjectSender<Package::NetString<1024>,ID::Command>		command;
	TCP::Channel<Package::NetString<1024>,0,ID::Complete>		complete(Handler::OnComplete);
	TCP::Channel<Package::NetString<1024>,0,ID::Error>		error(Handler::OnError);
	TCP::Channel<Package::NetString<1024>,0,ID::Context>	context(Handler::OnContext);
	TCP::Channel<Package::NetString<1024>,0,ID::Response>	response(Handler::OnResponse);

}


TCP::Client client;
Buffer0<String>	bufferedCommands;
std::mutex	bufferedCommandsMutex;


void	CompleteInput()
{
	StringRef line = Console::input.ToStringRef();
	//if (line.length() >= 2 && line.beginsWith("ex"))
	//	Console::SetInput("exit");
	//else
	{
		if (client.IsConnected())
		{
			Channels::complete.SendTo(client,line);
		}
	}
}

static void LogLine(const String&line)
{
	TUpdate update;
	update.content = line;
	update.type = TUpdate::LogLine;
	updates << update;
}

void	Dispatch(const StringRef&line)
{
	if (line == "exit")
	{
		application_shutting_down = true;
		return;
	}
	history.FindAndErase(line);
	history << line;
	historyAt = history.Count();

	//Console::EchoCurrent();
	if (client.IsConnected())
	{
		Channels::command.SendTo(client,line);
	}
	else
	{
		if (Handler::interactive)
		{
			LogLine("Unable to execute command. Not connected");
		}
		else
		{
			bufferedCommandsMutex.lock();
			bufferedCommands << line;
			bufferedCommandsMutex.unlock();
		}
	}
}




void OnDeserializationFailed(UINT32 channel, TCP::Peer&peer, const String&exceptionMessage)
{
	LogLine(peer.ToString()+": Deserialization failed on channel "+String(channel)+" ("+exceptionMessage+")");
}


Sys::Event	exitLock;

namespace ParameterConfig
{
	UINT16 basePort = DEFAULT_SERVER_BASE_PORT;
	
	enum mode_t
	{
		Idle,
		ExpectPort,
		ReadCommand

	}		mode = Idle;

	const char*program = "";
	StringBuffer	command;
	bool	interactive = true,
				server = false;


	void	Process(const char*arg)
	{
		//Console::WriteLine("Parameter: "+String(arg));
		switch (mode)
		{
			case ReadCommand:
				command << ' '<<arg;
			return;
			case Idle:
				if (*arg == '-')
				{
					if (!strcmp(arg,"--port") || !strcmp(arg,"-p"))
						mode = ExpectPort;
					elif (!strcmp(arg,"--command") || !strcmp(arg,"-c"))
					{
						mode = ReadCommand;
						Handler::interactive = interactive = false;
						if (server)
							std::cerr << "--command and --server are incompatible. Ignoring --server"<<std::endl;
						server = false;
					}
					elif (!strcmp(arg,"--server") || !strcmp(arg,"-s"))
					{
						if (!interactive)
							std::cerr << "--command and --server are incompatible. Ignoring --server"<<std::endl;
						server = interactive;
					}
					elif (!strcmp(arg,"--help") || !strcmp(arg,"-h"))
					{
						std::cout << "usage: "<<program<<" [-p basePort] [[-c] command(s)]"<<std::endl;
						std::cout << " -p/--port [port]\tUse a non-default base port"<<std::endl;
						std::cout << " -c/--command [cmd]\tExecute command"<<std::endl;
						std::cout << " -s/--server\tStart server and bind to the lifetime of this console"<<std::endl;
						std::cout << " *\t\tExecute command"<<std::endl;
						std::cout << " [no parameters]\tStart in interactive mode"<<std::endl;
					}
					else
						std::cerr << "Unsupported parameter: "<<arg<<std::endl;

				}
				else
				{
					command << arg;
					mode = ReadCommand;
					Handler::interactive = interactive = false;
					if (server)
						std::cerr << "Direct commands and --server are incompatible. Ignoring --server"<<std::endl;
					server = false;
				}
			return;
			case ExpectPort:
				if (!convert(arg,basePort))
				{
					LogLine("Parameters: Unable to convert '"+String(arg)+"' to a valid port number");
				}
				else
				{
					//Network::Config::sessionPort = basePort + PortOffset::Session;
					LogLine("Parameters: Base port redefined as "+String(basePort));
					//LogEvent(EventType::ConfigRelated,"Base port redefined as "+String(basePort)+", session port set to "+String(Network::Config::sessionPort));
				}
				mode = Idle;
			return;
		}
	}


}

void OnEvent(TCP::event_t event, TCP::Peer&peer)
{
	switch (event)
	{
		case TCP::Event::ConnectionFailed:
		case TCP::Event::ConnectionLost:
			if (Handler::interactive)
			{
				LogLine(peer.ToString()+": "+TCP::event2str(event));
				if (!connectionRetryThread.joinable())
				{
					connectionRetryThread.swap(std::thread(ConnectionRetry));
				}
			}
			else
				exitLock.signal();
		return;
		case TCP::Event::ConnectionEstablished:
			if (Handler::interactive)
			{
				LogLine("Connection established to "+peer.ToString()+" after "+String(timer.GetSecondsSince(connectionAttemptStarted)));
				if (Console::context.IsNotEmpty())
					Channels::command.SendTo(peer,"cd /"+Console::context);
			}
			if (connectionRetryThread.joinable())
			{
				try
				{
					connectionRetryThread.join();
				}
				catch (...)
				{}
			}
			//if (Handler::interactive)
				//Console::WriteLine(peer.ToString()+": "+TCP::event2str(event));
			bufferedCommandsMutex.lock();
			{
				for (index_t i = 0; i < bufferedCommands.Count(); i++)
				{
					Channels::command.SendTo(client,bufferedCommands[i]);
				}
			}
			bufferedCommandsMutex.unlock();
		return;
	}
	LogLine(peer.ToString()+": "+TCP::event2str(event));
}


void OnIgnorePackage(UINT32 channel, UINT32 size, TCP::Peer&peer)
{
	LogLine(peer.ToString()+": Ignoring package of size "+String(size)+" on channel "+String(channel));
}

void OnSignal(UINT32 signal, TCP::Peer&peer)
{
	if (signal == Channels::ID::ExecutionDone)
	{
		if (!Handler::interactive)
		{
			exitLock.signal();
			return;
		}

	}
	else
		LogLine(peer.ToString()+": Received signal on channel "+String(signal));

}

void ConnectionRetry()
{
	while (!client.IsConnected())
	{
		Sleep(1000);
		if (!client.IsAttemptingToConnect() && !client.IsConnected())
		{
			connectionAttemptStarted = timer.Now();
			client.ConnectAsync(targetHost+":"+String(ParameterConfig::basePort+PortOffset::Control));
		}
	}
}



class InputReader : public Sys::ThreadObject
{
public:
	virtual void ThreadMain() override
	{
		while (!application_shutting_down)
		{
			int ch = _getch();
			if (ch == 224 || ch == 0)
			{
				int next = _getch();
				switch(next)
				{
					case 72:
					{
						TUpdate update;
						update.type = TUpdate::MoveHistoryUp;
						updates << update;
					}
					break;
					case 80:
					{
						TUpdate update;
						update.type = TUpdate::MoveHistoryDown;
						updates << update;
					}
					break;
					case 75:
						//left
					break;
					case 77:
						//right
					break;
				}
			}
			else
			{
				char c = ((char)(BYTE)(unsigned)ch);
				TUpdate update;
				update.content = c;
				update.type = TUpdate::AppendToInput;
				updates << update;
			}
		}
	}

};

InputReader	readThread;


int main(int argc, char* argv[])
{
	
	SetConsoleCtrlHandler(MyHandlerRoutine,TRUE);
	Sys::DisableConsoleQuickEdit();
	Console::handle = GetStdHandle(STD_OUTPUT_HANDLE);
	ASSERT__(DuplicateHandle(GetCurrentProcess(),GetCurrentThread(),GetCurrentProcess(),&mainThreadHandle,DUPLICATE_SAME_ACCESS,FALSE,0));

	ParameterConfig::program = argc > 0 ? argv[0] : "";
	for (int i = 1; i < argc; i++)
		ParameterConfig::Process(argv[i]);


	
	client.InstallChannel(Channels::error);
	client.InstallChannel(Channels::context);
	client.InstallChannel(Channels::response);
	client.InstallChannel(Channels::complete);
	client.OpenSignalChannel(Channels::ID::ExecutionDone,0);

	client.onDeserializationFailed = OnDeserializationFailed;
	client.onEvent = OnEvent;
	client.onIgnorePackage = OnIgnorePackage;
	client.onSignal = OnSignal;

	if (!ParameterConfig::interactive && ParameterConfig::command.IsEmpty())
		return 0;


	if (ParameterConfig::server)
	{
		ServerProcess::Setup(onProgramEnd,nullptr,LogLine,LogLine,LogLine);
		ServerProcess::SetOnEndAsync([]()
		{
			application_shutting_down = true;
			onProgramEnd();
			TerminateProcess(GetCurrentProcess(),0);
		});
		ServerProcess::Begin(true,false,"",ParameterConfig::basePort);
	}

	connectionAttemptStarted = timer.Now();
	client.ConnectAsync(targetHost+":"+String(ParameterConfig::basePort+PortOffset::Control));
	std::cout << std::endl;


	if (ParameterConfig::interactive)
	{
		readThread.Start();
		//Console::Refresh();
		Console::_UpdateLastLine();
		std::cout << Console::lastLine;
		Console::ScanCursorPosition();

		Buffer0<String>	newLines;
		while (!application_shutting_down)
		{
			bool contextUpdated = false,inputChanged = false;
			updates.WaitForContent();
			updates.SignalRead();
				foreach (updates,up)
				{
					switch (up->type)
					{
						case TUpdate::LogLine:
							newLines.MoveAppend(up->content);
						break;
						case TUpdate::NewContext:
							Console::context.swap(up->content);
							contextUpdated = true;
						break;
						case TUpdate::SetInput:
							Console::input = up->content;
							inputChanged = true;
						break;
						case TUpdate::AppendToInput:
							foreach (up->content,c)
								Console::AddChar(*c);
							inputChanged = true;
						break;
						case TUpdate::MoveHistoryDown:
							if (historyAt < history.Count())
							{
								historyAt ++;
								Console::input = (historyAt < history.Count() ? history[historyAt] : "");
								inputChanged = true;
							}
						break;
						case TUpdate::MoveHistoryUp:
							if (historyAt && history.IsNotEmpty())
							{
								historyAt--;

								Console::input = (history[historyAt]);
								inputChanged = true;
							}
							break;

					}
				}
			updates.ExitRead();

			if (newLines.IsNotEmpty())
			{
				Console::RestoreCursorPosition();
				Console::ClearLine();
				foreach (newLines,line)
					printf("%s\n",line->c_str());
				newLines.Clear();
				if (inputChanged || contextUpdated)
					Console::_UpdateLastLine();
				printf("%s",Console::lastLine.c_str());
				Console::ScanCursorPosition();
			}
			else
			{
				if (inputChanged || contextUpdated)
				{
					Console::RestoreCursorPosition();
					const String newLine = Console::AssembleLine();
					index_t match = 0;
					while (match < Console::lastLine.Length() && match < newLine.Length() && newLine[match] == Console::lastLine[match])
						match++;
					bool forceChange = false;
					if (match == 0)
					{
						Console::ClearLine();
						Console::lastLine = newLine;
						std::cout << Console::lastLine;
					}
					else
					{
						const count_t eraseOld = Console::lastLine.Length() - match;
						for (index_t i = 0; i < eraseOld; i++)
							printf("\b \b");
						printf("%s",newLine.GetPointer() + match);
						if (newLine.Length() != Console::lastLine.Length())
							forceChange = true;
						Console::lastLine = newLine;
					}
					Console::ScanCursorPosition(forceChange);
				}
			}


		}
		client.Disconnect();
	}
	else
	{
		Dispatch(ParameterConfig::command.ToStringRef());
		exitLock.wait();
		client.Destroy();
	}

	onProgramEnd();
	TerminateProcess(GetCurrentProcess(),0);
	return 0;
}


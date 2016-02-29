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


HANDLE mainThreadHandle = nullptr;

BOOL WINAPI MyHandlerRoutine(_In_  DWORD dwCtrlType)
{
	switch (dwCtrlType)
	{
		case CTRL_C_EVENT:
		case CTRL_CLOSE_EVENT:
		case CTRL_SHUTDOWN_EVENT:
			//TerminateNow(false);
			application_shutting_down = true;
			if (cancelSynchronousIo)
				cancelSynchronousIo(mainThreadHandle);
			else
				TerminateProcess(GetCurrentProcess(),0);
			//exit(0);
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

namespace Console
{
	std::recursive_mutex	mutex;
	StringBuffer	input;
	String	context;
	String lastLine;


	StringRef Truncate(const StringRef&ref, count_t toChars)
	{
		if (ref.GetLength() <= toChars)
			return ref;
		return StringRef(ref.pointer() + (ref.GetLength() - toChars),toChars);
	}

	static void ClearLine()
	{
		std::cout << '\r';
		for (index_t i = 0; i < lastLine.length(); i++)
			if (lastLine.get(i) != '\t')
				std::cout << ' ';
			else
				std::cout << '\t';
		std::cout << '\r';
	}


	void	WriteLine(const String&line)
	{
		mutex.lock();
			ClearLine();
			std::cout << line << std::endl;
			std::cout << lastLine;
		mutex.unlock();

	}

	//void	EchoCurrent()
	//{
	//	mutex.lock();
	//	WriteLine(lastLine);
	//	mutex.unlock();
	//}


	void	_UpdateLastLine()
	{
		lastLine = Truncate(context.ref(),30);
		lastLine += '>';
		lastLine += Truncate(input.ToStringRef(),getConsoleWidth() - lastLine.length());
	}

	void	Refresh()
	{
		mutex.lock();
			ClearLine();
			_UpdateLastLine();
			std::cout << lastLine;
		mutex.unlock();
	}

	void	SetInput(const String&line)
	{
		mutex.lock();
		input.Clear();
		input << line;
		Refresh();
		mutex.unlock();
	}
	void	AddChar(char c)
	{
		mutex.lock();
		bool refresh = true;
		switch (c)
		{
			case '\b':
				if (input.IsNotEmpty())
				{
					if (input.last() != '\t' && lastLine.length() +2 < (count_t)getConsoleWidth())
					{
						std::cout << "\b \b";
						refresh = false;
					}
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
				if (Package::NetString<1024>::IsValidChar(c))
				{
					input << c;
					if (lastLine.length()+1 < (count_t)getConsoleWidth())
					{
						std::cout << c;
						refresh = false;
					}
				}
				else
				{
					mutex.unlock();
					return;
				}
		}
		if (refresh)
			Refresh();
		else
			_UpdateLastLine();
		mutex.unlock();
	}
}

namespace Handler
{
	bool	interactive = true;

	void	OnContext(Package::NetString<1024>&ctx)
	{

		if (interactive)
		{
			Console::mutex.lock();
			if (Console::context != ctx)
			{
				Console::context = ctx;
				Console::Refresh();
			}
			Console::mutex.unlock();
		}
		else
			std::cout << '='<<ctx<<std::endl;
	}


	void	OnResponse(Package::NetString<1024>&output)
	{
		if (interactive)
			Console::WriteLine(output);
		else
			std::cout << '+'<<output<<std::endl;
	}

	void	OnError(Package::NetString<1024>&error)
	{
		if (interactive)
			Console::WriteLine("ERROR: "+error);
		else
			std::cout << '-'<<error<<std::endl;;
	}

	void	OnComplete(Package::NetString<1024>&error)
	{
		Console::SetInput(error);
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
			Console::WriteLine("Unable to execute command. Not connected");
		}
		else
		{
			bufferedCommandsMutex.lock();
			bufferedCommands << line;
			bufferedCommandsMutex.unlock();
		}
	}
}


void OnDeserializationFailed(UINT32 channel, TCP::Peer&peer)
{
	Console::WriteLine(peer.ToString()+": Deserialization failed on channel "+String(channel));
}


Event	exitLock;

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
	bool	interactive = true;


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
					}
					elif (!strcmp(arg,"--help") || !strcmp(arg,"-h"))
					{
						std::cout << "usage: "<<program<<" [-p basePort] [-i] [command(s)]"<<std::endl;
						std::cout << " -c/--command [cmd]\tExecute command"<<std::endl;
						std::cout << " -p/--port [port]\tUse a non-default base port"<<std::endl;
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
				}
			return;
			case ExpectPort:
				if (!convert(arg,basePort))
				{
					Console::WriteLine("Parameters: Unable to convert '"+String(arg)+"' to a valid port number");
				}
				else
				{
					//Network::Config::sessionPort = basePort + PortOffset::Session;
					Console::WriteLine("Parameters: Base port redefined as "+String(basePort));
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
		case TCP::Event::ConnectionClosed:
		case TCP::Event::ConnectionFailed:
		case TCP::Event::ConnectionLost:
			if (Handler::interactive)
			{
				Console::WriteLine(peer.ToString()+": "+TCP::event2str(event));
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
				Console::WriteLine("Connection established to "+peer.ToString()+" after "+String(timer.GetSecondsSince(connectionAttemptStarted)));
				if (Console::context.IsNotEmpty())
					Channels::command.SendTo(peer,"cd "+Console::context);
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
	Console::WriteLine(peer.ToString()+": "+TCP::event2str(event));
}


void OnIgnorePackage(UINT32 channel, UINT32 size, TCP::Peer&peer)
{
	Console::WriteLine(peer.ToString()+": Ignoring package of size "+String(size)+" on channel "+String(channel));
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
		Console::WriteLine(peer.ToString()+": Received signal on channel "+String(signal));

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





int main(int argc, char* argv[])
{
	
	SetConsoleCtrlHandler(MyHandlerRoutine,TRUE);

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
	connectionAttemptStarted = timer.Now();
	client.ConnectAsync(targetHost+":"+String(ParameterConfig::basePort+PortOffset::Control));
	std::cout << std::endl;

	if (ParameterConfig::interactive)
	{
		Console::Refresh();
		while (!application_shutting_down)
		{
			int ch = _getch();
			if (ch == 224 || ch == 0)
			{
				int next = _getch();
				switch(next)
				{
					case 72:
						if (historyAt)
						{
							historyAt--;
							Console::SetInput(history[historyAt]);
						}
					break;
					case 80:
						if (historyAt < history.Count())
						{
							historyAt ++;
							Console::SetInput(historyAt < history.Count() ? history[historyAt] : "");
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
				Console::AddChar((char)(BYTE)(unsigned)ch);
		}
		client.Disconnect();
	}
	else
	{
		Dispatch(ParameterConfig::command.ToStringRef());
		exitLock.wait();
		client.Destroy();
	}


	TerminateProcess(GetCurrentProcess(),0);
	return 0;
}


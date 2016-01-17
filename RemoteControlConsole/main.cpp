// RemoteControlConsole.cpp : Defines the entry point for the console application.
//

#include <global_root.h>
#include <net/tcp.h>
#include <net/containers.h>
#include <string/string_buffer.h>
#include <mutex>
#include "../shared/protocol.h"

BOOL WINAPI MyHandlerRoutine(_In_  DWORD dwCtrlType)
{
	switch (dwCtrlType)
	{
		case CTRL_C_EVENT:
		case CTRL_CLOSE_EVENT:
		case CTRL_SHUTDOWN_EVENT:
			//TerminateNow(false);
			application_shutting_down = true;
			exit(0);
		return TRUE;
	
	}
	return FALSE;
}

void	Dispatch(const StringRef&);

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
			std::cout << ' ';
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

	void	Refresh()
	{
		mutex.lock();
			ClearLine();
			lastLine = Truncate(context.ref(),30);
			lastLine += '>';
			lastLine += Truncate(input.ToStringRef(),40);
			std::cout << lastLine;
		mutex.unlock();
	}

	void	AddChar(char c)
	{
		mutex.lock();
		switch (c)
		{
			case 8:
				input.EraseLast();
			break;
			//case '\t':
			//{
			//	input << ' ';

			//	lastLine += ' ';
			//	while (lastLine.length()%8)
			//		lastLine += ' ';
			//	Refresh();
			//}
			//break;
			case '\n':
			case '\r':
				Dispatch(input.ToStringRef());
				input.Clear();
			break;
			default:
				if (Package::NetString<1024>::IsValidChar(c))
					input << c;
				else
				{
					mutex.unlock();
					return;
				}
		}
		Refresh();
		mutex.unlock();
	}
}

namespace Handler
{
	bool	interactive = false;

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

}

namespace Channels
{
	namespace ID
	{
		enum id_t
		{
			Command,	//c2s
			Error,		//s2c : string
			Context,	//s2c : string
			Response,	//s2c : string
			ExecutionDone,	//s2c : <signal>
		};
	}


	TCP::ObjectSender<Package::NetString<1024>,ID::Command>	command;
	TCP::Channel<Package::NetString<1024>,0,ID::Error>		error(Handler::OnError);
	TCP::Channel<Package::NetString<1024>,0,ID::Context>	context(Handler::OnContext);
	TCP::Channel<Package::NetString<1024>,0,ID::Response>	response(Handler::OnResponse);
}


TCP::Client client;
Buffer0<String>	bufferedCommands;
std::mutex	bufferedCommandsMutex;

void	Dispatch(const StringRef&line)
{
	if (line == "exit")
	{
		application_shutting_down = true;
		return;
	}
	//Console::EchoCurrent();
	if (client.IsConnected())
	{
		Channels::command.SendTo(client,line);
	}
	else
	{
		bufferedCommandsMutex.lock();
		bufferedCommands << line;
		bufferedCommandsMutex.unlock();
	}
}


void OnDeserializationFailed(UINT32 channel, TCP::Peer&peer)
{
	Console::WriteLine(peer.ToString()+": Deserialization failed on channel "+String(channel));
}


Event	exitLock;


void OnEvent(TCP::event_t event, TCP::Peer&peer)
{
	switch (event)
	{
		case TCP::Event::ConnectionClosed:
		case TCP::Event::ConnectionFailed:
		case TCP::Event::ConnectionLost:
			if (Handler::interactive)
			{
				Console::WriteLine(peer.ToString()+": "+TCP::event2str(event)+" => closing down");
				exit(-1);
			}
			else
				exitLock.signal();
		return;
		case TCP::Event::ConnectionEstablished:
			if (Handler::interactive)
				Console::WriteLine(peer.ToString()+": "+TCP::event2str(event));
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
	bool	interactive = false;


	void	Process(const char*arg)
	{
		//Console::WriteLine("Parameter: "+String(arg));
		switch (mode)
		{
			case ReadCommand:
				command << ' '<<arg;
			return;
			case Idle:
				if (!strcmp(arg,"--port") || !strcmp(arg,"-p"))
					mode = ExpectPort;
				elif (!strcmp(arg,"--interactive") || !strcmp(arg,"-i"))
					Handler::interactive = interactive = true;
				elif (!strcmp(arg,"--help") || !strcmp(arg,"-i"))
				{
					std::cout << "usage: "<<program<<" [-p basePort] [-i] [command(s)]"<<std::endl;
					std::cout << " -i/--interactive\tOpen a shell to enter commands interactively"<<std::endl;
					std::cout << " -p/--port [port]\tUse a non-default base port"<<std::endl;
				}
				else
				{
					command << arg;
					mode = ReadCommand;
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


int main(int argc, char* argv[])
{
	
	SetConsoleCtrlHandler(MyHandlerRoutine,TRUE);

	ParameterConfig::program = argc > 0 ? argv[0] : "";
	for (int i = 1; i < argc; i++)
		ParameterConfig::Process(argv[i]);


	
	client.InstallChannel(Channels::error);
	client.InstallChannel(Channels::context);
	client.InstallChannel(Channels::response);
	client.OpenSignalChannel(Channels::ID::ExecutionDone,0);

	client.onDeserializationFailed = OnDeserializationFailed;
	client.onEvent = OnEvent;
	client.onIgnorePackage = OnIgnorePackage;
	client.onSignal = OnSignal;

	if (!ParameterConfig::interactive && bufferedCommands.IsEmpty())
		return 0;
	client.ConnectAsync("localhost:"+String(ParameterConfig::basePort+PortOffset::Control));
	std::cout << std::endl;

	if (ParameterConfig::interactive)
	{
		Console::Refresh();
		while (!application_shutting_down)
		{
			char c;
			std::cin.get(c);
			Console::AddChar(c);
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


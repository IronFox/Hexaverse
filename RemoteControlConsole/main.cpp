// RemoteControlConsole.cpp : Defines the entry point for the console application.
//

#include <global_root.h>
#include <net/tcp.h>
#include <net/containers.h>
#include <string/string_buffer.h>

#include <mutex>

BOOL WINAPI MyHandlerRoutine(_In_  DWORD dwCtrlType)
{
	switch (dwCtrlType)
	{
		case CTRL_C_EVENT:
		case CTRL_CLOSE_EVENT:
		case CTRL_SHUTDOWN_EVENT:
			//TerminateNow(false);
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

	void	WriteLine(const String&line)
	{
		mutex.lock();
			for (index_t i = 0; i < lastLine.length(); i++)
				std::cout << '\r';
			std::cout << line << std::endl;
			std::cout << lastLine;
		mutex.unlock();

	}

	void	EchoCurrent()
	{
		mutex.lock();
		WriteLine(lastLine);
		mutex.unlock();
	}

	void	Refresh()
	{
		mutex.lock();
			for (index_t i = 0; i < lastLine.length(); i++)
				std::cout << '\r';
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
			case 9:
				input << ' ';
			break;
			case 13:
				Dispatch(input.ToStringRef());
				input.Clear();
			break;
			default:
				if (Template::isalnum((BYTE)c))
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
	void	OnContext(Package::NetString<1024>&ctx)
	{
		Console::mutex.lock();
		if (Console::context != ctx)
		{
			Console::context = ctx;
			Console::Refresh();
		}
		Console::mutex.unlock();
	}


	void	OnResponse(Package::NetString<1024>&output)
	{
		Console::WriteLine(output);
	}

	void	OnError(Package::NetString<1024>&error)
	{
		Console::WriteLine("ERROR: "+error);
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
		};
	}


	TCP::ObjectSender<Package::NetString<1024>,ID::Command>	command;
	TCP::Channel<Package::NetString<1024>,0,ID::Error>		error(Handler::OnError);
	TCP::Channel<Package::NetString<1024>,0,ID::Context>	context(Handler::OnContext);
	TCP::Channel<Package::NetString<1024>,0,ID::Response>	response(Handler::OnResponse);
}


TCP::Client client;


void	Dispatch(const StringRef&line)
{
	Console::EchoCurrent();
	Channels::command.SendTo(client,line);
}


void OnDeserializationFailed(UINT32 channel, TCP::Peer&peer)
{
	
}


int main(int argc, char* argv[])
{

	SetConsoleCtrlHandler(MyHandlerRoutine,TRUE);
	
	client.InstallChannel(Channels::error);
	client.InstallChannel(Channels::context);
	client.InstallChannel(Channels::response);

	client.onDeserializationFailed = OnDeserializationFailed;
	client.onEvent = OnEvent;
	client.onIgnorePackage = OnIgnorePackage;
	client.onSignal = OnSignal;

	client.ConnectAsync("localhost:10250");

	while (!application_shutting_down)
	{
		char c;
		std::cin.get(c);
		Console::AddChar(c);
	}



	return 0;
}


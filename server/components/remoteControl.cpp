#include <global_root.h>
#include "remoteControl.h"

#include <thread>
#include <general/thread_system.h>


namespace RemoteControl
{
	
	std::thread		listenThread;

	NamedPipeClient	client;


	void	Start(Protocol::ChannelID::id_t waitForPacket /*= Protocol::ChannelID::Unused*/)
	{
	//	listenThread.swap(std::thread(ThreadMain));
	
	
	}

	void	Destroy()
	{
	//	listenThread.join();
	
	
	}



}




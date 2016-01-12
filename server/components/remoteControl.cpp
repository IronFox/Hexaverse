#include <global_root.h>
#include "remoteControl.h"

#include <thread>
#include <general/thread_system.h>

#include <string/cli_defaults.h>

#include "reflection.h"


namespace RemoteControl
{
	namespace Detail
	{


		TCP::Server		server;
		bool			created = false;


		void Debug( void *ctx, int level, const char *file, int line, const char *str )
		{
			((void) level);
			_CRT_UNUSED(ctx);
			LogEvent(EventType::Debug,nullptr,"In "+String(file)+", line "+String(line)+": '"+str+"'");
		}

		void OnDeserializationFailed(UINT32 channel, TCP::Peer&origin)
		{
			LogEvent(EventType::InternalFault,nullptr,"Consensus: "+origin.ToString()+": Deserialization failed on channel "+String(channel));
		}

		void OnEvent(TCP::event_t event, TCP::Peer&origin)
		{
			LogEvent(EventType::Debug,nullptr,"Consensus: "+origin.ToString()+": "+TCP::event2str(event));
			switch (event)
			{
				case TCP::Event::ConnectionEstablished:
					//ASSERT__(Channels::hello.SendTo(client,Protocol::ProtocolVersion));
				break;

			}
		}

		void OnIgnorePackage(UINT32 channel,UINT32 size, TCP::Peer&origin)
		{
			LogEvent(EventType::InternalFault,nullptr,"Consensus: "+origin.ToString()+": Package of size "+String(size)+" ignored on channel "+String(channel));
		}

		void OnSignal(UINT32 channel, TCP::Peer&origin)
		{
			LogEvent(EventType::Debug,nullptr,"Consensus: "+origin.ToString()+": Signal on channel "+String(channel));
			//switch (channel)
			//{
			//	case Protocol::ChannelID::NotAuthenticated:
			//		client.SendSignal(Protocol::ChannelID::RequestNewAccount);
			//		LogEvent(EventType::SessionRelated,nullptr,"Not authenticated: Requested new account");
			//	break;
			//	case Protocol::ChannelID::VersionMismatch:
			//		LogEvent(EventType::OperationAborted,nullptr,"Server signalled protocol version mismatch. Unable to connect to Consensus network");
			//		connect = false;
			//		client.Disconnect();
			//	break;

			//}
			//Dialog::SignalSituationChange();
		}


		namespace Handler
		{

			
			void	OnCommand( Package::NetString<1024>&cmd, TCP::Peer&peer);
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


			TCP::Channel<Package::NetString<1024>,0,ID::Command>	command(Handler::OnCommand);
			TCP::ObjectSender<Package::NetString<1024>,ID::Error>	error;
			TCP::ObjectSender<Package::NetString<1024>,ID::Context>	context;
			TCP::ObjectSender<Package::NetString<1024>,ID::Response>	response;
		}


		namespace ExecutionContext
		{
			TCP::Peer*currentPeer;


			void PrintLn(const String&line)
			{
				LogEvent(EventType::RemoteControl,"Response: '"+line+"'");
				Channels::response.SendTo(*currentPeer,line);
			}
			void	OnFocusChange()
			{
				String context = Reflection::state.GetFocus()->GetPath();
				LogEvent(EventType::RemoteControl,"Focus change to '"+context+"'");
				Channels::context.SendTo(*currentPeer,context);
			}
		}


		void	Handler::OnCommand(Package::NetString<1024>&cmd, TCP::Peer&peer)
		{
			LogEvent(EventType::RemoteControl, Reflection::state.GetContext()+">'"+cmd+"'");
			CLI::println = ExecutionContext::PrintLn;
			CLI::onFocusChange = ExecutionContext::OnFocusChange;
			ExecutionContext::currentPeer = &peer;

			if (!Reflection::state.Interpret(cmd))
			{
				LogEvent(EventType::RemoteControlError,Reflection::state.GetError());
				Channels::error.SendTo(peer,Reflection::state.GetError());
			}
		}


		void	Create()
		{
			if (created)
				return;
			
			server.onDeserializationFailed = OnDeserializationFailed;
			server.onEvent = OnEvent;
			server.onIgnorePackage = OnIgnorePackage;
			server.onSignal = OnSignal;
			server.SetAsync(false);

			server.InstallChannel(Channels::command);

			//client.OpenSignalChannel(Protocol::ChannelID::VersionMismatch,0);

			created = true;
		};

	}



	
	void	Start(UINT16 serverBasePort /*= DEFAULT_SERVER_ROOT_PORT*/)
	{
		Detail::Create(); 
		UINT16 port = serverBasePort + PortOffset::Control;
		if (Detail::server.StartService(port,true))
		{
			LogEvent(EventType::RemoteControl,"Remote control server online on port "+String(port));
		}
		else
			LogEvent(EventType::RemoteControlError,"Failed to register remote control server on port "+String(port));
	}


	void	Resolve()
	{
		Detail::server.Resolve();

	}

	void	Destroy()
	{
		Detail::server.Shutdown();

	}



}




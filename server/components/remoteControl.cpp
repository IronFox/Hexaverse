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
			LogEvent(EventSignificance::Debug,nullptr,"In "+String(file)+", line "+String(line)+": '"+str+"'");
		}

		void OnDeserializationFailed(UINT32 channel, TCP::Peer&origin)
		{
			LogEvent(EventClass::Preset::networkError,nullptr,"RemoteControl: "+origin.ToString()+": Deserialization failed on channel "+String(channel));
		}



		void OnIgnorePackage(UINT32 channel,UINT32 size, TCP::Peer&origin)
		{
			LogEvent(EventClass::Preset::networkError,nullptr,"RemoteControl: "+origin.ToString()+": Package of size "+String(size)+" ignored on channel "+String(channel));
		}

		void OnSignal(UINT32 channel, TCP::Peer&origin)
		{
			LogEvent(EventSignificance::Debug,nullptr,"RemoteControl: "+origin.ToString()+": Signal on channel "+String(channel));
		}


		namespace Handler
		{

			
			void	OnCommand(Package::NetString<1024>&cmd, TCP::Peer&peer);
			void	OnComplete(Package::NetString<1024>&cmd, TCP::Peer&peer);
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


			TCP::Channel<Package::NetString<1024>,0,ID::Command>	command(Handler::OnCommand);
			TCP::Channel<Package::NetString<1024>,0,ID::Complete>	complete(Handler::OnComplete);
			TCP::ObjectSender<Package::NetString<1024>,ID::Error>	error;
			TCP::ObjectSender<Package::NetString<1024>,ID::Context>	context;
			TCP::ObjectSender<Package::NetString<1024>,ID::Response>	response;
		}


		void OnEvent(TCP::event_t event, TCP::Peer&origin)
		{
			LogEvent(EventSignificance::Debug,nullptr,"RemoteControl: "+origin.ToString()+": "+TCP::event2str(event));
			switch (event)
			{
				case TCP::Event::ConnectionEstablished:
					Channels::context.SendTo(origin,Reflection::state.GetFocus()->GetPath());
					//ASSERT__(Channels::hello.SendTo(client,Protocol::ProtocolVersion));
				break;

			}
		}
		namespace ExecutionContext
		{
			TCP::Peer*currentPeer;


			void PrintLn(const String&line)
			{
				LogEvent(EventClass::Preset::remoteControlResponse,line);
				Channels::response.SendTo(*currentPeer,line);
			}
			void	OnFocusChange()
			{
				String context = Reflection::state.GetFocus()->GetPath();
				LogEvent(EventClass::Preset::remoteControlResponse,"Focus change to '"+context+"'");
				Channels::context.SendTo(*currentPeer,context);
			}
		}


		void	Handler::OnCommand(Package::NetString<1024>&cmd, TCP::Peer&peer)
		{
			LogEvent(EventClass::Preset::remoteControlCommand, Reflection::state.GetContext()+">"+cmd);
			CLI::println = ExecutionContext::PrintLn;
			CLI::onFocusChange = ExecutionContext::OnFocusChange;
			ExecutionContext::currentPeer = &peer;

			index_t equalAt = cmd.GetIndexOf('=');
			if (equalAt)
			{
				cmd = "set "+cmd.subString(0,equalAt-1) + ' '+cmd.subString(equalAt);
			}
			if (!Reflection::state.Interpret(cmd))
			{
				LogEvent(EventClass::Preset::remoteControlError,Reflection::state.GetError());
				Channels::error.SendTo(peer,Reflection::state.GetError());
			}

			peer.SendSignal(Channels::ID::ExecutionDone);
		}

		void	Handler::OnComplete(Package::NetString<1024>&cmd, TCP::Peer&peer)
		{
			//LogEvent(EventClass::Preset::remoteControlCommand, Reflection::state.GetContext()+"complete:'"+cmd+"'");
			StringList alternatives;
			String result = Reflection::state.ExtendedComplete(cmd,alternatives);
			if (alternatives > 1)
			{
				for (index_t i = 0; i < alternatives.count() && i < 5; i++)
					Channels::response.SendTo(peer,alternatives[i]);
				if (alternatives.count() > 5)
					Channels::response.SendTo(peer,"(...)");
					
				Channels::response.SendTo(peer,"--");
			}
			if (result.IsNotEmpty())
				Channels::complete.SendTo(peer,result);
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
			server.InstallChannel(Channels::complete);

			//client.OpenSignalChannel(Protocol::ChannelID::VersionMismatch,0);

			created = true;
		};

	}


	void	RelayEvent(const String&message)
	{
		Detail::Channels::response.SendTo(Detail::server,message);

	}


	
	void	Start(UINT16 serverBasePort, TCP::Server::Protocol proto)
	{
		Detail::Create(); 
		UINT16 port = serverBasePort + PortOffset::Control;
		if (Detail::server.ExtStartService(port,proto, true))
		{
			LogEvent(EventClass::Preset::remoteControlCommand,"Remote control server online on port "+String(port));
		}
		else
			LogEvent(EventClass::Preset::remoteControlError,"Failed to register remote control server on port "+String(port));
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




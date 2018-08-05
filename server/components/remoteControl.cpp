#include <global_root.h>
#include "remoteControl.h"

#include <thread>
#include <general/thread_system.h>

#include <string/cli_defaults.h>
#include <globalEvents.h>

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

		void OnDeserializationFailed(UINT32 channel, TCP::Peer&origin, const String&exception)
		{
			LogEvent(EventClass::Preset::networkError,nullptr,"RemoteControl: "+origin.ToString()+": Deserialization failed on channel "+String(channel)+": "+exception);
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



		class Attachment : public TCP::Peer::Attachment
		{
		public:
			const CLI::PParser	parser;
			const TCP::WPeer	peer;


			/**/			Attachment(TCP::PPeer&peer):peer(peer), parser(Reflection::state->NewParser())
			{
				parser->onFocusChanged = [this]()
				{
					const auto context = parser->GetFocus()->GetPath();
					LogEvent(EventClass::Preset::remoteControlResponse,"Focus change to '"+context+"'");
					Channels::context.SendTo(this->peer.lock(),context);
				};
				parser->onPrintLine = [this](const String&line)
				{
					//LogEvent(EventClass::Preset::remoteControlResponse,line);	//these may contain personal information, so do not log
					Channels::response.SendTo(this->peer.lock(),line);
				};
				parser->onFocusChanged();
			}

		};
		typedef std::shared_ptr<Attachment>	PAttachment;

		void OnEvent(TCP::event_t event, TCP::Peer&origin)
		{
			ASSERT1__(origin.IsLocalhost(),origin.ToString());	//this should be implied through the way the server is created. but just in case something goes wrong, better crash than let someone remote control the system
			LogEvent(EventClass::Preset::remoteControlCommand,nullptr,"Control connection: "+origin.ToString()+": "+TCP::event2str(event));
			switch (event)
			{
				case TCP::Event::ConnectionEstablished:
				{
					origin.attachment.reset(new Attachment(server.GetSharedPointerOfClient(&origin)));
				}
				break;

			}
		}
		namespace ExecutionContext
		{
			TCP::Peer*currentPeer;
		}





		void	Handler::OnCommand(Package::NetString<1024>&cmd, TCP::Peer&peer)
		{
			ExecutionContext::currentPeer = &peer;
			PAttachment attach = To<Attachment>::Cast(peer.attachment);
			auto parser = attach->parser;

			const String ctx = parser->GetContext();
			//LogEvent(EventClass::Preset::remoteControlCommand, parser->GetContext()+">"+cmd);
			Channels::response.SendTo(peer,parser->GetContext()+">"+cmd);

			index_t equalAt = cmd.Find('=');
			if (equalAt != InvalidIndex)
			{
				cmd = "set "+cmd.subString(0,equalAt) + ' '+cmd.subString(equalAt+1);
			}
			if (!parser->Interpret(cmd))
			{
				LogEvent(EventClass::Preset::remoteControlError,ctx + ">"+cmd+" => "+ parser->GetError());
				Channels::error.SendTo(peer,parser->GetError());
			}

			peer.SendSignal(Channels::ID::ExecutionDone);
			ExecutionContext::currentPeer = nullptr;
		}

		void	Handler::OnComplete(Package::NetString<1024>&cmd, TCP::Peer&peer)
		{
			PAttachment attach = To<Attachment>::Cast(peer.attachment);
			auto parser = attach->parser;

			//LogEvent(EventClass::Preset::remoteControlCommand, Reflection::state.GetContext()+"complete:'"+cmd+"'");
			Ctr::StringList alternatives;
			String result = parser->ExtendedComplete(cmd.ToRef(),alternatives);
			if (alternatives > 1)
			{
				for (index_t i = 0; i < alternatives.Count() && i < 5; i++)
					Channels::response.SendTo(peer,alternatives[i]);
				if (alternatives.Count() > 5)
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

		index_t	eventKey = InvalidIndex;

		void Destroy()
		{
			server.Shutdown();
		}
	}


	void	RelayEvent(const String&message)
	{
		if (Detail::ExecutionContext::currentPeer)
			Detail::Channels::response.SendTo(*Detail::ExecutionContext::currentPeer,message);
		else
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

		if (Detail::eventKey == InvalidIndex)
			Detail::eventKey = GlobalEvents::applicationTerminate.Register(Detail::Destroy);
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




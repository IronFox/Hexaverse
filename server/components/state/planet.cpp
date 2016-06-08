#include <global_root.h>
#include <net/tcp.h>
#include <protocol.h>
#include "planet.h"
#include <general/thread_system.h>
#include <container/buffer.h>
#include <logEvent.h>
#include "../networkChannels.h"
#include "../journalSaveGame.h"
#include "networkState.h"
#include "worldState.h"
#include "../reflection.h"

namespace Planet
{
	

	namespace Details
	{

		Location	location;
		
		bool created = false;

		class LocationData;
		typedef std::shared_ptr<LocationData>	PLocationData;


		class InterplanetarySensoryRequest : public Journal::Round::GlobalEvent
		{
		public:
			Journal::Round::ChangeInterplanetarySensory	data;
			TUniqueID		factionID;
			CryptographicID	accessToken;	//change to user ID on steam

			/**/			InterplanetarySensoryRequest(){}
			/**/			InterplanetarySensoryRequest(const TUniqueID&factionID, const CryptographicID&accessToken, const Journal::Round::ChangeInterplanetarySensory&data):data(data),factionID(factionID), accessToken(accessToken)	{}
			Event*			Instantiate() const override {return new InterplanetarySensoryRequest();}
			void			Decode(const Journal::Entry&e) override;
			void			Execute()	const override;

		protected:
			virtual void	Compile(Journal::Entry&)	const override;
		};



		namespace Handler
		{
			namespace Object
			{
				NETSTRUCT2(Peer
					, NET(Location), location
					, Package::NetString<128>, host
				)

				typedef NETA(Package::NetString<512>) WorldConfig;

				NETSTRUCT3(Hello
					, WorldConfig, worldConfig
					, NET(Location), location
					, NETA(Peer), neighbors
				)

				typedef NET(RuntimeConfig::RoundNumber)	BeginRound;

				NETSTRUCT3(FactionSensorAccess
					, NET(TUniqueID), factionID
					, NET(UINT8), type
					, NET(CryptographicID), accessID
				)
				typedef NETA(FactionSensorAccess)	GainSensorAccess;

				typedef NET(Location)	PeerHello;
			}
			
		}
		
		void	RegisterPeer(const Handler::Object::Peer&loc);
		void	RefreshPeer(TCP::Peer&p);
		PLocationData	FindLocationData(const Location&loc);

		namespace Handler
		{
			class Attachment : public TCP::Peer::Attachment
			{
			public:
				RuntimeConfig::RoundNumber	roundNumber;
				PLocationData		locationData;
				//Grid::SkySensorState	sensor;

				UINT32			GetSensorLocation() const;

				/**/			Attachment(const PLocationData&data):locationData(data) {}
			};
			typedef std::shared_ptr<Attachment>	PAttachment;

			void OnMasterEvent(TCP::event_t ev, TCP::Peer&p)
			{
				switch (ev)
				{
					case TCP::Event::ConnectionEstablished:
						LogEvent(EventSignificance::Important,"Planet: Connection established to master server");
					break;
					case TCP::Event::ConnectionFailed:
					case TCP::Event::ConnectionClosed:
					case TCP::Event::ConnectionLost:
						FATAL__("Planet: Connection problems to master server ("+p.GetError(true)+")");
					break;
				
				}
			}
			void OnPeerEvent(TCP::event_t ev, TCP::Peer&p);
			void Hello(Object::Hello&hello);

			void PeerHello(Object::PeerHello&hello, TCP::Peer&peer)
			{
				if (peer.userLevel > 0)
					return;
				PLocationData data = FindLocationData(hello);
				if (data)
				{
					peer.attachment.reset(new Attachment(data));
					peer.userLevel = 1;
					RefreshPeer(peer);
				}
			}

			void GainSensorAccess(Object::GainSensorAccess&sens, TCP::Peer&peer);
			void BeginRound(Object::BeginRound&rnd, TCP::Peer&p)
			{
				PAttachment a = To<Attachment>::Cast(p.attachment);
				if (a)
				{
					a->roundNumber = rnd;
				}
			}

			namespace Channel
			{
				namespace ID
				{
					enum id_t
					{
						None,
						Hello,
						PeerHello,
						BeginRound,
						SensorAccess,
						GainSensorAccess,
					};
				}

				TCP::Channel<Object::Hello,0,ID::Hello>				hello(Hello);
				TCP::Channel<Object::PeerHello,0,ID::PeerHello>		peerHello(PeerHello);
				TCP::Channel<Object::BeginRound,1,ID::BeginRound>	beginRound(BeginRound);
				TCP::Channel<Object::GainSensorAccess,1,ID::GainSensorAccess>	gainSensorAccess(GainSensorAccess);
			}
		}
	
		class Peer : public ThreadObject
		{
			TCP::Client		connection;
			const Handler::Object::Peer	host;
			count_t			failCount = 0;
			std::atomic<bool>	end = false;

			void			SelfDestruct();
		public:
			/**/			Peer(const Handler::Object::Peer&host):host(host)
			{
				PLocationData data = FindLocationData(host.location);
				ASSERT__(data);
				if (data)
				{
					connection.attachment.reset(new Handler::Attachment(data));
					connection.userLevel = 1;
				}
				start();
			}

			void		Visit(const FPeerVisitor&v)
			{
				if (connection.IsConnected())
					v(connection);
			}

			void		Destroy();
		protected:
			virtual void ThreadMain() override
			{
				//connection.onSignal = Receiver::OnSignal;
				connection.onEvent = Handler::OnPeerEvent;
				connection.onIgnorePackage = Receiver::OnIgnorePackage;
				connection.onDeserializationFailed = Receiver::OnDeserializationFailed;
				
				connection.InstallChannel(Handler::Channel::beginRound);
				connection.InstallChannel(Handler::Channel::gainSensorAccess);
		
				for (;;)
				{
					if (end)
					{
						SelfDestruct();
						return;
					}
					while (!connection.Connect(host.host))
					{
						failCount++;
						if (failCount > 10 || end)
						{
							SelfDestruct();
							return;
						}
						Sleep(500);
					}
					failCount = 0;
					Handler::Channel::peerHello.SendTo(connection,location);
					while (connection.IsConnected())
					{
						if (end)
						{
							SelfDestruct();
							return;
						}
						Sleep(500);
					}

				}

			}
		};
		typedef std::shared_ptr<Peer>	PPeer;

		TCP::Client	masterConnection;
		TCP::Server p2pServer;
		Buffer0<PPeer>	peers;
		SpinLock	peerLock;
		UINT16		lastP2pPort=0;


		class LocationData
		{
		public:
			const Location	location;
			SpinLock		dataLock;
			
			GenericHashTable<TUniqueID,Handler::Object::FactionSensorAccess,TUniqueID>	accessMap;
			Buffer0<Objects::PMobileEntity>	transferQueue;

			GenericHashTable<RuntimeConfig::RoundNumber,Journal::Round,PODHash,Copy,Swap>	rounds;


			/**/			LocationData(const Location&loc):location(loc)
			{}
		};
		typedef std::shared_ptr<LocationData>	PLocationData;

		Array<PLocationData>		neighbors;
		SpinLock					neighborLock;
		RuntimeConfig::RoundNumber	compareRoundNumber;
		volatile bool				lockCurrentRoundNumber = false;
		Handler::Object::WorldConfig	worldConfig;
		SpinLock					worldLock;
		volatile bool				haveNewWorld=false;

		void	RegisterPeer(const Handler::Object::Peer&loc)
		{
			peerLock.lock();
			peers.Append().reset(new Peer(loc));
			peerLock.unlock();
		}

		void	FlushPeers(bool restart = true)
		{
			peerLock.lock();
				foreach (peers,p)
				{
					(*p)->Destroy();
				}
				peers.Clear();
			
				p2pServer.Shutdown();
				if (restart)
					p2pServer.StartService(lastP2pPort);
			peerLock.unlock();
		}


		void	Peer::Destroy()
		{
			bool b = false;
			if (end.compare_exchange_strong(b,true))
			{
				LogEvent(EventSignificance::Uncommon,"Closing connection to "+host.host);
				connection.Disconnect();
				this->CheckAwaitCompletion(1000);
			}
		}

		void	Peer::SelfDestruct()
		{
			bool b = false;
			if (end.compare_exchange_strong(b,true))
			{
				LogEvent(EventSignificance::Uncommon,"Lost connection to "+host.host);
				connection.Disconnect();
				PPeer copy;
				peerLock.lock();
					foreach (peers,p)
						if (p->get() == this)
						{
							copy = *p;
							peers.erase(p);
							break;
						}
				peerLock.unlock();
			}
		}

		TCP::Peer*	FindPeerFor(const Location&loc)
		{
			TCP::Peer*rs = nullptr;
			NeighborOperations::VisitPeers([loc,&rs](TCP::Peer&p)
			{
				PLocationData data = To<Handler::Attachment>::Cast(p.attachment)->locationData;
				if (data->location == loc)
				{
					rs = &p;
				}
			});
			return rs;
		}

		void	RefreshPeer(TCP::Peer&p)
		{
			if (!p.attachment)
				return;
			PLocationData data = To<Handler::Attachment>::Cast(p.attachment)->locationData;
			data->dataLock.lock();
			{
				Handler::Object::GainSensorAccess access;
				
				data->accessMap.exportTo(access);
				Handler::Channel::beginRound.SendTo(p,RuntimeConfig::roundNumber);
				Handler::Channel::gainSensorAccess.SendTo(p,access);
			}
			data->dataLock.unlock();

		}
	}

	UINT32			Details::Handler::Attachment::GetSensorLocation() const
	{
		return Grid::low.Locate(locationData->location-location);
	}

	void Details::Handler::Hello(Object::Hello&hello)
	{
		location = hello.location;

		neighborLock.lock();
		neighbors.SetSize(hello.neighbors.Count());
		for (index_t i = 0; i < hello.neighbors.Count(); i++)
			neighbors[i].reset(new LocationData(hello.neighbors[i].location));
		neighborLock.unlock();
		FlushPeers();
		foreach (hello.neighbors,n)
		{
			if (n->host.IsNotEmpty())
				RegisterPeer(*n);
		}
		
		worldLock.lock();
		worldConfig.swap(hello.worldConfig);
		haveNewWorld = true;
		worldLock.unlock();
	}

	void Details::Handler::OnPeerEvent(TCP::event_t ev, TCP::Peer&p)
	{
		switch (ev)
		{
			case TCP::Event::ConnectionEstablished:
				LogEvent(EventSignificance::Important,"Planet: Connection established to peer "+p.ToString());
				RefreshPeer(p);
			break;
			case TCP::Event::ConnectionFailed:
			case TCP::Event::ConnectionClosed:
			case TCP::Event::ConnectionLost:
			{
				LogEvent(EventSignificance::Important,"Planet: Connection lost to peer "+p.ToString());
				PAttachment a = To<Attachment>::Cast(p.attachment);
				if (a)
				{
					UINT32 loc = a->GetSensorLocation();
					Objects::ForeachFaction([loc](const Objects::PFaction&f)
					{
						f->RemoveInterplanetarySensor(loc);
					});
					a->locationData->dataLock.lock();
					{
						a->locationData->rounds.Clear();
					}
					a->locationData->dataLock.unlock();
				}
			}
			break;
				
		}
	}


	void Details::Handler::GainSensorAccess(Object::GainSensorAccess&sens, TCP::Peer&peer)
	{
		Attachment&attachment = *To<Attachment>::Cast(peer.attachment);
		PLocationData data = attachment.locationData;
		data->dataLock.lock();
		if (attachment.roundNumber < compareRoundNumber || (lockCurrentRoundNumber && attachment.roundNumber == compareRoundNumber))
		{
			LogEvent(EventClass::Preset::networkError,"Received message from the past from "+peer.ToString());
			data->dataLock.unlock();
			return;
		}

		Journal::Round&rnd = data->rounds.Set(attachment.roundNumber);

		foreach (sens,facID)
		{
			UINT32 locI = attachment.GetSensorLocation();
			{
				rnd.Register(false,new Journal::Round::CreateFaction(Journal::Round::CreateFaction::FactionType::Reinterpret(facID->type),facID->factionID,facID->accessID));
				rnd.Register(false, facID->factionID,new Journal::Round::ChangeInterplanetarySensory(false,locI));
			}
		}
		data->dataLock.unlock();
	}

	

	/*virtual override*/void			Details::InterplanetarySensoryRequest::Decode(const Journal::Entry&e)
	{
		data.Decode(e);
		factionID = e.uuidParameters.Require(CLOCATION,"FactionID");
		const Array<BYTE>&token = e.binaryParameters.Require(CLOCATION,"AccessToken");
		if (token.GetContentSize() != CryptographicID::NumBytes)
			throw Program::DataConsistencyFault( CLOCATION,"Access token has unexpected length: "+String(token.GetContentSize()));
		this->accessToken = CryptographicID(token);
	}

	/*virtual override*/void			Details::InterplanetarySensoryRequest::Execute()	const
	{
		Objects::PFaction f = Objects::Faction::FindByID(factionID);
		if (!f)
		{
			f.reset(new Objects::Faction(factionID));
			f->accessToken = accessToken;
			Objects::Faction::all << f;
		}
		Journal::currentRound.Register(true,f,new Journal::Round::ChangeInterplanetarySensory(data));
	}

	/*virtual override*/void			Details::InterplanetarySensoryRequest::Compile(Journal::Entry&e)	const
	{
		data.ExportTo(e);
		e.operation = Journal::Round::OperationNameOf(typeid(*this));

		e.uuidParameters.Set("FactionID",factionID);
		Array<BYTE> token;
		accessToken.Export(token);
		e.binaryParameters.Set("AccessToken",token);
	}


	void	NeighborOperations::VisitLocations(const FLocationVisitor&f)
	{
		using namespace Details;
		neighborLock.lock();
		foreach (neighbors,n)
		{
			f((*n)->location, FindPeerFor((*n)->location));
		}
		neighborLock.unlock();
	}


	void	NeighborOperations::VisitPeers(const FPeerVisitor&f)
	{
		using namespace Details;
		p2pServer.VisitAllPeers([f](const TCP::PPeer&p)
		{
			if (p->attachment)
				f(*p);
		});
		peerLock.lock();
		foreach (peers,p2)
			(*p2)->Visit(f);
		peerLock.unlock();
	}

	void	Create(const String&masterServer, UINT16 p2pPort)
	{
		using namespace Details;
		created = true;
		
		//masterConnection.onSignal = Receiver::OnSignal;
		masterConnection.onEvent = Handler::OnMasterEvent;
		masterConnection.onIgnorePackage = Receiver::OnIgnorePackage;
		masterConnection.onDeserializationFailed = Receiver::OnDeserializationFailed;

		masterConnection.InstallChannel(Handler::Channel::hello);

		masterConnection.ConnectAsync(masterServer);


		p2pServer.onEvent = Handler::OnPeerEvent;
		p2pServer.onIgnorePackage = Receiver::OnIgnorePackage;
		p2pServer.onDeserializationFailed = Receiver::OnDeserializationFailed;

		p2pServer.InstallChannel(Handler::Channel::peerHello);
		p2pServer.InstallChannel(Handler::Channel::beginRound);
		p2pServer.InstallChannel(Handler::Channel::gainSensorAccess);

		ASSERT1__(p2pServer.StartService(p2pPort,false,&p2pPort),p2pPort);
		lastP2pPort = p2pPort;
	}


	bool		NeighborOperations::RegisterSensorAccess(const Objects::PFaction&f, const Location&loc)
	{
		if (HasSensorAccess(f,loc))
			return true;
		using namespace Details;
		neighborLock.lock();
		PLocationData data = FindLocationData(loc);
		if (data)
		{
			Handler::Object::FactionSensorAccess	access;
			access.accessID = f->accessToken;
			access.factionID = f->id;
			access.type = (UINT8)Journal::GetType(f);
			data->dataLock.lock();
			data->accessMap.Set(f->id,access);
			data->dataLock.unlock();
		}
		neighborLock.unlock();
		return !!data;
	}

	void		NeighborOperations::UnregisterSensorAccess(const Objects::PFaction&f, const Location&loc)
	{
		using namespace Details;

		neighborLock.lock();
		PLocationData data = FindLocationData(loc);
		if (data)
		{
			data->dataLock.lock();
			data->accessMap.Unset(f->id);
			data->dataLock.unlock();
		}
		neighborLock.unlock();
	}

	bool		NeighborOperations::HasSensorAccess(const Objects::PFaction&f, const Location&loc)
	{
		using namespace Details;
		bool rs = false;
		neighborLock.lock();
		PLocationData data = FindLocationData(loc);
		if (data)
		{
			data->dataLock.lock();
			rs = data->accessMap.IsSet(f->id);
			data->dataLock.unlock();
		}
		neighborLock.unlock();
		return rs;
	}

	Location	GetMyLocation()
	{
		using namespace Details;
		return location;
	}

	bool		NeighborOperations::TransferMobileEntity(const Objects::PMobileEntity&e, const Location&dest)
	{
		using namespace Details;
		bool rs = false;
		neighborLock.lock();
		PLocationData data = FindLocationData(dest);
		if (data)
		{
			data->dataLock.lock();
			if (!data->transferQueue.Contains(e))
				data->transferQueue.Append(e);
			data->dataLock.unlock();
			rs = true;
		}
		neighborLock.unlock();
		return rs;
	}

		
	void	SignalCommandPhaseBegin()
	{
		using namespace Details;
		if (!created)
			return;
		NeighborOperations::VisitPeers([](TCP::Peer&p)
		{
			RefreshPeer(p);
		});
		lockCurrentRoundNumber = false;
		compareRoundNumber = RuntimeConfig::roundNumber;
	}

	void	SignalCommandPhaseEnd()
	{
		using namespace Details;
		if (!created)
			return;
		lockCurrentRoundNumber = true;
		NeighborOperations::VisitPeers([](TCP::Peer&p)
		{
			auto d = To<Handler::Attachment>::Cast(p.attachment)->locationData;
			d->dataLock.lock();
			Journal::Round*rnd = d->rounds.QueryPointer(RuntimeConfig::roundNumber);
			if (rnd)
			{
				Journal::currentRound.Include(*rnd,true);
				d->rounds.Unset(RuntimeConfig::roundNumber);
			}
			d->transferQueue.Clear();
			d->dataLock.unlock();
		});
	}

	void	Resolve()
	{
		using namespace Details;

		if (haveNewWorld)
		{
			worldLock.lock();
			haveNewWorld = false;
			Handler::Object::WorldConfig cfg;
			cfg.swap(worldConfig);
			worldLock.unlock();

			Reflection::state.PushAndReplaceFocus(Reflection::state.GetRoot());
			foreach (cfg,cmd)
			{
				Reflection::state.Interpret(cmd->c_str(),false);
			}
			Reflection::state.PopFocus();
			compareRoundNumber = RuntimeConfig::roundNumber = Game::GetGridRoundNumber();
			lockCurrentRoundNumber = false;
		}

	}


	void	Destroy()
	{
		using namespace Details;

		FlushPeers(false);
		neighborLock.lock();
		neighbors.Clear();
		neighborLock.unlock();
		masterConnection.Disconnect();
	}
	
}

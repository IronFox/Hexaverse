#include <global_root.h>
#include <net/tcp.h>
#include <protocol.h>
#include "planet.h"
#include <general/thread_system.h>
#include <container/buffer.h>
#include <logEvent.h>
#include "../networkChannels.h"
#include "../journalSaveGame.h"


namespace Planet
{
	

	namespace Details
	{

		Location	location;
		bool created = false;

		

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

				NETSTRUCT2(Hello
					, NET(Location), location
					, NETA(Peer), neighbors
				)

				typedef NETA(UINT32)	BeginRound;

				NETSTRUCT2(FactionSensorAccess
					, NET(TUniqueID), factionID
					, NET(CryptographicID), accessID
				)
				typedef NETA(FactionSensorAccess)	GainSensorAccess;

				typedef NET(Location)	PeerHello;
			}
			
		}
		
		void	RegisterPeer(const Handler::Object::Peer&loc);


		namespace Handler
		{
			class Attachment : TCP::Peer::Attachment
			{
			public:
				Object::PeerHello	peer;
				//Grid::SkySensorState	sensor;

				UINT32			GetSensorLocation() const
				{
					return Grid::low.Locate(float3(peer.value-location,0));
				}

				/**/			Attachment(const Object::PeerHello&p):peer(p){}
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
			void OnPeerEvent(TCP::event_t ev, TCP::Peer&p)
			{
				switch (ev)
				{
					case TCP::Event::ConnectionEstablished:
						LogEvent(EventSignificance::Important,"Planet: Connection established to peer "+p.ToString());
					break;
					case TCP::Event::ConnectionFailed:
					case TCP::Event::ConnectionClosed:
					case TCP::Event::ConnectionLost:
					{
						LogEvent(EventSignificance::Important,"Planet: Connection lost to peer "+p.ToString());
						PAttachment a = To<Attachment>::Cast(p.attachment);
						UINT32 loc = a->GetSensorLocation();
						Objects::ForeachFaction([loc](const Objects::PFaction&f)
						{
							f->RemoveInterplanetarySensor(loc);
						});
					}
					break;
				
				}
			}

			void Hello(Object::Hello&hello)
			{
				location = hello.location;

				foreach (hello.neighbors,n)
					RegisterPeer(*n);
			}

			void PeerHello(Object::PeerHello&hello, TCP::Peer&peer)
			{
				if (peer.userLevel > 0)
					return;
				peer.attachment.reset(new Attachment(hello));
				peer.userLevel = 1;
				PAttachment a = To<Attachment>::Cast(peer.attachment);
				RefreshPeer(peer,a->peer);
			}

			void GainSensorAccess(Object::GainSensorAccess&sens, TCP::Peer&peer);

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
				connection.attachment.reset(new Handler::Attachment(host.location));
				connection.userLevel = 1;
				start();
			}

			void		Visit(const FPeerVisitor&v)
			{
				if (connection.IsConnected())
					v(connection,host.location);
			}
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
						Sleep(2000);
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

		struct LocationHash
		{
			static hash_t hash(const Location&loc)
			{
				return StdHash::memHash(&loc,sizeof(loc));
			}


		};

		class LocationData
		{
		public:
			
			GenericHashTable<TUniqueID,Handler::Object::FactionSensorAccess,TUniqueID>	accessMap;
			Buffer0<Objects::PMobileEntity>	transferQueue;

			Journal::Round::EventStorage<Journal::Round::GlobalEvent>	receivedEvents;

			void	swap(LocationData&other)
			{
				accessMap.swap(other.accessMap);
				transferQueue.swap(other.transferQueue);
				receivedEvents.swap(other.receivedEvents);
			}
		};

		GenericHashTable<Location,LocationData,LocationHash,Swap>	locationMap;
		SpinLock locationMapLock;


		void	RegisterPeer(const Handler::Object::Peer&loc)
		{
			peerLock.lock();
			peers.Append().reset(new Peer(loc));
			peerLock.unlock();
		}

		void	Peer::SelfDestruct()
		{
			bool b = false;
			if (end.compare_exchange_strong(b,true))
			{
				LogEvent(EventSignificance::Uncommon,"Lost connection to "+host.host);
				end = true;
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
			VisitPeers([loc,&rs](TCP::Peer&p, const Location&loc2)
			{
				if (loc2 == loc)
				{
					rs = &p;
				}
			});
			return rs;
		}

		void	RefreshPeer(TCP::Peer&p, const Location&loc)
		{
			static Handler::Object::BeginRound	round;
			round.set(1,RuntimeConfig::roundNumber);
			locationMapLock.lock();
			auto*data = locationMap.QueryPointer(loc);
			if (data)
			{
				Handler::Object::GainSensorAccess access;
				
				data->accessMap.exportTo(access);
				Handler::Channel::beginRound.SendTo(p,round);
				Handler::Channel::gainSensorAccess.SendTo(p,access);
			}
			locationMapLock.unlock();

		}
	}

	void Details::Handler::OnPeerEvent(TCP::event_t ev, TCP::Peer&p)
	{
		switch (ev)
		{
			case TCP::Event::ConnectionEstablished:
				LogEvent(EventSignificance::Important,"Planet: Connection established to peer "+p.ToString());
				if (p.attachment)
				{
					PAttachment a = To<Attachment>::Cast(p.attachment);
					RefreshPeer(p,a->peer);
				}
			break;
			case TCP::Event::ConnectionFailed:
			case TCP::Event::ConnectionClosed:
			case TCP::Event::ConnectionLost:
			{
				LogEvent(EventSignificance::Important,"Planet: Connection lost to peer "+p.ToString());
				PAttachment a = To<Attachment>::Cast(p.attachment);
				UINT32 loc = a->GetSensorLocation();
				Objects::ForeachFaction([loc](const Objects::PFaction&f)
				{
					f->RemoveInterplanetarySensor(loc);
				});
				locationMapLock.lock();
				LocationData*data = locationMap.QueryPointer(a->peer);
				if (data)
					data->receivedEvents.Clear();
				locationMapLock.unlock();
			}
			break;
				
		}
	}


	void Details::Handler::GainSensorAccess(Object::GainSensorAccess&sens, TCP::Peer&peer)
	{
		Attachment&attachment = *To<Attachment>::Cast(peer.attachment);
		const Location&loc = attachment.peer;
		locationMapLock.lock();
		LocationData*data = locationMap.QueryPointer(loc);
		if (data)
			foreach (sens,facID)
			{
				UINT32 locI = attachment.GetSensorLocation();
				data->receivedEvents.Append(Journal::Round::PGlobalEvent(new InterplanetarySensoryRequest(facID->factionID,facID->accessID, Journal::Round::ChangeInterplanetarySensory( true,locI,10))));
			}
		locationMapLock.unlock();
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




	void	VisitPeers(const FPeerVisitor&f)
	{
		using namespace Details;
		p2pServer.VisitAllPeers([f](const TCP::PPeer&p)
		{
			f(*p,To<Handler::Attachment>::Cast(p->attachment)->peer);
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



	}


	void		RegisterSensorAccess(const Objects::PFaction&f, const Location&loc)
	{
		if (HasSensorAccess(f,loc))
			return;
		using namespace Details;
		locationMapLock.lock();
		locationMap.Set(loc).accessMap.Set(f->id);
		locationMapLock.unlock();
	}

	void		UnregisterSensorAccess(const Objects::PFaction&f, const Location&loc)
	{
		using namespace Details;

		auto*p = locationMap.QueryPointer(loc);
		if (p && p->accessMap.Unset(f->id))
		{}
	}

	bool		HasSensorAccess(const Objects::PFaction&f, const Location&loc)
	{
		using namespace Details;

		auto*p = locationMap.QueryPointer(loc);
		return (p && p->accessMap.IsSet(f->id));
	}

	Location	GetMyLocation()
	{
		using namespace Details;
		return location;
	}

	bool		TransferMobileEntity(const Objects::PMobileEntity&e, const Location&dest)
	{
		using namespace Details;

		locationMap.Set(dest).transferQueue.Append(e);
	}

		
	void	SignalControlPhaseBegin()
	{
		using namespace Details;
		if (!created)
			return;
		VisitPeers([](TCP::Peer&p,const Location&loc)
		{
			RefreshPeer(p,loc);
		});
	}

	void	SignalControlPhaseEnd()
	{
		using namespace Details;
		if (!created)
			return;
		locationMapLock.lock();
		locationMap.VisitAllValues([](LocationData&dat)
		{
			foreach (dat.receivedEvents,ev)
			{
				(*ev)->Execute();
			}
			dat.receivedEvents.Clear();
			dat.transferQueue.Clear();
		});
		locationMapLock.unlock();

	}




	void	Destroy();
	
}
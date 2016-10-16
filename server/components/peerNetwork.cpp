#include <global_root.h>
#include "peerNetwork.h"
#include "networkChannels.h"
#include "serverLogEvent.h"
#include "../../client/version.h"


namespace PeerNetwork
{
	namespace Detail
	{
		typedef std::shared_ptr<TCP::Client>	PClient;
		TCP::Server			server;
		Buffer0<PClient>	clients;
		bool				serverIsConfigured=false;
		ID					myLogin;
		TUniqueID			myID;
		GenericHashTable<HashValue,ID,PODHash>	keys;
		float3				myLocation;
		bool				haveTimeConfig = false;
		time_t				time0,secondsPerRound;

		StringTable<PClient>	connecting;

		namespace IO
		{
			namespace ChannelID
			{
				enum channel_t
				{
					Unused,
					Challenge,
					ChallengeResponse,
					PlanetInfo,
					TimeConfig,

				};
			}

			NETSTRUCT2(TimeConfig
				, NET(UINT64), time0
				, NET(UINT32), secondsPerRound
			);

			NETSTRUCT2(Challenge
				, Package::NetString<64>, serverVersion
				, NET(CryptographicID), challengeBytes
			);

			NETSTRUCT2(ChallengeResponse
				, NET(HashValue), publicLoginID
				, NET(HashValue), hashedSecret
			);
			NETSTRUCT2(PlanetInfo
				, NET(TUniqueID), uniqueID
				, NET(TVec3<>), location
			);

			class Attachment : public TCP::Peer::Attachment
			{
			public:
				PlanetInfo		info;
				TLink			link;
				CryptographicID	challenge;
			};
			typedef std::shared_ptr<Attachment>	PAttachment;

			/**
			Contains all attachments with set links (from authenticated servers)
			*/
			Buffer0<PAttachment>	activeAttachments;
			IndexTable<index_t>		locationAttachmentMap;


			void OnChallenge(Challenge&challenge, TCP::Peer&peer);
			void OnChallengeResponse(ChallengeResponse&response, TCP::Peer&peer);
			void OnPlanetInfo(PlanetInfo&, TCP::Peer&peer);

			TCP::Channel<Challenge,0,ChannelID::Challenge>	challenge(OnChallenge);
			TCP::Channel<ChallengeResponse,0,ChannelID::ChallengeResponse>	challengeResponse(OnChallengeResponse);
			TCP::Channel<PlanetInfo,0,ChannelID::PlanetInfo>	planetInfo(OnPlanetInfo);	//!< Leave this at min-user-level 0, since asynchronous TCP message delivery may cause this message to be rejected otherwise (if received (and rejected) before userLevel is updated)

			void HashID(const CryptographicID&challenge, const ID&id, HashValue&rs)
			{
				BYTE toHash[sizeof(CryptographicID) + sizeof(HashValue)*2];
				memcpy(toHash,&challenge,sizeof(CryptographicID));
				memcpy(toHash+sizeof(CryptographicID),&id.publicID,sizeof(HashValue));
				memcpy(toHash+sizeof(CryptographicID)+sizeof(HashValue),&id.secret,sizeof(HashValue));
				Hash::Hash(toHash,sizeof(toHash),rs);
			}


			void OnChallenge(Challenge&challenge, TCP::Peer&peer)
			{
				if (challenge.serverVersion != SERVER_NAME)
				{
					LogEvent(EventClass( EventSignificance::ClientFatal, EventFlavor::Loss ), peer,"P2P: Remote server version mismatch. Expected "+String(SERVER_NAME)+", but received "+challenge.serverVersion);
					peer.DisconnectPeer();
					return;
				}
				ChallengeResponse response;
				HashID(challenge.challengeBytes,myLogin,response.hashedSecret.value);
				response.publicLoginID = myLogin.publicID;
				challengeResponse.SendTo(peer,response,0);
			}

			void OnChallengeResponse(ChallengeResponse&response, TCP::Peer&peer)
			{
				ID known;
				if (!keys.Query(response.publicLoginID,known))
				{
					LogEvent(EventClass( EventSignificance::ClientFatal, EventFlavor::Loss ), peer,"P2P: Remote ID not known");
					peer.DisconnectPeer();
					return;
				}
				HashValue shouldBe;
				PAttachment attach = To<Attachment>::Cast(peer.attachment);
				ASSERT__(attach);
				HashID(attach->challenge,known,shouldBe);
				if (shouldBe != response.hashedSecret)
				{
					LogEvent(EventClass( EventSignificance::ClientFatal, EventFlavor::Loss ), peer,"P2P: Remote password mismatch");
					peer.DisconnectPeer();
					return;
				}
				peer.userLevel = 1;

				PlanetInfo begin;
				begin.location = myLocation;
				begin.uniqueID = myID;

				IO::planetInfo.SendTo(peer,begin,0);

				LogEvent(EventClass( EventSignificance::Debug, EventFlavor::Gain ), peer,"P2P: Logged in. Planet info sent.");

			}

			void RemoveActiveAttachment(const PAttachment&attach)
			{
				index_t setAt = activeAttachments.GetIndexOf(attach);
				if (setAt != InvalidIndex)
				{
					locationAttachmentMap.Unset(attach->link.localLowCellID);
					locationAttachmentMap.VisitAllValues([setAt](index_t&i)
					{
						if (i > setAt)
							i--;
					});
					activeAttachments.Erase(setAt);
				}
			}

			void OnPlanetInfo(PlanetInfo&info, TCP::Peer&peer)
			{
				if (peer.userLevel == 0)
				{
					LogEvent(EventClass( EventSignificance::ClientFatal, EventFlavor::Loss ), peer,"P2P: Received planet info but remote end did not authenticate.");
					return;
				}
				PAttachment attach = To<Attachment>::Cast(peer.attachment);
				ASSERT__(attach);
				attach->info = info;

				RemoveActiveAttachment(attach);
				attach->link.relativeRemoteEnd = info.location.value - myLocation;
				attach->link.localLowCellID = Grid::low.Locate(attach->link.relativeRemoteEnd);
				attach->link.remoteLowCellID = Grid::low.Locate(-attach->link.relativeRemoteEnd);
				attach->link.transitRounds = (UINT32)ceil(attach->link.relativeRemoteEnd.length() / MaxTransitSpeed);
				index_t at = activeAttachments.Count();
				activeAttachments << attach;
				locationAttachmentMap.Set(attach->link.localLowCellID,at);
				LogEvent(EventClass( EventSignificance::Debug, EventFlavor::Gain ), peer,"P2P: Received planet info.");
			}



			void OnEvent(TCP::event_t ev, TCP::Peer&peer)
			{
				switch (ev)
				{
					case TCP::Event::ConnectionFailed:
					{
						TCP::PPeer p;
						{
							LogEvent(EventClass( EventSignificance::Uncommon, EventFlavor::Gain ), peer,"P2P: Outbound client connection failed");
							foreach (clients,cl)
								if (cl->get() == &peer)
								{
									p = *cl;
									break;
								}	
						}
						ASSERT__(p);
						connecting.UnsetEntry(p);
					}
					break;
					case TCP::Event::ConnectionEstablished:
					{
						TCP::PPeer p = server.GetSharedPointerOfClient(&peer);
						if (p)
						{
							LogEvent(EventClass( EventSignificance::Uncommon, EventFlavor::Gain ), peer,"P2P: Inbound client connected");
						}
						else
						{
							LogEvent(EventClass( EventSignificance::Uncommon, EventFlavor::Gain ), peer,"P2P: Outbound client connected");
							foreach (clients,cl)
								if (cl->get() == &peer)
								{
									p = *cl;
									break;
								}	
						}
						ASSERT__(p);	//should not be possible for this to be false. Either it's a passive client, or it's actively created
						connecting.UnsetEntry(p);
						PAttachment attach (new Attachment());
						p->attachment = attach;
						attach->challenge.Generate();
						Challenge ch;
						ch.challengeBytes = attach->challenge;
						ch.serverVersion = SERVER_NAME;
						challenge.SendTo(peer,ch,0);
					}
					break;
					case TCP::Event::ConnectionClosed:
					case TCP::Event::ConnectionLost:
					{
						if (peer.attachment)
						{
							RemoveActiveAttachment(To<Attachment>::Cast(peer.attachment));
						}
						LogEvent(EventClass( EventSignificance::Uncommon, EventFlavor::ServerPrivate ),peer,"P2P: Client disconnected ("+peer.GetError(true)+")");
						foreach (clients,cl)
							if (cl->get() == &peer)
							{
								clients.erase(cl);
								break;
							}
					}
					break;
	
	
				}
			}

			void OnSignal(UINT32 signal, TCP::Peer&peer)
			{
			}
		}
	}

	void EstablishPlanetLink(const String & remoteAddress)
	{
		using namespace Detail;
		ASSERT__(server.IsOnline());
		PClient&cl = clients.Append();
		cl->RelayTo(&server);
		cl->ConnectAsync(remoteAddress);
		connecting.Set(remoteAddress,cl);
	}

	bool IsEstablishingPlanetLink(const String & remoteAddress)
	{
		return Detail::connecting.IsSet(remoteAddress);
	}

	count_t CountLinkedPlanets()
	{
		return Detail::IO::activeAttachments.Count();
	}

	const TLink&	GetLink(index_t index)
	{
		return Detail::IO::activeAttachments[index]->link;
	}

	index_t FindLinkFrom(UINT32 lowCellID)
	{
		index_t at = InvalidIndex;
		Detail::IO::locationAttachmentMap.Query(lowCellID,at);
		return at;
	}

}

void PeerNetwork::Create(USHORT servicePort, const ID & myLogin, const float3 & myLocation, time_t time0, time_t secondsPerRound)
{
	Create(servicePort,myLogin,myLocation);
	Detail::haveTimeConfig = true;
	Detail::time0 = time0;
	Detail::secondsPerRound = secondsPerRound;
}

void PeerNetwork::Create(USHORT servicePort, const ID & myLogin, const float3 & myLocation)
{
	using namespace Detail;

	Destroy();

	if (!serverIsConfigured)
	{
		serverIsConfigured = true;

		server.InstallChannel(IO::challenge);
		server.InstallChannel(IO::challengeResponse);
		server.InstallChannel(IO::planetInfo);
		//server.InstallChannel(Channels::entityDirectCommand);
		//server.InstallChannel(Channels::choiceIntersection);
		//server.InstallChannel(Channels::dropLastTask);
		//server.InstallChannel(Channels::dropAllTasks);
		//server.InstallChannel(Channels::signalNewJob);
		//server.InstallChannel(Channels::signalTerrainMod);
		//server.InstallChannel(Channels::constructionAbort);
		//server.InstallChannel(Channels::constructionBegin);
		//server.InstallChannel(Channels::enableStructure);
		//server.InstallChannel(Channels::clientFactionRequest);
		//server.InstallChannel(Channels::removeEntity);
		//server.InstallChannel(Channels::restoreCoreLink);
		//server.InstallChannel(Channels::automate);
		//server.InstallChannel(Channels::sleep);
		//server.InstallChannel(Channels::createMarker);
		//server.InstallChannel(Channels::destroyMarker);
		//server.InstallChannel(Channels::touchMarker);
		//server.InstallChannel(Channels::changeMyName);
		//server.InstallChannel(Channels::identifyUserRecv);
		//server.InstallChannel(Channels::chatMessage);
		//server.InstallChannel(Channels::planetConfig);
		//server.InstallChannel(Channels::gameConfig);

		//server.OpenSignalChannel(Protocol::ChannelID::RequestRoundEnd,(UINT32)UserLevel::CommonFaction);
		//server.OpenSignalChannel(Protocol::ChannelID::RequestContinueRound,(UINT32)UserLevel::CommonFaction);
		//server.OpenSignalChannel(Protocol::ChannelID::Ping,0);
		//server.OpenSignalChannel(Protocol::ChannelID::RequestEverything,(UINT32)UserLevel::Spectator);
		//server.OpenSignalChannel(Protocol::ChannelID::EndConfigurationMode,(UINT32)UserLevel::PlanetConfigurator);
		
		server.onSignal = IO::OnSignal;
		server.SetAsync(false);
		server.onEvent = IO::OnEvent;
		server.onIgnorePackage = Receiver::OnIgnorePackage;
		server.onDeserializationFailed = Receiver::OnDeserializationFailed;
	}
	Detail::myLogin = myLogin;
	myID.Generate();
	Detail::myLocation = myLocation;
	keys.Set(myLogin.publicID,myLogin);
	server.StartService(servicePort);
}

void PeerNetwork::Destroy()
{
	using namespace Detail;
	server.Shutdown();
	clients.Clear();
	keys.Clear();
	haveTimeConfig = false;
	IO::activeAttachments.Clear();
	IO::locationAttachmentMap.Clear();
}

bool PeerNetwork::IsOnline()
{
	return Detail::server.IsOnline();
}

void PeerNetwork::AcceptRemoteIdentifier(const ID & id)
{
	ASSERT__(id.publicID != Detail::myLogin.publicID || id.secret == Detail::myLogin.secret);
	Detail::keys.Set(id.publicID,id);
}

bool PeerNetwork::HaveRemoteIdentifier(const ID & id)
{
	ID stored;
	return Detail::keys.Query(id.publicID,stored) && stored == id;
}

bool PeerNetwork::HaveRemoteIdentifier(const HashValue & publicID)
{
	return Detail::keys.IsSet(publicID);;
}

void PeerNetwork::RemoveRemoteIdentifier(const ID & id)
{
	ASSERT__(id.publicID != Detail::myLogin.publicID);
	Detail::keys.Unset(id.publicID);
}



void	PeerNetwork::Resolve()
{
	Detail::server.Resolve();
}


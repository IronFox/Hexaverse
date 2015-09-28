#include <global_root.h>
#include "techniteLogicLink.h"
#include "network.h"
#include "../../shared/monitoredProcess.h"
#include <io/file_system.h>

namespace TechniteLogicLink
{

	class PeerAttachment : public TCP::Client::Attachment
	{
	public:
		String			secret;
		//index_t			techniteGenerationIndex = InvalidIndex;
	};

	typedef std::shared_ptr<PeerAttachment>	PPeerAttachment;
	static TCP::Server		logicServer;
	static Buffer<Technites::Technite::TCompactState>	recordedHistory;	//all technite states received since last flush



	static void SendWorldDataTo(TCP::Peer&peer)
	{
		LogEvent(EventType::SessionRelated,nullptr,peer.ToString()+ ": Technite logic: Sending world data to client");
		Protocol::WorldDelta delta;
		TechniteProtocol::GridDelta outDelta;
		for (int i = 0; i < BaseGrid::NumFamilies; i++)
		{
			count_t range = Grid::high.grid.CountFamilyMembers(i);
			index_t offset = Grid::high.grid.GetFamilyOffset(i);

			Protocol::GenerateWorldDelta(nullptr,&Grid::actualWorld,delta,(BYTE)i,true,false,Protocol::WorldDeltaType::GlobalCopy);

			outDelta.nodeCount = (UINT32)range;
			outDelta.nodeOffset = (UINT32)offset;
			outDelta.data0.adoptData(delta.data0);
			outDelta.data1.adoptData(delta.data1);
			outDelta.data2.adoptData(delta.data2);
			Channels::gridDelta.SendTo(peer,outDelta);
		}
	}
	static void SendWorld(TCP::Peer&peer)
	{
		LogEvent(EventType::SessionRelated,nullptr,peer.ToString()+ ": Technite logic: Sending grid layout to client");
		TechniteProtocol::NodeChunk chunk;
		for (int i = 0; i < BaseGrid::NumFamilies; i++)
		{
			count_t range = Grid::high.grid.CountFamilyMembers(i);
			index_t offset = Grid::high.grid.GetFamilyOffset(i);
			chunk.isLast = i+1 == BaseGrid::NumFamilies;
			chunk.nodes.SetSize(range);
			for (index_t j = 0; j < range; j++)
			{
				auto&out = chunk.nodes[j];
				const auto&in = Grid::high.grid.GetCell(j+offset);

				out.neighbors.SetSize(in.neighborhood.numNeighbors);
				out.stackBase = in.center * Grid::ToRadius(Grid::CellStack::BottomHeight);
				out.stackDirection = in.center;
				for (int k = 0; k < in.neighborhood.numNeighbors; k++)
					out.neighbors[k] = in.neighborhood.neighborID[k];
			}
			Channels::nodeChunk.SendTo(peer,chunk);
		}
		TechniteProtocol::GridConfig cfg;
		cfg.heightPerLayer = Grid::SingleLayerHeight;
		cfg.numLayersPerStack = Grid::CellStack::Range;
		Channels::gridConfig.SendTo(peer,cfg);

		SendWorldDataTo(peer);
	}


	void		SignalTerrainReset()
	{
		if (!logicServer.HasAnyClients())
			return;
		LogEvent(EventType::SessionRelated,nullptr,"Technite logic: Sending world data to all client");
		Protocol::WorldDelta delta;
		TechniteProtocol::GridDelta outDelta;
		for (int i = 0; i < BaseGrid::NumFamilies; i++)
		{
			count_t range = Grid::high.grid.CountFamilyMembers(i);
			index_t offset = Grid::high.grid.GetFamilyOffset(i);

			Protocol::GenerateWorldDelta(nullptr,&Grid::actualWorld,delta,(BYTE)i,true,false,Protocol::WorldDeltaType::GlobalCopy);

			outDelta.nodeCount = (UINT32)range;
			outDelta.nodeOffset = (UINT32)offset;
			outDelta.data0.adoptData(delta.data0);
			outDelta.data1.adoptData(delta.data1);
			outDelta.data2.adoptData(delta.data2);

			logicServer.VisitAllPeers([&outDelta](const TCP::PPeer&peer)
			{
				if (peer->userLevel > 0)
					Channels::gridDelta.SendTo(peer,outDelta);
			});
		}
		recordedHistory.Clear();
		Protocol::TechniteChunk chunk;
		chunk.first = true;
		logicServer.VisitAllPeers([&chunk](const TCP::PPeer&peer)
		{
			if (peer->userLevel > 0)
				Channels::techniteStateChunk.SendTo(peer,chunk);
		});
	}


	namespace Receiver
	{
		void TechniteInstructionChunk(Protocol::TechniteInstructions&chunk)
		{
			//re-route to server as is
			Network::Channels::techniteInstructions.SendTo(Network::client,chunk);
		}

		void Authenticate(Package::NetString<64>&secret, TCP::Peer&peer)
		{
			PPeerAttachment attachment = To<PeerAttachment>::DynamicCast(peer.attachment);

			if (!attachment || attachment->secret != secret)
			{
				LogEvent(EventType::UnexpectedBehavior,nullptr,peer.ToString()+ ": Client secret does not match local secret. Closing connection.");
				peer.Disconnect();
				return;
			}
			LogEvent(EventType::SessionRelated,nullptr,peer.ToString()+": Authenticated.");

			SendWorld(peer);

			//if (Network::amTechniteFaction)
			{
				//Technites::PFaction tf = To<Technites::Faction>::Cast(Network::myFaction);

				Protocol::TechniteChunk chunk;
				chunk.first = true;

				count_t numChunks = recordedHistory.Count() / Protocol::MaxTechnitesPerChunk;
				if (recordedHistory.Count() % Protocol::MaxTechnitesPerChunk)
					numChunks++;
				index_t offset = 0;
				for (index_t i = 0; i < numChunks; i++)
				{
					chunk.first = i == 0;
					count_t numTechs = std::min(Protocol::MaxTechnitesPerChunk,recordedHistory.Count() - offset);
					chunk.technites.SetSize(numTechs);
					chunk.technites.copyFrom(recordedHistory+offset,numTechs);

					Channels::techniteStateChunk.SendTo(peer,chunk);

					offset += numTechs;
				}


			}
			peer.userLevel = 1;
		}
	}





	namespace Channels
	{
		TCP::Channel<Package::NetString<64>,0,
					TechniteProtocol::ChannelID::Authenticate>						authenticate(Receiver::Authenticate);
		TCP::Channel<Protocol::TechniteInstructions,1,
					TechniteProtocol::ChannelID::TechniteInstructionChunk>			techniteInstructionChunk(Receiver::TechniteInstructionChunk);
		TCP::ObjectSender<Protocol::TechniteChunk,
					TechniteProtocol::ChannelID::TechniteStateChunk>				techniteStateChunk;
		TCP::ObjectSender<TechniteProtocol::GridConfig,
					TechniteProtocol::ChannelID::GridConfig>						gridConfig;
		TCP::ObjectSender<TechniteProtocol::NodeChunk,
					TechniteProtocol::ChannelID::NodeChunk>							nodeChunk;
		TCP::ObjectSender<TechniteProtocol::GridDelta,
					TechniteProtocol::ChannelID::GridDelta>							gridDelta;

	}


	static MonitoredProcess	logicProcess;
	static String			lastSecret;

	//static index_t			techniteGenerationIndex = 0;


	static void Init(TCP::Peer&peer)
	{
		PPeerAttachment attachment(new PeerAttachment());
		attachment->secret = lastSecret;
		//lastSecret = "";

		peer.attachment = attachment;
	}



	void		OnEvent(TCP::event_t ev, TCP::Peer&peer)
	{
		switch (ev)
		{
			case TCP::Event::ConnectionEstablished:
			{
				LogEvent(EventType::SessionRelated, nullptr, "TechniteLogic: Peer connected: "+peer.ToString());
				Init(peer);

			}
			break;
			case TCP::Event::ConnectionFailed:
			{
			}
			break;
			case TCP::Event::ConnectionLost:
			case TCP::Event::ConnectionClosed:
			{
				LogEvent(EventType::SessionRelated, nullptr, "TechniteLogic: Peer connection lost: "+peer.ToString());
			}
			break;
		}
	}

	static void		OnSignal(UINT32 signal, TCP::Peer&origin)
	{
		//none supported for now
	}

	UINT16 myPort;

	void		Create()
	{
		static bool setup = false;
		if (!setup)
		{
			setup = true;

			logicServer.InstallChannel(Channels::techniteInstructionChunk);
			logicServer.InstallChannel(Channels::authenticate);

			logicServer.SetAsync(false);	//can't be async because of world grid. damn
			logicServer.onEvent = OnEvent;
			logicServer.onSignal = OnSignal;
		}

		if (!logicServer.IsOnline())
		{
			if (!logicServer.StartService(0,&myPort))
			{
				FATAL__(logicServer.GetError(false));
			}

			CryptographicID id;
			id.Generate();

			lastSecret = id.ToString(32);	//32 bytes => 64 characters. actually the full id
		}

		if (!logicProcess.Start(FileSystem::GetWorkingDirectory(),"TechniteLogic/Client.exe",String(myPort)+" "+lastSecret))
		{
			FATAL__("Failed to launch technite logic");
		}
	}



	void		Forward(Protocol::TechniteChunk&chunk)
	{
		if (chunk.first)
			recordedHistory.Clear();
		recordedHistory.Append(chunk.technites);
		logicServer.VisitAllPeers([&chunk](const TCP::PPeer&peer)
		{
			if (peer->userLevel > 0)
			{
				Channels::techniteStateChunk.SendTo(peer,chunk);
			}
		});
	}


	void		SendDelta(Protocol::WorldDelta&delta)
	{
		if (!logicServer.HasAnyClients())
			return;
		TCP::Serial<TechniteProtocol::GridDelta>	serial;
		TechniteProtocol::GridDelta&outDelta = serial.Ref();
		BYTE family = delta.familyAndLastFlag & 0x7F;

		count_t range = Grid::high.grid.CountFamilyMembers(family);
		index_t offset = Grid::high.grid.GetFamilyOffset(family);

		outDelta.nodeCount = (UINT32)range;
		outDelta.nodeOffset = (UINT32)offset;
		outDelta.data0.swap(delta.data0);
		outDelta.data1.swap(delta.data1);
		outDelta.data2.swap(delta.data2);

		serial.Seal();



		logicServer.VisitAllPeers([&serial](const TCP::PPeer&peer)
		{
			if (peer->userLevel > 0)
				Channels::gridDelta.SendTo(peer,serial);
		});

		//swap back:
		outDelta.data0.swap(delta.data0);
		outDelta.data1.swap(delta.data1);
		outDelta.data2.swap(delta.data2);
	}


	void		ProcessEvents()
	{
		if (!logicServer.IsOnline())
			return;
		logicServer.Resolve();
		logicProcess.Check();
	}


	void		Destroy()
	{
		logicServer.EndService();
		logicProcess.Terminate();
	}


}

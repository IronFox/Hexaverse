#include <global_root.h>
#include "techniteLogicLink.h"
#include "network.h"
#include "../../shared/monitoredProcess.h"
#include <io/file_system.h>

#include "dialog.h"

extern void	SignalRoundEnd();

namespace TechniteLogicLink
{
	String autoStartClientPath="TechniteLogic/Client.exe";
	bool autoStartClient=true;
	
	UINT16 initialServerPortNumber = 0,
			actualServerPortNumber = 0;

	static const String DialogName = "TechniteFactionLink";

	Dialog::WDialog		connectionNotification;
	count_t numClients = 0;


	class PeerAttachment : public TCP::Client::Attachment
	{
	public:
	};

	typedef std::shared_ptr<PeerAttachment>	PPeerAttachment;
	static TCP::Server		logicServer;
	static Buffer<Technites::Technite::TCompactState>	recordedHistory;	//all technite states received since last flush

	TechniteProtocol::WorldInfo lastWorldInfo = TechniteProtocol::WorldInfo(0);

	void		SetWorldInfo(Grid::Content coreContent)
	{
		lastWorldInfo = TechniteProtocol::WorldInfo(  coreContent );
		logicServer.VisitAllPeers([](const TCP::PPeer&peer)
		{
			if (peer->userLevel > 0)
				Channels::worldInfo.SendTo(*peer,lastWorldInfo);
		});
		
	}



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
		cfg.matterYieldByContentType.SetSize(Grid::Content::NumberOfPossibilities);
		static_assert(Grid::Content::NumberOfPossibilities == ARRAYSIZE(Technites::Technite::MatterYield),"Field size mismatch");
		for (index_t i = 0; i < Grid::Content::NumberOfPossibilities; i++)
			cfg.matterYieldByContentType[i] = Technites::Technite::MatterYield[i];
		Channels::gridConfig.SendTo(peer,cfg);

		if (lastWorldInfo != 0)
			Channels::worldInfo.SendTo(peer,lastWorldInfo);


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


	void		ExecuteLogic()
	{
		logicServer.VisitAllPeers([](const TCP::PPeer&peer)
		{
			if (peer->userLevel > 0)
				peer->SendSignal(TechniteProtocol::ChannelID::InstructTechnites);
		});
	}


	namespace Receiver
	{
		void TechniteInstructionChunk(Protocol::TechniteInstructions&chunk)
		{
			//re-route to server as is
			LogEvent(EventType::TechniteLogic,nullptr,"Forwarding technite instruction chunk to server, containing "+String(chunk.instructions.Count())+" technites, starting from "+String(chunk.offset));
			Network::Channels::techniteInstructions.SendTo(Network::client,chunk);
			
			if (Network::myFaction && Network::myFaction->IsTechniteFaction())
			{
				Technites::PFaction pf = To<Technites::Faction>::Cast(Network::myFaction);

				count_t end = chunk.instructions.Count() + chunk.offset;
				if (end <= pf->technites.Count())
					for (index_t i = 0; i < chunk.instructions.Count(); i++)
					{
						index_t at = i+chunk.offset;
						pf->technites[at].Set(chunk.instructions[i]);
						Grid::high.family[Grid::high.grid.GetCell(pf->technites[at].location.GetStackID()).family].SetOutdated(0.f);
					}
				else
					LogEvent(EventType::TechniteLogic,nullptr,"Local technite field ("+String(pf->technites.Count())+") does not support that many technite instructions ("+String(end)+"). How weird");
			}

			if (chunk.offset + chunk.instructions.Count() == recordedHistory.Count())
			{
				LogEvent(EventType::TechniteLogic,nullptr,"All technites instructed");
				
				if (Network::amTechniteFaction && !Network::amRegularFaction)
				{
					//auto-new-round, if set. Not sure if this should be set
					//SignalRoundEnd();
				}
			}
		}

		void Ready(TCP::Peer&peer)
		{
			LogEvent(EventType::TechniteLogic,nullptr,peer.ToString()+": Ready");

			SendWorld(peer);

			//if (Network::amTechniteFaction)
			{
				//Technites::PFaction tf = To<Technites::Faction>::Cast(Network::myFaction);

				Protocol::TechniteChunk chunk;
				chunk.first = true;

				count_t numChunks = recordedHistory.Count() / Protocol::MaxTechnitesPerChunk;
				if (recordedHistory.Count() % Protocol::MaxTechnitesPerChunk)
					numChunks++;
				LogEvent(EventType::TechniteLogic,nullptr,"Sending "+String(numChunks)+" technite state chunks to logic client");
				index_t offset = 0;
				for (index_t i = 0; i < numChunks; i++)
				{
					chunk.first = i == 0;
					count_t numTechs = std::min(Protocol::MaxTechnitesPerChunk,recordedHistory.Count() - offset);
					chunk.technites.SetSize(numTechs);
					chunk.technites.copyFrom(recordedHistory+offset,numTechs);

					LogEvent(EventType::TechniteLogic,nullptr," Sending chunk containing "+String(numTechs)+" technites, starting from "+String(offset)+", flagged as first="+String(chunk.first));
					Channels::techniteStateChunk.SendTo(peer,chunk);

					offset += numTechs;
				}


			}
			LogEvent(EventType::TechniteLogic,nullptr," Elevating user level");
			peer.userLevel = 1;

			numClients ++;

			if (!connectionNotification.expired())
			{
				Dialog::RemoveDialog(connectionNotification.lock());
				ASSERT__(connectionNotification.expired());
				connectionNotification.reset();

			}
		}

		
	}





	namespace Channels
	{
		//TCP::Channel<Package::NetString<64>,0,
		//			TechniteProtocol::ChannelID::Authenticate>						authenticate(Receiver::Authenticate);
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
		TCP::ObjectSender<TechniteProtocol::WorldInfo,
					TechniteProtocol::ChannelID::WorldInfo>							worldInfo;

	}


	static MonitoredProcess	logicProcess;

	//static index_t			techniteGenerationIndex = 0;


	static void Init(TCP::Peer&peer)
	{
		PPeerAttachment attachment(new PeerAttachment());

		peer.attachment = attachment;
	}

	void RefreshUserNotification();

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
				if (peer.userLevel > 0)
				{
					numClients--;
					if (numClients == 0 && !autoStartClient)
					{
						RefreshUserNotification();
					}
				}
				LogEvent(EventType::SessionRelated, nullptr, "TechniteLogic: Peer connection lost");
			}
			break;
		}
	}

	static void		OnSignal(UINT32 signal, TCP::Peer&origin)
	{
		switch (signal)
		{
			case TechniteProtocol::ChannelID::Ready:
				Receiver::Ready(origin);
			break;
			case TechniteProtocol::ChannelID::RequestNextRound:
				SignalRoundEnd();
			break;
		}
		//none supported for now
	}


	void		Create()
	{
		static bool setup = false;
		if (!setup)
		{
			setup = true;

			logicServer.InstallChannel(Channels::techniteInstructionChunk);
//			logicServer.InstallChannel(Channels::authenticate);
			logicServer.OpenSignalChannel(TechniteProtocol::ChannelID::Ready,0);
			logicServer.OpenSignalChannel(TechniteProtocol::ChannelID::RequestNextRound,1);

			logicServer.SetAsync(false);	//can't be async because of world grid. damn
			logicServer.onEvent = OnEvent;
			logicServer.onSignal = OnSignal;
		}

		if (!logicServer.IsOnline())
		{
			logicProcess.Terminate();
			if (!logicServer.StartService(initialServerPortNumber,true,&actualServerPortNumber))
			{
				FATAL__(logicServer.GetError(false));
			}
		}

		if (autoStartClient)
		{
			if (!logicProcess.IsStarted() && !logicProcess.Start(FileSystem::GetWorkingDirectory(),autoStartClientPath,String(actualServerPortNumber)))
				FATAL__("Failed to launch technite logic");
		}

		RefreshUserNotification();
	}

	void 	RefreshUserNotification()
	{
		if (AwaitsClient())
		{
			if (!connectionNotification.expired() && initialServerPortNumber == 0)
			{
				Dialog::RemoveDialog(connectionNotification.lock());
				ASSERT__(connectionNotification.expired());
				connectionNotification.reset();
			}

			if (connectionNotification.expired())
				connectionNotification = Dialog::PermanentMessage("Technite logic link active, no logic clients connected. Please connect logic client on port "+String(actualServerPortNumber)+" to continue",[]()->Dialog::CheckResult
				{
					return AwaitsClient() ? Dialog::CheckResult::OkayContinue : Dialog::CheckResult::Abort;
				});
			else
				Dialog::SignalSituationChange();
		}
		else
			if (!connectionNotification.expired())
			{
				Dialog::RemoveDialog(connectionNotification.lock());
				ASSERT__(connectionNotification.expired());
				connectionNotification.reset();
			}
	}

	bool		AwaitsClient()
	{
		return !autoStartClient && logicServer.IsOnline() && !logicServer.HasAnyClients();
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
		numClients = 0;
		recordedHistory.Clear();
	}


}

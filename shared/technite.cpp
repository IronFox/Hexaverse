#include <global_root.h>
#include "technite.h"
#include <ppl.h>
#include <io/file_system.h>
#include "objects/gridAndChoices.h"
#include "protocol.h"

#ifndef CLIENT
	#include "../server/components/journalSaveGame.h"
	void CheckSendTerrain();
#endif


namespace Technites
{
	#ifdef CLIENT
		Array<String>			supportedTypes;
	#endif



	const EventClass techniteError = EventClass(EventSignificance::NonFatalError, EventFlavor::SystemError);
	const EventClass regularEvent = EventClass(EventSignificance::Common, EventFlavor::ServerPrivate);

	#ifndef CLIENT

		namespace Attachment
		{
			class Challenge : public TCP::Peer::Attachment
			{
			public:
				CryptographicID		challenge;

				/**/				Challenge()
				{
					challenge.Generate();
				}
			};

			class Technite :  public TCP::Peer::Attachment
			{
			public:
				const TUniqueID		techniteID;

				/**/				Technite(const TUniqueID&uuid):techniteID(uuid)
				{}
			};


		}

		namespace Protocol
		{

			#pragma pack(push)  /* push current alignment to stack */
			#pragma pack(1)     /* set alignment to 1 byte boundary */

			ALIGN1
			struct TerrainStateCell
			{
				UINT32		compressedLocation;
				BYTE		content;
			};
			#pragma pack(pop)  /* push current alignment to stack */

			static_assert(sizeof(TerrainStateCell)==5,"Fix struct alignment");



			NETSTRUCT2(CommonTechniteState
				, NET(Technite::CompressedLocation),	location
				, NET(Technite::Resources),			resources
			);
		
			NETSTRUCT4(OwnState
				, CommonTechniteState, commonState
				, NET(Technite::TaskResult), taskResult
				, NET(Technite::CompressedState), state
				, NET(float), visionRadius
			);

			NETSTRUCT2(OtherState
				, NET(TUniqueID), id
				, CommonTechniteState, commonState
			);


			NETSTRUCT6(EntityContact
				, NET(TUniqueID), id
				, Package::NetString<256>, type
				, NET(BYTE), height
				, NET(Technite::CompressedLocation), location
				, NET(UINT16), currentHealth
				, NET(UINT16), maxHealth
			);
			NETSTRUCT2(Message
				, NET(TUniqueID), sender
				, Package::NetString<1024>, message
			);

			NETSTRUCT3(Authenticate
				, NET(TUniqueID), myID
				, NET(SHA256::HashContainer), challengeResponse
				, Package::NetString<256>, protocolVersion
			);

			NETSTRUCT3(RegularInstruction
				, NET(BYTE), nextTask
				, NET(BYTE), relativeTarget
				, NET(BYTE), taskParameter
			);

			NETSTRUCT3(MessageInstruction
				, NET(TUniqueID), receiver
				, NET(BYTE), receiverLocation
				, Package::NetString<1024>, message
			);

			NETSTRUCT2(ProcessRound
				, NET(UINT32), roundNumber
				, NET(UINT32), techniteSubRoundNumber
			);

			
			NETSTRUCT3(GridNode
                , NETA(UINT32), neighbors
                , NET(TVec3<>), stackBase
				, NET(TVec3<>), stackDirection
            );

			NETSTRUCT5(GridConfig
				, NET(float), heightPerLayer
				, NET(INT32), numLayersPerStack
				, NETA(BYTE), initialTTLAtLayer
				, NETA(BYTE), energyYieldAtLayer
				, NET(BYTE), coreType
			);

			NETSTRUCT2(NodeChunk
				, NET(bool), isLast
				, NETA(GridNode), nodes
			);

		}



		namespace UserLevel
		{
			enum t
			{
				None,
				Authenticated


			};

		}

		namespace Receiver
		{
			namespace Handler
			{
				void	Authenticate(Protocol::Authenticate&authenticate, TCP::Peer&origin);
				void	RegularInstruction(Protocol::RegularInstruction&, TCP::Peer&origin);
				void	MessageInstruction(Protocol::MessageInstruction&, TCP::Peer&origin);
			}


			void OnDeserializationFailed(UINT32 channel, TCP::Peer&origin)
			{
				LogEvent(EventClass::Preset::networkError,nullptr,"Technite service: "+origin.ToString()+": Deserialization failed on channel "+String(channel));
			}



			void OnIgnorePackage(UINT32 channel,UINT32 size, TCP::Peer&origin)
			{
				LogEvent(EventClass::Preset::networkError,nullptr,"Technite service: "+origin.ToString()+": Package of size "+String(size)+" ignored on channel "+String(channel));
			}

			void OnSignal(UINT32 channel, TCP::Peer&origin)
			{
				LogEvent(EventSignificance::Debug,nullptr,"Technite service: "+origin.ToString()+": Signal on channel "+String(channel));
			}

			void OnEvent(TCP::event_t event, TCP::Peer&origin);

		}

		namespace Channels
		{
			enum channel_t
			{
				Unused,
				Challenge,			//	s2c : CryptographicID
				Authenticate,		//	c2s : Authenticate
				Authenticated,		//	s2c : <signal>
				NodeChunk,			//	s2c : NodeChunk
				GridConfig,			//	s2c : GridConfig
				StateUpdateBegin,	//	s2c : <signal>
				EntityContact,		//	s2c : EntityContact
				OwnTechniteState,	//	s2c : OwnState
				OtherTechniteState,	//	s2c : OtherState
				TerrainState,		//	s2c : NETA<BYTE>
				Message,			//	s2c : Message
				IsDead,				//	s2c : <signal>
				RegularInstruction,	//	c2s : RegularInstruction
				MessageInstruction,	//	c2s : MessageInstruction
				ProcessRound,		//	s2c : ProcessRound

				ChannelCount
			};


			TCP::SignalSender<StateUpdateBegin>	stateUpdateBegin;
			TCP::SignalSender<IsDead>			isDead;
			TCP::SignalSender<Authenticated>	authenticated;
			TCP::ObjectSender<Protocol::EntityContact,EntityContact>	entityContact;
			TCP::ObjectSender<Protocol::OwnState,OwnTechniteState>	ownTechniteState;
			TCP::ObjectSender<Protocol::OtherState,OtherTechniteState>	otherTechniteState;
			TCP::ObjectSender<NETA(Protocol::TerrainStateCell),TerrainState>	terrainState;
			TCP::ObjectSender<Protocol::Message, Message> message;
			TCP::ObjectSender<NET(CryptographicID), Challenge> challenge;
			TCP::ObjectSender<Protocol::ProcessRound, ProcessRound> processRound;
			TCP::ObjectSender<Protocol::NodeChunk, NodeChunk> nodeChunk;
			TCP::ObjectSender<Protocol::GridConfig, GridConfig> gridConfig;

			TCP::Channel<Protocol::MessageInstruction, UserLevel::Authenticated, MessageInstruction>	messageInstruction(Receiver::Handler::MessageInstruction);
			TCP::Channel<Protocol::RegularInstruction, UserLevel::Authenticated, RegularInstruction>	regularInstruction(Receiver::Handler::RegularInstruction);
			TCP::Channel<Protocol::Authenticate, UserLevel::None, Authenticate>							authenticate(Receiver::Handler::Authenticate);
		}



		static void SendWorldStructure(TCP::Peer&peer)
		{
			LogEvent(regularEvent,peer.ToString()+ ": Technite logic: Sending grid layout to client");
			Protocol::NodeChunk chunk;
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
			Protocol::GridConfig cfg;
			cfg.heightPerLayer = Grid::SingleLayerHeight;
			cfg.numLayersPerStack = Grid::CellStack::Range;
			cfg.energyYieldAtLayer.SetSize(Grid::CellStack::Range);
			cfg.initialTTLAtLayer.SetSize(Grid::CellStack::Range);
			for (UINT32 layer = 0; layer < Grid::CellStack::Range; layer++)
			{
				int h = Grid::CellStack::ToHeight(layer);
				cfg.energyYieldAtLayer[layer] = Technites::Technite::GetLightEnergyYieldAtHeight(h);
				cfg.initialTTLAtLayer[layer] = Technites::Technite::GetInitialTTLAtLayer(layer);
			}
			cfg.coreType = Grid::CellStack::bottom.content;
			Channels::gridConfig.SendTo(peer,cfg);
		}

	#endif

	/*static*/ FixedArray<BYTE,Technite::TaskCount>		Technite::energyCost =
	{
		0,	//None
		0,	//Scan
		1,	//Message
		1,	//Move
		0,	//Transfer
		0,	//Replace
		0,	//End
		0,	//Spawn
		1,	//Kill
	};



	/*static*/ FixedArray<ExecutionLayer::T,Technite::TaskCount>	Technite::layer= 
	{
		ExecutionLayer::Count,	//None
		ExecutionLayer::Scan,	//Scan
		ExecutionLayer::Messages,	//Message
		ExecutionLayer::ReplaceMoveSpawn,	//Move
		ExecutionLayer::Transfer,	//Transfer
		ExecutionLayer::ReplaceMoveSpawn,	//Replace
		ExecutionLayer::End,	//End
		ExecutionLayer::ReplaceMoveSpawn,	//Spawn
		ExecutionLayer::Kill,	//Kill
	};

	AtomicGrid					atomicGrid;
	Buffer<Technite>			technites;
	GenericHashTable<TUniqueID,index_t,TUniqueID>	techniteIDMap;
	BroadTechniteMap			techniteMap;

	#ifndef CLIENT
		TCP::Server				serviceHost;
		String					serviceURL="localhost";

		FixedArray<Buffer0<Technite*>,ExecutionLayer::Count>	layers;

		index_t					currentLayer = 0;
		volatile bool			anyDied = false;
		volatile bool			familyChanged[BaseGrid::NumFamilies] = {false};
		WorkPipe<TNewTechnite>	newTechnites;
	#endif

	Technite::Resources			GetCost(Technite::Task, BYTE parameter, BYTE currentGridContent);

	void						CheckTechniteFoundations(float t);


	#ifndef CLIENT
		float						timeStamp=0;

		void						ProcessLighting(Technite&technite);


		void Receiver::OnEvent(TCP::event_t event, TCP::Peer&origin)
		{
			LogEvent(EventSignificance::Debug,nullptr,"TechniteControl: "+origin.ToString()+": "+TCP::event2str(event));
			switch (event)
			{
				case TCP::Event::ConnectionEstablished:
				{
					Attachment::Challenge * attach = new Attachment::Challenge();
					origin.attachment.reset(attach);
					Channels::challenge.SendTo(origin,attach->challenge);
				}
				break;

			}
		}
	
		void	Receiver::Handler::RegularInstruction(Protocol::RegularInstruction&inst, TCP::Peer&origin)
		{
			Attachment::Technite*attach = dynamic_cast<Attachment::Technite*>(origin.attachment.get());
			if (!attach)
			{
				LogEvent(EventClass::Preset::unexpectedBehavior,origin.ToString()+ " attempted to instruct technite without challenge peer attachment");
				#ifndef _DEBUG
					origin.DisconnectPeer();
				#endif
				return;
			}
			Technite*t = GetTechnite(attach->techniteID);
			if (!t)
			{
				LogEvent(EventClass::Preset::unexpectedBehavior,origin.ToString()+ " attempted to instruct dead technite "+attach->techniteID.ToString());
				#ifndef _DEBUG
					origin.DisconnectPeer();
				#endif
				return;
			}

			t->nextInstruction.nextTask = (Technite::Task) inst.nextTask.value;
			t->nextInstruction.payload = "";
			t->nextInstruction.targetTechnite = TUniqueID::Empty();
			t->nextInstruction.taskTarget = Technite::CompressedTarget(inst.relativeTarget.value);
			t->nextInstruction.taskParameter = inst.taskParameter;
		}

		void	Receiver::Handler::MessageInstruction(Protocol::MessageInstruction&msg, TCP::Peer&origin)
		{
			Attachment::Technite*attach = dynamic_cast<Attachment::Technite*>(origin.attachment.get());
			if (!attach)
			{
				LogEvent(EventClass::Preset::unexpectedBehavior,origin.ToString()+ " attempted to instruct technite without challenge peer attachment");
				#ifndef _DEBUG
					origin.DisconnectPeer();
				#endif
				return;
			}
			Technite*t = GetTechnite(attach->techniteID);
			if (!t)
			{
				LogEvent(EventClass::Preset::unexpectedBehavior,origin.ToString()+ " attempted to instruct dead technite "+attach->techniteID.ToString());
				#ifndef _DEBUG
					origin.DisconnectPeer();
				#endif
				return;
			}

			t->nextInstruction.nextTask = Technite::Task::Message;
			t->nextInstruction.payload = msg.message;
			t->nextInstruction.targetTechnite = msg.receiver;
			t->nextInstruction.taskTarget = Technite::CompressedTarget(msg.receiverLocation.value);
		}

		void	Receiver::Handler::Authenticate(Protocol::Authenticate&authenticate, TCP::Peer&origin)
		{
			Attachment::Challenge*attach = dynamic_cast<Attachment::Challenge*>(origin.attachment.get());
			if (!attach)
			{
				LogEvent(EventClass::Preset::unexpectedBehavior,origin.ToString()+ " attempted authentication without challenge peer attachment");
				#ifndef _DEBUG
					origin.DisconnectPeer();
				#endif
				return;
			}
			SHA256::HashContainer	shouldBe;
			SHA256 sha;
			Technite*t = GetTechnite(authenticate.myID);
			if (!t)
			{
				LogEvent(EventClass::Preset::unexpectedBehavior,origin.ToString()+ " attempted authentication on unknown technite "+authenticate.myID.value.ToString());
				#ifndef _DEBUG
					origin.DisconnectPeer();
				#endif
				return;
			}
			sha.AppendPOD(t->secret);
			sha.AppendPOD(attach->challenge);
			sha.Finish(shouldBe);
			attach = nullptr;
			if (shouldBe == authenticate.challengeResponse)
			{
				origin.attachment.reset(new Attachment::Technite(t->id));
				origin.userLevel = UserLevel::Authenticated;
				t->peer = serviceHost.GetSharedPointerOfClient(&origin);
				Channels::authenticated.SendTo(origin,0);
				LogEvent(EventClass::Preset::uncommon,origin.ToString()+ " authenticated on technite "+authenticate.myID.value.ToString());
				SendWorldStructure(origin);
			}
			else
			{
				LogEvent(EventClass::Preset::unexpectedBehavior,origin.ToString()+ " failed authentication on technite "+t->id.ToString());
				origin.DisconnectPeer();
			}
		}
	#endif

	index_t					GetTechniteIndex(const TUniqueID&id)
	{
		index_t rs;
		if (!techniteIDMap.Query(id,rs))
		{
			//String keyStrings;
			//Array<TUniqueID>	keys;
			//techniteIDMap.exportKeys(keys);
			//foreach (keys,k)
			//	keyStrings += ", "+k->ToFullString();
			//ShowMessage(keyStrings + " <> "+id.ToFullString());


			return InvalidIndex;
		}
		return rs;
	}

	Technite*				GetTechnite(const TUniqueID&id)
	{
		index_t index = GetTechniteIndex(id);
		if (index == InvalidIndex)
			return nullptr;
		ASSERT_LESS__(index,technites.Count());
		return technites + index;
	}

	index_t					GetTechniteIndex(const Grid::HCellIdU&id)
	{
		index_t rs;
		if (!techniteMap.Query(id,rs))
			return InvalidIndex;
		return rs;
	}

	Technite*				GetTechnite(const Grid::HCellIdU&id)
	{
		index_t index = GetTechniteIndex(id);
		if (index == InvalidIndex)
			return nullptr;
		ASSERT_LESS__(index,technites.Count());
		return technites + index;
	}


	#ifndef CLIENT
		void GC()
		{
			if (anyDied)
			{
				anyDied = false;
				static Buffer<Technite>	survivors;
				foreach (technites,t)
					if (t->isDead)
					{
						t->ShutdownClient();
						techniteMap.Unset(t->location.GetU());
					}
					else
						survivors << std::move(*t);
				technites.swap(survivors);
				survivors.Clear();

				techniteIDMap.Clear();
				techniteMap.Clear();
				for (index_t i = 0; i < technites.Count(); i++)
				{
					techniteMap.Set(technites[i].location.GetU(),i);
					techniteIDMap.Set(technites[i].id,i);
				}
			}

		}

		void						BeginProcessing(float t0)
		{
			timeStamp = t0;
			atomicGrid.Fill(0);
			currentLayer = 0;
			anyDied = false;

			foreach (layers,layer)
				layer->Clear();
			foreach (technites,t)
			{
				t->taskResult = Technite::TaskResult::Success;
				if (t->visionRadius >= 0.f)
					t->SetStandardFieldOfVision();
				ProcessLighting(*t);
				ExecutionLayer::T layer = Technite::layer[(index_t)t->nextInstruction.nextTask];
				if (layer == ExecutionLayer::Count)
				{
					t->taskResult = Technite::TaskResult::Success;
					continue;
				}
				layers[layer] << t;
			}

			CheckTechniteFoundations(t0);

		}


		bool ChangeGrid(const Grid::HCellIdU&target, Grid::Content content)
		{
			if (Grid::actualWorld.cellStack[target.stackID].LocalOverwriteU(target.layer,content))
			{
				index_t family1 = Grid::high.grid.GetCell(target.stackID).family;
				familyChanged[family1] = true;
				return true;
			}
			return false;
		}

		void MoveTechnite(Technite&t, const Grid::HCellIdU&target)
		{
			ChangeGrid(t.location.GetU(),Grid::Content::Clear);
			ChangeGrid(target,Grid::Content::Technite);

		
			index_t index = techniteMap.Unset(t.location.GetU());
			ASSERT__(index != InvalidIndex);
			techniteMap.Set(target,index);

			t.location = target;
			t.SetStandardFieldOfVision();
		}

		void ApplyFamilyChange(float t)
		{
			for (index_t i = 0; i < ARRAYSIZE(familyChanged); i++)
			{
				if (familyChanged[i])
				{
					Grid::low.family[i].SetOutdated(t);
					Grid::high.family[i].SetOutdated(t);
					familyChanged[i] = false;
				}
			}
		}

		bool						ProcessNextLayer()
		{
			switch (currentLayer)
			{
				case ExecutionLayer::Scan:
					::Concurrency::parallel_for_each(layers[currentLayer].begin(),layers[currentLayer].end(),[](Technite*t)
					{
						Technite::Resources cost = GetCost(t->nextInstruction.nextTask,t->nextInstruction.taskParameter,Grid::Content::Technite);
						if (!(t->resources -= cost))
						{
							t->taskResult = Technite::TaskResult::InsufficientResources;
							return;
						}
						float radius = sqrt(float(cost.energy));
						t->SetSphericalFieldOfVision(radius);
					});
				break;
				case ExecutionLayer::Messages:
					::Concurrency::parallel_for_each(layers[currentLayer].begin(),layers[currentLayer].end(),[](Technite*t)
					{
						Technite::Resources cost = GetCost(t->nextInstruction.nextTask,t->nextInstruction.taskParameter,Grid::Content::Technite);
						if (!(t->resources -= cost))
						{
							t->taskResult = Technite::TaskResult::InsufficientResources;
							return;
						}
						index_t tIndex;
						if (!techniteIDMap.Query(t->nextInstruction.targetTechnite,tIndex) )
						{
							t->taskResult = Technite::TaskResult::TargetNoTechnite;
							return;
						}
						const Technite&other = technites[tIndex];
						if (Grid::GetQuadraticGeometricDistance(other.location.GetU(),t->location.GetU()) > sqr(MaxTransferRange))
						{
							t->taskResult = Technite::TaskResult::TargetOutOfRange;
							return;
						}

						TCP::PPeer peer = other.peer.lock();
						if (!peer || !Channels::message.SendTo(peer,Protocol::Message(t->id,t->nextInstruction.payload)))
						{
							t->taskResult = Technite::TaskResult::TargetOffline;
							return;
						}
					});
				break;
				case ExecutionLayer::End:
					::Concurrency::parallel_for_each(layers[currentLayer].begin(),layers[currentLayer].end(),[](Technite*t)
					{
						Technite::Resources cost = GetCost(t->nextInstruction.nextTask,t->nextInstruction.taskParameter,Grid::Content::Technite);
						if (! (t->resources -= cost))
						{
							t->taskResult = Technite::TaskResult::InsufficientResources;
							return;
						}
						t->isDead = true;
						anyDied = true;

						ChangeGrid(t->location.GetU(),Grid::Content::Reinterpret(t->nextInstruction.taskParameter));
					});
				break;
				case ExecutionLayer::Kill:
					::Concurrency::parallel_for_each(layers[currentLayer].begin(),layers[currentLayer].end(),[](Technite*t)
					{
						if (t->isDead)
							return;
						Grid::HCellIdU target;
						if (!t->DecodeAbsoluteTarget(target))
						{
							t->taskResult = Technite::TaskResult::TargetOutOfRange;
							return;
						}
						index_t tIndex;
						if (!techniteMap.Query(target,tIndex) || technites[tIndex].id != t->nextInstruction.targetTechnite)
						{
							t->taskResult = Technite::TaskResult::TargetNoTechnite;
							return;
						}
						Technite&other = technites[tIndex];
						Technite::Resources cost = other.resources;
						if (! (t->resources -= cost))
						{
							t->taskResult = Technite::TaskResult::InsufficientResources;
							return;
						}
						if (other.isDead.Set(false,true))
							anyDied = true;
						//else
							//t->resources += cost;	//do not refund: otherwise inconsistent
					});
				break;
				case ExecutionLayer::Transfer:
					::Concurrency::parallel_for_each(layers[currentLayer].begin(),layers[currentLayer].end(),[](Technite*t)
					{
						if (t->isDead)
							return;
						if (!t->nextInstruction.taskParameter)
						{
							t->taskResult = Technite::TaskResult::Success;
							return;
						}
						if (t->resources.energy < t->nextInstruction.taskParameter)
						{
							t->taskResult = Technite::TaskResult::InsufficientResources;
							return;
						}
						Grid::HCellIdU target;
						if (!t->DecodeAbsoluteTarget(target))
						{
							t->taskResult = Technite::TaskResult::TargetOutOfRange;
							return;
						}
						index_t tIndex;
						if (!techniteMap.Query(target,tIndex) || technites[tIndex].id != t->nextInstruction.targetTechnite || technites[tIndex].isDead)
						{
							t->taskResult = Technite::TaskResult::TargetNoTechnite;
							return;
						}
						atomicGrid.cellStack[target.stackID].volumeCell[target.layer].content += t->nextInstruction.taskParameter;
						t->resources -= Technite::Resources(t->nextInstruction.taskParameter);
						t->taskResult = Technite::TaskResult::Success;
					});
					if (layers[currentLayer].IsNotEmpty())
						::Concurrency::parallel_for_each(technites.begin(),technites.end(),[](Technite&t)
						{
							if (t.isDead)
								return;
							Grid::HCellIdU target = t.location.GetU();
							if (atomicGrid.cellStack[target.stackID].volumeCell[target.layer].content)
							{
								t.resources += Technite::Resources(atomicGrid.cellStack[target.stackID].volumeCell[target.layer].content);
								atomicGrid.cellStack[target.stackID].volumeCell[target.layer].content = 0;
							}
						});
				break;
				case ExecutionLayer::ReplaceMoveSpawn:
				{
					::Concurrency::parallel_for_each(layers[currentLayer].begin(),layers[currentLayer].end(),[](Technite*t)
					{
						if (t->isDead)
							return;
						Grid::HCellIdU target;
						if (!t->DecodeAbsoluteTarget(target))
						{
							t->taskResult = Technite::TaskResult::TargetOutOfRange;
							return;
						}
						const auto&cell = Grid::actualWorld.cellStack[target.stackID].GetCellU(target.layer);
						if (cell.structureCounter > 0 || cell.content == Grid::Content::Technite)	//these tasks cannot target structures or technites
						{
							t->taskResult = Technite::TaskResult::TargetBlocked;
							return;
						}
						if (t->resources < GetCost(t->nextInstruction.nextTask,t->nextInstruction.taskParameter, cell.content))
						{
							t->taskResult = Technite::TaskResult::InsufficientResources;
							return;
						}
						if ((t->nextInstruction.nextTask == Technite::Task::Spawn || t->nextInstruction.nextTask == Technite::Task::Move) && !CanStandHere(target))
						{
							t->taskResult = Technite::TaskResult::CannotStandThere;
							return;
						}
						atomicGrid.cellStack[target.stackID].volumeCell[target.layer].content ++;
					});
					::Concurrency::parallel_for_each(layers[currentLayer].begin(),layers[currentLayer].end(),[](Technite*t)
					{
						if (t->isDead)
							return;
						if (t->taskResult != Technite::TaskResult::Success)
							return;
						Grid::HCellIdU target;
						ASSERT__(t->DecodeAbsoluteTarget(target));
						const auto&content = atomicGrid.cellStack[target.stackID].volumeCell[target.layer].content;
						//can't reset here :S
						if (content != 1)
						{
							t->taskResult = Technite::TaskResult::TargetBlocked;
							return;
						}
						const auto&cell = Grid::actualWorld.cellStack[target.stackID].GetCellU(target.layer);
						t->resources -= GetCost(t->nextInstruction.nextTask,t->nextInstruction.taskParameter,cell.content);
					

						switch (t->nextInstruction.nextTask)
						{
							case Technite::Task::Move:
							{
								MoveTechnite(*t,target);

							}
							break;
							case Technite::Task::Replace:
							{
								ChangeGrid(target,Grid::Content::Reinterpret(t->nextInstruction.taskParameter));
							}
							break;
							case Technite::Task::Spawn:
							{
								TNewTechnite ntech;
								index_t separatorAt = t->nextInstruction.payload.find('|');
								if (separatorAt)
								{
									ntech.repo = t->nextInstruction.payload.subStringRef(0,separatorAt-1);
									ntech.initMessage = t->nextInstruction.payload.subStringRef((int)separatorAt);
								}
								else
								{
									ntech.repo = t->nextInstruction.payload;
								}
								ntech.id = t->nextInstruction.targetTechnite;
								ntech.location = target;
								ntech.parent = t->id;
								newTechnites << ntech;
								/*
								hurgh, can't just add to technites because of parallel-for.
								have to add to some other lock-protected list.
								then spawn new process somewhere, pass on source/assembly location, access secret, init parameter, etc
								*/
							}
							break;
						};
					});
					::Concurrency::parallel_for_each(layers[currentLayer].begin(),layers[currentLayer].end(),[](Technite*t)
					{
						if (t->isDead)
							return;
						if (t->taskResult != Technite::TaskResult::Success && t->taskResult != Technite::TaskResult::TargetBlocked)
							return;
						Grid::HCellIdU target;
						ASSERT__(t->DecodeAbsoluteTarget(target));
						atomicGrid.cellStack[target.stackID].volumeCell[target.layer].content = 0;
					});
				}
				break;
			}
			ApplyFamilyChange(timeStamp);

			currentLayer++;

			if (currentLayer == ExecutionLayer::Count)
			{
				GC();
				CheckTechniteFoundations(timeStamp);
			

				newTechnites.SignalRead();
					ByOperator::QuickSort(newTechnites);
					foreach (newTechnites,tech)
					{
						if (Create(*tech,timeStamp) == InvalidIndex)
						{
							LogEvent(EventClass::Preset::internalError,"Failed to spawn technite");	//this should not happen. if verified that it does not happen the quicksort operation can be avoided

						}
					}
				newTechnites.ExitRead();



				Channels::stateUpdateBegin.SendTo(serviceHost,UserLevel::Authenticated);

				Concurrency::parallel_for_each(technites.begin(),technites.end(),[](Technite&t)
				{
					TCP::PPeer peer = t.peer.lock();
					if (!peer)
						return;

					Protocol::OwnState state;
					state.commonState.location = t.location;
					state.commonState.resources = t.resources;
					state.state = t.state;
					state.taskResult = t.taskResult;
					state.visionRadius = t.visionRadius;
					Channels::ownTechniteState.SendTo(peer,state);

					const count_t volume = t.hFieldOfVision.GetVolume();
					if (volume < 32768)
					{
						NETA(Protocol::TerrainStateCell) cellContent;
						cellContent.SetSize(volume);
						Protocol::TerrainStateCell*at = cellContent.pointer();
						index_t techniteAt;

						Technite::CompressedLocation loc;
						foreach (t.hFieldOfVision.coverage,c)
						{
							const Grid::CellStack&stack = Grid::actualWorld.cellStack[c->stackID];
							for (UINT l = c->layerBegin; l < c->layerEnd; l++)
							{
								loc = Grid::HCellIdU(c->stackID,l);
								at->compressedLocation = loc.GetData();
								at->content = stack.volumeCell[l].content;

								if (at->content == Grid::Content::Technite)
								{
									if (techniteMap.Query(Grid::HCellIdU(c->stackID,l),techniteAt))
									{
										const Technite&other = technites[techniteAt];
										if (&other != &t)
										{
											Protocol::OtherState state;
											state.commonState.location = other.location;
											state.commonState.resources = other.resources;
											state.id = other.id;
											Channels::otherTechniteState.SendTo(peer,state);
										}
									}
									else
									{
										bool brk = true;
									}
								}
								at++;
							}
						}
						ASSERT_CONCLUSION(cellContent,at);
						Channels::terrainState.SendTo(peer,cellContent);
					}
					foreach (t.lFieldOfVision.coverage,c)
					{
						for (UINT l = c->layerBegin; l < c->layerEnd; l++)
						{
							Objects::PWorldEntity e = Objects::GetEntity(Grid::LCellIdI(c->stackID,Grid::CellStack::ToHeight(l)));

							if (e)
							{
								Protocol::EntityContact contact;
								contact.currentHealth = e->health;
								contact.maxHealth = e->GetMaxHealth();
								contact.id = e->id;
								Base::CompressedLocation loc;
								loc = Grid::HCellIdU(Grid::low.grid.GetCell( e->location.stackID ).higherEquivalent,Grid::CellStack::ToLayer(e->location.height));
								contact.location = loc;
								contact.type = e->GetTypeName();
								contact.height = e->GetHeight();


								Channels::entityContact.SendTo(peer,contact);
							}
						}
					}

				});

			}

			return currentLayer < ExecutionLayer::Count;
		}






		void					ProcessLighting(Technite&technite)
		{
			bool lit = true;


			unsigned layer = technite.location.GetLayer()+1;
			//Grid::CellStack::ToLayer(technite.location.height + 1);
			for (; layer < Grid::CellStack::Range; layer++)
			{
				BYTE top = Grid::actualWorld.cellStack[technite.location.GetStackID()].volumeCell[layer].content;
				if (top != Grid::Content::Clear && top != Grid::Content::Water)
				{
					lit = false;
					break;
				}
			}
			technite.state.SetLit(lit);

			if (lit)
			{
				Technite::Resources::Increase(technite.resources.energy, technite.GetLightEnergyYield());
			}
		}

		static void TryDocker(const String&cmd)
		{
			LogEvent(EventClass(EventSignificance::Uncommon,EventFlavor::ServerPrivate),cmd);
			return;

			FileSystem::TempFile file;
			if (!file.Create())
				throw IO::DriveAccess::FileOpenFault(CLOCATION,"Unable to create temporary file for command output");
			const String instruction = "docker "+cmd+" > "+String(file.GetLocation())+" 2>&1";
			int rs = system(instruction);
			if (rs != 0)
			{
				FILE*f = FOPEN(file.GetLocation().c_str(),"rb");
				if (!f)
				{
					throw IO::DriveAccess::FileOpenFault(CLOCATION,instruction+"\n failed but could not open temporary output file for printout");
				}
				fseek(f,0,SEEK_END);
				size_t size = (size_t)ftell(f);
				fseek(f,0,SEEK_SET);
				String output = String(TStringLength(size));
				fread(output.mutablePointer(),1,size,f);
				fclose(f);
				throw IO::DriveAccess::FileOpenFault(CLOCATION,instruction+"\n failed: "+output);
			}
		}

		bool			Technite::RelaySpectatorMessage(const String&msg)
		{
			TCP::PPeer peer = this->peer.lock();
			if (!peer)
				return false;
			spectatorMessage = msg;
			return true;
		}

	#endif

	Grid::Content GetContent(const Grid::HCellIdU&location)
	{
		return Grid::Content::Reinterpret(Grid::actualWorld.cellStack[location.stackID].GetCellU(location.layer).content);
	}

	bool CanStandHere(const Grid::HCellIdU&location)
	{
//		Grid::Content local = GetContent(location);
		//if (local != Grid::Content::Clear && local != Grid::Content::Water)
		//	return false;
		if (location.layer >= Grid::CellStack::Range)
			return false;
		if (location.layer == 0)
		{
			return Technite::SupportedBy(Grid::Content::Reinterpret(Grid::CellStack::bottom.content));
		}
		if (!Technite::SupportedBy(GetContent(location - 1)))
		{
			const auto&n = Grid::high.grid.GetCell(location.stackID).neighborhood;
			bool anyGood = false;
			for (int k = 0; k < n.numNeighbors; k++)
			{
				if (Technite::SupportedBy(GetContent(Grid::HCellIdU(n.neighborID[k], location.layer)))
					&&
					Technite::SupportedBy(GetContent(Grid::HCellIdU(n.neighborID[k], location.layer - 1)))
					)
				{
					anyGood = true;
					break;
				}
			}
			return anyGood;
		}
		return true;
	}


	#ifndef CLIENT
		index_t				Create( const TNewTechnite& init, float t)
		{
			Grid::HCellIdU location = init.location;
			if (location.layer>=Grid::CellStack::Range)
				return InvalidIndex;
			if (GetContent(location) == Grid::Content::Technite)
				return InvalidIndex;
			//if (techniteMap.IsSet(location))
			//	return InvalidIndex;
			if (!CanStandHere(location))
				return InvalidIndex;

			const index_t at = technites.Count();

			Technite&tech = technites.Append();
			tech.secret.Generate();
			tech.id = init.id;
			tech.location = location;
			techniteMap.Set(location, at);
			techniteIDMap.Set(tech.id,at);
			tech.isDead = false;
			tech.startOptions = init;
			tech.SetStandardFieldOfVision();
			ProcessLighting(tech);

			ASSERT__(ChangeGrid(location,Grid::Content::Technite));
			ApplyFamilyChange(t);

			tech.StartClient();

			return at;
		}



		void	Technite::ShutdownClient()
		{
			TCP::PPeer peer = this->peer.lock();
			if (peer)
			{
				LogEvent(EventSignificance::Uncommon,id.ToString()+": Shutting down technite system");
				Channels::isDead.SendTo(peer);
			}
			try
			{
				if (this->dockerProcessThread.joinable())
					this->dockerProcessThread.join();
			}
			catch (...)
			{}

		}


		void Technite::StartClient()
		{
			LogEvent(EventSignificance::Uncommon,id.ToString()+": Launching new technite system");

			dockerProcessThread.swap(std::thread([this]()
			{
				try
				{
					String config = " --id "+id.ToFullString()+
						" --at "+String(location.GetStackID())+'.'+String(location.GetLayer())+
						" --parent "+startOptions.parent.ToFullString()+
						" --url "+serviceURL+":"+String(serviceHost.GetPort())+
						" --secret "+ secret.ToFullString()+
						" "+startOptions.initMessage;
					System::copyToClipboard(NULL,config.c_str());
					TryDocker("pull "+startOptions.repo);
					TryDocker("run --rm --name "+GetDockerName()+" "+startOptions.repo+" technite"
						+ config
						);
				}
				catch (const std::exception&ex)
				{
					LogEvent(EventSignificance::NonFatalError,"Failed to launch technite process: "+String(ex.what()));
				}
				LogEvent(EventSignificance::Uncommon,id.ToString()+": Technite system thread exit");
			}));
		}
	#endif

	int EnergyCost(BYTE content)
	{
		DBG_ASSERT_LESS__(content,Grid::Content::N);
		return Grid::CellStack::Cell::typeStrength[content];
	}

	int EnergyYield(BYTE content)
	{
		return EnergyCost(content)/2;
	}

	Technite::Resources			GetCost(Technite::Task task, BYTE parameter, BYTE currentGridContent)
	{
		int rs = Technite::energyCost[(int)task];
		switch (task)
		{
			case Technite::Task::Replace:
				rs += EnergyCost(parameter) - EnergyYield(currentGridContent);
			break;
			case Technite::Task::End:
				rs += EnergyCost(parameter) - EnergyYield(Grid::Content::Technite);
			break;
			case Technite::Task::Spawn:
				rs += EnergyCost(Grid::Content::Technite) - EnergyYield(currentGridContent);
			break;
			case Technite::Task::Kill:
				FATAL__("Should not call GetCost for enum type "+String(int(task)));
			break;
		}


		return Technite::Resources((BYTE)clamp(rs,0,255));
	}


	void					Clear()
	{
		LogEvent(EventSignificance::Uncommon,"Clearing out technites");
		#ifndef CLIENT
			foreach (technites,t)
			{
				t->ShutdownClient();
			}
		#endif
		technites.Clear();
		techniteMap.Clear();
		techniteIDMap.Clear();
	}

	#ifndef CLIENT

		void					SetupAndStartService(USHORT port)
		{

			static bool setup = false;
			if (!setup)
			{
				setup = true;
			
				//server.InstallChannel(...);
				//server.OpenSignalChannel(...);

				serviceHost.InstallChannel( Channels::messageInstruction );
				serviceHost.InstallChannel( Channels::regularInstruction );
				serviceHost.InstallChannel( Channels::authenticate );


				serviceHost.onSignal = Receiver::OnSignal;
				serviceHost.SetAsync(false);
				serviceHost.onEvent = Receiver::OnEvent;
				serviceHost.onIgnorePackage = Receiver::OnIgnorePackage;
				serviceHost.onDeserializationFailed = Receiver::OnDeserializationFailed;
			}
			serviceHost.StartService(port);
		}

		void					StopService()
		{
			serviceHost.Shutdown();
		}

		void					DestroyService()
		{
			Clear();
			StopService();
		}


		void					Resolve()
		{
			serviceHost.Resolve();
		}

		bool					ServiceIsOnline()
		{
			return serviceHost.IsOnline();
		}


		void				CheckTechniteFoundations(float t)
		{
			static WorkPipe<std::pair<Technite*, Grid::HCellIdU> >	falling;
			bool anyBad = false;
			count_t numIterations = 0, numFallen = 0;
			do
			{
				numIterations++;
				anyBad = false;
				Concurrency::parallel_for_each(technites.begin(),technites.end(),[&anyBad](Technite&t)
				{
					Grid::HCellIdU loc = t.location.GetU();
					if (CanStandHere(loc))
						return;
					anyBad = true;
					loc--;
					while (loc.layer > 0 && !CanStandHere(loc))
						loc--;
					if (loc.layer == 0 && !CanStandHere(loc))
					{
						t.isDead = true;
					}
					else
						falling << std::make_pair(&t,loc);
				});
				if (anyBad)
				{
					falling.SignalRead();
						numFallen += falling.Count();
						Concurrency::parallel_for_each(falling.begin(),falling.end(),[](const std::pair<Technite*, Grid::HCellIdU>&t)
						{
							MoveTechnite(*t.first,t.second);
						});
					falling.ExitRead();

					GC();
				}
			}
			while (anyBad);
			LogEvent(EventSignificance::Debug,String(numFallen)+" falling technite(s) processed in "+String(numIterations)+" iteration(s)");

			ApplyFamilyChange(t);
		}


		void				Base::SetStandardFieldOfVision()
		{
			Grid::high.GenerateStep2Volume(location.GetU(),hFieldOfVision);
			Grid::low.GenerateStep1MultiVolume(Grid::highToLowMap[location.GetStackID()],location.GetLayer(),lFieldOfVision);

			visionRadius = -1.f;
		}

		void				Base::SetSphericalFieldOfVision(float radius)
		{
			Sphere<> sphere(Grid::ToWorld(location.GetU()),radius);
			Grid::high.GenerateVolume(sphere,hFieldOfVision);
			Grid::low.GenerateVolume(sphere,lFieldOfVision);

			visionRadius = radius;
		}

	#endif

	bool			Base::operator==(const Base&other) const
	{
		return id == other.id 
			&& location == other.location 
			&& state == other.state 
			&& resources == other.resources 
			&& taskResult == other.taskResult 
			&& visionRadius == other.visionRadius
			&& nextInstruction.nextTask == other.nextInstruction.nextTask
			&& nextInstruction.taskParameter == other.nextInstruction.taskParameter 
			&& nextInstruction.taskTarget == other.nextInstruction.taskTarget;
	}


	/*static*/ const serial_size_t	Base::ConstSize = 
				(serial_size_t)( 
					sizeof(id) 
					+ sizeof(location) 
					+ sizeof(state) 
					+ sizeof(resources) 
					+ sizeof(taskResult) 
					+ sizeof(visionRadius)
					+ sizeof(nextInstruction.nextTask)
//					+ sizeof(nextInstruction.targetTechnite)
					+ sizeof(nextInstruction.taskParameter)
					+ sizeof(nextInstruction.taskTarget)
				);

	/*virtual override*/ serial_size_t	Base::GetSerialSize(bool exportSize) const
	{
		return ConstSize + nextInstruction.payload.GetSerialSize(exportSize);
	}

	/*virtual override*/ bool			Base::Serialize(IWriteStream&outStream, bool exportSize) const
	{
		return 
					outStream.WritePrimitive(id) 
					&& outStream.WritePrimitive(location) 
					&& outStream.WritePrimitive(state) 
					&& outStream.WritePrimitive(resources) 
					&& outStream.WritePrimitive(taskResult) 
					&& outStream.WritePrimitive(visionRadius)
					&& outStream.WritePrimitive(nextInstruction.nextTask)
//					&& outStream.WritePrimitive(nextInstruction.targetTechnite)
					&& outStream.WritePrimitive(nextInstruction.taskParameter)
					&& outStream.WritePrimitive(nextInstruction.taskTarget)
					&& nextInstruction.payload.Serialize(outStream,exportSize)
		;
	}

	/*virtual override*/ bool			Base::Deserialize(IReadStream&inStream, serial_size_t fixedSize)
	{
		if (fixedSize != EmbeddedSize)
		{
			if (fixedSize < ConstSize)
				return false;
			fixedSize -= ConstSize;
		}

		return 
					inStream.ReadPrimitive(id) 
					&& inStream.ReadPrimitive(location) 
					&& inStream.ReadPrimitive(state) 
					&& inStream.ReadPrimitive(resources) 
					&& inStream.ReadPrimitive(taskResult) 
					&& inStream.ReadPrimitive(visionRadius)
					&& inStream.ReadPrimitive(nextInstruction.nextTask)
//					&& inStream.ReadPrimitive(nextInstruction.targetTechnite)
					&& inStream.ReadPrimitive(nextInstruction.taskParameter)
					&& inStream.ReadPrimitive(nextInstruction.taskTarget)
					&& nextInstruction.payload.Deserialize(inStream,fixedSize)
		;


	}


	bool	Base::DecodeAbsoluteTarget(Grid::HCellIdU & outTarget) const
	{
		Grid::HCellIdU loc;
		return DecodeAbsoluteTarget(loc, outTarget);
	}


	bool	Base::DecodeAbsoluteTarget(Grid::HCellIdU & loc, Grid::HCellIdU & outTarget) const
	{
		const int heightDelta = this->nextInstruction.taskTarget.GetHeightDelta();
		const UINT32 nIndex = this->nextInstruction.taskTarget.GetNeighborIndex();

		if (heightDelta < -1 || heightDelta > 1)
			return false;
		loc = location.GetU();
		if (loc.stackID >= Grid::high.grid.CellCount)
			return false;
		if (loc.layer >= Grid::CellStack::Range)
			return false;
		if (loc.layer == 0 && heightDelta < 0)
			return false;
		if (loc.layer+heightDelta >= Grid::CellStack::Range)
			return false;

		if (nIndex == CompressedTarget::SelfNeighbor)
		{
			outTarget = Grid::HCellIdU(loc.stackID, loc.layer + heightDelta);
		}
		else
		{
			const auto&n = Grid::high.grid.GetCell(loc.stackID).neighborhood;
			if (nIndex >= n.numNeighbors)
			{
				return false;
			}
			outTarget = Grid::HCellIdU(n.neighborID[nIndex], loc.layer + heightDelta);
		}
		return outTarget.layer < Grid::CellStack::Range;
	}

	
	index_t					BroadTechniteMap::MapCluster(const Grid::HCellIdU&loc, index_t&outFamily, UINT32&heightRemainder) const
	{
		outFamily = Grid::high.grid.GetCell(loc.stackID).family;
		heightRemainder = loc.layer % SectionLayers;
		return outFamily * HeightSections
			+ loc.layer / SectionLayers;
	}


	index_t					BroadTechniteMap::Map(const Grid::HCellIdU&location, Cluster*&outCluster, bool initIfEmpty)
	{
		index_t family;
		UINT32 heightRemainder;
		index_t clusterIndex = MapCluster(location, family, heightRemainder);
		//ASSERT_LESS__(heightRemainder, SectionLayers);
		//ASSERT_LESS__(clusterIndex, ARRAYSIZE(cluster));
		outCluster = cluster + clusterIndex;
		count_t range = Grid::high.grid.CountFamilyMembers((BYTE)family);
		index_t inner = (location.stackID - Grid::high.grid.GetFamilyOffset((BYTE)family));
		//ASSERT_LESS__(inner, range);
		if (initIfEmpty && outCluster->volumeCells.IsEmpty())
		{
			outCluster->volumeCells.SetSize(range * SectionLayers);
			outCluster->volumeCells.Fill(Empty);
		}
		return inner + range * heightRemainder;
	}

	//index_t					BroadTechniteMap::Map(const Grid::HCellIdU&location, const Cluster*&outCluster) const
	//{
	//	return Map(Grid::HCellIdU(location.stackID,Grid::CellStack::ToLayer(location.height)),outCluster);
	//}
	index_t					BroadTechniteMap::Map(const Grid::HCellIdU&location, const Cluster*&outCluster) const
	{
		//UINT32 layer = Grid::CellStack::ToLayer(location.height);
		index_t family;
		UINT32 heightRemainder;
		index_t clusterIndex = MapCluster(location, family,heightRemainder);
		//ASSERT_LESS__(heightRemainder, SectionLayers);
		//ASSERT_LESS__(clusterIndex, ARRAYSIZE(cluster));
		outCluster = cluster + clusterIndex;
		//const count_t numFamilies = BaseGrid::NumFamilies;
		index_t inner = (location.stackID - Grid::high.grid.GetFamilyOffset((BYTE)family));
		count_t range = Grid::high.grid.CountFamilyMembers((BYTE)family);
		//ASSERT_EQUAL__(family, Grid::high.grid.GetCell(location.stackID).family);
		//ASSERT_LESS__(inner, range);
		return inner + range * heightRemainder;
	}

	index_t					BroadTechniteMap::Unset(const Grid::HCellIdU& id)
	{
		Cluster*cluster;
		index_t inner = Map(id, cluster,false);
		if (cluster->numEntries == 0)
		{
			FATAL__("Failed to unset " + id.ToString());
			return InvalidIndex;
		}
		auto&cell = cluster->volumeCells[inner];
		if (cell != Empty)
		{
			index_t rs = cell;
			cell = Empty;
			cluster->numEntries--;
			totalEntries--;
			return rs;
		}
		else
		{
			FATAL__("Failed to unset " + id.ToString());
			return InvalidIndex;
		}

	}

	void					BroadTechniteMap::Set(const Grid::HCellIdU&id, index_t to)
	{
		Cluster*cluster;
		index_t inner = Map(id, cluster,true);
		if (cluster->volumeCells[inner] == Empty)
		{
			cluster->numEntries++;
			totalEntries++;
		}
		cluster->volumeCells[inner] = (UINT32)to;
	}

	bool					BroadTechniteMap::Query(const Grid::HCellIdU&id, index_t&out) const
	{
		const Cluster*cluster;
		index_t inner = Map(id, cluster);
		if (cluster->numEntries == 0)
		{
			out = InvalidIndex;
			return false;
		}
		UINT32 v = cluster->volumeCells[inner];
		if (v == Empty)
		{
			out = InvalidIndex;
			return false;
		}
		out = (index_t)v;
		return true;
	}
	
	bool					BroadTechniteMap::IsSet(const Grid::HCellIdU&id) const
	{
		const Cluster*cluster;
		index_t inner = Map(id, cluster);
		if (cluster->numEntries == 0)
			return false;
		UINT32 v = cluster->volumeCells[inner];
		return v != Empty;
	}

	//bool					BroadTechniteMap::SetNew(const Grid::HCellIdU&id, index_t to)
	//{
	//	return SetNew(Grid::HCellIdU(id.stackID,Grid::CellStack::ToLayer(id.height)),to);
	//}
	bool					BroadTechniteMap::SetNew(const Grid::HCellIdU&id, index_t to)
	{
		Cluster*cluster;
		index_t inner = Map(id, cluster, true);
		UINT32&v = cluster->volumeCells[inner];
		if (v != Empty)
			return false;
		cluster->numEntries++;
		totalEntries++;
		v = (UINT32)to;
		return true;
	}


	count_t					BroadTechniteMap::Count() const
	{
		return totalEntries;
	}

	count_t					BroadTechniteMap::ThoroughCount() const
	{
		count_t rs = 0;
		for (index_t i = 0; i < ClusterCount; i++)
			rs += cluster[i].numEntries;
		ASSERT_EQUAL__(rs,totalEntries);
		return rs;
	}

	#ifndef CLIENT
		void			VerifyGrid()
		{
			std::atomic<count_t>	abandonedCounter = 0,
									totalCounter = 0;

			//bool familyChanged[BaseGrid::NumFamilies] = {false};

			Concurrency::parallel_for(index_t(0),Grid::HighSetup::GridType::CellCount,[&abandonedCounter,&totalCounter](index_t stackIndex)
			{
				Grid::CellStack&stack = Grid::actualWorld.cellStack[stackIndex];

				bool anyChanged = false;
				for (UINT32 layer = 0; layer < Grid::CellStack::Range; layer++)
				{
					if (stack.volumeCell[layer].content == Grid::Content::Technite)
					{
						totalCounter++;
						index_t techniteIndex;
					
					
						const Grid::HCellIdU loc(stackIndex,layer);
						if (!techniteMap.Query(loc,techniteIndex))
						{
							//LogEvent(EventType::TechniteLogic,nullptr,"Found homeless technite in "+loc.ToString()+", clearing cell");

							stack.MakeLocalHole(Grid::CellStack::ToHeight(layer));
							abandonedCounter++;
							anyChanged = true;
							continue;
						}
						const Technite&t = technites[techniteIndex];
						if (t.isDead)
						{
							foreach (technites,t2)
								if (t2->location.GetU() == loc)
								{
									FATAL__("Map inconsistency found with technite at location "+loc.ToString());
								}

							//LogEvent(EventType::TechniteLogic,nullptr,"Found abandoned technite of faction "+f->GetLogName()+" in "+loc.ToString()+", clearing cell");
							stack.MakeLocalHole(Grid::CellStack::ToHeight(layer));
							abandonedCounter++;
							anyChanged = true;
							continue;
						}

					}
				}
				if (anyChanged)
					familyChanged[Grid::high.grid.GetCell(stackIndex).family] = true;
			});
			ApplyFamilyChange(0.f);


			const count_t shouldBeTechniteCount = technites.Count();

			count_t gridTechniteCount = totalCounter - abandonedCounter;
			if (gridTechniteCount < shouldBeTechniteCount)
			{
				LogEvent(techniteError,"Encountered "+String(shouldBeTechniteCount-gridTechniteCount)+" technite ghost(s). Ignoring");
				//ignore this case, won't last long anyways
			}
			else
				ASSERT_EQUAL__(gridTechniteCount,shouldBeTechniteCount);

			if (abandonedCounter > 0)
			{
				LogEvent(techniteError,"Cleared "+String(abandonedCounter)+"/"+String(totalCounter)+" abandoned technite cell(s), "+String(shouldBeTechniteCount)+" technite(s) left");

				#ifndef CLIENT
					CheckSendTerrain();
				#endif

				//RemapFaction(Objects::Faction::nexus);
				//foreach (Faction::all,f)
				//	RemapFaction(f->lock())
			}


			//std::atomic<count_t>	visibilityErrors = 0;

			//Concurrency::parallel_for(index_t(0),Grid::HighSetup::GridType::CellCount,[&visibilityErrors](index_t stackIndex)
			//{
			//	Grid::CellStack&stack = Grid::actualWorld.cellStack[stackIndex];
			//	const auto&n = Grid::high.grid.GetCell(stackIndex).neighborhood;
			//	for (UINT32 layer = 0; layer < Grid::CellStack::Range; layer++)
			//	{
			//		if (stack.volumeCell[layer].content == Grid::Content::Technite)
			//		{
			//			PFaction f = Faction::GetFactionOf(stack.volumeCell[layer].techniteFaction);
			//			const Grid::HCellIdU loc(stackIndex,layer);
			//			Technite*t = f->GetOwnTechnite(loc);

			//			//live technite must have visibility field of 1 around it
			//			for (int hd = -1; hd <= 1; hd++)
			//			{
			//				if (layer == 0 && hd < 0)
			//					continue;
			//				if (layer + hd >= Grid::CellStack::Range)
			//					continue;
			//				UINT32 layer2 = layer+hd;
			//				for (int k = 0; k < n.numNeighbors; k++)
			//				{
			//					UINT32 stackID = n.neighborID[k];
			//					if (!VerifyIsVisibleAndMatches(f,Grid::HCellIdU(stackID,layer2)))
			//						visibilityErrors++;
			//				}
			//				if (!VerifyIsVisibleAndMatches(f,Grid::HCellIdU(stackIndex,layer2)))
			//					visibilityErrors++;

			//			}
			//		}
			//	}
			//});
			//if (visibilityErrors > 0)
			//{
			//	LogEvent(techniteError,"Detected "+String(visibilityErrors)+" visibility error(s). Rebuilding visibility grid");

			//	for (const auto&tf : Faction::allTechniteFactions)
			//	{
			//		tf->visibility.FlushAllData(false);
			//		tf->visibility.LockChangeChecking();
			//		float t= 0.f;
			//		foreach (tf->allMobiles,_m)
			//			_m->lock()->RegisterSensory(t,false);
			//		foreach (tf->structures,_s)
			//			_s->lock()->RegisterSensory(t,false);
			//		foreach (tf->satellites,_sat)
			//			_sat->lock()->Objects::WorldEntity::RegisterSensory(t,false);
			//		foreach (tf->technites,tc)
			//			tf->RegisterTechniteSensory(tc->location.GetU(), t,false);
			//		tf->visibility.UnlockChangeChecking(0.f);
			//		ApplyCoverageChange(tf);

			//		Objects::ForeachWorldEntity([tf,&t](const Objects::PWorldEntity&e)
			//		{
			//			tf->CheckGridVisibility(e,t);
			//		});
			//	}

			//	VerifyGrid();

			//}

		}

		void	SignalCommandPhase()
		{
			Protocol::ProcessRound process;
			process.roundNumber = RuntimeConfig::roundNumber;
			process.techniteSubRoundNumber = RuntimeConfig::techniteSubRoundNumber;

			Concurrency::parallel_for_each(technites.begin(),technites.end(),[](Technite&t)
			{
				TCP::PPeer peer = t.peer.lock();
				if (!peer)
					return;
				if (t.spectatorMessage.IsNotEmpty())
				{
					Channels::message.SendTo(peer,Protocol::Message(TUniqueID::Empty(),t.spectatorMessage));
					t.spectatorMessage = "";
				}
			});

			Channels::processRound.SendTo(serviceHost,process,UserLevel::Authenticated);
		}

	#else

		void				Add(const Base&t)
		{
			index_t at = technites.Count();
			((Base&)technites.Append()) = t;
			techniteMap.Set(t.location.GetU(),at);
			techniteIDMap.Set(t.id,at);
		}

	#endif
}



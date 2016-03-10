#include <global_root.h>
#include "technite.h"
#include <ppl.h>
#include <io/file_system.h>
#include "objects/gridAndChoices.h"
#include "protocol.h"
#include "../server/components/journalSaveGame.h"


namespace Technites
{
	namespace Protocol
	{
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
			, ::Protocol::LCellIdI, location
			, NET(UINT16), currentHealth
			, NET(UINT16), maxHealth
		);
		NETSTRUCT2(Message
			, NET(TUniqueID), sender
			, Package::NetString<1024>, message
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

		void OnEvent(TCP::event_t event, TCP::Peer&origin)
		{
			LogEvent(EventSignificance::Debug,nullptr,"RemoteControl: "+origin.ToString()+": "+TCP::event2str(event));
			switch (event)
			{
				case TCP::Event::ConnectionEstablished:
					//maybe init here later
				break;

			}
		}

	}

	namespace Channels
	{
		enum channel_t
		{
			StateUpdateBegin,	//	s2c : <signal>
			EntityContact,		//	s2c : EntityContact
			OwnTechniteState,	//	s2c : OwnState
			OtherTechniteState,	//	s2c : OtherState
			TerrainState,		//	s2c : NETA<BYTE>
			Message,			//	s2c : Message
			IsDead,				//	s2c : <signal>
		};


		TCP::SignalSender<StateUpdateBegin>	stateUpdateBegin;
		TCP::SignalSender<IsDead>			isDead;
		TCP::ObjectSender<Protocol::EntityContact,EntityContact>	entityContact;
		TCP::ObjectSender<Protocol::OwnState,OwnTechniteState>	ownTechniteState;
		TCP::ObjectSender<Protocol::OtherState,OtherTechniteState>	otherTechniteState;
		TCP::ObjectSender<NETA(BYTE),TerrainState>	terrainState;
		TCP::ObjectSender<Protocol::Message, Message> message;
	}


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
	BroadTechniteMap			techniteMap;


	TCP::Server					serviceHost;
	String						serviceURL="localhost";
	

	Buffer0<Technite*>			layers[ExecutionLayer::Count];

	index_t						currentLayer = 0;
	bool						anyDied = false;
	WorkPipe<TNewTechnite>		newTechnites;

	Technite::Resources			GetCost(Technite::Task, BYTE parameter, BYTE currentGridContent);

	void						CheckTechniteFoundations();


	float						timeStamp=0;

	void						ProcessLighting(Technite&technite);

	void						BeginProcessing(float t0)
	{
		timeStamp = t0;
		atomicGrid.Fill(0);
		currentLayer = 0;
		anyDied = false;

		foreach (technites,t)
		{
			if (t->visionType != Technite::VisionType::Standard)
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

		CheckTechniteFoundations();

	}

	bool familyChanged[BaseGrid::NumFamilies] = {false};

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
					const Technite&other = technites[tIndex];

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

					UINT32 stackID = t->location.GetStackID();
					Grid::actualWorld.cellStack[stackID].volumeCell[t->location.GetLayer()].content = t->nextInstruction.taskParameter;

					index_t family1 = Grid::high.grid.GetCell(stackID).family;
					familyChanged[family1] = true;

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
							swp(Grid::actualWorld.cellStack[t->location.GetStackID()]	.volumeCell[t->location.GetLayer()].content, 
								Grid::actualWorld.cellStack[target.stackID]				.volumeCell[target.layer].content);

							index_t family0 = Grid::high.grid.GetCell(t->location.GetStackID()).family,
									family1 = Grid::high.grid.GetCell(target.stackID).family;
							familyChanged[family0] = true;
							familyChanged[family1] = true;

							t->location = target;
							t->SetStandardFieldOfVision();

						}
						break;
						case Technite::Task::Replace:
						{
							Grid::actualWorld.cellStack[target.stackID].volumeCell[target.layer].content = t->nextInstruction.taskParameter;
							index_t family1 = Grid::high.grid.GetCell(target.stackID).family;
							familyChanged[family1] = true;
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
		for (index_t i = 0; i < ARRAYSIZE(familyChanged); i++)
		{
			if (familyChanged[i])
			{
				Grid::low.family[i].SetOutdated(timeStamp);
				Grid::high.family[i].SetOutdated(timeStamp);
				familyChanged[i] = false;
			}
		}

		currentLayer++;

		if (currentLayer == ExecutionLayer::Count)
		{
			if (anyDied)
			{
				static Buffer<Technite>	survivors;
				foreach (technites,t)
					if (t->isDead)
					{
						t->ShutdownClient();
					}
					else
						survivors << std::move(*t);
				technites.swap(survivors);
				survivors.Clear();
			}

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

			CheckTechniteFoundations();


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
					NETA(BYTE) cellContent;
					cellContent.SetSize(volume);
					BYTE*at = cellContent.pointer();
					index_t techniteAt;

					foreach (t.hFieldOfVision.coverage,c)
					{
						const Grid::CellStack&stack = Grid::actualWorld.cellStack[c->stackID];
						for (UINT l = c->layerBegin; l < c->layerEnd; l++)
						{
							*at = stack.volumeCell[l].content;

							if (*at == Grid::Content::Technite)
							{
								if (techniteMap.Query(Grid::HCellIdU(c->stackID,l),techniteAt))
								{
									const Technite&other = technites[techniteAt];
									Protocol::OtherState state;
									state.commonState.location = other.location;
									state.commonState.resources = other.resources;
									state.id = other.id;
									Channels::otherTechniteState.SendTo(peer,state);
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
							contact.location = e->location;
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

		index_t at = technites.Count();

		Technite&tech = technites.Append();
		tech.secret.Generate();
		tech.id = init.id;
		tech.location = location;
		techniteMap.Set(location, at);
		tech.isDead = false;
		tech.startOptions = init;
		ProcessLighting(tech);

		Grid::CellStack&stack = Grid::actualWorld.cellStack[location.stackID];
		ASSERT__(stack.LocalOverwrite(location.layer, Grid::Content::Technite));

		index_t family = Grid::high.grid.GetCell(location.stackID).family;

		Grid::low.family[family].SetOutdated(t);
		Grid::high.family[family].SetOutdated(t);

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
				TryDocker("pull "+startOptions.repo);
				TryDocker("run --rm --name "+GetDockerName()+" "+startOptions.repo+" technite --id="+id.ToFullString()+" --parent="+startOptions.parent.ToFullString()+" --url="+serviceURL+":"+String(serviceHost.GetPort())+" --secret="+ secret.ToFullString()+" "+startOptions.initMessage);
			}
			catch (const std::exception&ex)
			{
				LogEvent(EventSignificance::NonFatalError,"Failed to launch technite process: "+String(ex.what()));
			}
			LogEvent(EventSignificance::Uncommon,id.ToString()+": Technite system thread exit");
		}));
	}


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


	void					SetupAndStartService()
	{

		static bool setup = false;
		if (!setup)
		{
			setup = true;
			
			//server.InstallChannel(...);
			//server.OpenSignalChannel(...);

			serviceHost.onSignal = Receiver::OnSignal;
			serviceHost.SetAsync(false);
			serviceHost.onEvent = Receiver::OnEvent;
			serviceHost.onIgnorePackage = Receiver::OnIgnorePackage;
			serviceHost.onDeserializationFailed = Receiver::OnDeserializationFailed;
		}
		serviceHost.Start();
	}

	void					Clear()
	{
		LogEvent(EventSignificance::Uncommon,"Clearing out technites");
		foreach (technites,t)
		{
			t->ShutdownClient();
		}
		technites.Clear();
		techniteMap.Clear();

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


}



#include <global_root.h>
#include "technite.h"
#include <ppl.h>
#include <io/file_system.h>


namespace Technites
{
	AtomicGrid					atomicGrid;
	Buffer<Technite>			technites;
	Buffer0<TSensor>			sensors;
	BroadTechniteMap			techniteMap;

	TCP::Server					serviceHost;
	String						serviceURL="localhost";
	

	Buffer0<Technite*>			layers[ExecutionLayer::Count];

	index_t						currentLayer = 0;
	bool						foundationChanged = false;	//a technite changed anything in the world such that a technite could potentially fall
	bool						anyDied = false;

	Technite::Resources			GetCost(Technite::Task, BYTE parameter, BYTE currentGridContent);

	void						CheckTechniteFoundations();

	void						BeginProcessing(float t0)
	{
		atomicGrid.Fill(0);
		currentLayer = 0;
		anyDied = false;
		foundationChanged = false;

		foreach (technites,t)
		{
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

	bool						ProcessNextLayer()
	{
		

		switch (currentLayer)
		{
			case ExecutionLayer::Scan:
				for (const auto&t : layers[currentLayer])
				{
					Technite::Resources cost = GetCost(t->nextInstruction.nextTask,t->nextInstruction.taskParameter,Grid::Content::Technite);
					if (!(t->resources -= cost))
					{
						t->taskResult = Technite::TaskResult::InsufficientResources;
						continue;
					}
					float radius = sqrt(float(cost.energy));
					TSensor	sensor(Grid::ToWorld(t->location.GetU()),radius);
					sensors.Append(sensor);
					RegisterSensorCreation(sensor,false,t,false);
				}
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
				});
			break;
			case ExecutionLayer::Kill:


			break;
			case ExecutionLayer::Transfer:
				::Concurrency::parallel_for_each(layers[currentLayer].begin(),layers[currentLayer].end(),[](Technite*t)
				{
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
					if (!techniteMap.Query(target,tIndex) || technites[tIndex].id != t->nextInstruction.targetTechnite)
					{
						t->taskResult = Technite::TaskResult::TargetOutOfRange;
						return;
					}
					atomicGrid.cellStack[target.stackID].volumeCell[target.layer].content += t->nextInstruction.taskParameter;
					t->resources -= Technite::Resources(t->nextInstruction.taskParameter);
					t->taskResult = Technite::TaskResult::Success;
				});
				if (layers[currentLayer].IsNotEmpty())
					::Concurrency::parallel_for_each(technites.begin(),technites.end(),[](Technite&t)
					{
						Grid::HCellIdU target = t.location.GetU();
						if (atomicGrid.cellStack[target.stackID].volumeCell[target.layer].content)
						{
							t.resources += Technite::Resources(atomicGrid.cellStack[target.stackID].volumeCell[target.layer].content);
							atomicGrid.cellStack[target.stackID].volumeCell[target.layer].content = 0;
						}
					});
			break;
			case ExecutionLayer::ReplaceMoveSpawn:
				::Concurrency::parallel_for_each(layers[currentLayer].begin(),layers[currentLayer].end(),[](Technite*t)
				{
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
							t->location = target;
							foundationChanged = true;
						}
						break;
						case Technite::Task::Replace:
						{
							Grid::actualWorld.cellStack[target.stackID].volumeCell[target.layer].content = t->nextInstruction.taskParameter;
							if (!Grid::CellStack::Cell::IsSolid( t->nextInstruction.taskParameter))
								foundationChanged = true;
						}
						break;
						case Technite::Task::Spawn:
						{
							/*
							hurgh, can't just add to technites because of parallel-for.
							have to add to some other lock-protected list.
							then spawn new process somewhere, pass on source/assembly location, access secret, init parameter, etc

							TODO
							*/
						}
						break;
					};
				});
				::Concurrency::parallel_for_each(layers[currentLayer].begin(),layers[currentLayer].end(),[](Technite*t)
				{
					if (t->taskResult != Technite::TaskResult::Success && t->taskResult != Technite::TaskResult::TargetBlocked)
						return;
					Grid::HCellIdU target;
					ASSERT__(t->DecodeAbsoluteTarget(target));
					atomicGrid.cellStack[target.stackID].volumeCell[target.layer].content = 0;
				});
			break;
		}


		currentLayer++;

		if (currentLayer == ExecutionLayer::Count)
		{
			if (anyDied)
			{
				static Buffer<Technite>	survivors;
				foreach (technites,t)
					if (!t->isDead)
						survivors << *t;
				technites.swap(survivors);
				survivors.Clear();
			}

			CheckTechniteFoundations();
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

	
	index_t				Create(const Technite::TCompactState&st, const String&repo, const String&initMessage, float t)
	{
		Grid::HCellIdU location = st.location.GetU();
		if (location.layer>=Grid::CellStack::Range)
			return InvalidIndex;

		if (techniteMap.IsSet(location))
			return InvalidIndex;
		if (!CanStandHere(location))
			return InvalidIndex;

		index_t at = technites.Count();
		Technite&tech = technites.Append();
		tech.secret.Generate();
		tech.id = st.id;
		techniteMap.Set(location, at);
		tech.location = location;
		tech.resources = st.resources;
		tech.state = st.state;
		ProcessLighting(tech);

		Grid::CellStack&stack = Grid::actualWorld.cellStack[location.stackID];
		ASSERT__(stack.LocalOverwrite(location.layer, Grid::Content::Technite));

		index_t family = Grid::high.grid.GetCell(location.stackID).family;

		Grid::low.family[family].SetOutdated(t);
		Grid::high.family[family].SetOutdated(t);

		#ifndef CLIENT
			RegisterTechniteSensory(tech.location.GetU(),t);
		#endif

		LogEvent(EventSignificance::Uncommon,"Launching new technite container");

		CryptographicID secret = tech.secret;
		String dockerName = tech.GetDockerName();
		tech.dockerProcessThread.reset(new std::thread([secret,repo,initMessage,dockerName]()
		{
			try
			{
				TryDocker("pull "+repo);
				TryDocker("run --rm --name "+dockerName+" "+repo+" technite --url="+serviceURL+":"+String(serviceHost.GetPort())+" --secret="+ secret.ToFullString()+" "+initMessage);
			}
			catch (const std::exception&ex)
			{
				LogEvent(EventSignificance::NonFatalError,"Failed to launch technite process: "+String(ex.what()));
			}
		}));
		return at;
	}




}



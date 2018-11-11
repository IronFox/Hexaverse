#include "scriptedAI.h"
#include <lua/luaGameAPI.h>
#include <objects/faction.h>
#include "clientEvents.h"
#include "objectNetworkHelper.h"
#include "network.h"
#include "visual/hud/windowStack.h"
#include "visual/hud/hexGrid.h"
#include "visual/hud/dialog.h"
#include "visual/hud/guiComponents.h"
#include <io/string_file.h>
#include "io.h"
#include "playerControlBegin.h"
#include "clientRuntimeConfig.h"

namespace ScriptedAI
{
	using namespace DeltaWorks;
	bool			visualDebugActive = false;
	Observable<>	onExecutionFinished;
	
	namespace Details
	{
		index_t			frameBeginRegistration = InvalidIndex;
		PathString		scriptPath;
		FileSystem::ftime_t	scriptTime;
		bool			reloadScript = true;
		Timer::Time		completionTime = timer.Now();

		bool			completedThisPhase = false,
						exportedThisPhase = false;

		/**
		Indicates that no further tasks should be scheduled with the thread,
		and instead execution should await some synchronized callbacks
		*/
		bool			stopAndSynchronize = false;
		bool			rebuildEntitiesOnSynchronize = false;

		index_t			controlBeginRegistration = InvalidIndex,
						selectionChangedRegistration = InvalidIndex;

		HashTable<TUniqueID,TUniqueID>	awaitEntity;	//entity ID -> request ID


		FileSystem::Folder	GetScriptFolder()
		{
			FileSystem::Folder rs = RuntimeConfig::sharedConfig.Lookup("AIScripts");
			if (!rs.IsValidLocation())
				FileSystem::ForceCreateDirectory(rs.GetLocation());

			return rs;
		}


		class Thread : public Sys::ThreadObject
		{
			typedef Sys::ThreadObject Super;
		public:
			Lua::API		api;
			Lua::Function<Lua::Reference()> exportState;
			Lua::Function<void(Lua::Reference)> init;
			Lua::Function<String(Lua::Reference)> checkStateIsLoopFree;
			Lua::Function<void()> execute,selectionChanged;

			bool			disallowEnqueue = false;

			Array<BYTE>		nonCompressedExportedState;
			bool			hasSuccessfullyExecuted = false;

			typedef std::function<void(Thread&)> FEvent;
			typedef std::pair<String,FEvent>	Event;


			void			Enqueue(const String&title, const FEvent&func)
			{
				ASSERT__(!disallowEnqueue);
				queue << std::make_pair(title,func);
			}

			void			Quit()
			{
				quit = true;
				queue.OverrideSignalNow();
				AwaitCompletion();
			}

			bool			IsBusy() const
			{
				queueLock.lock();
					bool rs = isRunning || queue.IsNotEmpty();
				queueLock.unlock();
				return rs;
			}

			void			DestroyAPI()
			{
				if (api.IsCreated())
				{
					exportState.ClearReference();
					init.ClearReference();
					execute.ClearReference();
					selectionChanged.ClearReference();
					api.Destroy();
				}

			}

			String		GetErrors()
			{
				if (errorQueue.IsEmpty())
					return "";
				Buffer0<String> localErrorQueue;
				errorQueue.FlushContentTo(localErrorQueue);
				StringBuffer rs;
				foreach (localErrorQueue,err)
				{
					if (rs.IsNotEmpty())
						rs << nl;
					rs << *err;
				}
				return rs.ToStringRef();
			}

			bool		CallbacksDone()
			{
				return thread.api.onDeployedCallbacks.IsEmpty()
					||
					timer.GetSecondsSince(thread.api.lastDeploymentCallbackRegistered) > 3.f;
			}

			virtual void ThreadMain() override
			{
				Buffer0<Event,Swap>	workItems;
				while (!quit)
				{
					workItems.Clear();
					isRunning = false;
					queue.WaitForContent();
					queueLock.lock();
						isRunning = true;
						queue.FlushContentTo(workItems);
					queueLock.unlock();
				
					foreach (workItems, item)
					{
						LogEvent(EventClass::Preset::debug,"ScriptedAI.Thread: Executing "+item->first);
						const auto t = timer.Now();
						try
						{
							item->second(*this);
						}
						catch (const Lua::Exception&ex)
						{
							auto error = ex.GetRawMessage();
							errorQueue << error;
							LogEvent(EventClass::Preset::scriptError,"Failed to execute '"+item->first+"'. Lua exception: '"+error+"'");
						}
						catch (const std::exception&ex)
						{
							auto error = ex.what();
							errorQueue << error;
							LogEvent(EventClass::Preset::scriptError,"Failed to execute '"+item->first+"'. Standard exception: '"+error+"'");
						}
						catch (...)
						{
							errorQueue << "???";
							//error = "???";
							LogEvent(EventClass::Preset::scriptError,"Failed to execute '"+item->first+"'. Unsupported exception type");
						}
						const float seconds = timer.GetSecondsSince(t);
						#ifndef _DEBUG
						if (seconds > 0.5f)
						#endif
							LogEvent(EventClass::Preset::scriptLog,"ScriptedAI: Executed "+item->first+" in "+String(timer.GetSecondsSince(t)));
						#ifndef _DEBUG
						else
							LogEvent(EventSignificance::PerformanceMeasurement,"ScriptedAI.Thread: Executed "+item->first+" in "+String(timer.GetSecondsSince(t)));
						#endif

					}
					workItems.Clear();
				}
				isRunning = false;
			}

			void	Start()
			{
				quit = false;
				Super::Start();
			}

			void	FlushQueue(BasicBuffer<Event,Swap>&destBuffer)
			{
				queueLock.lock();
				queue.FlushContentTo(destBuffer);
				queueLock.unlock();
			}

		private:
			mutable Sys::SpinLock	queueLock;
			WorkPipe<Event,Swap>	queue;
			WorkPipe<String,Swap>	errorQueue;

			volatile bool	isRunning = false;
			volatile bool	quit = false;

		}	thread;

		namespace Callback
		{
			void	AttackTargets(const TUniqueID&entityID, Array<Grid::LCellIdI>&disposableTargets)
			{
				Protocol::EntityDirectCommand	cmd;
				cmd.id = entityID;
				cmd.mode = (UINT16)Objects::CursorMode::AttackMode;
				cmd.path.swap(disposableTargets);
				cmd.directFlags = ObjectHelper::AssembleMotionFlags(false, true, false);
				Network::Channels::entityDirectCommand.SendTo(Network::client, cmd);
			}

			void	DetailedDirectTo(const TUniqueID&entityID, const ArrayRef<Grid::LCellIdI>&path)
			{
				Protocol::EntityDirectCommand cmd;
				cmd.id = entityID;
				cmd.mode = (UINT16)Objects::CursorMode::Idle;
				cmd.path = path;
				cmd.directFlags = ObjectHelper::AssembleMotionFlags(true, false, false);
				Network::Channels::entityDirectCommand.SendTo(Network::client, cmd);
			}

			void	DropAllTasks(const TUniqueID&entityID)
			{
				Protocol::DropTasks cmd;
				cmd.dropMode = Protocol::DropMode::AllTasks;
				cmd.targetID = entityID;
				Network::Channels::dropTasks.SendTo(Network::client, cmd);
			}

			void	DropSafeTasks(const TUniqueID&entityID)
			{
				Protocol::DropTasks cmd;
				cmd.dropMode = Protocol::DropMode::AllSafeTasks;
				cmd.targetID = entityID;
				Network::Channels::dropTasks.SendTo(Network::client, cmd);
			}

			void	UpdateMobileAutomation(const TUniqueID&entityID, bool doAutomate)
			{
				Protocol::EntityStateUpdate update;
				update.entityID = entityID;
				update.doSet = doAutomate;
				Network::Channels::automate.SendTo(Network::client, update);
			}

			void	UpdateSleepState(const TUniqueID&entityID, bool doSleep)
			{
				Protocol::EntityStateUpdate update;
				update.entityID = entityID;
				update.doSet = doSleep;
				Network::Channels::sleep.SendTo(Network::client, update);
			}


			void	EnableWorkingStructure(const TUniqueID&entityID, bool doEnable)
			{
				Protocol::EnableStructure cmd;
				cmd.enable = doEnable;
				cmd.id = entityID;
				Network::Channels::enableStructure.SendTo(Network::client, cmd);
			}

			Buffer0<Protocol::Marker> createdMarkers;
			Buffer0<TUniqueID> destroyedMarkers,touchedMarkers;

			void	Submit()
			{
				foreach(createdMarkers, m)
					ObjectHelper::SignalMarkerCreation(*m);
				createdMarkers.Clear();
				foreach(destroyedMarkers, m)
					ObjectHelper::SignalMarkerDeletion(*m);
				destroyedMarkers.Clear();
				foreach (touchedMarkers,m)
				{
					auto marker = Objects::Marker::Find(*m);
					if (marker)
						ObjectHelper::SignalMarkerTouch(*marker);
				}
				touchedMarkers.Clear();
			}

			void	MarkerCreate( const TUniqueID&mID, Objects::MarkerType t, const Objects::PWorldEntity&e, const Grid::LCellIdI&loc )
			{
				Protocol::Marker&marker = createdMarkers.Append();
				marker.id = mID;
				marker.createdInRound = RuntimeConfig::roundNumber;
				static_assert(Objects::MarkerType::EntityDeath==0, "MarkerTyp expectation violation");
				static_assert(Objects::MarkerType::HostileContact==1, "MarkerTyp expectation violation");
				static_assert(Objects::MarkerType::Visited == 2, "MarkerTyp expectation violation");
				switch (t)
				{
					case Objects::MarkerType::EntityDeath:
						ObjectHelper::FillMarker(marker, e, marker.data.Set0());
					break;
					case Objects::MarkerType::HostileContact:
						ObjectHelper::FillMarker(marker, e, marker.data.Set1());
						break;
					case Objects::MarkerType::Visited:
						marker.data.Set2();
						break;
				}
				marker.location = loc;
			}

			void	MarkerDestroy( const TUniqueID&mID )
			{
				destroyedMarkers << mID;
			}

			void	MarkerTouch( const TUniqueID&mID )
			{
				touchedMarkers << mID;
			}

			void	RemoveEntity( const TUniqueID&eID )
			{
				Network::Channels::removeEntity.SendTo(Network::client,eID);
			}

			void	DropLastTask( const TUniqueID&eID )
			{
				Protocol::DropTasks drop;
				drop.dropMode = Protocol::DropMode::LastTask;
				drop.targetID = eID;
				Network::Channels::dropTasks.SendTo(Network::client,drop);
			}

			void	BeginConstruction( const TUniqueID&eID, const Objects::EntityType&type )
			{
				Protocol::ConstructionBegin begin;
				begin.plantID = eID;
				begin.type = type;
				Network::Channels::constructionBegin.SendTo(Network::client,begin);
			}

			void	AbortConstruction( const TUniqueID&eID, const TUniqueID&tID )
			{
				Protocol::ConstructionAbort abort;
				abort.plantID = eID;
				abort.taskId = tID;
				Network::Channels::constructionAbort.SendTo(Network::client,abort);
			}

			void	DeployStored( const TUniqueID&hID, const TUniqueID&mID, const Grid::LCellIdI&loc, const TUniqueID&requestID )
			{
				Protocol::ChoiceIntersection ch;
				ch.id = hID;
				ch.deployingWhat.Set0() = mID;
				ch.mode = (UINT16)Objects::CursorMode::Idle;
				ch.target = loc;
				if (requestID.IsNotEmpty())
					ch.requestID.Set0() = requestID;
				Network::Channels::choiceIntersection.SendTo(Network::client,ch);
			}

			void	DeployCompletedConstruction( const TUniqueID&eID, const Grid::LCellIdI&loc, const TUniqueID&requestID )
			{
				Protocol::ChoiceIntersection ch;
				ch.id = eID;
				ch.mode = (UINT16)Objects::CursorMode::Idle;
				ch.target = loc;
				if (requestID.IsNotEmpty())
					ch.requestID.Set0() = requestID;
				Network::Channels::choiceIntersection.SendTo(Network::client,ch);
			}

			void	BeginStructureConstruction( const TUniqueID&eID, const Objects::EntityType&type )
			{
				Protocol::StandardJob job;
				job.jobName = ConstName::Job_Construct;
				job.parameterName = type;
				job.ownerID = eID;
				Network::Channels::signalNewJob.SendTo(Network::client,job);
			}

			void	QueueResearch( index_t topicIndex, bool doDrop )
			{
				Protocol::QueueResearch queue;
				queue.research = (UINT32) topicIndex;
				queue.drop = doDrop;
				Network::Channels::QueueResearch.SendTo(Network::client,queue);
			}

			Lua::API::TExportResult CheckExportValidity( const Lua::Reference&ref )
			{
				Lua::API::TExportResult rs;
				rs.maxSerialSize = Protocol::ScriptedAIState{}.dataVector.MaxElementCount;
				try
				{
					String err = thread.checkStateIsLoopFree(ref);
					if (err.IsNotEmpty())
					{
						rs.error = Lua::API::TExportResult::ErrorType::TableLoop;
						rs.errorExt = err;
					}
					else
					{
						ByteStreamBuffer stream;
						ref.Serialize(stream);
						const auto data = stream.GetWrittenData();

						Array<BYTE> temp(data.GetLength()+100);
						const size_t compressed = BZ2::compress(data.GetPointer(),data.GetLength(),temp.pointer()+4,temp.GetLength()-4);
						rs.serialSize = compressed;
						if (rs.serialSize > rs.maxSerialSize)
							rs.error = Lua::API::TExportResult::ErrorType::TooLarge;
					}
				}
				catch (const std::exception&ex)
				{
					rs.errorExt = ex.what();
					rs.error = Lua::API::TExportResult::ErrorType::Other;
				}
				return rs;
			}

		}


		Buffer0<std::pair<String,std::function<void()> > > onSynchronized;
		Buffer0<Thread::Event,Swap> onSynchronizedEnqueue;
		Array<BYTE>			transientImportState;

		void			FlushAndRebuildEntities()
		{
			const auto t0 = timer.Now();
			LogEvent(EventClass::Preset::debug,"ScriptedAI: Rebuilding entities and local research...");
			ASSERT__(!thread.IsBusy());
			thread.api.FlushAllObjects();
			thread.api.UpdateResearch();
			thread.api.AddAllFactionsAndAlliances();
			thread.api.AddAllEntities();
			thread.api.AddMyMarkers();
			thread.api.AddObjectAndPathfindingFunctions(RuntimeConfig::aiMode);
			#ifdef HAS_WINDOW
				thread.api.SetSelectedUnit(Objects::selectedEntity,false);
			#endif
			LogEvent(EventSignificance::PerformanceMeasurement,"ScriptedAI: Rebuilt entities in "+String(timer.GetSecondsSince(t0))+" seconds");
		}


		bool			SynchronizeWithThread()
		{
			if (stopAndSynchronize)
			{
				if (rebuildEntitiesOnSynchronize)
				{
					rebuildEntitiesOnSynchronize = false;
					FlushAndRebuildEntities();
				}
				thread.disallowEnqueue = true;	//make sure no synchronized event enqueues job (which can just start right away)
				for (index_t i = 0; i < onSynchronized.Count(); i++)
				{
					const auto&sync = onSynchronized[i];
					LogEvent(EventSignificance::Debug,"ScripedAI: On synchronized: Executing "+sync.first+"...");
					sync.second();
				}
				onSynchronized.Clear();
				thread.disallowEnqueue = false;
				stopAndSynchronize = false;	//must unset here or later since some sync function do try to enqueue

				foreach (onSynchronizedEnqueue,sync)
				{
					thread.Enqueue(sync->first,sync->second);
				}
				onSynchronizedEnqueue.Clear();

				return true;
			}
			return false;

		}

		void			SynchronizeWithThread(bool flushQueued, bool flushAndRebuildEntitiesOnSynchronize, const String&name, const std::function<void()>&exec)
		{
			stopAndSynchronize = true;
			Buffer0<Thread::Event,Swap>	buffer;
			thread.FlushQueue(onSynchronizedEnqueue);

			if (flushQueued)
			{
				onSynchronized.Clear();
				onSynchronizedEnqueue.Clear();
				rebuildEntitiesOnSynchronize = flushAndRebuildEntitiesOnSynchronize;
			}
			else
			{
				rebuildEntitiesOnSynchronize |= flushAndRebuildEntitiesOnSynchronize;
			}


			if (exec)
				onSynchronized << std::make_pair(name,exec);


			if (!thread.IsBusy())
				SynchronizeWithThread();
		};

		

		void			ThreadEnqueue(const String&name, const std::function<void(Thread&)>&exec)
		{
			if (stopAndSynchronize)
				onSynchronizedEnqueue << std::make_pair(name,exec);
			else
				thread.Enqueue(name,exec);
		}


		void			RegisterExecution()
		{
			if (controlBeginRegistration != InvalidIndex)
				return;
			controlBeginRegistration = GlobalEvents::clientControlBegin.Register(ScriptedAI::Start);

		}

		void			UnregisterExecution()
		{
			GlobalEvents::clientControlBegin.UnregisterAndSetInvalid(controlBeginRegistration);
			#ifdef HAS_WINDOW
				Objects::onSelectionChanged.UnregisterAndSetInvalid(Details::selectionChangedRegistration);
			#endif
		}
	


		void Interrupt(const String&cause, bool forceReload)
		{
			SynchronizeWithThread(true,false,"Interrupt ("+cause+")",[forceReload]()
			{
				if (forceReload)
					Reload(true,true);
			});
		}


		
		void PerFrameCheck( )
		{
			auto&thread = Details::thread;
			namespace Callback = Details::Callback;
			if (!thread.IsBusy() && thread.CallbacksDone())
			{
				if (Details::SynchronizeWithThread())
					return;

				if (!Details::exportedThisPhase && WillExecuteOnNewRound() && thread.exportState)
				{
					Details::ThreadEnqueue("Export State",[](Details::Thread&thread)
					{
						if (!thread.exportState)
							return;
						Lua::Reference ref;
						ref = thread.exportState();
						if (!ref)
							thread.nonCompressedExportedState.Clear();
						else
						{
							String err = thread.checkStateIsLoopFree(ref);
							if (err.IsNotEmpty())
							{
								LogEvent(EventClass(EventSignificance::NonFatalError,EventFlavor::UserError),"Error exporting AI state: "+err.SubString(0,32)+". Rejecting export");
								thread.nonCompressedExportedState.Clear();
							}
							else
							{
								ByteStreamBuffer stream;
								ref.Serialize(stream);
								thread.nonCompressedExportedState = stream.GetWrittenData();
							}
						}

						if (Network::client.IsConnected() && Network::haveFaction)
						{
							Protocol::ScriptedAIState compressedState;
							compressedState.scriptName = FileSystem::PathToString(FileSystem::ExtractFileName(Details::scriptPath));
							if (thread.nonCompressedExportedState.IsNotEmpty())
							{
								Array<BYTE> temp(thread.nonCompressedExportedState.GetLength()+100);
								const size_t compressed = BZ2::compress(thread.nonCompressedExportedState.pointer(),thread.nonCompressedExportedState.GetLength(),temp.pointer()+4,temp.GetLength()-4);
								compressedState.dataVector.ResizeAndCopy(temp,compressed+4);
								(*(UINT32*)compressedState.dataVector.GetPointer()) = (UINT32)thread.nonCompressedExportedState.GetLength();
							}
							if (compressedState.dataVector.GetLength() <= compressedState.dataVector.MaxElementCount)
								Network::Channels::ScriptedAIState.SendTo(Network::client,compressedState);
							else
								LogEvent(EventClass::Preset::scriptError,"Failed to upload script state to server: Maximum state size ("+String(compressedState.dataVector.MaxElementCount)+" byte(s)) exceeded: "+String(compressedState.dataVector.GetLength())+" bytes");
						}
					});
					Details::exportedThisPhase = true;
					return;
				}


				GlobalEvents::frameBegin.UnregisterAndSetInvalid(Details::frameBeginRegistration);


				Callback::Submit();
				String err = thread.GetErrors();
				if (err.IsNotEmpty())
				{
					#ifdef HAS_WINDOW
						HUD::Dialog::ShowMessage("Something seems to have gone wrong execting the Lua AI script:\n" + err);
					#endif
				}

				if (!thread.hasSuccessfullyExecuted)
					Details::UnregisterExecution();

				onExecutionFinished();

				Details::completionTime = timer.Now();
				Details::completedThisPhase = true;

				PlayerControlBegin::CheckAutoNextRound();
				#ifdef HAS_WINDOW
					HUD::HexGrid::RefreshButtonState();
				#endif
			}
		}


		void HandleCallbackCandidate(const TUniqueID&reqID, const Objects::PWorldEntity&e)
		{
			if (e)
			{
				LogEvent(EventClass::Preset::scriptLog,e.get(), "Identified callback candidate entity: "+e->GetName());
				ASSERT__(e->isOwnedByMe);
				Details::ThreadEnqueue("Invoke callback function "+ToString(reqID),[e,reqID](Details::Thread&thread)
				{
					thread.nonCompressedExportedState.Clear();
					thread.api.SignalDeployment(reqID,e);
				});
			}
			else
			{
				LogEvent(EventClass::Preset::scriptLog,"Recorded callback result (failure) of "+ToString(reqID));
				Details::ThreadEnqueue("Remove callback function "+ToString(reqID),[reqID](Details::Thread&thread)
				{
					thread.api.onDeployedCallbacks.Unset(reqID);
				});
			}

		}

	}


	void	Resume( const String&scriptName, Array<BYTE>&compressedState )
	{
		using namespace Details;

		if (scriptName.IsEmpty())
		{
			SynchronizeWithThread(true,false,"End",[]()
			{
				thread.DestroyAPI();
				Details::UnregisterExecution();
				Details::scriptPath = "";
			});
			return;
		}

		FileSystem::Folder scripts = GetScriptFolder();
		FileSystem::File scriptLocation;

		if (!scripts.FindFile(PathString(scriptName)+".lua",scriptLocation))
		{
			LogEvent(EventClass::Preset::scriptError,"ScriptedAI: Unable to resume server-stored script '"+scriptName+"'. Cannot find file.");
			#ifdef HAS_WINDOW
				HUD::Dialog::ShowMessage("The server suggested AI script '"+scriptName+"', but the corresponding script file apparently doesn't exist. Server-stored AI state cannot be applied.");
			#endif
			return;
		}
		LogEvent(EventSignificance::Important,"ScriptedAI: Attempting to resume script '"+scriptName+"', using "+String(compressedState.GetLength())+" bytes of compressed state");
		
		if (compressedState.GetLength() <= 8)
			transientImportState.Clear();
		else
		{
			static const size_t MaxSafeSize = 10000000;
			size_t decompressedSize = *(const UINT32*)compressedState.GetPointer();
			if (decompressedSize > MaxSafeSize)
			{
				LogEvent(EventClass::Preset::scriptError,"Decompressed data size ("+String(decompressedSize)+") exceeds safe size ("+String(MaxSafeSize)+")");
				transientImportState.Clear();
			}
			else
			{
				transientImportState.SetSize(decompressedSize);
				size_t rs = BZ2::decompress(compressedState.GetPointer()+4,compressedState.GetContentSize()-4,transientImportState.GetPointer(),decompressedSize);
				if (rs != decompressedSize)
				{
					LogEvent(EventClass::Preset::scriptError,"Decompression error. BZ2 reports "+String(rs)+" decompressed bytes. Expected "+String(decompressedSize));
					transientImportState.Clear();
				}
				else
					LogEvent(EventClass::Preset::common,"ScriptedAI: Decompressed "+String(decompressedSize)+" of state bytes for script '"+scriptName+"'");
			}
		}

		SynchronizeWithThread(true,rebuildEntitiesOnSynchronize,"AdoptState",[scriptLocation]()
		{
			thread.nonCompressedExportedState.swap(transientImportState);
			Load(scriptLocation,false);
		});
	}

	bool	WillExecuteOnNewRound()
	{
		return Details::controlBeginRegistration != InvalidIndex;
	}

	void	Reload(bool force, bool freshState)
	{
		ASSERT__(IsLoaded());
		const bool doLoad = force || FileSystem::GetModificationTime(Details::scriptPath) != Details::scriptTime;
		if (doLoad)
			Load(Details::scriptPath,freshState);
	}

	void	Load(const PathString&scriptPath, bool freshState)
	{
		ASSERT__(!Details::thread.IsBusy());
		
		static bool installed = false;
		if (!installed)
		{
			GlobalEvents::phaseChange << []()
			{
				Details::Interrupt("Phase change detected (->"+String(RuntimeConfig::phase.ToString())+")",false);
				Details::exportedThisPhase = false;
				Details::completedThisPhase = false;
			};

			GlobalEvents::objectFlush << []()
			{
				Details::Interrupt("Flushed scene",true);

			};
			installed = true;
		}

		auto&thread = Details::thread;
		thread.DestroyAPI();
		
		Details::reloadScript = true;
		Details::completionTime = timer.Now();
		Details::completedThisPhase = true;		//upon (re)loading, is considered having executed this phase so player can request new round
		thread.hasSuccessfullyExecuted = true;	//set true here, since we don't know yet

		Details::awaitEntity.Clear(); 
		thread.api.onDeployedCallbacks.Clear();

		auto&api = thread.api;

		namespace Callback = Details::Callback;

		api.onAttackTargets = Callback::AttackTargets;
		api.onDetailedDirectTo = Callback::DetailedDirectTo;
		api.onDropAllTasks = Callback::DropAllTasks;
		api.onDropSafeTasks = Callback::DropSafeTasks;
		api.onEnableWorkingStructure = Callback::EnableWorkingStructure;
		api.onUpdateMobileAutomation = Callback::UpdateMobileAutomation;
		api.onUpdateSleep = Callback::UpdateSleepState;
		api.onMarkerCreate = Callback::MarkerCreate;
		api.onMarkerDestroy = Callback::MarkerDestroy;
		api.onMarkerTouch = Callback::MarkerTouch;

		api.onRemoveEntity =Callback::RemoveEntity;
		api.onDropLastTask = Callback::DropLastTask;
		api.onBeginProduction = Callback::BeginConstruction;
		api.onBeginStructureConstruction = Callback::BeginStructureConstruction;
		api.onAbortProduction = Callback::AbortConstruction;
		api.onDeployCompletedProduction = Callback::DeployCompletedConstruction;
		api.onDeployStored = Callback::DeployStored;
		api.onQueueResearch = Callback::QueueResearch;

		api.onCheckExportValidity = Callback::CheckExportValidity;


		api.Create("AI:"+FileSystem::PathToString(FileSystem::ExtractFileName(scriptPath)));
		api.AddTypes();
		api.AddGrid();	//must be executed by main thread, because mapped entities need it, which are added by main thread, too

		Details::exportedThisPhase = false;
		Details::scriptPath = scriptPath;
		Details::scriptTime = FileSystem::GetModificationTime(scriptPath);
		if (freshState)
			thread.nonCompressedExportedState.Clear();

		if (!thread.IsActive())
		{
			//is started only once. doesn't quit until the program shuts down
			thread.Start();
			GlobalEvents::applicationTerminate << [](){Details::thread.Quit();};
		}
		Details::RegisterExecution();
	}
	

	void	Start()
	{
		if (Details::frameBeginRegistration == InvalidIndex)
		{
			Details::frameBeginRegistration = GlobalEvents::frameBegin.Register(Details::PerFrameCheck);


			#ifdef HAS_WINDOW
				if (Details::selectionChangedRegistration == InvalidIndex)
					Details::selectionChangedRegistration = Objects::onSelectionChanged.Register([]()
					{
						if (Details::thread.selectionChanged)
						{
							if (Details::frameBeginRegistration == InvalidIndex)
								Details::frameBeginRegistration = GlobalEvents::frameBegin.Register(Details::PerFrameCheck);

							Details::onSynchronizedEnqueue << std::make_pair("OnSelectionChanged",[](Details::Thread&thread)
							{
								if (thread.selectionChanged)
									thread.selectionChanged();
							});
							Details::SynchronizeWithThread(false,true,"OnSelectionChanged",[]()	//enqueues scheduled onSynchronizedEnqueue if could synchronized now
							{
								if (Details::thread.selectionChanged)
									Details::thread.api.SetSelectedUnit(Objects::selectedEntity,false);
							});
						}
					});
			#endif
		}

		Details::SynchronizeWithThread(true, true,"Start",[]()
		{

			auto&thread = Details::thread;

			thread.api.onDeployedCallbacks.Clear();
			Details::awaitEntity.Clear();
			thread.hasSuccessfullyExecuted = false;

			if (Details::reloadScript)
			{
				Details::ThreadEnqueue("Map Terrain",[](Details::Thread&thread)
				{
					auto&api = thread.api;
					api.AddTerrain();
				});


				Details::ThreadEnqueue("Load Script",[](Details::Thread&thread)
				{
					auto&api = thread.api;

					Details::reloadScript = false;

					api.program->ExecuteFile(Details::scriptPath);
					thread.exportState = api.program->RequireGlobal("ExportState");
					thread.init = api.program->RequireGlobal("Init");
					thread.execute = api.program->RequireGlobal("Execute");
					thread.selectionChanged = api.program->FindGlobal("OnSelectionChanged");

					#define LOOP_CHECK_FUNCTION_NAME "_hexaverse_export_loop_check__"

					api.program->Execute(
						"function " LOOP_CHECK_FUNCTION_NAME "_inner(state,exclude,history)\n"
							"for k,v in pairs(state) do\n"
								"if type(v) == 'table' then\n"
									"if exclude[v] ~= nil then\n"
										"return 'Table loop detected in exported state: '..history..'->'..tostring(k);\n"
									"end;\n"
									"exclude[v] = true;\n"
										"local rs = " LOOP_CHECK_FUNCTION_NAME "_inner(v,exclude,history..'->'..tostring(k));\n"
										"if rs ~= nil then return rs; end;\n"
									"exclude[v] = nil;\n"
								"end;\n"
							"end;\n"
							"return nil;\n"
						"end;\n"
						"function " LOOP_CHECK_FUNCTION_NAME "(state)\n"
							"local rs = " LOOP_CHECK_FUNCTION_NAME "_inner(state,{},'');\n"
							"if rs == nil then rs = ''; end;\n"
							"return rs;\n"
						"end;\n"
					);
					thread.checkStateIsLoopFree = api.program->RequireGlobal(LOOP_CHECK_FUNCTION_NAME);
				});

				Details::ThreadEnqueue("Script Init",[](Details::Thread&thread)
				{
					auto&api = thread.api;

					if (thread.nonCompressedExportedState.IsNotEmpty())
					{
						auto state = api.program->DeserializeToReference(
							MemReadStream(
								thread.nonCompressedExportedState.GetPointer(),
								(serial_size_t)thread.nonCompressedExportedState.GetContentSize()
							)
						);
						thread.init(state);
					}
					else
						thread.init(Lua::nil);
				});
			}

			Details::ThreadEnqueue("Run",[](Details::Thread&thread)
			{
				auto&api = thread.api;
				thread.nonCompressedExportedState.Clear();	//state is lost
				thread.execute();
				thread.hasSuccessfullyExecuted = true;
			});
		});

		//thread.Start();
	}



	void			SignalAppearance(const Objects::PWorldEntity&e)
	{
		if (!IsLoaded())
			return;
		TUniqueID reqID;
		if (Details::awaitEntity.QueryAndUnset(e->id,reqID))
		{
			Details::HandleCallbackCandidate(reqID,e);
		}
	}

	void			SignalChoiceResponse(const TUniqueID&requestID, const TUniqueID&entityID)
	{
		if (!IsLoaded() || requestID.IsEmpty())
			return;
		if (entityID.IsEmpty())
		{
			Details::HandleCallbackCandidate(requestID,nullptr);

		}
		else
		{
			Objects::PWorldEntity e = Objects::GetEntity(entityID);
			if (!e)
			{
				LogEvent(EventClass::Preset::scriptLog,"Recorded callback result (entity "+ToString(entityID)+") of "+ToString(requestID));
				Details::awaitEntity.Set(entityID,requestID);
			}
			else
			{
				Details::HandleCallbackCandidate(requestID,e);
			}
		}

	}


	bool	IsRunning()
	{
		return Details::thread.IsBusy() || Details::stopAndSynchronize;
	}


	bool			IsLoaded()
	{
		return Details::scriptPath.IsNotEmpty();
	}

	D::PathString	GetScriptPath()
	{
		return Details::scriptPath;
	}


	bool			HasSuccessfullyExecuted()
	{
		return Details::thread.hasSuccessfullyExecuted;
	}


	D::PathString	GetDocumentationPath()
	{
		return FileSystem::Folder().FindFile("../Lua/General Lua API Documentation.txt")->GetLocation();
	}


	void	CreateTemplateAt(const D::PathString&path)
	{
		StringFile f;
		f.Create(path);

		f << "--[[" << nl
			<< "Generated Lua AI file. Fill out the details to define what the AI is supposed to do." << nl
			<< "You have access to a set of API functions to examine units and instruct them," << nl
			<< "documented in " <<
			FileSystem::PathToString(GetDocumentationPath()) << nl
			<< "]]" << nl << nl

			<< "--[[" << nl
			<< "@brief State export" << nl << nl
			<< "This function is expected to export any persistent AI state." << nl
			<< "Whatever you return will be fed to Init() when resuming interrupted execution." << nl
			<< "Anything you keep in the local Lua state but don't export here will be lost during interruptions" << nl
			<< "(e.g. save game loading, client crash, AI script reloading during debug, etc)." << nl
			<< "Possibly use Game.CheckStateExportValidity(*) to determine validity of return values." << nl
			<< "@return Exported state. May be any regular Lua type, including arbitrarily nested tables, or"<<nl
			<< "plain nil. Avoid table loops." << nl
			<< "]]" << nl
			<< "function ExportState()" << nl
			<< "\treturn nil;" << nl
			<< "end;" << nl << nl

			<< "--[[" << nl
			<< "@brief Initialization" << nl << nl
			<< "Invoked ONCE before the first execution. If a previously exported state exists, it is passed" <<nl
			<< "as parameter." << nl
			<< "@param state Some previously exported state, or nil." << nl
			<< "]]" << nl
			<< "function Init(state)" << nl
			<< "end;" << nl << nl

			<< "--[[" << nl
			<< "@brief Optional selection change reaction function" << nl << nl
			<< "If defined, executed when the local client provides user interaction and the user has selected" << nl
			<< "some other entity, or deselected the previously selected entity." << nl
			<< "]]" << nl
			<< "function OnSelectionChanged()" << nl
			<< "\t-- Your handling here. Query Game.selectedUnit to find the selected unit (if any) " << nl
			<< "end;" << nl << nl

			<< "--[[" << nl
			<< "@brief AI execution entry point" << nl << nl
			<< "This function is executed whenever the AI is supposed to do something." << nl
			<< "During production runs, this function will be called exactly once at the beginning of a round." << nl
			<< "Executed asynchronously. May take as much time as needed, though bad things WILL happen if it" << nl
			<< "takes longer than the server permits any player. Also, endless loops are bad as those will" << nl
			<< "force the user to restart the client. There is currently no kill-switch." << nl
			<< "]]" << nl
			<< "function Execute()" << nl
			<< "\t-- Your AI goes here" << nl
			<< "\t-- Again, check API documentation in " <<
			FileSystem::PathToString(GetDocumentationPath()) << nl
			<< "end;" << nl << nl

			;

	}

	void	CreateAndLoad(const PathString&path)
	{
		CreateTemplateAt(path);
		Load(path,true);
		HUD::HexGrid::RefreshButtonState();
		LogEvent(EventSignificance::Uncommon,"New AI script "+FileSystem::PathToString(FileSystem::ExtractFileNameExt(path))+" created");
	}

	D::Timer::Time	GetCompletionTime()
	{
		return Details::completionTime;
	}


	bool			DoneThisRound()
	{
		if (!IsLoaded())
			return true;
		if (IsRunning())
			return false;
		return Details::completedThisPhase && Details::thread.CallbacksDone();
	}

	bool			SuccessfullyExecutedThisRound()
	{
		return Details::completedThisPhase && Details::thread.hasSuccessfullyExecuted;
	}



	#ifdef HAS_WINDOW
	void	DefineCommands(const Objects::Command::PCommandGroup&aiCommands)
	{
		using namespace Objects::Command;

		aiCommands->Add(new GlobalCommand("Load AI...",
		[]()
		{
			using namespace HUD;
			using namespace HUD::Component;
			PWindow window = Window::Create();

			FileSystem::FolderScan scripts = Details::GetScriptFolder();
			if (!scripts.IsValidLocation())
				FileSystem::ForceCreateDirectory(scripts.GetLocation());

			FileSystem::File file;
			WWindow w = window;
			Buffer0<PButton> buttons;
			while (scripts.NextFile(".lua",file))
			{
				PButton button {new Button{FileSystem::PathToString(file.GetInnerName()),"Load this AI script",[file,w]()
				{
					PWindow window = w.lock();
					WindowStack::Hide(window);
					Load(file.GetLocation(),true);

					HUD::HexGrid::RefreshButtonState();
				}}};
				buttons << button;

				window->AnchorBelowPrevious(CLOCATION,button);
			};

			for (index_t i = 0; i < buttons.Count(); i++)
			{
				buttons[i]->SetCutCorners(M::Quad<bool>(true,i+1 == buttons.Count(),true,i==0));
			}

			window->AnchorBelowPrevious(CLOCATION,new Label{"Press Escape to abort"},0,0);
			WindowStack::Show(window);
		}));

		aiCommands->Add(new GlobalCommand("Create new AI...",
		[]()
		{
			static const String aiAlpha = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ";

			HUD::Dialog::Query("Enter name for new AI script","debug",aiAlpha,[](const StringRef&str)->bool
			{
				return str.GetLength() > 3;
			},
			[](const String&input)
			{
				FileSystem::Folder scripts = Details::GetScriptFolder();
				if (!scripts.IsValidLocation())
					FileSystem::CreateDirectory(scripts.GetLocation());
				ASSERT1__(scripts.IsValidLocation(), FileSystem::PathToString(scripts.IsValidLocation()));
				FileSystem::File scriptAt;
				const auto path = PathString(input+".lua");
				scripts.FindFile(path, scriptAt, false);
				if (scriptAt.DoesExist())
					HUD::Dialog::Confirm("The specified AI script '"+input+".lua' already exists. Replace?",[scriptAt]()
					{
						CreateAndLoad(scriptAt);
					});
				else
					CreateAndLoad(scriptAt);


					
			});
		}));

				

		const PIconCommand runCmd { new GlobalCommand("Reload and run AI",
		[]()
		{
			if (IsRunning())
				return;
			Reload(true,Engine::input.pressed[Key::Shift]);
			Start();
			HUD::HexGrid::RefreshButtonState();
		},
		[](bool controlEnabled)->Objects::CommandFlags
		{
			using namespace Objects;
			CommandFlags rs = CommandFlags::None;

			if (ScriptedAI::IsRunning())
				rs |= CommandFlags::IsPermaActive | CommandFlags::IsFlashing | CommandFlags::SoftFlashing;
			elif (controlEnabled && IsLoaded())
				rs |= CommandFlags::IsEnabled;
			return rs;
		})};

		aiCommands->Add(runCmd);

		aiCommands->Add(new GlobalCommand("Edit Loaded AI",
		[]()
		{
			OpenDocumentExternally(ScriptedAI::GetScriptPath());
		},
		[](bool controlEnabled)->Objects::CommandFlags
		{
			using namespace Objects;
			CommandFlags rs = CommandFlags::None;
			if (IsLoaded())
				rs |= CommandFlags::IsEnabled;
			return rs;
		}));

		aiCommands->Add(new GlobalCommand("Open AI Documentation",
		[]()
		{
			OpenDocumentExternally(ScriptedAI::GetDocumentationPath());
		}));

	}
	#endif
}


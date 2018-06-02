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

namespace ScriptedAI
{
	using namespace DeltaWorks;
	bool			visualDebugActive = false;
	Observable<>	onExecutionFinished;
	
	namespace Detail
	{
		index_t			frameBeginRegistration = InvalidIndex;
		PathString		scriptPath;
		FileSystem::ftime_t	scriptTime;
		bool			reloadScript = true;

		bool			completedThisPhase = false;

		index_t			controlBeginRegistration = InvalidIndex;

		HashTable<TUniqueID,TUniqueID>	awaitEntity;	//entity ID -> request ID

		void			RegisterExecution()
		{
			if (controlBeginRegistration != InvalidIndex)
				return;
			controlBeginRegistration = GlobalEvents::playerControlBegin.Register(ScriptedAI::Start);
		}

		void			UnregisterExecution()
		{
			if (controlBeginRegistration != InvalidIndex)
				GlobalEvents::playerControlBegin.Unregister(controlBeginRegistration);
			controlBeginRegistration = InvalidIndex;
		}
	}

	class Thread : public Sys::ThreadObject
	{
		typedef Sys::ThreadObject Super;
	public:
		Lua::API		api;
		Lua::Function<Lua::Reference()> exportState;
		Lua::Function<void(Lua::Reference)> init;
		Lua::Function<void()> execute;
		Array<BYTE>		exportedState;
		bool			freshState = false;
		bool			hasSuccessfullyExecuted = false;

		typedef std::function<void(Thread&)> FEvent;
		typedef std::pair<String,FEvent>	Event;


		void			Enqueue(const String&title, const FEvent&func)
		{
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
					LogEvent(EventClass::Preset::scriptLog,"Executing "+item->first);
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
					LogEvent(EventClass::Preset::scriptLog,"Executed "+item->first+" in "+String(timer.GetSecondsSince(t)));

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
	}


	bool	WillExecuteOnNewRound()
	{
		return Detail::controlBeginRegistration != InvalidIndex;
	}

	void	Reload(bool force, bool freshState)
	{
		ASSERT__(IsLoaded());
		const bool doLoad = force || FileSystem::GetModificationTime(Detail::scriptPath) != Detail::scriptTime;
		if (doLoad)
			Load(Detail::scriptPath,freshState);
	}

	void	Load(const PathString&scriptPath, bool freshState)
	{
		ASSERT__(!IsRunning());
		
		static bool installed = false;
		if (!installed)
		{
			GlobalEvents::phaseChange << []()
			{
				if (IsRunning())
				{
					LogEvent(EventClass::Preset::scriptError,"WARNING: Phase change detected (->"+String(RuntimeConfig::phase.ToString())+") but AI script is still busy");
					return;
				}
				Detail::completedThisPhase = false; 
				Detail::awaitEntity.Clear(); 
				thread.api.onDeployedCallbacks.Clear();
			};

			GlobalEvents::objectFlush << []()
			{
				Detail::awaitEntity.Clear();
				Reload(true,true);
				if (IsRunning())
				{
					LogEvent(EventClass::Preset::scriptError,"WARNING: Flushing scene while AI script is still busy.");
					return;
				}

			};
			installed = true;
		}

		if (thread.api.IsCreated())
		{
			Lua::Reference ref;
			if (thread.exportState.IsSet())
				ref = thread.exportState();
			if (!ref)
				thread.exportedState.Clear();
			else
			{
				ByteStreamBuffer stream;
				ref.Serialize(stream);
				thread.exportedState = stream.GetWrittenData();
			}
			thread.exportState.ClearReference();
			thread.init.ClearReference();
			thread.execute.ClearReference();
			thread.api.Destroy();
		}
		
		Detail::reloadScript = true;
		Detail::completedThisPhase = true;		//upon (re)loading, is considered having executed this phase so player can request new round
		thread.hasSuccessfullyExecuted = true;	//set true here, since we don't know yet

		auto&api = thread.api;

		api.onAttackTargets = Callback::AttackTargets;
		api.onDetailedDirectTo = Callback::DetailedDirectTo;
		api.onDropAllTasks = Callback::DropAllTasks;
		api.onEnableWorkingStructure = Callback::EnableWorkingStructure;
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


		api.Create("AI:"+FileSystem::PathToString(FileSystem::ExtractFileName(scriptPath)));
		api.AddGrid();	//must be executed by main thread, because mapped entities need it, which are added by main thread, too

		Detail::scriptPath = scriptPath;
		Detail::scriptTime = FileSystem::GetModificationTime(scriptPath);
		thread.freshState = freshState;

		if (!thread.IsActive())
		{
			thread.Start();
			GlobalEvents::applicationTerminate << [](){thread.Quit();};
		}


		Detail::RegisterExecution();
	}
	
	static void Check()
	{
		if (!thread.IsBusy())
		{
			GlobalEvents::frameBegin.Unregister(Detail::frameBeginRegistration);
			Detail::frameBeginRegistration = InvalidIndex;
			Callback::Submit();
			String err = thread.GetErrors();
			if (err.IsNotEmpty())
			{
				#ifdef HAS_WINDOW
					HUD::Dialog::ShowMessage("Something seems to have gone wrong execting the Lua AI script:\n" + err);
				#endif
			}

			if (!thread.hasSuccessfullyExecuted)
				Detail::UnregisterExecution();

			onExecutionFinished();

			Detail::completedThisPhase = true;

			PlayerControlBegin::CheckAutoNextRound();
		}
	}

	void	Start()
	{
		if (IsRunning())
			return;
		thread.api.FlushAllObjects();
		thread.api.AddAllFactionsAndAlliances();
		thread.api.AddAllEntities();
		thread.api.AddMyMarkers();
		thread.api.AddObjectAndPathfindingFunctions();
		thread.hasSuccessfullyExecuted = false;

		if (Detail::frameBeginRegistration == InvalidIndex)
			Detail::frameBeginRegistration = GlobalEvents::frameBegin.Register(Check);

		thread.api.onDeployedCallbacks.Clear();
		Detail::awaitEntity.Clear();

		if (Detail::reloadScript)
		{
			thread.Enqueue("Map Terrain",[](Thread&thread)
			{
				auto&api = thread.api;
				api.AddTerrain();
			});


			thread.Enqueue("Load Script",[](Thread&thread)
			{
				auto&api = thread.api;

				Detail::reloadScript = false;

				api.program->ExecuteFile(Detail::scriptPath);
				thread.exportState = api.program->RequireGlobal("ExportState");
				thread.init = api.program->RequireGlobal("Init");
				thread.execute = api.program->RequireGlobal("Execute");
			});

			thread.Enqueue("Script Init",[](Thread&thread)
			{
				auto&api = thread.api;

				if (!thread.freshState && thread.exportedState.IsNotEmpty())
				{
					auto state = api.program->DeserializeToReference(
						MemReadStream(
							thread.exportedState.GetPointer(),
							(serial_size_t)thread.exportedState.GetContentSize()
						)
					);
					thread.init(state);
				}
				else
					thread.init(Lua::nil);
			});
		}

		thread.Enqueue("Run",[](Thread&thread)
		{
			auto&api = thread.api;
			thread.execute();
			thread.hasSuccessfullyExecuted = true;
		});


		//thread.Start();
	}



	void			SignalAppearance(const Objects::PWorldEntity&e)
	{
		if (!IsLoaded())
			return;
		TUniqueID reqID;
		if (Detail::awaitEntity.QueryAndUnset(e->id,reqID))
		{
			LogEvent(EventClass::Preset::scriptLog,e.get(), "Identified callback candidate entity: "+e->GetName());
			ASSERT__(e->isOwnedByMe);
			thread.Enqueue("Invoke callback function "+ToString(reqID),[e,reqID](Thread&thread)
			{
				thread.api.SignalDeployment(reqID,e);
			});
		}
	}

	void			SignalChoiceResponse(const TUniqueID&requestID, const TUniqueID&entityID)
	{
		if (!IsLoaded() || requestID.IsEmpty())
			return;
		if (entityID.IsEmpty())
		{
			LogEvent(EventClass::Preset::scriptLog,"Recorded callback result (failure) of "+ToString(requestID));
			thread.Enqueue("Remove callback function "+ToString(requestID),[requestID](Thread&thread)
			{
				thread.api.onDeployedCallbacks.Unset(requestID);
			});
		}
		else
		{
			Objects::PWorldEntity e = Objects::GetEntity(entityID);
			if (!e)
			{
				LogEvent(EventClass::Preset::scriptLog,"Recorded callback result (entity "+ToString(entityID)+") of "+ToString(requestID));
				Detail::awaitEntity.Set(entityID,requestID);
			}
			else
			{
				LogEvent(EventClass::Preset::scriptLog,e.get(), "Identified callback candidate entity: "+e->GetName());
				ASSERT__(e->isOwnedByMe);
				thread.Enqueue("Invoke callback function "+ToString(requestID),[e,requestID](Thread&thread)
				{
					thread.api.SignalDeployment(requestID,e);
				});
			}
		}

	}


	bool	IsRunning()
	{
		return thread.IsBusy();
	}


	bool			IsLoaded()
	{
		return Detail::scriptPath.IsNotEmpty();
	}

	D::PathString	GetScriptPath()
	{
		return Detail::scriptPath;
	}


	bool			HasSuccessfullyExecuted()
	{
		return thread.hasSuccessfullyExecuted;
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
			<< "]]" << nl
			<< nl
			<< "--[[" << nl
			<< "@brief State export" << nl << nl
			<< "This function is expected to export any persistent AI state." << nl
			<< "Whatever you return will be fed to Init() when resuming interrupted execution." << nl
			<< "Anything you keep in the local Lua state but don't export here will be lost during interruptions" << nl
			<< "(e.g. save game loading, client crash, AI script reloading during debug, etc)." << nl
			<< "@return Exported state. May be any regular Lua type, including arbitrarily nested tables, or"<<nl
			<< "plain nil." << nl
			<< "]]" << nl
			<< "function ExportState()" << nl
			<< "\treturn nil;" << nl
			<< "end;" << nl
			<< nl
			<< "--[[" << nl
			<< "@brief Initialization" << nl << nl
			<< "Invoked ONCE before the first execution. If a previously exported state exists, it is passed" <<nl
			<< "as parameter." << nl
			<< "@param state Some previously exported state, or nil." << nl
			<< "]]" << nl
			<< "function Init(state)" << nl
			<< "end;" << nl << nl
			<< nl
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

	FileSystem::Folder	GetScriptFolder()
	{
		return RuntimeConfig::myConfig.Lookup("AIScripts");
	}

	bool			DoneThisRound()
	{
		if (!IsLoaded())
			return true;
		if (IsRunning())
			return false;
		return Detail::completedThisPhase && thread.CallbacksDone();
	}

	bool			SuccessfullyExecutedThisRound()
	{
		return Detail::completedThisPhase && thread.hasSuccessfullyExecuted;
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

			FileSystem::Folder scripts = GetScriptFolder();
			if (!scripts.IsValidLocation())
				FileSystem::CreateDirectory(scripts.GetLocation());

			FileSystem::File file;
			scripts.Rewind();
			WWindow w = window;
			while (scripts.NextFile(".lua",file))
			{
				PButton button {new Button{FileSystem::PathToString(file.GetInnerName()),[file,w]()
				{
					PWindow window = w.lock();
					WindowStack::Hide(window);
					Load(file.GetLocation(),true);

					HUD::HexGrid::RefreshButtonState();
				}}};

				window->AnchorBelowPrevious(CLOCATION,button);
			};

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
				FileSystem::Folder scripts = GetScriptFolder();
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


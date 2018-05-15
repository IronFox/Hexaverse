#include "scriptedAI.h"
#include <lua/luaGameAPI.h>
#include <objects/faction.h>
#include "clientEvents.h"
#include "objectNetworkHelper.h"
#include "network.h"
#include "visual/hud/windowStack.h"
#include "visual/hud/hexGrid.h"

namespace ScriptedAI
{
	using namespace DeltaWorks;
	bool			visualDebugActive = false;
	index_t			frameBeginRegistration = InvalidIndex;
	Observable<>	onExecutionFinished;
	Array<BYTE>		exportedState;
	Lua::Function<Lua::Reference()> exportFunction;
	Lua::Function<void(Lua::Reference)> importFunction;
	Lua::Function<void()> execute;

	class Thread : public Sys::ThreadObject
	{
	public:
		Lua::API		api;
		volatile bool	isRunning = false;
		virtual void ThreadMain() override
		{
			execute();

		}
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

		void	MarkerCreate( const TUniqueID&mID, Objects::MarkerType, const Objects::PWorldEntity&e, const Grid::LCellIdI&loc )
		{
			Protocol::Marker&marker = createdMarkers.Append();
			marker.id = mID;
			marker.createdInRound = RuntimeConfig::roundNumber;
			static_assert(Objects::MarkerType::EntityDeath==0, "MarkerTyp expectation violation");
			static_assert(Objects::MarkerType::HostileContact==1, "MarkerTyp expectation violation");
			static_assert(Objects::MarkerType::Visited == 2, "MarkerTyp expectation violation");
			if (e)
			{
				if (e->isOwnedByMe)
					ObjectHelper::FillMarker(marker, e, marker.data.Set0());
				else
					ObjectHelper::FillMarker(marker, e, marker.data.Set1());
			}
			else
				marker.data.Set2();
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
	}

	void	Load(const PathString&scriptPath, bool freshState)
	{
		ASSERT__(!IsRunning());
		
		if (thread.api.IsCreated())
		{
			Lua::Reference ref;
			if (exportFunction.IsSet())
				ref = exportFunction();
			if (!ref)
				exportedState.Clear();
			else
			{
				ByteStreamBuffer stream;
				ref.Serialize(stream);
				exportedState = stream.GetWrittenData();
			}
			exportFunction.ClearReference();
			importFunction.ClearReference();
			execute.ClearReference();
			thread.api.Destroy();
		}

		auto&api = thread.api;

		api.onAttackTargets = Callback::AttackTargets;
		api.onDetailedDirectTo = Callback::DetailedDirectTo;
		api.onDropAllTasks = Callback::DropAllTasks;
		api.onEnableWorkingStructure = Callback::EnableWorkingStructure;
		api.onMarkerCreate = Callback::MarkerCreate;
		api.onMarkerDestroy = Callback::MarkerDestroy;
		api.onMarkerTouch = Callback::MarkerTouch;

		api.Create("ScriptedAI");
		api.AddGrid();
		api.program->ExecuteFile(scriptPath);
		exportFunction = api.program->RequireGlobal("ExportState");
		importFunction = api.program->RequireGlobal("ImportState");
		execute = api.program->RequireGlobal("Execute");
		if (!freshState && exportedState.IsNotEmpty())
		{
			auto state = api.program->DeserializeToReference(MemReadStream(exportedState.GetPointer(), exportedState.GetContentSize()));
			importFunction(state);
		}
	}
	
	static void Check()
	{
		if (!thread.IsActive())
		{
			thread.isRunning = false;
			GlobalEvents::frameBegin.Unregister(frameBeginRegistration);
			frameBeginRegistration = InvalidIndex;
			Callback::Submit();
			onExecutionFinished();
			#ifdef CLIENT
				HUD::WindowStack::SignalSituationChange();
				HUD::HexGrid::RefreshButtonState();
			#endif
		}
	}

	void	Start()
	{
		if (IsRunning())
			return;
		thread.api.FlushAllObjects();
		thread.api.AddAllEntities();
		thread.api.AddAllAlliances();
		thread.api.AddMyMarkers();
		thread.api.AddObjectAndPathfindingFunctions();
		thread.isRunning = true;
		thread.Start();
		if (frameBeginRegistration == InvalidIndex)
			frameBeginRegistration = GlobalEvents::frameBegin.Register(Check);
	}

	bool	IsRunning()
	{
		return thread.isRunning;
	}


	void	CreateTemplateAt(const D::PathString&path)
	{
		StringFile f;
		f.Create(path);

		f << "--[[" << nl
			<< "Generated Lua AI file. Fill out the details to define what the AI is supposed to do." << nl
			<< "You have access to a set of API functions to examine units and instruct them," << nl
			<< "documented in " <<
			FileSystem::PathToString(FileSystem::Folder().FindFile("../Lua/General Lua API Documentation.txt")->GetLocation()) << nl
			<< "]]" << nl
			<< nl
			<< "--[[" << nl
			<< "@brief State export" << nl << nl
			<< "This function is expected to export any persistent AI state." << nl
			<< "Whatever you return (unless nil) will be fed to ImportState() when resuming interrupted execution." << nl
			<< "Anything you keep in the local Lua state but don't export here will be lost during discontinuities" << nl
			<< "(e.g. save game loading, client crash, AI script reloading during debug)." << nl
			<< "@return Exported state. May be any regular Lua type, including recursive tables, or nil." << nl
			<< "]]" << nl
			<< "function ExportState()" << nl
			<< "\treturn nil;" << nl
			<< "end;" << nl
			<< nl
			<< "--[[" << nl
			<< "@brief State import" << nl << nl
			<< "This function is expected to import any previously exported state. Whatever you are getting," << nl
			<< "SOME ExportState() function has produced it. Usually your own, but who knows..." << nl
			<< "Not invoked if there was no state." << nl
			<< "@param state Some previously exported state." << nl
			<< "]]" << nl
			<< "function ImportState(state)" << nl
			<< "end;" << nl << nl
			<< nl
			<< "--[[" << nl
			<< "@brief AI execution entry point" << nl << nl
			<< "This function is executed whenever the AI is supposed to do something." << nl
			<< "During production runs, this function will be called exactly once at the beginning of a round." << nl
			<< "Executed asynchronously. Take as much time as you need, though bad things _will_ happen if you" << nl
			<< "take longer than the server permits any player. Try to not produce endless loops as those will" << nl
			<< "force the client to restart." << nl
			<< "]]" << nl
			<< "function Execute()" << nl
			<< "\t-- Your stuff here" << nl
			<< "\t-- Check the API documentation: " <<
			FileSystem::PathToString(FileSystem::Folder().FindFile("../Lua/General Lua API Documentation.txt")->GetLocation()) << nl
			<< "end;" << nl << nl
			;

	}

}


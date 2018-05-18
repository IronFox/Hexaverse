#include "scriptedAI.h"
#include <lua/luaGameAPI.h>
#include <objects/faction.h>
#include "clientEvents.h"
#include "objectNetworkHelper.h"
#include "network.h"
#include "visual/hud/windowStack.h"
#include "visual/hud/hexGrid.h"
#include "visual/hud/dialog.h"
#include <io/string_file.h>

namespace ScriptedAI
{
	using namespace DeltaWorks;
	bool			visualDebugActive = false;
	Observable<>	onExecutionFinished;
	
	namespace Detail
	{
		index_t			frameBeginRegistration = InvalidIndex;
		Array<BYTE>		exportedState;
		PathString		scriptPath;
		bool			freshState = false, reloadScript = true;
	}

	class Thread : public Sys::ThreadObject
	{
	public:
		Lua::API		api;
		Lua::Function<Lua::Reference()> exportState;
		Lua::Function<void(Lua::Reference)> init;
		Lua::Function<void()> execute;

		String			error;

		volatile bool	isRunning = false;
		virtual void ThreadMain() override
		{
			error = "";
			try
			{
				if (Detail::reloadScript)
				{
					Detail::reloadScript = false;
					api.program->ExecuteFile(Detail::scriptPath);
					exportState = api.program->RequireGlobal("ExportState");
					init = api.program->RequireGlobal("Init");
					execute = api.program->RequireGlobal("Execute");

					if (!Detail::freshState && Detail::exportedState.IsNotEmpty())
					{
						auto state = api.program->DeserializeToReference(
							MemReadStream(
								Detail::exportedState.GetPointer(),
								(serial_size_t)Detail::exportedState.GetContentSize()
							)
						);
						init(state);
					}
					else
						init(Lua::nil);
				execute();
			}
			catch (const Lua::Exception&ex)
			{
				error = ex.GetRawMessage();
			}
			catch (const std::exception&ex)
			{
				error = ex.what();
			}
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
	}

	void	Load(const PathString&scriptPath, bool freshState)
	{
		ASSERT__(!IsRunning());
		
		if (thread.api.IsCreated())
		{
			Lua::Reference ref;
			if (thread.exportState.IsSet())
				ref = thread.exportState();
			if (!ref)
				Detail::exportedState.Clear();
			else
			{
				ByteStreamBuffer stream;
				ref.Serialize(stream);
				Detail::exportedState = stream.GetWrittenData();
			}
			thread.exportState.ClearReference();
			thread.init.ClearReference();
			thread.execute.ClearReference();
			thread.api.Destroy();
		}
		
		Detail::reloadScript = true;

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
		api.AddTerrain();
		Detail::scriptPath = scriptPath;
		Detail::freshState = freshState;

	}
	
	static void Check()
	{
		if (!thread.IsActive())
		{
			thread.isRunning = false;
			GlobalEvents::frameBegin.Unregister(Detail::frameBeginRegistration);
			Detail::frameBeginRegistration = InvalidIndex;
			Callback::Submit();
			if (thread.error.IsNotEmpty())
			{
				LogEvent(EventClass::Preset::scriptError, thread.error);
				#ifdef HAS_WINDOW
					HUD::Dialog::ShowMessage("Something seems to have gone wrong execting the Lua AI script:\n" + thread.error);
				#endif
			}

			onExecutionFinished();
			#ifdef HAS_WINDOW
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
		thread.api.AddAllFactionsAndAlliances();
		thread.api.AddAllEntities();
		thread.api.AddMyMarkers();
		thread.api.AddObjectAndPathfindingFunctions();
		thread.isRunning = true;
		thread.Start();
		if (Detail::frameBeginRegistration == InvalidIndex)
			Detail::frameBeginRegistration = GlobalEvents::frameBegin.Register(Check);
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
			<< "Whatever you return will be fed to Init() when resuming interrupted execution." << nl
			<< "Anything you keep in the local Lua state but don't export here will be lost during discontinuities" << nl
			<< "(e.g. save game loading, client crash, AI script reloading during debug, etc)." << nl
			<< "@return Exported state. May be any regular Lua type, including arbitrarily nested tables, or plain nil." << nl
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
			<< "Executed asynchronously. Take as much time as you need, though bad things _will_ happen if you" << nl
			<< "take longer than the server permits any player. Try to not produce endless loops as those will" << nl
			<< "force the user to restart the client." << nl
			<< "]]" << nl
			<< "function Execute()" << nl
			<< "\t-- Your stuff here" << nl
			<< "\t-- Check the API documentation: " <<
			FileSystem::PathToString(FileSystem::Folder().FindFile("../Lua/General Lua API Documentation.txt")->GetLocation()) << nl
			<< "end;" << nl << nl
			;

	}

}


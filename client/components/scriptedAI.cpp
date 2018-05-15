#include "scriptedAI.h"
#include <lua/luaGameAPI.h>
#include <objects/faction.h>
#include "clientEvents.h"
#include "objectNetworkHelper.h"
#include "network.h"

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
			Lua::Reference ref = exportFunction();
			ByteStreamBuffer stream;
			ref.Serialize(stream);
			exportedState = stream.GetWrittenData();
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
}


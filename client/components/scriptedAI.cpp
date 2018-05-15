#include "scriptedAI.h"
#include <lua/luaGameAPI.h>
#include <objects/faction.h>
#include "clientEvents.h"

namespace ScriptedAI
{
	using namespace DeltaWorks;
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

		thread.api.Create("ScriptedAI");
		thread.api.AddGrid();
		thread.api.program->ExecuteFile(scriptPath);
		exportFunction = thread.api.program->RequireGlobal("ExportState");
		importFunction = thread.api.program->RequireGlobal("ImportState");
		execute = thread.api.program->RequireGlobal("Execute");
		if (!freshState && exportedState.IsNotEmpty())
		{
			auto state = thread.api.program->DeserializeToReference(MemReadStream(exportedState.GetPointer(), exportedState.GetContentSize()));
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


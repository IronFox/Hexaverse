#pragma once

#include <io/file_system.h>
#include <structure/observable.h>

namespace ScriptedAI
{
	namespace D = DeltaWorks;

	/**
	AI script completion event.
	Triggered during GlobalEvents::frameBegin once completion has been detected
	*/
	extern D::Observable<>	onExecutionFinished;

	/**
	If set, client will show special AI controls
	*/
	extern bool	visualDebugActive;

	/**
	(Re)loads a Lua AI script from the given path to be used during AI computation.
	May throw exceptions if loading fails due to IO or syntactical reasons.
	AI execution must not be currently active.
	@param scriptPath File path of the Lua script to load
	@param freshState Set true to disregard any previously exported AI script state.
		Otherwise if such was exported, the ImportState() function is called.
	*/
	void	Load(const D::PathString&scriptPath, bool freshState);

	/**
	Checks and potentially reloads the currently loaded file.
	Demand to reload is determined by modification timestamp.
	A file must be loaded.
	@param force If set, ignore file modification timestamp and reload
	@param freshState If set, ignore any previously exported state and start fresh. Effective only if the file actually is reloaded
	*/
	void	Reload(bool force, bool freshState);

	/**
	Starts asynchronous execution of the loaded AI script.
	Fails silently if already active.
	Hooks itself in GlobalEvents::frameBegin to re-check completion, and possibly invoke
	cleanup.
	*/
	void	Start();

	/**
	Checks whether or not AI is still running or has completed.
	@return True if the AI is still busy, false if it has terminated.
	*/
	bool	IsRunning();

	bool			IsLoaded();
	D::PathString	GetScriptPath();
	bool			HasSuccessfullyExecuted();
	D::PathString	GetDocumentationPath();


	void			CreateTemplateAt(const D::PathString&);
}


#pragma once

#include <functional>
#include <global_string.h>

#ifdef HAS_WINDOW
	#include <engine/display.h>
	#include "../ExtRenderer/DirectX11.h"
	#include <engine/textout.h>
#endif


/**
Queued loading handler
*/
namespace Loading
{
	using namespace DeltaWorks;

	/**
	Initializes all local variables to begin queued loading
	*/
	void		Begin();
	/**
	@brief Registers a new loading stage.
	
	The specified function may be executed right away or queued for latter execution.
	The Loading module assures that all registered stages are executed in order of registration,
	and that all loading functions have been executed once Finish() has returned.
	
	@param name Name of the loading stage
	@param loader Function that executes the loading
	*/
	void		Stage(const String&name, const std::function<void()>&loader);
	
	/**
	Executes any remaining queued stages, and returns the display window to its previous configuration (if any)
	*/
	void		Finish();
	
	
	#ifdef HAS_WINDOW

		typedef Engine::Display<Engine::DirectX11>	Display;

		/**
		Resizes/relocates the loading screen window as needed, loads, and displays a splash screen.
		Any changes are reverted once Finish() is called.
		Text output cannot start until SetupText() has also been called.
		Must be called only once.
		*/
		void	SetupDisplay(Display&);
	
		/**
		Assigns a text-renderer to print text with.
		Text size will be changed as appropritate.
		Any changes are reverted once Finish() is called.
		Visual output cannot start until SetupDisplay() has also been called.
		Must be called only once.
		*/
		void	SetupText(Engine::VirtualTextout&);
	#endif
	
};
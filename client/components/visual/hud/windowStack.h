#pragma once


#ifdef HAS_WINDOW

#include "common.h"
#include "guiComponents.h"

namespace HUD
{
	

	namespace WindowStack
	{
	
		/**
		Checks that the top-most window is active (if any), and all others are inactive (if any).
		Will halt execution in Debug mode, and fix the issue in release mode.
		*/
		void				VerifyIntegrity(const DeltaWorks::TCodeLocation&);

		void				SignalScreenResize();


		void				Create();


		index_t				FindWindowIndex(const Component::Window*window);

		/**
		Makes sure the specified window is visible. Raises the window if it's not top-most
		*/
		void				Show(const Component::PWindow&window);
		/**
		Makes sure the specified window is visible. Lowers it to the bottom of the window stack if it's not there already
		@return true if the window has been added, false if it's empty or already visible (though order changes may still be applied)
		*/
		bool				ShowBottom(const Component::PWindow&window);
		bool				Hide(const Component::PWindow&window);
		bool				Hide(Component::Window*window);
		bool				IsVisible(const Component::PWindow&window);
		Component::PWindow	GetTopMostWindow();
		/**
		Signals that the global situation has changed and active modal dialogs need to be re-evalutated.
		This may be invoked by a server state change or removal of some entity
		*/
		void				SignalSituationChange();

		/**
		Forcefully sends mouse motion signal to the topmost windows until a modal window was processed
		*/
		void				OverrideSignalMouseMotion();


		Component::WheelRs	OnMouseWheel(short delta);
		bool				OnMouseDown();
		void				HandleMouseUp();
		/**
		Checks if any modal dialogs are open at this time
		*/
		bool				IsModal();
		/**
		Closes all modal dialogs.
		Dialogs declared as non implicitly closable are unaffected
		@return true, if any modal dialogs were closed
		*/
		bool				CloseAllModal();
		/**
		Closes the top-most modal dialog.
		If the top-most dialog is declared as non implicitly closable, then the function fails.
		@return true, if any modal dialog was closed
		*/
		bool				CloseTopModal();
		/**
		If the specified window is shown open, it is closed, and all closable windows beneath it
		*/
		bool				CloseAllModalBeneath(const Component::PWindow&);
		/**
		Renders visible non-modal windows/dialogs up to the first modal window
		*/
		void				RenderNonModal();
		/**
		Renders visible modal windows/dialogs following the first encountered modal window/dialog, bottom up
		*/
		void				RenderModal();
	}


}

#endif

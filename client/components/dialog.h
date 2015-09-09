#ifdef HAS_WINDOW

#ifndef dialogH
#define dialogH

#include <general/enumeration.h>
#include <functional>
#include <global_string.h>
#include "guiElements.h"

namespace Dialog
{
	CONSTRUCT_ENUMERATION3(CheckResult,
		OkayContinue,		//operation applicable. User can continue any time they wish to
		OkayNotRightNow,	//often returned when the server is in some non-command mode
		Abort				//sitatuation changed. Operation no longer applicable
	);

	typedef std::function<void()>	POnConfirm;
	typedef std::function<CheckResult()>	PCheckState;
	typedef std::function<bool(const String&)>	PCheckInput;
	typedef std::function<void(const String&)>	PFetchInput;


	struct Faction
	{
		String	name;
		bool	online, requestedNewRound;
	};

	/**
	Signals that the global situation has changed and active modal dialogs need to be re-evalutated.
	This may be invoked by a server state change or removal of some entity
	*/
	void	SignalSituationChange();

	/**
	Creates a new modal dialog on top of any other active modal dialogs
	*/
	void	Confirm(const String&query, const POnConfirm&onConfirm, const PCheckState&check);

	/**
	Creates a new modal dialog on top of any other active modal dialogs that simply shows a message
	*/
	void	ShowMessage(const String&msg);


	/**
	Creates a new modal dialog on top of any other active modal dialogs that attempts to read a string from the user
	*/
	void	Query(const String&query, const String&initial, const String&allowChars, const PCheckInput&, const PFetchInput&);

	/**
	Queues a tutorial message. Allows hiding all following tutorials
	*/
	void	EnqueueTutorial(const String&key, const Array<String>&text, const String&imageName = "", const Rect<>&screenReference = Rect<>::Invalid);
	void	RemoveTutorial(const String&key);


	void	ShowFactions(const Buffer0<Faction>&factions);

	bool	ShowingFactions();


	void	ShowNextTutorial();

	bool	HasMoreTutorials();


	/**
	Checks if any modal dialogs are open at this time
	*/
	bool	IsModal();

	/**
	Closes all modal dialogs
	*/
	void	CloseAllModal();

	/**
	Confirms the current top-most modal dialog, if possible and existing
	@return true if a modal dialog exists, false otherwise. Response does not reveal whether or not the dialog was closed
	*/
	bool	ConfirmCurrent(bool bySpace);

	void	Render();

	bool	HandleClick();


	void	Create();
	void	Destroy();





}




#endif
#endif

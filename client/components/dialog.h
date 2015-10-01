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


	class Dialog : public std::enable_shared_from_this<Dialog>
	{
	public:
		//CONSTRUCT_ENUMERATION5(Type, Confirmation, Message, TutorialMessage, StringQuery,FactionList);

		//Type					type;

		Buffer0<GUIElements::PComponent>	components;
		GUIElements::PButton	confirmButton;
		GUIElements::PInput		userInput;
		GUIElements::PTextField	textField;
		GUIElements::PImage		image;

		String					key;


		PCheckState			check;
		POnConfirm			onConfirm;
		PCheckInput			checkInput;
		PFetchInput			fetchInput;
		bool				isTutorial = false;

		Rect<>				totalSize;

		std::function<void(Dialog&)>	reLayout;

		bool				isActive = false;


		void		Activate()
		{
			foreach (components,c)
				(*c)->Activate();
			Refresh();
			isActive = true;
		}
		void		Deactivate()
		{
			foreach (components,c)
				(*c)->Deactivate();
			isActive = false;
		}

		void		Refresh();


		bool		SignalChange()
		{
			if (check)
			{
				CheckResult rs = check();
				if (rs == CheckResult::Abort)
					return false;
			}
			foreach (components,c)
				(*c)->Check();
			return true;
		}


		void		Render()
		{
			bool anyChanged = false;
			foreach (components,c)
				if ((*c)->UpdateIfNecessary())
					anyChanged = true;
			if (anyChanged)
				reLayout(*this);

			GUIElements::Darken(Rect<>(0,0,1,1),0.7f);
			GUIElements::Darken(totalSize);

			foreach (components,c)
				(*c)->Render();
		}

		void		HandleClick();
		void		Confirm(bool bySpace);

		void		DumpMe();
	};

	typedef std::shared_ptr<Dialog>	PDialog;
	typedef std::weak_ptr<Dialog>	WDialog;


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
	WDialog	Confirm(const String&query, const POnConfirm&onConfirm, const PCheckState&check);

	/**
	Creates a new modal dialog that has no buttons and will remain until the check fails
	*/
	WDialog	PermanentMessage(const String&message, const PCheckState&check);

	/**
	Creates a new modal dialog on top of any other active modal dialogs that simply shows a message
	*/
	WDialog	ShowMessage(const String&msg);


	/**
	Creates a new modal dialog on top of any other active modal dialogs that attempts to read a string from the user
	*/
	WDialog	Query(const String&query, const String&initial, const String&allowChars, const PCheckInput&, const PFetchInput&);

	/**
	Queues a tutorial message. Allows hiding all following tutorials
	*/
	WDialog	EnqueueTutorial(const String&key, const Array<String>&text, const String&imageName = "", const Rect<>&screenReference = Rect<>::Invalid);
	bool	RemoveTutorial(const String&key);
	bool	RemoveTutorial(const PDialog&dialog);

	bool	RemoveDialog(const PDialog&dialog);


	WDialog	ShowFactions(const Buffer0<Faction>&factions);

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

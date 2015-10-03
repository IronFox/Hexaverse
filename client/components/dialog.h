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


	class Dialog : public GUIElements::Window
	{
		typedef GUIElements::Window	Super;
	public:
		//CONSTRUCT_ENUMERATION5(Type, Confirmation, Message, TutorialMessage, StringQuery,FactionList);

		//Type					type;

		GUIElements::PButton	confirmButton;
		GUIElements::PInput		userInput;
		GUIElements::PLabel		textField;
		GUIElements::PImage		image;

		String					key;


		PCheckState			check;
		POnConfirm			onConfirm;
		PCheckInput			checkInput;
		PFetchInput			fetchInput;
		bool				isTutorial = false;

		Rect<>				totalSize;

		//std::function<void(Dialog&)>	reLayout;

		bool				isActive = false;


		void		Activate()
		{
			Super::Activate();
			Super::DetectEnabledState();
			isActive = true;
		}
		void		Deactivate()
		{
			Super::Deactivate();
			isActive = false;
		}



		bool		SignalChange()
		{
			if (check)
			{
				CheckResult rs = check();
				if (rs == CheckResult::Abort)
					return false;
			}
			Super::DetectEnabledState();
			return true;
		}

		void		ReLayout()
		{
			Super::FullAutoLayout();
		}


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
	
	bool	Hide(const GUIElements::PWindow&window);
	bool	IsVisible(const GUIElements::PWindow&window);
	void	Show(const GUIElements::PWindow&window);


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
	@return true, if any modal dialogs were closed
	*/
	bool	CloseAllModal();

	/**
	Confirms the current top-most modal dialog, if possible and existing
	@return true if a modal dialog exists, false otherwise. Response does not reveal whether or not the dialog was closed
	*/
	bool	ConfirmCurrent(bool bySpace);

	void	Render();

	//bool	HandleClick();
	bool	HandleMouseDown();
	void	HandleMouseUp();


	void	Create();
	void	Destroy();





}




#endif
#endif

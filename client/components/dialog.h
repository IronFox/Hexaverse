#ifdef HAS_WINDOW

#ifndef dialogH
#define dialogH

#include <general/enumeration.h>
#include <functional>
#include <global_string.h>
#include "guiElements.h"
#include "../../shared/cast.h"

namespace Dialog
{
	CONSTRUCT_ENUMERATION3(CheckResult,
		OkayContinue,		//operation applicable. User can continue any time they wish to
		OkayNotRightNow,	//often returned when the server is in some non-command mode
		Abort				//sitatuation changed. Operation no longer applicable
	);

	typedef std::function<void()>	POnConfirm;
	typedef std::function<void(bool)>	PBoolOnConfirm;
	typedef std::function<CheckResult()>	PCheckState;
	typedef std::function<bool(const String&)>	PCheckInput;
	typedef std::function<void(const String&)>	PFetchInput;
	typedef std::function<void(const char*)>	POnPassword;






	class Dialog : public GUIElements::Window
	{
		typedef GUIElements::Window	Super;
	public:
		//CONSTRUCT_ENUMERATION5(Type, Confirmation, Message, TutorialMessage, StringQuery,FactionList);

		//Type					type;

		GUIElements::PButton	confirmButton,noButton,helpButton;
		GUIElements::PInput		userInput;
		GUIElements::PPasswordInput	passwordInput;
		GUIElements::PLabel		textField;
		GUIElements::PImage		image;

		String					key;


		PCheckState			check;
		POnConfirm			onConfirm,
							onNo;
		PCheckInput			checkInput;
		PFetchInput			fetchInput;
		POnPassword			onPassword;
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


		/**
		@return true, if the dialog will close (if conditions are met), false if it is not designed to be confirmed (lacks string-handler function and confirm button)
		*/
		bool		CanBeConfirmed(bool bySpace) const;
		/**
		Issues a confirmation event on the local dialog.
		The state of the dialog is checked before handler functions may be called, and the dialog removed automatically.
		*/
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

	WDialog	AskYesNoHelp(const String&query, const String&help, const PBoolOnConfirm&onAnswer, const PCheckState&check);

	WDialog	QueryPassword(const String&query, const POnPassword&onPassword, const PCheckState&check);
	/**
	Creates a new modal dialog that has no buttons and will remain until the check fails
	*/
	WDialog	PermanentMessage(const String&message, const PCheckState&check);

	/**
	Creates a new modal dialog on top of any other active modal dialogs that simply shows a message
	*/
	WDialog	ShowMessage(const String&msg);
	WDialog	ShowMessage(const String&msg, const PCheckState&check);


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

	/**
	Makes sure the specified window is visible. Raises the window if it's not top-most
	*/
	void	Show(const GUIElements::PWindow&window);
	/**
	Makes sure the specified window is visible. Lowers it to the bottom of the window stack if it's not there already
	*/
	void	ShowBottom(const GUIElements::PWindow&window);


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
	@return true if a dialog is top-most, and it can be confirmed, false otherwise. Response does not reveal whether or not the dialog was closed, but whether or not the event should be propagated somewhere else
	*/
	bool	ConfirmCurrent(bool bySpace);

	/**
	Renders all visible windows/dialogs
	*/
	void	Render();

	/**
	Checks that the top-most window is active (if any), and all others are inactive (if any).
	Will halt execution in Debug mode, and fix the issue in release mode.
	*/
	void	VerifyIntegrity(const TCodeLocation&);



	//bool	HandleClick();
	bool	HandleMouseDown();
	void	HandleMouseUp();


	void	CheckServersAreShown(bool doShow);


	void	Create();
	void	Destroy();





	class Menu : public GUIElements::Window
	{
		typedef GUIElements::Window	Super;
	public:
		/**/				Menu(float initialAbsoulteWidth, const String&title)
		{
			Super::SetCenteredWidth(initialAbsoulteWidth,GUIElements::TAnchor::RelativeToScreen);
			Super::fillBackground = false;

			if (title.IsNotEmpty())
			{
				GUIElements::PLabel label(new GUIElements::Label());
				label->SetText(title);
				label->extraSpacing = 0.1f;
				Super::AnchorBelowPrevious(label,0.015f,0.f);
			}
		}


		GUIElements::PButton				AddCommand(const String&name, const std::function<void()>&execute, const std::function<bool()>&check = std::function<bool()>(), bool autoHide=true)
		{
			GUIElements::PButton button(new GUIElements::Button(name,
				[execute,this,autoHide]()
				{
					execute();
					if (autoHide)
						Hide(To<GUIElements::Window>::Cast(this->shared_from_this()));
				},
				check
			));
			Super::AnchorBelowPrevious(button,0.f,Super::components.IsEmpty() ? 0.f : 0.015f);
			return button;
		}


	};
	typedef std::shared_ptr<Menu>	PMenu;

	


	extern PMenu				loginMenu;
	extern GUIElements::PWindow	serverChoice;


}




#endif
#endif

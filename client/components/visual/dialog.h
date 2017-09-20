#ifdef HAS_WINDOW

#ifndef dialogH
#define dialogH

#include <general/enumeration.h>
#include <functional>
#include <global_string.h>
#include "guiElements.h"
#include <cast.h>

namespace Dialog
{
	using namespace DeltaWorks;

	CONSTRUCT_ENUMERATION4(CheckResult,
		OkayContinue,		//operation applicable. User can continue any time they wish to
		OkayNotRightNow,	//often returned when the server is in some non-command mode
		Abort,				//situation changed. Operation no longer applicable
		Confirm				//situation changed. Operation has completed otherwise and the dialog should be confirmed
	);

	typedef std::function<void()>	FOnConfirm;
	typedef std::function<void(bool)>	FOnConfirmBool;
	typedef std::function<CheckResult()>	FCheckState;
	typedef std::function<bool(const String&)>	FCheckInput;
	typedef std::function<void(const String&)>	FFetchInput;
	typedef std::function<void(const char*)>	FOnPassword;





	class Dialog : public GUI::Window
	{
		typedef GUI::Window	Super;
		friend class Super;

	protected:
		/**/			Dialog()	{}
	public:
		//CONSTRUCT_ENUMERATION5(Type, Confirmation, Message, TutorialMessage, StringQuery,FactionList);

		//Type					type;

		GUI::PButton	confirmButton,noButton,helpButton,cancelButton;
		GUI::PInput		userInput;
		GUI::PPasswordInput	passwordInput;
		GUI::PLabel		textField;
		GUI::PImage		image;

		String					key;


		FCheckState			check;
		FOnConfirm			onConfirm,
							onNo;
		FCheckInput			checkInput;
		FFetchInput			fetchInput;
		FOnPassword			onPassword;
		bool				isTutorial = false,
							autoNextTutorial = true,
							sendNullPasswordOnAbortByUser = false;

		M::Rect<>				totalSize;

		static std::shared_ptr<Dialog>	Create()
		{
			return Super::CreateDerivative<Dialog>();
		}

		//std::function<void(Dialog&)>	reLayout;

	//	bool				isActive = false;


		//void		SetActive(bool isActive)
		//{
		//	Super::Activate(isActive);
		//	Super::DetectEnabledState();
		//	isActive = true;
		//}
		//void		Deactivate()
		//{
		//	Super::Deactivate();
		//	isActive = false;
		//}



		virtual bool		SignalChange() override
		{
			if (check)
			{
				CheckResult rs = check();
				if (rs == CheckResult::Abort)
					return false;
				if (rs == CheckResult::Confirm && confirmButton)
				{
					onConfirm();
					return false;
				}
			}
			Super::DetectEnabledState();
			return true;
		}

		void		ReLayout()
		{
			Super::FullAutoLayout(CLOCATION);
		}

		virtual	void	OnHide() override
		{
			if (onPassword && sendNullPasswordOnAbortByUser && (!check || check() == CheckResult::OkayContinue))
			{
				onPassword(nullptr);
			}
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
		void*	factionReference;
	};


	/**
	Creates a new modal dialog on top of any other active modal dialogs
	*/
	WDialog	Confirm(const String&query, const FOnConfirm&onConfirm, const FCheckState&check);
	WDialog	Confirm(const String&query, const FOnConfirm&onConfirm);

	WDialog	AskYesNoHelp(const String&query, const String&help, const FOnConfirmBool&onAnswer, const FCheckState&check);

	/**
	Show a new dialog that queries a password from the user
	@param query Message to display, prompting the user to enter some password
	@param onPassword Callback function to call once the user confirmed the input
	@param check Check function to evaluate whether or not the dialog should be preserved
	@param sendNullPasswordOnAbortByUser If set, pressing escape, or clicking the abort button will still call @a onPassword with nullptr as paramter
	*/
	WDialog	QueryPassword(const String&query, const FOnPassword&onPassword, const FCheckState&check, bool sendNullPasswordOnAbortByUser=false);
	/**
	Creates a new modal dialog that has no buttons and will remain until the check fails
	*/
	WDialog	PermanentMessage(const String&message, const FCheckState&check);

	/**
	Creates a new modal dialog on top of any other active modal dialogs that simply shows a message
	*/
	WDialog	ShowMessage(const String&msg);
	WDialog	ShowMessage(const String&msg, const FCheckState&check);


	/**
	Creates a new modal dialog on top of any other active modal dialogs that attempts to read a string from the user
	*/
	WDialog	Query(const String&query, const String&initial, const String&allowChars, const FCheckInput&, const FFetchInput&);
	WDialog	CheckedQuery(const String&query, const String&initial, const String&allowChars, const FCheckState&, const FFetchInput&);

	/**
	Queues a tutorial message. Allows hiding all following tutorials
	*/
	WDialog	EnqueueTutorial(const String&key, const Array<String>&text, const String&imageName = "", bool prioritize=false);
	PDialog	CreateTutorialMessage(const String&key, const ArrayRef<const String>&text, const String&imageName = "");

	bool	RemoveTutorial(const String&key);
	bool	RemoveTutorial(const PDialog&dialog);

	bool	RemoveDialog(const PDialog&dialog);
	



	WDialog	ShowFactions(const Buffer0<Faction>&factions);

	bool	ShowingFactions();


	void	ShowNextTutorial(bool autoNext);

	bool	HasMoreTutorials();




	/**
	Confirms the current top-most modal dialog, if possible and existing
	@return true if a dialog is top-most, and it can be confirmed, false otherwise. Response does not reveal whether or not the dialog was closed, but whether or not the event should be propagated somewhere else
	*/
	bool	ConfirmCurrent(bool bySpace);

	





	void	CheckServersAreShown(bool doShow);


	void	Create();
	void	Destroy();


	class Menu;
	typedef std::shared_ptr<Menu>	PMenu;


	class Menu : public GUI::Window
	{
		typedef GUI::Window	Super;

		String		myTitle;

		friend class Super;
	protected:
		/**/		Menu(){Super::logFullAutoLayout = true;}
	public:
		void		Setup(float initialAbsoluteWidth, const String&title);

		static PMenu		Create()
		{
			return Super::CreateDerivative<Menu>();;
		}


		virtual void				OnScreenSizeOrUIScaleChange() override;

		GUI::PButton				AddCommand(const String&name, const std::function<void()>&execute, const std::function<bool()>&check = std::function<bool()>(), bool autoHide=true);


	};

	


	extern GUI::PWindow	serverChoice;


}




#endif
#endif

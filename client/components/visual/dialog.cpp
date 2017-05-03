#ifdef HAS_WINDOW

#include <global_root.h>
#include "dialog.h"
#include <engine/display.h>
#include "../../ExtRenderer/DirectX11.h"
#include "../../DX11/texture_font.h"
#include <container/buffer.h>
#include "surfaceRenderer.h"
#include <engine/aspect.h>
#include <image/converter/magic.h>
#include <general/random.h>
#include <cast.h>
#include <logEvent.h>
#include "../clientRuntimeConfig.h"

namespace DX11 = Engine::DX11; 

extern Engine::Textout<DX11::TextureFont>	textout;
extern Engine::Display<Engine::DirectX11>	display;

void	OnTutorialsChanged();

Random random;



namespace Dialog
{
	namespace Detail
	{

		Queue<PDialog,Swap>		pendingTutorials;
		PDialog					factionDialog;
		bool		showTutorials = true;

		struct ComponentFlags
		{
			enum flag_t
			{
				ConfirmButton = 0x1,
				CancelButton = 0x2,
				Input = 0x4,
				Image = 0x8,
				NoButton = 0x10,
				HelpButton = 0x20,
				PasswordInput = 0x40,
			};
		};



		void Configure(PDialog dialog, UINT32 componentFlags, const String&confirmWord, bool isTutorial);

		PDialog	NewModalDialog(UINT32 componentFlags, const String&confirmWord, bool isTutorial)
		{
			PDialog dialog(new Dialog());
			Configure(dialog,componentFlags,confirmWord, isTutorial);
			Show(dialog);
			return dialog;
		}

		void Configure(PDialog dialog, UINT32 componentFlags, const String&confirmWord, bool isTutorial)
		{
			dialog->isTutorial = isTutorial;

			dialog->anchorConfig.Left().Set(0.25f);
			dialog->anchorConfig.Right().Set(0.25f);
			
			dialog->fillBackground = false;
			//GUI::PComponent	common(new GUI::Area());

			GUI::PPanel	textPanel(new GUI::Panel());
			GUI::PLabel textField(new GUI::Label());
			textPanel->AddComponent(textField);
			textField->anchorConfig.Left().Set(0.f);
			textField->anchorConfig.Right().Set(0.f);
			textField->anchorConfig.Top().Set(0.f);
			textPanel->anchorConfig = textField->anchorConfig;
			textPanel->autoHeight = true;
			dialog->AddComponent(textPanel);
			
			//height controlled by content

			GUI::PButton confirmButton,cancelButton,noButton,helpButton;
			GUI::PInput	inputField;
			GUI::PPasswordInput passwordInputField;
			GUI::PImage	imageField;
			Buffer0<std::pair<float,GUI::PButton> > buttons;


			if (componentFlags & ComponentFlags::Input)
			{
				inputField.reset(new GUI::Input());
				inputField->anchorConfig.Left().Set(0.f);
				inputField->anchorConfig.Right().Set(0.f);
				inputField->anchorConfig.Top().Set(textField,0.f);//0.025f);
				WDialog wd = dialog;
				inputField->onEnter = [wd]()
				{
					PDialog d = wd.lock();
					if (d && d->CanBeConfirmed(false))
						d->Confirm(false);
				};
				textPanel->AddComponent (inputField);
			}
			if (componentFlags & ComponentFlags::PasswordInput)
			{
				passwordInputField.reset(new GUI::PasswordInput());
				passwordInputField->anchorConfig.Left().Set(0.f);
				passwordInputField->anchorConfig.Right().Set(0.f);
				passwordInputField->anchorConfig.Top().Set(textField,0.f);//0.025f);
				textPanel->AddComponent (passwordInputField);
			}

			if (componentFlags & ComponentFlags::ConfirmButton)
			{
				confirmButton.reset(new GUI::Button(confirmWord));
				confirmButton->anchorConfig.Left().Set(0.f);
				confirmButton->anchorConfig.Right().Set(0.f);
				confirmButton->anchorConfig.Top().Set(textPanel,0.015f,GUI::TAnchor::RelativeToScreen);
				dialog->AddComponent( confirmButton);
				buttons << std::make_pair(1, confirmButton);
			}
			if (componentFlags & ComponentFlags::HelpButton)
			{
				helpButton.reset(new GUI::Button("?"));
				helpButton->anchorConfig.Top().Set(textPanel,0.015f,GUI::TAnchor::RelativeToScreen);
				dialog->AddComponent(helpButton);
				buttons << std::make_pair(0.25f, helpButton);
			}
			if (componentFlags & ComponentFlags::NoButton)
			{
				noButton.reset(new GUI::Button("No"));
				noButton->anchorConfig.Top().Set(textPanel,0.015f,GUI::TAnchor::RelativeToScreen);
				dialog->AddComponent( noButton);
				buttons << std::make_pair(1, noButton);
			}
			if (componentFlags & ComponentFlags::CancelButton)
			{
				cancelButton.reset(new GUI::Button("Cancel"));

				//if (confirmButton)
				//	confirmButton->anchorConfig.Right().Set(0.505f);
				cancelButton->anchorConfig.Left().Set(0.505f);
				cancelButton->anchorConfig.Right().Set(0.f);
				cancelButton->anchorConfig.Top().Set(textPanel,0.015f,GUI::TAnchor::RelativeToScreen);
				dialog->AddComponent(cancelButton);
				buttons << std::make_pair(1, cancelButton);
			}

			{
				float total = 0.f;
				foreach (buttons,b)
					total += b->first;
				float width = 1.f / float(total);
				float at = 0.f;
				
				for (index_t i = 0; i < buttons.Count(); i++)
				{
					buttons[i].second->anchorConfig.Left().Set(at*width + 0.005f*(i != 0));
					at += buttons[i].first;
				}
				at = 0.f;
				for (index_t i = buttons.Count()-1; i < buttons.Count(); i--)
				{
					buttons[i].second->anchorConfig.Right().Set(at*width + 0.005f*(i+1 != buttons.Count()));
					at += buttons[i].first;
				}
			}


			if (componentFlags & ComponentFlags::Image)
			{
				imageField.reset(new GUI::Image());
				

				textField->anchorConfig.Left().Set(0.405f);
				imageField->anchorConfig.Left().Set(0.f);
				imageField->anchorConfig.Right().Set(0.6f);
				imageField->anchorConfig.Top().Set(0.05f);
				if (confirmButton)
				{
					//imageField->autoSetHeight = false;
					//imageField->anchorConfig.Bottom().Set(confirmButton,0.03f,false);
					imageField->autoSetHeight = true;
					imageField->anchorConfig.Top().SetAuto();
					imageField->anchorConfig.Bottom().SetAuto();
				}
				textField->anchorConfig.Bottom().SetAuto();
				textField->anchorConfig.Top().SetAuto();
				//textField->anchorConfig.axis[1].center.Set(imageField,0.f);
	//			imageField->SetLocation(Rect<>(0,1,0.25,0.75f));
				//textField->rect.Set(0.405f,0.25f,0.75f,0.75f);
				//imageField->rect.Set(0.25f,0.25f,0.4f,0.75f);
				textField->alignCenter = false;
				textPanel->AddComponent(imageField);
			}


			
			//{
			//	case ModalDialogType::Confirmation:
			//	{
			//		//confirm, cancel
			//	}
			//	break;
			//	case ModalDialogType::FactionList:
			//	case ModalDialogType::Message:
			//	{
			//		confirmButton.reset(new GUI::Button());
			//	}
			//	break;
			//	case ModalDialogType::StringQuery:
			//	{
			//		confirmButton.reset(new GUI::Button());
			//		cancelButton.reset(new GUI::Button());
			//		inputField.reset(new GUI::Input());
			//	}
			//	break;
			//	case ModalDialogType::TutorialMessage:
			//	{
			//		confirmButton.reset(new GUI::Button());
			//		imageField.reset(new GUI::Image());
			//	}
			//	break;
			//}
			dialog->cancelButton = cancelButton;
			dialog->confirmButton = confirmButton;
			dialog->helpButton = helpButton;
			dialog->noButton = noButton;
			dialog->userInput = inputField;
			dialog->textField = textField;
			dialog->image = imageField;
			dialog->passwordInput = passwordInputField;

			
		/*	dialog->components << textField;
			if (confirmButton)
				dialog->components << confirmButton;
			if (cancelButton)
				dialog->components  << cancelButton;
			if (imageField)
				dialog->components  << imageField;
			if (inputField)
				dialog->components  << inputField;
*/
			if (confirmButton)
			{

//				confirmButton->SetText(confirmWord);
				Dialog*d = dialog.get();
				confirmButton->SetCheck([d, inputField]()->bool
				{
					bool canConfirmNow = !d->check || d->check() == CheckResult::OkayContinue;
					if (d->checkInput && inputField)
					{
						if (!d->checkInput(inputField->GetInput()))
							canConfirmNow = false;
					}
					return canConfirmNow;
				});
				confirmButton->SetOnPress(std::bind(&Dialog::Confirm,d,false));
			}

			if (cancelButton)
			{
				cancelButton->SetOnPress(std::bind(&Dialog::DumpMe,dialog.get()));
			}

					//confirmButton->SetOnPress

			//dialog->reLayout = [textField,confirmButton,cancelButton,inputField,imageField](Dialog&dialog)
			//{
			//	//chicken-egg-dance
			//	if (confirmButton)
			//		confirmButton->MoveTopTo(textField->rect.y.min - 0.025f);
			//	if (cancelButton)
			//		cancelButton->MoveTopTo(textField->rect.y.min - 0.025f);


			//	dialog.totalSize = textField->rect;
			//	for (const auto&c : dialog.components)
			//		if (c != textField && c != imageField)
			//			dialog.totalSize.Include(c->rect);

			//	//float center = totalSize.y.center();
			//	float top = 0.5f + dialog.totalSize.y.GetExtent()/2.f;
			//	textField->MoveTopTo(top);

			//	if (inputField)
			//	{
			//		inputField->MoveTopTo(textField->rect.y.min - 0.025f);

			//		if (confirmButton)
			//			confirmButton->MoveTopTo(inputField->rect.y.min - 0.025f);
			//		if (cancelButton)
			//			cancelButton->MoveTopTo(inputField->rect.y.min - 0.025f);
			//	}
			//	else
			//	{
			//		if (confirmButton)
			//			confirmButton->MoveTopTo(textField->rect.y.min - 0.025f);
			//		if (cancelButton)
			//			cancelButton->MoveTopTo(textField->rect.y.min - 0.025f);
			//	}

			//	dialog.totalSize = textField->rect;
			//	for (const auto&c : dialog.components)
			//		if (c != textField && c != imageField)
			//			dialog.totalSize.Include(c->rect);

			//	dialog.totalSize.Expand(0.015f);

			//	if (imageField)
			//		imageField->rect.y = textField->rect.y;

			//	Rect<> r = textField->rect;
			//	if (imageField)
			//		r.Include(imageField->rect);
			//	if (inputField)
			//		r.Include(inputField->rect);
			//	dialog->Set
			//	common->rect = r;

			//	dialog.UpdateHeight();
			//	//foreach (dialog.components,c)
			//	//	(*c)->UpdateHeight();
			//};

			dialog->ReLayout();

			//rs.type = type;
		}

	}






	void		Dialog::DumpMe()
	{
		if (!GUI::Hide(this))
			return;	//already erased somewhere else
		//components.Clear();	//redundant, will be erased automatically
		//confirmButton.reset();
		//userInput.reset();
		//textField.reset();
		//image.reset();
	}

	bool		Dialog::CanBeConfirmed(bool bySpace)	const
	{
		if (!confirmButton && !fetchInput && !onPassword)
			return false;	//some dialogs do not have close-options, and do not respond to confirmations

		if ((fetchInput || onPassword) && bySpace)
			return false;

		return true;
	}

	void		Dialog::Confirm(bool bySpace)
	{
		if (!confirmButton && !fetchInput && !onPassword)
			return;
		if (confirmButton && !confirmButton->IsEnabled())
			return;	//would, if it could
		GUI::VerifyIntegrity(CLOCATION);
		bool tut = isTutorial;
		if (!fetchInput && !onPassword)
		{
			if (onConfirm)
				onConfirm();
			DumpMe();
			if (tut)
				OnTutorialsChanged();
		}
		else
		{
			if (bySpace)
				return;
			if (fetchInput)
				fetchInput(userInput->GetInput());
			if (onPassword)
			{
				onPassword(passwordInput->GetInput());
				//passwordInput->WipeInput();	//redundant, possibly dangerous. Once callback has been called, this might be dead
			}
			DumpMe();
		}

		if (tut && Detail::pendingTutorials.IsNotEmpty() && !GUI::IsModal())
		//Detail::visibleDialogs.IsEmpty())
		{
			ShowNextTutorial();
		}
		return;
	}


	namespace Detail
	{
		void OnDone()
		{
			ConfirmCurrent(false);
		}
	}


	bool	ConfirmCurrent(bool bySpace)
	{
		GUI::VerifyIntegrity(CLOCATION);
		GUI::PWindow window = GUI::GetTopMostWindow();
		if (!window)
			return false;
		PDialog d = To<Dialog>::DynamicCast(window);
		if (d && d->CanBeConfirmed(bySpace))
		{
			d->Confirm(bySpace);
			GUI::VerifyIntegrity(CLOCATION);
			return true;
		}
		GUI::VerifyIntegrity(CLOCATION);
		return false;
	}


	bool	HasMoreTutorials()
	{
		return Detail::pendingTutorials.IsNotEmpty();

	}


	
	void	ShowNextTutorial()
	{
		if (RuntimeConfig::tutorialMode)
			return;	//they really should not mix
		GUI::VerifyIntegrity(CLOCATION);
		if (Detail::pendingTutorials.IsNotEmpty())
		{
			Show(Detail::pendingTutorials.Peek());
			Detail::pendingTutorials.EraseFront();
			GUI::VerifyIntegrity(CLOCATION);
		}
	
		OnTutorialsChanged();
		GUI::VerifyIntegrity(CLOCATION);
	
	}

	WDialog	QueryPassword(const String&query, const FOnPassword&onPassword, const FCheckState&check, bool sendNullPasswordOnAbortByUser/*=false*/)
	{
		GUI::VerifyIntegrity(CLOCATION);

		PDialog d = Detail::NewModalDialog(Detail::ComponentFlags::ConfirmButton | Detail::ComponentFlags::CancelButton | Detail::ComponentFlags::PasswordInput, "OK", false);
		d->textField->SetText(query);

		ASSERT__(!d->isTutorial);

		d->sendNullPasswordOnAbortByUser = sendNullPasswordOnAbortByUser;
		d->onPassword = onPassword;
		d->check = check;

		d->confirmButton->DetectEnabledState();

		if (sendNullPasswordOnAbortByUser)
			d->cancelButton->SetOnPress([d](){d->onPassword(nullptr); d->DumpMe();});

		d->ReLayout();
		GUI::VerifyIntegrity(CLOCATION);

		return d;
	}


	WDialog	Query(const String&query, const String&initial, const String&allowChars, const FCheckInput&check, const FFetchInput&fetch)
	{
		GUI::VerifyIntegrity(CLOCATION);
		PDialog d = Detail::NewModalDialog(Detail::ComponentFlags::ConfirmButton | Detail::ComponentFlags::CancelButton | Detail::ComponentFlags::Input, "OK", false);
		d->textField->SetText(query);

		ASSERT__(!d->isTutorial);


		d->fetchInput = fetch;
		d->checkInput = check;

		d->userInput->SetCurrent(initial);
		d->userInput->SetAllowed(allowChars);

		d->confirmButton->DetectEnabledState();

		d->ReLayout();
		GUI::VerifyIntegrity(CLOCATION);

		return d;
	}

	WDialog	CheckedQuery(const String&query, const String&initial, const String&allowChars, const FCheckState&check, const FFetchInput&fetch)
	{
		GUI::VerifyIntegrity(CLOCATION);

		PDialog d = Detail::NewModalDialog(Detail::ComponentFlags::ConfirmButton | Detail::ComponentFlags::CancelButton | Detail::ComponentFlags::Input, "OK", false);
		d->textField->SetText(query);

		ASSERT__(!d->isTutorial);


		d->fetchInput = fetch;
		d->check = check;

		d->userInput->SetCurrent(initial);
		d->userInput->SetAllowed(allowChars);

		d->confirmButton->DetectEnabledState();

		d->ReLayout();
		GUI::VerifyIntegrity(CLOCATION);

		return d;
	}

	String	WordForThinking(const RuntimeConfig::RoundNumber&roundNumber, const void*factionPointer)
	{
		static String words[] = {
			"Thinking",
			"Looking really closely at ... not the 'End Round' button",
			"Trying hard and long to be smarter than you. Mostly long",
			"Despairing",
			"Twiddling thumbs. Not pressing buttons you want them to",
			"Totally not hitting return right now",
			"Trolling you",
			"One with the game. Not one with the 'End Round' button",
			"It's a bird, it's a plane. It's not the 'End Round' button",
			"Looking at the beautiful textures. Wait, what?",
			"Beyond the flow of time",
			"Please hold the line...",
		};
		CRC32::beginSequence();
		CRC32::calcNext(&roundNumber,sizeof(roundNumber));
		CRC32::calcNext(&factionPointer,sizeof(factionPointer));
		
		UINT32 random = CRC32::endSequence();

		return words[random % ARRAYSIZE(words)];
	}


	



	WDialog	ShowFactions(const Buffer0<Faction>&factions)
	{
		GUI::VerifyIntegrity(CLOCATION);
		if (Detail::factionDialog)
		{
			RemoveDialog(Detail::factionDialog);
			GUI::VerifyIntegrity(CLOCATION);
		}


		PDialog d = Detail::NewModalDialog(Detail::ComponentFlags::ConfirmButton, "Close", false);

		Detail::factionDialog = d;

		Array<String> list(factions.Count()+1);
		list.First() = "Known factions:";
		for (index_t i = 0; i < factions.Count(); i++)
			list[i+1] = "* "+factions[i].name+(factions[i].online?
			(factions[i].requestedNewRound ? "" : " ("+WordForThinking(RuntimeConfig::roundNumber,factions[i].factionReference )+")")
			:"   -    offline");

		d->textField->SetText( list );
		d->textField->alignCenter = false;
		d->textField->zoom = 0.9f;
		d->ReLayout();

		GUI::VerifyIntegrity(CLOCATION);
		return d;
	}

	bool	RemoveTutorial(const PDialog&dialog)
	{
		GUI::VerifyIntegrity(CLOCATION);
		if (!dialog)
			return false;

		bool found = Detail::pendingTutorials.Contains(dialog);

		if (!found)
		{
			return RemoveDialog(dialog);
		}

		Queue<PDialog,Swap>	temp;
		foreach (Detail::pendingTutorials,t)
			if ((*t) != dialog)
				temp.Push(*t);
		Detail::pendingTutorials.adoptData(temp);
		GUI::VerifyIntegrity(CLOCATION);
		return true;
	}


	bool	RemoveTutorial(const String&key)
	{
		GUI::VerifyIntegrity(CLOCATION);
		bool found = false;
		foreach (Detail::pendingTutorials,t)
			if ((*t)->key == key)
			{
				found = true;
				break;
			}
		if (!found)
			return false;

		Queue<PDialog,Swap>	temp;
		foreach (Detail::pendingTutorials,t)
			if ((*t)->key != key)
				temp.Push(*t);
		Detail::pendingTutorials.adoptData(temp);
		GUI::VerifyIntegrity(CLOCATION);

		return true;
	}

	bool	RemoveDialog(const PDialog&dialog)
	{
		return GUI::Hide(dialog);
	}

	PDialog	CreateTutorialMessage(const String&key, const ArrayRef<const String>&text, const String&imageName /*= ""*/, const Rect<>&screenReference /*= Rect<>::Invalid*/)
	{

		
		static StringTable<DX11::Texture,Strategy::Swap>	textureMap;


		PDialog d(new Dialog());


		DX11::Texture setTexture;

		if (imageName.IsNotEmpty())
		{
			DX11::Texture*tex = textureMap.QueryPointer(imageName);
			if (tex)
			{
				setTexture = *tex;
			}
			else
			{
				try
				{
					Image image;
					Magic::LoadFromFile(image,PathString("resources/tutorial/"+imageName));
					setTexture.Load(image);
					textureMap.Set(imageName,setTexture);
				}
				catch (const std::exception&e)
				{
					FATAL__(e.what());
				}
			}
		}
		bool hasImage = setTexture.IsNotEmpty();
		Detail::Configure(d,hasImage ?  Detail::ComponentFlags::ConfirmButton|Detail::ComponentFlags::Image : Detail::ComponentFlags::ConfirmButton, "Got it", true);
		if (hasImage)
			d->image->texture = setTexture;
		d->textField->SetText(text);

		d->key = key;
		//d.cancelButton.SetText( "Skip Tutorial" );

		d->ReLayout();
		GUI::VerifyIntegrity(CLOCATION);

		return d;
	}


	WDialog	EnqueueTutorial(const String&key, const Array<String>&text, const String&imageName /*= ""*/, const Rect<>&screenReference /*= Rect<>::Invalid*/)
	{
		GUI::VerifyIntegrity(CLOCATION);
		if (!Detail::showTutorials)
			return PDialog();

		PDialog d = CreateTutorialMessage(key, text, imageName, screenReference);
		Detail::pendingTutorials.Push(d);

		OnTutorialsChanged();

		GUI::VerifyIntegrity(CLOCATION);
		return d;
	}



	//bool	HandleClick()
	//{
	//	if (Detail::visibleWindows.IsNotEmpty())
	//	{
	//		Detail::visibleWindows.last()->HandleClick();
	//		return true;
	//	}
	//	return false;
	//}


	


	size_t	CharLength(char c)
	{
		return (size_t)textout.GetFont().GetWidth(c);
	}


	WDialog	AskYesNoHelp(const String&query, const String&help, const FOnConfirmBool&onAnswer, const FCheckState&check)
	{
		GUI::VerifyIntegrity(CLOCATION);
		CheckResult ck = check();
		if (ck == CheckResult::Abort)
			return PDialog();
		if (ck == CheckResult::Confirm)
		{
			onAnswer(true);
			return PDialog();
		}

		PDialog d = Detail::NewModalDialog(Detail::ComponentFlags::ConfirmButton| Detail::ComponentFlags::NoButton|Detail::ComponentFlags::HelpButton, "Yes", false);

		d->check = check;
		d->onConfirm = [onAnswer](){onAnswer(true);};
		d->onNo = [onAnswer](){onAnswer(false);};
		d->textField->SetText( query );
		d->helpButton->SetOnPress([help,check]() {ShowMessage(help,check);});
		d->ReLayout();
		GUI::VerifyIntegrity(CLOCATION);
		return d;
	}

	/**
	Creates a new modal dialog on top of any other active modal dialogs
	*/
	WDialog	Confirm(const String&query, const FOnConfirm&onConfirm, const FCheckState&check)
	{
		GUI::VerifyIntegrity(CLOCATION);
		CheckResult ck = check();
		if (ck == CheckResult::Abort)
			return PDialog();
		if (ck == CheckResult::Confirm)
		{
			onConfirm();
			return PDialog();
		}
		if (RuntimeConfig::aiMode)
		{
			if (ck == CheckResult::OkayContinue)
				onConfirm();
			return PDialog();
		}


		PDialog d = Detail::NewModalDialog(Detail::ComponentFlags::ConfirmButton| Detail::ComponentFlags::CancelButton, "Confirm", false);

		d->check = check;
		d->onConfirm = onConfirm;
		d->textField->SetText( query );

		d->ReLayout();
		GUI::VerifyIntegrity(CLOCATION);
		return d;
	}

	WDialog	PermanentMessage(const String&message, const FCheckState&check)
	{
		GUI::VerifyIntegrity(CLOCATION);
		CheckResult ck = check();
		if (ck == CheckResult::Abort)
			return PDialog();



		PDialog d = Detail::NewModalDialog(0, "", false);
		d->canBeImplicitlyClosed = false;

		d->check = check;
		d->textField->SetText( message );

		d->ReLayout();
		GUI::VerifyIntegrity(CLOCATION);
		return d;
	}


	WDialog	ShowMessage(const String&msg, const FCheckState&check)
	{
		GUI::VerifyIntegrity(CLOCATION);


		PDialog d = Detail::NewModalDialog(Detail::ComponentFlags::ConfirmButton, "Got it", false);

		d->textField->SetText( msg );
		d->check = check;

		d->ReLayout();

		GUI::VerifyIntegrity(CLOCATION);
		return d;


	}

	/**
	Creates a new modal dialog on top of any other active modal dialogs that simply shows a message
	*/
	WDialog	ShowMessage(const String&msg)
	{
		return ShowMessage(msg,FCheckState());
	}





	PMenu				loginMenu;
	GUI::PWindow	serverChoice;


	void	Create()
	{
		Detail::showTutorials = !FileSystem::IsFile(RuntimeConfig::ConfigFile("tutorialsDisabled.mark"));
		GUI::Create();


		loginMenu.reset(new Menu(0.25f,"Login"));
		loginMenu->canBeImplicitlyClosed = false;	//once visible, must explicitly close

		serverChoice.reset(new GUI::Window());
		serverChoice->SetCenteredWidth(0.9f,GUI::TAnchor::RelativeToScreen);
		serverChoice->canBeImplicitlyClosed = false;


	}


	void	CheckServersAreShown(bool doShow)
	{
		GUI::VerifyIntegrity(CLOCATION);

		if (doShow)
		{
			ShowBottom(serverChoice);
		}
		else
			Hide(serverChoice);
		GUI::VerifyIntegrity(CLOCATION);
	}


	void	Destroy()
	{
		loginMenu.reset();
		Detail::pendingTutorials.Clear();
		GUI::Destroy();
	}

	bool	ShowingFactions()
	{
		return IsVisible( Detail::factionDialog ) ;
	}




}


#endif

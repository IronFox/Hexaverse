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
#include "../clientEvents.h"
#include "music.h"

namespace DX11 = Engine::DX11; 

extern Engine::Textout<DX11::TextureFont>	textout;
extern Engine::Display<Engine::DirectX11>	display;

void	OnTutorialsChanged();

Random random;



namespace Dialog
{
	/*virtual override*/ void				Menu::OnScreenSizeOrUIScaleChange()
	{
		LogEvent(EventClass::Preset::gui,"Menu["+String(this)+"/"+myTitle+"]::OnScreenSizeOrUIScaleChange()");
		Super::OnScreenSizeOrUIScaleChange();
	}

	GUI::PButton				Menu::AddCommand(const String&name, const std::function<void()>&execute, const std::function<bool()>&check /*= std::function<bool()>()*/, bool autoHide/*=true*/)
	{
		GUI::PButton button(new GUI::Button(name,
			[execute,this,autoHide]()
			{
				execute();
				if (autoHide)
					GUI::CloseAllModalBeneath(To<GUI::Window>::Cast(this->shared_from_this()));
			},
			check
		));
		Super::RemoveComponent(hintLabel);
		Super::AnchorBelowPrevious(button,0.f,Super::components.IsEmpty() ? 0.f : 0.015f);
		if (hintLabel)
			Super::AnchorBelowPrevious(hintLabel,0.015f,0.015f);

		LogEvent(EventClass::Preset::gui,"Menu["+String(this)+"/"+myTitle+"]::AddCommand("+name+", ...)");
		return button;
	}

	void		Menu::Setup(float initialAbsoluteWidth, const String&title, bool escapeCloses)
	{
		canBeImplicitlyClosed = escapeCloses;
		Super::ClearComponents();
		hintLabel.reset();
		myTitle = title;
		if (Super::DoesAutoCenter())
			SetWidth(initialAbsoluteWidth);
		else
			Super::SetCenteredWidth(initialAbsoluteWidth,GUI::TAnchor::RelativeToScreen);
		Super::fillBackground = false;

		if (title.IsNotEmpty())
		{
			GUI::PLabel label(new GUI::Label());
			label->SetText(title);
			label->SetVerticalPadding( 0.1f );
			Super::AnchorBelowPrevious(label,0.015f,0.f);
		}

		if (escapeCloses)
		{
			hintLabel.reset(new GUI::Label());
			hintLabel->SetLocalFontScale(0.75f);
			hintLabel->SetText("Press Escape to close");
			hintLabel->SetVerticalPadding( 0.1f );
			Super::AnchorBelowPrevious(hintLabel,0.015f,0.f);
		}
		LogEvent(EventClass::Preset::gui,"Menu["+String(this)+"]::Setup("+String(initialAbsoluteWidth)+","+myTitle+")");
	}


	namespace Detail
	{

		Queue<PDialog,Swap>		pendingTutorials;
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
			PDialog dialog = Dialog::Create();
			Configure(dialog,componentFlags,confirmWord, isTutorial);
			Show(dialog);
			return dialog;
		}


		static void SetLeft(const GUI::PComponent&c, float v)
		{
			auto cfg = c->GetAnchorConfig();
			cfg.Left().Set(v);
			c->SetAnchorConfig(cfg);
		}
		static void SetRight(const GUI::PComponent&c, float v)
		{
			auto cfg = c->GetAnchorConfig();
			cfg.Right().Set(v);
			c->SetAnchorConfig(cfg);
		}

		void Configure(PDialog dialog, UINT32 componentFlags, const String&confirmWord, bool isTutorial)
		{
			dialog->isTutorial = isTutorial;

			if (dialog->DoesAutoCenter())
			{
				dialog->SetWidth(0.5f);
			}
			else
			{
				auto cfg = dialog->GetAnchorConfig();
				cfg.Left().Set(0.25f);
				cfg.Right().Set(0.25f);
				dialog->SetAnchorConfig(cfg);
			}
			
			dialog->fillBackground = false;
			//GUI::PComponent	common(new GUI::Area());

			GUI::PPanel	textPanel(new GUI::Panel());
			GUI::PLabel textField(new GUI::Label());
			textPanel->AddComponent(textField);
			{
				auto cfg = textField->GetAnchorConfig();
				cfg.Left().Set(0.f);
				cfg.Right().Set(0.f);
				cfg.Top().Set(0.f);
				textField->SetAnchorConfig(cfg);
				textPanel->SetAnchorConfig(cfg);
			}
			textPanel->SetAutomaticHeight(true);
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
				{
					auto cfg = inputField->GetAnchorConfig();
					cfg.Left().Set(0.f);
					cfg.Right().Set(0.f);
					cfg.Top().Set(textField,0.f);//0.025f);
					inputField->SetAnchorConfig(cfg);
				}
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
				{
					GUI::AnchorConfig cfg;
					cfg.Left().Set(0.f);
					cfg.Right().Set(0.f);
					cfg.Top().Set(textField,0.f);//0.025f);
					passwordInputField->SetAnchorConfig(cfg);
				}
				textPanel->AddComponent (passwordInputField);
			}

			if (componentFlags & ComponentFlags::ConfirmButton)
			{
				confirmButton.reset(new GUI::Button(confirmWord));
				{
					GUI::AnchorConfig cfg;
					cfg.Left().Set(0.f);
					cfg.Right().Set(0.f);
					cfg.Top().Set(textPanel,0.015f,GUI::TAnchor::RelativeToScreen);
					confirmButton->SetAnchorConfig(cfg);
				}
				dialog->AddComponent( confirmButton);
				buttons << std::make_pair(1, confirmButton);
			}
			if (componentFlags & ComponentFlags::HelpButton)
			{
				helpButton.reset(new GUI::Button("?"));
				{
					GUI::AnchorConfig cfg;
					cfg.Top().Set(textPanel,0.015f,GUI::TAnchor::RelativeToScreen);
					helpButton->SetAnchorConfig(cfg);
				}
				dialog->AddComponent(helpButton);
				buttons << std::make_pair(0.25f, helpButton);
			}
			if (componentFlags & ComponentFlags::NoButton)
			{
				noButton.reset(new GUI::Button("No"));
				{
					GUI::AnchorConfig cfg;
					cfg.Top().Set(textPanel,0.015f,GUI::TAnchor::RelativeToScreen);
					noButton->SetAnchorConfig(cfg);
				}
				dialog->AddComponent( noButton);
				buttons << std::make_pair(1, noButton);
			}
			if (componentFlags & ComponentFlags::CancelButton)
			{
				cancelButton.reset(new GUI::Button("Cancel"));

				//if (confirmButton)
				//	confirmButton->anchorConfig.Right().Set(0.505f);
				{
					GUI::AnchorConfig cfg;
					cfg.Left().Set(0.505f);
					cfg.Right().Set(0.f);
					cfg.Top().Set(textPanel,0.015f,GUI::TAnchor::RelativeToScreen);
					cancelButton->SetAnchorConfig(cfg);
				}
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
					SetLeft(buttons[i].second,at*width + 0.005f*(i != 0));
					at += buttons[i].first;
				}
				at = 0.f;
				for (index_t i = buttons.Count()-1; i < buttons.Count(); i--)
				{
					SetRight(buttons[i].second,at*width + 0.005f*(i+1 != buttons.Count()));
					at += buttons[i].first;
				}
			}


			if (componentFlags & ComponentFlags::Image)
			{
				imageField.reset(new GUI::Image());
				

				{
					auto cfg = imageField->GetAnchorConfig();
					cfg.Left().Set(0.f);
					cfg.Right().Set(0.6f);
					cfg.Top().Set(0.05f);
					if (confirmButton)
					{
						//imageField->autoSetHeight = false;
						//imageField->anchorConfig.Bottom().Set(confirmButton,0.03f,false);
						imageField->SetAutoHeight(true);
						cfg.Top().SetAuto();
						cfg.Bottom().SetAuto();
					}
					imageField->SetAnchorConfig(cfg);
				}
				{
					auto cfg = textField->GetAnchorConfig();
					cfg.Left().Set(0.405f);
					cfg.Bottom().SetAuto();
					cfg.Top().SetAuto();
					textField->SetAnchorConfig(cfg);
				}
				//textField->anchorConfig.axis[1].center.Set(imageField,0.f);
	//			imageField->SetLocation(M::Rect<>(0,1,0.25,0.75f));
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
						if (!d->checkInput(inputField->GetText()))
							canConfirmNow = false;
					}
					return canConfirmNow;
				});
				confirmButton->SetTriggerFunction(std::bind(&Dialog::Confirm,d,false));
			}

			if (cancelButton)
			{
				cancelButton->SetTriggerFunction(std::bind(&Dialog::DumpMe,dialog.get()));
			}

					//confirmButton->SetTriggerFunction

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

			//	M::Rect<> r = textField->rect;
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
		const bool tut = isTutorial;
		const bool nextTut = tut && autoNextTutorial;
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
				fetchInput(userInput->GetText());
			if (onPassword)
			{
				onPassword(passwordInput->GetText());
				//passwordInput->WipeInput();	//redundant, possibly dangerous. Once callback has been called, this might be dead
			}
			DumpMe();
		}

		if (nextTut && Detail::pendingTutorials.IsNotEmpty() && !GUI::IsModal())
		//Detail::visibleDialogs.IsEmpty())
		{
			ShowNextTutorial(true);
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


	
	void	ShowNextTutorial(bool autoNext)
	{
		if (RuntimeConfig::tutorialMode)
			return;	//they really should not mix
		GUI::VerifyIntegrity(CLOCATION);
		if (Detail::pendingTutorials.IsNotEmpty())
		{
			auto tut = Detail::pendingTutorials.Peek();
			tut->autoNextTutorial = autoNext;
			Show(tut);
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
			d->cancelButton->SetTriggerFunction([d](){d->onPassword(nullptr); d->DumpMe();});

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

		d->userInput->SetText(initial);
		d->userInput->SetAllowedAlphabet(allowChars);

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

		d->userInput->SetText(initial);
		d->userInput->SetAllowedAlphabet(allowChars);

		d->confirmButton->DetectEnabledState();

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

	PDialog	CreateTutorialMessage(const String&key, const ArrayRef<const String>&text, const String&imageName /*= ""*/)
	{

		
		static StringTable<DX11::Texture,Strategy::Swap>	textureMap;


		PDialog d = Dialog::Create();


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
			d->image->SetTexture(setTexture);
		d->textField->SetText(text);

		d->key = key;
		//d.cancelButton.SetText( "Skip Tutorial" );

		d->ReLayout();
		GUI::VerifyIntegrity(CLOCATION);

		return d;
	}


	WDialog	EnqueueTutorial(const String&key, const Array<String>&text, const String&imageName /*= ""*/, bool prioritize/*=false*/)
	{
		GUI::VerifyIntegrity(CLOCATION);
		if (!Detail::showTutorials)
			return PDialog();

		PDialog d = CreateTutorialMessage(key, text, imageName);
		if (prioritize)
		{
			Detail::pendingTutorials.PushFront(d);
		}
		else
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
		d->helpButton->SetTriggerFunction([help,check]() {ShowMessage(help,check);});
		d->ReLayout();
		GUI::VerifyIntegrity(CLOCATION);
		return d;
	}


	WDialog	Confirm(const String&query, const FOnConfirm&onConfirm)
	{
		return Confirm(query,onConfirm,[](){return CheckResult::OkayContinue;});
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





	GUI::PWindow	serverChoice;


	void	Create()
	{
		Detail::showTutorials = !FileSystem::IsFile(RuntimeConfig::ConfigFile("tutorialsDisabled.mark"));
		GUI::Create();



		serverChoice = GUI::Window::Create();
		serverChoice->SetAutoCenter(false);
		serverChoice->SetCenteredWidth(0.9f,GUI::TAnchor::RelativeToScreen);
		serverChoice->canBeImplicitlyClosed = false;


		GlobalEvents::applicationTerminate << Destroy;
	}


	void	CheckServersAreShown(bool doShow)
	{
		GUI::VerifyIntegrity(CLOCATION);

		Music::SignalServerListShown(doShow);

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
		Detail::pendingTutorials.Clear();
		GUI::Destroy();
	}




}


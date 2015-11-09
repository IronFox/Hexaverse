#ifdef HAS_WINDOW

#include <global_root.h>
#include "dialog.h"
#include <engine/display.h>
#include "../ExtRenderer/DirectX11.h"
#include "../DX11/texture_font.h"
#include <container/buffer.h>
#include "surfaceRenderer.h"
#include <engine/aspect.h>
#include <image/converter/magic.h>
#include <general/random.h>
#include "../../shared/cast.h"
#include "../../shared/logEvent.h"
#include "clientRuntimeConfig.h"

namespace DX11 = Engine::DX11; 

extern Engine::Textout<DX11::TextureFont>	textout;
extern Engine::Display<Engine::DirectX11>	display;

void	OnTutorialsChanged();

Random random;



namespace Dialog
{
	namespace Detail
	{

		Buffer0<GUIElements::PWindow,Swap>	visibleWindows;
		Buffer0<PDialog,Swap>	visibleDialogs;//subset of visibleWindows, not necessarily the top-most one(s)
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
			if (visibleWindows.IsNotEmpty())
				visibleWindows.last()->Deactivate();
			PDialog dialog(new Dialog());
			visibleWindows << dialog;
			visibleDialogs << dialog;
			Configure(dialog,componentFlags,confirmWord, isTutorial);
			dialog->Activate();
			return dialog;
		}

		void Configure(PDialog dialog, UINT32 componentFlags, const String&confirmWord, bool isTutorial)
		{
			dialog->isTutorial = isTutorial;

			dialog->anchorConfig.Left().Set(0.25f);
			dialog->anchorConfig.Right().Set(0.25f);
			
			dialog->fillBackground = false;
			//GUIElements::PComponent	common(new GUIElements::Area());

			GUIElements::PPanel	textPanel(new GUIElements::Panel());
			GUIElements::PLabel textField(new GUIElements::Label());
			textPanel->components.Append(textField);
			textField->anchorConfig.Left().Set(0.f);
			textField->anchorConfig.Right().Set(0.f);
			textField->anchorConfig.Top().Set(0.f);
			textPanel->anchorConfig = textField->anchorConfig;
			textPanel->autoHeight = true;
			dialog->components << textPanel;
			
			//height controlled by content

			GUIElements::PButton confirmButton,cancelButton,noButton,helpButton;
			GUIElements::PInput	inputField;
			GUIElements::PPasswordInput passwordInputField;
			GUIElements::PImage	imageField;
			Buffer0<std::pair<float,GUIElements::PButton> > buttons;


			if (componentFlags & ComponentFlags::Input)
			{
				inputField.reset(new GUIElements::Input());
				inputField->anchorConfig.Left().Set(0.f);
				inputField->anchorConfig.Right().Set(0.f);
				inputField->anchorConfig.Top().Set(textField,0.f);//0.025f);
				textPanel->components.Append (inputField);
			}
			if (componentFlags & ComponentFlags::PasswordInput)
			{
				passwordInputField.reset(new GUIElements::PasswordInput());
				passwordInputField->anchorConfig.Left().Set(0.f);
				passwordInputField->anchorConfig.Right().Set(0.f);
				passwordInputField->anchorConfig.Top().Set(textField,0.f);//0.025f);
				textPanel->components.Append (passwordInputField);
			}

			if (componentFlags & ComponentFlags::ConfirmButton)
			{
				confirmButton.reset(new GUIElements::Button(confirmWord));
				confirmButton->anchorConfig.Left().Set(0.f);
				confirmButton->anchorConfig.Right().Set(0.f);
				confirmButton->anchorConfig.Top().Set(textPanel,0.015f,GUIElements::TAnchor::RelativeToScreen);
				dialog->components << confirmButton;
				buttons << std::make_pair(1, confirmButton);
			}
			if (componentFlags & ComponentFlags::HelpButton)
			{
				helpButton.reset(new GUIElements::Button("?"));
				helpButton->anchorConfig.Top().Set(textPanel,0.015f,GUIElements::TAnchor::RelativeToScreen);
				dialog->components << helpButton;
				buttons << std::make_pair(0.25f, helpButton);
			}
			if (componentFlags & ComponentFlags::NoButton)
			{
				noButton.reset(new GUIElements::Button("No"));
				noButton->anchorConfig.Top().Set(textPanel,0.015f,GUIElements::TAnchor::RelativeToScreen);
				dialog->components << noButton;
				buttons << std::make_pair(1, noButton);
			}
			if (componentFlags & ComponentFlags::CancelButton)
			{
				cancelButton.reset(new GUIElements::Button("Cancel"));

				//if (confirmButton)
				//	confirmButton->anchorConfig.Right().Set(0.505f);
				cancelButton->anchorConfig.Left().Set(0.505f);
				cancelButton->anchorConfig.Right().Set(0.f);
				cancelButton->anchorConfig.Top().Set(textPanel,0.015f,GUIElements::TAnchor::RelativeToScreen);
				dialog->components << cancelButton;
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
				imageField.reset(new GUIElements::Image());
				

				textField->anchorConfig.Left().Set(0.405f);
				imageField->anchorConfig.Left().Set(0.f);
				imageField->anchorConfig.Right().Set(0.6f);
				imageField->anchorConfig.Top().Set(0.05f);
				if (confirmButton)
				{
					//imageField->autoSetHeight = false;
					//imageField->anchorConfig.Bottom().Set(confirmButton,0.03f,false);
					imageField->autoSetHeight = true;
				}
				textField->anchorConfig.Bottom().SetAuto();
				textField->anchorConfig.Top().SetAuto();
				//textField->anchorConfig.axis[1].center.Set(imageField,0.f);
	//			imageField->SetLocation(Rect<>(0,1,0.25,0.75f));
				//textField->rect.Set(0.405f,0.25f,0.75f,0.75f);
				//imageField->rect.Set(0.25f,0.25f,0.4f,0.75f);
				textField->alignCenter = false;
				textPanel->components << imageField;
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
			//		confirmButton.reset(new GUIElements::Button());
			//	}
			//	break;
			//	case ModalDialogType::StringQuery:
			//	{
			//		confirmButton.reset(new GUIElements::Button());
			//		cancelButton.reset(new GUIElements::Button());
			//		inputField.reset(new GUIElements::Input());
			//	}
			//	break;
			//	case ModalDialogType::TutorialMessage:
			//	{
			//		confirmButton.reset(new GUIElements::Button());
			//		imageField.reset(new GUIElements::Image());
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
			//	float top = 0.5f + dialog.totalSize.y.GetExtend()/2.f;
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


	void	VerifyIntegrity(const TCodeLocation&loc)
	{
		if (Detail::visibleWindows.IsEmpty())
			return;


		for (index_t i = 0; i < Detail::visibleWindows.Count(); i++)
		{
			bool shouldBeActive = (i+1 == Detail::visibleWindows.Count());
			GUIElements::PWindow w = Detail::visibleWindows[i];
			if (w->IsActive() != shouldBeActive)
			{
				String msg = String(loc.ToString())+ ": Activity mismatch for window "+String(i+1)+"/"+String(Detail::visibleWindows.Count())+" (active="+String(w->IsActive())+", should be "+String(shouldBeActive)+")";
				LogEvent(EventType::InternalFault,nullptr,msg);
				DBG_FATAL__(msg);
				if (shouldBeActive)
					w->Activate();
				else
					w->Deactivate();
			}
		}
	}



	void		Dialog::DumpMe()
	{
//		reLayout.swap(std::function<void(Dialog&)>());
		index_t dialogAt = InvalidIndex;
		foreach (Detail::visibleWindows,_w)
			if (_w->get() == this)
			{
				dialogAt = _w - Detail::visibleWindows.pointer();
				break;
			}
		if (dialogAt == InvalidIndex)
			return;	//already erased somewhere else


		bool active = Super::IsActive();
		if (active)
			Deactivate();

		components.Clear();
		confirmButton.reset();
		userInput.reset();
		textField.reset();
		image.reset();

		Detail::visibleWindows.Erase(dialogAt);
		Detail::visibleDialogs.FindAndErase(shared_from_this());
		if (active && Detail::visibleWindows.IsNotEmpty())
			Detail::visibleWindows.last()->Activate();
		VerifyIntegrity(CLOCATION);
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
		VerifyIntegrity(CLOCATION);
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

		if (tut && Detail::pendingTutorials.IsNotEmpty() && !IsModal())
		//Detail::visibleDialogs.IsEmpty())
		{
			ShowNextTutorial();
		}
		return;
	}


	namespace Detail
	{
		void OnInput()
		{
			if (visibleWindows.IsNotEmpty())
				visibleWindows.last()->DetectEnabledState();
		}
		void OnDone()
		{
			ConfirmCurrent(false);
		}
	}


	bool	ConfirmCurrent(bool bySpace)
	{
		VerifyIntegrity(CLOCATION);
		if (Detail::visibleWindows.IsEmpty())
			return false;
		PDialog d = To<Dialog>::DynamicCast(Detail::visibleWindows.last());
		if (d && d->CanBeConfirmed(bySpace))
		{
			d->Confirm(bySpace);
			VerifyIntegrity(CLOCATION);
			return true;
		}
		VerifyIntegrity(CLOCATION);
		return false;
	}


	bool	HasMoreTutorials()
	{
		return Detail::pendingTutorials.IsNotEmpty();

	}


	
	void	ShowNextTutorial()
	{
		VerifyIntegrity(CLOCATION);
		if (Detail::pendingTutorials.IsNotEmpty())
		{
			Show(Detail::pendingTutorials.Peek());
			Detail::pendingTutorials.EraseFront();
			VerifyIntegrity(CLOCATION);
		}
	
		OnTutorialsChanged();
		VerifyIntegrity(CLOCATION);
	
	}

	WDialog	QueryPassword(const String&query, const FOnPassword&onPassword, const FCheckState&check, bool sendNullPasswordOnAbortByUser/*=false*/)
	{
		VerifyIntegrity(CLOCATION);
		GUIElements::Input::onChange = Detail::OnInput;
		GUIElements::Input::onDone = Detail::OnDone;


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
		VerifyIntegrity(CLOCATION);

		return d;
	}


	WDialog	Query(const String&query, const String&initial, const String&allowChars, const FCheckInput&check, const FFetchInput&fetch)
	{
		VerifyIntegrity(CLOCATION);
		GUIElements::Input::onChange = Detail::OnInput;
		GUIElements::Input::onDone = Detail::OnDone;


		PDialog d = Detail::NewModalDialog(Detail::ComponentFlags::ConfirmButton | Detail::ComponentFlags::CancelButton | Detail::ComponentFlags::Input, "OK", false);
		d->textField->SetText(query);

		ASSERT__(!d->isTutorial);


		d->fetchInput = fetch;
		d->checkInput = check;

		d->userInput->SetCurrent(initial);
		d->userInput->SetAllowed(allowChars);

		d->confirmButton->DetectEnabledState();

		d->ReLayout();
		VerifyIntegrity(CLOCATION);

		return d;
	}

	String	WordForThinking(UINT32 roundNumber, const void*factionPointer)
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


	
	bool	Hide(const GUIElements::PWindow&window)
	{
		VerifyIntegrity(CLOCATION);
		index_t index = Detail::visibleWindows.GetIndexOf(window);
		if (index == InvalidIndex)
			return false;

		Detail::visibleWindows.Erase(index);

		PDialog d = To<Dialog>::DynamicCast(window);
		if (d)
		{
			if (d->onPassword && d->sendNullPasswordOnAbortByUser && (!d->check || d->check() == CheckResult::OkayContinue))
			{
				d->onPassword(nullptr);
			}
			Detail::visibleDialogs.FindAndErase(d);
		}

		if (index == Detail::visibleWindows.Count())
		{
			window->Deactivate();
			if (Detail::visibleWindows.IsNotEmpty())
				Detail::visibleWindows.last()->Activate();
		}

		VerifyIntegrity(CLOCATION);

		SignalSituationChange();

		return true;

	}

	bool	IsVisible(const GUIElements::PWindow&window)
	{
		return Detail::visibleWindows.Contains(window);
	}

	void	Show(const GUIElements::PWindow&window)
	{
		VerifyIntegrity(CLOCATION);
		if (Detail::visibleWindows.IsNotEmpty() && Detail::visibleWindows.last() == window)
		{
			DBG_ASSERT__(window->IsActive());
			if (!window->IsActive())
				window->Activate();
			return;
		}
		//if (Detail::visibleWindows.Count() == 1 && Detail::visibleWindows.first() == window)
		//	return;
		Detail::visibleWindows.FindAndErase(window);
		if (Detail::visibleWindows.IsNotEmpty())
			Detail::visibleWindows.last()->Deactivate();
		Detail::visibleWindows << window;
		window->Activate();
		PDialog d = To<Dialog>::DynamicCast(window);
		if (d)
			Detail::visibleDialogs << d;

		VerifyIntegrity(CLOCATION);

		SignalSituationChange();

	}



	void	ShowBottom(const GUIElements::PWindow&window)
	{
		VerifyIntegrity(CLOCATION);
		if (Detail::visibleWindows.IsNotEmpty() && Detail::visibleWindows.first() == window)
		{
			if (Detail::visibleWindows.Count() == 1)
			{
				DBG_ASSERT__(window->IsActive());
				if (!window->IsActive())
					window->Activate();
			}
			return;
		}
		//if (Detail::visibleWindows.Count() == 1 && Detail::visibleWindows.first() == window)
		//	return;

		index_t at = Detail::visibleWindows.GetIndexOf(window);
		if (at != InvalidIndex)
		{
			if (at+1 == Detail::visibleWindows.Count())	//ist last
			{
				window->Deactivate();
				Detail::visibleWindows.fromEnd(1)->Activate();
			}
			Detail::visibleWindows.Erase(at);
		} 
		Detail::visibleWindows.Insert(0,window);
		if (Detail::visibleWindows.Count() == 1)
			window->Activate();

		VerifyIntegrity(CLOCATION);
		SignalSituationChange();

	}



	WDialog	ShowFactions(const Buffer0<Faction>&factions)
	{
		VerifyIntegrity(CLOCATION);
		if (Detail::factionDialog)
		{
			RemoveDialog(Detail::factionDialog);
			VerifyIntegrity(CLOCATION);
		}


		PDialog d = Detail::NewModalDialog(Detail::ComponentFlags::ConfirmButton, "Close", false);

		Detail::factionDialog = d;

		Array<String> list(factions.Count()+1);
		list.first() = "Known factions:";
		for (index_t i = 0; i < factions.Count(); i++)
			list[i+1] = "* "+factions[i].name+(factions[i].online?
			(factions[i].requestedNewRound ? "" : " ("+WordForThinking(RuntimeConfig::roundNumber,factions[i].factionReference )+")")
			:"   -    offline");

		d->textField->SetText( list );
		d->textField->alignCenter = false;
		d->textField->zoom = 0.9f;
		d->ReLayout();

		VerifyIntegrity(CLOCATION);
		return d;
	}

	bool	RemoveTutorial(const PDialog&dialog)
	{
		VerifyIntegrity(CLOCATION);
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
		VerifyIntegrity(CLOCATION);
		return true;
	}


	bool	RemoveTutorial(const String&key)
	{
		VerifyIntegrity(CLOCATION);
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
		VerifyIntegrity(CLOCATION);

		return true;
	}

	bool	RemoveDialog(const PDialog&dialog)
	{
		VerifyIntegrity(CLOCATION);
		if (!dialog)
			return false;
		index_t at = Detail::visibleWindows.GetIndexOf(dialog);
		if (at == InvalidIndex)
			return false;
		Detail::visibleDialogs.FindAndErase(dialog);
		if (at == Detail::visibleWindows.Count()-1)
		{
			dialog->Deactivate();
			Detail::visibleWindows.EraseLast();
			if (Detail::visibleWindows.IsNotEmpty())
				Detail::visibleWindows.last()->Activate();
		}
		else
			Detail::visibleWindows.Erase(at);
		VerifyIntegrity(CLOCATION);
		return true;
	}

	WDialog	EnqueueTutorial(const String&key, const Array<String>&text, const String&imageName /*= ""*/, const Rect<>&screenReference /*= Rect<>::Invalid*/)
	{
		VerifyIntegrity(CLOCATION);
		if (!Detail::showTutorials)
			return PDialog();
		static StringTable<DX11::Texture,Strategy::Swap>	textureMap;



		PDialog&d = /*Detail::visibleWindows.IsEmpty() ? Detail::visibleWindows.Append() : */Detail::pendingTutorials.Push();
		d.reset(new Dialog());


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
		VerifyIntegrity(CLOCATION);

		OnTutorialsChanged();

		VerifyIntegrity(CLOCATION);
		return d;
	}

	bool	HandleMouseDown()
	{
		for (index_t i = Detail::visibleWindows.Count()-1; i < Detail::visibleWindows.Count(); i--)
		{
			GUIElements::PWindow w = Detail::visibleWindows[i];
			GUIElements::mousePressedComponent = w->HandleMouseDown();
			if (GUIElements::mousePressedComponent || w->isModal)
				return true;
		}
		GUIElements::mousePressedComponent.reset();
		return false;
	}

	void	HandleMouseUp()
	{
		if (GUIElements::mousePressedComponent)
		{
			GUIElements::mousePressedComponent->SignalMouseUp();
			GUIElements::mousePressedComponent.reset();
		}

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


	



	/**
	Signals that the global situation has changed and active modal dialogs need to be re-evalutated.
	This may be invoked by a server state change or removal of some entity
	*/
	void	SignalSituationChange()
	{
		VerifyIntegrity(CLOCATION);
		for (index_t i = 0; i < Detail::visibleDialogs.Count(); i++)
		{
			const PDialog&d = Detail::visibleDialogs[i];
			if (!d->SignalChange())
			{
				index_t at = Detail::visibleWindows.GetIndexOf(d);
				if (at != InvalidIndex)
				{
					Detail::visibleWindows.Erase(at);
					if (at == Detail::visibleWindows.Count())
					{
						d->Deactivate();
						if (Detail::visibleWindows.IsNotEmpty())
							Detail::visibleWindows.last()->Activate();
					}
				}
				Detail::visibleDialogs.Erase(i);
				i--;
			}
		}
		VerifyIntegrity(CLOCATION);

		foreach (Detail::visibleWindows,w)
			(*w)->DetectEnabledState();
	}



	size_t	CharLength(char c)
	{
		return (size_t)textout.getFont().getWidth(c);
	}


	WDialog	AskYesNoHelp(const String&query, const String&help, const FOnConfirmBool&onAnswer, const FCheckState&check)
	{
		VerifyIntegrity(CLOCATION);
		CheckResult ck = check();
		if (ck == CheckResult::Abort)
			return PDialog();

		PDialog d = Detail::NewModalDialog(Detail::ComponentFlags::ConfirmButton| Detail::ComponentFlags::NoButton|Detail::ComponentFlags::HelpButton, "Yes", false);

		d->check = check;
		d->onConfirm = [onAnswer](){onAnswer(true);};
		d->onNo = [onAnswer](){onAnswer(false);};
		d->textField->SetText( query );
		d->helpButton->SetOnPress([help,check]() {ShowMessage(help,check);});
		d->ReLayout();
		VerifyIntegrity(CLOCATION);
		return d;
	}

	/**
	Creates a new modal dialog on top of any other active modal dialogs
	*/
	WDialog	Confirm(const String&query, const FOnConfirm&onConfirm, const FCheckState&check)
	{
		VerifyIntegrity(CLOCATION);
		CheckResult ck = check();
		if (ck == CheckResult::Abort)
			return PDialog();



		PDialog d = Detail::NewModalDialog(Detail::ComponentFlags::ConfirmButton| Detail::ComponentFlags::CancelButton, "Confirm", false);

		d->check = check;
		d->onConfirm = onConfirm;
		d->textField->SetText( query );

		d->ReLayout();
		VerifyIntegrity(CLOCATION);
		return d;
	}

	WDialog	PermanentMessage(const String&message, const FCheckState&check)
	{
		VerifyIntegrity(CLOCATION);
		CheckResult ck = check();
		if (ck == CheckResult::Abort)
			return PDialog();



		PDialog d = Detail::NewModalDialog(0, "", false);
		d->canBeImplicitlyClosed = false;

		d->check = check;
		d->textField->SetText( message );

		d->ReLayout();
		VerifyIntegrity(CLOCATION);
		return d;
	}


	WDialog	ShowMessage(const String&msg, const FCheckState&check)
	{
		VerifyIntegrity(CLOCATION);


		PDialog d = Detail::NewModalDialog(Detail::ComponentFlags::ConfirmButton, "Got it", false);

		d->textField->SetText( msg );
		d->check = check;

		d->ReLayout();

		VerifyIntegrity(CLOCATION);
		return d;


	}

	/**
	Creates a new modal dialog on top of any other active modal dialogs that simply shows a message
	*/
	WDialog	ShowMessage(const String&msg)
	{
		return ShowMessage(msg,FCheckState());
	}


	/**
	Checks if any modal dialogs are open at this time
	*/
	bool	IsModal()
	{
		foreach (Detail::visibleWindows,w)
			if ((*w)->isModal)
				return true;
		return false;
	}

	bool	CloseAllModal()
	{
		VerifyIntegrity(CLOCATION);
		if (Detail::visibleWindows.IsNotEmpty())
		{
			bool anyNonClosable = false;
			foreach (Detail::visibleWindows,w)
				if (!(*w)->canBeImplicitlyClosed)
				{
					anyNonClosable = true;
					break;
				}

			if (anyNonClosable)
			{
				bool rs = false;
				for (index_t i = 0; i < Detail::visibleWindows.Count(); i++)
				{
					GUIElements::PWindow w = Detail::visibleWindows[i];
					if (w->canBeImplicitlyClosed)
					{
						Hide(w);
						i--;
						rs = true;
					}
				}
				VerifyIntegrity(CLOCATION);
				return rs;
			}
			else
			{
				foreach (Detail::visibleDialogs,_d)
				{
					PDialog d = *_d;
					if (d->onPassword && d->sendNullPasswordOnAbortByUser && (!d->check || d->check() == CheckResult::OkayContinue))
					{
						d->onPassword(nullptr);
					}

				}
				Detail::visibleWindows.last()->Deactivate();
				Detail::visibleWindows.Clear();
				Detail::visibleDialogs.Clear();
			}
			VerifyIntegrity(CLOCATION);
			return true;
		}
		VerifyIntegrity(CLOCATION);
		return false;
	}




	void	Render()
	{
		foreach (Detail::visibleWindows,d)
		{
			(*d)->Render();
		}
	}


	PMenu				loginMenu;
	GUIElements::PWindow	serverChoice;


	void	Create()
	{
		Detail::showTutorials = !FileSystem::IsFile(RuntimeConfig::ConfigFile("tutorialsDisabled.mark"));
		GUIElements::Create();


		loginMenu.reset(new Menu(0.25f,"Login"));
		loginMenu->canBeImplicitlyClosed = false;	//once visible, must explicitly close

		serverChoice.reset(new GUIElements::Window());
		serverChoice->SetCenteredWidth(0.9f,GUIElements::TAnchor::RelativeToScreen);
		serverChoice->canBeImplicitlyClosed = false;


	}


	void	CheckServersAreShown(bool doShow)
	{
		VerifyIntegrity(CLOCATION);

		if (doShow)
		{
			ShowBottom(serverChoice);
		}
		else
			Hide(serverChoice);
		VerifyIntegrity(CLOCATION);
	}


	void	Destroy()
	{
		loginMenu.reset();
		Detail::visibleWindows.Clear();
		Detail::pendingTutorials.Clear();
		GUIElements::Destroy();
	}

	bool	ShowingFactions()
	{
		return IsVisible( Detail::factionDialog ) ;
	}




}


#endif

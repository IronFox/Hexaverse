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

			GUIElements::PButton confirmButton;
			GUIElements::PButton cancelButton;
			GUIElements::PInput	inputField;
			GUIElements::PImage	imageField;


			if (componentFlags & ComponentFlags::Input)
			{
				inputField.reset(new GUIElements::Input());
				inputField->anchorConfig.Left().Set(0.f);
				inputField->anchorConfig.Right().Set(0.f);
				inputField->anchorConfig.Top().Set(textField,0.f);//0.025f);
				textPanel->components.Append (inputField);
			}

			if (componentFlags & ComponentFlags::ConfirmButton)
			{
				confirmButton.reset(new GUIElements::Button());
				confirmButton->anchorConfig.Left().Set(0.f);
				confirmButton->anchorConfig.Right().Set(0.f);
				confirmButton->anchorConfig.Top().Set(textPanel,0.015f,false);
				dialog->components << confirmButton;
			}
			if (componentFlags & ComponentFlags::CancelButton)
			{
				cancelButton.reset(new GUIElements::Button());

				if (confirmButton)
					confirmButton->anchorConfig.Right().Set(0.505f);
				cancelButton->anchorConfig.Left().Set(0.505f);
				cancelButton->anchorConfig.Right().Set(0.f);
				cancelButton->anchorConfig.Top().Set(textPanel,0.015f,false);
				dialog->components << cancelButton;

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
					imageField->autoSetHeight = false;
					imageField->anchorConfig.Bottom().Set(confirmButton,0.03f,false);
					//imageField->autoSetWidth = true;
				}
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
			dialog->confirmButton = confirmButton;
			dialog->userInput = inputField;
			dialog->textField = textField;
			dialog->image = imageField;

			
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

				confirmButton->SetText(confirmWord);
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
				cancelButton->SetText("Cancel");
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


	void		Dialog::DumpMe()
	{
//		reLayout.swap(std::function<void(Dialog&)>());
		components.Clear();
		confirmButton.reset();
		userInput.reset();
		textField.reset();
		image.reset();

		Deactivate();
		Detail::visibleWindows.FindAndErase(shared_from_this());
		Detail::visibleDialogs.FindAndErase(shared_from_this());
		if (Detail::visibleWindows.IsNotEmpty())
			Detail::visibleWindows.last()->Activate();
	}

	void		Dialog::Confirm(bool bySpace)
	{
		if (confirmButton && !confirmButton->IsEnabled())
			return;

		bool tut = isTutorial;
		if (!fetchInput)
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
			fetchInput(userInput->GetInput());
			DumpMe();
		}

		if (tut && Detail::pendingTutorials.IsNotEmpty() && Detail::visibleWindows.IsEmpty())
		{
			ShowNextTutorial();
		}
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
		if (Detail::visibleWindows.IsEmpty())
			return false;
		PDialog d = To<Dialog>::DynamicCast(Detail::visibleWindows.last());
		if (d)
		{
			d->Confirm(bySpace);
			return true;
		}
		return false;
	}


	bool	HasMoreTutorials()
	{
		return Detail::pendingTutorials.IsNotEmpty();

	}


	
	void	ShowNextTutorial()
	{
		if (Detail::pendingTutorials.IsNotEmpty())
		{
			Show(Detail::pendingTutorials.Peek());
			Detail::pendingTutorials.EraseFront();
		}
	
		OnTutorialsChanged();
	
	}

	WDialog	Query(const String&query, const String&initial, const String&allowChars, const PCheckInput&check, const PFetchInput&fetch)
	{
		GUIElements::Input::onChange = Detail::OnInput;
		GUIElements::Input::onDone = Detail::OnDone;


		PDialog d = Detail::NewModalDialog(Detail::ComponentFlags::ConfirmButton | Detail::ComponentFlags::CancelButton | Detail::ComponentFlags::Input, "OK", false);
		d->textField->SetText(query);

		ASSERT__(!d->isTutorial);


		d->fetchInput = fetch;
		d->checkInput = check;

		d->userInput->SetCurrent(initial);
		d->userInput->SetAllowed(allowChars);

		d->ReLayout();
		return d;
	}

	String	WordForThinking()
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

		return words[random.getIndex(ARRAYSIZE(words)-1)];
	}


	
	bool	Hide(const GUIElements::PWindow&window)
	{
		index_t index = Detail::visibleWindows.GetIndexOf(window);
		if (index == InvalidIndex)
			return false;

		Detail::visibleWindows.Erase(index);

		PDialog d = To<Dialog>::DynamicCast(window);
		if (d)
			Detail::visibleDialogs.FindAndErase(d);

		if (index == Detail::visibleWindows.Count())
		{
			window->Deactivate();
			if (Detail::visibleDialogs.IsNotEmpty())
				Detail::visibleDialogs.last()->Activate();
		}


		SignalSituationChange();

		return true;

	}

	bool	IsVisible(const GUIElements::PWindow&window)
	{
		return Detail::visibleWindows.Contains(window);
	}

	void	Show(const GUIElements::PWindow&window)
	{
		if (Detail::visibleWindows.IsNotEmpty() && Detail::visibleWindows.last() == window)
			return;
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


		SignalSituationChange();

	}



	void	ShowBottom(const GUIElements::PWindow&window)
	{
		if (Detail::visibleWindows.IsNotEmpty() && Detail::visibleWindows.first() == window)
			return;
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

		SignalSituationChange();

	}



	WDialog	ShowFactions(const Buffer0<Faction>&factions)
	{
		if (Detail::factionDialog)
			RemoveDialog(Detail::factionDialog);


		PDialog d = Detail::NewModalDialog(Detail::ComponentFlags::ConfirmButton, "Close", false);

		Detail::factionDialog = d;

		Array<String> list(factions.Count()+1);
		list.first() = "Known factions:";
		for (index_t i = 0; i < factions.Count(); i++)
			list[i+1] = "* "+factions[i].name+(factions[i].online?
			(factions[i].requestedNewRound ? "" : " ("+WordForThinking()+")")
			:"   -    offline");

		d->textField->SetText( list );
		d->textField->alignCenter = false;
		d->textField->zoom = 0.9f;
		d->ReLayout();

		return d;
	}

	bool	RemoveTutorial(const PDialog&dialog)
	{
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
		return true;
	}


	bool	RemoveTutorial(const String&key)
	{
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

		return true;
	}

	bool	RemoveDialog(const PDialog&dialog)
	{
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
		return true;
	}

	WDialog	EnqueueTutorial(const String&key, const Array<String>&text, const String&imageName /*= ""*/, const Rect<>&screenReference /*= Rect<>::Invalid*/)
	{
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
					Magic::LoadFromFile(image,"resources/tutorial/"+imageName);
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

		OnTutorialsChanged();

		d->ReLayout();
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
		for (index_t i = 0; i < Detail::visibleDialogs.Count(); i++)
		{
			if (!Detail::visibleDialogs[i]->SignalChange())
			{
				Detail::visibleWindows.FindAndErase(Detail::visibleDialogs[i]);
				Detail::visibleDialogs.Erase(i);
				i--;
				continue;
			}
		}

		foreach (Detail::visibleWindows,w)
			(*w)->DetectEnabledState();
	}



	size_t	CharLength(char c)
	{
		return (size_t)textout.getFont().getWidth(c);
	}

	/**
	Creates a new modal dialog on top of any other active modal dialogs
	*/
	WDialog	Confirm(const String&query, const POnConfirm&onConfirm, const PCheckState&check)
	{
		CheckResult ck = check();
		if (ck == CheckResult::Abort)
			return PDialog();



		PDialog d = Detail::NewModalDialog(Detail::ComponentFlags::ConfirmButton| Detail::ComponentFlags::CancelButton, "Confirm", false);

		d->check = check;
		d->onConfirm = onConfirm;
		d->textField->SetText( query );

		d->ReLayout();
		return d;
	}

	WDialog	PermanentMessage(const String&message, const PCheckState&check)
	{
		CheckResult ck = check();
		if (ck == CheckResult::Abort)
			return PDialog();



		PDialog d = Detail::NewModalDialog(0, "", false);

		d->check = check;
		d->textField->SetText( message );

		d->ReLayout();
		return d;
	}


	/**
	Creates a new modal dialog on top of any other active modal dialogs that simply shows a message
	*/
	WDialog	ShowMessage(const String&msg)
	{


		PDialog d = Detail::NewModalDialog(Detail::ComponentFlags::ConfirmButton, "Got it", false);

		d->textField->SetText( msg );

		d->ReLayout();

		return d;
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
				return rs;
			}
			else
			{
				Detail::visibleWindows.last()->Deactivate();
				Detail::visibleWindows.Clear();
				Detail::visibleDialogs.Clear();
			}
			return true;
		}
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
		Detail::showTutorials = !FileSystem::IsFile("config/tutorialsDisabled.mark");
		GUIElements::Create();


		loginMenu.reset(new Menu(0.25f));
		loginMenu->canBeImplicitlyClosed = false;	//once visible, must explicitly close

		serverChoice.reset(new GUIElements::Window());
		serverChoice->SetCenteredWidth(0.9f,false);
		serverChoice->canBeImplicitlyClosed = false;


	}


	void	CheckServersAreShown(bool doShow)
	{

		if (doShow)
		{
			ShowBottom(serverChoice);
		}
		else
			Hide(serverChoice);
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
		return !!Detail::factionDialog;
	}




}


#endif

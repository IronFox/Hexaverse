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

		

		//class ModalDialog
		//{
		//public:
		//	CONSTRUCT_ENUMERATION5(Type, Confirmation, Message, TutorialMessage, StringQuery,FactionList);


		//	Type					type;

		//	GUIElements::TextField	textField,
		//							confirmButton,
		//							cancelButton;
		//	GUIElements::Input		inputField;
		//	GUIElements::ImageField	imageField;
		//	GUIElements::Area		common;
		//	String					key;


		//	PCheckState			check;
		//	POnConfirm			confirm;
		//	PCheckInput			checkInput;
		//	PFetchInput			fetchInput;

		//	Rect<>				totalSize;
		//	bool				canConfirmNow = true;


		//	void		adoptData(ModalDialog&other)
		//	{
		//		key.adoptData(other.key);
		//		imageField.adoptData(other.imageField);
		//		textField.adoptData(other.textField);
		//		confirmButton.adoptData(other.confirmButton);
		//		cancelButton.adoptData(other.cancelButton);
		//		inputField.adoptData(other.inputField);
		//		common.adoptData(other.common);
		//		check.swap(other.check);
		//		confirm.swap(other.confirm);
		//		checkInput.swap(other.checkInput);
		//		fetchInput.swap(other.fetchInput);
		//		canConfirmNow = other.canConfirmNow;
		//		totalSize = other.totalSize;
		//		type = other.type;
		//	}

		//	void		Activate()
		//	{
		//		if (HasInput())
		//			inputField.ActivateInput();
		//		Refresh();
		//	}
		//	void		Deactivate()
		//	{
		//		if (HasInput())
		//			inputField.DeactivateInput();
		//	}

		//	void		Refresh();


		//	bool		SignalChange()
		//	{
		//		if (type == Type::Confirmation)
		//		{
		//			CheckResult rs = check();
		//			if (rs == CheckResult::Abort)
		//				return false;
		//			canConfirmNow = rs == CheckResult::OkayContinue;
		//			confirmButton.SetText( rs == CheckResult::OkayContinue ? "Confirm" : "Confirm (not right now)");
		//		}
		//		return true;
		//	}

		//	bool		HasCancelButton()	const
		//	{
		//		return type == Type::Confirmation;// || type == Type::Message;
		//	}

		//	bool		HasInput() const
		//	{
		//		return type == Type::StringQuery;
		//	}


		//	void		Render()
		//	{
		//		if (textField.UpdateIfNecessary()
		//			|	//binary to evaluate all three
		//			confirmButton.UpdateIfNecessary()
		//			|
		//			cancelButton.UpdateIfNecessary()
		//			|
		//			inputField.UpdateIfNecessary()
		//			)
		//		{
		//			//chicken-egg-dance
		//			confirmButton.MoveTopTo(textField.rect.y.min - 0.025f);
		//			cancelButton.MoveTopTo(textField.rect.y.min - 0.025f);

		//			totalSize = textField.rect;
		//			totalSize.Include(confirmButton.rect);
		//			
		//			if (HasCancelButton())
		//				totalSize.Include(cancelButton.rect);
		//			if (HasInput())
		//				totalSize.Include(inputField.rect);

		//			//float center = totalSize.y.center();
		//			float top = 0.5f + totalSize.y.GetExtend()/2.f;
		//			textField.MoveTopTo(top);

		//			if (HasInput())
		//			{
		//				inputField.MoveTopTo(textField.rect.y.min - 0.025f);

		//				confirmButton.MoveTopTo(inputField.rect.y.min - 0.025f);
		//				if (HasCancelButton())
		//					cancelButton.MoveTopTo(inputField.rect.y.min - 0.025f);
		//			}
		//			else
		//			{
		//				confirmButton.MoveTopTo(textField.rect.y.min - 0.025f);
		//				if (HasCancelButton())
		//					cancelButton.MoveTopTo(textField.rect.y.min - 0.025f);
		//			}

		//			totalSize = textField.rect;
		//			totalSize.Include(confirmButton.rect);
		//			if (HasCancelButton())
		//				totalSize.Include(cancelButton.rect);
		//			if (HasInput())
		//				totalSize.Include(inputField.rect);

		//			totalSize.Expand(0.015f);

		//			imageField.rect.y = textField.rect.y;

		//			Rect<> r = textField.rect;
		//			if (imageField.texture.IsNotEmpty())
		//				r.Include(imageField.rect);
		//			if (HasInput())
		//				r.Include(inputField.rect);
		//			common.rect = r;

		//			cancelButton.UpdateNow();
		//			confirmButton.UpdateNow();
		//			common.UpdateNow();
		//		}
		//		GUIElements::Darken(Rect<>(0,0,1,1),0.7f);
		//		GUIElements::Darken(totalSize);


		//		common.Render(true);

		//		textField.Render(true,false,false);

		//		imageField.Render(true,false);
		//		confirmButton.Render(canConfirmNow);
		//		if (HasCancelButton())
		//			cancelButton.Render(true);
		//		if (HasInput())
		//			inputField.Render(true);
		//	}

		//	void		HandleClick();
		//	void		Confirm(bool bySpace);

		//	void		DumpMe();
		//};
	

		
		Buffer0<PDialog,Swap>	modalDialogs;
		Queue<PDialog,Swap>		pendingTutorials;
		PDialog					factionDialog;
		bool		showTutorials = true;
	
		//CONSTRUCT_ENUMERATION5(ModalDialogType, Confirmation, Message, TutorialMessage, StringQuery,FactionList);

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

		//String ButtonName(ModalDialogType t)
		//{
		//	switch (t)
		//	{
		//		case ModalDialogType::Confirmation:
		//			return "Confirm";
		//		case ModalDialogType::FactionList:
		//			return "Close";
		//		case ModalDialogType::Message:
		//		case ModalDialogType::TutorialMessage:
		//			return "Got it";
		//		case ModalDialogType::StringQuery:
		//			return "OK";

		//	}
		//	return "Confirm";
		//}

		void Configure(PDialog dialog, UINT32 componentFlags, const String&confirmWord, bool isTutorial);

		PDialog	NewModalDialog(UINT32 componentFlags, const String&confirmWord, bool isTutorial)
		{
			if (modalDialogs.IsNotEmpty())
				modalDialogs.last()->Deactivate();
			PDialog dialog(new Dialog());
			modalDialogs<<dialog;
			Configure(dialog,componentFlags,confirmWord, isTutorial);
			dialog->Activate();
			return dialog;
		}

		void Configure(PDialog dialog, UINT32 componentFlags, const String&confirmWord, bool isTutorial)
		{
			dialog->isTutorial = isTutorial;

			GUIElements::PComponent	common(new GUIElements::Area());
			GUIElements::PTextField textField(new GUIElements::TextField);
			textField->rect.Set(0.25f,0.25f,0.75f,0.75f);
			
			GUIElements::PButton confirmButton;
			GUIElements::PButton cancelButton;
			GUIElements::PInput	inputField;
			GUIElements::PImage	imageField;
			if (componentFlags & ComponentFlags::ConfirmButton)
			{
				confirmButton.reset(new GUIElements::Button());
				confirmButton->rect.Set(0.25f,0.25f,0.75f,0.75f);
			}
			if (componentFlags & ComponentFlags::CancelButton)
			{
				cancelButton.reset(new GUIElements::Button());
				cancelButton->rect.Set(0.55f,0.25f,0.75f,0.75f);
			}
			if (componentFlags & ComponentFlags::Input)
				inputField.reset(new GUIElements::Input());
			if (componentFlags & ComponentFlags::Image)
			{
				imageField.reset(new GUIElements::Image());
				
				textField->rect.Set(0.405f,0.25f,0.75f,0.75f);
				imageField->rect.Set(0.25f,0.25f,0.4f,0.75f);
				textField->alignCenter = false;
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

			dialog->components << common;
			
			dialog->components << textField;
			if (confirmButton)
				dialog->components << confirmButton;
			if (cancelButton)
				dialog->components  << cancelButton;
			if (imageField)
				dialog->components  << imageField;
			if (inputField)
				dialog->components  << inputField;

			if (textField)
			{
				textField->sensitiveToMouse = false;
				textField->renderRect = false;
			}
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

			dialog->reLayout = [common,textField,confirmButton,cancelButton,inputField,imageField](Dialog&dialog)
			{
				//chicken-egg-dance
				if (confirmButton)
					confirmButton->MoveTopTo(textField->rect.y.min - 0.025f);
				if (cancelButton)
					cancelButton->MoveTopTo(textField->rect.y.min - 0.025f);


				dialog.totalSize = textField->rect;
				for (const auto&c : dialog.components)
					if (c != textField && c != common && c != imageField)
						dialog.totalSize.Include(c->rect);

				//float center = totalSize.y.center();
				float top = 0.5f + dialog.totalSize.y.GetExtend()/2.f;
				textField->MoveTopTo(top);

				if (inputField)
				{
					inputField->MoveTopTo(textField->rect.y.min - 0.025f);

					if (confirmButton)
						confirmButton->MoveTopTo(inputField->rect.y.min - 0.025f);
					if (cancelButton)
						cancelButton->MoveTopTo(inputField->rect.y.min - 0.025f);
				}
				else
				{
					if (confirmButton)
						confirmButton->MoveTopTo(textField->rect.y.min - 0.025f);
					if (cancelButton)
						cancelButton->MoveTopTo(textField->rect.y.min - 0.025f);
				}

				dialog.totalSize = textField->rect;
				for (const auto&c : dialog.components)
					if (c != textField && c != common && c != imageField)
						dialog.totalSize.Include(c->rect);

				dialog.totalSize.Expand(0.015f);

				if (imageField)
					imageField->rect.y = textField->rect.y;

				Rect<> r = textField->rect;
				if (imageField)
					r.Include(imageField->rect);
				if (inputField)
					r.Include(inputField->rect);
				common->rect = r;

				foreach (dialog.components,c)
					(*c)->UpdateNow();
			};

			dialog->reLayout(*dialog);

			//rs.type = type;
		}

	}
	void		Dialog::Refresh()
	{
		foreach (components, c)
			(*c)->Check();
	}


	void		Dialog::DumpMe()
	{
		reLayout.swap(std::function<void(Dialog&)>());
		components.Clear();
		confirmButton.reset();
		userInput.reset();
		textField.reset();
		image.reset();

		Deactivate();
		Detail::modalDialogs.FindAndErase(shared_from_this());
		if (Detail::modalDialogs.IsNotEmpty())
			Detail::modalDialogs.last()->Activate();
	}

	void		Dialog::Confirm(bool bySpace)
	{
		if (confirmButton && !confirmButton->enabled)
			return;

		if (!fetchInput)
		{
			if (onConfirm)
				onConfirm();
			DumpMe();
			if (isTutorial)
				OnTutorialsChanged();
		}
		else
		{
			if (bySpace)
				return;
			fetchInput(userInput->GetInput());
			DumpMe();
		}

		if (isTutorial && Detail::pendingTutorials.IsNotEmpty() && Detail::modalDialogs.IsEmpty())
		{
			Detail::modalDialogs << Detail::pendingTutorials.Peek();
			Detail::modalDialogs.last()->Activate();
			Detail::pendingTutorials.EraseFront();
		}
	}


	void		Dialog::HandleClick()
	{
		foreach (components,c)
		{
			(*c)->HandleClick();
			if (components.IsEmpty())	//self-destructed
				return;
		}
	}


	namespace Detail
	{
		void OnInput()
		{
			if (modalDialogs.IsNotEmpty())
				modalDialogs.last()->Refresh();
		}
		void OnDone()
		{
			ConfirmCurrent(false);
		}
	}


	bool	ConfirmCurrent(bool bySpace)
	{
		if (Detail::modalDialogs.IsEmpty())
			return false;
		Detail::modalDialogs.last()->Confirm(bySpace);
		return true;


	}


	bool	HasMoreTutorials()
	{
		return Detail::pendingTutorials.IsNotEmpty();

	}


	
	void	ShowNextTutorial()
	{
		if (Detail::pendingTutorials.IsNotEmpty())
		{
			Detail::modalDialogs << Detail::pendingTutorials.Peek();
			Detail::pendingTutorials.EraseFront();
		}
	
		OnTutorialsChanged();
	
	}

	WDialog	Query(const String&query, const String&initial, const String&allowChars, const PCheckInput&check, const PFetchInput&fetch)
	{
		textout.pushState();
		textout.setScale(1.f,1.f);

		GUIElements::Input::onChange = Detail::OnInput;
		GUIElements::Input::onDone = Detail::OnDone;


		PDialog d = Detail::NewModalDialog(Detail::ComponentFlags::ConfirmButton | Detail::ComponentFlags::CancelButton, "OK", false);
		d->textField->SetText(query);



		d->fetchInput = fetch;
		d->checkInput = check;

		d->userInput->SetCurrent(initial);
		d->userInput->SetAllowed(allowChars);

		d->reLayout(*d);

		textout.popState();
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



	WDialog	ShowFactions(const Buffer0<Faction>&factions)
	{
		if (Detail::factionDialog)
			RemoveDialog(Detail::factionDialog);

		textout.pushState();
		textout.setScale(1.f,1.f);


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
		d->reLayout(*d);

		textout.popState();
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
		index_t at = Detail::modalDialogs.GetIndexOf(dialog);
		if (at == InvalidIndex)
			return false;
		if (at == Detail::modalDialogs.Count()-1)
		{
			dialog->Deactivate();
			Detail::modalDialogs.EraseLast();
			if (Detail::modalDialogs.IsNotEmpty())
				Detail::modalDialogs.last()->Activate();
		}
		else
			Detail::modalDialogs.Erase(at);
		return true;
	}

	WDialog	EnqueueTutorial(const String&key, const Array<String>&text, const String&imageName /*= ""*/, const Rect<>&screenReference /*= Rect<>::Invalid*/)
	{
		if (!Detail::showTutorials)
			return PDialog();
		static StringTable<DX11::Texture,Strategy::Swap>	textureMap;


		textout.pushState();
		textout.setScale(1.f,1.f);

		PDialog&d = /*Detail::modalDialogs.IsEmpty() ? Detail::modalDialogs.Append() : */Detail::pendingTutorials.Push();
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

		textout.popState();
		OnTutorialsChanged();

		d->reLayout(*d);
		return d;
	}


	bool	HandleClick()
	{
		if (Detail::modalDialogs.IsNotEmpty())
		{
			Detail::modalDialogs.last()->HandleClick();
			return true;
		}
		return false;
	}


	



	/**
	Signals that the global situation has changed and active modal dialogs need to be re-evalutated.
	This may be invoked by a server state change or removal of some entity
	*/
	void	SignalSituationChange()
	{
		for (index_t i = 0; i < Detail::modalDialogs.Count(); i++)
		{
			if (!Detail::modalDialogs[i]->SignalChange())
			{
				Detail::modalDialogs.Erase(i);
				i--;
				continue;
			}

		}
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

		textout.pushState();
		textout.setScale(1.f,1.f);


		PDialog d = Detail::NewModalDialog(Detail::ComponentFlags::ConfirmButton| Detail::ComponentFlags::CancelButton, "Confirm", false);

		d->check = check;
		d->onConfirm = onConfirm;
		d->textField->SetText( query );

		d->reLayout(*d);
		textout.popState();
		return d;
	}

	WDialog	PermanentMessage(const String&message, const PCheckState&check)
	{
		CheckResult ck = check();
		if (ck == CheckResult::Abort)
			return PDialog();

		textout.pushState();
		textout.setScale(1.f,1.f);


		PDialog d = Detail::NewModalDialog(0, "", false);

		d->check = check;
		d->textField->SetText( message );

		d->reLayout(*d);
		textout.popState();
		return d;
	}


	/**
	Creates a new modal dialog on top of any other active modal dialogs that simply shows a message
	*/
	WDialog	ShowMessage(const String&msg)
	{
		textout.pushState();
		textout.setScale(1.f,1.f);


		PDialog d = Detail::NewModalDialog(Detail::ComponentFlags::ConfirmButton, "Got it", false);

		d->textField->SetText( msg );

		d->reLayout(*d);
		textout.popState();

		return d;
	}


	/**
	Checks if any modal dialogs are open at this time
	*/
	bool	IsModal()
	{
		return Detail::modalDialogs.IsNotEmpty();
	}

	/**
	Closes all modal dialogs
	*/
	void	CloseAllModal()
	{
		if (Detail::modalDialogs.IsNotEmpty())
			Detail::modalDialogs.last()->Deactivate();
		Detail::modalDialogs.Clear();
	}




	void	Render()
	{
		foreach (Detail::modalDialogs,d)
		{
			(*d)->Render();
		}
	}


	void	Create()
	{
		Detail::showTutorials = !FileSystem::IsFile("config/tutorialsDisabled.mark");
		GUIElements::Create();
	}


	void	Destroy()
	{
		Detail::modalDialogs.Clear();
		Detail::pendingTutorials.Clear();
		GUIElements::Destroy();
	
	}

	bool	ShowingFactions()
	{
		return !!Detail::factionDialog;
	}




}


#endif

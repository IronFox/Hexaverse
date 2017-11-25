#include <global_root.h>
#include "dialog.h"
#include <engine/display.h>
#include "common.h"
#include <container/buffer.h>
#include "surfaceRenderer.h"
#include <engine/aspect.h>
#include <image/converter/magic.h>
#include <general/random.h>
#include <cast.h>
#include <logEvent.h>
#include "../../clientRuntimeConfig.h"
#include "../../audio/music.h"

namespace DX11 = Engine::DX11; 

extern Engine::Textout<DX11::TextureFont>	textout;
extern Engine::Display<Engine::DirectX11>	display;

void	OnTutorialsChanged();

Random random;


namespace HUD{
namespace Dialog
{
	

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
			WindowStack::Show(dialog);
			return dialog;
		}


		static void SetLeft(const HUD::Component::PComponent&c, float v)
		{
			auto cfg = c->GetAnchorConfig();
			cfg.GetHAxis().lower.Set(HUD::Component::Distance::OfParentSize( v));
			c->SetAnchorConfig(cfg);
		}
		static void SetRight(const HUD::Component::PComponent&c, float v)
		{
			auto cfg = c->GetAnchorConfig();
			cfg.GetHAxis().upper.Set(HUD::Component::Distance::OfParentSize( v));
			c->SetAnchorConfig(cfg);
		}

		void Configure(PDialog dialog, UINT32 componentFlags, const String&confirmWord, bool isTutorial)
		{
			dialog->isTutorial = isTutorial;

			if (dialog->GetAnchorConfig().GetHAxis().Stretches())
				dialog->Anchor().FillHorizontally(0.25f,0.25f);
			else
				dialog->SetWidth(0.5f);
			
			dialog->SetBackgroundTransparent();
			//HUD::Component::PComponent	common(new HUD::Component::Area());

			HUD::Component::PPanel	textPanel(new HUD::Component::Panel());
			HUD::Component::PLabel textField(new HUD::Component::Label());
			textPanel->AddComponent(textField);
			{
				auto cfg = textField->GetAnchorConfig();
				cfg.GetHAxis().Fill();
				cfg.GetVAxis().AnchorUpper(HUD::Component::Distance::Unknown);
				textField->SetAnchorConfig(cfg);
				textPanel->SetAnchorConfig(cfg);
			}
			textPanel->SetAutomaticHeight(true);
			dialog->AddComponent(textPanel);
			
			//height controlled by content

			HUD::Component::PButton confirmButton,cancelButton,noButton,helpButton;
			HUD::Component::PInput	inputField;
			HUD::Component::PPasswordInput passwordInputField;
			HUD::Component::PImage	imageField;
			Buffer0<std::pair<float,HUD::Component::PButton> > buttons;


			
			if (componentFlags & ComponentFlags::Input)
			{
				inputField.reset(new HUD::Component::Input());
				{
					auto cfg = inputField->GetAnchorConfig();
					cfg.GetHAxis().Fill();
					cfg.GetVAxis().AnchorUpper(HUD::Component::Distance::Unknown,textField);
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
				passwordInputField.reset(new HUD::Component::PasswordInput());
				{
					HUD::Component::AnchorConfig cfg;
					cfg.GetHAxis().Fill();
					cfg.GetVAxis().AnchorUpper(HUD::Component::Distance::Unknown,textField);
					passwordInputField->SetAnchorConfig(cfg);
				}
				textPanel->AddComponent (passwordInputField);
			}

			if (componentFlags & ComponentFlags::ConfirmButton)
			{
				confirmButton.reset(new HUD::Component::Button(confirmWord));
				confirmButton->Anchor().FillHorizontally().Beneath(textPanel,0.015f);
				dialog->AddComponent( confirmButton);
				buttons << std::make_pair(1, confirmButton);
			}
			if (componentFlags & ComponentFlags::HelpButton)
			{
				helpButton.reset(new HUD::Component::Button("?"));
				{
					HUD::Component::AnchorConfig cfg;
					cfg.GetVAxis().AnchorUpper(HUD::Component::Distance::Unknown,textPanel,HUD::Component::Distance::GameWindow(0.015f));
					helpButton->SetAnchorConfig(cfg);
				}
				dialog->AddComponent(helpButton);
				buttons << std::make_pair(0.25f, helpButton);
			}
			if (componentFlags & ComponentFlags::NoButton)
			{
				noButton.reset(new HUD::Component::Button("No"));
				{
					HUD::Component::AnchorConfig cfg;
					cfg.GetVAxis().AnchorUpper(HUD::Component::Distance::Unknown,textPanel,HUD::Component::Distance::GameWindow(0.015f));
					noButton->SetAnchorConfig(cfg);
				}
				dialog->AddComponent( noButton);
				buttons << std::make_pair(1, noButton);
			}
			if (componentFlags & ComponentFlags::CancelButton)
			{
				cancelButton.reset(new HUD::Component::Button("Cancel"));
				cancelButton->Anchor().FillHorizontally(HUD::Component::Distance::OfParentSize(0.505f),0).Beneath(textPanel,0.015f);
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
				imageField.reset(new HUD::Component::Image());
				

				{
					auto cfg = imageField->GetAnchorConfig();
					cfg.GetHAxis().Fill(0,HUD::Component::Distance::OfParentSize( 0.6f));
					cfg.GetVAxis().AnchorUpper(HUD::Component::Distance::Unknown,0.05f);
					if (confirmButton)
					{

						imageField->SetAutoHeight(true);
						cfg.GetVAxis().SetAutoCenter();
					}
					imageField->SetAnchorConfig(cfg);
				}
				textField->Anchor().FillHorizontally(HUD::Component::Distance::OfParentSize(0.405f),0).CenterVertically();
				textField->alignCenter = false;
				textPanel->AddComponent(imageField);
			}


			dialog->cancelButton = cancelButton;
			dialog->confirmButton = confirmButton;
			dialog->helpButton = helpButton;
			dialog->noButton = noButton;
			dialog->userInput = inputField;
			dialog->textField = textField;
			dialog->image = imageField;
			dialog->passwordInput = passwordInputField;

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


			dialog->ReLayout();

		}

	}






	void		Dialog::DumpMe()
	{
		if (!WindowStack::Hide(this))
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
		WindowStack::VerifyIntegrity(CLOCATION);
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

		if (nextTut && Detail::pendingTutorials.IsNotEmpty() && !WindowStack::IsModal())
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
		WindowStack::VerifyIntegrity(CLOCATION);
		HUD::Component::PWindow window = WindowStack::GetTopMostWindow();
		if (!window)
			return false;
		PDialog d = To<Dialog>::DynamicCast(window);
		if (d && d->CanBeConfirmed(bySpace))
		{
			d->Confirm(bySpace);
			WindowStack::VerifyIntegrity(CLOCATION);
			return true;
		}
		WindowStack::VerifyIntegrity(CLOCATION);
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
		WindowStack::VerifyIntegrity(CLOCATION);
		if (Detail::pendingTutorials.IsNotEmpty())
		{
			auto tut = Detail::pendingTutorials.Peek();
			tut->autoNextTutorial = autoNext;
			WindowStack::Show(tut);
			Detail::pendingTutorials.EraseFront();
			WindowStack::VerifyIntegrity(CLOCATION);
		}
	
		OnTutorialsChanged();
		WindowStack::VerifyIntegrity(CLOCATION);
	
	}

	WDialog	QueryPassword(const String&query, const FOnPassword&onPassword, const FCheckState&check, bool sendNullPasswordOnAbortByUser/*=false*/)
	{
		WindowStack::VerifyIntegrity(CLOCATION);

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
		WindowStack::VerifyIntegrity(CLOCATION);

		return d;
	}


	WDialog	Query(const String&query, const String&initial, const String&allowChars, const FCheckInput&check, const FFetchInput&fetch)
	{
		WindowStack::VerifyIntegrity(CLOCATION);
		PDialog d = Detail::NewModalDialog(Detail::ComponentFlags::ConfirmButton | Detail::ComponentFlags::CancelButton | Detail::ComponentFlags::Input, "OK", false);
		d->textField->SetText(query);

		ASSERT__(!d->isTutorial);


		d->fetchInput = fetch;
		d->checkInput = check;

		d->userInput->SetText(initial);
		d->userInput->SetAllowedAlphabet(allowChars);

		d->confirmButton->DetectEnabledState();

		d->ReLayout();
		WindowStack::VerifyIntegrity(CLOCATION);

		return d;
	}

	WDialog	CheckedQuery(const String&query, const String&initial, const String&allowChars, const FCheckState&check, const FFetchInput&fetch)
	{
		WindowStack::VerifyIntegrity(CLOCATION);

		PDialog d = Detail::NewModalDialog(Detail::ComponentFlags::ConfirmButton | Detail::ComponentFlags::CancelButton | Detail::ComponentFlags::Input, "OK", false);
		d->textField->SetText(query);

		ASSERT__(!d->isTutorial);


		d->fetchInput = fetch;
		d->check = check;

		d->userInput->SetText(initial);
		d->userInput->SetAllowedAlphabet(allowChars);

		d->confirmButton->DetectEnabledState();

		d->ReLayout();
		WindowStack::VerifyIntegrity(CLOCATION);

		return d;
	}



	




	bool	RemoveTutorial(const PDialog&dialog)
	{
		WindowStack::VerifyIntegrity(CLOCATION);
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
		WindowStack::VerifyIntegrity(CLOCATION);
		return true;
	}


	bool	RemoveTutorial(const String&key)
	{
		WindowStack::VerifyIntegrity(CLOCATION);
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
		WindowStack::VerifyIntegrity(CLOCATION);

		return true;
	}

	bool	RemoveDialog(const PDialog&dialog)
	{
		return WindowStack::Hide(dialog);
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
		WindowStack::VerifyIntegrity(CLOCATION);

		return d;
	}


	WDialog	EnqueueTutorial(const String&key, const Array<String>&text, const String&imageName /*= ""*/, bool prioritize/*=false*/)
	{
		WindowStack::VerifyIntegrity(CLOCATION);
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

		WindowStack::VerifyIntegrity(CLOCATION);
		return d;
	}



	//bool	HandleClick()
	//{
	//	if (Detail::visibleWindows.IsNotEmpty())
	//	{
	//		Detail::visibleWindows.Last()->HandleClick();
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
		WindowStack::VerifyIntegrity(CLOCATION);
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
		WindowStack::VerifyIntegrity(CLOCATION);
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
		WindowStack::VerifyIntegrity(CLOCATION);
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
		WindowStack::VerifyIntegrity(CLOCATION);
		return d;
	}

	WDialog	PermanentMessage(const String&message, const FCheckState&check)
	{
		WindowStack::VerifyIntegrity(CLOCATION);
		CheckResult ck = check();
		if (ck == CheckResult::Abort)
			return PDialog();



		PDialog d = Detail::NewModalDialog(0, "", false);
		d->canBeImplicitlyClosed = false;

		d->check = check;
		d->textField->SetText( message );

		d->ReLayout();
		WindowStack::VerifyIntegrity(CLOCATION);
		return d;
	}


	WDialog	ShowMessage(const String&msg, const FCheckState&check)
	{
		WindowStack::VerifyIntegrity(CLOCATION);


		PDialog d = Detail::NewModalDialog(Detail::ComponentFlags::ConfirmButton, "Got it", false);

		d->textField->SetText( msg );
		d->check = check;

		d->ReLayout();

		WindowStack::VerifyIntegrity(CLOCATION);
		return d;


	}

	/**
	Creates a new modal dialog on top of any other active modal dialogs that simply shows a message
	*/
	WDialog	ShowMessage(const String&msg)
	{
		return ShowMessage(msg,FCheckState());
	}



	void	Destroy()
	{
		Detail::pendingTutorials.Clear();
	}


	
	void	Create()
	{
		Detail::showTutorials = !FileSystem::IsFile(RuntimeConfig::ConfigFile("tutorialsDisabled.mark"));
		GlobalEvents::applicationTerminate << Destroy;
	}






}
}

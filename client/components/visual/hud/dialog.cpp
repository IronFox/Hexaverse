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
#include "../../network.h"
#include <objects/faction.h>

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
				DeleteButton = 0x4,
				Input = 0x8,
				Image = 0x10,
				NoButton = 0x20,
				HelpButton = 0x40,
				PasswordInput = 0x80,
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
			cfg.GetHAxis().SetExtent(HUD::Component::Distance::Disabled);
			c->SetAnchorConfig(cfg);
		}

		void Configure(PDialog dialog, UINT32 componentFlags, const String&confirmWord, bool isTutorial)
		{
			dialog->isTutorial = isTutorial;

			if (dialog->GetAnchorConfig().GetHAxis().Stretches())
				dialog->Anchor().FillHorizontally(0.25f,0.25f);
			else
				dialog->SetWidth(0.5f);
			
			dialog->SetBackground(false);
			dialog->SetEdges(false);
			//HUD::Component::PComponent	common(new HUD::Component::Area());

			HUD::Component::PPanel	textPanel(new HUD::Component::Panel());
			textPanel->SetEdges(true);
			textPanel->SetBackgroundTone((float3)0.6f);
			HUD::Component::PLabel textField(new HUD::Component::Label());
			textField->Anchor().FillHorizontally().Top();
			textPanel->AddComponent(textField,CLOCATION);
			dialog->AddComponent(textPanel,CLOCATION);
			
			//height controlled by content

			HUD::Component::PButton confirmButton,cancelButton,deleteButton,noButton,helpButton;
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
				textPanel->AddComponent (inputField,CLOCATION);
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
				textPanel->AddComponent (passwordInputField,CLOCATION);
			}

			if (componentFlags & ComponentFlags::ConfirmButton)
			{
				HUD::Component::PButton b (new HUD::Component::Button(confirmWord));
				buttons << std::make_pair(1, b);
				confirmButton = b;
			}
			if (componentFlags & ComponentFlags::HelpButton)
			{
				HUD::Component::PButton b (new HUD::Component::Button("?"));
				buttons << std::make_pair(0.25f, b);
				helpButton = b;
			}
			if (componentFlags & ComponentFlags::NoButton)
			{
				HUD::Component::PButton b (new HUD::Component::Button("No"));
				buttons << std::make_pair(1, b);
				noButton = b;
			}
			if (componentFlags & ComponentFlags::DeleteButton)
			{
				HUD::Component::PButton b (new HUD::Component::Button("Delete"));
				buttons << std::make_pair(1, b);
				deleteButton = b;
			}
			if (componentFlags & ComponentFlags::CancelButton)
			{
				HUD::Component::PButton b (new HUD::Component::Button("Cancel"));
				buttons << std::make_pair(1, b);
				cancelButton = b;
			}

			{
				float total = 0.f;
				foreach (buttons,b)
				{
					b->second->Anchor().Beneath(textPanel,0.015f);
					dialog->AddComponent(b->second,CLOCATION);
					total += b->first;
				}
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
					cfg.GetHAxis().Fill(0.01,HUD::Component::Distance::OfParentSize( 0.6f));
					cfg.GetVAxis().AnchorUpper(HUD::Component::Distance::Unknown,0.05f);
					if (confirmButton)
					{

						imageField->SetAutoHeight(true);
						cfg.GetVAxis().SetAutoCenter();
					}
					imageField->SetAnchorConfig(cfg);
				}
				textField->Anchor().FillHorizontally(HUD::Component::Distance::OfParentSize(0.405f),0).CenterVertically();
				textField->SetAlign(HUD::Component::HorizontalTextAlign::Left);
				textPanel->AddComponent(imageField,CLOCATION);
			}


			dialog->cancelButton = cancelButton;
			dialog->confirmButton = confirmButton;
			dialog->deleteButton = deleteButton;
			dialog->helpButton = helpButton;
			dialog->noButton = noButton;
			dialog->userInput = inputField;
			dialog->textField = textField;
			dialog->image = imageField;
			dialog->passwordInput = passwordInputField;

			if (confirmButton)
			{

//				confirmButton->SetText(confirmWord);
				WDialog wd = dialog;
				confirmButton->SetCheck([wd, inputField]()->bool
				{
					auto d = wd.lock();
					if (!d)
						return false;
					bool canConfirmNow = !d->check || d->check() == CheckResult::OkayContinue;
					if (d->checkInput && inputField)
					{
						if (!d->checkInput(inputField->GetTextRef()))
							canConfirmNow = false;
					}
					return canConfirmNow;
				});
				confirmButton->SetTriggerFunction([wd]
				{
					auto d = wd.lock();
					if (!d)
						return;
					d->Confirm(false);
				});
			}

			if (cancelButton)
				cancelButton->SetTriggerFunction(std::bind(&Dialog::DumpMe,dialog.get()));

			if (deleteButton)
			{
				WDialog wd = dialog;

				deleteButton->SetTriggerFunction([wd]()
				{
					auto d = wd.lock();
					if (!d)
						return;
					if (d->onDelete)
						d->onDelete();
					d->DumpMe();
				});
			}

			switch (buttons.Count())
			{
				case 0:
					break;
				case 1:
					textPanel->SetCutCorners(M::Quad<bool>(true,false,true,true));
					buttons.Last().second->SetCutCorners(M::Quad<bool>(true,true,true,false));
					break;
				default:
					textPanel->SetCutCorners(M::Quad<bool>(true,false,true,true));
					buttons.First().second->SetCutCorners(M::Quad<bool>(true,true,false,false));
					buttons.Last().second->SetCutCorners(M::Quad<bool>(false,true,true,false));

					for (index_t i = 1; i+1 < buttons.Count(); i++)
						buttons[i].second->SetCutCorners(false);
					break;
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
				fetchInput(userInput->GetTextUTF8());
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


	WDialog	DeleteQuery(const String&query, const String&initial, const Engine::Keyboard::FAllowed&allowChars, const FCheckState&check, const FFetchInput&fetch, const FOnDelete&onDelete)
	{
		WindowStack::VerifyIntegrity(CLOCATION);
		PDialog d = Detail::NewModalDialog(Detail::ComponentFlags::ConfirmButton | Detail::ComponentFlags::CancelButton | Detail::ComponentFlags::Input | Detail::ComponentFlags::DeleteButton, "OK", false);
		d->textField->SetText(query);

		ASSERT__(!d->isTutorial);


		d->fetchInput = fetch;
		d->check = check;
		d->onDelete = onDelete;

		d->userInput->SetText(initial);
		d->userInput->SetAllowed(allowChars);

		d->confirmButton->DetectEnabledState();

		d->ReLayout();
		WindowStack::VerifyIntegrity(CLOCATION);

		return d;
	}

	WDialog	Query(const String&query, const String&initial, const Engine::Keyboard::FAllowed&allowChars, const FCheckInput&check, const FFetchInput&fetch)
	{
		WindowStack::VerifyIntegrity(CLOCATION);
		PDialog d = Detail::NewModalDialog(Detail::ComponentFlags::ConfirmButton | Detail::ComponentFlags::CancelButton | Detail::ComponentFlags::Input, "OK", false);
		d->textField->SetText(query);

		ASSERT__(!d->isTutorial);


		d->fetchInput = fetch;
		d->checkInput = check;

		d->userInput->SetText(initial);
		d->userInput->SetAllowed(allowChars);

		d->confirmButton->DetectEnabledState();

		d->ReLayout();
		WindowStack::VerifyIntegrity(CLOCATION);

		return d;
	}

	WDialog	CheckedQuery(const String&query, const String&initial, const Engine::Keyboard::FAllowed&allowChars, const FCheckState&check, const FFetchInput&fetch)
	{
		WindowStack::VerifyIntegrity(CLOCATION);

		PDialog d = Detail::NewModalDialog(Detail::ComponentFlags::ConfirmButton | Detail::ComponentFlags::CancelButton | Detail::ComponentFlags::Input, "OK", false);
		d->textField->SetText(query);

		ASSERT__(!d->isTutorial);


		d->fetchInput = fetch;
		d->check = check;

		d->userInput->SetText(initial);
		d->userInput->SetAllowed(allowChars);

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


	static DX11::Texture	GetTextureByName(const String&imageName)
	{
		if (imageName.IsEmpty())
			return DX11::Texture();
			
		static StringTable<DX11::Texture, Strategy::Swap>	textureMap;

		DX11::Texture*tex = textureMap.QueryPointer(imageName);
		if (tex)
			return *tex;

		try
		{
			Image image;
			Magic::LoadFromFile(image, FileSystem::StringToPath("resources/tutorial/" + imageName));
			DX11::Texture setTexture;
			setTexture.Load(image);
			textureMap.Set(imageName, setTexture);
			return setTexture;
		}
		catch (const std::exception&e)
		{
			LogEvent(EventClass::Preset::internalError, e.what());
			DBG_FATAL__(e.what());
		}
		return DX11::Texture();
	}

	PDialog	CreateTutorialMessage(const String&key, const ArrayRef<const String>&text, const String&imageName /*= ""*/)
	{

		


		PDialog d = Dialog::Create();

		const auto setTexture = GetTextureByName(imageName);
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

	Component::WWindow	SelectAlliedFaction(const String&query, const FOnFactionSelected&onFactionSelected, const FCheckState&check, bool failIfModalWindowExists/*=false*/)
	{
		WindowStack::VerifyIntegrity(CLOCATION);
		CheckResult ck = check();
		if (ck == CheckResult::Abort)
			return Component::PWindow{};
		if (!Objects::Faction::mine || Objects::Faction::mine->alliance->CountLivingMembers() <= 1)
			return Component::PWindow{};


		Component::PWindow rs{Component::Window::Create()};


		Component::PLabel label{new Component::Label(query)};
		label->FillHorizontally(0);
		label->AnchorTop(0);
		rs->AddComponent(label,CLOCATION);

		Component::PPanel panel{new Component::Panel()};
		rs->AddComponent(panel,CLOCATION);

		panel->FillHorizontally(0.005);
		
		Component::WWindow ww = rs;

		Component::PButton cancel{new Component::Button("Cancel",[ww]()
		{
			Component::PWindow w = ww.lock();
			if (w)
				WindowStack::Hide(w);
		})};
		rs->AddComponent(cancel,CLOCATION);

		cancel->FillHorizontally(0.005);
		panel->Anchor().Beneath(label,0.005);
		cancel->Anchor().Beneath(panel,0.005);
		
		Objects::Faction::mine->alliance->VisitMembers([panel,ww,onFactionSelected](const Objects::PFaction&f)
		{
			if (f == Objects::Faction::mine)
				return;
			Objects::WFaction wf = f;
			Component::PButton button{new Component::Button(f->GetName(),[wf,ww,onFactionSelected]()
			{
				Objects::PFaction f = wf.lock();
				Component::PWindow w = ww.lock();
				if (!f || !w)
					return;
				WindowStack::Hide(w);
				onFactionSelected(f);
			})};
			button->SetCutCorners(false);
			button->SetColor(f->color);
			button->FillHorizontally(0);
			button->AnchorTop(0);
			button->Anchor().BeneathLastOf(panel);
			panel->AddComponent(button,CLOCATION);
		});

		panel->SetAutoHeightSpacing(0.005);
		


		//panel->SetBackgroundColor(float4(1,0,0,1));
		panel->SetCutCorners(false);
		panel->SetAutomaticHeight(true);

		cancel->SetCutCorners(true,true,true,false);

		rs->SetWidth(0.75);
		rs->SetMaxAutoHeight(0.75);
		rs->SetAutomaticHeight(true);

		WindowStack::Show(rs);

		WindowStack::VerifyIntegrity(CLOCATION);
		return rs;
	}



	WDialog	Confirm(const String&query, const FOnConfirm&onConfirm, bool failIfModalWindowExists/*=false*/)
	{
		return Confirm(query,onConfirm,[](){return CheckResult::OkayContinue;},failIfModalWindowExists);
	}

	/**
	Creates a new modal dialog on top of any other active modal dialogs
	*/
	WDialog	Confirm(const String&query, const FOnConfirm&onConfirm, const FCheckState&check, bool failIfModalWindowExists/*=false*/)
	{
		if (failIfModalWindowExists && WindowStack::IsModal())
			return WDialog();

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

	WDialog	ShowImageMessage( const String&imageName, const String&msg, const FCheckState&check )
	{
		WindowStack::VerifyIntegrity(CLOCATION);

		const auto setTexture = GetTextureByName(imageName);
		PDialog d = Detail::NewModalDialog(Detail::ComponentFlags::ConfirmButton | Detail::ComponentFlags::Image , "Got it", false);
		d->image->SetTexture(setTexture);
		d->textField->SetText(msg);
		d->check = check;

		d->ReLayout();

		WindowStack::VerifyIntegrity(CLOCATION);
		return d;
	}

	WDialog	ShowMessage( const String&msg, const FCheckState&check )
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
	WDialog	ShowMessage( const String&msg )
	{
		return ShowMessage(msg,nullptr);
	}

	WDialog	ShowImageMessage( const String&imageName, const String&msg )
	{
		return ShowImageMessage(imageName,msg,nullptr);
	}

	void	Destroy()
	{
		Detail::pendingTutorials.Clear();
	}


	
	void	Create()
	{
		Detail::showTutorials = !FileSystem::IsFile(RuntimeConfig::myConfig.Lookup("tutorialsDisabled.mark"));
		GlobalEvents::applicationTerminate << Destroy;
	}






	void	QueryNickname()
	{
		if (RuntimeConfig::aiMode)
			return;
		//if (Network::chosenNickname.IsEmpty())
		{
			//static const String allowed = "abcdefghijklmnopqrstuvwxyzäöüABCDEFGHIJKLMNOPQRSTUVWXYZÄÖÜ0123456789_-";

			HUD::Dialog::Query("Please enter a nickname to continue (length 4-16)",::Network::chosenNickname,
				[](char32_t c){return c != ' ';},
				[](const UTF32StringRef&candidate)->bool
				{
					return Network::IsValidNickname(candidate);
				},
				[](const String&name)
				{
					StringFile userName;
					FileSystem::CreateDirectory(L"config");
					userName.Create(RuntimeConfig::GetUserNameFile());
					userName << name;
					Network::chosenNickname = name;
					if (Network::client.IsConnected())
					{
						Network::Channels::changeMyName.SendTo(Network::client,name);


					}
				}
			);
		}
	}

	/**
	Checks loaded nickname and queries a new one if none or an invalid is currently set.
	No check is performed if the client is configured to run in AI mode.
	@return true if a new name was queried, false otherwise. False indicates the currently set name is good, or AI mode is enabled
	*/
	bool	QueryNicknameIfEmpty()
	{
		if (RuntimeConfig::aiMode)
			return false;

		if (!Network::IsValidNickname(Network::chosenNickname))
		{
			QueryNickname();
			return true;
		}
		return false;
	}



}
}

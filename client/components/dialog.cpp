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

namespace DX11 = Engine::DX11; 

extern Engine::Textout<DX11::TextureFont>	textout;
extern Engine::Display<Engine::DirectX11>	display;

void	OnTutorialsChanged();

Random random;


namespace Dialog
{
	namespace Detail
	{

		

		class ModalDialog
		{
		public:
			CONSTRUCT_ENUMERATION5(Type, Confirmation, Message, TutorialMessage, StringQuery,FactionList);


			Type					type;

			GUIElements::TextField	textField,
									confirmButton,
									cancelButton;
			GUIElements::Input		inputField;
			GUIElements::ImageField	imageField;
			GUIElements::Area		common;
			String					key;


			PCheckState			check;
			POnConfirm			confirm;
			PCheckInput			checkInput;
			PFetchInput			fetchInput;

			Rect<>				totalSize;
			bool				canConfirmNow = true;


			void		adoptData(ModalDialog&other)
			{
				key.adoptData(other.key);
				imageField.adoptData(other.imageField);
				textField.adoptData(other.textField);
				confirmButton.adoptData(other.confirmButton);
				cancelButton.adoptData(other.cancelButton);
				inputField.adoptData(other.inputField);
				common.adoptData(other.common);
				check.swap(other.check);
				confirm.swap(other.confirm);
				checkInput.swap(other.checkInput);
				fetchInput.swap(other.fetchInput);
				canConfirmNow = other.canConfirmNow;
				totalSize = other.totalSize;
				type = other.type;
			}

			void		Activate()
			{
				if (HasInput())
					inputField.ActivateInput();
				Refresh();
			}
			void		Deactivate()
			{
				if (HasInput())
					inputField.DeactivateInput();
			}

			void		Refresh();


			bool		SignalChange()
			{
				if (type == Type::Confirmation)
				{
					CheckResult rs = check();
					if (rs == CheckResult::Abort)
						return false;
					canConfirmNow = rs == CheckResult::OkayContinue;
					confirmButton.SetText( rs == CheckResult::OkayContinue ? "Confirm" : "Confirm (not right now)");
				}
				return true;
			}

			bool		HasCancelButton()	const
			{
				return type == Type::Confirmation;// || type == Type::Message;
			}

			bool		HasInput() const
			{
				return type == Type::StringQuery;
			}


			void		Render()
			{
				if (textField.UpdateIfNecessary()
					|	//binary to evaluate all three
					confirmButton.UpdateIfNecessary()
					|
					cancelButton.UpdateIfNecessary()
					|
					inputField.UpdateIfNecessary()
					)
				{
					//chicken-egg-dance
					confirmButton.MoveTopTo(textField.rect.y.min - 0.025f);
					cancelButton.MoveTopTo(textField.rect.y.min - 0.025f);

					totalSize = textField.rect;
					totalSize.Include(confirmButton.rect);
					
					if (HasCancelButton())
						totalSize.Include(cancelButton.rect);
					if (HasInput())
						totalSize.Include(inputField.rect);

					//float center = totalSize.y.center();
					float top = 0.5f + totalSize.y.GetExtend()/2.f;
					textField.MoveTopTo(top);

					if (HasInput())
					{
						inputField.MoveTopTo(textField.rect.y.min - 0.025f);

						confirmButton.MoveTopTo(inputField.rect.y.min - 0.025f);
						if (HasCancelButton())
							cancelButton.MoveTopTo(inputField.rect.y.min - 0.025f);
					}
					else
					{
						confirmButton.MoveTopTo(textField.rect.y.min - 0.025f);
						if (HasCancelButton())
							cancelButton.MoveTopTo(textField.rect.y.min - 0.025f);
					}

					totalSize = textField.rect;
					totalSize.Include(confirmButton.rect);
					if (HasCancelButton())
						totalSize.Include(cancelButton.rect);
					if (HasInput())
						totalSize.Include(inputField.rect);

					totalSize.Expand(0.015f);

					imageField.rect.y = textField.rect.y;

					Rect<> r = textField.rect;
					if (imageField.texture.IsNotEmpty())
						r.Include(imageField.rect);
					if (HasInput())
						r.Include(inputField.rect);
					common.rect = r;

					cancelButton.UpdateNow();
					confirmButton.UpdateNow();
					common.UpdateNow();
				}
				GUIElements::Darken(Rect<>(0,0,1,1),0.7f);
				GUIElements::Darken(totalSize);


				common.Render(true);

				textField.Render(true,false,false);

				imageField.Render(true,false);
				confirmButton.Render(canConfirmNow);
				if (HasCancelButton())
					cancelButton.Render(true);
				if (HasInput())
					inputField.Render(true);
			}

			void		HandleClick();
			void		Confirm(bool bySpace);

			void		DumpMe();
		};
	

		
		Buffer0<ModalDialog,Adopt>	modalDialogs;
		Queue<ModalDialog,Adopt>  pendingTutorials;
		bool		showTutorials = true;
	

		ModalDialog&	NewModalDialog(ModalDialog::Type type)
		{
			if (modalDialogs.IsNotEmpty())
				modalDialogs.last().Deactivate();
			ModalDialog&rs = modalDialogs.Append();
			rs.type = type;
			rs.Activate();
			return rs;

		}

		void		ModalDialog::Refresh()
		{
			
			switch (type)
			{
				case Type::Confirmation:
				{
					if (check)
					{
						CheckResult rs = check();
						if (rs == CheckResult::Abort)
						{
							DumpMe();
						}
						else
							canConfirmNow = rs == CheckResult::OkayContinue;
					}
				}
				break;
				case Type::StringQuery:
				{
					if (checkInput)
						canConfirmNow = checkInput(inputField.GetInput());
				}
				break;
				default:
				{
				}
				break;
			}
		}



		void		ModalDialog::DumpMe()
		{
			Deactivate();
			modalDialogs.erase(this);
			if (modalDialogs.IsNotEmpty())
				modalDialogs.last().Activate();
		}

		void		ModalDialog::Confirm(bool bySpace)
		{
			bool checkTutorials = type == Type::TutorialMessage;
			if (type == Type::Confirmation)
			{
				CheckResult rs = check();
				if (rs == CheckResult::Abort)
				{
					DumpMe();
				}
				else
				{
					if (rs == CheckResult::OkayContinue)
					{
						confirm();
						DumpMe();
					}
				}
			}
			else
			{
				if (type != Type::StringQuery)
				{
					bool tut = type == Type::TutorialMessage;
					DumpMe();
					if (tut)
						OnTutorialsChanged();
				}
				else
				{
					if (bySpace)
						return;
					if (canConfirmNow)
					{
						fetchInput(inputField.GetInput());
						DumpMe();
					}
				}
			}

			if (checkTutorials && Detail::pendingTutorials.IsNotEmpty() && Detail::modalDialogs.IsEmpty())
			{
				Detail::modalDialogs.Append().adoptData(Detail::pendingTutorials.Peek());
				modalDialogs.last().Activate();
				Detail::pendingTutorials.EraseFront();
			}

		}


		void		ModalDialog::HandleClick()
		{
			bool checkTutorials = type == Type::TutorialMessage ;
			if (type != Type::TutorialMessage && cancelButton.ContainsMouse())
			{
				//if (type == Type::TutorialMessage)
				//{
				//	FileSystem::CreateDirectory("config");
				//	fclose(fopen("config/tutorialsDisabled.mark","w+"));
				//	DumpMe();
				//	pendingTutorials.Clear();
				//}

				DumpMe();
			}
			else
			{
				switch (type)
				{
					case Type::Confirmation:
					{
						CheckResult rs = check();
						if (rs == CheckResult::Abort)
						{
							DumpMe();
						}
						else
							if (confirmButton.ContainsMouse())
							{
								if (rs == CheckResult::OkayContinue)
								{
									confirm();
									DumpMe();
								}
							}
					}
					break;
					case Type::StringQuery:
					{
						
						if (confirmButton.ContainsMouse())
						{
							String input = inputField.GetInput();
							bool rs = checkInput(input);
							if (rs)
							{
								fetchInput(input);
								DumpMe();
							}
						}
					}
					break;
					default:
					{
						if (confirmButton.ContainsMouse())
						{
							bool tut = type == Type::TutorialMessage;
							DumpMe();
							if (tut)
								OnTutorialsChanged();
						}
					}
					break;
				}
			}
			if (checkTutorials && Detail::pendingTutorials.IsNotEmpty() && Detail::modalDialogs.IsEmpty())
			{
				Detail::modalDialogs.Append().adoptData(Detail::pendingTutorials.Peek());
				modalDialogs.last().Activate();
				Detail::pendingTutorials.EraseFront();
			}

		}


		void OnInput()
		{
			if (modalDialogs.IsNotEmpty())
				modalDialogs.last().Refresh();
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
		Detail::modalDialogs.last().Confirm(bySpace);
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
			Detail::modalDialogs.Append().adoptData(Detail::pendingTutorials.Peek());
			Detail::pendingTutorials.EraseFront();
		}
	
		OnTutorialsChanged();
	
	}

	void	Query(const String&query, const String&initial, const String&allowChars, const PCheckInput&check, const PFetchInput&fetch)
	{
		textout.pushState();
		textout.setScale(1.f,1.f);

		GUIElements::Input::onChange = Detail::OnInput;
		GUIElements::Input::onDone = Detail::OnDone;


		Detail::ModalDialog&d = Detail::NewModalDialog(Detail::ModalDialog::Type::StringQuery);



		d.fetchInput = fetch;
		d.checkInput = check;
		d.canConfirmNow = check(initial);

		d.imageField.rect.Set(0.25f,0.25f,0.25f,0.75f);
		d.textField.rect.Set(0.25f,0.25f,0.75f,0.75f);
		d.textField.SetText( query );
		d.confirmButton.rect.Set(0.25f,0.25f,0.75f,0.75f);
		d.confirmButton.SetText( "OK" );
		d.inputField.rect.Set(0.25f,0.25f,0.75f,0.75f);
		d.inputField.SetCurrent(initial);
		d.inputField.SetAllowed(allowChars);
		//d.cancelButton.rect.Set(0.55f,0.25f,0.75f,0.75f);
		//d.cancelButton.SetText( "Cancel" );

		textout.popState();


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

	void	ShowFactions(const Buffer0<Faction>&factions)
	{
		textout.pushState();
		textout.setScale(1.f,1.f);


		Detail::ModalDialog&d = Detail::NewModalDialog(Detail::ModalDialog::Type::FactionList);

		d.imageField.rect.Set(0.25f,0.25f,0.25f,0.75f);
		d.textField.rect.Set(0.25f,0.25f,0.75f,0.75f);

		Array<String> list(factions.Count()+1);
		list.first() = "Known factions:";
		for (index_t i = 0; i < factions.Count(); i++)
			list[i+1] = "* "+factions[i].name+(factions[i].online?
			(factions[i].requestedNewRound ? "" : " ("+WordForThinking()+")")
			:"   -    offline");

		d.textField.SetText( list );
		d.textField.alignCenter = false;
		d.textField.zoom = 0.9f;
		d.confirmButton.rect.Set(0.25f,0.25f,0.75f,0.75f);
		d.confirmButton.SetText( "Close");
		d.canConfirmNow = true;

		textout.popState();
	}



	void	RemoveTutorial(const String&key)
	{
		if (!Detail::showTutorials)
			return;

		bool found = true;
		foreach (Detail::pendingTutorials,t)
			if (t->key == key)
			{
				found = true;
				break;
			}
		if (!found)
			return;

		Queue<Detail::ModalDialog,Adopt>	temp;
		foreach (Detail::pendingTutorials,t)
			if (t->key != key)
				temp.Push().adoptData(*t);
		Detail::pendingTutorials.adoptData(temp);
	}

	void	EnqueueTutorial(const String&key, const Array<String>&text, const String&imageName /*= ""*/, const Rect<>&screenReference /*= Rect<>::Invalid*/)
	{
		if (!Detail::showTutorials)
			return;
		static StringTable<DX11::Texture,Strategy::Swap>	textureMap;


		textout.pushState();
		textout.setScale(1.f,1.f);

		Detail::ModalDialog&d = /*Detail::modalDialogs.IsEmpty() ? Detail::modalDialogs.Append() : */Detail::pendingTutorials.Push();
		d.type = Detail::ModalDialog::Type::TutorialMessage;

		if (imageName.IsNotEmpty())
		{
			DX11::Texture*tex = textureMap.QueryPointer(imageName);
			if (tex)
			{
				d.imageField.texture = *tex;
			}
			else
			{
				try
				{
					Image image;
					Magic::LoadFromFile(image,"resources/tutorial/"+imageName);
					d.imageField.texture.Load(image);
					textureMap.Set(imageName,d.imageField.texture);
				}
				catch (const std::exception&e)
				{
					FATAL__(e.what());
				}
			}
		}

		if (d.imageField.texture.IsEmpty())
		{
			d.textField.rect.Set(0.25f,0.25f,0.75f,0.75f);
		}
		else
		{
			d.textField.rect.Set(0.405f,0.25f,0.75f,0.75f);
			d.imageField.rect.Set(0.25f,0.25f,0.4f,0.75f);
			d.textField.alignCenter = false;
		}
		d.textField.SetText( text );
		d.confirmButton.rect.Set(0.25f,0.25f,0.75f,0.75f);
		d.confirmButton.SetText( "Got it" );
		d.cancelButton.rect.Set(0.55f,0.25f,0.75f,0.75f);
		d.key = key;
		//d.cancelButton.SetText( "Skip Tutorial" );

		textout.popState();
		OnTutorialsChanged();
	}


	bool	HandleClick()
	{
		if (Detail::modalDialogs.IsNotEmpty())
		{
			Detail::modalDialogs.last().HandleClick();
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
			if (!Detail::modalDialogs[i].SignalChange())
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
	void	Confirm(const String&query, const POnConfirm&onConfirm, const PCheckState&check)
	{
		CheckResult ck = check();
		if (ck == CheckResult::Abort)
			return;

		textout.pushState();
		textout.setScale(1.f,1.f);


		Detail::ModalDialog&d = Detail::NewModalDialog(Detail::ModalDialog::Type::Confirmation);

		d.check = check;
		d.confirm = onConfirm;
		d.canConfirmNow = ck == CheckResult::OkayContinue;

		d.imageField.rect.Set(0.25f,0.25f,0.25f,0.75f);
		d.textField.rect.Set(0.25f,0.25f,0.75f,0.75f);
		d.textField.SetText( query );
		d.confirmButton.rect.Set(0.25f,0.25f,0.45f,0.75f);
		d.confirmButton.SetText( ck == CheckResult::OkayContinue ? "Confirm" : "Confirm (not right now)" );
		d.cancelButton.rect.Set(0.55f,0.25f,0.75f,0.75f);
		d.cancelButton.SetText( "Cancel" );

		textout.popState();
	}

	/**
	Creates a new modal dialog on top of any other active modal dialogs that simply shows a message
	*/
	void	ShowMessage(const String&msg)
	{
		textout.pushState();
		textout.setScale(1.f,1.f);


		Detail::ModalDialog&d = Detail::NewModalDialog(Detail::ModalDialog::Type::Message);

		//d.check = check;
		//d.confirm = onConfirm;
		//d.canConfirmNow = ck == CheckResult::OkayContinue;

		d.imageField.rect.Set(0.25f,0.25f,0.25f,0.75f);
		d.textField.rect.Set(0.25f,0.25f,0.75f,0.75f);
		d.textField.SetText( msg );
		d.confirmButton.rect.Set(0.25f,0.25f,0.75f,0.75f);
		d.confirmButton.SetText( "Got it" );
		d.cancelButton.rect.Set(0.55f,0.25f,0.75f,0.75f);

		textout.popState();
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
			Detail::modalDialogs.last().Deactivate();
		Detail::modalDialogs.Clear();
	}




	void	Render()
	{
		foreach (Detail::modalDialogs,d)
		{
			d->Render();
		}
	}


	void	Create()
	{
		Detail::showTutorials = !FileSystem::IsFile("config/tutorialsDisabled.mark");
		GUIElements::Create();
	}


	void	Destroy()
	{
		GUIElements::Destroy();
	
	}

	bool	ShowingFactions()
	{
		foreach (Detail::modalDialogs,d)
			if (d->type == Detail::ModalDialog::Type::FactionList)
				return true;
		return false;
	}




}


#endif

#include "factionTable.h"
#include <runtimeConfig.h>
#include <objects/faction.h>
#include "../network.h"

namespace GUI
{
	namespace FactionTable
	{
		namespace Details
		{

			PWindow	window;
			PPanel	chatHistory,
					chatInput,
					factionTable,
					buttons;
			PInput	chatMessage;
			bool	visible=false;


			String	WordForThinking(const RuntimeConfig::RoundNumber&roundNumber, const void*factionPointer)
			{
				//return "Sed ut perspiciatis unde omnis iste natus error sit voluptatem accusantium doloremque laudantium, totam rem aperiam, eaque ipsa quae ab illo inventore veritatis et quasi architecto";


				static String words[] = {
					"Thinking",
					"Pretending to think",
					"Looking really closely at ... not the 'End Round' button",
					"Trying hard and long to be smarter than you. Mostly long",
					"Despairing",
					"Twiddling thumbs. Not pressing buttons you want them to",
					"Totally not hitting the 'End Round' button right now",
					"Trolling you",
					"One with the game. Not one with the 'End Round' button",
					"Gazing at the beautiful textures. Wait, what?",
					"Beyond the flow of time",
					"Please hold the line...",
				};
				CRC32::Sequence s;
				s.AppendPOD(roundNumber);
				s.AppendPOD(factionPointer);
		
				UINT32 random = s.Finish();

				return words[random % ARRAYSIZE(words)];
			}



			PPanel	AddPanel(const String&labelText)
			{
				PPanel rs{new Panel{}};

				rs->SetAutomaticHeight(true);
				rs->SetMaxAutoHeight(0.4f);
				
				if (labelText.IsNotEmpty())
				{
					PLabel label{new Label{labelText,0.75f}};
					rs->AddComponent(label);
					{
						auto mod = label->ModifyAnchorConfig();
						mod.Left().Set(0);
						mod.Top().Set(0);
					}
					label->LockToIdealWidth();
					label->LockToIdealHeight();
				}
				else
					rs->fillBackground = false;

				window->AnchorBelowPrevious(rs,0,0.01f);

				return rs;
			}


			void	Hide()
			{
				GUI::Hide(window);
			}

			void	Show()
			{
				GUI::Show(window);
			}


			void	AddFaction(const Objects::OwnerParameter&faction)
			{
				using namespace Details;
				PPanel panel{new Panel{}};
				PLabel nameLabel{new Label{faction->name}},statusLabel{new Label{faction->online ? (faction->requestedNewRound ? "" : WordForThinking(RuntimeConfig::roundNumber,faction.get())) : "<offline>" ,0.8f}};

				nameLabel->SetWidth(0.15f);
				nameLabel->ModifyAnchorConfig().Left().Set(0);
				nameLabel->alignCenter = false;
				nameLabel->SetPadding(0.2f);
				nameLabel->SetHorizontalPadding(1,1);
				if (!faction->online)
				{
					statusLabel->SetColor(float3(1),0.4); 
					nameLabel->SetColor(faction->color,0.4f);
				}
				else
				{
					statusLabel->SetColor(faction->color); 
					nameLabel->SetColor(faction->color);
				}
				statusLabel->ModifyAnchorConfig().Left().Set(nameLabel,0);
				statusLabel->ModifyAnchorConfig().Right().Set(0);
				statusLabel->ModifyAnchorConfig().Bottom().Set(0);
			
				statusLabel->SetPadding(0.2f);
				//textLabel->ModifyAnchorConfig().Top().Set(0);
				statusLabel->alignCenter = false;

				//timeLabel->fillBackground = true;
				//senderLabel->fillBackground = true;

				panel->SetAutomaticHeight(true);
				panel->AddComponent(nameLabel);
				//statusLabel->dbgMarked = true;
				panel->AddComponent(statusLabel);
				panel->fillBackground = false;

				factionTable->AnchorBelowPrevious(panel,0,0);
			}

			void	Update()
			{

				GUI::VerifyIntegrity(CLOCATION);

				if (!window)
				{
					window = Window::Create();
					window->SetWidth(0.5f);
					
					PPanel factionBack = AddPanel("Factions");
					PPanel chatBack = AddPanel("Chat History");
					chatInput = AddPanel("");
					buttons = AddPanel("");

					
					{
						chatHistory.reset(new Panel{});
						chatBack->AnchorBelowPrevious(chatHistory,0,0);
						chatHistory->SetMaxAutoHeight(0.3f);
						chatHistory->SetAutomaticHeight(true);
						chatHistory->fillBackground = false;
					}
					{
						factionTable.reset(new Panel{});
						factionBack->AnchorBelowPrevious(factionTable,0,0);
						factionTable->SetMaxAutoHeight(0.3f);
						factionTable->SetAutomaticHeight(true);
						factionTable->fillBackground = false;
					}
					{
						PButton close{new Button{"Close",Hide}};
						close->SetVerticalPadding(0.5f);
						close->FillHorizontally(0);
						buttons->AddComponent(close);
						close->ModifyAnchorConfig().Top().Set(0);
					}

					{
						chatMessage.reset(new Input{});
						chatMessage->SetAllowedAlphabet(Network::allowedChatCharacters);
						PButton send{new Button{"Send",[](){
							Network::SendChatMessage(chatMessage->GetText());
							chatMessage->SetText("");
						}}};
						send->SetVerticalPadding(0.5f);
						{
							auto cfg = send->ModifyAnchorConfig();
							cfg.Top().SetAuto();
							cfg.Bottom().SetAuto();
							cfg.Right().Set(0);
						}
						send->LockToIdealWidth();
						send->LockToIdealHeight();
						{
							auto cfg = chatMessage->ModifyAnchorConfig();
							cfg.Top().Set(0);
							cfg.Right().Set(send,0);
							cfg.Left().Set(0);
						}
						chatInput->AddComponent(chatMessage);
						chatInput->AddComponent(send);
						chatMessage->onEnter = [send](){send->Trigger();};
					}
				}
				factionTable->ClearComponents();
				Objects::ForeachFaction([](const Objects::PFaction&f)
				{
					AddFaction(f);
				});
				//PDialog d = Detail::NewModalDialog(Detail::ComponentFlags::ConfirmButton, "Close", false);

				//Detail::factionDialog = d;

				//Array<String> list(factions.Count()+1);
				//list.First() = "Known factions:";
				//for (index_t i = 0; i < factions.Count(); i++)
				//	list[i+1] = "* "+factions[i].name+(factions[i].online?
				//	(factions[i].requestedNewRound ? "" : " ("+WordForThinking(RuntimeConfig::roundNumber,factions[i].factionReference )+")")
				//	:"   -    offline");

				//d->textField->SetText( list );
				//d->textField->alignCenter = false;
				//d->textField->SetLocalFontScale(0.9f);
				//d->ReLayout();

				GUI::VerifyIntegrity(CLOCATION);

			}

		}

		void	Show()
		{
			Details::Update();
			Details::Show();
		}

		bool	IsVisible()
		{
			return GUI::IsVisible(Details::window);
		}

		void	Hide()
		{
			Details::Hide();
		}

		void	Update()
		{
			Details::Update();
		}


		void	ClearChatHistory()
		{
			Update();
			Details::chatHistory->ClearComponents();
		}

		void	SignalChatMessage(const Objects::OwnerParameter&owner, const String&msg)
		{
			Update();


			String timeStr;
			{
				time_t rawtime;
				struct tm * timeinfo;
				char buffer[80];

				time (&rawtime);
				timeinfo = localtime(&rawtime);

				strftime(buffer,sizeof(buffer),"%H:%M:%S",timeinfo);
				timeStr = buffer;
			}

			using namespace Details;
			PPanel chatEntry{new Panel{}};
			PLabel timeLabel{new Label{timeStr,0.75f}},senderLabel,textLabel{new Label{msg}};
			timeLabel->SetPadding(M::Quad<float>{0.5f,0,0.5f,0});
			timeLabel->LockToIdealWidth();
			//timeLabel->alignCenter = false;
			timeLabel->ModifyAnchorConfig().Bottom().Set(0);
			timeLabel->ModifyAnchorConfig().Right().Set(0);
			if (owner)
			{
				senderLabel.reset(new Label{owner->name+": "});
				senderLabel->SetColor(owner->color);
				senderLabel->LockToIdealWidth();
				senderLabel->ModifyAnchorConfig().Left().Set(0);
				//senderLabel->ModifyAnchorConfig().Top().Set(0);
				senderLabel->alignCenter = false;
				senderLabel->LockToIdealHeight();
				senderLabel->SetPadding(0);
				senderLabel->SetVerticalPadding(0.25f);
				senderLabel->SetHorizontalPadding(1,0);
				textLabel->SetColor(owner->color * 0.5f + 0.5f); 
				textLabel->ModifyAnchorConfig().Left().Set(senderLabel,0);
			}
			else
			{
				textLabel->SetColor(owner->color);
				textLabel->ModifyAnchorConfig().Left().Set(0);
			}
			textLabel->SetPadding(0);
			textLabel->SetVerticalPadding(0.25f);
			textLabel->ModifyAnchorConfig().Right().Set(timeLabel,0);
			//textLabel->ModifyAnchorConfig().Top().Set(0);
			textLabel->alignCenter = false;
			textLabel->LockToIdealHeight();

			//timeLabel->fillBackground = true;
			//senderLabel->fillBackground = true;

			chatEntry->SetAutomaticHeight(true);
			chatEntry->AddComponent(timeLabel);
			chatEntry->AddComponent(senderLabel);
			chatEntry->AddComponent(textLabel);
			chatEntry->fillBackground = false;

			chatHistory->AnchorBelowPrevious(chatEntry,0,0);

			window->FullAutoLayout(CLOCATION);

			chatHistory->ScrollToBottom();
		}

	}
}

#include "guiFactionTable.h"
#include <runtimeConfig.h>
#include <objects/faction.h>
#include "../../network.h"
#include "windowStack.h"
#include "dialog.h"

namespace HUD
{
	namespace FactionTable
	{
		namespace Details
		{
			using namespace Component;

			PWindow	window;
			PPanel	chatHistory,
					chatInput,
					factionTable,
					buttons;
			PInput	chatMessage;
			bool	visible=false;
			Objects::WAlliance	triedJoining;
			WButton triedJoiningButton;


			String	WordForThinking(const RuntimeConfig::RoundNumber&roundNumber, const void*factionPointer)
			{
				//return "Sed ut perspiciatis unde omnis iste natus error sit voluptatem accusantium doloremque laudantium, totam rem aperiam, eaque ipsa quae ab illo inventore veritatis et quasi architecto";
				return "Thinking, Sleeping, or AFK";

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
				rs->SetBorderBackground();

				if (labelText.IsNotEmpty())
				{
					PLabel label{new Label{labelText,0.75f}};
					rs->AddComponent(label);
					{
						auto mod = label->Anchor();
						mod.GetHAxis().AnchorLowerUnknown();
						mod.GetVAxis().AnchorUpperUnknown();
					}
					label->LockToIdealWidth();
					label->LockToIdealHeight();
				}
				else
					rs->SetBackgroundTransparent();

				window->AnchorBelowPrevious(rs,0,0.01f);
				rs->Anchor().FillHorizontally(0.01f,0.01f);

				return rs;
			}


			void	Hide()
			{
				WindowStack::Hide(window);
			}

			void	Show()
			{
				WindowStack::Show(window);
			}


			void	AddFaction(const PPanel&toPanel, const Objects::OwnerParameter&faction)
			{
				using namespace Details;
				PPanel panel{new Panel{}};
				PLabel nameLabel{new Label{faction->name}},statusLabel{new Label{faction->online ? (faction->requestedNewRound ? "" : WordForThinking(RuntimeConfig::roundNumber,faction.get())) : "<offline>" ,0.8f}};

				nameLabel->SetWidth(0.15f);
				nameLabel->Anchor().GetHAxis().AnchorLowerUnknown();
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
				statusLabel->Anchor().FillHBetween(nameLabel,nullptr).Bottom();
			
				statusLabel->SetPadding(0.2f);
				//textLabel->Anchor().Top().Set(0);
				statusLabel->alignCenter = false;

				//timeLabel->fillBackground = true;
				//senderLabel->fillBackground = true;

				panel->SetAutomaticHeight(true);
				panel->AddComponent(nameLabel);
				//statusLabel->dbgMarked = true;
				panel->AddComponent(statusLabel);
				panel->SetBackgroundTransparent();

				toPanel->AnchorBelowPrevious(panel,0,0);
			}

			static Objects::WAlliance tryingToJoin;

			static void DispatchJoinRequest(const Objects::PAlliance&a)
			{
				const bool cancel = tryingToJoin.lock() == a;
				Network::Channels::AllianceJoinRequest.SendTo(Network::client,Protocol::AllianceJoinRequest(a->id,cancel));
				tryingToJoin = cancel ? Objects::WAlliance() : a;
			}

			static String JoinText(const Objects::PAlliance&a)
			{
				const String req = tryingToJoin.lock() == a ? "Cancel request" : "Request";
				if (a->CountLivingMembers() == 1)
					return req +" to form alliance with "+a->GetMember(0)->GetName();
				return req +" to join "+a->id.ToNiceString();
			}

			void	Update()
			{

				WindowStack::VerifyIntegrity(CLOCATION);

				if (!window)
				{

					GlobalEvents::objectFlush << [](){triedJoining.reset();triedJoiningButton.reset();};
					GlobalEvents::onAllianceChange << [](const Objects::PFaction&){ Update();};

					window = Window::Create();
					window->SetWidth(0.75f);
					
					PPanel factionBack = AddPanel("Alliances");
					PPanel chatBack = AddPanel("Chat History");
					chatInput = AddPanel("");
					buttons = AddPanel("");

					
					{
						chatHistory.reset(new Panel{});
						chatBack->AnchorBelowPrevious(chatHistory,0,0);
						chatHistory->SetMaxAutoHeight(0.3f);
						chatHistory->SetAutomaticHeight(true);
						chatHistory->SetBackgroundTransparent();
					}
					{
						factionTable.reset(new Panel{});
						factionBack->AnchorBelowPrevious(factionTable,0,0);
						factionTable->SetMaxAutoHeight(0.3f);
						factionTable->SetAutomaticHeight(true);
						factionTable->SetBackgroundTransparent();
					}
					{
						PButton close{new Button{"Close",Hide}};
						close->SetVerticalPadding(0.5f);
						close->FillHorizontally(0);
						buttons->AddComponent(close);
						close->Anchor().GetVAxis().AnchorUpperUnknown();
					}

					{
						PButton filter = std::make_shared<Button>("All");
						filter->LockToIdealHeight();
						filter->LockToIdealWidth();
						WButton wFilter = filter;
						static bool sendToAlliance = false;

						filter->SetTriggerFunction([wFilter](){
							PButton b = wFilter.lock();
							if (!b)
								return;
							sendToAlliance = !sendToAlliance;
							if (sendToAlliance)
								b->SetText("Alliance");
							else
								b->SetText("All");

						});

						chatMessage.reset(new Input{});
						chatMessage->SetAllowedAlphabet(Network::allowedChatCharacters);
						PButton send{new Button{"Send",[](){
							if (sendToAlliance)
								Network::SendChatMessage(chatMessage->GetText(),Objects::Faction::mine);
							else
								Network::BroadcastChatMessage(chatMessage->GetText());
							chatMessage->SetText("");
						}}};
						chatMessage->onSpecialKey = [](Key::Name k)
						{
							if (k == Key::Tab)
								Hide();
						};
						send->SetVerticalPadding(0.5f);
						send->Anchor().CenterVertically().Right(0.01f);
						send->LockToIdealWidth();
						send->LockToIdealHeight();
						chatMessage->Anchor().Top().FillHBetween(filter,send);
						chatInput->AddComponent(filter);
						chatInput->AddComponent(send);
						chatInput->AddComponent(chatMessage);
						chatMessage->onEnter = [send](){send->Trigger();};
					}
				}
				factionTable->ClearComponents();
				Objects::Alliance::Foreach([](const Objects::PAlliance&a)
				{
					PPanel wrapper{new Panel{}};
					wrapper->Anchor().BeneathLastOf(factionTable);
					wrapper->fillBackground = false;
					wrapper->renderBorder = a->CountLivingMembers()  > 1;
					factionTable->AddComponent(wrapper);

					PPanel alliancePanel{new Panel{}};
					alliancePanel->fillBackground = false;
					alliancePanel->renderBorder = false;
					alliancePanel->Anchor().FillHorizontally(0.01f,0.001f).Top();


					if (a->CountLivingMembers() > 1)
					{
						PLabel allianceLabel{new Label{a->id.ToNiceString(),1.f}};
						allianceLabel->Anchor().Left().Top();
						allianceLabel->SetVerticalPadding(0.3f,0.1f);
						wrapper->AddComponent(allianceLabel);
						alliancePanel->Anchor().Beneath(allianceLabel);
					}

					wrapper->AddComponent(alliancePanel);


					Objects::WAlliance wa = a;


	


					if (Objects::Faction::mine)
					{
						if (a != Objects::Faction::mine->alliance)
						{
							PButton joinButton{new Button{JoinText(a)}};
							joinButton->fillBackground = false;
							joinButton->renderBorder = false;
							if (a->CountLivingMembers() == 1)
								joinButton->SetColor(a->GetMember(0)->color);
							WButton jb = joinButton;
							joinButton->SetTriggerFunction(
							[wa,jb]()
							{
								Objects::PAlliance now = wa.lock();
								if (now == nullptr || now == Objects::Faction::mine->alliance)
									return;
								DispatchJoinRequest(now);
								jb.lock()->SetText(JoinText(now));

							});

							joinButton->LockToIdealWidth(true);
							//joinButton->LockToIdealHeight(true);
							joinButton->SetPadding(M::Quad<float>(0.2f));
							joinButton->fillBackground = false;
							joinButton->renderBorder = false;
							joinButton->SetLocalFontScale(0.85f);

							joinButton->Anchor().Right(0.01f).Bottom(0.01f);
							wrapper->AddComponent(joinButton);
						}
						else if (a->CountLivingMembers() > 1)
						{
							PButton leaveButton{new Button{"Leave alliance "+a->id.ToNiceString()}};
							leaveButton->SetTriggerFunction(
							[]()
							{
								Dialog::Confirm("Really leave your current alliance?",[]()
								{
									Network::Channels::AllianceJoinRequest.SendTo(Network::client,Protocol::AllianceJoinRequest(TUniqueID::Empty(),false));
								});
							});

							leaveButton->LockToIdealWidth(true);
							//joinButton->LockToIdealHeight(true);
							leaveButton->SetPadding(M::Quad<float>(0.2f));
							leaveButton->fillBackground = false;
							leaveButton->renderBorder = false;
							leaveButton->SetLocalFontScale(0.85f);

							leaveButton->Anchor().Right(0.01f).Top(0.01f);
							wrapper->AddComponent(leaveButton);
						}

					}


					a->ForeachMember([alliancePanel](const Objects::PFaction&f)
					{
						AddFaction(alliancePanel,f);
					});
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

				WindowStack::VerifyIntegrity(CLOCATION);

			}

		}

		void	Show()
		{
			Details::Update();
			Details::Show();
		}

		bool	IsVisible()
		{
			return WindowStack::IsVisible(Details::window);
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
			timeLabel->Anchor().GetVAxis().AnchorLowerUnknown();
			timeLabel->Anchor().GetHAxis().AnchorUpperUnknown();
			if (owner)
			{
				senderLabel.reset(new Label{owner->name+": "});
				senderLabel->SetColor(owner->color);
				senderLabel->LockToIdealWidth();
				senderLabel->Anchor().GetHAxis().AnchorLowerUnknown();
				//senderLabel->Anchor().Top().Set(0);
				senderLabel->alignCenter = false;
				senderLabel->LockToIdealHeight();
				senderLabel->SetPadding(0);
				senderLabel->SetVerticalPadding(0.25f);
				senderLabel->SetHorizontalPadding(1,0);
				textLabel->SetColor(owner->color * 0.5f + 0.5f); 
				{
					auto cfg = textLabel->Anchor();
					cfg.GetHAxis().lower.Set(senderLabel,0);
					cfg.GetHAxis().upper.Disable();
				}
			}
			else
			{
				textLabel->SetColor(owner->color);
				textLabel->Anchor().GetHAxis().AnchorLowerUnknown();
			}
			textLabel->SetPadding(0);
			textLabel->SetVerticalPadding(0.25f);
			textLabel->Anchor().FillHBetween(senderLabel,timeLabel);
			//textLabel->Anchor().Top().Set(0);
			textLabel->alignCenter = false;
			textLabel->LockToIdealHeight();

			//timeLabel->fillBackground = true;
			//senderLabel->fillBackground = true;

			chatEntry->SetAutomaticHeight(true);
			chatEntry->AddComponent(timeLabel);
			chatEntry->AddComponent(senderLabel);
			chatEntry->AddComponent(textLabel);
			chatEntry->SetBackgroundTransparent();

			chatHistory->AnchorBelowPrevious(chatEntry,0,0);

			window->FullAutoLayout(CLOCATION);

			chatHistory->ScrollToBottom();
		}

	}
}

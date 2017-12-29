#include "guiHostList.h"
#include "guiComponents.h"
#include "../../clientEvents.h"
#include "../../voting.h"
#include "windowStack.h"


namespace HUD
{
	namespace VoteList
	{
		namespace Details
		{
			Component::PWindow window;
			//Component::PPanel activePanel,
				//				closedPanel;
			Timer::Time last = timer.Now();

			struct TEntry
			{
				PVote vote;
				Component::PLabel	timerLabel,phraseLabel;
				Component::PButton	yesButton,noButton;
				Component::PPanel	panel;

				void	Refresh() const
				{
					const float4 fontColor = vote->HaveVoted() ? float4(1,1,1,0.5) : float4(1);

					phraseLabel->SetColor(fontColor);
					timerLabel->SetColor(fontColor);
					timerLabel->SetText(String{std::max(0,int(vote->GetRemainingSeconds()))}+" s. remaining");

					if (vote->HaveVoted())
					{
						if (vote->HaveVotedInFavor())
							yesButton->SetAlwaysDown(true);
						else
							noButton->SetAlwaysDown(true);
						panel->fillBackground = false;
					}
					else
						panel->fillBackground = true;
				}
			};

			Buffer0<TEntry>	entries;

			


			void	Create(const PVote&v)
			{

				TEntry&entry = entries.Append();
				entry.vote = v;

				Component::PPanel panel (new Component::Panel());
				entry.panel = panel;

				panel->Anchor().FillHorizontally().BeneathLastOf(window);
				window->AddComponent(panel);


				panel->SetAutoHeightSpacing(0.01f);
				Component::PLabel label{new Component::Label{v->phrase}};
				label->Anchor().FillHorizontally().Top();
				panel->AddComponent(label);

				WVote w = v;

				Component::PLabel timeLabel{new Component::Label{"",0.7f}};
				timeLabel->Anchor().Left().Bottom();
				panel->AddComponent(timeLabel);


				Component::PButton noButton{new Component::Button{"No",[w]()
				{
					PVote v2 = w.lock();
					if (v2)
						v2->CastVote(false);
				}}};
				noButton->Anchor().Right(0.01f).Beneath(label);
				panel->AddComponent(noButton);



				Component::PButton yesButton{new Component::Button{"Yes",[w]()
				{
					PVote v2 = w.lock();
					if (v2)
						v2->CastVote(true);
				}}};

				yesButton->Anchor().LeftOf(noButton,0.01f).Beneath(label);
				panel->AddComponent(yesButton);
				
				entry.yesButton = yesButton;
				entry.noButton = noButton;
				entry.timerLabel = timeLabel;
				entry.phraseLabel = label;

				entry.Refresh();
			}

			void	Rebuild(const PVote&v)
			{
				window->ClearComponents();
				entries.Clear();
				Vote::VisitActiveVotes([](const PVote&v)
				{
					Create(v);
				});
				last = timer.Now();
			}

			void	Refresh(const PVote&v)
			{
				foreach (entries,e)
					e->Refresh();
			}



			void Count()
			{
				if (timer.GetSecondsSince(last) > 1)
				{
					Refresh(nullptr);
				}
				
			}

		}


		void	Create()
		{
			using namespace Details;

			window = Component::Window::Create();
			window->Anchor().CenterVertically().CenterHorizontally(0.75f);
			

			GlobalEvents::voteCast << Refresh;
			GlobalEvents::voteClosed << Rebuild;
			GlobalEvents::voteCreated << Rebuild;
			GlobalEvents::frameBegin << Count;

			GlobalEvents::objectFlush << [](){Rebuild(nullptr);};

			//GlobalEvents::applicationTerminate << CleanUp;	//not sure i even need this

		}

		void	Show(bool doShow)
		{
			if (doShow)
				HUD::WindowStack::Show(Details::window);
			else
				HUD::WindowStack::Hide(Details::window);
		}
		bool	IsVisible()
		{
			return HUD::WindowStack::IsVisible(Details::window);
		}

	}
}

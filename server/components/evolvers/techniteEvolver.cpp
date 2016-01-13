#include <global_root.h>
#include "techniteEvolver.h"
#include <objects/gridAndChoices.h>
#include <objects/mobileEntity.h>
#include <objects/faction.h>
#include "../state.h"

#include "../networkChannels.h"

void CheckSendTerrain();


namespace Evolver
{


	namespace TechniteDetails
	{


		float GetBaseAnimationLength()
		{
			float animationLength = 0.f;
			Objects::ForeachMobileEntityIncludingStored([&animationLength](const Objects::PMobileEntity&m)
			{
				animationLength = std::max(m->timeAt,animationLength);
			});
			return animationLength;
		}


		bool InExtraTechniteRound()
		{
			if (!State::CurrentRoundIsExtraTechniteRound())
				return false;

			return State::techniteFactionsOnline > 0;

		}


	}



	/*virtual override*/ bool	Technite::ContinueRoundProcessing()
	{
		switch (RuntimeConfig::phase)
		{
			case RuntimeConfig::Phase::TechnitePhase:

				if (!Technites::ProcessNext())
				{
					CheckSendTerrain();
					for (const auto&tf : Technites::Faction::allTechniteFactions)
					{
						State::PClient client;
						if (!State::playerMap.query(tf,client))
							continue;
						TCP::PPeer peer = client->peer.lock();
						if (peer)
							SendTechniteStateTo(tf,*peer);
					}
					return false;
				}
				return true;

			break;
			default:
				if (!Super::ContinueRoundProcessing())
				{
					State::ChangePhase(RuntimeConfig::Phase::TechnitePhase);
					Technites::BeginProcessing(TechniteDetails::GetBaseAnimationLength());
				}
				return true;
		}
	}

	/*virtual override*/ void Technite::FinishRoundProcessing()
	{
		RuntimeConfig::techniteSubRoundNumber ++;
		if (TechniteDetails::InExtraTechniteRound())
		{
			State::ChangePhase(RuntimeConfig::Phase::TechniteCommandPhase,State::Config::extraTechniteRoundTimer > 0 && !State::roundTimerPaused ? State::Config::extraTechniteRoundTimer : -1);
		}
		else
		{
			RuntimeConfig::roundNumber++;
			RuntimeConfig::techniteSubRoundNumber = 0;

			State::ChangePhase(RuntimeConfig::Phase::CommandPhase,State::Config::roundTimer > 0 && !State::roundTimerPaused ? State::Config::roundTimer : -1);
		}
	}

	/*virtual override*/ void Technite::BeginRoundProcessing()
	{
		if (RuntimeConfig::phase == RuntimeConfig::Phase::TechniteCommandPhase)
		{
			State::ChangePhase(RuntimeConfig::Phase::TechnitePhase);
			Technites::BeginProcessing(TechniteDetails::GetBaseAnimationLength());
		}
		else
			Super::BeginRoundProcessing();
	}


}

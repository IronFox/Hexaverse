#include <global_root.h>
#include "techniteEvolver.h"
#include <objects/gridAndChoices.h>
#include <objects/mobileEntity.h>
#include <objects/faction.h>
#include "../state.h"

#include "../networkChannels.h"

void CheckSendTerrain()
{
	World::Network::SendChangedFamilies();
}


namespace Evolver
{


	namespace TechniteDetails
	{


		float GetBaseAnimationLength()
		{
			float animationLength = 0.f;
			Objects::ForeachWorldEntityIncludingStored([&animationLength](const Objects::PWorldEntity&m)
			{
				animationLength = std::max(m->motionTime,animationLength);
			});
			return animationLength;
		}


		bool InExtraTechniteRound()
		{
			if (!Game::CurrentRoundIsExtraTechniteRound())
				return false;

			return Network::techniteFactionsOnline > 0;

		}


	}



	/*virtual override*/ bool	Technite::ContinueRoundProcessing()
	{
		switch (RuntimeConfig::phase)
		{
			case RuntimeConfig::Phase::TechnitePhase:

				if (!Technites::ProcessNext())
				{
					Technites::VerifyGrid();
					foreach (Technites::Faction::allTechniteFactions,f)
						(*f)->VerifyIntegrity();
					CheckSendTerrain();
					for (const auto&tf : Technites::Faction::allTechniteFactions)
					{
						Network::PClient client;
						if (!Network::playerMap.query(tf,client))
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
					Game::Network::ChangePhase(RuntimeConfig::Phase::TechnitePhase);
					Technites::BeginProcessing(TechniteDetails::GetBaseAnimationLength());
				}
				return true;
		}
	}

	/*virtual override*/ void Technite::FinishRoundProcessing()
	{
		RuntimeConfig::roundNumber.technite ++;
		if (TechniteDetails::InExtraTechniteRound())
		{
			Game::Network::ChangePhase(RuntimeConfig::Phase::TechniteCommandPhase,Game::Config::extraTechniteRoundTimer > 0 && !Game::roundTimerPaused ? Game::Config::extraTechniteRoundTimer : -1);
		}
		else
		{
			RuntimeConfig::roundNumber.main++;
			RuntimeConfig::roundNumber.technite = 0;

			Game::Network::ChangePhase(RuntimeConfig::Phase::CommandPhase,Game::Config::roundTimer > 0 && !Game::roundTimerPaused ? Game::Config::roundTimer : -1);
		}
	}

	/*virtual override*/ void Technite::BeginRoundProcessing()
	{
		if (RuntimeConfig::phase == RuntimeConfig::Phase::TechniteCommandPhase)
		{
			Game::Network::ChangePhase(RuntimeConfig::Phase::TechnitePhase);
			Technites::BeginProcessing(TechniteDetails::GetBaseAnimationLength());
		}
		else
			Super::BeginRoundProcessing();
	}


}

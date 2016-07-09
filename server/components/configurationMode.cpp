#include <global_root.h>
#include "configurationMode.h"
#include "state/networkState.h"
#include "state/gameState.h"


namespace ConfigurationMode
{
	namespace Details
	{
		bool isActive = false;
		UINT32 originalFlags = 0;
		UINT32 planetSeed = 0;
	}


}



void ConfigurationMode::Begin()
{
	using namespace Details;
	if (isActive)
		return;
	isActive = true;
	Network::CloseSession(true);
	originalFlags = Game::Config::factionSupportFlags;
	Game::Config::factionSupportFlags = Protocol::GameFlag::SupportSpectatorFactions;
	planetSeed = Random().getSeed();
	LogEvent(EventSignificance::Important,"Starting configuration mode");
	//Network::RestartSession();
}

void ConfigurationMode::End()
{
	using namespace Details;
	if (!isActive)
		return;
	isActive = false;
	//Network::CloseSession(false);
	Game::Config::factionSupportFlags = originalFlags;
	Network::RestartSession();
	LogEvent(EventSignificance::Important,"Ending configuration mode");

}

bool ConfigurationMode::IsActive()
{
	return Details::isActive;
}

UINT ConfigurationMode::GetPlanetSeed()
{
	using namespace Details;
	return planetSeed;
}

void ConfigurationMode::SetPlanetSeed(UINT32 seed)
{
	using namespace Details;
	planetSeed = seed;
}

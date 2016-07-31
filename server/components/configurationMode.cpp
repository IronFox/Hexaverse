#include <global_root.h>
#include "configurationMode.h"
#include "state/networkState.h"
#include "state/gameState.h"
#include "state/worldState.h"

extern Random random;

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
	Network::BuildConfiguredWorld();

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
	LogEvent(EventSignificance::Important,"Ending configuration mode");
	Objects::FlushAllData(true);
	Network::server.Shutdown();
	ASSERT__(!Network::server.IsOnline());
	Network::activePlayersAndSpectators.Clear();
	Network::spectators.Clear();
	World::BuildConfiguredWorld(random);


	Network::RestartSession();

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

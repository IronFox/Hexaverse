#include <global_root.h>
#include "configurationMode.h"
#include "state/networkState.h"
#include "state/gameState.h"
#include "state/worldState.h"
#include "state/fileIO.h"

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
	originalFlags = Game::Config::factionSupportFlags;	//must copy this before actually closing the session
	Network::CloseSession(true);						//this line wipes factionSupportFlags
	//Game::Config::factionSupportFlags = Protocol::GameFlag::SupportSpectatorFactions;	//and this is redundant
	planetSeed = Random().NextSeed();
	LogEvent(EventSignificance::Important,"Starting configuration mode");
	Network::BuildConfiguredWorld();
	FileIO::RefreshWindowTitle();

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
	World::BuildConfiguredWorld(random, customResourceDistribution);


	Network::RestartSession(true);
	FileIO::RefreshWindowTitle();

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

#include <global_root.h>
#include "configurationMode.h"
#include "state/networkState.h"
#include "state/gameState.h"
#include "state/worldState.h"
#include "state/fileIO.h"
#include <globalEvents.h>

extern TwisterRandom random;

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

void ConfigurationMode::Abort()
{
	using namespace Details;
	if (!isActive)
		return;
	Game::Config::factionSupportFlags = originalFlags;
	LogEvent(EventSignificance::Important,"Aborting configuration mode");

	FileIO::RefreshWindowTitle();
	isActive = false;
}

void ConfigurationMode::End()
{
	using namespace Details;
	if (!isActive)
		return;
	//Network::CloseSession(false);
	Game::Config::factionSupportFlags = originalFlags;
	LogEvent(EventSignificance::Important,"Ending configuration mode");
	GlobalEvents::FlushObjects();
	Network::server.Shutdown();
	ASSERT__(!Network::server.IsOnline());
	Network::activePlayersAndSpectators.Clear();
	Network::spectators.Clear();
	World::BuildConfiguredWorld(random);


	Network::RestartSession(true);
	FileIO::RefreshWindowTitle();

	isActive = false;
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

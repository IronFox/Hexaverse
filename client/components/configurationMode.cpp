#include <global_root.h>
#include "configurationMode.h"
#include "world.h"

namespace ConfigurationMode
{
	static bool isActive = false;
	static Grid::ExtendedPlanetConfig	config;
	static UINT32 seed = 0;
	static Random rng;

	static void Update()
	{
		Random random(seed);
		config.RandomizeSeeds(random);

		if (isActive)
		{
			//Grid::actualWorld.FlushAllData();
			Grid::actualWorld.BuildWorld(config);
			Grid::SetAllFamiliesOutdated();	//force rebuild
			World::allowRecompilation = true;
		}


	}

	void	RandomizeSeed()
	{
		seed = rng.getSeed();
		Update();
	}

	void	Update(const Grid::ExtendedPlanetConfig&cfg)
	{
		config = cfg;
		Update();
	}


	void	SetActive(bool newActive)
	{
		if (isActive == newActive)
			return;
		isActive = newActive;



	}

	bool	IsActive()
	{
		return isActive;
	}
}

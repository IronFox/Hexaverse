#include <global_root.h>
#include "gridControl.h"

#include <grid/planet.h>
#include <grid/resources.h>
#include <grid/worldData.h>
#include "visual/terrain.h"
#include "visual/world.h"
#include "visual/lighting.h"
#include "clientEvents.h"
#include <objects/faction.h>

namespace ClientGrid
{
	void	Create()
	{
		Grid::CreateHighAndLow();
		Grid::CreateResources();
		World::Create();

		#ifdef HAS_WINDOW
			GlobalEvents::frameAfterCamera << World::RecompileIfNecessary;
		#endif
	}

	void	Destroy()
	{
		#ifdef HAS_WINDOW
			VisualTerrain::Destroy();
		#endif
	}


	void	Apply(const Grid::TSharedPlanetConfig&cfg)
	{
		Grid::Content coreContent = Grid::Content::Reinterpret(cfg.innerCoreType);
		Grid::CellStack::bottom.content = coreContent;
		#ifdef HAS_WINDOW
			Lighting::UpdateAtmosphere(cfg.atmosphereColor);
			Lighting::UpdateSun(cfg.sunPlaneNormal_Speed,cfg.sunColor);
		#endif
	}


	void CheckSensory()
	{
		if (World::actual.SensoryIsInvalid())
		{
			World::actual.FlushAndValidateSensory();
			Objects::ForeachWorldEntity([](const Objects::PWorldEntity&e)
			{
				if (e->isOwnedByMe)
				{
					float t= 0;
					e->RegisterSensory(t);
				}
			},false);
		}

	}
}


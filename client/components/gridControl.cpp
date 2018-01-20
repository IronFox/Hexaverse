#include <global_root.h>
#include "gridControl.h"

#include <grid/planet.h>
#include <grid/resources.h>
#include <grid/worldData.h>
#include "visual/scene/terrain.h"
#include "visual/scene/world.h"
#include "visual/scene/lighting.h"
#include "clientEvents.h"
#include <objects/faction.h>
#include "network.h"

namespace ClientGrid
{
	void	Create()
	{

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
			#ifdef HAS_WINDOW
				VisualTerrain::visibleSubOrbitGrid.Clear();
			#endif
			if (Network::amSpectator)
				World::actual.FlushAndValidateSensory(1);
			else
			{
				World::actual.FlushAndValidateSensory();
				Objects::ForeachWorldEntity([](const Objects::PWorldEntity&e)
				{
					if (e->isOwnedByMe || e->owner->SharingSensorWith(Objects::Faction::mine))
					{
						float t= 0;
						e->ReapplySensory(t);
					}
				},false);
			}
		}

	}
}


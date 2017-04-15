#include <global_root.h>
#include "gridControl.h"

#include <grid/planet.h>
#include <grid/resources.h>
#include <grid/worldData.h>
#include "visual/terrain.h"
#include "visual/world.h"
#include "visual/lighting.h"
#include "clientEvents.h"

namespace ClientGrid
{
	void	Create()
	{
		Grid::CreateHighAndLow();
		Grid::CreateResources();

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
}


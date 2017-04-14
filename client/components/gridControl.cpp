#include <global_root.h>
#include "gridControl.h"

#include <grid/planet.h>
#include <grid/resources.h>
#include <grid/worldData.h>
#include "gridRenderer.h"

namespace ClientGrid
{
	void	Create()
	{
		Grid::CreateHighAndLow();
		Grid::CreateResources();
	}

	void	Destroy()
	{
		Grid::Destroy();
	}


	void	Apply(const Grid::TSharedPlanetConfig&cfg)
	{
		Grid::Content coreContent = Grid::Content::Reinterpret(cfg.innerCoreType);
		Grid::CellStack::bottom.content = coreContent;
		#ifdef HAS_WINDOW
			Grid::UpdateAtmosphere(cfg.atmosphereColor);
			Grid::UpdateSun(cfg.sunPlaneNormal_Speed,cfg.sunColor);
		#endif
	}
}


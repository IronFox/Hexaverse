#include <global_root.h>
#include "gridControl.h"

#include <grid/planet.h>
#include <grid/resources.h>
#include <grid/worldData.h>
#include "visual/terrain.h"

namespace ClientGrid
{
	void	Create()
	{
		Grid::CreateHighAndLow();
		Grid::CreateResources();
	}

	void	Destroy()
	{
		#ifdef HAS_WINDOW
			VisualTerrain::Destroy();
		#endif
	}

	void	FlushAllData()
	{
		World::actual.FlushAllData(true);
		Grid::Resource::FlushAll();
		Grid::ResourceDiscovery::client.FlushAllData();
	}

	void	Apply(const Grid::TSharedPlanetConfig&cfg)
	{
		Grid::Content coreContent = Grid::Content::Reinterpret(cfg.innerCoreType);
		Grid::CellStack::bottom.content = coreContent;
		#ifdef HAS_WINDOW
			VisualTerrain::UpdateAtmosphere(cfg.atmosphereColor);
			VisualTerrain::UpdateSun(cfg.sunPlaneNormal_Speed,cfg.sunColor);
		#endif
	}
}


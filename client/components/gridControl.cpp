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
#include <general/thread_system.h>

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


	namespace SensorPainting
	{
		bool started = false;
	}


	/*
	This is the client equivalent of server side Faction::CheckSensory()
	*/
	void CheckSensory()
	{

		if (SensorPainting::started && !World::actual.PaintingSensorsConcurrently())
		{
			SensorPainting::started = false;
			#ifdef HAS_WINDOW
				VisualTerrain::Invalidate();
			#endif

			if (!World::actual.SensoryIsInvalid())
				GlobalEvents::gridSensoryValid();
		}

		if (World::actual.SensoryIsInvalid())
		{
			if (World::actual.PaintingSensorsConcurrently())
				return;

			#ifdef HAS_WINDOW
				VisualTerrain::visibleSubOrbitGrid.Clear();
			#endif
			if (Network::amSpectator)
				World::actual.FlushAndValidateSensory(1);
			else
			{
				static Buffer0<World::Sensor>	sensors;
				Objects::ForeachWorldEntity([](const Objects::PWorldEntity&e)
				{
					if (e->owner->SharesSensorsWithMe())
					{
						e->VisitSensors(e->location,[](const World::Sensor&s)
						{
							sensors << s;
						});
						e->OverrideSetRegisteredSensorToCurrentLocation();
					}
				},Objects::IncludeGhosts::None);
				World::actual.PaintSensorsConcurrently(sensors);
				sensors.Clear();

				SensorPainting::started = true;

			}
		}

	}
}

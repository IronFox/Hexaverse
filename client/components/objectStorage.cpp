#include <global_root.h>
#include "objectStorage.h"
#include <objects/structure.h>
#include <objects/vehicle.h>
#include <objects.h>
#include <runtimeConfig.h>
#include "visual/scene/terrain.h"
#include "clientEvents.h"

namespace Objects
{
	void	Storage::Clear()
	{
		objects.Clear();
		storedMobiles.Clear();
		markerObjects.Clear();
		markers.Clear();
		unitLimits.Clear();
		loaded = false;
	}

	void	Storage::Import(bool preserveGridVisibility)
	{
		ASSERT__(!loaded);
		//Clear();
		World::actual.InvalidateSensory();	//prevent expensive removal operations
		Objects::ForeachWorldEntityIncludingStored([this](const PWorldEntity&e)
		{
			e->isInserted = false;
			e->isInsertedIntoGrid = false;
			float t = 0;
			e->UnregisterSensory(t);
			if (e->IsDeadOrGhost())
				markerObjects << e;
			else
			{
				PHangar h;
				if (e->IsMobile() && (h = To<MobileEntity>::Cast(e)->storedIn.lock()))
					storedMobiles << std::make_pair(h, To<MobileEntity>::Cast(e)); 
				else
					objects << e; 
			}
		},true);
		markers.swap(Marker::all);
		RuntimeConfig::UnitLimits::Export(unitLimits);

		Objects::preserveReferencesOnFlush = true;
		GlobalEvents::FlushObjects();
		Objects::preserveReferencesOnFlush = false;

		BaseGrid::FamiliesChanged	fam;
		if (World::actual.ResetStructureCounters(!preserveGridVisibility,&fam))
		{
			#ifdef HAS_WINDOW
				foreach (fam,f)
					VisualTerrain::InvalidateFamily(*f);	//recompile as a result to structure counts (they darken the terrain) reset
			#endif
		}
		loaded = true;
	}

	void	Storage::Export()
	{
		GlobalEvents::FlushObjects();
		World::actual.ResetStructureCounters(true);
		markers.swap(Marker::all);
		RuntimeConfig::UnitLimits::Import(unitLimits);
		for (const PWorldEntity&e : objects)
		{
			float t = 0;
			{
				if (e->IsStructure())
					Objects::InsertConstructed(To<Structure>::Cast(e),t,false,false);
				elif (e->IsMobile())
					Objects::InsertConstructed(To<MobileEntity>::Cast(e),t,true,false);
			}
		}

		for (const PWorldEntity&e : markerObjects)
		{
			if (e->IsMobile())
				Marker::mobileGhosts << To<MobileEntity>::Cast(e);
			elif (e->IsStructure())
				Marker::structureGhosts << To<Structure>::Cast(e);
		}
		for (const auto&p : storedMobiles)
		{
			p.first->StoreLocallyFromImport(p.second);
		}


		Objects::RefreshCompoundCoverage(0);
		Clear();
	}
	
	
}

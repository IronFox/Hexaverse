#include <global_root.h>
#include "objectStorage.h"
#include <objects/structure.h>
#include <objects/vehicle.h>
#include <objects.h>
#include <runtimeConfig.h>

namespace Objects
{
	void	Storage::Clear()
	{
		objects.Clear();
		storedObjects.Clear();
		markerObjects.Clear();
		markers.Clear();
		unitLimits.Clear();
		loaded = false;
	}

	void	Storage::Import(bool preserveGridVisibility)
	{
		ASSERT__(!loaded);
		//Clear();
		
		Objects::ForeachWorldEntityIncludingStored([this](const PWorldEntity&e)
		{
			e->isInserted = false;
			e->isInsertedIntoGrid = false;
			if (e->health <= 0)
				markerObjects << e;
			else
			{
				PHangar h;
				if (e->IsMobile() && (h = To<MobileEntity>::Cast(e)->storedIn.lock()))
					storedObjects << std::make_pair(h, To<MobileEntity>::Cast(e)); 
				else
					objects << e; 
			}
		},true);
		markers.swap(Marker::all);
		RuntimeConfig::UnitLimits::Export(unitLimits);
		Objects::FlushAllData(true);
		Grid::actualWorld.ResetStructureCounters(!preserveGridVisibility);
		Grid::SetAllFamiliesOutdated(0);
		loaded = true;
	}

	void	Storage::Export()
	{
		Objects::FlushAllData(true);
		Grid::actualWorld.ResetStructureCounters(true);
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
		for (const auto&p : storedObjects)
		{
			p.first->StoreLocallyFromImport(p.second);
		}


		Objects::RefreshCompoundCoverage(0);
		Clear();
	}
	
	
}

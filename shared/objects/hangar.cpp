#include <global_root.h>
#include "hangar.h"
#include "mobileEntity.h"
#include "gridAndChoices.h"

namespace Objects
{
	


	bool	Hangar::StoreLocally(const PMobileEntity&e, float&atTime, count_t maxStorage, const PHangar&sharedSelf)
	{
		if (!e || !CanStoreType(*e) || stored.Count() >= maxStorage)
			return false;

		#ifndef CLIENT
			e->SyncTime(ToEntity(),atTime,timeEpsilon);
		#endif

		if (e->isInserted)
		{
			Remove(e,RemovalType::EnteredStorage,atTime,false,!e->isInsertedIntoGrid,true);
			#ifndef NO_HIBERNATION
				e->Hibernate(false,atTime);
			#endif
		}
		else
		{
			if (!GetEntity(e->id))
			{
				RegisterNewOffGrid(e,atTime,true);
			}
		}
		LogEvent(EventSignificance::Common,e.get()," Now stored in "+ToEntity().GetName());
		e->storedIn = sharedSelf;

		stored.Append().entity = e;

		#ifdef HAS_WINDOW
			WMobileEntity w = e;
			stored.last().deployCommand = DeployMobileCommand(e,
				[w]()
				{
					//focus = To<VehicleStore>::DynamicCast(storeEntity).get();
					if (deployingVehicle)
						deployingVehicle.reset();
					else
						deployingVehicle = w.lock();
				},
				[w](bool controlEnabled)
				{
					BaseCommand::Flags rs = CommonCmd::CommonButtonCheck(controlEnabled);
					PVehicleStore s = To<VehicleStore>::DynamicCast(selectedEntity);
					if (deployingVehicle && s == selectedEntity && deployingVehicle == w.lock() && deployingVehicle->health > 0)
						rs |= BaseCommand::Flags::IsPermaActive;
					return rs;
				}
				);
			stored.last().deployCommand.RasterizeImage();
		#endif

		#ifndef CLIENT
			//RegisterChange(StorageChanged,atTime);	might be irrelevant. let's see
			e->RegisterChange(WorldEntity::StorageChanged,atTime);
		#endif

		ASSERT2__(!e->isInsertedIntoGrid,ToEntity().GetName(), e->GetName());

		#ifdef _DEBUG
			CheckMobileEntitySanity();
		#endif


		return true;
	}
}

#include <global_root.h>
#include "hangar.h"
#include "mobileEntity.h"
#include "gridAndChoices.h"
#include "faction.h"
#include <runtimeConfig.h>

namespace Objects
{
	/*static*/ Observable<Hangar&,Hangar::TStored&>	Hangar::onStored;


	/*static*/ void		Hangar::StaticSetup()
	{
	


	}


	bool						Hangar::RemoveStored(const PMobileEntity&v)
	{
		foreach (stored,s)
			if (s->entity.lock() == v)
			{
				stored.erase(s);
				OnStorageChanged();
				return true;
			}
		return false;
	}

	
	bool	Hangar::HasStoredLocally(const PMobileEntity&e) const
	{
		for (const auto&s : stored)
			if (s.entity.lock() == e)
				return true;
		return false;
	}

	void	Hangar::OnDestroy(float&t)
	{
		ToEntity().SyncTime(t);
		#ifndef CLIENT
			for(const auto&v:stored)
			{
				ASSERT1__(!v.entity.expired(),ToEntity().GetName());
				Remove(v.entity.lock(),RemovalType::DestroyedInStorage,t);
			}
		#endif
		stored.Clear();
	}


	#ifdef CLIENT
		/*virtual*/ void	Hangar::Lose(const PMobileEntity&v)
		{
			ASSERT3__(v->storedIn.lock().get() == this,v->GetName(),v->storedIn.lock()->ToEntity().GetName(),ToEntity().GetName());
			v->storedIn.reset();
			ASSERT2__(RemoveStored(v),v->GetName(),ToEntity().GetName());
			OnStorageChanged();
		}

	#endif

	/*virtual*/ void	Hangar::LocalCheckout(const PMobileEntity&v, const Grid::LCellIdI&targetLocation, float&atTime)
	{
		auto&self = ToEntity();
		ASSERT1__(v,self.GetName());
		v->SyncTime(self,atTime,timeEpsilon);

		#ifndef CLIENT
		{
			String error;
			if (!self.owner->CanSpawnFromStorage(v,error))
			{
				LogEvent(EventClass::Preset::commandRejected,&self,"Failed to check out "+v->GetNiceName()+": "+error);
				return;
			}
			v->lastDeployment = RuntimeConfig::roundNumber;
		}
		#endif

		ASSERT1__(!v->isInsertedIntoGrid,v->GetName());
		#ifdef _DEBUG
			CheckMobileEntitySanity();
		#endif
		ASSERT3__(v->storedIn.lock().get() == this, v->GetName(),v->storedIn.lock()->ToEntity().GetName(),self.GetName());
		ASSERT2__(RemoveStored(v),v->GetName(),self.GetName());
		LogEvent(EventSignificance::Debug,&self,"Checking out "+v->GetName()+" to "+ToString(targetLocation));

		bool doInsert = true;

		#ifndef CLIENT
			MobilePlacementResult rs = v->PlaceExactlyAt(targetLocation,0.f,0.f,false,false,true);
			if (Failed(rs))
			{
				if (v->IsDeadOrGhost())
				{
					LogEvent(EventSignificance::Debug,&self,v->GetName()+" did not survive checkout to "+ToString(targetLocation));
				}
				else
				{
					LogEvent(EventClass::Preset::fatal,&self,"Checkout of "+v->GetName()+" failed unexpectedly: "+String(rs));
					FATAL__("Checkout failed");
				}
			}
			else
			{
				doInsert = rs != MobilePlacementResult::EnteredHangar;
				v->RegisterChange(WorldEntity::PositionChanged,atTime);
				//as it turns out, inserting into a hangar does not guarantee it actually ends up in the hangar directly selected,
				//but is relocated to some other connected hangar. So only check if we do not insert into the grid
				DBG_ASSERT4__(!doInsert || v->location.stackID == targetLocation.stackID,self.GetName(),v->GetName(),targetLocation, v->location);
			}
		#else
			v->location = targetLocation;
		#endif
		if (doInsert)
		{
			if (v->IsAlive())
			{
				InsertConstructed(v,atTime);
				#ifndef CLIENT
					v->SignalDeployment();
				#endif
			}
			else
				v->storedIn.reset();
			ASSERT__(v->storedIn.expired());
		}
		#ifdef _DEBUG
			CheckMobileEntitySanity();
		#endif

		OnStorageChanged();
	}


	bool	Hangar::StoreLocally(const PMobileEntity&e, float&atTime, bool setHealthToFullIfNew, const PHangar&sharedSelf)
	{
		ASSERT2__(sharedSelf.get() == this,sharedSelf->ToEntity().GetName(),ToEntity().GetName());
		if (!e || !CanStoreType(*e) || stored.Count() >= GetMaxStorage())
			return false;

		#ifndef CLIENT
			e->SyncTime(ToEntity(),atTime,timeEpsilon);
		#endif

		if (e->isInserted)
		{
			ForceRemove(e,RemovalType::EnteredStorage,atTime);
			#ifndef NO_HIBERNATION
				e->Hibernate(false,atTime);
			#endif

			LogEvent(EventSignificance::Common,e.get()," Now stored in "+ToEntity().GetName());
			e->storedIn = sharedSelf;
			e->location = ToEntity().location;

		}
		else
		{
			LogEvent(EventSignificance::Common,e.get()," Now stored in "+ToEntity().GetName());
			e->storedIn = sharedSelf;
			e->location = ToEntity().location;

			if (!GetEntity(e->id))
			{
				RegisterNewOffGrid(e,atTime,setHealthToFullIfNew);
			}
		}

		stored.Append().entity = e;


		#ifndef CLIENT
			//RegisterChange(StorageChanged,atTime);	might be irrelevant. let's see
			e->RegisterChange(WorldEntity::changeType_t(  WorldEntity::StorageChanged | WorldEntity::PositionChanged ),atTime);
		#endif

		ASSERT2__(!e->isInsertedIntoGrid,ToEntity().GetName(), e->GetName());

		#ifdef _DEBUG
			CheckMobileEntitySanity();	//this could fail (under rare circumstances) when mobile entities are currently not inserted into grid during animation
		#endif

		onStored(*this,stored.Last());
		OnStorageChanged();

		return true;
	}
}

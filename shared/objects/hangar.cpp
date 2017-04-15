#include <global_root.h>
#include "hangar.h"
#include "mobileEntity.h"
#include "gridAndChoices.h"
#include "faction.h"

namespace Objects
{
	/*static*/ Observer<Hangar&,Hangar::TStored&>	Hangar::onStored;

	#ifdef HAS_WINDOW
		/*static*/ index_t			Hangar::storeOffset;
		/*static*/ PBaseCommand		Hangar::cmdLeft,
									Hangar::cmdRight;
		/*static*/ int2				Hangar::cmdLeftCoords,
									Hangar::cmdRightCoords;

	#endif

	/*static*/ void		Hangar::StaticSetup()
	{
	


	}


	bool						Hangar::RemoveStored(const PMobileEntity&v)
	{
		foreach (stored,s)
			if (s->entity.lock() == v)
			{
				stored.erase(s);
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
				Remove(v.entity.lock(),RemovalType::DestroyedInStorage,t,false,true);
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
		}
		#endif

		ASSERT1__(!v->isInsertedIntoGrid,v->GetName());
		#ifdef _DEBUG
			CheckMobileEntitySanity();
		#endif
		ASSERT3__(v->storedIn.lock().get() == this, v->GetName(),v->storedIn.lock()->ToEntity().GetName(),self.GetName());
		ASSERT2__(RemoveStored(v),v->GetName(),self.GetName());
		LogEvent(EventSignificance::Debug,&self,"Checking out "+v->GetName()+" to "+targetLocation.ToString());
		#ifndef CLIENT
			MobilePlacementResult rs = v->PlaceExactlyAt(targetLocation,0.f,0.f,false,false,true);
			if (rs != MobilePlacementResult::PlacementOkay && rs != MobilePlacementResult::PlacementOkayFellDown)
			{
				if (v->health <= 0)
				{
					LogEvent(EventSignificance::Debug,&self,v->GetName()+" did not survive checkout to "+targetLocation.ToString());
				}
				else
				{
					LogEvent(EventClass::Preset::fatal,&self,"Checkout of "+v->GetName()+" failed unexpectedly: "+String(rs));
					FATAL__("Checkout failed");
				}
			}
			else
			{
				v->RegisterChange(WorldEntity::PositionChanged,atTime);
				ASSERT3__(v->location.stackID == targetLocation.stackID,self.GetName(),v->GetName(),targetLocation.ToString());
			}
		#else
			v->preAnimationLocation = v->location = targetLocation;
		#endif
		if (v->health > 0)
		{
			InsertConstructed(v,atTime);
			#ifndef CLIENT
				v->SignalDeployment();
			#endif
		}
		else
			v->storedIn.reset();
		ASSERT__(v->storedIn.expired());
		#ifdef _DEBUG
			CheckMobileEntitySanity();
		#endif
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
			Remove(e,RemovalType::EnteredStorage,atTime,false,!e->isInsertedIntoGrid,true);
			#ifndef NO_HIBERNATION
				e->Hibernate(false,atTime);
			#endif
		}
		else
		{
			if (!GetEntity(e->id))
			{
				RegisterNewOffGrid(e,atTime,setHealthToFullIfNew);
			}
		}
		LogEvent(EventSignificance::Common,e.get()," Now stored in "+ToEntity().GetName());
		e->storedIn = sharedSelf;

		stored.Append().entity = e;


		#ifndef CLIENT
			//RegisterChange(StorageChanged,atTime);	might be irrelevant. let's see
			e->RegisterChange(WorldEntity::StorageChanged,atTime);
		#endif

		ASSERT2__(!e->isInsertedIntoGrid,ToEntity().GetName(), e->GetName());

		#ifdef _DEBUG
			CheckMobileEntitySanity();	//this could fail (under rare circumstances) when mobile entities are currently not inserted into grid during animation
		#endif

		onStored(*this,stored.Last());

		return true;
	}
}

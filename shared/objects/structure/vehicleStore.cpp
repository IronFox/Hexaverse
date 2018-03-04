#include <global_root.h>
#include "vehicleStore.h"
#include "../mobileEntity.h"
#include "../faction.h"

namespace Objects
{
	#ifdef HAS_WINDOW
		/*static*/ Geometry			VehicleStore::interConnector;
	#endif


	void MobilePark::ReCountVehicles()
	{
		vehiclesStored = 0;
		for (const auto&w : members)
		{
			PVehicleStore s = w.lock();
			if (s)
				vehiclesStored += s->stored.Count();
		}
	}

	void		MobilePark::FindAndRemove(VehicleStore*s)
	{
		foreach (members,m)
			if (m->lock().get() == s)
			{
				members.erase(m);
				ReCountVehicles();
				return;
			}
	}


	/*virtual override*/ void VehicleStore::OnStorageChanged()
	{
		if (park)
		{
			park->ReCountVehicles();
		}
	}


	VehicleStore::VehicleStore(BaseAttributeTable*table, const OwnerParameter&owner, const Grid::LCellIdI&location, float atTime) : Super(table,owner,location, true,atTime)
	{}


	/*virtual override*/ bool						VehicleStore::CanStoreType(const MobileEntity&e) const 
	{
		return e.IsVehicle();
	}

	/*virtual override*/ void				VehicleStore::StoreLocallyFromImport(const PMobileEntity&e)
	{
		ASSERT__(e);
		ASSERT1__(e->IsVehicle(),e->GetName());
		float t = 0.f;
		ASSERT__(Hangar::StoreLocally(e,t,false,To<VehicleStore>::Cast(shared_from_this())));
	}


	/*virtual override*/ PHangar			VehicleStore::CastToDestinationForType(const MobileEntity&e)
	{
		if (CanStoreType(e))
		{
			return To<VehicleStore>::Cast(shared_from_this());
		}
		return PHangar();
	}



	/*virtual override*/ void			VehicleStore::OnInsert( float&atTime, bool freshConstruction )
	{
		Super::OnInsert(atTime,freshConstruction);

		MergePark(owner->sharedPark);
	}




	/*virtual override*/ void				VehicleStore::OnNewConnection(const PStructure&s, UINT32 acrossHighCell, float atTime)
	{
		Super::OnNewConnection(s,acrossHighCell,atTime);

		PVehicleStore other = To<VehicleStore>::DynamicCast(s);
		if (other && other->location.height == location.height)
		{
			sneighbors.Append(other);

			if (other->park)
				MergePark(other->park);
			else
			{
				if (park)
				{
					park->members.Append(other);
					other->park = park;
				}
				else
				{
					park.reset(new Park());
					park->members.Append(To<VehicleStore>::Cast(shared_from_this()));
					park->members.Append(other);
					other->park = park;
				}
			}

		}


	}
	/*virtual override*/ void				VehicleStore::OnConnectionLost(const PStructure&s, UINT32 acrossHighCell, float atTime)
	{
		Super::OnConnectionLost(s,acrossHighCell,atTime);

		PVehicleStore other = To<VehicleStore>::DynamicCast(s);
		if (other)
		{
			foreach (sneighbors,_s)
			{
				if (_s->lock() == other)
				{
					sneighbors.erase(_s);
					break;
				}
			}
		}
	}

	void						VehicleStore::PropagatePark(const PPark&c)
	{
		if (park == c)
			return;
		MergePark(c);
		for (const auto&n : sneighbors)
			n.lock()->PropagatePark(c);
	}


	void						VehicleStore::MergePark(const PPark&other)
	{
		if (other)
		{
			if (this->park)
			{
				if (park == other)
					return;
				//compound->members.Append(other);
				for (const auto&_store : other->members)
				{
					PVehicleStore store = _store.lock();
					DBG_ASSERT__(store);
					if (store)
					{
						DBG_ASSERT__(!park->Contains(store));
						DBG_ASSERT__(store->park != park);
						park->members.Append(store);
						store->park = park;
					}
				}
				park->vehiclesStored += other->vehiclesStored;
				other->vehiclesStored = 0;
			}
			else
			{
				park = other;
				park->members.Append(To<VehicleStore>::Cast(shared_from_this()));
				park->vehiclesStored += stored.Count();
			}
		}
	}
	


	#ifdef HAS_WINDOW
	
		/*virtual override*/ const CC*	VehicleStore::GetCommands(CC&dynamicStorage) const
		{
			VisitParkMembers([&dynamicStorage](const VehicleStore*s)
			{
				foreach (s->stored,s2)
					dynamicStorage.Add(s2->deployCommand);
			});

			return &dynamicStorage;
		}


		void						VehicleStore::VisitParkMembers(const std::function<void(VehicleStore*)>&f)
		{
			if (!park)
			{
				f(this);
				return;
			}
			foreach (park->members,m)
			{
				PVehicleStore s = m->lock();
				DBG_ASSERT__(s);
				f(s.get());
			}
		}

		void						VehicleStore::VisitParkMembers(const std::function<void(const VehicleStore*)>&f) const
		{
			if (!park)
			{
				f(this);
				return;
			}
			foreach (park->members,m)
			{
				PVehicleStore s = m->lock();
				DBG_ASSERT__(s);
				f(s.get());
			}
		}


	#endif


	/*virtual override*/ void				VehicleStore::PrototypeSetup()
	{
		Super::PrototypeSetup();


	}




	/*virtual override*/ bool VehicleStore::SharesStorageWith(const PHangar&other) const
	{
		if (!other)
			return false;
		PVehicleStore store = To<VehicleStore>::DynamicCast( other->ToEntity().shared_from_this() );
		return store && /*store->owner == owner && */store->park == park;
	}


	#ifdef CLIENT
		#ifdef HAS_WINDOW
			/*virtual override*/ void			VehicleStore::DescribeType(Description&label, UINT32 descFlags)
			{
				label.BeginBrief();
				Super::DescribeType(label,descFlags);
				label << " Max Vehicles per Store: "+String(MaxVehiclesPerStore);
				label.MarkBriefEnd();
				label << "Vehicle stores can be used to store a number of mobile units."
					" Storage capacity increases with each additional vehicle store."
					" Stored mobile units may be deployed in the surrounding area of any vehicle store of the same faction,"
					" allowing instant relocation across any distance.";
			}
		#endif
	#else

		bool						VehicleStore::StoreInPark(const PMobileEntity&v, float&atTime)
		{
			if (StoreLocally(v,atTime,false,To<VehicleStore>::Cast(shared_from_this())))
				return true;
			if (!park)
				return false;
			foreach (park->members,m)
			{
				auto locked = m->lock();
				if (locked->StoreLocally(v,atTime,false,locked))
					return true;
			}
			return false;
		}

	#endif

	/*virtual override*/ void				VehicleStore::OnDestroy(RemovalType t, float&atTime)
	{
		//Buffer0<WVehicleStore>	copy = sneighbors;
		Super::OnDestroy(t, atTime);	//disconnects from neighborhood. sneighbors should be empty now


		if (park)
		{
			if (park == owner->sharedPark)
			{
				park->FindAndRemove(this);
				#ifndef CLIENT
					for (const auto&w : park->members)
					{
						PVehicleStore s = w.lock();
						if (s)
							while (stored.IsNotEmpty() && s->StoreLocally(stored.Last().entity.lock(),atTime,false,s))
							{
								stored.EraseLast();
							}
					}
				#endif
			}
			else
			{
				for (const auto&w : park->members)
				{
					PVehicleStore s = w.lock();
					if (s && s.get() != this)
					{
						s->park.reset();
						#ifndef CLIENT
							while (stored.IsNotEmpty() && s->StoreLocally(stored.Last().entity.lock(),atTime,false,s))
							{
								stored.EraseLast();
							}
						#endif
					}
				}
				for (const auto&w : park->members)
				{
					PVehicleStore s = w.lock();
					if (s && s.get() != this && !s->park)
					{
						s->PropagatePark(PPark(new Park()));
					}
				}
			}
		}
		Hangar::OnDestroy(atTime);
	}


}

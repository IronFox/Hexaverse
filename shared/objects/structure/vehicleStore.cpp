#include <global_root.h>
#include "vehicleStore.h"
#include "../mobileEntity.h"

namespace Objects
{
	/*static*/ Geometry			VehicleStore::interConnector;




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
	#if 0
	bool	VehicleStore::StoreLocally(const PVehicle&v, float&atTime, bool setHealthToFullIfNew/*=true*/)
	{
		if (LocalIsFull() || !v || v->owner != owner || v->health <= 0)
			return false;
		#ifndef CLIENT
			v->SyncTime(atTime,timeEpsilon);
		#endif
		if (v->isInserted)
		{
			Remove(v,RemovalType::EnteredStorage,atTime,false,!v->isInsertedIntoGrid,true);
			#ifndef NO_HIBERNATION
				v->Hibernate(false,atTime);
			#endif
		}
		else
		{
			if (!GetEntity(v->id))
			{
				RegisterNewOffGrid(v,atTime,setHealthToFullIfNew);
			}
		}
		LogEvent(EventSignificance::Common,v.get()," Now stored in "+GetName());
		v->storedIn = To<VehicleStore>::Cast(shared_from_this());
		//v->location = location;
		//#ifndef CLIENT
		//	v->SignalChange(WorldEntity::PositionChanged,atTime);
		//#endif
		this->localStore.Append().vehicle = v;

		#ifdef HAS_WINDOW
			WVehicle w = v;
			this->localStore.last().deployCommand.reset(new DeployMobileCommand(v,
				[w]()
				{
					//focus = To<VehicleStore>::DynamicCast(storeEntity).get();
					if (deployingMobileEntity)
						deployingMobileEntity.reset();
					else
						deployingMobileEntity = w.lock();
				},
				[w](bool controlEnabled)
				{
					BaseCommand::Flags rs = CommonCmd::CommonButtonCheck(controlEnabled);
					PVehicleStore s = To<VehicleStore>::DynamicCast(selectedEntity);
					if (deployingMobileEntity && s == selectedEntity && deployingMobileEntity == w.lock() && deployingMobileEntity->health > 0)
						rs |= BaseCommand::Flags::IsPermaActive;
					return rs;
				}
				));
			this->localStore.last().deployCommand->RasterizeImage();
		#endif

		if (park)
			park->vehiclesStored ++;
		

		#ifndef CLIENT
			//RegisterChange(StorageChanged,atTime);	might be irrelevant. let's see
			v->RegisterChange(StorageChanged,atTime);
		#endif

		ASSERT1__(!v->isInsertedIntoGrid,GetName(),v->GetName());

		#ifdef _DEBUG
			CheckMobileEntitySanity();
		#endif

		return true;
	}
	#endif /*0*/


	#ifdef HAS_WINDOW
	
		/*virtual override*/ void		VehicleStore::VisitAllCommands(FCommandVisitor visitor, bool force)
		{
			Super::VisitAllCommands(visitor,force);
			if (storeOffset > 0 || force)
				cmdLeft->VisitMembers(visitor,force);
			index_t at = 0;
			if (!park)
			{
				foreach (stored,s2)
				{
					if (at >= storeOffset || force)
					{
						int2 coords;
						if (at >= storeOffset)
							coords = cmdLeftCoords + int2(at - storeOffset+1 ,0);
						s2->deployCommand->VisitMembers(visitor,force,coords);
					}

					at++;
					if (at >= storeOffset + MaxVisibleStoreItems && !force)
						break;
				}
				if (stored.Count() - storeOffset > MaxVisibleStoreItems || force)
					cmdRight->VisitMembers(visitor,force);
				return;
			}
			foreach (park->members,m)
			{
				PVehicleStore s = m->lock();
				DBG_ASSERT__(s);
				if (s)
				{
					foreach (s->stored,s2)
					{
						if (at >= storeOffset || force)
						{
							int2 coords;
							if (at >= storeOffset)
								coords = cmdLeftCoords + int2(at - storeOffset+1 ,0);
							s2->deployCommand->VisitMembers(visitor,force,coords);
						}

						at++;
						if (at >= storeOffset + MaxVisibleStoreItems && !force)
							break;
					}
				}
				if (at >= storeOffset + MaxVisibleStoreItems)
					break;
			}
			if (this->park->vehiclesStored - storeOffset > MaxVisibleStoreItems || force)
				cmdRight->VisitMembers(visitor,force);
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

	/*virtual override*/ void	VehicleStore::LocalCheckout(const PMobileEntity&e, const Grid::LCellIdI&targetLocation, float&atTime)
	{
		Hangar::LocalCheckout(e,targetLocation,atTime);
		if (park)
			park->vehiclesStored --;
	}


	#ifdef CLIENT
		/*virtual override*/ void	VehicleStore::Lose(const PMobileEntity&v)
		{
			Hangar::Lose(v);
			if (park)
				park->vehiclesStored--;
		}

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
			for (const auto&w : park->members)
			{
				PVehicleStore s = w.lock();
				if (s && s.get() != this)
				{
					s->park.reset();
					#ifndef CLIENT
						while (stored.IsNotEmpty() && s->StoreLocally(stored.last().entity.lock(),atTime,false,s))
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
		Hangar::OnDestroy(atTime);
	}


}

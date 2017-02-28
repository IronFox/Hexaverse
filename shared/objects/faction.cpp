#include <global_root.h>
#include "faction.h"
#include "../cast.h"
//#include "vehicle.h"
#include "mobileEntity.h"
#include "gridAndChoices.h"
#include "prototypes.h"
#include "visualSelection.h"
#include "satellite.h"
#include "vehicle.h"
#include "../sphereGridHelper.h"
#include "../runtimeConfig.h"
#include "prototypes.h"

namespace Objects
{

	#ifndef CLIENT

		/*static*/ Buffer0<WFaction>	Faction::all;
		/*static*/ PFaction				Faction::nexus;

		void	Faction::RecreateNexus(const TUniqueID&id)
		{
			nexus.reset(new Faction(id, "<Nexus>",true));
			ApplicationInterface::SignalFactionsChanged();
		}

		int		GetStorage(const MobileEntity&v, Resource res)
		{
			#ifndef ONE_COMPOUND_PER_FACTION
				return v.currentStorage.level[res];
			#else
				const Compound&compound = v.owner->compound;
				return compound.total.storage.level[res] + compound.GetOverproduction(res);
			#endif

		}

		bool	CanWithdrawFromStorage(const MobileEntity&v, Resource res, int demand)
		{
			if (demand == 0)
				return true;
			return GetStorage(v,res) >= demand;
		}


		void ChangeStorage(WorldEntity&e, float atTime, const ResourceVector&max, ResourceVector&storage, Resource res, int change)
		{
			if (change == 0)
				return;
			storage.level[res] += change;
			LogEvent(EventSignificance::Common,&e,String(res.ToString())+ " storage changed by "+String(change)+" to "+String(storage.level[res])+"/"+String(max.level[res]));
			e.RegisterChange(WorldEntity::StorageChanged,atTime);
			ASSERT_IS_CONSTRAINED__(storage.level[res] , 0, max.level[res]);
		}

		void ChangeStorage(WorkingStructure&ws, float atTime, Resource res, int change)
		{
			if (change == 0)
				return;
			ResourceVector dummy,current,max;
			ws.GetResourceVector(dummy,current,max);
			ChangeStorage(ws,atTime,max,current,res,change);
			ws.SetResourceStorage(current);
			#ifdef _DEBUG
				ResourceVector testCurrent;
				ws.GetResourceVector(dummy,testCurrent,max);
				ASSERT2__(current == testCurrent,current.ToString(),testCurrent.ToString());
			#endif
		}

		void ChangeCompoundStorage(Compound&compound, MobileEntity*originatingAt, float atTime, Resource res, int change)
		{
			ResourceVector demand;

			if (change < 0 )
			{
				ResourceVector withdraw;
				withdraw.level[res] = -change;
				compound.TryWithdrawOverproduction(withdraw);
				change = -withdraw.level[res];
				if (change == 0)
					return;
			}
			demand.level[res] = change;
			compound.ChangeStorage(demand,atTime);
		}


		void ChangeStorage(MobileEntity&v, float atTime, Resource res, int change)
		{
			if (change == 0)
				return;
			#ifndef ONE_COMPOUND_PER_FACTION
				ChangeStorage(v,atTime,v.GetMaxStorageCapacity(),v.currentStorage,res,change);
			#else
				ChangeCompoundStorage(v.owner->compound, &v, atTime,res,change);
			#endif
		}


		//void MapVisibility2(const PWorldEntity&e, const PWorldEntity&other, const OwnerParameter&checkForOwner, float&atTime)
		//{
		//	if (e == other || other->owner != checkForOwner)
		//		return;
		//	if (e->CanSee(other))
		//	{
		//		//seeing:
		//		/*bool seeingNewEntity = true;
		//		foreach (other->seenBy,s)
		//			if ((*s)->owner == e->owner)
		//			{
		//				seeingNewEntity = false;
		//				break;
		//			}*/
		//		e->seeing << other;
		//		other->seenBy << e;
		//		//if (seeingNewEntity)
		//		{
		//			//SignalAppearanceToHostileFaction(e->owner,other,atTime);
		//			e->owner->SignalAppearance(other,atTime,false);
		//		}
		//	}

		//	if (other->CanSee(e))
		//	{
		//		//beeing seen:
		//		//bool beingSeenByNewOwner = true;
		//		//foreach (e->seenBy,s)
		//		//	if ((*s)->owner == other->owner)
		//		//	{
		//		//		beingSeenByNewOwner = false;
		//		//		break;
		//		//	}
		//		e->seenBy << other;
		//		other->seeing << e;
		//		//if (beingSeenByNewOwner)
		//		{
		//			//SignalAppearanceToHostileFaction(other->owner,e,atTime);
		//			other->owner->SignalAppearance(e,atTime,false);

		//		}
		//	}
		//}

		void		ChangeOwnership( const PWorldEntity&e, const OwnerParameter&newOwner, float&atTime)
		{
			if (e->owner == newOwner)
				return;
			LogEvent( EventClass(EventSignificance::Important,EventFlavor::Loss) ,e.get(),"Changing ownership ('"+e->owner->GetName()+"' -> '"+newOwner->GetName()+"')");

			OwnerParameter oldOwner = e->owner;





			Remove(e,RemovalType::ChangingOwnership,atTime,true,false,true);
			e->owner = newOwner;
			atTime += 0.025f;
			
			if (e->IsMobile())
				InsertConstructed(To<MobileEntity>::Cast(e),atTime,true,false);
			else
				if (e->IsStructure())
					InsertConstructed(To<Structure>::Cast(e),atTime,true,false);
				else
					LogEvent(EventClass::Preset::error,e.get(), "Unexpected entity type. Cannot change ownership");
			//VerifyVisibilityNetwork();

			LogEvent( EventClass(EventSignificance::Important,EventFlavor::Gain) ,e.get(),"Changed ownership ('"+oldOwner->GetName()+"' -> '"+newOwner->GetName()+"')");
		}

		#ifdef ONE_COMPOUND_PER_FACTION
			void Faction::UpdateCompoundCoverage()
			{
				compound.allStructures.Clear();
				compound.workingStructures.Clear();
				foreach (this->structures,s)
				{
					PStructure s1 = s->lock();
					DBG_ASSERT__(s1);
					if (s1)
					{
						compound.AppendStructure(s1.get());
					}
				}
			}
		#endif



		void		Faction::RegisterLoss(const PWorldEntity&e)
		{
			if (e->IsMobile())
			{
				if (To<MobileEntity>::Cast(e)->storedIn.expired())
					RegisterActiveMobileLoss(e);
				else
					RegisterStoredMobileLoss(e);
			}
			else
				if (e->IsStructure())
					RegisterStructureLoss(e);
				else
					FATAL__("WTF is this: "+e->GetName());
		}

		void		Faction::RegisterStructureLoss(const PWorldEntity&e)
		{
			bool found0 = false;
			foreach (structures,_s)
			{
				if (_s->lock() == e)
				{
					structures.erase(_s);
					found0 = true;
					break;
				}
			}
			if (!found0)
			{
				ASSERT1__(e->IsStructure(),e->GetName());
				FATAL__(e->GetName()+": Unable to register loss for faction "+GetLogName());
				return;
			}
			if (To<Tower>::DynamicCast(e))
			{
				foreach (towers,_t)
					if (_t->lock() == e)
					{
						towers.erase(_t);
						break;
					}
			}
		}
		void		Faction::RegisterActiveMobileLoss(const PWorldEntity&e)
		{
			foreach (allMobiles,_v)
			{
				if (_v->lock() == e)
				{
					allMobiles.erase(_v);

					#ifndef NO_SATELLITES
						if (e->IsSatellite())
						{
							foreach (this->satellites,sat)
								if (sat->lock() == e)
								{
									this->satellites.erase(sat);
									break;
								}
							if (To<Satellite>::Cast(e)->IsSkyHub())
								foreach (this->skyHubs,sat)
									if (sat->lock() == e)
									{
										this->skyHubs.erase(sat);
										break;
									}
							}
					#endif
					return;
				}
			}
			ASSERT1__(e->IsMobile(),e->GetName());
			ASSERT1__(To<MobileEntity>::Cast(e)->storedIn.expired(),e->GetName());

			FATAL__(e->GetName()+": Unable to register loss for faction "+GetLogName());
		}

		void		Faction::RegisterStoredMobileLoss(const PWorldEntity&e)
		{
			foreach (storedMobiles,_v)
			{
				if (_v->lock() == e)
				{
					storedMobiles.erase(_v);
					return;
				}
			}
			ASSERT1__(e->IsMobile(),e->GetName());
			ASSERT1__(!To<MobileEntity>::Cast(e)->storedIn.expired(),e->GetName());
			FATAL__(e->GetName()+": Unable to register loss for faction "+GetLogName());
		}

		void		Faction::RegisterActiveMobileGain(const PWorldEntity&e)
		{
			PMobileEntity v = To<MobileEntity>::DynamicCast(e);
			ASSERT1__(v,e->GetName());
			ASSERT1__(v->storedIn.expired(),e->GetName());
			allMobiles << v;
			#ifndef NO_SATELLITES
				if (v->IsSatellite())
				{
					PSatellite sat = To<Satellite>::Cast(v);
					this->satellites << sat;
					if (sat->IsSkyHub())
						this->skyHubs << To<SkyHub>::Cast(sat);
				}
			#endif
		}

		bool		Faction::HasActiveMobile(const PMobileEntity&v) const
		{

			foreach (allMobiles,_v)
				if (_v->lock() == v)
					return true;
			return false;
		}
		bool		Faction::HasMobileInStorage(const PMobileEntity&v) const
		{
			foreach (storedMobiles,_v)
				if (_v->lock() == v)
					return true;
			return false;

		}

		//void		Faction::SignalLossOfTemporarySensory()
		//{
		//	static Buffer0<PWorldEntity>	toDump;
		//	seeing.VisitAllKeys([this](const PWorldEntity&e)
		//	{
		//		if (!e->IsSatellite() && !GridCanSee(e))
		//		{
		//			toDump << e;
		//		}
		//	});

		//	if (toDump.IsEmpty())
		//		return;
		//	for (const auto&e : toDump)
		//	{
		//		float t = 0;
		//		gainedVisualAt.Query(e,t);
		//		if (e->IsMobile())
		//			t = std::max(t,To<MobileEntity>::Cast(e)->timeAt);

		//		SignalDisappearance(e,t,true);
		//	}


		//	toDump.Clear();

		//}


		bool Faction::GridCanSee(const Grid::HighSetup::TVolumeDescriptor&v) const
		{
			foreach (v.coverage,st)
			{
				DBG_ASSERT_LESS__(st->range.start,Grid::CellStack::Range);
				DBG_ASSERT_LESS_OR_EQUAL__(st->range.end,Grid::CellStack::Range);
				DBG_ASSERT_LESS_OR_EQUAL__(st->range.start,st->range.end);
				const auto&stack = visibility.cellStack[st->stackID];
				for (UINT32 layer = st->range.start; layer < st->range.end; layer++)
				{
					if (stack.volumeCell[layer].IsCurrentlyVisible())
						return true;
				}
			}
			return false;
		}


		bool Faction::GridCanSee(const PWorldEntity&e) const
		{
			const UINT32 fine = Grid::low.grid.GetCell(e->location.stackID).higherEquivalent;
			const auto&c = Grid::high.grid.GetCell(fine);
			const UINT32 bottom = Grid::CellStack::ToLayer(e->location.height);
			const int entityHeight =e->GetHeight();
			const UINT32 end = std::min(Grid::CellStack::Range,bottom + (UINT32)entityHeight);
			{
				const auto&stack = visibility.cellStack[fine];
				//const UINT32 fine2 = c.neighborhood.neighborID[k];
				for (UINT32 layer = bottom; layer < end; layer++)
					if (stack.volumeCell[layer].IsCurrentlyVisible())
						return true;
			}

			//for (int k = 0; k < c.neighborhood.numNeighbors; k++)
			//{
			//	const UINT32 fine2 = c.neighborhood.neighborID[k];
			//	for (UINT32 layer = bottom; layer < end; layer++)
			//		if (visibility.cellStack[fine2].volumeCell[layer].IsVisible())
			//			return true;
			//}
			return false;

		}

		bool		Faction::CanSee(const PWorldEntity&e, float t)	const
		{
			if (!e)
				return false;
			if ( e->owner.get() == this )
				return true;
			if (!seeing.IsSet(e))
			{
				float t1;
				if (lostVisualAt.Query(e,t1))
					return t <= t1;
				return false;
			}
			float t0;
			if (gainedVisualAt.Query(e,t0))
				return t >= t0;
			return true;
		}


		bool		Faction::SignalAppearance(const PWorldEntity&e, float&t, bool cascade/*=true*/, bool stack/*=true*/, index_t incStackBy/*=1*/)
		{
			SeeingReference&ref = seeing.Set(e);

			if (ref.sightLevel == 0 || stack)
			{
				ref.sightLevel +=incStackBy;
				LogEvent(EventSignificance::Common,e.get(),this->GetLogName()+": Sight level increased to "+String(ref.sightLevel));

			}
			else
			{
				LogEvent(EventSignificance::Debug,e.get(),this->GetLogName()+": Sight level already set to "+String(ref.sightLevel));
				return false;
			}


			if (ref.sightLevel == incStackBy)
			{
				LogEvent(EventSignificance::Common,e.get(),this->GetLogName()+": Gained visual at time="+String(t));

				if (e->IsStructure())
				{
					PStructure s = To<Structure>::Cast(e);
					Structure::CommonRegisterStructure(currentState,e->location,s->GetBroadHeight(),e->GetHeight(),s->HasFoundation(),t);
					t += timeEpsilon;
				}

				float t0;
				if (lostVisualAt.QueryAndUnset(e,t0))
				{
					//ASSERT_LESS_OR_EQUAL__(t0,t)
					//we probably have to accept this happens
					t = std::max(t0,t) + timeEpsilon;
				}

				PMobileEntity v;
				PTower tower;
				e->SyncTime(t,timeEpsilon);
				LogEvent(EventSignificance::Common,e.get(),this->GetLogName()+": Gained visual of entity, time updated to "+String(t));

				ASSERT1__(gainedVisualAt.SetNew(e,t),e->id.ToString());

				if (cascade)
					ApplicationInterface::SignalAppearanceToHostileFaction(shared_from_this(),e,t);

				e->SyncTime(t,timeEpsilon);

				return true;
			}
			return false;
			//ASSERT_EQUAL__(canSee.count(),gainedVisualAt.Count());
		}

		void		Faction::SignalDisappearance(const PWorldEntity&e, float&t, bool force, bool signalConnectedClient/*=true*/)
		{
			SeeingReference*ref = seeing.QueryPointer(e);

			if (ref)
			{
				ref->sightLevel --;
				LogEvent(EventSignificance::Common,e.get(),this->GetLogName()+": Sight level reduced to "+String(ref->sightLevel));
			}
			else
				LogEvent(EventSignificance::Common,e.get(),this->GetLogName()+": ref not set");


			if (ref && (!ref->sightLevel || force))
			{
				LogEvent(EventSignificance::Common,e.get(),this->GetLogName()+": Lost visual at time="+String(t));
				if (ref->sightLevel)
				{
					LogEvent(EventSignificance::Common,e.get(),this->GetLogName()+": WARNING: Visual loss was forced");
				}

				if (e->IsStructure())
				{
					PStructure s = To<Structure>::Cast(e);
					Structure::CommonUnregisterStructure(currentState,e->location,s->GetBroadHeight(),e->GetHeight(),s->HasFoundation(),t);
					t += timeEpsilon;
				}

				float t0;
				if (gainedVisualAt.QueryAndUnset(e,t0))
				{
					t = std::max(t0,t);	//permissive. whatever. seriously
					LogEvent(EventSignificance::Common,e.get(),this->GetLogName()+": Lost visual, time updated to "+String(t));
				}
				PMobileEntity v;
				PTower tower;
				e->SyncTime(t,timeEpsilon);

				ASSERT1__(lostVisualAt.SetNew(e,t-timeEpsilon),e->id.ToString());
				seeing.Unset(e);
				if (signalConnectedClient)
					ApplicationInterface::SignalDisappearanceToHostileFaction(shared_from_this(),e,t);

				e->SyncTime(t,timeEpsilon);
			}
			//ASSERT_EQUAL__(canSee.count(),gainedVisualAt.Count());

		}

		void		Faction::ChangeSensory(const Grid::HighSetup::TVolumeDescriptor&desc, float t, short changeBy)
		{
			visibility.ChangeSensory(desc, t, changeBy);
		}

		//void		Faction::ChangeSensory(const Grid::LCellIdI&center, float radius, float t, short change)
		//{
		//	visibility.ChangeSensory(center, radius, t, change);
		//}

		void		Faction::ChangeSensory(const Grid::SkySensorState&state, float t, short change)
		{
			visibility.ChangeSensory(state, t, change);
		}

		template <typename T>
		count_t		CountRawSensorCoverage(const TVec3<>&center, const BasicBuffer<T>&scope)
		{
			count_t rs = 0;
			foreach (scope,s)
			{
				auto sat = s->lock();
				if (sat && Sphere<>(sat->GetWorldPosition(),sat->GetScanningRange()).Contains(center))
					rs++;
			}
			return rs;
		}

		template <typename T>
		count_t		CountRawSensorCoverage(const Sphere<>&sphere, const BasicBuffer<T>&scope)
		{
			count_t rs = 0;
			foreach (scope,s)
			{
				auto sat = s->lock();
				if (sat && Sphere<>(sat->GetWorldPosition(),sat->GetScanningRange()).Intersects(sphere))
					rs++;
			}
			return rs;
		}

		
		count_t		Faction::CountOffGridCoverage(int channelHeight, const TVec3<>&p)	const
		{
			#ifndef NO_SATELLITES
				if (Satellite::IsSatelliteHeight(channelHeight))
					return CountRawSensorCoverage(p,satellites);
			#endif

			return 0;
		}

		count_t		Faction::CountOffGridCoverage(const Sphere<>&p)	const
		{
			count_t rs = 0;
			#ifndef NO_SATELLITES
				rs += CountRawSensorCoverage(p,satellites);
			#endif

			return rs;
		}



		void		Faction::CheckGridVisibility(const PWorldEntity&e,float&timeAt)
		{
			if (!e || e->owner.get() == this)
				return;
			LogEvent(EventSignificance::Common,e.get(),this->GetLogName()+ ": CheckGridVisibility at time "+String(timeAt));
			if (seeing.IsSet(e))
			{
				if (e->IsAboveGrid())
				{
					count_t cover = CountOffGridCoverage(e->location.height, e->GetWorldPosition());
					if (cover == 0)
						SignalDisappearance(e,timeAt,true);
					else
						seeing[e].sightLevel = cover;
				}
				else
					if (!GridCanSee(e))
					{
						SignalDisappearance(e,timeAt,true);
					}
			}
			else
			{
				if (e->IsAboveGrid())
				{
					count_t cover = CountOffGridCoverage(e->location.height, e->GetWorldPosition());
					if (cover > 0)
						SignalAppearance(e,timeAt,true,false,cover);
				}
				else
					if (GridCanSee(e))
					{
						LogEvent(EventSignificance::Common,e.get(),this->GetLogName()+ ": SignalAppearance at time "+String(timeAt));
						SignalAppearance(e,timeAt,true,false);
					}
			}
		}


		void		Faction::RegisterSensorCreation(const Sphere<>&sphere, int sensorHeight, float&t, bool checkChangesNow, Buffer0<PWorldEntity>*newContacts /*= nullptr*/)
		{
			//if (sensorHeight <= Grid::CellStack::TopHeight)
			{
				static Grid::HighSetup::TVolumeDescriptor	desc0;
				Grid::high.GenerateVolume(sphere,desc0);
				ChangeSensory(desc0,t,1);
				if (checkChangesNow)
					CheckChanges(t);

				static Grid::LowSetup::TVolumeDescriptor	desc;
				Grid::low.GenerateVolume(Sphere<>(sphere.center,sphere.radius+1.f),desc);
				foreach (desc.coverage,c)
				{
					UINT32 min = c->range.start;
					if (min)
						min--;
					for (UINT l = min; l < c->range.end; l++)
					{
						PWorldEntity e = GetEntity(Grid::LCellIdI(c->stackID,Grid::CellStack::ToHeight(l)));
						if (!e || e->owner.get() == this)
							continue;
						if (GridCanSee(e))
						{
							if (SignalAppearance(e,t,true,false) && newContacts)
								newContacts->Append(e);
						}
					}
				}
			}
			#ifndef NO_SATELLITES
				if (Objects::Satellite::IsSatelliteHeight(sensorHeight))
				{
					//count_t coverage = CountSatelliteCoverage(sat->system.w.xyz);
					//LogEvent(EventSignificance::Debug,"Registering orbital sensor sphere "+sphere.ToString());
					foreach (Objects::satellites,s)
					{
						PSatellite sat = *s;
						if (sat->owner.get() != this && sphere.Contains(sat->GetWorldPosition()))
						{
							if (SignalAppearance(sat,t,true) && newContacts)
								newContacts->Append(sat);
						}
					}
				}
			#endif
		}

		//void		Faction::RegisterSensorMotion(const Sphere<>&from, const Sphere<>&to, float t);
		void		Faction::RegisterSensorRemoval(const Sphere<>&sphere, int sensorHeight, float&t, bool checkChangesNow)
		{
			//if (sensorHeight <= Grid::CellStack::TopHeight)
			{
				static Grid::HighSetup::TVolumeDescriptor	desc0;
				Grid::high.GenerateVolume(sphere,desc0);
				ChangeSensory(desc0,t,-1);
				if (checkChangesNow)
					CheckChanges(t);
			}
//			float escaped = sqr(sphere.radius+2);
			//if (isSatellite)
			//	LogEvent(EventSignificance::Debug,"Unregistering orbital sensor sphere "+sphere.ToString());

			static Buffer<PWorldEntity>	disappear;

			bool offGrid = sensorHeight > Grid::CellStack::TopHeight;

			this->seeing.VisitAllKeys([this,offGrid, sphere](const PWorldEntity&s)
			{
				if (s->IsAboveGrid() != offGrid)
					return;
				if (s->IsAboveGrid())
				{
					if (sphere.Contains(s->GetWorldPosition()))
					{
						disappear << s;
					}
				}
				else
					if (!GridCanSee(s))
					{
						disappear << s;
					}
			});
			foreach (disappear,s)
				SignalDisappearance(*s,t);
			disappear.Clear();
		}

		void		Faction::RegisterSensorCreation(const Grid::SkySensorState&state, float&t, bool checkChangesNow, Buffer0<PWorldEntity>*newContacts /*= nullptr*/)
		{
			ChangeSensory(state,t,+1);
			if (checkChangesNow)
				CheckChanges(t);


			ForeachWorldEntity([this,&t,newContacts](const PWorldEntity&other)
			{
				if (other->owner.get() == this || other->IsAboveGrid())
					return;
				if (GridCanSee(other))
				{
					SignalAppearance(other,t,true,false);
					if (newContacts)
						newContacts->Append(other);
				}
			},false);
		}
		void		Faction::RegisterSensorRemoval(const Grid::SkySensorState&state, float&t, bool checkChangesNow)
		{
			ChangeSensory(state,t,-1);
			if (checkChangesNow)
				CheckChanges(t);

			static Buffer<PWorldEntity>	disappear;
			this->seeing.VisitAllKeys([this](const PWorldEntity&s)
			{
				if (!s->IsAboveGrid() && !GridCanSee(s))
					disappear << s;
			});
			foreach (disappear,s)
				SignalDisappearance(*s,t);
			disappear.Clear();

		}




		void		Faction::CheckChanges(float t)
		{
			visibility.CheckChanges(t);
		}


		static bool CheckLimit(count_t currentCount, count_t maxCount, String&errorOut)
		{
			if (currentCount < maxCount)
			{
				errorOut = "";
				return true;
			}
			errorOut = "Unit limit reached ("+String(maxCount)+")";
			return false;
		}

		bool		Faction::CanLive(String&whyNot) const
		{
			bool hasMineralGeneratorCycle = hasFuelMine && hasMineralMine && hasGenerator;
			#ifndef ONE_COMPOUND_PER_FACTION
				if (!hasMineralContainer)
					hasMineralGeneratorCycle = false;
			#endif

//				static Buffer0<String> missing;
			if (!hasMineralGeneratorCycle)
			{
				whyNot = "";
				if (!hasFuelMine)
					whyNot += "Fuel Mine";
				if (!hasMineralMine)
				{
					if (whyNot.IsNotEmpty())
						whyNot += ", ";
					whyNot += "Mineral Mine";
				}

				#ifndef ONE_COMPOUND_PER_FACTION
					if (!hasMineralContainer)
					{
						if (whyNot.IsNotEmpty())
							whyNot += ", ";
						whyNot += "Mineral Container";
					}
				#endif
				if (!hasGenerator)
				{
					if (whyNot.IsNotEmpty())
						whyNot += ", ";
					whyNot += "Generator";
				}



				whyNot = "Lacking endless mineral production facility ("+whyNot+")";
				return false;
			}
			if (hasTerraformer)
				return true;
			bool hasVehicleProductionCycle = hasFuelMine && hasMineralMine && hasVehiclePlant && hasGenerator;
			if (!hasVehicleProductionCycle)
			{
				whyNot = "Vehicle Plant";
				whyNot = "Lacking vehicle production facility ("+whyNot+")";

				return false;
			}

			return true;
		}



		bool		Faction::CanSpawnType(const PWorldEntity&e, String&errorOut)	const
		{
			if (this == nexus.get())
				return true;
			if (e->IsMobile())
			{
				if (e->IsVehicle())
				{
					const Component::VehicleBase*base = To<Vehicle>::Cast(e)->base;

					if (!researchState.CheckUnlocked(*base,errorOut))
						return false;
				}
				const Component::Turret*turret = To<MobileEntity>::Cast(e)->turret;
				ConstName type = turret->constName;
				return researchState.CheckUnlocked(*turret,errorOut) && CheckLimit(CountMobileType(type), RuntimeConfig::UnitLimits::Get(type), errorOut);
			}
			ConstName type = e->GetCTypeName();
			if (e->IsStructure())
				return researchState.CheckUnlocked(type,errorOut) && CheckLimit(CountStructureType(type), RuntimeConfig::UnitLimits::Get(type), errorOut);
			FATAL__("Unknown type: "+e->GetName());
			return false;
		}
		bool		Faction::CanSpawnFromStorage(const PWorldEntity&e, String&errorOut)	const
		{
			return CanSpawnType(e,errorOut);
		}

		count_t		Faction::CountMobileType(ConstName turretType) const
		{
			count_t rs = 0;
			foreach (this->allMobiles, v0)
			{
				PMobileEntity m = v0->lock();
				if (m->turret->constName == turretType)
					rs ++;
			}
			return rs;
		}

		count_t		Faction::CountStructureType(ConstName structureType) const
		{
			count_t rs = 0;
			foreach (this->structures, s0)
			{
				if (s0->lock()->GetCTypeName() == structureType)
					rs ++;
			}
			return rs;
		}




		void		Faction::RegisterExplosionCrater(const Grid::HCellIdI&center, float radius, float t)
		{
			visibility.RegisterExplosionCrater(center, radius, t);


		}


		//void		Faction::RegisterSensorCreation(const Grid::LCellIdI&center, float radius, float t)
		//{
		//	ChangeSensory(center,radius,t,1);
		//	CheckChanges(t);

		//}
		//void		Faction::RegisterSensorMotion(const Grid::LCellIdI&oldCenter, const Grid::LCellIdI&newCenter, float radius, float t)
		//{
		//	ChangeSensory(oldCenter,radius,t,-1);
		//	ChangeSensory(newCenter,radius,t,1);
		//	CheckChanges(t);
		//}
		//void		Faction::RegisterSensorRemoval(const Grid::LCellIdI&center, float radius, float t)
		//{
		//	ChangeSensory(center,radius,t,-1);
		//	CheckChanges(t);
		//}

	#else
		/*static*/ Buffer0<PFaction>			Faction::allKnown;
		/*static*/ GenericHashTable<TUniqueID,PFaction,TUniqueID>	Faction::idMap;
		/*static*/ PFaction						Faction::mine;

		/*static*/ Buffer0<Marker, Strategy::Adopt>	Marker::all; 
		/*static*/ GenericHashTable<TUniqueID, index_t, TUniqueID>	Marker::idMap,
								Marker::referenceIDMap,
								Marker::visitedSystemIDMap;
		/*static*/ Buffer0<Marker::TVisited>	Marker::visitedSystems;
		/*static*/ SharedContainer<Structure>		Marker::structureGhosts;
		/*static*/ SharedContainer<MobileEntity>	Marker::mobileGhosts;

		/*static*/ PFaction				Faction::FindByID(const TUniqueID&id)
		{
			PFaction*f = idMap.QueryPointer(id);
			if (f)
				return *f;
			return PFaction();
		}


		/*static*/ PFaction				Faction::GetOrCreate(const TUniqueID&id)
		{
			{
				PFaction*rs = idMap.QueryPointer(id);
				if (rs)
					return *rs;
			}
			PFaction rs(new Faction(id));
			idMap.Set(id,rs);
			allKnown << rs;
			return rs;
		}



		void	ChangeOwnership( const PWorldEntity&e,const OwnerParameter&newOwner, bool isNowMine)
		{
			LogEvent(EventSignificance::Debug, e.get(),"Changing ownership to "+newOwner->GetName());
			PStructure s = To<Structure>::DynamicCast(e);
			#ifdef CLIENT
				if (e->isOwnedByMe)
				{
					float t = 0.f;
					if (!s && e->IsMobile())
						e->UnregisterSensory(To<MobileEntity>::Cast(e)->preAnimationLocation,t,false);
					else
						e->UnregisterSensory(t,false);
					#ifdef THOROUGLY_DEBUG_SENSORY
						VerifySensory();
					#endif
				}
			#endif
			LogEvent(EventSignificance::Important, e.get(),"Changed ownership to "+newOwner->GetName());	//this is also notified from the server, hence: common
			if (s)
			{
				float t = 0;
				s->DisconnectFromNeighborhood(t);
			}
			e->owner = newOwner;
			e->isOwnedByMe = isNowMine;

			#ifdef CLIENT
				if (e->isOwnedByMe)
				{
					float t = 0.f;
					if (!s && e->IsMobile())
						e->RegisterSensory(To<MobileEntity>::Cast(e)->preAnimationLocation,t,false);
					else
						e->RegisterSensory(t,false);
					#ifdef THOROUGLY_DEBUG_SENSORY
						VerifySensory();
					#endif
				}
			#endif
			//if (s)
			//	InjectIntoNeighborhood(s,0);

			
			if (s)	
				RefreshCompoundCoverage(0);
		}


	#endif


	void			ForeachFaction(const std::function<void(const PFaction&)>&func)
	{
		#ifdef CLIENT
			foreach (Faction::allKnown,a)
				func(*a);
		#else
			
			if (Faction::nexus)
				func(Faction::nexus);
			for (index_t i = 0; i < Faction::all.Count(); i++)
			{
				PFaction f = Faction::all[i].lock();
				if (!f)
				{
					Faction::all.Erase(i);
					i--;
				}
				else
					func(f);
			}
		#endif
	}



	void			ForeachMobileWorldEntity(const std::function<void(const PMobileEntity&)>&func, bool includeGhosts)
	{
		vehicles.Foreach(func);
		#ifndef NO_SATELLITES
			satellites.Foreach(func);
		#endif
		#ifdef CLIENT
			if (includeGhosts)
				Marker::mobileGhosts.Foreach(func);
		#endif
	}

	void			ForeachMobileEntityIncludingStored(const std::function<void(const PMobileEntity&)>&func, bool includeGhosts)
	{
		ForeachMobileWorldEntity(func,includeGhosts);
		storedMobileEntities.Foreach(func);
	}


	void			ForeachWorldEntityIncludingStored(const std::function<void(const PWorldEntity&)>&func, bool includeGhosts)
	{
		ForeachWorldEntity(func,includeGhosts);
		storedMobileEntities.Foreach(func);
	}

	void ForeachWorldEntity(const std::function<void(const PWorldEntity&)>&func, bool includeGhosts)
	{
		ForeachMobileWorldEntity(func,includeGhosts);
		structures.Foreach(func);
		#ifdef CLIENT
			if (includeGhosts)
				Marker::structureGhosts.Foreach(func);
		#endif
	}


	
	#ifdef CLIENT


		void Marker::UpdateAge(const RuntimeConfig::RoundNumber& currentRoundNumber)
		{
			age = currentRoundNumber - createdInRound;
			ASSERT_LESS__(age,100000);	//overflow otherwise, 100000 seems a good limit
			if (visualization)
				visualization->health = -(int)(age);
			if (type == Type::Visited)
			{
				index_t at = visitedSystemIDMap.Require(id);
				visitedSystems[at].age = age;
			}
		}

		/*static*/ void				Marker::GetOutdated(BasicBuffer<Marker>&outOudated, const RuntimeConfig::RoundNumber& currentRoundNumber, UINT32 deathOutdatedAfterRounds)
		{
			outOudated.Clear();
			foreach(all, m)
			{
				m->UpdateAge(currentRoundNumber);
				switch (m->type)
				{
					case Type::EntityDeath:
						
						if (m->age > deathOutdatedAfterRounds)
						{
							LogEvent(EventSignificance::Debug,nullptr,"Marker "+m->GetName()+" considered outdated after "+String(m->age)+" round(s)");

							outOudated << *m;
						}
					break;
					case Type::HostileContact:
					{
						//TVec3<> center = Grid::ToWorld(m->location);
						//if ()
						bool seen = m->age > 1 &&	//don't delete hostile markers in round <2. grid might be outdated
									 m->visualization && Grid::actualWorld.GridCanSee(m->location,m->visualization->GetHeight(),m->visualization->IsBroad());
						/*foreach(structures, s)
							if ((*s)->isOwnedByMe && (*s)->CanSee(center, 0))
							{
								LogEvent(EventSignificance::Debug,nullptr,"Marker "+m->GetName()+" considered visible (seen by "+(*s)->GetName()+")");
								seen = true;
								break;
							}
						if (!seen)
							foreach(vehicles, s)
								if ((*s)->isOwnedByMe && (*s)->CanSee(center, 0))
								{
									LogEvent(EventSignificance::Debug,nullptr,"Marker "+m->GetName()+" considered visible (seen by "+(*s)->GetName()+")");
									seen = true;
									break;
								}
						if (!seen)
							foreach(satellites, s)
								if ((*s)->isOwnedByMe && (*s)->CanSee(center, 0))
								{
									LogEvent(EventSignificance::Debug,nullptr,"Marker "+m->GetName()+" considered visible (seen by "+(*s)->GetName()+")");
									seen = true;
									break;
								}*/
						if (seen)
							outOudated << *m;
					}
					break;

				}
		

			}
		}


		/*static*/ void				Marker::Create(const TUniqueID&id, Type type, const Grid::LCellIdI&location, const RoundNumber& createdInRound, const TUniqueID&referenceID, const TUniqueID&referenceOwnerID, ConstName referenceType[2], const RoundNumber& currentRound)
		{
			const index_t newIndex = all.Count();
			Marker&m = all.Append();
			m.id = id;
			m.referenceEntityOwner = referenceOwnerID;
			m.location = location;
			m.createdInRound = createdInRound;
			m.age = currentRound - createdInRound;

			m.type = type;
			ASSERT__(idMap.SetNew(id, newIndex));
			if (m.UtilizesReferenceEntity())
			{
				m.referenceEntity = referenceID;
				referenceIDMap.Set(referenceID,newIndex);
				m.referencedType[0] = referenceType[0];
				m.referencedType[1] = referenceType[1];
				if (referenceType[1] != ConstName::Empty)
				{
					//vehicle
					Component::VehicleBase* base = Component::GetVehicleBaseByName(referenceType[0]);
					Component::Turret* turret = Component::GetTurretByName(referenceType[1]);
					ASSERT1__(base, referenceType);
					ASSERT1__(turret, referenceType);
					m.visualization.reset(new Vehicle(OwnerParameter(), location, base, turret));
					mobileGhosts << To<MobileEntity>::Cast( m.visualization );
				}
				else
				{
					//structure
					PWorldEntity prototype = Objects::GetNonVehiclePrototypeByName(referenceType[0]);
					if (prototype)
					{
						ASSERT1__(prototype, referenceType);
						m.visualization = (prototype->CreateNewInstance(OwnerParameter(), location, 0));
						if (prototype->IsStructure())
							structureGhosts << To<Structure>::Cast(m.visualization);
						else
							if (prototype->IsMobile())
								mobileGhosts << To<MobileEntity>::Cast( m.visualization);
							else
								LogEvent(EventSignificance::NonFatalError,"Unable to classify ghost for '"+prototype->GetTypeName()+"'");
					}
					else
						LogEvent(EventSignificance::NonFatalError,"Unable to create ghost for type #"+String((int)referenceType[0]));

				}

				if (m.visualization)
				{
					if (m.visualization->IsMobile())
						To<MobileEntity>::Cast( m.visualization)->UpdateSystem();
					m.visualization->id = referenceID;
					m.visualization->owner = referenceOwnerID == TUniqueID::Empty() ? PFaction() : Faction::GetOrCreate(referenceOwnerID);
					m.visualization->health = -(int)m.age;
					m.visualization->isOwnedByMe = (m.visualization->owner && m.visualization->owner->isOwnedByMe) || (!m.visualization->owner && m.type == Type::EntityDeath);
					m.visualization->UpdateNiceName();
				}
			}
			else
			{
				if (type == Type::Visited)//should be always true but regardless
				{
					TMatrix4<> system;
					Mat::makeAxisSystem(Grid::ToWorld(m.location),Grid::low.cells[m.location.stackID].centerVertex.xyz,2,system);
					index_t systemID = visitedSystems.Count();
					visitedSystems.Append().system = system;
					visitedSystems.last().age = m.age;
					visitedSystemIDMap.Set(id,systemID);
				}
				else
					FATAL__("Not anticipating this");
			}

			//m.UpdateAge(currentRound);

		}
		/*static*/ void				Marker::Destroy(const TUniqueID&id, bool controlEnabled)
		{
			index_t index;
			ASSERT1__(idMap.QueryAndUnset(id, index),id.ToString());	//this mustn't happen anymore
			Marker&m = all[index];
			if (m.UtilizesReferenceEntity())
			{
				ASSERT__(referenceIDMap.Unset(m.referenceEntity));
				if (m.visualization)
				{
					if (m.visualization->IsMobile())
					{
						mobileGhosts.FindAndErase(m.visualization);
					}
					elif(m.visualization->IsStructure())
					{
						structureGhosts.FindAndErase(m.visualization);
					}
					#ifdef HAS_WINDOW
						if (selectedEntity == m.visualization)
						{
							Deselect(controlEnabled);
						}
					#endif
					m.visualization.reset();
				}
			}
			else
			{
				if (m.type == Type::Visited)//should be always true but regardless
				{
					index_t sysID;
					if (visitedSystemIDMap.QueryAndUnset(id,sysID))
					{
						visitedSystems.Erase(sysID);
						visitedSystemIDMap.VisitAllValues([sysID](index_t&value)
						{
							if (value > sysID)
								value--;
						});
					}
				}
				else
					FATAL__("Not anticipating this");	//assuming an error for now

			}
			referenceIDMap.VisitAllValues([index](index_t&value)
			{
				if (value > index)
					value--;
			});
			idMap.VisitAllValues([index](index_t&value)
			{
				if (value > index)
					value--;
			});
			all.Erase(index);
			#ifdef _DEBUG
				foreach(all, m)
				{
					index_t at;
					ASSERT__(idMap.Query(m->id, at));
					ASSERT_LESS__(at, all.Count());
					ASSERT__(m == all + at);
				}
			#endif
		}

		/*static*/ Marker*			Marker::Find(const Grid::LCellIdI&loc)
		{
			foreach (all,m)
				if (m->visualization && m->visualization->location == loc)
					return m;
			return nullptr;
		}

		/*static*/ Marker*			Marker::FindByReference(const TUniqueID&referenceID)
		{
			index_t index;
			if (referenceIDMap.Query(referenceID, index))
				return all + index;
			return nullptr;
		}

		/*static*/ Marker*			Marker::Find(const TUniqueID&referenceID)
		{
			index_t index;
			if (idMap.Query(referenceID, index))
				return all + index;
			return nullptr;
		}

		/*static*/ void				Marker::GetDeletionCandidates(BasicBuffer<TCandidate>&candidates)
		{
			candidates.Clear();
			for (index_t i = 0; /*i < MaxMarkersPerFaction / 10 && */i < all.Count(); i++)
			{
				candidates.Append().markerID = all[i].id;
				candidates.last().priority = all[i].age;
			}
			std::sort(candidates.begin(),candidates.end());
			candidates.Truncate(MaxMarkersPerFaction / 10);
		}

		/*static*/ void				Marker::FlushAll()
		{
			all.Clear();
			idMap.Clear();
			referenceIDMap.Clear();
			mobileGhosts.Clear();
			structureGhosts.Clear();
			visitedSystems.Clear();
			visitedSystemIDMap.Clear();
		}
	#else


		String Convert(ConstName name)
		{
			if (name == ConstName::Empty)
				return "";
			PWorldEntity e = GetPrototypeByName(name);
			if (e)
				return e->GetTypeName();
			Component::Turret* t = Component::GetTurretByName(name);
			if (t)
				return t->typeName;
			Component::VehicleBase* b = Component::GetVehicleBaseByName(name);
			if (b)
				return b->typeName;
			return "";
		}

		ConstName Convert(const String&name)
		{
			if (name == "")
				return ConstName::Empty;
			PWorldEntity e = GetPrototypeByName(name);
			if (e)
				return e->GetCTypeName();
			Component::Turret* t = Component::GetTurretByName(name);
			if (t)
				return t->constName;
			Component::VehicleBase* b = Component::GetVehicleBaseByName(name);
			if (b)
				return b->constName;
			return ConstName::Empty;
		}
		/*virtual override*/	serial_size_t		Marker::GetSerialSize(bool exportSize) const
		{
			serial_size_t size = sizeof(TUniqueID);	//marker id;
			size += 1;	//type
			size += sizeof(createdInRound);
			size += sizeof(location);
			if (UtilizesReferenceEntity())
			{
				size += 2 * sizeof(TUniqueID);	//referenced entity id


				String t0 = Convert(this->referencedType[0]);
				String t1 = Convert(this->referencedType[1]);
				size += t0.GetSerialSize(true);
				size += t1.GetSerialSize(true);
//				size += sizeof(this->referencedType);
				//this->referencedType.GetSerialSize(exportSize);
			}
			return size;
		}
		/*virtual override*/	bool		Marker::Serialize(IWriteStream&outStream, bool exportSize) const
		{
			if (!outStream.WritePrimitive(id) || !outStream.WritePrimitive((UINT8)type)
				 || !outStream.WritePrimitive(createdInRound)
				|| !outStream.WritePrimitive(location)
				)
				return false;
			if (UtilizesReferenceEntity())
			{
				if (!outStream.WritePrimitive(this->referenceEntity) || !outStream.WritePrimitive(this->referenceEntityOwner))
					return false;
				String t0 = Convert(this->referencedType[0]);
				String t1 = Convert(this->referencedType[1]);
				return t0.Serialize(outStream,true) && t1.Serialize(outStream,true);
			}
			return true;
		}
		/*virtual override*/	bool		Marker::Deserialize(IReadStream&inStream, serial_size_t fixedSize)
		{
			UINT8 typei;
			if (!inStream.ReadPrimitive(id) || !inStream.ReadPrimitive(typei) || !inStream.ReadPrimitive(createdInRound)
				||  !inStream.ReadPrimitive(location)
				)
				return false;
			type = Type::Reinterpret(typei);
			if (UtilizesReferenceEntity())
			{
				if (fixedSize != EmbeddedSize)
				{
					fixedSize -= sizeof(TUniqueID) + 1 + sizeof(createdInRound) + sizeof(TUniqueID) + sizeof(location);
					{
						fixedSize -= sizeof(TUniqueID);
					}
				}
				String t0,t1;
				if (!inStream.ReadPrimitive(this->referenceEntity))
					return false;

				if (ApplicationInterface::globalVersionOfCurrentFile >= ApplicationInterface::FileVersions::MarkersHaveOwner)
				{
					if (!inStream.ReadPrimitive(this->referenceEntityOwner))
						return false;
				}
				else
					this->referenceEntityOwner = TUniqueID::Empty();
				if (!t0.Deserialize(inStream,EmbeddedSize) || !t1.Deserialize(inStream,EmbeddedSize))
					return false;
				this->referencedType[0] = Convert(t0);
				this->referencedType[1] = Convert(t1);
				//this->referenceEntityType.Deserialize(inStream, fixedSize);
			}
			else
			{
				referenceEntity = TUniqueID::Empty();
				referencedType[0] = referencedType[1] = ConstName::Empty;
			}
			return true;
		}


		Marker&		Faction::CreateMarker(const TUniqueID&id, bool priveleged, Marker::Type type, const TUniqueID&referenceID, const TUniqueID&referenceOwnerID)
		{
			ASSERT__(CanCreateMarker(priveleged)); 
			Marker&rs = markers.Append(); 
			rs.id = id; 
			rs.type = type;
			rs.referenceEntity = referenceID;
			rs.referenceEntityOwner = referenceOwnerID;
			markerMap.Set(rs.id, markers.Count() - 1); 
			if (rs.UtilizesReferenceEntity())
				ASSERT__(referenceMap.SetNew(referenceID,markers.Count()-1));

			return rs; 
		}



		void		Faction::RestoreMarkers(const ArrayData<Marker>&source)
		{
			ASSERT__(markers.IsEmpty());
			ASSERT__(markerMap.IsEmpty());
			markers = source;

			foreach(markers, m)
			{
				markerMap.Set(m->id, m - markers.pointer());
				ASSERT__(m->location != Grid::LCellIdI::Invalid);
				if (m->UtilizesReferenceEntity())
					ASSERT__(referenceMap.SetNew(m->referenceEntity,m - markers.pointer()));

			}
		}

		bool		Faction::DestroyMarker(const TUniqueID&markerID)
		{
			index_t at;
			if (!markerMap.QueryAndUnset(markerID, at))
			{
				LogEvent(EventClass::Preset::error,nullptr,"Unable to destroy Marker. ID not set " + markerID.ToString());
				return false;
			}
			ASSERT_LESS__(at, markers.Count());
			if (markers[at].UtilizesReferenceEntity())
				ASSERT__(referenceMap.Unset(markers[at].referenceEntity));

			markers.Erase(at);
			markerMap.VisitAllValues([at](index_t&val)
			{
				if (val > at)
					val--;
			});
			referenceMap.VisitAllValues([at](index_t&val)
			{
				if (val > at)
					val--;
			});
			

			#ifdef _DEBUG
				foreach(markers, m)
				{
					ASSERT__(markerMap.Query(m->id, at));
					ASSERT_LESS__(at, markers.Count());
					ASSERT__(m == markers + at);
					if (m->UtilizesReferenceEntity())
					{
						ASSERT__(referenceMap.Query(m->referenceEntity,at));
						ASSERT_LESS__(at, markers.Count());
						ASSERT__(m == markers + at);
					}
				}
			#endif
			return true;
		}
		
		Marker*		Faction::FindMarker(const TUniqueID&markerID)
		{
			index_t at;
			if (markerMap.Query(markerID,at))
				return markers + at;
			return nullptr;

		}

		Marker*		Faction::FindReferenceMarker(Marker::Type type, const TUniqueID&referenceID)
		{
			index_t at;
			if (referenceMap.Query(referenceID,at) && markers[at].type == type)
				return markers + at;
			return nullptr;

		}

		/*static*/ void	Faction::Kill(const PFaction&f)
		{
			LogEvent(EventSignificance::Debug,nullptr, "Erasing faction "+f->id.ToString());
			static Buffer0<PWorldEntity>	toDelete;
			for (index_t i = 0; i < f->structures.Count(); i++)
				if (!f->structures[i].lock()->IsIndestructible())
				{
					toDelete << f->structures[i].lock();
				}

			for (index_t i = 0; i < f->allMobiles.Count(); i++)
				//if (vehicles[i]->owner == f)
				{
					toDelete << f->allMobiles[i].lock();
				}
			float t=0;
			foreach (toDelete,del)
				Remove(*del,Objects::RemovalType::ControlledExplosion,t,false);
			toDelete.Clear();

			if (f != nexus)
			{
				for (index_t i = 0; i < all.Count(); i++)
					if (all[i].lock() == f)
					{
						all.Erase(i);
						break;
					}
				LogEvent(EventClass::Preset::factionDeath, "Faction terminated: " + f->GetLogName());
				ApplicationInterface::SignalFactionsChanged();
			}
			else
				LogEvent(EventClass::Preset::remoteControlError, "Destructible entities destroyed, but cannot terminate nexus faction itself");
		}

		/*static*/ PFaction			Faction::FindByID(const TUniqueID&id)
		{
			if (nexus->id == id)
				return nexus;
			foreach (all,_f)
			{
				PFaction f = _f->lock();
				if (f && f->id == id)
					return f;
			}
			return PFaction();
		}


//		void		VerifyVisibilityNetwork(const PWorldEntity&e)
//		{
//			foreach (e->seeing,s)
//			{
//				ASSERT2__(vehicles.Contains(*s) || structures.Contains(*s) || satellites.Contains(*s), (e)->GetName(), (*s)->GetName());
//				ASSERT2__(e->CanSee(*s), (e)->GetName(), (*s)->GetName());
//				ASSERT2__((*s)->seenBy.Contains(e), (e)->GetName(), (*s)->GetName());
//			}
//			
//			foreach (e->seenBy,s)
//			{
//				ASSERT2__(vehicles.Contains(*s) || structures.Contains(*s) || satellites.Contains(*s), (e)->GetName(), (*s)->GetName());
////					ASSERT__((*s)->CanSee(*v));	done in other pass
//				ASSERT2__((*s)->seeing.Contains(e), (e)->GetName(), (*s)->GetName());
//			}
//
//			ForeachWorldEntity([e](const PWorldEntity&w)
//			{
//				if (e->owner == (w)->owner)
//					return;
//				bool visible = e->CanSee(w);
//				if (visible)
//					ASSERT2__(e->seeing.Contains(w), (e)->GetName(), (w)->GetName());
//			}
//			);
//		}
//
//		void			VerifyVisibilityNetwork()
//		{
//			foreach (vehicles,v)
//			{
//				VerifyVisibilityNetwork(*v);
//
//			}
//			foreach (structures,s)
//			{
//				VerifyVisibilityNetwork(*s);
//			}
//			foreach (satellites,s)
//			{
//				VerifyVisibilityNetwork(*s);
//			}
//		}
//



	#endif

	float Next(std::mt19937&random)
	{
		return float(random()) / float(random.max());
	}

#if 0
	/*static*/ void Faction::IDToColor(TVec3<>&rs, const TUniqueID&id, int iteration/*=0*/)
	{
		USHORT	v0 = *(const USHORT*)id.bytes,
				v1 = *(((const USHORT*)id.bytes)+1),
				v2 = *(((const USHORT*)id.bytes)+2);
		int primary = v0 % 3;	//not perfectly evenly distributed since 65536 isn't divisible by 3, but it should be close enough
		int secondary = (primary + (v1 % 2) + 1) % 3;
		float secondaryLevel = (float(v2)/float(0xFFFF));
		Vec::clear(rs);
		rs.v[primary] = sqrt(1.f - sqr(secondaryLevel));
		rs.v[secondary] = secondaryLevel;
	}
#endif /*0*/

#ifndef CLIENT
	/*static*/ void Faction::FindFreeColor(float3&outColor)
	{
		static Random random;
		float bestDist = -1;
		for (index_t i = 0; i < 20; i++)
		{
			TVec3<> candidate;
			Vec::HSL2RGB(float3(random.NextFloat(),0.5f + random.NextFloat(0.5f),0.5f),candidate);
			count_t competition = 0;
			float closest = std::numeric_limits<float>::max();
			ForeachFaction([&](const PFaction&f)
			{
				if (&f->color != &outColor)
				{
					competition++;
					float dist = Vec::quadraticDistance(candidate,f->color);
					closest = vmin(dist,closest);
				}
			});
			if (closest > bestDist)
			{
				bestDist = closest;
				outColor = candidate;
			}
			if (!competition)
			{
			//	LogEvent(EventClass::Preset::configurationPrint,"No other faction. Using color as is");
				break;	//no point randomizing more often
			}
		}
		//LogEvent(EventClass::Preset::configurationPrint,"Final color is "+outColor.ToString());
	}
#endif

	void		Faction::InitID(const TUniqueID&toID,bool nexusFaction)
	{
		if (toID == TUniqueID::Empty())
			id.Generate();
		else
			id = toID;
		#ifndef CLIENT
			if (nexusFaction)
				color = float3 (1.f, 0.25f, 0.25f);
			else
				FindFreeColor(color);
		#else
			color = float3();
		#endif
	}

}

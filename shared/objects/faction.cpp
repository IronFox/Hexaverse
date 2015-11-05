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

namespace Objects
{

	#ifndef CLIENT

		/*static*/ Buffer0<WFaction>	Faction::all;
		/*static*/ PFaction				Faction::nexus(new Faction("<Nexus>"));

		int		GetStorage(const MobileEntity&v, Resource res)
		{
			#ifndef CONNECTOR_LESS
				return v.currentStorage.level[res];
			#else
				const Compound&compound = v.owner->compound;
				return compound.total.storage.level[res] + compound.overproduction.level[res];
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
			LogEvent(EventType::MinorEntityProcess,&e,String(res.ToString())+ " storage changed by "+String(change)+" to "+String(storage.level[res])+"/"+String(max.level[res]));
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

			#ifdef CONNECTOR_LESS
				if (change < 0 && compound.overproduction.level[res] > 0)
				{
					int production = compound.overproduction.level[res];
					int overproductionTransfer = std::min(production,-change);
					LogEvent(EventType::MinorEntityProcess,originatingAt,String(res.ToString())+ " overproduction ("+String(production)+") redirected for demand ("+String(change)+"): "+String(overproductionTransfer)+" transferred");
					change += overproductionTransfer;
					compound.overproduction.level[res]-= overproductionTransfer;
					if (change == 0)
						return;
				}
			#endif
			demand.level[res] = abs(change);
			if (change < 0)
				compound.Collect(demand,atTime);
			else
				compound.Distribute(demand,atTime);
		}


		void ChangeStorage(MobileEntity&v, float atTime, Resource res, int change)
		{
			if (change == 0)
				return;
			#ifndef CONNECTOR_LESS
				ChangeStorage(v,atTime,v.GetMaxStorageCapacity(),v.currentStorage,res,change);
			#else
				ChangeCompoundStorage(v.owner->compound, &v, atTime,res,change);
			#endif
		}


		void MapVisibility2(const PWorldEntity&e, const PWorldEntity&other, const OwnerParameter&checkForOwner, float&atTime)
		{
			if (e == other || other->owner != checkForOwner)
				return;
			if (e->CanSee(other))
			{
				//seeing:
				/*bool seeingNewEntity = true;
				foreach (other->seenBy,s)
					if ((*s)->owner == e->owner)
					{
						seeingNewEntity = false;
						break;
					}*/
				e->seeing << other;
				other->seenBy << e;
				//if (seeingNewEntity)
				{
					//SignalAppearanceToHostileFaction(e->owner,other,atTime);
					e->owner->SignalAppearance(other,atTime,false);
				}
			}

			if (other->CanSee(e))
			{
				//beeing seen:
				//bool beingSeenByNewOwner = true;
				//foreach (e->seenBy,s)
				//	if ((*s)->owner == other->owner)
				//	{
				//		beingSeenByNewOwner = false;
				//		break;
				//	}
				e->seenBy << other;
				other->seeing << e;
				//if (beingSeenByNewOwner)
				{
					//SignalAppearanceToHostileFaction(other->owner,e,atTime);
					other->owner->SignalAppearance(e,atTime,false);

				}
			}
		}

		void		ChangeOwnership( const PWorldEntity&e, const OwnerParameter&newOwner, float&atTime)
		{
			if (e->owner == newOwner)
				return;
			LogEvent(EventType::GameProgressRelated,e.get(),"Changing ownership ('"+e->owner->GetName()+"' -> '"+newOwner->GetName()+"')");

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
					LogEvent(EventType::InternalFault,e.get(), "Unexpected entity type. Cannot change ownership");
			VerifyVisibilityNetwork();

			return;

			e->owner->RegisterLoss(e);
			SignalOwnershipChange(e,newOwner,atTime);

			newOwner->canSee.Unset(e);

			{
				bool found = false;
				foreach (e->owner->allMobiles, _v)
				{
					PMobileEntity v = _v->lock();
					DBG_ASSERT1__(v,e->owner->GetLogName());
					if (v && v->CanSee(e))
					{
						found = true;
						break;
					}
				}
				if (!found)
					foreach (e->owner->structures, _s)
					{
						PStructure s = _s->lock();
						DBG_ASSERT1__(s,e->owner->GetLogName());
						if (s && s->CanSee(e))
						{
							found = true;
							break;
						}
					}


				//foreach (e->seenBy,s2)
				//{
				//	if ((*s2)->owner == e->owner)	//this is always false, i think
				//	{
				//		found = true;
				//		break;
				//	}
				//}
				if (!found)
					e->owner->SignalDisappearance(e,atTime,true);
				else
					e->owner->SignalAppearance(e,atTime,false);
			}

			for (index_t i = 0; i < e->seeing.Count(); i++)
			{
				PWorldEntity other = e->seeing[i];
				if (other->owner != newOwner)
				{
					newOwner->SignalAppearance(other,atTime);


					bool found = false;
					foreach (other->seenBy,s2)
					{
						if ((*s2) != e && (*s2)->owner == e->owner)
						{
							found = true;
							break;
						}
					}
					if (!found)
						e->owner->SignalDisappearance(other,atTime);


					continue;
				}
				e->seeing.Erase(i); i--;
				other->seenBy.FindAndErase(e);
			}
			for (index_t i = 0; i < e->seenBy.Count(); i++)
			{
				PWorldEntity other = e->seenBy[i];
				if (other->owner != newOwner)
					continue;
				e->seenBy.Erase(i); i--;
				other->seeing.FindAndErase(e);
			}
			e->owner = newOwner;
			newOwner->RegisterGain(e);
			foreach (structures,s)
				MapVisibility2(e,*s,oldOwner, atTime);
			foreach (vehicles,v)
				MapVisibility2(e,*v,oldOwner, atTime);
			foreach (satellites,v)
				MapVisibility2(e,*v,oldOwner, atTime);


			compoundCoverageChanged = true;
			LogEvent(EventType::EntityProcess,e.get(),"Ownership changed");

			VerifyVisibilityNetwork();

		}

		#ifdef CONNECTOR_LESS
			void Faction::UpdateCompoundCoverage()
			{
				compound.containedStructures.Clear();
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
			foreach (structures,_s)
			{
				if (_s->lock() == e)
				{
					structures.erase(_s);
					return;
				}
			}
			ASSERT1__(e->IsStructure(),e->GetName());
			LogEvent(EventType::Fatal,e.get(),"Unable to register loss for faction "+GetLogName());
			FATAL__("Unable to register loss of entity");
		}
		void		Faction::RegisterActiveMobileLoss(const PWorldEntity&e)
		{
			foreach (allMobiles,_v)
			{
				if (_v->lock() == e)
				{
					allMobiles.erase(_v);

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
					return;
				}
			}
			ASSERT1__(e->IsMobile(),e->GetName());
			ASSERT1__(To<MobileEntity>::Cast(e)->storedIn.expired(),e->GetName());

			LogEvent(EventType::Fatal,e.get(),"Unable to register loss for faction "+GetLogName());
			FATAL__("Unable to register loss of entity");
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
			LogEvent(EventType::Fatal,e.get(),"Unable to register loss for faction "+GetLogName());
			FATAL__("Unable to register loss of entity");
		}

		void		Faction::RegisterGain(const PWorldEntity&e)
		{
			if (e->IsMobile())
			{
				if (To<MobileEntity>::Cast(e)->storedIn.expired())
					RegisterActiveMobileGain(e);
				else
					RegisterStoredMobileGain(e);
			}
			else
				if (e->IsStructure())
					RegisterStructureGain(e);
				else
					FATAL__("WTF is this: "+e->GetName());

		}
		void		Faction::RegisterStructureGain(const PWorldEntity&e)
		{
			PStructure s = To<Structure>::DynamicCast(e);
			ASSERT1__(s,e->GetName());
			structures << s;
		}
		void		Faction::RegisterActiveMobileGain(const PWorldEntity&e)
		{
			PMobileEntity v = To<MobileEntity>::DynamicCast(e);
			ASSERT1__(v,e->GetName());
			ASSERT1__(v->storedIn.expired(),e->GetName());
			allMobiles << v;
			if (v->IsSatellite())
			{
				PSatellite sat = To<Satellite>::Cast(v);
				this->satellites << sat;
				if (sat->IsSkyHub())
					this->skyHubs << To<SkyHub>::Cast(sat);
			}
		}
		void		Faction::RegisterStoredMobileGain(const PWorldEntity&e)
		{
			PMobileEntity v = To<MobileEntity>::DynamicCast(e);
			ASSERT1__(v,e->GetName());
			ASSERT1__(!v->storedIn.expired(),e->GetName());
			storedMobiles << v;
			if (v->IsSatellite())
			{
				PSatellite sat = To<Satellite>::Cast(v);
				this->satellites << sat;
				if (sat->IsSkyHub())
					this->skyHubs << To<SkyHub>::Cast(sat);
			}
		}


		bool		Faction::HasActiveMobile(const PMobileEntity&v, bool forceDoNotCheckSatellites) const
		{
			if (!forceDoNotCheckSatellites && v->IsSatellite())
			{
				foreach (this->satellites,_v)
					if (_v->lock() == v)
						return true;
				return false;
			}
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


		bool		Faction::CanSee(const PWorldEntity&e, float t)	const
		{
			if ( e->owner.get() == this )
				return true;
			if (!canSee.IsSet(e))
				return false;
			float t0;
			if (gainedVisualAt.Query(e,t0))
				return t >= t0;
			return true;
		}

		bool		Faction::SignalAppearance(const PWorldEntity&e, float&t, bool cascade/*=true*/)
		{
			if (canSee.SetNew(e))
			{

				float t0;
				if (lostVisualAt.QueryAndUnset(e,t0))
				{
					//ASSERT_LESS_OR_EQUAL__(t0,t)
					//we probably have to accept this happens
					t = std::max(t0,t) + 0.025f;
				}
				//t += 0.025f;

				PMobileEntity v= To<MobileEntity>::DynamicCast(e);
				if (v)
					v->timeAt = std::max(v->timeAt,t);

				ASSERT1__(gainedVisualAt.SetNew(e,t),e->id.ToString());

				if (cascade)
					SignalAppearanceToHostileFaction(shared_from_this(),e,t);
				return true;
			}
			return false;
			//ASSERT_EQUAL__(canSee.count(),gainedVisualAt.Count());
		}

		void		Faction::SignalDisappearance(const PWorldEntity&e, float&t, bool force)
		{
			if (canSee.Unset(e) || force)
			{
				float t0;
				if (gainedVisualAt.QueryAndUnset(e,t0))
					t = std::max(t0,t);	//permissive. whatever. seriously
				t += 0.025f;
				PMobileEntity v= To<MobileEntity>::DynamicCast(e);
				if (v)
					v->timeAt = std::max(v->timeAt,t);
				ASSERT1__(lostVisualAt.SetNew(e,t),e->id.ToString());

				SignalDisappearanceToHostileFaction(shared_from_this(),e,t);
			}
			//ASSERT_EQUAL__(canSee.count(),gainedVisualAt.Count());

		}

		void		Faction::ChangeSensory(const Grid::LCellIdI&center, float radius, float t, short change)
		{
			visibility.ChangeSensory(center, radius, t, change);
		}

		void		Faction::ChangeSensory(const Grid::SkySensorState&state, float t, short change)
		{
			visibility.ChangeSensory(state, t, change);
		}

		void		Faction::RegisterSensorCreation(const Grid::SkySensorState&state, float t)
		{
			ChangeSensory(state,t,+1);
			CheckChanges(t);
		}
		void		Faction::RegisterSensorRemoval(const Grid::SkySensorState&state, float t)
		{
			ChangeSensory(state,t,-1);
			CheckChanges(t);

		}


		//void		Faction::ReplaceSensory(const Grid::SkySensorState&oldState, const Grid::SkySensorState&newState, float t, short change)
		//{
		//	Concurrency::parallel_for(index_t(0),oldState.coverage.Count(),[&oldState,&newState,this,change](index_t i)
		//	{
		//		const auto&c = oldState.coverage[i];
		//		int newH;
		//		if (newState.coverageMap.Query(c.hStackID,newH) && newH == c.downToHeight)
		//			return;
		//		
		//		CellStack&stack = cellStack[c.hstackID];
		//		UINT32 downTo = Grid::CellStack::ToLayer(c.downToHeight);
		//		for (UINT32 layer = Grid::CellStack::HalfRange-1; layer >= downTo; layer--)
		//		{
		//			CellStack::Cell&cell1 = stack.volumeCell[layer];
		//			cell1.visibility += change;
		//			if (change > 0)
		//			{
		//				cell1.temporarilyVisible = true;
		//			}
		//		}
		//		families[Grid::high.grid.GetCell(c.hStackID).family].changed = true;
		//	});
		//}



		void		Faction::FlushData()
		{
			markers.Clear();
			markerMap.Clear();
			nexusControl = acquiringNexusControl = 0;
			deathInRounds = 0;
			this->satellites.Clear();
			skyHubs.Clear();
			allMobiles.Clear();
			structures.Clear();
			storedMobiles.Clear();

			#ifdef CONNECTOR_LESS
				compound.Clear();
			#endif

			currentState.FlushAllData();
			visibility.FlushAllData();

		}



		//void		Faction::SignalCoverageChange(const bool*familiesChanged, float atTime)
		//{
		//	for (BYTE f = 0; f < BaseGrid::NumFamilies; f++)
		//	{
		//		if (familiesChanged[f])
		//		{
		//			families[f].coverageChanged = true;
		//			families[f].lastChange = atTime;
		//		}
		//	}
		//}

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
			if (SurvivesByOtherMeans())
				return true;

			bool hasMineralGeneratorCycle = hasFuelMine && hasMineralMine && hasGenerator;
			#ifndef CONNECTOR_LESS
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

				#ifndef CONNECTOR_LESS
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
				ConstName type = To<MobileEntity>::Cast(e)->turret->constName;
				return CheckLimit(CountMobileType(type), RuntimeConfig::UnitLimits::Get(type), errorOut);
			}
			ConstName type = e->GetCTypeName();
			if (e->IsStructure())
				return CheckLimit(CountStructureType(type), RuntimeConfig::UnitLimits::Get(type), errorOut);
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


		void		Faction::RegisterSensorCreation(const Grid::LCellIdI&center, float radius, float t)
		{
			ChangeSensory(center,radius,t,1);
			CheckChanges(t);

		}
		void		Faction::RegisterSensorMotion(const Grid::LCellIdI&oldCenter, const Grid::LCellIdI&newCenter, float radius, float t)
		{
			ChangeSensory(oldCenter,radius,t,-1);
			ChangeSensory(newCenter,radius,t,1);
			CheckChanges(t);
		}
		void		Faction::RegisterSensorRemoval(const Grid::LCellIdI&center, float radius, float t)
		{
			ChangeSensory(center,radius,t,-1);
			CheckChanges(t);
		}

	#else
		/*static*/ Buffer0<PFaction>			Faction::allKnown;

		/*static*/ Buffer0<Marker, Strategy::Adopt>	Marker::all; 
		/*static*/ GenericHashTable<TUniqueID, index_t, TUniqueID>	Marker::idMap,
								Marker::referenceIDMap,
								Marker::visitedSystemIDMap;
		/*static*/ Buffer0<Marker::TVisited>	Marker::visitedSystems;
		/*static*/ Buffer0<PStructure>	Marker::structureGhosts;
		/*static*/ Buffer0<PMobileEntity>	Marker::mobileGhosts;


		void	ChangeOwnership( const PWorldEntity&e,const OwnerParameter&newOwner, bool isNowMine)
		{
			PStructure s = To<Structure>::DynamicCast(e);
			#ifdef HAS_WINDOW
				if (e->isMine)
				{
					if (!s)
					{
						if (e->IsMobile())
							Grid::actualWorld.RemoveSensor(To<MobileEntity>::Cast(e)->preAnimationLocation,e->GetScanningRange());
						else
							Grid::actualWorld.RemoveSensor(e->location,e->GetScanningRange());
					}
					else
						Grid::actualWorld.RemoveSensor(e->location,e->GetScanningRange());
					#ifdef THOROUGLY_DEBUG_SENSORY
						VerifySensory();
					#endif
				}
			#endif
			LogEvent(EventType::MinorEntityProcess, e.get(),"Changed ownership to "+newOwner->GetName());	//this is also notified from the server, hence: minor
			if (s)
			{
				s->DisconnectFromNeighborhood(0);
			}
			e->owner = newOwner;
			e->isMine = isNowMine;

			#ifdef HAS_WINDOW
				if (e->isMine)
				{
					if (!s)
					{
						if (e->IsMobile())
							Grid::actualWorld.InsertSensor(To<MobileEntity>::Cast(e)->preAnimationLocation,e->GetScanningRange());
						else
							Grid::actualWorld.InsertSensor(e->location,e->GetScanningRange());
					}
					else
						Grid::actualWorld.InsertSensor(e->location,e->GetScanningRange());
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


	void			ForeachMobileWorldEntity(const std::function<void(const PMobileEntity&)>&func)
	{
		for (const PMobileEntity&e : vehicles)
			func(e);
		for (const PMobileEntity&e : satellites)
			func(e);
	}

	void			ForeachMobileEntityIncludingStored(const std::function<void(const PMobileEntity&)>&func)
	{
		for (const PMobileEntity&e : vehicles)
			func(e);
		for (const PMobileEntity&e : satellites)
			func(e);
		for (const PMobileEntity&e : storedMobileEntities)
			func(e);
	}



	void ForeachWorldEntity(const std::function<void(const PWorldEntity&)>&func)
	{
		for (const PWorldEntity&e : vehicles)
			func(e);
		for (const PWorldEntity&e : satellites)
			func(e);
		for (const PWorldEntity&e : structures)
			func(e);
		#ifdef CLIENT
			for (const PWorldEntity&e : Marker::structureGhosts)
				func(e);
			for (const PWorldEntity&e : Marker::mobileGhosts)
				func(e);
		#endif
	}


	
	#ifdef CLIENT


		void Marker::UpdateAge(UINT32 currentRoundNumber)
		{
			age = currentRoundNumber - createdInRound;
			ASSERT_LESS__(age,100000);	//overflow otherwise, 100000 seems a good limit
			if (mobileVisualization)
				mobileVisualization->health = -(int)(age);
			elif(structureVisualization)
				structureVisualization->health = -(int)(age);
			if (type == Type::Visited)
			{
				index_t at = visitedSystemIDMap.Require(id);
				visitedSystems[at].age = age;
			}
		}

		/*static*/ void				Marker::GetOutdated(BasicBuffer<TUniqueID>&outOudated, UINT32 currentRoundNumber, UINT32 deathOutdatedAfterRounds)
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
							LogEvent(EventType::Debug,nullptr,"Marker "+m->GetName()+" considered outdated after "+String(m->age)+" round(s)");

							outOudated << m->id;
						}
					break;
					case Type::HostileContact:
					{
						TVec3<> center = Grid::ToWorld(m->location);
						
						bool seen = false;
						foreach(structures, s)
							if ((*s)->isMine && (*s)->CanSee(center, 0))
							{
								LogEvent(EventType::Debug,nullptr,"Marker "+m->GetName()+" considered visible (seen by "+(*s)->GetName()+")");
								seen = true;
								break;
							}
						if (!seen)
							foreach(vehicles, s)
								if ((*s)->isMine && (*s)->CanSee(center, 0))
								{
									LogEvent(EventType::Debug,nullptr,"Marker "+m->GetName()+" considered visible (seen by "+(*s)->GetName()+")");
									seen = true;
									break;
								}
						if (!seen)
							foreach(satellites, s)
								if ((*s)->isMine && (*s)->CanSee(center, 0))
								{
									LogEvent(EventType::Debug,nullptr,"Marker "+m->GetName()+" considered visible (seen by "+(*s)->GetName()+")");
									seen = true;
									break;
								}
						if (seen)
							outOudated << m->id;
					}
					break;

				}
		

			}
		}


		/*static*/ void				Marker::Create(const TUniqueID&id, Type type, const Grid::LCellIdI&location, UINT32 createdInRound, const TUniqueID&referenceID, ConstName referenceType[2], UINT32 currentRound)
		{
			const index_t newIndex = all.Count();
			Marker&m = all.Append();
			m.id = id;
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
					m.mobileVisualization.reset(new Vehicle(OwnerParameter(), location, base, turret));
					m.mobileVisualization->id = referenceID;
					m.mobileVisualization->UpdateSystem();
					m.mobileVisualization->health = -(int)m.age;
					m.mobileVisualization->isMine = type == Type::EntityDeath;
					mobileGhosts << m.mobileVisualization;
				}
				else
				{
					//structure
					PStructure prototype = Objects::GetPrototypeByName(referenceType[0]);
					if (prototype)
					{
						ASSERT1__(prototype, referenceType);
						m.structureVisualization = To<Structure>::Cast(prototype->CreateNewInstance(OwnerParameter(), location, 0));
						m.structureVisualization->id = referenceID;
						m.structureVisualization->health = -(int)m.age;
						m.structureVisualization->isMine = type == Type::EntityDeath;
						structureGhosts << m.structureVisualization;
					}
					else
					{
						PSatellite prototype = Objects::GetSatellitePrototypeByName(referenceType[0]);
						if (prototype)
						{
							//satellite
							m.mobileVisualization = To<MobileEntity>::Cast(prototype->CreateNewInstance(OwnerParameter(), location, 0.f));
							m.mobileVisualization->id = referenceID;
							m.mobileVisualization->UpdateSystem();
							m.mobileVisualization->health = -(int)m.age;
							m.mobileVisualization->isMine = type == Type::EntityDeath;
							mobileGhosts << m.mobileVisualization;
						}
						//probably sattelite, but not gonna go there now

					}
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
				elif (type == Type::ControlMarker)
				{
					//later
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
				if (m.mobileVisualization)
				{
					mobileGhosts.FindAndErase(m.mobileVisualization);
					#ifdef HAS_WINDOW
						if (selectedEntity == m.mobileVisualization)
						{
							Deselect(controlEnabled);
						}
					#endif

					m.mobileVisualization.reset();
				}
				elif(m.structureVisualization)
				{
					structureGhosts.FindAndErase(m.structureVisualization);
					#ifdef HAS_WINDOW
						if (selectedEntity == m.structureVisualization)
						{
							Deselect(controlEnabled);
						}
					#endif
					m.structureVisualization.reset();
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
				elif (m.type == Type::ControlMarker)
				{
					//later
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
			size += GetSerialSizeOfSize(createdInRound);
			size += sizeof(location);
			if (UtilizesReferenceEntity())
			{
				size += sizeof(TUniqueID);	//referenced entity id

				String t0 = Convert(this->referencedType[0]);
				String t1 = Convert(this->referencedType[1]);
				size += t0.GetSerialSize(true);
				size += t1.GetSerialSize(true);
//				size += sizeof(this->referencedType);
				//this->referencedType.GetSerialSize(exportSize);
			}
			if (UtilizesRadius())
				size += 4;
			if (UtilizesSubType())
				size ++;
			return size;
		}
		/*virtual override*/	bool		Marker::Serialize(IWriteStream&outStream, bool exportSize) const
		{
			if (!outStream.WritePrimitive(id) || !outStream.WritePrimitive((UINT8)type)
				 || !outStream.WriteSize(createdInRound)
				|| !outStream.WritePrimitive(location)
				)
				return false;
			if (UtilizesReferenceEntity())
			{
				if (!outStream.WritePrimitive(this->referenceEntity))
					return false;
				String t0 = Convert(this->referencedType[0]);
				String t1 = Convert(this->referencedType[1]);
				if (!t0.Serialize(outStream,true) || !t1.Serialize(outStream,true))
					return false;
			}
			if (UtilizesRadius() && !outStream.WritePrimitive(radius))
				return false;
			if (UtilizesSubType() && !outStream.WritePrimitive(subType))
				return false;

			return true;
		}
		/*virtual override*/	bool		Marker::Deserialize(IReadStream&inStream, serial_size_t fixedSize)
		{
			UINT8 typei;
			if (!inStream.ReadPrimitive(id) || !inStream.ReadPrimitive(typei) || !inStream.ReadSize(createdInRound)
				||  !inStream.ReadPrimitive(location)
				)
				return false;
			type = Type::Reinterpret(typei);
			if (UtilizesReferenceEntity())
			{
				if (fixedSize != EmbeddedSize)
					fixedSize -= sizeof(TUniqueID) + 1 + GetSerialSizeOfSize(createdInRound) + sizeof(TUniqueID) + sizeof(location);
				String t0,t1;
				if (!inStream.ReadPrimitive(this->referenceEntity) || !t0.Deserialize(inStream,EmbeddedSize) || !t1.Deserialize(inStream,EmbeddedSize))
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

			if (UtilizesRadius() && !inStream.ReadPrimitive(radius))
				return false;
			if (UtilizesSubType() && !inStream.ReadPrimitive(subType))
				return false;

			return true;
		}


		Marker&		Faction::CreateMarker(const TUniqueID&id, bool priveleged, Marker::Type type, const TUniqueID&referenceID)
		{
			ASSERT__(CanCreateMarker(priveleged)); 
			Marker&rs = markers.Append(); 
			rs.id = id; 
			rs.type = type;
			rs.referenceEntity = referenceID;
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
				LogEvent(EventType::InternalFault,nullptr,"Unable to destroy Marker. ID not set " + markerID.ToString());
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
			f->OnKill();
			LogEvent(EventType::Debug,nullptr, "Erasing faction "+f->id.ToString());
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
			foreach (toDelete,del)
				Remove(*del,Objects::RemovalType::ControlledExplosion,0,false);
			toDelete.Clear();

			for (index_t i = 0; i < all.Count(); i++)
				if (all[i].lock() == f)
				{
					all.Erase(i);
					break;
				}
			LogEvent(EventType::GameProgressRelated, nullptr,"Faction terminated: "+f->id.ToString());
		}


		void		VerifyVisibilityNetwork(const PWorldEntity&e)
		{
			foreach (e->seeing,s)
			{
				ASSERT2__(vehicles.Contains(*s) || structures.Contains(*s) || satellites.Contains(*s), (e)->GetName(), (*s)->GetName());
				ASSERT2__(e->CanSee(*s), (e)->GetName(), (*s)->GetName());
				ASSERT2__((*s)->seenBy.Contains(e), (e)->GetName(), (*s)->GetName());
			}
			
			foreach (e->seenBy,s)
			{
				ASSERT2__(vehicles.Contains(*s) || structures.Contains(*s) || satellites.Contains(*s), (e)->GetName(), (*s)->GetName());
//					ASSERT__((*s)->CanSee(*v));	done in other pass
				ASSERT2__((*s)->seeing.Contains(e), (e)->GetName(), (*s)->GetName());
			}

			ForeachWorldEntity([e](const PWorldEntity&w)
			{
				if (e->owner == (w)->owner)
					return;
				bool visible = e->CanSee(w);
				if (visible)
					ASSERT2__(e->seeing.Contains(w), (e)->GetName(), (w)->GetName());
			}
			);
		}

		void			VerifyVisibilityNetwork()
		{
			foreach (vehicles,v)
			{
				VerifyVisibilityNetwork(*v);

			}
			foreach (structures,s)
			{
				VerifyVisibilityNetwork(*s);
			}
			foreach (satellites,s)
			{
				VerifyVisibilityNetwork(*s);
			}
		}




	#endif

}
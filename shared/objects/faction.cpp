#include <global_root.h>
#include "faction.h"
#include "../cast.h"
//#include "vehicle.h"
#include "mobileEntity.h"
#include "gridAndChoices.h"
#include "prototypes.h"
#include "satellite.h"
#include "vehicle.h"
#include "../sphereGridHelper.h"
#include "../runtimeConfig.h"
#include "prototypes.h"
#include "structure/tower.h"
#include "../protocol.h"

namespace Objects
{

	/*static*/ const String			Faction::nexusName = "<Nexus>";

	/*static*/ HashTable<TUniqueID,WAlliance>	Alliance::all;


	#ifndef CLIENT
		UINT64							Alliance::GetWinningScore() const
		{
			UINT64 rs = 0;
			ForeachMember([&rs](const PFaction&f)
			{
				rs += f->GetWinningScore();
			});
			return rs;
		}


		Alliance::TNexusStats			Alliance::GetNexusStats() const
		{
			TNexusStats rs;
			ForeachMember([&rs](const PFaction&f)
			{
				rs.acquiringNexusControl += f->acquiringNexusControl;
				rs.nexusControl += f->nexusControl;
			},true);
			return rs;
		}
	#endif


	/**/		Alliance::Alliance(const TUniqueID&id) : id(id)
	{
		LogEvent(EventSignificance::Debug,"Constructed Alliance "+ToString(id));
	}
	/*virtual	*/	Alliance::~Alliance()
	{
		if (!application_shutting_down) 
		{
			all.Unset(id);
			LogEvent(EventSignificance::Debug,"Destructed Alliance "+ToString(id)+". "+String(all.Count())+" remaining");

		}
	}

	/*static*/ void	Alliance::Foreach(const std::function<void(const PAlliance&)>&visitor)
	{
		all.VisitAllValues([visitor](const WAlliance&a)
		{
			visitor(a.lock());
		});
	}

	void		Alliance::Invite(const PFaction&f)
	{
		if (HaveInvited(f))
			return;
		invited.Append().rawPointer = f.get();
		invited.Last().wFaction = f;
		LogEvent(EventSignificance::Common,"Alliance "+ToString(id)+" invited faction "+f->GetLogName());
		
		#ifndef CLIENT
			ApplicationInterface::SignalAllianceInvite(shared_from_this(), f, Protocol::AllianceInviteType::Invite);
		#endif
	}

	void		Alliance::CancelInvite(const PFaction&f)
	{
		const index_t at = FindInviteOf(f);
		if (at == InvalidIndex)
			return;
		invited.Erase(at);
		LogEvent(EventSignificance::Common,"Alliance "+ToString(id)+" invite for faction "+f->GetLogName()+" canceled");
		
		#ifndef CLIENT
			ApplicationInterface::SignalAllianceInvite(shared_from_this(), f, Protocol::AllianceInviteType::Cancel);
		#endif
	}

	bool		Alliance::HaveInvited(const PFaction&f)
	{
		return FindInviteOf(f) != InvalidIndex;
	}

	index_t		Alliance::FindInviteOf(const Objects::PFaction&f) const
	{
		foreach (invited,inv)
		{
			if (inv->rawPointer == f.get())
				return inv - invited.pointer();
		}
		return InvalidIndex;
	}

	void		Alliance::VisitAllInvitedFactions(const std::function<void(const PFaction&)>&visitor, bool livingOnly/*=true*/) const
	{
		foreach (invited,m)
		{
			PFaction f = m->wFaction.lock();
			#ifndef CLIENT
				if (f->isDead && livingOnly)
					continue;
			#endif
			visitor(f);
		}
	}



	void		Alliance::ForeachMember(const std::function<void(const PFaction&)>&visitor, bool livingOnly)	const
	{
		foreach (members,m)
		{
			PFaction f = m->wFaction.lock();
			#ifndef CLIENT
				if (f->isDead && livingOnly)
					continue;
			#endif
			visitor(f);
		}
	}

	count_t		Alliance::CountLivingMembers() const
	{
		if (!this)
			return 0;
		#ifndef CLIENT
			count_t cnt  = 0;
			foreach (members,m)
				if (!m->wFaction.lock()->isDead)
					cnt++;
			return cnt;
		#else
			return members.Count();
		#endif
	}


	bool		Alliance::IsMember(const PFaction&f) const
	{
		foreach (members,m)
			if (m->wFaction.lock() == f)
				return true;
		return false;
	}

	void		Alliance::UpdateConfiguration(const TConfig&newCfg)
	{
		if (config == newCfg)
			return;
		const auto oldCfg = config;
		config = newCfg;

		if (oldCfg.shareSensors != newCfg.shareSensors)
		{
			#ifndef CLIENT
				this->ForeachMember([](const PFaction&f)
				{
					f->knownWorld.InvalidateSensory();
					f->CheckSensory();
				},true);
			#elif HAS_WINDOW
				World::actual.InvalidateSensory();
			#endif
		}

		#ifndef CLIENT
			if (oldCfg.exposeEntityDetails != newCfg.exposeEntityDetails)
				this->ForeachMember([](const PFaction&f)
				{
					f->VisitMyEntities([](const PWorldEntity&e)
					{
						e->SignalChange(Objects::WorldEntity::CompleteChange,0);
					});
				},true);
			ApplicationInterface::SignalAllianceConfigChange(shared_from_this());
		#endif

	}


	Alliance::TConfig::TConfig(UINT32 flags):
								exposeEntityDetails((Protocol::AllianceFlag::ExposeEntityDetails&flags) != 0),
								shareSensors((Protocol::AllianceFlag::ShareSensors&flags) != 0)
	{}


	UINT32	Alliance::TConfig::ToFlags() const
	{
		UINT32	rs = 0;
		if (exposeEntityDetails)
			rs |= Protocol::AllianceFlag::ExposeEntityDetails;
		if (shareSensors)
			rs |= Protocol::AllianceFlag::ShareSensors;
		return rs;
	}


	void		Alliance::RefreshSensors(bool force)
	{
		if (!config.shareSensors && !force)
			return;
		#ifndef CLIENT
			foreach (members,m)
			{
				PFaction f = m->wFaction.lock();

				f->knownWorld.InvalidateSensory();
				f->CheckSensory();


				f->VisitMyEntities([](const Objects::PWorldEntity&e)
				{
					e->RegisterChange(WorldEntity::CompleteChange,0);	//should trigger resubmission
				});

			}
		#endif
	}

	void		Alliance::Join(const PFaction&f)
	{
		ASSERT__(f);
		ASSERT__(!IsMember(f));
		LogEvent(EventSignificance::Debug,f, "@"+String((const void*)f.get())+ " Joined alliance "+ToString(id));
		members.Append().wFaction = f;
		members.Last().rawPointer = f.get();
		LogEvent(EventSignificance::Common,"New member count in alliance "+ToString(id)+": "+String(members.Count()));
		if (members.Count() == 1)
			all.Set(id,shared_from_this());
		f->alliance = shared_from_this();

		const index_t at = FindInviteOf(f);
		if (at != InvalidIndex)
		{
			invited.Erase(at);
			LogEvent(EventSignificance::Debug,"Erased existing invite");
			
			#ifndef CLIENT
				ApplicationInterface::SignalAllianceInvite(shared_from_this(), f, Protocol::AllianceInviteType::Accept);
			#endif

		}



		#ifndef CLIENT
			ApplicationInterface::SignalAllianceMembershipChange(f);
			ApplicationInterface::SignalNexusControlChange(shared_from_this());
		#endif

		RefreshSensors(false);

	}

	void		Alliance::Leave(const PFaction&f)
	{
		if (!this)
			return;
		LogEvent(EventSignificance::Common,f,"Leaving alliance "+ToString(id));
		foreach (members,m)
			if ( m->wFaction == f)
			{
				members.erase(m);
				LogEvent(EventSignificance::Common,"New member count in alliance "+ToString(id)+": "+String(members.Count()));
				f->alliance = nullptr;
				if (members.IsEmpty())
					all.Unset(id);
				else
				{
					RefreshSensors(false);
					#ifndef CLIENT
						ApplicationInterface::SignalNexusControlChange(shared_from_this());
					#endif
				}
				return;
			} 
		FATAL__("Faction not found");
	}


	void		Alliance::Leave(Faction*f)
	{
		LogEvent(EventSignificance::Common,f->GetLogName()+ "@"+String((const void*)f)+ ": Leaving alliance "+ToString(id));
		const count_t cnt = members.Count();
		foreach (members,m)
			if ( m->rawPointer == f)
			{
				members.erase(m);
				ASSERT_EQUAL__(members.Count()+1,cnt);
				LogEvent(EventSignificance::Common,"New member count in alliance "+ToString(id)+": "+String(members.Count()));
				f->alliance = nullptr;
				if (members.IsEmpty())
					all.Unset(id);
				else
					RefreshSensors(false);
				return;
			} 
		FATAL__("Faction not found");
	}



	Faction::~Faction()
	{
		LogEvent(EventSignificance::Debug,"Destruct faction "+ToString(id)+" "+name);

		if (!application_shutting_down && alliance)
			alliance->Leave(this);
	}
	
	#ifndef CLIENT

		/*static*/ Ctr::Buffer0<PFaction>	Faction::all;
		/*static*/ PFaction				Faction::nexus;



		count_t	Faction::GetWinningScore() const
		{
			return 100000 * this->nexusControl + 10000ULL* this->acquiringNexusControl + GetOffensiveCapabilities()*100 + GetTotalEntityValue();
		}

		count_t Faction::GetOffensiveCapabilities() const
		{
			count_t rs = 0;
			foreach (allMobiles,m0)
			{
				PMobileEntity m = m0->lock();
				if (m)
					rs += m->GetDamagePotentialPerRound();
			}
			foreach (storedMobiles,m0)
			{
				PMobileEntity m = m0->lock();
				if (m)
					rs += m->GetDamagePotentialPerRound();
			}
			foreach (towers,t0)
			{
				PTower t = t0->lock();
				if (t)
					rs += t->GetDamagePotentialPerRound();
			}
			return rs;
		}

		count_t		Faction::GetTotalEntityValue() const
		{
			count_t rs = 0;
			foreach (allMobiles,m0)
			{
				PMobileEntity m = m0->lock();
				if (m)
					rs += m->GetConstructionCost();
			}
			foreach (storedMobiles,m0)
			{
				PMobileEntity m = m0->lock();
				if (m)
					rs += m->GetConstructionCost();
			}
			foreach (structures,s0)
			{
				PStructure s = s0->lock();
				if (s)
					rs += s->GetConstructionCost();
			}
			return rs;
		}



		/*static*/ PFaction				Faction::Create(const TUniqueID&id, const PAlliance&alliance, const String&name /*= ""*/)
		{
			ASSERT__(id.IsNotEmpty());
			ASSERT__(alliance);
			ASSERT__(id != alliance->id);
			PFaction rs(new Faction(id, name));
			all << rs;

			alliance->Join(rs);
			return rs;

		}

		/*static*/ PFaction				Faction::Create(const TUniqueID&id, const TUniqueID&allianceID, const String&name/* = ""*/)
		{
			ASSERT__(allianceID.IsNotEmpty());
			auto alliance = Alliance::FindByID(allianceID);
			if (!alliance)
				alliance.reset(new Alliance(allianceID));
			return Create(id,alliance,name);
		}

		void	Faction::RecreateNexus(const TUniqueID&id)
		{
			nexus.reset(new Faction(id, Faction::nexusName,true));
			PAlliance alliance { new Alliance(TUniqueID::New()) };
			alliance->Join(nexus);

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

			oldOwner->statistics.SignalLossOf(e,newOwner,true);
			newOwner->statistics.SignalUnitAcquired(e,oldOwner);


			ForceRemove(e,RemovalType::ChangingOwnership,atTime);
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
									this->satellites.Erase(sat);
									break;
								}
							if (To<Satellite>::Cast(e)->IsSkyHub())
								foreach (this->skyHubs,sat)
									if (sat->lock() == e)
									{
										this->skyHubs.Erase(sat);
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

		void		Faction::VisitMyEntities(const std::function<void(const PWorldEntity&)>&f) const
		{
			foreach (structures,s)
				f(s->lock());
			foreach (allMobiles,m)
				f(m->lock());
			foreach (storedMobiles,m)
				f(m->lock());
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

		void	Faction::AddEntitySensory(World::Data&worldData, float&t) const
		{
			foreach (this->structures,_s)
			{
				Objects::PStructure s = _s->lock();
				if (s)
					s->InsertSensoryInto(worldData,t);
			}
			foreach (this->allMobiles,_m)
			{
				Objects::PWorldEntity e = _m->lock();
				if (e)
					e->InsertSensoryInto(worldData,t);
			}
		}



		void Faction::HandlePotentialAppearance(const Objects::PWorldEntity&e, float&t, bool shareSensors)
		{
			bool appear = shareSensors && IsAlliedWith(e->owner);

			if (!appear)
			{
				if (e->IsAboveGrid())
				{
					count_t cover = CountOffGridCoverage(e->location.height, e->GetWorldPosition());
					if (cover > 0)
						appear = true;
				}
				else
					if (GridCanSee(e,t))
						appear = true;
			}
			if (appear)
				SignalAppearance(e,t,true);
		}

		void Faction::HandlePotentialDisappearance(const Objects::PWorldEntity&e, float&t, bool shareSensors)
		{
			bool stillVisible = shareSensors && IsAlliedWith(e->owner);

			if (!stillVisible)
			{
				if (e->IsAboveGrid())
				{
					count_t cover = CountOffGridCoverage(e->location.height, e->GetWorldPosition());
					stillVisible = cover > 0;
				}
				else
					stillVisible = GridCanSee(e,t);
			}
			if (!stillVisible)
				SignalDisappearance(e,t);
		}


		void Faction::CheckSensory(float&t)
		{
			if (this->sensorLockLevel > 0)
				return;
			if (knownWorld.SensoryIsInvalid())
			{
				knownWorld.FlushAndValidateSensory();
				if (alliance->config.shareSensors)
				{
					alliance->ForeachMember([this,&t](const PFaction&f)	//includes this
					{
						f->AddEntitySensory(knownWorld,t);
					},true);
				}
				else
					this->AddEntitySensory(knownWorld,t);

				LogEvent(EventSignificance::Common,shared_from_this(),"Recomputing sensory");
				Timer::Time started = timer.Now();
				LogEvent(EventSignificance::PerformanceMeasurement,shared_from_this(),"Computed sensory grid coverage in "+String(timer.GetSecondsSince(started)));
				started = timer.Now();
				const bool shareSensors = alliance->config.shareSensors;
				ForeachWorldEntity([this,&t,shareSensors](const PWorldEntity&e)
				{
					if (e->owner.get() == this)
						return;
					HandlePotentialAppearance(e,t,shareSensors);
				},false);

				LogEvent(EventSignificance::PerformanceMeasurement,shared_from_this(),"Computed appearances in "+String(timer.GetSecondsSince(started)));
				started = timer.Now();


				VisitVisibleForeignEntities(t,[this,&t,shareSensors](const PWorldEntity&e)
				{
					HandlePotentialDisappearance(e,t,shareSensors);
				});
				LogEvent(EventSignificance::PerformanceMeasurement,shared_from_this(),"Computed disappearances in "+String(timer.GetSecondsSince(started)));
			}
		}
		

		void Faction::CheckSensory()
		{
			float t = 0;
			CheckSensory(t);
		}


		bool Faction::GridCanSee(const Grid::HighSetup::TVolumeDescriptor&v, float &t)
		{
			CheckSensory(t);
			foreach (v.coverage,st)
			{
				DBG_ASSERT_LESS__(st->range.start,Grid::CellStack::Range);
				DBG_ASSERT_LESS_OR_EQUAL__(st->range.end,Grid::CellStack::Range);
				DBG_ASSERT_LESS_OR_EQUAL__(st->range.start,st->range.end);
				const auto&stack = knownWorld.GetStack(st->stackID);
				for (UINT32 layer = st->range.start; layer < st->range.end; layer++)
				{
					if (stack.volumeCell[layer].IsCurrentlyVisible())
						return true;
				}
			}
			return false;
		}


		bool Faction::GridCanSee(const PWorldEntity&e, float &t)
		{
			CheckSensory(t);
			const UINT32 fine = Grid::low.grid.GetCell(e->location.stackID).higherEquivalent;
			const auto&c = Grid::high.grid.GetCell(fine);
			const UINT32 bottom = Grid::CellStack::ToLayer(e->location.height);
			const int entityHeight =e->GetHeight();
			const UINT32 end = std::min(Grid::CellStack::Range,bottom + (UINT32)entityHeight);
			{
				const auto&stack = knownWorld.GetStack(fine);
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
			if ( SharingSensorWith(e->owner) )
				return true;
			auto p = seeing.QueryPointer(e.get());
			return p && p->VisibleAt(t);
		}



		bool					Faction::SeeingReference::VisibleAt(float t) const
		{
			auto it = reverseVisibilityChange.upper_bound(-t);
			if (it == reverseVisibilityChange.end())
				return wasVisibleAtBeginning;
			return it->second;
		}

		void					Faction::SeeingReference::Appear(const PWorldEntity&e, float&t)
		{
			ASSERT__(!CurrentlyVisible());
			if (RuntimeConfig::IsInAnyCommandPhase())
			{
				LogEvent(EventSignificance::Debug,e.get(),"["+String(e.get())+"] Registering as newly visible now");
				reference = e;
				wasVisibleAtBeginning = true;
				ASSERT__(CurrentlyVisible());
				return;
			}

			if (!reverseVisibilityChange.empty())
				t = std::max(t,-reverseVisibilityChange.begin()->first);
			LogEvent(EventSignificance::Debug,e.get(),"["+String(e.get())+"] Registering as newly visible at "+String(t));

			t += timeEpsilon;
			reverseVisibilityChange[-t] = true;
			ASSERT__(CurrentlyVisible());
			e->SyncTime(t,timeEpsilon);
			reference = e;
		}

		void					Faction::SeeingReference::Disappear(const PWorldEntity&e, float&t)
		{
			ASSERT__(CurrentlyVisible());
			if (RuntimeConfig::IsInAnyCommandPhase())
			{
				LogEvent(EventSignificance::Debug,e.get(),"["+String(e.get())+"] Registering as newly invisible now");
				wasVisibleAtBeginning = false;
				ASSERT__(!CurrentlyVisible());
				return;
			}
			if (!reverseVisibilityChange.empty())
				t = std::max(t,-reverseVisibilityChange.begin()->first);
			LogEvent(EventSignificance::Debug,e.get(),"["+String(e.get())+"] Registering as newly invisible at "+String(t));

			t += timeEpsilon;
			reverseVisibilityChange[-t] = false;
			ASSERT__(!CurrentlyVisible());
			e->SyncTime(t,timeEpsilon);
		}

		bool		Faction::SignalAppearance(const PWorldEntity&e, float&t, bool cascade/*=true*/)
		{
			SeeingReference&ref = seeing.Set(e.get());
			if (ref.CurrentlyVisible())
				return false;
			ref.reference = e;	//just in case
			
			ref.Appear(e,t);

			LogEvent(EventSignificance::Common,e.get(),this->GetLogName()+": Gained visual at time="+String(t));

			if (cascade)
				ApplicationInterface::SignalAppearanceToHostileFaction(shared_from_this(),e,t);

			e->SyncTime(t,timeEpsilon);

			return true;
		}

		void		Faction::SignalDisappearance(const PWorldEntity&e, float&t, bool force, bool signalConnectedClient/*=true*/)
		{
			SeeingReference*ref = seeing.QueryPointer(e.get());
			if (!ref || !ref->CurrentlyVisible())
				return;
			ref->Disappear(e,t);

			LogEvent(EventSignificance::Common,e.get(),this->GetLogName()+": Lost visual at time="+String(t));
			if (signalConnectedClient)
				ApplicationInterface::SignalDisappearanceToHostileFaction(shared_from_this(),e,t);

			e->SyncTime(t,timeEpsilon);
		}

		//void		Faction::ChangeSensory(const Grid::HighSetup::TVolumeDescriptor&desc, float t, short changeBy)
		//{
		//	visibility.ChangeSensory(desc, t, changeBy);
		//}

		////void		Faction::ChangeSensory(const Grid::LCellIdI&center, float radius, float t, short change)
		////{
		////	visibility.ChangeSensory(center, radius, t, change);
		////}

		//void		Faction::ChangeSensory(const Grid::SkySensorState&state, float t, short change)
		//{
		//	visibility.ChangeSensory(state, t, change);
		//}

		template <typename T>
		count_t		CountRawSensorCoverage(const M::TVec3<>&center, const Ctr::BasicBuffer<T>&scope)
		{
			count_t rs = 0;
			foreach (scope,s)
			{
				auto sat = s->lock();
				if (sat && M::Sphere<>(sat->GetWorldPosition(),sat->GetScanningRange()).Contains(center))
					rs++;
			}
			return rs;
		}

		template <typename T>
		count_t		CountRawSensorCoverage(const M::Sphere<>&sphere, const Ctr::BasicBuffer<T>&scope)
		{
			count_t rs = 0;
			foreach (scope,s)
			{
				auto sat = s->lock();
				if (sat && M::Sphere<>(sat->GetWorldPosition(),sat->GetScanningRange()).Intersects(sphere))
					rs++;
			}
			return rs;
		}

		
		count_t		Faction::CountOffGridCoverage(int channelHeight, const M::TVec3<>&p)	const
		{
			#ifndef NO_SATELLITES
				if (Satellite::IsSatelliteHeight(channelHeight))
					return CountRawSensorCoverage(p,satellites);
			#endif

			return 0;
		}

		count_t		Faction::CountOffGridCoverage(const M::Sphere<>&p)	const
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
			LogEvent(EventSignificance::Debug,e.get(),this->GetLogName()+ ": CheckGridVisibility at time "+String(timeAt));
			if (seeing.IsSet(e.get()))
			{
				HandlePotentialDisappearance(e,timeAt,alliance->config.shareSensors);
			}
			else
			{
				HandlePotentialAppearance(e,timeAt,alliance->config.shareSensors);
			}
		}


		void		Faction::RegisterSensorCreation(const World::Sensor&sensor, int sensorHeight, float&t, Ctr::Buffer0<PWorldEntity>*newContacts /*= nullptr*/)
		{
			//if (sensorHeight <= Grid::CellStack::TopHeight)
			{
				if (!sensorLockLevel)
					CheckSensory(t);
				else
					knownWorld.InvalidateSensory();
				sensor.InsertInto(knownWorld,t);

				if (!knownWorld.SensoryIsInvalid())
				{
					static Grid::LowSetup::TVolumeDescriptor	desc;
					sensor.GetMaxVolume(desc);
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
							if (GridCanSee(e,t))
							{
								if (SignalAppearance(e,t,true) && newContacts)
									newContacts->Append(e);
							}
						}
					}
				}
				else
					ASSERT_IS_NULL__(newContacts);
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

		Faction::TSensorLock::~TSensorLock()
		{
			owner->sensorLockLevel--;
			if (owner->sensorLockLevel == 0)
				owner->CheckSensory();
		}



		//void		Faction::RegisterSensorMotion(const M::Sphere<>&from, const M::Sphere<>&to, float t);
		void		Faction::RegisterSensorRemoval(const World::Sensor&sensor, int sensorHeight, float&t)
		{
			sensor.RemoveFrom(knownWorld,t);
//			float escaped = M::Sqr(sphere.radius+2);
			//if (isSatellite)
			//	LogEvent(EventSignificance::Debug,"Unregistering orbital sensor sphere "+sphere.ToString());

			if (!knownWorld.SensoryIsInvalid())
			{
				bool offGrid = sensorHeight > Grid::CellStack::TopHeight;

				this->VisitVisibleForeignEntities(t,[this,offGrid, sensor,&t](const PWorldEntity&s)
				{
					if (s->IsAboveGrid() != offGrid)
						return;
					HandlePotentialDisappearance(s,t,SharingSensorWith(s->owner));
				});
			}
		}

		//void		Faction::RegisterSensorCreation(const Grid::SkySensorState&state, float&t, bool checkChangesNow, Buffer0<PWorldEntity>*newContacts /*= nullptr*/)
		//{
		//	ChangeSensory(state,t,+1);
		//	if (checkChangesNow)
		//		CheckChanges(t);


		//	ForeachWorldEntity([this,&t,newContacts](const PWorldEntity&other)
		//	{
		//		if (other->owner.get() == this || other->IsAboveGrid())
		//			return;
		//		if (GridCanSee(other))
		//		{
		//			SignalAppearance(other,t,true,false);
		//			if (newContacts)
		//				newContacts->Append(other);
		//		}
		//	},false);
		//}
		//void		Faction::RegisterSensorRemoval(const Grid::SkySensorState&state, float&t, bool checkChangesNow)
		//{
		//	ChangeSensory(state,t,-1);
		//	if (checkChangesNow)
		//		CheckChanges(t);

		//	static Buffer<PWorldEntity>	disappear;
		//	this->seeing.VisitAllKeys([this](const PWorldEntity&s)
		//	{
		//		if (!s->IsAboveGrid() && !GridCanSee(s))
		//			disappear << s;
		//	});
		//	foreach (disappear,s)
		//		SignalDisappearance(*s,t);
		//	disappear.Clear();

		//}





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
			if (isDead)
			{
				whyNot = "Died";
				return false;
			}

			const bool hasFuelMine = (hasUnitClasses & UnitClass::FuelMine);
			const bool hasMineralMine = (hasUnitClasses & UnitClass::MineralMine);
			const bool hasGenerator = (hasUnitClasses & UnitClass::Generator);
			const bool hasMineralContainer = (hasUnitClasses & UnitClass::MineralContainer);
			const bool hasTerraformer = (hasUnitClasses & UnitClass::Terraformer);
			const bool hasVehiclePlant = (hasUnitClasses & UnitClass::VehiclePlant);

			const bool hasMineralGeneratorCycle = hasFuelMine && hasMineralMine && hasGenerator;

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
			EntityType type = e->GetType();
			if (e->IsStructure())
				return researchState.CheckUnlocked(type.baseType,errorOut) && CheckLimit(CountStructureType(type.baseType), RuntimeConfig::UnitLimits::Get(type.baseType), errorOut);
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
				if (s0->lock()->GetType() == structureType)
					rs ++;
			}
			return rs;
		}


		count_t		Faction::CountTurretType(ConstName turretType) const
		{
			count_t rs = 0;
			foreach (this->towers, t0)
			{
				if (t0->lock()->turret->constName == turretType)
					rs ++;
			}
			foreach (this->allMobiles, m0)
			{
				if (m0->lock()->turret->constName == turretType)
					rs ++;
			}
			return rs;
		}



		void		Faction::RegisterExplosionCrater(const Grid::HCellIdI&center, float radius, float t)
		{
			knownWorld.RegisterExplosionCrater(center, radius, t);
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
		/*static*/ GenericHashTable<TUniqueID,PFaction>	Faction::idMap;
		/*static*/ PFaction						Faction::mine;

		/*static*/ Buffer0<Marker, Strategy::Adopt>	Marker::all; 
		/*static*/ GenericHashTable<TUniqueID, index_t>	Marker::idMap,
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


		/*static*/ PFaction				Faction::GetOrCreate(const TUniqueID&id, const TUniqueID&allianceID, bool&outAllianceChanged)
		{
			{
				PFaction rs = FindByID(id);
				if (rs)
				{
					if (rs->alliance && rs->alliance->id == allianceID)
						outAllianceChanged = false;
					else
					{
						outAllianceChanged = true;
						PAlliance newAlliance = Alliance::FindByID(allianceID);
						if (!newAlliance)
							newAlliance.reset(new Alliance(allianceID));
						rs->ChangeAlliance(newAlliance);
					}
					return rs;
				}
			}
			outAllianceChanged = true;
			PAlliance alliance (new Alliance(TUniqueID::New()));
			PFaction rs(new Faction(id));
			idMap.Set(id,rs);
			allKnown << rs;
			alliance->Join(rs);	//must init temporary unique alliance, since unknown at this point
			return rs;
		}


		/*static*/ PFaction				Faction::GetOrCreate(const TUniqueID&id)
		{
			{
				PFaction*rs = idMap.QueryPointer(id);
				if (rs)
					return *rs;
			}
			PAlliance alliance (new Alliance(TUniqueID::New()));
			PFaction rs(new Faction(id));
			idMap.Set(id,rs);
			allKnown << rs;
			alliance->Join(rs);	//must init temporary unique alliance, since unknown at this point
			return rs;
		}



		void	ChangeOwnership( const PWorldEntity&e,const PFaction&newOwner, bool isNowMine)
		{
			LogEvent(EventSignificance::Debug, e.get(),"Changing ownership to "+newOwner->GetName());
			PStructure s = To<Structure>::DynamicCast(e);
			#ifdef CLIENT
				//if (e->isOwnedByMe)
				{
					float t = 0.f;
					e->UnregisterSensory(t);
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
				//if (e->isOwnedByMe)
				{
					float t = 0.f;
					if (!s && e->IsMobile())
						e->RegisterSensory(To<MobileEntity>::Cast(e)->preAnimationLocation,t);
					else
						e->RegisterSensory(t);
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

	void	Faction::ChangeAlliance(const PAlliance&a)
	{
		if (alliance == a)
			return;

		#ifdef CLIENT
		const bool rebuildSensory = mine && (this->isOwnedByMe || alliance == mine->alliance || a == mine->alliance);

		if (rebuildSensory)
			ForeachWorldEntity([](const Objects::PWorldEntity&e)
			{
				float t = 0;
				e->UnregisterSensory(t);
			},false);
		#endif

		alliance->Leave(shared_from_this());
		a->Join(shared_from_this());

		#ifdef CLIENT
		if (rebuildSensory)
			ForeachWorldEntity([](const Objects::PWorldEntity&e)
			{
				float t = 0;
				e->RegisterSensory(t);
			},false);
		#endif
	}



	void			ForeachFaction(const std::function<void(const PFaction&)>&func, bool includeDead/*=false*/)
	{
		#ifdef CLIENT
			foreach (Faction::allKnown,a)
				func(*a);
		#else
			
			if (Faction::nexus && (!Faction::nexus->isDead || includeDead))
				func(Faction::nexus);
			for (index_t i = 0; i < Faction::all.Count(); i++)
			{
				PFaction f = Faction::all[i];
				if (!f->isDead || includeDead)
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
						//M::TVec3<> center = Grid::ToWorld(m->location);
						//if ()
						bool seen = m->age > 1 &&	//don't delete hostile markers in round <2. grid might be outdated
									 m->visualization && World::actual.GridCanSee(m->location,m->visualization->GetHeight(),m->visualization->IsBroad());
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


		/*static*/ void				Marker::Create(const TUniqueID&id, Type type, const Grid::LCellIdI&location, const RoundNumber& createdInRound, const TUniqueID&referenceID, const TUniqueID&referenceOwnerID, EntityType referenceType, const RoundNumber& currentRound)
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
				m.referencedType = referenceType;

				PWorldEntity prototype = Objects::Prototypes::all.Find(referenceType);
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
				{
					//vehicle
					if (referenceType.HasTurret())
					{
						Component::VehicleBase* base = Component::baseComponents.Find(referenceType.baseType);
						Component::Turret* turret = Component::vehicleTurretComponents.Find(referenceType.turretType);
						ASSERT1__(base, referenceType);
						ASSERT1__(turret, referenceType);
						m.visualization.reset(new Vehicle(OwnerParameter(), location, base, turret));
						mobileGhosts << To<MobileEntity>::Cast( m.visualization );
					}
					else
						LogEvent(EventSignificance::NonFatalError,"Unable to create ghost for type #"+String((int)referenceType.baseType));
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
					M::TMatrix4<> system;
					Mat::makeAxisSystem(Grid::ToWorld(m.location),Grid::low.cells[m.location.stackID].centerVertex.xyz,2,system);
					index_t systemID = visitedSystems.Count();
					visitedSystems.Append().system = system;
					visitedSystems.Last().age = m.age;
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
			ASSERT1__(idMap.QueryAndUnset(id, index),ToString(id));	//this mustn't happen anymore
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
						ApplicationInterface::OnVanish(m.visualization,RemovalType::Vanished,true);
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
				candidates.Last().priority = all[i].age;
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




		/*virtual override*/	void		Marker::Serialize(IWriteStream&s) const
		{
			s.WritePrimitive(id);
			s.WritePrimitive((UINT8)type);
			s.WritePrimitive(createdInRound);
			s.WritePrimitive(location);
			if (UtilizesReferenceEntity())
			{
				s.WritePrimitive(this->referenceEntity);
				s.WritePrimitive(this->referenceEntityOwner);
				using Serialization::Serialize;
				Serialize(s,ToString(referencedType));
			}
		}
		/*virtual override*/	void		Marker::Deserialize(IReadStream&inStream)
		{
			UINT8 typei;
			inStream.ReadPrimitive(id);
			inStream.ReadPrimitive(typei);
			inStream.ReadPrimitive(createdInRound);
			inStream.ReadPrimitive(location);
			type = Type::Reinterpret(typei);
			if (UtilizesReferenceEntity())
			{
				String t0;
				inStream.ReadPrimitive(this->referenceEntity);

				if (ApplicationInterface::globalVersionOfCurrentFile >= ApplicationInterface::FileVersions::MarkersHaveOwner)
				{
					inStream.ReadPrimitive(this->referenceEntityOwner);
				}
				else
					this->referenceEntityOwner = TUniqueID::Empty();
				using Serialization::Deserialize;
				Deserialize(inStream,t0);
				this->referencedType = DecodeType(t0);
				//this->referenceEntityType.Deserialize(inStream, fixedSize);
			}
			else
			{
				referenceEntity = TUniqueID::Empty();
				referencedType = EntityType();
			}
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



		void		Faction::RestoreMarkers(const Ctr::ArrayData<Marker>&source)
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
				LogEvent(EventClass::Preset::error,nullptr,"Unable to destroy Marker. ID not set " + ToString(markerID));
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



		/*static*/ PFaction			Faction::FindByID(const TUniqueID&id)
		{
			if (nexus->id == id)
				return nexus;
			foreach (all,_f)
			{
				const PFaction&f = *_f;
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


	/*static*/ void	Faction::Kill(PFaction f)
	{
		if (!f)
		{
			DBG_FATAL__("Trying to kill null-faction");
			LogEvent(EventClass::Preset::internalError,nullptr, "Trying to kill null-faction");
			return;
		}
		#ifndef CLIENT
		if (f == nexus)
		{
			LogEvent(EventClass::Preset::commandRejected,nullptr, "Cannot kill Nexus faction");
			return;
		}
		if (f->isDead)
		{
			LogEvent(EventClass::Preset::commandRejected,nullptr, "Faction "+ToString(f->id)+" has already ended");
			return;
		}
		#endif
			
		LogEvent(EventSignificance::Debug,nullptr, "Erasing faction "+ToString(f->id));

		#ifdef CLIENT
			if (!allKnown.FindAndErase(f))
			{
				LogEvent(EventClass::Preset::internalError,nullptr, "Trying to kill non-registered faction");
				return;
			}
			idMap.Unset(f->id);
			if (mine == f)
				mine = nullptr;

			ForeachMobileEntityIncludingStored([&f](const PWorldEntity&e)
			{
				float t = 0;
				if (e->owner == f)
					Remove(e,e->isInserted ? Objects::RemovalType::Vanished : Objects::RemovalType::DestroyedInStorage,t);
			},false);

			
			
		#else

			{
				auto lock = f->LockSensorUpdates();
				static Ctr::Buffer0<PWorldEntity>	toDelete;
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
					if (*del)
						Remove(*del,Objects::RemovalType::ControlledExplosion,t);
				toDelete.Clear();
				ASSERT__(f->allMobiles.IsEmpty());	//should have cascaded
				ASSERT__(f->structures.IsEmpty());
			}	//exiting lock: recomputing sensory of f, which is now empty, so should be fast

			f->isDead = true;
			f->hasUnitClasses = UnitClass::None;

			LogEvent(EventClass::Preset::factionDeath, "Destructible entities destroyed");
			ApplicationInterface::SignalFactionsChanged();
			ApplicationInterface::SignalNexusControlChange(f->alliance);
		#endif
	}

	float Next(std::mt19937&random)
	{
		return float(random()) / float(random.max());
	}

#if 0
	/*static*/ void Faction::IDToColor(M::TVec3<>&rs, const TUniqueID&id, int iteration/*=0*/)
	{
		USHORT	v0 = *(const USHORT*)id.bytes,
				v1 = *(((const USHORT*)id.bytes)+1),
				v2 = *(((const USHORT*)id.bytes)+2);
		int primary = v0 % 3;	//not perfectly evenly distributed since 65536 isn't divisible by 3, but it should be close enough
		int secondary = (primary + (v1 % 2) + 1) % 3;
		float secondaryLevel = (float(v2)/float(0xFFFF));
		Vec::clear(rs);
		rs.v[primary] = sqrt(1.f - M::Sqr(secondaryLevel));
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
			M::TVec3<> candidate;
			Vec::HSL2RGB(float3(random.NextFloat(),0.5f + random.NextFloat(0.5f),0.5f),candidate);
			count_t competition = 0;
			float closest = std::numeric_limits<float>::max();
			ForeachFaction([&](const PFaction&f)
			{
				if (&f->color != &outColor)
				{
					competition++;
					float dist = Vec::quadraticDistance(candidate,f->color);
					closest = M::Min(dist,closest);
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

	bool			Faction::IsAlliedWith(const PFaction&f)const
	{
		return f && alliance==f->alliance;
	}

	#ifdef CLIENT
		bool		Faction::IsAlliedWithMe() const
		{
			return this && (this->isOwnedByMe || (mine && alliance == mine->alliance));
		}
	#endif

}

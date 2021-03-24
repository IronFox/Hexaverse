#include <global_root.h>
#include "faction.h"
#include "../cast.h"
#include "mobileEntity.h"
#include "gridAndChoices.h"
#include "prototypes.h"
#include "satellite.h"
#include "vehicle.h"
#include "subOrbital.h"
#include "../sphereGridHelper.h"
#include "../runtimeConfig.h"
#include "prototypes.h"
#include "structure/tower.h"
#include "structure/vehicleStore.h"
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

	void		Alliance::CancelAllInvites()
	{
		if (invited.IsEmpty())
			return;
		LogEvent(EventSignificance::Common,"Canceling all invites of alliance "+ToString(id));
		#ifndef CLIENT
			foreach (invited,a)
				ApplicationInterface::SignalAllianceInvite(shared_from_this(), a->wFaction.lock(), Protocol::AllianceInviteType::Cancel);
		#endif
		invited.Clear();
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
			if (f->isDead && livingOnly)
				continue;
			visitor(f);
		}
	}

	#ifndef CLIENT

		void		Alliance::SignalMarkerCreation(const PFaction&owner, const PMarker&m)
		{
			ApplicationInterface::SignalMarkerCreationToAlliedFactions(shared_from_this(),owner,m);
		}


		void		Alliance::SignalMarkerDestruction(const PFaction&owner, const PMarker&m)
		{
			ApplicationInterface::SignalMarkerDestructionToAlliedFactions(shared_from_this(),owner,m->id);
		}


		void		Alliance::ForeachMarker(Objects::MarkerType type, const std::function<void(const PMarker&)>&func) const
		{
			ASSERT__(!Marker::TypeIsPrivate(type));
			ForeachMember([type,func](const PFaction&f)
			{
				f->markers.Foreach(type,func);
			});

		}
	#endif

	void		Alliance::ForeachMember(const std::function<void(const PFaction&)>&visitor, bool livingOnly)	const
	{
		foreach (members,m)
		{
			PFaction f = m->wFaction.lock();
			if (f->isDead && livingOnly)
				continue;
			visitor(f);
		}
	}

	count_t		Alliance::CountLivingMembers() const
	{
		if (!this)
			return 0;
		count_t cnt  = 0;
		foreach (members,m)
			if (!m->wFaction.lock()->isDead)
				cnt++;
		return cnt;
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
				this->ForeachMember([this](const PFaction&f)
				{
					f->VisitMyEntities([](const PWorldEntity&e)
					{
						e->RegisterChange(Objects::WorldEntity::CompleteChange);
					},config.exposeEntityDetails);
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
			}
		#endif
	}

	void		Alliance::Join(const PFaction&f)
	{
		ASSERT__(f);
		ASSERT__(!IsMember(f));
		LogEvent(EventSignificance::Debug,f, "@"+String((const void*)f.get())+ " Joined alliance "+ToString(id));


		
		#ifndef CLIENT
			for (index_t mi = 0; mi < MarkerType::N; mi++)
			{
				MarkerType t = MarkerType::Reinterpret(mi);
				if (!Marker::TypeIsPrivate(t))
				{
					ForeachMarker(t,[this,f](const PMarker&m)
					{
						ApplicationInterface::SignalMarkerCreation(f,m);
					});
				}
			}
		#endif



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


			for (index_t mi = 0; mi < MarkerType::N; mi++)
			{
				MarkerType t = MarkerType::Reinterpret(mi);
				if (!Marker::TypeIsPrivate(t))
				{
					f->markers.Foreach(t,[this,f](const PMarker&m)
					{
						SignalMarkerCreation(f,m);
					});
				}
			}
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

				#ifndef CLIENT
					for (index_t mi = 0; mi < MarkerType::N; mi++)
					{
						MarkerType t = MarkerType::Reinterpret(mi);
						if (!Marker::TypeIsPrivate(t))
						{
							this->ForeachMarker(t,[f](const Objects::PMarker&m)
							{
								ApplicationInterface::SignalMarkerDestruction(f,m->id);
							});
						}
					}
				#endif

				if (members.IsEmpty())
					all.Unset(id);
				else
				{
					RefreshSensors(false);
					#ifndef CLIENT
						ApplicationInterface::SignalNexusControlChange(shared_from_this());

						for (index_t mi = 0; mi < MarkerType::N; mi++)
						{
							MarkerType t = MarkerType::Reinterpret(mi);
							if (!Marker::TypeIsPrivate(t))
							{
								f->markers.Foreach(t,[this](const PMarker&m)
								{
									SignalMarkerDestruction(nullptr,m);
								});
							}
						}
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
				auto faction = m->wFaction.lock();
				members.erase(m);
				ASSERT_EQUAL__(members.Count()+1,cnt);
				LogEvent(EventSignificance::Common,"New member count in alliance "+ToString(id)+": "+String(members.Count()));
				f->alliance = nullptr;

				#ifndef CLIENT
				{
					if (faction)
						for (index_t mi = 0; mi < MarkerType::N; mi++)
						{
							MarkerType t = MarkerType::Reinterpret(mi);
							if (!Marker::TypeIsPrivate(t))
							{
								this->ForeachMarker(t,[faction](const Objects::PMarker&marker)
								{
									ApplicationInterface::SignalMarkerDestruction(faction,marker->id);
								});
							}
						}
					else
						LogEvent(EventSignificance::NonFatalError,"Unable to clear out deprecated markers. Faction handle lost");
				}
				#endif

				if (members.IsEmpty())
					all.Unset(id);
				else
				{
					RefreshSensors(false);
					#ifndef CLIENT
						if (faction)
							for (index_t mi = 0; mi < MarkerType::N; mi++)
							{
								MarkerType t = MarkerType::Reinterpret(mi);
								if (!Marker::TypeIsPrivate(t))
								{
									faction->markers.Foreach(t,[this](const PMarker&m)
									{
										SignalMarkerDestruction(nullptr,m);
									});
								}
							}
					#endif
				}
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


	const Research::Attributes::Table& Faction::GetAttributeTable() const
	{
		return this ? researchState.attributeTable : Research::globalState.zeroTable;
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
			foreach (allTurretBases,b0)
			{
				PTurretBase b = b0->lock();
				if (b)
					rs += b->GetDamagePotentialPerRound();
			}
			return rs;
		}

		count_t		Faction::GetTotalEntityValue() const
		{
			count_t rs = 0;
			foreach (activeMobiles,m0)
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
			LogEvent(EventSignificance::Common,"Recreating Nexus faction using ID "+ToString(id));
			nexus.reset(new Faction(id, Faction::nexusName,true));
			PAlliance alliance { new Alliance(TUniqueID::New()) };
			alliance->Join(nexus);

			ApplicationInterface::SignalFactionsChanged();
		}

		int		GetStorage(const MobileEntity&v, Resource::ID res)
		{
			#ifndef ONE_COMPOUND_PER_FACTION
				return v.currentStorage[res];
			#else
				const Compound&compound = v.owner->compound;
				return compound.total.storage[res] + compound.GetOverproduction(res);
			#endif

		}

		bool	CanWithdrawFromStorage(const MobileEntity&v, Resource::ID res, int demand)
		{
			if (demand == 0)
				return true;
			return GetStorage(v,res) >= demand;
		}


		void ChangeStorage(WorldEntity&e, float&atTime, const Resource::Vector&max, Resource::Vector&storage, Resource::ID res, int change)
		{
			if (change == 0)
				return;
			storage.Add(res, change);
			LogEvent(EventSignificance::Common,&e,String(res.ToString())+ " storage changed by "+String(change)+" to "+String(storage[res])+"/"+String(max[res]));
			e.SignalChangeByExternalCause(WorldEntity::StorageChanged,atTime);
			ASSERT_IS_CONSTRAINED__(storage[res] , 0, max[res]);
		}

		bool FillStorage(WorldEntity&e, float&atTime, const Resource::Vector&max, Resource::Vector&storage)
		{
			if (storage == max)
				return false;
			storage = max;
			LogEvent(EventSignificance::Common,&e,"Storage filled to "+storage.ToString()+"/"+max.ToString());
			e.SignalChangeByExternalCause(WorldEntity::StorageChanged,atTime);
			return true;
		}

		void ChangeStorage(WorkingStructure&ws, float&atTime, Resource::ID res, int change)
		{
			if (change == 0)
				return;
			Resource::Vector dummy,current,max;
			ws.GetResourceVector(dummy,current,max);
			ChangeStorage(ws,atTime,max,current,res,change);
			ws.SetResourceStorage(current);
			#ifdef _DEBUG
				Resource::Vector testCurrent;
				ws.GetResourceVector(dummy,testCurrent,max);
				ASSERT2__(current == testCurrent,current.ToString(),testCurrent.ToString());
			#endif
		}

		void	FillStorage(WorkingStructure&ws, float&atTime)
		{
			Resource::Vector dummy,current,max;
			ws.GetResourceVector(dummy,current,max);
			if (FillStorage(ws,atTime,max,current))
			{
				ws.SetResourceStorage(current);
				#ifdef _DEBUG
					Resource::Vector testCurrent;
					ws.GetResourceVector(dummy,testCurrent,max);
					ASSERT2__(current == testCurrent,current.ToString(),testCurrent.ToString());
				#endif
			}
		}

		void	FillStorage(MobileEntity&m, float&atTime)
		{
			#ifndef ONE_COMPOUND_PER_FACTION
				FillStorage(v,atTime,v.GetMaxStorageCapacity(),v.currentStorage);
			#endif
		}

		void ChangeCompoundStorage(Compound&compound, MobileEntity*originatingAt, float&atTime, Resource::ID res, int change)
		{
			Resource::Vector demand;

			if (change < 0 )
			{
				Resource::Vector withdraw;
				withdraw.Set(res, -change);
				compound.TryWithdrawOverproduction(withdraw);
				change = -withdraw[res];
				if (change == 0)
					return;
			}
			demand.Set(res, change);
			compound.ChangeStorage(demand,atTime);
		}


		void ChangeStorage(MobileEntity&v, float&atTime, Resource::ID res, int change)
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
			if (e->IsWorkingStructure())
			{
				PWorkingStructure w = To<WorkingStructure>::Cast(e);
				if (w->WouldLikeToEnable())
					w->SetEnabled(true,atTime);
			}

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



		template <typename T, typename S, typename F>
			static bool	FindAndEraseF(BasicBuffer<T,S>&container, const F&f)
			{
				foreach (container,item)
					if (f(*item))
					{
						container.erase(item);
						return true;
					}
				return false;
			}
		template <typename T>
			static bool FindAndErase(BasicBuffer<std::weak_ptr<T> >&container, const std::shared_ptr<T>&match)
			{
				foreach (container,item)
					if (item->lock() == match)
					{
						container.erase(item);
						return true;
					}
				return false;
			}


		void		Faction::RegisterLoss(const PWorldEntity&e)
		{
			if (e->IsMobile())
			{
				auto m = To<MobileEntity>::Cast(e);
				if (m->storedIn.expired())
					RegisterActiveMobileLoss(m);
				else
					RegisterStoredMobileLoss(m);
			}
			else
				if (e->IsStructure())
					RegisterStructureLoss(To<Structure>::Cast(e));
				else
					FATAL__("WTF is this: "+e->GetName());
		}


		void		Faction::RegisterStructureGain( const PStructure&s )
		{
			structures << s;
			if (s->IsTower())
			{
				const PTower t = To<Tower>::Cast(s);
				if (t->turret->CanAttack() && t->turret->attacksMovingHostilesDuringMotionPhase)
					autoFiringTurrets << t;
				allTurretBases << t;
				if (t->turret->subOrbitalScanningRange.IsFunctionOrConstNonZero())
					subOrbitalSensors.Append(std::make_pair(t,s));
			}
		}

		void		Faction::RegisterStructureLoss( const PStructure&s )
		{
			DBG_VERIFY2__(FindAndErase(structures,s),s->GetName(),this->GetLogName());
			if (s->IsTower())
			{
				const PTower t = To<Tower>::Cast(s);
				if (t->turret->CanAttack() && t->turret->attacksMovingHostilesDuringMotionPhase)
					DBG_VERIFY2__(FindAndErase(autoFiringTurrets,To<TurretBase>::Cast(t)),s->GetName(),GetLogName());
				DBG_VERIFY2__(FindAndErase(allTurretBases,To<TurretBase>::Cast(t)),s->GetName(),this->GetLogName());
				if (t->turret->subOrbitalScanningRange.IsFunctionOrConstNonZero())
					DBG_VERIFY2__(FindAndEraseF(subOrbitalSensors,[s](const TSubOrbitalSensor&p){return p.second.lock() == s;}), s->GetName(),GetLogName());
			}
		}



		void		Faction::RegisterActiveMobileGain( const PMobileEntity&v )
		{
			ASSERT1__(v,v->GetName());
			ASSERT1__(v->storedIn.expired(),v->GetName());
			LogEvent(EventSignificance::Common,v.get(),"RegisterActiveMobileGain");

			if (v->turret->subOrbitalScanningRange.IsFunctionOrConstNonZero())
				subOrbitalSensors.Append(std::make_pair(v,v));
			if (v->turret->attacksMovingHostilesDuringMotionPhase)
				autoFiringTurrets << v;
			allTurretBases << v;
			activeMobiles << v;
			#ifndef NO_SATELLITES
				if (v->IsSatellite())
				{
					PSatellite sat = To<Satellite>::Cast(v);
					this->satellites << sat;
					if (sat->IsSkyHub())
						this->skyHubs << To<SkyHub>::Cast(sat);
				}
			#endif
			if (v->IsPlane())
				this->planes << To<SubOrbital::Plane>::Cast(v);
		}

		void		Faction::RegisterActiveMobileLoss( const PMobileEntity&e )
		{
			ASSERT1__(e,e->GetName());
			DBG_ASSERT1__(e->storedIn.expired(),e->GetName());
			LogEvent(EventSignificance::Common,e.get(),"RegisterActiveMobileLoss");

			if (e->turret->subOrbitalScanningRange.IsFunctionOrConstNonZero())
				DBG_VERIFY2__(FindAndEraseF(subOrbitalSensors,[e](const TSubOrbitalSensor&p){return p.second.lock() == e;}), e->GetName(),GetLogName());
			if (e->turret->attacksMovingHostilesDuringMotionPhase)
				DBG_VERIFY2__(FindAndErase(autoFiringTurrets,To<TurretBase>::Cast(e)),e->GetName(),this->GetLogName());
			DBG_VERIFY2__(FindAndErase(activeMobiles,e),e->GetName(),this->GetLogName());
			DBG_VERIFY2__(FindAndErase(allTurretBases,To<TurretBase>::Cast(e)),e->GetName(),this->GetLogName());
			#ifndef NO_SATELLITES
				if (e->IsSatellite())
				{
					const auto sat = To<Satellite>::Cast(e);
					DBG_VERIFY2__(FindAndErase(satellites,sat),sat->GetName(),GetLogName());
					if (sat->IsSkyHub())
						DBG_VERIFY2__(FindAndErase(skyHubs,sat),sat->GetName(),GetLogName());
				}
			#endif
			if (e->IsPlane())
				DBG_VERIFY2__(FindAndErase(planes,To<SubOrbital::Plane>::Cast(e)),e->GetName(),GetLogName());
		}


		void		Faction::RegisterStoredMobileGain( const PMobileEntity&m )
		{
			ASSERT1__(m,m->GetName());
//			ASSERT1__(!m->storedIn.expired(),m->GetName());

			LogEvent(EventSignificance::Common,m.get(),"RegisterStoredMobileGain");
			storedMobiles << m;
			allTurretBases << m;
		}

		void		Faction::RegisterStoredMobileLoss( const PMobileEntity&e )
		{
			LogEvent(EventSignificance::Common,e.get(),"RegisterStoredMobileLoss");
			DBG_VERIFY2__(FindAndErase(allTurretBases,To<TurretBase>::Cast(e)),e->GetName(),this->GetLogName());
			DBG_VERIFY2__(FindAndErase(storedMobiles,e),e->GetName(),this->GetLogName());
		}




		void Faction::VisitMyAutoFiringTurrets(const std::function<void(const PTurretBase&)>&f) const
		{
			foreach (autoFiringTurrets,_t)
				f(_t->lock());
		}

		void Faction::VisitMyActiveMobileEntities(const std::function<void(const PMobileEntity&)>&f) const
		{
			foreach (activeMobiles,m)
				f(m->lock());
		}

		void Faction::VisitMyMobileEntities(const std::function<void(const PMobileEntity&)>&f) const
		{
			foreach (activeMobiles,m)
				f(m->lock());
			foreach (storedMobiles,m)
				f(m->lock());
		}


		void Faction::VisitMyStructures(const std::function<void(const PStructure&)>&f) const
		{
			foreach (structures,s)
				f(s->lock());
		}


		void Faction::VisitVisibleForeignEntities(const std::function<void(const PWorldEntity&e)>&f)	const
		{
			seeing.VisitAllValues([&f](const Objects::Faction::SeeingReference&ref)
			{
				if (ref.CurrentlyVisible())
				{
					DBG_ASSERT__(ref.reverseVisibilityChange.empty());
					auto e = ref.reference.lock();
					if (e)
						f(e);
				}
			});
		};


		void Faction::VisitVisibleForeignEntities(float t, const std::function<void(const PWorldEntity&e)>&f)	const
		{
			seeing.VisitAllValues([&f,t](const Objects::Faction::SeeingReference&ref)
			{
				if (ref.VisibleAt(t))
				{
					auto e = ref.reference.lock();
					if (e)
						f(e);
				}
			});
		};




		bool		Faction::HasActiveMobile(const PMobileEntity&v) const
		{
			foreach (activeMobiles,_v)
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

		void		Faction::VisitMyEntities(const std::function<void(const PWorldEntity&)>&f, bool includeStored) const
		{
			foreach (structures,s)
				f(s->lock());
			foreach (activeMobiles,m)
				f(m->lock());
			if (includeStored)
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
			foreach (this->activeMobiles,_m)
			{
				Objects::PWorldEntity e = _m->lock();
				if (e)
					e->InsertSensoryInto(worldData,t);
			}
		}

		void	Faction::CollectSensors(Buffer0<World::Sensor>&outSensors, float&t) const
		{
			foreach (this->structures,_s)
			{
				Objects::PStructure s = _s->lock();
				if (s)
					s->VisitSensors(s->location,[&outSensors](const World::Sensor&s)
					{
						outSensors.Append(s);
					});
			}
			foreach (this->activeMobiles,_m)
			{
				Objects::PWorldEntity e = _m->lock();
				if (e)
					e->VisitSensors(e->location,[&outSensors](const World::Sensor&s)
					{
						outSensors.Append(s);
					});
			}
		}

		void Faction::HandlePotentialAppearance(const Objects::PWorldEntity&e, float&t, bool shareSensors)
		{
			bool appear = shareSensors && IsAlliedWith(e->owner);

			if (!appear)
			{
				if (e->IsAboveTerrain())
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
			bool stillVisible = false;
			if (e->IsMobile() && !To<MobileEntity>::Cast(e)->storedIn.expired())//stored in something
				stillVisible = IsAlliedWith(e->owner) && alliance->config.exposeEntityDetails;
			else
			{
				stillVisible = shareSensors && IsAlliedWith(e->owner);

				if (!stillVisible)
				{


					if (e->IsAboveTerrain())
					{
						count_t cover = CountOffGridCoverage(e->location.height, e->GetWorldPosition());
						stillVisible = cover > 0;
					}
					else
						stillVisible = GridCanSee(e,t);
				}
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
				LogEvent(EventSignificance::Common,shared_from_this(),"Recomputing sensory");
				Time::Stamp started;

				static Buffer0<World::Sensor>	sensors;
				sensors.Clear();

				if (alliance->config.shareSensors)
				{
					alliance->ForeachMember([this,&t](const PFaction&f)	//includes this
					{
						f->CollectSensors(sensors, t);
					},true);
				}
				else
					this->CollectSensors(sensors,t);

				this->VisitMyEntities([](const PWorldEntity&e)
				{
					e->OverrideSetRegisteredSensorToCurrentLocation();
				},false);


				knownWorld.FlushAndValidateSensory();

				LogEvent(EventSignificance::PerformanceMeasurement,shared_from_this(),"Collected sensors and flushed known world in "+String(started.ElapsedSeconds()));
				started.Update();


				{
					auto lock = knownWorld.LockOnChange();
					Sys::ParallelFor(sensors.Count(),4,[this,t](index_t index,index_t processorIndex)
					{
						sensors[index].InsertInto(knownWorld, t,processorIndex);
					});
				}
				LogEvent(EventSignificance::PerformanceMeasurement,shared_from_this(),"Computed sensory grid coverage in "+String(started.ElapsedSeconds()));
				started.Update();
				const bool shareSensors = alliance->config.shareSensors;
				ForeachWorldEntity([this,&t,shareSensors](const PWorldEntity&e)
				{
					if (e->owner.get() == this)
						return;
					HandlePotentialAppearance(e,t,shareSensors);
				});

				if (alliance->config.exposeEntityDetails)
					storedMobileEntities.Foreach([this,&t,shareSensors](const PMobileEntity&m)
					{
						if (m->owner.get() == this)
							return;
						HandlePotentialAppearance(m,t,shareSensors);
					});

				LogEvent(EventSignificance::PerformanceMeasurement,shared_from_this(),"Computed appearances in "+String(started.ElapsedSeconds()));
				started.Update();


				VisitVisibleForeignEntities(t,[this,&t,shareSensors](const PWorldEntity&e)
				{
					HandlePotentialDisappearance(e,t,shareSensors);
				});
				LogEvent(EventSignificance::PerformanceMeasurement,shared_from_this(),"Computed disappearances in "+String(started.ElapsedSeconds()));
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
			if (e->owner.get() == this)
				return true;
			//if ( SharingSensorWith(e->owner) )	//theoretically, this should be identical, but practically it may not. therefor, for a non-owning ally, we should always check this->seeing (timing issues, etc)
				//return true;
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

		void					Faction::SeeingReference::Appear( const Faction*parent, const PWorldEntity&e, float&t)
		{
			ASSERT__(!CurrentlyVisible());
			if (RuntimeConfig::IsInAnyCommandPhase())
			{
				LogEvent(EventSignificance::Debug,e.get(),parent->GetLogName()+ ": ["+String(e.get())+"] Registering as newly visible now");
				if (!reverseVisibilityChange.empty())
				{
					LogEvent(EventClass::Preset::internalError,"reverseVisibilityChange still contains "+String(reverseVisibilityChange.size())+" element(s)");
					reverseVisibilityChange.clear();	//this should be the case implicitly, but for some reason it's not
				}
				reference = e;
				wasVisibleAtBeginning = true;
				ASSERT__(CurrentlyVisible());
				return;
			}

			if (!reverseVisibilityChange.empty())
				t = std::max(t,-reverseVisibilityChange.begin()->first);
			LogEvent(EventSignificance::Debug,e.get(),parent->GetLogName()+ ": ["+String(e.get())+"] Registering as newly visible at "+String(t));

			t += timeEpsilon;
			reverseVisibilityChange[-t] = true;
			ASSERT__(CurrentlyVisible());
			e->SyncTime(t);
			reference = e;
		}

		void					Faction::SeeingReference::Disappear(const Faction*parent, const PWorldEntity&e, float&t)
		{
			ASSERT__(CurrentlyVisible());
			if (RuntimeConfig::IsInAnyCommandPhase())
			{
				LogEvent(EventSignificance::Debug,e.get(),parent->GetLogName()+ ": ["+String(e.get())+"] Registering as newly INvisible now");
				if (!reverseVisibilityChange.empty())
				{
					LogEvent(EventClass::Preset::internalError,"reverseVisibilityChange still contains "+String(reverseVisibilityChange.size())+" element(s)");
					reverseVisibilityChange.clear();	//this should be the case implicitly, but for some reason it's not
				}
				wasVisibleAtBeginning = false;
				ASSERT__(!CurrentlyVisible());
				return;
			}
			if (!reverseVisibilityChange.empty())
				t = std::max(t,-reverseVisibilityChange.begin()->first);
			LogEvent(EventSignificance::Debug,e.get(),parent->GetLogName()+ ": ["+String(e.get())+"] Registering as newly INvisible at "+String(t));

			t += timeEpsilon;
			reverseVisibilityChange[-t] = false;
			ASSERT__(!CurrentlyVisible());
			e->SyncTime(t);
		}

		bool		Faction::SignalAppearance(const PWorldEntity&e, float&t, bool cascade/*=true*/)
		{
			SeeingReference&ref = seeing.Set(e.get());
			if (ref.CurrentlyVisible())
				return false;
			ref.reference = e;	//just in case
			
			ref.Appear(this,e,t);

			LogEvent(EventSignificance::Common,e.get(),this->GetLogName()+": Gained visual at time="+String(t));

			if (cascade)
				ApplicationInterface::SignalAppearanceToHostileFaction(shared_from_this(),e,t);

			e->SyncTime(t);

			return true;
		}

		void		Faction::SignalDisappearance(const PWorldEntity&e, float&t, bool force, bool signalConnectedClient/*=true*/)
		{
			SeeingReference*ref = seeing.QueryPointer(e.get());
			if (!ref || !ref->CurrentlyVisible())
				return;
			ref->Disappear(this,e,t);

			LogEvent(EventSignificance::Common,e.get(),this->GetLogName()+": Lost visual at time="+String(t));
			if (signalConnectedClient)
				ApplicationInterface::SignalDisappearanceToHostileFaction(shared_from_this(),e,t);

			e->SyncTime(t);
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

		count_t		Faction::CountOrbitalSensorCoverage(const M::Sphere<>&sphere) const
		{
			count_t rs = 0;
			foreach (subOrbitalSensors,s0)
			{
				auto s = s0->second.lock();
				if (!s)
				{
					LogEvent(EventClass::Preset::internalError,"Found abandoned sub orbital sensor");
					continue;
				}
				if (s->empDamageLevel > 0)
					continue;
				const float r = s0->first.lock()->GetSubOrbitalScanningRange() + sphere.radius;
				const float dist = Vec::quadraticDistance(Grid::ToWorld(Grid::LCellIdI(s->location.stackID,SubOrbital::Height)),sphere.center);
				if (dist <= r*r)
					rs++;
			}
			return rs;
		}


		count_t		Faction::CountOrbitalSensorCoverage(const M::TVec3<>&p) const
		{
			count_t rs = 0;
			foreach (subOrbitalSensors,s0)
			{
				auto s = s0->second.lock();
				if (!s)
				{
					LogEvent(EventClass::Preset::internalError,"Found abandoned sub orbital sensor");
					continue;
				}
				if (s->empDamageLevel > 0)
					continue;
				const float r = s0->first.lock()->GetSubOrbitalScanningRange();
				const float dist = Vec::quadraticDistance(Grid::ToWorld(Grid::LCellIdI(s->location.stackID,SubOrbital::Height)),p);
				if (dist <= r*r)
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
			if (channelHeight == SubOrbital::Height)
				return 
					CountRawSensorCoverage(p,planes)
					+
					CountOrbitalSensorCoverage(p);
				;

			return 0;
		}

		count_t		Faction::CountOffGridCoverage(const M::Sphere<>&p)	const
		{
			count_t rs = 0;
			#ifndef NO_SATELLITES
				rs += CountRawSensorCoverage(p,satellites);
			#endif
			rs += CountRawSensorCoverage(p,planes);
			rs += CountOrbitalSensorCoverage(p);

			return rs;
		}



		void		Faction::CheckGridVisibility(const PWorldEntity&e,float&timeAt)
		{
			if (!e || e->owner.get() == this)
				return;
			const bool visible = CanSee(e,timeAt);
			LogEvent(EventSignificance::Debug,e.get(),this->GetLogName()+ ": CheckGridVisibility at time "+String(timeAt)+", visible="+String(visible));
			if (visible)
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
						if (sat->owner.get() != this && sensor.Contains(sat->location))
						{
							if (SignalAppearance(sat,t,true) && newContacts)
								newContacts->Append(sat);
						}
					}
				}
			#endif
			if (sensor.DetectsSubOrbitalObjects())
			{
				Objects::SubOrbital::planes.Foreach([&sensor,this,newContacts,&t](const SubOrbital::PPlane&plane)
				{
					if (plane->owner.get() != this && sensor.Contains(plane->location))
					{
						if (SignalAppearance(plane,t,true) && newContacts)
							newContacts->Append(plane);
					}
				}
				);
			}
			
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
				const bool offGrid = sensor.DetectsSubOrbitalObjects();
					//sensorHeight > Grid::CellStack::TopHeight || sensor.gridCenter.layer >= Grid::CellStack::Range;

				this->VisitVisibleForeignEntities(t,[this,offGrid, sensor,&t](const PWorldEntity&s)
				{
					if (s->IsAboveTerrain() != offGrid)
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

			const bool hasFuelMine = (hasUnitClasses & UnitClass::FuelMine) || (hasUnitClasses & UnitClass::NexusPort);
			const bool hasMineralMine = (hasUnitClasses & UnitClass::MineralMine) || (hasUnitClasses & UnitClass::NexusPort);
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
			if (e->IsIndestructible() && !RuntimeConfig::tutorialMode)
			{
				errorOut = "Type is non-constructible";
				return false;	//never build these
			}
			if (this == nexus.get())
				return true;
			if (e->IsMobile())
			{
				if (e->IsVehicle())
				{
					const Component::VehicleBase*base = To<Vehicle>::Cast(e)->base;

					if (!base->isUnlocked.GetValue(researchState.attributeTable))
					{
						errorOut = base->typeName+" has not (yet) been unlocked";
						return false;
					}
				}
				const Component::Turret*turret = To<MobileEntity>::Cast(e)->turret;
				ConstName type = turret->constName;
				if (!CheckLimit(CountMobileType(type), RuntimeConfig::UnitLimits::Get(type), errorOut))
					return false;
				if (!turret->isUnlocked.GetValue(researchState.attributeTable))
				{
					errorOut = turret->typeName+" has not (yet) been unlocked";
					return false;
				}
				return true;
			}
			EntityType type = e->GetType();
			if (e->IsStructure())
			{
				if (!CheckLimit(CountStructureType(type), RuntimeConfig::UnitLimits::Get(type), errorOut))
					return false;
				if (!e->IsUnlockedAtState(researchState.attributeTable))
				{
					errorOut = e->GetTypeName()+" has not (yet) been unlocked";
					return false;
				}
				return true;
			}
			FATAL__("Unknown type: "+e->GetName());
			return false;
		}
		bool		Faction::CanSpawnFromStorage(const PWorldEntity&e, String&errorOut)	const
		{
			return true;
			//return CanSpawnType(e,errorOut);	we count all units. no exception for stored units, therefore deployment will always succeed from storage
		}

		count_t		Faction::CountMobileType(ConstName turretType) const
		{
			count_t rs = 0;
			foreach (this->activeMobiles, v0)
			{
				PMobileEntity m = v0->lock();
				if (m->turret->constName == turretType)
					rs ++;
			}
			foreach (this->storedMobiles, v0)
			{
				PMobileEntity m = v0->lock();
				if (m->turret->constName == turretType)
					rs ++;
			}
			return rs;
		}

		count_t		Faction::CountStructureType(EntityType structureType) const
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
			foreach (this->allTurretBases, t0)
			{
				if (t0->lock()->turret->constName == turretType)
					rs ++;
			}
			return rs;
		}



		void		Faction::RegisterExplosionCrater(const Grid::HCellIdI&center, float radius, float t)
		{
			knownWorld.RegisterExplosionCrater(center, radius, t);
		}


		PMarker		Faction::CreateMarker(MarkerType type, const ArrayRef<BYTE>&data, bool fromSaveGame)
		{
			auto s = MemReadStream(data.GetPointer(), data.GetContentSize());
			return CreateMarker(type,s,fromSaveGame);
		}

		PMarker		Faction::CreateMarker(MarkerType type, IReadStream&data, bool fromSaveGame)
		{
			PMarker rs = markers.Create(type,data,fromSaveGame,[this](const PMarker&m)->bool
			{
				bool result = true;
				ForeachFaction([m,&result,this](const PFaction&f)
				{
					if (f->markers.FindByID(m->id))
					{
						LogEvent(EventClass::Preset::commandRejected, shared_from_this(), "Marker creation denied. ID is already in use by other faction: "+f->GetLogName());
						result = false;
					}
				});
				return result;
			});

			if (!rs)
				return nullptr;

			rs->ownerFactionID = this->id;

			if (!fromSaveGame)
			{
				if (!rs->IsPrivate())	//alliance and spectators see this
					alliance->SignalMarkerCreation(shared_from_this(),rs);
			}


			return rs;
		}

		bool		Faction::DestroyMarker(const TUniqueID&id)
		{
			PMarker m = markers.Destroy(id);
			if (m)
			{
				if (!m->IsPrivate())
					alliance->SignalMarkerDestruction(shared_from_this(),m);
				return true;
			}
			return false;
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
		/*static*/ HashTable<TUniqueID,PFaction>	Faction::idMap;
		/*static*/ PFaction						Faction::mine;


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

			PAlliance alliance = Alliance::FindByID(allianceID);
			if (!alliance)
				alliance.reset(new Alliance(allianceID.IsNotEmpty() ? allianceID : TUniqueID::New()));

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
			},IncludeGhosts::None);
		#endif

		alliance->Leave(shared_from_this());
		a->Join(shared_from_this());

		#ifdef CLIENT
		if (rebuildSensory)
			ForeachWorldEntity([](const Objects::PWorldEntity&e)
			{
				float t = 0;
				e->RegisterSensory(t);
			},IncludeGhosts::None);
		#endif
	}



	void			ForeachFaction(const std::function<void(const PFaction&)>&func, bool includeDead/*=false*/)
	{
		#ifdef CLIENT
			foreach (Faction::allKnown,f)
				if (!(*f)->isDead || includeDead)
					func(*f);
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



	void			ForeachMobileWorldEntity(const std::function<void(const PMobileEntity&)>&func, IncludeGhosts include)
	{
		vehicles.Foreach(func);
		#ifndef NO_SATELLITES
			satellites.Foreach(func);
		#endif
		SubOrbital::planes.Foreach(func);
		#ifdef CLIENT
			MarkerStore::client.GetArtifacts().ForeachMobileGhost(func,include);
		#endif
	}

	void			ForeachMobileEntityIncludingStored(const std::function<void(const PMobileEntity&)>&func, IncludeGhosts include)
	{
		ForeachMobileWorldEntity(func,include);
		storedMobileEntities.Foreach(func);
	}


	void			ForeachWorldEntityIncludingStored(const std::function<void(const PWorldEntity&)>&func, IncludeGhosts include)
	{
		ForeachWorldEntity(func,include);
		storedMobileEntities.Foreach(func);
	}

	void ForeachWorldEntity(const std::function<void(const PWorldEntity&)>&func, IncludeGhosts include)
	{
		ForeachMobileWorldEntity(func,include);
		structures.Foreach(func);
		#ifdef CLIENT
			MarkerStore::client.GetArtifacts().ForeachStructureGhost(func,include);
		#endif
	}


	

	#ifndef CLIENT

	/*
		Marker&		Faction::CreateMarker(const TUniqueID&id, bool priveleged, Marker::Type type, const TUniqueID&referenceID, const TUniqueID&referenceOwnerID)
		{
			ASSERT__(CanCreateMarker(priveleged)); 

			PMarker&rs = markers.Create(


			Marker&rs = markers.Append(); 
			rs.id = id; 
			rs.type = type;
			rs.refEntityID = referenceID;
			rs.refEntityOwnerID = referenceOwnerID;
			markerMap.Set(rs.id, markers.Count() - 1); 
			if (rs.UtilizesReferenceEntity())
				ASSERT__(referenceMap.SetNew(referenceID,markers.Count()-1));


			if (rs.UtilizesReferenceEntity())
			{
				if (referenceOwnerID.IsEmpty())
					LogEvent(EventClass::Preset::uncommon,shared_from_this(), "Marker defined for entity "+ToString(referenceID)+", but owner ID is empty");
				elif (!FindByID(referenceOwnerID))
					LogEvent(EventClass::Preset::uncommon,shared_from_this(), "Marker defined for entity "+ToString(referenceID)+", but owner ID could not be matched: "+ToString(referenceOwnerID));
			}

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
					ASSERT__(referenceMap.SetNew(m->refEntityID,m - markers.pointer()));

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
				ASSERT__(referenceMap.Unset(markers[at].refEntityID));

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
						ASSERT__(referenceMap.Query(m->refEntityID,at));
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
		*/



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
			LogEvent(EventClass::Preset::internalError,nullptr, "Cannot kill Nexus faction");
			return;
		}
		if (f->isDead)
		{
			LogEvent(EventClass::Preset::internalError,nullptr, "Faction "+ToString(f->id)+" has already ended");
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
			},IncludeGhosts::None);

			
			f->alliance->Leave(f);
			LogEvent(EventClass::Preset::debug, "Faction death: Destructible entities destroyed");

			
		#else

			{
				auto lock = f->LockSensorUpdates();
				static Ctr::Buffer0<PWorldEntity>	toDelete;
				for (index_t i = 0; i < f->structures.Count(); i++)
					if (!f->structures[i].lock()->IsIndestructible())
					{
						toDelete << f->structures[i].lock();
					}

				for (index_t i = 0; i < f->activeMobiles.Count(); i++)
					//if (vehicles[i]->owner == f)
					{
						toDelete << f->activeMobiles[i].lock();
					}
				float t=0;
				foreach (toDelete,del)
					if (*del)
						Remove(*del,Objects::RemovalType::ControlledExplosion,t);
				toDelete.Clear();
				ASSERT__(f->activeMobiles.IsEmpty());	//should have cascaded
				ASSERT__(f->structures.IsEmpty());	//even if nexus buildings were owned: should have reverted to nexus faction
			}	//exiting lock: recomputing sensory of f, which is now empty, so should be fast

			f->isDead = true;
			f->hasUnitClasses = UnitClass::None;

			LogEvent(EventClass::Preset::debug, "Faction death: Destructible entities destroyed");
			ApplicationInterface::SignalFactionsChanged();
			ApplicationInterface::SignalNexusControlChange(f->alliance);
			ApplicationInterface::SignalFactionDied(f);
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

	void		Faction::Setup(const TUniqueID&toID,bool nexusFaction)
	{
		sharedPark.reset(new MobilePark());
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

		bool		Faction::SharesSensorsWithMe() const
		{
			if (!this || !mine)
				return false;
			if (isOwnedByMe)
				return true;
			if (!alliance || alliance != mine->alliance)
				return false;
			return alliance->config.shareSensors;
		}

	#endif

}

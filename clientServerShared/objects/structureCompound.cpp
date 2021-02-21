#include <global_root.h>
#include "structureCompound.h"
#include "structure.h"
#include "faction.h"

namespace Objects
{
	#ifndef ONE_COMPOUND_PER_FACTION
		Buffer0<Compound,Swap>		compounds;
	#endif



	void				Compound::AppendStructure(Structure*s)
	{
		if (s->IsWorkingStructure())
		{
			workingStructures << To<WorkingStructure>::Cast( s->shared_from_this());
		}
		allStructures << To<Structure>::Cast( s->shared_from_this() );

	}



	static EventClass	forceEnable(EventSignificance::Uncommon,EventFlavor::Gain),
						forceDisable(EventSignificance::Uncommon,EventFlavor::Loss);

	/*static*/ PWorkingStructure	Compound::Reference(const WWorkingStructure&_s)
	{
		PWorkingStructure s = _s.lock();
		if (!s || s->IsDeadOrGhost())
			return PWorkingStructure();
		return s;
	}

	count_t						Compound::CountWorkingStructures() const
	{
		count_t rs = 0;
		foreach (workingStructures,_s)
		{
			if (Reference(*_s))
				rs++;
		}
		return rs;
	}


	#ifndef CLIENT

		WorkingStructure*	Compound::lastDisabled = nullptr;

		Resource::Vector		Compound::GetOverproduction() const
		{
			Resource::Vector result;
			foreach (workingStructures,_s)
			{
				PWorkingStructure s = Reference(*_s);
				if (s)
					result += s->overproduction;
			}
			return result;
		}

		int					Compound::GetOverproduction(Resource::ID res) const
		{
			int result = 0;
			foreach (workingStructures,_s)
			{
				PWorkingStructure s = Reference(*_s);
				if (s)
					result += s->overproduction[res];
			}
			return result;
		}


		void				Compound::DistributeOverproduction(const Resource::Vector&v)
		{
			if (v.IsZero())
				return;
			int active = (int)CountWorkingStructures();
			if (!active)
				return;
			Resource::Vector perStructure,remainder;
			for (index_t i = 0; i < Resource::table.GetSize(); i++)
			{
				perStructure.Set(i,v[i] / active);
				remainder.Set(i,v[i] % active);
			}
			bool first = true;
			foreach (workingStructures,_s)
			{
				PWorkingStructure s = Reference(*_s);
				if (s)
				{
					s->overproduction = perStructure;
					if (first)
						s->overproduction += remainder;

					first = false;
				}
			}
		}

		void			Compound::TryWithdrawOverproduction(Resource::Vector&positiveVector)
		{
			foreach (workingStructures,_s)
			{
				PWorkingStructure s = Reference(*_s);
				if (s)
				{
					for (index_t r = 0; r < Resource::table.GetSize(); r++)
					{
						if (s->overproduction[r] > 0)
						{
							int transfer = std::min(s->overproduction[r],positiveVector[r]);
							positiveVector.Add(r, -transfer);
							s->overproduction.Add(r, -transfer);
						}
					}
					if (positiveVector.IsZero())
						return;
				}
			}
		}

		void			Compound::ForceWithdrawOverproduction(Resource::ID res ,int positiveAmount)
		{
			if (positiveAmount <= 0)
				return;
			foreach (workingStructures,_s)
			{
				PWorkingStructure s = Reference(*_s);
				if (s)
				{
					if (s->overproduction[res] > 0)
					{
						int transfer = std::min(s->overproduction[res],positiveAmount);
						positiveAmount -= transfer;
						s->overproduction.Add(res, -transfer);
						if (!positiveAmount)
							return;
					}
				}
			}
			FATAL__("Should have reached positiveAmount=0, but have "+String(positiveAmount)+" left");
		}


		void				Compound::FeedLeftovers( Resource::Vector&vector, float&atTime )
		{
			if (vector.IsNonZero())
			{
				Resource::Vector supply,demand;
				vector.SeparateBySign(supply,demand);
				if (supply.IsNonZero())
				{
					bool done = false;
					for (index_t i = 0; i <= WorkingStructure::FinalLeftoverIteration && !done; i++)
						foreach (workingStructures,s0)
						{	
							PWorkingStructure s= s0->lock();
							if (!s || s->IsHibernating())
								continue;
							if (s->IsEnabled() || i == WorkingStructure::FinalLeftoverIteration/*&& !s->AwaitsUserInteraction()*/)
							{
								s->ProcessLeftovers(supply,i,atTime);
								if (supply.IsZero())
								{
									done = true;
									break;
								}
							}
						}
				}
				if (demand.IsNonZero())
				{
					bool done = false;
					for (index_t i = WorkingStructure::FinalLeftoverIteration-1; i <= WorkingStructure::FinalLeftoverIteration && !done; i--)
						foreach (workingStructures,s0)
						{	
							PWorkingStructure s= s0->lock();
							if (!s || s->IsHibernating())
								continue;
							if (s->IsEnabled())
							{
								s->ProcessLeftovers(demand,i,atTime);
								if (demand.IsZero())
								{
									done = true;
									break;
								}
							}
						}
					if (!done)
						foreach (workingStructures,s0)
						{	
							PWorkingStructure s= s0->lock();
							if (!s || s->IsHibernating())
								continue;
							{
								s->ProcessLeftovers(demand,WorkingStructure::FinalLeftoverIteration,atTime);
								if (demand.IsZero())
								{
									done = true;
									break;
								}
							}
						}
				}
				vector = supply;
				vector += demand;
			}
		}


		void				Compound::TickEnd( float&atTime )
		{
			//#ifdef ONE_COMPOUND_PER_FACTION
				if (stable)
				{
					auto p = GetOverproduction();
					FeedLeftovers(p,atTime);
					total = GetVector();
				}
			//#endif

		}

		void				Compound::TickBegin()
		{
			if (stable)
			{
				#ifdef _DEBUG
					typedef std::pair<PStructure, Resource::Vector>	EventResult;
					static Ctr::Buffer0<EventResult>	events;
					events.Clear();
				#endif
				Resource::Vector vector;
				#ifdef _DEBUG
					events << std::make_pair(PStructure(), vector);
				#endif
				for (index_t i = 0; i < workingStructures.Count(); i++)
				{	
					//must be pointer-copy, must access via index, because Tick() might create new structures
					PWorkingStructure s= workingStructures[i].lock();
					if (!s || s->IsHibernating())
						continue;
					if (s->IsEnabled() && !s->AwaitsUserInteraction())
					{
						Resource::Vector local;
						s->Tick(local);
						vector += local;

						s->owner->statistics.SignalStructureResourceDelta(s,local);

						#ifdef _DEBUG
							events << std::make_pair(s, vector);
						#endif
					}
				}
				#ifdef _DEBUG
					events << std::make_pair(PStructure(), vector);
				#endif

				
				Resource::Vector localDemand,overproduction;
				vector.SeparateBySign(overproduction,localDemand);
				float t0 = 0;
				FeedLeftovers(localDemand,t0);
				DistributeOverproduction(overproduction);
				vector = localDemand;
				
				if (!vector.IsStable())
				{
					LogEvent(EventClass::Preset::error, nullptr, "Compound resource vector unstable: {"+vector.ToString()+"}");
					#ifdef _DEBUG
						foreach(events, ev)
						{
							if (ev->first)
								LogEvent(EventClass::Preset::error, ev->first.get(), "Tick() called. Vector is now {" + ev->second.ToString()+"}");
							else
								LogEvent(EventClass::Preset::error, nullptr, "Event processed. Vector is now {" + ev->second.ToString() + "}");
						}
					#endif
					FATAL__("Compound resource vector unstable. Cannot recover. Terminating");
				}
			}


		}
		void			Compound::ChangeStorage( Resource::Vector&change, float&atTime )
		{
			ASSERT__(total.balance.IsStable());

			Resource::Vector original = change;
			FeedLeftovers(change, atTime);
			Resource::Vector added = original - change;

			total.storage += added;
			total.balance += added;
			ASSERT2__(total.storage <= total.storageMax, total.storage.ToString(), total.storageMax.ToString());
			ASSERT1__(total.storage.IsStable(), total.storage.ToString());
			if (!total.balance.IsStable())
			{
				LogEvent(EventClass::Preset::internalError, "Compound balance is not stable: balance="+total.balance.ToString()+", storage="+total.storage.ToString()+", change="+added.ToString());
				RefreshState(atTime);
			}
		}


		//void			Compound::Collect(Resource::Vector&change, float atTime)
		//{
		//	change.Negate();
		//	Distribute(change,atTime);
		//	change.Negate();
		//}

		Resource::FullVector Compound::GetVector(const Ctr::Array<bool>&virtualEnabled) const
		{
			ASSERT_EQUAL__(virtualEnabled.Count(),workingStructures.Count());

			Resource::FullVector result;
			for (index_t i = 0; i < workingStructures.Count(); i++)
			{
				PWorkingStructure s= workingStructures[i].lock();
				DBG_ASSERT__(s);
				if (!s || s->IsHibernating())
					continue;
				bool enabled = virtualEnabled[i] && s->CanEnable();
				//if (!s->IsEnabled())
					//continue;
				if (s->AwaitsUserInteraction())
					enabled = false;
				Resource::Vector local,storage,storageMax;
				s->GetResourceVector(local,storage,storageMax);
				if (!enabled)
					local.Clear();
				result.Include(local,storage,storageMax);
			}
			return result;
		}

	#endif

	/*static*/ void					Compound::TempSetEnabled(WorkingStructure*ws,bool isEnabled)
	{
		#ifdef CLIENT
			ws->ApplyEnableChangeFromServer(isEnabled,true);
		#else
			ws->SetEnabledOverride(isEnabled,false);
		#endif
	}

	/**
	Checks whether or not the specified working structure can be enabled.
	Extensively checks whether the auto balancing strategy can fix any inbalances caused by enabling this structure.
	Temporarily changes the structure's state to enabled without signalling the change or registering change.
	This method must be kept in sync with RefreshState() auto-balancing strategy!
	*/
	bool				Compound::CanEnable( WorkingStructure*toEnable, String*outWhy/*=nullptr*/, Resource::FlagSet*outMissedResources/*=nullptr*/, bool ignoreCanEnable/*=false*/) const
	{
		DBG_ASSERT_NOT_NULL__(toEnable);
		if (toEnable->IsHibernating() || (!ignoreCanEnable && !toEnable->CanEnable()))
		{
			if (outWhy)
			{
				if (toEnable->IsHibernating())
					(*outWhy) = "currently hibernating";
				else
					(*outWhy) = "generally cannot enable";
			}
			return false;
		}

		int balance = -1000;
		count_t depth = 0;
		PWorkingStructure bestEnable[2];
		TempSetEnabled(toEnable,true);
			Resource::FullVector total = GetVector();
			if (total.balance.IsStable())
			{
				TempSetEnabled(toEnable,false);
				return true;
			}
			if (FindBestEnableCandidates(bestEnable,balance, depth, total.balance, ARRAYSIZE(bestEnable)-1,0,nullptr))
			{
				TempSetEnabled(toEnable,false);
				return true;
			}
		TempSetEnabled(toEnable,false);

		Resource::FlagSet lacking;
		count_t currentlyLacking = 0;
		for (index_t r = 0; r < Resource::table.Count(); r++)
			if (total.balance[r] < 0)
			{
				currentlyLacking ++;
				lacking |= r;
			}

		if (!outWhy && !outMissedResources)
			return false;

		if (outWhy)
			(*outWhy) = "lacking";
		count_t cnt = 0;
		for (index_t r = 0; r < Resource::table.Count(); r++)
			if (lacking & r)
			{
				if (outWhy)
				{
					if (cnt!=0)
						(*outWhy) += ',';
					*outWhy += ' '+Resource::ID(r).ToType().name;
				}
				cnt++;
			}
		if (outMissedResources)
			*outMissedResources = lacking;
		return false;
	}

	Resource::FullVector Compound::GetVector() const
	{
		Resource::FullVector result;

		foreach (workingStructures,s0)
		{
			PWorkingStructure s= s0->lock();
			DBG_ASSERT__(s);
			if (!s || s->IsHibernating())
				continue;
			bool enabled = s->IsEnabled();
			//if (!s->IsEnabled())
				//continue;
			#ifndef CLIENT
				if (enabled && !s->CanEnable())
				{
					float t = 0;
					s->SetEnabled(false,t,false);
					enabled = false;
				}
			#endif
			if (s->AwaitsUserInteraction())
				enabled = false;
			Resource::Vector local,storage,storageMax;
			s->GetResourceVector(local,storage,storageMax);
			if (!enabled)
				local.Clear();
			result.Include(local,storage,storageMax);
		}
		return result;
	}

	Resource::FullVector Compound::GetAllEnabledVector() const
	{
		Resource::FullVector result;

		foreach (workingStructures,s0)
		{
			PWorkingStructure s= s0->lock();
			DBG_ASSERT__(s);
			if (!s || s->IsHibernating())
				continue;
			Resource::Vector local,storage,storageMax;
			s->GetResourceVector(local,storage,storageMax);
			result.Include(local,storage,storageMax);
		}
		return result;
	}

	bool				Compound::FindBestEnableCandidates(PWorkingStructure*bestOut, count_t bestOutCount) const
	{
		Resource::Vector balance = total.balance;
		int bestBalance = -1000;
		count_t bestDepth = 0;
		return FindBestEnableCandidates(bestOut, bestBalance, bestDepth, balance,  bestOutCount-1, 0,nullptr);
	}

	/*static*/ int Compound::GetEnableChangeBalance(const Resource::Vector&local)
	{
		return local.SumNegative() + local.SumPositive()/10;
	}

	bool				Compound::FindBestEnableCandidates(PWorkingStructure*bestOut, int&bestBalance, count_t&bestDepth, Resource::Vector&balance,  index_t recursionDepth, int parentSum, const WWorkingStructure*parent) const
	{
		bool rs = false;
		Resource::Vector dummy;
		const WWorkingStructure*_s = parent;
		if (!_s)
			_s = workingStructures.begin();
		else
			++_s;
		for (;_s != workingStructures.end(); ++_s)
		{
			PWorkingStructure s= _s->lock();
			DBG_ASSERT__(s);
			if (!s || s->IsHibernating())
				continue;
			if (s->IsEnabled() || !s->CanEnable())
				continue;
			#ifndef CLIENT
				if (lastDisabled == s.get())
					continue;
			#endif

			Resource::Vector local,storage;
			s->GetResourceVector(local,storage,dummy);
			local -= storage;
			
			balance += local;
			int summed = GetEnableChangeBalance(local)+parentSum;
			if (balance.IsStable())
			{
				if (summed > bestBalance || (summed == bestBalance && bestDepth < recursionDepth))
				{
					rs = true;
					bestBalance = summed;
					bestDepth = recursionDepth;
					*bestOut = s;
					for (index_t r = 1; r <= recursionDepth; r++)
						bestOut[r].reset();
				}
			}
			if (recursionDepth)
			{
				if (FindBestEnableCandidates(bestOut+1, bestBalance, bestDepth, balance,  recursionDepth-1, summed, _s))
				{
					*bestOut = s;
					rs = true;
				}
			}
			balance -= local;
		}
		return rs;
	}

	void				Compound::RefreshState(float atTime)
	{
		total = GetVector();

		stable = total.balance.IsStable();

		#ifndef CLIENT

			if (!stable)
			{
				//This section must be kept in sync with Compound::CanEnable(...) oracle function, or bad shit will happen!
				PWorkingStructure bestEnable[2];
				if (FindBestEnableCandidates(bestEnable, ARRAYSIZE(bestEnable)))
				{
					for (index_t i = 0; i < ARRAYSIZE(bestEnable); i++)
					{
						if (bestEnable[i])
						{
							LogEvent(forceEnable,bestEnable[i].get(), "Auto-balance: "+bestEnable[i]->GetTypeName()+" enabled");
							bestEnable[i]->SetEnabled(true,atTime,false);
						}
					}
					total = GetVector();

					stable = total.balance.IsStable();
					ASSERT__(stable);
				}
			}

			//if (!stable)
			//{
			//	foreach (containedStructures,s0)
			//	{
			//		PWorkingStructure s= s0->lock();
			//		DBG_ASSERT__(s);
			//		if (!s)
			//			continue;
			//		if (!s->IsEnabled() || !s->CanEnable())
			//			continue;
			//	
			//		s->SetEnabled(false,atTime,false);

			//		Resource::FullVector v2 = GetVector();
			//		if (v2.balance.IsStable())
			//		{
			//			LogEvent(EventType::NetworkProcess,s.get(), "Auto-balance: Structure disabled");
			//			total = v2;
			//			stable = true;
			//			break;
			//		}
			//		s->SetEnabled(true,atTime,false);
			//	}
			
			//}
			while (!stable)
			{
				PWorkingStructure	toDisable;
				int damageLevel = std::numeric_limits<int>::max();

				foreach (workingStructures,s0)
				{
					PWorkingStructure s= s0->lock();
					DBG_ASSERT__(s);
					if (!s || s->IsHibernating())
						continue;
					if (!s->IsEnabled())
						continue;

					Objects::Resource::Vector vec, storageCurrent,storageMax;
					s->GetResourceVector(vec,storageCurrent,storageMax);

					bool isAtFault = false;
					for (index_t i = 0; i < Objects::Resource::table.GetSize(); i++)
					{
						if (total.balance[i] < 0 && vec[i] < 0)
						{
							isAtFault = true;
							break;
						}
					}
					if (!isAtFault)
						continue;

					
					s->SetEnabled(false,atTime,false);
					Resource::FullVector v2 = GetVector();
							

					int dl = -v2.balance.SumNegative();
					if (s->IsNexusPort())
						dl += 20;
					if (dl < damageLevel)
					{
						dl = damageLevel;
						toDisable = s;
					}
					s->SetEnabled(true,atTime,false);
				}
				if (!toDisable)	//due to upkeep this can happen
					break;

				{
					LogEvent(forceDisable, toDisable.get(),"Auto-balance: "+toDisable->GetTypeName()+" disabled ");
					toDisable->SetEnabled(false,atTime,false);
					total = GetVector();
					stable = total.balance.IsStable();
				}
			}

			foreach (workingStructures,s0)
			{
				PWorkingStructure s= s0->lock();
				if (!s || s->IsHibernating())
					continue;
				if ((/*!stable || */s->AwaitsUserInteraction()) && s->IsEnabled())
				{
					s->unstable = true;
				}
				else
					s->unstable = false;
			}
		#endif

	}


	void RefreshCompoundStates( float atTime)
	{
		ForeachCompound([atTime](Compound&c)
		{
			c.RefreshState(atTime);
		});
	}

	void ForeachCompound(const std::function<void(Compound&)>&func)
	{
		#ifdef ONE_COMPOUND_PER_FACTION
			#ifdef CLIENT
				for (const auto&f: Faction::allKnown)
					func(f->compound);
			#else
				for (const auto&f: Faction::all)
				{
					func(f->compound);
				}
				func(Faction::nexus->compound);
			#endif
		#else
			for (auto&c : compounds)
				func(c);
		#endif


	}






}

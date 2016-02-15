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
		if (!s || s->health <= 0)
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

		ResourceVector		Compound::GetOverproduction() const
		{
			ResourceVector result;
			foreach (workingStructures,_s)
			{
				PWorkingStructure s = Reference(*_s);
				if (s)
					result += s->overproduction;
			}
			return result;
		}

		int					Compound::GetOverproduction(Resource res) const
		{
			int result = 0;
			foreach (workingStructures,_s)
			{
				PWorkingStructure s = Reference(*_s);
				if (s)
					result += s->overproduction.level[res];
			}
			return result;
		}


		void				Compound::DistributeOverproduction(const ResourceVector&v)
		{
			if (v.IsEmpty())
				return;
			int active = (int)CountWorkingStructures();
			if (!active)
				return;
			ResourceVector perStructure,remainder;
			for (index_t i = 0; i < Resource::NumberOfPossibilities; i++)
			{
				perStructure.level[i] = v.level[i] / active;
				remainder.level[i] = v.level[i] % active;
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

		void			Compound::TryWithdrawOverproduction(ResourceVector&positiveVector)
		{
			foreach (workingStructures,_s)
			{
				PWorkingStructure s = Reference(*_s);
				if (s)
				{
					for (index_t r = 0; r < Resource::NumberOfPossibilities; r++)
					{
						if (s->overproduction.level[r] > 0)
						{
							int transfer = std::min(s->overproduction.level[r],positiveVector.level[r]);
							positiveVector.level[r] -= transfer;
							s->overproduction.level[r] -= transfer;
						}
					}
					if (positiveVector.IsEmpty())
						return;
				}
			}
		}

		void			Compound::ForceWithdrawOverproduction(Resource res ,int positiveAmount)
		{
			if (positiveAmount <= 0)
				return;
			foreach (workingStructures,_s)
			{
				PWorkingStructure s = Reference(*_s);
				if (s)
				{
					if (s->overproduction.level[res] > 0)
					{
						int transfer = std::min(s->overproduction.level[res],positiveAmount);
						positiveAmount -= transfer;
						s->overproduction.level[res] -= transfer;
						if (!positiveAmount)
							return;
					}
				}
			}
			FATAL__("Should have reached positiveAmount=0, but have "+String(positiveAmount)+" left");
		}


		void				Compound::FeedLeftovers(ResourceVector&vector, float atTime)
		{
			if (vector.IsNotEmpty())
			{
				ResourceVector supply,demand;
				vector.SeparateBySign(supply,demand);
				if (supply.IsNotEmpty())
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
								if (supply.IsEmpty())
								{
									done = true;
									break;
								}
							}
						}
				}
				if (demand.IsNotEmpty())
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
								if (demand.IsEmpty())
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
								if (demand.IsEmpty())
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


		void				Compound::TickEnd(float atTime)
		{
			//#ifdef ONE_COMPOUND_PER_FACTION
				if (stable)
				{
					FeedLeftovers(GetOverproduction(),atTime);
					total = GetVector();
				}
			//#endif

		}

		void				Compound::TickBegin()
		{
			if (stable)
			{
				#ifdef _DEBUG
					typedef std::pair<PStructure, ResourceVector>	EventResult;
					static Buffer0<EventResult>	events;
					events.Clear();
				#endif

				ResourceVector vector;
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
						s->Tick(vector);
						#ifdef _DEBUG
							events << std::make_pair(s, vector);
						#endif
					}
				}
				#ifdef _DEBUG
					events << std::make_pair(PStructure(), vector);
				#endif

				
				ResourceVector localDemand,overproduction;
				vector.SeparateBySign(overproduction,localDemand);
				FeedLeftovers(localDemand,0);
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
		void			Compound::ChangeStorage(ResourceVector&change, float atTime)
		{
			ASSERT__(total.balance.IsStable());

			ResourceVector original = change;
			FeedLeftovers(change, atTime);
			ResourceVector added = original - change;

			total.storage += added;
			total.balance += added;
			ASSERT2__(total.storage <= total.storageMax, total.storage.ToString(), total.storageMax.ToString());
			ASSERT__(total.storage.IsStable());
			ASSERT__(total.balance.IsStable());
		}


		//void			Compound::Collect(ResourceVector&change, float atTime)
		//{
		//	change.Negate();
		//	Distribute(change,atTime);
		//	change.Negate();
		//}

		FullResourceVector Compound::GetVector(const Array<bool>&virtualEnabled) const
		{
			ASSERT_EQUAL__(virtualEnabled.Count(),workingStructures.Count());

			FullResourceVector result;
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
				ResourceVector local,storage,storageMax;
				s->GetResourceVector(local,storage,storageMax);
				if (!enabled)
					local.Clear();
				result.Include(local,storage,storageMax);
			}
			return result;
		}

		/**
		Checks whether or not the specified working structure can be enabled.
		Extensively checks whether the auto balancing strategy can fix any inbalances caused by enabling this structure.
		Temporarily changes the structure's state to enabled without signalling the change or registering change.
		This method must be kept in sync with RefreshState() auto-balancing strategy!
		*/
		bool				Compound::CanEnable( WorkingStructure*toEnable, String&outWhy) const
		{
			DBG_ASSERT_NOT_NULL__(toEnable);
			if (toEnable->IsHibernating() || !toEnable->CanEnable())
			{
				if (toEnable->IsHibernating())
					outWhy = "currently hibernating";
				else
					outWhy = "generally cannot enable";
				return false;
			}

			int balance = -1000;
			PWorkingStructure bestEnable[2];
			toEnable->SetEnabledOverride(true,false);
				FullResourceVector total = GetVector();
				if (total.balance.IsStable())
				{
					toEnable->SetEnabledOverride(false,false);
					return true;
				}
				if (FindBestEnableCandidates(bestEnable,balance, total.balance, ARRAYSIZE(bestEnable)-1,0,nullptr))
				{
					toEnable->SetEnabledOverride(false,false);
					return true;
				}
			toEnable->SetEnabledOverride(false,false);

			bool lacking[Resource::NumberOfPossibilities]	= {false};
			count_t currentlyLacking = 0;
			for (index_t r = 0; r < Resource::NumberOfPossibilities; r++)
				if (total.balance.level[r] < 0)
				{
					currentlyLacking ++;
					lacking[r] = true;
				}

			outWhy = "lacking";
			for (index_t r = 0; r < Resource::NumberOfPossibilities; r++)
				if (lacking[r])
					outWhy += " "+String(Resource::Reinterpret(r).ToString());


			return false;
		}


	#endif

	FullResourceVector Compound::GetVector() const
	{
		FullResourceVector result;

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
					float t = 0.f;
					s->SetEnabled(false,t,false);
					enabled = false;
				}
			#endif
			if (s->AwaitsUserInteraction())
				enabled = false;
			ResourceVector local,storage,storageMax;
			s->GetResourceVector(local,storage,storageMax);
			if (!enabled)
				local.Clear();
			result.Include(local,storage,storageMax);
		}
		return result;
	}

	#ifndef CLIENT
		bool				Compound::FindBestEnableCandidates(PWorkingStructure*bestOut, count_t bestOutCount) const
		{
			ResourceVector balance = total.balance;
			int bestBalance = -1000;
			return FindBestEnableCandidates(bestOut, bestBalance, balance,  bestOutCount-1, 0,nullptr);
		}

		/*static*/ int Compound::GetEnableChangeBalance(const ResourceVector&local)
		{
			return local.SumNegative() + local.SumPositive()/10;
		}

		bool				Compound::FindBestEnableCandidates(PWorkingStructure*bestOut, int&bestBalance, ResourceVector&balance,  index_t recursionDepth, int parentSum, const WWorkingStructure*parent) const
		{
			bool rs = false;
			ResourceVector dummy;
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
				if (lastDisabled == s.get())
					continue;

				ResourceVector local;
				s->GetResourceVector(local,dummy,dummy);
				balance += local;
				int summed = GetEnableChangeBalance(local)+parentSum;
				if (balance.IsStable())
				{
					if (summed > bestBalance)
					{
						rs = true;
						bestBalance = summed;
						*bestOut = s;
						for (index_t r = 1; r <= recursionDepth; r++)
							bestOut[r].reset();
					}
				}
				if (recursionDepth)
				{
					if (FindBestEnableCandidates(bestOut+1, bestBalance, balance,  recursionDepth-1, summed, _s))
					{
						*bestOut = s;
						rs = true;
					}
				}
				balance -= local;
			}
			return rs;
		}
	#endif

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
							LogEvent(forceEnable,bestEnable[i].get(), "Auto-balance: Structure enabled");
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

			//		FullResourceVector v2 = GetVector();
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

					Objects::ResourceVector vec, storageCurrent,storageMax;
					s->GetResourceVector(vec,storageCurrent,storageMax);

					bool isAtFault = false;
					for (index_t i = 0; i < Objects::ResourceVector::NumEntries; i++)
					{
						if (total.balance.level[i] < 0 && vec.level[i] < 0)
						{
							isAtFault = true;
							break;
						}
					}
					if (!isAtFault)
						continue;

					
					s->SetEnabled(false,atTime,false);
					FullResourceVector v2 = GetVector();
							

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
					LogEvent(forceDisable, toDisable.get(),"Auto-balance: Structure disabled ");
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
					PFaction faction = f.lock();
					DBG_ASSERT__(faction);
					if (faction)
						func(faction->compound);
				}
				func(Faction::nexus->compound);
			#endif
		#else
			for (auto&c : compounds)
				func(c);
		#endif


	}






}

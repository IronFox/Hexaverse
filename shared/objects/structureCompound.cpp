#include <global_root.h>
#include "structureCompound.h"
#include "structure.h"
#include "faction.h"

namespace Objects
{
	#ifndef CONNECTOR_LESS
		Buffer0<Compound,Swap>		compounds;
	#endif



	void				Compound::AppendStructure(Structure*s)
	{
		WorkingStructure*s2 = dynamic_cast<WorkingStructure*>(s);
		if (s2)
			containedStructures << To<WorkingStructure>::Cast( s2->shared_from_this());
	}



	static EventClass	forceEnable(EventSignificance::Uncommon,EventFlavor::Gain),
						forceDisable(EventSignificance::Uncommon,EventFlavor::Loss);


	#ifndef CLIENT

		WorkingStructure*	Compound::lastDisabled = nullptr;


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
						foreach (containedStructures,s0)
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
						foreach (containedStructures,s0)
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
						foreach (containedStructures,s0)
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
			#ifdef CONNECTOR_LESS
				if (stable)
				{
					FeedLeftovers(overproduction,atTime);
					overproduction.Clear();
					total = GetVector();
				}
			#endif

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
				foreach (containedStructures,s0)
				{	
					PWorkingStructure s= s0->lock();
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

				#ifdef CONNECTOR_LESS
					ResourceVector localDemand;
					vector.SeparateBySign(overproduction,localDemand);
					FeedLeftovers(localDemand,0);
					vector = localDemand;
				#else
					FeedLeftovers(vector,0);
				#endif

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
		void			Compound::Distribute(ResourceVector supply, float atTime)
		{
			ASSERT__(total.balance.IsStable());
			total.storage += supply;
			ASSERT2__(total.storage <= total.storageMax, total.storage.ToString(), total.storageMax.ToString());
			total.balance += supply;
			ASSERT__(total.storage.IsStable());
			ASSERT__(total.balance.IsStable());
			FeedLeftovers(supply, atTime);

		}


		void			Compound::Collect(ResourceVector demand, float atTime)
		{
			ASSERT__(total.balance.IsStable());
			demand.Negate();
			total.storage += demand;
			total.balance += demand;
			ASSERT__(total.storage.IsStable());
			//ASSERT__(total.balance.IsStable());	//not guaranteed
			FeedLeftovers(demand, atTime);
			if (!total.balance.IsStable())
			{
				//RefreshCompoundCoverageIfNeeded(atTime);
				this->RefreshState(atTime);

			}


		}

		FullResourceVector Compound::GetVector(const Array<bool>&virtualEnabled) const
		{
			FullResourceVector result;
			for (index_t i = 0; i < containedStructures.Count(); i++)
			{
				PWorkingStructure s= containedStructures[i].lock();
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


		bool				Compound::CanEnable( WorkingStructure*toEnable, String&outWhy) const
		{
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

		foreach (containedStructures,s0)
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
					s->SetEnabled(false,0,false);
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
				_s = containedStructures.begin();
			else
				++_s;
			for (;_s != containedStructures.end(); ++_s)
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

				foreach (containedStructures,s0)
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

				ASSERT__(toDisable);
				{
					LogEvent(forceDisable, toDisable.get(),"Auto-balance: Structure disabled ");
					toDisable->SetEnabled(false,atTime,false);
					total = GetVector();
					stable = total.balance.IsStable();
				}
			}

			foreach (containedStructures,s0)
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
		#ifdef CONNECTOR_LESS
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

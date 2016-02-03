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
		foreach (containedStructures,_s)
		{
			if (Reference(*_s))
				rs++;
		}
		return rs;
	}


	#ifndef CLIENT


		ResourceVector		Compound::GetOverproduction() const
		{
			ResourceVector result;
			foreach (containedStructures,_s)
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
			foreach (containedStructures,_s)
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
			foreach (containedStructures,_s)
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
			foreach (containedStructures,_s)
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
			foreach (containedStructures,_s)
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
			//#ifdef CONNECTOR_LESS
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

				
				ResourceVector localDemand,overproduction;
				vector.SeparateBySign(overproduction,localDemand);
				FeedLeftovers(localDemand,0);
				DistributeOverproduction(overproduction);
				vector = localDemand;
				
				if (!vector.IsStable())
				{
					LogEvent(EventType::InternalFault, nullptr, "Compound resource vector unstable: {"+vector.ToString()+"}");
					#ifdef _DEBUG
						foreach(events, ev)
						{
							if (ev->first)
								LogEvent(EventType::InternalFault, ev->first.get(), "Tick() called. Vector is now {" + ev->second.ToString()+"}");
							else
								LogEvent(EventType::InternalFault, nullptr, "Event processed. Vector is now {" + ev->second.ToString() + "}");
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


		bool				Compound::CanEnable( const WorkingStructure*toEnable, String&outWhy) const
		{
			if (toEnable->IsHibernating() || !toEnable->CanEnable())
			{
				if (toEnable->IsHibernating())
					outWhy = "currently hibernating";
				else
					outWhy = "generally cannot enable";
				return false;
			}
			Array<bool>	virtualEnabled(containedStructures.Count());
			for (index_t i = 0; i < containedStructures.Count(); i++)
			{
				PWorkingStructure s = containedStructures[i].lock();
				if (!s || s->IsHibernating())
					continue;
				virtualEnabled[i] = s && (s->IsEnabled() || s.get() == toEnable);
			}

			FullResourceVector total = GetVector(virtualEnabled);

			bool stable = total.balance.IsStable();

			if (stable)
				return true;


			bool lacking[Resource::NumberOfPossibilities]	= {false};
			count_t currentlyLacking = 0;
			for (index_t r = 0; r < Resource::NumberOfPossibilities; r++)
				if (total.balance.level[r] < 0)
				{
					currentlyLacking ++;
					lacking[r] = true;
				}
				
			for (index_t i = 0; i < containedStructures.Count(); i++)
			{
				PWorkingStructure s= containedStructures[i].lock();
				DBG_ASSERT__(s);
				if (!s || s->IsHibernating())
					continue;
				if (virtualEnabled[i] || !s->CanEnable())
					continue;
				virtualEnabled[i] = true;

				FullResourceVector v2 = GetVector(virtualEnabled);
				if (v2.balance.IsStable())
					return true;

				count_t nowLacking = 0;
				for (index_t r = 0; r < Resource::NumberOfPossibilities; r++)
					if (v2.balance.level[r] < 0)
						nowLacking ++;

				if (nowLacking < currentlyLacking)
				{
					currentlyLacking = nowLacking;
					for (index_t r = 0; r < Resource::NumberOfPossibilities; r++)
						lacking[r] = v2.balance.level[r] < 0;
				}

				virtualEnabled[i] = false;
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

	void				Compound::RefreshState(float atTime)
	{
		total = GetVector();

		stable = total.balance.IsStable();

		#ifndef CLIENT

			if (!stable)
			{
				foreach (containedStructures,s0)
				{
					PWorkingStructure s= s0->lock();
					DBG_ASSERT__(s);
					if (!s || s->IsHibernating())
						continue;
					if (s->IsEnabled() || !s->CanEnable())
						continue;
					
					s->SetEnabled(true,atTime,false);

					FullResourceVector v2 = GetVector();
					if (v2.balance.IsStable())
					{
						LogEvent(EventType::CompoundProcess,s.get(), "Auto-balance: Structure enabled");
						total = v2;
						stable = true;
						break;
					}

					auto s1 = s0;
					++s1;
					for (;s1 != containedStructures.end(); ++s1)
					{
						PWorkingStructure s= s1->lock();
						DBG_ASSERT__(s);
						if (!s || s->IsHibernating())
							continue;
						if (s->IsEnabled() || !s->CanEnable())
							continue;
					
						s->SetEnabled(true,atTime,false);

						FullResourceVector v2 = GetVector();
						if (v2.balance.IsStable())
						{
							LogEvent(EventType::CompoundProcess,s.get(), "Auto-balance: Structure enabled");
							total = v2;
							stable = true;
							break;
						}

						s->SetEnabled(false,atTime,false);
					}
					
					if (stable)
						break;


					s->SetEnabled(false,atTime,false);
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
					LogEvent(EventType::CompoundProcess, toDisable.get(),"Auto-balance: Structure disabled ");
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

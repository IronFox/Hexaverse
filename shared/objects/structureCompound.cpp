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
		if (s->IsWorkingStructure())
		{
			workingStructures << To<WorkingStructure>::Cast( s->shared_from_this());
		}
		allStructures << To<Structure>::Cast( s->shared_from_this() );

	}


	#ifndef CLIENT



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
			if (stable)
			{
				FeedLeftovers(overproduction,atTime);
				overproduction.Clear();
				total = GetVector();
			}
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
				foreach (workingStructures,s0)
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
				foreach (allStructures,s0)
				{	
					PStructure s= s0->lock();
					if (!s || !s->IsMaintained() || s->IsIndestructible())
						continue;
					{
						ResourceVector upkeep;
						s->GetUpkeep(upkeep);
						vector -= upkeep;
						#ifdef _DEBUG
							events << std::make_pair(s, vector);
						#endif
					}
				}

				#ifdef _DEBUG
					events << std::make_pair(PStructure(), vector);
				#endif

				
				ResourceVector localDemand;
				vector.SeparateBySign(overproduction,localDemand);
				FeedLeftovers(localDemand,0);
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
			for (index_t i = 0; i < allStructures.Count(); i++)
			{
				PStructure s= allStructures[i].lock();
				DBG_ASSERT__(s);
				if (!s || !s->IsMaintained() || s->IsIndestructible())
					continue;
				ResourceVector upkeep;
				s->GetUpkeep(upkeep);
				result.IncludeUpkeep(upkeep);
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
			Array<bool>	virtualEnabled(workingStructures.Count());
			for (index_t i = 0; i < workingStructures.Count(); i++)
			{
				PWorkingStructure s = workingStructures[i].lock();
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
				
			for (index_t i = 0; i < workingStructures.Count(); i++)
			{
				PWorkingStructure s= workingStructures[i].lock();
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
		foreach (allStructures,s0)
		{
			PStructure s= s0->lock();
			DBG_ASSERT__(s);
			if (!s || !s->IsMaintained() || s->IsIndestructible())
				continue;
			ResourceVector upkeep;
			s->GetUpkeep(upkeep);
			result.IncludeUpkeep(upkeep);
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
				foreach (workingStructures,s0)
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
					LogEvent(EventType::CompoundProcess, toDisable.get(),"Auto-balance: Structure disabled ");
					toDisable->SetEnabled(false,atTime,false);
					total = GetVector();
					stable = total.balance.IsStable();
				}
			}
			while (!stable)
			{
				PStructure	toDisable;
				int damageLevel = std::numeric_limits<int>::max();
				Buffer0<PStructure>	partOfTheProblem;

				foreach (allStructures,s0)
				{
					PStructure s= s0->lock();
					DBG_ASSERT__(s);
					if (!s || !s->IsMaintained() || s->IsIndestructible())
						continue;

					Objects::ResourceVector upkeep;
					s->GetUpkeep(upkeep);

					bool isAtFault = false;
					for (index_t i = 0; i < Objects::ResourceVector::NumEntries; i++)
					{
						if (total.balance.level[i] < 0 && upkeep.level[i] != 0)
						{
							isAtFault = true;
							break;
						}
					}
					if (!isAtFault)
						continue;
					partOfTheProblem << s;
				}

				ASSERT__(partOfTheProblem.IsNotEmpty());
				static Random random;
				toDisable = partOfTheProblem[random.getIndex(partOfTheProblem.Count()-1)];


				{
					LogEvent(EventType::CompoundProcess, toDisable.get(),"Auto-balance: Maintenance disabled ");
					toDisable->SetMaintenance(false,atTime);
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

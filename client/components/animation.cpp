#include <global_root.h>
#include "animation.h"
#include "network.h"
#include "visual/scene/world.h"
#include "clientRuntimeConfig.h"
#include "ai/vehicles.h"
#include "ai/factories.h"
#include "visual/objectChoices.h"
#include "playerControlBegin.h"
#include "animation/trajectory.h"
#include "animation/mobileState.h"
#include "animation/delayedMobileInsertion.h"
#include "clientEvents.h"

namespace Animation
{


	float						atTime = 0.f;






	/**
	This method is called as the first instruction possible during command mode
	*/
	void UpdateAutomation(const TCodeLocation&loc)
	{
	//	ASSERT_NOT_EQUAL__(Network::roundNumber,Network::dbgRoundNumber);
		//Network::dbgRoundNumber = Network::roundNumber;



		if (RuntimeConfig::aiMode)
		{
			AI::ClearPending();
		}
		Objects::vehicles.Foreach([](const Objects::PVehicle&v)
		{
			if (v->isOwnedByMe)
			{
				if (v->isAutomated && v->turret->detectsResources)
					MobileState::UpdateAutomation(v);
				else
					if (RuntimeConfig::aiMode)
					{
						AI::AddVehicle(v);
					}
			}
		});
		if (RuntimeConfig::aiMode)
		{
			Objects::SubOrbital::planes.Foreach([](const Objects::SubOrbital::PPlane&p)
			{
				if (p->isOwnedByMe)
					AI::AddVehicle(p);
			});
			DBG_ASSERT__(!Network::newRoundRequested);
			AI::ProcessFactories();
			AI::SignalStart("UpdateAutomation",loc);
		}
	}


	void Update();

	void						Create()
	{
		Trajectory::StaticCreate();
		MobileState::StaticCreate();

		GlobalEvents::frameBegin << Update;
	}

	void						Destroy()
	{
		Trajectory::StaticDestroy();
		MobileState::StaticDestroy();
	}













	#ifdef _DEBUG
		float dbgLastRelative;
	#endif


	struct TTimedEvent
	{
		TTime	time;
		PEvent	event;
	};

	
	enum class UpdateResult
	{
		Continue,
		Done
	};

	static UpdateResult			UpdateEvent( const TTimedEvent&ev, float t )
	{
		Event::currentTime = t;
		float x = ev.time.Relativate(t);
		if (x >1.f)
			return UpdateResult::Done;
		ev.event->Update(x);
		return UpdateResult::Continue;
	}
	
	Buffer0<PEvent>				whenDone;
	Buffer0<TTimedEvent>		primitives,actives;

	count_t		CountScheduledEvents(const std::function<bool(const PEvent&, const TTime&t)>&predicate)
	{
		count_t rs = 0;
		foreach (primitives,prim)
			if (predicate(prim->event,prim->time))
				rs++;
		return rs;
	}

	PEvent		FindScheduledEvent(const std::function<bool(const PEvent&, const TTime&t)>&predicate)
	{
		foreach (primitives,prim)
			if (predicate(prim->event,prim->time))
				return prim->event;
		return nullptr;
	}


	bool		AlterScheduledEvent(const std::function<bool(const PEvent&, const TTime&t)>&predicate, const std::function<void(const PEvent&ev, TTime&t)>&updater)
	{
		foreach (primitives,prim)
			if (predicate(prim->event,prim->time))
			{
				updater(prim->event,prim->time);
				return true;
			}
		return false;
	}


	PEvent		FindFirstScheduledEventAtTime(const Protocol::Timestamp&st,const std::function<bool(const PEvent&)>&predicate)
	{
		if (st.rightNow)
			return nullptr;
		foreach (primitives,prim)
			if (prim->time.min == st.atTime && predicate(prim->event))
				return prim->event;
		return nullptr;
	}


	bool AllDone()
	{
		return (primitives.IsEmpty() && actives.IsEmpty());
	}

	void	ScheduleEvent( const TTime&time, const PEvent&ev )
	{
		primitives.Append({time,ev});
		LogEvent(EventSignificance::Debug, "Animation: Appended '"+ev->Describe()+"' to animation sequence at t="+ToString(time));
	}

	void	ScheduleEventWhenDone( const PEvent&ev )
	{
		whenDone.Append(ev);
		LogEvent(EventSignificance::Debug, "Animation: Appended '"+ev->Describe()+"' to post-animation execution sequence");
	}


	void	Flush()
	{
		primitives.Clear();
		actives.Clear();
		whenDone.Clear();
		atTime = 0;
	}

	#ifdef HAS_WINDOW
		void RenderActiveEvents()
		{
			foreach (Animation::actives,a)
				a->event->Render();
		}

	#endif

	
	void Update()
	{
	
		if (!AllDone())
		{
			if ( RuntimeConfig::phase == RuntimeConfig::Phase::AnimationPhase || RuntimeConfig::IsInAnyCommandPhase())
			{
				//allowWorldRecompilation = false;
				#ifndef HAS_WINDOW
					float increase = 0.1f * Network::animationTimeFactor;
				#else
					float increase = Engine::timing.delta * Network::animationTimeFactor;
				#endif
				//LogEvent(EventSignificance::Debug,nullptr,"Animation::Update(@"+String(Animation::atTime)+"+"+String(increase)+")");
				Animation::atTime += increase;


				struct TSortItem
				{
					index_t	elementIndex;
					float	atTime;
			
					bool operator<(const TSortItem&other) const
					{
						return atTime < other.atTime;
					}
				};


				for (index_t i = 0; i < Animation::actives.Count(); i++)	//complete before adding new ones
				{
					const auto&p = Animation::actives[i];
					if (UpdateEvent(p,Animation::atTime) == UpdateResult::Done)
					{
						LogEvent(EventSignificance::Debug, "Ending '"+p.event->Describe()+"' at "+String(atTime)+"/"+ToString(p.time));
						p.event->End();
						Animation::actives.Erase(i);
						i--;
					}
				}

				static Buffer0<TSortItem>	items;
				items.Clear();
				for (index_t i = 0; i < Animation::primitives.Count(); i++)
				{
					const auto&p = Animation::primitives[i];
					if (Animation::atTime >= p.time.min)
					{
						items.Append().elementIndex = i;
						items.Last().atTime = p.time.min;
					}
				}


				std::sort(items.begin(),items.end());

				foreach (items,it)
				{
					Animation::actives << Animation::primitives[it->elementIndex];
					{
						const auto&p = Animation::actives.Last();
						LogEvent(EventSignificance::Debug, "Starting '"+p.event->Describe()+"' at "+String(atTime)+"/"+ToString(p.time));
						p.event->Begin(false);
						if (UpdateEvent(p,Animation::atTime) == UpdateResult::Done)
						{
							LogEvent(EventSignificance::Debug, "Ending '"+p.event->Describe()+"' at "+String(atTime)+"/"+ToString(p.time));
							p.event->End();
							Animation::actives.EraseLast();
						}
					}
					Animation::primitives.Erase(it->elementIndex);
					auto nxt = it;
					nxt++;
					for (;nxt != items.end(); ++nxt)
					{
						if (nxt->elementIndex > it->elementIndex)
							nxt->elementIndex--;
					}
				}



				//for (index_t i = 0; i < Animation::actives.Count(); i++)
				//{
				//	if (Animation::actives[i].Update(Animation::atTime) == Animation::Primitive::Done)
				//	{
				//		Animation::actives[i].End();
				//		Animation::actives.Erase(i);
				//		i--;
				//	}
				//}
			}

			if (Animation::AllDone())
			{
				if (whenDone.IsNotEmpty())
				{
					LogEvent(EventSignificance::Debug,nullptr,"Executing "+String(whenDone.Count())+" post-animation event(s)");
					for (auto&entry : whenDone)
					{
						entry->Begin(true);
						entry->End();
					}

					whenDone.Clear();
				}


				#ifdef HAS_WINDOW
					World::allowRecompilation = true;
					Objects::UpdateChoices(Network::ControlEnabled());
				#endif
				Animation::atTime = 0;

				Objects::ForeachMobileWorldEntity([](const Objects::PMobileEntity&e)
				{
					if (e->owner->SharesSensorsWithMe() && !Animation::insertIntoGridLater.IsSet(e->id))
					{
						float t = 0.f;
						e->MoveSensoryToCurrentLocation(t);
					}
				},Objects::IncludeGhosts::None);


				Animation::insertIntoGridLater.VisitAllKeys([](const TUniqueID&key)
				{
					Objects::PMobileEntity v = To<Objects::MobileEntity>::Cast(Objects::GetEntity(key));
					if (v)
					{
						LogEvent(EventSignificance::Debug,v.get(),"Delayed inserting into grid...");
						ASSERT__(v->storedIn.expired());
						if (v->owner->SharesSensorsWithMe())
						{
							float t = 0.f;
							v->MoveSensoryToCurrentLocation(t);
						}
						Objects::InsertIntoGrid(v,v->location);
					}
				});
				Animation::insertIntoGridLater.Clear();

				Objects::ForeachMobileWorldEntity([](const Objects::PMobileEntity&e)
				{
					e->isInMovementStream = false;	//just in case
				},Objects::IncludeGhosts::None);

				if (RuntimeConfig::IsInAnyCommandPhase() && Network::gameHasBegun)
				{
					LogEvent(EventSignificance::Debug,"Animation done. OnPlayerControlBegin");

					PlayerControlBegin::OnPlayerControlBegin();
				}
			}

	
		}
	}

}

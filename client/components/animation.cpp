#include <global_root.h>
#include "animation.h"
#include "network.h"
#include "visual/scene/world.h"
#include "clientRuntimeConfig.h"
#include "ai.h"
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
			//LogEvent(EventSignificance::Debug,nullptr,"Automation issued by "+String(loc.ToString()));
			AI::vehicles.Clear();
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
						AI::vehicles << v;
					}
			}
		});
		if (RuntimeConfig::aiMode)
		{
			DBG_ASSERT__(!Network::newRoundRequested);
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


	
	enum class UpdateResult
	{
		Continue,
		Done
	};

	static UpdateResult			UpdateEvent( const PEvent&ev, float t )
	{
		Event::currentTime = t;
		float x = ev->time.Relativate(t);
		if (x >1.f)
			return UpdateResult::Done;
		ev->Update(x);
		return UpdateResult::Continue;
	}
	
	Buffer0<PEvent>		primitives,actives,whenDone;


	void	ScheduleEvent( const PEvent&ev )
	{
		primitives.Append(ev);
		LogEvent(EventSignificance::Debug, "Appended '"+ev->Describe()+"' to animation sequence at t="+ToString(ev->time));
	}

	void	ScheduleEventWhenDone( const PEvent&ev )
	{
		whenDone.Append(ev);
		LogEvent(EventSignificance::Debug, "Appended '"+ev->Describe()+"' to post-animation execution sequence");
	}

	
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
					if (UpdateEvent(Animation::actives[i],Animation::atTime) == UpdateResult::Done)
					{
						Animation::actives[i]->End();
						Animation::actives.Erase(i);
						i--;
					}
				}

				static Buffer0<TSortItem>	items;
				items.Clear();
				for (index_t i = 0; i < Animation::primitives.Count(); i++)
				{
					const auto&p = Animation::primitives[i];
					if (Animation::atTime >= p->time.min)
					{
						items.Append().elementIndex = i;
						items.Last().atTime = p->time.min;
					}
				}


				std::sort(items.begin(),items.end());

				foreach (items,it)
				{
					Animation::actives << Animation::primitives[it->elementIndex];
					{
						const auto&p = Animation::actives.Last();
						p->Begin();
						if (UpdateEvent(p,Animation::atTime) == UpdateResult::Done)
						{
							p->End();
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
						entry->time.min = 0.f;
						entry->time.max = 1.f;

						entry->Begin();
						//entry->Update(0.5f);
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

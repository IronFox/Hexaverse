#include "eventLog.h"

#include <general/thread_system.h>
#include <global_string.h>
#include <math/vclasses.h>
#include <container/buffer.h>
#include <objects/entity.h>
#include <objects/faction.h>
#include "../clientEvents.h"

#include "display.h"


namespace VisualEventLog
{
	using namespace DeltaWorks;
	
	struct VisualEvent;
	
	Sys::SpinLock	eventLock;
	


	Buffer0<VisualEvent,Adopt>	events;
	
	struct VisualEvent
	{
		String		message;
		float3		color = float3(1);
		float		age=0.f,scale=1.f;
		static const float MaxAge;

		/**/		VisualEvent():color(1.f),age(0)
		{}

		static float3 ToAverage(const float3&source, float level)
		{
			float3 avg = (float3)1.f;
			//float3((source.r + source.g + source.b)/3.f);
			Vec::interpolate(source,avg,level,avg);
			return avg;
		}

		/**/		VisualEvent(const String&message, EventClass type):message(message)
		{
			color = float3(1.f);
			switch (type.flavor)
			{
				case EventFlavor::Gain:
					color = float3(0,1,0);
				break;
				case EventFlavor::Loss:
					color = float3(1,0.35,0.3);
				break;
				case EventFlavor::SystemError:
				case EventFlavor::UserError:
					color = float3(1,0.3,0.4);
				break;
				case EventFlavor::Neutral:
					if (type.significance == EventSignificance::Important)
						color = float3(0.5,0.5,1);
				break;
				case EventFlavor::ChatMessage:
					scale = 2.f;
				break;
			}


			switch (type.significance)
			{
				case EventSignificance::ClientFatal:
				case EventSignificance::FactionFatal:
				case EventSignificance::SystemFatal:
				case EventSignificance::Important:
				case EventSignificance::NonFatalError:
					age = -MaxAge;
				break;
				case EventSignificance::Uncommon:
					color = ToAverage(color,0.65f);
					color *= 0.85f;
					age = -MaxAge/2.f;
				break;
				default:
					color = ToAverage(color,0.75f);
					color *= 0.7f;
					age = -MaxAge/4.f;
				break;
			}
		}

		void		adoptData(VisualEvent&other)
		{
			message.adoptData(other.message);
			color = other.color;
			age = other.age;
			scale = other.scale;
		}


	};

	/*static */const float VisualEvent::MaxAge = 30.f;



	float		historyHeight=1;
	static const float eventFontScale = 0.9f;
	
	static void		UpdateHistoryHeight()
	{
		float h0 = textout.GetScaledHeight() * eventFontScale;
		historyHeight = 0;
		foreach (events,ev)
			historyHeight += h0 * ev->scale;
	}


	void SignalWindowResize()
	{
		UpdateHistoryHeight();
	}

	void Render()
	{
		eventLock.lock();


			bool anyChanged = false;
			for (index_t i = 0; i < events.Count(); i++)
			{
				events[i].age += Engine::timing.delta;
				if (events[i].age > VisualEvent::MaxAge)
				{
					events.Erase(i);
					i--;
					anyChanged = true;
				}
			}
			if (anyChanged)
				UpdateHistoryHeight();






			count_t rows = events.Count();
					//std::min(events.Count(), (count_t)floor(0.7f / textout.GetScaledHeight()));


			const auto old = textout.state;
				textout.Scale(eventFontScale,eventFontScale);
				textout.MoveTo(0.0f, 280.f/display.GetClientHeight() + historyHeight);
				for (index_t i = events.Count() - rows; i < events.Count(); i++)
					//if (events[i].stackID < 10)
					{
						const auto&ev = events[i];
						//if (ev.age < 30.f)
						{
							textout.Scale(ev.scale);
							textout.SetColor(ev.color, (VisualEvent::MaxAge - ev.age)/VisualEvent::MaxAge);
							textout << ev.message << nl;
							textout.SetScale(old.scale_x,old.scale_y);
						}
					}

			textout.state = old;

		eventLock.unlock();
	}


	void LogEvent( EventClass type,  const Objects::WorldEntity*e, const String&_event)
	{
		String event = _event, outEvent = _event;
		if (e)
		{
			event = e->owner->GetName()+","+ e->GetNiceName()+": "+event;
			outEvent = e->owner->GetLogName()+","+ e->GetName()+": "+event;
		}
		eventLock.lock();

		if (type.significance > EventSignificance::Common
			#ifdef DEBUG
				 || type.significance == EventSignificance::Debug
			#endif
			)
		{
			events << VisualEvent(event,type);
			UpdateHistoryHeight();
		}
		eventLock.unlock();



		
	}

}

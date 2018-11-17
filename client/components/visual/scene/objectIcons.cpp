#include "objectIcons.h"
#include "spriteRenderer.h"
#include <objects/entity.h>
#include <objects/faction.h>
#include <math/vector.h>
#include "../../cameraControl.h"
#include "../../objectNetworkHelper.h"
#include <globalEvents.h>



namespace Objects
{
	namespace Vec = DeltaWorks::Vec;

	class SpriteFamily
	{
	public:
		typedef	SpriteRenderer	Sprites;
		Sprites					renderer;

		struct Prepared
		{
			Sprites::Attributes	attributes;
			Objects::PWorldEntity	entity;
			bool				halfHeight;
		};

		struct ExtPrepared : public Prepared
		{
			std::function<float(float)>		getLocationHeightDelta;
			Timer::Time						referenceTime;
			std::function<float(float)>		getSize;
		};


		Buffer0<Prepared>		prepared;
		Buffer0<ExtPrepared>	extPrepared;
		float3					pulse;
		float					scale;


		void					Setup(const PathString&sourceFileName, float worldScale, const float3&pulse)
		{
			PathString cached = sourceFileName.EndsWithIgnoreCase(".png") ? sourceFileName : sourceFileName+".png";
			scale = worldScale;
			this->pulse = pulse;
			renderer.CreateOrLoadFromIcon("resources/icons/"+sourceFileName,"resources/floaters/"+cached,false,1.f / worldScale);
		}

		void					Render(const Prepared&p, const float size, const float locationHeightDelta)
		{
			Sprites::Instance inst;

			inst.attributes = p.attributes;
			const auto&e = p.entity;
			const bool m = p.halfHeight;
			float h = Grid::GetLayerHeight(e->location.height);
			Vec::mad(e->system.w.xyz,e->system.z.xyz,(m ? h*1.5f : h*(1 + e->GetHeight())) + h * locationHeightDelta,inst.location);
			if (!m)
			{
				M::TVec3<> dir;
				Vec::sub(Camera::scene.GetAbsoluteLocation(),inst.location,dir);
				Vec::setLen(dir,h);
				Vec::add(inst.location,dir);
			}
			inst.size = h * size;
			renderer.Render(inst);
		}

		void					Render()
		{
			foreach (prepared, p)
				Render(*p,scale,0);
			foreach (extPrepared, p)
			{
				auto t = timer.GetSecondsSince(p-> referenceTime);
				Render(*p,scale * p->getSize(t),p->getLocationHeightDelta(t));
			}
			renderer.Flush();
		}


		void					Put(Prepared&inst, const Objects::PWorldEntity&e)
		{
			inst.entity = e;
			inst.halfHeight = e->IsMobile();

			if (e->owner)
				inst.attributes.color = e->owner->color*0.5f + 0.5f;
			inst.attributes.pulse = pulse;
		}

		void					BeginInclusion()	{prepared.Clear();extPrepared.Clear();}
		void					Include(const Objects::PWorldEntity&e)
		{
			auto&inst = prepared.Append();
			Put(inst,e);
		}

		void					IncludeExt(const Objects::PWorldEntity&e, Timer::Time referenceTime, const std::function<float(float)>&sizeFunction, const std::function<float(float)>&locationHeightDeltaFunction, const float3&color)
		{
			auto&inst = extPrepared.Append();
			Put(inst,e);
			inst.referenceTime = referenceTime;
			inst.getSize = sizeFunction;
			inst.getLocationHeightDelta = locationHeightDeltaFunction;
			inst.attributes.color = color;
		}

	};



	namespace Icons
	{

		struct TAnimatedFloater
		{
			SpriteFamily			*family;
			Objects::PWorldEntity	entity;
			Timer::Time				created;
			float					locationHeightDelta;

			void					Insert();
			static float			SizeFunction(float seconds);
			static float			LocationFunction(float seconds);

			void					Begin()
			{
			}
			void					End()
			{
			}

		};

		static SpriteFamily		sleepIcon
								,automationIcon
								,questionIcon
								;
		static FixedArray<SpriteFamily,Objects::Resource::N>
								resourceFamily;

		static Queue<TAnimatedFloater>	animatedFloaters;

		static const float AnimatedTTL = 5.f;

		/*static*/ float TAnimatedFloater::SizeFunction(float seconds)
		{
			if (seconds < AnimatedTTL-1)
				return 1.f;
			float x = seconds - AnimatedTTL + 1;
			//return sin((1.f-M::Sqr(1.f-x)) * M_PI);
			return std::max(0.f, 1.f - (x));
		}

		/*static*/ float TAnimatedFloater::LocationFunction(float seconds)
		{
			float x = seconds / AnimatedTTL;
			return x  * 0.3f;
			//return 0.f;//x * 2.f;
		}

		void TAnimatedFloater::Insert()
		{
			float h = locationHeightDelta;
			family->IncludeExt(entity,created,
				SizeFunction,
				[h](float t){return h + LocationFunction(t);},
				float3(1,0.3,0.3));
		}

		void Create()
		{
			SpriteRenderer::StaticCreate();
			sleepIcon.Setup("Sleep.png",1.f,float3(0.8f,1.f,1.f));
			automationIcon.Setup("Automate.png",0.5f,float3(1));
			questionIcon.Setup("Question-Mark.png",1.f / 1.5f,float3(0.5,1.5f,3));
			for (index_t i = 0; i < Objects::Resource::N; i++)
				resourceFamily[i].Setup(PathString(Objects::Resource::Reinterpret(i).ToString())+".png",0.75f,float3(0.5f,1.f,10));

			GlobalEvents::invalidateObjectCache << Refresh;

		}

		void	Render()
		{
			{
				const auto t = timer.Now();
				bool changed = false;
				while (animatedFloaters.IsNotEmpty() && timer.ToSeconds(t - animatedFloaters.Peek().created) > AnimatedTTL)
				{
					animatedFloaters.Peek().End();
					animatedFloaters.EraseFront();
					changed = true;
				}
				if (changed)
					Refresh();
			}

			display.SetRasterizer(true,false);

			automationIcon.Render();
			sleepIcon.Render();
			questionIcon.Render();

			foreach (resourceFamily,res)
				res->Render();
		}




		void	Refresh()
		{
			automationIcon.BeginInclusion();
			sleepIcon.BeginInclusion();
			questionIcon.BeginInclusion();
			foreach (resourceFamily,rf)
				rf->BeginInclusion();

			ForeachWorldEntity([](const Objects::PWorldEntity&e)
			{
				if (!e->isOwnedByMe)
					return;
				if (e->IsAutomated())
					automationIcon.Include(e);
				else
				if (e->IsSleeping())
				{
					sleepIcon.Include(e);
				}
				else
				if (ObjectHelper::IsIdle(e))
					questionIcon.Include(e);
			});


			foreach (animatedFloaters,f)
			{
				f->Insert();
			}
		}


		void	AddAnimatedFloater( const Objects::PWorldEntity&e, const ArrayRef<BYTE>&resources )
		{
			index_t at = 1;
			foreach (resources,r)
			{
				auto&a = animatedFloaters.Push();
				a.created = timer.Now();
				a.entity = e;
				a.family = resourceFamily+(index_t)*r;
				a.locationHeightDelta = at;
			
				a.Begin();
				a.Insert();
				at++;
			}
		}


	}


}

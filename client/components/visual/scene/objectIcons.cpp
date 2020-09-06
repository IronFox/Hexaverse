#include "objectIcons.h"
#include "spriteRenderer.h"
#include <objects/entity.h>
#include <objects/faction.h>
#include <math/vector.h>
#include "../../cameraControl.h"
#include "../../objectNetworkHelper.h"
#include <globalEvents.h>
#include "../hud/iconNameToFileName.h"


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
			index_t				slotIndex;
			count_t				slotCount;
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
		float					scale,
								forward;


		void					Setup(const PathString&sourceFileName, float worldScale, const float3&pulse, float forward = 0)
		{
			PathString cached = sourceFileName.EndsWithIgnoreCase(".png") ? sourceFileName : sourceFileName+".png";
			scale = worldScale;
			this->pulse = pulse;
			this->forward = forward;
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
			if (forward)
				Vec::mad(inst.location,Camera::scene.direction,forward);
			inst.size = h * size;
			if (p.slotCount > 1)
			{
				 //inst.size /= p.slotCount;
				 M::TVec3<> right;
				 //Vec::cross(e->system.z.xyz,Camera::scene.direction,right);
				 Vec::cross(Camera::scene.upAxis,Camera::scene.direction,right);
				 Vec::normalize0(right);
				 right = Camera::scene.vsystem.x;
				 Vec::mad(inst.location,right,(float(p.slotIndex) - float(p.slotCount-1)/2)*h*size);
			}
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


		void					Put(Prepared&inst, const Objects::PWorldEntity&e, index_t slotIndex, count_t slotCount)
		{
			inst.entity = e;
			inst.halfHeight = e->IsMobile();
			inst.slotIndex = slotIndex;
			inst.slotCount = slotCount;

			if (e->owner)
				inst.attributes.color = e->owner->color*0.5f + 0.5f;
			inst.attributes.pulse = pulse;
		}

		void					BeginInclusion()	{prepared.Clear();extPrepared.Clear();}
		void					Include(const Objects::PWorldEntity&e, index_t slotIndex, count_t slotCount)
		{
			auto&inst = prepared.Append();
			Put(inst,e,slotIndex, slotCount);
		}

		void					IncludeExt(const Objects::PWorldEntity&e, index_t slotIndex, count_t slotCount, Timer::Time referenceTime, const std::function<float(float)>&sizeFunction, const std::function<float(float)>&locationHeightDeltaFunction, const float3&color)
		{
			auto&inst = extPrepared.Append();
			Put(inst,e,slotIndex,slotCount);
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
								,t2LevelUp
								;
		static Array<SpriteFamily>
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
			return x  * 1.f;
			//return 0.f;//x * 2.f;
		}

		void TAnimatedFloater::Insert()
		{
			float h = locationHeightDelta;
			family->IncludeExt(entity,0,1,created,
				SizeFunction,
				[h](float t){return h + LocationFunction(t);},
				float3(1,0.3,0.3));
		}

		void RebuildResourceFamilies()
		{
			animatedFloaters.Clear();
			resourceFamily.SetSize(Objects::Resource::table.GetSize());
			for (index_t i = 0; i < Objects::Resource::table.GetSize(); i++)
			{
				const String&name = Objects::Resource::ID(i).GetIcon();
				resourceFamily[i].Setup(PathString(::Icons::NameToFileName(name))+".png",0.75f,float3(0.5f,1.f,10));
			}
		}

		void Create()
		{
			SpriteRenderer::StaticCreate();
			sleepIcon.Setup("Sleep.png",1.f,float3(0.8f,1.f,1.f));
			automationIcon.Setup("Automate.png",0.5f,float3(1));
			questionIcon.Setup("Question-Mark.png",1.f / 1.5f,float3(0.5,1.5f,3));
			t2LevelUp.Setup("Upgrade.png",0.5,float3(1.1,1.5,1),-0.1);

			RebuildResourceFamilies();

			GlobalEvents::invalidateObjectCache << Refresh;
			GlobalEvents::resourceTableUpdated << RebuildResourceFamilies;
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
			t2LevelUp.Render();

			foreach (resourceFamily,res)
				res->Render();


		}




		void	Refresh()
		{
			automationIcon.BeginInclusion();
			sleepIcon.BeginInclusion();
			questionIcon.BeginInclusion();
			t2LevelUp.BeginInclusion();
			foreach (resourceFamily,rf)
				rf->BeginInclusion();

			ForeachWorldEntity([](const Objects::PWorldEntity&e)
			{
				if (!e->isOwnedByMe)
					return;
				count_t slotCount = 0;
				if (e->t2 && e->t2->CanLevelUp())
					slotCount++;
				if (e->IsAutomated())
					slotCount++;
				else if (e->IsSleeping())
					slotCount++;
				else if (ObjectHelper::IsIdle(e))
					slotCount++;

				index_t slotIndex = 0;
				if (e->t2 && e->t2->CanLevelUp())
					t2LevelUp.Include(e,slotIndex++,slotCount);
				if (!e->isOwnedByMe)
					return;
				if (e->IsAutomated())
					automationIcon.Include(e,slotIndex++,slotCount);
				else if (e->IsSleeping())
					sleepIcon.Include(e,slotIndex++,slotCount);
				else if (ObjectHelper::IsIdle(e))
					questionIcon.Include(e,slotIndex++,slotCount);
			});


			foreach (animatedFloaters,f)
			{
				f->Insert();
			}
		}


		void	AddAnimatedFloater( const Objects::PWorldEntity&e, const Resource::FlagSet&resourceIndexes )
		{
			index_t at = 1;
			for (index_t i = 0; i < Resource::table.GetSize(); i++)
				if (resourceIndexes & i)
				{
					auto&a = animatedFloaters.Push();
					a.created = timer.Now();
					a.entity = e;
					a.family = resourceFamily+i;
					a.locationHeightDelta = at;
			
					a.Begin();
					a.Insert();
					at++;
				}
		}


	}


}

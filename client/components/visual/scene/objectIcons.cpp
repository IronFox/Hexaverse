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
			bool				mobile;
		};

		Buffer0<Prepared>		prepared;
		float3					pulse;
		float					scale;


		void					Setup(const PathString&sourceFileName, float worldScale, const float3&pulse)
		{
			PathString cached = sourceFileName.EndsWithIgnoreCase(".png") ? sourceFileName : sourceFileName+".png";
			scale = worldScale;
			this->pulse = pulse;
			renderer.CreateOrLoadFromIcon("resources/icons/"+sourceFileName,"resources/floaters/"+cached,false,1.f / worldScale);
		}

		void					Render()
		{
			Sprites::Instance inst;
			foreach (prepared, p)
			{
				inst.attributes = p->attributes;
				auto e = p->entity;
				bool m = p->mobile;
				float h = Grid::GetLayerHeight(e->location.height);
				Vec::mad(e->system.w.xyz,e->system.z.xyz,m ? h*1.5f : h*(1 + e->GetHeight()),inst.location);
				if (!m)
				{
					M::TVec3<> dir;
					Vec::sub(Camera::scene.GetAbsoluteLocation(),inst.location,dir);
					Vec::setLen(dir,h);
					Vec::add(inst.location,dir);
//					Vec::mad(inst.location,Camera::scene.GetForwardDirection(),-h);
				}
				inst.size = h * scale;

				renderer.Render(inst);
			}
			renderer.Flush();
		}


		void					BeginInclusion()	{prepared.Clear();}
		void					Include(const Objects::PWorldEntity&e)
		{
			auto&inst = prepared.Append();
			inst.entity = e;
			inst.mobile = e->IsMobile();

			if (e->owner)
				inst.attributes.color = e->owner->color*0.5f + 0.5f;
			inst.attributes.pulse = pulse;
		}

	};

	SpriteFamily				sleepIcon
								,automationIcon
								,questionIcon
								;


	namespace Icons
	{
		void Create()
		{
			SpriteRenderer::StaticCreate();
			sleepIcon.Setup("Sleep.png",1.f,float3(0.8f,1.f,1.f));
			automationIcon.Setup("Automate.png",0.5f,float3(1));
			questionIcon.Setup("Question-Mark.png",1.f / 1.5f,float3(0.5,1.5f,3));

			GlobalEvents::invalidateObjectCache << Refresh;

		}

		void	Render()
		{
			//if (!objectView)
				//return;
			display.SetRasterizer(true,false);

			automationIcon.Render();
			sleepIcon.Render();
			questionIcon.Render();
		}


		void	Refresh()
		{
			automationIcon.BeginInclusion();
			sleepIcon.BeginInclusion();
			questionIcon.BeginInclusion();


			ForeachWorldEntity([](const Objects::PWorldEntity&e)
			{
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
		}

	}


}

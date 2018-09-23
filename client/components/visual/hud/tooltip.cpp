#include "tooltip.h"
#include <objects/description.h>
#include "../../clientEvents.h"
#include "../display.h"
#include "resourceGauges.h"
#include "surfaceRenderer.h"

namespace HUD
{
	namespace Tooltip
	{
		namespace Details
		{
			
			float			hoveringWidth = 0;
			bool			hoveringBrief = true;
			const IProvider*	hoveringOver = nullptr;
			String				shortText,longText;
			Array<StringRef>	lines;



			void			Check()
			{
				static float stationaryCounter = 0;
				static int2 refPos;

				if (!Vec::similar(refPos, Engine::mouse.location.absolute,10))
				{
					stationaryCounter = 0;
					refPos = Engine::mouse.location.absolute;
				}
				else
					stationaryCounter += Engine::timing.delta;

				const bool stationary = stationaryCounter > 0.25f;
				const bool extStationary = stationaryCounter > 1.5f;

				if (stationary)
				{
					const IProvider* next = nullptr;
					foreach (tooltipQuery,q)
					{
						next = (*q)();
						if (next)
							break;
					}

					if (next != hoveringOver)
					{
						hoveringOver = next;
						if (hoveringOver)
						{
							auto cp = next->GetTooltip();
							shortText.adoptData(cp.first);
							longText.adoptData(cp.second);

							Wrap(shortText,60,lines);
						}
						else
						{
							shortText = "";
							longText = "";
							lines.Clear();
						}
						hoveringWidth = 0;
						hoveringBrief = true;
					}
					else
					{
						bool wasBefore = hoveringBrief;
						hoveringBrief = !extStationary;
						if (wasBefore != hoveringBrief)
						{
							Wrap(longText,60,lines);
							hoveringWidth = 0;
						}
					}
				}
				else
				{
					Hide();
				}
			}
		}


		D::Buffer0<FGet,D::Swap>	tooltipQuery;

		void	Create()
		{
			GlobalEvents::frameBegin << Details::Check;
		}

		void	Hide()
		{
			Details::hoveringOver = nullptr;
			Details::hoveringBrief = true;
		}

		void	Render()
		{
			using namespace Details;
			if (!hoveringOver)
				return;
			
			display.Pick(ResourceGauges::textoutCamera);

			textout.PushState();
			float2 base = Engine::mouse.location.windowRelative * 2.f - 1.f;
			textout.Scale(2.5f,2.5f);

			SurfaceRenderer::SetColor(float4(0,0,0,0.9f));
			if (hoveringWidth == 0)
			{
				foreach (lines,t)
					hoveringWidth = std::max(hoveringWidth, textout.QueryScaledWidth(*t));
			}
			
			const count_t lineCount = lines.Count();
			const float fullH = textout.GetScaledHeight()*lineCount;
			float h = textout.GetScaledHeight()*(lineCount-1);
			M::Rect<> r;


			const bool flipX = Engine::mouse.location.windowRelative.x > 1.f - hoveringWidth;
			const bool flipY = Engine::mouse.location.windowRelative.y > fullH;

			if (flipX)
			{
				if (flipY)
				{
					textout.MoveTo(base - float2(hoveringWidth,textout.GetScaledHeight()));
					r = M::Rect<>(base -float2(hoveringWidth,textout.GetScaledHeight() + fullH),base + float2(0,-textout.GetScaledHeight()*0.8f));
				}
				else
				{
					textout.MoveTo(base + float2(-hoveringWidth,fullH));
					r = M::Rect<>(base-float2(hoveringWidth,0) ,base + float2(0,h + textout.GetScaledHeight()*1.2f));
				}
			}
			else
			{
				base.x = std::min(base.x,1.f - hoveringWidth);

				if (flipY)
				{
					textout.MoveTo(base - float2(0,textout.GetScaledHeight()));
					r = M::Rect<>(base -float2(0,textout.GetScaledHeight() + fullH),base + float2(hoveringWidth,-textout.GetScaledHeight()*0.8f));
				}
				else
				{
					textout.MoveTo(base + float2(0,fullH));
					r = M::Rect<>(base ,base + float2(hoveringWidth,h + textout.GetScaledHeight()*1.2f));
				}
			}


			SurfaceRenderer::Rasterize(r);
			SurfaceRenderer::Flush();

			textout.SetColor(1,1,1);
			foreach (lines,l)
				textout << *l << nl;
			textout.PopState();
		}




	}
}

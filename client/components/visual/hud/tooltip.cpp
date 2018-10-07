#include "tooltip.h"
#include <objects/description.h>
#include "../../clientEvents.h"
#include "../display.h"
#include "resourceGauges.h"
#include "surfaceRenderer.h"
#include "guiComponents.h"

namespace HUD
{
	namespace Tooltip
	{
		namespace Details
		{
			
			const IProvider*	hoveringOver = nullptr;	//!< Components that can display tooltips

			

			Array<StringRef>	lines;	//!< Rendering of the current tooltip text

			float				hoveringWidth = 0;
			Info				hoveringInfo=nullptr;
			String				hoveringText;

			float				stationaryCounter = 0, lastCall = 0;


			void			RefreshText()
			{
				auto next = hoveringOver ? hoveringOver->ResolveTooltip(stationaryCounter,lastCall) : nullptr;
				lastCall = stationaryCounter;
				if (next == hoveringInfo || next == nullptr)
					return;
				hoveringInfo = next;
				if (hoveringInfo[0] == 0)
				{
					hoveringInfo = nullptr;
					return;
				}
				hoveringText = hoveringInfo;
				Wrap(hoveringText,60,lines);
				hoveringWidth = 0;
			}

			void			Check()
			{
				static int2 refPos;

				if (!Vec::similar(refPos, Engine::mouse.location.absolute,10))
				{
					Hide();
					refPos = Engine::mouse.location.absolute;
				}
				else
					stationaryCounter += Engine::timing.delta;

				const bool minStationary = stationaryCounter > 0.25f;
				//const bool extStationary = stationaryCounter > 1.5f;

				if (minStationary)
				{
					const IProvider* candidate = nullptr;
					foreach (tooltipQuery,q)
					{
						candidate = (*q)();
						if (candidate)
							break;
					}

					if (!candidate)
					{
						Hide();
						return;
					}
					if (hoveringOver && candidate != hoveringOver)
					{
						Hide();
						return;
					}
					hoveringOver = candidate;
					RefreshText();
				}

			}
		}


		D::Buffer0<FGet,D::Swap>	tooltipQuery;
		HUD::Component::Decoration decoration;

		void	Create()
		{
			decoration= HUD::Component::Decoration();
			decoration.SetCutCorners(false);
			decoration.SetBackgroundColor(float4(0,0,0,0.4f));
			decoration.SetShadow(float4(0,0,0,0.3f));
			GlobalEvents::frameBegin << Details::Check;
		}

		void	Hide()
		{
			Details::hoveringOver = nullptr;
			Details::stationaryCounter = 0;
			Details::lastCall = 0;
			Details::hoveringInfo = nullptr;
		}

		void	Render()
		{
			using namespace Details;
			if (!hoveringInfo)
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


			const bool flipX = Engine::mouse.location.windowRelative.x > 1.f - 0.5f * hoveringWidth;	//right to left
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

			decoration.SignalBackgroundChange();
			decoration.UpdateLocation(r);
			decoration.RenderBackground(float2(),false,false,true,HUD::Component::Decoration::Context(1.f));


	//		SurfaceRenderer::Rasterize(r);
		//	SurfaceRenderer::Flush();

			textout.SetColor(1,1,1);
			foreach (lines,l)
				textout << *l << nl;
			textout.PopState();
		}


	}
}

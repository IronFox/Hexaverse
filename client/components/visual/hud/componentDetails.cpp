#include "componentDetails.h"

#include "../display.h"

namespace HUD{
namespace Component{
namespace Detail
	{


		Engine::OrthographicAspect<>	aspect;
		Resolution						lastClientSpace;


		Buffer0<Viewport>	viewportStack;
		float2	totalOffset;

		void	LoadView(const M::Rect<>&view, const float2&offset)
		{
			if (!view.IsValid())
				FATAL__("HUD: Trying to load invalid view: "+ToString(view));
			LineRenderer::Flush();
			SurfaceRenderer::Flush();
			M::Rect<> view2 = view;
			view2.Translate(offset);
			aspect.UpdateProjection(view2,-1,1);
			aspect.region = view;
			display.Pick(aspect);
		}

		void	PushLoadView(const M::Rect<>&view, const float2&offset/*=float2()*/)
		{

			viewportStack.Append().view = view;
			viewportStack.Last().offset = offset+totalOffset;
			viewportStack.Last().relativeOffset = offset;
			if (viewportStack.Count() > 1)
			{
				viewportStack.Last().view.Translate(-totalOffset);
				viewportStack.Last().view.ConstrainBy(viewportStack.GetFromEnd(1).view);
				//viewportStack.Last().second += viewportStack.GetFromEnd(1).second;
			}
			totalOffset += offset;
			LoadView(viewportStack.Last().view,viewportStack.Last().offset);
		}

		void	PopLoadView()
		{
			totalOffset -= viewportStack.Last().relativeOffset;
			viewportStack.EraseLast();
			if (viewportStack.IsNotEmpty())
				LoadView(viewportStack.Last().view,viewportStack.Last().offset);
			else
			{
				LoadView(M::Rect<>(0,0,1,1),float2());
				totalOffset = float2();
			}
		}



		Buffer<Component*>	allComponents;
	}
}}

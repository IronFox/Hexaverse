#ifdef HAS_WINDOW

#include <global_root.h>
#include "guiElements.h"
#include <engine/display.h>
#include "../../ExtRenderer/DirectX11.h"
#include "../../DX11/texture_font.h"
#include <container/buffer.h>
#include "surfaceRenderer.h"
#include "lineRenderer.h"
#include <engine/aspect.h>
#include <objects.h>

namespace DX11 = Engine::DX11; 

extern Engine::Textout<DX11::TextureFont>	textout;
extern Engine::Display<Engine::DirectX11>	display;


namespace GUI
{
	index_t windowResolutionVersion = 0;

	namespace Detail
	{
		Buffer0<GUI::PWindow,Swap>		visibleWindows;
		Engine::OrthographicAspect<>	aspect;

		struct Viewport
		{
			Rect<>	view;
			float2	offset,
					relativeOffset;


		};
		Buffer0<Viewport>	viewportStack;
		float2	totalOffset;

		void	LoadView(const Rect<>&view, const float2&offset)
		{
			LineRenderer::Flush();
			SurfaceRenderer::Flush();
			Rect<> view2 = view;
			view2.Translate(offset);
			aspect.UpdateProjection(view2,-1,1);
			aspect.region = view;
			display.Pick(aspect);
		}

		void	PushLoadView(const Rect<>&view, const float2&offset=float2())
		{

			viewportStack.Append().view = view;
			viewportStack.last().offset = offset+totalOffset;
			viewportStack.last().relativeOffset = offset;
			if (viewportStack.Count() > 1)
			{
				viewportStack.last().view.Translate(-totalOffset);
				viewportStack.last().view.ConstrainBy(viewportStack.GetFromEnd(1).view);
				//viewportStack.last().second += viewportStack.GetFromEnd(1).second;
			}
			totalOffset += offset;
			LoadView(viewportStack.last().view,viewportStack.last().offset);
		}

		void	PopLoadView()
		{
			totalOffset -= viewportStack.last().relativeOffset;
			viewportStack.EraseLast();
			if (viewportStack.IsNotEmpty())
				LoadView(viewportStack.last().view,viewportStack.last().offset);
			else
			{
				LoadView(Rect<>(0,0,1,1),float2());
				totalOffset = float2();
			}
		}


		void OnInput()
		{
			if (visibleWindows.IsNotEmpty())
				visibleWindows.last()->DetectEnabledState();
		}


	}



	PComponent	mousePressedComponent;

	/*static*/ DX11::BaseInstructionBlock	Image::texturedQuad;

	static UINT ViewPortWidth()
	{
		return std::max(100U,display.clientWidth());
	}
	static UINT ViewPortHeight()
	{
		return std::max(100U,display.clientHeight());
	}

	static float2 ViewPortSizef()
	{
		return float2(ViewPortWidth(),ViewPortHeight());
	}


	PWindow				GetTopMostWindow()
	{
		if (Detail::visibleWindows.IsNotEmpty())
			return Detail::visibleWindows.last();
		return PWindow();
	}


	index_t	FindWindowIndex(const Window*window)
	{
		foreach (Detail::visibleWindows,_w)
			if (_w->get() == window)
			{
				return _w - Detail::visibleWindows.pointer();
			}
		return InvalidIndex;
	}


	bool	Hide(const PWindow&window)
	{
		return Hide(window.get());
	}

	bool	Hide(Window*window)
	{
		VerifyIntegrity(CLOCATION);
		index_t index = FindWindowIndex(window);
		if (index == InvalidIndex)
			return false;

		#ifdef _DEBUG
			if (window->breakOnHide)
				__debugbreak();
		#endif

		window->OnHide();
		if (index+1 == Detail::visibleWindows.Count())
		{
			window->SetActive(false);
			if (Detail::visibleWindows.Count() > 1)
				Detail::visibleWindows.GetFromEnd(1)->SetActive(true);
		}
		Detail::visibleWindows.Erase(index);	//should be last. window is not guaranteed to be valid past this point

		VerifyIntegrity(CLOCATION);

		SignalSituationChange();
		OverrideSignalMouseMotion();
		return true;

	}

	
	bool	IsVisible(const PWindow&window)
	{
		return Detail::visibleWindows.Contains(window);
	}

	void	Show(const PWindow&window)
	{
		VerifyIntegrity(CLOCATION);
		if (Detail::visibleWindows.IsNotEmpty() && Detail::visibleWindows.last() == window)
		{
			DBG_ASSERT__(window->IsActive());
			if (!window->IsActive())
				window->SetActive(true);
			return;
		}
		//if (Detail::visibleWindows.Count() == 1 && Detail::visibleWindows.First() == window)
		//	return;
		Detail::visibleWindows.FindAndErase(window);
		if (Detail::visibleWindows.IsNotEmpty())
			Detail::visibleWindows.last()->SetActive(false);
		Detail::visibleWindows << window;
		window->SetActive(true);
		SignalSituationChange();
		OverrideSignalMouseMotion();
	}



	void	ShowBottom(const PWindow&window)
	{
		VerifyIntegrity(CLOCATION);
		if (Detail::visibleWindows.IsNotEmpty() && Detail::visibleWindows.First() == window)
		{
			if (Detail::visibleWindows.Count() == 1)
			{
				DBG_ASSERT__(window->IsActive());
				if (!window->IsActive())
					window->SetActive(true);
			}
			return;
		}
		//if (Detail::visibleWindows.Count() == 1 && Detail::visibleWindows.First() == window)
		//	return;

		index_t at = Detail::visibleWindows.GetIndexOf(window);
		if (at != InvalidIndex)
		{
			if (at+1 == Detail::visibleWindows.Count())	//ist last
			{
				window->SetActive(false);
				Detail::visibleWindows.GetFromEnd(1)->SetActive(true);
			}
			Detail::visibleWindows.Erase(at);
		} 
		Detail::visibleWindows.Insert(0,window);
		if (Detail::visibleWindows.Count() == 1)
			window->SetActive(true);

		SignalSituationChange();
		OverrideSignalMouseMotion();
	}




	bool	CloseAllModal()
	{
		VerifyIntegrity(CLOCATION);
		if (Detail::visibleWindows.IsNotEmpty())
		{
			bool anyNonClosable = false;
			foreach (Detail::visibleWindows,w)
				if (!(*w)->canBeImplicitlyClosed)
				{
					anyNonClosable = true;
					break;
				}

			if (anyNonClosable)
			{
				bool rs = false;
				for (index_t i = 0; i < Detail::visibleWindows.Count(); i++)
				{
					GUI::PWindow w = Detail::visibleWindows[i];
					if (w->canBeImplicitlyClosed)
					{
						Hide(w);
						i--;
						rs = true;
					}
				}
				GUI::VerifyIntegrity(CLOCATION);
				return rs;
			}
			else
			{
				foreach (Detail::visibleWindows,_d)
				{
					PWindow d = *_d;
					d->OnHide();
				}
				Detail::visibleWindows.last()->SetActive(false);
				Detail::visibleWindows.Clear();
			}
			GUI::VerifyIntegrity(CLOCATION);
			return true;
		}
		VerifyIntegrity(CLOCATION);
		return false;
	}


	bool	IsModal()
	{
		foreach (Detail::visibleWindows,w)
			if ((*w)->isModal)
				return true;
		return false;
	}


	static float2 lastMouseLocation;

	void	OverrideSignalMouseMotion()
	{
		lastMouseLocation = Engine::mouse.location.windowRelative;
		for (index_t i = Detail::visibleWindows.Count()-1; i< Detail::visibleWindows.Count(); i--)
		{
			const auto&wnd = Detail::visibleWindows[i];
			wnd->SignalMouseMotion(lastMouseLocation,true);
			if (wnd->isModal)
				break;
		}
	}

	static void CheckMouseMotion()
	{
		if (lastMouseLocation != Engine::mouse.location.windowRelative)
		{
			OverrideSignalMouseMotion();
		}
	}

	WheelRs	HandleMouseWheel(short delta)
	{
		for (index_t i = Detail::visibleWindows.Count()-1; i < Detail::visibleWindows.Count(); i--)
		{
			PWindow w = Detail::visibleWindows[i];
			WheelRs rs = w->HandleMouseWheel(delta);
			if (w->isModal && rs == WheelRs::NotContained)
				rs = WheelRs::Ignored;
			if (rs != WheelRs::NotContained)
			{
				if (rs == WheelRs::Processed)
					w->SignalMouseMotion(Engine::mouse.location.windowRelative,true);
					
				return rs;
			}
		}
		return WheelRs::NotContained;
	}

	bool	HandleMouseDown()
	{
		for (index_t i = Detail::visibleWindows.Count()-1; i < Detail::visibleWindows.Count(); i--)
		{
			PWindow w = Detail::visibleWindows[i];
			mousePressedComponent = w->HandleMouseDown();
			if (mousePressedComponent || w->isModal)
				return true;
		}
		mousePressedComponent.reset();
		return false;
	}

	void	HandleMouseUp()
	{
		if (mousePressedComponent)
		{
			mousePressedComponent->SignalMouseUp();
			mousePressedComponent.reset();
		}

	}


	void	SignalSituationChange()
	{
		static bool checking = false;
		if (checking)
			return;
		checking = true;
		VerifyIntegrity(CLOCATION);
		for (index_t i = 0; i < Detail::visibleWindows.Count(); i++)
		{
			PWindow d = Detail::visibleWindows[i];	//make copy to allow SignalChange() to create new windows
			if (!d->SignalChange())
			{
				d->OnHide();
				if (i+1 == Detail::visibleWindows.Count())
				{
					d->SetActive(false);
					if (Detail::visibleWindows.Count() > 1)
						Detail::visibleWindows.GetFromEnd(1)->SetActive(true);
				}
				Detail::visibleWindows.Erase(i);
				i--;
			}
			else
				d->DetectEnabledState();
		}
		VerifyIntegrity(CLOCATION);
		checking = false;
	}


	void	VerifyIntegrity(const TCodeLocation&loc)
	{
		if (Detail::visibleWindows.IsEmpty())
			return;


		for (index_t i = 0; i < Detail::visibleWindows.Count(); i++)
		{
			bool shouldBeActive = (i+1 == Detail::visibleWindows.Count());
			GUI::PWindow w = Detail::visibleWindows[i];
			if (w->IsActive() != shouldBeActive)
			{
				String msg = String(loc)+ ": Activity mismatch for window "+String(i+1)+"/"+String(Detail::visibleWindows.Count())+" (active="+String(w->IsActive())+", should be "+String(shouldBeActive)+")";
				LogEvent(EventClass::Preset::error,msg);
				DBG_FATAL__(msg);
				if (shouldBeActive)
					w->SetActive(true);
				else
					w->SetActive(false);
			}
		}
	}


	void			TAnchor::SetSomeOf(const PPanel&p, float distance, const std::function<bool(const PComponent&)>&callback, distanceType_t dtype/*=RelativeToParentComponent*/)
	{
		if (!p || !p->HasComponents())
			return;
		type = Fixed;
		distanceFromReference = distance;
		foreach (p->GetComponents(),c)
			if (callback(*c))
				references.Append(*c);
		distanceType = dtype;

	}


	void			TAnchor::SetAllOf(const PPanel&p, float distance, distanceType_t dtype/*=RelativeToParentComponent*/)
	{
		if (!p || !p->HasComponents())
			return;
		type = Fixed;
		distanceFromReference = distance;
		references.Clear();
		foreach (p->GetComponents(),c)
			references.Append(*c);
		distanceType = dtype;
	}

	void			TAnchor::SetLastOf(const PPanel&p, float distance, distanceType_t dtype/*=RelativeToParentComponent*/)
	{
		if (!p)
			return;
		type = Fixed;
		distanceFromReference = distance;
		references.Clear();
		if (p->HasComponents())
			references.Append(p->GetComponents().last());
		distanceType = dtype;
	}


	float			TAnchor::ToScreenDistance(float parentSize, bool isHorizontal) const
	{
		switch (distanceType)
		{
			case RelativeToParentComponent:
				return distanceFromReference * parentSize;

			case RelativeToScreen:
				return distanceFromReference;
			case AbsolutePixels:
				return distanceFromReference / float(isHorizontal ? ViewPortWidth() : ViewPortHeight());
		}
		FATAL__("Unexpected distance type");
		return 0.f;

	}

	/*virtual override*/ bool	Panel::AutoLayout(const Rect<>&context)
	{
		bool rs = Super::AutoLayout(context); 
		//Rect<> childSpace = rect;
		if (vscroll.visible)
			rect.x.max = vmax(rect.x.min, rect.x.max - TScrollbar::ContentSpacing - TScrollbar::Width);
		foreach (components,c) 
			rs |= (*c)->AutoLayout(rect);
		return rs;
	}

	void	Panel::TScrollbar::Render(const Rect<>&rect)
	{
		
		targetViewTop = vmin(targetViewTop,scrollMax - viewSize);
		float speed = vmax(1.f,fabs(targetViewTop - viewTop))*2.f;
		if (targetViewTop > viewTop)
		{
			viewTop = vmin(targetViewTop, viewTop + Engine::timing.delta*speed);
		}
		elif (targetViewTop < viewTop)
		{
			viewTop = vmax(targetViewTop, viewTop - Engine::timing.delta*speed);
		}


		float left = rect.x.max + TScrollbar::ContentSpacing;
		float right = left + TScrollbar::Width;
		float buttonHeight = TScrollbar::Width * display.pixelAspect();
		Rect<> topButton(left,rect.y.max - buttonHeight,right,rect.y.max);
		Rect<> bottomButton(left,rect.y.min,right,rect.y.min + buttonHeight);

		outline = Rect<>(left,bottomButton.y.max + TScrollbar::ContentSpacing,right,topButton.y.min - TScrollbar::ContentSpacing);
		SurfaceRenderer::SetColor(float4(0,0,0,0.9));
		SurfaceRenderer::Rasterize(outline);

		bar = Rect<>(outline.x.min,outline.y.Derelativate(1.f - (viewTop + viewSize) / scrollMax),outline.x.max,outline.y.Derelativate(1.f - (viewTop) / scrollMax));
		RenderArea(bar,barContainsMouse,dragging,float3(1));

		up->SetLocation(topButton);
		up->Render();
		down->SetLocation(bottomButton);
		down->Render();
	}


	WheelRs	Panel::TScrollbar::HandleMouseWheel(short delta)
	{
		if (!visible)
			return WheelRs::NotContained;
		//if (outlineContainsMouse || barContainsMouse)	//always scroll
		{
			if (!dragging)
			{
				constexpr float speed = -1.f / 5.f;
				viewTop = clamp(viewTop + speed * viewSize* delta,0,scrollMax - viewSize);
				targetViewTop = clamp(targetViewTop + speed * viewSize* delta,0,scrollMax - viewSize);
				return WheelRs::Processed;
			}
			return WheelRs::Ignored;
		}
		//return up->HandleMouseWheel(delta) || down->HandleMouseWheel(delta);
	}

	void		Panel::TScrollbar::SignalMouseMotion(const TVec2<>&mouseAt, bool clipAreaContainsMouse)
	{
		if (visible)
		{
			outlineContainsMouse = outline.Contains(mouseAt) && !bar.Contains(mouseAt) && clipAreaContainsMouse;
			scrollDownOnClick = mouseAt.y < bar.y.min;
			if (dragging)
			{
				viewTop = targetViewTop = clamp((1.f-outline.y.Relativate(mouseAt.y - dragRelative)) * scrollMax,0, scrollMax-viewSize);
			}
			else
			{
				barContainsMouse = bar.Contains(mouseAt);
				dragRelative = mouseAt.y - bar.y.max;
			}
			up->SignalMouseMotion(mouseAt,clipAreaContainsMouse);
			down->SignalMouseMotion(mouseAt,clipAreaContainsMouse);
		}
		else
			outlineContainsMouse = false;
	}


	void Panel::TScrollbar::SignalMouseUp()
	{
		if (mouseDown)
		{
			if (scrollDownOnClick)
			{
				targetViewTop += vmax(viewSize, scrollMax /10.f);
				targetViewTop = vmin(targetViewTop,scrollMax - viewSize);	
			}
			else
			{
				targetViewTop -= vmax(viewSize, scrollMax /10.f);
				targetViewTop = vmax(targetViewTop,0);	
			}
		}
		dragging = false;
		mouseDown = false;

	}

	PComponent	Panel::TScrollbar::HandleMouseDown()
	{
		if (visible)
		{
			PComponent c;
			c = up->HandleMouseDown();
			if (c)
				return c;
			c = down->HandleMouseDown();
			if (c)
				return c;
			if (outlineContainsMouse)
			{
				mouseDown = true;
				return owner->shared_from_this();
			}
			if (barContainsMouse)
			{
				dragging = true;
				return owner->shared_from_this();
			}
		}
		return PComponent();
	}

	void	Panel::TScrollbar::Enable(Panel*p,const Rect<>&rect)
	{
		owner = p;
		if (!up)
		{
			up.reset(new ScrollButton(ScrollButton::Up,[this,p]()
			{
				targetViewTop -= viewSize;
				targetViewTop = vmax(targetViewTop,0);	
			}));

			down.reset(new ScrollButton(ScrollButton::Down,[this,p]()
			{
				targetViewTop += viewSize;
				targetViewTop = vmin(targetViewTop,scrollMax - viewSize);
			}));
			up->SetActive(p->IsActive());
			down->SetActive(p->IsActive());
		}
		visible = true;
	}

	/*virtual override*/ void	Panel::SetActive(bool isActive)
	{
		foreach (components,c)
			(*c)->SetActive(isActive); 
		Super::SetActive(isActive);
		if (vscroll.up)
		{
			vscroll.up->SetActive(isActive);
			vscroll.down->SetActive(isActive);
		}
	}

	/*static*/ void		ScrollButton::RenderArrow(const Rect<>&r, direction_t dir, bool lit)
	{
		SurfaceRenderer::SetColor(float4(1,1,1,lit?0.75f:0.5f));

		switch (dir)
		{
			case Up:
				SurfaceRenderer::Rasterize(float2(r.x.center(),r.y.Derelativate(0.75f)),float2(r.x.Derelativate(0.25f),r.y.Derelativate(0.25f)),float2(r.x.Derelativate(0.75f),r.y.Derelativate(0.25f)));
			break;
			case Down:
				SurfaceRenderer::Rasterize(float2(r.x.center(),r.y.Derelativate(0.25f)),float2(r.x.Derelativate(0.75f),r.y.Derelativate(0.75f)),float2(r.x.Derelativate(0.25f),r.y.Derelativate(0.75f)));
			break;

			default:
				FATAL__("Unexpected direction parameter");
		}

	}


	/*virtual override*/ void	Panel::Render()
	{
		Super::Render(); 

		if (vscroll.visible)
		{
			vscroll.Render(rect);
			Detail::PushLoadView(rect,float2(0,-vscroll.viewTop));
		}
		else
			Detail::PushLoadView(rect);
		RenderChildren();
		Detail::PopLoadView();
	}


	/*virtual override*/ bool	Panel::UpdateHeight()
	{
		bool rs = Super::UpdateHeight();
		foreach (components,c)
			rs |= (*c)->UpdateHeight();

		if (autoHeight)
		{
			float lowest = std::numeric_limits<float>::max();
			foreach (components,c)
			{
				const auto&bottom = (*c)->anchorConfig.Bottom();
				const auto&top = (*c)->anchorConfig.Top();
				switch (bottom.type)
				{
					case TAnchor::Disabled:
						lowest = std::min(lowest, (*c)->GetLocation().y.min);
					break;
					case TAnchor::Fixed:
						if (bottom.references.IsEmpty() && top.type == TAnchor::Disabled)
						{
							float dist = bottom.distanceFromReference;
							if (bottom.distanceType != TAnchor::RelativeToParentComponent)
								lowest = std::min(lowest,rect.y.max - (*c)->GetLocation().y.GetExtent() - bottom.ToScreenDistance(0.f,false));
						}
					break;
					case TAnchor::Auto:
						if (top.type == TAnchor::Auto)
						{
							lowest = std::min(lowest,rect.y.max - (*c)->GetLocation().y.GetExtent());
						}
					break;
				}
			}
			if (components.IsEmpty())
				lowest = rect.y.max;
			float newY = lowest;// - autoHeightSpacing;
			float currentHeight = rect.y.max - newY;
			bool newVScroll = currentHeight > maxHeight;
			bool changed = vscroll.visible != newVScroll;
			rs |= changed;
			if (newVScroll)
			{
				vscroll.scrollMax = currentHeight;
				vscroll.viewSize = maxHeight;
				float barSize = vmax(maxHeight,0.2f * rect.y.GetExtent()*currentHeight);
				float overSize = barSize - maxHeight;
				vscroll.scrollMax += overSize;
				vscroll.viewSize += overSize;

				newY = rect.y.max - maxHeight;
				rs |= Set(rect.y.min,newY);
				vscroll.Enable(this,rect);
			}
			else
			{
				vscroll.visible = false;
				rs |= Set(rect.y.min,newY);
			}
		}

		return rs;
	}


	/*static*/ void				Component::RenderArea(const Rect<>&rect, bool lit, bool flip, const float3&backgroundTone)
	{
		float low = 0.55f;  //0.3;
		float high = lit ? 0.89f : 0.77f;   //0.8f : 0.6;
		if (flip)
			std::swap(low,high);

		//low = sqrt(low);
		float4 lowColor(low,low,low,low);
		Vec::stretch(lowColor.rgb,backgroundTone,lowColor.rgb);
		//high = sqrt(high);
		float4 highColor(high,high,high,high);
		Vec::stretch(highColor.rgb,backgroundTone,highColor.rgb);

		SurfaceRenderer::Rasterize(float2(rect.x.min,rect.y.min), lowColor, float2(rect.x.max,rect.y.min),lowColor, float2(rect.x.max,rect.y.max), highColor, float2(rect.x.min,rect.y.max),highColor);
		SurfaceRenderer::Flush();
		

		float4 lineLowColor(backgroundTone,low*low);
		float4 lineHighColor(backgroundTone,high*high);
	

		LineRenderer::Rasterize(float3(rect.x.min,rect.y.min,0.f), lineLowColor, float3(rect.x.max,rect.y.min,0.f),lineLowColor);
		LineRenderer::Rasterize(float3(rect.x.max,rect.y.max,0.f), lineHighColor, float3(rect.x.min,rect.y.max,0.f),lineHighColor);
		LineRenderer::Rasterize(float3(rect.x.max,rect.y.min,0.f),lineLowColor, float3(rect.x.max,rect.y.max,0.f), lineHighColor);
		LineRenderer::Rasterize(float3(rect.x.min,rect.y.max,0.f),lineHighColor, float3(rect.x.min,rect.y.min,0.f), lineLowColor);
		LineRenderer::Flush();
	}
	

	static float CharWidth(char c)
	{
		if ((index_t)c >= Objects::TextoutResourceTagOffset && (index_t)c <= (index_t)Objects::TextoutResourceTagOffset + Objects::Resource::NumberOfPossibilities)
		{
			return textout.GetFont().GetHeight() * textout.state.scale_x;
		}
		return textout.GetFont().GetWidth(c) * textout.state.scale_x;
	}

	void OnWindowResize()
	{
		windowResolutionVersion++;
		if (windowResolutionVersion == InvalidIndex)
			windowResolutionVersion++;
	}


	static const float textZoom = 1.5f;

	bool			Area::UpdateHeightIfNecessary()
	{
		if (this->sizeVersion == windowResolutionVersion)
			return false;
		return UpdateHeight();
	}


	/*virtual override*/ void	Panel::SignalMouseMotion(const TVec2<>&mouseAt, bool clipAreaContainsMouse)
	{
		Super::SignalMouseMotion(mouseAt,clipAreaContainsMouse);

	
		vscroll.SignalMouseMotion(mouseAt,clipAreaContainsMouse);
		
		TVec2<> moved = mouseAt;
		moved.y += vscroll.GetOffset();

		for (const auto&c : components)
		{
			c->SignalMouseMotion(moved,ContainsMouse());
		}
	}


	/*virtual override*/ WheelRs	Panel::HandleMouseWheel(short delta)
	{
		if (!ContainsMouse())
			return WheelRs::NotContained;

		for (index_t i = components.Count()-1; i < components.Count(); --i)
		{
			WheelRs rs = components[i]->HandleMouseWheel(delta);
			if (rs == WheelRs::Processed)
				return rs;
			if (rs == WheelRs::Ignored)
				break;
		}

		if (vscroll.HandleMouseWheel(delta) == WheelRs::Processed)
			return WheelRs::Processed;

		return WheelRs::Ignored;
		//Super::HandleMouseWheel(delta);
	}


	/*virtual override*/ PComponent	Panel::HandleMouseDown()
	{
		PComponent c = vscroll.HandleMouseDown();
		if (c)
			return c;



		if (!ContainsMouse())
			return PComponent();

		for (index_t i = components.Count()-1; i < components.Count(); --i)
		{
			PComponent c = components[i]->HandleMouseDown();
			if (c)
				return c;
		}
		return Super::HandleMouseDown();
	}


	void			Panel::AnchorBelowPrevious(const PComponent&newComponent, float distanceFromSides /*= 0.015f*/,  float distanceToPrevious/* = 0.015f*/, TAnchor::distanceType_t distanceType/*=TAnchor::RelativeToScreen*/)
	{
		if (components.IsNotEmpty())
			newComponent->anchorConfig.Top().Set(components.last(), distanceToPrevious,distanceType);
		else
			newComponent->anchorConfig.Top().Set(distanceToPrevious,distanceType);
		
		newComponent->anchorConfig.Left().Set(distanceFromSides,distanceType);
		newComponent->anchorConfig.Right().Set(distanceFromSides,distanceType);
		components.Append(newComponent);
	}


	void				Area::BaseRender(bool lit, bool flipped)
	{
		RenderArea(rect,lit,flipped,backgroundTone);
	}

	static const float CellSize = 0.01f;

	bool			Area::UpdateHeight()
	{
		this->sizeVersion = windowResolutionVersion;
		
		return Super::UpdateHeight();
	}

	/*virtual override*/ bool			Panel::DetectEnabledState()
	{
		bool change = Super::DetectEnabledState();
		foreach (components,c)
			change |= (*c)->DetectEnabledState();
		return change;
	};

	//DX11::BaseInstructionBlock	Area::shader;


	//void		Area::Render(bool lit)
	//{
	//	display.RenderVertices(instruction);
	//
	//}


	//void		Area::StaticCreate()
	//{
	//	shader.shader.LoadFromFile("shaders/hexBackground.hlsl","vsMain","gsMain", "psMain");
	//	shader.binding.Append("POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT);
	//	shader.UpdateLayout();
	//	shader.SetPoints();
	//}

	//void		Area::StaticDestroy()
	//{
	//	shader.Clear();
	//
	//}



	bool			BaseInput::UpdateHeight()
	{
		textout.PushState();
			textout.Scale(textZoom,textZoom);

			float newMin = this->rect.y.max - textout.GetScaledHeight() * 2.5f;
			bool same = Math::similar(newMin,this->rect.y.min);
			this->rect.y.min = newMin;

		textout.PopState();
		return Super::UpdateHeight() || !same;
	}



	void			Input::SetCurrent(const String&current)
	{
		if (IsActive())
			Engine::keyboard.FillInput(current);
		else
			backup.SetTo(current);
	}

	static Engine::InputProfile	profile;

	void			Input::SetAllowed(const String&allowed)
	{
		this->allowed = allowed;
		if (Super::IsActive())
			Engine::keyboard.Filter(allowed.c_str());
	}

	/*static*/ void BaseInput::OnCharHandled(char)
	{
		Detail::OnInput();
	}

	void			BaseInput::SetInputActive(bool isActive)
	{
		if (isActive)
		{
			Engine::input.pushProfile();
			//profile = *Engine::input.currentProfile();//this copies over a shitload of commands we don't want
			//let's copy some over, however...
			profile.key[Key::Escape] = Engine::input.currentProfile()->key[Key::Escape];
			profile.key[Key::Enter] = Engine::input.currentProfile()->key[Key::Enter];
			profile.key[Key::Return] = Engine::input.currentProfile()->key[Key::Return];
			profile.key[Key::MouseButton0] = Engine::input.currentProfile()->key[Key::MouseButton0];
			profile.key[Key::MouseButton1] = Engine::input.currentProfile()->key[Key::MouseButton1];
			profile.key[Key::MouseButton2] = Engine::input.currentProfile()->key[Key::MouseButton2];
			profile.key[Key::MouseButton3] = Engine::input.currentProfile()->key[Key::MouseButton3];
			profile.key[Key::MouseButton4] = Engine::input.currentProfile()->key[Key::MouseButton4];
			Engine::input.bindProfile(profile);
		
			//Engine::input.resetKeys();
			Engine::input.bind(Key::Backspace,[](){Engine::keyboard.DropLastCharacter(); Detail::OnInput();});
			Engine::input.bind(Key::Return,onEnter);
			Engine::input.bind(Key::Enter,onEnter);
			Engine::input.bindCtrl(Key::V,[](){
				char buffer[0x100];
				memset(buffer,0,sizeof(buffer));
				if (System::getFromClipboardIfText(Engine::context.getWindow(),buffer,sizeof(buffer)-1))
				{
					Engine::keyboard.FilterAppend(buffer);	
				}
			});

			const char*filter = GetAllowed();
			if (filter)
				Engine::keyboard.Filter(filter);
			else
				Engine::keyboard.ResetTranslation();
			Engine::keyboard.onCharHandled = BaseInput::OnCharHandled;
			Engine::keyboard.read = true;
			Engine::keyboard.ImportBuffer(backup);
		}
		else
		{
			Engine::keyboard.ExportBuffer(backup);
			Engine::input.popProfile();
			Engine::keyboard.read = false;
			Engine::keyboard.onCharHandled =  NULL;
		}
	}



	String			Input::GetInput()
	{
		if (IsActive())
			return String(Engine::keyboard.GetInput(),Engine::keyboard.GetInputLen());
		return String(backup.GetCharacters(),backup.GetLength());
	}

	const char*		PasswordInput::GetInput()
	{
		if (IsActive())
			return Engine::keyboard.GetInput();
		return backup.GetCharacters();
	}

	void			PasswordInput::WipeInput()
	{
		if (IsActive())
			Engine::keyboard.FlushInput();
		backup.Wipe();
	}





	/*virtual*/ StringRef BaseInput::GetText()
	{
		const char*input;
		size_t len;
		bool active = IsActive();
		input = active ? Engine::keyboard.GetInput() : backup.GetCharacters();
		len = active ? Engine::keyboard.GetInputLen() : backup.GetLength();
		return StringRef(input,len);
	}


	/*static*/ StringBuffer	PasswordInput::placeholder;

	/*virtual override*/ StringRef PasswordInput::GetText()
	{
		size_t len;
		bool active = IsActive();
		len = active ? Engine::keyboard.GetInputLen() : backup.GetLength();

		while (placeholder.length() < len)
		{
//			placeholder << "********";
			placeholder << "........";
		}
		return StringRef(placeholder.pointer(),len);
	}


	/*virtual override*/ void			BaseInput::Render()
	{
		core = rect;
		textout.PushState();
		textout.Scale(textZoom,textZoom);
		core.Expand(-textout.GetScaledHeight()*0.4f);
		//core.Scale(0.97f);

		if (fillBackground)
			RenderArea(core,false,true,backgroundTone);

		StringRef text = GetText();


		static const char cursorChar = '<';
		float cursorLen = textout.GetScaledWidth(StringRef(&cursorChar,1));
		float visualLen = textout.GetScaledWidth(text) + cursorLen;


		SurfaceRenderer::Flush();


		textout.MoveTo(core.x.min,core.y.max - textout.GetScaledHeight() * 0.4f);


		if (visualLen < core.x.GetExtent())
		{
		//	textout.write(input,len);
			textout << text;
		}
		else
		{
			while (text.DropFirstChar())
			{
				visualLen = textout.QueryScaledWidth(text) + cursorLen;
				if (visualLen < core.x.GetExtent())
				{
					textout << text;
					//textout.write(input,len);
					break;
				}
			}
		}
		if (((UINT64)(Engine::timing.now*2.0))%2)
			textout << cursorChar;


		textout.PopState();


	}



	void		Create()
	{
		Image::StaticCreate();
	}

	/*static*/ void Image::StaticDestroy()
	{
		texturedQuad.Clear();
	}

	/*static*/ void Image::StaticCreate()
	{
		if (texturedQuad.quickSlots.IsEmpty())
		{
			texturedQuad.quickSlots.SetSize(1);
			Engine::VertexBinding vtx;
			vtx.vertex.set(0,2);
			vtx.normal.unset();
			vtx.floats_per_vertex = 2;
			texturedQuad.binding = vtx;

			DX11::Shader::Binding bindings[] = {
				DX11::Shader::Binding(0,"Texture"),
				DX11::Shader::SamplerBinding(true,Engine::VisualEnum::TextureFilter::Trilinear,1,"Sampler"),
			};


			texturedQuad.shader.LoadFromFile("shaders/guiImageField.hlsl","vsMain","psMain",bindings,ARRAYSIZE(bindings) );
			texturedQuad.UpdateLayout();
			texturedQuad.SetTriangles();


			float2 vertices[6] = 
			{
				float2(0,0),
				float2(1,0),
				float2(1,1),
				float2(0,0),
				float2(1,1),
				float2(0,1)
			};

			texturedQuad.vbo.CreateFromStructs(vertices,6);
		}


	//	Area::StaticCreate();
	
	}

	void		Destroy()
	{
		Image::StaticDestroy();
		//Area::StaticDestroy();
		Detail::visibleWindows.Clear();
	}




	void	RenderNonModal()
	{
		if (Detail::visibleWindows.IsNotEmpty())
		{
			CheckMouseMotion();
		}


		Detail::LoadView(Rect<>(0,0,1,1),float2());
		
		foreach (Detail::visibleWindows,d)
		{
			if ((*d)->isModal)
				break;
			(*d)->Render();
		}
	}

	void	RenderModal()
	{
		if (Detail::visibleWindows.IsNotEmpty())
		{
			CheckMouseMotion();
		}


		Detail::LoadView(Rect<>(0,0,1,1),float2());
		
		bool nonModal = true;
		foreach (Detail::visibleWindows,d)
		{
			if ((*d)->isModal)
				nonModal = false;
			if (!nonModal)
				(*d)->Render();
		}
	}

	/*virtual*/ float			TextField::GetIdealWidth() const
	{
		textout.PushState();
		textout.Scale(textZoom*zoom,textZoom*zoom);
		float w = 0.f;

		foreach (rawText,t)
		{
			float w1 = 0.f;
			foreach (*t,c)
				w1 += CharWidth(*c);
			w = std::max(w,w1);
		}
		textout.PopState();
		return w/ 0.85f;
	}

	bool			TextField::UpdateIdealWidth()
	{
		float w = GetIdealWidth();

		bool changed = Set(rect.x.max, rect.x.min + w);

		return changed;
	}

	void			TextField::SetIdealWidth()
	{

		if (UpdateIdealWidth())
			InvalidateSize();
	}




	bool		TextField::UpdateHeight()
	{
		return UpdateHeight(rect.x);
	}

	/*virtual override*/ bool		TextField::UpdateHeight(const TFloatRange<>&hRange)
	{
		bool changed = false;
		if (widthLockedToIdeal)
			changed = UpdateIdealWidth();

		textout.PushState();
		textout.Scale(textZoom*zoom,textZoom*zoom);

		wrappedText.Clear();
		Array<String>	wrapped;
		for (const String&raw : rawText)
		{
			wrapf(raw,hRange.GetExtent()*0.9f,CharWidth,wrapped);
			wrappedText.MoveAppend(wrapped);
		}
		lineAt.SetSize(wrappedText.Count());
		float lineHeight = textout.GetScaledHeight();
		float y = -lineHeight*(extraSpacing);
		for (index_t i = 0; i < lineAt.Count(); i++)
		{
			lineAt[i] =
				float2(
					alignCenter ? 
						hRange.center() - textout.QueryScaledWidth(wrappedText[i])*0.5f
						:
						hRange.min,
				y - lineHeight * (i));
		}
		textHeight = textout.GetScaledHeight() * (float(wrappedText.Count()) + extraSpacing*2.f);

		changed |= Set(this->rect.y.min,rect.y.max - textHeight);

		textout.PopState();
		return Super::UpdateHeight() || changed;
	}



	/*virtual override*/ void			TextField::Render()
	{
		UpdateHeightIfNecessary();
	
	
		
		bool lit = IsLit();

		if (fillBackground)
		{
			Super::BaseRender(lit,false);
		}
		RenderText(lit);
	}

	void TextField::RenderText(bool lit)
	{
		textout.PushState();
		textout.Scale(textZoom*zoom,textZoom*zoom);

		//bool hovering = !Super::sensitiveToMouse || (enabled && ContainsMouse());

		SurfaceRenderer::Flush();

		//textout.locate(rect.x.min + rect.x.GetExtent()*0.05f,rect.y.max - textout.GetScaledHeight()*1.5f);
		//float lineHeight = textout.GetScaledHeight();
		float top = rect.y.max;


		float4 baseColor;
		if (IsEnabled())
			baseColor = (lit ? float4(1) : float4(0.75,0.75,0.75,1));
		else
			baseColor = float4(0.5,0.5,0.5,1);

		textout.SetColor(baseColor & this->color);
		for (index_t i = 0; i < wrappedText.Count(); i++)
		{
			textout.MoveTo(lineAt[i].x,lineAt[i].y+top);
			textout << wrappedText[i];
		}
		textout.PopState();
	}

	/*virtual override*/ bool	Image::UpdateHeight()
	{
		if (texture.IsEmpty() || !autoSetHeight)
			return Super::UpdateHeight();

		const Resolution3D imageResolution = texture.GetResolution();
		const float2 relativeComponentSize = float2(rect.x.GetExtent(),rect.y.GetExtent());
		const float2 viewportSize = ViewPortSizef();
		const float2 pixelComponentSize = relativeComponentSize & viewportSize;
		const float2 pixelImageSize = float2(imageResolution.width,imageResolution.height);

		const float horizontalStretch = pixelImageSize.x / pixelComponentSize.x;



		const float wantPixelImageHeight = pixelComponentSize.x / imageResolution.GetFlatAspect();
		const float wantRelativeHeight = wantPixelImageHeight / float(ViewPortHeight());

		bool change = Set(rect.y.min,rect.y.max - wantRelativeHeight);

		return Super::UpdateHeight() || change;
	}


	/*virtual override*/ void			Image::Render()
	{
		if (texture.IsEmpty())
			return;

		bool lit = IsLit();
	
		TMatrix4<> matrix = Matrix<>::eye4;
		
		Resolution3D res = texture.GetResolution();
		float2 resf = float2(res.width,res.height);

		float2 size = float2(rect.x.GetExtent(),rect.y.GetExtent());
		float2 pixelDim = size & ViewPortSizef();
		float aspect = pixelDim.x / pixelDim.y;
		float wantAspect = resf.x / resf.y;
		if (aspect < wantAspect)
		{
			//image too broad
			size.y *= (aspect/wantAspect);
		}
		else
		{
			//image too heigh
			size.x *= (wantAspect / aspect);
		}

		matrix.x.x = size.x;
		matrix.y.y = size.y;

		matrix.w.xy = float2(rect.x.center() - size.x*0.5f,rect.y.center() - size.y*0.5f);

		display.SetObjectMatrix(matrix);
		texturedQuad.quickSlots.First() = texture;

		display.RenderVertices(texturedQuad);
	}

	/*virtual*/ void	Component::SignalMouseMotion(const TVec2<>&mouseAt, bool clipAreaContainsMouse)
	{
		containsMouse = clipAreaContainsMouse && rect.Contains(mouseAt);
	}


	void			Component::MoveTopTo(float h)
	{
		float delta = h - rect.y.max;
		rect.y.Translate(delta);
	}

	bool			Component::FullAutoLayout(const Rect<>&context)
	{
		count_t numRounds = 0;
		bool rs = UpdateHeight();
		for (;;)
		{
			if (!AutoLayout(context))
			{
				//nothing changed
				rs |= UpdateHeightIfNecessary();
				return rs;
			}
			if (!UpdateHeight())
			{
				//layout changed, but heights stayed the same
				return rs;
			}

			rs = true;
			numRounds++;

			#ifdef _DEBUG
				if (numRounds > 900)
					ShowMessage(__func__ ": Debug: GUI will abort auto-layout shortly. Attach now and debug.");

			#endif

			if (numRounds > 1000)	//might be linear to the number of contained components, depending on order
				return false;
		}

	}


	bool			Component::LayoutAxis(int axis, const Rect<>&context)
	{
		const Axial<TAnchor>&anchor = anchorConfig.axis[axis];
		TFloatRange<>&a = rect.axis[axis];

		bool rs = false;
		//if (anchor.center.IsActive())
		//{
		//	ASSERT__(!anchor.min.IsActive());
		//	ASSERT__(!anchor.max.IsActive());
		//	const TAnchor&anc = anchor.center;
		//	const float dist = anc.distanceIsRelative ? anc.distanceFromReference * context.axis[axis].GetExtent() : anc.distanceFromReference;
		//	if (anc.references.IsNotEmpty())
		//	{
		//		float avg = 0.f;
		//		count_t cnt = 0;
		//		foreach (anc.references,ref0)
		//		{
		//			PComponent ref = ref0->lock();
		//			if (ref)
		//			{
		//				avg += ref->GetLocation().axis[axis].center();
		//				cnt++;
		//			}
		//		}
		//		if (cnt > 0)
		//		{
		//			avg /= cnt;
		//			rs |= SetCenter(a, avg + dist);
		//		}
		//	}
		//	else
		//		rs |= SetCenter(a, context.axis[axis].center() + dist);

		//	return rs;
		//}

		if (anchor.min.IsInactive() && anchor.max.IsInactive())
			return false;
		if (anchor.min.IsAuto() && anchor.max.IsAuto())
		{
			{
				const TAnchor&anc = anchor.min;

				if (anc.references.IsNotEmpty())
				{
					FATAL__("Auto not applicable on set references");
					return false;
					//float highest = -std::numeric_limits<float>::max();
					//foreach (anc.references,ref0)
					//{
					//	PComponent ref = ref0->lock();
					//	highest = std::max(highest,ref->GetLocation().axis[axis].max);
					//}
					//if (highest == -std::numeric_limits<float>::max())
					//	return false;
					//bottom = highest;
				}
			}
			{
				const TAnchor&anc = anchor.max;

				if (anc.references.IsNotEmpty())
				{
					FATAL__("Auto not applicable on set references");
					return false;
				}
			}
			const TFloatRange<>&ctx = context.axis[axis];
			float ext = a.GetExtent();
			float center = ctx.GetCenter();
			float top = std::min(ctx.max,center + ext*0.5f);
			bool changed = Set(a.max,top);
			a.min = a.max - ext;
			return changed;
		}

		if (anchor.min.IsAuto() || anchor.max.IsAuto())
		{
			FATAL__("Both top and bottom anchors must be set to auto");
			return false;
		}


		float extent = a.GetExtent();

		if (anchor.min.IsFixed())
		{
			const TAnchor&anc = anchor.min;

			const float dist = anc.ToScreenDistance(context.axis[axis].GetExtent(),axis==0);

			if (anc.references.IsNotEmpty())
			{
				float highest = -std::numeric_limits<float>::max();
				foreach (anc.references,ref0)
				{
					PComponent ref = ref0->lock();
					highest = std::max(highest,ref->GetLocation().axis[axis].max);
				}
				if (highest > -std::numeric_limits<float>::max())
					rs |= Set(a.min, highest + dist);
			}
			else
				rs |= Set(a.min, context.axis[axis].min + dist);
		}
		if (anchor.max.IsFixed())
		{
			const TAnchor&anc = anchor.max;

			const float dist = anc.ToScreenDistance(context.axis[axis].GetExtent(),axis==0);
//			const float dist = anc.distanceIsRelative ? anc.distanceFromReference * context.axis[axis].GetExtent() : anc.distanceFromReference;

			if (anc.references.IsNotEmpty())
			{
				float lowest = std::numeric_limits<float>::max();
				foreach (anc.references,ref0)
				{
					PComponent ref = ref0->lock();
					lowest = std::min(lowest,ref->GetLocation().axis[axis].min);
				}
				if (lowest < std::numeric_limits<float>::max())
					rs |= Set(a.max, lowest - dist);
			}
			else
				rs |= Set(a.max, context.axis[axis].max - dist);
		}
		else
			rs |= Set(a.max,a.min + extent);

		if (!anchor.min.IsFixed())
			rs |= Set(a.min,a.max - extent);


		return rs;
	}

	Component&			Component::FillHorizontally(float spacing, TAnchor::distanceType_t type /*= TAnchor::RelativeToParentComponent*/)
	{
		anchorConfig.Left().Set(spacing,type);
		anchorConfig.Right().Set(spacing,type);
		return *this;
	}
	
	Component&			Component::AnchorTop(float spacing, TAnchor::distanceType_t type/* = TAnchor::RelativeToParentComponent*/)
	{
		anchorConfig.Top().Set(spacing,type);
		return *this;
	}


	Component&			Component::SetCenteredWidth(float w, TAnchor::distanceType_t type)
	{
		float extra = 1.f - w;
		if (type == TAnchor::AbsolutePixels)
			extra = float(ViewPortWidth()) - w;
		anchorConfig.Left().Set(extra*0.5f,type);
		anchorConfig.Right().Set(extra*0.5f,type);
		return *this;
	}



	/*static*/ bool		Component::Set(float&ref, float to)
	{
		bool changed = !Math::similar(ref,to);
		ref = to;
		return changed;
	}

	/*static*/ bool		Component::SetCenter(TFloatRange<>&ref, float to)
	{
		float current = ref.center();

		bool changed = !Math::similar(current,to);
		ref.SetCenter(to);
		return changed;
	}


	/*virtual*/ bool	Component::AutoLayout(const Rect<>&context)
	{
		bool rs = false;
		rs |= LayoutAxis(0,context);
		rs |= LayoutAxis(1,context);
		return rs;
	}


	static bool CheckHovering(const Rect<>&area)
	{
		return area.Contains(Engine::mouse.location.windowRelative);
	}


	/*static*/ void	Component::Darken(const Rect<>&rect, float alpha/*=0.9f*/, bool flush/*=false*/)
	{
		SurfaceRenderer::SetColor(float4(0,0,0,alpha));
		SurfaceRenderer::Rasterize(rect);
		if (flush)
			SurfaceRenderer::Flush();
	
	}

	/*virtual override*/ float	Checkbox::GetIdealWidth() const
	{
		float checkBoxWidth = textZoom*zoom * textout.GetScaledHeight() / display.pixelAspect() * 1.5f;
		return Super::GetIdealWidth() + checkBoxWidth;
	}


	/*virtual override*/ bool	Checkbox::UpdateHeight()
	{
		float checkBoxWidth = textZoom*zoom * textout.GetScaledHeight() / display.pixelAspect() * 1.5f;
		return Super::UpdateHeight(FloatRange(rect.x.min + checkBoxWidth,rect.x.max ));
	}

	/*virtual override*/ void Checkbox::Render()
	{
		bool lit = IsLit();
		//Super::BaseRender(lit,false);
		Super::RenderText(lit);
		SurfaceRenderer::SetColor(lit ? float4(0.7f):float4(0.5f));
		float checkBoxHeight = textZoom*zoom * textout.GetScaledHeight();
		float checkBoxWidth = checkBoxHeight / display.pixelAspect();
		float top = rect.y.center() + checkBoxHeight*0.7f;// Super::extraSpacing * checkBoxHeight;
		TFloatRect box = TFloatRect(rect.x.min,top - checkBoxHeight,rect.x.min+checkBoxWidth,top);
		SurfaceRenderer::Rasterize(box);
		if (isChecked)
		{
			//SurfaceRenderer::SetColor(lit ? float4(0.7f):float4(0.4f));
			TFloatRect inner = box;
			inner.Scale(0.8f);
			SurfaceRenderer::SetColor(lit ? float4(1.f):float4(0.7f));
			SurfaceRenderer::Rasterize(inner);
		}
		SurfaceRenderer::Flush();
		LineRenderer::SetColor(lit ? float4(1) : float4(0.7f));
		LineRenderer::Rasterize(box);
		LineRenderer::Flush();
	}

}

#endif

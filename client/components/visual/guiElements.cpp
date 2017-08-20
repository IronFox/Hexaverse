#ifdef HAS_WINDOW

#include <global_root.h>
#include "guiElements.h"
#include <engine/display.h>
#include "../../ExtRenderer/DirectX11.h"
#include "../../DX11/texture_font.h"
#include <container/buffer.h>
#include <engine/aspect.h>
#include <objects.h>
#include "../clientEvents.h"

namespace DX11 = Engine::DX11; 

extern Engine::Textout<DX11::TextureFont>	textout;
extern Engine::Display<Engine::DirectX11>	display;


namespace GUI
{
	namespace Detail
	{
		static const float textZoom = 1.5f;

		Buffer0<GUI::PWindow,Swap>		visibleWindows;
		Engine::OrthographicAspect<>	aspect;
		Resolution						lastClientSpace;

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

		Buffer<Component*>	allComponents;
	}


	bool			ComponentLocation::SetWidth(float w)
	{
		if (M::Similar(this->rect.x.GetExtent(),w))
			return false;
		ASSERT__(!isnan(w));
		rect.x.max = rect.x.min + w;

		DBG_ASSERT_IS_CONSTRAINED__(rect.x.max,Lower,Upper);


		OnHorizontalRectChange();
		OnWidthChange();
		OnRectChange();
		return true;
	}
	bool			ComponentLocation::SetHeight(float h)
	{
		if (M::Similar(this->rect.y.GetExtent(), h))
			return false;
		ASSERT__(!isnan(h));
		rect.y.max = rect.y.min + h;

		DBG_ASSERT_IS_CONSTRAINED__(rect.y.max,Lower,Upper);


		OnVerticalRectChange();
		OnHeightChange();
		OnRectChange();
		return true;
	}
	void			ComponentLocation::SetSize(const TVec2<>&size)
	{
		auto r = rect;
		r.x.max = r.x.min + size.x;
		r.y.max = r.y.min + size.y;
		SetRect(r);
	}
	void			ComponentLocation::SetRect(const Rect<>&rect)
	{
		const bool hChange = rect.x != this->rect.x;
		const bool vChange = rect.y != this->rect.y;
		if (!hChange && !vChange)
			return;

		rect.AssertIsNotNaN();

		const bool widthChange = !Math::Similar(rect.x.GetExtent(),this->rect.x.GetExtent());
		const bool heightChange = !Math::Similar(rect.y.GetExtent(),this->rect.y.GetExtent());
		this->rect = rect;

		DBG_ASSERT_IS_CONSTRAINED__(rect.x.min,Lower,Upper);
		DBG_ASSERT_IS_CONSTRAINED__(rect.x.max,Lower,Upper);
		DBG_ASSERT_IS_CONSTRAINED__(rect.y.min,Lower,Upper);
		DBG_ASSERT_IS_CONSTRAINED__(rect.y.max,Lower,Upper);

		if (hChange)
			OnHorizontalRectChange();
		if (vChange)
			OnVerticalRectChange();
		if (widthChange)
			OnWidthChange();
		if (heightChange)
			OnHeightChange();
		OnRectChange();
	}


	AnchorModifier::AnchorModifier(Component*cmp):parent(cmp)
	{
		(*(AnchorConfig*)this) = parent->GetAnchorConfig();
	}

	AnchorModifier::~AnchorModifier()
	{
		if (parent)
			parent->SetAnchorConfig(*this);
	}

	AnchorModifier::AnchorModifier(AnchorModifier&&other)
	{
		(*(AnchorConfig*)this) = std::move(other);
		parent = other.parent;
		other.parent = nullptr;
	}


	/*virtual*/ void	Component::Render()
	{}


	Component::Component()
	{
		Detail::allComponents << this;
	}

	/*virtual*/			Component::~Component()
	{
		if (!application_shutting_down)
			Detail::allComponents.FindAndErase(this);
	}

	void Component::InvalidateAutoLayout()
	{
		PWindow w = GetWindow();
		if (w)
			w->SignalLayoutChange();
	}




	PComponent	mousePressedComponent;

	/*static*/ DX11::BaseInstructionBlock	Image::texturedQuad;

	static UINT ViewPortWidth()
	{
		return std::max(100U,Detail::lastClientSpace.width);
	}
	static UINT ViewPortHeight()
	{
		return std::max(100U,Detail::lastClientSpace.height);
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


	bool	CloseTopModal()
	{
		VerifyIntegrity(CLOCATION);
		if (Detail::visibleWindows.IsEmpty())
			return false;
		GUI::PWindow w = Detail::visibleWindows.Last();
		if (!w->canBeImplicitlyClosed)
			return false;
		Hide(w);
		return true;
	}

	bool	CloseAllModalBeneath(const PWindow&w)
	{
		VerifyIntegrity(CLOCATION);
		index_t at = Detail::visibleWindows.GetIndexOf(w);
		if (at == InvalidIndex)
			return false;

		for (index_t i = 0; i <= at; i++)
		{
			GUI::PWindow w = Detail::visibleWindows[i];
			if (w->canBeImplicitlyClosed)
			{
				Hide(w);
				i--;
				if (at == 0)
					return true;
				at--;
			}
		}

		return true;
	}


	bool	CloseAllModal()
	{
		VerifyIntegrity(CLOCATION);
		if (Detail::visibleWindows.IsEmpty())
			return false;

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
			wnd->OnMouseMove(lastMouseLocation,true);
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

	WheelRs	OnMouseWheel(short delta)
	{
		for (index_t i = Detail::visibleWindows.Count()-1; i < Detail::visibleWindows.Count(); i--)
		{
			PWindow w = Detail::visibleWindows[i];
			WheelRs rs = w->OnMouseWheel(delta);
			if (w->isModal && rs == WheelRs::NotContained)
				rs = WheelRs::Ignored;
			if (rs != WheelRs::NotContained)
			{
				if (rs == WheelRs::Processed)
					w->OnMouseMove(Engine::mouse.location.windowRelative,true);
					
				return rs;
			}
		}
		return WheelRs::NotContained;
	}

	bool	OnMouseDown()
	{
		for (index_t i = Detail::visibleWindows.Count()-1; i < Detail::visibleWindows.Count(); i--)
		{
			PWindow w = Detail::visibleWindows[i];
			mousePressedComponent = w->OnMouseDown();
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
			auto up = mousePressedComponent;
			mousePressedComponent.reset();
			up->OnMouseUp();
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


	/*virtual override*/ bool	Panel::ResolveAnchorAxis(int axis, const Rect<>&context)
	{
		bool rs = Super::ResolveAnchorAxis(axis,context); 
		Rect<> childSpace = GetRect();
		if (vscroll.visible)
			childSpace.x.max = vmax(childSpace.x.min, childSpace.x.max - TScrollbar::ContentSpacing - TScrollbar::Width);
		foreach (components,c) 
			rs |= (*c)->ResolveAnchorAxis(axis,childSpace);
		return rs;
	}


	/*virtual override*/ void		Slider::OnScreenSizeOrUIScaleChange() 
	{
		Super::OnScreenSizeOrUIScaleChange();
		relativeHeight = GetPixelHeight() / ViewPortHeight();
		relativeThumbWidth = GetThumbPixelWidth() / ViewPortWidth();
		UpdateDynamicSize();
	}

	/*static*/ float		Slider::GetThumbPixelWidth()
	{
		return Detail::textZoom * 25 * UI::GetScale();
	}

	/*static*/ float		Slider::GetPixelHeight()
	{
		return Detail::textZoom * 15 * UI::GetScale();
	}

	Slider::Slider()
	{}



	/*virtual override*/ void	Slider::Render()
	{
		const auto&rect = GetRect();
		const float top = rect.y.max;
		const float bottom = rect.y.min;

		const float left = rect.x.min;
		const float right = rect.x.max;

		barBack = Rect<>(left,bottom,right,top);
		SurfaceRenderer::SetColor(float4(0,0,0,0.9));
		SurfaceRenderer::Rasterize(barBack);


		dragRange = Math::FloatRange(left + relativeThumbWidth/2.f,right-relativeThumbWidth/2.f);
		const float barCenter = dragRange.Derelativate(current/max);
		bar = Rect<>(barCenter-relativeThumbWidth/2.f,barBack.y.min,barCenter+relativeThumbWidth/2.f,barBack.y.max);

		if (showMarkers)
		{
			float4 markerColor(1,1,1,0.5f);
			float y0 = rect.y.Derelativate(0.2f);
			float y1 = rect.y.Derelativate(0.8f);
			Rect<> marker;
			marker.y.Set(y0,y1);
			SurfaceRenderer::SetColor(markerColor);
			for (index_t i = 0; i < numMarkers; i++)
			{
				float fi = dragRange.Derelativate((float(i) * markersEvery) / max);
				if (!bar.x.Contains(fi))
				{
					marker.x.SetCenter(fi,2.f/ViewPortWidth());
					SurfaceRenderer::Rasterize(marker);
					//LineRenderer::Rasterize(float3(fi,rect.y.min,0.f), markerColor, float3(fi,y0,0.f),markerColor);
					//LineRenderer::Rasterize(float3(fi,rect.y.max,0.f), markerColor, float3(fi,y1,0.f),markerColor);
				}
			}
		}

		RenderArea(bar,barContainsMouse,dragging,float3(1),Quad<bool>(true));
	}


	/*virtual override*/ WheelRs	Slider::OnMouseWheel(short delta)
	{
		if (this->ContainsMouse())
		{
			if (!dragging)
			{
				//constexpr float speed = -1.f / 5.f;
				current = clamp(current + slideStep * delta,0,max);
				if (onChange)
					onChange(current);
				return WheelRs::Processed;
			}
			return WheelRs::Ignored;
		}
		else
			return WheelRs::NotContained;
		//return up->HandleMouseWheel(delta) || down->HandleMouseWheel(delta);
	}

	/*virtual override*/ void		Slider::OnMouseMove(const TVec2<>&mouseAt, bool clipAreaContainsMouse)
	{
		Super::OnMouseMove(mouseAt,clipAreaContainsMouse);
		outlineContainsMouse = barBack.Contains(mouseAt) && !bar.Contains(mouseAt) && clipAreaContainsMouse;
		lastMouseX = mouseAt.x;
		//slideLeftOnClick = mouseAt.x < bar.x.min;
		if (dragging)
		{
			current = clamp((dragRange.Relativate(mouseAt.x - dragRelative)) * max,0, max);
			changed = true;
		}
		else
		{
			barContainsMouse = bar.Contains(mouseAt);
			dragRelative = mouseAt.x - bar.x.center();
		}
	}


	/*virtual override*/ void Slider::OnMouseUp()
	{
		if (mouseDown)
		{
			bool slideLeftOnClick = lastMouseX < bar.x.min;
			if (slideLeftOnClick)
			{
				current -= slideStep;
				current = std::max(0.f,current);
			}
			else
			{
				current += slideStep;
				current = std::min(max,current);
			}
			changed = true;
		}
		if (changed && onChange)
			onChange(current);
		dragging = false;
		mouseDown = false;
		changed = false;
	}

	/*virtual override*/ PComponent	Slider::OnMouseDown()
	{
		if (!ContainsMouse())
			return PComponent();

		if (outlineContainsMouse)
		{
			mouseDown = true;
		}
		elif (barContainsMouse)
		{
			dragging = true;
		}

		return shared_from_this();
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
		float buttonHeight = TScrollbar::Width * display.GetPixelAspect();
		Rect<> topButton(left,rect.y.max - buttonHeight,right,rect.y.max);
		Rect<> bottomButton(left,rect.y.min,right,rect.y.min + buttonHeight);

		outline = Rect<>(left,bottomButton.y.max + TScrollbar::ContentSpacing,right,topButton.y.min - TScrollbar::ContentSpacing);
		SurfaceRenderer::SetColor(float4(0,0,0,0.9));
		SurfaceRenderer::Rasterize(outline);

		bar = Rect<>(outline.x.min,outline.y.Derelativate(1.f - (viewTop + viewSize) / scrollMax),outline.x.max,outline.y.Derelativate(1.f - (viewTop) / scrollMax));
		RenderArea(bar,barContainsMouse,dragging,float3(1),Quad<bool>(true));

		up->SetLocation(topButton);
		up->Render();
		down->SetLocation(bottomButton);
		down->Render();
	}


	WheelRs	Panel::TScrollbar::OnMouseWheel(short delta)
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

	void		Panel::TScrollbar::OnMouseMove(const TVec2<>&mouseAt, bool clipAreaContainsMouse)
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
			up->OnMouseMove(mouseAt,clipAreaContainsMouse);
			down->OnMouseMove(mouseAt,clipAreaContainsMouse);
		}
		else
			outlineContainsMouse = false;
	}


	void Panel::TScrollbar::OnMouseUp()
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

	PComponent	Panel::TScrollbar::OnMouseDown()
	{
		if (visible)
		{
			PComponent c;
			c = up->OnMouseDown();
			if (c)
				return c;
			c = down->OnMouseDown();
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
		const auto&rect = GetRect();
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


	/*virtual override*/ bool	Panel::UpdateDynamicSize()
	{
		bool rs = Super::UpdateDynamicSize();
		foreach (components,c)
			rs |= (*c)->UpdateDynamicSize();

		if (autoHeight)
		{
			float lowest = std::numeric_limits<float>::max();
			foreach (components,c)
			{
				const auto&bottom = (*c)->GetAnchorConfig().Bottom();
				const auto&top = (*c)->GetAnchorConfig().Top();
				switch (bottom.type)
				{
					case TAnchor::Disabled:
						lowest = std::min(lowest, (*c)->GetRect().y.min);
					break;
					case TAnchor::Fixed:
						if (bottom.references.IsEmpty() && top.type == TAnchor::Disabled)
						{
							float dist = bottom.distanceFromReference;
							if (bottom.distanceType != TAnchor::RelativeToParentComponent)
								lowest = std::min(lowest,GetRect().y.max - (*c)->GetRect().y.GetExtent() - bottom.ToScreenDistance(0.f,false));
						}
					break;
					case TAnchor::Auto:
						if (top.type == TAnchor::Auto)
						{
							lowest = std::min(lowest,GetRect().y.max - (*c)->GetRect().y.GetExtent());
						}
					break;
				}
			}
			if (lowest == std::numeric_limits<float>::max())
				lowest = GetRect().y.max;
			float newY = lowest - autoHeightSpacing;
			float currentHeight = GetRect().y.max - newY;
			bool newVScroll = currentHeight > maxHeight;
			bool changed = vscroll.visible != newVScroll;
			rs |= changed;
			if (newVScroll)
			{
				vscroll.scrollMax = currentHeight;
				vscroll.viewSize = maxHeight;
				float barSize = vmax(maxHeight,0.2f * GetHeight()*currentHeight);
				float overSize = barSize - maxHeight;
				vscroll.scrollMax += overSize;
				vscroll.viewSize += overSize;

				newY = GetRect().y.max - maxHeight;
				{
					auto rect = GetRect();
					rs |= Set(rect.y.min,newY);
					SetRect(rect);
				}
				vscroll.Enable(this,GetRect());
			}
			else
			{
				vscroll.visible = false;
				{
					auto rect = GetRect();
					rs |= Set(rect.y.min,newY);
					SetRect(rect);
				}
			}
		}

		return rs;
	}

	bool			Component::SetEnabled(bool newEnabled)
	{
		if (newEnabled == localState.isEnabled) 
			return false;

		localState.isEnabled = newEnabled; 
		OnEnableChange();
		if (parentState.isEnabled)
		{
			PropagateToChildren(CreatePropagating());
			return true;
		}
		return false;
	}

	void			Component::SetParent(const PWindow&w, const PComponent&c)
	{
		ASSERT__(!c || parent.expired());
		parent = c;
		AcceptParentWindow(w);
		if (c)
		{
			AcceptStateFromParent(c->CreatePropagating());
			InvalidateAutoLayout();
		}
	}


	/*static*/ void				Component::RenderArea(const Rect<>&rect, bool lit, bool flip, const float3&backgroundTone, const Quad<bool>&edges, float lowOpacity/*=1*/, float highOpacity/*=1*/)
	{
		float low = 0.1f;  //0.3;
		float high = lit ? 0.89f : 0.77f;   //0.8f : 0.6;
		high *= high;
		if (flip)
			std::swap(low,high);

		//low = sqrt(low);
		float4 lowColor(low,low,low,lowOpacity);
		Vec::stretch(lowColor.rgb,backgroundTone,lowColor.rgb);
		//high = sqrt(high);
		float4 highColor(high,high,high,highOpacity);
		Vec::stretch(highColor.rgb,backgroundTone,highColor.rgb);

		SurfaceRenderer::Rasterize(float2(rect.x.min,rect.y.min), lowColor, float2(rect.x.max,rect.y.min),lowColor, float2(rect.x.max,rect.y.max), highColor, float2(rect.x.min,rect.y.max),highColor);
		SurfaceRenderer::Flush();
		

		float4 lineLowColor(backgroundTone*0.5f,lowOpacity*0.5f);
		float4 lineHighColor(backgroundTone*0.5f,highOpacity*0.5f);
	
		if (edges.bottom)
			LineRenderer::Rasterize(float3(rect.x.min,rect.y.min,0.f), lineLowColor, float3(rect.x.max,rect.y.min,0.f),lineLowColor);
		if (edges.top)
			LineRenderer::Rasterize(float3(rect.x.max,rect.y.max,0.f), lineHighColor, float3(rect.x.min,rect.y.max,0.f),lineHighColor);
		if (edges.right)
			LineRenderer::Rasterize(float3(rect.x.max,rect.y.min,0.f),lineLowColor, float3(rect.x.max,rect.y.max,0.f), lineHighColor);
		if (edges.left)
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

	void SignalScreenResize()
	{
		Detail::lastClientSpace = display.GetClientSize();
		foreach (Detail::allComponents,c)
			(*c)->OnScreenSizeOrUIScaleChange();
	}





	/*virtual override*/ void	Panel::OnMouseMove(const TVec2<>&mouseAt, bool clipAreaContainsMouse)
	{
		Super::OnMouseMove(mouseAt,clipAreaContainsMouse);

	
		vscroll.OnMouseMove(mouseAt,clipAreaContainsMouse);
		
		TVec2<> moved = mouseAt;
		moved.y += vscroll.GetOffset();

		for (const auto&c : components)
		{
			c->OnMouseMove(moved,ContainsMouse());
		}
	}


	/*virtual override*/ WheelRs	Panel::OnMouseWheel(short delta)
	{
		if (!ContainsMouse())
			return WheelRs::NotContained;

		for (index_t i = components.Count()-1; i < components.Count(); --i)
		{
			WheelRs rs = components[i]->OnMouseWheel(delta);
			if (rs == WheelRs::Processed)
				return rs;
			if (rs == WheelRs::Ignored)
				break;
		}

		if (vscroll.OnMouseWheel(delta) == WheelRs::Processed)
			return WheelRs::Processed;

		return WheelRs::Ignored;
		//Super::HandleMouseWheel(delta);
	}


	/*virtual override*/ PComponent	Panel::OnMouseDown()
	{
		PComponent c = vscroll.OnMouseDown();
		if (c)
			return c;



		if (!ContainsMouse())
			return PComponent();

		for (index_t i = components.Count()-1; i < components.Count(); --i)
		{
			PComponent c = components[i]->OnMouseDown();
			if (c)
				return c;
		}
		return Super::OnMouseDown();
	}


	void			Panel::AnchorBelowPrevious(const PComponent&newComponent, float distanceFromSides /*= 0.015f*/,  float distanceToPrevious/* = 0.015f*/, TAnchor::distanceType_t distanceType/*=TAnchor::RelativeToScreen*/)
	{
		auto cfg = newComponent->GetAnchorConfig();

		if (components.IsNotEmpty())
			cfg.Top().Set(components.last(), distanceToPrevious,distanceType);
		else
			cfg.Top().Set(distanceToPrevious,distanceType);
		
		cfg.Left().Set(distanceFromSides,distanceType);
		cfg.Right().Set(distanceFromSides,distanceType);
		newComponent->SetAnchorConfig(cfg);
		components.Append(newComponent);
		newComponent->SetParent(GetWindow(),shared_from_this());
	}


	void				Box::BaseRender(bool lit, bool flipped)
	{
		RenderArea(GetRect(),lit,flipped,backgroundTone,edges,bottomOpacity,topOpacity);
	}

	static const float CellSize = 0.01f;


	/*virtual*/ bool	Component::UpdateDynamicSize()
	{
		const float h = GetContentHeight();
		if (h < 0)
			return false;

		if (anchorConfig.Top().IsFixed() && anchorConfig.Bottom().IsFixed())
			return false;

		return SetHeight(h);
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



	/*virtual override*/ float			BaseInput::GetContentHeight() const
	{
		textout.PushState();
			textout.Scale(Detail::textZoom);
			const float rs = textout.GetScaledHeight() * 2.5f;
		textout.PopState();
		return rs;
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
		core = GetRect();
		textout.PushState();
		textout.Scale(Detail::textZoom);
		core.Expand(-textout.GetScaledHeight()*0.4f);
		//core.Scale(0.97f);

		if (fillBackground)
			RenderArea(core,false,true,backgroundTone,Quad<bool>(true));

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

		GlobalEvents::uiScaleChanged += []()
		{
			foreach (Detail::visibleWindows,w)
				(*w)->InvalidateAutoLayout();
			SignalScreenResize();
		};

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
			vtx.vertex.Set(0,2);
			vtx.normal.Unset();
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
			if ((*d)->RelayoutRequired())
				(*d)->FullAutoLayout();
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

	float			TextField::GetTotalFontScale() const
	{
		return zoom * Detail::textZoom;
	}

	/*virtual*/ float			TextField::GetIdealWidth() const
	{
		textout.PushState();
		textout.Scale(Detail::textZoom*zoom);
		float w = 0.f;

		foreach (rawText,t)
		{
			float w1 = 0.f;
			foreach (*t,c)
				w1 += CharWidth(*c);
			w = std::max(w,w1);
		}
		w += textout.GetScaledWidth('X') * (padding.left + padding.right);
		textout.PopState();
		return w;
	}

	bool			TextField::UpdateIdealWidth()
	{
		float w = GetIdealWidth();

		bool changed = SetWidth(w);

		return changed;
	}

	void			TextField::SetToIdealWidth()
	{
		if (UpdateIdealWidth())
			InvalidateAutoLayout();
	}




	/*virtual override*/ float		TextField::GetContentHeight() const
	{
		return heightLockedToIdeal ? textHeight : -1;
	}

	/*virtual override*/ bool	TextField::UpdateDynamicSize()
	{
		bool changed = false;
		if (widthLockedToIdeal)
		{
			changed = UpdateIdealWidth();
		}
		return Super::UpdateDynamicSize() || changed;	//queries TextField::ContentHeight()
	}

	void		TextField::ReWrap()
	{
		//const auto hRange = GetRect().x;
		const float width = GetWidth();
		const float center = width/2;
		#ifdef _DEBUG
			wrappedAtWidth = width;
		#endif

		textout.PushState();
		textout.Scale(Detail::textZoom*zoom);

		wrappedText.Clear();
		Array<String>	wrapped;
		for (const String&raw : rawText)
		{
			wrapf(raw,width*0.9f,CharWidth,wrapped);
			wrappedText.MoveAppend(wrapped);
		}
		if (wrappedText.IsEmpty())
			wrappedText.Append();	//at least one empty line
		lineAt.SetSize(wrappedText.Count());
		const float lineHeight = textout.GetScaledHeight();
		const float y = -lineHeight*(padding.top);
		const float leftPadding = textout.GetScaledWidth('X') * padding.left;
		for (index_t i = 0; i < lineAt.Count(); i++)
		{
			lineAt[i] =
				float2(
					
					(	alignCenter ? 
						center - textout.QueryScaledWidth(wrappedText[i])*0.5f
						:
						leftPadding),
				y - lineHeight * (i));
		}
		textHeight = textout.GetScaledHeight() * (float(wrappedText.Count()) + padding.top + padding.bottom);
		textout.PopState();
	}




	/*virtual override*/ void			TextField::Render()
	{
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
		textout.Scale(Detail::textZoom*zoom);

		//bool hovering = !Super::sensitiveToMouse || (enabled && ContainsMouse());

		SurfaceRenderer::Flush();

		//textout.locate(rect.x.min + rect.x.GetExtent()*0.05f,rect.y.max - textout.GetScaledHeight()*1.5f);
		//float lineHeight = textout.GetScaledHeight();
		const float top = GetRect().y.max;
		const float left = GetRect().x.min;

		float4 baseColor;
		if (IsEnabled())
			baseColor = (lit ? float4(1) : float4(0.75,0.75,0.75,1));
		else
			baseColor = float4(0.5,0.5,0.5,1);

		textout.SetColor(baseColor & this->color);
		for (index_t i = 0; i < wrappedText.Count(); i++)
		{
			textout.MoveTo(lineAt[i].x+left,lineAt[i].y+top);
			textout << wrappedText[i];
		}
		textout.PopState();
	}

	/*virtual override*/ float	Image::GetContentHeight() const
	{
		if (texture.IsEmpty())
			return -1;	//no change
		const Resolution3D imageResolution = texture.GetResolution();
		const float2 relativeComponentSize = float2(GetWidth(),GetHeight());
		const float2 viewportSize = ViewPortSizef();
		const float2 pixelComponentSize = relativeComponentSize & viewportSize;
		const float2 pixelImageSize = float2(imageResolution.width,imageResolution.height);

		const float horizontalStretch = pixelImageSize.x / pixelComponentSize.x;



		const float wantPixelImageHeight = pixelComponentSize.x / imageResolution.GetFlatAspect();
		const float wantRelativeHeight = wantPixelImageHeight / float(ViewPortHeight());

		if (isnan(wantRelativeHeight))
		{
			DBG_FATAL__("Relative height is nan");
			return -1;	//no change
		}

		return wantRelativeHeight;
	}


	/*virtual override*/ void			Image::Render()
	{
		if (texture.IsEmpty())
			return;

		bool lit = IsLit();
	
		TMatrix4<> matrix = Matrix<>::eye4;
		
		Resolution3D res = texture.GetResolution();
		float2 resf = float2(res.width,res.height);

		float2 size = float2(GetWidth(),GetHeight());
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

		matrix.w.xy = float2(GetRect().x.center() - size.x*0.5f,GetRect().y.center() - size.y*0.5f);

		display.SetObjectMatrix(matrix);
		texturedQuad.quickSlots.First() = texture;

		display.RenderVertices(texturedQuad);
	}

	/*virtual*/ void	Component::OnMouseMove(const TVec2<>&mouseAt, bool clipAreaContainsMouse)
	{
		containsMouse = clipAreaContainsMouse && GetRect().Contains(mouseAt);
	}


	void			Component::MoveTopTo(float h)
	{
		float delta = h - GetRect().y.max;
		TranslateRect(float2(0,delta));
	}

	bool			Component::FullAutoLayout(const Rect<>&context)
	{
		count_t numRounds = 0;
		bool rs = UpdateDynamicSize();
		for (;;)
		{
			const bool changed0 = ResolveAnchorAxis(0,context);
			const bool changed1 = ResolveAnchorAxis(1,context);
			const bool sizeChanged = UpdateDynamicSize();

			if (!changed0 && !changed1 && !sizeChanged)
				return rs;
			rs = true;
			numRounds++;

			#ifdef _DEBUG
				if (numRounds > 900)
				{
					if (IsDebuggerPresent())
						DebugBreak();
					else
						ShowMessage(__func__ ": Debug: GUI will abort auto-layout shortly. Attach now and debug.");
				}

			#endif

			if (numRounds > 1000)	//might be linear to the number of contained components, depending on order
				return false;
		}

	}


	bool			Component::ResolveAnchorAxis(int axis, const Rect<>&context)
	{
		const Axial<TAnchor>&anchor = anchorConfig.axis[axis];

		if (anchor.min.IsInactive() && anchor.max.IsInactive())
			return false;
		
		auto rect = GetRect();
		TFloatRange<>&a = rect.axis[axis];
		bool rs = false;

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

			SetRect(rect);
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
					highest = std::max(highest,ref->GetRect().axis[axis].max);
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
					lowest = std::min(lowest,ref->GetRect().axis[axis].min);
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

		SetRect(rect);

		return rs;
	}

	Component&			Component::FillHorizontally(float spacing, TAnchor::distanceType_t type /*= TAnchor::RelativeToParentComponent*/)
	{
		auto cfg = anchorConfig;
		cfg.Left().Set(spacing,type);
		cfg.Right().Set(spacing,type);
		SetAnchorConfig(cfg);
		return *this;
	}

	Component&			Component::FillVertically(float spacing, TAnchor::distanceType_t type /*= TAnchor::RelativeToParentComponent*/)
	{
		auto cfg = anchorConfig;
		cfg.Top().Set(spacing,type);
		cfg.Bottom().Set(spacing,type);
		SetAnchorConfig(cfg);
		return *this;
	}

	Component&			Component::AnchorTop(float spacing, TAnchor::distanceType_t type/* = TAnchor::RelativeToParentComponent*/)
	{
		auto cfg = anchorConfig;
		cfg.Top().Set(spacing,type);
		SetAnchorConfig(cfg);
		return *this;
	}


	Component&			Component::SetCenteredWidth(float w, TAnchor::distanceType_t type)
	{
		float extra = 1.f - w;
		if (type == TAnchor::AbsolutePixels)
			extra = float(ViewPortWidth()) - w;
		auto cfg = anchorConfig;
		cfg.Left().Set(extra*0.5f,type);
		cfg.Right().Set(extra*0.5f,type);
		SetAnchorConfig(cfg);
		return *this;
	}



	/*static*/ bool		Component::Set(float&ref, float to)
	{
		ASSERT__(!isnan(to));
		bool changed = !Math::Similar(ref,to);
		ref = to;
		return changed;
	}

	/*static*/ bool		Component::SetCenter(TFloatRange<>&ref, float to)
	{
		float current = ref.center();

		bool changed = !Math::Similar(current,to);
		ref.SetCenter(to);
		return changed;
	}

	#if 0
	/*virtual*/ bool	Component::ResolveAnchors(const Rect<>&context)
	{
		//if (!resolveAnchors)
		//	return false;
		bool rs = false;
		rs |= ResolveAnchorAxis(0,context);
		rs |= ResolveAnchorAxis(1,context);
		return rs;
	}
	#endif /*0*/

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
		float checkBoxWidth = GetTotalFontScale() * textout.GetScaledHeight() / display.GetPixelAspect() * 1.5f;
		return Super::GetIdealWidth() + checkBoxWidth;
	}


	/*virtual override*/ float	Checkbox::GetContentHeight() const
	{
		const float textHeight = Super::GetContentHeight();
		
		const float checkBoxHeight = GetTotalFontScale() * textout.GetScaledHeight() * 1.4f;	//compensate vertical translation (see Render())

		return Math::Max(textHeight,checkBoxHeight);
	}

	/*virtual override*/ void Checkbox::Render()
	{
		bool lit = IsLit();
		//Super::BaseRender(lit,false);
		Super::RenderText(lit);
		SurfaceRenderer::SetColor(lit ? float4(0.7f):float4(0.5f));
		float checkBoxHeight = GetTotalFontScale() * textout.GetScaledHeight();
		float checkBoxWidth = checkBoxHeight / display.GetPixelAspect();
		float top = GetRect().y.center() + checkBoxHeight*0.7f;// Super::extraSpacing * checkBoxHeight;
		TFloatRect box = TFloatRect(GetRect().x.min,top - checkBoxHeight,GetRect().x.min+checkBoxWidth,top);
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


	/*virtual override*/ PWindow	Window::GetWindow()
	{
		return To<Window>::Cast(shared_from_this());
	}



	/**/			TabBar::TabBar()
	{
		SetAutomaticHeight(true);
		fillBackground = false;
		SetAutoHeightSpacing(0.02f);

	}

	
	PButton			TabBar::AddTab(const String&caption)
	{
		//auto&tab = tabs.Append();
		PButton tab(new Button(caption));
		WButton b = tab;
		tab->SetTriggerFunction([this,b]()
		{
			SetActiveTab(b.lock());
		});

		{
			auto mod = tab->ModifyAnchorConfig();
			if (Super::HasComponents())
				mod.Left().Set(Super::GetComponents().Last(),0.01f);
			else
				mod.Left().Set(0);
			mod.Top().Set(0);
		}
		tab->SetVerticalPadding(0.25f);
		tab->LockToIdealWidth();

		ConfigureInactive(tab);

		Super::AddComponent(tab);

		if (Super::CountComponents() == 1)
			SetActiveTab(0,true);
		return tab;
	}

	index_t			TabBar::GetTabIndex(const PButton&tabButton)	const
	{
		if (!tabButton)
			return InvalidIndex;
		return Super::GetComponents().GetIndexOf(tabButton);
	}

	/*virtual*/ void	TabBar::OnTabChange()
	{
		if (onTabChange)
			onTabChange();
	}

	/*static*/ void		TabBar::ConfigureInactive(const PButton&active)
	{
		active->SetAlwaysDown(false);
		active->edges = Quad<bool>(true);
		active->bottomOpacity = 1;
		active->ModifyAnchorConfig().Bottom().Unset();
		active->topOpacity = 0.85f;
		active->bottomOpacity = 0.85f;
		active->SetColor(float4(1,1,1,0.85f));

	}
	/*static*/ void		TabBar::ConfigureActive(const PButton&active)
	{
		active->SetAlwaysDown(true);
		active->ModifyAnchorConfig().Bottom().Set(0);
		active->topOpacity = 1.f;
		active->bottomOpacity = 0;
		active->edges = Quad<bool>(true,false,true,true);
		active->SetColor(float4(1));
	}


	void			TabBar::SetActiveTab(index_t tabIndex, bool triggerCallback)
	{
		if (tabIndex == activeIndex)
			return;
		ASSERT__(tabIndex < CountComponents());
		if (active)
		{
			ConfigureInactive(active);
		}

		activeIndex = tabIndex;
		active = To<Button>::Cast( GetComponents()[tabIndex] );
		{
			ConfigureActive(active);
		}
		if (triggerCallback)
			OnTabChange();
	}



	void			TabCollection::Setup()
	{
		if (tabBar)
			return;
		this->fillBackground = false;
		tabBar.reset(new TabBar());
		{
			auto mod = tabBar->ModifyAnchorConfig();
			mod.Left().Set(0);
			mod.Right().Set(0);
			mod.Top().Set(0);
		}
		tabBar->SetOnTabChange([this]()
		{
			if (tabBar->GetActiveTabIndex() != InvalidIndex)
				this->SetActiveTab(tabBar->GetActiveTabIndex(),true);
		});

		Super::AddComponent(tabBar);
	}


	PPanel			TabCollection::AddTab(const String&caption)
	{
		Setup();

		auto&tab = tabs.Append();
		tab.panel.reset(new Panel());
		tab.panel->edges.top = false;
		{
			auto mod = tab.panel->ModifyAnchorConfig();
			mod.Left().Set(0);
			mod.Right().Set(0);
			mod.Top().Set(tabBar,0);
			mod.Bottom().Set(0);
		}
		tab.button = tabBar->AddTab(caption);
		return tab.panel;
	}

	index_t			TabCollection::GetTabIndex(const PPanel&tabPanel)	const
	{
		if (!tabPanel)
			return InvalidIndex;
		for (index_t i = 0; i < tabs.Count(); i++)
			if (tabs[i].panel == tabPanel)
				return i;
		return InvalidIndex;
	}

	void			TabCollection::SetCaptionOf(const PPanel&tabPanel, const String&caption)
	{
		if (!tabPanel)
			return;
		index_t at = GetTabIndex(tabPanel);
		ASSERT__(at != InvalidIndex);
		tabs[at].button->SetText(caption);
	}

	const String&	TabCollection::GetCaptionOf(const PPanel&tabPanel)	const
	{
		index_t at = GetTabIndex(tabPanel);
		ASSERT__(at != InvalidIndex);
		return tabs[at].button->GetText().First();
	}

	PPanel			TabCollection::GetActiveTab() const
	{
		return active.panel;
	}

	const String&	TabCollection::GetActiveCaption() const
	{
		static const String empty;
		if (active.button)
			return active.button->GetText().First();
		return empty;
	}

	void			TabCollection::SetOnTabChange(const FOnTabChange&onChange)
	{
		onTabChange = onChange;
	}

	bool			TabCollection::ClearTabs()
	{
		if (tabs.IsEmpty())
			return false;
		Super::RemoveComponent(active.panel);
		tabBar->ClearTabs();
		tabs.Clear();
		activeIndex = InvalidIndex;
		active = TTab();
		OnTabChange();
		return true;
	}

	/*virtual*/ void	TabCollection::OnTabChange()
	{
		if (onTabChange)
			onTabChange();
	}


	void			TabCollection::SetActiveTab(index_t tabIndex, bool triggerCallback)
	{
		if (tabIndex == activeIndex)
			return;
		ASSERT__(tabIndex < tabs.Count());

		if (tabBar)
			tabBar->SetActiveTab(tabs[tabIndex].button,false);
		Super::RemoveComponent(active.panel);
		activeIndex = tabIndex;
		active = tabs[tabIndex];
		Super::AddComponent(active.panel);
		if (triggerCallback)
			OnTabChange();
	}



	void	PlotStack::Plot::AddNode(const index_t x, const float y)
	{
		PPlotStack s = stack.lock();

		if (start == InvalidIndex)
			start = x;
		else
		{
			ASSERT_GREATER_OR_EQUAL__(x,start + data.Count());
			float yOff = data.Last().yRaw;

			const count_t intermediate = x - start - data.Count();

			const index_t from = data.Count();
			for (index_t xi = 0; xi < intermediate; xi++)
			{
				float fi = float(xi + 1) / float(intermediate + 1);
				const float y0 = yOff * (1.f - fi) + y * (fi);
				const float sampled = s->SampleStackedHeight(xi + from);
				if (s->CountPlots() == 1 && sampled != 0)
				{
					const float sampled = s->SampleStackedHeight(xi + from);
					FATAL__("Got "+String(sampled));
				}
				data << TPoint{y0,sampled,sampled+y0};
			}
		}

		const float sampled = s->SampleStackedHeight(x);
		const float yTop = y + sampled;
		data << TPoint{y,sampled,yTop};

		s->xRange.Include(x);
		s->yRange.Include(yTop);

		changed = true;
	}


	void	PlotStack::Plot::RenderSurfaces() const
	{
		surface.Rasterize();
	}

	void	PlotStack::Plot::RenderLines() const
	{
		lines.Rasterize();
	}

	/*virtual override*/ void	PlotStack::Render()
	{
		Super::Render();
		const Rect<> plotRect = GetPlotRect();
		foreach (graphs,g)
			if (g->changed)
				g->Replot(GetRect(),plotRect);
		foreach (graphs,g)
			g->RenderSurfaces();
		foreach (graphs,g)
			g->RenderLines();
	}

	/*virtual override*/ void	PlotStack::OnRectChange()
	{
		Super::OnRectChange();
		const Rect<> plotRect = GetPlotRect();
		foreach (graphs,g)
			g->Replot(GetRect(),plotRect);
	}

	Rect<>			PlotStack::GetPlotRect() const
	{
		return Rect<>(xRange.start,0.f,xRange.end-1,yRange.max).ExpandRelatively(0.05f);
	}



	float	PlotStack::SampleStackedHeight(index_t x) const
	{
		if (!xRange.Contains(x))
			return 0;

		float rs = 0;
		foreach (graphs,g)
			rs += g->Sample(x);

		return rs;
	}


	float	PlotStack::Plot::Sample(index_t x) const
	{
		if (x < start)
			return 0;
		x -= start;

		if (x >= data.Count())
			return 0;
		return data[x].yRaw;
	}


	void	PlotStack::Plot::MakeVertex(const float x, const float y)
	{
		const float2	p{x,y};
		lines.AddVertex(p,float4(color,1.f));
		surface.AddVertex(p,float4(color,0.5f));
	}


	void	PlotStack::Plot::Replot(const Rect<>&componentRect, const Rect<>&dataRect)
	{
		changed = false;


		surface.Clear();
		lines.Clear();


		for (index_t i = 0; i < data.Count(); i++)
		{
			float rX = dataRect.x.Relativate(i);
			ASSERT_IS_CONSTRAINED__(rX,0,1);
			MakeVertex( 
				componentRect.x.Derelativate( rX ),
				componentRect.y.Derelativate( dataRect.y.Relativate(data[i].yMin) )
				);

			MakeVertex( 
				componentRect.x.Derelativate( rX ),
				componentRect.y.Derelativate( dataRect.y.Relativate(data[i].yMax) )
				);
		}

		for (index_t i = 1; i < data.Count(); i++)
		{

			surface.AddQuad(
				((i-1)*2+1),
				((i-1)*2),
				((i)*2),
				((i)*2+1));
			lines.AddLine(
				((i-1)*2+1),
				((i)*2+1));
		}
		lines.AddLine(0,1);
		lines.AddLine(data.Count()*2-1,data.Count()*2-2);

		lines.Bake();
		surface.Bake();
	}




}

#endif

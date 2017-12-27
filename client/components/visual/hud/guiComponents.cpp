#ifdef HAS_WINDOW

#include <global_root.h>
#include "guiComponents.h"
#include <engine/display.h>
#include "../../../DX11/texture_font.h"
#include <container/buffer.h>
#include <engine/aspect.h>
#include <objects.h>
#include "../../clientEvents.h"
#include "componentDetails.h"

namespace DX11 = Engine::DX11; 

extern Engine::Textout<DX11::TextureFont>	textout;
extern Engine::Display<Engine::DirectX11>	display;


namespace HUD 
{

namespace Component
{

	/*static*/ const Distance Distance::Unknown = Distance(NAN,Type::Placeholder);
	/*static*/ const Distance Distance::Disabled = Distance(NAN,Type::Disabled);


	void	Distance::VerifyIntegrity() const
	{
		if (IsDefined())
		{
			ASSERT__(!isnan(value) && !isinf(value));
		}
		else
			ASSERT__(isnan(value));
	}


	bool	Distance::IsWellDefined() const
	{
		return !isnan(value) && !isinf(value) && value >= 0;
	}

	bool	Distance::IsWellDefinedNonZero() const
	{
		return !isnan(value) && !isinf(value) && value > 0;
	}


	void		Anchor::AddReference(const PComponent&c)
	{
		ASSERT__(enabled);
		references << c;
	}

	void		Anchor::VerifyIntegrity(const FVerification&check, const AnchorAxis&axis, bool isUpper) const
	{
		distanceFromReference.VerifyIntegrity();
		ASSERT2__(distanceFromReference.IsWellDefined(),axis.ToString(),isUpper);
		ASSERT2__(enabled || references.IsEmpty(),axis.ToString(),isUpper);
		if (check)
		{
			foreach (references,ref)
			{
				auto locked = ref->lock();
				ASSERT2__(locked,axis.ToString(),isUpper);
				check(locked);
			}
		}
	}


	void		AnchorConfig::Axis::VerifyIntegrity(const FVerification&v, bool allowUnknownExtent) const
	{
		lower.VerifyIntegrity(v,myAxis,false);
		upper.VerifyIntegrity(v,myAxis,true);
		extent.VerifyIntegrity();
		if (isAutoCentering || lower.IsInactive() || upper.IsInactive())
		{
			ASSERT__(extent.IsDefined() || (allowUnknownExtent && extent.IsPlaceholder()));
		}
		//ASSERT1__(lower.IsFixed() || upper.IsFixed(),myAxis.ToString());
		
		if (isAutoCentering)
			ASSERT__(BothAnchorsAreOff());
		
		if (lower.IsEnabled() && upper.IsEnabled())
			ASSERT_EQUAL1__(extent, Distance::Disabled,myAxis.ToString());
	}

	void		AnchorConfig::Axis::SetFreeFloating(const Distance&extent /*= Distance::Unknown*/)
	{
		this->extent = extent;
		lower.Disable();
		upper.Disable();
		isAutoCentering = false;
		VerifyIntegrity(FVerification(),true);
	}

	bool		AnchorConfig::Axis::SetExtent(const Distance&d)
	{
		ASSERT__(!Stretches());
		//ASSERT__(lower.IsFixed() || upper.IsFixed());
		if (extent == d)
			return false;
		extent = d;
		VerifyIntegrity(FVerification(),true);
		return false;
	}
	void		AnchorConfig::Axis::AnchorLower(const Distance&extent, const Distance&offset/* = Distance()*/)
	{
		lower.Set(offset);
		upper.Disable();
		isAutoCentering = false;
		this->extent = extent;
		VerifyIntegrity(FVerification(),true);
	}

	void		AnchorConfig::Axis::AnchorLower(const Distance&extent, const PComponent&reference, const Distance&lowerOffset/* = Distance()*/)
	{
		lower.Set(reference,lowerOffset);
		upper.Disable();
		isAutoCentering = false;
		this->extent = extent;
		VerifyIntegrity(FVerification(),true);
	}
	
	void		AnchorConfig::Axis::AnchorLowerUnknown(const Distance&lowerOffset/* = Distance()*/)
	{
		AnchorLower(Distance::Unknown,lowerOffset);
	}



	void		AnchorConfig::Axis::AnchorUpper(const Distance&extent, const PComponent&reference, const Distance&upperOffset/* = Distance()*/)
	{
		upper.Set(reference, upperOffset);
		lower.Disable();
		isAutoCentering = false;
		this->extent = extent;
		VerifyIntegrity(FVerification(),true);
	}


	void		AnchorConfig::Axis::AnchorUpper(const Distance&extent, const Distance&offset /*= Distance()*/)
	{
		upper.Set(offset);
		lower.Disable();
		isAutoCentering = false;
		this->extent = extent;
		VerifyIntegrity(FVerification(),true);
	}
	
	void		AnchorConfig::Axis::AnchorUpperUnknown(const Distance&upperOffset /*= Distance()*/)
	{
		AnchorUpper(Distance::Unknown,upperOffset);
	}

	void		AnchorConfig::Axis::Fill(const Distance&lowerOffset/* = Distance()*/, const Distance&upperOffset/* = Distance()*/)
	{
		upper.Set(upperOffset);
		lower.Set(lowerOffset);
		isAutoCentering = false;
		extent = Distance::Disabled;
		VerifyIntegrity(FVerification(),true);
	}

	void			AnchorConfig::SetFreeFloating(const Distance&width /*= Distance::Unknown*/,const Distance&height /*= Distance::Unknown*/)
	{
		GetHAxis().SetFreeFloating(width);
		GetVAxis().SetFreeFloating(height);
	}


	void			ComponentLocation::SetRect(const M::Rect<>&rect)
	{
		const bool hChange = rect.x != this->rect.x;
		const bool vChange = rect.y != this->rect.y;
		if (!hChange && !vChange)
			return;

		rect.AssertIsNotNaN();

		const bool widthChange = !M::Similar(rect.x.GetExtent(),this->rect.x.GetExtent());
		const bool heightChange = !M::Similar(rect.y.GetExtent(),this->rect.y.GetExtent());
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

	AnchorModifier&	AnchorModifier::Top(const Distance&upperOffset /*= Distance()*/, const Distance&height /*= Distance::Unknown*/)
	{
		GetVAxis().AnchorUpper(height,upperOffset);
		return *this;
	}

	AnchorModifier&	AnchorModifier::Bottom(const Distance&lowerOffset /*= Distance()*/, const Distance&height /*= Distance::Unknown*/)
	{
		GetVAxis().AnchorLower(height,lowerOffset);
		return *this;
	}

	AnchorModifier&	AnchorModifier::Left(const Distance&leftOffset /*= Distance()*/, const Distance&width /*= Distance::Unknown*/)
	{
		GetHAxis().AnchorLower(width,leftOffset);
		return *this;
	}

	AnchorModifier&	AnchorModifier::Right(const Distance&rightOffset /*= Distance()*/, const Distance&width /*= Distance::Unknown*/)
	{
		GetHAxis().AnchorUpper(width,rightOffset);
		return *this;
	}


	AnchorModifier&	AnchorModifier::Beneath(const PComponent&p, const Distance&upperOffset /*= Distance()*/, const Distance&height /*= Distance::Unknown*/)
	{
		ASSERT__(p.get() != this->parent);
		GetVAxis().lower.Disable();
		GetVAxis().upper.Set(p,upperOffset);
		GetVAxis().extent = height;
		GetVAxis().isAutoCentering = false;
		return *this;
	}

	AnchorModifier&	AnchorModifier::Above(const PComponent&p, const Distance&lowerOffset /*= Distance()*/, const Distance&height /*= Distance::Unknown*/)
	{
		ASSERT__(p.get() != this->parent);
		GetVAxis().upper.Disable();
		GetVAxis().lower.Set(p,lowerOffset);
		GetVAxis().extent = height;
		GetVAxis().isAutoCentering = false;
		return *this;
	}

	AnchorModifier&	AnchorModifier::RightOf(const PComponent&p, const Distance&leftOffset /*= Distance()*/, const Distance&width /*= Distance::Unknown*/)
	{
		ASSERT__(p.get() != this->parent);
		GetHAxis().upper.Disable();
		GetHAxis().lower.Set(p,leftOffset);
		GetHAxis().extent = width;
		GetHAxis().isAutoCentering = false;
		return *this;
	}

	AnchorModifier&	AnchorModifier::LeftOf(const PComponent&p, const Distance&rightOffset /*= Distance()*/, const Distance&width /*= Distance::Unknown*/)
	{
		ASSERT__(p.get() != this->parent);
		GetHAxis().lower.Disable();
		GetHAxis().upper.Set(p,rightOffset);
		GetHAxis().extent = width;
		GetHAxis().isAutoCentering = false;
		return *this;
	}


	AnchorModifier&	AnchorModifier::RightOfLastOf(const PPanel&p, const Distance&leftOffset /*= Distance()*/, const Distance&width /*= Distance::Unknown*/)
	{
		GetHAxis().lower.SetLastOf(p,leftOffset,parent);
		GetHAxis().upper.Disable();
		GetHAxis().extent = width;
		GetHAxis().isAutoCentering = false;
		return *this;
	}

	AnchorModifier&	AnchorModifier::LeftOfLastOf(const PPanel&p, const Distance&rightOffset /*= Distance()*/, const Distance&width /*= Distance::Unknown*/)
	{
		GetHAxis().upper.SetLastOf(p,rightOffset,parent);
		GetHAxis().lower.Disable();
		GetHAxis().extent = width;
		GetHAxis().isAutoCentering = false;
		return *this;
	}

	AnchorModifier&	AnchorModifier::BeneathLastOf(const PPanel&p, const Distance&upperOffset /*= Distance()*/, const Distance&height /*= Distance::Unknown*/)
	{
		GetVAxis().upper.SetLastOf(p,upperOffset,parent);
		GetVAxis().lower.Disable();
		GetVAxis().extent = height;
		GetVAxis().isAutoCentering = false;
		return *this;
	}

	AnchorModifier&	AnchorModifier::AboveLastOf(const PPanel&p, const Distance&lowerOffset /*= Distance()*/, const Distance&height /*= Distance::Unknown*/)
	{
		GetVAxis().lower.SetLastOf(p,lowerOffset,parent);
		GetVAxis().upper.Disable();
		GetVAxis().extent = height;
		GetVAxis().isAutoCentering = false;
		return *this;
	}

	AnchorModifier&	AnchorModifier::RightOfAllOf(const PPanel&p, const Distance&leftOffset /*= Distance()*/, const Distance&width /*= Distance::Unknown*/)
	{
		GetHAxis().lower.SetAllOf(p,leftOffset,parent);
		GetHAxis().upper.Disable();
		GetHAxis().extent = width;
		GetHAxis().isAutoCentering = false;
		return *this;
	}

	AnchorModifier&	AnchorModifier::LeftOfAllOf(const PPanel&p, const Distance&rightOffset /*= Distance()*/, const Distance&width /*= Distance::Unknown*/)
	{
		GetHAxis().upper.SetAllOf(p,rightOffset,parent);
		GetHAxis().lower.Disable();
		GetHAxis().extent = width;
		GetHAxis().isAutoCentering = false;
		return *this;
	}

	AnchorModifier&	AnchorModifier::BeneathAllOf(const PPanel&p, const Distance&upperOffset /*= Distance()*/, const Distance&height /*= Distance::Unknown*/)
	{
		GetVAxis().upper.SetAllOf(p,upperOffset,parent);
		GetVAxis().lower.Disable();
		GetVAxis().extent = height;
		GetVAxis().isAutoCentering = false;
		return *this;
	}

	AnchorModifier&	AnchorModifier::AboveAllOf(const PPanel&p, const Distance&lowerOffset /*= Distance()*/, const Distance&height /*= Distance::Unknown*/)
	{
		GetVAxis().lower.SetAllOf(p,lowerOffset,parent);
		GetVAxis().upper.Disable();
		GetVAxis().extent = height;
		GetVAxis().isAutoCentering = false;
		return *this;
	}


	AnchorModifier&	AnchorModifier::RightOfSomeOf(const PPanel&p, const FSelector&sel, const Distance&leftOffset /*= Distance()*/, const Distance&width /*= Distance::Unknown*/)
	{
		GetHAxis().lower.SetSomeOf(p,leftOffset,sel,parent);
		GetHAxis().upper.Disable();
		GetHAxis().extent = width;
		GetHAxis().isAutoCentering = false;
		return *this;
	}

	AnchorModifier&	AnchorModifier::LeftOfSomeOf(const PPanel&p, const FSelector&sel, const Distance&rightOffset /*= Distance()*/, const Distance&width /*= Distance::Unknown*/)
	{
		GetHAxis().upper.SetSomeOf(p,rightOffset,sel,parent);
		GetHAxis().lower.Disable();
		GetHAxis().extent = width;
		GetHAxis().isAutoCentering = false;
		return *this;
	}

	AnchorModifier&	AnchorModifier::BeneathSomeOf(const PPanel&p, const FSelector&sel, const Distance&upperOffset /*= Distance()*/, const Distance&height /*= Distance::Unknown*/)
	{
		GetVAxis().upper.SetSomeOf(p,upperOffset,sel,parent);
		GetVAxis().lower.Disable();
		GetVAxis().extent = height;
		GetVAxis().isAutoCentering = false;
		return *this;
	}

	AnchorModifier&	AnchorModifier::AboveSomeOf(const PPanel&p, const FSelector&sel, const Distance&lowerOffset /*= Distance()*/, const Distance&height /*= Distance::Unknown*/)
	{
		GetVAxis().lower.SetSomeOf(p,lowerOffset,sel,parent);
		GetVAxis().upper.Disable();
		GetVAxis().extent = height;
		GetVAxis().isAutoCentering = false;
		return *this;
	}



	AnchorModifier&	AnchorModifier::FillHorizontally()
	{
		GetHAxis().Fill();
		return *this;
	}

	AnchorModifier&	AnchorModifier::FillHorizontally(const Distance&leftOffset, const Distance&rightOffset)
	{
		GetHAxis().Fill(leftOffset, rightOffset);
		return *this;
	}

	AnchorModifier&	AnchorModifier::FillHBetween(const PComponent&leftEdge, const PComponent&rightEdge)
	{
		GetHAxis().FillBetween(leftEdge,rightEdge);
		return *this;
	}

	void AnchorModifier::Axis::FillBetween(const PComponent&lowerC, const PComponent&upperC)
	{
		lower.Set(lowerC,0);
		upper.Set(upperC,0);
		isAutoCentering = false;
		extent = Distance::Disabled;
	}

	AnchorModifier&	AnchorModifier::CenterVertically(const Distance&height /*= Distance::Unknown*/)
	{
		GetVAxis().SetAutoCenter(height);
		return *this;
	}


	AnchorModifier&	AnchorModifier::CenterHorizontally(const Distance&width /*= Distance::Unknown*/)
	{
		GetHAxis().SetAutoCenter(width);
		return *this;
	}


	AnchorModifier&	AnchorModifier::FillVertically()
	{
		GetVAxis().Fill();
		return *this;
	}

	AnchorModifier&	AnchorModifier::FillVertically(const Distance&lowerOffset, const Distance&upperOffset)
	{
		GetVAxis().Fill(lowerOffset, upperOffset);
		return *this;
	}




	/*virtual*/ void	Component::Render()
	{}


	/*virtual*/ void	Component::SetLocation(const M::Rect<>&loc)
	{
		SetRect(loc);
		anchorConfig.GetHAxis().UpdateExtentIfEnabled( Distance::GameWindow( GetWidth() ));
		anchorConfig.GetVAxis().UpdateExtentIfEnabled( Distance::GameWindow( GetHeight() ));
	}

	/*virtual override*/ void Component::OnRectChange()
	{
		//anchorConfig.GetHAxis().UpdateExtentIfEnabled( Distance::GameWindow( GetWidth() ));
		//anchorConfig.GetVAxis().UpdateExtentIfEnabled( Distance::GameWindow( GetHeight() ));
		anchorConfig.VerifyIntegrity(FVerification(),GetWindow() != nullptr);
		InvalidateAutoLayout();
	}

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

	/*virtual*/ void	Component::VerifyAnchorIntegrity() const
	{
		anchorConfig.VerifyIntegrity(
		[this](const PComponent&c)
		{
			ASSERT__(c->GetWindow() == nullptr || c->GetWindow() == this->GetWindow());
		},GetWindow() == nullptr);
	}

	/*virtual*/ void	Component::SetAnchorConfig(const AnchorConfig&cfg)
	{
		anchorConfig = cfg;
		if (anchorConfig.HasExtentPlaceholders())
		{
			UpdateDynamicSize();
			if (anchorConfig.HasExtentPlaceholders())
			{
				anchorConfig.GetHAxis().UpdateExtentIfEnabled(Distance::GameWindow( GetWidth() ));
				anchorConfig.GetVAxis().UpdateExtentIfEnabled(Distance::GameWindow( GetHeight() ));

			}
			ASSERT__(!anchorConfig.HasExtentPlaceholders());
		}
//		VerifyAnchorIntegrity(); 
		InvalidateAutoLayout();
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
	static float ViewPortAspect()
	{
		return float(ViewPortWidth()) / ViewPortHeight();
	}
	

	static float2 ViewPortSizef()
	{
		return float2(ViewPortWidth(),ViewPortHeight());
	}




	void			Anchor::SetSomeOf(const PPanel&p, const Distance& distance, const std::function<bool(const PComponent&)>&callback, const Component*mustNotInclude)
	{
		if (!p || !p->HasComponents())
			return;
		enabled = true;
		distanceFromReference = distance;
		references.Clear();
		foreach (p->GetReferences(),c)
		{
			ASSERT__(c->get() != mustNotInclude);
			if (callback(*c))
				references.Append(*c);
		}
	}


	void			Anchor::SetAllOf(const PPanel&p, const Distance& distance, const Component*mustNotInclude)
	{
		if (!p || !p->HasComponents())
			return;
		enabled = true;
		distanceFromReference = distance;
		references.Clear();
		foreach (p->GetReferences(),c)
		{
			ASSERT__(c->get() != mustNotInclude);
			references.Append(*c);
		}
	}

	void			Anchor::SetLastOf(const PPanel&p, const Distance& distance, const Component*mustNotInclude)
	{
		if (!p)
			return;
		enabled = true;
		distanceFromReference = distance;
		references.Clear();
		if (p->HasComponents())
		{
			ASSERT__(p->GetReferences().Last().get() != mustNotInclude);
			references.Append(p->GetReferences().Last());
		}
	}


	float			Distance::ToScreenDistance(float parentSize, AnchorAxis myAxis) const
	{
		switch (type)
		{
			case Type::RelativeToParentComponent:
				return value * parentSize;
			case Type::RelativeToGameWindow:
				return value;
			case Type::AbsolutePixels:
				return value / float(myAxis == AnchorAxis::Horizontal ? ViewPortWidth() : ViewPortHeight());
			case Type::Disabled:
				return NAN;
		}
		FATAL__("Unexpected distance type");
		return 0.f;

	}

	float		AnchorConfig::Axis::GetMinimumEdge(float parentUpperEdge, const M::TFloatRange<>&rectRange) const
	{
		if (isAutoCentering)
			return parentUpperEdge - rectRange.GetExtent();

		if (lower.IsInactive())
			return rectRange.min;

		if (lower.GetReferences().IsEmpty() && upper.IsInactive() && !lower.GetOffset().RequiresFixedParentSize())
			return parentUpperEdge - rectRange.GetExtent() - lower.GetOffset().ToScreenDistance(0.f,myAxis);

		return std::numeric_limits<float>::max();
	}



	/*virtual*/ bool		Panel::AddComponent(const PComponent&c)
	{
		if (!c)
			return false;
		if (components.Contains(c))
			return false;

		components << c;
		c->SetParent( GetWindow(), shared_from_this());
		c->VerifyAnchorIntegrity();
		VerifyAnchorIntegrity();
		return true;
	}

	/*virtual override*/ bool	Panel::ResolveAnchorAxis(AnchorAxis axis, const M::Rect<>&context)
	{
		bool rs = Super::ResolveAnchorAxis(axis,context); 
		M::Rect<> childSpace = GetRect();
		if (vscroll.visible)
			childSpace.x.max = M::Max(childSpace.x.min, childSpace.x.max - GetVisibleScrollbarContentExpansion());
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


	/*static*/ float		Panel::TScrollbar::GetPixelWidth()
	{
		return Detail::textZoom * 15 * UI::GetScale();
	}
	
	/*static*/ float		Panel::TScrollbar::GetScreenWidth()
	{
		return float(GetPixelWidth()) / ViewPortWidth();
	}

	/*static*/ float		Panel::TScrollbar::GetButtonScreenHeight()
	{
		return float(GetPixelWidth()) / ViewPortHeight();
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

		barBack = M::Rect<>(left,bottom,right,top);
		SurfaceRenderer::SetColor(float4(0,0,0,0.9));
		SurfaceRenderer::Rasterize(barBack);


		dragRange = M::FloatRange(left + relativeThumbWidth/2.f,right-relativeThumbWidth/2.f);
		const float barCenter = dragRange.Derelativate(current/max);
		bar = M::Rect<>(barCenter-relativeThumbWidth/2.f,barBack.y.min,barCenter+relativeThumbWidth/2.f,barBack.y.max);

		if (showMarkers)
		{
			float4 markerColor(1,1,1,0.5f);
			float y0 = rect.y.Derelativate(0.2f);
			float y1 = rect.y.Derelativate(0.8f);
			M::Rect<> marker;
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

		RenderArea(bar,barContainsMouse,dragging,float3(1));
		RenderBorder(bar,float3(1),true);
	}


	/*virtual override*/ WheelRs	Slider::OnMouseWheel(short delta)
	{
		if (this->ContainsMouse())
		{
			if (!dragging)
			{
				//constexpr float speed = -1.f / 5.f;
				current = M::Clamp(current + slideStep * delta,0,max);
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

	/*virtual override*/ void		Slider::OnMouseMove(const M::TVec2<>&mouseAt, bool clipAreaContainsMouse)
	{
		Super::OnMouseMove(mouseAt,clipAreaContainsMouse);
		outlineContainsMouse = barBack.Contains(mouseAt) && !bar.Contains(mouseAt) && clipAreaContainsMouse;
		lastMouseX = mouseAt.x;
		//slideLeftOnClick = mouseAt.x < bar.x.min;
		if (dragging)
		{
			current = M::Clamp((dragRange.Relativate(mouseAt.x - dragRelative)) * max,0, max);
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




	void	Panel::TScrollbar::Render(const M::Rect<>&rect)
	{
		
		targetViewTop = M::Min(targetViewTop,scrollMax - viewSize);
		float speed = M::Max(1.f,fabs(targetViewTop - viewTop))*2.f;
		if (targetViewTop > viewTop)
		{
			viewTop = M::Min(targetViewTop, viewTop + Engine::timing.delta*speed);
		}
		elif (targetViewTop < viewTop)
		{
			viewTop = M::Max(targetViewTop, viewTop - Engine::timing.delta*speed);
		}


		const float left = rect.x.max - TScrollbar::GetScreenWidth() - ContentSpacing * 0.5f;
		const float right = rect.x.max - ContentSpacing * 0.5f;
		const float top = rect.y.max - ContentSpacing * 0.5f;
		const float bottom = rect.y.min + ContentSpacing * 0.5f;
		//const float aspect = display.GetPixelAspect();
		const float buttonHeight = TScrollbar::GetButtonScreenHeight();
		M::Rect<> topButton(left,top - buttonHeight,right,top);
		M::Rect<> bottomButton(left,bottom,right,bottom + buttonHeight);

		outline = M::Rect<>(left,bottomButton.y.max + TScrollbar::ContentSpacing,right,topButton.y.min - TScrollbar::ContentSpacing);
		SurfaceRenderer::SetColor(float4(0,0,0,0.9));
		SurfaceRenderer::Rasterize(outline);

		bar = M::Rect<>(outline.x.min,outline.y.Derelativate(1.f - (viewTop + viewSize) / scrollMax),outline.x.max,outline.y.Derelativate(1.f - (viewTop) / scrollMax));
		RenderArea(bar,barContainsMouse,dragging,float3(1));
		RenderBorder(bar,float3(1),true);

		up->SetLocation(topButton);
		up->Render();
		down->SetLocation(bottomButton);
		down->Render();
	}


	void	Panel::TScrollbar::ScrollToBottom()
	{
		targetViewTop = scrollMax - viewSize;
	}

	void	Panel::TScrollbar::ScrollToTop()
	{
		targetViewTop = 0;
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
				viewTop = M::Clamp(viewTop + speed * viewSize* delta,0,scrollMax - viewSize);
				targetViewTop = M::Clamp(targetViewTop + speed * viewSize* delta,0,scrollMax - viewSize);
				return WheelRs::Processed;
			}
			return WheelRs::Ignored;
		}
		//return up->HandleMouseWheel(delta) || down->HandleMouseWheel(delta);
	}

	void		Panel::TScrollbar::OnMouseMove(const M::TVec2<>&mouseAt, bool clipAreaContainsMouse)
	{
		if (visible)
		{
			outlineContainsMouse = outline.Contains(mouseAt) && !bar.Contains(mouseAt) && clipAreaContainsMouse;
			scrollDownOnClick = mouseAt.y < bar.y.min;
			if (dragging)
			{
				viewTop = targetViewTop = M::Clamp((1.f-outline.y.Relativate(mouseAt.y - dragRelative)) * scrollMax,0, scrollMax-viewSize);
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
				targetViewTop += M::Max(viewSize, scrollMax /10.f);
				targetViewTop = M::Min(targetViewTop,scrollMax - viewSize);	
			}
			else
			{
				targetViewTop -= M::Max(viewSize, scrollMax /10.f);
				targetViewTop = M::Max(targetViewTop,0);	
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

	void	Panel::TScrollbar::Enable(Panel*p,const M::Rect<>&rect)
	{
		owner = p;
		if (!up)
		{
			up.reset(new ScrollButton(ScrollButton::Up,[this,p]()
			{
				targetViewTop -= viewSize;
				targetViewTop = M::Max(targetViewTop,0);	
			}));

			down.reset(new ScrollButton(ScrollButton::Bottom,[this,p]()
			{
				targetViewTop += viewSize;
				targetViewTop = M::Min(targetViewTop,scrollMax - viewSize);
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

	/*virtual override*/ void Button::Render()
	{
		bool lit = IsLit();
		bool flipped = mousePressedComponent.get() == this;
		Super::BaseRender((lit && !flipped)  || alwaysDown,(lit && flipped) || (alwaysDown && flipWhenAlwaysDown));
		Super::RenderText(lit || alwaysDown);
	}


	/*static*/ void		ScrollButton::RenderArrow(const M::Rect<>&r, direction_t dir, bool lit)
	{
		SurfaceRenderer::SetColor(float4(1,1,1,lit?0.75f:0.5f));

		switch (dir)
		{
			case Up:
				SurfaceRenderer::Rasterize(float2(r.x.center(),r.y.Derelativate(0.75f)),float2(r.x.Derelativate(0.25f),r.y.Derelativate(0.25f)),float2(r.x.Derelativate(0.75f),r.y.Derelativate(0.25f)));
			break;
			case Bottom:
				SurfaceRenderer::Rasterize(float2(r.x.center(),r.y.Derelativate(0.25f)),float2(r.x.Derelativate(0.75f),r.y.Derelativate(0.75f)),float2(r.x.Derelativate(0.25f),r.y.Derelativate(0.75f)));
			break;

			default:
				FATAL__("Unexpected direction parameter");
		}

	}


	/*virtual override*/ void	Panel::Render()
	{
		const auto&rect = GetRect();
		if (!rect.IsValid())
			return;
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


	/*virtual override*/ bool	Panel::UpdateDynamicSize()
	{
		bool rs = Super::UpdateDynamicSize();
		foreach (components,c)
			rs |= (*c)->UpdateDynamicSize();

		float lowest = std::numeric_limits<float>::max();
		foreach (components,c)
		{
			float newLowest = (*c)->GetAnchorConfig().GetVAxis().GetMinimumEdge(GetRect().y.max, (*c)->GetRect().y);
			if (newLowest < lowest)
			{
				lowest = newLowest;
			}
		}
		if (lowest == std::numeric_limits<float>::max())
			lowest = GetRect().y.max;
		float newY = lowest - autoHeightSpacing;
		const float currentHeight = GetRect().y.max - newY;

		const float maxHeight = autoHeight ? this->maxHeight : GetRect().y.GetExtent();

		bool newVScroll = currentHeight > maxHeight;
		bool changed = vscroll.visible != newVScroll;
		rs |= changed;
		if (newVScroll)
		{
			vscroll.scrollMax = currentHeight;
			vscroll.viewSize = maxHeight;
			float barSize = M::Max(maxHeight,0.2f * GetHeight()*currentHeight);
			float overSize = barSize - maxHeight;
			vscroll.scrollMax += overSize;
			vscroll.viewSize += overSize;

			newY = GetRect().y.max - maxHeight;

			if (autoHeight)
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

			if (autoHeight)
			{
				auto rect = GetRect();
				rs |= Set(rect.y.min,newY);
				SetRect(rect);
			}
		}
		if (autoHeight)
			anchorConfig.GetVAxis().UpdateExtentIfEnabled( Distance::GameWindow( GetRect().y.GetExtent() ));
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


	void			Component::AcceptParentWindow(const PWindow&w)
	{
		if (window.lock() != w)
		{
			window = w;
			PropagateToChildren(w);
			if (w && w->IsVisible())
				OnScreenSizeOrUIScaleChange();
		}

	}

	void			Component::SetParent(const PWindow&w, const PComponent&c)
	{
		ASSERT__(!c || parent.expired());
		parent = c;
		AcceptParentWindow(w);
		if (c)
		{
			active = c->active;
			AcceptStateFromParent(c->CreatePropagating());
			InvalidateAutoLayout();
			if (w && w->IsVisible())
				OnScreenSizeOrUIScaleChange();
		}
		else
			active = false;
	}


	/*static*/ void				Component::RenderArea(const M::Rect<>&rect, bool lit, bool flip, const float3&backgroundTone, float lowOpacity/*=1*/, float highOpacity/*=1*/)
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
	}

	/*static*/ void				Component::RenderBorder(const M::Rect<>&rect, const float3&backgroundTone, const M::Quad<bool>&edges, float lowOpacity/*=1*/, float highOpacity/*=1*/)
	{
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






	/*virtual override*/ void	Panel::OnMouseMove(const M::TVec2<>&mouseAt, bool clipAreaContainsMouse)
	{
		Super::OnMouseMove(mouseAt,clipAreaContainsMouse);

	
		vscroll.OnMouseMove(mouseAt,clipAreaContainsMouse);
		
		M::TVec2<> moved = mouseAt;
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


	void			Panel::AnchorBelowPrevious(const PComponent&newComponent, const Distance& distanceFromSides /*= 0.015f*/,  const Distance&  distanceToPrevious/* = 0.015f*/)
	{
		{
			auto&mod = newComponent->Anchor();
			if (components.IsNotEmpty())
				mod.Beneath(components.Last(), distanceToPrevious);
			else
				mod.Top(distanceToPrevious);
			mod.FillHorizontally(distanceFromSides,distanceFromSides);
		}
		AddComponent(newComponent);
	}


	void				Box::BaseRender(bool lit, bool flipped, bool fill, bool outline)
	{
		const M::Rect<>&r = GetRect();
		if (fill)
			RenderArea(r,lit,flipped,backgroundTone,bottomOpacity,topOpacity);
		if (outline)
			RenderBorder(r,backgroundTone,true,bottomOpacity,topOpacity);
	}

	static const float CellSize = 0.01f;


	/*virtual*/ bool	Component::UpdateDynamicSize()
	{
		bool rs = false;
		const float h = GetContentHeight();
		if (h >= 0)
		{
			auto&axis = anchorConfig.GetVAxis();
			if (!axis.Stretches())
			{
				rs = SetHeight(Distance::GameWindow( h ));
			}
		}
		return rs;
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



	void			Input::SetText(const String&current)
	{
		if (IsActive())
			Engine::keyboard.FillInput(current);
		else
			backup.SetTo(current);
	}

	static Engine::InputProfile	profile;

	void			Input::SetAllowedAlphabet(const String&allowed)
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



	String			Input::GetText()
	{
		if (IsActive())
			return String(Engine::keyboard.GetInput(),Engine::keyboard.GetInputLen());
		return String(backup.GetCharacters(),backup.GetLength());
	}

	const char*		PasswordInput::GetText()
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





	/*virtual*/ StringRef BaseInput::GetTextRef()
	{
		const char*input;
		size_t len;
		bool active = IsActive();
		input = active ? Engine::keyboard.GetInput() : backup.GetCharacters();
		len = active ? Engine::keyboard.GetInputLen() : backup.GetLength();
		return StringRef(input,len);
	}


	/*static*/ StringBuffer	PasswordInput::placeholder;

	/*virtual override*/ StringRef PasswordInput::GetTextRef()
	{
		size_t len;
		bool active = IsActive();
		len = active ? Engine::keyboard.GetInputLen() : backup.GetLength();

		while (placeholder.GetLength() < len)
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
			RenderArea(core,false,true,backgroundTone);
		if (renderBorder)
			RenderBorder(core,backgroundTone,true);

		StringRef text = GetTextRef();


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


			GlobalEvents::applicationTerminate << StaticDestroy;
		}


	//	Area::StaticCreate();
	
	}


	float			TextField::GetTotalFontScale() const
	{
		return zoom * Detail::textZoom;
	}

	void			TextField::SetText(const String&string)
	{
		if (rawText.GetLength() == 1 && rawText.First() == string)
			return;
		rawText.SetSize(1);
		rawText.First() = string;
		HandleTextChange();
	}

	void	TextField::HandleTextChange()
	{
		ReWrap();
		UpdateDynamicSize();
		OnTextChange();
		if (this->widthLockedToIdeal || this->heightLockedToIdeal)
			this->InvalidateAutoLayout();
	}
	void			TextField::SetText(const Array<String>&string)
	{
		if (rawText == string)
			return;
		rawText = string;
		HandleTextChange();

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
		w *= 1.01f;
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
		textout.PushState();
		textout.Scale(Detail::textZoom*zoom);
		
		const float leftPadding = textout.GetScaledWidth('X') * padding.left;
		const float rightPadding = textout.GetScaledWidth('X') * padding.right;
		const float width = GetWidth() - leftPadding - rightPadding;
		const float center = width/2 + leftPadding;
		#ifdef _DEBUG
			wrappedAtWidth = width;
		#endif

	

		wrappedText.Clear();
		Array<String>	wrapped;
		for (const String&raw : rawText)
		{
			wrapf(raw,width,CharWidth,wrapped);
			wrappedText.MoveAppend(wrapped);
		}
		if (wrappedText.IsEmpty())
			wrappedText.Append();	//at least one empty line
		lineAt.SetSize(wrappedText.Count());
		const float lineHeight = textout.GetScaledHeight();
		const float y = -lineHeight*(padding.top);
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

		Super::BaseRender(lit,false,fillBackground,renderBorder);
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
	
		M::TMatrix4<> matrix = M::Matrix<>::eye4;
		
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

	/*virtual*/ void	Component::OnMouseMove(const M::TVec2<>&mouseAt, bool clipAreaContainsMouse)
	{
		containsMouse = clipAreaContainsMouse && GetRect().Contains(mouseAt);
	}


	void			Component::MoveTopTo(float h)
	{
		float delta = h - GetRect().y.max;
		TranslateRect(float2(0,delta));
	}

	bool			Component::FullAutoLayout(const M::Rect<>&context)
	{
		count_t numRounds = 0;
		bool rs = UpdateDynamicSize();
		for (;;)
		{
			const bool changed0 = ResolveAnchorAxis(AnchorAxis::Horizontal,context);
			const bool changed1 = ResolveAnchorAxis(AnchorAxis::Vertical,context);
			const bool sizeChanged = UpdateDynamicSize();

			if (!changed0 && !changed1 && !sizeChanged)
				return rs;
			rs = true;
			numRounds++;

			#ifdef _DEBUG
				if (numRounds > 900)
				{
					VerifyAnchorIntegrity();
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


	bool			Component::ResolveAnchorAxis(AnchorAxis a, const M::Rect<>&context)
	{
		const auto&axis = anchorConfig.axis[a];
		if (axis.IsFreeFloating())
			return false;
		
		auto rect = GetRect();
		M::TFloatRange<>&range = rect.axis[a];
		bool rs = false;
		const M::TFloatRange<>&ctx = context.axis[a];

		const float extent = axis.extent.ToScreenDistance(ctx.GetExtent(),a);
	
		if (axis.isAutoCentering)
		{
			float center = ctx.GetCenter();
			float top = std::min(ctx.max,center + extent*0.5f);
			bool changed = Set(range.max,top);
			changed |= Set(range.min,range.max - extent);

			SetRect(rect);
			return changed;
		}


		if (axis.lower.IsEnabled())
		{
			const auto&anc = axis.lower;

			const float dist = anc.GetOffset().ToScreenDistance(ctx.GetExtent(),a);

			const auto&ref = anc.GetReferences();
			if (ref.IsNotEmpty())
			{
				float highest = -std::numeric_limits<float>::max();
				foreach (ref,ref0)
				{
					PComponent ref = ref0->lock();
					highest = std::max(highest,ref->GetRect().axis[a].max);
				}
				if (highest > -std::numeric_limits<float>::max())
					rs |= Set(range.min, highest + dist);
			}
			else
				rs |= Set(range.min, ctx.min + dist);
		}
		if (axis.upper.IsEnabled())
		{
			const auto&anc = axis.upper;

			const float dist = anc.GetOffset().ToScreenDistance(ctx.GetExtent(),a);
			const auto&ref = anc.GetReferences();

			if (ref.IsNotEmpty())
			{
				float lowest = std::numeric_limits<float>::max();
				foreach (ref,ref0)
				{
					PComponent ref = ref0->lock();
					lowest = std::min(lowest,ref->GetRect().axis[a].min);
				}
				if (lowest < std::numeric_limits<float>::max())
					rs |= Set(range.max, lowest - dist);
			}
			else
				rs |= Set(range.max, ctx.max - dist);
		}
		else
			rs |= Set(range.max,range.min + extent);

		if (axis.lower.IsInactive())
			rs |= Set(range.min,range.max - extent);

		SetRect(rect);

		return rs;
	}

	Component&			Component::FillHorizontally(const Distance& spacing)
	{
		auto cfg = anchorConfig;
		cfg.GetHAxis().Fill(spacing,spacing);
		SetAnchorConfig(cfg);
		return *this;
	}

	Component&			Component::FillVertically(const Distance& spacing)
	{
		auto cfg = anchorConfig;
		cfg.GetVAxis().Fill(spacing,spacing);
		SetAnchorConfig(cfg);
		return *this;
	}

	Component&			Component::AnchorTop(const Distance&spacing)
	{
		auto cfg = anchorConfig;
		cfg.GetVAxis().AnchorUpper(Distance::Unknown, spacing);
		SetAnchorConfig(cfg);
		return *this;
	}


	Component&			Component::SetCenteredWidth(const Distance&width)
	{
		auto cfg = anchorConfig;
		auto&axis = cfg.GetHAxis();
		axis.SetAutoCenter(width);
		SetAnchorConfig(cfg);
		return *this;
	}



	/*static*/ bool		Component::Set(float&ref, float to)
	{
		ASSERT__(!isnan(to));
		bool changed = !M::Similar(ref,to);
		ref = to;
		return changed;
	}

	/*static*/ bool		Component::SetCenter(M::TFloatRange<>&ref, float to)
	{
		float current = ref.center();

		bool changed = !M::Similar(current,to);
		ref.SetCenter(to);
		return changed;
	}

	#if 0
	/*virtual*/ bool	Component::ResolveAnchors(const M::Rect<>&context)
	{
		//if (!resolveAnchors)
		//	return false;
		bool rs = false;
		rs |= ResolveAnchorAxis(0,context);
		rs |= ResolveAnchorAxis(1,context);
		return rs;
	}
	#endif /*0*/

	static bool CheckHovering(const M::Rect<>&area)
	{
		return area.Contains(Engine::mouse.location.windowRelative);
	}


	/*static*/ void	Component::Darken(const M::Rect<>&rect, float alpha/*=0.9f*/, bool flush/*=false*/)
	{
		SurfaceRenderer::SetColor(float4(0,0,0,alpha));
		SurfaceRenderer::Rasterize(rect);
		if (flush)
			SurfaceRenderer::Flush();
	
	}

	/*virtual override*/ float	Checkbox::GetIdealWidth() const
	{
		float checkBoxWidth = GetTotalFontScale() * textout.GetScaledHeight() / ViewPortAspect() * 1.5f;
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
		float checkBoxWidth = checkBoxHeight / ViewPortAspect();
		float top = GetRect().y.center() + checkBoxHeight*0.7f;// Super::extraSpacing * checkBoxHeight;
		M::TFloatRect box = M::TFloatRect(GetRect().x.min,top - checkBoxHeight,GetRect().x.min+checkBoxWidth,top);
		SurfaceRenderer::Rasterize(box);
		if (isChecked)
		{
			//SurfaceRenderer::SetColor(lit ? float4(0.7f):float4(0.4f));
			M::TFloatRect inner = box;
			inner.Scale(0.8f);
			SurfaceRenderer::SetColor(lit ? float4(1.f):float4(0.7f));
			SurfaceRenderer::Rasterize(inner);
		}
		SurfaceRenderer::Flush();
		LineRenderer::SetColor(lit ? float4(1) : float4(0.7f));
		LineRenderer::Rasterize(box);
		LineRenderer::Flush();
	}

	void		Panel::SetAutomaticHeight(bool b)
	{
		if (autoHeight == b)
			return;
		autoHeight = b;
		this->InvalidateAutoLayout();
		if (b && anchorConfig.GetVAxis().Stretches())
			AnchorTop(0);
		VerifyAnchorIntegrity();
	}


	/*virtual override*/ void		Panel::VerifyAnchorIntegrity() const
	{
		Super::VerifyAnchorIntegrity();
		if (autoHeight && (anchorConfig.GetVAxis().Stretches()))
			__debugbreak();
		ASSERT__(!autoHeight || (!anchorConfig.GetVAxis().Stretches()));
		if (autoHeight)
		{
			foreach (components,c)
				ASSERT__(!(*c)->GetAnchorConfig().GetVAxis().RequiresFixedParentSize());
		}
		foreach (components,c)
			(*c)->VerifyAnchorIntegrity();
	}


	/*virtual override*/ PWindow	Window::GetWindow()
	{
		return To<Window>::Cast(shared_from_this());
	}

	/*virtual override*/ PConstWindow	Window::GetWindow() const
	{
		return To<const Window>::Cast(shared_from_this());
	}

	/*virtual*/ void	Window::OnShow()
	{
		if (logFullAutoLayout)
			LogEvent(EventClass::Preset::gui,"Window["+String(this)+"]::OnShow()");

		visible = true; 
		SetActive(true); 
		OnScreenSizeOrUIScaleChange();
		VerifyAnchorIntegrity();
	}

	bool			Window::FullAutoLayout(const TCodeLocation&caller)
	{
		if (nestedCall)
			return false;
		nestedCall = true;
		if (logFullAutoLayout)
			LogEvent(EventClass::Preset::gui,"Window["+String(this)+"]::FullAutoLayout("+String(caller)+")");

		AcceptStateFromParent(TPropagating());
		bool rs = Super::FullAutoLayout(context);
		relayoutRequired = false;

		UpdateOuterRect();
		nestedCall = false;

		return rs;
	}




	/**/			TabBar::TabBar()
	{
		SetAutomaticHeight(true);
		SetBackgroundTransparent();
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
			auto mod = tab->Anchor();
			mod.GetHAxis().AnchorLower(Distance::Unknown);	//trigger exception when not updated later
			if (Super::HasComponents())
				mod.GetHAxis().lower.Set(Super::GetReferences().Last(),0.01f);
			mod.GetVAxis().AnchorUpper(Distance::Unknown);	//trigger exception when not updated later
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
		return Super::GetReferences().GetIndexOf(tabButton);
	}

	/*virtual*/ void	TabBar::OnTabChange()
	{
		if (onTabChange)
			onTabChange();
	}

	/*static*/ void		TabBar::ConfigureInactive(const PButton&active)
	{
		active->SetAlwaysDown(false);
		active->edges = M::Quad<bool>(true);
		active->bottomOpacity = 1;
		active->Anchor().GetVAxis().lower.Disable();
		active->LockToIdealHeight(true);
		active->topOpacity = 0.85f;
		active->bottomOpacity = 0.85f;
		active->SetColor(float4(1,1,1,0.85f));

	}
	/*static*/ void		TabBar::ConfigureActive(const PButton&active)
	{
		active->SetAlwaysDown(true);
		active->LockToIdealHeight(false);
		active->Anchor().GetVAxis().lower.Set(0);
		active->topOpacity = 1.f;
		active->bottomOpacity = 0;
		active->edges = M::Quad<bool>(true,false,true,true);
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
		active = To<Button>::Cast( GetReferences()[tabIndex] );
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
		this->SetBackgroundTransparent();
		tabBar.reset(new TabBar());
		{
			auto mod = tabBar->Anchor();
			mod.GetHAxis().Fill();
			mod.GetVAxis().AnchorUpper(Distance::Unknown);
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
			auto mod = tab.panel->Anchor();
			mod.GetHAxis().Fill();
			mod.GetVAxis().Fill();
			mod.GetVAxis().upper.Set(tabBar,Distance());
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


	/*virtual override*/ void	LinkLabel::OnMouseMove(const M::TVec2<>&mouseAt, bool clipAreaContainsMouse)
	{
		if (clipAreaContainsMouse)
			Engine::mouse.SetCursor(Engine::Mouse::CursorType::Hand);
		else
			Engine::mouse.SetCursor(Engine::Mouse::CursorType::Default);
	}



	void	MultiPlot::Plot::AddNode(MultiPlot*s, const index_t x, const float y)
	{

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
		s->stackedYRange.Include(yTop);
		s->rawYRange.Include(y);
		s->SignalAxesChanged();

		changed = true;
	}


	void	MultiPlot::Plot::RenderSurfaces() const
	{
		surface.Rasterize();
	}

	void	MultiPlot::Plot::RenderLines() const
	{
		lines.Rasterize();
	}

	/*virtual override*/ void	MultiPlot::Render()
	{
		Super::Render();
		bool anyChanged = axesChanged;
		foreach (plots,g)
			if (g->changed)
			{
				anyChanged = true;
				break;
			}
		if (anyChanged)
			Replot();

		display.StoreRendererState();
		display.SetRasterizer(true,true);
		foreach (plots,g)
			g->RenderSurfaces();

		display.RestoreRendererState();

		foreach (plots,g)
			g->RenderLines();


		axes.Rasterize();

		{
			BeginLabelPrint();
			foreach (labels,label)
			{
				textout.MoveTo(label->coords);
				textout << label->caption;
			}

			EndLabelPrint();
		}


		Box::RenderBorder(GetRect(),float3(1),true);
	}

	void MultiPlot::BeginLabelPrint()
	{
		textout.PushState();

		textout.Scale(Detail::textZoom*0.8f);
		float g = 0.6f;
		textout.SetColor(g,g,g);
	}

	void MultiPlot::EndLabelPrint()
	{
		textout.PopState();
	}


	/*static*/ float	MultiPlot::GetAxisStep(float rangeExtent)
	{
		const float lg = log(rangeExtent) / log(10.f);
		const int digits = int(floor(lg));
		const float base = pow(10.f,digits);
		const float relative = rangeExtent / base;
		if (relative < 2.f)
			return base*0.5f;
		if (relative < 5.f)
			return base;
		return base * 2.f;
	}

	/*static*/ bool	MultiPlot::GetAxisStepWidthAndCount(float rangeExtent, float&outStep, count_t&outCount)
	{
		if (rangeExtent <= 0)
			return false;
		outStep = GetAxisStep(rangeExtent);
		if (outStep == 0 || isnan(outStep))
			return false;
		outCount = M::Min((count_t)floor(rangeExtent / outStep),1000);
		return true;
	}

	/*static*/ void		MultiPlot::DoAxisCalculationAndCrash()
	{
		float s = GetAxisStep(0);
		float fcnt = floor(0 / s);
		count_t cnt = (count_t)fcnt;
		FATAL__("GetAxisStep(0) = "+ String(s)+" => fcnt="+String(fcnt)+", cnt="+String(cnt));
	}

	void	MultiPlot::Replot()
	{
		Super::OnRectChange();

		
		static const float labelPadding = 0.01f;
		const auto effectiveRange = isStack ? stackedYRange : rawYRange;
		const float yMax = effectiveRange.max;
		BeginLabelPrint();
			rightSpace = std::max(0.025f,textout.GetScaledWidth(String(yMax)) + labelPadding * 2.f);
		EndLabelPrint();

		const M::Rect<> plotScope = GetPlotScope();
		const M::Rect<> plotRect = GetPlotRect();
		foreach (plots,g)
			g->Replot(plotRect,plotScope, g == plots.begin() && isStack, isStack);


		const M::Rect<>&cRect = GetRect();
		{
			axesChanged = false;

			axes.Clear();

			//const M::Rect<> plotRect = GetPlotRect();



			const float yRange = plotRect.y.min - cRect.y.min;
			const float xRange = cRect.x.max - plotRect.x.max;

			const float4 color{float3{0.5f},1.f};

			axes.SetIndexOffsetToVertexCount();
			axes.AddVertex(float2(plotRect.x.min,cRect.y.min + yRange*0.1f),color);
			axes.AddVertex(float2(plotRect.x.min,cRect.y.min + yRange*0.9f),color);
			axes.AddLine(0,1);

			axes.AddVertex(float2(plotRect.x.max,cRect.y.min + yRange*0.1f),color);
			axes.AddVertex(float2(plotRect.x.max,cRect.y.min + yRange*0.9f),color);
			axes.AddLine(2,3);
			axes.AddLine(1,3);

			axes.SetIndexOffsetToVertexCount();
			axes.AddVertex(float2(plotRect.x.max + xRange*0.1f,plotRect.y.min),color);
			axes.AddVertex(float2(plotRect.x.max + xRange*0.9f,plotRect.y.min),color);
			axes.AddLine(0,1);

			axes.AddVertex(float2(plotRect.x.max + xRange*0.1f,plotRect.y.max),color);
			axes.AddVertex(float2(plotRect.x.max + xRange*0.9f,plotRect.y.max),color);
			axes.AddLine(2,3);
			axes.AddLine(0,2);

			{
				const M::TFloatRange<float> fx = M::FloatRange<float>(this->xRange.start,this->xRange.end-1);
				count_t count;
				float xLineStep;

				if (GetAxisStepWidthAndCount(fx.GetExtent(),xLineStep,count))
				{
					const index_t start = ceil(float(this->xRange.start+1) / xLineStep);
					for (index_t i = start; i < start + count; i++)
					{
						const float rel = fx.Relativate(xLineStep * i);
						const float x = plotRect.x.Derelativate(rel);
						axes.SetIndexOffsetToVertexCount();
						axes.AddVertex(float2(x,cRect.y.min + yRange*0.3f),color);
						axes.AddVertex(float2(x,cRect.y.min + yRange*0.7f),color);
						axes.AddLine(0,1);
					}
				}
			}

			if (effectiveRange.IsValid())
			{
				const M::TFloatRange<float> fy = M::FloatRange<float>(0,yMax);
				count_t count;
				float yLineStep;
				if (GetAxisStepWidthAndCount(fy.GetExtent(),yLineStep,count))
				{
					const index_t start = 1;
					for (index_t i = start; i < start + count; i++)
					{
						const float rel = fy.Relativate(yLineStep * i);
						const float y = plotRect.y.Derelativate(rel);
						axes.SetIndexOffsetToVertexCount();
						axes.AddVertex(float2(plotRect.x.max + xRange*0.3f,y),color);
						axes.AddVertex(float2(plotRect.x.max + xRange*0.7f,y),color);
						axes.AddLine(0,1);
					}
				}
			}

			axes.Bake();


			BeginLabelPrint();
			labels.Clear();
			{
				labels.Append().caption = String(this->xRange.start);
				labels.Last().coords = float2(plotRect.x.min + 0.005f,cRect.y.min + yRange * 0.5f + textout.GetScaledHeight()*0.5f);
			}
			{
				labels.Append().caption = String(this->xRange.end-1);
				labels.Last().coords = float2(plotRect.x.max - 0.005f - textout.GetScaledWidth(labels.Last().caption),cRect.y.min + yRange * 0.5f + textout.GetScaledHeight()*0.5f);
			}
			{
				labels.Append().caption = "0";
				labels.Last().coords = float2(plotRect.x.max + labelPadding,plotRect.y.min + textout.GetScaledHeight());
			}
			{
				labels.Append().caption = String(yMax);
				labels.Last().coords = float2(plotRect.x.max + labelPadding,plotRect.y.max);
			}

			EndLabelPrint();
		}

	}

	M::Rect<>			MultiPlot::GetPlotScope() const
	{
		return M::Rect<>(xRange.start,0.f,xRange.end-1,isStack ? stackedYRange.max : rawYRange.max);
	}

	M::Rect<>			MultiPlot::GetPlotRect() const
	{
		M::Rect<> rs = GetRect();
		rs.x.min += 0.025f;
		rs.x.max -= rightSpace;
		rs.y.min += 0.05f;
		rs.y.max -= 0.05f;
		return rs;
	}



	float	MultiPlot::SampleStackedHeight(index_t x) const
	{
		if (!xRange.Contains(x))
			return 0;

		float rs = 0;
		foreach (plots,g)
			rs += g->Sample(x);

		return rs;
	}


	float	MultiPlot::Plot::Sample(index_t x) const
	{
		if (x < start)
			return 0;
		x -= start;

		if (x >= data.Count())
			return 0;
		return data[x].yRaw;
	}


	void	MultiPlot::Plot::MakeVertex(const float x, const float y)
	{
		const float2	p{x,y};
		lines.AddVertex(p,float4(color,1.f));
		surface.AddVertex(p,float4(color,0.5f));
	}


	void	MultiPlot::Plot::Replot(const M::Rect<>&componentRect, const M::Rect<>&dataRect, bool addBottomLine, bool stacked)
	{
		changed = false;


		surface.Clear();
		lines.Clear();

		if (data.Count() > 1)
		{
			//if (stacked)
			{
				for (index_t i = 0; i < data.Count(); i++)
				{
					const float rX = dataRect.x.Relativate(i + start);
					const float y0 = stacked ? data[i].yMin : 0;
					const float y1 = stacked ? data[i].yMax : data[i].yRaw;
					ASSERT_IS_CONSTRAINED__(rX,0,1);
					MakeVertex( 
						componentRect.x.Derelativate( rX ),
						componentRect.y.Derelativate( dataRect.y.Relativate(y0) )
						);

					MakeVertex( 
						componentRect.x.Derelativate( rX ),
						componentRect.y.Derelativate( dataRect.y.Relativate(y1) )
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

					if (addBottomLine)
						lines.AddLine(
							((i-1)*2),
							((i)*2));
				}
				lines.AddLine(0,1);
				lines.AddLine(data.Count()*2-1,data.Count()*2-2);
			}
			#if 0
			else
			{
				const float w = ViewPortWidth();
				const float h = ViewPortHeight();

				struct TPoint
				{
					float2	raw,dir,normal,base,final;
				};

				Array<TPoint> points(data.Count());
				for (index_t i = 0; i < data.Count(); i++)
				{
					const float rX = dataRect.x.Relativate(i + start);
					ASSERT_IS_CONSTRAINED__(rX,0,1);
					const float x = componentRect.x.Derelativate( rX );
					const float y = componentRect.y.Derelativate( dataRect.y.Relativate(data[i].yRaw) );
					points[i].raw.x = x * w;
					points[i].raw.y = y * h;

					MakeVertex(x,y);
				}

				for (index_t i = 0; i+1 < points.Count(); i++)
				{
					auto&p = points[i];
					p.dir = points[i+1].raw - points[i].raw;
					p.normal = float2{p.dir.y,-p.dir.x}.Normalize0();
					p.base = p.raw + p.normal * 20 * UI::GetScale();
				}

				for (index_t i = 1; i+1 < points.Count(); i++)
				{
					const float2&n1 = points[i].normal;
					const float2&b0 = points[i-1].base,
								&b1 = points[i].base,
								&d0 = points[i-1].dir;
					const float2 bd = b1 - b0;
					const float div = Vec::dot(d0,n1);
					if (fabs(div) > TypeInfo<float>::error)
					{
						const float alpha = Vec::dot(n1, b1 - b0) / div;
						points[i].final = b0 + alpha * d0;
					}
					else
						points[i].final = points[i].base;
				}

				{
					points.First().final = points.First().base;
					points.Last().final = points.GetFromEnd(1).base + points.GetFromEnd(1).dir;
				}
				foreach (points,p)
				{
					MakeVertex(p->final.x / w, p->final.y / h);
				}



				for (index_t i = 1; i < data.Count(); i++)
				{
					surface.AddQuadInv(i-1,i,i+data.Count(),i+data.Count()-1);
					lines.AddLine(
						i-1,
						i);
				}

			}
			#endif
		}

		lines.Bake();
		surface.Bake();
	}


	void SignalScreenResize()
	{
		Detail::lastClientSpace = display.GetClientSize();
	}

}






}

#endif

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
#include <math/graph.h>

namespace DX11 = Engine::DX11; 

extern Engine::Textout<DX11::TextureFont>	textout;
extern Engine::Display<Engine::DirectX11>	display;


namespace HUD 
{

namespace Component
{

	/*static*/ PBaseInput	BaseInput::focusedInput = nullptr;
	/*static*/ const Distance Distance::Unknown = Distance(NAN,Type::Placeholder);
	/*static*/ const Distance Distance::Disabled = Distance(NAN,Type::Disabled);

	ChangeRecorder::TChange::TChange(const PComponent&c, const PComponent&cause, const TCodeLocation&loc) : component(c), cause(cause), loc(loc),componentRect(c->GetRect())
	{}

	void			ChangeRecorder::Record(const PComponent&self, const PComponent&cause, const TCodeLocation&loc)
	{
		if (groups.IsEmpty())
			groups.Append();
		groups.Last().changes << TChange(self,cause,loc);
		any = true;
	}

	void			ChangeRecorder::Record(Component*cause, const TCodeLocation&loc)
	{
		Record(cause->shared_from_this(),loc);
	}

	void			ChangeRecorder::Record(Component*self, const PComponent&cause, const TCodeLocation&loc)
	{
		Record(self->shared_from_this(),cause,loc);
	}


	static void		Print(const String&line)
	{
		OutputDebugStringA((line+"\n").c_str());
		LogEvent(EventClass::Preset::internalError,line);
	}


	static String ToString(const PComponent&c)
	{
		PButton button = To<Button>::DynamicCast(c);
		if (button)
			return "Button["+button->GetText().First()+"]@"+String((const void*)c.get())+" "+ToString(c->GetRect());
		PLabel label = To<Label>::DynamicCast(c);
		if (label)
			return "Label["+label->GetText().First()+"]@"+String((const void*)c.get())+" "+ToString(c->GetRect());
		return String(typeid(*c.get()).name())+"@"+String((const void*)c.get())+" "+ToString(c->GetRect());
	}


	String	ToString(const ChangeRecorder::TChange&ch)
	{
		using StringConversion::ToString;
		if (ch.cause)
			return ToString(ch.cause)+" -> "+ToString(ch.component)+", "+ToString(ch.loc);
		return ToString(ch.component)+", "+ToString(ch.loc);
	}


	static void ReportTree(const String&indent, const PComponent&comp)
	{
		Print(indent+ToString(comp));
		for (index_t i = 0; i < comp->CountChildren(); i++)
			ReportTree(indent+" ",comp->GetChild(i));
	}
	

	void ChangeRecorder::Report(const PComponent&forComp, const ChangeRecorder&prev) const
	{
		Print("Component Tree:");
		ReportTree(" ",forComp);

		#undef grp1

		Print("Change Trace:");
		for (index_t i = 0; i < groups.Count(); i++)
		{
			const auto*grp0 = groups + i;
			const auto*grp1 = prev.groups.Count() > i ? prev.groups + i : nullptr;
			if (grp0->title == nullptr)
			{
				if (grp1 != nullptr && grp1->title != nullptr)
					grp1 = nullptr;
			}
			elif (grp1 != nullptr)
				if (grp1->title == nullptr)
					grp1 = nullptr;
				elif (strcmp(grp0->title,grp1->title))
					grp1 = nullptr;
			if (grp0->title != nullptr)
				Print(" "+String(grp0->title));
			for (index_t j = 0; j < grp0->changes.Count(); j++)
			{
				const auto*ch0 = grp0->changes+j;
				const auto*ch1 = grp1 != nullptr && j < grp1->changes.Count() ? grp1->changes + j : nullptr;
				if (ch1 != nullptr)
					if (ch1->component != ch0->component || ch1->cause != ch1->cause)
						ch1 = nullptr;

				if (ch1 != nullptr)
				{
					if (ch1->componentRect.x == ch0->componentRect.x)
						Print("  "+ToString(ch1->componentRect.y)+">>"+ToString(ch0->componentRect.y)+"  "+ToString(*ch0));
					else
						Print("  "+ToString(ch1->componentRect)+">>"+ToString(ch0->componentRect)+"  "+ToString(*ch0));
				}
				else
					Print("  "+ToString(*ch0));
			}
		}

	}

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

	void		Anchor::VerifyIntegrity(const TCodeLocation&sender, const FVerification&check, const AnchorAxis&axis, bool isUpper) const
	{
		distanceFromReference.VerifyIntegrity();
		ASSERT2__(distanceFromReference.IsWellDefined(),axis.ToString(),isUpper);
		ASSERT2__(enabled || references.IsEmpty(),axis.ToString(),isUpper);
		if (check)
		{
			foreach (references,ref)
			{
				auto locked = ref->lock();
				if (!locked)
					Except::TriggerFatal(sender,"Anchor::VerifyIntegrity. Found null reference "+String(ref-references.pointer())+"/"+String(references.Count())+" on axis "+String(axis.ToString())+" upper="+String(isUpper));
				//ASSERT2__(locked,axis.ToString(),isUpper);
				check(locked);
			}
		}
	}


	void		AnchorConfig::Axis::VerifyIntegrity(const TCodeLocation&sender, const FVerification&v, bool allowUnknownExtent) const
	{
		lower.VerifyIntegrity(sender,v,myAxis,false);
		upper.VerifyIntegrity(sender,v,myAxis,true);
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

	void		AnchorConfig::Axis::SetFreeFloating(const TCodeLocation&sender, const Distance&extent /*= Distance::Unknown*/)
	{
		this->extent = extent;
		lower.Disable();
		upper.Disable();
		isAutoCentering = false;
		VerifyIntegrity(sender,FVerification(),true);
	}

	bool		AnchorConfig::Axis::SetExtent(const Distance&d)
	{
		ASSERT__(!Stretches());
		//ASSERT__(lower.IsFixed() || upper.IsFixed());
		if (extent == d)
			return false;
		extent = d;
		VerifyIntegrity(CLOCATION,FVerification(),true);
		return false;
	}
	void		AnchorConfig::Axis::AnchorLower(const Distance&extent, const Distance&offset/* = Distance()*/)
	{
		lower.Set(offset);
		upper.Disable();
		isAutoCentering = false;
		this->extent = extent;
		VerifyIntegrity(CLOCATION,FVerification(),true);
	}

	void		AnchorConfig::Axis::AnchorLower(const Distance&extent, const PComponent&reference, const Distance&lowerOffset/* = Distance()*/)
	{
		lower.Set(reference,lowerOffset);
		upper.Disable();
		isAutoCentering = false;
		this->extent = extent;
		VerifyIntegrity(CLOCATION,FVerification(),true);
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
		VerifyIntegrity(CLOCATION,FVerification(),true);
	}


	void		AnchorConfig::Axis::AnchorUpper(const Distance&extent, const Distance&offset /*= Distance()*/)
	{
		upper.Set(offset);
		lower.Disable();
		isAutoCentering = false;
		this->extent = extent;
		VerifyIntegrity(CLOCATION,FVerification(),true);
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
		VerifyIntegrity(CLOCATION,FVerification(),true);
	}

	void			AnchorConfig::SetFreeFloating(const Distance&width /*= Distance::Unknown*/,const Distance&height /*= Distance::Unknown*/)
	{
		GetHAxis().SetFreeFloating(CLOCATION,width);
		GetVAxis().SetFreeFloating(CLOCATION,height);
	}



	void			ComponentLocation::SetAxis(AnchorAxis a, const M::TFloatRange<>&range)
	{
		switch (a)
		{
			case AnchorAxis::Horizontal:
				SetHorizontal(range);
				break;
			case AnchorAxis::Vertical:
				SetVertical(range);
				break;
		}
	}

	void			ComponentLocation::SetHorizontal(const M::TFloatRange<>&range)
	{
		const bool hChange = range != this->rect.x;
		if (!hChange)
			return;

		range.AssertIsNotNaN();

		const bool widthChange = !M::Similar(range.GetExtent(),this->rect.x.GetExtent());
		this->rect.x = range;

		DBG_ASSERT_IS_CONSTRAINED__(range.min,Lower,Upper);
		DBG_ASSERT_IS_CONSTRAINED__(range.max,Lower,Upper);

		if (hChange)
			OnHorizontalRectChange();
		if (widthChange)
			OnWidthChange();
		OnRectChange();
	}

	void			ComponentLocation::SetVertical(const M::TFloatRange<>&range)
	{
		const bool vChange = range != this->rect.y;
		if (!vChange)
			return;

		range.AssertIsNotNaN();

		const bool heightChange = !M::Similar(range.GetExtent(),this->rect.y.GetExtent());
		this->rect.y = range;

		DBG_ASSERT_IS_CONSTRAINED__(range.min,Lower,Upper);
		DBG_ASSERT_IS_CONSTRAINED__(range.max,Lower,Upper);

		if (vChange)
			OnVerticalRectChange();
		if (heightChange)
			OnHeightChange();
		OnRectChange();
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

	AnchorModifier&	AnchorModifier::FillHBetween(const PComponent&leftEdge, const PComponent&rightEdge, const Distance&leftOffset /*= Distance()*/, const Distance&rightOffset /*= Distance()*/)
	{
		GetHAxis().FillBetween(leftEdge,rightEdge,leftOffset,rightOffset);
		return *this;
	}

	void AnchorModifier::Axis::FillBetween(const PComponent&lowerC, const PComponent&upperC, const Distance&lowerOffset /*= Distance()*/, const Distance&upperOffset /*= Distance()*/)
	{
		lower.Set(lowerC,lowerOffset);
		upper.Set(upperC,upperOffset);
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




	/*virtual*/ void	Component::Render( float opacity )
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
		anchorConfig.VerifyIntegrity(CLOCATION,FVerification(),GetWindow() != nullptr);
		SignalLayoutChangeUp();
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

	/*virtual override*/ void	Panel::SetAnchorConfig(const AnchorConfig&cfg)
	{
		if (cfg.GetVAxis().Stretches())
			SetAutomaticHeight(false);
		Super::SetAnchorConfig(cfg);
	}


	void Panel::SignalLayoutChangeUp()
	{
		if (resolvingAxis)
			return;
		this->InvalidateOrder();
		Super::SignalLayoutChangeUp();
	}

	void Panel::SignalWindowLayoutChange()
	{
		this->InvalidateOrder();
		foreach (this->components,c)
			(*c)->SignalWindowLayoutChange();
	}


	void Component::SignalLayoutChangeUp()
	{
		PComponent parent = this->GetParent();
		if (parent)
			parent->SignalLayoutChangeUp();
	}

	/*virtual*/ void	Component::VerifyAnchorIntegrity(const TCodeLocation&sender) const
	{
		auto p0 = this->GetParent();
		anchorConfig.VerifyIntegrity(sender,
		[this,&p0](const PComponent&c)
		{
			auto p1 = c->GetParent();
			if (p0 && p1 && p0 != p1)
				FATAL__("Component parents mismatch. Components referencing each other via anchors must have an immediate, common parent");
		},GetWindow() == nullptr);
	}

	/*virtual*/ void	Component::SetAnchorConfig(const AnchorConfig&cfg)
	{
		anchorConfig = cfg;
		if (anchorConfig.HasExtentPlaceholders())
		{
			UpdateDynamicSize(ChangeRecorder());
			if (anchorConfig.HasExtentPlaceholders())
			{
				anchorConfig.GetHAxis().UpdateExtentIfEnabled(Distance::GameWindow( GetWidth() ));
				anchorConfig.GetVAxis().UpdateExtentIfEnabled(Distance::GameWindow( GetHeight() ));

			}
			ASSERT__(!anchorConfig.HasExtentPlaceholders());
		}
		VerifyAnchorIntegrity(CLOCATION); 
		SignalLayoutChangeUp();
	}



	PComponent	mousePressedComponent;

	/*static*/ DX11::BaseInstructionBlock	Image::texturedQuad;
	/*static*/ DX11::ObjectBuffer<float4>	Image::texturedQuadColor;


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



	/*virtual*/ bool		Panel::AddComponent( const PComponent&c, const TCodeLocation&caller)
	{
		if (!c)
			return false;
		if (components.Contains(c))
			return false;

		c->SetActive(IsActive());
		components << c;
		c->SetParent( GetWindow(), shared_from_this());
		c->VerifyAnchorIntegrity(caller);
		VerifyAnchorIntegrity(caller);
		InvalidateOrder();
		return true;
	}


	void Panel::RecomputeOrder(AnchorAxis axis)
	{
		order[axis].SetSize(components.Count());

		M::Graph g;


		PointerTable<M::Graph::PNode>	nodeMap;

		foreach (components,c)
		{
			auto newNode = g.AddNode();
			nodeMap.Set(c->get(),newNode);
			newNode->attachment = *c;
		}

		foreach (components,c0)
		{
			const auto&c = **c0;
			const auto myNode = nodeMap.Require(c0->get());
			const auto&a = c.GetAnchorConfig().axis[axis];
			foreach (a.lower.GetReferences(),ref)
				g.AddEdge(nodeMap.Require(ref->lock().get()),myNode);
			foreach (a.upper.GetReferences(),ref)
				g.AddEdge(nodeMap.Require(ref->lock().get()),myNode);
		}
		Buffer<M::Graph::PNode>	finalOrder(components.Count());
		if (!g.CreateTopologicalOrder(finalOrder))
			FATAL__("Failed to create topological order of component along axis "+String(axis.ToString()));
		for (index_t i = 0; i < components.Count(); i++)
			order[axis][i] = To<Component>::Cast( finalOrder[i]->attachment );
	}


	/*virtual override*/ void	Panel::ResolveAnchorAxis(AnchorAxis axis, const M::Rect<>&context, ChangeRecorder&rec)
	{
		resolvingAxis = true;
		Super::ResolveAnchorAxis(axis,context,rec); 
		M::Rect<> childSpace = GetRect();
		if (vscroll.visible)
			childSpace.x.max = M::Max(childSpace.x.min, childSpace.x.max - GetVisibleScrollbarContentExpansion());
		if (order[axis].Count() != components.Count())
			RecomputeOrder(axis);
		foreach (order[axis],c) 
			(*c)->ResolveAnchorAxis(axis,childSpace,rec);
		resolvingAxis = false;
	}


	/*virtual override*/ void		Slider::OnScreenSizeOrUIScaleChange() 
	{
		Super::OnScreenSizeOrUIScaleChange();
		relativeHeight = GetPixelHeight() / ViewPortHeight();
		relativeThumbWidth = GetThumbPixelWidth() / ViewPortWidth();
		UpdateDynamicSize(ChangeRecorder());
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



	/*virtual override*/ void	Slider::Render( float opacity )
	{
		const auto&rect = GetRect();
		const float top = rect.y.max;
		const float bottom = rect.y.min;

		const float left = rect.x.min;
		const float right = rect.x.max;

		barBack = M::Rect<>(left,bottom,right,top);
		SurfaceRenderer::SetColor(float4(0,0,0,0.9*opacity));
		SurfaceRenderer::Rasterize(barBack);


		dragRange = M::FloatRange(left + relativeThumbWidth/2.f,right-relativeThumbWidth/2.f);
		const float barCenter = dragRange.Derelativate(current/max);
		bar = M::Rect<>(barCenter-relativeThumbWidth/2.f,barBack.y.min,barCenter+relativeThumbWidth/2.f,barBack.y.max);

		if (showMarkers)
		{
			float4 markerColor(1,1,1,0.5f*opacity);
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

		RenderArea(bar,barContainsMouse,dragging,float3(1),opacity,opacity);
		RenderBorder(bar,float3(1),true,opacity,opacity);
	}


	///*virtual override*/ WheelRs	Slider::OnMouseWheel(short delta)
	//{
	//	if (this->ContainsMouse())
	//	{
	//		if (!dragging)
	//		{
	//			//constexpr float speed = -1.f / 5.f;
	//			current = M::Clamp(current + slideStep * delta,0,max);
	//			if (onChange)
	//				onChange(current);
	//			return WheelRs::Processed;
	//		}
	//		return WheelRs::Ignored;
	//	}
	//	else
	//		return WheelRs::NotContained;
	//	//return up->HandleMouseWheel(delta) || down->HandleMouseWheel(delta);
	//}

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




	void	Panel::TScrollbar::Render( float opacity, const M::Rect<>&rect )
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
		SurfaceRenderer::SetColor(float4(0,0,0,0.9 * opacity));
		SurfaceRenderer::Rasterize(outline);

		bar = M::Rect<>(outline.x.min,outline.y.Derelativate(1.f - (viewTop + viewSize) / scrollMax),outline.x.max,outline.y.Derelativate(1.f - (viewTop) / scrollMax));
		RenderArea(bar,barContainsMouse,dragging,float3(1),opacity,opacity);
		RenderBorder(bar,float3(1),true,opacity,opacity);

		up->SetLocation(topButton);
		up->Render(opacity);
		down->SetLocation(bottomButton);
		down->Render(opacity);
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

	/*virtual override*/ void	Panel::SetActive( bool isActive)
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

	/*virtual override*/ void Button::Render( float opacity )
	{
		bool lit = IsLit();
		bool flipped = mousePressedComponent.get() == this;
		Super::BaseRender(opacity, (lit && !flipped)  || alwaysDown,(lit && flipped) || (alwaysDown && flipWhenAlwaysDown), fillBackground, renderBorder);
		Super::RenderText(opacity, lit || alwaysDown);
	}


	/*static*/ void		ScrollButton::RenderArrow( float opacity, const M::Rect<>&r, direction_t dir, bool lit )
	{
		SurfaceRenderer::SetColor(float4(1,1,1,(lit?0.75f:0.5f) * opacity));

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


	/*virtual override*/ void	Panel::Render( float opacity )
	{
		const auto&rect = GetRect();
		if (!rect.IsValid())
			return;
		Super::Render(opacity); 
		if (vscroll.visible)
		{
			vscroll.Render(opacity, rect);
			Detail::PushLoadView(rect,float2(0,-vscroll.viewTop));
		}
		else
			Detail::PushLoadView(rect);
		RenderChildren(opacity);
		Detail::PopLoadView();
	}


	/*virtual override*/ void	Panel::UpdateDynamicSize(ChangeRecorder&rec)
	{
		Super::UpdateDynamicSize(rec);
		foreach (components,c)
			(*c)->UpdateDynamicSize(rec);

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
		if (vscroll.visible != newVScroll)
			rec.Record(this,CLOCATION);
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
				if (Set(rect.y.min,newY))
					rec.Record(this,CLOCATION);
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
				if (Set(rect.y.min,newY))
					rec.Record(this,CLOCATION);
				SetRect(rect);
			}
		}
		if (autoHeight)
			anchorConfig.GetVAxis().UpdateExtentIfEnabled( Distance::GameWindow( GetRect().y.GetExtent() ));
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
			SignalLayoutChangeUp();
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


	void			Panel::AnchorBelowPrevious(const TCodeLocation&caller, const PComponent&newComponent, const Distance& distanceFromSides /*= 0.015f*/,  const Distance&  distanceToPrevious/* = 0.015f*/)
	{
		{
			auto&mod = newComponent->Anchor();
			if (components.IsNotEmpty())
				mod.Beneath(components.Last(), distanceToPrevious);
			else
				mod.Top(distanceToPrevious);
			mod.FillHorizontally(distanceFromSides,distanceFromSides);
		}
		AddComponent(newComponent,caller);
	}


	void				Box::BaseRender( float opacity, bool lit, bool flipped, bool fill, bool outline )
	{
		const M::Rect<>&r = GetRect();
		if (fill)
			RenderArea(r,lit,flipped,backgroundTone,bottomOpacity*opacity,topOpacity*opacity);
		if (outline)
			RenderBorder(r,backgroundTone,true,bottomOpacity*opacity,topOpacity*opacity);
	}

	static const float CellSize = 0.01f;


	/*virtual*/ void	Component::UpdateDynamicSize(ChangeRecorder&rec)
	{
		const float h = GetContentHeight();
		if (h >= 0)
		{
			auto&axis = anchorConfig.GetVAxis();
			if (!axis.Stretches())
			{
				if (SetHeight(Distance::GameWindow( h )))
					rec.Record(this,CLOCATION);
			}
		}
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
		{
			ASSERT__(Detail::InputIsActive());
			Engine::keyboard.FillInput(current);
		}
		else
			backup.SetTo(current);
	}


	void			Input::SetAllowedAlphabet(const String&allowed)
	{
		this->allowed = allowed;
		if (Super::IsActive())
			Engine::keyboard.Filter(allowed.c_str());
	}






	bool			BaseInput::IsFocused() const
	{
		return focusedInput.get() == this;
	}


	void			BaseInput::SetInputActive(bool isActive)
	{
		const bool myInputIsActive = IsFocused();
		if (myInputIsActive == isActive)
			return;
		if (!isActive)
			focusedInput = nullptr;
		else
		{
			if (focusedInput)
				focusedInput->SetInputActive(false);
			focusedInput = To<BaseInput>::Cast(shared_from_this());
		}

		if (isActive)
		{
			WBaseInput w = To<BaseInput>::Cast(shared_from_this());
			Detail::ActivateInput(GetAllowed(),[w](Key::Name k)
			{
				PBaseInput self = w.lock();
				if (!self)
					return;
				if (k == Key::Enter || k == Key::Return)
				{
					if (self->onEnter)
						self->onEnter();
				}
				else
					if (self->onSpecialKey)
						self->onSpecialKey(k);
			},backup);
		}
		else
			Detail::DeactivateInput(&backup);
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


	/*virtual override*/ void			BaseInput::Render( float opacity )
	{
		core = GetRect();
		textout.PushState();
		textout.Scale(Detail::textZoom);
		core.Expand(-textout.GetScaledHeight()*0.4f);
		//core.Scale(0.97f);

		if (fillBackground)
			RenderArea(core,false,true,backgroundTone,opacity,opacity);
		if (renderBorder)
			RenderBorder(core,backgroundTone,true,opacity,opacity);

		StringRef text = GetTextRef();


		static const char cursorChar = '<';
		float cursorLen = textout.GetScaledWidth(StringRef(&cursorChar,1));
		float visualLen = textout.GetScaledWidth(text) + cursorLen;


		SurfaceRenderer::Flush();

		float4 baseColor;
		if (IsEnabled())
			baseColor = (IsLit() ? float4(1,1,1,opacity) : float4(0.75,0.75,0.75,opacity));
		else
			baseColor = float4(0.5,0.5,0.5,opacity);

		textout.SetColor(baseColor);

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
		texturedQuadColor.Clear();
	}

	/*static*/ void Image::StaticCreate()
	{
		if (texturedQuad.quickSlots.IsEmpty())
		{
			texturedQuadColor.Create(float4(1));
			texturedQuad.quickSlots.SetSize(1);
			Engine::VertexBinding vtx;
			vtx.vertex.Set(0,2);
			vtx.normal.Unset();
			vtx.floats_per_vertex = 2;
			texturedQuad.binding = vtx;

			DX11::Shader::Binding bindings[] = {
				DX11::Shader::Binding(0,"Texture"),
				DX11::Shader::Binding(texturedQuadColor,"Color"),
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
		UpdateDynamicSize(ChangeRecorder());
		OnTextChange();
		if (this->widthLockedToIdeal || this->heightLockedToIdeal)
			this->SignalLayoutChangeUp();
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
			SignalLayoutChangeUp();
	}




	/*virtual override*/ float		TextField::GetContentHeight() const
	{
		return heightLockedToIdeal ? textHeight : -1;
	}

	/*virtual override*/ void	TextField::UpdateDynamicSize(ChangeRecorder&rec)
	{
		bool changed = false;
		if (widthLockedToIdeal && UpdateIdealWidth())
			rec.Record(this,CLOCATION);
		Super::UpdateDynamicSize(rec);
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
		Array<StringRef>	wrapped;
		for (const String&raw : rawText)
		{
			Wrap(raw,width,CharWidth,wrapped);
			#ifdef _DEBUG
				if (width > CharWidth('X'))
					foreach (wrapped,w)
					{
							float accum = 0;
						Buffer0<float> widths,lengths;
						foreach (*w, c)
						{
							float w = CharWidth(*c);
							widths << w;
							accum += w;
							lengths << accum;
						}
						//const float swidth = textout.GetScaledWidth(*w);
						//ASSERT__(M::Similar(swidth,accum));	//<- this may fail if the string contains special icons, since they are considered by CharWidth() but not by GetScaledWidth()
						if (accum > width)
						{
							const index_t atWord = w - wrapped.pointer();
							const char*const word = w->begin();
							Array<StringRef>	wrapped2;
							Wrap(raw,width,CharWidth,wrapped2);
						}
						ASSERT_LESS_OR_EQUAL__(accum,width);
					}
			#endif
			wrappedText.MoveAppendAll(wrapped);
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




	/*virtual override*/ void			TextField::Render( float opacity )
	{
		bool lit = IsLit();

		Super::BaseRender(opacity,lit,false,fillBackground,renderBorder);
		RenderText(opacity,lit);
	}

	void TextField::RenderText( float opacity, bool lit )
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
			baseColor = (lit ? float4(1,1,1,opacity) : float4(0.75,0.75,0.75,opacity));
		else
			baseColor = float4(0.5,0.5,0.5,opacity);

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


	/*virtual override*/ void			Image::Render( float opacity )
	{
		if (texture.IsEmpty())
			return;

		const bool lit = IsLit();
	
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
		texturedQuadColor.Update(float4(1,1,1,opacity));
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

	void			Component::FullAutoLayout(const M::Rect<>&context)
	{
		count_t numRounds = 0;
		ChangeRecorder rec;
		UpdateDynamicSize(rec);
		for (;;)
		{
			ChangeRecorder prev = rec;
			rec.Clear();
			rec.NewGroup("Horizontal");
			ResolveAnchorAxis(AnchorAxis::Horizontal,context, rec);
			rec.NewGroup("Vertical");
			ResolveAnchorAxis(AnchorAxis::Vertical,context, rec);
			rec.NewGroup("Size");
			UpdateDynamicSize(rec);

			if (!rec.HasChanged())
				return;
			numRounds++;

			#ifdef _DEBUG
				if (numRounds > 900)
				{
					rec.Report(shared_from_this(),prev);

					VerifyAnchorIntegrity(CLOCATION);
					if (IsDebuggerPresent())
						DebugBreak();
					else
						ShowMessage(__func__ ": Debug: GUI will abort auto-layout shortly. Attach now and debug.");
				}

			#endif

			if (numRounds > 1000)	//might be linear to the number of contained components, depending on order
			{
				static bool first = true;
				if (first)
				{
					rec.Report(shared_from_this(),prev);
					first = false;
				}
				return;
			}
		}

	}


	void			Component::ResolveAnchorAxis(AnchorAxis a, const M::Rect<>&context, ChangeRecorder&rec)
	{
		const auto&axis = anchorConfig.axis[a];
		if (axis.IsFreeFloating())
			return;
		
		auto rect = GetRect();
		M::TFloatRange<>&range = rect.axis[a];
		const M::TFloatRange<>&ctx = context.axis[a];

		const float extent = axis.extent.ToScreenDistance(ctx.GetExtent(),a);
	
		if (axis.isAutoCentering)
		{
			float center = ctx.GetCenter();
			float top = std::min(ctx.max,center + extent*0.5f);
			if (Set(range.max,top))
				rec.Record(this,CLOCATION);
			if (Set(range.min,range.max - extent))
				rec.Record(this,CLOCATION);

			SetRect(rect);
			return;
		}


		if (axis.lower.IsEnabled())
		{
			const auto&anc = axis.lower;

			const float dist = anc.GetOffset().ToScreenDistance(ctx.GetExtent(),a);

			const auto&ref = anc.GetReferences();
			if (ref.IsNotEmpty())
			{
				float highest = -std::numeric_limits<float>::max();
				PComponent hComp;
				foreach (ref,ref0)
				{
					PComponent ref = ref0->lock();
					float candidate = ref->GetRect().axis[a].max;
					if (candidate > highest)
					{
						hComp = ref;
						highest = candidate;
					}
				}
				if (highest > -std::numeric_limits<float>::max())
					if (Set(range.min, highest + dist))
						rec.Record(this,hComp,CLOCATION);
			}
			else
				if (Set(range.min, ctx.min + dist))
					rec.Record(this,CLOCATION);
		}
		if (axis.upper.IsEnabled())
		{
			const auto&anc = axis.upper;

			const float dist = anc.GetOffset().ToScreenDistance(ctx.GetExtent(),a);
			const auto&ref = anc.GetReferences();

			if (ref.IsNotEmpty())
			{
				PComponent lComp;
				float lowest = std::numeric_limits<float>::max();
				foreach (ref,ref0)
				{
					PComponent ref = ref0->lock();
					float candidate = ref->GetRect().axis[a].min;
					if (candidate < lowest)
					{
						lowest = candidate;
						lComp = ref;
					}
				}
				if (lowest < std::numeric_limits<float>::max())
					if (Set(range.max, lowest - dist))
						rec.Record(this,lComp,CLOCATION);
			}
			else
				if (Set(range.max, ctx.max - dist))
					rec.Record(this,CLOCATION);
		}
		else
			if (Set(range.max,range.min + extent))
				rec.Record(this,CLOCATION);

		if (axis.lower.IsInactive())
			if (Set(range.min,range.max - extent))
				rec.Record(this,CLOCATION);

		SetAxis(a,range);
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

	/*virtual override*/ void Checkbox::Render( float opacity )
	{
		bool lit = IsLit();
		//Super::BaseRender(lit,false);
		Super::RenderText(opacity, lit);
		SurfaceRenderer::SetColor((lit ? float4(0.7f):float4(0.5f)) & float4(1,1,1,opacity));
		float checkBoxHeight = GetTotalFontScale() * textout.GetScaledHeight();
		float checkBoxWidth = checkBoxHeight / ViewPortAspect();
		float top = GetRect().y.center() + checkBoxHeight*0.7f;// Super::extraSpacing * checkBoxHeight;
		M::TFloatRect box = M::TFloatRect(GetRect().x.min,top - checkBoxHeight,GetRect().x.min+checkBoxWidth,top);
		SurfaceRenderer::Rasterize(box);
		const float4 color = lit ? float4(1,1,1,opacity):float4(float3(0.7f), 0.7*opacity);
		if (isChecked)
		{
			//SurfaceRenderer::SetColor(lit ? float4(0.7f):float4(0.4f));
			M::TFloatRect inner = box;
			inner.Scale(0.8f);
			SurfaceRenderer::SetColor(color);
			SurfaceRenderer::Rasterize(inner);
		}
		SurfaceRenderer::Flush();
		LineRenderer::SetColor(color);
		LineRenderer::Rasterize(box);
		LineRenderer::Flush();
	}

	void		Panel::SetAutomaticHeight(bool b)
	{
		if (autoHeight == b)
			return;
		autoHeight = b;
		this->SignalLayoutChangeUp();
		if (b && anchorConfig.GetVAxis().Stretches())
			AnchorTop(0);
		VerifyAnchorIntegrity(CLOCATION);
	}


	/*virtual override*/ void		Panel::VerifyAnchorIntegrity(const TCodeLocation&sender) const
	{
		Super::VerifyAnchorIntegrity(sender);
		if (autoHeight && (anchorConfig.GetVAxis().Stretches()))
			__debugbreak();
		ASSERT__(!autoHeight || (!anchorConfig.GetVAxis().Stretches()));
		if (autoHeight)
		{
			foreach (components,c)
				ASSERT__(!(*c)->GetAnchorConfig().GetVAxis().RequiresFixedParentSize());
		}
		foreach (components,c)
			(*c)->VerifyAnchorIntegrity(sender);
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
		VerifyAnchorIntegrity(CLOCATION);
	}

	void			Window::FullAutoLayout(const TCodeLocation&caller)
	{
		if (nestedCall)
			return;
		nestedCall = true;
		if (logFullAutoLayout)
			LogEvent(EventClass::Preset::gui,"Window["+String(this)+"]::FullAutoLayout("+String(caller)+")");

		AcceptStateFromParent(TPropagating());
		Super::FullAutoLayout(context);
		relayoutRequired = false;

		UpdateOuterRect();
		nestedCall = false;
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

		Super::AddComponent(tab,CLOCATION);

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
		active->Anchor().Top();
		active->LockToIdealHeight(true);
		active->topOpacity = 0.85f;
		active->bottomOpacity = 0.85f;
		active->SetColor(float4(1,1,1,0.85f));

	}
	/*static*/ void		TabBar::ConfigureActive(const PButton&active)
	{
		active->SetAlwaysDown(true);
		active->LockToIdealHeight(false);
		active->Anchor().FillVertically();
//			GetVAxis().lower.Set(0);
		active->topOpacity = 1.f;
		active->bottomOpacity = 0;
		active->edges = M::Quad<bool>(true,false,true,true);
		active->SetColor(float4(1));
	}


	void			TabBar::SetActiveTab( index_t tabIndex, bool triggerCallback)
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

		Super::AddComponent(tabBar,CLOCATION);
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


	void			TabCollection::SetActiveTab( index_t tabIndex, bool triggerCallback)
	{
		if (tabIndex == activeIndex)
			return;
		ASSERT__(tabIndex < tabs.Count());

		if (tabBar)
			tabBar->SetActiveTab(tabs[tabIndex].button,false);
		Super::RemoveComponent(active.panel);
		activeIndex = tabIndex;
		active = tabs[tabIndex];
		Super::AddComponent(active.panel,CLOCATION);
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


	void	MultiPlot::Plot::RenderSurfaces( float opacity ) const
	{
		surface.Rasterize(float4(1,1,1,opacity));
	}

	void	MultiPlot::Plot::RenderLines( float opacity) const
	{
		lines.Rasterize(float4(1,1,1,opacity));
	}

	/*virtual override*/ void	MultiPlot::Render( float opacity )
	{
		Super::Render(opacity);
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
			g->RenderSurfaces(opacity);

		display.RestoreRendererState();

		foreach (plots,g)
			g->RenderLines(opacity);


		axes.Rasterize(float4(1,1,1,opacity));

		{
			BeginLabelPrint();
			foreach (labels,label)
			{
				textout.MoveTo(label->coords);
				textout << label->caption;
			}

			EndLabelPrint();
		}


		Box::RenderBorder(GetRect(),float3(1),true,opacity,opacity);
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

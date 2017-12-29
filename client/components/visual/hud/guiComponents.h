#ifdef HAS_WINDOW
#ifndef gameGUIH
#define gameGUIH

#include "common.h"

#include <global_string.h>
#include <math/vclasses.h>
#include <engine/interface/keyboard.h>
#include <general/exception.h>

#include <memory>
//#include "../../ExtRenderer/DirectX11.h"
#include <typeDecl.h>
#include <cast.h>
#include "uiScale.h"
#include "surfaceRenderer.h"
#include "../lineRenderer.h"


namespace HUD
{
namespace Component
{
	using namespace DeltaWorks;

	namespace DX11 = Engine::DX11;


	DECLARE_POINTER_CLASS(Component);
		DECLARE_POINTER_CLASS(Image);
		DECLARE_POINTER_CLASS(Slider);
		DECLARE_POINTER_CLASS(Box);
			DECLARE_POINTER_CLASS(MultiPlot);
			DECLARE_POINTER_CLASS(TextField);
				DECLARE_POINTER_CLASS(Checkbox);
				DECLARE_POINTER_CLASS(Label);
					DECLARE_POINTER_CLASS(LinkLabel);
				DECLARE_POINTER_CLASS(Button);
					DECLARE_POINTER_CLASS(ScrollButton);
			DECLARE_POINTER_CLASS(BaseInput);
				DECLARE_POINTER_CLASS(Input);
				DECLARE_POINTER_CLASS(PasswordInput);
			DECLARE_POINTER_CLASS(Panel);
				DECLARE_POINTER_CLASS(ButtonPanel);
				DECLARE_POINTER_CLASS(TabCollection);
				DECLARE_POINTER_CLASS(TabBar);
				DECLARE_POINTER_CLASS(Window);
	

	CONSTRUCT_ENUMERATION3(WheelRs,
		NotContained,
		Ignored,
		Processed
		);

	CONSTRUCT_ENUMERATION2(AnchorAxis,Horizontal,Vertical);



	struct Distance
	{
		CONSTRUCT_ENUMERATION5(Type,RelativeToParentComponent,RelativeToGameWindow,AbsolutePixels,Placeholder,Disabled);


		float	value = 0;
		Type	type;

		/**/	Distance()	{}
		/**/	Distance(float d, Type t = Type::RelativeToGameWindow) : value(d),type(t)	{}

		static Distance GameWindow(float d) {return Distance(d,Type::RelativeToGameWindow);}
		static Distance OfParentSize(float d) {return Distance(d,Type::RelativeToParentComponent);}
		static const Distance Unknown, Disabled;

		void	VerifyIntegrity() const;
		bool	IsWellDefined() const;
		bool	IsWellDefinedNonZero() const;
		bool	IsPlaceholder() const {return type == Type::Placeholder;}
		bool	IsDisabled() const {return type == Type::Disabled;}
		bool	IsZero() const {return IsDefined() && value ==0;}
		bool	IsNonZero() const {return IsDefined() && value != 0;}
		bool	IsPositiveNonZero() const {return IsDefined() && value > 0;}
		bool	IsPositiveNonZeroOrPlaceholder() const {return !IsDisabled() && ( value > 0 || type == Type::Placeholder);}
		bool	IsDefined() const {return type != Type::Placeholder && type != Type::Disabled;}
		bool	RequiresFixedParentSize() const {return type == Type::RelativeToParentComponent && value != 0;}

		void	operator=(const Distance&other)
		{
			if (other.IsPlaceholder() && IsDefined())
				return;
			type = other.type;
			value = other.value;
		}

		float	ToScreenDistance(float parentSize, AnchorAxis myAxis) const;
		bool	operator==(const Distance&other) const
		{
			return ((isnan(value) && isnan(other.value)) || value == other.value) && type == other.type;
		}

		bool	operator!=(const Distance&other) const
		{
			return !operator==(other);
		}

		friend String ToString(const Distance&dist)
		{
			return String(dist.value)+"["+String(dist.type.ToString())+"]";
		}
	};


	typedef std::function<void(const PComponent&)> FVerification;
	typedef std::function<bool(const PComponent&)> FSelector;

	struct Anchor
	{
		typedef Buffer0<WComponent>	Components;

		void			Disable()
		{
			enabled = false;
			references.Clear();
		}
		void			Set(const PComponent&c, const Distance&dist)
		{
			enabled = true;
			distanceFromReference = dist;
			references.Clear();
			if (c)
				references.Append(c);
		}
		void			Set(const Distance&dist)
		{
			enabled = true;
			distanceFromReference = dist;
			references.Clear();
		}

		void			VerifyIntegrity(const FVerification&, const AnchorAxis&, bool isUpper) const;

		void			SetAllOf(const PPanel&, const Distance&distance, const Component*mustNotInclude);
		void			SetSomeOf(const PPanel&, const Distance&distance, const FSelector&, const Component*mustNotInclude);
		void			SetLastOf(const PPanel&, const Distance&distance, const Component*mustNotInclude);

		bool			IsEnabled() const {return enabled;}
		bool			IsInactive() const {return !enabled;}
		const Distance&	GetOffset() const {return distanceFromReference;}
		const Components&GetReferences() const {return references;}
		void			AddReference(const PComponent&c);
		bool			RequiresFixedParentSize() const {return IsEnabled() && distanceFromReference.RequiresFixedParentSize();}
	private:
		bool			enabled = true;
		Distance		distanceFromReference;	//!< Distance to position extreme from closest reference. If no references are set, uses context (parent) edge instead. If no references or parent are available, gamewindow edge is used.
		Components		references;			//!< Zero or more reference components. If none are set, the respective context extreme is used. If more than one are set, the closest is used
	};

	class AnchorConfig
	{
	public:

		class Axis
		{
		public:
			Anchor		lower, upper;
			/**
			Configured spacing between lower and upper.
			Effective onlye of either lower or upper is not fixed
			*/
			Distance	extent=Distance::Disabled;
			AnchorAxis	myAxis;
			bool		isAutoCentering=false;

			bool		Stretches() const {return BothAnchorsAreOn() && !isAutoCentering;}
			bool		RequiresFixedParentSize() const {return lower.RequiresFixedParentSize() || upper.RequiresFixedParentSize() || extent.RequiresFixedParentSize() /*|| Stretches()*/;}	//stretching is valid
			bool		SetExtent(const Distance&);
			void		AnchorLower(const Distance&extent, const Distance&lowerOffset = Distance());
			void		AnchorLower(const Distance&extent, const PComponent&lowerReference, const Distance& lowerOffset = Distance());
			void		AnchorUpper(const Distance&extent, const Distance&upperOffset = Distance());
			void		AnchorUpper(const Distance&extent, const PComponent&upperReference, const Distance& upperOffset = Distance());
			void		AnchorUpperUnknown(const Distance&upperOffset = Distance());
			void		AnchorLowerUnknown(const Distance&lowerOffset = Distance());
			void		Fill(const Distance&lowerOffset = Distance(), const Distance&upperOffset = Distance());
			void		SetAutoCenter(const Distance&extent = Distance::Unknown) {isAutoCentering = true; this->extent = extent; lower.Disable(); upper.Disable();}
			void		VerifyIntegrity(const FVerification&, bool allowUnknownExtent) const;
			bool		BothAnchorsAreOff() const {return lower.IsInactive() && upper.IsInactive();}
			bool		BothAnchorsAreOn() const {return lower.IsEnabled() && upper.IsEnabled();}
			bool		IsFreeFloating() const {return BothAnchorsAreOff() && !isAutoCentering;}
			void		SetFreeFloating(const Distance&extent = Distance::Unknown);
			bool		HasExtentPlaceholder() const {return !Stretches() && extent.IsPlaceholder();}
			void		FillBetween(const PComponent&lowerC, const PComponent&upperC);
			float		GetMinimumEdge(float parentUpperEdge, const M::TFloatRange<>&rectRange) const;
			void		UpdateExtentIfEnabled(const Distance&d) {if (!extent.IsDisabled()) extent = d;}
		};



		Axis			axis[2];

		/**/			AnchorConfig()	{axis[AnchorAxis::Horizontal].myAxis = AnchorAxis::Horizontal; axis[AnchorAxis::Vertical].myAxis = AnchorAxis::Vertical;}
		void			VerifyIntegrity(const FVerification&check, bool allowUnknownExtent) const {axis[0].VerifyIntegrity(check,allowUnknownExtent); axis[1].VerifyIntegrity(check,allowUnknownExtent);}
		Axis&			GetHAxis() {return axis[AnchorAxis::Horizontal];}
		const Axis&		GetHAxis() const {return axis[AnchorAxis::Horizontal];}
		Axis&			GetVAxis() {return axis[AnchorAxis::Vertical];}
		const Axis&		GetVAxis() const {return axis[AnchorAxis::Vertical];}
		bool			HasExtentPlaceholders() const {return axis[0].HasExtentPlaceholder() || axis[1].HasExtentPlaceholder();}
		void			SetFreeFloating(const Distance&width = Distance::Unknown,const Distance&height = Distance::Unknown);
	};



	class AnchorModifier : public AnchorConfig
	{
	public:
		/**/			AnchorModifier(const AnchorModifier&)=delete;
		void			operator=(const AnchorModifier&)=delete;

		/**/			AnchorModifier(AnchorModifier&&);
		/**/			AnchorModifier(Component*cmp);
		/**/			~AnchorModifier();



		AnchorModifier&	Top(const Distance&upperOffset = Distance(), const Distance&height = Distance::Unknown);
		AnchorModifier&	Bottom(const Distance&lowerOffset = Distance(), const Distance&height = Distance::Unknown);
		AnchorModifier&	Left(const Distance&leftOffset = Distance(), const Distance&width = Distance::Unknown);
		AnchorModifier&	Right(const Distance&rightOffset = Distance(), const Distance&width = Distance::Unknown);
		AnchorModifier&	Beneath(const PComponent&, const Distance&upperOffset = Distance(), const Distance&height = Distance::Unknown);
		AnchorModifier&	Above(const PComponent&, const Distance&lowerOffset = Distance(), const Distance&height = Distance::Unknown);
		AnchorModifier&	RightOf(const PComponent&, const Distance&leftOffset = Distance(), const Distance&width = Distance::Unknown);
		AnchorModifier&	LeftOf(const PComponent&, const Distance&rightOffset = Distance(), const Distance&width = Distance::Unknown);
		AnchorModifier&	RightOfLastOf(const PPanel&, const Distance&leftOffset = Distance(), const Distance&width = Distance::Unknown);
		AnchorModifier&	LeftOfLastOf(const PPanel&, const Distance&rightOffset = Distance(), const Distance&width = Distance::Unknown);
		AnchorModifier&	BeneathLastOf(const PPanel&, const Distance&upperOffset = Distance(), const Distance&height = Distance::Unknown);
		AnchorModifier&	AboveLastOf(const PPanel&, const Distance&lowerOffset = Distance(), const Distance&height = Distance::Unknown);
		AnchorModifier&	RightOfSomeOf(const PPanel&, const FSelector&, const Distance&leftOffset = Distance(), const Distance&width = Distance::Unknown);
		AnchorModifier&	LeftOfSomeOf(const PPanel&, const FSelector&, const Distance&rightOffset = Distance(), const Distance&width = Distance::Unknown);
		AnchorModifier&	BeneathSomeOf(const PPanel&, const FSelector&, const Distance&upperOffset = Distance(), const Distance&height = Distance::Unknown);
		AnchorModifier&	AboveSomeOf(const PPanel&, const FSelector&, const Distance&lowerOffset = Distance(), const Distance&height = Distance::Unknown);
		AnchorModifier&	RightOfAllOf(const PPanel&, const Distance&leftOffset = Distance(), const Distance&width = Distance::Unknown);
		AnchorModifier&	LeftOfAllOf(const PPanel&, const Distance&rightOffset = Distance(), const Distance&width = Distance::Unknown);
		AnchorModifier&	BeneathAllOf(const PPanel&, const Distance&upperOffset = Distance(), const Distance&height = Distance::Unknown);
		AnchorModifier&	AboveAllOf(const PPanel&, const Distance&lowerOffset = Distance(), const Distance&height = Distance::Unknown);
		AnchorModifier&	FillHorizontally();
		AnchorModifier&	FillHorizontally(const Distance&leftOffset, const Distance&rightOffset);
		AnchorModifier&	FillHBetween(const PComponent&leftEdge, const PComponent&rightEdge);
		AnchorModifier&	FillVertically();
		AnchorModifier&	FillVertically(const Distance&lowerOffset, const Distance&upperOffset);
		AnchorModifier&	CenterVertically(const Distance&height = Distance::Unknown);
		AnchorModifier&	CenterHorizontally(const Distance&width = Distance::Unknown);

	private:
		Component		*parent;
	};

	extern PComponent	mousePressedComponent;	//component that caught the last mouse down event

	class ComponentLocation
	{
	public:
		#ifdef _DEBUG
			index_t		dbgMarked = 0;
		#endif

		const M::Rect<>&GetRect() const {return rect;}
		float			GetWidth() const {return rect.x.GetExtent();}
		float			GetHeight() const {return rect.y.GetExtent();}

	protected:
		virtual void	OnRectChange()		{};
		virtual void	OnHorizontalRectChange(){};
		virtual void	OnVerticalRectChange()	{};
		virtual void	OnWidthChange()		{};
		virtual void	OnHeightChange()	{};
		virtual void	OnTranslate(const M::TVec2<>&delta)	{};

		void			TranslateRect(const float2&delta)
		{
			M::Rect<> r = rect;
			r.Translate(delta);
			SetRect(r);
			OnTranslate(delta);
		}

		void			SetRect(const M::Rect<>&rect);
	private:
		static const constexpr float Lower = -10000.f, Upper = 10000.f;
		M::Rect<>			rect;
	};


	class Component : public std::enable_shared_from_this<Component>, public ComponentLocation
	{
		typedef ComponentLocation Super;
	protected:
		struct TPropagating
		{
			bool		isEnabled = true,
						isLit = true;

			/**/		TPropagating(bool isEnabled=true, bool isLit=true):isEnabled(isEnabled),isLit(isLit)	{}
		};

	private:
		TPropagating	localState,parentState;
		bool			containsMouse = false,
						active = false;
		WComponent		parent;
		WWindow			window;

		friend void		SignalScreenResize();

	protected:
		static void		Darken(const M::Rect<>&rect, float alpha=0.9f, bool flush=false);
		static void		RenderArea(const M::Rect<>&rect, bool lit, bool flip, const float3&backgroundTone, float lowOpacity=1, float highOpacity=1);
		static void		RenderBorder(const M::Rect<>&rect, const float3&backgroundTone, const M::Quad<bool>&edges, float lowOpacity=1, float highOpacity=1);

		virtual bool	ClientIsLit() const {return true;}


		virtual void	OnEnableChange()	{};

		/**
		Fetches the screen-relative height of the local component, given its current location (and width).
		Queried by the base implementation of UpdateDynamicSize().
		@return Designed component height relative to the current screen height. Return values < 0 to disable automatic resizing by Component::UpdateDynamicSize()
		*/
		virtual float	GetContentHeight() const {return -1;}
		virtual void	PropagateToChildren(const TPropagating&p) {}
		TPropagating	CreatePropagating() const {return TPropagating(IsEnabled(),IsLit());}
		virtual void	OnRectChange() override;
		virtual void	PropagateToChildren(const PWindow&)	{}

		static bool		Set(float&ref, float to);
		static bool		SetCenter(M::TFloatRange<>&ref, float to);
		
		virtual void	SetLocation(const M::Rect<>&loc);

		AnchorConfig	anchorConfig;
	public:	//do not call outside container classes:

		virtual void	OnScreenSizeOrUIScaleChange()	{}


	public:


		/**/			Component();
		virtual			~Component();

		bool			IsEnabled() const {return localState.isEnabled && parentState.isEnabled;}
		bool			IsLit() const {return ClientIsLit() && parentState.isLit;}
		bool			IsActive() const {return active;}

			
		bool			SizeChangesDynamically(int axis) const {return anchorConfig.axis[axis].Stretches();}
		bool			WidthChangesDynamically() const {return anchorConfig.GetHAxis().Stretches();}
		bool			HeightChangesDynamically() const {return anchorConfig.GetVAxis().Stretches();}

		
		bool			SetWidth(const Distance&w)
		{
			if (anchorConfig.GetHAxis().SetExtent(w))
			{
				InvalidateAutoLayout();
				return true;
			}
			return false;
		}

		bool			SetHeight(const Distance&h)
		{
			if (anchorConfig.GetVAxis().SetExtent(h))
			{
				InvalidateAutoLayout();
				return true;
			}
			return false;
		}
		


		/**
		Updates the enabled-state of the local component
		@return true on change, false otherwise
		*/
		bool			SetEnabled(bool newEnabled);

		bool			ContainsMouse() const	{return containsMouse;}
		void			MoveTopTo(float h);


		virtual void	SetActive(bool isActive) {active = isActive;}
		virtual bool	DetectEnabledState() {return false;}
		
		virtual PComponent	OnMouseDown() {return PComponent();}
		virtual WheelRs	OnMouseWheel(short) {return ContainsMouse() ? WheelRs::Ignored : WheelRs::NotContained;}
		virtual void	OnMouseUp()	{}
		virtual void	OnMouseMove(const M::TVec2<>&mouseAt, bool clipAreaContainsMouse);
		
		virtual bool	UpdateDynamicSize();

		virtual void	Render();

		virtual bool	ResolveAnchorAxis(AnchorAxis axis, const M::Rect<>&context);

		virtual void	Translate(const float2&delta)	{TranslateRect(delta); /*InvalidateAnchorResolution();*/}
		virtual bool	HasComponent(const PComponent&c) const {return c.get() == this;}


		void			SetParent(const PWindow&w, const PComponent&c);
		PComponent		GetParent() const {return parent.lock();}
		virtual PWindow	GetWindow() {return window.lock();}
		virtual PConstWindow	GetWindow() const {return window.lock();}

		bool			FullAutoLayout(const M::Rect<>&context);


		void			InvalidateAutoLayout();

		AnchorModifier	Anchor() {return this;}
		const AnchorConfig&GetAnchorConfig() const {return anchorConfig;}

		virtual void	VerifyAnchorIntegrity() const;
		virtual void	SetAnchorConfig(const AnchorConfig&cfg);

		/**
		Updates the left and right anchors such that the local component maintains the given width
		@param w Target component width. If @a type is AbsolutePixels, then w must be specified in pixels, otherwise any value in [0,1]
		*/
		Component&		SetCenteredWidth(const Distance&width);
		Component&		FillHorizontally(const Distance&sideSpacing);
		Component&		FillVertically(const Distance&verticalSpacing);
		Component&		AnchorTop(const Distance& topSpacing);

		void			AcceptStateFromParent(const TPropagating&p)	{parentState = p; PropagateToChildren(CreatePropagating());}
		void			AcceptParentWindow(const PWindow&w);
	};



	class Image : public Component
	{
		typedef Component	Super;
		static DX11::BaseInstructionBlock	texturedQuad;
		bool			autoSetHeight = true;
		DX11::Texture	texture;
		
		static void		StaticDestroy();
	public:
		static void		StaticCreate();

		void			SetAutoHeight(bool b)	{if (autoSetHeight == b) return; ASSERT__(!b || !anchorConfig.GetVAxis().RequiresFixedParentSize()); autoSetHeight = b; InvalidateAutoLayout();}
		void			SetTexture(const DX11::Texture&tex) {texture = tex;InvalidateAutoLayout();}

		virtual void	Render() override;
		//virtual bool	HandleClick() override {return ContainsMouse();}
	protected:
		virtual float	GetContentHeight() const override;

	};



	class Box : public Component
	{
	protected:
		void			BaseRender(bool lit, bool flipped, bool fill=true, bool border=true);
	public:
		typedef Component	Super;

		M::Quad<bool>	edges = M::Quad<bool>(true);
		bool			fillBackground = true,
						renderBorder = true;
		
		float			bottomOpacity = 1.f,
						topOpacity = 1.f;
		float3			backgroundTone = float3(1);

		void			SetBackgroundTransparent()
		{
			fillBackground = false;
			renderBorder = false;
		}

		void			SetBorderBackground()
		{
			fillBackground = false;
			renderBorder = true;
		}
		virtual void	Render() override	{Super::Render(); BaseRender(false,false,fillBackground,renderBorder);}
	};

	/**
	Multi-plot drawing component.
	May display plots as percentual stacks, or overlapping lines
	*/
	class MultiPlot : public Component
	{
		typedef Component	Super;
	
		/**
		Set of X-Y-coordinates that form the final plot
		*/
		class Plot
		{
		public:

			/**
			Adds a new node to the plot.
			@param x X slot of the height value. Must be greater than any nodes previously added to this plot.
					Intermediate nodes are automatically added if missing. The first node added this way defines the left most sampling point.
			*/
			void	AddNode(MultiPlot*owner, const index_t x, const float y);

			friend void swap(Plot&a, Plot&b)
			{
				using std::swap;
				swap(a.surface,b.surface);
				swap(a.lines,b.lines);
				swap(a.color,b.color);
				swap(a.start,b.start);
				swap(a.data,b.data);
				swap(a.changed,b.changed);
			}

			float	Sample(index_t x) const;

		
			void	RenderSurfaces()	const;
			void	RenderLines()	const;
			void	Replot(const M::Rect<>&componentRect, const M::Rect<>&dataRect, bool addBottomLine, bool stack);


			void	MakeVertex(const float x, const float y);

			struct TPoint
			{
				float	yRaw,
						yMin, yMax;
			};

			SurfaceRenderer::Baked	surface;
			LineRenderer::Baked		lines;
			float3					color;
			index_t					start = InvalidIndex;
			Buffer0<TPoint>			data;
			bool					changed = true;

		};

	public:
		/**/	MultiPlot()
		{}

		count_t	CountPlots() const
		{
			return plots.Count();
		}

		void	ClearPlots()
		{
			plots.Clear();
			xRange = M::TIntRange<index_t>::Invalid;
			stackedYRange = M::TFloatRange<>::Invalid;
			rawYRange = M::TFloatRange<>::Invalid;
			SignalAxesChanged();
		}

		/**
		Adds a new node to the current plot.
		@param x X slot of the height value. Must be greater than any nodes previously added to the current plot.
				Intermediate nodes are automatically added if missing. The first node added this way defines the left most sampling point.
		@param y Y coordinate of the point. Arbitrary. Must not be NaN or +/-INF
		*/
		void	AddNode(const index_t x, const float y)
		{
			plots.Last().AddNode(this,x,y);
		}

		/**
		Adds a new plot
		*/
		void	NextPlot(const float3&color)
		{
			Plot&rs = plots.Append();
			rs.color = color;
		}


		virtual void	Render() override;

		void			SignalAxesChanged()	{axesChanged = true;}
		float			SampleStackedHeight(index_t) const;

		bool			IsStacked() const {return isStack;}
		void			SetStacked(bool stacked) {if (stacked == isStack) return; isStack = stacked;  Replot();}

		static float	GetAxisStep(float rangeExtent);
		static bool		GetAxisStepWidthAndCount(float rangeExtent ,float&outStep, count_t&outCount);

		/**
		For arithmetic testing purposes. Runs GetAxisStepWidthAndCount() on a 0-range but without the checks.
		The resulting failure highlights the differences between processor and target architecture as a NaN->count_t
		cast produces unpredictable/different results.
		*/
		static void		DoAxisCalculationAndCrash();
	protected:
		virtual void	OnRectChange() override	{Replot();}
		virtual void	OnScreenSizeOrUIScaleChange() override	{Replot();}

		void			Replot();
	private:

		void			BeginLabelPrint();
		void			EndLabelPrint();

		struct TLabel
		{
			String		caption;
			float2		coords;
		};
		Buffer0<TLabel>	labels;

		bool			axesChanged = false;
		LineRenderer::Baked	axes;
		M::Rect<>			GetPlotScope() const;
		M::Rect<>			GetPlotRect() const;
		float			rightSpace = 0.1f;
		Buffer0<Plot>	plots;
		bool			isStack = false;
		M::TIntRange<index_t>	xRange = M::TIntRange<index_t>::Invalid;
		M::TFloatRange<>		stackedYRange = M::TFloatRange<>::Invalid,
							rawYRange = M::TFloatRange<>::Invalid;
		friend class Plot;
	};


	class BaseInput : public Box
	{
		typedef Box	Super;
		M::Rect<>			core;

	protected:

		Engine::Keyboard::CharacterBuffer	backup;

		static void			OnCharHandled(char);

		virtual void		SetInputActive(bool isActive);
		virtual const char*	GetAllowed() const {return nullptr;}
		virtual StringRef	GetTextRef();
		virtual float		GetContentHeight() const override;
	public:

		/**
		Triggered when the user confirms the input (via enter/return)
		*/
		std::function<void()>	onEnter;
		/**
		Triggered when special keys are pressed while this input receives characters:
		F1-F12, Backspace, Delete, Print, Home, End, Page up and down, Space, CapsLock, Control, Alt, Arrow keys
		*/
		std::function<void(Key::Name)> onSpecialKey;

		virtual void	Render() override;
		virtual void	SetActive(bool isActive) override {SetInputActive(isActive); Super::SetActive(isActive);}

	};

	class Input : public BaseInput
	{
		typedef BaseInput	Super;
		String				allowed = GetStandardAlphabet();

	protected:
		virtual const char*	GetAllowed() const override {return allowed.c_str();}
	public:
		static const char*	GetStandardAlphabet()  {return "abcdefghijklmnopqrstuvwxyzäöüABCDEFGHIJKLMNOPQRSTUVWXYZÄÖÜ0123456789_- ";};

		String				GetText();
		void				SetText(const String&);
		void				SetAllowedAlphabet(const String&);
	};

	class PasswordInput : public BaseInput
	{
		typedef BaseInput	Super;
		static StringBuffer	placeholder;
	protected:
		virtual StringRef	GetTextRef() override;
	public:
		const char*			GetText();
		void				WipeInput();
	};

	class TextField : public Box
	{
	private:
		#ifdef _DEBUG
			float		wrappedAtWidth=0;
		#endif

		Array<String>	rawText;
		Buffer0<String>	wrappedText;
		Array<float2>	lineAt;
		float4			color = float4(1,1,1,1);
		float			textHeight = 1.f;
		float			zoom = 1.f;
		bool			widthLockedToIdeal=false;
		bool			heightLockedToIdeal=true;
		M::Quad<float>		padding;	//Vertical spacing is measured in multiples of line-heights, horizontal in multiples of X-letter-widths

		bool			UpdateIdealWidth();

		void			ReWrapSetHeight()
		{
			ReWrap();
			if (heightLockedToIdeal) 
				SetHeight(textHeight);
		}

		void			HandleTextChange();

	protected:
		void			RenderText(bool lit);

		virtual float	GetIdealWidth() const;
		virtual void	OnTextChange() {};
		virtual void	OnWidthChange() {ReWrapSetHeight();}
		virtual void	OnScreenSizeOrUIScaleChange() override
		{
			Super::OnScreenSizeOrUIScaleChange();
			ReWrapSetHeight();
		}

		virtual float	GetContentHeight() const override;
		void			ReWrap();
	public:
		typedef Box	Super;

		bool			alignCenter = true;
		
		/**/			TextField()
		{
			padding.bottom = padding.top = 0.75f;
			padding.left = padding.right = 1;
			anchorConfig.GetVAxis().AnchorUpperUnknown();	//since heightLockedToIdeal is set by default
		}

		/**
		Sets the extra spacing at the top and bottom in multiples of line heights. Default is 0.75
		*/
		void			SetVerticalPadding(float topAndBottom)	{SetVerticalPadding(topAndBottom,topAndBottom);}
		void			SetVerticalPadding(float top, float bottom)	{padding.top = top; padding.bottom = bottom; ReWrapSetHeight();}
		void			SetHorizontalPadding(float leftAndRight)	{SetHorizontalPadding(leftAndRight,leftAndRight);}
		void			SetHorizontalPadding(float left, float right)	{padding.left = left; padding.right = right; ReWrapSetHeight();}
		void			ScaleVerticalPadding(float factor)			{padding.bottom *= factor; padding.top *= factor; ReWrapSetHeight();}
		void			SetPadding(const M::Quad<float>&padding)		{this->padding = padding; if (this->widthLockedToIdeal) SetToIdealWidth(); ReWrapSetHeight();}
		const M::Quad<float>&GetPadding() const {return padding;}

		void			SetColor(const float4&color)	{this->color = color;}
		void			SetColor(const float3&color, float alpha = 1.f)	{this->color.rgb = color; this->color.a = alpha;}
		void			SetColor(float r, float g, float b, float a = 1.f)	{this->color = float4(r,g,b,a);}
		void			SetLocalFontScale(float zoom)	{this->zoom = zoom; ReWrap(); UpdateDynamicSize();OnTextChange();}
		float			GetLocalFontScale() const {return zoom;}
		float			GetTotalFontScale() const;

		void			SetText(const String&string);
		void			SetText(const Array<String>&string);

		const Array<String>&GetText() const {return rawText;}

		void			SetToIdealWidth();

		float			GetTextHeight() const {return textHeight;}

		void			LockToIdealWidth(bool doLock=true)	{DBG_ASSERT__(!WidthChangesDynamically()); widthLockedToIdeal = doLock; if (doLock) SetToIdealWidth();}
		bool			IsLockedToIdealWidth() const {return widthLockedToIdeal;}
		void			LockToIdealHeight(bool doLock=true)	{DBG_ASSERT__(!HeightChangesDynamically()); heightLockedToIdeal = doLock; if (doLock) SetHeight(textHeight);}
		bool			IsLockedToIdealHeight() const {return heightLockedToIdeal;}

		virtual void		SetAnchorConfig(const AnchorConfig&cfg)	override 
		{
			Super::SetAnchorConfig(cfg);
			if (WidthChangesDynamically())
				ASSERT__(!widthLockedToIdeal);
			if (HeightChangesDynamically())
				ASSERT__(!heightLockedToIdeal);
		}

		virtual void	VerifyAnchorIntegrity() const override 
		{
			ASSERT__(!WidthChangesDynamically() || !widthLockedToIdeal);
			ASSERT__(!HeightChangesDynamically() || !heightLockedToIdeal);
			Super::VerifyAnchorIntegrity();
		}

		virtual bool	UpdateDynamicSize() override;
		virtual void	Render() override;
	};


	class Label : public TextField
	{
		typedef TextField	Super;

	public:
		/**/			Label()
		{
			Super::SetBackgroundTransparent();
		}
		/**/			Label(const String&text, float scale=1.f)
		{
			Super::SetBackgroundTransparent();
			Super::SetLocalFontScale(scale);
			SetText(text);
			anchorConfig.GetHAxis().AnchorLowerUnknown();
			SetToIdealWidth();
		}
	};

	class LinkLabel : public Label
	{
		typedef Label Super;
	public:

		typedef std::function<void()>	FOnPress;

		FOnPress		onPress;
			
		/**/			LinkLabel()	{};
		/**/			LinkLabel(const String&text, float scale=1.f, const FOnPress&onPress = FOnPress()):Super(text,scale),onPress(onPress){}

		virtual void	OnMouseMove(const M::TVec2<>&mouseAt, bool clipAreaContainsMouse) override;
		virtual PComponent	OnMouseDown() override {return ContainsMouse() ? shared_from_this() : nullptr;}

		virtual void	OnMouseUp() override
		{
			if (onPress)
				onPress();
		}
	};

	class Button : public TextField
	{
	public:
		typedef std::function<void()>	FTrigger;
	private:
		typedef TextField	Super;

		FTrigger		onPress;
		std::function<bool()>	check;

		bool			alwaysDown = false;
		bool			flipWhenAlwaysDown = true;

	protected:
		virtual bool	ClientIsLit() const override {return IsActive() && IsEnabled() && ContainsMouse();}

	public:
		/**/			Button()
		{
			SetText("[-]");
		}

		/**/			Button(const String&text, std::function<void()> onPress = std::function<void()>(), std::function<bool()> check = std::function<bool()>() )
		{
			this->onPress = onPress;
			this->check = check;
			SetText(text);
			anchorConfig.GetHAxis().AnchorLowerUnknown();
			SetToIdealWidth();
		}


		void			SetAlwaysDown(bool down) {alwaysDown = down;}
		bool			IsAlwaysDown() const {return alwaysDown;}


		/**
		Updates the trigger function executed with the local button is pressed
		@return Previous trigger function
		*/
		FTrigger		SetTriggerFunction(const FTrigger&event)
		{
			FTrigger rs = onPress;
			onPress = event;
			return rs;
		}

		const FTrigger&	GetTriggerFunction() const
		{
			return onPress;
		}

		void			Trigger()	const
		{
			if (onPress)
				onPress();
		}



		void			SetCheck(const std::function<bool()>&check)
		{
			this->check = check;
			DetectEnabledState();
		}

		virtual bool	DetectEnabledState() override
		{
			//String set = baseLabel;
			bool changed =false;
			if (check)
			{
				if (!check())
				{
				
					//set += " (not right now)";
					changed = SetEnabled(false);
				}
				else
					changed = SetEnabled(true);
			}

			//Super::SetText(set);
			return changed;
		}

		virtual void Render() override;

		virtual PComponent	OnMouseDown() override
		{
			DetectEnabledState();
			if (IsEnabled() && ContainsMouse())
			{
				return shared_from_this();
			}
			return PComponent();
		}

		virtual	void			OnMouseUp() override
		{
			if (ContainsMouse())
			{
				DetectEnabledState();
				if (IsEnabled() && onPress)
				{
					onPress();
				}
			}
		}
	};

	class Checkbox : public TextField
	{
		typedef TextField	Super;

		std::function<void(bool)>	onChanged;
		std::function<bool()>		check;
		bool						isChecked = false;

		//String					baseLabel;
//		TextField::SetText;

		void			Setup()
		{
			Super::alignCenter = false;
			Super::ScaleVerticalPadding(0.5f);
			Super::SetBackgroundTransparent();
			auto padding = Super::GetPadding();
			padding.left = 2;
			Super::SetPadding(padding);
			anchorConfig.GetHAxis().AnchorLowerUnknown();
		}

	protected:
		virtual bool	ClientIsLit() const override {return IsActive() && IsEnabled() && ContainsMouse();}
		virtual float	GetIdealWidth() const override;
		virtual float	GetContentHeight() const override;
	public:
		/**/			Checkbox()
		{
			Setup();
			SetText("");
		}

		/**/			Checkbox(const String&text, bool checked, std::function<void(bool)> onChanged = std::function<void(bool)>(), std::function<bool()> check = std::function<bool()>() )
		{
			Setup();
			this->onChanged = onChanged;
			this->check = check;
			isChecked = checked;
			SetText(text);
			SetToIdealWidth();
		}

		void			SetChecked(bool checked, bool doSignal = true)
		{
			if (checked == isChecked)
				return;
			isChecked = checked;
			if (doSignal)
				onChanged(isChecked);
		}

		bool			IsChecked() const {return isChecked;}

		void			SetOnChange(const std::function<void(bool)>&onChanged)
		{
			this->onChanged = onChanged;
		}

		void			SetCheck(const std::function<bool()>&check)
		{
			this->check = check;
			DetectEnabledState();
		}

		virtual bool	DetectEnabledState() override
		{
			//String set = baseLabel;
			bool changed =false;
			if (check)
			{
				if (!check())
				{
				
					//set += " (not right now)";
					changed = SetEnabled(false);
				}
				else
					changed = SetEnabled(true);
			}

			//Super::SetText(set);
			return changed;
		}

		virtual void Render() override;

		virtual PComponent	OnMouseDown() override
		{
			DetectEnabledState();
			if (IsEnabled() && ContainsMouse())
			{
				return shared_from_this();
			}
			return PComponent();
		}

		virtual	void			OnMouseUp() override
		{
			if (ContainsMouse())
			{
				DetectEnabledState();
				if (IsEnabled())
				{
					isChecked = !isChecked;
					onChanged(isChecked);
				}
			}
		}

	};


	class ScrollButton : public Button
	{
		typedef Button	Super;
	public:
		enum direction_t
		{
			Up,
			Bottom
		};
		const direction_t	direction;



		/**/			ScrollButton(direction_t dir,std::function<void()> onPress = std::function<void()>()):Super("",onPress), direction(dir)
		{}

		virtual void	Render() override
		{
			bool lit = IsLit();
			bool flipped = mousePressedComponent.get() == this;
			Super::BaseRender(lit && !flipped,lit && flipped);
			RenderArrow(GetRect(),direction,lit);
		}

		static void		RenderArrow(const M::Rect<>&, direction_t, bool lit);

		virtual void	SetLocation(const M::Rect<>&loc) override {Super::SetLocation(loc);}

	};



	class Slider : public Component
	{
		typedef Component	Super;

	protected:
		virtual void		OnScreenSizeOrUIScaleChange() override;
	public:
		typedef std::function<void(float&)>	FOnChange;

		/**/				Slider();
		virtual PComponent	OnMouseDown() override;
		virtual WheelRs		OnMouseWheel(short) override;
		virtual void		OnMouseUp() override;
		virtual void		OnMouseMove(const M::TVec2<>&mouseAt, bool clipAreaContainsMouse) override;
		virtual void		Render() override;
		void				Setup(float current, float max, float slideStep) {this->current = current; this->max = max; this->slideStep = slideStep;numMarkers = markersEvery ? 1+(max / markersEvery) : 0;}
		void				SetupMarkers(float markerStep, bool show)	{this->markersEvery = markerStep; showMarkers = show; numMarkers = markersEvery ? 1+(max / markersEvery) : 0;}
		float				GetMax() const {return max;}
		float				GetCurrent() const {return current;}
		void				SetOnChange(const FOnChange&f)	{onChange = f;}
		virtual float		GetContentHeight() const override	{return GetHeight();}

		static float		GetThumbPixelWidth();
		static float		GetPixelHeight();

		float				GetHeight() const	{return relativeHeight;}
		float				GetThumbWidth() const {return relativeThumbWidth;}

		//static constexpr float Height = 0.03f;
		//static constexpr float BarWidth = 0.03f;
	private:
		float				current=0,max=1,		//!< Slider maximum value (inclusive)
							slideStep=1;
		M::TFloatRange<>		dragRange;
		M::Rect<>				barBack,	//!< Shadow behind slider
							bar;		//!< Actual grabbable bar
		bool				showMarkers=true;
		count_t				numMarkers=0;
		float				markersEvery=1.f;
		float				relativeHeight=1,relativeThumbWidth = 1;
		bool				outlineContainsMouse = false,
							barContainsMouse = false,
							dragging = false;
		bool				mouseDown = false;//,slideLeftOnClick =false;
		float				lastMouseX = 0;
		float				dragRelative = 0;
		FOnChange			onChange;
		bool				changed =false;

	};
	typedef std::shared_ptr<Slider>	PSlider;


	class Panel : public Box
	{
		typedef Box Super;

		struct TScrollbar
		{
			bool		visible=false;
			float		scrollMax=1.f;
			float		viewTop=0.f,	//!< counting top (=0) down (>=0)
						targetViewTop=0.f;
			float		viewSize = 0.2f;
			M::Rect<>	outline,	//!< Shadow behind scrollbar. Does not contain buttons, but does contain bar
						bar;		//!< Actual grabbable bar
			PScrollButton		up,down;
			Panel		*owner=nullptr;
			bool		outlineContainsMouse = false,barContainsMouse = false,dragging = false;
			bool		mouseDown = false,scrollDownOnClick =false;
			float		dragRelative = 0;

			void		ScrollToTop();
			void		ScrollToBottom();
			PComponent	OnMouseDown();
			WheelRs		OnMouseWheel(short);
			void		OnMouseUp();
			void		OnMouseMove(const M::TVec2<>&mouseAt, bool clipAreaContainsMouse);
			void		Enable(Panel*,const M::Rect<>&rect);
			void		Render(const M::Rect<>&rect);
			float		GetOffset() const {return visible ? -viewTop : 0;}

			static float	GetPixelWidth();
			static float	GetScreenWidth();
			static float	GetButtonScreenHeight();

			//static constexpr float Width = 0.03f;
			static constexpr float ContentSpacing = 0.006f;
		};

		TScrollbar		vscroll;
		
	protected:
		virtual void	PropagateToChildren(const PWindow&w)	override
		{
			foreach (components,c)
				(*c)->AcceptParentWindow(w);
		}

		virtual void	PropagateToChildren(const TPropagating&p) override
		{
			foreach (components,c)
				(*c)->AcceptStateFromParent(p);
		}
		void			RenderChildren()
		{
			foreach (components,c) 
				(*c)->Render();
		}

		Buffer0<PComponent>	components;

		/**
		Queries whether or not the vertical scrollbar is currently visible.
		*/
		bool			ScrollbarIsVisible() const
		{
			return vscroll.visible;
		}

		/**
		Queries the width+horizontal offset of any currently visible scrollbar, 0 if invisible.
		*/
		float			GetVisibleScrollbarContentExpansion() const
		{
			return vscroll.visible ? TScrollbar::GetScreenWidth()+TScrollbar::ContentSpacing : 0;
		}

		virtual void	OnScreenSizeOrUIScaleChange() override
		{
			Super::OnScreenSizeOrUIScaleChange();
			foreach (components,c)
				(*c)->OnScreenSizeOrUIScaleChange();
		}

		float			autoHeightSpacing = 0.f;//0.015f;
		/**
		Maximum height allocatable by client components.
		A scrollbar will be rendered if maxHeight is exceeded
		*/
		float			maxHeight = 0.8f;
	public:
		/**/			Panel()	{topOpacity = 0.75f; bottomOpacity = 0.75f; Anchor().FillHorizontally().Top();}
		bool			DeterminesHeightAutomatically() const {return autoHeight;}
		void			SetAutomaticHeight(bool b);
		float			GetAutoHeightSpacing() const {return autoHeightSpacing;}
		void			SetAutoHeightSpacing(float s) {if (s == autoHeightSpacing) return; autoHeightSpacing = s; this->InvalidateAutoLayout();}
		float			GetMaxAutoHeight() const {return maxHeight;}
		void			SetMaxAutoHeight(float h) {if (h == maxHeight) return; maxHeight = h; InvalidateAutoLayout();}

		void			ScrollToBottom()	{vscroll.ScrollToBottom();}
		void			ScrollToTop()	{vscroll.ScrollToTop();}

		virtual bool	DetectEnabledState() override;
		
		virtual void	SetActive(bool isActive) override;
		virtual bool	UpdateDynamicSize() override;
		virtual void	Render() override;
		virtual bool	ResolveAnchorAxis(AnchorAxis axis, const M::Rect<>&context) override;
		virtual void	Translate(const float2&delta)	{Super::Translate(delta); foreach (components,c) (*c)->Translate(delta);}

		virtual bool	HasComponent(const PComponent&c) const override {if (c.get() == this) return true; foreach (components, c2) if ((*c2)->HasComponent(c)) return true; return false;}
		virtual WheelRs	OnMouseWheel(short) override;
		virtual PComponent	OnMouseDown() override;
		virtual void	OnMouseUp() override {vscroll.OnMouseUp();}
		virtual void	OnMouseMove(const M::TVec2<>&mouseAt, bool clipAreaContainsMouse) override;


		virtual void		VerifyAnchorIntegrity() const override;


		virtual const Buffer0<PComponent>&		GetReferences() const { return components; }

		virtual bool		HasComponents() const
		{
			return components.IsNotEmpty();
		}

		virtual count_t		CountComponents() const
		{
			return components.Count();
		}

		/**
		Adds a new component to the local panel.
		The component must not be child of some other panel.
		The component must also not reference any components in its anchor references that are not already child of the same window (not necessarily of the local panel).
		These constraints are checked and lead to program termination if found in violation
		@return true if the component was added, false if it is null, or already a member
		*/
		virtual bool		AddComponent(const PComponent&c);
		virtual PComponent	AddNewComponent(Component*c)
		{
			ASSERT_NOT_NULL__(c);
			PComponent comp(c);
			ASSERT__(AddComponent(comp));
			return comp;
		}

		virtual bool		RemoveComponent(const PComponent&c)
		{
			if (!c || c->GetParent().get() != this)
				return false;
			ASSERT__(components.FindAndErase(c));
			c->SetParent(PWindow(),PComponent());
			return true;
		}
		virtual bool		ClearComponents()
		{
			if (components.IsNotEmpty())
			{
				foreach (components,c)
				{
					(*c)->SetParent(PWindow(), PComponent());
				}
				components.Clear();
				return true;
			}
			return false;
		}
		template <typename Component>
		std::shared_ptr<Component>		AnchorBelowPrevious(Component*newComponent, const Distance& distanceFromSides =  0.015f, const Distance& distanceToPrevious = 0.015f)
		{
			std::shared_ptr<Component> c(newComponent);
			AnchorBelowPrevious(c,distanceFromSides,distanceToPrevious);
			return c;
		}
		void			AnchorBelowPrevious(const PComponent&newComponent, const Distance& distanceFromSides = 0.015f, const Distance& distanceToPrevious = 0.015f);


	private:
		bool			autoHeight = true;


	};

	class ButtonPanel : public Panel
	{
		typedef Panel	Super;

		std::function<void()>	onPress;
		std::function<bool()>	check;

		bool					active = false;
		int						lastLitTriState = -1;

	protected:
		virtual bool	ClientIsLit() const override {return active && IsEnabled() && ContainsMouse();}
		bool	OnComponentsChanged() {lastLitTriState = -1; return true;};

	public:
		/**/			ButtonPanel()	{topOpacity = 1.f; bottomOpacity = 1.f;}

		virtual void	SetActive(bool isActive) override
		{
			this->active = isActive;
			Super::SetActive(isActive);
		}

		virtual bool		AddComponent(const PComponent&c) override
		{
			return Super::AddComponent(c) && OnComponentsChanged();
		}
		virtual bool		RemoveComponent(const PComponent&c) override
		{
			return Super::RemoveComponent(c) && OnComponentsChanged();
		}
		virtual bool		ClearComponents() override
		{
			return Super::ClearComponents() && OnComponentsChanged();
		}



		void			SetTriggerFunction(const std::function<void()>&event)
		{
			onPress = event;
		}

		void			SetCheck(const std::function<bool()>&check)
		{
			this->check = check;
			DetectEnabledState();
		}

		virtual bool	DetectEnabledState() override
		{
			bool changed = false;
			if (check && !check())
			{
				changed |= SetEnabled(false);
			}
			else
				changed |= SetEnabled(true);
			//if (changed)
			//	Super::PropagateStateToChildren();	//implicitly done
			return Super::DetectEnabledState() || changed;
		}

		virtual void	OnMouseMove(const M::TVec2<>&mouseAt, bool clipAreaContainsMouse) override
		{
			Super::OnMouseMove(mouseAt,clipAreaContainsMouse);
			bool lit = IsLit();
			int newLit = lit?1:0;
			if (lastLitTriState == -1 || lastLitTriState != newLit)
			{
				lastLitTriState = newLit;
				PropagateToChildren(CreatePropagating());
			}
		}



		virtual void Render() override
		{
			bool lit = IsLit();
			bool flipped = mousePressedComponent.get() == this;
			Super::BaseRender(lit && !flipped,lit && flipped);
			Super::RenderChildren();
		}

		virtual PComponent	OnMouseDown() override
		{
			DetectEnabledState();
			if (IsEnabled() && ContainsMouse())
			{
				for (index_t i = components.Count()-1; i < components.Count(); --i)
				{
					PComponent c = components[i]->OnMouseDown();
					if (c)
						return c;
				}
				return shared_from_this();
			}
			return PComponent();
		}

		virtual	void			OnMouseUp() override
		{
			if (ContainsMouse())
			{
				DetectEnabledState();
				if (IsEnabled() && onPress)
				{
					onPress();
				}
			}
		}



	};


	class TabBar : public Panel
	{
		typedef Panel Super;
	public:
		typedef std::function<void()>	FOnTabChange;

		/**/			TabBar();
		/**
		Adds a new tab to the bar, to the right of the last added tab (if any).
		If this is the first tab, then it is selected immediately.
		@param caption Caption to use for the tab. May be empty or already in use (but probably should not)
		@return Handle to the panel of the new tab. The panel is anchored to fill most of the local collection area
		*/
		PButton			AddTab(const String&caption);
		/**
		Retrieves the index of the specified tab.
		This method may be used to determine if a button belongs to this bar.
		@param tabButton Button of the tab to find the index of
		@return Index of the found tab, or InvalidIndex if it was not found
		*/
		index_t			GetTabIndex(const PButton&tabButton)	const;
		/**
		Fetches the button associated with the currently active tab.
		May return an empty pointer if the bar is currently empty.
		*/
		PButton			GetActiveTab() const;
		/**
		Fetches the index of the active tab.
		May return InvalidIndex if the collection is currently empty.
		*/
		index_t			GetActiveTabIndex() const	{return activeIndex;}
		/**
		Changes the currently active tab to the one associated with the specified button.
		The method triggers a fatal exception if the specified panel is neither null nor found.
		Nothing changes or is triggered if the specified tab is already active or null.
		@param tabPanel Panel of the tab to activate
		@param triggerCallback Set true to invoke any set tab-change callback function
		*/
		void			SetActiveTab(const PButton&tabButton, bool triggerCallback=true)		{if (tabButton) SetActiveTab(GetTabIndex(tabButton),triggerCallback);}
		/**
		Defines a new callback function to call whenever the active tab changes.
		*/
		void			SetOnTabChange(const FOnTabChange&onChange)	{onTabChange = onChange;}

		/**
		Removes all tabs in the local collection.
		The operation may trigger an on-tab-change event.
		@return true if any tabs were removed, false otherwise
		*/
		bool			ClearTabs()	{if (!Super::HasComponents()) return false; Super::ClearComponents(); active.reset(); activeIndex = InvalidIndex; OnTabChange(); return true;}

		bool			IsEmpty() const {return !Super::HasComponents();}
		bool			IsNotEmpty() const {return Super::HasComponents();}
		count_t			CountTabs() const {return Super::CountComponents();}
	
	protected:
		virtual void	OnTabChange();

	private:
		static void		ConfigureInactive(const PButton&);
		static void		ConfigureActive(const PButton&);
		virtual bool	AddComponent(const PComponent&c) override	{return false;}
		virtual bool	RemoveComponent(const PComponent&c) override {return false;}
		virtual bool	ClearComponents() override {return false;}

		FOnTabChange	onTabChange;
		PButton			active;
		index_t			activeIndex = InvalidIndex;

		void			SetActiveTab(index_t, bool triggerCallback);
	};

	class TabCollection : public Panel
	{
		typedef Panel Super;
	public:
		typedef std::function<void()>	FOnTabChange;

		/**
		Adds a new tab to the collection to the right of the local collection.
		If this is the first tab, then it is selected immediately.
		@param caption Caption to use for the tab. May be empty or already in use (but probably should not)
		@return Handle to the panel of the new tab. The panel is anchored to fill most of the local collection area
		*/
		PPanel			AddTab(const String&caption);
		/**
		Retrieves the index of the specified tab.
		This method may be used to determine if a panel belongs to this collection.
		@param tabPanel Panel of the tab to find the index of
		@return Index of the found tab, or InvalidIndex if it was not found
		*/
		index_t			GetTabIndex(const PPanel&tabPanel)	const;
		
		#if 0	//too complicated for now. may add later
		/**
		Changes the order of tabs such that the specified tab is moved to the specified location.
		The tab is effectively removed from its old location and inserted into the new location.
		If it was previously active, then it still is now.
		The method triggers a fatal exception if the specified panel is not found.
		@param tabPanel Panel of the tab to set the index of
		@param to The index to move the panel to
		*/
		void			SetTabIndex(const PPanel&tabPanel, index_t to);
		#endif /*0*/

		/**
		Updates the caption associated with the specified tab panel.
		The method triggers a fatal exception if the specified panel is not found.
		*/
		void			SetCaptionOf(const PPanel&, const String&caption);
		/**
		Retrieves the caption of the tab associated with the specified panel.
		The method triggers a fatal exception if the specified panel is not found.
		*/
		const String&	GetCaptionOf(const PPanel&)	const;
		/**
		Fetches the panel associated with the currently active tab.
		May return an empty pointer if the collection is currently empty.
		*/
		PPanel			GetActiveTab() const;
		/**
		Fetches the caption of the active tab.
		May return an empty string if the collection is currently empty.
		*/
		const String&	GetActiveCaption() const;
		/**
		Fetches the index of the active tab.
		May return InvalidIndex if the collection is currently empty.
		*/
		index_t			GetActiveTabIndex() const	{return activeIndex;}
		/**
		Changes the currently active tab to the one associated with the specified panel.
		The method triggers a fatal exception if the specified panel is neither null nor found.
		Nothing changes or is triggered if the specified tab is already active or null.
		@param tabPanel Panel of the tab to activate
		@param triggerCallback Set true to invoke any set tab-change callback function
		*/
		void			SetActiveTab(const PPanel&tabPanel, bool triggerCallback=true)		{if (tabPanel) SetActiveTab(GetTabIndex(tabPanel),triggerCallback);}
		/**
		Defines a new callback function to call whenever the active tab changes.
		*/
		void			SetOnTabChange(const FOnTabChange&onChange);

		#if 0
		/**
		Removes the tab associated with the specified panel from the local collection.
		The operation may trigger an on-tab-change event.
		The method triggers a fatal exception if the specified panel is neither null nor found.
		*/
		void			RemoveTab(const PPanel&);
		#endif /*0*/

		/**
		Removes all tabs in the local collection.
		The operation may trigger an on-tab-change event.
		@return true if any tabs were removed, false otherwise
		*/
		bool			ClearTabs();

		bool			IsEmpty() const {return tabs.IsEmpty();}
		bool			IsNotEmpty() const {return tabs.IsNotEmpty();}
		count_t			CountTabs() const {return tabs.Count();}

	
		virtual void Render() override
		{
			Super::Render();
		}

	protected:
		virtual void	OnTabChange();

	private:
		virtual bool	AddComponent(const PComponent&c) override	{return false;}
		virtual bool	RemoveComponent(const PComponent&c) override {return false;}
		virtual bool	ClearComponents() override {return false;}



		struct TTab
		{
			PPanel	panel;
			PButton	button;

			friend void swap(TTab&a, TTab&b)
			{
				a.panel.swap(b.panel);
				a.button.swap(b.button);
			}
		};

		PTabBar			tabBar;

		FOnTabChange	onTabChange;
		Buffer0<TTab>	tabs;
		TTab			active;
		index_t			activeIndex = InvalidIndex;

		void			SetActiveTab(index_t, bool triggerCallback);
		void			Setup();

	};

	

	class Window : public Panel
	{
		typedef Panel Super;

		void				UpdateOuterRect()
		{
			outerRect= GetRect();
			outerRect.x.max += GetVisibleScrollbarContentExpansion();
			outerRect.x.Expand(outlineMargin.x);
			outerRect.y.Expand(outlineMargin.y);
		}
		M::Rect<>			outerRect = M::Rect<>(0,0,1,1);
		bool				nestedCall = false;
		float2				outlineMargin = float2(0.015f);
		bool				relayoutRequired = true,
							visible = false;


	protected:

		/**/				Window()
		{
			anchorConfig.GetHAxis().SetAutoCenter();
			anchorConfig.GetVAxis().SetAutoCenter();
		}

		template <typename T>	
			static std::shared_ptr<T>	CreateDerivative()
			{
				std::shared_ptr<T> rs(new T());
				rs->SetLocation(M::TFloatRect(float2(0.25f),float2(0.5f)));
				rs->SetAutomaticHeight(true);
				//Super::SetCenteredWidth(0.75f,GUI::TAnchor::RelativeToScreen);
				rs->VerifyAnchorIntegrity();
				return rs;
			}

	public:
		bool				logFullAutoLayout = false;

		void				OnScreenSizeOrUIScaleChange()
		{
			Super::OnScreenSizeOrUIScaleChange();
			FullAutoLayout(CLOCATION);
			UpdateOuterRect();
		}

	public:
		M::Rect<>			context = M::Rect<>(0,0,1,1);
		bool				canBeImplicitlyClosed = true;	//!< Indicates that this window can be closed by events such as the Escape being pressed
		bool				isModal = true;

		#ifdef _DEBUG
			bool			breakOnHide = false;
		#endif


		static PWindow		Create()
		{
			return CreateDerivative<Window>();
		}


		bool				IsVisible() const {return visible;}

		virtual PWindow		GetWindow() override;
		virtual PConstWindow	GetWindow() const override;

		void				SetOutlineMargin(const float2&v)
		{
			outlineMargin = v;
			UpdateOuterRect();
		}
		const float2&		GetOutlineMargin() const {return outlineMargin;}


		virtual void		Render() override
		{
			if (isModal)
				Darken(context,0.7f);
			Darken(outerRect);
			//Darken(rect);
			Super::Render();
		}

		void			SignalLayoutChange()
		{
			relayoutRequired = true;
		}

		bool			RelayoutRequired() const {return relayoutRequired;}
		bool			FullAutoLayout(const TCodeLocation&caller);

		virtual bool	DetectEnabledState() override
		{
			if (Super::DetectEnabledState())
			{
				FullAutoLayout(CLOCATION);
				return true;
			}
			return false;
		}

		virtual bool	UpdateDynamicSize() override
		{
			if (Super::UpdateDynamicSize())
			{
				if (!nestedCall)
				{
					nestedCall= true;
					FullAutoLayout(CLOCATION);	
					nestedCall = false;
				}
				return true;
			}
			return false;
		}

		virtual void	OnHide() {visible = false;}
		virtual void	OnShow();
		virtual bool	SignalChange() {return true;}
	};

	
	
	void	SignalScreenResize();

}



}

#endif
#endif

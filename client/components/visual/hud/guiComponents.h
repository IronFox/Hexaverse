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



	template <typename T>
		class Axial
		{
		public:
			T			min = T(),
					//	center = T(),
						max = T();
		};

	template <typename T>
		class Axial2
		{
		public:
			Axial<T>	axis[2];

			T&			Top() {return axis[1].max;}
			T&			Bottom() {return axis[1].min;}
			T&			Left() {return axis[0].min;}
			T&			Right() {return axis[0].max;}
			const T&	Top() const {return axis[1].max;}
			const T&	Bottom() const {return axis[1].min;}
			const T&	Left() const {return axis[0].min;}
			const T&	Right() const {return axis[0].max;}
		};



	struct TAnchor
	{
		enum type_t
		{
			Disabled,
			Fixed,
			Auto
		};

		enum distanceType_t
		{
			RelativeToParentComponent,
			RelativeToScreen,
			AbsolutePixels,
		};
		type_t			type;

		float			distanceFromReference = 0.f;	//!< Distance to position extreme from closest reference. If no references are set, uses context edge instead.
		distanceType_t		distanceType = RelativeToParentComponent;	
		Buffer0<WComponent>	references;			//!< Zero or more reference components. If none are set, the respective context extreme is used. If more than one are set, the closest is used


		void			Unset()
		{
			type = Disabled;
		}
		void			Set(const PComponent&c, float distance, distanceType_t dtype=RelativeToParentComponent)
		{
			if (!c)
				return;
			type = Fixed;
			distanceFromReference = distance;
			references.Clear();
			references.Append(c);
			distanceType = dtype;
		}
		void			Set(float distance, distanceType_t dtype=RelativeToParentComponent)
		{
			type = Fixed;
			distanceFromReference = distance;
			distanceType = dtype;
		}
		void			SetAuto()
		{
			type = Auto;
		}

		void			SetAllOf(const PPanel&, float distance, distanceType_t dtype=RelativeToParentComponent);
		void			SetSomeOf(const PPanel&, float distance, const std::function<bool(const PComponent&)>&, distanceType_t dtype=RelativeToParentComponent);
		void			SetLastOf(const PPanel&, float distance, distanceType_t dtype=RelativeToParentComponent);

		bool			IsFixed() const {return type == Fixed;}
		bool			IsAuto() const {return type == Auto;}
		bool			IsInactive() const {return type == Disabled;}


		float			ToScreenDistance(float parentSize, bool isHorizontal) const;
	};

	typedef Axial2<TAnchor>	AnchorConfig;



	class AnchorModifier : private AnchorConfig
	{
	public:
		/**/			AnchorModifier(const AnchorModifier&)=delete;
		void			operator=(const AnchorModifier&)=delete;

		/**/			AnchorModifier(AnchorModifier&&);
		/**/			AnchorModifier(Component*cmp);
		/**/			~AnchorModifier();
		auto&			Top() {return axis[1].max;}
		auto&			Bottom() {return axis[1].min;}
		auto&			Left() {return axis[0].min;}
		auto&			Right() {return axis[0].max;}
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

		const M::Rect<>&	GetRect() const {return rect;}
		bool			SetWidth(float w);
		bool			SetHeight(float h);
		void			SetSize(const M::TVec2<>&size);
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
		static void		RenderOutline(const M::Rect<>&rect, const float3&backgroundTone, const M::Quad<bool>&edges, float lowOpacity=1, float highOpacity=1);

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
		virtual void	OnRectChange() override		{InvalidateAutoLayout();};
		virtual void	PropagateToChildren(const PWindow&)	{}

		static bool		Set(float&ref, float to);
		static bool		SetCenter(M::TFloatRange<>&ref, float to);
		

		Axial2<TAnchor>	anchorConfig;
	public:	//do not call outside container classes:

		virtual void	OnScreenSizeOrUIScaleChange()	{}


	public:


		/**/			Component();
		virtual			~Component();

		bool			IsEnabled() const {return localState.isEnabled && parentState.isEnabled;}
		bool			IsLit() const {return ClientIsLit() && parentState.isLit;}
		bool			IsActive() const {return active;}

			
	

		#ifdef _DEBUG
		bool			SetWidth(float w)
		{
			ASSERT__(!anchorConfig.Left().IsFixed() || !anchorConfig.Right().IsFixed()); 
			return Super::SetWidth(w);
		}
		bool			SetHeight(float h)
		{
			ASSERT__(!anchorConfig.Top().IsFixed() || !anchorConfig.Bottom().IsFixed()); 
			return Super::SetHeight(h);
		}
		#endif



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

		virtual bool	ResolveAnchorAxis(int axis, const M::Rect<>&context);

		virtual void	Translate(const float2&delta)	{TranslateRect(delta); /*InvalidateAnchorResolution();*/}
		virtual void	SetLocation(const M::Rect<>&loc) {SetRect(loc);}
		virtual bool	HasComponent(const PComponent&c) const {return c.get() == this;}


		void			SetParent(const PWindow&w, const PComponent&c);
		PComponent		GetParent() const {return parent.lock();}
		virtual PWindow	GetWindow() {return window.lock();}

		bool			FullAutoLayout(const M::Rect<>&context);


		void			InvalidateAutoLayout();

		AnchorModifier	ModifyAnchorConfig() {return this;}
		const AnchorConfig&GetAnchorConfig() const {return anchorConfig;}
		virtual void	SetAnchorConfig(const Axial2<TAnchor>&cfg)	{anchorConfig = cfg; InvalidateAutoLayout();}

		/**
		Updates the left and right anchors such that the local component maintains the given width
		@param w Target component width. If @a type is AbsolutePixels, then w must be specified in pixels, otherwise any value in [0,1]
		*/
		Component&		SetCenteredWidth(float w, TAnchor::distanceType_t type);
		Component&		FillHorizontally(float spacing, TAnchor::distanceType_t type = TAnchor::RelativeToParentComponent);
		Component&		FillVertically(float spacing, TAnchor::distanceType_t type = TAnchor::RelativeToParentComponent);
		Component&		AnchorTop(float spacing, TAnchor::distanceType_t type = TAnchor::RelativeToParentComponent);

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

		void			SetAutoHeight(bool b)	{if (autoSetHeight == b) return; autoSetHeight = b; InvalidateAutoLayout();}
		void			SetTexture(const DX11::Texture&tex) {texture = tex;InvalidateAutoLayout();}

		virtual void	Render() override;
		//virtual bool	HandleClick() override {return ContainsMouse();}
	protected:
		virtual float	GetContentHeight() const override;

	};



	class Box : public Component
	{
	protected:
		void			BaseRender(bool lit, bool flipped);
	public:
		typedef Component	Super;

		M::Quad<bool>		edges = M::Quad<bool>(true);
		bool			fillBackground = true;
		float			bottomOpacity = 1.f,
						topOpacity = 1.f;
		float3			backgroundTone = float3(1);

		virtual void	Render() override	{Super::Render(); if (fillBackground) BaseRender(false,false);}
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
	protected:
		virtual void	OnRectChange() override	{Replot();}
		virtual void	OnScreenSizeOrUIScaleChange() override	{Replot();}

		void			Replot();
	private:

		float			GetAxisStep(const M::TFloatRange<>&valueRange) const;
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

		void			SetText(const String&string)
		{
			if (rawText.GetLength() == 1 && rawText.First() == string)
				return;
			rawText.SetSize(1);
			rawText.First() = string;
			ReWrap();
			UpdateDynamicSize();
			OnTextChange();
		}
		void			SetText(const Array<String>&string)
		{
			if (rawText == string)
				return;
			rawText = string;
			ReWrap();
			UpdateDynamicSize();
			OnTextChange();
		}

		const Array<String>&GetText() const {return rawText;}

		void			SetToIdealWidth();

		float			GetTextHeight() const {return textHeight;}

		void			LockToIdealWidth(bool doLock=true)	{widthLockedToIdeal = doLock; if (doLock) SetToIdealWidth();}
		bool			IsLockedToIdealWidth() const {return widthLockedToIdeal;}
		void			LockToIdealHeight(bool doLock=true)	{heightLockedToIdeal = doLock; if (doLock) SetHeight(textHeight);}
		bool			IsLockedToIdealHeight() const {return heightLockedToIdeal;}

		virtual bool	UpdateDynamicSize() override;
		virtual void	Render() override;
	};


	class Label : public TextField
	{
		typedef TextField	Super;

	public:
		/**/			Label()
		{
			Super::fillBackground = false;
		}
		/**/			Label(const String&text, float scale=1.f)
		{
			Super::fillBackground = false;
			Super::SetLocalFontScale(scale);
			SetText(text);
			SetToIdealWidth();
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
			Super::fillBackground = false;
			auto padding = Super::GetPadding();
			padding.left = 2;
			Super::SetPadding(padding);
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
			Down
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

		bool			autoHeight = false;
		float			autoHeightSpacing = 0.f;//0.015f;
		/**
		Maximum height allocatable by client components.
		A scrollbar will be rendered if maxHeight is exceeded
		*/
		float			maxHeight = 0.8f;
	public:
		/**/			Panel()	{topOpacity = 0.75f; bottomOpacity = 0.75f;}
		bool			DeterminesHeightAutomatically() const {return autoHeight;}
		void			SetAutomaticHeight(bool b) {if (autoHeight == b) return; autoHeight = b; this->InvalidateAutoLayout();}
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
		virtual bool	ResolveAnchorAxis(int axis, const M::Rect<>&context) override;
		virtual void	Translate(const float2&delta)	{Super::Translate(delta); foreach (components,c) (*c)->Translate(delta);}

		virtual bool	HasComponent(const PComponent&c) const override {if (c.get() == this) return true; foreach (components, c2) if ((*c2)->HasComponent(c)) return true; return false;}
		virtual WheelRs	OnMouseWheel(short) override;
		virtual PComponent	OnMouseDown() override;
		virtual void	OnMouseUp() override {vscroll.OnMouseUp();}
		virtual void	OnMouseMove(const M::TVec2<>&mouseAt, bool clipAreaContainsMouse) override;




		virtual const Buffer0<PComponent>&		GetComponents() const { return components; }

		virtual bool		HasComponents() const
		{
			return components.IsNotEmpty();
		}

		virtual count_t		CountComponents() const
		{
			return components.Count();
		}

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
		std::shared_ptr<Component>		AnchorBelowPrevious(Component*newComponent, float distanceFromSides = 0.015f, float distanceToPrevious = 0.015f, TAnchor::distanceType_t distanceType=TAnchor::RelativeToScreen)
		{
			std::shared_ptr<Component> c(newComponent);
			AnchorBelowPrevious(c,distanceFromSides,distanceToPrevious,distanceType);
			return c;
		}
		void			AnchorBelowPrevious(const PComponent&newComponent, float distanceFromSides = 0.015f,  float distanceToPrevious = 0.015f, TAnchor::distanceType_t distanceType=TAnchor::RelativeToScreen);

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
		M::Rect<>				outerRect = M::Rect<>(0,0,1,1);
		bool				nestedCall = false;
		float2				outlineMargin = float2(0.015f);
		bool				autoCenter = true,
							relayoutRequired = true,
							visible = false;


	protected:

		/**/				Window()	{}

		template <typename T>	
			static std::shared_ptr<T>	CreateDerivative()
			{
				std::shared_ptr<T> rs(new T());
				rs->autoHeight = true;
				//Super::SetCenteredWidth(0.75f,GUI::TAnchor::RelativeToScreen);
				rs->SetLocation(M::TFloatRect(float2(0.25f),float2(0.5f)));
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
		M::Rect<>				context = M::Rect<>(0,0,1,1);
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

		virtual void		SetAnchorConfig(const Axial2<TAnchor>&cfg)	override 
		{
			ASSERT__(!autoCenter);
			anchorConfig = cfg; InvalidateAutoLayout();
		}

		bool				DoesAutoCenter() const {return autoCenter;}

		void				SetAutoCenter(bool center)
		{
			if (center)
			{
				ASSERT__(anchorConfig.Left().IsInactive());
				ASSERT__(anchorConfig.Right().IsInactive());
				ASSERT__(anchorConfig.Top().IsInactive());
				ASSERT__(anchorConfig.Bottom().IsInactive());
			}
			autoCenter = center;
			InvalidateAutoLayout();
		}

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
				if (autoCenter)
				{
					float2 delta = context.center() - GetRect().center();
					if (!Vec::similar(delta,M::Vector2<>::zero))
						Super::Translate(delta);
				}
				UpdateOuterRect();
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

#ifdef HAS_WINDOW
#ifndef gameGUIH
#define gameGUIH

#include <global_string.h>
#include <math/vclasses.h>
#include <engine/interface/keyboard.h>

#include <memory>
#include "../../ExtRenderer/DirectX11.h"
#include <typeDecl.h>
#include <cast.h>

namespace GUI
{
	using namespace DeltaWorks;
	using namespace DeltaWorks::Math;

	namespace DX11 = Engine::DX11;


	DECLARE_POINTER_CLASS(Component);
		DECLARE_POINTER_CLASS(TextField);
			DECLARE_POINTER_CLASS(Checkbox);
			DECLARE_POINTER_CLASS(Label);
			DECLARE_POINTER_CLASS(Button);
				DECLARE_POINTER_CLASS(ScrollButton);
		DECLARE_POINTER_CLASS(BaseInput);
			DECLARE_POINTER_CLASS(Input);
			DECLARE_POINTER_CLASS(PasswordInput);
		DECLARE_POINTER_CLASS(Image);
		DECLARE_POINTER_CLASS(Panel);
			DECLARE_POINTER_CLASS(ButtonPanel);
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
		const Rect<>&	GetRect() const {return rect;}
		bool			SetWidth(float w)
		{
			if (M::similar(this->rect.x.GetExtent(),w))
				return false;
			rect.x.max = rect.x.min + w;
			OnHorizontalRectChange();
			OnWidthChange();
			OnRectChange();
			return true;
		}
		bool			SetHeight(float h)
		{
			if (M::similar(this->rect.y.GetExtent(), h))
				return false;
			rect.y.max = rect.y.min + h;
			OnVerticalRectChange();
			OnHeightChange();
			OnRectChange();
			return true;
		}
		void			SetSize(const TVec2<>&size)
		{
			auto r = rect;
			r.x.max = r.x.min + size.x;
			r.y.max = r.y.min + size.y;
			SetRect(r);
		}
		float			GetWidth() const {return rect.x.GetExtent();}
		float			GetHeight() const {return rect.y.GetExtent();}

	protected:
		virtual void	OnRectChange()		{};
		virtual void	OnHorizontalRectChange(){};
		virtual void	OnVerticalRectChange()	{};
		virtual void	OnWidthChange()		{};
		virtual void	OnHeightChange()	{};
		virtual void	OnTranslate(const TVec2<>&delta)	{};

		void			TranslateRect(const float2&delta)
		{
			Rect<> r = rect;
			r.Translate(delta);
			SetRect(r);
			OnTranslate(delta);
		}

		void			SetRect(const Rect<>&rect)
		{
			const bool hChange = rect.x != this->rect.x;
			const bool vChange = rect.y != this->rect.y;
			if (!hChange && !vChange)
				return;
			const bool widthChange = !Math::similar(rect.x.GetExtent(),this->rect.x.GetExtent());
			const bool heightChange = !Math::similar(rect.y.GetExtent(),this->rect.y.GetExtent());
			this->rect = rect;

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

	private:
		Rect<>			rect;
	};


	class Component : public std::enable_shared_from_this<Component>, public ComponentLocation
	{
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

		friend void		OnScreenResize(const Resolution&res);

	protected:
		static void		Darken(const Rect<>&rect, float alpha=0.9f, bool flush=false);
		static void		RenderArea(const Rect<>&rect, bool lit, bool flip, const float3&backgroundTone);
		virtual bool	ClientIsLit() const {return true;}


		virtual void	OnEnableChange()	{};
		virtual void	OnScreenSizeChange(const Resolution&)	{}

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
		

		Axial2<TAnchor>	anchorConfig;
	public:

		/**/			Component();
		virtual			~Component();

		static bool		Set(float&ref, float to);
		static bool		SetCenter(TFloatRange<>&ref, float to);

		bool			IsEnabled() const {return localState.isEnabled && parentState.isEnabled;}
		bool			IsLit() const {return ClientIsLit() && parentState.isLit;}
		bool			IsActive() const {return active;}

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
		virtual void	OnMouseMove(const TVec2<>&mouseAt, bool clipAreaContainsMouse);
		
		virtual bool	UpdateDynamicSize();

		virtual void	Render() {};

		virtual bool	ResolveAnchorAxis(int axis, const Rect<>&context);

		virtual void	Translate(const float2&delta)	{TranslateRect(delta); /*InvalidateAnchorResolution();*/}
		virtual void	SetLocation(const Rect<>&loc) {SetRect(loc);}
		virtual bool	HasComponent(const PComponent&c) const {return c.get() == this;}


		void			SetParent(const PWindow&w, const PComponent&c);
		PComponent		GetParent() const {return parent.lock();}
		virtual PWindow	GetWindow() {return window.lock();}

		bool			FullAutoLayout(const Rect<>&context);


		void			InvalidateAutoLayout();

		AnchorModifier	ModifyAnchorconfig() {return this;}
		const AnchorConfig&GetAnchorConfig() const {return anchorConfig;}
		virtual void	SetAnchorConfig(const Axial2<TAnchor>&cfg)	{anchorConfig = cfg; InvalidateAutoLayout();}

		/**
		Updates the left and right anchors such that the local component maintains the given width
		@param w Target component width. If @a type is AbsolutePixels, then w must be specified in pixels, otherwise any value in [0,1]
		*/
		Component&		SetCenteredWidth(float w, TAnchor::distanceType_t type);
		Component&		FillHorizontally(float spacing, TAnchor::distanceType_t type = TAnchor::RelativeToParentComponent);
		Component&		AnchorTop(float spacing, TAnchor::distanceType_t type = TAnchor::RelativeToParentComponent);

		void			AcceptStateFromParent(const TPropagating&p)	{parentState = p; PropagateToChildren(CreatePropagating());}
		void			AcceptParentWindow(const PWindow&w)	{if (window.lock() != w) {window = w;PropagateToChildren(w);}}
	};



	class Image : public Component
	{
		typedef Component	Super;
		static DX11::BaseInstructionBlock	texturedQuad;
		bool			autoSetHeight = true;
		DX11::Texture	texture;
	public:
		static void		StaticCreate();
		static void		StaticDestroy();

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

		bool			fillBackground = true;
		float3			backgroundTone = float3(1);

		virtual void	Render() override	{if (fillBackground) BaseRender(false,false);}
	};

	class BaseInput : public Box
	{
		typedef Box	Super;
		Rect<>			core;

	protected:

		Engine::Keyboard::CharacterBuffer	backup;

		static void		OnCharHandled(char);

		virtual void	SetInputActive(bool isActive);
		virtual const char*	GetAllowed() const {return nullptr;}
		virtual StringRef GetText();
		virtual float	GetContentHeight() const override;
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
		String			allowed = "abcdefghijklmnopqrstuvwxyzäöüABCDEFGHIJKLMNOPQRSTUVWXYZÄÖÜ0123456789_- ";

	protected:
		virtual const char*	GetAllowed() const override {return allowed.c_str();}
	public:

		String			GetInput();
		void			SetCurrent(const String&);
		void			SetAllowed(const String&);
	};

	class PasswordInput : public BaseInput
	{
		typedef BaseInput	Super;
		static StringBuffer	placeholder;
	protected:
		virtual StringRef GetText() override;
	public:
		const char*		GetInput();
		void			WipeInput();
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
		bool			UpdateIdealWidth();
		float			extraSpacing = 0.75f;	//!< Vertical extra spacing in multiples of line-height

	protected:
		void			RenderText(bool lit);

		virtual float	GetIdealWidth() const;
		virtual void	OnTextChange() {};
		virtual void	OnWidthChange() {ReWrap();SetHeight(textHeight);}
		virtual void	OnScreenSizeChange(const Resolution&res) override 
		{
			ReWrap();
			SetHeight(textHeight);
		}

		virtual float	GetContentHeight() const override;
		void			ReWrap();
	public:
		typedef Box	Super;

		bool			alignCenter = true;
		
		/**
		Sets the extra spacing at the top and bottom in multiples of line heights. Default is 0.75
		*/
		void			SetExtraSpacing(float spacing)	{extraSpacing = spacing;ReWrap();SetHeight(textHeight);}
		float			GetExtraSpacing() const {return extraSpacing;}

		void			SetColor(const float4&color)	{this->color = color;}
		void			SetColor(float r, float g, float b, float a = 1.f)	{this->color = float4(r,g,b,a);}
		void			SetLocalFontScale(float zoom)	{this->zoom = zoom; ReWrap();UpdateDynamicSize();OnTextChange();}
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

		void			SetIdealWidth();
		void			LockIdealWidth(bool doLock=true)	{widthLockedToIdeal = doLock; if (doLock) SetIdealWidth();}

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
			SetIdealWidth();
		}
	};

	class Button : public TextField
	{
		typedef TextField	Super;

		std::function<void()>	onPress;
		std::function<bool()>	check;

		//String					baseLabel;
//		TextField::SetText;

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
			SetIdealWidth();
		}



		//void			SetText(const String&string)
		//{
		//	baseLabel = string;
		//	DetectEnabledState();
		//}


		void			SetOnPress(const std::function<void()>&event)
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

		virtual void Render() override
		{
			bool lit = IsLit();
			bool flipped = mousePressedComponent.get() == this;
			Super::BaseRender(lit && !flipped,lit && flipped);
			Super::RenderText(lit);
		}

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

	protected:
		virtual bool	ClientIsLit() const override {return IsActive() && IsEnabled() && ContainsMouse();}
		virtual float	GetIdealWidth() const override;
		virtual float	GetContentHeight() const override;
	public:
		/**/			Checkbox()
		{
			Super::alignCenter = false;
			Super::SetExtraSpacing(Super::GetExtraSpacing() / 2);
			Super::fillBackground = false;
			SetText("");
		}

		/**/			Checkbox(const String&text, bool checked, std::function<void(bool)> onChanged = std::function<void(bool)>(), std::function<bool()> check = std::function<bool()>() )
		{
			Super::alignCenter = false;
			Super::fillBackground = false;
			Super::SetExtraSpacing(Super::GetExtraSpacing() / 2);
			this->onChanged = onChanged;
			this->check = check;
			isChecked = checked;
			SetText(text);
			SetIdealWidth();
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

		static void		RenderArrow(const Rect<>&, direction_t, bool lit);

	};



	class Slider : public Component
	{
		typedef Component	Super;

	protected:
		virtual void		OnScreenSizeChange(const Resolution&res)
		{
			relativeHeight = GetPixelHeight() / res.height;
			relativeThumbWidth = GetThumbPixelWidth() / res.width;
			UpdateDynamicSize();
		}
	public:
		typedef std::function<void(float&)>	FOnChange;

		/**/				Slider();
		virtual PComponent	OnMouseDown() override;
		virtual WheelRs		OnMouseWheel(short) override;
		virtual void		OnMouseUp() override;
		virtual void		OnMouseMove(const TVec2<>&mouseAt, bool clipAreaContainsMouse) override;
		virtual void		Render() override;
		void				Setup(float current, float max, float slideStep) {this->current = current; this->max = max; this->slideStep = slideStep;}
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
		TFloatRange<>		dragRange;
		Rect<>				barBack,	//!< Shadow behind slider
							bar;		//!< Actual grabbable bar
		bool				showMarkers=true;
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
			Rect<>		outline,	//!< Shadow behind scrollbar. Does not contain buttons, but does contain bar
						bar;		//!< Actual grabbable bar
			PScrollButton		up,down;
			Panel		*owner=nullptr;
			bool		outlineContainsMouse = false,barContainsMouse = false,dragging = false;
			bool		mouseDown = false,scrollDownOnClick =false;
			float		dragRelative = 0;

			PComponent	OnMouseDown();
			WheelRs		OnMouseWheel(short);
			void		OnMouseUp();
			void		OnMouseMove(const TVec2<>&mouseAt, bool clipAreaContainsMouse);
			void		Enable(Panel*,const Rect<>&rect);
			void		Render(const Rect<>&rect);
			float		GetOffset() const {return visible ? -viewTop : 0;}
			static constexpr float Width = 0.03f;
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
			return vscroll.visible ? TScrollbar::Width+TScrollbar::ContentSpacing : 0;
		}

		bool			autoHeight = false;
		float			autoHeightSpacing = 0.f;//0.015f;
		/**
		Maximum height allocatable by client components.
		A scrollbar will be rendered if maxHeight is exceeded
		*/
		float			maxHeight = 0.8f;
	public:
		bool			DeterminesHeightAutomatically() const {return autoHeight;}
		void			SetAutomaticHeight(bool b) {if (autoHeight == b) return; autoHeight = b; this->InvalidateAutoLayout();}
		float			GetAutoHeightSpacing() const {return autoHeightSpacing;}
		void			SetAutoHeightSpacing(float s) {if (s == autoHeightSpacing) return; autoHeightSpacing = s; this->InvalidateAutoLayout();}
		float			GetMaxAutoHeight() const {return maxHeight;}
		void			SetMaxAutoHeight(float h) {if (h == maxHeight) return; maxHeight = h; InvalidateAutoLayout();}

		virtual bool	DetectEnabledState() override;
		
		virtual void	SetActive(bool isActive) override;
		virtual bool	UpdateDynamicSize() override;
		virtual void	Render() override;
		virtual bool	ResolveAnchorAxis(int axis, const Rect<>&context) override;
		virtual void	Translate(const float2&delta)	{Super::Translate(delta); foreach (components,c) (*c)->Translate(delta);}

		virtual bool	HasComponent(const PComponent&c) const override {if (c.get() == this) return true; foreach (components, c2) if ((*c2)->HasComponent(c)) return true; return false;}
		virtual WheelRs	OnMouseWheel(short) override;
		virtual PComponent	OnMouseDown() override;
		virtual void	OnMouseUp() override {vscroll.OnMouseUp();}
		virtual void	OnMouseMove(const TVec2<>&mouseAt, bool clipAreaContainsMouse) override;




		virtual const Buffer0<PComponent>&		GetComponents() const { return components; }

		virtual bool		HasComponents() const
		{
			return components.IsNotEmpty();
		}

		virtual count_t		CountComponents() const
		{
			return components.Count();
		}

		virtual bool		AddComponent(const PComponent&c)
		{
			if (components.Contains(c))
				return false;
			components << c;
			c->SetParent( GetWindow(), shared_from_this());
			return true;
		}
		virtual PComponent	AddNewComponent(Component*c)
		{
			PComponent comp(c);
			ASSERT__(AddComponent(comp));
			return comp;
		}

		virtual bool		RemoveComponent(const PComponent&c)
		{
			if (c->GetParent().get() != this)
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
		void			AnchorBelowPrevious(Component*newComponent, float distanceFromSides = 0.015f, float distanceToPrevious = 0.015f, TAnchor::distanceType_t distanceType=TAnchor::RelativeToScreen)
		{
			AnchorBelowPrevious(PComponent(newComponent),distanceFromSides,distanceToPrevious,distanceType);
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



		void			SetOnPress(const std::function<void()>&event)
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

		virtual void	OnMouseMove(const TVec2<>&mouseAt, bool clipAreaContainsMouse) override
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
		Rect<>				outerRect = Rect<>(0,0,1,1);
		bool				nestedCall = false;
		float2				outlineMargin = float2(0.015f);
		bool				autoCenter = true,
							relayoutRequired = true;


	protected:
		/**/				Window()	{}

		template <typename T>	
			static std::shared_ptr<T>	CreateDerivative()
			{
				std::shared_ptr<T> rs(new T());
				rs->autoHeight = true;
				//Super::SetCenteredWidth(0.75f,GUI::TAnchor::RelativeToScreen);
				rs->SetLocation(Math::TFloatRect(float2(0.25f),float2(0.5f)));
				return rs;
			}


	public:
		Rect<>				context = Rect<>(0,0,1,1);
		bool				canBeImplicitlyClosed = true;	//!< Indicates that this window can be closed by events such as the Escape being pressed
		bool				isModal = true;

		#ifdef _DEBUG
			bool			breakOnHide = false;
		#endif


		static PWindow		Create()
		{
			return CreateDerivative<Window>();
		}



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

		bool			FullAutoLayout()
		{
			AcceptStateFromParent(TPropagating());
			bool rs = Super::FullAutoLayout(context);
			relayoutRequired = false;

			if (autoCenter)
			{
				float2 delta = context.center() - GetRect().center();
				if (!Vec::similar(delta,Vector2<>::zero))
					Super::Translate(delta);
			}
			UpdateOuterRect();

			return rs;
		}

		virtual bool	DetectEnabledState() override
		{
			if (Super::DetectEnabledState())
			{
				FullAutoLayout();
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
					FullAutoLayout();	
					nestedCall = false;
				}
				if (autoCenter)
				{
					float2 delta = context.center() - GetRect().center();
					if (!Vec::similar(delta,Vector2<>::zero))
						Super::Translate(delta);
				}
				UpdateOuterRect();
				return true;
			}
			return false;
		}

		virtual void	OnHide() {};
		virtual bool	SignalChange() {return true;}
	};

	
	

	/**
	Checks that the top-most window is active (if any), and all others are inactive (if any).
	Will halt execution in Debug mode, and fix the issue in release mode.
	*/
	void				VerifyIntegrity(const TCodeLocation&);

	void				OnScreenResize(const Resolution&);


	void				Create();
	void				Destroy();


	index_t				FindWindowIndex(const Window*window);

	/**
	Makes sure the specified window is visible. Raises the window if it's not top-most
	*/
	void				Show(const GUI::PWindow&window);
	/**
	Makes sure the specified window is visible. Lowers it to the bottom of the window stack if it's not there already
	*/
	void				ShowBottom(const GUI::PWindow&window);
	bool				Hide(const PWindow&window);
	bool				Hide(Window*window);
	bool				IsVisible(const GUI::PWindow&window);
	PWindow				GetTopMostWindow();
	/**
	Signals that the global situation has changed and active modal dialogs need to be re-evalutated.
	This may be invoked by a server state change or removal of some entity
	*/
	void				SignalSituationChange();

	/**
	Forcefully sends mouse motion signal to the topmost windows until a modal window was processed
	*/
	void				OverrideSignalMouseMotion();


	WheelRs				OnMouseWheel(short delta);
	bool				OnMouseDown();
	void				HandleMouseUp();
	/**
	Checks if any modal dialogs are open at this time
	*/
	bool				IsModal();
	/**
	Closes all modal dialogs.
	Dialogs declared as non implicitly closable are unaffected
	@return true, if any modal dialogs were closed
	*/
	bool				CloseAllModal();
	/**
	Closes the top-most modal dialog.
	If the top-most dialog is declared as non implicitly closable, then the function fails.
	@return true, if any modal dialog was closed
	*/
	bool				CloseTopModal();
	/**
	If the specified window is shown open, it is closed, and all closable windows beneath it
	*/
	bool				CloseAllModalBeneath(const PWindow&);
	/**
	Renders visible non-modal windows/dialogs up to the first modal window
	*/
	void				RenderNonModal();
	/**
	Renders visible modal windows/dialogs following the first encountered modal window/dialog, bottom up
	*/
	void				RenderModal();

}






#endif
#endif

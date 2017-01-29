#ifdef HAS_WINDOW
#ifndef gameGUIH
#define gameGUIH

#include <global_string.h>
#include <math/vclasses.h>
#include <engine/interface/keyboard.h>

#include <memory>
#include "../ExtRenderer/DirectX11.h"
#include "../../shared/typeDecl.h"

namespace GUI
{

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


	extern PComponent	mousePressedComponent;	//component that caught the last mouse down event

	class Component : public std::enable_shared_from_this<Component>
	{
	private:
		bool			enabled = true;
		bool			containsMouse = false,
						parentIsLit = true,
						parentIsEnabled = true;
		bool			active = false;


		bool			LayoutAxis(int axis, const Rect<>&context);
	protected:
		static void		Darken(const Rect<>&rect, float alpha=0.9f, bool flush=false);

		static void		RenderArea(const Rect<>&rect, bool lit, bool flip, const float3&backgroundTone);

		virtual bool	ClientIsLit() const {return true;}

		Rect<>			rect;
	public:
		Axial2<TAnchor>	anchorConfig;

		static bool		Set(float&ref, float to);
		static bool		SetCenter(TFloatRange<>&ref, float to);

		bool			IsEnabled() const {return enabled && parentIsEnabled;}
		bool			IsLit() const {return ClientIsLit() && parentIsLit;}
		bool			IsActive() const {return active;}

		bool			SetEnabled(bool newEnabled)	{ if (newEnabled != enabled) {enabled = newEnabled; PropagateParentState(parentIsEnabled,parentIsLit); return parentIsEnabled; } return false;  }

		bool			ContainsMouse() const	{return containsMouse;}
		void			MoveTopTo(float h);

		virtual void	PropagateParentState(bool enabled, bool lit)	{parentIsLit = lit; parentIsEnabled = enabled;}

		virtual void	SetActive(bool isActive) {active = isActive;}
		virtual bool	DetectEnabledState() {return false;}
		
		virtual PComponent	HandleMouseDown() {return PComponent();}
		virtual WheelRs	HandleMouseWheel(short) {return ContainsMouse() ? WheelRs::Ignored : WheelRs::NotContained;}
		virtual void	SignalMouseUp()	{}
		virtual void	SignalMouseMotion(const TVec2<>&mouseAt, bool clipAreaContainsMouse);
		
		virtual bool	UpdateHeightIfNecessary()	{return false;}
		virtual bool	UpdateHeight() {return false;}
		virtual void	Render() {};
		virtual bool	AutoLayout(const Rect<>&context);
		virtual void	Translate(const float2&delta)	{rect.Translate(delta);}
		virtual void	SetLocation(const Rect<>&loc) {rect = loc;}
		virtual bool	HasComponent(const PComponent&c) const {return c.get() == this;}

		bool			FullAutoLayout(const Rect<>&context);

		void			SetCenteredWidth(float w, TAnchor::distanceType_t type);

		const Rect<>&	GetLocation() const {return rect;}
	};



	class Image : public Component
	{
		typedef Component	Super;
		static DX11::BaseInstructionBlock	texturedQuad;
	public:
		static void		StaticCreate();
		static void		StaticDestroy();

		DX11::Texture	texture;
		bool			autoSetHeight = true;

		virtual void	Render() override;
		//virtual bool	HandleClick() override {return ContainsMouse();}
		virtual bool	UpdateHeight() override;

	};



	class Area : public Component
	{
	private:
		index_t			sizeVersion = InvalidIndex;
	protected:
		void			BaseRender(bool lit, bool flipped);
	public:
		typedef Component	Super;

		bool			fillBackground = true;
		float3			backgroundTone = float3(1);

		virtual bool	UpdateHeight() override;
		virtual bool	UpdateHeightIfNecessary() override;

		virtual bool	AutoLayout(const Rect<>&context) override {InvalidateSize(); return Super::AutoLayout(context); }

		//virtual bool	HandleClick() override {return fillBackground && ContainsMouse();}
		virtual void	SetLocation(const Rect<>&loc) {Super::SetLocation(loc); InvalidateSize();}


		void			InvalidateSize()
		{
			sizeVersion = InvalidIndex;
		}
		virtual void	Render() override	{if (fillBackground) BaseRender(false,false);}
	};

	class BaseInput : public Area
	{
		typedef Area	Super;
		Rect<>			core;

	protected:

		Engine::Keyboard::CharacterBuffer	backup;

		static void		OnCharHandled(char);

		virtual void	SetInputActive(bool isActive);
		virtual const char*	GetAllowed() const {return nullptr;}
		virtual StringRef GetText();
	public:

		/**
		Triggered when the user confirms the input (via enter/return)
		*/
		std::function<void()>	onEnter;

		virtual void	Render() override;
		virtual bool	UpdateHeight() override;
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

	class TextField : public Area
	{
	private:
		Array<String>	rawText;
		Buffer0<String>	wrappedText;
		Array<float2>	lineAt;
		float4			color = float4(1,1,1,1);
		float			textHeight = 1.f;
		bool			widthLockedToIdeal=false;
		bool			UpdateIdealWidth();

	protected:
		void			RenderText(bool lit);
		bool			UpdateHeight(const TFloatRange<>&hRange);

		virtual float	GetIdealWidth() const;

	public:
		typedef Area	Super;

		bool			alignCenter = true;
		float			zoom = 1.f;
		
		float			extraSpacing = 0.75f;	//!< Vertical extra spacing in multiples of line-height

		void			SetColor(const float4&color)	{this->color = color;}
		void			SetColor(float r, float g, float b, float a = 1.f)	{this->color = float4(r,g,b,a);}

		void			SetText(const String&string)
		{
			if (rawText.GetLength() == 1 && rawText.First() == string)
				return;
			rawText.SetSize(1);
			rawText.First() = string;
			Super::InvalidateSize();
		}
		void			SetText(const Array<String>&string)
		{
			if (rawText == string)
				return;
			rawText = string;
			Super::InvalidateSize();
		}

		void			SetIdealWidth();
		void			LockIdealWidth(bool doLock=true)	{widthLockedToIdeal = doLock; if (doLock) SetIdealWidth();}


		virtual bool	UpdateHeight() override;

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
			Super::zoom = scale;
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

		virtual PComponent	HandleMouseDown() override
		{
			DetectEnabledState();
			if (IsEnabled() && ContainsMouse())
			{
				return shared_from_this();
			}
			return PComponent();
		}

		virtual	void			SignalMouseUp() override
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
	public:
		/**/			Checkbox()
		{
			Super::alignCenter = false;
			Super::extraSpacing /= 2;
			Super::fillBackground = false;
			SetText("");
		}

		/**/			Checkbox(const String&text, bool checked, std::function<void(bool)> onChanged = std::function<void(bool)>(), std::function<bool()> check = std::function<bool()>() )
		{
			Super::alignCenter = false;
			Super::fillBackground = false;
			Super::extraSpacing /= 2;
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

		virtual PComponent	HandleMouseDown() override
		{
			DetectEnabledState();
			if (IsEnabled() && ContainsMouse())
			{
				return shared_from_this();
			}
			return PComponent();
		}

		virtual	void			SignalMouseUp() override
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

		virtual bool	UpdateHeight() override;
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
			RenderArrow(rect,direction,lit);
		}

		static void		RenderArrow(const Rect<>&, direction_t, bool lit);

	};


	class Panel : public Area
	{
		typedef Area Super;

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

			PComponent	HandleMouseDown();
			WheelRs		HandleMouseWheel(short);
			void		SignalMouseUp();
			void		SignalMouseMotion(const TVec2<>&mouseAt, bool clipAreaContainsMouse);
			void		Enable(Panel*,const Rect<>&rect);
			void		Render(const Rect<>&rect);
			float		GetOffset() const {return visible ? -viewTop : 0;}
			static constexpr float Width = 0.03f;
			static constexpr float ContentSpacing = 0.006f;
		};

		TScrollbar		vscroll;
		
	protected:
		void			PropagateStateToChildren()
		{
			bool sublit = IsLit();
			bool subEnabled = IsEnabled();
			foreach (components,c)
				(*c)->PropagateParentState(subEnabled, sublit);
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
	public:
		bool			autoHeight = false;
		float			autoHeightSpacing = 0.f;//0.015f;
		/**
		Maximum height allocatable by client components.
		A scrollbar will be rendered if maxHeight is exceeded
		*/
		float			maxHeight = 1.f;

		virtual bool	DetectEnabledState() override;
		
		virtual void	SetActive(bool isActive) override;
		virtual bool	UpdateHeight() override;
		virtual void	Render() override;
		virtual bool	AutoLayout(const Rect<>&context) override;
		virtual void	Translate(const float2&delta)	{Super::Translate(delta); foreach (components,c) (*c)->Translate(delta);}

		virtual bool	HasComponent(const PComponent&c) const override {if (c.get() == this) return true; foreach (components, c2) if ((*c2)->HasComponent(c)) return true; return false;}
		virtual WheelRs	HandleMouseWheel(short) override;
		virtual PComponent	HandleMouseDown() override;
		virtual void	SignalMouseUp() override {vscroll.SignalMouseUp();}
		virtual void	SignalMouseMotion(const TVec2<>&mouseAt, bool clipAreaContainsMouse) override;


		virtual void	PropagateParentState(bool enabled, bool lit)	override
		{
			Super::PropagateParentState(enabled, lit);  
			PropagateStateToChildren();
		}

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
			return components.FindAndErase(c);
		}
		virtual bool		ClearComponents()
		{
			if (components.IsNotEmpty())
			{
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



		virtual void Render() override
		{
			bool lit = IsLit();
			bool flipped = mousePressedComponent.get() == this;
			Super::BaseRender(lit && !flipped,lit && flipped);
			int newLit = lit?1:0;
			if (lastLitTriState == -1 || lastLitTriState != newLit)
			{
				lastLitTriState = newLit;
				Super::PropagateStateToChildren();
			}
			Super::RenderChildren();
		}

		virtual PComponent	HandleMouseDown() override
		{
			DetectEnabledState();
			if (IsEnabled() && ContainsMouse())
			{
				for (index_t i = components.Count()-1; i < components.Count(); --i)
				{
					PComponent c = components[i]->HandleMouseDown();
					if (c)
						return c;
				}
				return shared_from_this();
			}
			return PComponent();
		}

		virtual	void			SignalMouseUp() override
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
			outerRect= rect;
			outerRect.x.max += GetVisibleScrollbarContentExpansion();
			outerRect.Expand(0.015f);
		}
		Rect<>				outerRect = Rect<>(0,0,1,1);
		bool				nestedCall = false;

	public:
		Rect<>				context = Rect<>(0,0,1,1);
		bool				autoCenter = true;
		bool				canBeImplicitlyClosed = true;	//!< Indicates that this window can be closed by events such as the Escape being pressed
		bool				isModal = true;


		/**/				Window()
		{
			Super::autoHeight = true;
			Super::SetCenteredWidth(0.75f,GUI::TAnchor::RelativeToScreen);
			Super::SetLocation(Math::TFloatRect(float2(0.25f),float2(0.5f)));
		}

		virtual bool		AutoLayout(const Rect<>&context) override
		{
			if (Super::AutoLayout(context))
			{
				if (autoCenter)
				{
					float2 delta = context.center() - rect.center();
					if (!Vec::similar(delta,Vector2<>::zero))
						Super::Translate(delta);
				}
				UpdateOuterRect();
				return true;
			}
			return false;
		}

		virtual bool		UpdateHeight() override
		{
			if (Super::UpdateHeight())
			{
				if (autoCenter)
				{
					float2 delta = context.center() - rect.center();
					if (!Vec::similar(delta,Vector2<>::zero))
						Super::Translate(delta);
				}
				UpdateOuterRect();
				return true;
			}
			return false;
		}

		virtual void		Render() override
		{
			bool anyChanged = Super::UpdateHeightIfNecessary();
			if (anyChanged)
				FullAutoLayout();
			if (isModal)
				Darken(context,0.7f);
			Darken(outerRect);
			//Darken(rect);
			Super::Render();
		}

		bool			FullAutoLayout()
		{
			PropagateParentState(true,true);
			return Super::FullAutoLayout(context);
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



		virtual bool	UpdateHeightIfNecessary() override
		{
				
			if (Super::UpdateHeightIfNecessary())
			{
				if (!nestedCall)
				{
					nestedCall= true;
					FullAutoLayout();	
					nestedCall = false;
				}
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

	void				OnWindowResize();


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


	WheelRs				HandleMouseWheel(short delta);
	bool				HandleMouseDown();
	void				HandleMouseUp();
	/**
	Checks if any modal dialogs are open at this time
	*/
	bool				IsModal();
	/**
	Closes all modal dialogs
	@return true, if any modal dialogs were closed
	*/
	bool				CloseAllModal();
	/**
	Renders all visible windows/dialogs
	*/
	void				Render();

}






#endif
#endif

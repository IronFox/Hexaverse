#ifdef HAS_WINDOW
#ifndef gameGUIH
#define gameGUIH

#include <global_string.h>
#include <math/vclasses.h>

#include <memory>
#include "../ExtRenderer/DirectX11.h"

namespace GUIElements
{

	namespace DX11 = Engine::DX11;


	class Component;
	typedef std::shared_ptr<Component> PComponent;
	typedef std::weak_ptr<Component> WComponent;


	



	template <typename T>
		class Axial
		{
		public:
			T			min = T(),
						center = T(),
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
		bool			isActive = false;
		float			distanceFromReference = 0.f;	//!< Distance to position extreme from closest reference. If no references are set, uses context edge instead.
		bool			distanceIsRelative = true;	//Indicates that the value stored in @a distanceFromReference should be multiplied with the context extend
		Buffer0<WComponent>	references;			//!< Zero or more reference components. If none are set, the respective context extreme is used. If more than one are set, the closest is used

		void			Set(const PComponent&c, float distance, bool relative=true)
		{
			if (!c)
				return;
			isActive = true;
			distanceFromReference = distance;
			references.Append(c);
			distanceIsRelative = relative;
		}
		void			Set(float distance, bool relative=true)
		{
			isActive = true;
			distanceFromReference = distance;
			distanceIsRelative = relative;
		}

	};


	extern PComponent	mousePressedComponent;	//component that caught the last mouse down event

	class Component : public std::enable_shared_from_this<Component>
	{
	private:
		bool			LayoutAxis(int axis, const Rect<>&context);
	protected:
		static void		Darken(const Rect<>&rect, float alpha=0.9f, bool flush=false);
		static void		RenderArea(const Rect<>&rect, bool lit, bool flip);

		Rect<>			rect;
	public:
		bool			enabled = true;

		Axial2<TAnchor>	anchorConfig;

		static bool		Set(float&ref, float to);
		static bool		SetCenter(TFloatRange<>&ref, float to);

		bool			ContainsMouse() const;
		void			MoveTopTo(float h);

		virtual void	Activate() {};
		virtual void	Deactivate() {};
		virtual bool	DetectEnabledState() {return false;}
		virtual PComponent	HandleMouseDown() {return PComponent();}
		virtual void	SignalMouseUp()	{}
		virtual bool	UpdateHeightIfNecessary()	{return false;}
		virtual bool	UpdateHeight() {return false;}
		virtual void	Render() {};
		virtual bool	AutoLayout(const Rect<>&context);
		virtual void	Translate(const float2&delta)	{rect.Translate(delta);}
		virtual void	SetLocation(const Rect<>&loc) {rect = loc;}

		bool			FullAutoLayout(const Rect<>&context);

		void			SetCenteredWidth(float w, bool relative);

		const Rect<>&	GetLocation() const {return rect;}
	};



	class Image : public Component
	{
		typedef Component	Super;
	public:
		DX11::Texture	texture;
		bool			sensitiveToMouse = false;
		bool			autoSetHeight = true;

		virtual void	Render() override;
		//virtual bool	HandleClick() override {return ContainsMouse();}
		virtual bool	UpdateHeight() override;

	};
	typedef std::shared_ptr<Image> PImage;




	class Area : public Component
	{
	private:
		index_t			sizeVersion = InvalidIndex;
	protected:
		void			BaseRender(bool lit, bool flipped);
	public:
		typedef Component	Super;

		bool			sensitiveToMouse = true;
		bool			fillBackground = true;

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

	class Input : public Area
	{
		typedef Area	Super;
		String			backup;
		Rect<>			core;
		String			allowed = "abcdefghijklmnopqrstuvwxyzäöüABCDEFGHIJKLMNOPQRSTUVWXYZÄÖÜ0123456789_- ";
		bool			isActive = false;

		static void		OnCharHandled(char)
		{
			if (onChange)
				onChange();
		}
	public:

		static std::function<void()>	onChange;
		static std::function<void()>	onDone;


		void			ActivateInput();
		void			DeactivateInput();
		String			GetInput();
		void			SetCurrent(const String&);

		virtual void	Activate() override {ActivateInput();}
		virtual void	Deactivate() override {DeactivateInput();}
		virtual bool	UpdateHeight() override;


		virtual void	Render() override;

		void			SetAllowed(const String&);
	};
	typedef std::shared_ptr<Input>	PInput;

	class TextField : public Area
	{
	private:
		Array<String>	rawText;
		Buffer0<String>	wrappedText;
		Array<float2>	lineAt;
	protected:
		void			RenderText(bool lit);
	public:
		typedef Area	Super;

		bool			alignCenter = true;
		float			zoom = 1.f;

		void			SetText(const String&string)
		{
			if (rawText.GetLength() == 1 && rawText.first() == string)
				return;
			rawText.SetSize(1);
			rawText.first() = string;
			Super::InvalidateSize();
		}
		void			SetText(const Array<String>&string)
		{
			if (rawText == string)
				return;
			rawText = string;
			Super::InvalidateSize();
		}


		virtual bool	UpdateHeight() override;

		virtual void	Render() override;
	};
	typedef std::shared_ptr<TextField>	PTextField;


	class Label : public TextField
	{
		typedef TextField	Super;

	public:
		/**/			Label()
		{
			Super::sensitiveToMouse = false;
			Super::fillBackground = false;
		}
	};
	typedef std::shared_ptr<Label>	PLabel;

	class Button : public TextField
	{
		typedef TextField	Super;

		std::function<void()>	onPress;
		std::function<bool()>	check;

		String					baseLabel;
		bool					active = false;
		TextField::SetText;
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
		}

		virtual void	Activate() override {active = true;}
		virtual void	Deactivate() override {active = false;}


		void			SetText(const String&string)
		{
			baseLabel = string;
			DetectEnabledState();
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
			bool oldEnabled = enabled;
			String set = baseLabel;
			if (check && !check())
			{
				enabled = false;
				set += " (not right now)";
			}
			else
				enabled = true;

			Super::SetText(set);
			return enabled != oldEnabled;
		}

		virtual void Render()
		{
			bool lit = active && enabled && ContainsMouse();
			Super::BaseRender(lit,lit && mousePressedComponent.get() == this);
			Super::RenderText(lit);
		}

		virtual PComponent	HandleMouseDown() override
		{
			if (ContainsMouse())
			{
				return shared_from_this();
			}
			return PComponent();
		}

		virtual	void			SignalMouseUp() override
		{
			if (ContainsMouse())
			{
				if (enabled && onPress)
				{
					onPress();
				}
			}
		}
	};
	typedef std::shared_ptr<Button>	PButton;

	class Panel : public Area
	{
		typedef Area Super;
	public:
		Buffer0<PComponent>	components;
		bool			autoHeight = false;
		float			autoHeightSpacing = 0.f;//0.015f;

		virtual bool	DetectEnabledState() override;
		
		virtual void	Activate() override {foreach (components,c) (*c)->Activate(); Super::Activate();}
		virtual void	Deactivate() override {foreach (components,c) (*c)->Deactivate(); Super::Deactivate();}
		//virtual bool	HandleClick() override {for (index_t i = components.Count()-1; i < components.Count(); --i) if (components[i]->HandleClick()) return true; return Super::HandleClick();}
		virtual bool	UpdateHeight() override;
		virtual void	Render() override;
		virtual bool	AutoLayout(const Rect<>&context) override {bool rs = Super::AutoLayout(context); foreach (components,c) rs |= (*c)->AutoLayout(this->rect); return rs;}
		virtual void	Translate(const float2&delta)	{Super::Translate(delta); foreach (components,c) (*c)->Translate(delta);}

		virtual PComponent	HandleMouseDown() override;


		void			AnchorBelowPrevious(Component*newComponent, float distanceFromSides = 0.015f, float distanceToPrevious = 0.015f, bool distanceIsRelative=false)	{AnchorBelowPrevious(PComponent(newComponent),distanceFromSides,distanceToPrevious,distanceIsRelative);}
		void			AnchorBelowPrevious(const PComponent&newComponent, float distanceFromSides = 0.015f,  float distanceToPrevious = 0.015f, bool distanceIsRelative=false);

	};


	class Window : public Panel
	{
		typedef Panel Super;

		void				UpdateOuterRect()
		{
			outerRect= rect;
			outerRect.Expand(0.015f);
		}
		Rect<>				outerRect = Rect<>(0,0,1,1);
		bool				nestedCall = false;

	public:
		Rect<>				context = Rect<>(0,0,1,1);
		bool				autoCenter = true;

		/**/				Window()
		{
			Super::autoHeight = true;
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
			Darken(context,0.7f);
			Darken(outerRect);
			//Darken(rect);
			Super::Render();
		}

		bool			FullAutoLayout()
		{
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


	};

	typedef std::shared_ptr<Window>	PWindow;



	
	


	void				OnWindowResize();


	void				Create();
	void				Destroy();




}






#endif
#endif

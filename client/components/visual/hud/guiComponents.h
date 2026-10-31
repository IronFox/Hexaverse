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
#include "tooltip.h"
#include <math/graph.h>


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


	class BlurredBackground;
	

	class ChangeRecorder
	{
	public:
		struct TChange
		{
			PComponent		component, cause;
			TCodeLocation	loc;
			M::TFloatRect	componentRect;


			friend void swap(TChange&a,TChange&b)
			{
				using std::swap;
				swap(a.component,b.component);
				swap(a.cause,b.cause);
				swap(a.loc,b.loc);
				swap(a.componentRect,b.componentRect);
			}


			/**/		TChange(const PComponent&c, const PComponent&cause, const TCodeLocation&loc);
		};

		struct TGroup
		{
			const char*title = nullptr;
			Vector<TChange>	changes;

			friend void swap(TGroup&a, TGroup&b)
			{
				using std::swap;
				swap(a.title,b.title);
				a.changes.swap(b.changes);
			}
		};

		void			NewGroup(const char*groupName)	{groups.Append().title = groupName;}
		

		void			Record(const PComponent&self, const TCodeLocation&loc) {Record(self,nullptr,loc);}
		void			Record(const PComponent&self, const PComponent&cause, const TCodeLocation&loc);
		void			Record(Component*self, const TCodeLocation&loc);
		void			Record(Component*self, const PComponent&cause, const TCodeLocation&loc);

		bool			HasChanged() const {return any;}
		void			Clear() {groups.Clear(); any = false;}

		void			Report(const PComponent&forComp, const ChangeRecorder&prev) const;

	private:

		Vector<TGroup>	groups;
		bool			any = false;
	};


	/**
	Container class for tooltip generation.
	May be decleared via constrant string(s) or dynamic function
	*/
	class TooltipDeclaration
	{
		Tooltip::FInfo	getInfo;
	public:
		/**
		Popup delay in seconds. Defaults to 1
		*/
		static float	delay;

		/**/			TooltipDeclaration() {}
		/**/			TooltipDeclaration(const char*tooltip):getInfo{[tooltip](float t, float){return t > delay ? tooltip : nullptr;}}{}
		/**/			TooltipDeclaration(const String&tooltip):getInfo{[tooltip](float t, float){return t > delay ? tooltip.c_str() : nullptr;}}{}
		/**/			TooltipDeclaration(const Tooltip::FInfo&info):getInfo{info}	{}

		/**
		Resolves tooltip text
		*/
		Tooltip::Info	operator()(float t, float lastT) const {return getInfo != nullptr ? getInfo(t,lastT) : nullptr;}
	};


	/**
	Component anchor distance/size descriptor.
	Can be measured in different units and applied accordingly
	*/
	struct Distance
	{
		/**
		Distance unit type
		*/
		CONSTRUCT_ENUMERATION5(Type,RelativeToParentComponent,RelativeToGameWindow,AbsolutePixels,Placeholder,Disabled);

		/**
		Actual distance value.
		Significance depends on type and possibly surrounding constellation.
		May be NAN, indicating that distance/size is unknown/invalid
		*/
		float	value = 0;
		/**
		Distance unit
		*/
		Type	type;

		/**/	Distance()	{}
		/**/	Distance(float d, Type t = Type::RelativeToGameWindow) : value(d),type(t)	{}

		/**
		Defines a distance value relative to the size of the game window
		*/
		static Distance GameWindow(float d) {return Distance(d,Type::RelativeToGameWindow);}
		/**
		Defines a distance value relative to the size of the respective parent component
		*/
		static Distance OfParentSize(float d) {return Distance(d,Type::RelativeToParentComponent);}
		/**
		Defines a distance value in absolute pixels
		*/
		static Distance Pixels(float pixels) {return Distance(pixels,Type::AbsolutePixels);}
		
		static const Distance	Unknown;	//!< Defines a placeholder distance/size that is effectively defined completely by external factors
		static const Distance	Disabled;	//!< Disabled distance/size. Typically utilized to disable the anchor that the distance is recorded in


		void	AssertNotNaN(const TCodeLocation&caller);
		/**
		Asserts that the local distance is neither NAN nor INF if the local type is defined (IsDefined()), enforces NAN otherwise
		*/
		void	VerifyIntegrity(const TCodeLocation&caller);
		/**
		Determines whether the local value is neither NAN nor INF
		*/
		bool	IsWellDefined() const;
		/**
		Determines whether the local value is neither NAN, INF, nor <=0
		*/
		bool	IsWellDefinedGreaterZero() const;
		/**
		Determines whether the local distance/size is a placeholder and defined by external factors.
		True for Distance::Unknown
		*/
		bool	IsPlaceholder() const {return type == Type::Placeholder;}
		/**
		Determines whether the local distance/size is deactivated.
		True for Distance::Disabled
		*/
		bool	IsDisabled() const {return type == Type::Disabled;}
		/**
		Determines whether the local distance/size is defined and 0
		*/
		bool	IsZero() const {return IsDefined() && value ==0;}
		/**
		Determines whether the local distance/size is defined and non-0
		*/
		bool	IsNonZero() const {return IsDefined() && value != 0;}
		/**
		Determines whether the local distance/size is defined and >0
		*/
		bool	IsPositiveNonZero() const {return IsDefined() && value > 0;}
		/**
		Determines whether the local distance/size is not disabled and either a placeholder or >0
		*/
		bool	IsPositiveNonZeroOrPlaceholder() const {return !IsDisabled() && ( value > 0 || type == Type::Placeholder);}
		/**
		Determines whether the local distance/size has an actual value (i.e. neither a placeholder nor disabled)
		*/
		bool	IsDefined() const {return type != Type::Placeholder && type != Type::Disabled;}
		/**
		Checks whether or not the local size/distance requires a parent of invariant size
		*/
		bool	RequiresFixedParentSize() const {return type == Type::RelativeToParentComponent && value != 0;}

		void	operator=(const Distance&other)
		{
			if (other.IsPlaceholder() && IsDefined())
				return;
			type = other.type;
			value = other.value;
		}

		void	ResetToPlaceholder()
		{
			type = Type::Placeholder;
			value = NAN;
		}

		/**
		Converts the local value to a size/distance applicable in screen size (ideally in [0,1]).
		Automatically queries the global display size, as necessary.

		@param parentSize screen-space size of the parent component. 1 if none
		@param myAxis Axis this size/distance is measured along
		@return Screen space size/distance, ideally in [0,1] (may fall outside this range in weird conditions)
		*/
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

	/**
	Individual component anchor.
	Each component has four in total -one along each direction- which affects its location or size along that extreme.
	The local struct may be anchored to one or more components to enforce a distance to.
	This distance is measured between the local and opposite remote component anchor (or window edge if no component is referenced).
	Symmetry should be avoided. If one anchor is enabled, its referenced opposite anchor should not be set, or instead anchor to some other component.
	If multiple remote components are referenced, the one of currently closest distance (chosen dynamically) takes precedence.
	*/
	struct Anchor
	{
		typedef Vector<WComponent>	Components;

		/**
		Disables the local anchor.
		The size of the owning component along this axis becomes fixed.
		The opposing anchor is expected to be set
		*/
		void			Disable()
		{
			enabled = false;
			references.Clear();
		}

		/**
		Sets the local anchor.
		If the opposing anchor is not also set, this anchor defines the location of the owning component but not its size.
		If both are set, then the component either centers or stretches, depending on axis configuration
		@param c Singular component to anchor to. An existing anchor components are cleared
		@param dist Distance to the referenced component. Relative to screen edge if @a c is null
		*/
		void			Set(const PComponent&c, const Distance&dist)
		{
			enabled = true;
			distanceFromReference = dist;
			references.Clear();
			if (c)
				references.Append(c);
		}
		/**
		Sets the local anchor.
		If the opposing anchor is not also set, this anchor defines the location of the owning component but not its size.
		If both are set, then the component either centers or stretches, depending on axis configuration.
		Any existing anchor components are cleared.
		@param dist Distance to the respective screen edge
		*/
		void			Set(const Distance&dist)
		{
			enabled = true;
			distanceFromReference = dist;
			references.Clear();
		}

		void			VerifyIntegrity(const TCodeLocation&sender, const FVerification&, const AnchorAxis&, bool isUpper);

		/**
		Sets the same distance to all components contained by the specified panel.
		@param p Panel to reference components of. The method does nothing if this parameter is null or empty. Subsequent changes to (or destruction of) the referenced panel has no significance
		@param distance Distance to enforce to all referenced components
		@param mustNotInclude Optional debugging parameter. Typically set to the owning component. Asserts that no matching component is added
		*/
		void			SetAllOf(const PPanel&p, const Distance&distance, const Component*mustNotInclude);
		/**
		More selective version of SetAllOf().
		Only adds references which are marked true by the specified selector function.
		@param p Panel to reference components of. The method does nothing if this parameter is null or empty. Subsequent changes to (or destruction of) the referenced panel has no significance
		@param distance Distance to enforce to all referenced components
		@param del Selector function which returns true for all components that should be added and false for all else
		@param mustNotInclude Optional debugging parameter. Typically set to the owning component. Asserts that no matching component is added
		*/
		void			SetSomeOf(const PPanel&p, const Distance&distance, const FSelector&sel, const Component*mustNotInclude);
		/**
		Sets only the last component found in the given component.
		Identical Set(p->GetLastChild(),distance)
		@param p Panel to reference components of. The method does nothing if this parameter is null or empty. Subsequent changes to (or destruction of) the referenced panel has no significance
		@param distance Distance to enforce to all referenced components
		@param mustNotInclude Optional debugging parameter. Typically set to the owning component. Asserts that no matching component is added
		*/
		void			SetLastOf(const PPanel&p, const Distance&distance, const Component*mustNotInclude);

		/**
		Checks whether the local anchor is enabled
		*/
		bool			IsEnabled() const {return enabled;}
		bool			IsInactive() const {return !enabled;}
		/**
		Fetches the configured distance to the referenced component (if any).
		If not set, returns default distance (0)
		*/
		const Distance&	GetOffset() const {return distanceFromReference;}
		/**
		Returns a list reference of all referenced components. May be empty
		*/
		const Components&GetReferences() const {return references;}
		/**
		Adds another component to the list of references.
		The local anchor must be set
		*/
		void			AddReference(const PComponent&c);
		/**
		Checks whether the local anchor is enabled and requires a parent of fixed size
		*/
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
			void		ClearExtent();
			void		AnchorLower(const Distance&extent, const Distance&lowerOffset = Distance());
			void		AnchorLower(const Distance&extent, const PComponent&lowerReference, const Distance& lowerOffset = Distance());
			void		AnchorUpper(const Distance&extent, const Distance&upperOffset = Distance());
			void		AnchorUpper(const Distance&extent, const PComponent&upperReference, const Distance& upperOffset = Distance());
			void		AnchorUpperUnknown(const Distance&upperOffset = Distance());
			void		AnchorLowerUnknown(const Distance&lowerOffset = Distance());
			void		Fill(const Distance&lowerOffset = Distance(), const Distance&upperOffset = Distance());
			void		SetAutoCenter(const Distance&extent = Distance::Unknown) {isAutoCentering = true; this->extent = extent; lower.Disable(); upper.Disable();}
			void		VerifyIntegrity(const TCodeLocation&sender, const FVerification&, bool allowUnknownExtent);
			bool		BothAnchorsAreOff() const {return lower.IsInactive() && upper.IsInactive();}
			bool		BothAnchorsAreOn() const {return lower.IsEnabled() && upper.IsEnabled();}
			bool		IsFreeFloating() const {return BothAnchorsAreOff() && !isAutoCentering;}
			void		SetFreeFloating(const TCodeLocation&sender, const Distance&extent = Distance::Unknown);
			bool		HasExtentPlaceholder() const {return !Stretches() && extent.IsPlaceholder();}
			void		FillBetween(const PComponent&lowerC, const PComponent&upperC, const Distance&lowerOffset = Distance(), const Distance&upperOffset = Distance());
			float		GetMinimumEdge(float parentUpperEdge, const M::TFloatRange<>&rectRange) const;
			void		UpdateExtentIfEnabled(const Distance&d) {if (!extent.IsDisabled()) extent = d;}
		};



		Axis			axis[2];

		/**/			AnchorConfig()	{axis[AnchorAxis::Horizontal].myAxis = AnchorAxis::Horizontal; axis[AnchorAxis::Vertical].myAxis = AnchorAxis::Vertical;}
		void			VerifyIntegrity(const TCodeLocation&sender, const FVerification&check, bool allowUnknownExtent) {axis[0].VerifyIntegrity(sender,check,allowUnknownExtent); axis[1].VerifyIntegrity(sender,check,allowUnknownExtent);}
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
		AnchorModifier&	FillHBetween(const PComponent&leftEdge, const PComponent&rightEdge, const Distance&leftOffset = Distance(), const Distance&rightOffset = Distance());
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

		void			SetHorizontal(const M::TFloatRange<>&range);
		void			SetVertical(const M::TFloatRange<>&range);
		void			SetAxis(AnchorAxis a, const M::TFloatRange<>&range);
		void			SetRect(const M::Rect<>&rect);
	private:
		static const constexpr float Lower = -10000.f, Upper = 10000.f;
		M::Rect<>			rect;
	};


	/**
	Main component shape. Box-like.
	Optionally renders shadow, background, and edges.
	Edges can be enabled individually for each of the four directions.
	Shadow is disabled by default.
	*/
	class Decoration
	{
	public:
		struct Context
		{
			float		opacity;
			BlurredBackground*background;

			explicit	Context(float opacity, BlurredBackground*background):opacity(opacity),background(background){};
			Context		Recurse() const {return Context(opacity,background);}
		};

		explicit		Decoration(bool initInstructions=true);
		/**
		Updates the shape location of this decoration.
		*/
		void			UpdateLocation(const M::Rect<>&rect)	{location = rect; recompileShadow = true; recompileBackground=true; recompileEdges=true;}
		void			AssertLocationMatch(const M::Rect<>&rect) const {ASSERT_EQUAL__(location,rect);}

		void			SetCutCorners(const M::Quad<bool>&edges) {if (edges != this->cutCorners) {cutCorners = edges; recompileEdges=true; recompileBackground=true;}}
		void			SetCutCorners(bool left, bool bottom, bool right, bool top) {SetCutCorners(M::Quad<bool>(left,bottom,right,top));}
		M::Quad<bool>	GetCutCorners() const {return cutCorners;}
		void			SetEdgeVisibility(const M::Quad<bool>&edges) {/*if (edges != this->includeEdges) {includeEdges = edges; recompileEdges=true;}*/}
		void			SetEdges(bool renderEdges) {this->renderEdges = renderEdges;}
		void			SetBackground(bool back) {if (back != background) {background = back; recompileBackground=true;}}
		void			SetShadow(bool shade) {if (shade != shadow) {shadow = shade; recompileShadow = true;}}

		void			SetBackgroundColor(const float4&backgroundColor) {SetBackground(backgroundColor.a > 0); this->backgroundColor = backgroundColor;}
		const float4&	GetBackgroundColor() const {return backgroundColor;}
		void			SetBackgroundOpacity(float opacity) {SetBackground(opacity > 0); backgroundColor.a = opacity;}
		float			GetBackgroundOpacity() const {return backgroundColor.a;}
		void			SetBackgroundTone(const float3&backgroundColor) {SetBackground(true); this->backgroundColor.rgb = backgroundColor;}
		float3			GetBackgroundTone() {return backgroundColor.rgb;}
		void			SetOpacity(float opacity) {SetBackground(opacity > 0); this->bottomOpacity = this->topOpacity = opacity;}
		void			SetOpacity(float top, float bottom) {SetBackground(true); this->bottomOpacity = bottom; this->topOpacity = top;}
		void			SetShadow(const float4&tint/*, bool blurBack*/) {SetShadow(true); shadowTint = tint; /*blurBackground = blurBack;*/}

		/**
		Notifies the decoration renderer that background content can be expected to have changed and should be re-copied/blurred.
		This is normally unnecessary to be calle explicitly.
		*/
		static void		SignalBackgroundChange();

		void			OnScreenSizeOrUIScaleChange()	{recompileShadow = recompileEdges = recompileBackground =true;}

		/**
		Renders the local decoration depending on internal configuration.
		May recompile local shapes if changes have been registered
		*/
		Context			Render(const float2&mouse, bool lit, bool pressed, bool enabled, const Context&ctx)	{RenderBackground(mouse, lit,pressed,enabled,ctx); return RenderEdges(mouse, lit,pressed,enabled,ctx);}

		/**
		Renders shadow and background only (if enabled).
		Updates colors where necessary.
		*/
		Context			RenderBackground(const float2&mouse, bool lit, bool pressed, bool enabled, const Context&);
		/**
		Renders edges (if enabled).
		*/
		Context			RenderEdges(const float2&mouse, bool lit, bool pressed, bool enabled, const Context&);

		/**
		Gets the accessible client area, inside any decoration
		Valid after any Update*() has been called
		*/
		M::Rect<>		GetClientArea() const;
		float			GetClientHeight() const;
		float			GetClientWidth() const;
		float			GetClientHeightLoss() const;
		float			GetClientWidthLoss() const;

		static void		StaticCreate();

	private:
		M::Rect<>					location;
		DX11::InstructionBlock16	shadowInstruction,backgroundInstruction,blurInstruction,edgeInstruction;
		DX11::BaseInstructionBlock	lineInstruction;
		bool						recompileShadow=true,
									recompileBackground=true,
									recompileEdges=true,
									shadow=false,
									renderEdges = true,
									background=true
			//						blurBackground=true
			;
		M::Quad<bool>				includeEdges = true,
									cutCorners = true;
		float4						shadowTint = float4(1,1,1,0.5f);
		float4						backgroundColor = float4(float3(0.25f),0.8f);
		float						topOpacity=1.f,
									bottomOpacity=1.f;


	};


	/**
	Container for blurred background copying
	*/
	class BlurredBackground
	{
	public:
		static bool		realtimeBlur;

		Decoration::Context
						BeginWindowRendering(float opacity);
		/**
		Signals that the backbuffer content has significantly changed and a full update should
		be executed the next time PrepareForRendering() is called.
		*/
		void			ScheduleForcedUpdate();
		/**
		Signals (re)appearance of components referencing this container.
		Automatically calls ScheduleForcedUpdate().
		*/
		void			SignalAppearance();
		/**
		Updates the local blur opacity.
		May schedule a re-blend
		*/
		void			SetOpacity(float opacity);
		void			SignalScreenResize();

		const DX11::Texture&
						GetTexture() const {return blended.GetColorTarget();}
	private:
		DX11::FBO		blurred;
		DX11::FBO		blended;
		float			opacity = 0;
		bool			doCopy = true, doBlend = true;

		void			Copy();
		void			Blend();
	};



	class Component : public std::enable_shared_from_this<Component>, public ComponentLocation, public M::Graph::NodeAttachment, public Tooltip::IProvider
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
		float2			relativeMouse;

		virtual bool	ClientIsLit() const {return false;}


		virtual void	OnEnableChange()	{};

		/**
		Fetches the screen-relative height of the local component, given its current location (and width).
		Queried by the base implementation of UpdateDynamicSize().
		@return Designed component height relative to the current screen height. Return values < 0 to disable automatic resizing by Component::UpdateDynamicSize()
		*/
		virtual float	GetContentHeight() const {return -1;}
		virtual void	PropagateToChildren(const TPropagating&p){}
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
		typedef Decoration::Context	Ctx;

		/**/			Component();
		virtual			~Component();

		bool			IsEnabled() const {return localState.isEnabled && parentState.isEnabled;}
		bool			IsLit() const {return ClientIsLit() /*&& parentState.isLit*/;}
		bool			IsActive() const {return active;}

			
		bool			SizeChangesDynamically(int axis) const {return anchorConfig.axis[axis].Stretches();}
		bool			WidthChangesDynamically() const {return anchorConfig.GetHAxis().Stretches();}
		bool			HeightChangesDynamically() const {return anchorConfig.GetVAxis().Stretches();}

		virtual count_t	CountChildren() const {return 0;}
		virtual PComponent GetChild(index_t i) const {return nullptr;}
		PComponent		GetLastChild() const {const count_t cnt = CountChildren(); if (cnt) return GetChild(cnt-1); return nullptr;}

		virtual Tooltip::Info	ResolveTooltip(float t, float) const override {return nullptr;}
		
		bool			SetWidth(const Distance&w)
		{
			if (anchorConfig.GetHAxis().SetExtent(w))
			{
				SignalLayoutChangeUp();
				return true;
			}
			return false;
		}

		bool			SetHeight(const Distance&h)
		{
			if (anchorConfig.GetVAxis().SetExtent(h))
			{
				SignalLayoutChangeUp();
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


		/**
		Updates the active-state of the local component.
		A component is considered active if it can potentially react to user input (clicks, etc).

		For simplicity, the local component must tolerate multiple activation/deactivation, and track the state internally, 
		if a strict on-change reaction is expected.

		The window stack guarantees, that all components of the top-most window are set active at least once,
		and deactivated if this condition changes.
		*/
		virtual void	SetActive( bool isActive ) {active = isActive;}
		virtual bool	DetectEnabledState() {return false;}
		
		virtual WheelRs	OnMouseWheel(short) {return ContainsMouse() ? WheelRs::Ignored : WheelRs::NotContained;}
		/**
		Fetches the component currently beneath the mouse cursor that can actively react to a click.
		The method must test on its own, whether it contains the mouse cursor and return null otherwise
		@return Component that should receive any occurring OnMouseDown() event, null if none
		*/
		virtual PComponent	GetClickCandidate() {return PComponent();}
		/**
		Notifies that the local component has been chosen as a click candidate, and the mouse button has been pressed.
		Called only if the local component was previously returned by a GetClickCandidate() invokation.
		*/
		virtual void	OnMouseDown() {}
		/**
		Notifies that the local component has been chosen as a click dandidate, the mouse button has been pressed, and now released.
		Called only if the local component was previously returned by a GetClickCandidate() invokation, and OnMouseDown() has been called.
		*/
		virtual void	OnMouseUp()	{}
		virtual void	OnMouseMove(const M::TVec2<>&mouseAt, bool clipAreaContainsMouse);
		
		void			UpdateDynamicSize() { ChangeRecorder dummy; UpdateDynamicSize(dummy); }
		virtual void	UpdateDynamicSize(ChangeRecorder&);

		virtual Ctx		Render(const Decoration::Context&);

		virtual void	ResolveAnchorAxis(AnchorAxis axis, const M::Rect<>&context, ChangeRecorder&);

		virtual void	Translate(const float2&delta)	{TranslateRect(delta); /*InvalidateAnchorResolution();*/}
		virtual bool	HasComponent(const PComponent&c) const {return c.get() == this;}


		void			SetParent(const PWindow&w, const PComponent&c);
		PComponent		GetParent() const {return parent.lock();}
		virtual PWindow	GetWindow() {return window.lock();}
		virtual PConstWindow	GetWindow() const {return window.lock();}

		void			FullAutoLayout(const M::Rect<>&context);

		/**
		Signals layout changes upwards, from children to parent, until the window is reached.
		Call this if a change has been triggered by a component
		*/
		virtual void	SignalLayoutChangeUp();
		/**
		Signals layout change downwards, from the window to all children, recursively.
		Call this on a/all window(s) if a change has been triggered by some global effect
		*/
		virtual void	SignalWindowLayoutChange()	{};

		AnchorModifier	Anchor() {return this;}
		const AnchorConfig&GetAnchorConfig() const {return anchorConfig;}

		virtual void	VerifyAnchorIntegrity(const TCodeLocation&sender);
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
		static DX11::ObjectBuffer<float4>	texturedQuadColor;
		bool			autoSetHeight = true;
		DX11::Texture	texture;
		
		static void		StaticDestroy();
	public:
		static void		StaticCreate();

		void			SetAutoHeight(bool b)	{if (autoSetHeight == b) return; ASSERT__(!b || !anchorConfig.GetVAxis().RequiresFixedParentSize()); autoSetHeight = b; SignalLayoutChangeUp();}
		void			SetTexture(const DX11::Texture&tex) {texture = tex;SignalLayoutChangeUp();}

		virtual Ctx		Render(const Decoration::Context&) override;
		//virtual bool	HandleClick() override {return ContainsMouse();}
	protected:
		virtual float	GetContentHeight() const override;

	};



	class Box : public Component, public Decoration
	{
	public:
		typedef Component	Super;

		virtual void	OnRectChange() override
		{
			Decoration::UpdateLocation(this->GetRect());
			Super::OnRectChange();
		}

		virtual void	OnScreenSizeOrUIScaleChange() override
		{
			Decoration::OnScreenSizeOrUIScaleChange();
			Super::OnScreenSizeOrUIScaleChange();
		}

		virtual Ctx		Render( const Decoration::Context&ctx ) override	{Super::Render(ctx); return Decoration::Render(relativeMouse,this->IsLit(),false,this->IsEnabled(),ctx);}
	};

	/**
	Multi-plot drawing component.
	May display plots as percentual stacks, or overlapping lines
	*/
	class MultiPlot : public Box
	{
		typedef Box	Super;
	
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

		
			void	RenderSurfaces(float opacity)	const;
			void	RenderLines(float opacity)	const;
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
			Vector<TPoint>			data;
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


		virtual Ctx		Render(const Decoration::Context&) override;

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
		Vector<TLabel>	labels;

		bool			axesChanged = false;
		LineRenderer::Baked	axes;
		M::Rect<>			GetPlotScope() const;
		M::Rect<>			GetPlotRect() const;
		float			rightSpace = 0.1f;
		Vector<Plot>	plots;
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
		static PBaseInput	focusedInput;
	protected:

		Engine::Keyboard::CharacterBuffer	backup;


		virtual void		SetInputActive(bool isActive);
		virtual Engine::Keyboard::FAllowed	GetAllowed() const {return [](char32_t c){return c != '\t';};}
		virtual UTF32StringRef	GetTextRef();
		virtual float			GetContentHeight() const override;
	public:
		/**/				BaseInput();
		void				Focus() {SetInputActive(IsActive());}
		bool				IsFocused() const;

		/**
		Triggered when the user confirms the input (via enter/return)
		*/
		std::function<void()>	onEnter;
		/**
		Triggered when special keys are pressed while this input receives characters:
		F1-F12, Backspace, Delete, Print, Home, End, Page up and down, Space, CapsLock, Control, Alt, Arrow keys
		*/
		std::function<void(Key::Name)> onSpecialKey;

		virtual Ctx			Render(const Decoration::Context&) override;
		virtual void		SetActive( bool isActive ) override {SetInputActive(isActive); Super::SetActive(isActive);}

	};

	class Input : public BaseInput
	{
		typedef BaseInput	Super;
		Engine::Keyboard::FAllowed				allowed = nullptr;

	protected:
		virtual Engine::Keyboard::FAllowed	GetAllowed() const override {return [this](char32_t c){return c != '\t' && (!allowed || allowed(c));};}
	public:
		//static const char*	GetStandardAlphabet()  {return "abcdefghijklmnopqrstuvwxyzäöüABCDEFGHIJKLMNOPQRSTUVWXYZÄÖÜ0123456789_- ";};

		String				GetTextUTF8();
		virtual UTF32StringRef	GetTextRef() override;
		void				SetText(const String&);
		void				SetAllowed(const Engine::Keyboard::FAllowed&);
	};

	class PasswordInput : public BaseInput
	{
		typedef BaseInput	Super;
		typedef GenericStringBuffer<char32_t> StringBuffer;
		static StringBuffer	placeholder;
	protected:
		virtual UTF32StringRef	GetTextRef() override;
		virtual Engine::Keyboard::FAllowed	GetAllowed() const override {return nullptr;}
	public:
		UTF32StringRef		GetText();
		void				WipeInput();
	};

	CONSTRUCT_ENUMERATION3(HorizontalTextAlign,Center, Left, Right);
	CONSTRUCT_ENUMERATION3(VerticalTextAlign,Top, Center, Bottom);

	class TextField : public Box
	{
	private:
		#ifdef _DEBUG
			float		wrappedAtWidth=0;
		#endif

		Array<String>	rawText;
		Vector<StringRef>	wrappedText;
		Array<float2>	lineAt;
		float4			color = float4(1,1,1,1);
		float			textHeight = 1.f;
		float			zoom = 1.f;
		bool			widthLockedToIdeal=false;
		bool			heightLockedToIdeal=true;
		bool			enabledAlwaysLit = false;
		M::Quad<float>	padding;	//Vertical spacing is measured in multiples of line-heights, horizontal in multiples of X-letter-widths

		bool			UpdateIdealWidth();

		void			ReWrapSetHeight()
		{
			ReWrap();
			if (heightLockedToIdeal) 
				SetHeight(textHeight + Decoration::GetClientHeightLoss());
		}

		void			HandleTextChange();

	protected:
		void			RenderText(const Decoration::Context&ctx,bool lit);

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
		typedef HorizontalTextAlign	HAlign;
		typedef VerticalTextAlign	VAlign;

		/**/			TextField();

		/**
		Sets the extra spacing at the top and bottom in multiples of line heights. Default is 0.6
		*/
		void			SetVerticalPadding(float topAndBottom)	{SetVerticalPadding(topAndBottom,topAndBottom);}
		void			SetVerticalPadding(float top, float bottom)	{padding.top = top; padding.bottom = bottom; ReWrapSetHeight();}
		void			SetHorizontalPadding(float leftAndRight)	{SetHorizontalPadding(leftAndRight,leftAndRight);}
		void			SetHorizontalPadding(float left, float right)	{padding.left = left; padding.right = right; ReWrapSetHeight();}
		void			ScaleVerticalPadding(float factor)			{padding.bottom *= factor; padding.top *= factor; ReWrapSetHeight();}
		void			SetPadding(const M::Quad<float>&padding)		{this->padding = padding; if (this->widthLockedToIdeal) SetToIdealWidth(); ReWrapSetHeight();}
		const M::Quad<float>&GetPadding() const {return padding;}
		
		void			SetAlign(VAlign align);
		void			SetAlign(HAlign align);
		HAlign			GetHorizontalTextAlign() const	{return hAlign;}
		VAlign			GetVerticalTextAlign() const	{return vAlign;}

		void			SetColor(const float4&color)	{this->color = color;}
		void			SetColor(const float3&color, float alpha = 1.f)	{this->color.rgb = color; this->color.a = alpha;}
		void			SetColor(float r, float g, float b, float a = 1.f)	{this->color = float4(r,g,b,a);}
		void			SetLocalFontScale(float zoom) { this->zoom = zoom; ReWrap(); UpdateDynamicSize(); OnTextChange(); }
		float			GetLocalFontScale() const {return zoom;}
		float			GetTotalFontScale() const;

		void			SetText(const String&string);
		void			SetText(const ArrayRef<const String>&string);

		const Array<String>&GetText() const {return rawText;}

		void			SetToIdealWidth();

		float			GetTextHeight() const {return textHeight;}

		void			LockToIdealWidth(bool doLock=true)	{DBG_ASSERT__(!WidthChangesDynamically()); widthLockedToIdeal = doLock; if (doLock) SetToIdealWidth();}
		bool			IsLockedToIdealWidth() const {return widthLockedToIdeal;}
		void			LockToIdealHeight(bool doLock=true)	{DBG_ASSERT__(!HeightChangesDynamically()); heightLockedToIdeal = doLock; if (doLock) SetHeight(textHeight);}
		bool			IsLockedToIdealHeight() const {return heightLockedToIdeal;}

		void			SetEnabledTextAlwaysLit(bool lit) {enabledAlwaysLit = lit;}
		bool			GetEnabledTextAlwaysLit() const {return enabledAlwaysLit;}

		virtual void	SetAnchorConfig(const AnchorConfig&cfg)	override 
		{
			Super::SetAnchorConfig(cfg);
			if (WidthChangesDynamically())
				ASSERT__(!widthLockedToIdeal);
			if (HeightChangesDynamically())
				ASSERT__(!heightLockedToIdeal);
		}

		virtual void	VerifyAnchorIntegrity(const TCodeLocation&sender) override 
		{
			ASSERT__(!WidthChangesDynamically() || !widthLockedToIdeal);
			ASSERT__(!HeightChangesDynamically() || !heightLockedToIdeal);
			Super::VerifyAnchorIntegrity(sender);
		}

		virtual void	UpdateDynamicSize(ChangeRecorder&) override;
		using Super::UpdateDynamicSize;
		virtual Ctx		Render(const Decoration::Context&) override;
	private:
		HAlign			hAlign;
		VAlign			vAlign;
	};


	class Label : public TextField
	{
		typedef TextField	Super;
		void			Setup();

	public:
		/**/			Label()
		{
			Setup();
		}
		/**/			Label(const String&text, float scale=1.f)
		{
			Setup();
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
		virtual PComponent	GetClickCandidate() override {return ContainsMouse() ? shared_from_this() : nullptr;}

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

		friend class TabBar;
	private:
		typedef TextField	Super;

		FTrigger		onPress;
		std::function<bool()>	check;

		bool			alwaysDown = false;
		bool			flipWhenAlwaysDown = true;
		TooltipDeclaration tooltip;

		void			Setup();

	protected:
		virtual bool	ClientIsLit() const override {return IsActive() && IsEnabled() && ContainsMouse();}

	public:
		/**/			Button()
		{
			Setup();
			SetText("[-]","Undefined button title");
		}

		/**/			Button(const String&text, std::function<void()> onPress = std::function<void()>(), std::function<bool()> check = std::function<bool()>() )
		{
			Setup();
			this->onPress = onPress;
			this->check = check;
			SetText(text,"");
			anchorConfig.GetHAxis().AnchorLowerUnknown();
			SetToIdealWidth();
		}

		/**/			Button(const String&text, const TooltipDeclaration&tooltip, std::function<void()> onPress = std::function<void()>(), std::function<bool()> check = std::function<bool()>() )
		{
			Setup();
			this->onPress = onPress;
			this->check = check;
			SetText(text,tooltip);
			anchorConfig.GetHAxis().AnchorLowerUnknown();
			SetToIdealWidth();
		}

		void			SetText(const ArrayRef<const String>&lines, const TooltipDeclaration&tooltip)
		{
			Super::SetText(lines);
			this->tooltip = tooltip;
		}

		void			SetText(const String&text, const TooltipDeclaration&tooltip)
		{
			Super::SetText(text);
			this->tooltip = tooltip;
		}
		
		virtual Tooltip::Info	ResolveTooltip(float t, float lastT) const override
		{
			return tooltip(t, lastT);
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

		void			OnMouseDown() override;
		void			OnMouseUp()	override;

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

		virtual Ctx	 Render(const Decoration::Context&) override;

		virtual PComponent	GetClickCandidate() override
		{
			DetectEnabledState();
			if (IsEnabled() && ContainsMouse())
			{
				return shared_from_this();
			}
			return PComponent();
		}

	};

	class Checkbox : public TextField
	{
		typedef TextField	Super;

		std::function<void(bool)>	onChanged;
		std::function<bool()>		check;
		bool						isChecked = false;
		TooltipDeclaration			tooltip;

		void			Setup()
		{
			Super::SetAlign(HAlign::Left);
			Super::ScaleVerticalPadding(0.5f);
			Decoration::SetBackground(false);
			Decoration::SetEdges(false);
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
			SetText("","");
		}

		/**/			Checkbox(const String&text, bool checked, std::function<void(bool)> onChanged = std::function<void(bool)>(), std::function<bool()> check = std::function<bool()>() )
		{
			Setup();
			this->onChanged = onChanged;
			this->check = check;
			isChecked = checked;
			SetText(text,"");
			SetToIdealWidth();
		}

		/**/			Checkbox(const String&text, const TooltipDeclaration&tooltip, bool checked, std::function<void(bool)> onChanged = std::function<void(bool)>(), std::function<bool()> check = std::function<bool()>() )
		{
			Setup();
			this->onChanged = onChanged;
			this->check = check;
			isChecked = checked;
			SetText(text,tooltip);
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

		virtual Ctx	 Render(const Decoration::Context&) override;

		void			SetText(const ArrayRef<const String>&lines, const TooltipDeclaration&tooltip)
		{
			Super::SetText(lines);
			this->tooltip = tooltip;
		}

		void			SetText(const String&text, const TooltipDeclaration&tooltip)
		{
			Super::SetText(text);
			this->tooltip = tooltip;
		}
		
		virtual Tooltip::Info	ResolveTooltip(float t, float lastT) const override
		{
			return tooltip(t, lastT);
		}

		virtual PComponent	GetClickCandidate() override
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

		virtual Ctx		Render( const Decoration::Context&ctx ) override
		{
			const bool lit = IsLit();
			const bool flipped = mousePressedComponent.get() == this;
			auto cld = Decoration::Render(relativeMouse,lit,flipped,IsEnabled(),ctx);
			RenderArrow(cld,GetRect(),direction,lit);
			return cld;
		}

		static void		RenderArrow(const Decoration::Context&ctx,const M::Rect<>&, direction_t, bool lit);

		virtual void	SetLocation(const M::Rect<>&loc) override {Super::SetLocation(loc);}

	};



	class Slider : public Box
	{
		typedef Box	Super;

		Decoration			slider;

	protected:
		virtual void		OnScreenSizeOrUIScaleChange() override;
		virtual void		OnRectChange() override
		{
			slider.UpdateLocation(this->GetRect());
			Super::OnRectChange();
		}
	public:
		typedef std::function<void(float&)>	FOnChange;

		/**/				Slider();
		virtual PComponent	GetClickCandidate() override;
		//virtual WheelRs	OnMouseWheel(short) override;
		virtual void		OnMouseDown() override;
		virtual void		OnMouseUp() override;
		virtual void		OnMouseMove(const M::TVec2<>&mouseAt, bool clipAreaContainsMouse) override;
		virtual Ctx			Render(const Decoration::Context&) override;
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
		friend class TabCollection;

		struct TScrollbar
		{
			Decoration	decoration;
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

			/**/		TScrollbar();
			void		ScrollToTop();
			void		ScrollToBottom();
			PComponent	GetClickCandidate();
			WheelRs		OnMouseWheel(short);
			void		OnMouseDown();
			void		OnMouseUp();
			void		OnMouseMove(const M::TVec2<>&mouseAt, bool clipAreaContainsMouse);
			void		Enable(Panel*,const M::Rect<>&rect);
			void		Render(const float2&relativeMouse, const Decoration::Context&ctx,const M::Rect<>&rect);
			float		GetOffset() const {return visible ? -viewTop : 0;}

			static float	GetPixelWidth();
			static float	GetScreenWidth();
			static float	GetButtonScreenHeight();

			//static constexpr float Width = 0.03f;
			static constexpr float ContentSpacing = 0.006f;
		};

		TScrollbar		vscroll;
		bool			resolvingAxis = false;

		void			Setup();
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
		void			RenderChildren( const Decoration::Context&ctx )
		{
			foreach (components,c) 
				(*c)->Render(ctx);
		}

		Vector<PComponent>	components;
		Array<PComponent>	order[2];

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

		void			InvalidateOrder()
		{
			order[0].Clear();
			order[1].Clear();
		}
		void			RecomputeOrder(AnchorAxis);
	public:
		/**/			Panel()	{Anchor().FillHorizontally().Top(); Setup();}
		bool			DeterminesHeightAutomatically() const {return autoHeight;}
		void			SetAutomaticHeight(bool b);
		float			GetAutoHeightSpacing() const {return autoHeightSpacing;}
		void			SetAutoHeightSpacing(float s) {if (s == autoHeightSpacing) return; autoHeightSpacing = s; this->SignalLayoutChangeUp();}
		float			GetMaxAutoHeight() const {return maxHeight;}
		void			SetMaxAutoHeight(float h) {if (h == maxHeight) return; maxHeight = h; SignalLayoutChangeUp();}

		void			ScrollToBottom()	{vscroll.ScrollToBottom();}
		void			ScrollToTop()	{vscroll.ScrollToTop();}

		virtual count_t	CountChildren() const override {return components.Count();}
		virtual PComponent GetChild(index_t i) const override {return components[i];}

		virtual void	SignalLayoutChangeUp() override;
		virtual void	SignalWindowLayoutChange() override;

		virtual bool	DetectEnabledState() override;
		
		virtual void	SetActive(bool isActive) override;
		virtual void	UpdateDynamicSize(ChangeRecorder&) override;
		virtual Ctx		Render(const Decoration::Context&) override;
		virtual void	ResolveAnchorAxis(AnchorAxis axis, const M::Rect<>&context, ChangeRecorder&) override;
		virtual void	Translate(const float2&delta)	{Super::Translate(delta); foreach (components,c) (*c)->Translate(delta);}

		virtual bool	HasComponent(const PComponent&c) const override {if (c.get() == this) return true; foreach (components, c2) if ((*c2)->HasComponent(c)) return true; return false;}
		virtual WheelRs	OnMouseWheel(short) override;
		virtual PComponent	GetClickCandidate() override;
		virtual void	OnMouseDown() override {vscroll.OnMouseDown();}
		virtual void	OnMouseUp() override {vscroll.OnMouseUp();}
		virtual void	OnMouseMove(const M::TVec2<>&mouseAt, bool clipAreaContainsMouse) override;


		virtual void		VerifyAnchorIntegrity(const TCodeLocation&sender) override;


		virtual const Vector<PComponent>&		GetReferences() const { return components; }

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
		virtual bool		AddComponent(const PComponent&c, const TCodeLocation&caller);
		virtual PComponent	AddNewComponent(Component*c, const TCodeLocation&caller)
		{
			ASSERT_NOT_NULL__(c);
			PComponent comp(c);
			ASSERT__(AddComponent(comp,caller));
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
		std::shared_ptr<Component>		AnchorBelowPrevious(const TCodeLocation&caller, Component*newComponent, const Distance& distanceFromSides =  0.015f, const Distance& distanceToPrevious = 0.015f)
		{
			std::shared_ptr<Component> c(newComponent);
			AnchorBelowPrevious(caller, c,distanceFromSides,distanceToPrevious);
			return c;
		}
		void			AnchorBelowPrevious(const TCodeLocation&caller, const PComponent&newComponent, const Distance& distanceFromSides = 0.015f, const Distance& distanceToPrevious = 0.015f);

		virtual void	SetAnchorConfig(const AnchorConfig&cfg) override;
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

		void					Setup();
	protected:
		virtual bool	ClientIsLit() const override {return active && IsEnabled() && ContainsMouse();}
		bool	OnComponentsChanged() {lastLitTriState = -1; return true;};

	public:
		/**/			ButtonPanel()	{Setup();}

		virtual void	SetActive(bool isActive) override
		{
			this->active = isActive;
			Super::SetActive(isActive);
		}

		virtual bool		AddComponent(const PComponent&c, const TCodeLocation&caller) override
		{
			return Super::AddComponent(c,caller) && OnComponentsChanged();
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



		virtual Ctx	 Render( const Decoration::Context&ctx ) override
		{
			const bool lit = IsLit();
			const bool flipped = mousePressedComponent.get() == this;
			auto child = Decoration::Render(relativeMouse,lit,flipped,IsEnabled(),ctx);
			Super::RenderChildren(child);
			return child;
		}

		virtual PComponent	GetClickCandidate() override
		{
			DetectEnabledState();
			if (IsEnabled() && ContainsMouse())
			{
				for (index_t i = components.Count()-1; i < components.Count(); --i)
				{
					PComponent c = components[i]->GetClickCandidate();
					if (c)
						return c;
				}
				return shared_from_this();
			}
			return PComponent();
		}

		
		virtual	void			OnMouseDown() override;
		virtual	void			OnMouseUp() override;
	};


	class TabBar : public Panel
	{
		typedef Panel Super;
	public:
		typedef std::function<void()>	FOnTabChange;

		CONSTRUCT_ENUMERATION2(CornerStyle,
			LeftAligned,
			Compact
		);


		/**/			TabBar();
		/**
		Adds a new tab to the bar, to the right of the last added tab (if any).
		If this is the first tab, then it is selected immediately.
		@param caption Caption to use for the tab. May be empty or already in use (but probably should not)
		@return Handle to the panel of the new tab. The panel is anchored to fill most of the local collection area
		*/
		PButton			AddTab(const String&caption, const TooltipDeclaration&tooltip);
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

		void			SetActiveBottomOpacity(float op);
		float			GetActiveBottomOpacity() const {return activeBottomOpacity;}
		void			SetActiveTopOpacity(float op);
		float			GetActiveTopOpacity() const {return activeTopOpacity;}
		void			SetInactiveOpacity(float op);
		float			GetInactiveOpacity() const {return inactiveOpacity;}

		CornerStyle		GetCornerStyle() const {return cornerStyle;}
		void			SetCornerStyle(CornerStyle);
	
	protected:
		virtual void	OnTabChange();

	private:
		void			ConfigureInactive(const PButton&) const;
		void			ConfigureActive(const PButton&) const;
		virtual bool	AddComponent(const PComponent&c, const TCodeLocation&caller) override	{return false;}
		virtual bool	RemoveComponent(const PComponent&c) override {return false;}
		virtual bool	ClearComponents() override {return false;}

		float			activeTopOpacity = 1.f,
						activeBottomOpacity = 0.f,
						inactiveOpacity = 0.75f;
		FOnTabChange	onTabChange;
		PButton			active;
		index_t			activeIndex = InvalidIndex;
		CornerStyle		cornerStyle;

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
		PPanel			AddTab(const String&caption, const TooltipDeclaration&tooltip);
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
		void			SetCaptionOf(const PPanel&, const String&caption, const TooltipDeclaration&tooltip);
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

	

	protected:
		virtual void	OnTabChange();

	private:
		virtual bool	AddComponent(const PComponent&c, const TCodeLocation&caller) override	{return false;}
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
		Vector<TTab>	tabs;
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
			Decoration::SetShadow(float4(0,0,0,0.5f));
		}

		template <typename T>	
			static std::shared_ptr<T>	CreateDerivative()
			{
				std::shared_ptr<T> rs(new T());
				rs->SetLocation(M::TFloatRect(float2(0.25f),float2(0.5f)));
				rs->SetAutomaticHeight(true);
				//Super::SetCenteredWidth(0.75f,GUI::TAnchor::RelativeToScreen);
				rs->VerifyAnchorIntegrity(CLOCATION);
				return rs;
			}

	public:
		BlurredBackground	blurredBackground;
		bool				logFullAutoLayout = false;

		void				OnScreenSizeOrUIScaleChange()
		{
			Super::OnScreenSizeOrUIScaleChange();
			FullAutoLayout(CLOCATION);
			UpdateOuterRect();
			blurredBackground.SignalScreenResize();
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


		void				SetDarkenedOutline(bool outline)
		{
			Decoration::SetShadow(outline);
		}

		virtual void		SignalLayoutChangeUp()	override
		{
			relayoutRequired = true;
		}

		virtual void	SignalWindowLayoutChange() override
		{
			Super::SignalWindowLayoutChange();
			relayoutRequired = true;
		}


		bool			RelayoutRequired() const {return relayoutRequired;}
		void			FullAutoLayout(const TCodeLocation&caller);

		virtual bool	DetectEnabledState() override
		{
			if (Super::DetectEnabledState())
			{
				FullAutoLayout(CLOCATION);
				return true;
			}
			return false;
		}

		virtual void	UpdateDynamicSize(ChangeRecorder&rec) override
		{
			Super::UpdateDynamicSize(rec);

			if (rec.HasChanged())
			{
				if (!nestedCall)
				{
					nestedCall= true;
					FullAutoLayout(CLOCATION);	
					nestedCall = false;
				}
			}
		}

		virtual void	OnHide() {visible = false;}
		virtual void	OnShow();
		virtual bool	SignalChange() {return true;}

		virtual Ctx		Render(const Decoration::Context&ctx) override;
		void			Render(float opacity);
	};

	
	
	void	SignalScreenResize();

}



}

#endif
#endif

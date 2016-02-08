#ifdef HAS_WINDOW
#ifndef objectRenderingH
#define objectRenderingH

#include "../../client/ExtRenderer/DirectX11.h"
#include "../sphereGrid.h"
#include "turretBase.h"
#include <engine/display.h>



#define CASCADING_SHADOW

namespace Objects
{
	namespace DX11 = Engine::DX11;

	extern bool					resourceView, 
								showFriendlyRanges,
								showHostileRanges,
								objectView,
								anyCompoundsColored;


	extern DX11::ObjectBuffer<float4>	keyColor, overallTint;
	extern DX11::BaseInstructionBlock	stdVehicleShader,
										seeThroughShader,
										pathShader,
										jobShader,
										paraboloidShader,
										skyPortBeam,
										arrowInstruction,
										compoundShader;
	extern DX11::StreamVBO				arrowBuffer;
	extern DX11::VBO					sphereVBO[4];
	extern DX11::IBO32					sphereIBO[4], attackIBO;
	//extern DX11::InstructionBlock32		rangeInstruction[4];

	extern DX11::InstructionBlock32		cordon,
										foundation,
										userInteractionNotifier,
										gatheringPoint;
	extern DX11::Texture				resourceArrowTex;
	extern DX11::InstructionBlock16		resourceArrowInstruction;

	static const count_t NumCascades = 2;

	#ifdef CASCADING_SHADOW
		extern DX11::FBO						shadowMap[NumCascades];
		extern Engine::OrthographicAspect<>		shadowCamera[NumCascades];
		extern DX11::ObjectBuffer<TMatrix4<> >	shadowMatrix[NumCascades];
	#else
		extern DX11::FBO						shadowMap;
		extern Engine::OrthographicAspect<>		shadowCamera;
		extern DX11::ObjectBuffer<TMatrix4<> >	shadowMatrix;
	#endif
	extern DX11::BaseInstructionBlock		shadowInstruction;


	inline	int DetermineLODInstruction(float distance, float radius)
	{
		//outerDistance = std::max(0.f,outerDistance);
		float d = log(1.f + std::max(0.f,(distance-radius)/radius));
		return 3 - clamp((int)d,0,3);
	}

	
	namespace GeometryImage
	{
		void			BeginRendering(const float4&tone, float cameraDistanceFactor = 1.f);
		DX11::Texture	EndRendering();
	}


	namespace Renderer
	{
		extern Buffer0<PTurretBase>		animatedTurrets;
		extern double					now; //double-precision seconds since program start. Updated during RefreshFactionColors()

		/**
		Scene distance between core range visualization and the shadow behind it (to increase contrast).
		The shadow is rendered first and surrounds the actual visualization
		*/	
		static const float RangeShadowExtension = 0.2f;
		static const float RangeShadowOpacity = 0.2f;

		/**
		Creates/loads all commonly used rendering resources (shaders, for the most part)
		*/
		void	Create();
		void	Destroy();	//!< Frees all commonly used rendering resources
		


		void	AdvanceTurretAnimations();



		void	RenderBeam(const TVec3<>&base, float height, float width, const float4&bottomColor, const TVec4<>&topColor);
		void	RenderBeam(const TVec3<>&base, float height, float width);
		void	RenderPoleBeam(int pole, float width);
		void	SetBeamColor(const float4&bottomColor, const TVec4<>&topColor);




		void	Render(Structure&s, float light);
		void	Render(Structure&s);

		void	Render(Orbital::Ship&s, float light);
		void	Render(Orbital::Station&s, float light);

		void	RenderSimple(Structure&s, bool includeFoundation = false);
		void	RenderSimple(Orbital::Ship&s);
		void	RenderSimpleIdentity(Orbital::Ship&s);	//can't make const because of GetTurretBase
		void	RenderSimpleIdentity(const Vehicle&v);


		/**
		Renders all parts of a vehicle component. Called automatically where needed.
		Modifies (and does not reset) object matrix
		@param x Horizontal rotation angle of first and second parts
		@param y Vertical rotation angle of second part
		@param h Vertical offset from the specified system
		@param system Vehicle system
		@param comp Component to render
		*/
		void	RenderComponent(TMatrix4<> system, float h, const Component::Base&comp, float x, float y);
		void	RenderScaledComponent(TMatrix4<> system, float h, float scale, const Component::Base&comp, float x, float y);

		/**
		Renders both components (base/turret) of a vehicle. No extra calls beyond RenderComnponent()
		*/
		void	RenderSimple(const Vehicle&v);

		/**
		Renders the specified vehicle
		@param v Vehicle to render
		@param light Light level around the vehicle in [0,1]
		*/
		void	Render(const Vehicle&v, float light);



		/**
		Main rendering function, which renders all visible (and non-translucent) objects in the scene.
		Also clears the entity waste (if any)
		Has no effect (except clearing the waste), if @a objectView is false
		*/
		void	RenderScene();


		/**
		Renders any progress bars atop objects.
		Has no effect, if @a objectView is false
		*/
		void	RenderProgressBars();

		/**
		Renders translucent scene components (excluding progress bars).
		Has no effect, if @a objectView is false
		*/
		void	RenderTranslucent();

		/**
		Renders light shafts for satellites, as well as less visible ones atop every significant world pole
		*/
		void	RenderTranslucentBeams();


		/**
		Renders the resource space around each participating compound. Translucent. Alters depth-buffer. Should be called at the very end.
		Only renders compounds selected via any of the SelectCompoundAreas() overloads
		*/
		void	RenderCompoundAreas();

		/**
		Selects all compounds with a non-0 balance on the specified resource for area visualization
		@param owner Faction to select compounds of. Passing an empty parameter results in an empty selection
		@param producersOf Resource to look for. The result will be empty if there are no compounds with a non-0 balance on this resource
		*/
		void	SelectCompoundAreas(const OwnerParameter&owner, ResourceFlagSet producersOf, const TVec3<>&arrowTo, const MobileResourceEnvironment&, bool orbital);
		/**
		Selects all compounds with a non-0 balance on any resource for area visualization
		@param owner Faction to select compounds of. Passing an empty parameter results in all compounds with a non-0 balance being selected
		*/
		void	SelectCompoundAreas(const OwnerParameter&owner);


		/**
		Renders all ranges (attack, sensor, etc) of the selected entity and other entities if the respective settings are active.
		@param extendedShadow if true renders shaded and extended versions of the range spheres/cylinders/paraboloids
		*/
		void	RenderRanges(bool extendedShadow);


		#ifdef CASCADING_SHADOW
			/**
			Updates @a shadowMap, using the current configuration of @a shadowCamera
			*/
			void	RenderShadows(index_t cascade);
			void	RenderShadows(DX11::FBO&shadowMap, Engine::OrthographicAspect<>&shadowCamera, DX11::ObjectBuffer<TMatrix4<> >&shadowMatrix);

		#else
			void	RenderShadows();
		#endif

		void		EndShadowRendering();


		void	RenderRangeSphere(const TVec3<>&center, float radius, const float4&color);
		void	RenderRangeCylinder(const TVec3<>&direction, float bottomRadius, float topRadius, float bottom, float top, const float4&color);
		void	RenderRange(const Grid::LCellIdI&location, float zOffset, const PTurretBase&base, const float4&color, bool extended);



		/**
		Updates active and inactive colors of all factions. Should be called once per frame
		@param pulse Set true to let active entity colors pulse, false to use time-invariant colors
		*/
		void	RefreshFactionColors(bool pulse);
		void	Flush();

	}

}


#endif
#endif

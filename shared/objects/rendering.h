#ifdef HAS_WINDOW
#ifndef objectRenderingH
#define objectRenderingH

#include "../../client/ExtRenderer/DirectX11.h"
#include "../grid/stack.h"
#include "turretBase.h"
#include <engine/display.h>
#include <al/openal.h>



#define CASCADING_SHADOW

namespace Objects
{
	namespace DX11 = Engine::DX11;

	extern bool					resourceView, 
								showFriendlyRanges,
								showHostileRanges,
								objectView;


	extern DX11::ObjectBuffer<float4>	keyColor, overallTint;
	extern DX11::BaseInstructionBlock	stdVehicleShader,
										seeThroughShader,
										pathShader,
										jobShader,
										paraboloidShader,
										skyPortBeam,
										arrowInstruction;
	extern DX11::StreamVBO				arrowBuffer;
	extern DX11::VBO					sphereVBO[4];
	extern DX11::IBO32					sphereIBO[4], attackIBO;
	//extern DX11::InstructionBlock32		rangeInstruction[4];

	extern DX11::InstructionBlock32		cordon,
										foundation,
										userInteractionNotifier,
										gatheringPoint;
	//extern DX11::Texture				hexGrid;

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
		extern UINT		resultResolution;	//!< Defaults to 128 (=128*128)

		void			BeginRendering(const float4&tone, float cameraDistanceFactor = 1.f);
		DX11::Texture	EndRendering();
	}


	namespace Renderer
	{
		struct SmokeTrailIndex
		{
			index_t	trailIndex = InvalidIndex;
			bool	isOrbital = false;

			/**/	SmokeTrailIndex() = default;
			/**/	SmokeTrailIndex(index_t trailIndex, bool isOrbital):trailIndex(trailIndex),isOrbital(isOrbital)	{}
		};

		extern Buffer0<PTurretBase>		animatedTurrets;
		extern Buffer0<PWorldEntity>	attackers;	//!< everything that has a base and can attack goes in here
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
		

		void	CompileFlatDisc(Geometry&geo, int resolution, float r0, float r1, const float4&color0,const float4&color1);


		void	AdvanceTurretAnimations();



		void	RenderBeam(const TVec3<>&base, float height, float width, const float4&bottomColor, const TVec4<>&topColor);
		void	RenderBeam(const TVec3<>&base, float height, float width);
		void	RenderPoleBeam(int pole, float width);
		void	SetBeamColor(const float4&bottomColor, const TVec4<>&topColor);




		void	Render(Structure&s, float light);
		void	Render(Structure&s);

		#ifndef NO_SATELLITES
			void	Render(Satellite&s, float light);
		#endif

		void	RenderSimple(WorldEntity&e, bool includeFoundationIfStructure=false);
		void	RenderSimpleIdentity(WorldEntity&e, bool includeFoundationIfStructure=false);


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
		void	RenderTranslucent(bool orbital);

		/**
		Renders light shafts for satellites, as well as less visible ones atop every significant world pole
		*/
		void	RenderTranslucentBeams();


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


		/**
		Registers a new smoke trail as created by a weapon discharge
		@return Index to the newly created smoke trail. Valid until ClearSmokeTrails() is called.
		*/
		SmokeTrailIndex	RegisterSmokeTrail(const Buffer0<TVec4<> >&trajectory, bool aboveGrid);

		/**
		Tints the specified smoke trail in pulsing red.
		Passing an invalid index disables smoke trail highlighting.
		*/
		void	HighlightSmokeTrail(const SmokeTrailIndex&smokeTrailIndex);
		void	ClearSmokeTrailHighlighting();

		/**
		Removes all active smoke trails. May trigger gradual fading in the future
		*/
		void	ClearSmokeTrails();



		OpenAL::WaveBuffer	GetAmbientSoundOf(const String&entityTypeName, ConstName entityType);
	}

}


#endif
#endif

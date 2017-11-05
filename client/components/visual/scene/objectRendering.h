#ifdef HAS_WINDOW
#ifndef objectRenderingH
#define objectRenderingH

#include "../../../ExtRenderer/DirectX11.h"
#include <grid/stack.h>
#include <objects/turretBase.h>
#include <engine/display.h>
#include <al/openal.h>
#include <objects/components.h>
#include <objects/geometry.h>
#include "lighting.h"
#include "shadedRenderInstruction.h"


namespace Objects
{
	namespace DX11 = Engine::DX11;

	extern bool					resourceView, 
								showFriendlyRanges,
								showHostileRanges,
								objectView;


	extern DX11::ObjectBuffer<float4>	keyColor, overallTint;
	extern ShadedInstruction			stdVehicleShader;
	extern DX11::BaseInstructionBlock	seeThroughShader,
										pathShader,
										jobShader,
										skyPortBeam,
										arrowInstruction;
	extern DX11::StreamVBO				arrowBuffer;

	extern ShadedInstructionBlock32		userInteractionNotifier,
										cordon,
										foundation,
										gatheringPoint;
	//extern DX11::Texture				hexGrid;


	extern ShadedInstruction			standardCompiledShader, standardLocalizableShader;

	inline	int DetermineLODInstruction(float distance, float radius)
	{
		//outerDistance = std::max(0.f,outerDistance);
		float d = log(1.f + std::max(0.f,(distance-radius)/radius));
		return 3 - M::Clamp((int)d,0,3);
	}

	
	namespace GeometryImage
	{
		extern UINT		resultResolution;	//!< Defaults to 128 (=128*128)

		void			BeginRendering(const float4&tone, float cameraDistanceFactor = 1.f);
		DX11::Texture	EndRendering();
	}


	namespace Renderer
	{
		struct LODGeometry
		{
			static const constexpr count_t NumLODs = 4;

			FixedArray<DX11::VBO, NumLODs>		vbo;
			FixedArray<DX11::IBO32, NumLODs>	ibo;
			FixedArray<DX11::InstructionBlock32, NumLODs>	instruction;
			DX11::BaseInstructionBlock			shader;


			DX11::InstructionBlock32&			Prepare(int lod)
			{
				auto&inst = instruction[lod];
				if (inst.shader.IsEmpty())
				{
					inst.SetBase(shader);
					inst.vbo = vbo[lod];
					inst.ibo = ibo[lod];
				}
				return inst;
			}

			void					Destroy()
			{
				shader.Clear();
				foreach(vbo, v)
					v->Clear();
				foreach(ibo, i)
					i->Clear();
				foreach(instruction, inst)
					inst->Clear();
			}
		};

		extern LODGeometry					sphere, cylinder;



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





		void	RenderBeam(const M::TVec3<>&base, float height, float width, const float4&bottomColor, const M::TVec4<>&topColor);
		void	RenderBeam(const M::TVec3<>&base, float height, float width);
		void	RenderPoleBeam(int pole, float width);
		void	SetBeamColor(const float4&bottomColor, const M::TVec4<>&topColor);




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
		void	RenderComponent(M::TMatrix4<> system, float h, const Component::Base&comp, float x, float y);
		void	RenderScaledComponent(M::TMatrix4<> system, float h, float scale, const Component::Base&comp, float x, float y);

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


		void	Recompile(Objects::Structure&structure);

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
			void	RenderShadows(DX11::FBO&shadowMap, Engine::OrthographicAspect<>&shadowCamera, DX11::ObjectBuffer<M::TMatrix4<> >&shadowMatrix);

		#else
			void	RenderShadows();
		#endif

		void		EndShadowRendering();


		void	RenderRangeSphere(const M::TVec3<>&center, float radius, const float4&color, bool outer);
		void	RenderRangeCylinder(const M::TVec3<>&center, float radius, const float4&color, bool outer);
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
		SmokeTrailIndex	RegisterSmokeTrail(const Buffer0<M::TVec4<> >&trajectory, bool aboveGrid);

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

		void	RefreshSelectedSensory();



		Engine::OpenAL::Wave	GetAmbientSound(const WorldEntity&e);
		Engine::OpenAL::Wave	GetAmbientSoundOf(const String&entityTypeName, EntityType entityType);

	}

}


#endif
#endif

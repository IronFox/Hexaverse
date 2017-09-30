#ifdef HAS_WINDOW
#include <global_root.h>
#include "objectRendering.h"
#include <engine/aspect.h>
#include <math/vector.h>
#include <image/converter/magic.h>
#include <objects/structure.h>
#include <objects/vehicle.h>
#include <objects/satellite.h>
#include <objects/faction.h>
#include "progressBar.h"
#include "objectChoices.h"
#include "objectSelection.h"
#include <objects/gridAndChoices.h>
#include <runtimeConfig.h>
#include <objects/systems.h>
#include "objectComponent.h"
#include "terrainInstructions.h"
#include "terrain.h"
#include "display.h"
#include "../clientEvents.h"
#include "lighting.h"

namespace Camera
{
	extern Engine::VectorCamera<>	scene;
}



namespace Objects
{





	bool						resourceView = true,
								showHostileRanges = false,
								showFriendlyRanges = false,
								objectView = true;


	DX11::ObjectBuffer<float4>	keyColor, overallTint,
								cutOffConfig,
								beamBottomColor,
								beamTopColor;
	
	ShadedInstruction			stdVehicleShader;
								
	DX11::BaseInstructionBlock	seeThroughShader,
								pathShader,
								jobShader,
								attackRingShader,
								arrowInstruction,
								smokeTrailShader,
								skyPortBeam;
	DX11::StreamVBO				arrowBuffer;
	


	ShadedInstruction			standardCompiledShader, standardLocalizableShader;

	DX11::InstructionBlock32	attackRing;
	ShadedInstructionBlock32	cordon,
								userInteractionNotifier,
								foundation,
								gatheringPoint;
								
	DX11::BaseInstructionBlock	shadowInstruction;

	DX11::Texture				smokeTexture;

	static const float4			noTint;	//base constructor: (0,0,0,0)

	//DX11::Texture				hexGrid;




	M::TMatrix4<>							structureToPlaceSystem;






	namespace Renderer
	{

		class SelectionPrimitive
		{
		public:
			float4					tint;
			M::TMatrix4<>				innerSys, outerSys;
			LODGeometry				*geo = nullptr;
			float4					cutOffConfig;
			float					radius = 1.f;

			/**/					SelectionPrimitive() {}
			/**/					SelectionPrimitive(LODGeometry&geo) :geo(&geo) {}
			void					SetSystems(const M::Sphere<>&s)
			{
				radius = s.radius;
				cutOffConfig = float4(0, 0, Grid::BottomPlanetRadius, std::numeric_limits<float>::max());

				Mat::Eye(innerSys);
				innerSys.x.x = innerSys.y.y = innerSys.z.z = s.radius;
				innerSys.w.xyz = s.center;

				outerSys = innerSys;
				outerSys.x.x = outerSys.y.y = outerSys.z.z = s.radius + Renderer::RangeShadowExtension;


				CheckSystem();
			}

			void					SetCylinderSystems(const M::TVec3<>&topCenter, float radius)
			{
				const float radialHeight = Vec::length(topCenter);
				const M::TVec3<> dir = topCenter / radialHeight;

				radius = M::Min(radius, radialHeight);

				//const float alpha = asin(radius / radialHeight);


				const float flatRadialHeight = //cos(alpha)*sqrt(M::Sqr(radialHeight) - M::Sqr(radius));
					(M::Sqr(radialHeight) - M::Sqr(radius)) / radialHeight;
				const float flatRadius = sqrt(M::Sqr(radialHeight) - M::Sqr(radius) - M::Sqr(flatRadialHeight));


				//const float alpha = radius / radialHeight;
				//const float flatRadius = sin(alpha)*radialHeight;
				//const float flatRadialHeight = radialHeight * cos(alpha);

				this->radius = flatRadius;

				const constexpr float h0 = Grid::ToRadius(Grid::CellStack::BottomHeight);

				const float radiusAtBottom = flatRadius * h0 / radialHeight;
				float height = flatRadialHeight - h0;

				float fadeHeight = height;// +0.25f;

				cutOffConfig = float4(radiusAtBottom, flatRadius * fadeHeight / height, fadeHeight, sqrt(M::Sqr(radialHeight) - M::Sqr(radius)));


				Mat::makeAxisSystem(dir*h0, dir, 2, innerSys);

				outerSys = innerSys;

				CheckSystem();

			}

			void					CheckSystem() const
			{
				M::TVec3<> test;
				Vec::cross(outerSys.x.xyz, outerSys.y.xyz, test);
				ASSERT__(Vec::dot(test, outerSys.z.xyz) > 0);
			}

			void					Render(bool outer);

		};
		LODGeometry					sphere, cylinder;

		SelectionPrimitive			generalPurposePrimitive(sphere);
		Buffer0<SelectionPrimitive>	selectionPrimitives;


		void SelectionPrimitive::Render(bool outer)
		{
			const float d = Vec::distance(Camera::scene.GetAbsoluteLocation(), innerSys.w.xyz);
			const int instructionIndex = DetermineLODInstruction(d, radius);

			Objects::cutOffConfig.Update(outer ? (this->cutOffConfig + float4(Renderer::RangeShadowExtension, Renderer::RangeShadowExtension, 0, 0)) : this->cutOffConfig);
			auto&inst = geo->Prepare(instructionIndex);

			overallTint.Update(outer ? float4(0, 0, 0, RangeShadowOpacity) : tint);

			display.SetObjectMatrix(outer ? outerSys : innerSys);
			display.Render(inst, true);
		}





		void	AdvanceTurretAnimations();

		class SmokeTrailCollection
		{
		public:
			Buffer0<DX11::InstructionBlock32>	smokeTrails;
			index_t								selectedSmokeTrail = InvalidIndex;

			void	Render();
			index_t	RegisterSmokeTrail(const Buffer0<M::TVec4<> >&trajectory)
			{
				index_t result = smokeTrails.Count();
				DX11::InstructionBlock32&trail = smokeTrails.Append();
				static Geometry geo;
				geo.Clear();
				Component::Turret::CompileTrajectory(trajectory,Component::Turret::SmokeTrail,geo,1.f);
				((DX11::BaseInstructionBlock&)trail) = smokeTrailShader;
				trail.vbo.CreateFromStructs(geo.GetLOD(0).vertices);
				trail.ibo.Create(geo.GetLOD(0).indices);
				selectedSmokeTrail = InvalidIndex;
				return result;
			}
			void	Clear()
			{
				smokeTrails.Clear();
				selectedSmokeTrail = InvalidIndex;
			}

		}			smokeTrails[2];

	}

	Engine::OpenAL::Wave	Renderer::GetAmbientSound(const WorldEntity&e)
	{
		if (e.IsVehicle())
			return ((const Vehicle&)e).base->ambientSound;
		return GetAmbientSoundOf(e.GetTypeName(),e.GetType());
	}


	Engine::OpenAL::WaveBuffer	Renderer::GetAmbientSoundOf(const String&entityTypeName, EntityType entityType)
	{
		static GenericHashTable<EntityType, Engine::OpenAL::Wave>	waveTable;
		Engine::OpenAL::WaveBuffer result;
		if (waveTable.Query(entityType,result))
			return result;
		result = Engine::OpenAL::Load("resources/objectSounds/"+entityTypeName+".wav");
		waveTable.Set(entityType,result);	//don't care if it's empty
		return result;
	}


	Renderer::SmokeTrailIndex	Renderer::RegisterSmokeTrail(const Buffer0<M::TVec4<> >&trajectory, bool aboveGrid)
	{
		return SmokeTrailIndex(smokeTrails[aboveGrid].RegisterSmokeTrail(trajectory), aboveGrid);
	}

	void	Renderer::HighlightSmokeTrail(const SmokeTrailIndex&smokeTrailIndex)
	{
		smokeTrails[smokeTrailIndex.isOrbital].selectedSmokeTrail = smokeTrailIndex.trailIndex;
		smokeTrails[!smokeTrailIndex.isOrbital].selectedSmokeTrail = InvalidIndex;
	}

	void	Renderer::RefreshSelectedSensory()
	{
		selectionPrimitives.Clear();
		if (Objects::selectedEntity)
			Objects::selectedEntity->VisitSensors(Objects::selectedEntity->location, [](const World::Sensor&sensor)
			{
				auto&prim = selectionPrimitives.Append();
				prim.tint = float4(1, 1, 0, 0.8f);

				switch (sensor.type)
				{
				case World::Sensor::ShadedSphere:
				case World::Sensor::SolidSphere:
					prim.geo = &sphere;
					prim.SetSystems(M::Sphere<>(sensor.worldCenter, sensor.radius));
					break;
				case World::Sensor::UpperHalfSolidSphere:
				{
					prim.geo = &sphere;
					prim.SetSystems(M::Sphere<>(sensor.worldCenter, sensor.radius));
					const float radialHeight = Vec::length(sensor.worldCenter);
					float radius = M::Min(sensor.radius, radialHeight);
					const float flatRadialHeight = sqrt(M::Sqr(radialHeight) - M::Sqr(radius));
						//(M::Sqr(radialHeight) - M::Sqr(radius)) / radialHeight;
					//const float flatRadius = sqrt(M::Sqr(radialHeight) - M::Sqr(radius) - M::Sqr(flatRadialHeight));
					prim.cutOffConfig.z = flatRadialHeight;
					break;
				}
				case World::Sensor::ShadedCylinder:
				case World::Sensor::SolidCylinder:
					prim.geo = &cylinder;
					prim.SetCylinderSystems(sensor.worldCenter, sensor.radius);
					break;
				}
			}
			,true);



	}

	void	Renderer::ClearSmokeTrailHighlighting()
	{
		smokeTrails[0].selectedSmokeTrail = InvalidIndex;
		smokeTrails[1].selectedSmokeTrail = InvalidIndex;
	}

	void	Renderer::ClearSmokeTrails()
	{
		smokeTrails[0].Clear();
		smokeTrails[1].Clear();
	}


	void Renderer::RenderRangeSphere(const M::TVec3<>&center, float radius, const float4&color, bool outer)
	{
		generalPurposePrimitive.geo = &sphere;
		generalPurposePrimitive.SetSystems(M::Sphere<>(center, radius));
		generalPurposePrimitive.tint = color;

		generalPurposePrimitive.Render(outer);
	}

	void	Renderer::RenderRangeCylinder(const M::TVec3<>&center, float radius, const float4&color, bool outer)
	{

		generalPurposePrimitive.geo = &cylinder;
		generalPurposePrimitive.SetCylinderSystems(center, radius);
		generalPurposePrimitive.tint = color;

		generalPurposePrimitive.Render(outer);
	}


	

	void	Renderer::RenderRange(const Grid::LCellIdI&location, float zOffset, const PTurretBase&base, const float4&color, bool extended)
	{
		float explosionRadius = 0;//base->turret->explosionRadius;
		switch (base->turret->trajectoryType)
		{
			case TrajectoryType::Artillery:
			{
				if (extended)
					explosionRadius += RangeShadowExtension;
				Objects::ParaboloidBuffer::Allocate(*base).Render(location,
					zOffset,base->GetFlatAttackRange(),
					explosionRadius,color,
					extended
					);
			}
			break;
			case TrajectoryType::Missile:
			{
				const M::TVec3<>&d = Grid::low.cells[location.stackID].centerVertex.xyz;
				RenderRangeSphere(d * (Grid::ToRadius(location.height)+zOffset),base->GetFlatAttackRange() + explosionRadius,color, extended);
			}
			break;
			case TrajectoryType::VerticalBeam:
			{
				#ifndef NO_SATELLITES
					int h = Satellite::GetGroundHeight(location);
					if (h != badHeight)
					{
						const M::TVec3<>&d = Grid::low.cells[location.stackID].centerVertex.xyz;
						RenderRangeSphere(d * (Grid::ToRadius(h)+zOffset),base->GetFlatAttackRange() + explosionRadius,color);
					}
				#endif
			}
			break;
			default:
				FATAL__("Unsupported trajectory type "+String(base->turret->trajectoryType.ToString()));
		}
	}


	



	static bool flip=false;


	void Renderer::Destroy()
	{
		keyColor.Clear();
		sphere.Destroy();
		cylinder.Destroy();
		cutOffConfig.Clear();
		overallTint.Clear();
		beamBottomColor.Clear();
		beamTopColor.Clear();

		smokeTexture.Clear();

		seeThroughShader.Clear();
		stdVehicleShader.Clear();
		standardCompiledShader.Clear();
		standardLocalizableShader.Clear();
		pathShader.Clear();
		jobShader.Clear();
		attackRingShader.Clear();
		smokeTrailShader.Clear();
		arrowInstruction.Clear();
		skyPortBeam.Clear();
		foundation.Clear();
		cordon.Clear();
		userInteractionNotifier.Clear();
		attackRing.Clear();
		gatheringPoint.Clear();
		arrowBuffer.Clear();
		Objects::ParaboloidBuffer::StaticDestroy();

	}

	static	void Load(const M::Mesh<TDef<float> >&mesh, DX11::VBO&vbo, DX11::IBO32&ibo)
	{
		Geometry geo;
		for (index_t i = 0; i < mesh.vertex_field.Count(); i++)
		{
			const auto &vIn = mesh.vertex_field[i];
			geo.MakeVertex(vIn.position,float3(),float4(1));
		}
		foreach (mesh.triangle_field,t)
		{
			geo.MakeTriangle(t->v0 - mesh.vertex_field.pointer(),
							t->v1 - mesh.vertex_field.pointer(),
							t->v2 - mesh.vertex_field.pointer());
		}
		foreach (mesh.quad_field,t)
		{
			geo.MakeQuad(t->v0 - mesh.vertex_field.pointer(),
				t->v1 - mesh.vertex_field.pointer(),
				t->v2 - mesh.vertex_field.pointer(),
				t->v3 - mesh.vertex_field.pointer());
		}
		geo.ComputeNormals();
		vbo.CreateFromStructs(geo.GetLOD(0).vertices);
		ibo.Create(geo.GetLOD(0).indices);
	}


	void Renderer::CompileFlatDisc(Geometry&geo, int resolution, float r0, float r1, const float4&color0,const float4&color1)
	{
		geo.Clear();
		for (int i = 0; i < resolution; i++)
		{
			float fi = float(i) / float(resolution);
			float a = fi * M_PIF * 2.f;
			float x = cos(a);
			float y = sin(a);

			geo.MakeVertex(float3(x*r0,y*r0,0.1),float3(0,0,1),color0);
			geo.MakeVertex(float3(x*r1,y*r1,0.1),float3(0,0,1),color1);

			geo.MakeQuad(i*2,i*2+1,((i+1)%resolution)*2+1,((i+1)%resolution)*2);

		}

	}


	void Renderer::Create()
	{
		DXGI_FORMAT dbgFormat = DXGI_FORMAT_R8G8B8A8_UNORM;



		keyColor.Create(float4(0));
		overallTint.Create(float4(1));
		cutOffConfig.Create(float4(1));
		{
			Engine::VertexBinding binding;
			binding.color.Set(6, 4);
			binding.floats_per_vertex = 10;
			stdVehicleShader.binding = binding;
			seeThroughShader.binding = binding;
		}
		DX11::Shader::Binding bindings[] =
		{
			DX11::Shader::Binding(keyColor, "KeyColor"),
			DX11::Shader::Binding(overallTint, "Tint"),
			DX11::Shader::Binding(Lighting::GPUResources::atmosphereConfig, "AtmosphereConfig"),
			DX11::Shader::Binding(Lighting::GPUResources::sunDirection,"Sun"),
			DX11::Shader::Binding(Lighting::GPUResources::sunColor,"SunColor"),
			#ifdef CASCADING_SHADOW
				DX11::Shader::Binding(Lighting::GPUResources::shadowMatrix[0],"ShadowMatrix0"),
				DX11::Shader::Binding(Lighting::GPUResources::shadowMap[0].GetDepthStencilTarget(), "ShadowMap0"),
				DX11::Shader::Binding(Lighting::GPUResources::shadowMatrix[1],"ShadowMatrix1"),
				DX11::Shader::Binding(Lighting::GPUResources::shadowMap[1].GetDepthStencilTarget(), "ShadowMap1"),
			#else
				DX11::Shader::Binding(Lighting::GPUResources::shadowMatrix,"ShadowMatrix"),
				DX11::Shader::Binding(Lighting::GPUResources::shadowMap.GetDepthStencilTarget(), "ShadowMap"),
			#endif
			DX11::Shader::SamplerBinding(true, Engine::TextureFilter::Linear, 1, "ShadowSampler")
		};
		stdVehicleShader.LoadQualitiesFromFile("shaders/vehicleShape.hlsl", "vsMain", "psMain", bindings, ARRAYSIZE(bindings));
		stdVehicleShader.UpdateLayout();
		stdVehicleShader.SetTriangles();


		{
			{
				Engine::VertexBinding binding;
				binding.vertex.Set(0,2);
				binding.normal.Unset();
				binding.floats_per_vertex = 2;
				skyPortBeam.binding = binding;
			}

			beamBottomColor.Create(float4(1,0,0,1));
			beamTopColor.Create(float4(0,1,0,1));
			DX11::Shader::Binding bindings[] =
			{
				DX11::Shader::Binding(beamBottomColor, "BeamBottomColor"),
				DX11::Shader::Binding(beamTopColor, "BeamTopColor"),
				DX11::Shader::Binding(Lighting::GPUResources::atmosphereConfig, "AtmosphereConfig"),
				DX11::Shader::Binding(Lighting::GPUResources::sunDirection,"Sun"),
				DX11::Shader::Binding(Lighting::GPUResources::sunColor,"SunColor"),
				#ifdef CASCADING_SHADOW
					DX11::Shader::Binding(Lighting::GPUResources::shadowMatrix[0],"ShadowMatrix0"),
					DX11::Shader::Binding(Lighting::GPUResources::shadowMap[0].GetDepthStencilTarget(), "ShadowMap0"),
					DX11::Shader::Binding(Lighting::GPUResources::shadowMatrix[1],"ShadowMatrix1"),
					DX11::Shader::Binding(Lighting::GPUResources::shadowMap[1].GetDepthStencilTarget(), "ShadowMap1"),
				#else
					DX11::Shader::Binding(Lighting::GPUResources::shadowMatrix,"ShadowMatrix"),
					DX11::Shader::Binding(Lighting::GPUResources::shadowMap.GetDepthStencilTarget(), "ShadowMap"),
				#endif
				DX11::Shader::SamplerBinding(true, Engine::TextureFilter::Linear, 1, "ShadowSampler")
			};

			skyPortBeam.shader.LoadFromFile("shaders/skyPortBeam.hlsl","vsMain","psMain", bindings, ARRAYSIZE(bindings));
			skyPortBeam.UpdateLayout();
			skyPortBeam.SetTriangles();
			{
				float w = 1.f;
				float2 vertex[6] = {
					float2(-w,0),
					float2(w,0),
					float2(w,1),
					float2(-w,0),
					float2(w,1),
					float2(-w,1),
				};

				skyPortBeam.vbo.CreateFromStructs(vertex,6);
			}
		}


		seeThroughShader.shader.LoadFromFile("shaders/vehicleSeeThrough.hlsl","vsMain","psMain", bindings, ARRAYSIZE(bindings));
		seeThroughShader.UpdateLayout();
		seeThroughShader.SetTriangles();

		standardCompiledShader = stdVehicleShader;	//same for now

		standardLocalizableShader.binding = stdVehicleShader.binding;
		standardLocalizableShader.LoadQualitiesFromFile("shaders/structureShape.hlsl", "vsMain", "psMain", bindings, ARRAYSIZE(bindings));
		standardLocalizableShader.UpdateLayout();
		standardLocalizableShader.SetTriangles();

		{
			Image smoke;
			Magic::LoadFromFile(smoke,"resources/textures/smoke.png");
			smokeTexture.Load(smoke);


			DX11::Shader::Binding bindings[] =
			{
				DX11::Shader::Binding(overallTint, "Tint"),
				DX11::Shader::Binding(Lighting::GPUResources::sunDirection,"Sun"),
				DX11::Shader::Binding(Lighting::GPUResources::sunColor,"SunColor"),
				DX11::Shader::Binding(smokeTexture,"SmokeTexture"),
				DX11::Shader::SamplerBinding(false,Engine::TextureFilter::Trilinear,1,"SmokeSampler")
			};
			smokeTrailShader.binding = stdVehicleShader.binding;
			smokeTrailShader.shader.LoadFromFile("shaders/smokeTrail.hlsl", "vsMain", "psMain", bindings, ARRAYSIZE(bindings));
			smokeTrailShader.UpdateLayout();
			smokeTrailShader.SetTriangles();
		}

		{
			DX11::Shader::Binding bindings[] =
			{
				DX11::Shader::Binding(overallTint, "Tint"),
				DX11::Shader::Binding(Lighting::GPUResources::sunDirection,"Sun"),
				DX11::Shader::Binding(Lighting::GPUResources::sunColor,"SunColor"),
			};
			pathShader.binding = stdVehicleShader.binding;
			pathShader.shader.LoadFromFile("shaders/vehiclePath.hlsl", "vsMain", "psMain", bindings, ARRAYSIZE(bindings));
			pathShader.UpdateLayout();
			pathShader.SetTriangles();

			jobShader.binding = stdVehicleShader.binding;
			jobShader.shader.LoadFromFile("shaders/vehicleJob.hlsl", "vsMain", "psMain", bindings, ARRAYSIZE(bindings));
			jobShader.UpdateLayout();
			jobShader.SetTriangles();

			attackRingShader.binding = stdVehicleShader.binding;
			attackRingShader.shader.LoadFromFile("shaders/attackRing.hlsl", "vsMain", "psMain", bindings, ARRAYSIZE(bindings));
			attackRingShader.UpdateLayout();
			attackRingShader.SetTriangles();


		}

		{
			DX11::Shader::Binding bindings[] =
			{
				DX11::Shader::Binding(overallTint, "Tint"),
				DX11::Shader::Binding(Lighting::GPUResources::sunDirection,"Sun"),
				DX11::Shader::Binding(Lighting::GPUResources::sunColor,"SunColor"),
				DX11::Shader::Binding(cutOffConfig,"CutOffConfig"),
			};

			cylinder.shader.binding = stdVehicleShader.binding;
			cylinder.shader.shader.LoadFromFile("shaders/translucentCylinder.hlsl", "vsMain", "psMain", bindings, ARRAYSIZE(bindings));
			cylinder.shader.UpdateLayout();
			cylinder.shader.SetTriangles();
			sphere.shader.binding = stdVehicleShader.binding;
			sphere.shader.shader.LoadFromFile("shaders/translucentSphere.hlsl", "vsMain", "psMain", bindings, ARRAYSIZE(bindings));
			sphere.shader.UpdateLayout();
			sphere.shader.SetTriangles();
		}

		arrowInstruction = pathShader;
		arrowBuffer.Create(90,sizeof(Vertex));
		arrowInstruction.vbo = arrowBuffer;
		{
			shadowInstruction.binding = stdVehicleShader.binding;
			shadowInstruction.shader.LoadFromFile("shaders/objectShadow.hlsl", "vsMain", "psMain"/*, bindings, ARRAYSIZE(bindings)*/);
			shadowInstruction.UpdateLayout();
			shadowInstruction.SetTriangles();	
		}


		for (int k = 0; k < 4; k++)
		{
			{
				M::Mesh<TDef<float> >	mesh;
				mesh.buildSphere(20 * (1 << k));
				Load(mesh, sphere.vbo[k], sphere.ibo[k]);

			}
			{
				M::Mesh<TDef<float> >	mesh;
				mesh.buildCylinder(20 * (1 << k));
				Load(mesh, cylinder.vbo[k], cylinder.ibo[k]);
			}
		}


		{
			Geometry geo;
			static const int Resolution = 50;
			static const float r0 = 1,
								r1 = 2;
			CompileFlatDisc(geo,Resolution,r0,r1, float4(1,0,0,0), float4(1,0,0,0));	//red = key-color

			userInteractionNotifier = stdVehicleShader;
			userInteractionNotifier.vbo.CreateFromStructs(geo.GetLOD(0).vertices);
			userInteractionNotifier.ibo.Create(geo.GetLOD(0).indices);

			CompileFlatDisc(geo,Resolution,0.7f,0.8f,float4(1,1,1,0.5), float4(1,1,1,1));

			(DX11::BaseInstructionBlock&)attackRing = attackRingShader;
			attackRing.vbo.CreateFromStructs(geo.GetLOD(0).vertices);
			attackRing.ibo.Create(geo.GetLOD(0).indices);

			geo.LoadFromObj("objects/gatheringPoint.obj");

			gatheringPoint = stdVehicleShader;
			gatheringPoint.vbo.CreateFromStructs(geo.GetLOD(0).vertices);
			gatheringPoint.ibo.Create(geo.GetLOD(0).indices);

		}


		LogEvent(EventSignificance::Debug, nullptr, "Loading common geometry");

		{
			Geometry geo;
			geo.LoadFromObj("objects/cordon.obj");

			geo.Scale(vehicleScale);
			cordon = stdVehicleShader;
			cordon.vbo.CreateFromStructs(geo.GetLOD(0).vertices);
			cordon.ibo.Create(geo.GetLOD(0).indices);

			geo.Clear();
			geo.LoadFromObj("objects/foundation.obj");
			//geo.Scale(vehicleScale);
			foundation = stdVehicleShader;
			foundation.vbo.CreateFromStructs(geo.GetLOD(0).vertices);
			foundation.ibo.Create(geo.GetLOD(0).indices);
		}

		
		GlobalEvents::invalidateObjectCache << ClearSmokeTrailHighlighting;
		GlobalEvents::invalidateObjectCache << RefreshSelectedSensory;

		GlobalEvents::frameBegin << Renderer::AdvanceTurretAnimations;

		GlobalEvents::objectFlush << Renderer::Flush << ClearSmokeTrails;
		GlobalEvents::objectFlush << [](){Objects::Deselect(false);Objects::UpdateChoices(false);};
		GlobalEvents::terrainFlush << []()
		{
			Objects::resourceView = true;
			Objects::objectView = true;
		};
	}


		
	void Renderer::RenderScaledComponent(M::TMatrix4<> system, float h, float scale, const Component::Base&comp, float x, float y)
	{
		system.w.xyz += system.z.xyz * h;
		system.x.xyz *= scale;
		system.y.xyz *= scale;
		system.z.xyz *= scale;
		M::TMatrix4<> sub;
		const auto&attachment = Objects::VisualComponent::Of(comp);
		if (comp.parts.Count() > 0)
		{
			comp.GetSubSystem(0,x,sub);
			Mat::transformSystem(system,sub);
			system = sub;
			display.SetObjectMatrix(system);
			display.Render(attachment.partInstruction[0],true);
		}
		if (comp.parts.Count() > 1)
		{
			comp.GetSubSystem(1,y,sub);
			Mat::transformSystem(system,sub);
			system = sub;
			display.SetObjectMatrix(system);
			display.Render(attachment.partInstruction[1],true);
		}

	}

	static void _Locate(const WorldEntity&e)
	{
		display.SetObjectMatrix(e.system);
	}

	static void _RenderSimpleLocated(Structure&s, const M::TMatrix4<>&system)
	{
		const auto&shape = StructureHull::Of(s).constShape;
		display.Render(shape, true);
		DBG_ASSERT__(shape.ibo.IsSet());

		float zOffset;
		PTurretBase b = s.GetTurretBase(zOffset);
		if (b)
		{
			Renderer::RenderScaledComponent(system, zOffset/Grid::UnitScale /*Compensate for rescaled system matrix of structures: systems.cpp*/, 2.f/Grid::UnitScale, *b->turret, b->turretAngleX, b->turretAngleY);
			//display.SetObjectMatrix(system);
		}
	}

	#ifndef NO_SATELLITES
		static void _RenderSimpleLocated(Satellite&s, const M::TMatrix4<>&system)
		{
			display.Render(*s.baseRenderingInstruction, true);
			Renderer::RenderComponent(system, 0.f, *s.turret, s.turretAngleX, s.turretAngleY);
		}
	#endif

	static void _RenderSimpleLocated(Vehicle&v, const M::TMatrix4<>&system)
	{
		Renderer::RenderComponent(system,0,*v.base,0,0);
		Renderer::RenderComponent(system,v.base->height,*v.turret,v.turretAngleX,v.turretAngleY);
	}


	static void _RenderSimpleLocated(WorldEntity&e, const M::TMatrix4<>&system)
	{
		if (e.IsStructure())
		{
			_RenderSimpleLocated((Structure&)e,system);
			return;
		}
		#ifndef NO_SATELLITES
			if (e.IsSatellite())
			{
				_RenderSimpleLocated((Satellite&)e,system);
				return;
			}
		#endif
		if (e.IsVehicle())
		{
			_RenderSimpleLocated((Vehicle&)e,system);
			return;
		}
	}

	void Renderer::RenderSimple(WorldEntity&e, bool includeFoundation /*= false*/)
	{
		_Locate(e);
		if (e.IsStructure() && includeFoundation)
			display.Render(foundation, true);
		_RenderSimpleLocated(e,e.system);
	}





	void	Renderer::RenderSimpleIdentity(WorldEntity&e, bool includeFoundationIfStructure/*=false*/)
	{
		display.SetObjectMatrix(M::Matrix<>::eye4);
		if (e.IsStructure() )
			if (includeFoundationIfStructure)
				display.Render(foundation, true);
		_RenderSimpleLocated(e,M::Matrix<>::eye4);
	}



	namespace Renderer
	{
		double now = 0;
		float				GetPulse(double now, float min, float max)
		{
			return ((float)sin(now*2.0)*0.5f+0.5f)*(max - min) + min;
		}

		float					GetActiveColor(double now)
		{
			return GetPulse(now,0.75f,1.25f);
		}


		void	SmokeTrailCollection::Render()
		{
			if (smokeTrails.IsNotEmpty())
			{
				display.SetDepthTest(true,false);
				display.SetRasterizer(true,false);
				display.SetBlendMode(Engine::BlendMode::AlphaBlend);

		
				overallTint.Update(float4());
				foreach (smokeTrails,t)
				{
					display.Render(*t,true);
				}

				if (selectedSmokeTrail < smokeTrails.Count())
				{
					overallTint.Update(float4(1,0,0,GetPulse(now,0.5f,1.f)));
					display.Render(smokeTrails[selectedSmokeTrail],true);
				}

			}
		}

	}


	/*
	void					GetActiveColors(double now, float3&mine, float3&theirs)
	{
		float c = GetActiveColor(now);
		mine = float3 (0.5f, 0.5f, 1.f) * c;
		theirs = float3 (1.f, 0.5f, 0.5f) * c;
	}

	*/

	void	Renderer::RefreshFactionColors(bool doPulse)
	{
		static const Timer::Time t0 = timer.Now();
		now = timer.GetSecondsSinceD(t0);

		if (!Faction::mine)
		{
			//spectator
	
			float pulse = doPulse ? GetPulse(now,0.75f,1.f) : 1.f;


			foreach (Faction::allKnown,_f)
			{
				Faction&f = **_f;
				float3 c = f.color * 0.5f + 0.5f;
				
				f.inactiveEntityColor = c * 0.5f;
				f.activeEntityColor = c * pulse;
			}

		}
		else
		{
			float pulse = doPulse ? GetActiveColor(now) : 1.f;

			//float3 mine = float3 (0.5f, 0.5f, 1.f) * pulse;
			//float3 theirs = float3 (1.f, 0.5f, 0.5f) * pulse;
			static const float3 inactiveColorMine/*=0*/,
								inactiveColorTheirs(0.5f,0.25f,0.25f);

			foreach (Faction::allKnown,_f)
			{
				Faction&f = **_f;
				if (f.isOwnedByMe)
				{
					f.activeEntityColor = f.color * pulse;
						//mine;
					f.inactiveEntityColor = inactiveColorMine;
				}
				else
				{
					f.activeEntityColor = f.color * pulse;
						//theirs;
					f.inactiveEntityColor = f.color * 0.5f;
						//inactiveColorTheirs;
				}

			}

		}
	}


	#ifndef NO_SATELLITES
		void	Renderer::Render(Satellite&s, float light)
		{
			display.SetObjectMatrix(s.system);
			{
				M::TVec3<> color;
				Vec::interpolate(s.owner->inactiveEntityColor,s.owner->activeEntityColor,s.GetActivityLevel(),color);
				{
					if (s.unstable)
					{
						keyColor.Update(float4(flip ? float3() : color,light));
						
						if (!flip)
						{

							//WorkingStructure*ws = static_cast<WorkingStructure*>(&s);
							if (s.AwaitsUserInteraction())
							{
								display.Render(userInteractionNotifier,true);
							}
						}
					}
					else
						keyColor.Update(float4(color,light));
				}
			}
			_RenderSimpleLocated(s,s.system);
		}
	#endif

	void	Renderer::Render(Structure&s, float light)
	{
		_Locate(s);
		bool workingStructure = s.IsWorkingStructure();
		{
			M::TVec3<> color;
			Vec::interpolate(s.owner->inactiveEntityColor,s.owner->activeEntityColor,s.GetActivityLevel(),color);

			{
				if (s.unstable)
				{
					keyColor.Update(float4(flip ? float3() : color,light));
						
					if (!flip && workingStructure)
					{

						WorkingStructure*ws = static_cast<WorkingStructure*>(&s);
						if (ws->AwaitsUserInteraction())
						{
							display.Render(userInteractionNotifier,true);
						}
					}
				}
				else
					keyColor.Update(float4(color,light));
			}
		}
		if (s.HasFoundation())
			display.Render(foundation, true);
		_RenderSimpleLocated(s,s.system);

	}


	void	Renderer::Render(Structure&s)
	{
		const float light = GetLowLighting(s.location + (s.GetHeight()-1));
		//const float3&inactiveColor = s.owner->inactiveEntityColor;
		//const float3&activeColor = s.owner->activeEntityColor;

		Render(s,light);
	}


	void	Renderer::RenderComponent(M::TMatrix4<> system, float h, const Component::Base&comp, float x, float y)
	{
		M::TMatrix4<> sub;
		const auto&attachment = Objects::VisualComponent::Of(comp);

		if (comp.parts.Count() > 0)
		{
			system.w.xyz += system.z.xyz * h;

			comp.GetSubSystem(0,x,sub);
			Mat::transformSystem(system,sub);
			system = sub;
			display.SetObjectMatrix(system);
			display.Render(attachment.partInstruction[0],true);
		}
		if (comp.parts.Count() > 1)
		{
			comp.GetSubSystem(1,y,sub);
			Mat::transformSystem(system,sub);
			system = sub;
			display.SetObjectMatrix(system);
			display.Render(attachment.partInstruction[1],true);
		}

	}




	void Renderer::RenderSimple(const Vehicle&v)
	{
		RenderComponent(v.system,0,*v.base,0,0);
		RenderComponent(v.system,v.base->height,*v.turret,v.turretAngleX,v.turretAngleY);
	}

	void Renderer::Render(const Vehicle&v, float light)
	{
		{
			M::TVec3<> color;
			Vec::interpolate(v.owner->inactiveEntityColor,v.owner->activeEntityColor,v.GetActivityLevel(),color);
			{
				if (v.unstable)
					keyColor.Update(float4(flip ? float3() : color,light));
				else
					keyColor.Update(float4(color,light));
			}
		}
		RenderSimple(v);
	}




	void					ApplyStateTint(const WorldEntity&e, const float4&baseTint, bool selected)
	{
		M::TVec4<> effectiveTint={0,0,0,0};


		if (e.empDamageLevel)
		{
			float baseOpacity = M::Sqr(0.5f + 0.5f * (float)sin(Engine::timing.now*15.0));
			float opacity = (std::min(1.f,float(e.empDamageLevel) / 2.f)) * baseOpacity;
			//M::TVec4<> tint;
			Vec::interpolate(baseTint,float4(0,0,1,1),opacity,effectiveTint);



			//float okay = e.GetScanningRange() / e.GetMaxScanningRange() * (0.5f + 0.5f * sin(Engine::timing.now)) ;
			//float4 newTint = baseTint & float4( okay,okay ,1);
			//overallTint.Update(tint);
			//effectiveTint = tint;
		}
		else
			if (selected)
			{
				effectiveTint = baseTint;
			}
		#ifndef NO_HIBERNATION
			if (e.ToHibernating()->IsHibernating())
			{
				float t = (sin(Engine::timing.now*3.7)*0.5+0.5);
				//effectiveTint.rgb *= 0.3f * (1.f - t);
				effectiveTint = float4(0,0,0,1);
				//if ((Engine::timing.now64/Engine::timing.ticks_per_second) % 2)
				effectiveTint.rgb += float3(0,0.75f,0.75f) * (0.5f + 0.5f*t);
			}
		#endif

		if (e.IsSleeping())
			Vec::interpolate(effectiveTint,float4(0,0,0,1),0.7f,effectiveTint);
		else
		{
			if (e.IsWorkingStructure())
			{
				const WorkingStructure&ws = (const WorkingStructure&)(e);
				if (ws.IsEnabled() && !ws.IsHibernating())
				{
					float baseOpacity = (0.5f + 0.5f * (float)sin(Engine::timing.now*2.0));

					Vec::interpolate(effectiveTint,float4(1),baseOpacity*0.6f,effectiveTint);
				}
				else
					Vec::interpolate(effectiveTint,float4(0,0,0,1),0.7f,effectiveTint);
			}
			else
				if (e.IsVehicle())
				{
					if (((const Vehicle&)e).IsHibernating())
						Vec::interpolate(effectiveTint,float4(0,0,0,1),0.6f,effectiveTint);
				}
		}
		overallTint.Update(effectiveTint);
	}

	void					ApplyBaseTint(const WorldEntity&e, const float4&baseTint)
	{
		if (e.empDamageLevel)
			overallTint.Update(baseTint);
	}



	void					UpdateSeeThroughColor(bool isOwnedByMe, bool isSelected)
	{
		static const float4 mySeeThroughColor(0.6,0.7,1,1);
		static const float4 hostileSeeThroughColor(1,0.7,0.6,1);
		float4 color = isOwnedByMe ? mySeeThroughColor : hostileSeeThroughColor;
		if (isSelected)
			color.rgb *= Renderer::GetActiveColor(Renderer::now*5.0);
		overallTint.Update(color);

	}
	void					Renderer::RenderScene()
	{
		if (!objectView)
			return;


		//double now = Engine::timing.now;

	
		static const float4 baseTint(0);

		static const bool seeThrough = true;
		if (seeThrough)
		{
			display.StoreRendererState();
			display.SetDepthTest(false,false);

			display.LinkShader(seeThroughShader,false,true);
			display.SetBlendMode(Engine::BlendMode::AlphaBlend);

			overallTint.Update(noTint);

			vehicles.Foreach([](const Objects::PVehicle&v)
			{
				UpdateSeeThroughColor(v->isOwnedByMe, v==selectedEntity);
				//overallTint.Update(v->isOwnedByMe ? mySeeThroughColor : hostileSeeThroughColor);
				Renderer::RenderSimple(*v);
			});

			//foreach(structures, _s)
			//{
			//	Structure&s = **_s;
			//	//if (s.changed)
			//	//{
			//	//	s.Recompile();
			//	//	s.changed = false;
			//	//}
			//	UpdateSeeThroughColor(s.isOwnedByMe, &s==selectedEntity.get());
			//	
			//	//if (s.compiledShape.vbo.IsSet())
			//	//{
			//	//	display.SetObjectMatrix(M::Matrix<>::eye4);
			//	//	display.Render(s.compiledShape,true);
			//	//}
			//	_RenderSimple(s);
			//}

			display.UnlockShader();

			//display.SetBlendMode
			display.RestoreRendererState();
			//display.SetDepthTest(true,true);
		}



		overallTint.Update(baseTint);
		vehicles.Foreach([](const Objects::PVehicle&v)
		{
			const float light = GetLowLighting(v->location);

			bool isBuilding = v->IsBuilding();
			if (isBuilding)
			{
				if (v->isInsertedIntoGrid)
					display.SetObjectMatrix(v->system);
				else
				{
					M::TMatrix4<> m = M::Matrix<>::eye4;
					Objects::Vehicle::UpdateSystem(m,v->location);
					display.SetObjectMatrix(m);
				}
				keyColor.Update(float4(v->owner->activeEntityColor,light));
				display.Render(cordon,true);
			}
			if (v != selectedEntity)
			{
				ApplyStateTint(*v,baseTint,false);
				Renderer::Render(*v,light);
				ApplyBaseTint(*v,baseTint);
			}
		});
		#ifndef NO_SATELLITES
			satellites.Foreach([](const Objects::PVehicle&v)
			{
				const float light = GetLowLighting(v->location);

				bool isBuilding = v->IsBuilding();
				if (isBuilding)
				{
					display.SetObjectMatrix(v->system);
					keyColor.Update(float4(v->owner->activeEntityColor,light));
					display.Render(cordon,true);
				}
				if (v != selectedEntity)
				{
					ApplyStateTint(*v,baseTint,false);
					Renderer::Render(*v,light);
					ApplyBaseTint(*v,baseTint);
				}
			}
		#endif

		if (Marker::visitedSystems.IsNotEmpty())
		{
			display.SetObjectMatrix(M::Matrix<>::eye4);
			keyColor.Update(float4(0,1,1,1.f));
			foreach (Marker::visitedSystems,_sys)
			{
				float in = 1.f / (1.f + float(_sys->age)*0.1f);
				keyColor.Update(float4(0,in,in,1.f));
				display.SetObjectMatrix(_sys->system);
				display.Render(userInteractionNotifier,true);
			}
		}
		display.SetObjectMatrix(M::Matrix<>::eye4);

		overallTint.Update(noTint);

		structures.Foreach([](const Objects::PStructure&_s)
		{
			Structure&s = *_s;
			//if (&s == selectedEntity.get())	//Renderer compiled shape only here
				//continue;
			auto&shape = ConnectorHull::Allocate(s);
			if (s.changed)
			{
				shape.RecompileConnectors(s);
				s.changed = false;
			}
			if (shape.compiledShape.vbo.IsSet())
			{
				keyColor.Update(float4(s.owner->activeEntityColor,1.f));

				//ApplyStateTint(s,baseTint,false);
				display.Render(shape.compiledShape,true);
				//ApplyBaseTint(s,baseTint);
			}
		});

		flip = fmod(Engine::timing.now ,1.0) > 0.5;
		structures.Foreach([](const Objects::PStructure&_s)
		{
			Structure&s = *_s;
			const auto&shape = StructureHull::Of(s);
			//if (shape->constShape)
			{
				if (&s == selectedEntity.get())
					return;
				ApplyStateTint(s,baseTint,false);
				Renderer::Render(s);
				//ApplyBaseTint(s,baseTint);	//why?

			}
		});

		if (selectedEntity && selectedEntity->health > 0)
		{
			static const float3 red(1,0.25,0.25);
			float4 tint = selectedEntity->unstable ? float4(red,flip ? 0 : 0.8f) : float4(red,(float)sin(now*4.0)*0.2f + 0.6f);
			ApplyStateTint(*selectedEntity, tint,true);
			if (selectedEntity->IsStructure())
			{
				PStructure struc = To<Structure>::Cast(selectedEntity);
				Renderer::Render(*struc);
			}
			else
				if (selectedMobileEntity)
				{
					if (selectedVehicle)
						Renderer::Render(*selectedVehicle,GetLowLighting(selectedEntity->location));
					#ifndef NO_SATELLITES
						elif (selectedMobileEntity->IsSatellite())
							Renderer::Render(*To<Satellite>::Cast(selectedMobileEntity),GetLowLighting(selectedEntity->location));
					#endif
					else
						FATAL__("Can't render this: "+selectedEntity->GetName());
				}
				else
					FATAL__("Can't render this: "+selectedEntity->GetName());

			if (GatheringPoint::selected != Grid::LCellIdI::Invalid)
			{
				//PVehiclePlant plant = To<VehiclePlant>::Cast(struc);
				//if (plant->gatheringPoint != Grid::LCellIdI::Invalid)
				{
					display.SetObjectMatrix(GatheringPoint::selectedVisual);
					keyColor.Update(float4(selectedEntity->owner->activeEntityColor,1.f));
					//overallTint.Update(float4(1));

					display.Render(gatheringPoint,true);
				}
			}
			ApplyBaseTint(*selectedEntity, tint);
		}




		display.SetRasterizer(true,true);
		display.SetDepthTest(true,true);
	}

	void	Renderer::RenderProgressBars()
	{
		if (!objectView)
			return;
		
		ForeachMobileWorldEntity([](const PMobileEntity&v)
		{
			{
				int maxHealth = v->GetMaxHealth();
				bool isBuilding = v->IsBuilding();
				const MobileEntityProducer*p= v->CastToProducer();
				std::pair<int,int> progress2;
				bool isBuilding2 = (p && p->IsBuilding(progress2));
				if (isBuilding || v->health < maxHealth || isBuilding2)
				{
					ProgressBar::SetLocation(v->GetWorldPosition() IF_SATELLITES_P(, v->IsSatellite() ? 3.f : 1.f));
					//ProgressBar::RenderNewBar(0,float3(0.1,0.1,0.1));
					//ProgressBar::RenderNewBar(0.25f,float3(1,0.1,0.1));
					//ProgressBar::RenderNewBar(0.75f,float3(0.1,1,0.1));
					//ProgressBar::RenderNewBar(1,float3(0.1,0.1,1));
					if (isBuilding)
					{
						std::pair<int,int>progress = v->GetBuildingProgress();
						ProgressBar::RenderNewBar(progress.first,progress.second,float3(0.6,0.7,1));
					}
					if (isBuilding2)
					{
						ProgressBar::RenderNewBar(progress2.first,progress2.second,float3(0.6,0.7,1));
					}
					if (v->health < maxHealth)
					{
						float h = (float)v->health / float(maxHealth);
						ProgressBar::RenderNewBar(v->health,maxHealth,float3(1,0.5f*h,0.5f*h));
					}
				}
			}
		},false);


		structures.Foreach([](const Objects::PStructure&s)
		{
			int maxHealth = s->GetMaxHealth();
			//float progress;
			std::pair<int,int> progress;
			bool isBuilding = s->IsBuilding(progress);

			if (s->health < maxHealth || isBuilding)
			{
				ProgressBar::SetLocation(s->GetWorldPosition()
				+ 
				Grid::low.cells[s->location.stackID].centerVertex.xyz*(float(s->GetHeight()-1) * Grid::SingleLayerHeight));
				{
					if (isBuilding)
					{
						ProgressBar::RenderNewBar(progress.first,progress.second,float3(0.6,0.7,1));
					}
					if (s->health < maxHealth)
					{
						float h = (float)s->health / float(maxHealth);
						ProgressBar::RenderNewBar(s->health,maxHealth,float3(1,0.5f*h,0.5f*h));
					}
				}
			}
		});
		ProgressBar::End();
	}

				//RenderBeam(sat->system.w.xyz,-(Grid::ToRadius(sat->location.height) 
				//				- Grid::ToRadius(-Grid::CellStack::HalfRange)) /
				//				Grid::SingleLayerHeight,
				//				-0.1f,
				//				float4(color,0.25),
				//				float4(color,0.25));

	void	Renderer::RenderBeam(const M::TVec3<>&base, float height, float width)
	{
		M::TMatrix4<> sys;
		sys.w.xyz = base;
		sys.z.xyz = base;
		Vec::normalize(sys.z.xyz);
		Vec::cross(sys.z.xyz,sys.w.xyz - Camera::scene.GetAbsoluteLocation(),sys.x.xyz);
		Vec::normalize0(sys.x.xyz);
		Vec::cross(sys.x.xyz,sys.z.xyz,sys.y.xyz);
		Vec::normalize0(sys.y.xyz);
		Mat::resetBottomRow(sys);
		sys.y.xyz *= width;
		sys.x.xyz *= width;
			//ShowMessage(Vec::length(sys.z.xyz));
		sys.z.xyz *= height;
					
		display.SetObjectMatrix(sys);
		display.LazyRenderVertices(skyPortBeam);
	}

	void	Renderer::RenderPoleBeam(int pole, float width)
	{
		const M::TVec3<>&dir = Grid::low.cells[pole].cornerCenter;
		M::TVec3<> base;
		Vec::mult(dir,Grid::MeanPlanetRadius,base);
		M::TMatrix4<> sys;
		sys.w.xyz = base;
		sys.z.xyz = dir;
		Vec::cross(sys.z.xyz,sys.w.xyz - Camera::scene.GetAbsoluteLocation(),sys.x.xyz);
		Vec::normalize0(sys.x.xyz);
		Vec::cross(sys.x.xyz,sys.z.xyz,sys.y.xyz);
		Vec::normalize0(sys.y.xyz);
		Mat::resetBottomRow(sys);
		sys.y.xyz *= width;
		sys.x.xyz *= width;
			//ShowMessage(Vec::length(sys.z.xyz));
		sys.z.xyz *= Grid::SingleLayerHeight * Grid::CellStack::HalfRange * 2.f;
					
		display.SetObjectMatrix(sys);
		display.LazyRenderVertices(skyPortBeam);
		
	}

	void	Renderer::SetBeamColor(const float4&bottomColor, const M::TVec4<>&topColor)
	{
		beamBottomColor.Update(bottomColor);
		beamTopColor.Update(topColor);
	}

	void	Renderer::RenderBeam(const M::TVec3<>&base, float height, float width, const float4&bottomColor, const M::TVec4<>&topColor)
	{
		SetBeamColor(bottomColor,topColor);
		RenderBeam(base,height,width);
	}


	namespace Renderer
	{
		void RenderAttackRangeOf(const PWorldEntity&e, const float4&color, bool extendedShadow, bool includeEMPIfOwnedByMe)
		{
			float zOffset;
			PTurretBase base = e->GetTurretBase(zOffset);		
			if (base && base->turret->CanAttack() && (!e->isOwnedByMe || !base->turret->dealsEMPDamage || includeEMPIfOwnedByMe))
				RenderRange(e->location,zOffset,base,color,extendedShadow);
		}

		void RenderAttackRangeOf(const PWorldEntity&e, const Grid::LCellIdI&at,const float4&color, bool extendedShadow, bool includeEMPIfOwnedByMe)
		{
			float zOffset;
			PTurretBase base = e->GetTurretBase(zOffset);		
			if (base && base->turret->CanAttack() && (!e->isOwnedByMe || !base->turret->dealsEMPDamage || includeEMPIfOwnedByMe))
				RenderRange(at,zOffset,base,color,extendedShadow);
		}
	}

	void Renderer::RenderRanges(bool extendedShadow)
	{
		{
			float4 color = extendedShadow ? float4(0,0,0,RangeShadowOpacity) : float4(1,1,0,0.8f);

			foreach(selectionPrimitives, prim)
				prim->Render(extendedShadow);

			if (choiceVis.showArrow && choiceVis.hasArrowCellTarget && activeCursorMode == CursorMode::PlaceStructureMode && activeStructureToPlace)
			{
				float scanningRange = activeStructureToPlace->GetScanningRange();
				Renderer::RenderRangeSphere(structureToPlaceSystem.w.xyz,scanningRange,
					color, extendedShadow
					);
			}
		}
		float4 color = extendedShadow ? float4(0,0,0,RangeShadowOpacity) : float4(1,0,0,0.8f);
		if (selectedEntity)
			RenderAttackRangeOf(selectedEntity,color,extendedShadow,true);
		if (choiceVis.showArrow && choiceVis.hasArrowCellTarget && activeCursorMode == CursorMode::PlaceStructureMode && activeStructureToPlace)
			RenderAttackRangeOf(activeStructureToPlace,choiceVis.arrowCellTarget.cellLocation,color,extendedShadow,true);
		

		if (showHostileRanges || showFriendlyRanges)
		{
			ForeachWorldEntity(
				[color,extendedShadow](const PWorldEntity&e)
				{
					if (e != selectedEntity)
					{
						if ((e->isOwnedByMe && showFriendlyRanges && e->health > 0) || (!e->isOwnedByMe && showHostileRanges))
						{
							RenderAttackRangeOf(e,color,extendedShadow,false);
						}
					}
				}
				,true
			);
		}



		display.SetObjectMatrix(M::Matrix<>::eye4);
	}


	static void _RenderAttackRing(M::TMatrix4<>&sys, int i)
	{
		float fc = 1.f + float(i) * 0.175f;
		sys.x.xyz *= fc;
		sys.y.xyz *= fc;
		display.SetObjectMatrix(sys);
		display.Render(attackRing,true);

		sys.x.xyz /= fc;
		sys.y.xyz /= fc;
	}

	void	Renderer::RenderTranslucent(bool orbital)
	{
		if (!objectView)
			return;
		
		overallTint.Update(noTint);

		display.SetRasterizer(true,true);

		display.SetDepthTest(true,true);
			
		display.SetBlendMode(Engine::BlendMode::AlphaBlend);

		display.LinkShader(jobShader, false, true);

		Marker::mobileGhosts.Foreach([orbital](const Objects::PMobileEntity&m)
		{
			if (m->IsAboveGrid() != orbital)
				return;
			if (m->IsVehicle())
			{
				PVehicle v = To<Vehicle>::Cast(m);
				Renderer::RenderSimple(*v);
			}
			else
				#ifndef NO_SATELLITES
					if (m->IsSatellite())
					{
						PSatellite s = To<Satellite>::Cast(m);
						Renderer::RenderSimple(*s);
					}
					else
				#endif
					FATAL__("Unsupported ghost type "+m->GetTypeName());
		});
		Marker::structureGhosts.Foreach([orbital](const Objects::PStructure&s)
		{
			if (s->IsAboveGrid() != orbital)
				return;
			Renderer::RenderSimple(*s,s->HasFoundation());
		});

		Marker::mobileGhosts.Foreach([orbital](const Objects::PMobileEntity&m)
		{
			if (m->IsAboveGrid() != orbital)
				return;
			if (m->isOwnedByMe)
				overallTint.Update(float4(0.2, 0.2, 0.2, 1));
			else
				overallTint.Update(float4(1, 0, 0, 1));
			if (m->IsVehicle())
			{
				PVehicle v = To<Vehicle>::Cast(m);
				Renderer::RenderSimple(*v);
			}
			else
				#ifndef NO_SATELLITES
					if (m->IsSatellite())
					{
						PSatellite s = To<Satellite>::Cast(m);
						Renderer::RenderSimple(*s);
					}
					else
				#endif
					FATAL__("Unsupported ghost type "+m->GetTypeName());
		});
		Marker::structureGhosts.Foreach([orbital](const Objects::PStructure&s)
		{
			if (s->IsAboveGrid() != orbital)
				return;
			if (s->isOwnedByMe)
				overallTint.Update(float4(0.2, 0.2, 0.2, 1));
			else
				overallTint.Update(float4(1, 0, 0, 1));
			Renderer::RenderSimple(*s,s->HasFoundation());
		});
		display.UnlockShader();


		//depth-image:
		overallTint.Update(noTint);	//all zero tint makes shapes invisible
		ForeachMobileWorldEntity([orbital](const PMobileEntity&v)
		{
			if (v->IsAboveGrid() != orbital)
				return;

			{
				foreach (v->myTasks,t)
				{
					if (t->type == Vehicle::Task::Job)
					{
						const auto&visual = Objects::TaskAttachment::Of(*t);
						foreach (visual.localizableShapes,shape)
						{
							if (shape->depthTest && shape->preRenderDepth)
							{
//									display.SetDepthTest(shape->depthTest,false);

								display.SetObjectMatrix(shape->system);
								display.Render(shape->instruction);
							}

						}
					}
				}
			}
		},false);


		display.SetObjectMatrix(M::Matrix<>::eye4);
			
		display.SetDepthTest(true,false);


		overallTint.Update(noTint);

		static const float3 red(1,0.25,0.25);
		static const float3 red2(2,0.5,0.5);


		overallTint.Update(float4(0,0,0,1));	//make sure localizable shapes appear - but in black


		ForeachMobileWorldEntity([orbital](const PMobileEntity&v)
		{
			if (v->IsAboveGrid() != orbital)
				return;
			const bool selected = v == selectedEntity;
			if (selected)
				overallTint.Update(float4(red2,1));
			
			{
				foreach (v->myTasks,t)
				{
					if (t->visualization)
					{
						const auto&visual = Objects::TaskAttachment::Of(*t);
						foreach (visual.localizableShapes,shape)
						{
							if (shape->selectedOnly && !selected)
								continue;

							if (shape->flareDisplay)
							{
								if (!selected)
									continue;	//invisible anyway
								display.SetBlendMode(Engine::BlendMode::FlareBlend);
							}
							else
								display.SetBlendMode(Engine::BlendMode::AlphaBlend);

							display.SetDepthTest(shape->depthTest,false);

							display.SetObjectMatrix(shape->system);
							display.Render(shape->instruction);
						}
					}
				}
			}
			if (selected)
				overallTint.Update(float4(0,0,0,1));	//make sure localizable shapes appear - but in black
		},false);

		structures.Foreach([orbital](const Objects::PStructure&s)
		{
			if (s->IsAboveGrid() != orbital)
				return;

			float dummy;
			if (s->GetTurretBase(dummy))
			{
				PTower t = To<Tower>::DynamicCast(s);
				ASSERT__(t);
				if (s == selectedEntity)
					overallTint.Update(float4(red2,1));

			//if (v != selectedEntity)
				{
					foreach (t->attackQueue,at)
					{
						const auto&vis = TowerAttackVisualization::Of(*at);
						display.SetObjectMatrix(vis.blastSystem);
						display.Render(vis.blastVisualization);
					}
				}
				if (s == selectedEntity)
					overallTint.Update(noTint);

			}
			else
			{
					


			}
		
		});
		display.SetObjectMatrix(M::Matrix<>::eye4);




			
		display.SetBlendMode(Engine::BlendMode::AlphaBlend);

		//display.SetDepthTest(true,false);

		

		display.SetDepthTest(true,true);


		for (const auto&at : attackers)
		{
			if (at->IsAboveGrid() != orbital)
				continue;
			M::TMatrix4<> sys;
			const M::TVec3<>&up = Grid::low.cells[at->location.stackID].cornerCenter;
			Mat::makeAxisSystem(at->system.w.xyz, up,2,sys);
			sys.x.xyz *= Grid::UnitScale;
			sys.y.xyz *= Grid::UnitScale;
			sys.z.xyz *= Grid::UnitScale;
			#ifndef NO_SATELLITES
				if (at->IsSatellite())
				{
					sys.x.xyz *= 2.f;
					sys.y.xyz *= 2.f;
				}
			#endif
			const PTurretBase base = at->GetTurretBase();
			//DBG_ASSERT__(base);
			int rings = base->GetAttacksPerRound();
			int activeRings = base->CountPossibleAttacks();

			int delta = at->IsMobile() ? -1 : 0;

			if (activeRings)
			{
				overallTint.Update(at == selectedEntity ? float4(red,(float)sin(now*4.0)*0.2f + 0.6f) :   float4(at->owner->activeEntityColor,1.f));
				for (int i = 0; i < activeRings; i++)
				{
					_RenderAttackRing(sys,i+delta);
				}
			}

			if (activeRings != rings)
			{
				overallTint.Update(float4(float3(1),0.5f));
				for (int i = activeRings; i < rings; i++)
				{
					_RenderAttackRing(sys,i+delta);
				}
			}

		}

		display.SetObjectMatrix(M::Matrix<>::eye4);
		overallTint.Update(noTint);
	
		display.SetDepthTest(true,false);


		display.SetRasterizer(true,false);

		display.SetBlendMode(Engine::BlendMode::FlareBlend);
		overallTint.Update(float4(float3(0.18f,0.185f,2.f),1.f));

		ForeachMobileWorldEntity([orbital](const PMobileEntity&v)
		{
			if (v->IsAboveGrid() != orbital)
				return;
			if (v != selectedEntity)
			{
				foreach (v->myTasks,t)
				{
					if (t->visualization)
					{
						const auto&visual = Objects::TaskAttachment::Of(*t);

						display.Render(visual.instruction);
					}
				}

			}
		},false);
		display.SetBlendMode(Engine::BlendMode::AlphaBlend);
		overallTint.Update(noTint);
		display.SetDepthTest(false, false);

		structures.Foreach([orbital](const Objects::PStructure&s)
		{
			if (s->IsAboveGrid() != orbital)
				return;

			float zOffset;
			if (s != selectedEntity)
			{
				PTurretBase base =s->GetTurretBase(zOffset);
				if (base)
				{
					PTower t = To<Tower>::DynamicCast(s);

					ASSERT__(t);
					{
						foreach (t->attackQueue,at)
						{
							const auto&vis = TowerAttackVisualization::Of(*at);
							display.Render(vis.trajectoryVisualization);
						}
					}
				}
			}
		});

		display.SetDepthTest(false,false);
		display.SetBlendMode(Engine::BlendMode::AlphaBlend);


		display.SetDepthTest(true,false);
		display.SetBlendMode(Engine::BlendMode::AlphaBlend);
		RenderRanges(true);
		display.SetBlendMode(Engine::BlendMode::FlareBlend);
		RenderRanges(false);
		display.SetDepthTest(false,false);
		display.SetBlendMode(Engine::BlendMode::AlphaBlend);


		M::TVec4<> selectedTint;

		if (selectedEntity && selectedEntity->IsAboveGrid() == orbital)
		{
			const PMobileEntity&v = selectedMobileEntity;

			//To<Vehicle>::DynamicCast(selectedEntity);

			if (!selectedEntity->unstable)
				selectedTint = float4(red,(float)sin(now*4.0)*0.4f + 0.4f);
			else
				selectedTint = float4(red,flip ? 0 : 0.8f);
			overallTint.Update(selectedTint);
			if (v)
			{
				foreach (v->myTasks,t)
				{
					const auto&visual = Objects::TaskAttachment::Of(*t);
					display.Render(visual.instruction);
				}
			}

			float zOffset;
			PTurretBase base = selectedEntity->GetTurretBase(zOffset);
			if (!v && base)
			{
				PTower t = To<Tower>::DynamicCast(selectedEntity);
				ASSERT__(t);
				{
					foreach (t->attackQueue,at)
					{
						const auto&vis = TowerAttackVisualization::Of(*at);
						display.Render(vis.trajectoryVisualization);
					}
				}
			}



		}

		display.SetRasterizer(true,true);

		if (choiceVis.showArrow && choiceVis.hasArrowCellTarget && WorldEntity::IsAboveGrid(choiceVis.arrowFromCell) == orbital)
		{
			if (choiceVis.arrowCellTarget.cellLocation != choiceVis.arrowFromCell)
			{
				display.SetDepthTest(false,false);

				const float r =Grid::ToRadius(choiceVis.arrowCellTarget.radius);
				const float3&arrowTo = choiceVis.arrowCellTarget.worldLocation;
				float d2 = Vec::quadraticDistance(arrowTo,choiceVis.arrowFrom.worldLocation);

				static Buffer0<Vertex>	vertices;
				vertices.Clear();


				//if (d2 > 0.01f)
				{
					static const float3 color(1,0.5,0.5);
					float d = sqrt(d2);
					const auto&arrowFrom = choiceVis.arrowFrom.worldLocation;
					float3 dir = (arrowTo - arrowFrom).Normalize();
					float3 n = (dir | arrowTo).Normalize0() * 0.25f * Grid::UnitScale;
					float3 arrowBase = arrowTo - dir * 1.f;

					//arrowBuffer
					vertices	<< Vertex(arrowFrom-n,float3(),float4(0.25f,1,0,1))
								<< Vertex(arrowFrom,float3(),float4(0))
								<< Vertex(arrowBase,float3(),float4(0))

								<< Vertex(arrowFrom-n,float3(),float4(0.25f,1,0,1))
								<< Vertex(arrowBase,float3(),float4(0))
								<< Vertex(arrowBase-n,float3(),float4(1))

								<< Vertex(arrowFrom+n,float3(),float4(0.25f,1,0,1))
								<< Vertex(arrowBase,float3(),float4(0))
								<< Vertex(arrowFrom,float3(),float4(0))

								<< Vertex(arrowFrom+n,float3(),float4(0.25f,1,0,1))
								<< Vertex(arrowBase+n,float3(),float4(1))
								<< Vertex(arrowBase,float3(),float4(0))

								<< Vertex(arrowBase-n,float3(),float4(1))
								<< Vertex(arrowBase,float3(),float4(0))
								<< Vertex(arrowTo,float3(),float4(1))

								<< Vertex(arrowBase+n,float3(),float4(1))
								<< Vertex(arrowTo,float3(),float4(1))
								<< Vertex(arrowBase,float3(),float4(0))
					;
	/*

					LineRenderer::Rasterize(Objects::arrowFrom-n,float4(color,0),arrowBase-n,float4(color,1));
					LineRenderer::Rasterize(arrowBase-n,float4(color,1),arrowTo,float4(color,1));
					LineRenderer::Rasterize(Objects::arrowFrom+n,float4(color,0),arrowBase+n,float4(color,1));
					LineRenderer::Rasterize(arrowBase+n,float4(color,1),arrowTo,float4(color,1));

					const auto&cell = Grid::low.cells[dest.stackID];
					for (int i = 0; i < cell.numCorners; i++)
					{
						LineRenderer::Rasterize(cell.cornerVertex[i].xyz*r,float4(color,1),cell.cornerVertex[(i+1)%cell.numCorners].xyz*r,float4(color,1));

					}*/

					arrowBuffer.PartialUpdate(vertices.pointer(),vertices.Count(),sizeof(Vertex));

					display.RenderVertices(arrowInstruction,0,(UINT32)vertices.Count());
				}
			}

			if (activeCursorMode == CursorMode::PlaceStructureMode && activeStructureToPlace)
			{
				display.SetDepthTest(true,true);
				bool includeFoundation = false;//activeStructureToPlace->HasFoundation();	<- is correct but job visualization doesn't use foundations
				display.LinkShader(jobShader,false,true);
					//M::TMatrix4<> sys;
					BuildStructureSystem(choiceVis.arrowCellTarget.cellLocation,structureToPlaceSystem);
					display.SetObjectMatrix(structureToPlaceSystem);
					overallTint.Update(noTint);	//all zero tint makes shapes invisible
					if (includeFoundation)
						display.Render(foundation, true);
					_RenderSimpleLocated(*activeStructureToPlace,structureToPlaceSystem);
					display.SetObjectMatrix(structureToPlaceSystem);
					overallTint.Update(float4(selectedTint.rgb,selectedTint.a * 0.5f + 0.5f));
					if (includeFoundation)
						display.Render(foundation, true);
					_RenderSimpleLocated(*activeStructureToPlace,structureToPlaceSystem);
				display.UnlockShader();
				display.SetObjectMatrix(M::Matrix<>::eye4);
			}
		}

		smokeTrails[orbital].Render();

		display.SetRasterizer(true,true);
		display.SetDepthTest(true,true);

	}



	void		Renderer::Flush()
	{
		#ifndef ONE_COMPOUND_PER_FACTION
			compoundScope.Clear();
			interPylonArrows.Clear();
		#endif
		animatedTurrets.Clear();
		attackers.Clear();
	}

	
	namespace Renderer
	{
		void RenderFlyingUnitBeams(const Objects::PMobileEntity&e)
		{
			M::TVec3<> color = e->owner ? e->owner->activeEntityColor : float3();
			SetBeamColor(float4(color,0.25),
						float4(color,0.25));
			const float bottom = Grid::ToRadius(-Grid::CellStack::HalfRange);
			{
				//display.SetBlendMode(Engine::BlendMode::Add);
				RenderBeam(e->GetWorldPosition(),-(Grid::ToRadius(e->location.height) 
								- bottom) /
								Grid::SingleLayerHeight,
								0.1f
								);
			}
			bool first = true;
			for (const auto&t : e->myTasks)
			{
				if (t.ContainsMovement())
				{
					if (first)
					{
					//	display.SetBlendMode(Engine::BlendMode::AlphaBlend);
						SetBeamColor(float4(color,0.5f),
									float4(color,0.5f));
						first = false;
					}
					for (const auto&waypoint : t.foci)
					{
						RenderBeam(Grid::ToWorld(waypoint.target),-(Grid::ToRadius(waypoint.target.height) 
										- bottom) /
										Grid::SingleLayerHeight,
										0.05f);
					}

				}
			}
		}
	}

	void	Renderer::RenderTranslucentBeams()
	{
	/*	if (!objectView)
			return;
	*/	

		display.SetRasterizer(true,false);	//some shafts are flipped, but culling makes no sense anyways
		display.SetDepthTest(true,false);
		display.SetBlendMode(Engine::BlendMode::Add);
		
		#ifndef NO_SATELLITES
			foreach (satellites,_s)
				RenderFlyingUnitBeams(*_s);
		#endif
		bool nexusSet[12] = {false};

		foreach(skyLinks, _s)
		{
			const PSkyLinkStructure link = _s->first.lock();
			const PWorkingStructure ws = _s->second.lock();
			if (!link || !ws)
				continue;
			if (ws->location.stackID < 12)
				nexusSet[ws->location.stackID] = true;

			#ifndef NO_SATELLITES
			{
				bool render = false;
				int h1;
				PSkyHub hub = link->connectedToHub.lock();
				if (hub)
				{
					render = true;

					beamBottomColor.Update(float4(0.5,0.6,1.0,2.5));
					beamTopColor.Update(float4(0.5,0.6,1.0,2.5));
					h1 = hub->location.height;

				}
				else
				{
					if (ws->IsSkyPort())
					{
						PSkyPort port = To<SkyPort>::Cast(ws);
						if (ws->IsEnabled() && port->queue.IsNotEmpty() && port->queue.First().IsSatellite())
						{
							render = true;
							beamBottomColor.Update(float4(0.5,0.6,1.0,2.5));
							beamTopColor.Update(float4(0.5,0.6,1.0,0));
							h1 = Satellite::BaseResidenceHeight;
						}
					}
				}
				if (render)
				{
					M::TMatrix4<> sys = ws->system;
					Vec::cross(sys.z.xyz,sys.w.xyz - Camera::scene.GetAbsoluteLocation(),sys.x.xyz);
					Vec::normalize0(sys.x.xyz);
					Vec::cross(sys.x.xyz,sys.z.xyz,sys.y.xyz);
					Vec::normalize0(sys.y.xyz);
					sys.y.xyz *= 0.27f;
					sys.x.xyz *= 0.27f;
					//ShowMessage(Vec::length(sys.z.xyz));
					sys.z.xyz *= (Grid::ToRadius(h1) 
									- Grid::ToRadius(ws->location.height)) /
									Grid::SingleLayerHeight;
					
					display.SetObjectMatrix(sys);
					display.LazyRenderVertices(skyPortBeam);
				}
			}
			#endif
		}

		switch (RuntimeConfig::gameGoal)
		{
			case RuntimeConfig::GameGoal::ConquerNexus:
				for (int i = 0; i < 12; i++)
				{
					if (nexusSet[i])
						continue;
					M::TVec3<> color;
					if (BaseGrid::IsIsolatedCorner(i))
					{
						color= float3(1.0,0.6,0.5);
					}
					else
						if (RuntimeConfig::empZones)
							color= float3(0.5,0.6,1.0);
						else
							continue;
					SetBeamColor(float4(color,0.75),
								float4(color,0.75));
					RenderPoleBeam(i,0.2f);
				}
			break;
		}


		display.SetDepthTest(true,true);
		display.SetBlendMode(Engine::BlendMode::AlphaBlend);
		display.SetRasterizer(true,true);
	}

	static void _RenderShadow(const DX11::InstructionBlock32&instruction)
	{
		display.BindVertices(instruction.vbo,shadowInstruction.binding.GetStride(),shadowInstruction.GetLayout());
		display.BindIndices(instruction.ibo);
		display.GetDeviceContext()->DrawIndexed((UINT)instruction.ibo.GetEntryCount(),0,0);
	}


	static void _RenderShadow(const Structure&s)
	{
		display.SetObjectMatrix(s.system);
		const auto&vis = Objects::StructureHull::Of(s);
		_RenderShadow(vis.constShape);	
		if (s.HasFoundation())
			_RenderShadow(foundation);
	}


	static void _RenderShadow(M::TMatrix4<> system, float h, const Component::Base&comp, float x, float y)
	{
		system.w.xyz += system.z.xyz * h;
		M::TMatrix4<> sub;
		const auto&visual = Objects::VisualComponent::Of(comp);
		if (comp.parts.Count() > 0)
		{
			comp.GetSubSystem(0,x,sub);
			Mat::transformSystem(system,sub);
			system = sub;
			display.SetObjectMatrix(system);
			_RenderShadow(visual.partInstruction[0]);
		}
		if (comp.parts.Count() > 1)
		{
			comp.GetSubSystem(1,y,sub);
			Mat::transformSystem(system,sub);
			system = sub;
			display.SetObjectMatrix(system);
			_RenderShadow(visual.partInstruction[1]);
		}

	}
		
	static void _RenderShadowS(M::TMatrix4<> system, float h, float scale, const Component::Base&comp, float x, float y)
	{
		M::TMatrix4<> sub;
		const auto&visual = Objects::VisualComponent::Of(comp);

		if (comp.parts.Count() > 0)
		{
			system.w.xyz += system.z.xyz * h;
			system.x.xyz *= scale;
			system.y.xyz *= scale;
			system.z.xyz *= scale;

			comp.GetSubSystem(0,x,sub);
			Mat::transformSystem(system,sub);
			system = sub;
			display.SetObjectMatrix(system);
			_RenderShadow(visual.partInstruction[0]);
		}
		if (comp.parts.Count() > 1)
		{
			comp.GetSubSystem(1,y,sub);
			Mat::transformSystem(system,sub);
			system = sub;
			display.SetObjectMatrix(system);
			_RenderShadow(visual.partInstruction[1]);
		}

	}
	static void _RenderShadow(const Vehicle&v)
	{
		if (v.IsBuilding())
		{
			display.SetObjectMatrix(v.system);
			_RenderShadow(cordon);
		}
		_RenderShadow(v.system,0,*v.base,0,0);
		_RenderShadow(v.system,v.base->height,*v.turret,v.turretAngleX,v.turretAngleY);
	}
	#ifndef NO_SATELLITES
	static void _RenderShadow(const Satellite&s)
	{
		display.SetObjectMatrix(s.system);
		if (s.IsBuilding())
		{
			_RenderShadow(cordon);	//not sure if a cordon is appropriate here, but whatever
		}
		_RenderShadow(*s.baseRenderingInstruction);	
		_RenderShadow(s.system,0.f,*s.turret,s.turretAngleX,s.turretAngleY);
	}
	#endif

	void					Renderer::EndShadowRendering()
	{
		display.TargetBackbuffer();
	}

		

	void					Renderer::RenderShadows(
		#ifdef CASCADING_SHADOW
			DX11::FBO&shadowMap, Engine::OrthographicAspect<>&shadowCamera, DX11::ObjectBuffer<M::TMatrix4<> >&shadowMatrix
		#endif
	)
	{
		display.Pick(shadowCamera);

			
		display.ClearTarget(shadowMap);
		
		{
			M::TMatrix4<>	m;
			Mat::Mult(shadowCamera.projection,shadowCamera.view,m);
			shadowMatrix.Update(m);
		}

		display.TargetFBO(shadowMap);

		//if (objectView)
		{

			#if 0
			{
				static DX11::BaseInstructionBlock	debugInstruction;
				static DX11::ObjectBuffer<float4>	dbgFocusPoint;
				if (debugInstruction.shader.IsEmpty())
				{
					float2 p[3] = {float2(0,0),float2(1,0),float2(0,1)};

					dbgFocusPoint.Create(float4());

					DX11::Shader::Binding bindings[] =
					{
						DX11::Shader::Binding(dbgFocusPoint, "FocusPoint"),
					};


					debugInstruction.vbo.CreateFromStructs(p,3);
					debugInstruction.shader.LoadFromFile("shaders/shadowDebug.hlsl", "vsMain", "psMain", bindings, ARRAYSIZE(bindings));
					debugInstruction.binding.Append("POSITION",0,DXGI_FORMAT_R32G32_FLOAT);
					debugInstruction.UpdateLayout();
					debugInstruction.SetTriangles();
				}
				dbgFocusPoint.Update(float4(focusPoint,1));
				display.RenderVertices(debugInstruction);
			}
			#else
				display.LinkShader(shadowInstruction,false,true);

				vehicles.Foreach([](const Objects::PVehicle&v)
				{
					_RenderShadow(*v);
				});
				#ifndef NO_SATELLITES
					satellites.Foreach([](const Objects::PSatellite&s)
					{
						_RenderShadow(*s);
					});
				#endif
				display.SetObjectMatrix(M::Matrix<>::eye4);

				structures.Foreach([](const Objects::PStructure&_s)
				{
					Structure&s = *_s;
					auto&vis = Objects::ConnectorHull::Allocate(s);
					if (s.changed)
					{
						vis.RecompileConnectors(s);
						s.changed = false;
					}
					if (vis.compiledShape.ibo.IsSet())
						_RenderShadow(vis.compiledShape);

					float zOffset;
					PTurretBase b = s.GetTurretBase(zOffset);
					if (b)
					{
						_RenderShadowS(s.system, zOffset/Grid::UnitScale /*Compensate for rescaled system matrix of structures: systems.cpp*/, 2.f/Grid::UnitScale,*b->turret, b->turretAngleX, b->turretAngleY);
						display.SetObjectMatrix(M::Matrix<>::eye4);
					}

				});
				//display.SetObjectMatrix(M::Matrix<>::eye4);
				//return;

				structures.Foreach([](const Objects::PStructure&_s)
				{
					const Structure&s = *_s;
					display.SetObjectMatrix(s.system);
					const auto&vis = Objects::StructureHull::Of(s);
					_RenderShadow(vis.constShape);
				});
				display.SetObjectMatrix(M::Matrix<>::eye4);
				display.UnlockShader();
			#endif /*0*/
		}
	}


	#ifdef CASCADING_SHADOW
	void					Renderer::RenderShadows(index_t cascade)
	{
		//for (index_t i = 0; i < NumCascades; i++)
		RenderShadows(Lighting::GPUResources::shadowMap[cascade],Lighting::GPUResources::shadowCamera[cascade],Lighting::GPUResources::shadowMatrix[cascade]);
	}
	#endif



	Buffer0<PTurretBase>	Renderer::animatedTurrets;
	Buffer0<PWorldEntity>	Renderer::attackers;	//!< everything that has a base and can attack goes in here
	

	void			Renderer::AdvanceTurretAnimations()
	{
		#ifdef HAS_WINDOW
			foreach (animatedTurrets,v)
			{
				(*v)->turretAngleX += (*v)->turret->continuousRotationSpeed * Engine::timing.delta;
				//if ((*v)->turretAngleX > M_PIF*2.f)
				(*v)->turretAngleX = fmod((*v)->turretAngleX, M_PIF*2.f);
				//nothing else ... for now
			}
		#endif
	}

	


	
	namespace GeometryImage
	{
		DX11::FBO	fbo;
		UINT		resultResolution(128);
		Engine::AngularCamera<>	camera;

		void		BeginRendering(const float4&tone, float distanceFactor/*=1.f*/)
		{
			const Resolution res(resultResolution,resultResolution);
			Renderer::RefreshFactionColors(false);
			if (fbo.IsEmpty() || fbo.GetResolution() != res)
			{
				fbo.Create(res,DXGI_FORMAT_R8G8B8A8_UNORM,true,DX11::FBO::PrepareMips);

				camera.retraction = float3(0,0,-6 * distanceFactor);
				camera.angle.x = 45.f;
				camera.angle.z = 45.f;
				//camera.angle.y = 180.f;	//counters direct3d v-flipping during sampling
				camera.UpdateProjection2(1,0.001f,100000.f,30.f);
			}
			else
			{
				fbo.RecreateColorTarget(0,DXGI_FORMAT_R8G8B8A8_UNORM);	//previous one is in use

				camera.retraction = float3(0,0,-6 * distanceFactor);
			}
			camera.location.z = 0.5f*distanceFactor;
			camera.UpdateView();
			display.TargetFBO(fbo);
			display.ClearTarget(fbo,float4(tone.rgb*0.5f,tone.a));//, float4(0,1,0,1));

			display.Pick(camera);

			display.StoreRendererState();

			display.SetRasterizer(true,true);
			display.SetBlendMode(Engine::BlendMode::None);
			display.SetDepthTest(true,true);
			
			Lighting::UpdateSunDirection(float3(1,0,1),false);


			{
				keyColor.Update(float4(1.f - (1.f - tone.r)*0.5f,1.f - (1.f - tone.g)*0.5f,1.f - (1.f - tone.b)*0.5f,1));
				overallTint.Update(noTint);
//				overallTint.Update(float4(0,0,0,1));
			}
		}


		DX11::Texture	EndRendering()
		{
			
			display.TargetBackbuffer();
			fbo.GenerateMips();
			//display.SetBlendMode(Engine::BlendMode::AlphaBlend);
			display.RestoreRendererState();
				
			//fbo.Clear();
			return fbo.GetColorTarget();
		}






	}


}

#endif

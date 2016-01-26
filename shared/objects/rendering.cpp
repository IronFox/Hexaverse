#ifdef HAS_WINDOW
#include <global_root.h>
#include "rendering.h"
#include <engine/aspect.h>
#include "../../client/ExtRenderer/DirectX11.h"
#include <math/vector.h>
#include "structure.h"
#include "vehicle.h"
#include "satellite.h"
#include "faction.h"
#include "../../client/components/progressBar.h"
#include "visualChoices.h"
#include "visualSelection.h"
#include "gridAndChoices.h"
#include "../runtimeConfig.h"

namespace Camera
{
	extern Engine::VectorCamera<>	scene;
}
extern Engine::Display<Engine::DirectX11>	display;



namespace Objects
{

	bool						resourceView = false,
								showHostileRanges = false,
								showFriendlyRanges = false,
								objectView = true;


	DX11::ObjectBuffer<float4>	keyColor, overallTint,
								cylinderConfig,
								beamBottomColor,
								beamTopColor;

	DX11::BaseInstructionBlock	stdVehicleShader,
								seeThroughShader,
								pathShader,
								jobShader,
								cylinderShader,
								paraboloidShader,
								arrowInstruction,
								skyPortBeam;
	DX11::StreamVBO				arrowBuffer;
	DX11::VBO					sphereVBO[4],cylinderVBO[4];
	DX11::IBO32					sphereIBO[4],cylinderIBO[4], attackIBO;
	DX11::InstructionBlock32	sphereInstruction[4],cylinderInstruction[4];

	DX11::InstructionBlock32	cordon,
								foundation,
								userInteractionNotifier,
								gatheringPoint;

	static const float4			noTint;	//base constructor: (0,0,0,0)

	//DX11::Texture				hexGrid;

	#ifdef CASCADING_SHADOW

		DX11::FBO						shadowMap[NumCascades];
		Engine::OrthographicAspect<>	shadowCamera[NumCascades];
		DX11::ObjectBuffer<TMatrix4<> >	shadowMatrix[NumCascades];
	#else
		DX11::FBO						shadowMap;
		Engine::OrthographicAspect<>	shadowCamera;
		DX11::ObjectBuffer<TMatrix4<> >	shadowMatrix;
	#endif
	DX11::BaseInstructionBlock		shadowInstruction;


	void Renderer::RenderRangeSphere(const TVec3<>&center, float radius, const float4&color)
	{
		float d = Vec::distance(Camera::scene.GetAbsoluteLocation(),center);
		int instructionIndex = DetermineLODInstruction(d,radius);
		if (sphereInstruction[instructionIndex].shader.IsEmpty())
		{
			sphereInstruction[instructionIndex].SetBase(jobShader);
			sphereInstruction[instructionIndex].vbo = sphereVBO[instructionIndex];
			sphereInstruction[instructionIndex].ibo = sphereIBO[instructionIndex];
		}
		TMatrix4<> scaled = Matrix<>::eye4;
		scaled.x.x = scaled.y.y = scaled.z.z = radius;
		scaled.w.xyz = center;

		overallTint.Update(color);
				
		display.SetObjectMatrix(scaled);
		display.Render(sphereInstruction[instructionIndex],true);
	}

	void	Renderer::RenderRangeCylinder(const TVec3<>&direction, float bottomRadius, float topRadius, float bottom, float top, const float4&color)
	{
		float d = Vec::distance(Camera::scene.GetAbsoluteLocation(),direction*top);
		int instructionIndex = DetermineLODInstruction(d,topRadius);
		if (cylinderInstruction[instructionIndex].shader.IsEmpty())
		{
			cylinderInstruction[instructionIndex].SetBase(cylinderShader);
			cylinderInstruction[instructionIndex].vbo = cylinderVBO[instructionIndex];
			cylinderInstruction[instructionIndex].ibo = cylinderIBO[instructionIndex];
		}
		TMatrix4<> sys;
		Mat::makeAxisSystem(direction*bottom, direction,2,sys);

		overallTint.Update(color);
		cylinderConfig.Update(float4(bottomRadius,topRadius,top-bottom,0.f));
				
		display.SetObjectMatrix(sys);
		display.Render(cylinderInstruction[instructionIndex],true);
	}


	

	void	Renderer::RenderRange(const Grid::LCellIdI&location, float zOffset, const PTurretBase&base, const float4&color, bool extended)
	{
		switch (base->turret->trajectoryType)
		{
			case TrajectoryType::Artillery:
			{
				base->paraboloidBuffer.Render(location, 
					zOffset,base->GetFlatAttackRange(),
					base->turret->explosionRadius,color,
					extended
					);
			}
			break;
			case TrajectoryType::Missile:
			{
				const TVec3<>&d = Grid::low.cells[location.stackID].centerVertex.xyz;
				float explosionRadius = base->turret->explosionRadius;
				if (extended)
					explosionRadius += RangeShadowExtension;
				RenderRangeSphere(d * (Grid::ToRadius(location.height)+zOffset),base->GetFlatAttackRange() + explosionRadius,color);
			}
			break;
			case TrajectoryType::VerticalBeam:
			{
				int h = Satellite::GetGroundHeight(location);
				if (h != badHeight)
				{
					const TVec3<>&d = Grid::low.cells[location.stackID].centerVertex.xyz;
					float explosionRadius = base->turret->explosionRadius;
					if (extended)
						explosionRadius += RangeShadowExtension;
					RenderRangeSphere(d * (Grid::ToRadius(h)+zOffset),base->GetFlatAttackRange() + explosionRadius,color);
				}

			}
			break;
			default:
				FATAL__("Unsupported trajectory type "+String(base->turret->trajectoryType.ToString()));
		}
	}


	



	static const float3 inactiveColorMine(/*0.2f, 0.2f, 0.5f*/0.f), inactiveColorTheirs(0.5f,0.2f,0.2f);
	static bool flip=false;


	void Renderer::Destroy()
	{
		BaseCommand::textureMap.Clear();
		keyColor.Clear();
		cylinderConfig.Clear();
		overallTint.Clear();
		beamBottomColor.Clear();
		beamTopColor.Clear();

		seeThroughShader = stdVehicleShader = DX11::BaseInstructionBlock();
		Structure::standardCompiledShader = Structure::standardLocalizableShader =
			pathShader = jobShader = cylinderShader = paraboloidShader = 
			arrowInstruction = 
			skyPortBeam = 
			foundation = cordon = DX11::InstructionBlock32();
		userInteractionNotifier.Clear();
		gatheringPoint.Clear();
		arrowBuffer.Clear();
		attackIBO.Clear();

		for (index_t i = 0; i < ARRAYSIZE(sphereVBO); i++)
		{
			sphereVBO[i].Clear();
			sphereIBO[i].Clear();
			sphereInstruction[i].Clear();
		}
		for (index_t i = 0; i < ARRAYSIZE(cylinderVBO); i++)
		{
			cylinderVBO[i].Clear();
			cylinderIBO[i].Clear();
			cylinderInstruction[i].Clear();
		}
	}

	static	void Load(const ObjectMath::Mesh<TDef<float> >&mesh, DX11::VBO&vbo, DX11::IBO32&ibo)
	{
				//ASSERT_LESS__(mesh.vertex_field.length(),0x10000);

		Array<Vertex>	vertices(mesh.vertex_field.length());
		for (index_t i = 0; i < vertices.Count(); i++)
		{
			Vertex&vtx = vertices[i];
			const auto &vIn = mesh.vertex_field[i];
			vtx.color = float4(1,1,1,1);
			vtx.normal = vIn.position;
			vtx.vertex = vIn.position;
		}
		Array<UINT32>	indices(mesh.triangle_field.length()*3 + mesh.quad_field.length()*6);
		UINT32*iat = indices.pointer();
		foreach (mesh.triangle_field,t)
		{
			*iat++ = UINT32(t->v0 - mesh.vertex_field.pointer());
			*iat++ = UINT32(t->v1 - mesh.vertex_field.pointer());
			*iat++ = UINT32(t->v2 - mesh.vertex_field.pointer());
		}
		foreach (mesh.quad_field,t)
		{
			*iat++ = UINT32(t->v0 - mesh.vertex_field.pointer());
			*iat++ = UINT32(t->v1 - mesh.vertex_field.pointer());
			*iat++ = UINT32(t->v2 - mesh.vertex_field.pointer());

			*iat++ = UINT32(t->v0 - mesh.vertex_field.pointer());
			*iat++ = UINT32(t->v2 - mesh.vertex_field.pointer());
			*iat++ = UINT32(t->v3 - mesh.vertex_field.pointer());
		}
		ASSERT_CONCLUSION(indices,iat);
		vbo.CreateFromStructs(vertices);
		ibo.Create(indices);
	}


	void Renderer::Create()
	{
		DXGI_FORMAT dbgFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

		#ifdef CASCADING_SHADOW
			for (index_t i = 0; i < NumCascades; i++)
			{
				shadowMap[i].Create(Resolution(1024,1024),nullptr,0,true);
				shadowMatrix[i].Create(Matrix<>::eye4);
			}
		#else
	//		shadowMap.Create(Resolution(4096,4096),&dbgFormat,1,true);
			shadowMap.Create(Resolution(4096,4096),nullptr,0,true);
			shadowMatrix.Create(Matrix<>::eye4);
		#endif


		keyColor.Create(float4(0));
		overallTint.Create(float4(1));
		cylinderConfig.Create(float4(1));
		{
			Engine::VertexBinding binding;
			binding.color.set(6, 4);
			binding.floats_per_vertex = 10;
			stdVehicleShader.binding = binding;
			seeThroughShader.binding = binding;
		}
		DX11::Shader::Binding bindings[] =
		{
			DX11::Shader::Binding(keyColor, "KeyColor"),
			DX11::Shader::Binding(overallTint, "Tint"),
			DX11::Shader::Binding(Grid::Instruction::atmosphereConfig, "AtmosphereConfig"),
			DX11::Shader::Binding(Grid::Instruction::sunDirection,"Sun"),
			DX11::Shader::Binding(Grid::Instruction::sunColor,"SunColor"),
			#ifdef CASCADING_SHADOW
				DX11::Shader::Binding(Objects::shadowMatrix[0],"ShadowMatrix0"),
				DX11::Shader::Binding(Objects::shadowMap[0].GetDepthStencilTarget(), "ShadowMap0"),
				DX11::Shader::Binding(Objects::shadowMatrix[1],"ShadowMatrix1"),
				DX11::Shader::Binding(Objects::shadowMap[1].GetDepthStencilTarget(), "ShadowMap1"),
			#else
				DX11::Shader::Binding(Objects::shadowMatrix,"ShadowMatrix"),
				DX11::Shader::Binding(Objects::shadowMap.GetDepthStencilTarget(), "ShadowMap"),
			#endif
			DX11::Shader::SamplerBinding(true, Engine::TextureFilter::Linear, 1, "ShadowSampler")
		};
		stdVehicleShader.shader.LoadFromFile("shaders/vehicleShape.hlsl", "vsMain", "psMain", bindings, ARRAYSIZE(bindings));
		stdVehicleShader.UpdateLayout();
		stdVehicleShader.SetTriangles();


		{
			{
				Engine::VertexBinding binding;
				binding.vertex.set(0,2);
				binding.normal.unset();
				binding.floats_per_vertex = 2;
				skyPortBeam.binding = binding;
			}

			beamBottomColor.Create(float4(1,0,0,1));
			beamTopColor.Create(float4(0,1,0,1));
			DX11::Shader::Binding bindings[] =
			{
				DX11::Shader::Binding(beamBottomColor, "BeamBottomColor"),
				DX11::Shader::Binding(beamTopColor, "BeamTopColor"),
				DX11::Shader::Binding(Grid::Instruction::atmosphereConfig, "AtmosphereConfig"),
				DX11::Shader::Binding(Grid::Instruction::sunDirection,"Sun"),
				DX11::Shader::Binding(Grid::Instruction::sunColor,"SunColor"),
				#ifdef CASCADING_SHADOW
					DX11::Shader::Binding(Objects::shadowMatrix[0],"ShadowMatrix0"),
					DX11::Shader::Binding(Objects::shadowMap[0].GetDepthStencilTarget(), "ShadowMap0"),
					DX11::Shader::Binding(Objects::shadowMatrix[1],"ShadowMatrix1"),
					DX11::Shader::Binding(Objects::shadowMap[1].GetDepthStencilTarget(), "ShadowMap1"),
				#else
					DX11::Shader::Binding(Objects::shadowMatrix,"ShadowMatrix"),
					DX11::Shader::Binding(Objects::shadowMap.GetDepthStencilTarget(), "ShadowMap"),
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

		Structure::standardCompiledShader = stdVehicleShader;	//same for now

		Structure::standardLocalizableShader.binding = stdVehicleShader.binding;
		Structure::standardLocalizableShader.shader.LoadFromFile("shaders/structureShape.hlsl", "vsMain", "psMain", bindings, ARRAYSIZE(bindings));
		Structure::standardLocalizableShader.UpdateLayout();
		Structure::standardLocalizableShader.SetTriangles();

		{
			DX11::Shader::Binding bindings[] =
			{
				DX11::Shader::Binding(overallTint, "Tint"),
				DX11::Shader::Binding(Grid::Instruction::sunDirection,"Sun"),
				DX11::Shader::Binding(Grid::Instruction::sunColor,"SunColor"),
			};
			pathShader.binding = stdVehicleShader.binding;
			pathShader.shader.LoadFromFile("shaders/vehiclePath.hlsl", "vsMain", "psMain", bindings, ARRAYSIZE(bindings));
			pathShader.UpdateLayout();
			pathShader.SetTriangles();

			jobShader.binding = stdVehicleShader.binding;
			jobShader.shader.LoadFromFile("shaders/vehicleJob.hlsl", "vsMain", "psMain", bindings, ARRAYSIZE(bindings));
			jobShader.UpdateLayout();
			jobShader.SetTriangles();




		/*	Image img;
			Magic::LoadFromFile(img,"resources/hexgrid.png");

			hexGrid.Load(img);*/

			DX11::Shader::Binding bindings2[] =
			{
				DX11::Shader::Binding(overallTint, "Tint"),
				//DX11::Shader::Binding(hexGrid, "Texture"),
				//DX11::Shader::SamplerBinding(false,Engine::VisualEnum::TextureFilter::Trilinear,4,"Sampler"),
			};

			paraboloidShader.binding = stdVehicleShader.binding;
			paraboloidShader.shader.LoadFromFile("shaders/attackParaboloid.hlsl", "vsMain", "psMain", bindings2, ARRAYSIZE(bindings2));
			paraboloidShader.UpdateLayout();
			paraboloidShader.SetTriangles();
		}

		{
			DX11::Shader::Binding bindings[] =
			{
				DX11::Shader::Binding(overallTint, "Tint"),
				DX11::Shader::Binding(Grid::Instruction::sunDirection,"Sun"),
				DX11::Shader::Binding(Grid::Instruction::sunColor,"SunColor"),
				DX11::Shader::Binding(cylinderConfig,"CylinderConfig"),
			};

			cylinderShader.binding = stdVehicleShader.binding;
			cylinderShader.shader.LoadFromFile("shaders/translucentCylinder.hlsl", "vsMain", "psMain", bindings, ARRAYSIZE(bindings));
			cylinderShader.UpdateLayout();
			cylinderShader.SetTriangles();
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
				ObjectMath::Mesh<TDef<float> >	mesh;
				mesh.buildSphere(20 * (1 << k));
				Load(mesh,sphereVBO[k],sphereIBO[k]);

			}
			{
				ObjectMath::Mesh<TDef<float> >	mesh;
				mesh.buildCylinder(20 * (1 << k));
				Load(mesh,cylinderVBO[k],cylinderIBO[k]);
			}
		}
	}


		
	void Renderer::RenderScaledComponent(TMatrix4<> system, float h, float scale, const Component::Base&comp, float x, float y)
	{
		system.w.xyz += system.z.xyz * h;
		system.x.xyz *= scale;
		system.y.xyz *= scale;
		system.z.xyz *= scale;
		TMatrix4<> sub;
		if (comp.parts.Count() > 0)
		{
			comp.GetSubSystem(0,x,sub);
			Mat::transformSystem(system,sub);
			system = sub;
			display.SetObjectMatrix(system);
			display.Render(comp.parts[0].second,true);
		}
		if (comp.parts.Count() > 1)
		{
			comp.GetSubSystem(1,y,sub);
			Mat::transformSystem(system,sub);
			system = sub;
			display.SetObjectMatrix(system);
			display.Render(comp.parts[1].second,true);
		}

	}

	static void _Locate(Structure&s)
	{
		display.SetObjectMatrix(s.system);
	}

	static void _RenderSimpleLocated(Structure&s)
	{
		display.Render(*s.localizableShape, true);


		float zOffset;
		PTurretBase b = s.GetTurretBase(zOffset);
		if (b)
		{
			Renderer::RenderScaledComponent(s.system, zOffset, 2.f, *b->turret, b->turretAngleX, b->turretAngleY);
		}
	}

	void Renderer::RenderSimple(Structure&s, bool includeFoundation /*= false*/)
	{
		_Locate(s);
		if (includeFoundation)
			display.Render(foundation, true);
		_RenderSimpleLocated(s);
	}

	static void _RenderSimpleLocated(Satellite&s, const TMatrix4<>&system)
	{
		display.Render(*s.baseRenderingInstruction, true);
		Renderer::RenderComponent(system, 0.f, *s.turret, s.turretAngleX, s.turretAngleY);
	}



	void	Renderer::RenderSimple(Satellite&s)
	{
		//float scale = Vec::length(s.system.x.xyz);
		display.SetObjectMatrix(s.system);
		_RenderSimpleLocated(s,s.system);
	}

	void	Renderer::RenderSimpleIdentity(Satellite&s)
	{
		//float scale = Vec::length(s.system.x.xyz);
		display.SetObjectMatrix(Matrix<>::eye4);
		_RenderSimpleLocated(s,Matrix<>::eye4);
	}

	void	Renderer::Render(Satellite&s, const float3&activeColor, const float3&inactiveColor, float light)
	{
		display.SetObjectMatrix(s.system);
		{
			if (!s.IsActive())
			{
				keyColor.Update(float4(inactiveColor, light));
			}
			else
			{
				if (s.unstable)
				{
					keyColor.Update(float4(flip ? float3() : activeColor,light));
						
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
					keyColor.Update(float4(activeColor,light));
			}
		}
		_RenderSimpleLocated(s,s.system);
	}


	void	Renderer::Render(Structure&s, const float3&activeColor, const float3&inactiveColor, float light)
	{
		_Locate(s);
		bool workingStructure = s.IsWorkingStructure();
		{
			if (!s.IsActive())
			{
				keyColor.Update(float4(inactiveColor, light));
			}
			else
			{
				if (s.unstable)
				{
					keyColor.Update(float4(flip ? float3() : activeColor,light));
						
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
					keyColor.Update(float4(activeColor,light));
			}
		}
		if (s.HasFoundation())
			display.Render(foundation, true);
		_RenderSimpleLocated(s);

	}

	void	Renderer::Render(Structure&s, const float3&activeColorMine, const float3&activeColorTheirs)
	{
		const float light = GetLowLighting(s.location + (s.GetHeight()-1));
		const float3&inactiveColor = s.isMine ? inactiveColorMine : inactiveColorTheirs;
		const float3&activeColor = s.isMine ? activeColorMine : activeColorTheirs;

		Render(s,activeColor,inactiveColor,light);
	}


	void	Renderer::RenderComponent(TMatrix4<> system, float h, const Component::Base&comp, float x, float y)
	{
		system.w.xyz += system.z.xyz * h;
		TMatrix4<> sub;
		if (comp.parts.Count() > 0)
		{
			comp.GetSubSystem(0,x,sub);
			Mat::transformSystem(system,sub);
			system = sub;
			display.SetObjectMatrix(system);
			display.Render(comp.parts[0].second,true);
		}
		if (comp.parts.Count() > 1)
		{
			comp.GetSubSystem(1,y,sub);
			Mat::transformSystem(system,sub);
			system = sub;
			display.SetObjectMatrix(system);
			display.Render(comp.parts[1].second,true);
		}

	}




	void Renderer::RenderSimple(const Vehicle&v)
	{
		RenderComponent(v.system,0,*v.base,0,0);
		RenderComponent(v.system,v.base->height,*v.turret,v.turretAngleX,v.turretAngleY);
	}

	void Renderer::Render(const Vehicle&v, const float3&activeColorMine, const float3&activeColorTheirs, float light)
	{
		const float3&inactiveColor = v.isMine ? inactiveColorMine : inactiveColorTheirs;
		const float3&activeColor = v.isMine ? activeColorMine : activeColorTheirs;

		{
			if (!v.IsActive())
				keyColor.Update(float4(inactiveColor, light));
			else
			{
				if (v.unstable)
					keyColor.Update(float4(flip ? float3() : activeColor,light));
				else
					keyColor.Update(float4(activeColor,light));
			}
		}
		RenderSimple(v);
	}


	float					GetActiveColor(double now)
	{
		return 1.f + (float)sin(now*2.0) * 0.25f;
	}

	void					GetActiveColors(double now, float3&mine, float3&theirs)
	{
		float c = GetActiveColor(now);
		mine = float3 (0.5f, 0.5f, 1.f) * c;
		theirs = float3 (1.f, 0.5f, 0.5f) * c;
	}


	void					ApplyStateTint(const WorldEntity&e, const float4&baseTint, bool selected)
	{
		TVec4<> effectiveTint={0,0,0,0};


		if (e.empDamageLevel)
		{
			float baseOpacity = sqr(0.5f + 0.5f * (float)sin(Engine::timing.now*15.0));
			float opacity = (std::min(1.f,float(e.empDamageLevel) / 2.f)) * baseOpacity;
			//TVec4<> tint;
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

		if (e.IsWorkingStructure())
		{
			const WorkingStructure&ws = (const WorkingStructure&)(e);
			if (ws.IsEnabled() && !ws.IsHibernating())
			{
				float baseOpacity = (0.5f + 0.5f * (float)sin(Engine::timing.now*2.0));

				Vec::interpolate(effectiveTint,float4(1),baseOpacity*0.6f,effectiveTint);
			}
			else
				Vec::interpolate(effectiveTint,float4(0,0,0,1),0.6f,effectiveTint);
		}
		else
			if (e.IsVehicle())
			{
				if (((const Vehicle&)e).IsHibernating())
					Vec::interpolate(effectiveTint,float4(0,0,0,1),0.6f,effectiveTint);
			}

		overallTint.Update(effectiveTint);
	}

	void					ApplyBaseTint(const WorldEntity&e, const float4&baseTint)
	{
		if (e.empDamageLevel)
			overallTint.Update(baseTint);
	}

	double now = 0;


	void					UpdateSeeThroughColor(bool isMine, bool isSelected)
	{
		static const float4 mySeeThroughColor(0.6,0.7,1,1);
		static const float4 hostileSeeThroughColor(1,0.7,0.6,1);
		float4 color = isMine ? mySeeThroughColor : hostileSeeThroughColor;
		if (isSelected)
			color.rgb *= GetActiveColor(now*5.0);
		overallTint.Update(color);

	}
	void					Renderer::RenderScene()
	{
		waste.Clear();
		static const Timer::Time t0 = timer.Now();
		now = timer.GetSecondsSinceD(t0);
		if (!objectView)
			return;

		//double now = Engine::timing.now;

		float3 activeColorMine,activeColorTheirs;
		GetActiveColors(now,activeColorMine,activeColorTheirs);
	
		static const float4 baseTint(0);

		static const bool seeThrough = true;
		if (seeThrough)
		{
			display.StoreRendererState();
			display.SetDepthTest(false,false);

			display.LinkShader(seeThroughShader,false,true);
			display.SetBlendMode(Engine::BlendMode::AlphaBlend);

			overallTint.Update(noTint);
			foreach(vehicles, _v)
			{
				PVehicle&v = *_v;
				UpdateSeeThroughColor(v->isMine, v==selectedEntity);
				//overallTint.Update(v->isMine ? mySeeThroughColor : hostileSeeThroughColor);
				Renderer::RenderSimple(*v);
			}

			//foreach(structures, _s)
			//{
			//	Structure&s = **_s;
			//	//if (s.changed)
			//	//{
			//	//	s.Recompile();
			//	//	s.changed = false;
			//	//}
			//	UpdateSeeThroughColor(s.isMine, &s==selectedEntity.get());
			//	
			//	//if (s.compiledShape.vbo.IsSet())
			//	//{
			//	//	display.SetObjectMatrix(Matrix<>::eye4);
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
		foreach(vehicles, _v)
		{
			const PVehicle&v = *_v;
			const float light = GetLowLighting(v->location);

			bool isBuilding = v->IsBuilding();
			if (isBuilding)
			{
				display.SetObjectMatrix(v->system);
				keyColor.Update(float4(v->isMine ? activeColorMine : activeColorTheirs,light));
				display.Render(cordon,true);
			}
			if (v != selectedEntity)
			{
				ApplyStateTint(*v,baseTint,false);
				Renderer::Render(*v,activeColorMine,activeColorTheirs ,light);
				ApplyBaseTint(*v,baseTint);
			}
		}
		foreach(satellites, _v)
		{
			const PSatellite&v = *_v;
			const float light = GetLowLighting(v->location);

			bool isBuilding = v->IsBuilding();
			if (isBuilding)
			{
				display.SetObjectMatrix(v->system);
				keyColor.Update(float4(v->isMine ? activeColorMine : activeColorTheirs,light));
				display.Render(cordon,true);
			}
			if (v != selectedEntity)
			{
				ApplyStateTint(*v,baseTint,false);
				Renderer::Render(*v,v->isMine ? activeColorMine : activeColorTheirs,v->isMine ? inactiveColorMine : inactiveColorTheirs ,light);
				ApplyBaseTint(*v,baseTint);
			}
		}


		if (Marker::visitedSystems.IsNotEmpty())
		{
			display.SetObjectMatrix(Matrix<>::eye4);
			keyColor.Update(float4(0,1,1,1.f));
			foreach (Marker::visitedSystems,_sys)
			{
				float in = 1.f / (1.f + float(_sys->age)*0.1f);
				keyColor.Update(float4(0,in,in,1.f));
				display.SetObjectMatrix(_sys->system);
				display.Render(userInteractionNotifier,true);
			}
		}
		display.SetObjectMatrix(Matrix<>::eye4);

		overallTint.Update(noTint);

		foreach(structures, _s)
		{
			Structure&s = **_s;
			//if (&s == selectedEntity.get())	//Renderer compiled shape only here
				//continue;
			if (s.changed)
			{
				s.Recompile();
				s.changed = false;
			}
			if (s.compiledShape.vbo.IsSet())
			{
				if (s.isMine)
					keyColor.Update(float4(activeColorMine,1.f));
				else
					keyColor.Update(float4(activeColorTheirs,1.f));

				//ApplyStateTint(s,baseTint,false);
				display.Render(s.compiledShape,true);
				//ApplyBaseTint(s,baseTint);
			}
		}

		flip = fmod(Engine::timing.now ,1.0) > 0.5;
		foreach(structures, _s)
		{
			Structure&s = **_s;
			if (s.localizableShape)
			{
				if (&s == selectedEntity.get())
					continue;
				ApplyStateTint(s,baseTint,false);
				Renderer::Render(s,activeColorMine,activeColorTheirs );
				ApplyBaseTint(s,baseTint);

			}
		}


		if (selectedEntity && selectedEntity->health > 0)
		{
			static const float3 red(1,0.25,0.25);
			float4 tint = selectedEntity->unstable ? float4(red,flip ? 0 : 0.8f) : float4(red,(float)sin(now*4.0)*0.2f + 0.6f);
			ApplyStateTint(*selectedEntity, tint,true);
			if (selectedEntity->IsStructure())
			{
				PStructure struc = To<Structure>::Cast(selectedEntity);
				Renderer::Render(*struc,activeColorMine,activeColorTheirs);
			}
			else
				if (selectedVehicle)
					Renderer::Render(*selectedVehicle,activeColorMine,activeColorTheirs,GetLowLighting(selectedEntity->location));
				else
					if (selectedMobileEntity && selectedMobileEntity->IsSatellite())
					{
						Renderer::Render(*To<Satellite>::Cast(selectedMobileEntity),activeColorMine,activeColorTheirs,GetLowLighting(selectedEntity->location));
					}
					else
						FATAL__("Can't render this: "+selectedEntity->GetName());

			if (GatheringPoint::selected != Grid::LCellIdI::Invalid)
			{
				//PVehiclePlant plant = To<VehiclePlant>::Cast(struc);
				//if (plant->gatheringPoint != Grid::LCellIdI::Invalid)
				{
					display.SetObjectMatrix(GatheringPoint::selectedVisual);
					keyColor.Update(float4(activeColorMine,1.f));
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
					ProgressBar::SetLocation(v->system.w.xyz, v->IsSatellite() ? 3.f : 1.f);
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
		});


		for (const auto&s : structures)
		{
			int maxHealth = s->GetMaxHealth();
			//float progress;
			std::pair<int,int> progress;
			bool isBuilding = s->IsBuilding(progress);

			if (s->health < maxHealth || isBuilding)
			{
				ProgressBar::SetLocation(s->system.w.xyz 
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
		}
		ProgressBar::End();
	}

				//RenderBeam(sat->system.w.xyz,-(Grid::ToRadius(sat->location.height) 
				//				- Grid::ToRadius(-Grid::CellStack::HalfRange)) /
				//				Grid::SingleLayerHeight,
				//				-0.1f,
				//				float4(color,0.25),
				//				float4(color,0.25));

	void	Renderer::RenderBeam(const TVec3<>&base, float height, float width)
	{
		TMatrix4<> sys;
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
		const TVec3<>&dir = Grid::low.cells[pole].cornerCenter;
		TVec3<> base;
		Vec::mult(dir,Grid::VerticalBase,base);
		TMatrix4<> sys;
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

	void	Renderer::SetBeamColor(const float4&bottomColor, const TVec4<>&topColor)
	{
		beamBottomColor.Update(bottomColor);
		beamTopColor.Update(topColor);
	}

	void	Renderer::RenderBeam(const TVec3<>&base, float height, float width, const float4&bottomColor, const TVec4<>&topColor)
	{
		SetBeamColor(bottomColor,topColor);
		RenderBeam(base,height,width);
	}


	namespace Renderer
	{
		void RenderAttackRangeOf(const PWorldEntity&e, const float4&color, bool extendedShadow)
		{
			float zOffset;
			PTurretBase base = e->GetTurretBase(zOffset);		
			if (base && base->turret->CanAttack() && (!e->isMine || !base->turret->dealsEMPDamage))
				RenderRange(e->location,zOffset,base,color,extendedShadow);
		}
	}

	void Renderer::RenderRanges(bool extendedShadow)
	{
		if (selectedEntity)
		{
			float scanningRange = selectedEntity->GetScanningRange();
			if (extendedShadow)
				scanningRange += RangeShadowExtension;

			float4 color = extendedShadow ? float4(0,0,0,RangeShadowOpacity) : float4(1,1,0,0.8f);

			Renderer::RenderRangeSphere(selectedEntity->system.w.xyz,scanningRange,
				color
				);
			float zOffset;
			PTurretBase base = selectedEntity->GetTurretBase(zOffset);		
			if (base && base->turret->groundScanningRange > 0.f)
			{
				scanningRange = selectedEntity->EMPModify(base->turret->groundScanningRange,2);
				float refHeight = Grid::ToRadius(selectedEntity->location.height);
				float top = Grid::ToRadius(/*selectedEntity->location.height*/Grid::CellStack::HalfRange);
				float bottom = Grid::ToRadius(-Grid::CellStack::HalfRange);
				float topR = scanningRange*top/refHeight;
				float bottomR = scanningRange *bottom/refHeight;
				if (extendedShadow)
				{
					topR += RangeShadowExtension;
					bottomR += RangeShadowExtension;
				}
				RenderRangeCylinder(selectedEntity->system.z.xyz,bottomR,topR,bottom,top,color);
			}

		}
		float4 color = extendedShadow ? float4(0,0,0,RangeShadowOpacity) : float4(1,0,0,0.8f);
		if (selectedEntity)
			RenderAttackRangeOf(selectedEntity,color,extendedShadow);
		

		if (showHostileRanges || showFriendlyRanges)
		{
			ForeachWorldEntity(
				[color,extendedShadow](const PWorldEntity&e)
				{
					if (e != selectedEntity)
					{
						if ((e->isMine && showFriendlyRanges && e->health > 0) || (!e->isMine && showHostileRanges))
						{
							RenderAttackRangeOf(e,color,extendedShadow);
						}
					}
				}
			);


		}
		display.SetObjectMatrix(Matrix<>::eye4);
	}

	void	Renderer::RenderTranslucent()
	{
		if (!objectView)
			return;
		
		overallTint.Update(noTint);

		display.SetRasterizer(true,true);

		display.SetDepthTest(true,true);
			
		display.SetBlendMode(Engine::BlendMode::AlphaBlend);

		display.LinkShader(jobShader, false, true);
		foreach(Marker::mobileGhosts, _v)
		{
			const PMobileEntity&m = *_v;
			if (m->IsVehicle())
			{
				PVehicle v = To<Vehicle>::Cast(m);
				Renderer::RenderSimple(*v);
			}
			else
				if (m->IsSatellite())
				{
					PSatellite s = To<Satellite>::Cast(m);
					Renderer::RenderSimple(*s);
				}
				else
					FATAL__("Unsupported ghost type "+m->GetTypeName());
		}
		foreach(Marker::structureGhosts, _s)
		{
			const PStructure&s = *_s;
			Renderer::RenderSimple(*s,s->HasFoundation());

		}

		foreach(Marker::mobileGhosts, _v)
		{
			const PMobileEntity&m = *_v;
			if (m->isMine)
				overallTint.Update(float4(0.2, 0.2, 0.2, 1));
			else
				overallTint.Update(float4(1, 0, 0, 1));
			if (m->IsVehicle())
			{
				PVehicle v = To<Vehicle>::Cast(m);
				Renderer::RenderSimple(*v);
			}
			else
				if (m->IsSatellite())
				{
					PSatellite s = To<Satellite>::Cast(m);
					Renderer::RenderSimple(*s);
				}
				else
					FATAL__("Unsupported ghost type "+m->GetTypeName());
		}
		foreach(Marker::structureGhosts, _s)
		{
			const PStructure&s = *_s;
			if (s->isMine)
				overallTint.Update(float4(0.2, 0.2, 0.2, 1));
			else
				overallTint.Update(float4(1, 0, 0, 1));
			Renderer::RenderSimple(*s,s->HasFoundation());
		}
		display.UnlockShader();


		//depth-image:
		overallTint.Update(noTint);	//all zero tint makes shapes invisible
		ForeachMobileWorldEntity([](const PMobileEntity&v)
		{
			{
				foreach (v->myTasks,t)
				{
					if (t->type == Vehicle::Task::Job)
						foreach (t->localizableShapes,shape)
						{
							if (shape->depthTest)
							{
//									display.SetDepthTest(shape->depthTest,false);

								display.SetObjectMatrix(shape->system);
								display.Render(shape->instruction);
							}

						}

				}
			}
		});


		display.SetObjectMatrix(Matrix<>::eye4);
			
		display.SetDepthTest(true,false);


		overallTint.Update(noTint);

		static const float3 red(1,0.25,0.25);
		static const float3 red2(2,0.5,0.5);


		overallTint.Update(float4(0,0,0,1));	//make sure localizable shapes appear - but in black


		ForeachMobileWorldEntity([](const PMobileEntity&v)
		{
			//if (v != selectedEntity)
			if (v == selectedEntity)
				overallTint.Update(float4(red2,1));
			
			{
				foreach (v->myTasks,t)
				{
					if (t->type == Vehicle::Task::Job)
						display.SetBlendMode(Engine::BlendMode::AlphaBlend);
					else
						display.SetBlendMode(Engine::BlendMode::FlareBlend);

					foreach (t->localizableShapes,shape)
					{
						display.SetDepthTest(shape->depthTest,false);

						display.SetObjectMatrix(shape->system);
						display.Render(shape->instruction);

					}

				}
			}
			if (v == selectedEntity)
				overallTint.Update(float4(0,0,0,1));	//make sure localizable shapes appear - but in black
		});


		foreach(structures, _s)
		{
			const PStructure&s = *_s;
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
						display.SetObjectMatrix(at->blastSystem);
						display.Render(at->blastVisualization);
					}
				}
				if (s == selectedEntity)
					overallTint.Update(noTint);

			}
			else
			{
					


			}
		
		}
		display.SetObjectMatrix(Matrix<>::eye4);




		overallTint.Update(noTint);
			
		display.SetBlendMode(Engine::BlendMode::AlphaBlend);

		//display.SetDepthTest(true,false);

		

		display.SetDepthTest(false,false);


		display.SetRasterizer(true,false);

		ForeachMobileWorldEntity([](const PMobileEntity&v)
		{
			if (v != selectedEntity)
			{
				foreach (v->myTasks,t)
				{
					display.Render(t->visualization);
				}

			}
		});
		
		foreach(structures, _s)
		{
			const PStructure&s = *_s;
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
							display.Render(at->trajectoryVisualization);
						}
					}
				}
			}
		}

		display.SetDepthTest(false,false);
		display.SetBlendMode(Engine::BlendMode::AlphaBlend);


		display.SetDepthTest(true,false);
		display.SetBlendMode(Engine::BlendMode::AlphaBlend);
		RenderRanges(true);
		display.SetBlendMode(Engine::BlendMode::FlareBlend);
		RenderRanges(false);
		display.SetDepthTest(false,false);
		display.SetBlendMode(Engine::BlendMode::AlphaBlend);

		if (selectedEntity)
		{
			const PMobileEntity&v = selectedMobileEntity;
			//To<Vehicle>::DynamicCast(selectedEntity);
			if (!selectedEntity->unstable)
				overallTint.Update(float4(red,(float)sin(now*4.0)*0.4f + 0.4f));
			else
				overallTint.Update(float4(red,flip ? 0 : 0.8f));
			if (v)
			{
				foreach (v->myTasks,t)
				{
					display.Render(t->visualization);
				}
			}

			float dummy;
			if (!v && selectedEntity->GetTurretBase(dummy))
			{
				PTower t = To<Tower>::DynamicCast(selectedEntity);
				ASSERT__(t);
				{
					foreach (t->attackQueue,at)
					{
						display.Render(at->trajectoryVisualization);
					}
				}
			}



		}

		display.SetRasterizer(true,true);


		if (showArrow && hasArrowCellTarget && arrowCellTarget != arrowFromCell)
		{
			display.SetDepthTest(false,false);

			const float r =Grid::ToRadius(arrowCellTarget.height);
			float3 arrowTo = Grid::low.cells[arrowCellTarget.stackID].centerVertex.xyz * r;
			float d2 = Vec::quadraticDistance(arrowTo,Objects::arrowFrom);

			static Buffer0<Vertex>	vertices;
			vertices.Clear();


			//if (d2 > 0.01f)
			{
				static const float3 color(1,0.5,0.5);
				float d = sqrt(d2);
				float3 dir = (arrowTo - Objects::arrowFrom).Normalize();
				float3 n = (dir | arrowTo).Normalize0() * 0.25f;
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


		display.SetRasterizer(true,true);
		display.SetDepthTest(true,true);

	}

	void	Renderer::RenderTranslucentBeams()
	{
	/*	if (!objectView)
			return;
	*/	

		display.SetRasterizer(true,false);	//some shafts are flipped, but culling makes no sense anyways
		display.SetDepthTest(true,false);
		display.SetBlendMode(Engine::BlendMode::Add);

		foreach (satellites,_s)
		{
			const PSatellite&sat = *_s;
			TVec3<> color;
			if (sat->isMine)
			{
				color= float3(0.5,0.6,1.0);
			}
			else
			{
				color= float3(1.0,0.6,0.5);
			}
			SetBeamColor(float4(color,0.25),
						float4(color,0.25));
			const float bottom = Grid::ToRadius(-Grid::CellStack::HalfRange);
			{
				//display.SetBlendMode(Engine::BlendMode::Add);
				RenderBeam(sat->system.w.xyz,-(Grid::ToRadius(sat->location.height) 
								- bottom) /
								Grid::SingleLayerHeight,
								0.1f
								);
			}
			bool first = true;
			for (const auto&t : sat->myTasks)
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

		bool nexusSet[12] = {false};

		foreach(skyLinks, _s)
		{
			const PSkyLinkStructure&link = _s->first;
			const PWorkingStructure&ws = _s->second;
			if (ws->location.stackID < 12)
				nexusSet[ws->location.stackID] = true;


			{
				bool render = false;
				int h1;
				if (link->connectedToHub)
				{
					render = true;

					beamBottomColor.Update(float4(0.5,0.6,1.0,2.5));
					beamTopColor.Update(float4(0.5,0.6,1.0,2.5));
					h1 = link->connectedToHub->location.height;

				}
				else
				{
					if (ws->IsSkyPort())
					{
						PSkyPort port = To<SkyPort>::Cast(ws);
						if (ws->IsEnabled() && port->queue.IsNotEmpty() && port->queue.first().IsSatellite())
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
					TMatrix4<> sys = ws->system;
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

		}

		switch (RuntimeConfig::gameGoal)
		{
			case RuntimeConfig::GameGoal::EMPConquerNexus:
				for (int i = 0; i < 12; i++)
				{
					if (nexusSet[i])
						continue;
					TVec3<> color;
					if (BaseGrid::IsIsolatedCorner(i))
					{
						color= float3(1.0,0.6,0.5);
					}
					else
						color= float3(0.5,0.6,1.0);
					SetBeamColor(float4(color,0.75),
								float4(color,0.75));
					RenderPoleBeam(i,0.2f);
				}
			break;
			case RuntimeConfig::GameGoal::ConquerNexus:
				for (int i = 0; i < 12; i++)
				{
					if (nexusSet[i])
						continue;
					TVec3<> color;
					if (BaseGrid::IsIsolatedCorner(i))
					{
						color= float3(1.0,0.6,0.5);
					}
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
		_RenderShadow(*s.localizableShape);	
		if (s.HasFoundation())
			_RenderShadow(foundation);
	}


	static void _RenderShadow(TMatrix4<> system, float h, const Component::Base&comp, float x, float y)
	{
		system.w.xyz += system.z.xyz * h;
		TMatrix4<> sub;
		if (comp.parts.Count() > 0)
		{
			comp.GetSubSystem(0,x,sub);
			Mat::transformSystem(system,sub);
			system = sub;
			display.SetObjectMatrix(system);
			_RenderShadow(comp.parts[0].second);
		}
		if (comp.parts.Count() > 1)
		{
			comp.GetSubSystem(1,y,sub);
			Mat::transformSystem(system,sub);
			system = sub;
			display.SetObjectMatrix(system);
			_RenderShadow(comp.parts[1].second);
		}

	}
		
	static void _RenderShadowS(TMatrix4<> system, float h, float scale, const Component::Base&comp, float x, float y)
	{
		system.w.xyz += system.z.xyz * h;
		TMatrix4<> sub;
		system.x.xyz *= scale;
		system.y.xyz *= scale;
		system.z.xyz *= scale;

		if (comp.parts.Count() > 0)
		{
			comp.GetSubSystem(0,x,sub);
			Mat::transformSystem(system,sub);
			system = sub;
			display.SetObjectMatrix(system);
			_RenderShadow(comp.parts[0].second);
		}
		if (comp.parts.Count() > 1)
		{
			comp.GetSubSystem(1,y,sub);
			Mat::transformSystem(system,sub);
			system = sub;
			display.SetObjectMatrix(system);
			_RenderShadow(comp.parts[1].second);
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


	void					Renderer::EndShadowRendering()
	{
		display.TargetBackbuffer();
	}

		

	void					Renderer::RenderShadows(
		#ifdef CASCADING_SHADOW
			DX11::FBO&shadowMap, Engine::OrthographicAspect<>&shadowCamera, DX11::ObjectBuffer<TMatrix4<> >&shadowMatrix
		#endif
	)
	{
		display.pick(shadowCamera);

			
		display.ClearTarget(shadowMap);
		
		{
			TMatrix4<>	m;
			Mat::mult(shadowCamera.projection,shadowCamera.view,m);
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

				foreach(vehicles, v)
				{
					_RenderShadow(**v);
				}
				foreach (satellites, s)
					_RenderShadow(**s);
				display.SetObjectMatrix(Matrix<>::eye4);

				foreach(structures, _s)
				{
					Structure&s = **_s;
					if (s.changed)
					{
						s.Recompile();
						s.changed = false;
					}
					if (s.compiledShape.ibo.IsSet())
						_RenderShadow(s.compiledShape);

					float zOffset;
					PTurretBase b = s.GetTurretBase(zOffset);
					if (b)
					{
						_RenderShadowS(s.system, zOffset, 2.f,*b->turret, b->turretAngleX, b->turretAngleY);
						display.SetObjectMatrix(Matrix<>::eye4);
					}

				}
				//display.SetObjectMatrix(Matrix<>::eye4);
				//return;

				foreach(structures, _s)
				{
					const Structure&s = **_s;
					if (s.localizableShape)
					{
						_RenderShadow(s);
					}
				}
				display.SetObjectMatrix(Matrix<>::eye4);
				display.UnlockShader();
			#endif /*0*/
		}
	}


	#ifdef CASCADING_SHADOW
	void					Renderer::RenderShadows(index_t cascade)
	{
		//for (index_t i = 0; i < NumCascades; i++)
		RenderShadows(shadowMap[cascade],shadowCamera[cascade],shadowMatrix[cascade]);
	}
	#endif



	Buffer0<PTurretBase>	Renderer::animatedTurrets;
	

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

		Engine::AngularCamera<>	camera;

		void		BeginRendering(const float4&tone, float distanceFactor/*=1.f*/)
		{
			if (fbo.IsEmpty())
			{
				fbo.Create(Resolution(128,128),DXGI_FORMAT_R8G8B8A8_UNORM,true,DX11::FBO::PrepareMips);

				camera.retraction = float3(0,0,-6 * distanceFactor);
				camera.angle.x = 45.f;
				camera.angle.z = 45.f;
				//camera.angle.y = 180.f;	//counters direct3d v-flipping during sampling
				camera.location.z = 0.5f;
				camera.UpdateProjection2(1,0.001f,100000.f,30.f);
				camera.UpdateView();
			}
			else
			{
				fbo.RecreateColorTarget(0,DXGI_FORMAT_R8G8B8A8_UNORM);	//previous one is in use

				camera.retraction = float3(0,0,-6 * distanceFactor);
				camera.UpdateView();
			}
			display.TargetFBO(fbo);
			display.ClearTarget(fbo,float4(tone.rgb*0.5f,tone.a));//, float4(0,1,0,1));

			display.pick(camera);

			display.StoreRendererState();

			display.SetRasterizer(true,true);
			display.SetBlendMode(Engine::BlendMode::None);
			display.SetDepthTest(true,true);
			
			Grid::UpdateSunDirection(float3(1,0,1),false);


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

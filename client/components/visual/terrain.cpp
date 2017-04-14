#ifdef HAS_WINDOW
#include <global_root.h>
#include "gridRenderer.h"
#include <objects/rendering.h>
#include <grid/planet.h> 
#include <grid/worldData.h> 
#include <grid/resources.h>
#include "cameraControl.h"

#include "../ExtRenderer/DirectX11.h"
#include <engine/display.h>
#include <math/vclasses.h>
	
extern Engine::Display<Engine::DirectX11>	display;
//namespace Camera
//{
//	extern Engine::VectorCamera<>	scene;
//	void  ChangeGroundRadius(float);
//}






namespace VisualTerrain
{
	using namespace Engine;
	using namespace Grid;

	Box<>		unitSphereFamilyBoundingBox[BaseGrid::NumFamilies];
	Box<>		planetFamilyBoundingBox[BaseGrid::NumFamilies];



	void		BakeBlockInstruction(const Array<CellStack>&stacks, BYTE family, FClipHeight clipHeight);


	
	namespace Instruction
	{	
		enum displayMode_t
		{
			NormalRendering,
			ParaboloidColored,
			SphereColored
		};

		extern displayMode_t				displayMode;
		extern DX11::ObjectBuffer<float4>	ParaboloidDirection_Height,SphereDef,SphereColor;
		extern DX11::ObjectBuffer<float4>	ParaboloidColor_FlatRadius;
		extern DX11::ObjectBuffer<VecN<float,8> > atmosphereConfig;
		extern DX11::ObjectBuffer<float4> sunDirection,sunColor;


		struct THeightMapOrientation
		{
			TMatrix4<>	orientation0,
						orientation1,
						orientation2;
		};

		static const size_t sTHeightMapOrientation = sizeof(THeightMapOrientation);
		static_assert(sTHeightMapOrientation==sizeof(float)*16*3,"Sruct alignment out of wack");
		extern DX11::ObjectBuffer<THeightMapOrientation>	heightMapOrientation;	//used for lava
		extern DX11::Texture								cubeHeightMap;	//used for lava

			
		extern DX11::BaseInstructionBlock	terrainBase, terrainSphereBase, terrainParaboloidBase, 
											terrainShadowBase, 
											waterBase, waterSphereBase, waterParaboloidBase,
											overlayBase, overlayShadowBase, resourceBase;// , terrainEdgeBase;
	}



	namespace Instruction
	{	
		float4	lastColor;
		displayMode_t				displayMode = NormalRendering;
		DX11::BaseInstructionBlock	terrainBase, terrainSphereBase, terrainParaboloidBase,
									orbitGridBase,
									terrainShadowBase, 
									waterBase, waterSphereBase, waterParaboloidBase,
									overlayBase, 
									overlayShadowBase, resourceBase;// , terrainEdgeBase;

		DX11::ObjectBuffer<float4>	ParaboloidDirection_Height,SphereDef,SphereColor;
		DX11::ObjectBuffer<float4>	ParaboloidColor_FlatRadius;

		DX11::ObjectBuffer<VecN<float,8> > atmosphereConfig;
		DX11::ObjectBuffer<float4> sunDirection,sunColor;

		DX11::ObjectBuffer<THeightMapOrientation>	heightMapOrientation;	//used for lava
		DX11::Texture								cubeHeightMap;	//used for lava

		DX11::Texture				lineTex;
		DX11::Shader				gridShader,transparentGridShader;// , blockShader;
		DX11::InstructionBlock32	lowGridInstruction,
									transparentGridInstruction;

	}

	void	UpdateAtmosphere(const float4&fogColor)
	{
		UpdateAtmosphere(VerticalBase * 1.5f, fogColor);

	}

	void	UpdateAtmosphere(float radius, const float4&fogColor)
	{
		VecN<float,8>	atmosphereCfg;
		atmosphereCfg[0] = sqr(radius);
		atmosphereCfg[1] = radius;
		atmosphereCfg[2] = 0;
		atmosphereCfg[3] = 0;
		atmosphereCfg[4] = fogColor.r;
		atmosphereCfg[5] = fogColor.g;
		atmosphereCfg[6] = fogColor.b;
		atmosphereCfg[7] = fogColor.a;

		Instruction::atmosphereConfig.Update(atmosphereCfg);
	}

	void	UpdatePlanetRenderingParameters(float radius)	//more to come
	{
		TMatrix4<> matrix = Matrix<>::eye4;
		matrix.x.xyz *= radius;
		matrix.y.xyz *= radius;
		matrix.z.xyz *= radius;

		display.SetObjectMatrix(matrix);
	}

	namespace SunDetails
	{
		float4	plane_Speed = float4(0,0,1,0.5f);
		float4	color = float4(1);
		TMatrix3<>	sunSystem = Matrix<>::eye3;
		float3	sunDirection(1,0,0);
		double progression = 0.0;
	}

	const float3&	GetSunDirection()
	{
		return SunDetails::sunDirection;
	}

	void	UpdateSun(const float4&sunPlane_Speed,const float4&sunColor)
	{
		SunDetails::plane_Speed = sunPlane_Speed;
		SunDetails::color = sunColor;
		SunDetails::progression = 0.f;
		Mat::makeAxisSystem(sunPlane_Speed.xyz,2,SunDetails::sunSystem);

		Instruction::sunColor.Update(sunColor);
		UpdateSunDirection(SunDetails::sunSystem.x,true);
	}


	void	PerFrameUpdateSunDirection()
	{
		SunDetails::progression += SunDetails::plane_Speed.w * Engine::timing.delta;


		float3 p = float3(cos(SunDetails::progression),sin(SunDetails::progression),0);
		Mat::Mult(SunDetails::sunSystem,p,SunDetails::sunDirection);
		UpdateSunDirection(SunDetails::sunDirection,true);
	}

	double	GetAcceleratedSunDirection(TVec3<>&outDir, float speedFactor/*=1.f*/)
	{
		double angle = SunDetails::progression*speedFactor;
		Vec::def(outDir,cos(angle),sin(angle),0);
		Mat::Mult(SunDetails::sunSystem,outDir);
		return angle;
	}


	const TMatrix3<>& GetSunSystem()
	{
		return SunDetails::sunSystem;
	}


	void	ResetSunRotation()
	{
		SunDetails::progression = 0;
	}



	void	SetOrientation(TMatrix4<>&m, const float3&axis, double t, const float3&seedAxis, double seedRot)
	{
		TMatrix3<> sys1,sys2,sys;
		Mat::BuildRotationMatrix(t*1.5,axis,sys1);
		Mat::BuildRotationMatrix(seedRot,seedAxis,sys2);
		Mat::Mult(sys1,sys2,sys);
		Mat::copyOrientation(sys,m);
		m.w.xyz = Vector<>::zero;
		Mat::resetBottomRow(m);

	}

	void	UpdateSunDirection(const float3&p, bool shadows /*= true*/)
	{
		SunDetails::sunDirection = p;
		//#ifdef _DEBUG
		//	SunDetails::sunDirection = -p;
		//#endif
		Instruction::sunDirection.Update(float4(SunDetails::sunDirection,shadows));

		Random random(23984);
		Instruction::THeightMapOrientation	orientation;
		SetOrientation(orientation.orientation0,float3(1,0.75,0.75).Normalized0(),Engine::timing.now, float3(random.NextFloat(-1,1),random.NextFloat(-1,1),random.NextFloat(-1,1)).Normalized0(),random.NextFloatT<double>(0,100));
		SetOrientation(orientation.orientation1,float3(0.75,1,0.75).Normalized0(),-Engine::timing.now*1.35, float3(random.NextFloat(-1,1),random.NextFloat(-1,1),random.NextFloat(-1,1)).Normalized0(),random.NextFloatT<double>(0,100));
		SetOrientation(orientation.orientation2,float3(0.75,0.75,1).Normalized0(),Engine::timing.now/M_PI, float3(random.NextFloat(-1,1),random.NextFloat(-1,1),random.NextFloat(-1,1)).Normalized0(),random.NextFloatT<double>(0,100));

		Instruction::heightMapOrientation.Update(orientation);

	}


	
	static BYTE GetHeightMapHeight(const SimplexNoise&n, TVec3<>&p)
	{
		Vec::normalize(p);
		Vec::mult(p,2.f);
		float v = 0.5f * (Get(n,p) + Get(n,p*2.f));	//pseudo normal distributed -1..+1

		float x = //fabs(perlin);
				SimplexNoise::ToUniformDistribution(v);
		//float x = p.z * 0.5f + 0.5f;
		//;
		return (BYTE)(std::min(x ,1.f)* 255.f);
	}

	static void CreateHeightMapSide(Image&result, const SimplexNoise&n, const float3&origin, const float3&dx, const float3&dy)
	{
		result.SetSize(128,128,1);
		Concurrency::parallel_for(index_t(0),count_t(result.GetWidth()*result.GetHeight()),[&result,&n,origin,dx,dy](index_t i)
		{
			TVec3<> p;
			index_t x = i/result.GetHeight();
			index_t y = i%result.GetHeight();
			{
				float fx = float(x)/float(result.GetWidth()-1) * 2.f - 1.f;
				float fy = float(y)/float(result.GetHeight()-1) * 2.f - 1.f;
				
				Vec::mad(origin,dx,fx,p);
				Vec::mad(p,dy,fy);
				result.SetChannel((UINT32)x,(UINT32)y,0,GetHeightMapHeight(n,p));
			}
		});
	}


	void Create()
	{
		Buffer0<DX11::Shader::Binding> bindings;


		//bindings;
		
		bindings.Append(DX11::Shader::Binding(Instruction::sunDirection,"Sun"));
		bindings.Append(DX11::Shader::Binding(Instruction::sunColor,"SunColor"));
		#ifdef CASCADING_SHADOW
			for (index_t i = 0; i < Objects::NumCascades; i++)
			{
				ASSERT__(Objects::shadowMatrix[i].IsSet());
				bindings.Append(DX11::Shader::Binding(Objects::shadowMatrix[i],"ShadowMatrix"+String(i)));
				bindings.Append(DX11::Shader::Binding(Objects::shadowMap[i].GetDepthStencilTarget(), "ShadowMap"+String(i)));
			}
		#else
			bindings.Append(DX11::Shader::Binding(Objects::shadowMatrix,"ShadowMatrix"));
			bindings.Append(DX11::Shader::Binding(Objects::shadowMap.GetDepthStencilTarget(), "ShadowMap"));
		#endif
		bindings.Append(DX11::Shader::Binding(Instruction::atmosphereConfig, "AtmosphereConfig"));
		bindings.Append(DX11::Shader::SamplerBinding(true, Engine::TextureFilter::None, 1, "ShadowSampler"));

		bindings.Append(DX11::Shader::SamplerBinding(true,Engine::TextureFilter::Trilinear,1,"HeightMapSampler"));
		bindings.Append(DX11::Shader::Binding(Instruction::cubeHeightMap,"HeightMap"));
		bindings.Append(DX11::Shader::Binding(Instruction::heightMapOrientation,"HeightMapOrientation"));
		
		Instruction::terrainBase.shader.LoadFromFile("shaders/gridTerrain.hlsl", "vsMain", "psMain", bindings.pointer(), bindings.Count());

		

		{
			//Engine::VertexBinding binding;
			//binding.vertex.set(0, 4);
			//binding.normal.set(4, 4);
			//binding.color.set(8, 4);
			//binding.floats_per_vertex = 12;
			Instruction::terrainBase.binding.Append("POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT);
			Instruction::terrainBase.binding.Append("NORMAL",0,DXGI_FORMAT_R8G8B8A8_SNORM);
			Instruction::terrainBase.binding.Append("COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT);
			Instruction::terrainBase.SetTriangles();

			Instruction::terrainSphereBase.binding = Instruction::terrainBase.binding;
			Instruction::terrainSphereBase.SetTriangles();

			Instruction::terrainParaboloidBase.binding = Instruction::terrainBase.binding;
			Instruction::terrainParaboloidBase.SetTriangles();

			Instruction::terrainBase.UpdateLayout();
		}
		Instruction::terrainShadowBase.shader.LoadFromFile("shaders/gridTerrainShadow.hlsl", "vsMain", "psMain");
		Instruction::terrainShadowBase.binding = Instruction::terrainBase.binding;
		Instruction::terrainShadowBase.UpdateLayout();
		Instruction::terrainShadowBase.SetTriangles();


		{
			Engine::VertexBinding binding;
			binding.vertex.set(0, 4);
			binding.normal.set(4, 4);
			binding.color.set(8,1);
			binding.floats_per_vertex = 9;

			Instruction::orbitGridBase.binding = binding;
			Instruction::orbitGridBase.SetTriangles();
			Instruction::orbitGridBase.shader.LoadFromFile("shaders/orbitalGrid.hlsl", "vsMain", "psMain");
			Instruction::orbitGridBase.UpdateLayout();
		}
	
		{
			DX11::BaseInstructionBlock&base = Instruction::waterBase;
			base.shader.LoadFromFile("shaders/gridTerrainWater.hlsl", "vsMain", "psMain", bindings.pointer(), bindings.Count());

			{
				/*
			float4		vertex;
			float3		normal;
			float		light;	//water derives color from shader variable
				*/
				Engine::VertexBinding binding;
				binding.vertex.set(0, 4);
				binding.normal.set(4, 3);
				binding.color.set(7, 1);
				binding.floats_per_vertex = 8;
				base.binding = binding;
				base.UpdateLayout();
				base.SetTriangles();
			}


			Instruction::waterSphereBase.binding = Instruction::waterBase.binding;
			Instruction::waterSphereBase.SetTriangles();

			Instruction::waterParaboloidBase.binding = Instruction::waterBase.binding;
			Instruction::waterParaboloidBase.SetTriangles();

		}
		{
			Engine::VertexBinding binding;
			binding.vertex.set(0, 3);
			binding.normal.unset();
			binding.color.set(3, 1);
			binding.floats_per_vertex = 4;
			{
				DX11::BaseInstructionBlock&base = Instruction::overlayBase;
				base.shader.LoadFromFile("shaders/gridTerrainOverlay.hlsl", "vsMain", "psMain"/*, bindings, ARRAYSIZE(bindings)*/);
				base.binding = binding;
				base.UpdateLayout();
				base.SetTriangles();
			}
			{
				DX11::BaseInstructionBlock&base = Instruction::overlayShadowBase;
				base.shader.LoadFromFile("shaders/gridTerrainOverlayShadow.hlsl", "vsMain", "psMain"/*, bindings, ARRAYSIZE(bindings)*/);
				base.binding = binding;
				base.UpdateLayout();
				base.SetTriangles();
			}
		}

		{
			DX11::BaseInstructionBlock&base = Instruction::resourceBase;
			base.shader.LoadFromFile("shaders/gridResourceVisualization.hlsl", "vsMain", "psMain"/*, bindings, ARRAYSIZE(bindings)*/);
			{
				Engine::VertexBinding binding;
				binding.vertex.set(0, 4);
				binding.normal.set(4, 4);
				binding.color.set(8, 4);
				binding.floats_per_vertex = 12;
				base.binding = binding;
				base.UpdateLayout();
				base.SetTriangles();
			}
		}


		Instruction::ParaboloidDirection_Height.Create(float4());
		Instruction::ParaboloidColor_FlatRadius.Create(float4());
		Instruction::SphereDef.Create(float4());
		Instruction::SphereColor.Create(float4());

		DX11::Shader::RedefineMacro(DX11::Shader::Macro("SPHERE_DISPLAY","1"));
		bindings.Append(DX11::Shader::Binding(Instruction::SphereDef,"SphereDef"));
		bindings.Append(DX11::Shader::Binding(Instruction::SphereColor,"SphereColor"));
		Instruction::terrainSphereBase.shader.LoadFromFile("shaders/gridTerrain.hlsl", "vsMain", "psMain", bindings.pointer(), bindings.Count());
		DX11::Shader::macroDefinitions.Clear();
		Instruction::terrainSphereBase.UpdateLayout();
		bindings.EraseLast();
		bindings.EraseLast();


		DX11::Shader::RedefineMacro(DX11::Shader::Macro("PARABOLOID_DISPLAY","1"));
		bindings.Append(DX11::Shader::Binding(Instruction::ParaboloidDirection_Height,"ParaboloidDirection_Height"));
		bindings.Append(DX11::Shader::Binding(Instruction::ParaboloidColor_FlatRadius,"ParaboloidColor_FlatRadius"));
		Instruction::terrainParaboloidBase.shader.LoadFromFile("shaders/gridTerrain.hlsl", "vsMain", "psMain", bindings.pointer(), bindings.Count());
		DX11::Shader::macroDefinitions.Clear();
		Instruction::terrainParaboloidBase.UpdateLayout();
		bindings.EraseLast();
		bindings.EraseLast();



		DX11::Shader::RedefineMacro(DX11::Shader::Macro("SPHERE_DISPLAY","1"));
		bindings.Append(DX11::Shader::Binding(Instruction::SphereDef,"SphereDef"));
		bindings.Append(DX11::Shader::Binding(Instruction::SphereColor,"SphereColor"));
		Instruction::waterSphereBase.shader.LoadFromFile("shaders/gridTerrainWater.hlsl", "vsMain", "psMain", bindings.pointer(), bindings.Count());
		DX11::Shader::macroDefinitions.Clear();
		Instruction::waterSphereBase.UpdateLayout();
		bindings.EraseLast();
		bindings.EraseLast();


		DX11::Shader::RedefineMacro(DX11::Shader::Macro("PARABOLOID_DISPLAY","1"));
		bindings.Append(DX11::Shader::Binding(Instruction::ParaboloidDirection_Height,"ParaboloidDirection_Height"));
		bindings.Append(DX11::Shader::Binding(Instruction::ParaboloidColor_FlatRadius,"ParaboloidColor_FlatRadius"));
		Instruction::waterParaboloidBase.shader.LoadFromFile("shaders/gridTerrainWater.hlsl", "vsMain", "psMain", bindings.pointer(), bindings.Count());
		DX11::Shader::macroDefinitions.Clear();
		Instruction::waterParaboloidBase.UpdateLayout();
		bindings.EraseLast();
		bindings.EraseLast();










		//----------------------


		Instruction::heightMapOrientation.Create(Instruction::THeightMapOrientation());

		{
			SimplexNoise n(Random().NextSeed());
			Image sides[6];
			//+x, -x, +y, -y, +z, -z.
			CreateHeightMapSide(sides[0],n,float3(1,0,0),float3(0,0,-1),float3(0,1,0));
			CreateHeightMapSide(sides[1],n,float3(-1,0,0),float3(0,0,1),float3(0,1,0));
			CreateHeightMapSide(sides[2],n,float3(0,1,0),float3(1,0,0),float3(0,0,-1));
			CreateHeightMapSide(sides[3],n,float3(0,-1,0),float3(1,0,0),float3(0,0,1));
			CreateHeightMapSide(sides[4],n,float3(0,0,1),float3(1,0,0),float3(0,1,0));
			CreateHeightMapSide(sides[5],n,float3(0,0,-1),float3(-1,0,0),float3(0,1,0));


			Instruction::cubeHeightMap.LoadCube(sides,false);
		}

		Instruction::atmosphereConfig.Create(VecN<float,8>());
		UpdateAtmosphere(currentPlanetConfig.atmosphereColor);

		Instruction::sunDirection.Create(float4());
		Instruction::sunColor.Create(float4());
		UpdateSun(currentPlanetConfig.sunPlaneNormal_Speed,currentPlanetConfig.sunColor);



		Image img(64, 1, 4);
		for (UINT32 x = 0; x < img.GetWidth(); x++)
		{
			float a = smoothstep(0.9, 0.95, float(x) / float(img.GetWidth() - 1));
			float b = smoothstep(0.925, 0.95, float(x) / float(img.GetWidth() - 1));
			BYTE*pixel = img.get(x, 0);
			pixel[0] = (BYTE)(180 * b);
			pixel[1] = (BYTE)(200 * b);
			pixel[2] = (BYTE)(255 * b);
			pixel[3] = (BYTE)(a * 255.f);
		}


		Instruction::lineTex.Load(img, true);
		//blockConfig.Create(TMatrix4<>());

		{
			DX11::Shader::Binding bindings[] =
			{
				DX11::Shader::Binding(Instruction::lineTex, "LineTex"),
				DX11::Shader::SamplerBinding(true, Engine::TextureFilter::Trilinear, 1, "LineSampler")

			};
			Instruction::gridShader.LoadFromFile("shaders/sphereGrid.hlsl", "vsMain", "psMain", bindings, ARRAYSIZE(bindings));
			Instruction::transparentGridShader.LoadFromFile("shaders/transparentSphereGrid.hlsl", "vsMain", "psMain");
		}


		for (BYTE i = 0; i < BaseGrid::NumFamilies; i++)
			BakeBlockInstruction(World::actual.cellStack, i, Clip::NoClip);





		//------





		for (index_t i = 0; i < ARRAYSIZE(unitSphereFamilyBoundingBox); i++)
			unitSphereFamilyBoundingBox[i] = planetFamilyBoundingBox[i] =  Box<>::Invalid();

		float r0 = ToRadius(-CellStack::HalfRange);
		float r1 = ToRadius(CellStack::HalfRange);
		foreach (Grid::high.cells,c)
		{
			Box<>&box = planetFamilyBoundingBox[Grid::high.grid.GetCell(c - Grid::high.cells.pointer()).family];
			box.Include(c->centerVertex.xyz * r0);
			box.Include(c->centerVertex.xyz * r1);
			for (int i = 0; i < c->numCorners; i++)
			{
				box.Include(c->cornerVertex[i].xyz*r0);
				box.Include(c->cornerVertex[i].xyz*r1);
			}
		}
		foreach (Grid::high.cells,c)
		{
			Box<>&box = unitSphereFamilyBoundingBox[Grid::high.grid.GetCell(c - Grid::high.cells.pointer()).family];
			box.Include(c->centerVertex.xyz);
			for (int i = 0; i < c->numCorners; i++)
			{
				box.Include(c->cornerVertex[i].xyz);
			}
		}



		Array<float4> verticesToLoad(Grid::low.cellVertices.Count());
		for (index_t i = 0; i < Grid::low.cellVertices.Count(); i++)
			verticesToLoad[i] = float4(Grid::low.cellVertices[i].centerDirection,float(Grid::low.cellVertices[i].ownerCount > 1));
		Instruction::lowGridInstruction.vbo.CreateFromStructs(verticesToLoad);
		{
			Buffer<UINT32>	ibo;
			foreach(Grid::low.cells, cell)
			{
				cell->voffset = (UINT32)ibo.Count();
				for (int j = 0; j < cell->numCorners; j++)
					ibo << cell->centerVertexId << cell->cornerVertexId[j] << cell->cornerVertexId[(j + 1) % cell->numCorners];
			}
			Instruction::lowGridInstruction.ibo.Create(ibo);
		}

		Instruction::lowGridInstruction.shader = Instruction::gridShader;
		{
			Engine::VertexBinding	binding;
			binding.vertex.set(0, 4);
			binding.normal.unset();
			binding.floats_per_vertex = 4;
			Instruction::lowGridInstruction.binding = binding;
		}
		Instruction::lowGridInstruction.UpdateLayout();
		Instruction::lowGridInstruction.SetTriangles();


		Instruction::transparentGridInstruction = Instruction::lowGridInstruction;
		Instruction::transparentGridInstruction.shader = Instruction::transparentGridShader;
		Instruction::transparentGridInstruction.UpdateLayout();
	}



	struct TWeightedColor;



	struct TBaseColor
	{
		float3	color;

		/**/	TBaseColor() = default;
		/**/	TBaseColor(const float3&c):color(c)	{}
		/**/	TBaseColor(const TWeightedColor&c);

		void	operator+=(const TBaseColor&c)
		{
			color += c.color;
		}
		void	operator*=(float fc)
		{
			color *= fc;
		}
	};

	struct TWeightedColor : TBaseColor
	{
		typedef TBaseColor	Super;

		float	weight=0.f;

		/**/	TWeightedColor() = default;
		/**/	TWeightedColor(const TBaseColor&c, float weight):Super(c),weight(weight)	{}

		void	operator+=(const TWeightedColor&c)
		{
			Super::operator+=(c);
			weight += c.weight;
		}
		void	operator*=(float fc)
		{
			Super::operator*=(fc);
			weight *= fc;
		}
	};


	
	struct TLitColor : public TBaseColor
	{
		typedef TBaseColor	Super;

		float	lightLevel=0.f;
		bool	capped = false;
		float	emp = 0.f;
		float	visibility=0.f;

		/**/	TLitColor() = default;
		/**/	TLitColor(const TBaseColor&base, float lightLevel, float emp=0.f):Super(base),lightLevel(lightLevel),emp(emp)	{}
		/**/	TLitColor(const TBaseColor&base, float lightLevel, float emp, const TVisibility&vis):Super(base),lightLevel(lightLevel),emp(emp),visibility(vis.value)	{}
		/**/	TLitColor(const TWeightedColor&base, float lightLevel, float emp=0.f):Super(base),lightLevel(lightLevel),emp(emp)	{}
		/**/	TLitColor(float lightLevel, float emp=0.f):lightLevel(lightLevel),emp(emp)	{}
		/**/	TLitColor(const TVisibility&v):visibility(v.value)	{}
		void	CompileFinalColor(TVec4<>&)	const;

	private:
		static void ApplyEMP(float intensity,TVec3<>&rs);
		void	CapColor(TVec3<>&rs)	const;
	};


		struct TerrainVertex
	{
		float4		vertex;
		sbyte4		normal_structure;
		float4		color;

		/**/		TerrainVertex()	{};
		/**/		TerrainVertex(const float4&vtx, float h, const float3&n, const TLitColor&color, float structure) : vertex(vtx.xyz*h, vtx.w), normal_structure(n*127.f,structure*127.f) {color.CompileFinalColor(this->color);}
		/**/		TerrainVertex(const float4&vtx, float h, const float3&n, const float3&color, float structure) : vertex(vtx.xyz*h, vtx.w), normal_structure(n*127.f, structure*127.f), color(color,1.f){}
	};
	
	struct ResourceVertex //! Temporarily the same layout as TerrainVertex
	{
		float4		vertex;
		float4		normal;
		float4		color;

		/**/		ResourceVertex()	{};
		/**/		ResourceVertex(const float4&vtx, float h, const float3&n, const float4&color, float structure) : vertex(vtx.xyz*h, vtx.w), normal(n,structure), color(color){}
		/**/		ResourceVertex(const float4&vtx, float h, const float3&n, const float3&color, float structure) : vertex(vtx.xyz*h, vtx.w), normal(n, structure), color(color,1.f){}
		/**/		ResourceVertex(const float3&vtx, float h, const float3&n, const float4&color, float structure) : vertex(vtx*h, 1.f), normal(n,structure), color(color){}
		/**/		ResourceVertex(const float3&vtx, float h, const float3&n, const float3&color, float structure) : vertex(vtx*h, 1.f), normal(n, structure), color(color,1.f){}
	};

	struct OverlayVertex
	{
		float3		vertex;
		float		opacity;	//overlay is always black

		/**/		OverlayVertex()	{};
		/**/		OverlayVertex(const float4&vtx, float h, float opacity) : vertex(vtx.xyz*h), opacity(opacity){}
	};

	struct WaterVertex
	{
		float4		vertex;
		float3		normal;
		float		light;	//water derives color from shader variable

		/**/		WaterVertex()	{};
		/**/		WaterVertex(const float4&vtx, float h, const float3&n,float light, int emp) : vertex(vtx.xyz*h, vtx.w), normal(n),light((float(emp) + 0.05f + light*0.9f) * 0.25f ){}
	};
	

	template <typename Vtx>
		struct THull
		{
			Buffer<Vtx, 0>	vertices;
			Buffer<UINT32, 0>	indices;


			void			Clear()
			{
				vertices.Clear();
				indices.Clear();
			}

			void		AppendTo(THull<Vtx>&hull) const
			{
				UINT32 offset = (UINT32)hull.vertices.Count();
				hull.vertices.Append(vertices);
				const UINT32*iFrom = indices.pointer();
				UINT32*iTo = hull.indices.AppendRow(this->indices.Count());
				while (iFrom != this->indices.end())
				{
					(*iTo) = (*iFrom) + offset;
					iFrom++;
					iTo++;
				}
				DBG_ASSERT__(iTo == hull.indices.end());
			}
		};

	struct TTerrainHull : public THull<TerrainVertex>
	{
		DX11::InstructionBlock32	instruction[3],shadowInstruction;

		void						Destroy()
		{
			THull<TerrainVertex>::Clear();
			instruction[0] = instruction[1] = instruction[2]
				= shadowInstruction = DX11::InstructionBlock32();
		}
	};
	struct TResourceHull : public THull<ResourceVertex>
	{
		DX11::InstructionBlock32	instruction,shadowInstruction;	// might cast shadow some day

		void						Destroy()
		{
			THull<ResourceVertex>::Clear();
			shadowInstruction = instruction = DX11::InstructionBlock32();
		}
	};
	struct TWaterHull : public THull<WaterVertex>
	{
		DX11::InstructionBlock32	instruction[3];

		void						Destroy()
		{
			THull<WaterVertex>::Clear();
			instruction[0] = instruction[1] = instruction[2]
				= DX11::InstructionBlock32();
		}
	};

	struct TOverlayHull : public THull<OverlayVertex>
	{
		DX11::InstructionBlock32	instruction,shadowInstruction;

		void						Destroy()
		{
			THull<OverlayVertex>::Clear();
			shadowInstruction = instruction = DX11::InstructionBlock32();
		}
	};
	/**
	Stack-stored mesh container with constant capacity
	*/
	template <typename Vtx, count_t maxVertices, count_t maxIndices>
		class StaticMesh
		{
		public:
			static const count_t MaxVertices = maxVertices;
			static const count_t MaxIndices = maxIndices;

			BYTE		vertexData[maxVertices * sizeof(Vtx)];	//prevent constructor execution
			Vtx			*vertexRange,
						*vat;
			UINT16		indexRange[maxIndices],
						*iat,
						offset;

			/**/		StaticMesh() :vertexRange((Vtx*)vertexData),vat((Vtx*)vertexData), iat(indexRange), offset(0)	{}
			/**/		StaticMesh(const StaticMesh<Vtx,maxVertices,maxIndices>&other)	{Reset();}	//nothing for now

			void		operator=(const StaticMesh<Vtx,maxVertices,maxIndices>&other)	{Reset();}	//nothing for now


			void		Reset()
			{
				vertexRange = ((Vtx*)vertexData);
				vat = ((Vtx*)vertexData);
				iat = (indexRange);
				offset = (0);
			}

			bool		IsEmpty() const {return indexRange == iat;}
			bool		IsNotEmpty() const {return indexRange != iat;}



			void		SetOffset()
						{
							offset = (UINT16)(vat - vertexRange);
						}

			StaticMesh&	Quad(int i0, int i1, int i2, int i3)
						{
							DBG_ASSERT__(iat - indexRange+6 <= maxIndices);
							(*iat) = UINT16(i0) + offset;iat++;
							(*iat) = UINT16(i1) + offset;iat++;
							(*iat) = UINT16(i2) + offset;iat++;
							(*iat) = UINT16(i0) + offset;iat++;
							(*iat) = UINT16(i2) + offset;iat++;
							(*iat) = UINT16(i3) + offset;iat++;
							return *this;
						}

			StaticMesh&	InvQuad(int i0, int i1, int i2, int i3)
						{
							DBG_ASSERT__(iat - indexRange+6 <= maxIndices);
							(*iat) = UINT16(i0) + offset;iat++;
							(*iat) = UINT16(i2) + offset;iat++;
							(*iat) = UINT16(i1) + offset;iat++;
							(*iat) = UINT16(i0) + offset;iat++;
							(*iat) = UINT16(i3) + offset;iat++;
							(*iat) = UINT16(i2) + offset;iat++;
							return *this;
						}
			StaticMesh&	operator<<(UINT16 index)
						{
							DBG_ASSERT__(iat - indexRange < maxIndices);
							(*iat) = index + offset;
							iat++;
							return *this;
						}
			StaticMesh&	operator<<(const Vtx&vtx)
						{
							DBG_ASSERT_EQUAL__((void*)vertexRange ,(void*)vertexData);
							DBG_ASSERT__(vat - vertexRange < maxVertices);
							(*vat) = vtx;
							vat++;
							return *this;
						}

			void		AppendTo(THull<Vtx>&hull) const
						{
							UINT32 offset = (UINT32)hull.vertices.Count();
							hull.vertices.Append(vertexRange, vat - vertexRange);
							const UINT16*iFrom = indexRange;
							UINT32*iTo = hull.indices.AppendRow(iat - indexRange);
							while (iFrom != iat)
							{
								(*iTo) = UINT32(*iFrom) + offset;
								//DBG_ASSERT__(*iTo < _vertices.Count());
								iFrom++;
								iTo++;
							}
							DBG_ASSERT__(iTo == hull.indices.end());
						}
		};


	template <typename Vtx>
		class DynamicMesh
		{
		public:
			Buffer<Vtx>	vertices;
			Buffer<UINT32>	indices;
			//std::vector<Vtx>	vertices;
			//std::vector<UINT32>	indices;
			UINT32		offset;

			/**/		DynamicMesh() :offset(0)	{}

			void		Clear()
			{
				vertices.clear();
				indices.clear();
				offset = 0;
			}

			bool		IsEmpty() const {return indices.IsEmpty();}
			bool		IsNotEmpty() const {return indices.IsNotEmpty();}



			void		SetOffset()
						{
							offset = (UINT32)vertices.size();
						}

			DynamicMesh&	Quad(int i0, int i1, int i2, int i3)
						{
							//indices.push_back(i0);
							//indices.push_back(i1);
							//indices.push_back(i2);
							//indices.push_back(i0);
							//indices.push_back(i2);
							//indices.push_back(i3);
							UINT32*iat = indices.AppendRow(6);
							(*iat) = UINT32(i0) + offset;iat++;
							(*iat) = UINT32(i1) + offset;iat++;
							(*iat) = UINT32(i2) + offset;iat++;
							(*iat) = UINT32(i0) + offset;iat++;
							(*iat) = UINT32(i2) + offset;iat++;
							(*iat) = UINT32(i3) + offset;iat++;
							return *this;
						}

			DynamicMesh&	operator<<(UINT32 index)
						{
							//indices << index;
							indices.push_back(offset + index);
							return *this;
						}
			DynamicMesh&	operator<<(const Vtx&vtx)
						{
							//vertices << vtx;
							vertices.push_back(vtx);
							return *this;
						}

			void		AppendTo(THull<Vtx>&hull) const
						{
							UINT32 offset = (UINT32)hull.vertices.Count();
							//foreach (vertices,v)
								//hull.vertices.push_back(*v);
							hull.vertices.Append(vertices);
							//const UINT32*iFrom = indices.pointer();
							auto iFrom = indices.begin();
							UINT32*iTo = hull.indices.AppendRow(indices.size());
							while (iFrom != indices.end())
							{
								(*iTo) = UINT32(*iFrom) + offset;
								//DBG_ASSERT__(*iTo < _vertices.Count());
								iFrom++;
								iTo++;
							}
							DBG_ASSERT__(iTo == hull.indices.end());
						}
		};


	/**
	Homogenous geometrical section of the planet surface, to be rendered and updated as one chunk
	*/
	struct TFamily
	{
		struct TData
		{
			#ifdef HAS_WINDOW

				/**
				Various hulls used during rendering.
				*/
				TTerrainHull			terrain;	//!< high-resolution solid terrain, no transparency, shaded
				TOverlayHull			overlay;	//!< high-resolution overhanging terrain overlay, transparent
				TWaterHull				water;		//!< high-resolution overhanging terrain water, transparent, shaded
				TResourceHull			resources;	//!< low-resolution resource visualization. Similar to @a overlay, but uses different shader
			#endif 
			Box<>						boundingBox;
			Sphere<>					boundingSphere;
			bool						outdated,resourceOutdated;

			/**/						TData() :outdated(true),resourceOutdated(true)
			{}

			void						Destroy()
			{
				#ifdef HAS_WINDOW
					terrain.Destroy();
					overlay.Destroy();
					water.Destroy();
					resources.Destroy();
				#endif
			}
		};

		TData							normal;// , lower;
		float							outdatedAt;

		void							Destroy()
		{
			//	lower.Destroy();
			normal.Destroy();
		}


		void							SetOutdated(float atTime)
		{
			normal.outdated = normal.resourceOutdated = true;
			outdatedAt = atTime;
		}


	};
	TFamily						family[BaseGrid::NumFamilies];


	void	Destroy()
	{
		using namespace Instruction;
		sunDirection.Clear();
		sunColor.Clear();

		lineTex.Clear();
		gridShader.Clear();
		transparentGridShader.Clear();
		overlayBase = overlayShadowBase 
			= resourceBase = terrainBase 
			= terrainSphereBase = terrainParaboloidBase 
			= orbitGridBase
			= terrainShadowBase
			= waterBase = waterSphereBase = waterParaboloidBase
			= DX11::BaseInstructionBlock();
		atmosphereConfig .Clear();
		ParaboloidDirection_Height.Clear();
		ParaboloidColor_FlatRadius.Clear();
		SphereDef.Clear();
		heightMapOrientation.Clear();
		cubeHeightMap.Clear();

		for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
			family[k].Destroy();
		lowGridInstruction.Clear();
		transparentGridInstruction.Clear();
	}






	class TerrainCompiler
	{
	public:
		static const count_t BatchSize = 16;	//the bigger, the less paralism, but the less the overhead during locking (since more work per locking)

		StaticMesh<TerrainVertex,BatchSize * CellStack::Range * (7 + 7 + 4 * 6),BatchSize * CellStack::Range * 3 * (7 * 2 + 2 * 7)>	solidMesh;
		StaticMesh<OverlayVertex,BatchSize * 7,BatchSize * 3*7>	coverMesh;
		

		const CellStack * stacks;
		const BaseGrid::TCell*  gridCells;
		const BaseGrid::TNetCell*  netCells;
		FClipHeight	clipHeight;
		bool	wasEmpty;
		TLitColor	cornerColor[6];


		void	Setup(const CellStack *stacks, const BaseGrid::TCell*cells, const BaseGrid::TNetCell*netCells,FClipHeight clipHeight)
		{
			solidMesh.Reset();
			coverMesh.Reset();
			this->stacks = stacks;
			this->netCells = netCells;
			this->gridCells = cells;
			this->clipHeight = clipHeight;
			wasEmpty = true;
		}


		TWeightedColor	GetMixColorU(index_t stackIndex,  UINT ih, Content oppositeContent, float&brightness, int&sumVisibilityTo, Content&outContent) const;
		TWeightedColor	GetDownMixColorU(index_t stackIndex,  UINT ih, Content oppositeContent, float&brightness, int&sumVisibilityTo, Content&outContent) const;
//			static void ApplyEMP(UINT16 cornerFlags,TVec4<>&rs);
		static bool FlagsAffectedByEMP(UINT16 cornerFlags) {return !(cornerFlags & empZonesUnlocked);}
		static bool AffectedByEMP(index_t stackIndex);


		void	Compile(const index_t offset, const index_t end);
		void	CompileLayer(	const UINT ih, const CellStack&, const int clipH,
								const BaseGrid::TNetCell&cell,const BaseGrid::TCellNeighborhood&neighborhood);
		void	CompileBottomLayer(	const CellStack&stack, const int clipH,
									const BaseGrid::TNetCell&cell,const BaseGrid::TCellNeighborhood&neighborhood);
		void	CompileAboveTerrainOutline(const UINT ih, const int ir,const BaseGrid::TNetCell&cell,const BaseGrid::TCellNeighborhood&neighbors);
		void	CompileCap(	const UINT ih,const BaseGrid::TNetCell&cell, const bool capped, const CellStack::Cell&thisCell, 
							const TFloatRange<>&,const BaseGrid::TCellNeighborhood&, const CellStack&);
		void	CompileDownCap(	const UINT ih,const BaseGrid::TNetCell&cell, const CellStack::Cell&thisCell, 
								const int ir,const BaseGrid::TCellNeighborhood&, const CellStack&);
		void	CompileFaceToNeighbor(const int j, const UINT ih, const int ir,const BaseGrid::TNetCell&cell,const BaseGrid::TCellNeighborhood&neighbors,TLitColor newCornerColor[6], 
										const CellStack::Cell&thisCell, const CellStack::Cell&lastCell, const TFloatRange<>&range);
		void	DeliverResults(THull<TerrainVertex>&solidHull, THull<OverlayVertex>&coverHull);
	};


	class ResourceCompiler
	{
	public:
		static const count_t Resolution = 10;
		static const count_t BatchSize = TerrainCompiler::BatchSize;
		StaticMesh<ResourceVertex,BatchSize * 2 * 2 * (1 + Resolution + Resolution*2 ),BatchSize * 3 * 2* 2*(Resolution + 2*Resolution)>	mesh;
		
		const CellStack * stacks;
		const BaseGrid::TCell*  lowGridCells, *highGridCells;
		const BaseGrid::TNetCell*  lowNetCells;
		const Resource*resources;
		const ResourceDiscovery*visibility;
		count_t			numResources;
		FClipHeight	clipHeight;

		static float3 GetColor(float level)	{return float3(std::min(1.f,level*8.f),std::min(1.f,level*3.f), std::min(1.f,level)) * 0.7f + 0.3f;}
		static float3 GetColor(float level, const float3&flavor)
		{
			return flavor;//*0.75f + 0.5f*(std::min(1.f,level));
			//return flavor*0.5f + (1.2f * std::min(1.f,level) + 0.3f);
			// & (GetColor(level)*0.5f + 0.5f);
			//return flavor * 0.75 + GetColor(level) * 0.25f;
			//return 0.3f + 0.7f * std::min(1.f,level*8.f);
			//return float3(std::min(1.f,level*8.f),std::min(1.f,level*3.f), std::min(1.f,level)) * 0.7f + 0.3f;
		}
		static float GetExtend(float level) {return level/3.f;}
	
		void	Setup(const CellStack *stacks,const ResourceDiscovery*visibility,const BaseGrid::TCell*lowGridCells, const BaseGrid::TCell*highGridCells,  const BaseGrid::TNetCell*lowNetCells,FClipHeight clipHeight,const Resource*resources, count_t numResources)
		{
			mesh.Reset();
			this->stacks = stacks;
			this->lowNetCells = lowNetCells;
			this->highGridCells = highGridCells;
			this->lowGridCells = lowGridCells;
			this->clipHeight = clipHeight;
			this->resources = resources;
			this->numResources = numResources;
			this->visibility = visibility;
		}

		void	Compile(const index_t offset, const index_t end, THull<ResourceVertex>&, SpinLock&lock);
	};



	
	class WaterCompiler
	{
	public:
		static const count_t BatchSize = TerrainCompiler::BatchSize;
		static const float4 waterColor;


		StaticMesh<WaterVertex, BatchSize * CellStack::Range * (7 + 7 + 4 * 6),BatchSize * CellStack::Range * 3 * (7 * 2 + 2 * 7)>	mesh;
		

		const CellStack * stacks;
		const BaseGrid::TCell*  gridCells;
		const BaseGrid::TNetCell*  netCells;
		float	cornerBrightness[6];
		FClipHeight	clipHeight;
		bool	wasEmpty;

		void	Setup(const CellStack *stacks,const BaseGrid::TCell*cells, const BaseGrid::TNetCell*netCells,FClipHeight clipHeight)
		{
			mesh.Reset();
			this->stacks = stacks;
			this->netCells = netCells;
			this->gridCells = cells;
			this->clipHeight = clipHeight;
		}


		void	Compile(const index_t offset, const index_t end, THull<WaterVertex>&, SpinLock&lock);
		void	CompileLayer(	const index_t i, const UINT ih, const CellStack&stack,
								const int clipH,const BaseGrid::TNetCell&cell,
								const BaseGrid::TCellNeighborhood&neighborhood, bool localEMP);
		void	CompileFaceToNeighbor(const int j, const UINT ih, const int ir,const BaseGrid::TNetCell&cell,const BaseGrid::TCellNeighborhood&neighbors,float newCornerColor[6], 
									const CellStack::Cell&thisCell, const TFloatRange<>&range, bool localEMP);
		void	CompileBottomLayer(const CellStack&stack, const int clipH,const BaseGrid::TNetCell&cell,
									const BaseGrid::TCellNeighborhood&neighbors, bool empThis);
		void	CompileCap(	const UINT ih,const BaseGrid::TNetCell&cell, const bool capped, const CellStack::Cell&thisCell, 
							const TFloatRange<>&range, const BaseGrid::TCellNeighborhood&neighbors, 
							const CellStack&stack, bool empThis);

		static float	GetWaterLightLevel(const CellStack::Cell&cell);
		float			GetWaterLightLevel(UINT32 stackID, UINT32 layer)	const;
	};
	/*static*/ const float4 WaterCompiler::waterColor = float4(0.5f, 0.6f, 1.f,0.6f);

	
	
	TBaseColor	GetBaseColor(const Grid::CellStack::Cell&cell)
	{
		TBaseColor rs = cell.typeColor[cell.content]; 
		//if (submergence > 0)
			//color = float3(0.1f);
		//Vec::abs(color);color *= 0.5f;

		rs.color.r *= powi(0.8f, cell.submergence); 
		rs.color.g *= powi(0.85f, cell.submergence);
		rs.color.b *= powi(0.95f, cell.submergence); 
		//rs.visibility = visible > 0 ? 1.f : 0.f;
		return rs; 
	}

	TLitColor			GetColorU(const Grid::CellStack&stack,  unsigned layer, Grid::Content oppositeContent, bool&mixes, Grid::Content&outType)
	{
		if (layer >= Grid::CellStack::Range)
		{
			mixes = false;
			outType = Grid::Content::Clear;
			return TLitColor(TVisibility(1.f));
		}
		typedef Grid::CellStack::Cell Cell;
		const Cell&cell = layer > 0 ? stack.volumeCell[layer - 1] : Grid::CellStack::bottom;
		const TVisibility&vis = layer > 0 ? stack.volumeCell[layer - 1].GetVisibility() : stack.volumeCell[0].GetVisibility();
		mixes = cell.content == oppositeContent || Cell::typeColorMixes[cell.content];
		outType = Cell::Content::Reinterpret(cell.content);
		return TLitColor(GetBaseColor(cell), stack.volumeCell[layer].GetLightLevel(),0.f,vis);
	}
	
	TLitColor			GetDownColorU(const Grid::CellStack&stack, unsigned layer, Grid::Content oppositeContent, bool&mixes, Grid::Content&outType)
	{
		if (layer+1 >= Grid::CellStack::Range)
		{
			mixes = false;
			outType = Grid::Content::Clear;
			return TLitColor();
		}
		typedef Grid::CellStack::Cell Cell;
		const Cell&cell = stack.volumeCell[layer + 1];
		mixes = oppositeContent==cell.content || Cell::typeColorMixes[cell.content];
		outType = Cell::Content::Reinterpret(cell.content);
		return TLitColor(GetBaseColor(cell), stack.volumeCell[layer].GetLightLevel(),0.f, cell.GetVisibility());
	}


	TWeightedColor			GetMixColorU(const Grid::CellStack&stack, unsigned layer, Grid::Content oppositeContent, float&sumBrightnessTo, int&sumVisibilityTo, Content&outContent)
	{
		if (layer >= Grid::CellStack::Range)
		{
			//sumBrightnessTo += 1.f;
			outContent = Content::Clear;
			sumVisibilityTo ++;
			return TWeightedColor();
		}
		typedef Grid::CellStack::Cell Cell;
		const Cell&upper = stack.volumeCell[layer];

		if (!layer)
		{
			sumBrightnessTo += upper.GetLightLevel();
			sumVisibilityTo += upper.IsCurrentlyVisible();
			outContent = Content::Clear;
			return TWeightedColor();
		}
		const Cell&cell = stack.volumeCell[layer - 1];
		float light = upper.GetLightLevel();
		sumVisibilityTo += (cell.IsCurrentlyVisible());
		if (!cell.IsOpaque() || upper.IsOpaque())
		{
			sumBrightnessTo += light * 1.3f;
			outContent = Content::Clear;
			return TWeightedColor();
				//float4(0, 0, 0, -0.1f);
				//float4(0.2f) * light;	//slight light increase
		}
		else
		{
			sumBrightnessTo += light;
			if (oppositeContent == cell.content || Cell::typeColorMixes[cell.content])
			{
				outContent = Content::Reinterpret( cell.content );
				return TWeightedColor(GetBaseColor(cell), 1.f);
			}
			outContent = Content::Clear;
			return TWeightedColor();
		}

	}


	
	TWeightedColor			GetDownMixColorU(const Grid::CellStack&stack, unsigned layer, Grid::Content oppositeContent, float&sumBrightnessTo, int&sumVisibilityTo, Content&outContent)
	{
		if (layer >= Grid::CellStack::Range)
		{
			sumBrightnessTo += 1.f;
			outContent = Content::Clear;
			return TWeightedColor();
		}
		if (!layer)
		{
			sumBrightnessTo += stack.volumeCell[layer].GetLightLevel();
			outContent = Content::Clear;
			return TWeightedColor();
		}
		typedef Grid::CellStack::Cell Cell;
		const Cell&cell = stack.volumeCell[layer - 1],
				&upper = stack.volumeCell[layer];
		float light = cell.GetLightLevel();
		sumVisibilityTo += cell.IsCurrentlyVisible();
		if (cell.IsOpaque() || !upper.IsOpaque())
		{
			sumBrightnessTo += light * 1.3f;
			outContent = Content::Clear;
			return TWeightedColor();
				//float4(0, 0, 0, -0.1f);
				//float4(0.2f) * light;	//slight light increase
		}
		else
		{
			sumBrightnessTo += light;
			if (oppositeContent == upper.content || Cell::typeColorMixes[upper.content])
			{
				outContent = Content::Reinterpret( upper.content );
				return TWeightedColor(GetBaseColor(upper), 1.f);
			}
			outContent = Content::Clear;

			return TWeightedColor();
		}

	}


	void		Compile(const Array<CellStack>&stacks, THull<TerrainVertex>&solid, THull<OverlayVertex>&cover, index_t offset, count_t count, FClipHeight clipHeight)
	{
		const index_t end = offset + count;
		//SpinLock lock;
		const count_t numBatches = CeilDiv(count,TerrainCompiler::BatchSize);
		static Array<THull<TerrainVertex> >	hulls;
		static Array<THull<OverlayVertex> >	covers;
		count_t numWorkers = System::ParallelLoop::global_instance.getWorkerCount();
		if (hulls.Count() < numWorkers)
		{
			hulls.SetSize(numWorkers);
			covers.SetSize(numWorkers);
		}
				
				


		System::parallelFor(numBatches,8,[end,offset, &stacks, clipHeight](index_t i, index_t workerIndex)
		{
			BYTE data[sizeof(TerrainCompiler)];	//prevent implicit memset in release mode. i'm serious. visual studio has some funny ideas there...
			TerrainCompiler&compiler = *(TerrainCompiler*)data;
			compiler.Setup(stacks.pointer(),Grid::high.grid.GetCells(),Grid::high.cells.pointer(),clipHeight);
			compiler.Compile(offset + i*TerrainCompiler::BatchSize,end);
			compiler.DeliverResults(hulls[workerIndex],covers[workerIndex]);
		});
		for (index_t i = 0; i < numWorkers; i++)
		{
			hulls[i].AppendTo(solid);
			covers[i].AppendTo(cover);
			hulls[i].Clear();
			covers[i].Clear();
		}

	}



	void		CompileWater(const Array<CellStack>&stacks, THull<WaterVertex>&cover, index_t offset, count_t count, FClipHeight clipHeight)
	{
		const index_t end = offset + count;
		SpinLock lock;
		Concurrency::parallel_for(offset, end, WaterCompiler::BatchSize,[end,&lock, &cover, &stacks, clipHeight](index_t i)	//looks good - at least in release
		{
			BYTE data[sizeof(WaterCompiler)];	//prevent implicit memset in release mode. i'm serious. visual studio has some funny ideas there...
			WaterCompiler&compiler = *(WaterCompiler*)data;
			compiler.Setup(stacks.pointer(),Grid::high.grid.GetCells(),Grid::high.cells.pointer(),clipHeight);
			compiler.Compile(i,end,cover,lock);
		});
	}





	void		BakeBlockInstruction(const Array<CellStack>&stacks, BYTE family, FClipHeight clipHeight)
	{
		//if (!section)
		//{
		//	maxHeight = CellStack::HalfRange;
		//	maxDistance = 0xFFFFFFFF;
		//}
		TFamily &fam = VisualTerrain::family[family];
		TFamily::TData&dat = fam.normal;

		dat.terrain.Clear();
		dat.overlay.Clear();
		dat.water.Clear();
		Compile(stacks, dat.terrain, dat.overlay, Grid::high.grid.GetFamilyOffset(family), Grid::high.grid.CountFamilyMembers(family), clipHeight);
		CompileWater(stacks, dat.water, Grid::high.grid.GetFamilyOffset(family), Grid::high.grid.CountFamilyMembers(family), clipHeight);

		Box<> box = Box<>::Invalid();

		foreach(dat.terrain.vertices, vtx)
			box.Include(vtx->vertex.xyz);
		foreach(dat.water.vertices, vtx)
			box.Include(vtx->vertex.xyz);
		dat.boundingBox = box;
		dat.boundingSphere.center = box.GetCenter();
		dat.boundingSphere.radius = 0.f;
		foreach(dat.terrain.vertices, vtx)
			dat.boundingSphere.IncludeSquare(vtx->vertex.xyz);
		dat.boundingSphere.radius = sqrt(dat.boundingSphere.radius);


		if (dat.terrain.instruction[0].shader.IsEmpty())
		{
			(DX11::BaseInstructionBlock&)dat.terrain.shadowInstruction = Instruction::terrainShadowBase;
			(DX11::BaseInstructionBlock&)dat.terrain.instruction[0] = Instruction::terrainBase;
			(DX11::BaseInstructionBlock&)dat.terrain.instruction[1] = Instruction::terrainParaboloidBase;
			(DX11::BaseInstructionBlock&)dat.terrain.instruction[2] = Instruction::terrainSphereBase;

			(DX11::BaseInstructionBlock&)dat.overlay.instruction = Instruction::overlayBase;
			(DX11::BaseInstructionBlock&)dat.overlay.shadowInstruction = Instruction::overlayShadowBase;
			(DX11::BaseInstructionBlock&)dat.water.instruction[0] = Instruction::waterBase;
			(DX11::BaseInstructionBlock&)dat.water.instruction[1] = Instruction::waterParaboloidBase;
			(DX11::BaseInstructionBlock&)dat.water.instruction[2] = Instruction::waterSphereBase;
		}


		dat.terrain.instruction[0].vbo.CreateFromStructs(dat.terrain.vertices);
		dat.terrain.instruction[0].ibo.Create(dat.terrain.indices);
		dat.terrain.shadowInstruction.vbo = dat.terrain.instruction[0].vbo;
		dat.terrain.shadowInstruction.ibo = dat.terrain.instruction[0].ibo;
		dat.terrain.instruction[1].vbo = dat.terrain.instruction[0].vbo;
		dat.terrain.instruction[1].ibo = dat.terrain.instruction[0].ibo;
		dat.terrain.instruction[2].vbo = dat.terrain.instruction[0].vbo;
		dat.terrain.instruction[2].ibo = dat.terrain.instruction[0].ibo;
		dat.overlay.instruction.vbo.CreateFromStructs(dat.overlay.vertices);
		dat.overlay.instruction.ibo.Create(dat.overlay.indices);
		dat.overlay.shadowInstruction.vbo = dat.overlay.instruction.vbo;
		dat.overlay.shadowInstruction.ibo = dat.overlay.instruction.ibo;
		dat.water.instruction[0].vbo.CreateFromStructs(dat.water.vertices);
		dat.water.instruction[0].ibo.Create(dat.water.indices);
		dat.water.instruction[1].vbo = dat.water.instruction[0].vbo;
		dat.water.instruction[1].ibo = dat.water.instruction[0].ibo;
		dat.water.instruction[2].vbo = dat.water.instruction[0].vbo;
		dat.water.instruction[2].ibo = dat.water.instruction[0].ibo;
	}



	void CompileResources(THull<ResourceVertex>&cover, ResourceDiscovery&visibility, index_t offset, count_t count, FClipHeight clipHeight, const Resource*resources, count_t numResources)
	{
		const index_t end = offset + count;
		SpinLock lock;
		Concurrency::parallel_for(offset, end, ResourceCompiler::BatchSize,[end,&lock,&visibility, &cover, clipHeight,resources, numResources](index_t i)	//looks good - at least in release
		{
			BYTE data[sizeof(ResourceCompiler)];	//prevent implicit memset in release mode. i'm serious. visual studio has some funny ideas there...
			ResourceCompiler&compiler = *(ResourceCompiler*)data;
			compiler.Setup(World::actual.cellStack.pointer(),&visibility,low.grid.GetCells(),high.grid.GetCells(), low.cells.pointer(),clipHeight,resources, numResources);
			compiler.Compile(i,end,cover,lock);
		});
	}


	void	TerrainCompiler::CompileAboveTerrainOutline(const UINT ih, const int ir,const BaseGrid::TNetCell&cell,const BaseGrid::TCellNeighborhood&neighbors)
	{
		float h = ToRadius(ir + 1);
		coverMesh.SetOffset();
		coverMesh << OverlayVertex(cell.centerVertex, h, 0);
		for (int j = 0; j < cell.numCorners; j++)
		{
			const UINT32 n0 = neighbors.neighborID[j];
			const UINT32 n1 = neighbors.neighborID[(j + 1) % cell.numCorners];
			const UINT32 n2 = neighbors.neighborID[(j + 2) % cell.numCorners];
			bool anyOpen = !stacks[n0].GetCellU(ih + 1).IsOpaque() || !stacks[n1].GetCellU(ih + 1).IsOpaque();

			coverMesh << OverlayVertex(cell.cornerVertex[j], h, anyOpen ? 0.45f : 0.f);

			coverMesh << 0;
			coverMesh << 1 + (UINT16)j;
			coverMesh << 1 + (UINT16)(j + 1) % cell.numCorners;
		}	
	}

	TWeightedColor	TerrainCompiler::GetMixColorU(index_t stackIndex,  UINT ih, Content oppositeContent, float&brightness, int&sumVisibilityTo, Content&outContent) const
	{
		return VisualTerrain::GetMixColorU(stacks[stackIndex],ih,oppositeContent, brightness, sumVisibilityTo, outContent);
		//return rs;
	}

	TWeightedColor	TerrainCompiler::GetDownMixColorU(index_t stackIndex,  UINT ih, Content oppositeContent, float&brightness, int&sumVisibilityTo, Content&outContent) const
	{
		return VisualTerrain::GetDownMixColorU(stacks[stackIndex],ih, oppositeContent, brightness, sumVisibilityTo, outContent);
		//return rs;
	}



	/*static*/ bool TerrainCompiler::AffectedByEMP(index_t stackIndex)
	{
		return FlagsAffectedByEMP(Grid::high.cells[stackIndex].cornerFlags);
	}


	TBaseColor::TBaseColor(const TWeightedColor&c):color(c.color / c.weight)
	{}


	void	TLitColor::CompileFinalColor(TVec4<>&rs)	const
	{
		rs.a = capped ? 0.f : lightLevel;
		rs.rgb = color;
		#ifndef DEBUG_HIDE_SENSORY
			if (visibility < 0.5f)
			{
				#if 0
					color = (float3)0.f;
				#else
					//float avg = (color.r + color.g + color.b) / 3.f * 0.5f + 0.5f;
					if (color.r < 0)
					{
						rs.r = -(1.f - (1.f + color.r) / 4.f);
						rs.g = -(1.f - (1.f + color.g) / 4.f);
						rs.b = -(1.f - (1.f + color.b) / 4.f);
					}
					else
					{
						rs.r = 1.f - (1.f - color.r) / 4.f;
						rs.g = 1.f - (1.f - color.g) / 4.f;
						rs.b = 1.f - (1.f - color.b) / 4.f;
					}
					//color.r = (color.r + typeColor[0].r*2) / 3.f;
					//color.g = (color.g + typeColor[0].g*2) / 3.f;
					//color.b = (color.b + typeColor[0].b*2) / 3.f;
				#endif
			}
			else
		#endif

		if (capped)
			CapColor(rs.rgb);
		ApplyEMP(emp,rs.rgb);
	}

	/*static*/ void TLitColor::ApplyEMP(float intensity,TVec3<>&rs)
	{
		if (intensity > 0.f)
		{
			rs *= -1.5f;
			rs.rg *= 0.8f;
			rs.b *= 1.2f;
			//rs.b *= 1.5f;
			//rs.b -= (2.f + rs.b)*intensity;
			//TVec3<> rs2;
			//if (rs.r < 0)
			//	rs2 = rs * -0.4f + 0.6f;
			//else
			//	rs2 = rs * 0.4f + 0.6f;
			//rs2.r *= 0.7f;
			//rs2.g *= 0.85f;
			//if (rs.r < 0)
			//	rs2 = -rs2;
			//Vec::interpolate(rs,rs2,intensity,rs);
			//if (rs.b > 0)
			//	rs.b = -rs.b;

		}
	}

	void TLitColor::CapColor(TVec3<>&rs)	const
	{
		if (rs.r >= 0.f)
		{
			rs = Vector<>::zero;
			//rs.r = (1.f - visibility) ;
		}
		else
			rs *= 0.5f;
	}







	void	TerrainCompiler::CompileCap(const UINT ih,const BaseGrid::TNetCell&cell, const bool capped, const CellStack::Cell&thisCell, const TFloatRange<>&range,const BaseGrid::TCellNeighborhood&neighbors, const CellStack&stack)
	{

		bool mixes0;
		Content centerType;
		TLitColor color0 = GetColorU(stack,ih, Content::Clear, mixes0,centerType);
		color0.capped = capped;
		color0.emp = FlagsAffectedByEMP(cell.cornerFlags);
			
		solidMesh << TerrainVertex(cell.centerVertex, range.max, cell.centerVertex.xyz, color0, thisCell.GetStructureFactor());
		for (int j = 0; j < cell.numCorners; j++)
		{
			{
				float brightness = color0.lightLevel;
				TWeightedColor color;

				float emp = color0.emp;
				int visibility = color0.visibility > 0.5f;
				//if (capped)
				//{
				//	//brighness = 0.f;
				//	color = float4(0,0,0,1);
				//	brightness = 0;
				//}
				//else
				{
				//	color = TLitColor();
					Content c0,c1;
					index_t n1 = neighbors.neighborID[(j + 1) % cell.numCorners];
					index_t n2 = neighbors.neighborID[j];
					color += GetMixColorU(n1,ih,centerType,brightness,visibility,c0);
					color += GetMixColorU(n2, ih, centerType,brightness,visibility,c1);
					emp += AffectedByEMP(n1);
					emp += AffectedByEMP(n2);
					emp /= 3;

					TWeightedColor add(color0, 1.f);
					if (!mixes0)
					{
							//mild bleeding...
						if (c0 != centerType)
							add *= 2.f;
						if (c1 != centerType)
							add *= 2.f;
					}
					color += add;
					brightness /= 3;
					//color.rgb *= brightness / color.a;
					//brightness /= color.a;
				}
				TLitColor col(color,brightness,emp);
				col.visibility = float(visibility) / 3;
				col.capped = capped;

				solidMesh << TerrainVertex(cell.cornerVertex[j], range.max, cell.cornerVertex[j].xyz, col, 1.f);
			}
			solidMesh << 0 << 1 + (UINT16)j << 1 + (UINT16)(j + 1) % cell.numCorners;
		}	
	}

	
	void	TerrainCompiler::CompileDownCap(const UINT ih,const BaseGrid::TNetCell&cell, const CellStack::Cell&thisCell, const int ir,const BaseGrid::TCellNeighborhood&neighbors, const CellStack&stack)
	{
		float h = ToRadius(ir + 1);
		bool mixes0;
		solidMesh.SetOffset();
		Content centerType;
		TLitColor color0 = GetDownColorU(stack, ih, Content::Clear, mixes0,centerType);
		color0.emp = FlagsAffectedByEMP(cell.cornerFlags) ? 1.f : 0.f;

		solidMesh << TerrainVertex(cell.centerVertex, h, -cell.centerVertex.xyz, color0, thisCell.GetStructureFactor());

		for (int j = 0; j < cell.numCorners; j++)
		{
			{
				float brightness = color0.lightLevel;
				TWeightedColor color(color0.color, 1.f);
				int visibility = color0.visibility > 0.5f;
				float emp = color0.emp;
				{
					if (!mixes0)
						color *= 4.f; //mild bleeding
					Content c0,c1;
					index_t n1 = neighbors.neighborID[(j + 1) % cell.numCorners];
					index_t n2 = neighbors.neighborID[j];
					color += GetDownMixColorU(n1,ih + 1, centerType,brightness,visibility,c0);
					color += GetDownMixColorU(n2,ih + 1, centerType,brightness,visibility,c1);
					emp += AffectedByEMP(n1);
					emp += AffectedByEMP(n2);
					brightness /= 3;
					emp /= 3;
					//color.rgb *= brightness / color.a;
					//brightness /= color.a;
				}
				TLitColor col(color,brightness,emp);
				col.visibility = float(visibility) / 3;
				solidMesh << TerrainVertex(cell.cornerVertex[j], h, -cell.cornerVertex[j].xyz, col, 1.f);
			}
			solidMesh << 0 << 1 + (UINT16)(j + 1) % cell.numCorners << 1 + (UINT16)j;
		}	
	}

	void	TerrainCompiler::CompileFaceToNeighbor(const int j, const UINT ih, const int ir,const BaseGrid::TNetCell&cell,
											const BaseGrid::TCellNeighborhood&neighbors,TLitColor newCornerColor[6], const CellStack::Cell&thisCell, const CellStack::Cell&lastCell,
											const TFloatRange<>&range )
	{
		const UINT32 n0 = neighbors.neighborID[j];
		const UINT32 n1 = neighbors.neighborID[(j + 1) % cell.numCorners];
		const UINT32 n2 = neighbors.neighborID[(j + 2) % cell.numCorners];
		const CellStack&sn = stacks[n1];
		const bool filled = sn.GetCellU(ih).IsOpaque();
		//bool wasEmpty = false;
		const bool ncutoff = IsClipped(clipHeight, n1, ir);
		{
			TLitColor	color1 = newCornerColor[j],
					color2 = newCornerColor[(j + 1) % cell.numCorners],
					lowerColor1 = wasEmpty ? newCornerColor[j] : cornerColor[j],
					lowerColor2 = wasEmpty ? newCornerColor[(j + 1) % cell.numCorners] : cornerColor[(j + 1) % cell.numCorners];


			if (ncutoff && filled)
			{
				color1.capped = true;
				color2.capped = true;
				lowerColor1.capped = true;
				lowerColor2.capped = true;
			}
			if (!thisCell.Mixes() || !lastCell.Mixes())
			{
				lowerColor1.color = color1.color;// * lowerColor1.w;
				lowerColor2.color = color2.color;// * lowerColor2.w;
			}
			else
			{
				//lowerColor1.xyz = lowerColor1.xyz * lowerColor1.w;
				//lowerColor2.xyz = lowerColor2.xyz * lowerColor2.w;
			}
			//color1.xyz = color1.xyz * color1.w;
			//color2.xyz = color2.xyz * color2.w;

			if (!filled || ncutoff)
			{
				TVec3<> normal;
				const auto&v1 = cell.cornerVertex[(j + 1) % cell.numCorners];
				const auto&v0 = cell.cornerVertex[j];
				Vec::cross(v0.xyz, v0.xyz - v1.xyz, normal);
				Vec::normalize(normal);
				solidMesh.SetOffset();
				solidMesh <<  TerrainVertex(v0, range.max, normal, color1, 1.f);
				solidMesh <<  TerrainVertex(v0, range.min, normal, lowerColor1, 1.f);
				solidMesh <<  TerrainVertex(v1, range.min, normal, lowerColor2, 1.f);
				solidMesh <<  TerrainVertex(v1, range.max, normal, color2, 1.f);
				solidMesh << 0 << 1 << 2 << 0 << 2 << 3;
			}


		}
	
	}

	static float inline Brighten(float lightLevel)	//disabled
	{
		return lightLevel;
	
	}


	void	TerrainCompiler::CompileLayer(const UINT ih, const CellStack&stack, const int clipH,const BaseGrid::TNetCell&cell,const BaseGrid::TCellNeighborhood&neighbors)
	{
		const int ir = CellStack::ToHeight(ih);

		//if (IsClipped(clipHeight, i, ir))
			//break;

		const CellStack::Cell&lastCell = stack.GetCellI(ir - 1);
		const CellStack::Cell&thisCell = stack.volumeCell[ih];
		const CellStack::Cell&nextCell = stack.GetCellU(ih + 1);
			

		if (!thisCell.IsOpaque())
		{
			wasEmpty = true;
			//for (int j = 0; j < cell.numCorners; j++)
				//cornerColor[j] = float4(1);
			const bool capped = nextCell.IsOpaque();
			if (capped
				
				)
			{
				if (ir+1 >= clipH)
					CompileAboveTerrainOutline(ih,ir,cell,neighbors);
				else
				{
					CompileDownCap(ih,cell,thisCell,ir,neighbors,stack);
				}
			}
			return;
		}

		const TFloatRange<> range = FloatRange(
			ToRadius(ir),
			ToRadius(ir + 1));
		{
			//float b = smoothstep(0.9, 1, range.max / WorldScale)*0.9f + 0.1f;
			static const float delta = 0.1f;

			solidMesh.SetOffset();

			TLitColor baseColor = TLitColor(GetBaseColor(thisCell),0.f,FlagsAffectedByEMP( cell.cornerFlags),thisCell.GetVisibility());
			TLitColor nextColor = TLitColor(nextCell.IsOpaque() && nextCell.Mixes() ? TLitColor(GetBaseColor(nextCell),0.f, baseColor.emp,thisCell.GetVisibility()) : baseColor);

			const bool capped = nextCell.IsOpaque();
			if (!capped
				|| ir+1 >= clipH
				)
			{
				CompileCap(ih+1,cell,capped,thisCell,range,neighbors,stack);
			}

			TLitColor newCornerColor[6];

			{
				TLitColor color = thisCell.Mixes() ? nextColor : baseColor;
				for (int j = 0; j < cell.numCorners; j++)
				{
					const UINT32 n0 = neighbors.neighborID[j];
					const UINT32 n1 = neighbors.neighborID[(j + 1) % cell.numCorners];
					const CellStack&sn = stacks[n1];
					{
						float c1 = (stacks[n1].GetCellU(ih).GetLightLevel() + stacks[n1].GetCellU(ih + 1).GetLightLevel()) / 2.f;
						float light = (c1 * 2.f
							+
							(Brighten(stacks[n0].GetCellU(ih).GetLightLevel()) + Brighten(stacks[n0].GetCellU(ih + 1).GetLightLevel())) / 2.f);	//original 'mild' levels

						light /= 3.f;

						newCornerColor[j] = color;
						newCornerColor[j].lightLevel = light;
					}
				}
			}
			for (int j = 0; j < cell.numCorners; j++)
			{
				CompileFaceToNeighbor(j,ih,ir,cell,neighbors,newCornerColor,thisCell,lastCell,range);
			}
			for (int j = 0; j < cell.numCorners; j++)
				cornerColor[j] = newCornerColor[j];
			wasEmpty = false;
		}
		
	}
	

	void	TerrainCompiler::CompileBottomLayer(const CellStack&stack, const int clipH,const BaseGrid::TNetCell&cell,const BaseGrid::TCellNeighborhood&neighbors)
	{
		//static const int ir = -CellStack::HalfRange;
		//static const UINT ih = 0;

		//if (IsClipped(clipHeight, i, ir))
			//break;

		const CellStack::Cell&thisCell = CellStack::bottom;
		const CellStack::Cell&nextCell = stack.GetCellU(0);

			
		if (thisCell.content == Content::Clear)
		{
			if (nextCell.content != Content::Clear)
			{
				float h = ToRadius(-CellStack::HalfRange);
				bool mixes0;
				solidMesh.SetOffset();
				Content centerContent;
				TLitColor color0 = GetDownColorU(stack, 0, Content::Clear,mixes0,centerContent);
				solidMesh << TerrainVertex(cell.centerVertex, h, -cell.centerVertex.xyz, color0, thisCell.GetStructureFactor());
				for (int j = 0; j < cell.numCorners; j++)
				{
					{
						float brightness = color0.lightLevel;
						TWeightedColor color(color0.color, 1.f);
						int visibility = color0.visibility > 0.5f;
						{
							Content c0,c1;

							if (!mixes0)
								color *= 4.f; //mild bleeding
							const UINT32 n0 = neighbors.neighborID[(j + 1) % cell.numCorners];
							const UINT32 n1 = neighbors.neighborID[j];
							color += GetDownMixColorU(n0,0, centerContent,brightness,visibility,c0);
							color += GetDownMixColorU(n1,0, centerContent,brightness,visibility,c1);

							brightness /= 3;
							//color.rgb *= brightness / color.a;
							//brightness /= color.a;
						}
						TLitColor col(color,brightness);
						col.visibility = float(visibility)/3;
						solidMesh << TerrainVertex(cell.cornerVertex[j], h, -cell.cornerVertex[j].xyz, col, 1.f);
					}
					solidMesh << 0 << 1 + (UINT16)(j + 1) % cell.numCorners << 1 + (UINT16)j;
				}	

			}
			return;
		}


		const TFloatRange<> range = FloatRange(
			ToRadius(-1-CellStack::HalfRange),
			ToRadius(-CellStack::HalfRange));
		{
			//float b = smoothstep(0.9, 1, range.max / WorldScale)*0.9f + 0.1f;
			static const float delta = 0.1f;

			solidMesh.SetOffset();

			//const TBaseColor baseColor = thisCell.GetBaseColor();
			//const TBaseColor nextColor = nextCell.IsOpaque() && nextCell.Mixes() ? nextCell.GetBaseColor() : baseColor;
			const bool capped = nextCell.IsOpaque();
			if (!capped
				)
			{
				CompileCap(0,cell,capped,thisCell,range,neighbors,stack);
			}


		}
		
	}


	void	TerrainCompiler::Compile(const index_t offset, const index_t end)
	{
		for (index_t i = offset; i < offset+BatchSize && i < end; i++ )
		{
			const ::Grid::CellStack&stack = stacks[i];
			const auto&neighbors = gridCells[i].neighborhood;
			const auto&cell = netCells[i];



			UINT32 begin = stack.lowestOpenCell ? (UINT32)stack.lowestOpenCell - 1 : 0;
			begin = std::min(begin, CellStack::ToLayer(clipHeight(i)-1));
			//while (begin > 0 && clipped(i, begin))
				//begin--;
			UINT32 end = (UINT32)stack.highestOpaqueCell + 1;

			//TVec4<> cornerColor[6];

			for (int j = 0; j < cell.numCorners; j++)
			{
				UINT32 n = neighbors.neighborID[j];
				const ::Grid::CellStack&stack = stacks[n];
				UINT32 _begin = stack.lowestOpenCell ? (UINT32)stack.lowestOpenCell - 1 : 0;
				_begin = std::min(_begin, CellStack::ToLayer(clipHeight(n)-1));
				//while (_begin > 0 && clipped(n, _begin))
				//	_begin--;
				UINT32 _end = (UINT32)stack.highestOpaqueCell + 1;
				begin = std::min(begin, _begin);
				end = std::max(end, _end);

			}
			wasEmpty = true;

			const int clipH = clipHeight(i);
			end = std::min(end, CellStack::ToLayer(clipH));
			if (begin == 0 || CellStack::bottom.content == Content::Clear)
				CompileBottomLayer(stack,clipH,cell,neighbors);
			if (end >= CellStack::Range)
				begin = std::min(begin,end - 1);
			for (UINT ih = begin; ih < end; ih++)
			{
				CompileLayer(ih,stack,clipH,cell,neighbors);
			}
		}
	}

	void TerrainCompiler::DeliverResults(THull<TerrainVertex>&solidHull, THull<OverlayVertex>&coverHull)
	{
		if (solidMesh.IsNotEmpty() || coverMesh.IsNotEmpty())
		{
			//lock.lock();

			if (solidMesh.IsNotEmpty())
			{
				solidMesh.AppendTo(solidHull);
			}

			if (coverMesh.IsNotEmpty())
			{
				coverMesh.AppendTo(coverHull);
			}

			//lock.unlock();
		}
		
	}


	void	WaterCompiler::CompileBottomLayer(const CellStack&stack, const int clipH,const BaseGrid::TNetCell&cell,const BaseGrid::TCellNeighborhood&neighbors, bool empThis)
	{
		//static const int ir = -CellStack::HalfRange;
		//static const UINT ih = 0;

		//if (IsClipped(clipHeight, i, ir))
			//break;

		const CellStack::Cell&thisCell = CellStack::bottom;
		const CellStack::Cell&nextCell = stack.GetCellU(0);

			
		if (!thisCell.IsWater() || nextCell.IsWater())
		{
			return;
		}


		const TFloatRange<> range = FloatRange(
			ToRadius(-1-CellStack::HalfRange),
			ToRadius(-CellStack::HalfRange));
		{
			//float b = smoothstep(0.9, 1, range.max / WorldScale)*0.9f + 0.1f;
			static const float delta = 0.1f;

			//mesh.SetOffset();

			//const TVec3<> baseColor = thisCell.GetBaseColor();
			//const TVec3<> nextColor = nextCell.IsOpaque() && nextCell.Mixes() ? nextCell.GetBaseColor() : baseColor;
			//const bool capped = nextCell.IsOpaque();
			//if (!capped
				//)
			{
				//CompileCap(ih+1,cell,capped,thisCell,range,neighbors,stack);
				CompileCap(0,cell,false,thisCell,range,neighbors,stack,empThis);
			}


		}
		
	}


	void	WaterCompiler::CompileFaceToNeighbor(const int j, const UINT ih, const int ir,const BaseGrid::TNetCell&cell,
											const BaseGrid::TCellNeighborhood&neighbors,float newCornerLight[6], const CellStack::Cell&thisCell, 
											const TFloatRange<>&range,
											bool localEMP )
	{
			//const UINT32 n1 = neighbors.neighborID[(j + 1) % cell.numCorners];
			//const CellStack&sn = stacks[n1];
			//const bool filled = sn.GetCellU(ih).content == CellStack::Cell::Content::Water;
			//{


			//	if (!filled )
			//	{
			//		//const UINT32 n0 = neighbors.neighborID[j];
			//		//const UINT32 n2 = neighbors.neighborID[(j + 2) % cell.numCorners];
			//		float	light1 = newCornerLight[j],
			//				light2 = newCornerLight[(j + 1) % cell.numCorners],
			//				lowerLight1 = wasEmpty ? newCornerLight[j] : cornerBrightness[j],
			//				lowerLight2 = wasEmpty ? newCornerLight[(j + 1) % cell.numCorners] : cornerBrightness[(j + 1) % cell.numCorners];
			//		
			//		TVec3<> normal;
			//		const auto&v1 = cell.cornerVertex[(j + 1) % cell.numCorners];
			//		const auto&v0 = cell.cornerVertex[j];
			//		Vec::cross(v0.xyz, v0.xyz - v1.xyz, normal);
			//		Vec::normalize(normal);

			//		mesh.SetOffset();
			//		mesh <<  WaterVertex(v0, range.max, normal, light1);
			//		mesh <<  WaterVertex(v0, range.min, normal, lowerLight1);
			//		mesh <<  WaterVertex(v1, range.min, normal, lowerLight2);
			//		mesh <<  WaterVertex(v1, range.max, normal, light2);
			//		mesh << 0 << 1 << 2 << 0 << 2 << 3;
			//	}
			//}


		const UINT32 n0 = neighbors.neighborID[j];
		const UINT32 n1 = neighbors.neighborID[(j + 1) % cell.numCorners];
//		const UINT32 n2 = neighbors.neighborID[(j + 2) % cell.numCorners];
		const CellStack&sn = stacks[n1];
		const bool filled = sn.GetCellU(ih).content == CellStack::Cell::Content::Water;
		//const bool filled = sn.GetCellU(ih).IsOpaque();
		//bool wasEmpty = false;
		const bool ncutoff = IsClipped(clipHeight, n1, ir);
		{
			//TVec4<>	color1 = newCornerColor[j],
			//		color2 = newCornerColor[(j + 1) % cell.numCorners],
			//		lowerColor1 = wasEmpty ? newCornerColor[j] : cornerColor[j],
			//		lowerColor2 = wasEmpty ? newCornerColor[(j + 1) % cell.numCorners] : cornerColor[(j + 1) % cell.numCorners];

			//if (ncutoff && filled)
			//{
			//	color1 = float4();
			//	color2 = float4();
			//	lowerColor1 = float4();
			//	lowerColor2 = float4();
			//}
			//if (!thisCell.Mixes() || !lastCell.Mixes())
			//{
			//	lowerColor1.xyz = color1.xyz;// * lowerColor1.w;
			//	lowerColor2.xyz = color2.xyz;// * lowerColor2.w;
			//}
			//else
			//{
			//	//lowerColor1.xyz = lowerColor1.xyz * lowerColor1.w;
			//	//lowerColor2.xyz = lowerColor2.xyz * lowerColor2.w;
			//}
			////color1.xyz = color1.xyz * color1.w;
			////color2.xyz = color2.xyz * color2.w;

			if (!filled || ncutoff)
			{
				float	light1 = newCornerLight[j],
						light2 = newCornerLight[(j + 1) % cell.numCorners],
						lowerLight1 = wasEmpty ? newCornerLight[j] : cornerBrightness[j],
						lowerLight2 = wasEmpty ? newCornerLight[(j + 1) % cell.numCorners] : cornerBrightness[(j + 1) % cell.numCorners];
						
				TVec3<> normal;
				const auto&v1 = cell.cornerVertex[(j + 1) % cell.numCorners];
				const auto&v0 = cell.cornerVertex[j];
				Vec::cross(v0.xyz, v0.xyz - v1.xyz, normal);
				Vec::normalize(normal);


				bool empThat0 = !(this->gridCells[n0].cornerFlags & empZonesUnlocked);
				bool empThat1 = !(this->gridCells[n1].cornerFlags & empZonesUnlocked);
				int empMix = (int(localEMP) + int(empThat0) + int(empThat1));

				mesh.SetOffset();
				mesh <<  WaterVertex(v0, range.max, normal, light1, empMix);
				mesh <<  WaterVertex(v0, range.min, normal, lowerLight1, empMix);
				mesh <<  WaterVertex(v1, range.min, normal, lowerLight2, empMix);
				mesh <<  WaterVertex(v1, range.max, normal, light2, empMix);
				mesh << 0 << 1 << 2 << 0 << 2 << 3;

/*

				TVec3<> normal;
				const auto&v1 = cell.cornerVertex[(j + 1) % cell.numCorners];
				const auto&v0 = cell.cornerVertex[j];
				Vec::cross(v0.xyz, v0.xyz - v1.xyz, normal);
				Vec::normalize(normal);
				solidMesh.SetOffset();
				solidMesh <<  TerrainVertex(v0, range.max, normal, color1, 1.f);
				solidMesh <<  TerrainVertex(v0, range.min, normal, lowerColor1, 1.f);
				solidMesh <<  TerrainVertex(v1, range.min, normal, lowerColor2, 1.f);
				solidMesh <<  TerrainVertex(v1, range.max, normal, color2, 1.f);
				solidMesh << 0 << 1 << 2 << 0 << 2 << 3;*/
			}


		}
	
	}

	/*static*/ float	WaterCompiler::GetWaterLightLevel(const CellStack::Cell&cell)
	{
		#ifdef DEBUG_HIDE_SENSORY
			return 1.f;
		#else
			return float(cell.IsCurrentlyVisible());
		#endif
	}

	float	WaterCompiler::GetWaterLightLevel(UINT32 stackID, UINT32 layer) const
	{
		return GetWaterLightLevel(stacks[stackID].volumeCell[vmin(layer,CellStack::Range-1)]);
	}



	void	WaterCompiler::CompileCap(const UINT ih,const BaseGrid::TNetCell&cell, const bool capped, const CellStack::Cell&thisCell, const TFloatRange<>&range,const BaseGrid::TCellNeighborhood&neighbors, const CellStack&stack, bool empThis)
	{
		const UINT layer = vmin(ih, CellStack::Range-1);
		float light0 = GetWaterLightLevel(stack.volumeCell[layer]);
		
		mesh.SetOffset();
		mesh <<  WaterVertex(cell.centerVertex, range.max, cell.centerVertex.xyz, light0,empThis?3:0);
		for (int j = 0; j < cell.numCorners; j++)
		{
			{
				float brightness = light0;
				UINT32 n1 = neighbors.neighborID[(j + 1) % cell.numCorners];
				UINT32 n0 = neighbors.neighborID[j];
				{
					const auto& sn1 = stacks[n1];
					const auto&	sn2 = stacks[n0];
					brightness += GetWaterLightLevel(sn1.volumeCell[layer]);
					brightness += GetWaterLightLevel(sn2.volumeCell[layer]);

					brightness /= 3;
				}
				bool empThat0 = !(this->gridCells[n0].cornerFlags & empZonesUnlocked);
				bool empThat1 = !(this->gridCells[n1].cornerFlags & empZonesUnlocked);

				int empMix = (int(empThis) + int(empThat0) + int(empThat1));

				mesh << WaterVertex(cell.cornerVertex[j], range.max, cell.cornerVertex[j].xyz, brightness,empMix);
			}
					
			mesh << 0 <<  1 + (UINT16)j << 1 + (UINT16)(j + 1) % cell.numCorners;
		}
	}


	void	WaterCompiler::CompileLayer(const index_t i, const UINT ih, const CellStack&stack, const int clipH, const BaseGrid::TNetCell&cell,const BaseGrid::TCellNeighborhood&neighbors, bool empThis)
	{
		const int ir = int(ih) - CellStack::HalfRange;
		//if (clipped(i, ir))
			//break;

		const CellStack::Cell&thisCell = stack.volumeCell[ih];
		const CellStack::Cell&nextCell = stack.GetCellU(ih + 1);


		if (thisCell.content != CellStack::Cell::Content::Water)
		{
			wasEmpty = true;
			return;
		}

		const TFloatRange<> range = FloatRange(
			ToRadius(ir),
			ToRadius(ir + 1));
		{
			static const float delta = 0.1f;
	
			const bool capped = nextCell.content == CellStack::Cell::Content::Water;
			if (!capped
				|| ir+1 >= clipH
				)
			{
							
				//color0.rgb = waterColor;
				//CompileCap(const UINT ih,const BaseGrid::TNetCell&cell, const bool capped, const CellStack::Cell&thisCell, const TFloatRange<>&range,const BaseGrid::TCellNeighborhood&neighbors, const CellStack&stack)
				CompileCap(ih+1,cell,capped,thisCell,range,neighbors,stack, empThis);
			
			}

			float newCornerLight[6];

			for (int j = 0; j < cell.numCorners; j++)
			{
				const UINT32 n0 = neighbors.neighborID[j];
				const UINT32 n1 = neighbors.neighborID[(j + 1) % cell.numCorners];
				const CellStack&sn = stacks[n1];
				{
					float c1 = (GetWaterLightLevel(n1,ih) + GetWaterLightLevel(n1,ih+1))/2.f;
					float color1 = (c1 * 2.f
						+
						(Brighten(GetWaterLightLevel(n0,ih)) + Brighten(GetWaterLightLevel(n0,ih+1))) / 2.f);	//originally 'mild' levels

					color1 /= 3.f;
					newCornerLight[j] = color1;
				}
			}
			for (int j = 0; j < cell.numCorners; j++)
			{
				CompileFaceToNeighbor(j,ih,ir,cell,neighbors,newCornerLight,thisCell,range,empThis);
					
			}
			for (int j = 0; j < cell.numCorners; j++)
				cornerBrightness[j] = newCornerLight[j];
			wasEmpty = false;
		}
	}


	void	WaterCompiler::Compile(const index_t offset, const index_t end, THull<WaterVertex>&coverHull, SpinLock&lock)
	{

		//for (index_t i = offset; i < offset+BatchSize && i < end; i++ )
		//{
		//	const ::Grid::CellStack&stack = stacks[i];
		//	const auto&neighbors = gridCells[i].neighborhood;
		//	const auto&cell = netCells[i];



		//	UINT32 begin = stack.lowestOpenCell ? (UINT32)stack.lowestOpenCell - 1 : 0;
		//	begin = std::min(begin, CellStack::ToLayer(clipHeight(i)-1));
		//	//while (begin > 0 && clipped(i, begin))
		//		//begin--;
		//	UINT32 end = (UINT32)stack.highestOpaqueCell + 1;

		//	//TVec4<> cornerColor[6];

		//	for (int j = 0; j < cell.numCorners; j++)
		//	{
		//		UINT32 n = neighbors.neighborID[j];
		//		const ::Grid::CellStack&stack = stacks[n];
		//		UINT32 _begin = stack.lowestOpenCell ? (UINT32)stack.lowestOpenCell - 1 : 0;
		//		_begin = std::min(_begin, CellStack::ToLayer(clipHeight(n)-1));
		//		//while (_begin > 0 && clipped(n, _begin))
		//		//	_begin--;
		//		UINT32 _end = (UINT32)stack.highestOpaqueCell + 1;
		//		begin = std::min(begin, _begin);
		//		end = std::max(end, _end);

		//	}
		//	wasEmpty = true;

		//	const int clipH = clipHeight(i);
		//	end = std::min(end, CellStack::ToLayer(clipH));
		//	if (begin == 0)
		//		CompileBottomLayer(stack,clipH,cell,neighbors);
		//	for (UINT ih = begin; ih < end; ih++)
		//	{
		//		CompileLayer(ih,stack,clipH,cell,neighbors);
		//	}
		//}

		for (index_t i = offset; i < offset+BatchSize && i < end; i++ )
		{
			const ::Grid::CellStack&stack = stacks[i];
			const auto&cell = netCells[i];
			const bool empThis = !(cell.cornerFlags & empZonesUnlocked);
			if (!stack.HasWater())
			{
				if (CellStack::bottom.IsWater())
					CompileBottomLayer(stack, 100,netCells[i],gridCells[i].neighborhood,empThis);

				continue;
			}

			const auto&neighbors = gridCells[i].neighborhood;


			
			TIntRange<BYTE> waterRange = stack.waterRange;

			//waterRange.end = (BYTE)std::min<UINT>(waterRange.end, CellStack::ToLayer(clipHeight(i)));
			waterRange.start = (BYTE)std::min<UINT>(waterRange.start, CellStack::ToLayer(clipHeight(i)-1));

			for (int j = 0; j < cell.numCorners; j++)
			{
				UINT32 n = neighbors.neighborID[j];
				const ::Grid::CellStack&stack = stacks[n];
				TIntRange<BYTE> otherRange = stack.waterRange;
				//otherRange.end = (BYTE)std::min<UINT>(otherRange.end, CellStack::ToLayer(clipHeight(n)));
				otherRange.start = (BYTE)std::min<UINT>(otherRange.start, CellStack::ToLayer(clipHeight(n)-1));

				waterRange.Include(otherRange);
			}
			wasEmpty = true;
			//static const float waterOpacity = 0.6f;

			const int clipH = clipHeight(i);
			waterRange.end = (BYTE)std::min<UINT>(waterRange.end, CellStack::ToLayer(clipH));
			if (CellStack::bottom.IsWater())
				CompileBottomLayer(stack, clipH,cell,neighbors,empThis);
			for (UINT ih = waterRange.start; ih < waterRange.end; ih++)
			{
				CompileLayer(i,ih,stack, clipH,cell,neighbors,empThis);
			}
		}

		if (mesh.IsNotEmpty())
		{
			lock.lock();

			mesh.AppendTo(coverHull);

			lock.unlock();
		}
	}












	void		BakeResourceInstruction(BYTE family, ResourceDiscovery&vis, FClipHeight clipHeight,const Resource*resources, count_t numResources)
	{
	//	return;
		TFamily &fam = VisualTerrain::family[family];
		TFamily::TData&dat = fam.normal;

		const auto&grid = Grid::low.grid;

		dat.resources.Clear();
		CompileResources(dat.resources,  vis,grid.GetFamilyOffset(family), grid.CountFamilyMembers(family), clipHeight, resources, numResources);

		Box<> box = Box<>::Invalid();

		foreach(dat.resources.vertices, vtx)
			box.Include(vtx->vertex.xyz);
		dat.boundingBox = box;
		dat.boundingSphere.center = box.GetCenter();
		dat.boundingSphere.radius = 0.f;
		foreach(dat.resources.vertices, vtx)
			dat.boundingSphere.IncludeSquare(vtx->vertex.xyz);
		dat.boundingSphere.radius = sqrt(dat.boundingSphere.radius);


		if (dat.resources.instruction.shader.IsEmpty())
		{
			(DX11::BaseInstructionBlock&)dat.resources.instruction = Instruction::resourceBase;
		}

		dat.resources.instruction.vbo.CreateFromStructs(dat.resources.vertices);
		dat.resources.instruction.ibo.Create(dat.resources.indices);

		vis.familyChanged[family] = false;
	}

	//void		BakeLineInstruction(const Array<CellStack>&stacks, BYTE family, int minHeight)
	//{
	//	if (!section)
	//		minHeight = -CellStack::HalfRange;
	//	TFamily &fam = Grid::family[family];
	//	TFamily::TData&dat = fam.GetData(section);

	//	dat.edgeVertices.Clear();
	//	dat.edgeIndices.Clear();
	//	CompileEdges(stacks, dat.edgeVertices, dat.edgeIndices, grid.GetFamilyOffset(family), grid.CountFamilyMembers(family), minHeight);

	//	if (dat.edgeInstruction.shader.IsEmpty())
	//	{
	//		(DX11::BaseInstructionBlock&)dat.edgeInstruction = terrainEdgeBase;
	//		dat.edgeInstruction.SetLines();
	//	}


	//	dat.edgeInstruction.vbo.CreateFromStructs(dat.edgeVertices);
	//	dat.edgeInstruction.ibo.Create(dat.edgeIndices);
	//}


	void		RenderLowCell(index_t stackID)
	{
		display.Render(Instruction::lowGridInstruction, low.cells[stackID].voffset, low.cells[stackID].numCorners * 3);
	}
	/**
	Second-style version of RenderCell().
	Tran

	*/
	void		RenderTransparentLowCell(index_t stackID)
	{
		display.Render(Instruction::transparentGridInstruction, low.cells[stackID].voffset, low.cells[stackID].numCorners * 3);
	}

	//void RenderBlock(index_t cellID, float h0, float h1, const float4&color)
	//{
	//	TMatrix4<> config;
	//	memset(&config, 0, sizeof(config));
	//	config.x.x = h0;
	//	config.x.y = h1;
	//	config.z = color;
	//	Mat::transpose(config);
	//	blockConfig.Update(config);
	//	display.Render(blockInstruction, cells[stackID].voffset, cells[stackID].numCorners * 3);
	//}

	static const count_t HullTestBatch = 16;
	bool	TestHullIntersection(const TVec3<>&b, const TVec3<>&d, float&distance, TVec3<>&outNormal, bool includeWater)
	{
		bool rs = false;
		SpinLock lock;
		for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
		{
			const TFamily&fam = family[k];
			float dst = distance;
			const TFamily::TData&dat = fam.normal;
			if (_oDetectOpticalBoxIntersection(dat.boundingBox, b, d, dst))
			{
				if (includeWater)
				{
					const auto*indices = dat.water.indices.pointer();

					Concurrency::parallel_for(index_t(0), dat.water.indices.Count(), index_t(3) * HullTestBatch, [&lock, &distance, b, d, &rs, &dat, indices, &outNormal](index_t _i)
						//for (index_t i = 0; i < hullIndices.Count(); i += 3)
					{
						float dist = distance;
						bool any = false;
						TVec3<> normal;
						for (index_t i = _i; i < _i + HullTestBatch*3 && i < dat.water.indices.Count(); i+=3)
						{
							if (Vec::dot(dat.water.vertices[indices[i]].normal,d) > 0.f)
								continue;
							if (Obj::detectOpticalIntersection(
								dat.water.vertices[indices[i]].vertex.xyz,
								dat.water.vertices[indices[i + 1]].vertex.xyz,
								dat.water.vertices[indices[i + 2]].vertex.xyz,
								b, d, dist))
							{
								any = true;
								normal = dat.water.vertices[indices[i]].normal;	//correct enough
							}
						}
						if (any)
						{
							lock.lock();
							if (dist < distance)
							{
								rs = true;
								distance = dist;
								outNormal = normal;	//correct enough
							}
							lock.unlock();
						}

					});
				}

				{
					const auto*indices = dat.terrain.indices.pointer();

					Concurrency::parallel_for(index_t(0), dat.terrain.indices.Count(), index_t(3) * HullTestBatch, [&lock, &distance, b, d, &rs, &dat, indices, &outNormal](index_t _i)
						//for (index_t i = 0; i < hullIndices.Count(); i += 3)
					{
						float dist = distance;
						bool any = false;
						TVec3<> normal;
						for (index_t i = _i; i < _i + HullTestBatch*3 && i < dat.terrain.indices.Count(); i+=3)
						{
							if (Vec::dot(dat.terrain.vertices[indices[i]].normal_structure.xyz,d) > 0.f)
								continue;
							if (Obj::detectOpticalIntersection(
								dat.terrain.vertices[indices[i]].vertex.xyz,
								dat.terrain.vertices[indices[i + 1]].vertex.xyz,
								dat.terrain.vertices[indices[i + 2]].vertex.xyz,
								b, d, dist))
							{
								any = true;
								Vec::copy(dat.terrain.vertices[indices[i]].normal_structure.xyz,normal);	//correct enough
								Vec::div(normal,127.f);
							}
						}
						if (any)
						{
							lock.lock();
							if (dist < distance)
							{
								rs = true;
								distance = dist;
								outNormal = normal;	//correct enough
							}
							lock.unlock();
						}

					});
				}
			}
		}
		return rs;
	}

	index_t	TestLowGridIntersection(TVec3<> b, TVec3<> d, float radius, float&distance)
	{
		Vec::div(b,radius);
		Vec::div(d,radius);

		const auto&grid = low.grid;

		index_t rs = InvalidIndex;

		for (BYTE f = 0; f < BaseGrid::NumFamilies; f++)
		{
			float dist = distance;
			if (unitSphereFamilyBoundingBox[f].Contains(b) || _oDetectOpticalBoxIntersection(unitSphereFamilyBoundingBox[f],b,d,dist))
			{

				const index_t begin = grid.GetFamilyOffset(f);
				const count_t cnt = grid.CountFamilyMembers(f),
								end = begin+cnt;
				for (index_t i = begin; i < end; i++)
				{
					const auto&c = low.cells[i];
					for (int j = 0; j < c.numCorners; j++)
					{
						if (Obj::detectOpticalIntersection(c.centerVertex.xyz, c.cornerVertex[j].xyz,
							c.cornerVertex[(j + 1) % c.numCorners].xyz, b, d, distance))
							rs = i;
					}
				
				}
			}			
		}
/*
		for (index_t i = 0; i < grid.CellCount; i++)
		{
			const auto&c = cells[i];
			for (int j = 0; j < c.numCorners; j++)
			{
				if (Obj::detectOpticalIntersection(c.centerVertex.xyz, c.cornerVertex[j].xyz,
					c.cornerVertex[(j + 1) % c.numCorners].xyz, b, d, distance))
					rs = i;
			}

		}*/
		return rs;
	}

	void RenderLowGrid(float radius)
	{
		TMatrix4<> matrix = Matrix<>::eye4;
		matrix.x.xyz *= radius;
		matrix.y.xyz *= radius;
		matrix.z.xyz *= radius;

		display.SetObjectMatrix(matrix);
		display.Render(Instruction::lowGridInstruction);
	}




	namespace SeeThrough
	{
		static HighSetup::TDistanceField	currentCutoffDomain;
		//static int						currentLowerCutoff = 0;
		static Grid::LCellIdI			currentFocus(0xFFFFFFFF,0);
		//static UINT32					currentLowerCutoffCenter = 0xFFFFFFFF;
		static bool						cutoffMode = false;


		const Grid::LCellIdI&	GetFocusPoint()
		{
			return currentFocus;
		}

		int ClipHeight(index_t stackID)
		{
			//int d = (int)std::min(SeeThrough::currentCutoffDomain.distance[stackID], 0x1FFFFFFFU);
			//return  d < int(CutoffRadius) && /*std::max(d-4,0) +*/ SeeThrough::currentFocus.height < intRadius;
			return  //currentFocus.height < intRadius &&
				currentCutoffDomain.distance[stackID] < CutoffRadius ? currentFocus.height + 1 : NoClipHeight;
		}

		Grid::LCellIdI		LocateCamera()
		{
			UINT32 cell = low.Locate(::Camera::scene.location);
			#ifdef HYBRID_GRID
				UINT32 finer = low.grid.GetCell(cell).higherEquivalent;
			#else
				UINT32 finer = cell;
			#endif
			if (currentCutoffDomain.distance[finer] < CutoffRadius * 3 / 4)
				return currentFocus;
			return Grid::LCellIdI(cell,currentFocus.height);
		}

		bool	IsEnabled()
		{
			return cutoffMode;
		}

		void	Update(bool enable, const Grid::LCellIdI&targetFocus)
		{
			if (targetFocus.stackID >= LowSetup::GridType::CellCount)
				return;	// uninitialized focus
			//ASSERT_LESS__(targetFocus.stackID, LowSetup::GridType::CellCount);
			bool newHeight = currentFocus.height != targetFocus.height;
			if (!newHeight && currentFocus.stackID == targetFocus.stackID && enable == cutoffMode)
				return;
			#ifdef HYBRID_GRID
				UINT32 finer = low.grid.GetCell(targetFocus.stackID).higherEquivalent;
			#else
				UINT32 finer = targetFocus.stackID;
			#endif

			if (enable)
			{
				::Camera::ChangeGroundRadius(ToRadius(targetFocus.height));
			}

			HighSetup::TDistanceField	newDomain;
			if (enable)
				high.UpdatePlanarDistanceField(finer, CutoffRadius, newDomain);

			for (index_t i = 0; i < HighSetup::GridType::CellCount; i++)
			{
				if ((newDomain.distance[i] != 0xFFFFFFFF && newHeight) || (newDomain.distance[i] == 0xFFFFFFFF) != (currentCutoffDomain.distance[i] == 0xFFFFFFFF))
				{
					BYTE fam = high.grid.GetCell(i).family;
					World::actual.SignalFamilyStructureChange(fam);
				}
			}

			currentCutoffDomain = newDomain;
			cutoffMode = enable;
			currentFocus = targetFocus;
		}

	
	}


	void	ResourceCompiler::Compile(const index_t offset, const index_t end, THull<ResourceVertex>&coverHull, SpinLock&lock)
	{
		for (index_t i = offset; i < offset+BatchSize && i < end; i++ )
		{
			//const ::Grid::CellStack&stack = stacks[i];
			if (!this->visibility->resourcesDiscovered[i])	//
				continue;	//nothing known

			#ifndef _DEBUG
				if (!(lowGridCells[i].cornerFlags & empZonesUnlocked))
					continue;
			#endif

			#ifdef HYBRID_GRID
				const UINT32 fine = lowGridCells[i].higherEquivalent;
			#else
				const UINT32 fine = (UINT32)i;
			#endif
			//const auto&n = highGridCells[fine].neighborhood;
			const auto&fineStack = stacks[fine];
			UINT layer = fineStack.highestOpaqueCell;
			if (fineStack.waterRange.IsNotEmpty())
				layer = std::max<UINT>(fineStack.waterRange.end-1, layer);
			const int clipH = clipHeight(fine);
			const UINT clipL = CellStack::ToLayer(clipH);
			layer = std::min(layer, clipL-1);
			while (layer > 0 && fineStack.volumeCell[layer].content == Content::Clear)
				layer--;
			bool structureCenter = false;
			while (layer+1 < CellStack::Range && (fineStack.volumeCell[layer+1].structureCounter > 0 /*|| fineStack.volumeCell[layer].structureCounter > 0*/))
			{
				layer++;
				structureCenter = true;
			}

			if (layer != 0)
				layer++;
			else
				if (CellStack::bottom.content == Content::Clear || CellStack::bottom.content == Content::Lava)
					continue;
		/*	else
				if (CellStack::bottom.content == Content::Clear)
					continue;*/

			float add = 0.1f;
			//if (fineStack.volumeCell[layer].structureCounter > 0)
				//add += 0.5f;

			float h;
			const auto&cell = lowNetCells[i];
			TMatrix4<>	system;


			h =  ToRadiusf( float(CellStack::ToHeight(layer))+add);
			const TVec3<>&center = structureCenter? cell.cornerCenter : cell.centerVertex.xyz;
			Mat::makeAxisSystem(center * h,center,2,system);
			float r = cell.minCornerDistance * h / 4.f * 1.5f;	//dark coat

			int total = 0;
			for (index_t ri = 0; ri < this->numResources; ri++)
				total += this->resources[ri].coverage[i];
			if (total)
			{
				{
					float level = float(total);

					float3 color (-0.1f);

					float h2 = h + SingleLayerHeight * (level*0.25f + 0.05f);

					mesh.SetOffset();
					mesh << ResourceVertex(center,h,center,float4(color,level+0.8f),1.f);
					for (int j = 0; j < Resolution; j ++)
					{
						float a = float(j) / float(Resolution) * 2.f * M_PIF;
				
						TVec3<> p;
						Mat::transform(system,float3(cos(a)*r,sin(a)*r,0.f),p);
						//Vec::center(cell.cornerVertex[j].xyz,cell.centerVertex.xyz,p);
						Vec::normalize(p);
						mesh << ResourceVertex(float4(p,1.f),h,p,float4(color,level),1.f);
						mesh << 0 << 1 + j << 1 + (j+1)%Resolution;
					}

					if (h2 > h)
					{
						mesh.SetOffset();
						for (int j = 0; j < Resolution; j ++)
						{
							float a = float(j) / float(Resolution) * 2.f * M_PIF;
							TVec3<> p;
							Mat::transform(system,float3(cos(a)*r,sin(a)*r,0.f),p);
							//Vec::center(cell.cornerVertex[j].xyz,cell.centerVertex.xyz,p);
							Vec::normalize(p);
							float3 n = (p - cell.centerVertex.xyz).Normalized();
							mesh << ResourceVertex(float4(p,1.f),h,n,float4(color,0.f),1.f);
							mesh << ResourceVertex(float4(p,1.f),h2,n,float4(color,level),1.f);


							mesh.InvQuad(j * 2 + 1, j * 2, ((j+1)%Resolution)*2, ((j+1)%Resolution)*2+1);
						}
					}
				}

				r = cell.minCornerDistance * h / 4.f; 
				h =  ToRadiusf( float(CellStack::ToHeight(layer))+add);
				for (index_t ri = 0; ri < this->numResources; ri++)
				{
					const Resource&res = this->resources[ri];
					if (!res.coverage[i])
						continue;
					float level = float(res.coverage[i]);

					float3 color = GetColor(level/20.f,res.GetColor());

					float h2 = h + SingleLayerHeight * (level*0.25f);

					mesh.SetOffset();
					mesh << ResourceVertex(center,h2,center,float4(color,level+0.8f),1.f);
					for (int j = 0; j < Resolution; j ++)
					{
						float a = float(j) / float(Resolution) * 2.f * M_PIF;
				
						TVec3<> p;
						Mat::transform(system,float3(cos(a)*r,sin(a)*r,0.f),p);
						//Vec::center(cell.cornerVertex[j].xyz,cell.centerVertex.xyz,p);
						Vec::normalize(p);
						mesh << ResourceVertex(float4(p,1.f),h2,p,float4(color,level),1.f);
						mesh << 0 << 1 + j << 1 + (j+1)%Resolution;
					}

					if (h2 > h)
					{
						mesh.SetOffset();
						for (int j = 0; j < Resolution; j ++)
						{
							float a = float(j) / float(Resolution) * 2.f * M_PIF;
							TVec3<> p;
							Mat::transform(system,float3(cos(a)*r,sin(a)*r,0.f),p);
							//Vec::center(cell.cornerVertex[j].xyz,cell.centerVertex.xyz,p);
							Vec::normalize(p);
							float3 n = (p - cell.centerVertex.xyz).Normalized();
							mesh << ResourceVertex(float4(p,1.f),h,n,float4(color,0.f),1.f);
							mesh << ResourceVertex(float4(p,1.f),h2,n,float4(color,level),1.f);


							mesh.Quad(j * 2 + 1, j * 2, ((j+1)%Resolution)*2, ((j+1)%Resolution)*2+1);
						}
					}

					h = h2;
				}
			}
			else
			//if (!anyThere)
			{
				r = cell.minCornerDistance * h / 4.f;
				float level = 0.1f;

				float3 color (0.2f);

				float h2 = h + SingleLayerHeight * (level*0.25f);

				mesh.SetOffset();
				mesh << ResourceVertex(center,h2,center,float4(color,level+0.8f),1.f);
				for (int j = 0; j < Resolution; j ++)
				{
					float a = float(j) / float(Resolution) * 2.f * M_PIF;
				
					TVec3<> p;
					Mat::transform(system,float3(cos(a)*r,sin(a)*r,0.f),p);
					//Vec::center(cell.cornerVertex[j].xyz,cell.centerVertex.xyz,p);
					Vec::normalize(p);
					mesh << ResourceVertex(float4(p,1.f),h2,p,float4(color,level),1.f);
					mesh << 0 << 1 + j << 1 + (j+1)%Resolution;
				}

				//if (h2 > h)
				{
					mesh.SetOffset();
					for (int j = 0; j < Resolution; j ++)
					{
						float a = float(j) / float(Resolution) * 2.f * M_PIF;
						TVec3<> p;
						Mat::transform(system,float3(cos(a)*r,sin(a)*r,0.f),p);
						//Vec::center(cell.cornerVertex[j].xyz,cell.centerVertex.xyz,p);
						Vec::normalize(p);
						float3 n = (p - cell.centerVertex.xyz).Normalized();
						mesh << ResourceVertex(float4(p,1.f),h,n,float4(color,0.f),1.f);
						mesh << ResourceVertex(float4(p,1.f),h2,n,float4(color,level),1.f);


						mesh.Quad(j * 2 + 1, j * 2, ((j+1)%Resolution)*2, ((j+1)%Resolution)*2+1);
					}
				}

			}

		}

		if (mesh.IsNotEmpty())
		{
			lock.lock();

			mesh.AppendTo(coverHull);

			lock.unlock();
		}

	}
	
	

	void		SetSphereDisplay(const TVec3<>&center, float radius, const float4&color)
	{
		Instruction::displayMode = Instruction::SphereColored;
		Instruction::SphereDef.Update(float4(center,radius));
		Instruction::SphereColor.Update(color);
	}

	void		SetParaboloidDisplay(const TVec3<>&direction, float height, float flatRadius, const float4&_color)
	{
		Instruction::displayMode = Instruction::ParaboloidColored;
		Instruction::ParaboloidDirection_Height.Update(float4(direction,height));
		float4 color_r = float4(_color.rgb, flatRadius);

		if (Instruction::lastColor != color_r)
		{
			//LogEvent(EventSignificance::Debug,nullptr,"change2 detected");
			Instruction::lastColor = color_r;
			Instruction::ParaboloidColor_FlatRadius.Update(color_r);
		}
	}

	void		UnsetParaboloidAndSphereDisplay()
	{
		Instruction::displayMode = Instruction::NormalRendering;
	}



	void	FlushAllRendererData()
	{
		ResetSunRotation();
			
		#ifndef NO_SATELLITES
			visibleOrbitGrid.Clear();
		#endif
		UnsetParaboloidAndSphereDisplay();

		#ifdef DEBUG_SENSORY
			sensors.Clear();
		#endif
	}



	void	OrbitGrid::Render(const Engine::Frustum<>&frustum)
	{
		if (!valid)
			ForceRecompileNow();
		display.StoreRendererState();
			display.SetBlendMode(Engine::BlendMode::AlphaBlend);
			display.SetDepthTest(true,false);
			display.SetRasterizer(true,true);
			display.Render(instruction,true);
		display.RestoreRendererState();
	}


	void	OrbitGrid::ForceRecompileNow()
	{
		valid = true;

		const count_t numVertices = low.cellVertices.Count();
		vertices.SetSize(numVertices);
		Concurrency::parallel_for(index_t(0),numVertices,[this](index_t i)
		{
			Vertex&out = vertices[i];
			const BaseGrid::TCellVertex&in = low.cellVertices[i];
			
			float opacity = cells[in.owner[0]].visible ? 1.f : 0.f;
			float emp = !(low.grid.GetCell(in.owner[0]).cornerFlags & empZonesUnlocked) ? 1.f : 0.f;
			for (BYTE k = 1; k < in.ownerCount; k++)
			{
				opacity += cells[in.owner[k]].visible ? 1.f : 0.f;
				emp += !(low.grid.GetCell(in.owner[k]).cornerFlags & empZonesUnlocked) ? 1.f : 0.f;
			}

			opacity /= in.ownerCount;
			emp /= in.ownerCount;
			
			out.Set(in.centerDirection,in.ownerCount>1 ? 1.f : 0.f,orbit,opacity,emp);
		});

		((Engine::DX11::BaseInstructionBlock&)instruction) = Instruction::orbitGridBase;
		instruction.ibo = Instruction::lowGridInstruction.ibo;
		instruction.vbo.CreateFromStructs(vertices);
	}



	
	void		OrbitGrid::ChangeSensory(const LCellIdI&center, float radius, signed char change)
	{
		Timer::Time t0 = timer.Now();
		LogEvent(EventSignificance::Debug,nullptr,"Processing orbital sensor change around center "+center.ToString()+", radius="+String(radius)+", change="+String(change));

		Math::Box<> test;
		const TVec3<> worldCenter = ToWorld(center);
		test.SetCenter(worldCenter,radius);
		float r2 = radius*radius;
		Concurrency::parallel_for(index_t(0),LowSetup::GridType::CellCount,[this,worldCenter,r2,change](index_t i)
		{
			//bool anyChanged = false;
			bool anyVisible = false;
			UINT32 cell((UINT32)i);

			bool wasIn = false;
			TVec3<> localCenter = low.cells[i].centerVertex.xyz * orbit;
			float dist2 = Vec::quadraticDistance(localCenter,worldCenter);
			if (dist2 <= r2)
			{
				cells[i].visible += change;
				DBG_ASSERT_GREATER_OR_EQUAL__(cells[i].visible,0);

				//DBG_ASSERT_LESS_OR_EQUAL__(cells[i].visible,1);	//enable to debug in scenarios where sensors do not overlap
				valid = false;
			}
		});

		LogEvent(EventSignificance::PerformanceMeasurement,nullptr,"Orbital sensory change processed in "+String(timer.GetSecondsSince(t0))+". Used orbit was "+String(orbit));
		//ShowMessage(timer.GetSecondsSince(t0));
	}



	void RebuildOutdatedFamilies()
	{

		BYTE familiesRebuilt = 0;
		Timer::Time t0 = timer.Now();


		for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
		{
			{
				TFamily::TData&dat = family[k].normal;
				if (dat.outdated)
				{
	
					dat.outdated = false;

					#ifdef VOXEL_SCANNER
						Grid::RemapToLowStacks(k);
					#endif

					familiesRebuilt++;
					if (SeeThrough::IsEnabled())
					{
						BakeBlockInstruction(World::actual.cellStack, k, SeeThrough::ClipHeight);
					}
					else
					{
						BakeBlockInstruction(World::actual.cellStack, k, Grid::Clip::NoClip);
					}
				}
				if (dat.resourceOutdated)
				{
					dat.resourceOutdated = false;
					if (SeeThrough::IsEnabled())
					{
						BakeResourceInstruction(k,Grid::ResourceDiscovery::client, SeeThrough::ClipHeight,Grid::Resource::all.pointer(),Grid::Resource::all.Count());
					}
					else
					{
						BakeResourceInstruction(k, Grid::ResourceDiscovery::client, Grid::Clip::NoClip,Grid::Resource::all.pointer(),Grid::Resource::all.Count());
					}
				}
			}
		}
		if (familiesRebuilt > 0)
		{
			float seconds = timer.GetSecondsSince(t0);
			LogEvent(EventSignificance::PerformanceMeasurement, nullptr, "Rebuilt "+String(familiesRebuilt)+" families in "+String(seconds));
		}

	}

	bool FamilyIsOutdated(index_t f)
	{
		return family[f].normal.outdated;
	}

	void	RenderResources(const Engine::Frustum<>&frustum)
	{
		display.SetBlendMode(Engine::BlendMode::AlphaBlend);
	//	display.SetBlendMode(Engine::BlendMode::FlareBlend);
		for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
		{
			TFamily::TData&dat = family[k].normal;
			if (frustum.IsVisible(dat.boundingSphere))
				display.Render(dat.resources.instruction);
		}
	}

	void	RenderOverlay(const Engine::Frustum<>&frustum)
	{
		for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
		{
			TFamily::TData&dat = family[k].normal;
			if (frustum.IsVisible(dat.boundingSphere))
				display.Render(dat.overlay.instruction);
		}
	}

	void	RenderWater(const Engine::Frustum<>&frustum)
	{
		for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
		{
			TFamily::TData&dat = family[k].normal;
			if (frustum.IsVisible(dat.boundingSphere))
				display.Render(dat.water.instruction[Instruction::displayMode],true);
		}
	}

	void	RenderOpaqueTerrain(const Engine::Frustum<>&frustum)
	{
		for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
		{
			TFamily::TData&dat = family[k].normal;
			if (frustum.IsVisible(dat.boundingSphere))
				display.Render(dat.terrain.instruction[Instruction::displayMode],true);
		}
	}

	void	RenderShadows()
	{
		for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
		{
			TFamily::TData&dat = family[k].normal;
			if (dat.terrain.shadowInstruction.shader.IsSet())
				display.Render(dat.terrain.shadowInstruction);
			if (dat.overlay.shadowInstruction.shader.IsSet())
				display.Render(dat.overlay.shadowInstruction);
		}
	}
}

#endif
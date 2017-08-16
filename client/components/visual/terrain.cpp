#ifdef HAS_WINDOW
#include <global_root.h>
#include "terrain.h"
#include "objectRendering.h"
#include <grid/planet.h> 
#include <grid/worldData.h> 
#include <grid/resources.h>
#include "../cameraControl.h"

#include "../../ExtRenderer/DirectX11.h"
#include <engine/display.h>
#include <math/vclasses.h>
#include "terrainInstructions.h"
#include "lighting.h"
#include "../clientEvents.h"
#include "../gridControl.h"

#include "terrainCompilers/solidTerrain.h"
#include "terrainCompilers/water.h"
#include "terrainCompilers/resourceVisualization.h"
	
extern Engine::Display<Engine::DirectX11>	display;
//namespace Camera
//{
//	extern Engine::VectorCamera<>	scene;
//	void  ChangeGroundRadius(float);
//}






namespace VisualTerrain
{
	namespace DX11 = Engine::DX11;
	using namespace Grid;

	Box<>		unitSphereFamilyBoundingBox[BaseGrid::NumFamilies];
	Box<>		planetFamilyBoundingBox[BaseGrid::NumFamilies];



	void		BakeBlockInstruction(const Array<CellStack>&stacks, BYTE family, FClipHeight clipHeight);




	namespace Instruction
	{	
		float4	lastColor;
		displayMode_t				displayMode = NormalRendering;
		ShadedInstruction			orbitGridBase,
									terrainBase, terrainSphereBase, terrainParaboloidBase,
									waterBase, waterSphereBase, waterParaboloidBase;
		DX11::BaseInstructionBlock	terrainShadowBase, 
									overlayBase, 
									overlayShadowBase, resourceBase;// , terrainEdgeBase;

		DX11::ObjectBuffer<float4>	ParaboloidDirection_Height,SphereDef,SphereColor;
		DX11::ObjectBuffer<float4>	ParaboloidColor_FlatRadius;


		DX11::ObjectBuffer<THeightMapOrientation>	heightMapOrientation;	//used for lava
		DX11::Texture								cubeHeightMap;	//used for lava

		DX11::Texture				lineTex;
		DX11::Shader				gridShader,transparentGridShader;// , blockShader;
		DX11::InstructionBlock32	lowGridInstruction,
									transparentGridInstruction;

	}

	
	void	UpdatePlanetRenderingParameters(float radius)	//more to come
	{
		TMatrix4<> matrix = Matrix<>::eye4;
		matrix.x.xyz *= radius;
		matrix.y.xyz *= radius;
		matrix.z.xyz *= radius;

		display.SetObjectMatrix(matrix);
	}

	static void	SetOrientation(TMatrix4<>&m, const float3&axis, double t, const float3&seedAxis, double seedRot)
	{
		TMatrix3<> sys1,sys2,sys;
		Mat::BuildRotationMatrix(t*1.5,axis,sys1);
		Mat::BuildRotationMatrix(seedRot,seedAxis,sys2);
		Mat::Mult(sys1,sys2,sys);
		Mat::copyOrientation(sys,m);
		m.w.xyz = M::Vector3<>::zero;
		Mat::resetBottomRow(m);

	}



	void	UpdateLavaOrientation()
	{
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
		GlobalEvents::frameBegin << VisualTerrain::UpdateLavaOrientation;
		GlobalEvents::frameAfterCamera << []()
		{
			//if (SeeThrough::IsEnabled())
			SeeThrough::Update(SeeThrough::IsEnabled(), SeeThrough::LocateCamera());
		};


		GlobalEvents::terrainFlush << UnsetParaboloidAndSphereDisplay;
		GlobalEvents::objectFlush << UnsetParaboloidAndSphereDisplay;
		#ifndef NO_SATELLITES
			GlobalEvents::objectFlush << [](){visibileOrbitGrid.Clear();}
		#endif


		Buffer0<DX11::Shader::Binding> bindings;










		

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
			BakeBlockInstruction(World::actual.GetStacks(), i, Clip::NoClip);





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
			binding.vertex.Set(0, 4);
			binding.normal.Unset();
			binding.floats_per_vertex = 4;
			Instruction::lowGridInstruction.binding = binding;
		}
		Instruction::lowGridInstruction.UpdateLayout();
		Instruction::lowGridInstruction.SetTriangles();


		Instruction::transparentGridInstruction = Instruction::lowGridInstruction;
		Instruction::transparentGridInstruction.shader = Instruction::transparentGridShader;
		Instruction::transparentGridInstruction.UpdateLayout();












		//bindings;
		
		bindings.Append(DX11::Shader::Binding(Lighting::GPUResources::sunDirection,"Sun"));
		bindings.Append(DX11::Shader::Binding(Lighting::GPUResources::sunColor,"SunColor"));
		#ifdef CASCADING_SHADOW
			for (index_t i = 0; i < Lighting::NumCascades; i++)
			{
				ASSERT__(Lighting::GPUResources::shadowMatrix[i].IsSet());
				bindings.Append(DX11::Shader::Binding(Lighting::GPUResources::shadowMatrix[i],"ShadowMatrix"+String(i)));
				bindings.Append(DX11::Shader::Binding(Lighting::GPUResources::shadowMap[i].GetDepthStencilTarget(), "ShadowMap"+String(i)));
			}
		#else
			bindings.Append(DX11::Shader::Binding(Objects::shadowMatrix,"ShadowMatrix"));
			bindings.Append(DX11::Shader::Binding(Objects::shadowMap.GetDepthStencilTarget(), "ShadowMap"));
		#endif
		bindings.Append(DX11::Shader::Binding(Lighting::GPUResources::atmosphereConfig, "AtmosphereConfig"));
		bindings.Append(DX11::Shader::SamplerBinding(true, Engine::TextureFilter::None, 1, "ShadowSampler"));

		bindings.Append(DX11::Shader::SamplerBinding(true,Engine::TextureFilter::Trilinear,1,"HeightMapSampler"));
		bindings.Append(DX11::Shader::Binding(Instruction::cubeHeightMap,"HeightMap"));
		bindings.Append(DX11::Shader::Binding(Instruction::heightMapOrientation,"HeightMapOrientation"));
		
		Instruction::terrainBase.LoadQualitiesFromFile("shaders/gridTerrain.hlsl", "vsMain", "psMain", bindings.pointer(), bindings.Count());

		

		{
			//Engine::VertexBinding binding;
			//binding.vertex.Set(0, 4);
			//binding.normal.Set(4, 4);
			//binding.color.Set(8, 4);
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
			binding.vertex.Set(0, 4);
			binding.normal.Set(4, 4);
			binding.color.Set(8,1);
			binding.floats_per_vertex = 9;

			Instruction::orbitGridBase.binding = binding;
			Instruction::orbitGridBase.SetTriangles();
			Instruction::orbitGridBase.LoadQualitiesFromFile("shaders/orbitalGrid.hlsl", "vsMain", "psMain");
			Instruction::orbitGridBase.UpdateLayout();
		}
	
		{
			ShadedInstruction&base = Instruction::waterBase;
			base.LoadQualitiesFromFile("shaders/gridTerrainWater.hlsl", "vsMain", "psMain", bindings.pointer(), bindings.Count());

			{
				/*
			float4		vertex;
			float3		normal;
			float		light;	//water derives color from shader variable
				*/
				Engine::VertexBinding binding;
				binding.vertex.Set(0, 4);
				binding.normal.Set(4, 3);
				binding.color.Set(7, 1);
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
			binding.vertex.Set(0, 3);
			binding.normal.Unset();
			binding.color.Set(3, 1);
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
				binding.vertex.Set(0, 4);
				binding.normal.Set(4, 4);
				binding.color.Set(8, 4);
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
		Instruction::terrainSphereBase.LoadQualitiesFromFile("shaders/gridTerrain.hlsl", "vsMain", "psMain", bindings.pointer(), bindings.Count());
		DX11::Shader::macroDefinitions.Clear();
		Instruction::terrainSphereBase.UpdateLayout();
		bindings.EraseLast();
		bindings.EraseLast();


		DX11::Shader::RedefineMacro(DX11::Shader::Macro("PARABOLOID_DISPLAY","1"));
		bindings.Append(DX11::Shader::Binding(Instruction::ParaboloidDirection_Height,"ParaboloidDirection_Height"));
		bindings.Append(DX11::Shader::Binding(Instruction::ParaboloidColor_FlatRadius,"ParaboloidColor_FlatRadius"));
		Instruction::terrainParaboloidBase.LoadQualitiesFromFile("shaders/gridTerrain.hlsl", "vsMain", "psMain", bindings.pointer(), bindings.Count());
		DX11::Shader::macroDefinitions.Clear();
		Instruction::terrainParaboloidBase.UpdateLayout();
		bindings.EraseLast();
		bindings.EraseLast();



		DX11::Shader::RedefineMacro(DX11::Shader::Macro("SPHERE_DISPLAY","1"));
		bindings.Append(DX11::Shader::Binding(Instruction::SphereDef,"SphereDef"));
		bindings.Append(DX11::Shader::Binding(Instruction::SphereColor,"SphereColor"));
		Instruction::waterSphereBase.LoadQualitiesFromFile("shaders/gridTerrainWater.hlsl", "vsMain", "psMain", bindings.pointer(), bindings.Count());
		DX11::Shader::macroDefinitions.Clear();
		Instruction::waterSphereBase.UpdateLayout();
		bindings.EraseLast();
		bindings.EraseLast();


		DX11::Shader::RedefineMacro(DX11::Shader::Macro("PARABOLOID_DISPLAY","1"));
		bindings.Append(DX11::Shader::Binding(Instruction::ParaboloidDirection_Height,"ParaboloidDirection_Height"));
		bindings.Append(DX11::Shader::Binding(Instruction::ParaboloidColor_FlatRadius,"ParaboloidColor_FlatRadius"));
		Instruction::waterParaboloidBase.LoadQualitiesFromFile("shaders/gridTerrainWater.hlsl", "vsMain", "psMain", bindings.pointer(), bindings.Count());
		DX11::Shader::macroDefinitions.Clear();
		Instruction::waterParaboloidBase.UpdateLayout();
		bindings.EraseLast();
		bindings.EraseLast();









	}










	/**
	Homogenous geometrical section of the planet surface, to be rendered and updated as one chunk
	*/
	struct TFamily
	{
		struct TBounds
		{
			Box<>						boundingBox;
			Sphere<>					boundingSphere;

		};

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
			bool						outdated,resourceOutdated;

			TBounds						terrainBounds,resourceBounds;

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

		void							Destroy()
		{
			//	lower.Destroy();
			normal.Destroy();
		}


		void							SetOutdated(bool geometry, bool resources)
		{
			if (geometry)
			{
				normal.outdated = true;
				normal.resourceOutdated = true;	//implied
			}
			if (resources)
				normal.resourceOutdated = true;
		}


	};
	TFamily						family[BaseGrid::NumFamilies];


	void	Destroy()
	{
		using namespace Instruction;
		sunDirection.Clear();

		lineTex.Clear();
		gridShader.Clear();
		transparentGridShader.Clear();
		overlayBase.Clear();
		overlayShadowBase.Clear();
		resourceBase.Clear();
		terrainBase.Clear();
		terrainSphereBase.Clear();
		terrainParaboloidBase.Clear();
		orbitGridBase.Clear();
		terrainShadowBase.Clear();
		waterBase.Clear();
		waterSphereBase.Clear();
		waterParaboloidBase.Clear();
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
		const index_t offset = Grid::high.grid.GetFamilyOffset(family);
		const count_t members = Grid::high.grid.CountFamilyMembers(family);
		CompileSolidTerrain(stacks, dat.terrain, dat.overlay, offset, members, clipHeight);
		CompileWater(stacks, dat.water, offset, members, clipHeight);

		Box<> box = Box<>::Invalid();

		foreach(dat.terrain.vertices, vtx)
			box.Include(vtx->vertex.xyz);
		foreach(dat.water.vertices, vtx)
			box.Include(vtx->vertex.xyz);
		dat.terrainBounds.boundingBox = box;
		dat.terrainBounds.boundingSphere.center = box.GetCenter();
		dat.terrainBounds.boundingSphere.radius = 0.f;
		foreach(dat.terrain.vertices, vtx)
			dat.terrainBounds.boundingSphere.IncludeSquare(vtx->vertex.xyz);
		dat.terrainBounds.boundingSphere.radius = sqrt(dat.terrainBounds.boundingSphere.radius);


		if (dat.terrain.instruction[0].shader.IsEmpty())
		{
			(DX11::BaseInstructionBlock&)dat.terrain.shadowInstruction = Instruction::terrainShadowBase;
			dat.terrain.instruction[0] = Instruction::terrainBase;
			dat.terrain.instruction[1] = Instruction::terrainParaboloidBase;
			dat.terrain.instruction[2] = Instruction::terrainSphereBase;

			(DX11::BaseInstructionBlock&)dat.overlay.instruction = Instruction::overlayBase;
			(DX11::BaseInstructionBlock&)dat.overlay.shadowInstruction = Instruction::overlayShadowBase;
			dat.water.instruction[0] = Instruction::waterBase;
			dat.water.instruction[1] = Instruction::waterParaboloidBase;
			dat.water.instruction[2] = Instruction::waterSphereBase;
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
		dat.resourceBounds.boundingBox = box;
		dat.resourceBounds.boundingSphere.center = box.GetCenter();
		dat.resourceBounds.boundingSphere.radius = 0.f;
		foreach(dat.resources.vertices, vtx)
			dat.resourceBounds.boundingSphere.IncludeSquare(vtx->vertex.xyz);
		dat.resourceBounds.boundingSphere.radius = sqrt(dat.resourceBounds.boundingSphere.radius);


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
		Sys::SpinLock lock;
		for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
		{
			const TFamily&fam = family[k];
			float dst = distance;
			const TFamily::TData&dat = fam.normal;
			if (_oDetectOpticalBoxIntersection(dat.terrainBounds.boundingBox, b, d, dst))
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
			else
				::Camera::ChangeGroundRadiusToFocusedTerrain();

			HighSetup::TDistanceField	newDomain;
			if (enable)
				high.UpdatePlanarDistanceField(finer, CutoffRadius, newDomain);

			for (index_t i = 0; i < HighSetup::GridType::CellCount; i++)
			{
				if ((newDomain.distance[i] != 0xFFFFFFFF && newHeight) || (newDomain.distance[i] == 0xFFFFFFFF) != (currentCutoffDomain.distance[i] == 0xFFFFFFFF))
				{
					BaseGrid::family_t fam = high.grid.GetCell(i).family;
					VisualTerrain::family[fam].SetOutdated(true,false);
				}
			}

			currentCutoffDomain = newDomain;
			cutoffMode = enable;
			currentFocus = targetFocus;
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

		instruction = Instruction::orbitGridBase;
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
		Concurrency::parallel_for(UINT32(0),LowSetup::GridType::CellCount,[this,worldCenter,r2,change](UINT32 i)
		{
			//bool anyChanged = false;
			bool anyVisible = false;

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

		ClientGrid::CheckSensory();


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
						BakeBlockInstruction(World::actual.GetStacks(), k, SeeThrough::ClipHeight);
					}
					else
					{
						BakeBlockInstruction(World::actual.GetStacks(), k, Grid::Clip::NoClip);
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

	void	InvalidateFamily(BaseGrid::family_t f,bool geometry/*=true*/, bool resources/*=true*/)
	{
		family[f].SetOutdated(geometry,resources);
	}

	void	Invalidate(bool geometry/*=true*/, bool resources/*=true*/)
	{
		for (BaseGrid::family_t i = 0; i < BaseGrid::NumFamilies; i++)
			InvalidateFamily(i,geometry,resources);
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
			if (frustum.IsVisible(dat.resourceBounds.boundingSphere))
				display.Render(dat.resources.instruction);
		}
	}

	void	RenderOverlay(const Engine::Frustum<>&frustum)
	{
		for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
		{
			TFamily::TData&dat = family[k].normal;
			if (frustum.IsVisible(dat.terrainBounds.boundingSphere))
				display.Render(dat.overlay.instruction);
		}
	}

	void	RenderWater(const Engine::Frustum<>&frustum)
	{
		for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
		{
			TFamily::TData&dat = family[k].normal;
			if (frustum.IsVisible(dat.terrainBounds.boundingSphere))
				display.Render(dat.water.instruction[Instruction::displayMode],true);
		}
	}

	void	RenderOpaqueTerrain(const Engine::Frustum<>&frustum)
	{
		for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
		{
			TFamily::TData&dat = family[k].normal;
			if (frustum.IsVisible(dat.terrainBounds.boundingSphere))
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
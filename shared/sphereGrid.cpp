#include <global_root.h>
#include "sphereGrid.h"
#ifdef HAS_WINDOW
	#include <engine/display.h>
#endif

#include "noise.h"
#include "objects.h"
#include "logEvent.h"
#include "sphereGridHelper.h"


#ifdef HAS_WINDOW
	extern Engine::Display<Engine::DirectX11>	display;
	namespace Camera
	{
		extern Engine::VectorCamera<>	scene;
		void  TargetOrbit(float);
	}

#endif




namespace Grid
{
	
	UINT16				empZonesLocked = 0,
						empZonesUnlocked = 0xFFFF;

	//static Resource::coverage_t	GetResourceCoverage(const TVec3<>&seed, const TVec3<>&center0,float rarity,Resource::coverage_t maxYield, float probabilityModifier);
	#ifndef NO_MUNITION
		/*static*/ float			Resource::probability[Objects::Resource::NumberOfPossibilities] = {0.08f,0.08f,0,0,0.008f } ;
		/*static*/ Resource::coverage_t		Resource::maximum[Objects::Resource::NumberOfPossibilities] = {12,12,0,0,6};
	#else
		/*static*/ float			Resource::probability[Objects::Resource::NumberOfPossibilities] = {0.08f,0.08f,0,0.008f } ;
		/*static*/ Resource::coverage_t		Resource::maximum[Objects::Resource::NumberOfPossibilities] = {12,12,0,6};
	#endif


	/*static*/ Buffer0<Resource>	Resource::all;




	float2	RandomHeight(const TVec3<>&center0, const TPlanetConfig&config);



	#ifdef HAS_WINDOW
		OrbitGrid	visibleOrbitGrid;
		/*static*/ const float4 WaterCompiler::waterColor = float4(0.5f, 0.6f, 1.f,0.6f);
	#endif

	/*static*/ float3	CellStack::Cell::typeColor[Content::NumberOfPossibilities] =
	{
		float3(1.f),	//Unknown
		float3(0.25f),	//Granite
		float3(94.f / 255.f, 69.f / 255.f, 28.f / 255.f)*2.f,	//earth
		float3(0.5f, 1.f, 0.5f),	//grass
		float3(0.8f),	//rock
		float3(1.f,1.f,0.6f)*1.2f,	//sand
		float3(1.2f),	//Snow,
		float3(0.6f),	//foundation
		float3(0.3f, 0.3f, 0.3f), //road
		float3(1.2f,0.6f,0.5f)*-2.5f,	//Lava
		float3(0.5f, 0.6f, 1.f)*0.3f,	//Water
		float3(1)	//clear
	};

	/*static*/ int	CellStack::Cell::typeStrength[Content::NumberOfPossibilities] =
	{
		INT_MAX, //Unknown
		9,	//granite
		4,	//earth
		4,	//grass
		5,	//rock
		3,	//sand
		3,	//Snow,
		7,	//foundation
		6,	//road
		1,	//lava
		2,	//water
		0	//clear
	};

	/*static*/ bool	CellStack::Cell::typeColorMixes[Content::NumberOfPossibilities] =
	{
		true,	//Unkown
		true,	//granite
		true,	//earth
		true,	//grass
		true,	//rock
		true,	//sand
		true,	//Snow,
		false,	//foundation
		false,	//road
		false,	//lava
		false,	//water
		false	//clear
	};



	/*static*/ float	CellStack::Cell::typeStructureFactor[Content::NumberOfPossibilities] =
	{
		1,	//unknown
		1,	//granite
		1,	//earth
		1,	//grass
		1,	//rock
		1,	//sand
		1,	//Snow,
		0,	//foundation
		0,	//road
		1,	//lava
		1,	//water
		1	//clear
	};


	void			CellStack::ChangeStructureCounterI(int height, int by)
	{
		if (height >= BottomHeight && height <= TopHeight)
		{
			volumeCell[height + HalfRange].structureCounter += by;
		}
	}

	#ifdef HAS_WINDOW
		float4			CellStack::GetColorU(unsigned layer, Cell::Content oppositeContent, bool&mixes, Cell::Content&outType) const
		{
			if (layer >= Range)
			{
				mixes = false;
				outType = Cell::Content::Clear;
				return float4();
			}
			const Cell&cell = layer > 0 ? this->volumeCell[layer - 1] : bottom;
			mixes = cell.content == oppositeContent || Cell::typeColorMixes[cell.content];
			outType = Cell::Content::Reinterpret(cell.content);
			return float4(cell.GetBaseColor(), this->volumeCell[layer].GetLightLevel());
		}
	
		float4			CellStack::GetDownColorU(unsigned layer, Cell::Content oppositeContent, bool&mixes, Cell::Content&outType) const
		{
			if (layer+1 >= Range)
			{
				mixes = false;
				outType = Cell::Content::Clear;
				return float4();
			}
			const Cell&cell = this->volumeCell[layer + 1];
			mixes = oppositeContent==cell.content || Cell::typeColorMixes[cell.content];
			outType = Cell::Content::Reinterpret(cell.content);
			return float4(cell.GetBaseColor(), this->volumeCell[layer].GetLightLevel());
		}


		float4			CellStack::GetMixColorU(unsigned layer, Cell::Content oppositeContent, float&sumBrightnessTo, Content&outContent) const
		{
			if (layer >= Range)
			{
				sumBrightnessTo += 1.f;
				outContent = Content::Clear;
				return float4();
			}
			if (!layer)
			{
				sumBrightnessTo += this->volumeCell[layer].GetLightLevel();
				outContent = Content::Clear;
				return float4();
			}
			const Cell&cell = this->volumeCell[layer - 1],
				&upper = this->volumeCell[layer];
			float light = upper.GetLightLevel();
			if (!cell.IsSolid() || upper.IsSolid())
			{
				sumBrightnessTo += light * 1.3f;
				outContent = Content::Clear;
				return float4();
					//float4(0, 0, 0, -0.1f);
					//float4(0.2f) * light;	//slight light increase
			}
			else
			{
				sumBrightnessTo += light;
				if (oppositeContent == cell.content || Cell::typeColorMixes[cell.content])
				{
					outContent = Content::Reinterpret( cell.content );
					return float4(cell.GetBaseColor(), 1.f);
				}
				outContent = Content::Clear;
				return float4();
			}

		}


	
		float4			CellStack::GetDownMixColorU(unsigned layer, Cell::Content oppositeContent, float&sumBrightnessTo, Content&outContent) const
		{
			if (layer >= Range)
			{
				sumBrightnessTo += 1.f;
				outContent = Content::Clear;
				return float4();
			}
			if (!layer)
			{
				sumBrightnessTo += this->volumeCell[layer].GetLightLevel();
				outContent = Content::Clear;
				return float4();
			}
			const Cell&cell = this->volumeCell[layer - 1],
				&upper = this->volumeCell[layer];
			float light = cell.GetLightLevel();
			if (cell.IsSolid() || !upper.IsSolid())
			{
				sumBrightnessTo += light * 1.3f;
				outContent = Content::Clear;
				return float4();
					//float4(0, 0, 0, -0.1f);
					//float4(0.2f) * light;	//slight light increase
			}
			else
			{
				sumBrightnessTo += light;
				if (oppositeContent == upper.content || Cell::typeColorMixes[upper.content])
				{
					outContent = Content::Reinterpret( upper.content );
					return float4(upper.GetBaseColor(), 1.f);
				}
				outContent = Content::Clear;

				return float4();
			}

		}


	
		static DX11::Texture			lineTex;
		static DX11::Shader				gridShader,transparentGridShader;// , blockShader;
	#endif

	HighSetup		high;
	Setup<5>			low;

	WorldData			actualWorld;
	#ifndef CLIENT
		WorldData		initialWorld;
	#endif
	
	#ifdef VOXEL_SCANNER
		Array<LowCellStack>	lowCellStack;
	#endif


	#ifdef HAS_WINDOW
		namespace Instruction
		{	
			float4	lastColor;
			displayMode_t				displayMode = NormalRendering;
			DX11::BaseInstructionBlock	terrainBase, terrainSphereBase,
										terrainParaboloidBase,
										orbitGridBase,
									 terrainShadowBase, 
									 waterBase, overlayBase, 
									 overlayShadowBase, resourceBase;// , terrainEdgeBase;

			DX11::ObjectBuffer<float4>	ParaboloidDirection_Height,SphereDef,SphereColor;
			DX11::ObjectBuffer<float4>	ParaboloidColor_FlatRadius;

			DX11::ObjectBuffer<VecN<float,8> > atmosphereConfig;
			DX11::ObjectBuffer<float4> sunDirection,sunColor;



		}
	#endif

	/*static*/ CellStack::Cell	CellStack::bottom(CellStack::Cell::Content::Granite, 0), CellStack::top(CellStack::Cell::Content::Clear, 255);	//extremes below the bottom most, and above the top most


	const CellStack::Cell&		CellStack::GetCellU(unsigned layer)	const
	{
		if (layer >= Range)
			return top;
		return volumeCell[layer];
	}

	const CellStack::Cell&		CellStack::GetCellI(int height)	const
	{
		if (height < -HalfRange)
			return bottom;
		if (height >= HalfRange)
			return top;
		return volumeCell[height + HalfRange];
	}

	CellStack::Cell&		CellStack::GetMutableCellI(int height)
	{
		if (height < -HalfRange)
			FATAL__("index out of bounds");
		if (height >= HalfRange)
			FATAL__("index out of bounds");
		return volumeCell[height + HalfRange];
	}

	int				CellStack::FindTopOccupied() const
	{
		for (UINT32 j = Range - 1; j < Range; j--)
			if (volumeCell[j].IsOccupied())
				return int(j) - HalfRange;
		return -HalfRange-1;
	}

	bool				CellStack::FindTopOccupied(int&outHeight) const
	{
		for (UINT32 j = Range - 1; j < Range; j--)
			if (volumeCell[j].IsOccupied())
			{
				outHeight = int(j) - HalfRange;
				return true;
			}
		return false;
	}



	///*static*/ float		CellStack::HeightFactor(unsigned cellIndex)
	//{
	//	if (cellIndex >= Range)
	//		return 1.f;
	//	unsigned down = Range - 1 - cellIndex;
	//	float fdown = float(down) / (Range - 1);
	//	return 0.5f + 0.5f * (1.f - fdown * fdown * fdown);
	//}
	#ifdef HAS_WINDOW
		float	CellStack::Cell::GetLightLevel() const
		{
			if (IsSolid())
				return 0.f;
			float rs = float(intLightLevel ) / float(0xFF);//*0.75f + 0.25f;
				//rs *= 0.1f;
			//rs *= 1.5f - 0.5f * float(visible > 0);
			return rs;
		}

		float	CellStack::Cell::GetWaterLightLevel() const
		{
			#ifdef DEBUG_HIDE_SENSORY
				return 1.f;
			#else
				return float(visible > 0);
			#endif
		}

		void	CellStack::Cell::SetLightLevel(float v)
		{
			BYTE val = BYTE(clamped(v * 0xFF,0,0xFF));
			if (IsSolid())
				val = 0;
			else
				val = Degrade(val,64 * structureCounter);

			intLightLevel = val;
		}


	
		void	CellStack::Cell::SetIntLightLevel(int v)
		{
			BYTE val = BYTE(clamped(v,0,0xFF));
			if (IsSolid())
				val = 0;
			else
				val = Degrade(val,64 * structureCounter);

			intLightLevel = val;
		}

		void	CellStack::Cell::SetSubmergence(UINT sub)
		{
			submergence = (BYTE)std::min(sub,255U);
		}
		/**
		Calculates submergence levels that are propagated down from this cell
		Returns 0 if the local cell is solid, and an incremented local submergence value if water
		*/
		UINT		CellStack::Cell::GetIncreasedSubmergence()	const
		{
			if (IsSolid())
				return 0;
			UINT rs = submergence;
			if (content == Content::Water && rs < 0xFF)
				rs ++;
			return rs;
		}

	#endif


	#ifndef CLIENT

		static BYTE GetTopTypeAtHeight(const TPlanetConfig&config, int h)
		{
			if (h > config.snowHeight)
				return Content::Snow;

			if (h < config.liquidHeight && CellStack::Cell::IsLiquid(config.liquidCoverage) && CellStack::Cell::IsSolid(config.liquidProximity))
				return config.liquidProximity;

			if (h < 3 || config.innerType != Content::Unknown)
				return config.topType;
			else
				return Content::Rock;
		}


		void			CellStack::Init(float2 heights, const TPlanetConfig&config)
		{

			//float topHeight = heights.y;
			//based on this, h could go up to 40, though chances for that are excessively rare
			int up0 = (int)Round(heights.y * config.mountains.heightScale / RawLayerHeight);
			if (abs(up0) <= config.mountains.dampening)
				up0 = 0;
			else
				up0 -= config.mountains.dampening * sign(up0);
			int up1 = (int)Round(heights.x * config.extendedCore.heightScale / RawLayerHeight);
			if (abs(up1) <= config.extendedCore.dampening)
				up1 = 0;
			else
				up1 -= config.extendedCore.dampening * sign(up1);
			const int h = clamp(config.mountains.baseHeight + up0, -HalfRange, HalfRange - 2);
			const int coreH = clamp(config.extendedCore.baseHeight + up1, -HalfRange, HalfRange - 2);
		
//			int coreH = clamp(Round(fabs(heights.x) * config.coreScale / RawLayerHeight)-HalfRange, -HalfRange, HalfRange - 1);

			UINT uh = unsigned(h + HalfRange);
			UINT coreLayer = ToLayer(coreH);

			#ifdef CLIENT
				lowestOpenCell = (BYTE)uh + 1;
				highestOpaqueCell = (BYTE)uh;
			#endif
			if (config.innerType == Content::Unknown)
			{
				bool hasEarth = Cell::typeStrength[config.topType] <= Cell::typeStrength[Content::Earth];
				UINT rockHeight = h > 3 ? uh : uh - (3 - h);
				if (!hasEarth)
					rockHeight = Range;
				for (UINT i = 0; i <= uh; i++)
				{
			
					volumeCell[i].content = i <= rockHeight ? Cell::Content::Rock : Cell::Content::Earth;

					#ifdef CLIENT
						volumeCell[i].intLightLevel = 0;
						volumeCell[i].submergence = h < 0 && i == uh ? (BYTE)clamp(HalfRange - int(i),0,255) : 0;
					#endif
				}
			}
			else
			{
				for (UINT i = 0; i <= uh; i++)
				{
			
					volumeCell[i].content = config.innerType;

					#ifdef CLIENT
						volumeCell[i].intLightLevel = 0;
						volumeCell[i].submergence = h < 0 && i == uh ? (BYTE)clamp(HalfRange - int(i),0,255) : 0;
					#endif
				}


			}
			volumeCell[uh].content = GetTopTypeAtHeight(config,h);

			for (UINT i = 0; i < coreLayer; i++)
			{
				volumeCell[i].content = config.extendedCoreType;
			}


			if (config.liquidCoverage != Content::Clear)
			{
				for (UINT i = 0; i <= (UINT)(HalfRange + config.liquidHeight) && i < Range; i++)
				{
					if (volumeCell[i].content == Content::Clear)
					{
						volumeCell[i].content = config.liquidCoverage;
						#ifdef CLIENT
							volumeCell[i].submergence = (BYTE)clamp(HalfRange - int(i),0,255);
						#endif
					}
					#ifdef CLIENT
						volumeCell[i].intLightLevel = 0xFF;
					#endif
				}
			}

			#ifdef CLIENT
				if (h < 0)
					waterRange = IntRange<BYTE>(uh, HalfRange+1);
				else
					waterRange.SetEmpty(0);

				lightingChangedTo = uh+1;
			#endif


		}
	#endif
	

	void			CellStack::ZeroInit()
	{
		#ifdef HAS_WINDOW
			lowestOpenCell = (BYTE)Range;
			highestOpaqueCell = (BYTE)Range-1;
			waterRange = IntRange<BYTE>(0,0);
		#endif
		for (UINT i = 0; i < Range; i++)
		{
			volumeCell[i].content = Cell::Content::Unknown;
			volumeCell[i].structureCounter = 0;
			#ifdef HAS_WINDOW
				volumeCell[i].intLightLevel = 0xFF;
				volumeCell[i].submergence = 0;
				volumeCell[i].visible = 0;
			#endif
		}
		#ifdef HAS_WINDOW
			lightingChangedTo = Range;
		#endif
	}

	void			CellStack::ResetStructureCounters()
	{
		for (UINT i = 0; i < Range; i++)
		{
			volumeCell[i].structureCounter = 0;
			#ifdef HAS_WINDOW
				volumeCell[i].visible = 0;
			#endif
		}
	}


	bool			CellStack::MakeLocalHole(int h)
	{
		UINT uh = ToLayer(h);
		if (uh < Range)
		{
			if (volumeCell[uh].content != Content::Clear)
			{
				volumeCell[uh].content = Cell::Content::Clear;

				#ifdef HAS_WINDOW
					lowestOpenCell = std::min(lowestOpenCell, (BYTE)uh);
					if (highestOpaqueCell == (BYTE)uh)
					{
						while (!volumeCell[highestOpaqueCell].IsSolid() && highestOpaqueCell > 0)
							highestOpaqueCell--;
					}
					actualWorld.lightingOutdated = true;
					DebugCheckRange();
				#endif
				return true;
			}
		}
		return false;
	}


	bool			CellStack::LocalOverwrite(int h, Cell::Content type)
	{
		UINT uh = ToLayer(h);
		if (uh < Range)
		{
			if (volumeCell[uh].content != type)
			{
				bool wasSolid = volumeCell[uh].IsSolid();
				volumeCell[uh].content = type;

			
				#ifdef HAS_WINDOW
					if (!wasSolid && volumeCell[uh].IsSolid())
					{
						if (lowestOpenCell == (BYTE)uh)
							while (volumeCell[lowestOpenCell].IsSolid() && lowestOpenCell < (BYTE)Range)
								lowestOpenCell++;

						highestOpaqueCell = std::max(highestOpaqueCell, (BYTE)uh);
					}
					else
						if (wasSolid && !volumeCell[uh].IsSolid())
						{
							lowestOpenCell = std::min(lowestOpenCell, (BYTE)uh);
							if (highestOpaqueCell == (BYTE)uh)
							{
								while (!volumeCell[highestOpaqueCell].IsSolid() && highestOpaqueCell > 0)
									highestOpaqueCell--;
							}
						}
					actualWorld.lightingOutdated = true;
					DebugCheckRange();
				#endif
				return true;
			}
		}
		return false;
	}

	int CellStack::FindMostApplicableGround(int h, bool canStandOnWater) const
	{
		UINT uh = unsigned(h + HalfRange);
		if (uh < Range)
		{
			if (volumeCell[uh].IsSolid() || (volumeCell[uh].IsWater() && canStandOnWater))
			{
				for (UINT i = uh + 1; i < Range; i++)
					if (/*!volumeCell[i].IsOccupied() && */!volumeCell[i].IsSolid() && (!canStandOnWater || !volumeCell[i].IsWater()))
						return int(i) - HalfRange;
				return 0x1FFFFFFF;
			}
			for (UINT i = uh - 1; i < uh; --i)
			{
				if (volumeCell[i].IsSolid() || (volumeCell[i].IsWater() && canStandOnWater))
					return (int(i + 1) - HalfRange);
				//if (volumeCell[i].IsOccupied())
					//return 0x1FFFFFFF;
			}
			return -HalfRange;
		}
		return h;
	}



	static void 		SetW(BaseGrid::TNetCell&cell, float w)
	{
		cell.centerVertex.w = w;
		for (int i = 0; i < cell.numCorners; i++)
			cell.cornerVertex[i].w = w;
	}



	void				Create1()
	{
		#ifdef HAS_WINDOW
			Buffer0<DX11::Shader::Binding> bindings;

			//bindings;
		
			bindings.Append(DX11::Shader::Binding(Instruction::sunDirection,"Sun"));
			bindings.Append(DX11::Shader::Binding(Instruction::sunColor,"SunColor"));
			#ifdef CASCADING_SHADOW
				for (index_t i = 0; i < Objects::NumCascades; i++)
				{
					bindings.Append(DX11::Shader::Binding(Objects::shadowMatrix[i],"ShadowMatrix"+String(i)));
					bindings.Append(DX11::Shader::Binding(Objects::shadowMap[i].GetDepthStencilTarget(), "ShadowMap"+String(i)));
				}
			#else
				bindings.Append(DX11::Shader::Binding(Objects::shadowMatrix,"ShadowMatrix"));
				bindings.Append(DX11::Shader::Binding(Objects::shadowMap.GetDepthStencilTarget(), "ShadowMap"));
			#endif
			bindings.Append(DX11::Shader::Binding(Instruction::atmosphereConfig, "AtmosphereConfig"));
			bindings.Append(DX11::Shader::SamplerBinding(true, Engine::TextureFilter::None, 1, "ShadowSampler"));
		
			Instruction::terrainBase.shader.LoadFromFile("shaders/gridTerrain.hlsl", "vsMain", "psMain", bindings.pointer(), bindings.Count());

		

			{
				Engine::VertexBinding binding;
				binding.vertex.set(0, 4);
				binding.normal.set(4, 4);
				binding.color.set(8, 4);
				binding.floats_per_vertex = 12;
				Instruction::terrainBase.binding = binding;
				Instruction::terrainBase.SetTriangles();

				Instruction::terrainSphereBase.binding = binding;
				Instruction::terrainSphereBase.SetTriangles();

				Instruction::terrainParaboloidBase.binding = binding;
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


		#endif
	}
	
	#ifdef CLIENT
		#ifdef HAS_WINDOW
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
				Mat::mult(SunDetails::sunSystem,p,SunDetails::sunDirection);
				UpdateSunDirection(SunDetails::sunDirection,true);
			}

			void	UpdateSunDirection(const float3&p, bool shadows /*= true*/)
			{
				SunDetails::sunDirection = p;
				//#ifdef _DEBUG
				//	SunDetails::sunDirection = -p;
				//#endif
				Instruction::sunDirection.Update(float4(SunDetails::sunDirection,shadows));
			}


		#endif
	#else

		static bool ReplaceTop(BYTE content)
		{
			return content == Content::Earth || content == Content::Rock || content == Content::Sand;
		}

		void GenerateWorldVolume(const TPlanetConfig&config, WorldDiscovery discovery)
		{
			for (index_t i = 0; i < HighSetup::GridType::CellCount; i++)
			{
				actualWorld.cellStack[i].Init(RandomHeight(high.cells[i].centerVertex.xyz,config),config);
			}


			for (index_t i = 0; i < LowSetup::GridType::CellCount; i++)
			{
				index_t fine = Grid::low.grid.GetCell(i).higherEquivalent;
				auto&stack = actualWorld.cellStack[fine];
				const auto&n = Grid::high.grid.GetCell(fine).neighborhood;
				for (UINT l = 0; l < CellStack::Range; l++)
				{
					if (stack.volumeCell[l].IsClearOrWater())
					//{
					//	for (int ni = 0; ni < n.numNeighbors; ni++)
					//	{
					//		auto&other = actualWorld.cellStack[n.neighborID[ni]].volumeCell[l];
					//		if (other.IsClearOrWater())
					//		{
					//			//stack.volumeCell[l] = other;
					//			other = stack.volumeCell[l];
					//			//break;
					//		}
					//	}
					//
					//}
					//else
					{
						int cnt = 0;
						BYTE cont;
						for (int ni = 0; ni < n.numNeighbors; ni++)
						{
							auto&other = actualWorld.cellStack[n.neighborID[ni]].volumeCell[l];
							if (!other.IsClearOrWater())
							{
								cnt ++;
								cont = other.content;
							}
						}
						if (cnt > 1)
						{
							stack.volumeCell[l].content = cont;
	
						}
						else if (cnt > 0)
						{
							for (int ni = 0; ni < n.numNeighbors; ni++)
							{
								auto&otherStack = actualWorld.cellStack[n.neighborID[ni]];
								auto&other = otherStack.volumeCell[l];
								if (!other.IsClearOrWater())
								{
									cnt ++;
									other.content = stack.volumeCell[l].content;
								}
							}
						}
					}
				}
			}

			Concurrency::parallel_for(index_t(0),LowSetup::GridType::CellCount,[](index_t i)
			{
				index_t fine = Grid::low.grid.GetCell(i).higherEquivalent;
				auto&centerStack = actualWorld.cellStack[fine];
				const auto&n = Grid::high.grid.GetCell(fine).neighborhood;
				const auto&n0 = Grid::low.grid.GetCell(i).neighborhood;
				for (int ni = 0; ni < n.numNeighbors; ni++)
				{
					if ((index_t)n0.neighborID[ni] < i)
						continue;

					const auto&n2 = Grid::high.grid.GetCell(n.neighborID[ni]).neighborhood;

					auto&nStack = actualWorld.cellStack[n.neighborID[ni]];
					for (UINT l = 0; l < CellStack::Range; l++)
					{
						if (!nStack.volumeCell[l].IsClearOrWaterIgnoreStructure())
							continue;
						count_t cnt = 0;
						BYTE cont = 0;
						for (int nj = 0; nj < n2.numNeighbors; nj++)
						{
							if (!actualWorld.cellStack[n2.neighborID[nj]].volumeCell[l].IsClearOrWaterIgnoreStructure())
							{
								cnt ++;
								cont = actualWorld.cellStack[n2.neighborID[nj]].volumeCell[l].content;
							}
						}
						if (cnt > 3)
						{
							nStack.volumeCell[l].content = cont;
						}
					}

				}
			});


			const bool solidProx = CellStack::Cell::IsSolid(config.liquidProximity) && CellStack::Cell::IsLiquid(config.liquidCoverage);
			for (index_t i = 0; i < HighSetup::GridType::CellCount; i++)
			{
				auto&stack = actualWorld.cellStack[i];
				for (UINT l = 0; l < CellStack::Range; l++)
				{
					const BYTE cont = stack.volumeCell[l].content;
					if ((cont == Content::Water || cont == Content::Clear) && l > 0)
					{
						if (ReplaceTop(stack.volumeCell[l-1].content))
							stack.volumeCell[l-1].content = 
								GetTopTypeAtHeight(config,Grid::CellStack::ToHeight(l-1));
					}
				}
			}
			if (solidProx)
				for (index_t i = 0; i < HighSetup::GridType::CellCount; i++)
				{
					auto&stack = actualWorld.cellStack[i];
					for (UINT l = 0; l < CellStack::Range; l++)
					{
						const BYTE cont = stack.volumeCell[l].content;
						if (solidProx && cont == config.liquidCoverage)
						{
							const auto&n = high.grid.GetCell(i).neighborhood;
							for (BYTE k = 0; k < n.numNeighbors; k++)
								actualWorld.cellStack[n.neighborID[k]].volumeCell[l].SetToIfSolid(config.liquidProximity);
						}

					}
				}


			for (index_t i = 0; i < HighSetup::GridType::CellCount; i++)
			{
				auto&stack = initialWorld.cellStack[i];
				stack = actualWorld.cellStack[i];
				switch (discovery)
				{
					case WorldDiscovery::None:
						for (unsigned j = 0; j < CellStack::Range; j++)
						{
							CellStack::Cell&c = stack.volumeCell[j];
							if (c.content != Content::Clear)
							{
					
								c.content = Content::Unknown;
								DBG_ASSERT__(c.structureCounter == 0);
							}
						}
					break;
					case WorldDiscovery::Oceans:
						for (unsigned j = 0; j < CellStack::Range; j++)
						{
							CellStack::Cell&c = stack.volumeCell[j];
							if (c.content != Content::Clear && c.content != Content::Water)
							{
								c.content = Content::Unknown;
								DBG_ASSERT__(c.structureCounter == 0);
							}
						}
					break;
				}
			}
		}

	#endif

	void				Create0(const TPlanetConfig&config, WorldDiscovery discovery)
	{
		#ifdef HAS_WINDOW
			Instruction::atmosphereConfig.Create(VecN<float,8>());
			UpdateAtmosphere(config.atmosphereColor);

			Instruction::sunDirection.Create(float4());
			Instruction::sunColor.Create(float4());
			UpdateSun(config.sunPlaneNormal_Speed,config.sunColor);



			Image img(64, 1, 4);
			for (UINT32 x = 0; x < img.width(); x++)
			{
				float a = smoothstep(0.9, 0.95, float(x) / float(img.width() - 1));
				float b = smoothstep(0.925, 0.95, float(x) / float(img.width() - 1));
				BYTE*pixel = img.get(x, 0);
				pixel[0] = (BYTE)(180 * b);
				pixel[1] = (BYTE)(200 * b);
				pixel[2] = (BYTE)(255 * b);
				pixel[3] = (BYTE)(a * 255.f);
			}


			lineTex.Load(img, true);
			//blockConfig.Create(TMatrix4<>());

			DX11::Shader::Binding bindings[] =
			{
				DX11::Shader::Binding(lineTex, "LineTex"),
				DX11::Shader::SamplerBinding(true, Engine::TextureFilter::Trilinear, 1, "LineSampler")

			};
			gridShader.LoadFromFile("shaders/sphereGrid.hlsl", "vsMain", "psMain", bindings, ARRAYSIZE(bindings));
			transparentGridShader.LoadFromFile("shaders/transparentSphereGrid.hlsl", "vsMain", "psMain");

		#endif

		high.Create(false);
		low.Create(true);

		foreach(high.cells, cell)
			SetW(*cell, -1.f);

		bool tagged[HighSetup::GridType::CellCount];
		memset(tagged, false, sizeof(tagged));

			for (index_t i = 0; i < low.cells.count(); i++)
			{
				#ifdef HYBRID_GRID
				index_t high = low.grid.GetCell(i).higherEquivalent;
				const auto&n = Grid::high.grid.GetCell(high).neighborhood;

				for (int j = 0; j < n.numNeighbors; j++)
				{
					const UINT32 c = n.neighborID[j];
					if (tagged[c])
						continue;
					tagged[c] = true;
					const auto&n2 = Grid::high.grid.GetCell(c).neighborhood;
					int found = -1;
					for (int k = 0; k < n2.numNeighbors; k++)
						if (n2.neighborID[k] == high)
						{
							found = k;
							break;
						}
					ASSERT__(found != -1);
					auto&cell = Grid::high.cells[c];


					float3 v0 = cell.cornerVertex[(found + 1)%cell.numCorners].xyz;
					float3 v1 = cell.cornerVertex[(found + cell.numCorners-2)%cell.numCorners].xyz;
					float3 normal = (v0 | v1).Normalized();
				
					auto&vtx = cell.centerVertex;

					vtx.w = Vec::dot(vtx.xyz, normal) * 100.f;
					for (int k = 0; k < cell.numCorners; k++)
					{
						auto&vtx = cell.cornerVertex[k];
						vtx.w = Vec::dot(vtx.xyz, normal) * 100.f;
					}

				}
				#else
					auto&cell = Grid::high.cells[i];
					for (int i = 0; i < cell.numCorners; i++)
						cell.cornerVertex[i].w = 0.f;
					cell.centerVertex.w = 2.f;
				#endif
			}

		#ifdef VOXEL_SCANNER
			lowCellStack.SetSize(low.cells.Count());
		#endif

		CellStack::bottom.content = config.innerCoreType;


		#ifndef CLIENT
			GenerateWorldVolume(config,discovery);

		#else
			actualWorld.FlushAllData();
		#endif




		#ifdef HAS_WINDOW
			actualWorld.UpdateLighting();
		#endif

		Random random;

		//Resource::Create("Fuel", float3(0.1,1,0.1), float3(12.4,-2.124,0),0.68f,12);
//		Resource::Create("Minerals", float3(0.9,0.9,0.9), float3(13.2,12.4,-2.124),0.68f,12);
		//Resource::Create("Fuel", float3(0.1,1,0.1), Resource::FuelCoverage);
		//Resource::Create("Minerals", float3(0.9,0.9,0.9), Resource::MineralCoverage);
		//Resource::Create("Uranium", float3(1,1,0.1), Resource::UraniumCoverage);

		Resource::Create("Fuel", ConstName::Resource_Fuel, float3(0.1,1,0.1), Objects::Resource::Fuel);
		Resource::Create("Minerals", ConstName::Resource_Minerals, float3(0.9,0.9,0.9), Objects::Resource::Minerals);
		Resource::Create("Uranium", ConstName::Resource_Uranium, float3(1,1,0.1), Objects::Resource::Uranium);




		#ifdef HAS_WINDOW
			for (BYTE i = 0; i < BaseGrid::NumFamilies; i++)
				high.BakeBlockInstruction(actualWorld.cellStack, i, Clip::NoClip);
		#endif 

	}

	namespace Clip
	{
		bool NoClip(index_t, int)
		{
			return false;
		}

	}
	void				Destroy()
	{
		low.Destroy();
		high.Destroy();
		//blockConfig.Clear();
		#ifdef HAS_WINDOW
			Instruction::sunDirection.Clear();
			Instruction::sunColor.Clear();

			lineTex.Clear();
			gridShader.Clear();
			transparentGridShader.Clear();
			Instruction::overlayBase = Instruction::overlayShadowBase 
				= Instruction::resourceBase = Instruction::terrainBase 
				= Instruction::terrainSphereBase 
				= Instruction::terrainParaboloidBase 
				= Instruction::orbitGridBase
				= Instruction::terrainShadowBase 
				= Instruction::waterBase = 
				DX11::BaseInstructionBlock();
			Instruction::atmosphereConfig .Clear();
			Instruction::ParaboloidDirection_Height.Clear();
			Instruction::ParaboloidColor_FlatRadius.Clear();
			Instruction::SphereDef.Clear();
		#endif
		//terrainColor.Clear();
	}

	#ifndef CLIENT
		float	RandomHeight(const TVec3<>&center0, const TPlanetConfig::TSeedSet&config)
		{
			//center *= 3.f;
			TVec3<> center = center0;
			float rs = 0;

			float strength = 1.f;
			//noiseLevels = std::min(noiseLevels,5);
			for (int i = 0; i < 5; i++)
			{
				center *= 2.f;
				strength /= 2.f;
				const float3&seed = config.seed[i];
				rs += noise.Get(center.x + seed.x, center.y + seed.y, center.z + seed.z)*strength;
			}
			rs *= noise.Get(center0.x*2.f + config.productSeed.x, center0.y*2.f + config.productSeed.y, center0.z*2.f + config.productSeed.z) * 0.5 + 0.5f;

			float rf = 1.5f;
			float depress = smoothstep(0.1f, 0.2f, 
							0.5f * noise.Get(center0.x*6.f*rf + config.depressionSeed0.x, center0.y*6.f*rf + config.depressionSeed0.y, center0.z*6.f*rf + config.depressionSeed0.z)
							 + noise.Get(center0.x*3.f*rf + config.depressionSeed1.x, center0.y*3.f*rf + config.depressionSeed1.y, center0.z*3.f*rf + config.depressionSeed1.z));
			float fc = 0.25f * noise.Get(center0.x*4.f*rf + config.depressionSeed2.x, -center0.y*4.f*rf + config.depressionSeed2.y, -center0.z*4.f*rf + config.depressionSeed2.z) 
					+ 0.75f * noise.Get(-center0.x*2.f*rf + config.depressionSeed3.x, center0.y*2.f*rf + config.depressionSeed3.y, center0.z*2.f*rf + config.depressionSeed3.z);
			//noise.Get(center0.x*2.f + 10.f, center0.y*2.f, center0.z*2.f);
			depress *= fc;// *fabs(fc);

			rs -= depress*0.3f;

			return rs;
			//return (float)sign(rs)*pow(fabs(rs),1.3f) ;
		}

		float2	RandomHeight(const TVec3<>&center0, const TPlanetConfig&config)
		{
			return float2(
				RandomHeight(center0*config.extendedCore.frequency,config.extendedCore),
				RandomHeight(center0*config.mountains.frequency,config.mountains)
				);

		}
	#else
		#ifdef HAS_WINDOW
			void	UpdatePlanetRenderingParameters(float radius)	//more to come
			{
				TMatrix4<> matrix = Matrix<>::eye4;
				matrix.x.xyz *= radius;
				matrix.y.xyz *= radius;
				matrix.z.xyz *= radius;

				display.SetObjectMatrix(matrix);
			}

			//void	RenderNeighborhood(index_t stackID, float radius)
			//{
			//	UpdatePlanetRenderingParameters(radius);
			//	//display.ClearDepth();
			//	low.RenderCell(stackID);
			//	stackID = low.grid.GetCell(stackID).higherEquivalent;
			//	const BaseGrid::TCell&cell = high.grid.GetCell(stackID);
			//	high.RenderCell(stackID);
			//	for (int i = 0; i < cell.neighborhood.numNeighbors; i++)
			//		high.RenderCell(cell.neighborhood.neighborID[i]);

			//}
		#endif
	#endif



	void					WorldData::MapSensorState(UINT32 lowCellCenter, float radius, int refHeight, int groundPenetration, SkySensorState&outState) const
	{
		outState.Clear();
		outState.groundPenetration = groundPenetration;


		Queue<UINT32>	openCells;
		const UINT32 highCenter = Grid::low.grid.GetCell(lowCellCenter).higherEquivalent;
		openCells.Push(highCenter);

		

		const float r = ToRadius(refHeight);
		//radius /= r;
		const float hr2 = sqr(radius/r);
		const TVec3<>& center = high.cells[highCenter].centerVertex.xyz;
		UINT32 current;
		while (openCells >> current)
		{
			float d2 = Vec::quadraticDistance(high.cells[current].centerVertex.xyz,center);
			if (d2 > hr2)
				continue;
			if (!outState.coverageMap.SetNew(current,SkySensorState::Uninitialized))
				continue;
			auto&cell = outState.coverage.Append();
			cell.downToHeight = SkySensorState::Uninitialized;
			cell.hStackID = current;
			const auto&n = high.grid.GetCell(current).neighborhood;
			for (int k = 0; k < n.numNeighbors; k++)
				openCells << n.neighborID[k];
		}
	}

	int						WorldData::TraceSkySensor(UINT32 highCellCenter, int penetration) const
	{
		int penetrationLeft = penetration << 1;	//account for water effect
		//int h = CellStack::HalfRange-1;

		const CellStack&stack = cellStack[highCellCenter];
		UINT32 limit = CellStack::Range-1;

		#ifdef HAS_WINDOW
			if (stack.waterRange.IsNotEmpty())
			{
				limit = (UINT32)(stack.waterRange.end-1);
				if (stack.highestOpaqueCell >= stack.lowestOpenCell)
					limit = std::max(limit,(UINT32)stack.highestOpaqueCell);
			}
			else
				if (stack.highestOpaqueCell >= stack.lowestOpenCell)
					limit = stack.highestOpaqueCell;
		#endif
		ASSERT_LESS__(limit,CellStack::Range);

		UINT32 layer = limit;
		//stack.highestOpaqueCell




		for (; layer < CellStack::Range; layer--)
		{
			const CellStack::Cell&cell = stack.volumeCell[layer];

			if (cell.content == Content::Clear)
				continue;
			if (cell.content == Content::Water)
				penetrationLeft--;
			else
				penetrationLeft-=2;
			if (penetrationLeft < 0)
				break;
		}
		if (layer > CellStack::Range)
			layer = 0;
		//if (layer > 0)
		//	layer--;
		return CellStack::ToHeight(layer);
	}


	bool					WorldData::UpdateSensorState(SkySensorState&state) const
	{
		bool anyChanged = false;
		for (auto&c : state.coverage)
		{
			int h = TraceSkySensor(c.hStackID,state.groundPenetration);
			if (h == c.downToHeight)
				continue;
			c.downToHeight = h;
			state.coverageMap.Set(c.hStackID,h);
			anyChanged = true;
		}
		return anyChanged;
	}


	#ifdef HAS_WINDOW
	
		void				WorldData::InsertSensorCoverage(const SkySensorState&state)
		{
			ChangeSensorCoverage(state,1);
		}
		void				WorldData::RemoveSensorCoverage(const SkySensorState&state)
		{
			ChangeSensorCoverage(state,-1);
		}


	#endif



	bool					WorldData::LowCellHasStructure(const LCellIdI&cell)const
	{
		if (cell.height >= CellStack::HalfRange || cell.height < -CellStack::HalfRange)
			return false;
		#ifdef HYBRID_GRID
			index_t finer = Grid::low.grid.GetCell(cell.stackID).higherEquivalent;
			const auto&c = Grid::high.grid.GetCell(finer);
			return cellStack[finer].GetCellI(cell.height).structureCounter > 0;
		#else
			return cellStack[cell.stackID].GetCellI(cell.height).structureCounter > 0;

		#endif
	}

	bool					WorldData::LowCellIsClear(const LCellIdI&cell)	const
	{
		if (cell.height >= CellStack::HalfRange)
			return true;
		if (cell.height < -CellStack::HalfRange)
			return false;
		#ifdef HYBRID_GRID
			index_t finer = Grid::low.grid.GetCell(cell.stackID).higherEquivalent;
			const auto&c = Grid::high.grid.GetCell(finer);
			if (cellStack[finer].GetCellI(cell.height).IsOccupied())
				return false;
			for (int j = 0; j < c.neighborhood.numNeighbors; j++)
				if (cellStack[c.neighborhood.neighborID[j]].GetCellI(cell.height).IsOccupied())
					return false;
			return true;
		#else
			return !cellStack[cell.stackID].GetCellI(cell.height).IsOccupied();

		#endif
	}

	bool					WorldData::LowCellIsClearOrWater(const LCellIdI&cell)	const
	{
		if (cell.height >= CellStack::HalfRange)
			return true;
		if (cell.height < -CellStack::HalfRange)
			return false;
		#ifdef HYBRID_GRID
			index_t finer = Grid::low.grid.GetCell(cell.stackID).higherEquivalent;
			const auto&c = Grid::high.grid.GetCell(finer);
			if (!cellStack[finer].GetCellI(cell.height).IsClearOrWater())
				return false;
			for (int j = 0; j < c.neighborhood.numNeighbors; j++)
				if (!cellStack[c.neighborhood.neighborID[j]].GetCellI(cell.height).IsClearOrWater())
					return false;
			return true;
		#else
			return cellStack[cell.stackID].GetCellI(cell.height).IsClearOrWater();

		#endif
	}


	bool					WorldData::LowCellIsClearIgnoreStructure(const LCellIdI&cell)	const
	{
		if (cell.height >= CellStack::HalfRange)
			return true;
		if (cell.height < -CellStack::HalfRange)
			return false;
		#ifdef HYBRID_GRID
			index_t finer = Grid::low.grid.GetCell(cell.stackID).higherEquivalent;
			const auto&c = Grid::high.grid.GetCell(finer);
			if (cellStack[finer].GetCellI(cell.height).content != Content::Clear)
				return false;
			for (int j = 0; j < c.neighborhood.numNeighbors; j++)
				if (cellStack[c.neighborhood.neighborID[j]].GetCellI(cell.height).content != Content::Clear)
					return false;
			return true;
		#else
			return cellStack[cell.stackID].GetCellI(cell.height).content == Content::Clear;
		#endif
	}



	bool	WorldData::GetStrengthRange(const LCellIdI&cell, Content&outWeakest, Content&outStrongest, bool includeUnknown) const
	{
		#ifdef HYBRID_GRID
			index_t finer = Grid::low.grid.GetCell(cell.stackID).higherEquivalent;
			const auto&c = Grid::high.grid.GetCell(finer);
			outStrongest = outWeakest = Content::Reinterpret((cellStack[finer].GetCellI(cell.height).content));
			bool anyUnknown = false;
			int s0 = CellStack::Cell::typeStrength[outStrongest];
			TFloatRange<int> strength = FloatRange<int>(s0,s0);
			if (outStrongest == Content::Unknown)
			{
				anyUnknown = true;
				if (!includeUnknown)
					strength = MaxInvalidRange<int>();
			}
			for (int j = 0; j < c.neighborhood.numNeighbors; j++)
			{
				Content t = Content::Reinterpret(cellStack[c.neighborhood.neighborID[j]].GetCellI(cell.height).content);
				if (t == Content::Unknown)
				{
					anyUnknown = true;
					if (!includeUnknown)
						continue;
				}
				int strength2 = CellStack::Cell::typeStrength[t];
				if (strength2 < strength.min)
				{
					strength.min = strength2;
					outWeakest = t;
				}
				if (strength2 > strength.max)
				{
					strength.max = strength2;
					outStrongest = t;
				}
			}
			return anyUnknown;
		#else
			//const auto&c = Grid::high.grid.GetCell(cell.stackID);
			outStrongest = outWeakest = Content::Reinterpret((cellStack[cell.stackID].GetCellI(cell.height).content));
			return outWeakest == Content::Unknown;
		#endif
	}


	void TagCluster(bool*rebuild, index_t stackID)
	{
		const auto&n2 = Grid::high.grid.GetCell(stackID).neighborhood;
		rebuild[Grid::high.grid.GetCell(stackID).family] = true;
		for (int k = 0; k < n2.numNeighbors; k++)
			rebuild[Grid::high.grid.GetCell(n2.neighborID[k]).family] = true;
	}

	UINT32 TagLowCluster(bool *rebuild, const LCellIdI&cell)
	{
		#ifdef HYBRID_GRID
			UINT32 finer = low.grid.GetCell(cell.stackID).higherEquivalent;
		#else
			UINT32 finer = cell.stackID;
		#endif
		TagCluster(rebuild, finer);
		return finer;
	}

	#ifdef HAS_WINDOW
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
				UINT32 cell = low.Locate(Camera::scene.location);
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
					Camera::TargetOrbit(ToRadius(targetFocus.height));
				}

				HighSetup::TDistanceField	newDomain;
				if (enable)
					high.UpdatePlanarDistanceField(finer, CutoffRadius, newDomain);

				for (index_t i = 0; i < HighSetup::GridType::CellCount; i++)
				{
					if ((newDomain.distance[i] != 0xFFFFFFFF && newHeight) || (newDomain.distance[i] == 0xFFFFFFFF) != (currentCutoffDomain.distance[i] == 0xFFFFFFFF))
					{
						BYTE fam = high.grid.GetCell(i).family;
						high.family[fam].normal.outdated = true;
						low.family[fam].SetOutdated(0);
					}
				}

				currentCutoffDomain = newDomain;
				cutoffMode = enable;
				currentFocus = targetFocus;
			}

	
		}
	#endif

	bool	WorldData::Clear(const LCellIdI&ch, float atTime)
	{
		#ifdef HYBRID_GRID
			index_t finer = Grid::low.grid.GetCell(ch.stackID).higherEquivalent;
			const auto&c = Grid::high.grid.GetCell(finer);
		#else
			index_t finer = ch.stackID;
		#endif
		bool rebuild[BaseGrid::NumFamilies] = { false };

		count_t blobCnt = 0;
		if (cellStack[finer].MakeLocalHole(ch.height
			))
		{
			TagCluster(rebuild, finer);
			blobCnt++;
		}
		//rebuild[Grid::high.grid.GetCell(finer).family] = true;
		#ifdef HYBRID_GRID
			for (int j = 0; j < c.neighborhood.numNeighbors; j++)
			{
				UINT32 n = c.neighborhood.neighborID[j];
				if (cellStack[n].MakeLocalHole(ch.height
					))
				{
					TagCluster(rebuild, n);
					blobCnt++;
				}

			}
		#endif
		if (blobCnt > 0)
			for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
				if (rebuild[k])
				{
				//Grid::high.BakeBlockInstruction(Grid::cellStack,k,0,0);
					Grid::high.family[k].SetOutdated(atTime);
					Grid::low.family[k].SetOutdated(atTime);
				}
		return blobCnt > 0;
	}

	bool	WorldData::ClearNonLava(const LCellIdI&ch, float atTime)
	{
		#ifdef HYBRID_GRID
			index_t finer = Grid::low.grid.GetCell(ch.stackID).higherEquivalent;
			const auto&c = Grid::high.grid.GetCell(finer);
		#else
			index_t finer = ch.stackID;
		#endif
		bool rebuild[BaseGrid::NumFamilies] = { false };

		if (cellStack[finer].GetCellI(ch.height).content == Content::Lava)
			return false;
		bool any = false;
		if (cellStack[finer].MakeLocalHole(ch.height
			))
		{
			TagCluster(rebuild, finer);
			any = true;
		}
		//rebuild[Grid::high.grid.GetCell(finer).family] = true;
		#ifdef HYBRID_GRID
			for (int j = 0; j < c.neighborhood.numNeighbors; j++)
			{
				UINT32 n = c.neighborhood.neighborID[j];
				if (cellStack[n].GetCellI(ch.height).content == Content::Lava)
					return false;
				if (cellStack[n].MakeLocalHole(ch.height
					))
				{
					TagCluster(rebuild, n);
					any = true;
				}

			}
		#endif
		if (any)
			for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
				if (rebuild[k])
				{
				//Grid::high.BakeBlockInstruction(Grid::cellStack,k,0,0);
					Grid::high.family[k].SetOutdated(atTime);
					Grid::low.family[k].SetOutdated(atTime);
				}
		return true;
	}


	bool	WorldData::Overwrite(const LCellIdI&ch, Content with,  float atTime)
	{
		#ifdef HYBRID_GRID
			index_t finer = Grid::low.grid.GetCell(ch.stackID).higherEquivalent;
			const auto&c = Grid::high.grid.GetCell(finer);
		#else
			index_t finer = ch.stackID;
		#endif
		bool rebuild[BaseGrid::NumFamilies] = { false };

		count_t blobCnt = 0;
		if (cellStack[finer].LocalOverwrite(ch.height,with
			))
		{
			TagCluster(rebuild, finer);
			blobCnt++;
		}
		//rebuild[Grid::high.grid.GetCell(finer).family] = true;
		#ifdef HYBRID_GRID
			for (int j = 0; j < c.neighborhood.numNeighbors; j++)
			{
				UINT32 n = c.neighborhood.neighborID[j];
				if (cellStack[n].LocalOverwrite(ch.height,with
					))
				{
					TagCluster(rebuild, n);
					blobCnt++;
				}

			}
		#endif
		if (blobCnt > 0)
			for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
				if (rebuild[k])
				{
				//Grid::high.BakeBlockInstruction(Grid::cellStack,k,0,0);
					Grid::high.family[k].SetOutdated(atTime);
					Grid::low.family[k].SetOutdated(atTime);
				}
		return blobCnt > 0;
	}

	#ifdef HAS_WINDOW

		float	WorldData::GetAverageLightLevel(const LCellIdI&id) const
		{
			#ifdef HYBRID_GRID
				const UINT32 high = Grid::low.grid.GetCell(id.stackID).higherEquivalent;
				const BaseGrid::TCellNeighborhood&n = Grid::high.grid.GetCell(high).neighborhood;
				float h = cellStack[high].GetCellI(id.height).GetLightLevel();
				for (int j = 0; j < n.numNeighbors; j++)
					h += cellStack[n.neighborID[j]].GetCellI(id.height).GetLightLevel();
				return h / (1 + n.numNeighbors);
			#else
				return cellStack[id.stackID].GetCellI(id.height).GetLightLevel();
			#endif
		}

		float	WorldData::GetHighestLightLevel(const LCellIdI&id) const
		{
			#ifdef HYBRID_GRID
				const UINT32 high = Grid::low.grid.GetCell(id.stackID).higherEquivalent;
				const BaseGrid::TCellNeighborhood&n = Grid::high.grid.GetCell(high).neighborhood;
				float h = cellStack[high].GetCellI(id.height).GetLightLevel();
				for (int j = 0; j < n.numNeighbors; j++)
					h = std::max(h,cellStack[n.neighborID[j]].GetCellI(id.height).GetLightLevel());
				return h;
			#else
				return GetAverageLightLevel(id);
			#endif
		}



		void WorldData::CompileResources(THull<ResourceVertex>&cover, index_t offset, count_t count, FClipHeight clipHeight, const Resource*resources, count_t numResources) const
		{
			const index_t end = offset + count;
			SpinLock lock;
			Concurrency::parallel_for(offset, end, ResourceCompiler::BatchSize,[this,end,&lock, &cover, clipHeight,resources, numResources](index_t i)	//looks good - at least in release
			{
				BYTE data[sizeof(ResourceCompiler)];	//prevent implicit memset in release mode. i'm serious. visual studio has some funny ideas there...
				ResourceCompiler&compiler = *(ResourceCompiler*)data;
				compiler.Setup(cellStack.pointer(),low.grid.GetCells(),high.grid.GetCells(), low.cells.pointer(),clipHeight,resources, numResources);
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
				bool anyOpen = !stacks[n0].GetCellU(ih + 1).IsSolid() || !stacks[n1].GetCellU(ih + 1).IsSolid();

				coverMesh << OverlayVertex(cell.cornerVertex[j], h, anyOpen ? 0.45f : 0.f);

				coverMesh << 0;
				coverMesh << 1 + (UINT16)j;
				coverMesh << 1 + (UINT16)(j + 1) % cell.numCorners;
			}	
		}

		float4	TerrainCompiler::GetMixColorU(index_t stackIndex,  UINT ih, Content oppositeContent, float&brightness, Content&outContent) const
		{
			float4 rs = stacks[stackIndex].GetMixColorU(ih,oppositeContent, brightness, outContent);
			return rs;
		}

		float4	TerrainCompiler::GetDownMixColorU(index_t stackIndex,  UINT ih, Content oppositeContent, float&brightness, Content&outContent) const
		{
			float4 rs = stacks[stackIndex].GetDownMixColorU(ih, oppositeContent, brightness, outContent);
			return rs;
		}



		/*static*/ bool TerrainCompiler::AffectedByEMP(index_t stackIndex)
		{
			return FlagsAffectedByEMP(Grid::high.cells[stackIndex].cornerFlags);
		}

		/*static*/ void TerrainCompiler::ApplyEMP(float intensity,TVec3<>&rs)
		{
			if (intensity > 0.f)
			{
				TVec3<> rs2;
				if (rs.r < 0)
					rs2 = rs * -0.4f + 0.6f;
				else
					rs2 = rs * 0.4f + 0.6f;
				rs2.r *= 0.7f;
				rs2.g *= 0.85f;
				if (rs.r < 0)
					rs2 = -rs2;
				Vec::interpolate(rs,rs2,intensity,rs);

			}
		}



		float3	CellStack::Cell::GetBaseColor() const
		{
			float3 color = typeColor[content]; 
			//if (submergence > 0)
				//color = float3(0.1f);
			//Vec::abs(color);color *= 0.5f;

			color.r *= powi(0.8f, submergence); 
			color.g *= powi(0.85f, submergence);
			color.b *= powi(0.95f, submergence); 
			#ifndef DEBUG_HIDE_SENSORY
				if (visible <= 0)
				{
					//float avg = (color.r + color.g + color.b) / 3.f * 0.5f + 0.5f;
					if (color.r < 0)
					{
						color.r = -(1.f - (1.f + color.r) / 4.f);
						color.g = -(1.f - (1.f + color.g) / 4.f);
						color.b = -(1.f - (1.f + color.b) / 4.f);
					}
					else
					{
						color.r = 1.f - (1.f - color.r) / 4.f;
						color.g = 1.f - (1.f - color.g) / 4.f;
						color.b = 1.f - (1.f - color.b) / 4.f;
					}
					//color.r = (color.r + typeColor[0].r*2) / 3.f;
					//color.g = (color.g + typeColor[0].g*2) / 3.f;
					//color.b = (color.b + typeColor[0].b*2) / 3.f;
				}
			#endif
			return color; 
		}



		void	TerrainCompiler::CompileCap(const UINT ih,const BaseGrid::TNetCell&cell, const bool capped, const CellStack::Cell&thisCell, const TFloatRange<>&range,const BaseGrid::TCellNeighborhood&neighbors, const CellStack&stack)
		{

			bool mixes0;
			Content centerType;
			TVec4<> color0 = stack.GetColorU(ih, Content::Clear, mixes0,centerType);
			float emp0 = FlagsAffectedByEMP(cell.cornerFlags);
			
			float4 c0 = capped ? float4() : color0;
			ApplyEMP(emp0,c0.rgb);
			solidMesh << TerrainVertex(cell.centerVertex, range.max, cell.centerVertex.xyz, c0, thisCell.GetStructureFactor());
			for (int j = 0; j < cell.numCorners; j++)
			{
				{
					float brightness = color0.a;
					TVec4<> color;

					float emp = emp0;
					if (capped)
					{
						//brighness = 0.f;
						color = float4(0,0,0,1);
						brightness = 0;
					}
					else
					{
						Vec::clear(color);
						Content c0,c1;
						index_t n1 = neighbors.neighborID[(j + 1) % cell.numCorners];
						index_t n2 = neighbors.neighborID[j];
						color += GetMixColorU(n1,ih,centerType,brightness,c0);
						color += GetMixColorU(n2, ih, centerType,brightness,c1);
						emp += AffectedByEMP(n1);
						emp += AffectedByEMP(n2);
						emp /= 3;

						float4 add(color0.rgb, 1.f);
						if (!mixes0)
						{
							 //mild bleeding...
							if (c0 != centerType)
								add *= 2.f;
							if (c1 != centerType)
								add *= 2.f;
						}
						color += add;


						color.rgb /= color.a;
						brightness /= 3;
						//color.rgb *= brightness / color.a;
						//brightness /= color.a;
					}
					ApplyEMP(emp,color.rgb);

					solidMesh << TerrainVertex(cell.cornerVertex[j], range.max, cell.cornerVertex[j].xyz, float4(color.rgb,brightness), 1.f);
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
			TVec4<> color0 = stack.GetDownColorU(ih, Content::Clear, mixes0,centerType);
			float emp0 = FlagsAffectedByEMP(cell.cornerFlags);

			{
				float4 c0 = color0;
				ApplyEMP(emp0,c0.rgb);
				solidMesh << TerrainVertex(cell.centerVertex, h, -cell.centerVertex.xyz, c0, thisCell.GetStructureFactor());
			}
			for (int j = 0; j < cell.numCorners; j++)
			{
				{
					float brightness = color0.a;
					float4 color(color0.rgb, 1.f);
					float emp = emp0;
					{
						if (!mixes0)
							color *= 4.f; //mild bleeding
						Content c0,c1;
						index_t n1 = neighbors.neighborID[(j + 1) % cell.numCorners];
						index_t n2 = neighbors.neighborID[j];
						color += GetDownMixColorU(n1,ih + 1, centerType,brightness,c0);
						color += GetDownMixColorU(n2,ih + 1, centerType,brightness,c1);
						emp += AffectedByEMP(n1);
						emp += AffectedByEMP(n2);
						color.rgb /= color.a;
						brightness /= 3;
						emp /= 3;
						//color.rgb *= brightness / color.a;
						//brightness /= color.a;
					}
					ApplyEMP(emp,color.rgb);
					solidMesh << TerrainVertex(cell.cornerVertex[j], h, -cell.cornerVertex[j].xyz, float4(color.rgb,brightness), 1.f);
				}
				solidMesh << 0 << 1 + (UINT16)(j + 1) % cell.numCorners << 1 + (UINT16)j;
			}	
		}

		void	TerrainCompiler::CompileFaceToNeighbor(const int j, const UINT ih, const int ir,const BaseGrid::TNetCell&cell,
												const BaseGrid::TCellNeighborhood&neighbors,TVec4<> newCornerColor[6], const CellStack::Cell&thisCell, const CellStack::Cell&lastCell,
												const TFloatRange<>&range )
		{
			const UINT32 n0 = neighbors.neighborID[j];
			const UINT32 n1 = neighbors.neighborID[(j + 1) % cell.numCorners];
			const UINT32 n2 = neighbors.neighborID[(j + 2) % cell.numCorners];
			const CellStack&sn = stacks[n1];
			const bool filled = sn.GetCellU(ih).IsSolid();
			//bool wasEmpty = false;
			const bool ncutoff = IsClipped(clipHeight, n1, ir);
			{
				TVec4<>	color1 = newCornerColor[j],
						color2 = newCornerColor[(j + 1) % cell.numCorners],
						lowerColor1 = wasEmpty ? newCornerColor[j] : cornerColor[j],
						lowerColor2 = wasEmpty ? newCornerColor[(j + 1) % cell.numCorners] : cornerColor[(j + 1) % cell.numCorners];

				if (ncutoff && filled)
				{
					color1 = float4();
					color2 = float4();
					lowerColor1 = float4();
					lowerColor2 = float4();
				}
				if (!thisCell.Mixes() || !lastCell.Mixes())
				{
					lowerColor1.xyz = color1.xyz;// * lowerColor1.w;
					lowerColor2.xyz = color2.xyz;// * lowerColor2.w;
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

			

			if (!thisCell.IsSolid())
			{
				wasEmpty = true;
				//for (int j = 0; j < cell.numCorners; j++)
					//cornerColor[j] = float4(1);
				const bool capped = nextCell.IsSolid();
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

				TVec3<> baseColor = thisCell.GetBaseColor();
				ApplyEMP(FlagsAffectedByEMP( cell.cornerFlags), baseColor);
				TVec3<> nextColor = nextCell.IsSolid() && nextCell.Mixes() ? nextCell.GetBaseColor() : baseColor;
				ApplyEMP(FlagsAffectedByEMP( cell.cornerFlags), nextColor);

				const bool capped = nextCell.IsSolid();
				if (!capped
					|| ir+1 >= clipH
					)
				{
					CompileCap(ih+1,cell,capped,thisCell,range,neighbors,stack);
				}

				TVec4<> newCornerColor[6];

				{
					TVec3<> color = thisCell.Mixes() ? nextColor : baseColor;
					for (int j = 0; j < cell.numCorners; j++)
					{
						const UINT32 n0 = neighbors.neighborID[j];
						const UINT32 n1 = neighbors.neighborID[(j + 1) % cell.numCorners];
						const CellStack&sn = stacks[n1];
						{
							float c1 = (stacks[n1].GetCellU(ih).GetLightLevel() + stacks[n1].GetCellU(ih + 1).GetLightLevel()) / 2.f;
							float color1 = (c1 * 2.f
								+
								(Brighten(stacks[n0].GetCellU(ih).GetLightLevel()) + Brighten(stacks[n0].GetCellU(ih + 1).GetLightLevel())) / 2.f);	//original 'mild' levels

							color1 /= 3.f;

							newCornerColor[j].xyz = color;
							newCornerColor[j].w = color1;
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
					TVec4<> color0 = stack.GetDownColorU(0, Content::Clear,mixes0,centerContent);
					solidMesh << TerrainVertex(cell.centerVertex, h, -cell.centerVertex.xyz, color0, thisCell.GetStructureFactor());
					for (int j = 0; j < cell.numCorners; j++)
					{
						{
							float brightness = color0.a;
							float4 color(color0.rgb, 1.f);

							{
								Content c0,c1;

								if (!mixes0)
									color *= 4.f; //mild bleeding
								color += stacks[neighbors.neighborID[(j + 1) % cell.numCorners]].GetDownMixColorU(0, centerContent,brightness,c0);
								color += stacks[neighbors.neighborID[j]].GetDownMixColorU(0, centerContent,brightness,c1);

								color.rgb /= color.a;
								brightness /= 3;
								//color.rgb *= brightness / color.a;
								//brightness /= color.a;
							}
							solidMesh << TerrainVertex(cell.cornerVertex[j], h, -cell.cornerVertex[j].xyz, float4(color.rgb,brightness), 1.f);
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

				const TVec3<> baseColor = thisCell.GetBaseColor();
				const TVec3<> nextColor = nextCell.IsSolid() && nextCell.Mixes() ? nextCell.GetBaseColor() : baseColor;
				const bool capped = nextCell.IsSolid();
				if (!capped
					)
				{
					CompileCap(0,cell,capped,thisCell,range,neighbors,stack);
				}


			}
		
		}


		void	TerrainCompiler::Compile(const index_t offset, const index_t end, THull<TerrainVertex>&solidHull, THull<OverlayVertex>&coverHull, SpinLock&lock)
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

			if (solidMesh.IsNotEmpty() || coverMesh.IsNotEmpty())
			{
				lock.lock();

				if (solidMesh.IsNotEmpty())
				{
					solidMesh.AppendTo(solidHull);
				}

				if (coverMesh.IsNotEmpty())
				{
					coverMesh.AppendTo(coverHull);
				}

				lock.unlock();
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
				//const TVec3<> nextColor = nextCell.IsSolid() && nextCell.Mixes() ? nextCell.GetBaseColor() : baseColor;
				//const bool capped = nextCell.IsSolid();
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
			//const bool filled = sn.GetCellU(ih).IsSolid();
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

		void	WaterCompiler::CompileCap(const UINT ih,const BaseGrid::TNetCell&cell, const bool capped, const CellStack::Cell&thisCell, const TFloatRange<>&range,const BaseGrid::TCellNeighborhood&neighbors, const CellStack&stack, bool empThis)
		{

			float light0 = stack.GetCellU(ih).GetWaterLightLevel();
		
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
						brightness += sn1.GetCellU(ih).GetWaterLightLevel();
						brightness += sn2.GetCellU(ih).GetWaterLightLevel();

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
						float c1 = (stacks[n1].GetCellU(ih).GetWaterLightLevel() + stacks[n1].GetCellU(ih+1).GetWaterLightLevel())/2.f;
						float color1 = (c1 * 2.f
							+
							(Brighten(stacks[n0].GetCellU(ih).GetWaterLightLevel()) + Brighten(stacks[n0].GetCellU(ih+1).GetWaterLightLevel())) / 2.f);	//originally 'mild' levels

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
						CompileBottomLayer(stack,100,netCells[i],gridCells[i].neighborhood,empThis);

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
					CompileBottomLayer(stack,clipH,cell,neighbors,empThis);
				for (UINT ih = waterRange.start; ih < waterRange.end; ih++)
				{
					CompileLayer(i,ih,stack,clipH,cell,neighbors,empThis);
				}
			}

			if (mesh.IsNotEmpty())
			{
				lock.lock();

				mesh.AppendTo(coverHull);

				lock.unlock();
			}
		}

	#endif
		//static Resource::coverage_t	GetResourceCoverage(const TVec3<>&seed, const TVec3<>&center0,float rarity,Resource::coverage_t maxYield)
		//{
		//	TVec3<> center = center0 * 6.f;
		//	float rs = 0;

		//	float strength = 1.f;
		//	for (int i = 0; i < 5; i++)
		//	{
		//		center *= 2.f;
		//		strength /= 2.f;
		//		rs += (0.5f + 0.5f * noise.Get(seed.x + center.x, seed.y + center.y, seed.z + center.z)* 1.6f)*strength;
		//	}
		//	rs = clamp(rs,0,1);
		//	//rs *= pow(1.1f,probabilityModifier);
		//	//rs = pow(rs,1.f - 0.03f * probabilityModifier);
		//	//rs = std::min(1.f, rs * 1.4f);	//dont know, dont care

		//	rs = /*sqr*/(std::max(0.f,rs-rarity) / (1.f - rarity)) * (0.75f + 0.25f * noise.Get(seed.y + center0.x, seed.z + center0.y, seed.x + center0.z));
		//	int cv = (int)(rs * maxYield);
		//	if (cv > 0)
		//		cv = std::min<int>(maxYield,cv * 20);	//was * 2
		//	return (Resource::coverage_t)cv;
		//}
	#ifdef HAS_WINDOW
	
		void	ResourceCompiler::Compile(const index_t offset, const index_t end, THull<ResourceVertex>&coverHull, SpinLock&lock)
		{
			for (index_t i = offset; i < offset+BatchSize && i < end; i++ )
			{
				//const ::Grid::CellStack&stack = stacks[i];
				if (!this->resources[0].known[i])	//
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
				UINT layer = actualWorld.cellStack[fine].highestOpaqueCell;
				const auto&fineStack = actualWorld.cellStack[fine];
				if (fineStack.waterRange.IsNotEmpty())
					layer = std::max<UINT>(fineStack.waterRange.end-1, layer);
				const int clipH = clipHeight(fine);
				const UINT clipL = CellStack::ToLayer(clipH);
				layer = std::min(layer, clipL-1);
				while (layer > 0 && !fineStack.volumeCell[layer].IsSolid() && !fineStack.volumeCell[layer].IsWater())
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
	#endif

	/*static*/ Resource::coverage_t			Resource::Sample(const String&resourceName, index_t stackID)
	{
		ASSERT_LESS__(stackID, ARRAYSIZE(((Resource*)nullptr)->coverage));
		foreach (all,res)
			if (res->name.equalsIgnoreCase(resourceName))
				return res->coverage[stackID];
		FATAL__("Unable to locate resource '"+resourceName+'"');	//may be disabled later when resources are fully dynamic
		return 0;
	}
	/*static*/ Resource::coverage_t			Resource::Sample(ConstName resourceName, index_t stackID)
	{
		ASSERT_LESS__(stackID, ARRAYSIZE(((Resource*)nullptr)->coverage));
		foreach (all,res)
			if (res->cname == resourceName)
				return res->coverage[stackID];
		FATAL__("Unable to locate resource #"+String(int(resourceName)));	//may be disabled later when resources are fully dynamic
		return 0;
	}

	/*static*/ Resource&		Resource::Require(const String&name)
	{
		foreach (all,res)
			if (res->name.equalsIgnoreCase(name))
				return *res;

		FATAL__("Unable to locate resource '"+name+'"');
		return all.first();
	}

	/*static*/ Resource&		Resource::Require(ConstName name)
	{
		foreach (all,res)
			if (res->cname == name)
				return *res;

		FATAL__("Unable to locate resource #"+String(int(name)));
		return all.first();
	}


	///*static*/ void Resource::Create(const String&name, ConstName cname, const float3&color, const TVec3<>&seed,float rarity, float maxYield)
	//{
	//	// low.cells[i].centerVertex.xyz,
	//	Resource&rc = Resource::all.Append();
	//	rc.Name(name,cname,color);
	//	for (index_t i = 0; i < LowSetup::GridType::CellCount; i++)
	//	{
	//		#ifndef CLIENT
	//			rc.coverage[i] = GetResourceCoverage(seed, low.cells[i].centerVertex.xyz,rarity,maxYield);
	//		#else
	//			rc.coverage[i] = 0;
	//			rc.known[i] = false;
	//		#endif

	//	}
	//}

	/*static*/ Resource::coverage_t		Resource::GetRandomCoverage(const float3&seed, const float3&direction, float p, coverage_t max)
	{
		#ifdef GRID_INDEPENDENT_RANDOM
			static Random random;
			float q = 1.f - p;
			coverage_t rs (Round(sqr(sqr(sqr(std::max(0.f,(random.getFloat()-q)/p)))) * (max-1)));
			if (rs)
				rs++;
			return rs;
		#else
		//	{0.08,0.08,0,0,0.008 } ;


			float t0 = fabs(noise.Get(direction + seed,8.f));	//wide areas
			float t1 = fabs(noise.Get(direction + seed,M_PIF*5.f));	//small areas
			float t2 = fabs(noise.Get(direction + seed+3.f,9.f));	//cutout

			float common = std::min(1.f,(p/0.06f));
			t0 = std::min(1.f,t0*(t0*1.9f)*common);
			t1 = std::min(1.f,t1*(t1*1.9f)*(1.1f-0.2f * common));


			float t = std::max(t0,t1);

			t *= smoothstep(0.2f,0.3f,t2);


			//float t = t0 * (0.3f + 0.7f * t1);
//			float q = 0.26f-sqrt(p) * 0.5f;
			float q = 0.9f-sqrt(p) * 1.5f;
			p = 0.30f;// + 0.1f * t0;

			float h = 0.15f + (1.f - t0)*0.85f;
			//p = 0.75f;
			//float q = 1.f - p;
			coverage_t rs (Round(/*sqr*/(/*sqr*/(/*sqr*/(clamp((t-q)/p *h,0,1)))) * (max-1)));
			if (rs)
				rs++;
			return rs;
		#endif
	}


	/*static*/ void Resource::Create(const String&name, ConstName cname, const float3&color, float p, coverage_t max)
	{
		// low.cells[i].centerVertex.xyz,
		Resource&rc = Resource::all.Append();
		rc.Name(name,cname,color);
		static Random random;
		float3 seed(random.getFloat(-10.f,10.f),random.getFloat(-10.f,10.f),random.getFloat(-10.f,10.f));

		for (index_t i = 0; i < LowSetup::GridType::CellCount; i++)
		{
			#ifndef CLIENT
				rc.coverage[i] = Resource::GetRandomCoverage(seed, Grid::low.cells[i].centerVertex.xyz,  p,max);
			#else
				rc.coverage[i] = 0;
				rc.known[i] = false;
			#endif
		}

		#ifndef CLIENT
			if (p <= 0.f || !max)
				return;

			LogEvent(EventType::SessionRelated,nullptr,"Equalizing resource distribution of "+name);
			count_t familyTotal[BaseGrid::NumFamilies] = {0}, total = 0;

			for (index_t i = 0; i < LowSetup::GridType::CellCount; i++)
			{
				familyTotal[Grid::low.grid.GetCell(i).family] += rc.coverage[i];
				total += rc.coverage[i];
			}

			float avgPerCell = float(total) / float(LowSetup::GridType::CellCount);
			LogEvent(EventType::SessionRelated,nullptr,"Average per cell estimated at "+String(avgPerCell));
			for (int k = 0; k < BaseGrid::NumFamilies; k++)
			{
				LogEvent(EventType::Debug,nullptr,"Processing family "+String(k));
				const count_t count = Grid::low.grid.CountFamilyMembers(k);
				const index_t offset = Grid::low.grid.GetFamilyOffset(k);
				float want = avgPerCell * count;
				
				while (want * 1.5f < familyTotal[k] || want * 0.75f > familyTotal[k])
				{
					LogEvent(EventType::Debug,nullptr," Family found out of range. Randomizing...");
					float3 seed = float3(random.getFloat(-10.f,10.f),random.getFloat(-10.f,10.f),random.getFloat(-10.f,10.f));
					familyTotal[k] = 0;
					for (index_t i = offset; i < offset + count; i++)
					{
						rc.coverage[i] = Resource::GetRandomCoverage(seed, Grid::low.cells[i].centerVertex.xyz,  p,max);
						familyTotal[k]+=rc.coverage[i];
					}

				}

				
				
				//float p2 = p;
				//coverage_t cv = max;

				//while (familyTotal[k] == 0)
				//{
				//	LogEvent(EventType::SessionRelated,nullptr," Family has no coverage. Trying to increase...");
				//	p2 += 0.001f;
				//	p2 *= 1.1f;
				//	cv ++;

				//	familyTotal[k] = 0;
				//	for (index_t i = offset; i < offset + count; i++)
				//	{
				//		rc.coverage[i] = Resource::GetRandomCoverage(seed, Grid::low.cells[i].centerVertex.xyz,  p2,cv);
				//		familyTotal[k]+=rc.coverage[i];
				//	}
				//}


				//float scale = want / familyTotal[k];
				//LogEvent(EventType::SessionRelated,nullptr," Scale determined at "+String(scale));
				//for (index_t i = offset; i < offset + count; i++)
				//{
				//	rc.coverage[i] *= scale;
				//}
			}


		#endif



	}

	/*static*/ void Resource::Create(const String&name, ConstName cname, const float3&color, index_t resIndex)
	{
		ASSERT_LESS__(resIndex, Objects::Resource::NumberOfPossibilities);
		Create(name,cname,color,probability[resIndex],maximum[resIndex]);
	}

	/*static*/ void Resource::Create(const String&name, ConstName cname, const float3&color, coverage_t func(UINT32 stackID))
	{
		// low.cells[i].centerVertex.xyz,
		Resource&rc = Resource::all.Append();
		rc.Name(name,cname,color);
		for (index_t i = 0; i < LowSetup::GridType::CellCount; i++)
		{
			#ifndef CLIENT
				rc.coverage[i] = func((UINT32)i);
			#else
				rc.coverage[i] = 0;
				rc.known[i] = false;
			#endif

		}
	}


	#ifdef HAS_WINDOW
	void	WorldData::UpdateLighting()
	{
		lightingOutdated = false;
		Timer::Time t0 = timer.Now();

		UINT32 top = 0,bottom = CellStack::Range;
		count_t cnt = 0;
		foreach (cellStack,stack)
		{
			if (stack->lightingChangedTo > stack->lowestOpenCell)
			{
				cnt ++;
				top = std::max<UINT32>(top,stack->lightingChangedTo);
				bottom = std::min<UINT32>(bottom,stack->lowestOpenCell > 0 ? stack->lowestOpenCell-1 : 0);
			}
		}
		if (cnt < 512)
		{
			static Buffer<UINT32>	ids;
			ids.Clear();

			foreach (cellStack,stack)
			{
				if (stack->lightingChangedTo > stack->lowestOpenCell)
				{
					ids << stack - cellStack.pointer();
				}
			}


			UINT32 tp = std::min(top,CellStack::Range-1);
			for (UINT32 h = tp; /*h > 0 && */h >= bottom && h <= tp; h--)
			{
				Concurrency::parallel_for(index_t(0), ids.Count(),[this,h](index_t _i)
				{
					const UINT32 i = ids[_i];
					CellStack&thisStack = cellStack[i];
					if (h > thisStack.lightingChangedTo)
						return;
					int l = 0xFF;
				
					if (h < CellStack::Range-1)
					{
						l = thisStack.volumeCell[h+1].intLightLevel;
						const auto&n = high.grid.GetCell(i).neighborhood;
						for (int j = 0; j < n.numNeighbors; j++)
							l += cellStack[n.neighborID[j]].volumeCell[h+1].intLightLevel;
						l /= 1 + n.numNeighbors;
					}
					thisStack.volumeCell[h].SetIntLightLevel(l);
					BYTE next = h < CellStack::Range-1 ? thisStack.volumeCell[h+1].GetIncreasedSubmergence() : 0;
					//ASSERT__(h >= CellStack::Range-1 || h >= CellStack::HalfRange || next || thisStack.volumeCell[h+1].IsSolid());
					thisStack.volumeCell[h].SetSubmergence(next);
				});
			}
			foreach (cellStack,stack)
				stack->lightingChangedTo = 0;

		}
		else
		{
			UINT32 tp = std::min(top,CellStack::Range-1);
			for (UINT32 h = tp; /*h > 0 && */h >= bottom && h <= tp; h--)
			{
				Concurrency::parallel_for(index_t(0), HighSetup::GridType::CellCount,[this,h](index_t i)
				{
					CellStack&thisStack = cellStack[i];
					if (h > thisStack.lightingChangedTo)
						return;
					int l = 0xFF;
				
					if (h < CellStack::Range-1)
					{
						l = thisStack.volumeCell[h+1].intLightLevel;
						const auto&n = high.grid.GetCell(i).neighborhood;
						for (int j = 0; j < n.numNeighbors; j++)
							l += cellStack[n.neighborID[j]].volumeCell[h+1].intLightLevel;
						l /= 1 + n.numNeighbors;
					}
					thisStack.volumeCell[h].SetIntLightLevel(l);
					thisStack.volumeCell[h].SetSubmergence(h < CellStack::Range-1 ? thisStack.volumeCell[h+1].GetIncreasedSubmergence() : 0);
				});
			}
			foreach (cellStack,stack)
				stack->lightingChangedTo = 0;
		}

		LogEvent(EventType::PerformanceMeasurement,  nullptr, "Updated lighting in "+String(timer.GetSecondsSince(t0))+" seconds");
	
	
	}

	HighSetup::TDistanceField	field;
	void	WorldData::SignalLightingChange(const HCellIdI&stackID)
	{
		lightingOutdated = true;
		const UINT r = CellStack::ToLayer(stackID.height)+1;
		high.UpdatePlanarDistanceField(stackID.stackID,r,field);
		for (index_t i = 0; i < HighSetup::TDistanceField::NumEntries; i++)
			if (field.distance[i] != 0xFFFFFFFF)
			{
				cellStack[i].lightingChangedTo = r-field.distance[i];
				high.family[high.grid.GetCell(i).family].SetOutdated(0);
			}
	}

		#ifdef HYBRID_GRID
			void	WorldData::SignalLightingChange(const LCellIdI&stackID)
			{
				lightingOutdated = true;
				const UINT r = CellStack::ToLayer(stackID.height) + 2;
				UINT32 higher = low.grid.GetCell(stackID.stackID).higherEquivalent;
				high.UpdatePlanarDistanceField(higher,CellStack::ToLayer(stackID.height),field);
				for (index_t i = 0; i < HighSetup::TDistanceField::NumEntries; i++)
					if (field.distance[i] != 0xFFFFFFFF)
					{
						cellStack[i].lightingChangedTo = std::max<BYTE>( cellStack[i].lightingChangedTo, r-field.distance[i]);
						high.family[high.grid.GetCell(i).family].SetOutdated(0);
					}
			}
		#endif

	#endif


	void	WorldData::Explode(const HCellIdI&id,float radius, bool*familyChanged)
	{
		const float3 center = high.cells[id.stackID].centerVertex.xyz * ToRadiusf(0.5f + id.height);
		const float r2 = sqr(radius);
		Queue<UINT32> queue;
		
		#ifdef HAS_WINDOW
			Queue<UINT32> changed;
		#endif
		queue << id.stackID;

		Box<> b;
		b.SetCenter(center,radius);

		bool processed[HighSetup::GridType::CellCount] = {false};


		UINT32 current;
		while (queue >> current)
		{
			if (processed[current])
				continue;
			processed[current] = true;
			const TVec3<>&dir = high.cells[current].centerVertex.xyz;
			TVec3<> lower = dir * ToRadius(-CellStack::HalfRange);
			TVec3<> upper = dir * ToRadius(CellStack::HalfRange);
			float dummy = Vec::distance(upper,lower);
			if (b.Contains(lower) || b.Contains(upper) || _oDetectOpticalBoxIntersection(b,lower,(upper-lower)/dummy,dummy))
			{
				bool anyChanged = false;
				for (int h = -CellStack::HalfRange; h < CellStack::HalfRange; h++)
				{
					TVec3<> at = dir * ToRadius(h);
					if (Vec::quadraticDistance(center,at) < r2)
					{
						if (cellStack[current].MakeLocalHole(h))
						{
							UINT layer = CellStack::ToLayer(h);
							#ifdef HAS_WINDOW
								cellStack[current].lightingChangedTo = std::max<BYTE>(cellStack[current].lightingChangedTo, layer);
							#endif
							//ASSERT_LESS__(layer,CellStack::Range);

							//SignalLightingChange(HCellIdI(current,h+1));
							anyChanged = true;
						}
					}
				}
				if (anyChanged)
				{
					#ifdef HAS_WINDOW
						changed << current;
					#endif
					familyChanged[high.grid.GetCell(current).family] = true;
				}

				const auto&n = high.grid.GetCell(current).neighborhood;
				for (BYTE k = 0; k < n.numNeighbors; k++)
				{
					queue << n.neighborID[k];
				}
			}
		}
		//count_t counter = 0;
		//BYTE smallest = 0xff;
		#ifdef HAS_WINDOW
			while (changed >> current)
			{
				BYTE h = cellStack[current].lightingChangedTo;
				//ASSERT_LESS__(h,CellStack::Range);
				//if (h+1 >= cellStack[current].lowestOpenCell)
				if (!h)
					continue;
				h--;
				//smallest = std::min(smallest,h);
				//counter++;

		
				const auto&n = high.grid.GetCell(current).neighborhood;
				for (BYTE k = 0; k < n.numNeighbors; k++)
				{
					UINT32 nid = n.neighborID[k];
					if (cellStack[nid].lightingChangedTo < h)
					{
						cellStack[nid].lightingChangedTo = h;
						changed << nid;
					}
				}
			}
		#endif
		//ShowMessage(counter);
		//ShowMessage(smallest);

	
	}

	void	WorldData::Explode(const HCellIdI&id,float radius, float atTime)
	{
		bool familyChanged[BaseGrid::NumFamilies] = {false};
		Explode(id,radius,familyChanged);
		for (index_t i = 0; i < BaseGrid::NumFamilies; i++)
			if (familyChanged[i])
			{
				high.family[i].SetOutdated(atTime);
				low.family[i].SetOutdated(atTime);
				#ifdef HAS_WINDOW
					lightingOutdated = true;
				#endif
			}

	}

	bool	IsHighPartOfLow(UINT32 highCell, UINT32 lowCell)
	{
		#ifdef HYBRID_GRID
			if (highCell >= HighSetup::GridType::CellCount || lowCell >= LowSetup::GridType::CellCount)
				return false;
			UINT32 lowEq = low.grid.GetCell(lowCell).higherEquivalent;
			if (lowEq == highCell)
				return true;
			const auto&n = high.grid.GetCell(lowEq).neighborhood;
			for (int i = 0; i < n.numNeighbors; i++)
				if (n.neighborID[i] == highCell)
					return true;
			return false;
		#else
			return highCell == lowCell;
		#endif
	}
	
	float	GetGeometricDistance(const LCellIdI&a, const LCellIdI&b)
	{
		return sqrt(GetQuadraticGeometricDistance(a,b));
	}
	float	GetQuadraticGeometricDistance(const LCellIdI&a, const LCellIdI&b)
	{
		#ifndef _DEBUG
			ASSERT_LESS__(a.stackID,low.cells.Count());	//just to be sure ... for now
			ASSERT_LESS__(b.stackID,low.cells.Count());
		#endif
		TVec3<> wa = low.cells[a.stackID].centerVertex.xyz * ToRadius(a.height);
		TVec3<> wb = low.cells[b.stackID].centerVertex.xyz * ToRadius(b.height);
		return Vec::quadraticDistance(wa,wb);
	}

	bool IsRoad(BYTE /*CellStack::Cell::Content::type_t*/ t, bool clearIsRoad)
	{
		return (clearIsRoad && t == Content::Clear) || t == Content::Road || t == Content::Foundation;
	}

	bool	WorldData::IsRoad(const LCellIdI&cell, bool ignoreClearCellsOnEdge) const
	{
		#ifdef HYBRID_GRID
			const UINT32 lowEq = low.grid.GetCell(cell.stackID).higherEquivalent;

			if (!Grid::IsRoad(cellStack[lowEq].GetCellI(cell.height).content,false))
				return false;
			const auto&n = high.grid.GetCell(lowEq).neighborhood;
			for (int i = 0; i < n.numNeighbors; i++)
				if (!Grid::IsRoad(cellStack[n.neighborID[i]].GetCellI(cell.height).content,ignoreClearCellsOnEdge))
					return false;
			return true;
		#else
			return Grid::IsRoad(cellStack[cell.stackID].GetCellI(cell.height).content);
		#endif
	}



	void	WorldData::ApplyFamilyDelta(BYTE family, const Array<CellStack::TTypeDeltaBlock>&data0, bool initialization, bool actualWorld)
	{
		ASSERT_LESS__(family, BaseGrid::NumFamilies);
		const index_t offset = high.grid.GetFamilyOffset(family);
		const count_t range = high.grid.CountFamilyMembers(family);

		#ifdef HAS_WINDOW
			Queue<index_t>	changed;
		#endif
		bool anyChanged = false;
		if (data0.IsNotEmpty())
		{
			index_t at = 0;
			foreach (data0,block)
			{
				for (index_t i = 0; i < block->repitition; i++)
				{
					index_t stackIndex = (at % range) + offset;
					index_t layer = at / range;

					CellStack&stack = cellStack[stackIndex];

					#ifdef _DEBUG
						if (initialization)
						{
							ASSERT_EQUAL__(stack.volumeCell[layer].content, 0);
							ASSERT_EQUAL__(stack.volumeCell[layer].structureCounter, 0);
						}
					#endif
					if (stack.volumeCell[layer].ApplyDelta(block->value))
					{
						anyChanged = true;
						#ifdef HAS_WINDOW
							stack.lightingChangedTo = std::max<BYTE>(stack.lightingChangedTo,(BYTE)(layer+1));
						#endif
					}
				
					at++;
				}
			
			}
			ASSERT_EQUAL__(at, range* CellStack::Range);
		}
		//if (data1.IsNotEmpty())
		//{
		//	index_t at = 0;

		//	foreach (data1,block)
		//	{
		//		for (index_t i = 0; i < block->repitition; i++)
		//		{
		//			index_t stackIndex = (at % range) + offset;
		//			index_t layer = at / range;

		//			CellStack&stack = cellStack[stackIndex];

		//			if (stack.volumeCell[layer].ApplyDelta(block->value))
		//			{
		//				anyChanged = true;
		//				#ifdef HAS_WINDOW
		//					stack.lightingChangedTo = std::max<BYTE>(stack.lightingChangedTo,(BYTE)(layer+1));
		//				#endif
		//			}
		//		
		//			at++;
		//		}
		//	}
		//	ASSERT_EQUAL__(at, range* CellStack::Range);
		//	
		//}
		#ifdef CLIENT
			ASSERT__(actualWorld);	//client only knows one world
		#endif
		if (actualWorld)
		{
			#ifdef HAS_WINDOW
				if (anyChanged)
				{
					for (index_t i = 0; i < range; i++)
						if (cellStack[i+offset].lightingChangedTo > 0)
						{
							cellStack[i+offset].UpdateRange();
							changed << i+offset;
						}
				}
			#endif

			if (anyChanged)
			{
				high.family[family].SetOutdated(0);
				low.family[family].SetOutdated(0);
			}

			#ifdef HAS_WINDOW

				index_t current;
				while (changed >> current)
				{
					BYTE h = cellStack[current].lightingChangedTo;
					//ASSERT_LESS__(h,CellStack::Range);
					//if (h+1 >= cellStack[current].lowestOpenCell)
					{
						high.family[high.grid.GetCell(current).family].SetOutdated(0);
						low.family[high.grid.GetCell(current).family].SetOutdated(0);
						lightingOutdated = true;
					}
					if (!h)
						continue;
					h--;
					//smallest = std::min(smallest,h);
					//counter++;

		
					const auto&n = high.grid.GetCell(current).neighborhood;
					for (BYTE k = 0; k < n.numNeighbors; k++)
					{
						UINT32 nid = n.neighborID[k];
						if (cellStack[nid].lightingChangedTo < h)
						{
							cellStack[nid].lightingChangedTo = h;
							changed << nid;
						}
					}
				}
			#endif
		}
		//ASSERT_CONCLUSION(data,delta);
		
	}
/*
	void	ApplyFamilyDelta(Array<CellStack>&stacks, BYTE family, const Array<CellStack::TTypeDeltaBlock>&data0, const Array<CellStack::TStructCountDeltaBlock>&data1, bool initialization)
	{
		const index_t offset = high.grid.GetFamilyOffset(family);
		const count_t range = high.grid.CountFamilyMembers(family);

		index_t at = 0;
		foreach (data0,block)
		{
			for (index_t i = 0; i < block->repitition; i++)
			{
				index_t stackIndex = (at % range) + offset;
				index_t layer = at / range;

				CellStack&stack = stacks[stackIndex];

				bool rs = stack.volumeCell[layer].ApplyDelta(block->value);
				#ifdef _DEBUG
					if (initialization)
						ASSERT__(rs || !block->value.IsSet());
				#endif
				
				at++;

			}
			
		}
		at = 0;
		foreach (data1,block)
		{
			for (index_t i = 0; i < block->repitition; i++)
			{
				index_t stackIndex = (at % range) + offset;
				index_t layer = at / range;

				CellStack&stack = stacks[stackIndex];

				stack.volumeCell[layer].ApplyDelta(block->value);
				
				at++;
			}
			
		}
	}
*/

	void	WorldData::ApplyFamilyDelta(BYTE family, const Array<CellStack::TTypeDelta>&data0, bool initialization)
	{
		const index_t offset = high.grid.GetFamilyOffset(family);
		const count_t range = high.grid.CountFamilyMembers(family);
		ASSERT_EQUAL__(data0.Count(), range * CellStack::Range);
		//ASSERT_EQUAL__(data1.Count(), range * CellStack::Range);

		#ifdef HAS_WINDOW
			Queue<index_t>	changed;
		#endif
		{
			const CellStack::TTypeDelta*delta = data0.pointer();
			for (index_t i = 0; i < range; i++)
			{
				CellStack&stack = cellStack[i+offset];
				bool anyChanged = false;
				for (index_t j = 0; j < CellStack::Range; j++)
				{
					if (stack.volumeCell[j].ApplyDelta(*delta))
					{
						anyChanged = true;
						#ifdef HAS_WINDOW
							stack.lightingChangedTo = std::max<BYTE>(stack.lightingChangedTo,(BYTE)j+1);
						#endif
					}
					#ifdef _DEBUG
					else
						if (initialization)
							FATAL__("Failed to initialize");
					#endif
					delta ++;
				}
				if (anyChanged)
				{
					#ifdef HAS_WINDOW
						stack.UpdateRange();
						changed << i + offset;
					#endif
				}
			}
			ASSERT_CONCLUSION(data0,delta);
		}
		//{
		//	const CellStack::TStructCountDelta*delta = data1.pointer();
		//	for (index_t i = 0; i < range; i++)
		//	{
		//		CellStack&stack = cellStack[i+offset];
		//		bool anyChanged = false;
		//		for (index_t j = 0; j < CellStack::Range; j++)
		//		{
		//			if (stack.volumeCell[j].ApplyDelta(*delta))
		//			{
		//				anyChanged = true;
		//				#ifdef HAS_WINDOW
		//					stack.lightingChangedTo = std::max<BYTE>(stack.lightingChangedTo,(BYTE)j+1);
		//				#endif
		//			}
		//			delta ++;
		//		}
		//		if (anyChanged)
		//		{
		//			#ifdef HAS_WINDOW
		//				stack.UpdateRange();
		//				changed << i + offset;
		//			#endif
		//		}
		//	}
		//	ASSERT_CONCLUSION(data1,delta);
		//}
		high.family[family].SetOutdated(0);
		low.family[family].SetOutdated(0);



		#ifdef HAS_WINDOW
			index_t current;
			while (changed >> current)
			{
				BYTE h = cellStack[current].lightingChangedTo;
				//ASSERT_LESS__(h,CellStack::Range);
				//if (h+1 >= cellStack[current].lowestOpenCell)
				{
					high.family[high.grid.GetCell(current).family].SetOutdated(0);
					low.family[high.grid.GetCell(current).family].SetOutdated(0);
					lightingOutdated = true;
				}
				if (!h)
					continue;
				h--;
				//smallest = std::min(smallest,h);
				//counter++;

		
				const auto&n = high.grid.GetCell(current).neighborhood;
				for (BYTE k = 0; k < n.numNeighbors; k++)
				{
					UINT32 nid = n.neighborID[k];
					if (cellStack[nid].lightingChangedTo < h)
					{
						cellStack[nid].lightingChangedTo = h;
						changed << nid;
					}
				}
			}

		#endif
	}

	#ifdef HAS_WINDOW

		#ifdef DEBUG_SENSORY
			struct TSensor
			{
				LCellIdI	center;
				float radius;

				bool		operator==(const TSensor&other) const
				{
					return center == other.center && radius == other.radius;
				}
		
			};

			static Buffer0<TSensor> sensors;


		#endif


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
				//LogEvent(EventType::Debug,nullptr,"change2 detected");
				Instruction::lastColor = color_r;
				Instruction::ParaboloidColor_FlatRadius.Update(color_r);
			}
		}

		void		UnsetParaboloidAndSphereDisplay()
		{
			Instruction::displayMode = Instruction::NormalRendering;
		}


		//void	SetSphereDisplay(float3 center, float radius, const float4&color)
		//{
		//	Instruction::useSphereDisplay = true;
		//	float4 def(center,sqr(radius));
		//	if (def != Instruction::lastDef)
		//	{
		//		//LogEvent(EventType::Debug,nullptr,"change1 detected");
		//		Instruction::lastDef = def;
		//		Instruction::sphereDef.Update(def);
		//	}
		//	if (Instruction::lastColor != color)
		//	{
		//		//LogEvent(EventType::Debug,nullptr,"change2 detected");
		//		Instruction::lastColor = color;
		//		Instruction::sphereColor.Update(color);
		//	}
		//}
		//void	UnsetSphereDisplay()
		//{
		//	Instruction::useSphereDisplay = false;
		//}

	#endif

	void	FlushStackFamily(Array<CellStack>&cellStack,  BYTE family)
	{
		const index_t offset = high.grid.GetFamilyOffset(family);
		const count_t range = high.grid.CountFamilyMembers(family);
		for (index_t i = offset; i < offset+range; i++)
			cellStack[i].ZeroInit();		
	
	}



	void	ResetStructureCounters()
	{
		actualWorld.ResetStructureCounters();


	}

	void	FlushAllData()
	{
		empZonesLocked = 0;
		empZonesUnlocked = 0xFFFF;

		actualWorld.FlushAllData();
		#ifndef CLIENT
			initialWorld.FlushAllData();
		#endif
			
		#ifdef HAS_WINDOW
			visibleOrbitGrid.Clear();
			for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
			{
				high.family[k].SetOutdated(0);
				low.family[k].SetOutdated(0);
			}
			foreach (Resource::all,res)
				memset(res->known,false,sizeof(res->known));

			UnsetParaboloidAndSphereDisplay();
		#endif

		#ifdef DEBUG_SENSORY
			sensors.Clear();
		#endif
	}


	#ifdef VOXEL_SCANNER
		void	RemapToLowStacks(BYTE family)
		{
			index_t offset = low.grid.GetFamilyOffset(family);
			count_t length = low.grid.CountFamilyMembers(family);
			Concurrency::parallel_for(offset,offset+length,[offset](index_t i)
			{
				LowCellStack&target = lowCellStack[i];
				const UINT32 high = Grid::low.grid.GetCell(i).higherEquivalent;
				const auto&n = Grid::high.grid.GetCell(high).neighborhood;

				for (unsigned layer = 0; layer < CellStack::Range; layer++)
				{
					int cnt = cellStack[high].volumeCell[layer].IsSolid() ? 1 : 0;
					cellStack[high].volumeCell[layer].visible = target.volumeCell[layer].visibility;
					for (BYTE k = 0; k < n.numNeighbors; k++)
					{
						cellStack[n.neighborID[k]].volumeCell[layer].visible = target.volumeCell[layer].visibility;	//race-condition - dont care
						if (cellStack[n.neighborID[k]].volumeCell[layer].IsSolid())
							cnt ++;
					}
					target.volumeCell[layer].isEmpty = cnt == 0;
					target.volumeCell[layer].isFull = cnt == 1 + n.numNeighbors;
				}
			});
	
		}
	#endif


	#ifdef HYBRID_GRID
		TVec3<> ToWorld(const HCellIdU&id)
		{
			return high.cells[id.stackID].centerVertex.xyz * Grid::ToRadius(CellStack::ToHeight(id.layer));
		}
		TVec3<> ToWorld(const HCellIdI&id)
		{
			return high.cells[id.stackID].centerVertex.xyz * Grid::ToRadius(id.height);
		}
	#endif
	TVec3<> ToWorld(const LCellIdU&id)
	{
		return low.cells[id.stackID].centerVertex.xyz * Grid::ToRadius(CellStack::ToHeight(id.layer));
	}
	TVec3<> ToWorld(const LCellIdI&id)
	{
		return low.cells[id.stackID].centerVertex.xyz * Grid::ToRadius(id.height);
	}

	bool	AffectedByEMP(const HCellIdI&id)
	{
		return !(Grid::high.cells[id.stackID].cornerFlags & empZonesUnlocked);
	}
	bool	AffectedByEMP(const LCellIdI&id)
	{
		return !(Grid::low.cells[id.stackID].cornerFlags & empZonesUnlocked);
	}




	void				VisibilityState::ChangeSensory(const Grid::HCellIdU&center, float t, short changeBy)
	{
		bool familyChanged[BaseGrid::NumFamilies] = { false };

		Grid::TemplateHelper::ChangeSensory(center, changeBy, cellStack.pointer(), familyChanged);

		for (index_t i = 0; i < BaseGrid::NumFamilies; i++)
			if (familyChanged[i])
				families[i].changed = true;
	}

	void				VisibilityState::ChangeSensory(const Grid::HighSetup::TVolumeDescriptor&desc, float t, short changeBy)
	{
		bool familyChanged[BaseGrid::NumFamilies] = { false };
		Grid::TemplateHelper::ChangeSensory(desc, changeBy, cellStack.pointer(), familyChanged);
		for (index_t i = 0; i < BaseGrid::NumFamilies; i++)
			if (familyChanged[i])
				families[i].changed = true;

	}

	//void				VisibilityState::ChangeSensory(const LCellIdI&center, float radius, float t, short changeBy)
	//{
	//	bool familyChanged[BaseGrid::NumFamilies] = { false };
	//	Grid::TemplateHelper::ChangeSensory(Grid::ToWorld(center), radius, changeBy, cellStack.pointer(), familyChanged);
	//	for (index_t i = 0; i < BaseGrid::NumFamilies; i++)
	//		if (familyChanged[i])
	//			families[i].changed = true;
	//}
	void				VisibilityState::ChangeSensory(const SkySensorState&state, float t, short changeBy)
	{
		bool familyChanged[BaseGrid::NumFamilies] = { false };
		Grid::TemplateHelper::ChangeSensorCoverage(state, changeBy, cellStack.pointer(), familyChanged);
		for (index_t i = 0; i < BaseGrid::NumFamilies; i++)
			if (familyChanged[i])
				families[i].changed = true;
	}

	void				VisibilityState::RegisterExplosionCrater(const Grid::HCellIdI&center, float radius, float t)
	{
		bool familyChanged[BaseGrid::NumFamilies] = { false };
		const float3 c3 = Grid::high.cells[center.stackID].centerVertex.xyz * Grid::ToRadiusf(0.5f + center.height);

		Grid::TemplateHelper::ChangeSensory(c3, radius, 0, cellStack.pointer(), familyChanged);
		for (index_t i = 0; i < BaseGrid::NumFamilies; i++)
			if (familyChanged[i])
				families[i].changed = true;
	}



	void VisibilityState::FlushAllData(bool includeResources/*=true*/)
	{
		if (includeResources)
			resourcesDiscovered.Fill(false);
		for (index_t i = 0; i < ARRAYSIZE(families); i++)
			families[i].Reset();
		foreach (cellStack,stack)
			stack->Reset();

	}

	void		VisibilityState::CheckChanges(float t)
	{
		for (BYTE f = 0; f < BaseGrid::NumFamilies; f++)
		{
			if (families[f].changed)
			{
				families[f].changed = false;
				const index_t offset = Grid::high.grid.GetFamilyOffset(f);
				const count_t range = Grid::high.grid.CountFamilyMembers(f);
				bool anyChanged = false;
				Concurrency::parallel_for(offset, offset + range, [this, &anyChanged](index_t i)
				{
					for (unsigned j = 0; j < Grid::CellStack::Range; j++)
					{
						CellStack::Cell&cell1 = cellStack[i].volumeCell[j];
						if (cell1.Changed(true))
							anyChanged = true;
					}
				});
				if (anyChanged)
				{
					families[f].coverageChanged = true;
					families[f].lastChange = t;
				}
	}
			}

		}






	#ifdef CLIENT
		void		UpdateResourceMap(ConstName resourceName,const ArrayData<TCellResourceCoverage>&data)
		{
			foreach (Resource::all, res)
				if (res->GetCName() == resourceName)
				{
					foreach (data,el)
					{
						ASSERT_LESS__(el->cellIndex,ARRAYSIZE(res->coverage));
						res->coverage[el->cellIndex] = el->coverage;
						res->known[el->cellIndex] = true;
						low.family[low.grid.GetCell(el->cellIndex).family].SetOutdated(0);
					}
					return;
				}
			FATAL__("Unable to locate resource type #"+String((int)resourceName));
		}
	#endif

	#ifdef HAS_WINDOW




		//void	OrbitGrid::Color(const LCellIdI&center, int fuelRadius)
		//{
		//	TColorArea newArea = {center,fuelRadius,true};
		//	if (newArea == colorArea)
		//		return;
		//}
		//void	OrbitGrid::StripColor()
		//{
		//	if (!colorArea.active)
		//		return;
		//	colorArea.active = false;
		//}


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
		instruction.ibo = low.gridInstruction.ibo;
		instruction.vbo.CreateFromStructs(vertices);
	}






	
	void		OrbitGrid::ChangeSensory(const LCellIdI&center, float radius, signed char change)
	{
		Timer::Time t0 = timer.Now();

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

		LogEvent(EventType::PerformanceMeasurement,nullptr,"Orbital sensory change processed in "+String(timer.GetSecondsSince(t0))+". Used orbit was "+String(orbit));
		//ShowMessage(timer.GetSecondsSince(t0));
	}

	void		WorldData::ChangeSensorCoverage(const SkySensorState&state, signed char change)
	{
		bool familyChanged[BaseGrid::NumFamilies] = {false};
		TemplateHelper::ChangeSensorCoverage(state,change,cellStack.pointer(),familyChanged);
		for (index_t i = 0; i < BaseGrid::NumFamilies; i++)
			if (familyChanged[i])
				Grid::high.family[i].SetOutdated(0);
	}

	void		WorldData::ChangeSensory(const LCellIdI&center, float radius, signed char change)
	{
		bool familyChanged[BaseGrid::NumFamilies] = {false};
		TemplateHelper::ChangeSensory(Grid::ToWorld(center),radius,change,cellStack.pointer(),familyChanged);
		for (index_t i = 0; i < BaseGrid::NumFamilies; i++)
			if (familyChanged[i])
				Grid::high.family[i].SetOutdated(0);
		
	}

	#ifdef THOROUGLY_DEBUG_SENSORY
		void	InsertDbgSensor(const LCellIdI&center, float radius)
		{
			Timer::Time t0 = timer.Now();

			Math::Box<> test;
			const TVec3<> worldCenter = ToWorld(center);
			test.SetCenter(worldCenter,radius);
			for (BYTE family = 0; family < BaseGrid::NumFamilies; family++)
			{
				if (!high.planetFamilyBoundingBox[family].Intersects(test))
					continue;

				const index_t offset = high.grid.GetFamilyOffset(family);
				const count_t range = high.grid.CountFamilyMembers(family);
				Concurrency::parallel_for(offset,offset+range,[worldCenter,radius](index_t i)
				{
					//bool anyChanged = false;
					bool anyVisible = false;
					Grid::HCellIdU cell((UINT32)i,0U);
					{
						Grid::HCellIdU aboveTop(cell.stackID, Grid::CellStack::Range);
						TVec3<> bottom = Grid::ToWorld(cell),
								top = Grid::ToWorld(aboveTop);

						anyVisible = Obj::DetectSphereEdgeIntersection(worldCenter,radius,bottom,top);
					}
					if (!anyVisible)
						return;

					bool wasIn = false;
					for (unsigned j = 0; j < Grid::CellStack::Range; j++)
					{
						cell.layer = j;
						if (Vec::quadraticDistance(Grid::ToWorld(cell),worldCenter) > sqr(radius))
						{
							if (wasIn)
								break;
							continue;
						}
						wasIn = true;
						cellStack[i].volumeCell[j].dbgVisibility ++;
					}
					Grid::high.family[Grid::high.grid.GetCell(i).family].SetOutdated(0);
				});
			}

			LogEvent(EventType::Debug,nullptr,"Sensory change processed in "+String(timer.GetSecondsSince(t0)));
			//ShowMessage(timer.GetSecondsSince(t0));
		}
	#endif



	bool		WorldData::HasSensor(const LCellIdI&cell, float radius) const
	{
		#ifdef DEBUG_SENSORY
			TSensor s;
			s.center = cell;
			s.radius = radius;
			return sensors.Contains(s);	
		#else
			return true;
		#endif
	}

	void		WorldData::InsertSensor(const LCellIdI&cell, float radius)
	{
		ChangeSensory(cell,radius,1);
		#ifdef DEBUG_SENSORY
			TSensor s;
			s.center = cell;
			s.radius = radius;
			//ASSERT__(!sensors.Contains(s));	//this can fire falsely, if a vehicle is moved to the previous location of another vehicle (which hasn't updated yet)
			sensors.Append(s);
		#endif
	}
	void		WorldData::RemoveSensor(const LCellIdI&cell, float radius)
	{
		#ifdef DEBUG_SENSORY
			TSensor s;
			s.center = cell;
			s.radius = radius;
			ASSERT__(sensors.FindAndErase(s));	//this must always work, however
		#endif
		ChangeSensory(cell,radius,-1);
	}

	#endif

	/**/					WorldData::WorldData():cellStack(HighSetup::GridType::CellCount)
	{}

}

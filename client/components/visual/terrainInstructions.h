#pragma once

#ifdef HAS_WINDOW

#include "shadedRenderInstruction.h"

namespace VisualTerrain
{
	
	
	namespace Instruction
	{	
		namespace DX11 = Engine::DX11;
		using namespace DeltaWorks;

		enum displayMode_t
		{
			NormalRendering,
			ParaboloidColored,
			SphereColored
		};

		extern displayMode_t					displayMode;
		extern DX11::ObjectBuffer<float4>	ParaboloidDirection_Height,SphereDef,SphereColor;
		extern DX11::ObjectBuffer<float4>	ParaboloidColor_FlatRadius;


		struct THeightMapOrientation
		{
			M::TMatrix4<>	orientation0,
							orientation1,
							orientation2;
		};

		static const size_t sTHeightMapOrientation = sizeof(THeightMapOrientation);
		static_assert(sTHeightMapOrientation==sizeof(float)*16*3,"Sruct alignment out of wack");
		extern DX11::ObjectBuffer<THeightMapOrientation>	heightMapOrientation;	//used for lava
		extern DX11::Texture								cubeHeightMap;	//used for lava

		extern ShadedInstruction			waterBase, waterSphereBase, waterParaboloidBase, terrainBase, terrainSphereBase, terrainParaboloidBase;
		extern DX11::BaseInstructionBlock	terrainShadowBase, 
											overlayBase, overlayShadowBase, resourceBase;// , terrainEdgeBase;
	}


}


#endif

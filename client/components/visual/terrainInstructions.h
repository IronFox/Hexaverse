#pragma once

#ifdef HAS_WINDOW

#include "../../ExtRenderer/DirectX11.h"
#include <math/vclasses.h>

namespace VisualTerrain
{
	
	
	namespace Instruction
	{	
		namespace DX11 = Engine::DX11;

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


}


#endif

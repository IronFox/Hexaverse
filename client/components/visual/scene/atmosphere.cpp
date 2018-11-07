#ifdef HAS_WINDOW
#include <global_root.h>
#include "atmosphere.h"
#include <math/vclasses.h>

#include "../display.h"
#include "terrainInstructions.h"
#include "objectRendering.h"
#include "lighting.h"
#include "shadedRenderInstruction.h"

namespace Atmosphere
{
	namespace DX11 = Engine::DX11;
	ShadedInstructionBlock16	instruction;
//	static DX11::ObjectBuffer<float4>	helpers;


	void		Render(const Engine::Aspect<>&mainCamera)
	{
	//	float d = Vec::length(cameraPosition);	// |c-o|
	//	if (d <= TypeInfo<float>::error)
	//		return;
	//	float centerRadius = std::min(d-0.1f,upperPlanetRadius);
	////	(/*upperPlanetRadius + */lowerPlanetRadius) * 0.5f/*/ 2.f*/;
	//	float distanceFromCenter = M::Sqr(centerRadius) / d;
	//	float radius = sqrt(M::Sqr(centerRadius) - M::Sqr(distanceFromCenter));

	//	float3 normal = cameraPosition / d;
	//	float3 center = normal * distanceFromCenter;

		M::TMatrix4<> m;
		Mat::Mult(mainCamera.viewInvert,mainCamera.projectionInvert,m);
	//	Mat::transpose(m);	//might not be necessary

		display.SetObjectMatrix(m);


		//M::TMatrix4<> system;
		//Mat::makeAxisSystem(center,normal,2,system);
		//system.x.xyz *= radius;
		//system.y.xyz *= radius;
		////system.z.xyz *= radius;
		//display.SetObjectMatrix(system);
		display.StoreRendererState();
		display.SetBlendMode(Engine::BlendMode::Add);
		display.SetDepthTest(false,false);
		display.SetRasterizer(true,false);	//will be set to true once visible

		display.Render(instruction);
		//display.SetBlendMode(Engine::BlendMode::AlphaBlend);
		//display.SetDepthTest(true,true);
		//display.SetRasterizer(true,true);
		display.RestoreRendererState();
	}

	void		Create()
	{

		ShadedInstruction base;
		base.binding.Append("POSITION",0,DXGI_FORMAT_R32G32_FLOAT);

		Buffer0<DX11::Shader::Binding> bindings;

		//bindings;
		
		bindings <<	DX11::Shader::Binding(Lighting::GPUResources::sunDirection,"Sun");
		bindings << DX11::Shader::Binding(Lighting::GPUResources::sunColor,"SunColor");

		#ifdef CASCADING_SHADOW
			for (index_t i = 0; i < Lighting::NumCascades; i++)
			{
				bindings.Append(DX11::Shader::Binding(Lighting::GPUResources::shadowMatrix[i],"ShadowMatrix"+String(i)));
				bindings.Append(DX11::Shader::Binding(Lighting::GPUResources::shadowMap[i].GetDepthStencilTarget(), "ShadowMap"+String(i)));
			}
		#else
			bindings.Append(DX11::Shader::Binding(Lighting::GPUResources::shadowMatrix,"ShadowMatrix"));
			bindings.Append(DX11::Shader::Binding(Lighting::GPUResources::shadowMap.GetDepthStencilTarget(), "ShadowMap"));
		#endif
		bindings.Append(DX11::Shader::SamplerBinding(true, Engine::TextureFilter::None, 1, "ShadowSampler"));
		bindings.Append(DX11::Shader::Binding(Lighting::GPUResources::atmosphereConfig, "AtmosphereConfig"));

		base.LoadQualitiesFromFile("shaders/atmosphere.hlsl", "vsMain", "psMain", bindings.pointer(), bindings.Count());
		base.SetTriangles();
		base.UpdateLayout();	


		Array<float2>	vertices(4);
		Array<UINT16>	indices(6);
		vertices[0] = float2(-1);
		vertices[1] = float2(1,-1);
		vertices[2] = float2(1);
		vertices[3] = float2(-1,1);
		indices[0] = 0;
		indices[1] = 1;
		indices[2] = 2;
		indices[3] = 0;
		indices[4] = 2;
		indices[5] = 3;
		
		instruction = base;
		instruction.vbo.CreateFromStructs(vertices);
		instruction.ibo.Create(indices);


	}

	void		Destroy()
	{
		instruction.Clear();
	
	}
}

#endif

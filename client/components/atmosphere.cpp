#include <global_root.h>
#include "atmosphere.h"
#include <engine/display.h>
#include "../ExtRenderer\DirectX11.h"
#include "../../shared/objects.h"

#ifdef HAS_WINDOW


extern Engine::Display<Engine::DirectX11>	display;



namespace Atmosphere
{
	namespace DX11 = Engine::DX11;
	DX11::InstructionBlock16	instruction;
//	static DX11::ObjectBuffer<float4>	helpers;


	void		Render(const Engine::Aspect<>&mainCamera)
	{
	//	float d = Vec::length(cameraPosition);	// |c-o|
	//	if (d <= TypeInfo<float>::error)
	//		return;
	//	float centerRadius = std::min(d-0.1f,upperPlanetRadius);
	////	(/*upperPlanetRadius + */lowerPlanetRadius) * 0.5f/*/ 2.f*/;
	//	float distanceFromCenter = sqr(centerRadius) / d;
	//	float radius = sqrt(sqr(centerRadius) - sqr(distanceFromCenter));

	//	float3 normal = cameraPosition / d;
	//	float3 center = normal * distanceFromCenter;

		TMatrix4<> m;
		Mat::mult(mainCamera.viewInvert,mainCamera.projectionInvert,m);
	//	Mat::transpose(m);	//might not be necessary

		display.SetObjectMatrix(m);


		//TMatrix4<> system;
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
		//static const count_t Resolution = 100;
		//Array<float2>	vertices(Resolution+1);
		//Array<UINT16>	indices(Resolution*3);
		//vertices.first() = float2(0,0);
		//for (index_t i = 0; i < Resolution; i++)
		//{
		//	float a = float(i)/float(Resolution) * M_PIF * 2.f;
		//	vertices[i+1] = float2(cos(a),sin(a)) * 1.4f;
		//	indices[i*3] = (UINT16)0;
		//	indices[i*3+1] = (UINT16)(i+1);
		//	indices[i*3+2] = (UINT16)(((i+1)%Resolution) + 1);
		//}



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
		
		instruction.vbo.CreateFromStructs(vertices);
		instruction.ibo.Create(indices);

		instruction.binding.Append("POSITION",0,DXGI_FORMAT_R32G32_FLOAT);

		Buffer0<DX11::Shader::Binding> bindings;

		//bindings;
		
		bindings <<	DX11::Shader::Binding(Grid::Instruction::sunDirection,"Sun");
		bindings << DX11::Shader::Binding(Grid::Instruction::sunColor,"SunColor");

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
		bindings.Append(DX11::Shader::SamplerBinding(true, Engine::TextureFilter::None, 1, "ShadowSampler"));
		bindings.Append(DX11::Shader::Binding(Grid::Instruction::atmosphereConfig, "AtmosphereConfig"));

		instruction.shader.LoadFromFile("shaders/atmosphere.hlsl", "vsMain", "psMain", bindings.pointer(), bindings.Count());
		instruction.SetTriangles();
		instruction.UpdateLayout();
	}

	void		Destroy()
	{
		instruction.Clear();
	
	}
}

#endif

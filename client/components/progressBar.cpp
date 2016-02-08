#ifdef HAS_WINDOW

#include <global_root.h>
#include "progressBar.h"
#include <engine/display.h>
#include "../ExtRenderer\DirectX11.h"
#include "../../shared/sphereGrid.h"


extern Engine::Display<Engine::DirectX11>	display;

namespace ProgressBar
{
	namespace DX11 = Engine::DX11;
	DX11::StreamVBO	vbo;
	DX11::BaseInstructionBlock instruction;
	float3 center;
	float barScale= 1.f;

	static const count_t NumVertices = 64;
	struct TVertex
	{
		float4	positionMax,
				colorProgress;
		float2	lineProgress_scale;
	};

	Buffer0<TVertex>	vertices;
	int					lineProgress = 0;

	void	SetLocation(const float3&worldCenter, float scale/*=1.f*/)
	{
		center = worldCenter;
		lineProgress = 0;
		barScale = scale;
	}

	/**
	@brief Appends and renders a new bar. Multiple bars may be rendered for one location this way
	*/
	void	RenderNewBar(float progress, float max, const float3&color)
	{
		if (max > 10)
		{
			progress /= max;
			max = 1;
		}
		TVertex&vtx = vertices.Append();
		vtx.positionMax = float4(center,max);
		vtx.colorProgress = float4(color,progress);
		vtx.lineProgress_scale = float2(lineProgress++, barScale);
		if (vertices.Count() == NumVertices)
		{
			End();
		}
	
	
	}

	void	End()
	{
		//display.SetRasterizer(true,true);
		//display.SetDepthTest(false,false);
		//display.SetBlendMode(Engine::BlendMode::None);
		if (vertices.IsNotEmpty())
		{
			display.StoreRendererState();
			display.SetBlendMode(Engine::BlendMode::AlphaBlend);
			//display.SetDepthTest(true,false);
			display.SetDepthTest(false,false);	//experimental
			display.SetRasterizer(true,true);
			vbo.PartialUpdate(vertices.pointer(),vertices.Count(),sizeof(TVertex));

			display.RenderVertices(instruction,0,(UINT32)vertices.Count());
			vertices.Clear();

			display.RestoreRendererState();
		}
		//display.SetBlendMode(Engine::BlendMode::AlphaBlend);
		//display.SetDepthTest(true,true);
		//display.SetRasterizer(true,true);
	}

	void	Create()
	{
		vbo.Create(NumVertices,sizeof(TVertex));
	
		const count_t cnt = DX11::Shader::macroDefinitions.Count();

		DX11::Shader::macroDefinitions << DX11::Shader::Macro("BAR_SIZE",Grid::SingleLayerHeight);

		instruction.vbo = vbo;
		instruction.binding.Append("PositionMax",0,DXGI_FORMAT_R32G32B32A32_FLOAT);
		instruction.binding.Append("ColorProgress",0,DXGI_FORMAT_R32G32B32A32_FLOAT);
		instruction.binding.Append("LineProgress_Scale",0,DXGI_FORMAT_R32G32_FLOAT);

		instruction.shader.LoadFromFile("shaders/entityProgressBar.hlsl","vsMain","gsMain","psMain");
		instruction.SetPoints();
		instruction.UpdateLayout();

		DX11::Shader::macroDefinitions.Truncate(cnt);
	}
	void	Destroy()
	{
		instruction.Clear();
		vbo.Clear();
	}
}




#endif

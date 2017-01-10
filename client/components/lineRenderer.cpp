#ifdef HAS_WINDOW

#include <global_root.h>
#include "lineRenderer.h"

#include <engine/display.h>

extern Engine::Display<Engine::DirectX11>	display;

namespace LineRenderer
{
	static Engine::DX11::BaseInstructionBlock	lines;
	static Engine::DX11::StreamVBO	geometry;

	static const count_t maxLines = 1024;

	static float _outBuffer[maxLines * 2 * 7], *_at = _outBuffer;
	static float *const _end = _outBuffer + maxLines * 2 * 7;
	

	void	Create()
	{
		geometry.Create(maxLines*2, sizeof(float) * 7);
		lines.vbo = geometry;
		{
			Engine::VertexBinding binding;
			binding.normal.unset();
			binding.color.set(3, 4);
			binding.floats_per_vertex = 7;
			lines.binding = binding;
		}

		lines.shader.LoadFromFile("shaders/lines.hlsl", "vsMain", "psMain", nullptr, 0);
		lines.UpdateLayout();
		lines.SetLines();
	}
	void	Destroy()
	{
		geometry.Clear();
		lines = Engine::DX11::BaseInstructionBlock();
	}

	static float4 _color(1);

	void	SetColor(const float4&color)
	{
		_color = color;
	}
	void	Rasterize(const float3&p0, const float3&p1)
	{
		Rasterize(p0, _color, p1, _color);
	}

	void	Rasterize(const TFloatRect&r)
	{
		float3	p0 = float3(r.x.min, r.y.min, 0 ),
				p1 = float3(r.x.max, r.y.min, 0 ),
				p2 = float3(r.x.max, r.y.max, 0 ),
				p3 = float3(r.x.min, r.y.max, 0 );
		Rasterize(p0,p1);
		Rasterize(p1,p2);
		Rasterize(p2,p3);
		Rasterize(p3,p0);
	}

	void	Rasterize(const float3&p0, const float4&c0, const float3&p1, const float4&c1)
	{
		Vec::ref3(_at) = p0;	_at += 3;
		Vec::ref4(_at) = c0;	_at += 4;
		Vec::ref3(_at) = p1;	_at += 3;
		Vec::ref4(_at) = c1;	_at += 4;
		if (_at == _end)
			Flush();
	}
	void	Flush()
	{
		if (_at == _outBuffer)
			return;

		count_t vcnt = (_at - _outBuffer) / 7;
		geometry.PartialUpdate(_outBuffer, vcnt, 7 * sizeof(float));
		display.RenderVertices(lines, 0, (UINT32)vcnt);
		_at = _outBuffer;

	}
}

#endif
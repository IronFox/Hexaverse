#ifdef HAS_WINDOW

#include <global_root.h>
#include "surfaceRenderer.h"

#include "../display.h"

namespace SurfaceRenderer
{
	static Engine::DX11::BaseInstructionBlock	triangles;
	static Engine::DX11::StreamVBO	geometry;

	static const count_t maxTriangles = 1024;

	static const count_t fpv = 6;

	static float _outBuffer[maxTriangles * 3 * fpv], *_at = _outBuffer;
	static float *const _end = _outBuffer + maxTriangles * 3 * fpv;
	



	void	Create()
	{
		geometry.Create(maxTriangles*3, sizeof(float) * fpv);
		triangles.vbo = geometry;
		{
			Engine::VertexBinding binding;
			binding.normal.Unset();
			binding.color.Set(2, 4);
			binding.floats_per_vertex = (UINT16)fpv;
			triangles.binding = binding;
		}

		triangles.shader.LoadFromFile("shaders/triangles.hlsl", "vsMain", "psMain", nullptr, 0);
		triangles.UpdateLayout();
		triangles.SetTriangles();
	}
	void	Destroy()
	{
		geometry.Clear();
		triangles = Engine::DX11::BaseInstructionBlock();
	}

	static float4 _color(1);

	void	SetColor(const float4&color)
	{
		_color = color;
	}

	void	Rasterize(const M::TVec2<>&p0, const M::TVec2<>&p1, const M::TVec2<>&p2)
	{
		Rasterize(p0,_color,p1,_color,p2,_color);
	}

	void	Rasterize(const M::TVec2<>&p0, const M::TVec2<>&p1, const M::TVec2<>&p2, const M::TVec2<>&p3)
	{
		Rasterize(p0,p1,p2);
		Rasterize(p0,p2,p3);
	}

	void	Rasterize(const M::TVec2<>&p0, const M::TVec4<>&c0, const M::TVec2<>&p1, const M::TVec4<>&c1, const M::TVec2<>&p2, const M::TVec4<>&c2, const M::TVec2<>&p3, const M::TVec4<>&c3)
	{
		Rasterize(p0,c0,p1,c1,p2,c2);
		Rasterize(p0,c0,p2,c2,p3,c3);
	}


	void	Rasterize(const M::Rect<>&rect)
	{
		Rasterize(	float2(rect.x.min,rect.y.min),
					float2(rect.x.max,rect.y.min),
					float2(rect.x.max,rect.y.max),
					float2(rect.x.min,rect.y.max));
	}

	void	RasterizeConvexPolygon(const M::TVec2<>*field, count_t numVertices)
	{
		for (index_t i = 1; i < numVertices; i++)
		{
			Rasterize(field[0],field[i],field[(i+1)%numVertices]);
		}
	}



	void	Rasterize(const M::TVec2<>&p0, const M::TVec4<>&c0, const M::TVec2<>&p1, const M::TVec4<>&c1, const M::TVec2<>&p2, const M::TVec4<>&c2)
	{
		Vec::ref2(_at) = p0;	_at += 2;
		Vec::ref4(_at) = c0;	_at += 4;
		Vec::ref2(_at) = p1;	_at += 2;
		Vec::ref4(_at) = c1;	_at += 4;
		Vec::ref2(_at) = p2;	_at += 2;
		Vec::ref4(_at) = c2;	_at += 4;
		if (_at == _end)
			Flush();
	}

	void	Flush()
	{
		if (_at == _outBuffer)
			return;

		count_t vcnt = (_at - _outBuffer) / fpv;
		geometry.PartialUpdate(_outBuffer, vcnt, fpv * sizeof(float));
		display.RenderVertices(triangles, 0, (UINT32)vcnt);
		_at = _outBuffer;

	}






	void	Baked::Bake()
	{
		((DX11::BaseInstructionBlock&)instruction) = triangles;
		instruction.vbo.CreateFromStructs(vertices);
		instruction.ibo.Create(indices);
		Clear();
	}

	void	Baked::Rasterize() const
	{
		display.Render(instruction,true);
	}

}

#endif
